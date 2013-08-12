//
// CWndGDI.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#ifndef _INC_CWndGDI_H
#define _INC_CWndGDI_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
#include "TaksiCommon.h"

#ifdef _WIN32

class TAKSI_LINK CWndGDI 
{
	// HBITMAP, HBRUSH, HPEN, HRGN, HFONT
	// Similar to MFC CGdiObject m_hObject
public:
	CWndGDI( HGDIOBJ hGDIObj = NULL ) :
		m_hObject(hGDIObj)
	{
	}
	~CWndGDI()
	{
		DeleteObjectLast();
	}

	void DeleteObjectLast()
	{
		if ( m_hObject )
		{
			::DeleteObject(m_hObject);
		}
	}

	bool Attach( HGDIOBJ hObject )
	{
		DeleteObjectLast();
		m_hObject = hObject;
		if (hObject == NULL)
			return false;
		return true;
	}
	void Detach()
	{
		m_hObject = NULL;
	}

	void DeleteObject()
	{
		if ( m_hObject )
		{
			::DeleteObject(m_hObject);
			m_hObject = NULL;
		}
	}

    int GetObject( int iSize, void* pData ) const
	{
		// possibly LOGFONT if this is a font.
		return ::GetObject( m_hObject, iSize, pData );
	}
	
	HGDIOBJ get_Handle() const
	{
		return m_hObject;
	}
	operator HGDIOBJ() const
	{
		return m_hObject;
	}

	HBRUSH get_HBrush() const
	{
		return (HBRUSH) m_hObject;
	}
	HPEN get_HPen() const
	{
		return (HPEN) m_hObject;
	}
	HRGN get_HRgn() const
	{
		return (HRGN) m_hObject;
	}
	HBITMAP get_HBitmap() const
	{
		return (HBITMAP) m_hObject;
	}
	HFONT get_HFont() const
	{
		return (HFONT) m_hObject;
	}

	HBRUSH CreateSolidBrush( COLORREF color )
	{
		DeleteObjectLast();
		m_hObject = ::CreateSolidBrush( color );
		return (HBRUSH) ( m_hObject );
	}
	HFONT CreateFontIndirect( const LOGFONT* pLogFont )
	{
		DeleteObjectLast();
		m_hObject = ::CreateFontIndirect( pLogFont );
		return (HFONT) ( m_hObject );
	}

	// CreateRectRgn
public:
	HGDIOBJ m_hObject;
};

class TAKSI_LINK CWndDC // similar to Std MFC class CDC
{
public:
	CWndDC( HDC hDC = NULL, HWND hWndOwner = HWND_BROADCAST ) :
		m_hDC(hDC),
		m_hWndOwner(hWndOwner)
	{
		// HWND_BROADCAST = we must use DeleteDC when we are done
		// NOTE: HWND_DESKTOP = NULL = this is a valid window!
	}
	~CWndDC()
	{
		ReleaseDC();
	}

	bool ReleaseDC();
	bool GetDC( HWND hWndOwner )
	{
		ReleaseDC();
		m_hWndOwner = hWndOwner;
		m_hDC = ::GetDC( hWndOwner );
		return( IsValid()); 
	}
	bool GetDCEx( HWND hWndOwner, DWORD dwFlags )
	{
		// dwFlags = DCX_WINDOW;
		ReleaseDC();
		m_hWndOwner = hWndOwner;
		m_hDC = ::GetDCEx( hWndOwner, NULL, dwFlags );
		return( IsValid()); 
	}
	bool CreateCompatibleDC( HDC hDCScreen )
	{
		ReleaseDC();
		m_hDC = ::CreateCompatibleDC( hDCScreen );
		return( IsValid()); 
	}
	HBITMAP CreateCompatibleBitmap( int x, int y )
	{
		ASSERT(m_hDC);
		return ::CreateCompatibleBitmap(m_hDC,x,y);
	}
	HGDIOBJ SelectObject( IN HGDIOBJ hGDIObj ) const
	{
		ASSERT(m_hDC);
		return ::SelectObject(m_hDC,hGDIObj);
	}
	HGDIOBJ SelectObject( const CWndGDI& GDIObj ) const
	{
		ASSERT(m_hDC);
		return ::SelectObject(m_hDC,GDIObj.m_hObject);
	}

	bool IsValid() const
	{
		return( m_hDC ? true : false );
	}
	operator HDC() const { return m_hDC; }

public:
	HDC m_hDC;
	HWND m_hWndOwner;	// Created or it belongs to a window? HWND_BROADCAST = no owner
};

class TAKSI_LINK CWndGDISelect
{
public:
	CWndGDISelect( HDC hDC, HGDIOBJ hGDIObj ) :
		m_hDC(hDC)
	{
		ASSERT(hGDIObj);
		m_hGDIObjPrv = ::SelectObject(hDC,hGDIObj);
	}
	~CWndGDISelect()
	{
		::SelectObject(m_hDC,m_hGDIObjPrv);
	}
private:
	HDC m_hDC;
	HGDIOBJ m_hGDIObjPrv;
};

class TAKSI_LINK CWndGDICur : public CWndGDI, public CWndGDISelect
{
	// Select a new created GDI object.
	// on destruct we should destruct the object then select the previous.
public:
	CWndGDICur( HDC hDC, HGDIOBJ hObj ) :
		CWndGDI(hObj),
		CWndGDISelect( hDC, hObj )
	{
	}
};

#endif

#endif
