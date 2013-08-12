//
// TaksiDll.cpp
// Link this into the process space of the app being recorded.
//
#include "../stdafx.h"
#include <stddef.h> // offsetof
#include "TaksiDll.h"
#include "GAPI_Base.h"
#include "HotKeys.h"

#ifdef USE_GDIP
#include "../common/CImageGDIP.h"
#endif

//**************************************************************************************
// Shared by all processes
// NOTE: Must be init with 0
// WARN: Constructors WOULD BE CALLED for each DLL_PROCESS_ATTACH so we cant use Constructors.
//**************************************************************************************
#pragma data_seg(".SHARED")			// ".HKT" ".SHARED"
CTaksiShared sg_Shared = {0};		// API to present to the Master EXE 
CTaksiConfigData sg_Config = {0};	// Read from the INI file. and set via CGuiConfig
CTaksiProcStats sg_ProcStats = {0};	// For display in the Taksi.exe app.
#pragma data_seg()
#pragma comment(linker, "/section:.SHARED,rws") // tell the linker what this is.

//**************************************************************************************
// End of Inter-Process shared section 
//**************************************************************************************

HINSTANCE g_hInst = NULL;		// Handle for the dll for the current process.
CTaksiProcess g_Proc;			// information about the process i am attached to.
CTaksiLogFile g_Log;			// Log file for each process. seperate

static CTaksiGAPIBase* const s_aGAPIs[ TAKSI_GAPI_QTY ] = 
{
	NULL,	// TAKSI_GAPI_NONE
	NULL,	// TAKSI_GAPI_DESKTOP // lowest priority, 
	&g_GDI,	// TAKSI_GAPI_GDI // lowest priority, since all apps do GDI
	&g_OGL,	// TAKSI_GAPI_OGL
#ifdef USE_DIRECTX8
	&g_DX8,	// TAKSI_GAPI_DX8
#endif
#ifdef USE_DIRECTX9
	&g_DX9,	// TAKSI_GAPI_DX9 // almost no apps load this unless they are going to use it.
#endif
};

//**************************************************************************************

HRESULT CTaksiLogFile::OpenLogFile( const TCHAR* pszFileName )
{
	CloseLogFile();
	if ( ! sg_Config.m_bDebugLog)
	{
		return HRESULT_FROM_WIN32(ERROR_CANCELLED);
	}

	m_File.AttachHandle( ::CreateFile( pszFileName,            // file to create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // append existing else create
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL ));                        // no attr. template 
	if ( ! m_File.IsValidHandle())
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE));
		return hRes;
	}

	// Append to the end of the file if it exists
	if ( GetLastError() == ERROR_ALREADY_EXISTS )
	{
		SetFilePointer( (HANDLE)m_File, GetFileSize((HANDLE)m_File, NULL), NULL, FILE_BEGIN );
	}

	return S_OK;
}

void CTaksiLogFile::CloseLogFile()
{
	if ( ! m_File.IsValidHandle())
		return;
	Debug_Info("Closing log." LOG_CR);
	m_File.CloseHandle();
}

int CTaksiLogFile::EventStr( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszMsg )
{
	// Write to the m_File. 
	LOGCHAR szTmp[_MAX_PATH];
	int iLen = _snprintf( szTmp, sizeof(szTmp)-1,
		"Taksi:%s:%s", g_Proc.m_szProcessTitleNoExt, pszMsg );

	if ( sg_Config.m_bDebugLog && m_File.IsValidHandle())
	{
		DWORD dwWritten = 0;
		::WriteFile( m_File, szTmp, iLen, &dwWritten, NULL );
	}

	return __super::EventStr(dwGroupMask,eLogLevel,szTmp);
}

//**************************************************************************************

const WORD CTaksiProcStats::sm_Props[ TAKSI_PROCSTAT_QTY ][2] = // offset,size
{
#define ProcStatProp(a,b,c,d) { ( offsetof(CTaksiProcStats,b)), sizeof(((CTaksiProcStats*)0)->b) },
#include "../ProcStatProps.tbl"
#undef ProcStatProp
};

