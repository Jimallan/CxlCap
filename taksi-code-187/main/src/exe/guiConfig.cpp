//
// guiConfig.cpp
//
#include "../stdafx.h"
#include "Taksi.h"
#include "resource.h"
#include "gui.h"
#include "guiConfig.h"
#include <windowsx.h>
#include <commctrl.h>	// HKM_GETHOTKEY
#include "../common/CFileDirDlg.h"
#include "../common/CWaveDevice.h"

#define UPDATE_EDIT_LIMIT(c,i,p)	SendMessage(m_hControl##i, CB_LIMITTEXT, sizeof(((c*)0)->p)-1, 0)
#define UPDATE_CHECK(i,v)			SendMessage(m_hControl##i, BM_SETCHECK, (v)? BST_CHECKED : BST_UNCHECKED, 0 )

CGuiConfig g_GUIConfig;

CGuiConfig::CGuiConfig()
	: m_bDataUpdating(false)
	, m_iTabCur(0)
{
}

void CGuiConfig::SetStatusText( const TCHAR* pszText )
{
	TCHAR szTmp[ _MAX_PATH ];
	SetDlgItemText( m_hWnd, IDC_C_StatusText, CheckIntResource(pszText,szTmp));
}

void CGuiConfig::SetSaveState( bool bChanged )
{
	EnableWindow( GetDlgItem(IDC_C_SaveButton), bChanged );
	EnableWindow( GetDlgItem(IDC_C_RestoreButton), bChanged );
	sg_Config.CopyConfig( g_Config );
}

void CGuiConfig::OnChanges()
{
	SetStatusText( MAKEINTRESOURCE(IDS_STATUS_CHANGES));
	SetSaveState( true );
	sg_Shared.UpdateMaster();
}

//******************************************************

CTaksiConfigCustom* CGuiConfig::Custom_FindSelect( int index ) const
{
	// ASSUME CB_LIMITTEXT _MAX_PATH
	if ( index < 0 ) // CB_ERR
		return NULL;
	TCHAR szTmp[ _MAX_PATH ];
	szTmp[0] = '\0';
	LRESULT iLen = ::SendMessage(m_hControlCustomSettingsList, CB_GETLBTEXT, index, (LPARAM)szTmp);
	if ( iLen <= 0 )
		return NULL;
	return g_Config.CustomConfig_FindAppId(szTmp);
}

void CGuiConfig::Custom_Update( CTaksiConfigCustom* pCfg )
{
	if ( pCfg == NULL )
	{
		SetWindowText( m_hControlCustomPattern, _T(""));
		SetWindowText( m_hControlCustomFrameRate, _T(""));
		SetWindowText( m_hControlCustomFrameWeight, _T(""));
		return;
	}

	SetWindowText( m_hControlCustomPattern,pCfg->m_szPattern);

	TCHAR szTmp[_MAX_PATH];
	// TAKSI_CUSTOM_FrameRate 
	_sntprintf(szTmp, COUNTOF(szTmp), _T("%g"), pCfg->m_fFrameRate );
	SetWindowText( m_hControlCustomFrameRate,szTmp);

	// TAKSI_CUSTOM_FrameWeight
	_sntprintf(szTmp, COUNTOF(szTmp), _T("%g"), pCfg->m_fFrameWeight );
	SetWindowText( m_hControlCustomFrameWeight, szTmp );
}

void CGuiConfig::Custom_Read()
{
	// read users input from the dialog.
	if (m_pCustomCur == NULL)
		return;
	GetWindowText( m_hControlCustomPattern,
		m_pCustomCur->m_szPattern, sizeof(m_pCustomCur->m_szPattern));
	_tcslwr(m_pCustomCur->m_szPattern);

	TCHAR szTmp[_MAX_PATH];
	// TAKSI_CUSTOM_FrameRate
	szTmp[0] = '\0';
	GetWindowText( m_hControlCustomFrameRate,
		szTmp, sizeof(szTmp));
	m_pCustomCur->m_fFrameRate = (float) _tstof(szTmp);

	// TAKSI_CUSTOM_FrameWeight:
	szTmp[0] = '\0';
	GetWindowText( m_hControlCustomFrameWeight,
		szTmp, sizeof(szTmp));
	m_pCustomCur->m_fFrameWeight = (float) _tstof(szTmp);
}

bool CGuiConfig::Custom_ReadHook( CTaksiConfigCustom* pCfg )
{
	ASSERT(pCfg);
	if ( sg_ProcStats.m_szProcessPath[0] == '\0' )
		return false;
	lstrcpyn( pCfg->m_szPattern, GetFileTitlePtr(sg_ProcStats.m_szProcessPath),
		COUNTOF(pCfg->m_szPattern));

	pCfg->m_fFrameWeight = 0;
	pCfg->m_fFrameRate = sg_ProcStats.m_fFrameRate ? sg_ProcStats.m_fFrameRate : sg_Config.m_fFrameRateTarget;
	return true;
}

