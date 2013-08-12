//
// Gui.cpp
// main controls.
//
#include "../stdafx.h"
#include <richedit.h>	// NM_RCLICK
#include "Taksi.h"
#include "resource.h"
#include "gui.h"
#include "guiConfig.h"

#ifdef USE_TRAYICON
#include <shellapi.h>
#endif

CGui g_GUI;

static BOOL AppendMenuStr( HMENU hMenu, int id )
{
	// IDM_SC_HELP_URL
	TCHAR szTmp[ _MAX_PATH ];
	int iLen = LoadString( g_hInst, id, szTmp, sizeof(szTmp)-1 );
	return AppendMenu( hMenu, MF_INSERT|MF_STRING|MF_BYCOMMAND, 
		id, szTmp );
}

//**************************************************************

CGui::CGui()
	: m_uTimerStat(0)
#ifdef USE_TRAYICON
	, m_hTrayIconMenuDummy(NULL)
	, m_hTrayIconMenu(NULL)
#endif
{
}

int CGui::MakeWindowTitle( TCHAR* pszTitle, const TCHAR* pszHookApp, const TCHAR* pszState ) // static
{
	// ASSUME pszTitle is _MAX_PATH
	//TCHAR szName[ _MAX_PATH ];
	int iLen = LoadString( g_hInst, ID_APP, pszTitle, _MAX_PATH-1 );
	if ( iLen <= 0 )
	{
		ASSERT(0);
		return iLen;
	}

	if ( pszHookApp == NULL || pszHookApp[0] == '\0' )
	{
		return _sntprintf( pszTitle+iLen, _MAX_PATH-iLen, _T(" v") _T(TAKSI_VERSION_S) ); 
	}
	if ( pszState != NULL )
	{
		iLen += _sntprintf( pszTitle+iLen, _MAX_PATH-iLen, _T(" %s"), pszState ); 
	}

	return _sntprintf( pszTitle+iLen, _MAX_PATH-iLen, _T(" - %s"), pszHookApp ); 
}

bool CGui::UpdateWindowTitle()
{
	// Set the app title to reflect current hooked app.

	const TCHAR* pszHookApp = NULL;
	if ( sg_ProcStats.m_dwProcessId )
	{
		pszHookApp = ::GetFileTitlePtr( sg_ProcStats.m_szProcessPath );
	}

	int iLenState = 0;
	TCHAR szState[ _MAX_PATH ];

	switch (sg_ProcStats.m_eState)
	{
	case TAKSI_INDICATE_Hooking:
		iLenState += _sntprintf( szState, COUNTOF(szState),
			_T("(Hooking)") );
		break;
	case TAKSI_INDICATE_Recording:
		iLenState += _sntprintf( szState, COUNTOF(szState),
			_T("(Rec %.2gM)"), (float) sg_ProcStats.get_DataRecMeg());
		break;
	case TAKSI_INDICATE_RecordPaused:
		iLenState += _sntprintf( szState, COUNTOF(szState),
			_T("(Pause %.2gM)"), (float) sg_ProcStats.get_DataRecMeg());
		break;
	}

	TCHAR szTitle[ _MAX_PATH ];
	int iLen = MakeWindowTitle( szTitle, pszHookApp, (iLenState>0)? szState : NULL );
	if ( iLen <= 1 )
		return false;

	SetWindowText( m_hWnd, szTitle );
	return true;
}