void CTaksiProcStats::InitProcStats()
{
	m_dwProcessId = 0;
	m_szProcessPath[0] = '\0';
	m_szLastError[0] = '\0';

	m_hWndCap = NULL;
	m_SizeWnd.cx = 0;
	m_SizeWnd.cy = 0;
	m_eGAPI = TAKSI_GAPI_NONE;

	m_eState = TAKSI_INDICATE_Hooking;		// assume we are looking for focus
	m_fFrameRate = 0;			// measured frame rate. recording or not.
	m_nDataRecorded = 0;		// How much video data recorded in current stream (if any)

	m_dwPropChangedMask = 0xFFFFFFFF;	// all new
}

void CTaksiProcStats::ResetProcStats()
{
	m_szLastError[0] = '\0';
	m_fFrameRate = 0;			// measured frame rate. recording or not.
	m_nDataRecorded = 0;		// How much video data recorded in current stream (if any)
}

void CTaksiProcStats::CopyProcStats( const CTaksiProcStats& stats )
{
	memcpy( this, &stats, sizeof(stats));
	m_dwPropChangedMask = 0xFFFFFFFF;	// all new
}

//**************************************************************************************

LRESULT CALLBACK CTaksiShared::HookCBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	// WH_CBT = computer-based training - hook procedure
	// NOTE: This is how we inject this DLL into other processes.
	// NOTE: There is a race condition here where sg_Shared.m_hHookCBT can be NULL. 
	// NOTE: This can probably get called on any thread?!
	// ASSERT(sg_Shared.IsHookCBT());
	switch (nCode)
	{
	// case HCBT_CREATEWND:
	case HCBT_ACTIVATE:
	case HCBT_SETFOCUS:
		// Set the DLL implants on whoever gets the focus/activation!
		// ASSUME DLL_PROCESS_ATTACH was called.
		// TAKSI_INDICATE_Hooking
		// wParam = NULL. can be null if losing focus ? (NULL not used for desktop in this case)
		if ( g_Proc.m_bIsProcessIgnored )
			break;
		LOG_MSG(( "HookCBTProc: nCode=0x%x, wParam=0x%x" LOG_CR, (DWORD)nCode, wParam ));
		if ( wParam == NULL )	// ignore this!
			break;
		if ( g_Proc.AttachGAPIs( (HWND) wParam ) == S_OK )
		{
			g_Proc.CheckProcessCustom();	// determine frame capturing algorithm 
		}
		break;
	}

	// We must pass the all messages on to CallNextHookEx.
	return ::CallNextHookEx(sg_Shared.m_hHookCBT, nCode, wParam, lParam);
}

bool CTaksiShared::HookCBT_Install(void)
{
	// Installer for calling of HookCBTProc 
	// This causes my dll to be loaded into another process space. (And hook it!)
	if ( IsHookCBT())
	{
		LOG_MSG(( "HookCBT_Install: already installed. (0x%x)" LOG_CR, m_hHookCBT ));
		return false;
	}

	m_hHookCBT = ::SetWindowsHookEx( WH_CBT, HookCBTProc, g_hInst, 0);

	LOG_MSG(( "HookCBT_Install: hHookCBT=0x%x,ProcID=%d" LOG_CR,
		m_hHookCBT, ::GetCurrentProcessId()));

	UpdateMaster();
	return( IsHookCBT());
}

