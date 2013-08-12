//
// Common.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//

#pragma once
#include "TaksiCommon.h"
#include "CLogBase.h"
#include "CDll.h"
#include "CNTHandle.h"
#include "CHookJump.h"
#include "CIniObject.h"

#ifndef COUNTOF			// similar to MFC _countof()
#define COUNTOF(a) 		(sizeof(a)/sizeof((a)[0]))	// dimensionof() ? = count of elements of an array
#endif
#define ISINTRESOURCE(p)	(HIWORD((DWORD)(p))==0)	// MAKEINTRESOURCE()
#define GETINTRESOURCE(p)	LOWORD((DWORD)(p))

typedef LONGLONG TIMEFAST_t;
extern TAKSI_LINK TIMEFAST_t GetPerformanceCounter();

inline bool FILE_IsDirSep( TCHAR ch ) { return(( ch == '/')||( ch == '\\')); }

TAKSI_LINK TCHAR* GetFileTitlePtr( TCHAR* pszPath );
TAKSI_LINK HRESULT CreateDirectoryX( const TCHAR* pszDir );

TAKSI_LINK char* Str_SkipSpace( const char* pszNon );
TAKSI_LINK bool Str_IsSpace( char ch );

TAKSI_LINK HINSTANCE CHttpLink_GotoURL( const TCHAR* pszURL, int iShowCmd );

TAKSI_LINK int Mem_ConvertToString( char* pszDst, int iSizeDstMax, const BYTE* pSrc, int iLenSrcBytes );
TAKSI_LINK int Mem_ReadFromString( BYTE* pDst, int iLengthMax, const char* pszSrc );

TAKSI_LINK HWND FindWindowForProcessID( DWORD dwProcessID );
TAKSI_LINK HWND FindWindowTop( HWND hWnd );