bool CGuiConfig::Custom_ReadHook()
{
	// Use the current hooked app as the default if there is one.
	if ( sg_ProcStats.m_szProcessPath[0] == '\0' )
		return false;
	TCHAR szTitle[ _MAX_PATH ];
	int iLen = GetWindowText( sg_ProcStats.m_hWndCap, szTitle, COUNTOF(szTitle));
	if ( iLen <= 0 )
		return false;
	m_pCustomCur = g_Config.CustomConfig_Lookup(szTitle);
	return Custom_ReadHook(m_pCustomCur);
}

void CGuiConfig::Custom_Init( CTaksiConfigCustom* pCustom )
{
	m_pCustomCur = NULL;
	SendMessage(m_hControlCustomSettingsList, CB_RESETCONTENT, 0, 0);

	while (pCustom != NULL)
	{
		SendMessage(m_hControlCustomSettingsList,CB_INSERTSTRING,(WPARAM)0, (LPARAM)pCustom->m_szAppId);

		if (pCustom->m_pNext == NULL)
		{
			// remember current config
			SendMessage(m_hControlCustomSettingsList,CB_SETCURSEL,(WPARAM)0,(LPARAM)0);
			m_pCustomCur = pCustom;
			break;
		}

		pCustom = pCustom->m_pNext;
	}

	// clear status text
	Custom_Update(m_pCustomCur);
	SetStatusText( _T(""));
}

//******************************************************

void CGuiConfig::UpdateProcStats( const CTaksiProcStats& stats, DWORD dwMask )
{
	// Periodically update  this if it has changed.
	// dwMask = mask of TAKSI_PROCSTAT_TYPE 
	TCHAR szTmp[_MAX_PATH];

	if ( dwMask & (1<<TAKSI_PROCSTAT_ProcessFile))
	{
		SetWindowText( m_hControlStatProcessName, stats.m_szProcessPath );
	}
	if ( dwMask & (1<<TAKSI_PROCSTAT_LastError))
	{
		SetWindowText( m_hControlStatLastError, stats.m_szLastError );
	}
	if ( dwMask & (1<<TAKSI_PROCSTAT_SizeWnd))
	{
		_sntprintf( szTmp, COUNTOF(szTmp), _TEXT("%d x %d"), stats.m_SizeWnd.cx, stats.m_SizeWnd.cy  );
		SetWindowText( m_hControlStatFormat, szTmp );
	}
	if ( dwMask & (1<<TAKSI_PROCSTAT_GAPI))
	{
		const TCHAR* sm_szNames[TAKSI_GAPI_QTY+1] = 
		{
		_T(""),
		_T("Desktop"),
		_T("GDI"),		// TAKSI_GAPI_GDI
		_T("OpenGL"),	// TAKSI_GAPI_OGL
#ifdef USE_DIRECTX8
		_T("DirectX8"), // TAKSI_GAPI_DX8
#endif
#ifdef USE_DIRECTX9
		_T("DirectX9"), // TAKSI_GAPI_DX9
#endif
		_T(""),
		};
		int i = stats.m_eGAPI;
		if ( i < 0 || i >= COUNTOF(sm_szNames))
			i = COUNTOF(sm_szNames)-1;
		SetWindowText( m_hControlStatGAPI, sm_szNames[i] );
	}
	if ( dwMask & (1<<TAKSI_PROCSTAT_State))
	{
		//TAKSI_INDICATE_TYPE
		int id = stats.m_eState;
		if ( id < 0 || id >= TAKSI_INDICATE_QTY )
			id = TAKSI_INDICATE_QTY;
		LoadString( g_hInst, id + IDS_STATE_Idle, szTmp, sizeof(szTmp)-1 );
		SetWindowText( m_hControlStatState, szTmp );
	}
	if ( dwMask & (1<<TAKSI_PROCSTAT_FrameRate))
	{
		_sntprintf( szTmp, COUNTOF(szTmp), _TEXT("%g"), (float) stats.m_fFrameRate );
		SetWindowText( m_hControlStatFrameRate, szTmp );
	}
	if ( dwMask & (1<<TAKSI_PROCSTAT_DataRecorded))
	{
		_sntprintf( szTmp, COUNTOF(szTmp), _TEXT("%g"), (float) stats.get_DataRecMeg());
		SetWindowText( m_hControlStatDataRecorded, szTmp );
	}
}

void CGuiConfig::UpdateVideoCodec( const CVideoCodec& codec, bool bMessageBox )
{
	// Assume the video codec has changed
	ICINFO info;
	if ( ! codec.GetCodecInfo(info))
	{
		SetWindowText( m_hControlVideoCodec, _T("<NONE>"));
	}
	else
	{
		TCHAR szTmp[_MAX_PATH];
		_sntprintf( szTmp, COUNTOF(szTmp), _TEXT("%S - %S"), info.szName, info.szDescription );
		SetWindowText( m_hControlVideoCodec, szTmp );
	}
	if ( bMessageBox )
	{
		CheckVideoCodec( m_hWnd, info );
	}
}