bool CTaksiShared::HookCBT_Uninstall(void)
{
	// Uninstaller for WH_CBT
	// NOTE: This may not be the process that started the hook
	// We may have successfully hooked an API. so stop looking.
	if (!IsHookCBT())
	{
		LOG_MSG(( "HookCBT_Uninstall: already uninstalled." LOG_CR));
		return false;
	}

	if ( ::UnhookWindowsHookEx( m_hHookCBT ))
	{
		LOG_MSG(( "CTaksiShared::HookCBT_Uninstall: hHookCBT=0%x,ProcID=%d" LOG_CR,
			m_hHookCBT, ::GetCurrentProcessId()));
	}
	else
	{
		DEBUG_ERR(( "CTaksiShared::HookCBT_Uninstall FAIL" LOG_CR ));
	}
	m_hHookCBT = NULL;
	UpdateMaster();
	return true;
}

//**************************************************************************************

void CTaksiShared::UpdateMaster()
{
	// tell the Master EXE to redisplay state info. 
	// sg_ProcStats or sg_Shared has changed
	// PostMessage is Multi thread safe.
	if ( m_hMasterWnd == NULL )
		return;
	if ( m_bMasterExiting )
		return;
	if ( ::PostMessage( m_hMasterWnd, WM_APP_UPDATE, 0, 0 ))
	{
		m_iMasterUpdateCount ++;	// count unprocessed WM_APP_UPDATE messages
	}
}

void CTaksiShared::OnMasterTick()
{
	// The Master EXE must supply a Tick to make the TAKSIMODE_DESKTOP work.
	// Must tick at the rate of the recording.

	ASSERT( m_hMasterWnd );
	ASSERT( g_Proc.m_bIsProcessIgnored );
	ASSERT( g_Proc.CheckProcessMaster());

}

HRESULT CTaksiShared::LogMessage( const TCHAR* pszPrefix )
{
	// Log a common shared message for the dll, not the process.
	// LOG_NAME_DLL
	if ( ! sg_Config.m_bDebugLog )
	{
		return S_OK;
	}

	TCHAR szMsg[ _MAX_PATH + 64 ];
	int iLenMsg = _sntprintf( szMsg, COUNTOF(szMsg)-1, 
		_T("%s:%s"), 
		pszPrefix, g_Proc.m_szProcessTitleNoExt ); 

	// determine logfile full path
	TCHAR szLogFile[_MAX_PATH];	// DLL common, NOT for each process. LOG_NAME_DLL
	Str_MakeFilePath( szLogFile, COUNTOF(szLogFile), m_szIniDir, LOG_NAME_DLL );

	CNTHandle File( ::CreateFile( szLogFile,       // file to open/create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL ));                        // no attr. template 
	if ( !File.IsValidHandle()) 
	{
		return HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_TOO_MANY_OPEN_FILES));
	}

	DWORD dwBytesWritten;
	::SetFilePointer(File, 0, NULL, FILE_END);
	::WriteFile(File, (LPVOID)szMsg, iLenMsg, &dwBytesWritten, NULL);
	::WriteFile(File, (LPVOID)"\r\n", 2, &dwBytesWritten, NULL);
	return dwBytesWritten;
}

void CTaksiShared::UpdateConfigCustom()
{
	DEBUG_MSG(( "CTaksiShared::UpdateConfigCustom" LOG_CR ));
	// Increase the reconf-counter thus telling all the mapped DLLs that
	// they need to reconfigure themselves. (and check m_Config)
	m_dwConfigChangeCount++;
}

void CTaksiShared::SendReHookMessage()
{
	// My window went away, i must rehook to get a new window.
	// ASSUME: i was the current hook. then we probably want to re-hook some other app.
	// if .exe is still running, tell it to re-install the CBT hook

	if ( m_bMasterExiting )
		return;
	if ( m_hMasterWnd == NULL )
		return;
	if ( ! ::PostMessage( m_hMasterWnd, WM_APP_REHOOKCBT, 0, 0 ))
	{
		LOG_WARN(( "Post message for Master to re-hook CBT FAILED." LOG_CR ));
	}
	else
	{
		LOG_MSG(( "Post message for Master to re-hook CBT." LOG_CR ));
	}
}

