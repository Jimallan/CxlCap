//
// CWindow.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#ifndef _INC_CWindow_H
#define _INC_CWindow_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
#include "TaksiCommon.h"

struct TAKSI_LINK CWindowChild
{
	// NOTE: dont destroy window in destructor
	CWindowChild()
		: m_hWnd(NULL)
	{
	}
	operator HWND () const
	{
		return m_hWnd;
	}
	void DestroyWindow()
	{
		if ( m_hWnd == NULL )
			return;
		::DestroyWindow(m_hWnd);
		m_hWnd = NULL;
	}
	HWND GetDlgItem( int id ) const
	{
		return ::GetDlgItem(m_hWnd,id);
	}
public:
	HWND m_hWnd;		// Me.
};

#endif