int CGui::GetButtonState( TAKSI_HOTKEY_TYPE eKey ) const
{
	// RETURN: 
	//  The id of the bitmap to represent the state.
	//  <0 = disabled (cant take action!)

	switch (eKey)
	{
	case TAKSI_HOTKEY_ConfigOpen:
		return IDB_ConfigOpen_1;	// only 1 state.

	case TAKSI_HOTKEY_HookModeToggle:
		if ( sg_Shared.IsHookCBT())
			return IDB_HookModeToggle_2;
		if ( sg_ProcStats.m_hWndCap == NULL )
			return IDB_HookModeToggle_3;
		return( IDB_HookModeToggle_1 );

	case TAKSI_HOTKEY_IndicatorToggle:
		if ( sg_Config.m_bShowIndicator )
			return IDB_IndicatorToggle_1;
		return IDB_IndicatorToggle_2;
	}

	bool bIsRecording = ( sg_ProcStats.m_eState == TAKSI_INDICATE_Recording ||
			sg_ProcStats.m_eState == TAKSI_INDICATE_RecordPaused );

	switch (eKey)
	{
	case TAKSI_HOTKEY_RecordBegin:
		if ( sg_ProcStats.m_dwProcessId == 0 )
			return -IDB_RecordBegin_G;
		if ( bIsRecording )
			return -IDB_RecordBegin_G;
		return IDB_RecordBegin_1;

	case TAKSI_HOTKEY_Screenshot:
		if ( sg_ProcStats.m_dwProcessId == 0 )
			return -IDB_Screenshot_G;
		return IDB_Screenshot_1;

	case TAKSI_HOTKEY_SmallScreenshot:
		if ( sg_ProcStats.m_dwProcessId == 0 )
			return -IDB_SmallScreenshot_G;
		return IDB_SmallScreenshot_1;

	case TAKSI_HOTKEY_RecordPause:
		if ( sg_ProcStats.m_eState == TAKSI_INDICATE_RecordPaused )
			return IDB_RecordPause_2;
		if ( bIsRecording )
			return IDB_RecordPause_1;
		return( -IDB_RecordPause_G );

	case TAKSI_HOTKEY_RecordStop:
		if ( bIsRecording )
			return IDB_RecordStop_1;
		return( -IDB_RecordStop_G );
	}
	ASSERT(0);
	return -IDB_ConfigOpen_1;
}

LRESULT CGui::UpdateButton( TAKSI_HOTKEY_TYPE eKey )
{
	int id = abs( GetButtonState(eKey)) - IDB_ConfigOpen_1;
	ASSERT( id >= 0 && id < COUNTOF(m_Bitmap));
	return ::SendDlgItemMessage( m_hWnd, IDB_ConfigOpen_1 + eKey,
		BM_SETIMAGE, IMAGE_BITMAP, 
		(LPARAM) m_Bitmap[id].get_HBitmap());
}

void CGui::UpdateButtonStates()
{
	// Gray record/stop depending on state.
	// IDB_RecordStop = TAKSI_HOTKEY_RecordStop
	for ( int i=0; i<TAKSI_HOTKEY_QTY; i++ )
	{
		UpdateButton( (TAKSI_HOTKEY_TYPE) i );
	}

	UpdateWindowTitle();

	if ( sg_ProcStats.m_eState == TAKSI_INDICATE_Recording )
	{
		if ( m_uTimerStat == 0 )
		{
			m_uTimerStat = ::SetTimer( m_hWnd, IDT_UpdateStats, 1000, NULL );
		}
	}
	else
	{
		if ( m_uTimerStat )
		{
			::KillTimer( m_hWnd, IDT_UpdateStats );
			m_uTimerStat = 0;
		}
	}
}

int CGui::GetVirtKeyName( TCHAR* pszName, int iLen, BYTE bVirtKey ) // static
{
	if ( bVirtKey == 0 )
		return 0;
	WORD bScanCode = ::MapVirtualKey( bVirtKey, 0);
	return ::GetKeyNameText( bScanCode << 16, pszName, iLen );
}

int CGui::GetHotKeyName( TCHAR* pszName, int iLen, TAKSI_HOTKEY_TYPE eHotKey ) // static
{
	// Get the text to describe the hotkey.
	// Control, Alt, Shift

	static const BYTE sm_vKeysExt[3] = 
	{
		VK_SHIFT,		// HOTKEYF_SHIFT
		VK_CONTROL,		// HOTKEYF_CONTROL
		VK_MENU,		// HOTKEYF_ALT
	};

	WORD wHotKey = g_Config.GetHotKey( eHotKey );
	if ( wHotKey == 0 )
		return 0;

	int i=0;
	BYTE bExt = HIBYTE(wHotKey);
	for ( int j=0; bExt && j<COUNTOF(sm_vKeysExt); j++, bExt >>= 1 )
	{
		if ( bExt & 1 )
		{
			i += GetVirtKeyName( pszName+i, iLen-i, sm_vKeysExt[j] );
			pszName[i++] = '+';
		}
	}
	return GetVirtKeyName( pszName+i, iLen-i, LOBYTE(wHotKey));
}

