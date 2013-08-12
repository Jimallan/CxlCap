//
// Common.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "stdafx.h"
#include "Common.h"
#include "CWndGDI.h"
#include <windowsx.h>
#include <ShellAPI.h>
#include <tchar.h>

//**************************************************************************************

TIMEFAST_t GetPerformanceCounter()
{
	// Get high performance time counter. now
	LARGE_INTEGER count;
	::QueryPerformanceCounter(&count);
	return count.QuadPart;
}

TCHAR* GetFileTitlePtr( TCHAR* pszPath )
{
	// Given a full file path, get a pointer to its title.
	TCHAR* p = pszPath + lstrlen(pszPath); 
	while ((p > pszPath) && ! FILE_IsDirSep(*p)) 
		p--;
	if ( FILE_IsDirSep(*p))
		p++; 
	return p;
}

HRESULT CreateDirectoryX( const TCHAR* pszDir )
{
	// This is like CreateDirectory() except.
	// This will create intermediate directories if needed.
	// NOTE: like SHCreateDirectoryExA() but we cant use since thats only for Win2K+

	ASSERT(pszDir);
	TCHAR szTmp[ _MAX_PATH ];
	int iLen = 0;
	for (;;)
	{	
		int iStart = iLen;
		TCHAR ch;
		for (;;)
		{
			if ( iLen >= COUNTOF(szTmp)-1 )
				return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
			ch = pszDir[iLen];
			szTmp[iLen] = ch;
			if ( ch == '\0' )
				break;
			iLen++;
			if ( FILE_IsDirSep( ch ))
				break;
		}
		if ( iStart >= iLen )
		{
			if ( ch == '\0' )
				break;
			return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);	// bad name!
		}
		szTmp[iLen] = '\0';
		if ( ! CreateDirectory( szTmp, NULL ))
		{
			DWORD dwLastError = ::GetLastError();
			if ( dwLastError == ERROR_ALREADY_EXISTS )
				continue;
			if ( dwLastError == ERROR_ACCESS_DENIED && iStart==0 ) // create a drive? 'c:\' type thing.
				continue;
			return HRESULT_FROM_WIN32(dwLastError);
		}
	}

	return S_OK;
}

bool Str_IsSpace( char ch )
{
	// isspace() chokes on bad values!
	if ( ch < 0 || ch >= 0x80 )	// isspace() chokes on this!
		return false;
	if ( isspace(ch))
		return true;
	return false;
}

char* Str_SkipSpace( const char* pszNon )
{
	// NOTE: \n is a space.
	ASSERT(pszNon);
	for(;;)
	{
		char ch = *pszNon;
		if ( ch <= 0 )
			break;
		if ( ! Str_IsSpace(ch))
			break;
		pszNon++;
	}
	return (char*) pszNon;
}

int Mem_ConvertToString( char* pszDst, int iSizeDstMax, const BYTE* pSrc, int iLenSrcBytes )
{
	// Write bytes out in string format.
	// RETURN: the actual size of the string.
	iSizeDstMax -= 4;
	int iLenOut = 0;
	for ( int i=0; i<iLenSrcBytes; i++ )
	{
		if ( i )
		{
			pszDst[iLenOut++] = ',';
		}

		int iLenThis = _snprintf( pszDst+iLenOut, iSizeDstMax-iLenOut, "%d", pSrc[i] );
		if ( iLenThis <= 0 )
			break;
		iLenOut += iLenThis;
		if ( iLenOut >= iSizeDstMax )
			break;
	}
	pszDst[iLenOut] = '\0';
	return( iLenOut );
}