void CTaksiShared::HotKey_ConfigOpen()
{
	ASSERT(sg_Shared.m_hMasterWnd);
	PostMessage( sg_Shared.m_hMasterWnd, WM_COMMAND, IDC_HOTKEY_FIRST + TAKSI_HOTKEY_ConfigOpen, 0 );
}
void CTaksiShared::HotKey_IndicatorToggle()
{
	sg_Config.m_bShowIndicator = !sg_Config.m_bShowIndicator;
	UpdateMaster();
}
void CTaksiShared::HotKey_HookModeToggle()
{
	// don't switch during video capture
	if ( g_AVIFile.IsOpen())
		return;
	if ( sg_Shared.IsHookCBT())
	{
		LOG_MSG(( "HotKey_HookModeToggle unhook CBT." LOG_CR));
		if ( sg_Shared.HookCBT_Uninstall())
		{
			LOG_MSG(( "CTaksiHotKeys::DoHotKey:CBT unhooked." LOG_CR));
			// change indicator to "green"
		}
	}
	else
	{
		LOG_MSG(( "HotKey_HookModeToggle install CBT hook." LOG_CR));
		sg_Shared.HookCBT_Install();
	}
}

//**************************************************************************************

bool CTaksiShared::InitMasterWnd(HWND hWnd)
{
	// The Master TAKSI.EXE App just started.
	ASSERT(hWnd);
	m_hMasterWnd = hWnd;
	m_iMasterUpdateCount = 0;

	// Install the hook
	return HookCBT_Install();
}

void CTaksiShared::DestroyShared()
{
	// Master App Taksi.EXE is exiting. so close up shop.
	// Set flag, so that TaksiDll knows to unhook device methods
	DEBUG_TRACE(( "CTaksiShared::DestroyShared" LOG_CR ));
	// Signal to unhook from IDirect3DDeviceN methods
	m_bMasterExiting = true;

	// Uninstall the hook
	HookCBT_Uninstall();
}

bool CTaksiShared::InitShared()
{
	// Call this only once the first time.
	// determine my file name 
	ASSERT(g_hInst);
	DEBUG_MSG(( "CTaksiShared::InitShared" LOG_CR ));

	m_dwVersionStamp = TAKSI_VERSION_N;
	m_hMasterWnd = NULL;
	m_iMasterUpdateCount = 0;
	m_bMasterExiting = false;
	m_hHookCBT = NULL;
	m_iProcessCount = 0;	// how many processes attached?
	m_dwHotKeyMask = 0;
	m_bRecordPause = false;

#ifdef USE_DIRECTX8
	m_nDX8_Present = 0; // offset from start of DLL to the interface element i want.
	m_nDX8_Reset = 0;
#endif
#ifdef USE_DIRECTX9
	m_nDX9_Present = 0;
	m_nDX9_Reset = 0;
#endif

	// determine my directory 
	m_szIniDir[0] = '\0';
	DWORD dwLen = GetCurrentDirectory( sizeof(m_szIniDir)-1, m_szIniDir );
	if ( dwLen <= 0 )
	{
		dwLen = GetModuleFileName( g_hInst, m_szIniDir, sizeof(m_szIniDir)-1 );
		if ( dwLen > 0 )
		{
			TCHAR* pszTitle = GetFileTitlePtr(m_szIniDir);
			ASSERT(pszTitle);
			*pszTitle = '\0';
		}
	}
	if ( ! FILE_IsDirSep( m_szIniDir[dwLen-1] ))
	{
		m_szIniDir[dwLen++] = '\\';
		m_szIniDir[dwLen] = '\0';
	}

	// read optional configuration file. global init.
	m_dwConfigChangeCount = 0;	// changed when the Custom stuff in m_Config changes.

	// ASSUME InitMasterWnd will be called shortly.
	return true;
}

//**************************************************************************************