void CGui::UpdateButtonToolTips()
{
	// reflect GetButtonState() in tooltip ???
	for ( int i=0; i<BTN_QTY; i++ )
	{
		TCHAR szHelp[ _MAX_PATH ];
		int iLen = LoadString( g_hInst, i + IDB_ConfigOpen_1, szHelp, sizeof(szHelp)-1 );
		ASSERT( iLen > 0 );

		TCHAR szHotKey[ _MAX_PATH ];
		iLen = GetHotKeyName( szHotKey, COUNTOF(szHotKey), (TAKSI_HOTKEY_TYPE) i ); 

		TCHAR* pszText;
		TCHAR szText[ _MAX_PATH ];
		if ( iLen > 0 )
		{
			iLen = _sntprintf( szText, COUNTOF(szText), _T("(%s) %s"), szHotKey, szHelp );
			pszText = szText;
		}
		else
		{
			pszText = szHelp;	// no hotkey mapped.
		}

		HWND hWndCtrl = GetDlgItem(i + IDB_ConfigOpen_1);
		ASSERT(hWndCtrl);
		if ( ! m_ToolTips.AddTool( hWndCtrl, pszText ))
		{
			DEBUG_ERR(("No tooltips" LOG_CR));
		}
	}
}

void CGui::OnCommandHelpURL() // static
{
	// IDM_SC_HELP_URL
	// NOTE: use the proper URL here? 
	// _T("http://www.menasoft.com/taksi")
	// _T("http://taksi.sourceforge.net/")
	CHttpLink_GotoURL( _T("http://taksi.sourceforge.net/"), SW_SHOWNORMAL );
}

static void About_OnInitDialog( HWND hwndDlg )
{
	TCHAR szTmp[ _MAX_PATH ];
	int iLen = g_GUI.MakeWindowTitle( szTmp, NULL, NULL );
	iLen += _sntprintf( szTmp+iLen, COUNTOF(szTmp)-iLen, _T(", (built:") _T(__DATE__) _T(")") );
	SetDlgItemText( hwndDlg, IDC_ABOUT_1, szTmp );

	LoadString( g_hInst, IDC_ABOUT_5, szTmp, COUNTOF(szTmp));
	SetDlgItemText( hwndDlg, IDC_ABOUT_5, szTmp );
}

BOOL CALLBACK AboutDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    switch (message) 
    { 
	case WM_INITDIALOG:
		About_OnInitDialog(hwndDlg);
		break;
	case WM_COMMAND: 
		switch (LOWORD(wParam)) 
        { 
		case IDOK: 
        case IDCANCEL: 
			EndDialog(hwndDlg, wParam); 
            return true; 
		} 
    } 
    return false; 
} 

#ifdef USE_TRAYICON

