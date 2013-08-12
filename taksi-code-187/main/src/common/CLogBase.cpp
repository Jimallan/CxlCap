//
// CLogBase.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "stdafx.h"
#include "CLogBase.h"

CLogBase* g_pLog = NULL;	// singleton // always access the base via pointer. (or g_Log)

int CLogBase::VDebugEvent( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszFormat, va_list args ) // static
{
	// default OutputDebugString event if no other handler. (this == NULL)

	LOGCHAR szTemp[ 1024 + _MAX_PATH ];	// assume this magic number is big enough. _MAX_PATH
	ASSERT( pszFormat && *pszFormat );
	_vsnprintf( szTemp, sizeof(szTemp)-1, pszFormat, args );
#ifdef _WINDOWS
	::OutputDebugStringA( szTemp );
#endif
	return 0;
}

int CLogBase::EventStr( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszMsg ) // virtual
{
	// override this to route the messages elsewhere
	if ( ! IsLogged( dwGroupMask, eLogLevel ))	// I don't care about these ?
		return( -1 );
	if ( pszMsg == NULL )
		return 0;
	if ( pszMsg[0] == '\0' )
		return 0;
	size_t iLen = strlen(pszMsg);
	ASSERT(iLen);
	bool bHasCRLF = ( pszMsg[iLen-1]=='\r' || pszMsg[iLen-1]=='\n' );

	if ( eLogLevel >= LOGL_ERROR )
	{
#ifdef _WINDOWS
		::OutputDebugStringA( "ERROR:" );
#endif
	}
#ifdef _WINDOWS
	::OutputDebugStringA( pszMsg );
	if (( dwGroupMask & LOG_GROUP_CRLF ) && ! bHasCRLF )
	{
		::OutputDebugStringA( LOG_CR );
	}
#else
	// LINUX debug log ?
	UNREFERENCED_PARAMETER(pszMsg);
#endif
	return( 0 );
}

int CLogBase::VEvent( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszFormat, va_list args ) // virtual
{
	// This is currently not overriden but just in case we might want to.

	if ( ! IsLogged( dwGroupMask, eLogLevel ))	// I don't care about these ?
		return( 0 );

	LOGCHAR szTemp[ 1024 + _MAX_PATH ];	// assume this magic number is big enough. _MAX_PATH
	ASSERT( pszFormat && *pszFormat );
	_vsnprintf( szTemp, sizeof(szTemp)-1, pszFormat, args );

	return( EventStr( dwGroupMask, eLogLevel, szTemp ));
}

int _cdecl CLogBase::Event( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszFormat, ... )
{
	int iRet = 0;
	va_list vargs;
	va_start( vargs, pszFormat );
	if ( this == NULL )
	{
		iRet = VDebugEvent( dwGroupMask, eLogLevel, pszFormat, vargs );
	}
	else
	{
		iRet = VEvent( dwGroupMask, eLogLevel, pszFormat, vargs );
	}
	va_end( vargs );
	return( iRet );
}

int _cdecl CLogBase::Debug_Error( const LOGCHAR* pszFormat, ... )
{
	int iRet = 0;
	va_list vargs;
	va_start( vargs, pszFormat );
	if ( this == NULL )
	{
		iRet = VDebugEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_ERROR, pszFormat, vargs );
	}
	else
	{
		iRet = VEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_ERROR, pszFormat, vargs );
	}
	va_end( vargs );
	return( iRet );
}

int _cdecl CLogBase::Debug_Warn( const LOGCHAR* pszFormat, ... )
{
	int iRet = 0;
	va_list vargs;
	va_start( vargs, pszFormat );
	if ( this == NULL )
	{
		iRet = VDebugEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_WARN, pszFormat, vargs );
	}
	else
	{
		iRet = VEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_WARN, pszFormat, vargs );
	}
	va_end( vargs );
	return( iRet );
}

int _cdecl CLogBase::Debug_Info( const LOGCHAR* pszFormat, ... )
{
	int iRet = 0;
	va_list vargs;
	va_start( vargs, pszFormat );
	if ( this == NULL )
	{
		iRet = VDebugEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_INFO, pszFormat, vargs );
	}
	else
	{
		iRet = VEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_INFO, pszFormat, vargs );
	}
	va_end( vargs );
	return( iRet );
}

int _cdecl CLogBase::Debug_Trace( const LOGCHAR* pszFormat, ... )
{
	int iRet = 0;
	va_list vargs;
	va_start( vargs, pszFormat );
	if ( this == NULL )
	{
		iRet = VDebugEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_TRACE, pszFormat, vargs );
	}
	else
	{
		iRet = VEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_TRACE, pszFormat, vargs );
	}
	va_end( vargs );
	return( iRet );
}

//**************************************************************************

void CLogBase::Assert_CheckFail( const char* pExp, const char* pFile, unsigned uLine )
{
	// This is _assert() is MS code.
#ifdef _DEBUG
#if _MSC_VER >= 1400
	// _wassert( pExp, pFile, uLine );
#elif _MSC_VER >= 1000
	_assert( pExp, pFile, uLine );
#else
	// CGExceptionAssert::Throw( pExp, pFile, uLine );
#endif
#else
	if ( this == NULL )
	{
		// VDebugEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_INFO, pszFormat, vargs );
	}
	else
	{
		Event( LOG_GROUP_DEBUG, LOGL_CRIT, "Assert Fail:'%s' file '%s', line %d" LOG_CR, pExp, pFile, uLine );
	}
#endif
}

void CLogBase::Debug_CheckFail( const char* pExp, const char* pFile, unsigned uLine )
{
	// A 'softer' version of assert. non-fatal checks
#ifdef _DEBUG
	if ( this == NULL )
	{
		// VDebugEvent( LOG_GROUP_DEBUG|LOG_GROUP_CRLF, LOGL_INFO, pszFormat, vargs );
	}
	else
	{
		Event( LOG_GROUP_DEBUG, LOGL_ERROR, "Check Fail:'%s' file '%s', line %d" LOG_CR, pExp, pFile, uLine );
	}
#endif
}

//************************************************************************

CLogFiltered::CLogFiltered( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel ) :
	m_Filter(dwGroupMask,eLogLevel)
{
	if ( g_pLog == NULL )
		g_pLog = this;
}
CLogFiltered::~CLogFiltered()
{
	m_Filter.put_LogLevel(LOGL_NONE);
}