void CTaksiProcess::UpdateStat( TAKSI_PROCSTAT_TYPE eProp )
{
	// We just updated a m_Stats.X
	m_Stats.UpdateProcStat(eProp);
	if ( IsProcPrime())
	{
		// Dupe the data for global display.
		sg_ProcStats.CopyProcStat( m_Stats, eProp );
	}
	if ( eProp == TAKSI_PROCSTAT_LastError )
	{
		LOG_MSG(( "Last Error: %s" LOG_CR, m_Stats.m_szLastError ));
	}
}

int CTaksiProcess::MakeFileName( TCHAR* pszFileName, const TCHAR* pszExt )
{
	// pszExt = "avi" or "bmp"
	// ASSUME sizeof(pszFileName) >= _MAX_PATH

	SYSTEMTIME time;
	::GetLocalTime(&time);

	int iLen = _sntprintf( pszFileName, _MAX_PATH-1,
		_T("%s%s-%d%02d%02d-%02d%02d%02d%01d%s.%s"), 
		sg_Config.m_szCaptureDir, m_szProcessTitleNoExt, 
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond,
		time.wMilliseconds/100,	// uniqueness to tenths of a sec. 
		sg_Config.m_szFileNamePostfix,
		pszExt );

	return iLen;
}

bool CTaksiProcess::CheckProcessMaster() const
{
	// the master EXE process ?
	// TAKSI.EXE has special status!
	// ASSUME m_bIsProcessIgnored
	return( ! lstrcmpi( m_szProcessTitleNoExt, _T("taksi")));
}

bool CTaksiProcess::CheckProcessIgnored() const
{
	// This functions should be called when DLL is mapped into an application
	// to check if this is one of the special apps, that we shouldn't do any
	// graphics API hooking.
	// sets m_bIsProcessIgnored

	if ( CheckProcessMaster())
	{
		return true;
	}

	static const TCHAR* sm_SpecialNames[] = 
	{
		_T("dbgmon"),	// debugger!
		_T("devenv"),	// debugger!
		_T("dwwin"),	// debugger! crash
		_T("js7jit"),	// debugger!
		_T("monitor"),	// debugger!
		_T("taskmgr"),	// debugger!
		_T("dbgview"),	// debugger! SysInternals DebugView
	};

	for ( int i=0; i<COUNTOF(sm_SpecialNames); i++ )
	{
		if (!lstrcmpi( m_szProcessTitleNoExt, sm_SpecialNames[i] ))
			return true;
	}

	if ( ! ::IsWindow( sg_Shared.m_hMasterWnd ) && ! sg_Shared.m_bMasterExiting )
	{
		// The master app is gone! this is bad! This shouldnt really happen
		LOG_MSG(( "Taksi.EXE App is NOT Loaded! unload dll (0x%x)" LOG_CR, sg_Shared.m_hMasterWnd ));
		sg_Shared.m_hMasterWnd = NULL;
		sg_Shared.m_bMasterExiting = true;
		return true;
	}

	return false;
}