void CGui::UpdateMenuPopupHotKey( HMENU hMenu, TAKSI_HOTKEY_TYPE eKey )
{
	// update the menu item for its hotkey.

	int iState = GetButtonState( eKey );

	UINT uMenuFlags = MF_BYCOMMAND;
	uMenuFlags |=	(( iState < 0 ) ? (MF_GRAYED|MF_DISABLED) : MF_ENABLED );

	switch( eKey)
	{
	case TAKSI_HOTKEY_ConfigOpen:
		break;
	case TAKSI_HOTKEY_HookModeToggle:
		uMenuFlags |= (iState==IDB_HookModeToggle_1) ? MF_UNCHECKED : MF_CHECKED;
		break;
	case TAKSI_HOTKEY_IndicatorToggle:
		uMenuFlags |= (iState==IDB_IndicatorToggle_1) ? MF_CHECKED : MF_UNCHECKED;
		break;
	case TAKSI_HOTKEY_RecordPause:
		uMenuFlags |= (iState==IDB_RecordPause_2) ? MF_CHECKED : MF_UNCHECKED;
		break;
	}

	int id = IDB_ConfigOpen_1+eKey;

	TCHAR szMenuText[ _MAX_PATH ];
	int iLenMenuText = GetMenuString( hMenu, id,
		szMenuText, COUNTOF(szMenuText)-1, uMenuFlags );
	ASSERT(iLenMenuText>0);

	TCHAR szHotKey[ _MAX_PATH ];
	int iLenHotKey = GetHotKeyName( szHotKey, COUNTOF(szHotKey), eKey ); 

	TCHAR* pszParen = _tcschr( szMenuText, '(' );
	if ( iLenHotKey > 0 )
	{
		if ( pszParen )
		{
			_sntprintf( pszParen, COUNTOF(szMenuText) - (pszParen-szMenuText),
				_T("(%s)"), szHotKey );
		}
		else
		{
			_sntprintf( szMenuText+iLenMenuText, COUNTOF(szMenuText) - iLenMenuText,
				_T("\t(%s)"), szHotKey );
		}
	}
	else
	{
		if ( pszParen )
		{
			ASSERT(pszParen[-1] == '\t');
			pszParen[-1] = '\0';	// get rid of the key and space.
		}
	}

	UINT uRet = ModifyMenu( hMenu, id, uMenuFlags, id, szMenuText );
}

void CGui::OnInitMenuPopup( HMENU hMenu )
{
	// WM_INITMENUPOPUP
	// update the check marks, enabled states and hotkeys for the menu.
	for ( int i=0; i<TAKSI_HOTKEY_QTY; i++ )
	{
		UpdateMenuPopupHotKey( hMenu, (TAKSI_HOTKEY_TYPE)i );
	}
}

BOOL CGui::TrayIcon_Command( DWORD dwMessage, HICON hIcon, const TCHAR* pszTip, const TCHAR* pszInfo )
{
	// dwMessage = NIM_ADD;
	NOTIFYICONDATA tnd;
	tnd.cbSize = sizeof(NOTIFYICONDATA);
	tnd.hWnd = m_hWnd;
	tnd.uID = ID_APP;
	tnd.uFlags = NIF_MESSAGE|NIF_TIP;
	tnd.uCallbackMessage = WM_APP_TRAY_NOTIFICATION;
	if ( hIcon )
	{
		tnd.hIcon = hIcon;
		tnd.uFlags |= NIF_ICON;
	}
	if ( pszTip )
		lstrcpyn(tnd.szTip, pszTip, sizeof(tnd.szTip));
	else
		tnd.szTip[0] = '\0';
	if ( pszInfo )
	{
		tnd.uFlags |= NIF_INFO;
		lstrcpyn( tnd.szInfo, pszInfo, sizeof(tnd.szInfo));
		tnd.uTimeout = 1000;
		lstrcpyn( tnd.szInfoTitle, pszTip, sizeof(tnd.szInfoTitle));
		tnd.dwInfoFlags = NIIF_INFO;
	}

	return Shell_NotifyIcon(dwMessage, &tnd);
}

void CGui::TrayIcon_OnEvent( LPARAM lParam )
{
	switch (lParam )
	{
	case WM_RBUTTONUP:
		{
		// Make first menu item the default (bold font)
		SetMenuDefaultItem( m_hTrayIconMenu, IsWindowVisible(m_hWnd) ? 1 : 0, true);

		// Display the menu at the current mouse location. There's a "bug"
		// (Microsoft calls it a feature) in Windows 95 that requires calling
		// SetForegroundWindow. To find out more, search for Q135788 in MSDN.
		SetForegroundWindow(m_hWnd);
		POINT mouse;
		if ( GetCursorPos(&mouse))
		{
			TrackPopupMenu(m_hTrayIconMenu, 0, mouse.x, mouse.y, 0, m_hWnd, NULL);
		}
		}
		break;

	case WM_LBUTTONDOWN:
		// put up the status balloon help.
		{
		TCHAR szBuff[ _MAX_PATH ];
		LoadString( g_hInst, IDS_APP_DESC, szBuff, sizeof(szBuff));
		TrayIcon_Command( NIM_MODIFY, NULL, szBuff, "Status" );
		}
		break;

	case WM_LBUTTONDBLCLK:
		if ( IsWindowVisible(m_hWnd))
		{
			SetForegroundWindow(m_hWnd);
			SendMessage( m_hWnd, WM_COMMAND, IDB_ConfigOpen_1, 0);
		}
		else
		{
			ShowWindow(m_hWnd,SW_NORMAL);
			SetForegroundWindow(m_hWnd);
		}
		break;
	}
}

