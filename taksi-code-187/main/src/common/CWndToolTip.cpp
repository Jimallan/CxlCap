//
// CWndToolTip.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "stdafx.h"
#include "CWndToolTip.h"
#include "Common.h"

bool CWndToolTip::Create( HWND hWndParent, DWORD dwStyle /* = 0 */)
{
	m_hWnd = ::CreateWindow( TOOLTIPS_CLASS, NULL,
		WS_POPUP | dwStyle, // force WS_POPUP
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hWndParent, NULL, NULL /* g_hInst */, NULL );
	return m_hWnd ? true : false;
}

void CWndToolTip::Start()
{
	ASSERT(::IsWindow(m_hWnd));
	::SetWindowPos(m_hWnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	Activate( true );
}

void CWndToolTip::FillInToolInfo( TOOLINFO& ti, HWND hWnd, UINT_PTR nIDTool ) const
{
	// hWnd = the parent or the control depending on nIDTool

	ASSERT(hWnd);
	ASSERT(::IsWindow(m_hWnd));

	memset(&ti, 0, sizeof(ti));
	ti.cbSize = sizeof(ti);
	// ti.hinst = g_hInst;
	if (nIDTool == 0)
	{
		ti.hwnd = ::GetParent(hWnd);	// parent.
		ti.uFlags = TTF_IDISHWND;	// by window.
		ti.uId = (UINT_PTR)hWnd;	// TTF_IDISHWND = this is a window.
	}
	else
	{
		ti.hwnd = hWnd;	// this is the parent.
		ti.uFlags = 0;
		ti.uId = nIDTool;	// this is the child id.
	}
}

bool CWndToolTip::AddTool( HWND hWnd, LPCTSTR lpszText, LPCRECT lpRectTool, UINT_PTR nIDTool )
{
	// hWnd = the parent or the control depending on nIDTool
	ASSERT(hWnd);
	ASSERT(lpszText != NULL);

	TOOLINFO ti;
	FillInToolInfo(ti, hWnd, nIDTool);
	if (lpRectTool != NULL)
		memcpy(&ti.rect, lpRectTool, sizeof(RECT));
	ti.uFlags |= TTF_SUBCLASS;
	ti.lpszText = (LPTSTR)lpszText;

	return ::SendMessage(m_hWnd, TTM_ADDTOOL, 0, (LPARAM)&ti) ? true : false;
}

bool CWndToolTip::AddToolForControl( HWND hWnd, UINT_PTR nIDTool )
{
	ASSERT(hWnd);
	int id = nIDTool;
	if ( id <= 0 )
	{
		id = ::GetDlgCtrlID( hWnd );
		if ( id <= 0 )
			return false;
	}
	TCHAR szTmp[ _MAX_PATH ];
	int iLen = LoadString( NULL, id, szTmp, COUNTOF(szTmp)-1 ); 
	if ( iLen <= 0 )
	{
		HWND hWndControl = ( nIDTool == 0 ) ? hWnd : (::GetDlgItem(hWnd,nIDTool));
		iLen = GetWindowText( hWndControl, szTmp, COUNTOF(szTmp)-1 );
		if ( iLen <= 0 )
			return false;
	}
	return AddTool( hWnd, szTmp, NULL, nIDTool );
}

void CWndToolTip::DelTool( HWND hWnd, UINT_PTR nIDTool )
{
	// hWnd = the parent or the control depending on nIDTool
	ASSERT(hWnd);
	TOOLINFO ti;
	FillInToolInfo(ti, hWnd, nIDTool);
	::SendMessage(m_hWnd, TTM_DELTOOL, 0, (LPARAM)&ti);
}