void CTaksiProcess::CheckProcessCustom()
{
	// We have found a new process or config has changed
	// determine frame capturing algorithm special for the process.

	m_dwConfigChangeCount = sg_Shared.m_dwConfigChangeCount;

	// free the old custom config, if one existed
	if (m_pCustomConfig)
	{
		CTaksiConfig::CustomConfig_Delete(m_pCustomConfig);
		m_pCustomConfig = NULL;
	}

	// Ignored process.
	m_bIsProcessIgnored = CheckProcessIgnored();
	if ( m_bIsProcessIgnored )
	{
		return;
	}

	// Desktop process?
	TCHAR szExplorer[_MAX_PATH];
	UINT uLen = ::GetWindowsDirectory( szExplorer, sizeof(szExplorer));
	if ( uLen )
	{
		ASSERT( uLen < COUNTOF(szExplorer));
		lstrcpyn( szExplorer+uLen, _T("\\explorer.exe"), COUNTOF(szExplorer) - uLen );
		m_bIsProcessDesktop = !lstrcmpi( m_Stats.m_szProcessPath, szExplorer);
	}
	if ( m_bIsProcessDesktop )
	{
		// Check if it's Windows Explorer. We don't want to hook it either.
		// sg_Config.m_abUseGAPI[TAKSI_GAPI_DESKTOP]
		m_bIsProcessIgnored = true;
		return;
	}

	// Re-read the configuration file and try to match any custom config with the
	// specified pszProcessFile. If successfull, return true.
	// Parameter: [in out] ppCfg  - gets assigned to the pointer to new custom config.
	// NOTE: this function must be called from within the DLL - not the main Taksi app.
	//

	CTaksiConfig config;
	if ( config.ReadIniFile()) 
	{
		CTaksiConfigCustom* pCfgMatch = config.CustomConfig_FindPattern(m_Stats.m_szProcessPath);
		if ( pCfgMatch )
		{
			LOG_MSG(( "Using custom FrameRate=%g FPS, FrameWeight=%0.4f" LOG_CR, 
				pCfgMatch->m_fFrameRate, pCfgMatch->m_fFrameWeight ));

			// ignore this app?
			m_bIsProcessIgnored = ( pCfgMatch->m_fFrameRate <= 0 || pCfgMatch->m_fFrameWeight <= 0 );
			if ( m_bIsProcessIgnored )
			{
				LOG_MSG(( "FindCustomConfig: 0 framerate" LOG_CR));
				return;
			}

			// make a copy of custom config object
			m_pCustomConfig = config.CustomConfig_Alloc();
			if (!m_pCustomConfig)
			{
				LOG_WARN(( "FindCustomConfig: FAILED to allocate new custom config" LOG_CR));
				return;
			}
			*m_pCustomConfig = *pCfgMatch; // copy contents.
			m_pCustomConfig->m_pNext = NULL;
			return;
		}
	}

	LOG_MSG(( "CheckProcessCustom: No custom config match." LOG_CR ));
}

bool CTaksiProcess::StartGAPI( TAKSI_GAPI_TYPE eGAPI )
{
	// PresentFrameBegin() was called for this API.
	// This API/mode has successfully attached

	if ( m_Stats.m_eGAPI == eGAPI )	// its already the primary eGAPI
	{
		return true;
	}
	if ( eGAPI < m_Stats.m_eGAPI )	// lower priority than the current eGAPI.
	{
		return false;
	}
	ASSERT( eGAPI > TAKSI_GAPI_NONE && eGAPI < TAKSI_GAPI_QTY );

	// Unhook any lower priority types.
	for ( int i=m_Stats.m_eGAPI; i<eGAPI; i++ )
	{
		if ( s_aGAPIs[i] == NULL )
			continue;
		s_aGAPIs[i]->FreeDll();
	}

	m_Stats.m_eGAPI = eGAPI;
	UpdateStat( TAKSI_PROCSTAT_GAPI );
	return true;
}

HRESULT CTaksiProcess::AttachGAPIs( HWND hWnd )
{
	// see if any of supported graphics API DLLs are already loaded. (and can be hooked)
	// ARGS:
	//  hWnd = the window that is getting focus now.
	//  NULL = the Process just attached.
	// NOTE: 
	//  Not truly attached til PresentFrameBegin() is called.
	//  We may already be attached to some window.
	//  ONLY CALLED FROM: HookCBTProc()
	// TODO:
	//  Graphics modes should usurp GDI

	if ( m_bIsProcessIgnored )	// Dont hook special apps like My EXE or Explorer.
		return S_FALSE;
	if ( hWnd == NULL )	// losing focus i guess. ignore that.
		return S_FALSE;

	// Allow changing windows inside the same process.???
	m_hWndHookTry = FindWindowTop(hWnd);	// top parent. not WS_CHILD.
	if ( m_hWndHookTry == NULL )
	{
		return S_FALSE;
	}

	// Checks whether an application uses any of supported APIs (D3D8, D3D9, OpenGL).
	// If so, their corresponding buffer-swapping/Present routines are hooked. 
	// NOTE: We can only use ONE!
	LOG_MSG(( "AttachGAPIs: hWnd=0x%x" LOG_CR, m_hWndHookTry ));

	HRESULT hRes = S_FALSE;
	for ( int i=TAKSI_GAPI_NONE+1; i<COUNTOF(s_aGAPIs); i++ )
	{
		if ( s_aGAPIs[i] == NULL )
			continue;
		hRes = s_aGAPIs[i]->AttachGAPI();
	}
	return hRes;
}