bool CGui::TrayIcon_Create()
{
	// everyone expects this feature these days. 
	// so give the people what they want.
	// Set up my tray menu.

	TCHAR szBuff[ _MAX_PATH ];
    LoadString( g_hInst, IDS_APP_DESC, szBuff, sizeof(szBuff));

	HICON hIcon = (HICON) LoadImage(g_hInst,MAKEINTRESOURCE(ID_APP), IMAGE_ICON, 16, 16, 0);
	if ( ! TrayIcon_Command(NIM_ADD, hIcon, szBuff))
		return false;

	DestroyIcon(hIcon);

	m_hTrayIconMenuDummy = LoadMenu(g_hInst, MAKEINTRESOURCE(IDM_TRAYICON));
	m_hTrayIconMenu = GetSubMenu(m_hTrayIconMenuDummy, 0);
	if ( m_hTrayIconMenu == NULL )
		return false;

	// TrayIcon_Command( NIM_MODIFY, hIcon, szBuff);
	return true;
}
#endif

//***************************************************************

bool CGui::OnTimer( UINT idTimer )
{
	// IDT_UpdateStats
	if ( idTimer != IDT_UpdateStats )
	{
		return false;
	}
	if ( sg_ProcStats.m_eState == TAKSI_INDICATE_Recording )
	{
		UpdateWindowTitle();
	}
	return true;
}

int CGui::OnCommandHelpAbout( HWND hWndParent )
{
	// IDM_SC_HELP_ABOUT
	return DialogBox( g_hInst, MAKEINTRESOURCE(IDD_About), hWndParent, AboutDlgProc );
}

bool CGui::OnCommandKey( TAKSI_HOTKEY_TYPE eKey )
{
	if ( GetButtonState(eKey) <= 0 )
		return false;
	sg_Shared.m_dwHotKeyMask |= (1<<eKey);	// trigger the action in the dll.
	UpdateButtonStates();
	return true;
}

bool CGui::OnCommand( int id, int iNotify, HWND hControl )
{
	switch (id)
	{
	case TAKSI_HOTKEY_ConfigOpen:
	case IDB_ConfigOpen_1:
		if ( g_GUIConfig.m_hWnd == NULL )
		{
			g_GUIConfig.m_hWnd = CreateDialog( g_hInst, 
				MAKEINTRESOURCE(IDD_GuiConfig),
				NULL, CGuiConfig::DialogProc );
			if (g_GUIConfig.m_hWnd == NULL) 
			{
				return false;
			}
		}
		else
		{
			::ShowWindow(g_GUIConfig.m_hWnd,SW_SHOWNORMAL);
		}
		::SetForegroundWindow(g_GUIConfig.m_hWnd);	// moves window to the foreground
		::SetFocus(g_GUIConfig.m_hWnd);
		return true;
	case TAKSI_HOTKEY_HookModeToggle:
	case IDB_HookModeToggle_1:
		if ( sg_Shared.IsHookCBT())
		{
			sg_Shared.HookCBT_Uninstall();
		}
		else
		{
			sg_Shared.HookCBT_Install();
		}
		UpdateButtonStates();
		return true;
	case TAKSI_HOTKEY_IndicatorToggle:
	case IDB_IndicatorToggle_1:
		// toggle shared state and update config, no save?
		sg_Config.m_bShowIndicator = !sg_Config.m_bShowIndicator;
		g_Config.m_bShowIndicator = sg_Config.m_bShowIndicator;
		UpdateButtonStates();
		return true;

	case TAKSI_HOTKEY_RecordBegin:
	case TAKSI_HOTKEY_RecordPause:
	case TAKSI_HOTKEY_RecordStop:
	case TAKSI_HOTKEY_Screenshot:
	case TAKSI_HOTKEY_SmallScreenshot:
		return OnCommandKey( (TAKSI_HOTKEY_TYPE) id );

	case IDB_RecordBegin_1:
	case IDB_RecordPause_1:
	case IDB_RecordStop_1:
	case IDB_Screenshot_1:
	case IDB_SmallScreenshot_1:
		return OnCommandKey( (TAKSI_HOTKEY_TYPE)( TAKSI_HOTKEY_ConfigOpen + (id - IDB_ConfigOpen_1)));

	case IDM_SC_HELP_URL:
		OnCommandHelpURL();
		return true;
	case IDM_SC_HELP_ABOUT:
		OnCommandHelpAbout(m_hWnd);
		return true;

#ifdef USE_TRAYICON
	case IDC_SHOW_NORMAL:	// SC_RESTORE
		ShowWindow(m_hWnd, SW_NORMAL);
		return true;
	case SC_CLOSE:
		if ( m_hTrayIconMenu == NULL ) // tray didnt work.
			return false;
		DestroyWindow();
		return true;
	case SC_MINIMIZE:
		// hide the window.
		if ( m_hTrayIconMenu == NULL )	// tray didnt work.
			return false;
		ShowWindow( m_hWnd, SW_HIDE );
		return true;
#endif
	}
	return false;
}

