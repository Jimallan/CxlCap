//
// CWndToolTip.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//

#ifndef _INC_CWndToolTip_H
#define _INC_CWndToolTip_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

// lib comctl32.lib
#include "CWindow.h"
#include <commctrl.h>
#include <assert.h>

class TAKSI_LINK CWndToolTip : public CWindowChild
{
	// Like MFC CToolTipCtrl based on TOOLTIPS_CLASS
	// Since only one tooltip may be open at a time this can be shared with many controls/tools.
public:
	bool Create( HWND hWndParent, DWORD dwStyle=WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP );
	void Start();

	bool AddTool( HWND hWnd, LPCTSTR lpszText, LPCRECT lpRectTool=NULL, UINT_PTR nIDTool=0 );
	bool AddToolForControl( HWND hWnd, UINT_PTR nIDTool=0 );
	void DelTool( HWND hWnd, UINT_PTR nIDTool=0 );

	void Activate( bool bActivate=true )
	{ 
		ASSERT(::IsWindow(m_hWnd));
		::SendMessage(m_hWnd, TTM_ACTIVATE, bActivate, 0L); 
	}
	int GetToolCount() const
	{ 
		ASSERT(::IsWindow(m_hWnd));  
		return (int) ::SendMessage(m_hWnd, TTM_GETTOOLCOUNT, 0, 0L);
	}

	int GetDelayTime(DWORD dwDuration=TTDT_INITIAL) const
	{ 
		// dwDuration = TTDT_INITIAL;
		ASSERT(::IsWindow(m_hWnd));
		return (int) ::SendMessage(m_hWnd, TTM_GETDELAYTIME, dwDuration, 0L); 
	}
	void SetDelayTime(DWORD dwDuration, int iTime)
	{ 
		// dwDuration = TTDT_INITIAL;
		ASSERT(::IsWindow(m_hWnd));  
		::SendMessage(m_hWnd, TTM_SETDELAYTIME, dwDuration, MAKELPARAM(iTime, 0)); 
	}

private:
	void FillInToolInfo( TOOLINFO& ti, HWND hWndCtrl, UINT_PTR nIDTool=0 ) const;
};

#endif