int Mem_ReadFromString( BYTE* pDst, int iLengthMax, const char* pszSrc )
{
	// Read bytes in from string format.
	// RETURN: the number of bytes read.
#define STR_GETNONWHITESPACE( pStr ) while ( Str_IsSpace((pStr)[0] )) { (pStr)++; } // isspace()

	int i=0;
	for ( ; i<iLengthMax; )
	{
		STR_GETNONWHITESPACE(pszSrc);
		if (pszSrc[0]=='\0')
			break;
		const char* pszSrcStart = pszSrc;
		char* pszEnd;
		pDst[i++] = (BYTE) strtol(pszSrc,&pszEnd,0);
		if ( pszSrcStart == pszEnd )	// must be the field terminator? ")},;"
			break;
		pszSrc = pszEnd;
		STR_GETNONWHITESPACE(pszSrc);
		if ( pszSrc[0] != ',' )
			break;
		pszSrc++;
	}
	return i;
}

HWND FindWindowTop( HWND hWnd )
{
	if ( ! ::IsWindow(hWnd))
		return NULL;

	DWORD dwStyle = GetWindowStyle(hWnd);
	for (;;)
	{
		if ( ! (dwStyle&WS_CHILD))	// WS_POPUP | WS_OVERLAPPED
			break;
		HWND hWndParent = ::GetParent(hWnd);
		if ( hWndParent == NULL )
			break;
		dwStyle = GetWindowStyle(hWndParent);
		if ( ! (dwStyle&WS_VISIBLE))
			break;
		hWnd = hWndParent;
	}

	return hWnd;
}

#if 0
HWND FindWindowForProcessID( DWORD dwProcessID )
{
	// look through all the top level windows for the window that has this processid.
	// NOTE: there may be more than 1. just take the first one.

	if ( dwProcessID == 0 )
		return NULL;

	// Get the first window handle.
	HWND hWnd = ::FindWindow(NULL,NULL);
	ASSERT(hWnd);	// must be a desktop!

	HWND hWndBest=NULL;
	int iBestScore = 0;

	// Loop until we find the target or we run out
	// of windows.
    for ( ;hWnd!=NULL; hWnd = ::GetWindow(hWnd, GW_HWNDNEXT))
	{
		// See if this window has a parent. If not,
        // it is a top-level window.
		if ( ::GetParent(hWnd) != NULL )
			continue;
		// This is a top-level window. See if
        // it has the target instance handle.
		DWORD dwProcessIDTest = 0;
		DWORD dwThreadID = ::GetWindowThreadProcessId( hWnd, &dwProcessIDTest );
        if ( dwProcessIDTest != dwProcessID )
			continue;

		// WS_VISIBLE?
		DWORD dwStyle = GetWindowStyle(hWnd);
		if ( ! ( dwStyle & WS_VISIBLE ))
			continue;
		RECT rect;
		if ( ! ::GetWindowRect(hWnd,&rect))
			continue;
		int iScore = ( rect.right - rect.left ) * ( rect.bottom - rect.top );
		if ( iScore > iBestScore)
		{
			iBestScore = iScore;
			hWndBest = hWnd;
		}
	}
	return hWndBest;
}
#endif

HINSTANCE CHttpLink_GotoURL( const TCHAR* pszURL, int iShowCmd )
{
	// iShowCmd = SW_SHOWNORMAL = 1
	// pszURL = _T ("http://www.yoururl.com"), 
	// or _T ("mailto:you@hotmail.com"), 

    // First try ShellExecute()
	HINSTANCE hInstRet = ::ShellExecute(NULL, _T("open"), pszURL, NULL,NULL, iShowCmd);
    return hInstRet;
}

bool CWndDC::ReleaseDC()
{
	if ( m_hDC == NULL )
		return true;
	bool bRet;
	if ( m_hWndOwner != HWND_BROADCAST )
	{
		bRet = ::ReleaseDC(m_hWndOwner,m_hDC) ? true : false;
		m_hWndOwner = HWND_BROADCAST;
	}
	else
	{
		// We created this therefore we must delete it.
		bRet = ::DeleteDC(m_hDC) ? true : false;
	}
	m_hDC = NULL;
	return bRet;
}