bool CGui::OnCreate( HWND hWnd, CREATESTRUCT* pCreate )
{
	// WM_CREATE 
	DEBUG_MSG(( "CGui::WM_CREATE" LOG_CR ));
	ASSERT(pCreate);
	m_hWnd = hWnd;

	// SysMenu
	HMENU hMenu = ::GetSystemMenu(m_hWnd,false);
	if ( hMenu == NULL )
	{
		ASSERT(0);
		return false;
	}
	AppendMenuStr(hMenu,IDM_SC_HELP_URL);
	AppendMenuStr(hMenu,IDM_SC_HELP_ABOUT);

	// build GUI controls
	// NOTE: Bitmap text font is : Small Fonts 7 ?
	if ( ! m_ToolTips.Create( m_hWnd, WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON ))
	{
		DEBUG_ERR(("No tooltips" LOG_CR));
		ASSERT(0);
	}

	BITMAP* pbmi = (BITMAP*)( pCreate->lpCreateParams );
	ASSERT(pbmi);

	int x=0;
	for ( int i=0; i<BTN_QTY; i++ )
	{
		HWND hWndCtrl = CreateWindow( _T("BUTTON"), _T(""), 
			WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_BITMAP,
			pbmi->bmWidth*i, 0, 
			pbmi->bmWidth, pbmi->bmHeight,
			m_hWnd,
			(HMENU)(INT_PTR)( i + IDB_ConfigOpen_1 ),
			g_hInst,
			NULL );
		ASSERT(hWndCtrl);

		// set the proper bitmap for the button.
		ASSERT(m_Bitmap[i].get_HBitmap());
		::SendMessage( hWndCtrl, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM) m_Bitmap[i].get_HBitmap());
	}

	UpdateButtonToolTips();
	UpdateButtonStates();
#ifdef USE_TRAYICON
	TrayIcon_Create();
#endif
	return true;
}

//******************************************************************************