void CTaksiProcess::StopGAPIs()
{
	// this can be called in the PresentFrameEnd
	g_AVIThread.StopAVIThread();	// kill my work thread, i'm done
	g_AVIFile.CloseAVI();	// close AVI file, if we were in recording mode 
	g_HotKeys.DetachHotKeys();
	m_hWndHookTry = NULL;	// Not trying to do anything anymore.
#ifdef USE_GDIP
	g_gdiplus.DetachGDIPInt();
#endif
}

void CTaksiProcess::DetachGAPIs()
{
	// we are unloading or some other app now has the main focus/hook.
	StopGAPIs();

	// give graphics module a chance to clean up.
	for ( int i=TAKSI_GAPI_NONE+1; i<COUNTOF(s_aGAPIs); i++ )
	{
		if ( s_aGAPIs[i] == NULL )
			continue;
		s_aGAPIs[i]->FreeDll();
	}

	m_Stats.m_eGAPI = TAKSI_GAPI_NONE;
	UpdateStat( TAKSI_PROCSTAT_GAPI );
}

bool CTaksiProcess::OnDllProcessAttach()
{
	// DLL_PROCESS_ATTACH
	// We have attached to a new process. via the CBT most likely.
	// This is called before anything else.
	// NOTE: HookCBTProc is probably already active and could be called at any time!

	// Get Name of the process the DLL is attaching to
	if ( ! ::GetModuleFileName(NULL, m_Stats.m_szProcessPath, sizeof(m_Stats.m_szProcessPath)))
	{
		m_Stats.m_szProcessPath[0] = '\0';
	}
	else
	{
		_tcslwr(m_Stats.m_szProcessPath);
	}

	// determine process full path 
	const TCHAR* pszTitle = GetFileTitlePtr(m_Stats.m_szProcessPath);
	ASSERT(pszTitle);

	// save short filename without ".exe" extension.
	const TCHAR* pExt = pszTitle + lstrlen(pszTitle) - 4;
	if ( !lstrcmpi(pExt, _T(".exe"))) 
	{
		int iLen = pExt - pszTitle;
		lstrcpyn( m_szProcessTitleNoExt, pszTitle, iLen+1 ); 
		m_szProcessTitleNoExt[iLen] = '\0';
	}
	else 
	{
		lstrcpy( m_szProcessTitleNoExt, pszTitle );
	}

	bool bProcMaster = m_bIsProcessIgnored = CheckProcessMaster();
	if ( bProcMaster )
	{
		// First time here!
		if ( ! sg_Shared.InitShared())
		{
			DEBUG_ERR(( "InitShared FAILED!" LOG_CR ));
			return false;
		}
		sg_ProcStats.InitProcStats();
		sg_Config.InitConfig();		// Read from the INI file shortly.
	}
	else
	{
		if ( sg_Shared.m_dwVersionStamp != TAKSI_VERSION_N )	// this is weird!
		{
			DEBUG_ERR(( "InitShared BAD VERSION 0%x != 0%x" LOG_CR, sg_Shared.m_dwVersionStamp, TAKSI_VERSION_N ));
			return false;
		}
	}

#ifdef _DEBUG
	CTaksiShared* pDll = &sg_Shared;
	CTaksiConfigData* pConfig = &sg_Config;
#endif

	sg_Shared.m_iProcessCount ++;
	LOG_MSG(( "DLL_PROCESS_ATTACH '%s' v" TAKSI_VERSION_S " (num=%d)" LOG_CR, pszTitle, sg_Shared.m_iProcessCount ));

	// determine process handle that is loading this DLL. 
	m_Stats.m_dwProcessId = ::GetCurrentProcessId();
	m_hWndHookTry = NULL;

	// save handle to process' heap
	m_hHeap = ::GetProcessHeap();

	// do not hook on selected applications. set m_bIsProcessIgnored
	if ( ! bProcMaster )
	{
		// (such as: myself, Explorer.EXE)
		CheckProcessCustom();	// determine frame capturing algorithm 
		if ( m_bIsProcessIgnored && ! bProcMaster )
		{
			LOG_MSG(( "Special process ignored." LOG_CR ));
			return true;
		}
	}

	g_FrameRate.InitFreqUnits();

	// log information on which process mapped the DLL
	sg_Shared.LogMessage( _T("mapped: "));

#ifdef USE_LOGFILE
	// open log file, specific for this process
	if ( sg_Config.m_bDebugLog )
	{
		TCHAR szLogName[ _MAX_PATH ];
		int iLen = _sntprintf( szLogName, COUNTOF(szLogName)-1, 
			_T("%sTaksi_%s.log"), 
			sg_Shared.m_szIniDir, m_szProcessTitleNoExt ); 
		HRESULT hRes = g_Log.OpenLogFile(szLogName);
		if ( IS_ERROR(hRes))
		{
			LOG_MSG(( "Log start FAIL. 0x%x" LOG_CR, hRes ));
		}
		else
		{
			DEBUG_TRACE(( "Log started." LOG_CR));
		}
	}
#endif

	if ( ! bProcMaster )	// not read INI yet.
	{
		DEBUG_TRACE(( "sg_Config.m_bDebugLog=%d" LOG_CR, sg_Config.m_bDebugLog));
		DEBUG_TRACE(( "sg_Config.m_bUseDirectInput=%d" LOG_CR, sg_Config.m_bUseDirectInput));
		DEBUG_TRACE(( "sg_Shared.m_hHookCBT=%d" LOG_CR, (UINT_PTR)sg_Shared.m_hHookCBT));
		// DEBUG_TRACE(( "sg_Config.m_bUseGDI=%d" LOG_CR, sg_Config.m_abUseGAPI[TAKSI_GAPI_GDI]));
	}

	// ASSUME HookCBTProc will call AttachGAPIs later
	return true;
}