bool CGuiConfig::UpdateAudioDevices( int iAudioDevice )
{
	// IDC_C_AudioDevice m_hControlAudioDevices
	// List the audio input devices.

	int uQty = waveInGetNumDevs();
	int uQtyCnt = uQty + 2;
	int uDeviceID = WAVE_DEVICE_NONE;

	for ( ; uQtyCnt ; uDeviceID ++, uQtyCnt -- )
	{
		CWaveRecorder rec;
		const TCHAR* pszName;
		switch (uDeviceID)
		{
		case WAVE_DEVICE_NONE:
			pszName = _T("None");
			break;
		case WAVE_MAPPER:
			pszName = _T("Default");
			break;
		default:
			{
			if ( rec.put_DeviceID( uDeviceID ) != S_OK ) // MMRESULT
				pszName = _T("BAD DEVICE");
			else 
				pszName = rec.m_Caps.szPname;
			}
		}
		ComboBox_AddString( m_hControlAudioDevices, pszName );
	}

	ComboBox_SetCurSel( m_hControlAudioDevices, iAudioDevice - WAVE_DEVICE_NONE );
	return true;
}

bool CGuiConfig::UpdateAudioCodec( const CWaveFormat& codec )
{
	// ACM dialog to list formats and codecs
	ACMFORMATTAGDETAILS details;
	MMRESULT mmRet = CWaveACMInit::I().GetFormatDetails( codec, details );
	if ( mmRet || details.szFormatTag[0] == '\0' )
	{
		SetWindowText( m_hControlAudioCodec, _T("<NONE>"));
		return false;
	}
	SetWindowText( m_hControlAudioCodec, details.szFormatTag );
	return true;
}

void CGuiConfig::UpdateSettings( const CTaksiConfig& config )
{
	// display settings from config to the dialog
	m_bDataUpdating = true;

	// Dest Dir
	SetWindowText( m_hControlCaptureDirectory, config.m_szCaptureDir );
	UPDATE_CHECK(DebugLog,config.m_bDebugLog);
	SetWindowText( m_hControlFileNamePostfix, config.m_szFileNamePostfix );

	// Format
	char szTmp[_MAX_PATH];
	config.PropGet( TAKSI_CFGPROP_MovieFrameRateTarget, szTmp, sizeof(szTmp));
#ifdef _UNICODE
	ASSERT(0);
#else
	SetWindowText( m_hControlFrameRate, szTmp);
#endif

	UpdateVideoCodec( config.m_VideoCodec, false );
	UPDATE_CHECK(VideoHalfSize,config.m_bVideoHalfSize);

	UpdateAudioCodec( config.m_AudioFormat );
	UpdateAudioDevices( config.m_iAudioDevice );

	// hot keys
	HWND* pControlHotKey = &m_hControlKeyConfigOpen;
	for ( int i=0; i<TAKSI_HOTKEY_QTY; i++ )
	{
		SendMessage( pControlHotKey[i], 
			HKM_SETHOTKEY, config.GetHotKey((TAKSI_HOTKEY_TYPE)i), 0 );
	}

	UPDATE_CHECK(UseDirectInput,config.m_bUseDirectInput);

	// custom configs
	Custom_Init(config.m_pCustomList);

	// Display options
	UPDATE_CHECK(GDIFrame,config.m_bGDIFrame);
	UPDATE_CHECK(GDICursor,config.m_bGDICursor);
	UPDATE_CHECK(MasterTopMost,config.m_bMasterTopMost);
	UPDATE_CHECK(UseOverheadCompensation,config.m_bUseOverheadCompensation);

	UPDATE_CHECK(GDIDesktop,config.m_abUseGAPI[TAKSI_GAPI_DESKTOP]);
	UPDATE_CHECK(UseGDI,config.m_abUseGAPI[TAKSI_GAPI_GDI]);
	UPDATE_CHECK(UseOGL,config.m_abUseGAPI[TAKSI_GAPI_OGL]);
#ifdef USE_DIRECTX8
	UPDATE_CHECK(UseDX8,config.m_abUseGAPI[TAKSI_GAPI_DX8]);
#endif
#ifdef USE_DIRECTX9
	UPDATE_CHECK(UseDX9,config.m_abUseGAPI[TAKSI_GAPI_DX9]);
#endif

	m_bDataUpdating = false;
}

void CGuiConfig::UpdateGuiTabCur()
{
	ASSERT( m_iTabCur >= 0 && m_iTabCur < COUNTOF(m_hWndTab));
	for ( int i=0; i<COUNTOF(m_hWndTab); i++ )
	{
		ASSERT(m_hWndTab[i]);
		::ShowWindow( m_hWndTab[i], ( i == m_iTabCur ) ? SW_NORMAL : SW_HIDE );
	}
}

//**************************************************************

