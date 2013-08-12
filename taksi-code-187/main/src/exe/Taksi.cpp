//
// Taksi.cpp 
//
#include "../stdafx.h"
#include "Taksi.h"
#include "resource.h"
#include "gui.h"
#include "guiConfig.h"

// Program Configuration
HINSTANCE g_hInst = NULL;
const TCHAR g_szAppTitle[] = _T("Taksi");
CTaksiConfig g_Config;

const TCHAR* CheckIntResource( const TCHAR* pszText, TCHAR* pszTmp )
{
	// Interpret MAKEINTRESOURCE()
	if ( ! ISINTRESOURCE(pszText))	// MAKEINTRESOURCE()
		return pszText;
	int iLen = LoadString( g_hInst, GETINTRESOURCE(pszText), pszTmp, _MAX_PATH-1 );
	if ( iLen <= 0 )
		return _T("");
	return pszTmp;
}

int DlgHelp( HWND hWnd, const TCHAR* pszMsg, UINT uType )
{
	// uType = MB_OK
	if ( pszMsg == NULL )
	{
		// Explain to the user why this doesnt work yet.
		pszMsg = _T("TODO");
	}
	TCHAR szTmp[ _MAX_PATH ];
	pszMsg = CheckIntResource( pszMsg, szTmp );
	return ::MessageBox( hWnd, pszMsg, g_szAppTitle, uType );
}

void CheckVideoCodec( HWND hWnd, const ICINFO& info )
{
	bool bCompressed = true;
	switch ( g_Config.m_VideoCodec.m_v.fccHandler )
	{
	case MAKEFOURCC('D','I','B',' '): // no compress.
	case MAKEFOURCC('d','i','b',' '): // no compress.
		bCompressed = false;
		break;
	}

	if ( info.dwSize == 0 && bCompressed )
	{
		DlgHelp( hWnd, MAKEINTRESOURCE(IDS_HELP_CODEC_FAIL));
		return;
	}

	if ( g_Config.m_bVideoCodecMsg )
		return;
	g_Config.m_bVideoCodecMsg = true;

	int iRet;
	switch ( g_Config.m_VideoCodec.m_v.fccHandler )
	{
	case MAKEFOURCC('D','I','B',' '): // no compress.
	case MAKEFOURCC('d','i','b',' '): // no compress.
		// IDS_HELP_CODEC_DIB
		ASSERT( ! bCompressed );
		iRet = DlgHelp( hWnd, MAKEINTRESOURCE(IDS_HELP_CODEC_DIB));
		break;
	case MAKEFOURCC('m','s','v','c'): // Video1 = CRAM
	case MAKEFOURCC('M','S','V','C'): // Video1 = CRAM
		// just warn that the codec isnt very good. IDS_HELP_CODEC_CRAM
		iRet = DlgHelp( hWnd, MAKEINTRESOURCE(IDS_HELP_CODEC_CRAM));
		break;
#if 0
		// MPEG4 may slow frames per sec. high CPU usage.
#endif
	default:
		return;
	}
	if ( iRet == IDCANCEL )	// dont show this again!
	{
	}

	// Instructions on how to capture a window.
	iRet = DlgHelp( hWnd, MAKEINTRESOURCE(IDS_HELP_CAPTURE));
}

static void InitApp()
{
	if ( ! g_Config.ReadIniFile())
	{
		// this is ok since we can just take all defaults.
	}

	// Test my Save Dir
	HRESULT hRes = g_Config.FixCaptureDir();
	if ( FAILED(hRes))
	{
		DlgHelp( NULL, MAKEINTRESOURCE(IDS_ERR_SAVEDIR));
	}

	// Test my codec.
	// may want to select a better codec?
	ICINFO info;
	g_Config.m_VideoCodec.GetCodecInfo(info);
	CheckVideoCodec( NULL, info );

	sg_Config.CopyConfig( g_Config );

	LoadString( g_hInst, IDS_SELECT_APP_HOOK, 
		sg_ProcStats.m_szLastError, COUNTOF(sg_ProcStats.m_szLastError));

#ifdef _DEBUG
	CTaksiShared* pDll = &sg_Shared;
#endif

	InitCommonControls();
}

int APIENTRY WinMain( HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR     lpCmdLine,
					 int       nCmdShow)
{
	// NOTE: Dll has already been attached. DLL_PROCESS_ATTACH
	g_hInst = hInstance;

	HANDLE hMutex = ::CreateMutex( NULL, true, _T("TaksiMutex") );	// diff name than TAKSI_MASTER_CLASSNAME
	if ( ::GetLastError() == ERROR_ALREADY_EXISTS )
	{
		// Set focus to the previous window.
		HWND hWnd = FindWindow( TAKSI_MASTER_CLASSNAME, NULL );
		if ( hWnd )
		{
			::ShowWindow( hWnd, SW_NORMAL );
			::SetActiveWindow( hWnd );
		}
		return -1;
	}

	InitApp();

	if ( ! g_GUI.CreateGuiWindow(nCmdShow))
	{
		return -1;
	}

#ifdef USE_DIRECTX8
	Test_DirectX8(g_GUI.m_hWnd);		// Get method offsets of IDirect3DDevice8 vtable
#endif
#ifdef USE_DIRECTX9
	Test_DirectX9(g_GUI.m_hWnd);		// Get method offsets of IDirect3DDevice9 vtable
#endif

	// Set HWND for reference by the DLL
	if ( ! sg_Shared.InitMasterWnd(g_GUI.m_hWnd))
	{
		return -1;
	}

	for(;;)
	{
		MSG msg; 
		if ( ! GetMessage(&msg,NULL,0,0))
			break;
		if (!IsDialogMessage(g_GUIConfig.m_hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// Free memory taken by custom configs etc.
	sg_Shared.DestroyShared();
	return 0;
}