bool CTaksiProcess::OnDllProcessDetach()
{
	// DLL_PROCESS_DETACH
	LOG_MSG(( "DLL_PROCESS_DETACH (num=%d)" LOG_CR, sg_Shared.m_iProcessCount ));

	DetachGAPIs();

	// uninstall keyboard hook. was just for this process anyhow.
	g_UserKeyboard.UninstallHookKeys();

	// uninstall system-wide hook. then re-install it later if i want.
	// some process is detaching. assume this is NOT the master process.
	if ( IsProcPrime())
	{
		sg_ProcStats.InitProcStats();	// not prime anymore!
		sg_Shared.HookCBT_Uninstall();
		sg_Shared.SendReHookMessage();
	}

	if (m_pCustomConfig)
	{
		CTaksiConfig::CustomConfig_Delete(m_pCustomConfig);
		m_pCustomConfig = NULL;
	}

	// close specific log file 
	g_Log.CloseLogFile();

	// log information on which process unmapped the DLL 
	sg_Shared.LogMessage( _T("unmapped: "));
	sg_Shared.m_iProcessCount --;
	return true;
}

//**************************************************************************************

BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved )
{
	// DLL Entry Point 
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hInst = hInstance;
		ZeroMemory(&g_Proc, sizeof(g_Proc));
		return g_Proc.OnDllProcessAttach();
	case DLL_PROCESS_DETACH:
		return g_Proc.OnDllProcessDetach();
	}
	return true;    // ok
}