LRESULT CALLBACK CGui::WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) // static
{
	switch (uMsg)
	{
	case WM_CREATE:
		if ( ! g_GUI.OnCreate( hWnd, (CREATESTRUCT*) lParam ))
			return -1;	// destroy me.
		break;
	case WM_MOVE:
		{
		ASSERT(hWnd);
		RECT rect;
		::GetWindowRect(hWnd,&rect);
		g_Config.m_ptMasterWindow.x = rect.left;
		g_Config.m_ptMasterWindow.y = rect.top;
		sg_Config.m_ptMasterWindow = g_Config.m_ptMasterWindow;
		}
		break;
	case WM_DESTROY: // destroy the app with the window.
		// Exit the application when the window closes
		DEBUG_MSG(( "CGui::WM_DESTROY" LOG_CR ));
		g_GUI.m_ToolTips.DestroyWindow();
#ifdef USE_TRAYICON
		g_GUI.TrayIcon_Command( NIM_DELETE, NULL, NULL);
#endif
		sg_Shared.m_hMasterWnd = NULL;
		g_GUI.m_hWnd = NULL;
		::PostQuitMessage(1);
		break;
	case WM_SYSCOMMAND:	// off the sys menu.
		// wParam = SC_CLOSE
		// IDM_SC_HELP_URL 0xEEE0
		if ( g_GUI.OnCommand( wParam, 0, NULL ))
			return 0;
		break;
	case WM_COMMAND:
		if ( g_GUI.OnCommand( LOWORD(wParam), HIWORD(wParam), (HWND)lParam ))
			return 0;
		break;
	case WM_TIMER:
		return g_GUI.OnTimer((UINT) wParam );
	case WM_APP_UPDATE:
		// from UpdateMaster()
		// Use m_iMasterUpdateCount to throttle these ?
		g_GUI.UpdateButtonToolTips();
		g_GUI.UpdateButtonStates();
		// Update config.m_bMasterTopMost IsTopMost
		// if ( m_bMasterTopMost
		return 1;
	case WM_APP_REHOOKCBT:
		// Re-install system-wide hook
		sg_Shared.HookCBT_Install();
		g_GUI.UpdateButtonStates();
		return 1;

#ifdef USE_TRAYICON
	case WM_INITMENUPOPUP:
		// Set the gray levels of the menu items.
		if ( ! HIWORD(lParam))
		{
			g_GUI.OnInitMenuPopup( (HMENU) wParam );
		}
		break;
	case WM_APP_TRAY_NOTIFICATION:
		g_GUI.TrayIcon_OnEvent( lParam );
		break;
#endif
	}
	return ::DefWindowProc(hWnd,uMsg,wParam,lParam);
}

bool CGui::CreateGuiWindow( UINT nCmdShow )
{
	// CreateDialog - IDD_Gui
	// cbSize - the size of the structure.

	WNDCLASSEX wcx;
	ZeroMemory(&wcx,sizeof(wcx));
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = CGui::WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = g_hInst;
	wcx.hIcon = LoadIcon(g_hInst,MAKEINTRESOURCE(ID_APP));
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = TAKSI_MASTER_CLASSNAME;
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if ( ! ::RegisterClassEx(&wcx))
	{
		return false;
	}

	for ( int i=0; i<COUNTOF(m_Bitmap); i++ )
	{
		m_Bitmap[i].Attach( ::LoadBitmap( g_hInst, MAKEINTRESOURCE(i+IDB_ConfigOpen_1)));
		// ASSERT(m_Bitmap[i].get_HBitmap());
	}

	BITMAP bmi;
    int iRet = m_Bitmap[0].GetObject( sizeof(bmi), &bmi );
	if ( iRet != sizeof(bmi))
	{
		return false;
	}

	const DWORD c_dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	const DWORD c_dwExStyle = WS_EX_APPWINDOW ; // WS_EX_STATICEDGE

	RECT rect = { 0, 0, bmi.bmWidth*BTN_QTY, bmi.bmHeight };
	::AdjustWindowRectEx( &rect, c_dwStyle, false, c_dwExStyle );

	m_hWnd = CreateWindowEx( c_dwExStyle | ( g_Config.m_bMasterTopMost ? WS_EX_TOPMOST : 0 ),
		TAKSI_MASTER_CLASSNAME,      // class name
		_T(""), // title for our window (appears in the titlebar)
		c_dwStyle,
		g_Config.m_ptMasterWindow.x, g_Config.m_ptMasterWindow.y,  // initial x, y coordinate
		1+(rect.right-rect.left), 1+(rect.bottom-rect.top),   // width and height of the window
		HWND_DESKTOP,           // no parent window.
		NULL,           // no menu
		g_hInst,           // no creator
		&bmi );          // extra data
	if ( m_hWnd == NULL )
	{
		ASSERT(0);
		return false;
	}

	// Show the window
	::ShowWindow(m_hWnd, nCmdShow);

	//m_ToolTips.SetDelayTime(TTDT_INITIAL,200);
	m_ToolTips.Start();
	return true;
}
