//
// Gui.h
//
#ifndef _INC_Gui_H
#define _INC_Gui_H
#if _MSC_VER > 1000
#pragma once
#endif

#include "../common/CWndGDI.h"
#include "../common/CWindow.h"
#include "../common/CWndToolTip.h"
#include "resource.h"

struct CGui : public CWindowChild
{
public:
	CGui();

	bool CreateGuiWindow( UINT nCmdShow );

	static void OnCommandHelpURL();
	static int OnCommandHelpAbout( HWND hWnd );

	static int MakeWindowTitle( TCHAR* pszTitle, const TCHAR* pszHookApp, const TCHAR* pszState );
	bool UpdateWindowTitle();

private:
	void UpdateButtonStates();
	void UpdateButtonToolTips();

	bool OnTimer( UINT idTimer );
	bool OnCreate( HWND hWnd, CREATESTRUCT* pCreate );
	bool OnCommandKey( TAKSI_HOTKEY_TYPE eKey );
	bool OnCommand( int id, int iNotify, HWND hControl );

	static ATOM RegisterClass();
	static LRESULT CALLBACK WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

#ifdef USE_TRAYICON
	void OnInitMenuPopup( HMENU hMenu );
	void UpdateMenuPopupHotKey( HMENU hMenu, TAKSI_HOTKEY_TYPE eKey );
	BOOL TrayIcon_Command( DWORD dwMessage, HICON hIcon, const TCHAR* pszTip=NULL, const TCHAR* pszInfo=NULL );
	void TrayIcon_OnEvent( LPARAM lParam );
	bool TrayIcon_Create();
#endif

	int GetButtonState( TAKSI_HOTKEY_TYPE eKey ) const;
	LRESULT UpdateButton( TAKSI_HOTKEY_TYPE eKey );

	static int GetVirtKeyName( TCHAR* pszName, int iLen, BYTE bVirtKey );
	static int GetHotKeyName( TCHAR* pszName, int iLen, TAKSI_HOTKEY_TYPE eHotKey );

public:
#define BTN_QTY TAKSI_HOTKEY_QTY
	CWndGDI m_Bitmap[ ( IDB_HookModeToggle_3 - IDB_ConfigOpen_1 ) + 1 ];
	CWndToolTip m_ToolTips;
	UINT_PTR m_uTimerStat;

#ifdef USE_TRAYICON
	HMENU m_hTrayIconMenuDummy;
	HMENU m_hTrayIconMenu;
#endif
};

extern CGui g_GUI;

#endif