void CGuiConfig::OnCommandRestore(void)
{
	// Restores last saved settings.
	m_pCustomCur = NULL;

	// read optional configuration file
	if ( ! g_Config.ReadIniFile())
	{
		// this is ok since we can just take all defaults.
		g_Config.InitConfig();
	}
	
	UpdateSettings(g_Config);	// push out to the dialog.
	sg_Config.CopyConfig(g_Config);
	sg_Shared.UpdateConfigCustom();	// indicate global changes have been made.

	// modify status text
	SetStatusText( MAKEINTRESOURCE(IDS_STATUS_RESTORED));
	SetSaveState( false );
}

void CGuiConfig::OnCommandSave()
{
	// update current custom config
	Custom_Read();

	int iLen = GetWindowText( m_hControlCaptureDirectory,
		g_Config.m_szCaptureDir, sizeof(g_Config.m_szCaptureDir));

	HRESULT hRes = g_Config.FixCaptureDir();
	if ( hRes != S_FALSE )	// changed?
	{
		m_bDataUpdating = true;
		SetWindowText( m_hControlCaptureDirectory, g_Config.m_szCaptureDir); 
		m_bDataUpdating = false;
	}

	iLen = GetWindowText( m_hControlFileNamePostfix,
		g_Config.m_szFileNamePostfix, sizeof(g_Config.m_szFileNamePostfix));

	// TAKSI_CFGPROP_MovieFrameRateTarget
	TCHAR szTmp[_MAX_PATH];
	GetWindowText( m_hControlFrameRate, szTmp, sizeof(szTmp));
	g_Config.m_fFrameRateTarget = (float) _tstof(szTmp);
	if ( g_Config.m_fFrameRateTarget <= 0 ) 
	{
		// default rate.
		g_Config.m_fFrameRateTarget = 10;
		_sntprintf( szTmp, COUNTOF(szTmp), _T("%g"), g_Config.m_fFrameRateTarget );
		SetWindowText( m_hControlFrameRate, szTmp); 
	}

	// Save to INI file
	if (g_Config.WriteIniFile())
	{
		// Apply new settings globally
		SetSaveState( false );
		sg_Shared.UpdateConfigCustom();	// make global change.
		SetStatusText( MAKEINTRESOURCE(IDS_STATUS_SAVED));
	}
	else
	{
		SetStatusText( MAKEINTRESOURCE(IDS_STATUS_FAILSAVE));
	}
}

void CGuiConfig::OnCommandCustomNewButton()
{
	// IDC_C_CustomNewButton
	// update current custom config
	Custom_Read();
	m_pCustomCur = NULL;

	// Use the current hooked app as the default if there is one.
	Custom_ReadHook();

	// clear out controls
	SetWindowText( m_hControlCustomSettingsList, m_pCustomCur ? (m_pCustomCur->m_szAppId) : _T("") );
	Custom_Update(m_pCustomCur);

	// set focus on appId comboBox
	SetFocus(m_hControlCustomSettingsList);
}

void CGuiConfig::OnCommandCustomDeleteButton()
{
	// remove current custom config
	if (m_pCustomCur == NULL) 
		return;
	int idx = (int)SendMessage(m_hControlCustomSettingsList, CB_FINDSTRING, 
		(WPARAM) sizeof(m_pCustomCur->m_szAppId),
		(LPARAM) m_pCustomCur->m_szAppId );

	if (idx != -1)
	{
		SendMessage(m_hControlCustomSettingsList, CB_DELETESTRING, idx, 0);
		g_Config.CustomConfig_DeleteAppId(m_pCustomCur->m_szAppId);
		OnChanges();
	}

	SendMessage(m_hControlCustomSettingsList, CB_SETCURSEL, 0, 0);
	m_pCustomCur = Custom_FindSelect(0);

	Custom_Update(m_pCustomCur);
}

void CGuiConfig::OnCommandCustomKillFocus()
{
	// IDC_C_CustomSettingsList
	// CBN_KILLFOCUS

	TCHAR szTmp[_MAX_PATH];
	szTmp[0] = '\0';
	int iLen = GetWindowText(m_hControlCustomSettingsList, szTmp, sizeof(szTmp));
	if ( iLen <= 0 )
	{
		// cannot use empty string. Restore the old one, if known.
		if (m_pCustomCur == NULL)
			return;
		// restore previous text
		SetWindowText( m_hControlCustomSettingsList, m_pCustomCur->m_szAppId );
		return;
	}
	if (m_pCustomCur != NULL)
	{
		if ( !lstrcmp(szTmp, m_pCustomCur->m_szAppId)) // no change
			return;
		// find list item with old appId
		LRESULT idx = SendMessage(m_hControlCustomSettingsList, CB_FINDSTRING,
			(WPARAM) sizeof(m_pCustomCur->m_szAppId),
			(LPARAM)(UINT_PTR) m_pCustomCur->m_szAppId );
		// delete this item, if found
		if (idx >= 0)
		{
			SendMessage(m_hControlCustomSettingsList, CB_DELETESTRING, idx, 0);
		}
		// change appId
		_tcsncpy( m_pCustomCur->m_szAppId, szTmp, COUNTOF(szTmp)-1);
		// add a new list item
		SendMessage(m_hControlCustomSettingsList, CB_ADDSTRING, 0, (LPARAM)m_pCustomCur->m_szAppId);
		OnChanges();
	}
	else
	{
		// we have a new custom config
		m_pCustomCur = g_Config.CustomConfig_Lookup(szTmp);
		// add a new list item
		SendMessage(m_hControlCustomSettingsList, CB_ADDSTRING, 0, (LPARAM)m_pCustomCur->m_szAppId);
	}
}

void CGuiConfig::OnCommandVideoCodecButton()
{
	CVideoCodec VideoPrev;
	VideoPrev.InitCodec();
	VideoPrev.CopyCodec( g_Config.m_VideoCodec );
	if ( ! g_Config.m_VideoCodec.CompChooseDlg( m_hWnd,
		"Choose video compressor" ))
	{
		return;
	}
	CVideoCodec VideoCur;
	VideoCur.InitCodec();
	VideoCur.CopyCodec( g_Config.m_VideoCodec );
#ifdef _DEBUG
	const char* pCodec = (const char*) &(VideoCur.m_v.fccHandler);
	DEBUG_MSG(( "VideoCodec %c,%c,%c,%c selected" LOG_CR, pCodec[0], pCodec[1], pCodec[2], pCodec[3] ));
#endif
	if ( memcmp( &VideoCur, &VideoPrev, sizeof(VideoCur)))
	{
		// Really changed
		g_Config.m_bVideoCodecMsg = false;
		UpdateVideoCodec( g_Config.m_VideoCodec, VideoCur.m_v.fccHandler != VideoPrev.m_v.fccHandler );
		OnChanges();
	}
}

void CGuiConfig::OnCommandAudioCodecButton()
{
	int iRet = CWaveACMInit::I().FormatDlg( m_hWnd, g_Config.m_AudioFormat,
		_T("Choose Audio Format"), 0 );
	if (iRet!=IDOK)
	{
		return;
	}
	// Really changed??
	UpdateAudioCodec( g_Config.m_AudioFormat );
	OnChanges();
	if ( !g_Config.m_AudioFormat.IsValidFormat() )
	{
		DlgHelp( g_GUIConfig, _T("This codec is not supported, audio will not be recorded.") );
	}
}

void CGuiConfig::OnCommandKeyChange( HWND hControl )
{
	// LOBYTE uHotKey = vKey
	// HIBYTE uHotKey = HOTKEYF_ALT etc.
	WORD wHotKey = (WORD) ::SendMessage( hControl, HKM_GETHOTKEY, 0, 0 );

	DEBUG_MSG(( "HotKey = 0%x" LOG_CR, wHotKey ));

	// update config
	TAKSI_HOTKEY_TYPE eHotKey = (TAKSI_HOTKEY_TYPE)( TAKSI_HOTKEY_ConfigOpen + GetDlgCtrlID(hControl)-IDC_C_KeyConfigOpen );
	if ( ! g_Config.SetHotKey( eHotKey, wHotKey ))
		return;
	OnChanges();
}

bool CGuiConfig::OnCommandCaptureBrowse()
{
	CFileDirDlg dlg( m_hWnd, g_Config.m_szCaptureDir );
	HRESULT hRes = dlg.DoModal( _T("In what directory do you want to place files?"));
	if ( hRes != S_OK )
		return false;
	if ( ! lstrcmpi( g_Config.m_szCaptureDir, dlg.m_szDir ))
		return false;
	lstrcpy( g_Config.m_szCaptureDir, dlg.m_szDir );
	hRes = g_Config.FixCaptureDir();
	if ( FAILED(hRes))
	{
		// Error message!
	}
	OnChanges();
	m_bDataUpdating = true;
	SetWindowText( m_hControlCaptureDirectory, g_Config.m_szCaptureDir); 
	m_bDataUpdating = false;
	return true;
}

//**************************************************************

bool CGuiConfig::OnHelp( LPHELPINFO pHelpInfo )
{
	// WM_HELP
	// Try to give some help on the control?
	ASSERT(pHelpInfo);
	DEBUG_MSG(( "WM_HELP on %d" LOG_CR, pHelpInfo->iCtrlId ));

	TCHAR szTmp[ _MAX_PATH ];
    int iLen = LoadString( g_hInst, pHelpInfo->iCtrlId, szTmp, sizeof(szTmp));
	if ( iLen <= 0 )
	{
		DlgHelp( g_GUIConfig, _T("TODO: No context help for this") );
		return true;
	}

	DlgHelp( m_hWnd, szTmp );
	return true;
}

bool CGuiConfig::OnTimer( UINT idTimer )
{
	// IDD_GuiConfigTab6
	if ( idTimer != IDT_UpdateStats )
	{
		return false;
	}

	if ( m_iTabCur == 3 )
	{
		EnableWindow( m_hControlCustomUseCurrent, sg_ProcStats.m_szProcessPath[0] != '\0' );
		return true;
	}
	if ( m_iTabCur == 5 )
	{
		// Check for stats update. only if this tab is set.
		if ( sg_ProcStats.m_dwPropChangedMask )
		{
			UpdateProcStats( sg_ProcStats, sg_ProcStats.m_dwPropChangedMask );
			sg_ProcStats.m_dwPropChangedMask = 0;
		}
		return true;
	}

	return false;
}

bool CGuiConfig::OnNotify( int id, NMHDR* pHead )
{
	// WM_NOTIFY:
	switch (id)
	{
	case IDC_C_TAB:
		// switch to this visible tab.
		m_iTabCur = TabCtrl_GetCurSel( m_hControlTab );
		if ( m_iTabCur < 0 )
			m_iTabCur = 0;
		UpdateGuiTabCur();
		return true;
	}
	return false;
}

bool CGuiConfig::OnCommandCheck( HWND hWndCtrl )
{
	bool bVal = ( SendMessage( hWndCtrl, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	OnChanges();
	return bVal;
}

bool CGuiConfig::OnCommand( int id, int iNotify, HWND hControl )
{
	// WM_COMMAND
	switch (id)
	{
	case IDC_C_SaveButton:
		OnCommandSave();
		return true;
	case IDC_C_RestoreButton:
		OnCommandRestore();
		return true;

	case IDC_C_DebugLog:
		sg_Config.m_bDebugLog = g_Config.m_bDebugLog = OnCommandCheck( m_hControlDebugLog );
		return true;
	case IDC_C_GDIFrame:
		sg_Config.m_bGDIFrame = g_Config.m_bGDIFrame = OnCommandCheck( m_hControlGDIFrame );
		return true;
	case IDC_C_MasterTopMost:
		sg_Config.m_bMasterTopMost = g_Config.m_bMasterTopMost = OnCommandCheck( m_hControlMasterTopMost );
		return true;
	case IDC_C_UseOverheadCompensation:
		sg_Config.m_bUseOverheadCompensation = g_Config.m_bUseOverheadCompensation = OnCommandCheck( m_hControlUseOverheadCompensation );
		return true;
	case IDC_C_GDICursor:
		sg_Config.m_bGDICursor = g_Config.m_bGDICursor = OnCommandCheck( m_hControlGDICursor );
		return true;

	case IDC_C_GDIDesktop:
		sg_Config.m_abUseGAPI[TAKSI_GAPI_DESKTOP] = g_Config.m_abUseGAPI[TAKSI_GAPI_DESKTOP] = OnCommandCheck( m_hControlGDIDesktop );
		return true;
	case IDC_C_UseGDI:
		sg_Config.m_abUseGAPI[TAKSI_GAPI_GDI] = g_Config.m_abUseGAPI[TAKSI_GAPI_GDI] = OnCommandCheck( m_hControlUseGDI );
		return true;
	case IDC_C_UseOGL:
		sg_Config.m_abUseGAPI[TAKSI_GAPI_OGL] = g_Config.m_abUseGAPI[TAKSI_GAPI_OGL] = OnCommandCheck( m_hControlUseOGL );
		return true;
#ifdef USE_DIRECTX8
	case IDC_C_UseDX8:
		sg_Config.m_abUseGAPI[TAKSI_GAPI_DX8] = g_Config.m_abUseGAPI[TAKSI_GAPI_DX8] = OnCommandCheck( m_hControlUseDX8 );
		return true;
#endif
#ifdef USE_DIRECTX9
	case IDC_C_UseDX9:
		sg_Config.m_abUseGAPI[TAKSI_GAPI_DX9] = g_Config.m_abUseGAPI[TAKSI_GAPI_DX9] = OnCommandCheck( m_hControlUseDX9 );
		return true;
#endif

	case IDC_C_CaptureDirectory:
		if ( iNotify == EN_CHANGE && !m_bDataUpdating )
		{
			int iLen = GetWindowText( m_hControlCaptureDirectory,
				g_Config.m_szCaptureDir, sizeof(g_Config.m_szCaptureDir));
			OnChanges();
		}
		break;

	case IDC_C_FileNamePostfix:
		if ( iNotify == EN_CHANGE && !m_bDataUpdating )
		{
			int iLen = GetWindowText( m_hControlFileNamePostfix,
				g_Config.m_szFileNamePostfix, sizeof(g_Config.m_szFileNamePostfix));
			OnChanges();
		}
		break;

	case IDC_C_CustomPattern:
	case IDC_C_CustomFrameRate:
	case IDC_C_CustomFrameWeight:
	case IDC_C_FrameRate:
		// ignore EN_UPDATE
		if ( iNotify == EN_CHANGE && !m_bDataUpdating )
		{
			// modify status text
			OnChanges();
		}
		break;

	case IDM_SC_HELP_URL:
		g_GUI.OnCommandHelpURL();
		break;
	case IDM_SC_HELP_ABOUT:
		g_GUI.OnCommandHelpAbout( m_hWnd );
		return true;

	case IDC_C_CaptureDirectoryExplore:
		{
		HINSTANCE hInstRet = ::ShellExecute( NULL, NULL, 
			_T("Explorer.exe"), 
			g_Config.m_szCaptureDir, g_Config.m_szCaptureDir,
			SW_SHOWNORMAL );
		}
		break;

	case IDC_C_CaptureDirectoryBrowse:
		OnCommandCaptureBrowse();
		return true;

	case IDC_C_VideoCodecButton:
		// Display codec-selector dialog
		OnCommandVideoCodecButton();
		return true;
	case IDC_C_VideoHalfSize:
		sg_Config.m_bVideoHalfSize = g_Config.m_bVideoHalfSize = OnCommandCheck( m_hControlVideoHalfSize );
		return true;

	case IDC_C_AudioDevices:
		if ( iNotify == CBN_SELCHANGE )
		{
			LRESULT iDeviceId = SendMessage( m_hControlAudioDevices, CB_GETCURSEL, 0, 0 );
			if ( iDeviceId < 0 )
				break;
			iDeviceId += WAVE_DEVICE_NONE;
			if ( iDeviceId != g_Config.m_iAudioDevice )
			{
				g_Config.m_iAudioDevice = (UINT) iDeviceId;
				OnChanges();
			}
		}
		break;
	case IDC_C_AudioCodecButton:
		// Display codec-selector dialog
		OnCommandAudioCodecButton();
		return true;

	case IDC_C_KeyConfigOpen:
	case IDC_C_KeyHookModeToggle:
	case IDC_C_KeyIndicatorToggle:
	case IDC_C_KeyRecordBegin:
	case IDC_C_KeyRecordPause:
	case IDC_C_KeyRecordStop:
	case IDC_C_KeyScreenshot:
	case IDC_C_KeySmallScreenshot:
		// Change to the hotkey.
		OnCommandKeyChange( hControl );
		return true;

	case IDC_C_UseDirectInput:
		sg_Config.m_bUseDirectInput = g_Config.m_bUseDirectInput = OnCommandCheck( m_hControlUseDirectInput );
		return true;

	case IDC_C_CustomSettingsList:
		if ( iNotify == CBN_EDITCHANGE )	// modify status text
		{
			OnChanges();
			return true;
		}
		if ( iNotify == CBN_KILLFOCUS )
		{
			OnCommandCustomKillFocus();
			return true;
		}
		if ( iNotify == CBN_SELCHANGE )
		{
			// update previously selected custom config
			Custom_Read();

			// user selected different custom setting in drop-down
			int idx = (int)SendMessage(m_hControlCustomSettingsList, CB_GETCURSEL, 0, 0);
			CTaksiConfigCustom* pCustom = Custom_FindSelect(idx);
			if (pCustom == NULL) 
				return true;

			// remember new current config
			m_pCustomCur = pCustom;

			// update custom controls
			m_bDataUpdating = true;
			Custom_Update(m_pCustomCur);
			m_bDataUpdating = false;
			return true;
		}
		break;

	case IDC_C_CustomNewButton:
		// update current custom config
		OnCommandCustomNewButton();
		return true;
	case IDC_C_CustomDeleteButton:
		// remove current custom config
		OnCommandCustomDeleteButton();
		return true;
	case IDC_C_CustomUseCurrent:
		// Fill in the data from the current hooked window.
		{
		m_bDataUpdating = true;
		CTaksiConfigCustom custom;
		if ( Custom_ReadHook(&custom))
		{
			Custom_Update(&custom);
		}
		m_bDataUpdating = false;
		}
		return true;

	case IDC_C_StatsClear:
		sg_ProcStats.ResetProcStats();
		UpdateProcStats( sg_ProcStats,
			(1<<TAKSI_PROCSTAT_LastError) |
			(1<<TAKSI_PROCSTAT_FrameRate) |
			(1<<TAKSI_PROCSTAT_DataRecorded) );
		SetStatusText( _T(""));
		return true;

	default:
		return false;
	}

	return false;
}

bool CGuiConfig::OnInitDialog( HWND hWnd, LPARAM lParam )
{
	// CreateDialog - IDD_GuiConfig
	ASSERT(hWnd);
	if ( lParam )	// this is just a tab dialog.
		return true;
	if ( m_hWnd )
		return true;
	m_hWnd = hWnd;

	if ( ! m_ToolTips.Create( m_hWnd, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON ))
	{
		DEBUG_ERR(("No tooltips" LOG_CR));
	}

	// Add sub dialogs to SysTabControl32, WC_TABCONTROL, CTabCtrl
	// DS_CONTROL children
	m_hControlTab = GetDlgItem( IDC_C_TAB );
	if ( m_hControlTab == NULL )
	{
		ASSERT(0);
		return false;
	}

	TabCtrl_SetToolTips( m_hControlTab, m_ToolTips.m_hWnd );

	for ( int i=0; i<COUNTOF(m_hWndTab); i++ )
	{
		m_hWndTab[i] = CreateDialogParam( g_hInst, 
			MAKEINTRESOURCE( IDD_GuiConfigTab1 + i ),
			m_hWnd, DialogProc, i+1 );
		ASSERT(m_hWndTab[i]);

		TCHAR szTmp[ _MAX_PATH ];
		LoadString( g_hInst, IDD_GuiConfigTab1+i, szTmp, sizeof(szTmp)-1 ); 
		TC_ITEM tabitem;	// tagTCITEMA
		tabitem.mask = TCIF_TEXT | TCIF_PARAM;
		tabitem.pszText = szTmp;
		tabitem.lParam = (LPARAM)(UINT_PTR) m_hWndTab[i];
		TabCtrl_InsertItem( m_hControlTab, i, &tabitem );

		LoadString( g_hInst, IDS_GuiConfigHelp1+i, szTmp, sizeof(szTmp)-1 ); 
		if ( ! m_ToolTips.AddTool( m_hWndTab[i], szTmp))
		{
			DEBUG_ERR(("No tooltips" LOG_CR));
		}
	}

	TabCtrl_SetCurSel( m_hControlTab, m_iTabCur );
	UpdateGuiTabCur();

#define GuiConfigControl(a,b,c)	\
	m_hControl##a = ::GetDlgItem(m_hWndTab[b-1],IDC_C_##a); \
	if (m_hControl##a == NULL ) \
	{ ASSERT(0); return false; } \
	m_ToolTips.AddToolForControl(m_hControl##a); 
#include "GuiConfigControls.tbl"
#undef GuiConfigControl

#ifdef USE_DIRECTX8
	::EnableWindow( m_hControlUseDX8, true );
#endif
#ifdef USE_DIRECTX9
	::EnableWindow( m_hControlUseDX9, true );
#endif

	// show credits
	UPDATE_EDIT_LIMIT( CTaksiConfig, CaptureDirectory, m_szCaptureDir );
	UPDATE_EDIT_LIMIT( CTaksiConfig, FileNamePostfix, m_szFileNamePostfix );
	UPDATE_EDIT_LIMIT( CTaksiConfigCustom, CustomPattern, m_szPattern );
	UPDATE_EDIT_LIMIT( CTaksiConfigCustom, CustomSettingsList, m_szAppId );

	SetSaveState( false );

	m_ToolTips.AddToolForControl( GetDlgItem(IDC_C_SaveButton));
	m_ToolTips.AddToolForControl( GetDlgItem(IDC_C_RestoreButton));

	//m_ToolTips.SetDelayTime(TTDT_INITIAL,400);
	m_ToolTips.Start();

	// Initialize all controls
	UpdateSettings( g_Config );
	UpdateProcStats( sg_ProcStats, 0xFFFFFFFF );
	::SetTimer( m_hWnd, IDT_UpdateStats, 1000, NULL );

	SetStatusText( _T("Taksi: Version ") _T(TAKSI_VERSION_S) _T(" ") _T(__DATE__) _T("."));
	return true;
}

//********************************************************

INT_PTR CALLBACK CGuiConfig::DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// DLGPROC CreateDialog
	switch(uMsg)
	{
	case WM_INITDIALOG:
		return g_GUIConfig.OnInitDialog(hWnd,lParam);
	case WM_COMMAND:
		return g_GUIConfig.OnCommand( LOWORD(wParam), HIWORD(wParam), (HWND)lParam );
	case WM_NOTIFY:
		return g_GUIConfig.OnNotify((int) wParam, (NMHDR*) lParam );
	case WM_TIMER:
		return g_GUIConfig.OnTimer((UINT) wParam );
	case WM_HELP:
		return g_GUIConfig.OnHelp((LPHELPINFO) lParam );
	case WM_CLOSE:
		::DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		if ( g_GUIConfig.m_hWnd == hWnd )
		{
			g_GUIConfig.m_ToolTips.DestroyWindow();
			g_GUIConfig.m_hWnd = NULL;
		}
		break;

#ifdef _DEBUG
	case WM_SETCURSOR:
	case WM_NCHITTEST:		// 0x0084
	case WM_NCMOUSEMOVE:	// 0x00a0
	case WM_MOUSEMOVE:		// 0x0200
	case WM_CTLCOLOREDIT:	// 0x0133
	case WM_CTLCOLORBTN:	// 0x0135
	case WM_CTLCOLORSTATIC:	// 0x0138
		break;
#endif
	default:
		//DEBUG_TRACE(("DialogProc 0x%04x,%d,%d" LOG_CR, uMsg, wParam, lParam ));
		break;
	}
	return false;
}
