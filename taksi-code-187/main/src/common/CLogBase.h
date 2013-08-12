//
// CLogBase.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#ifndef _INC_CLogBase_H
#define _INC_CLogBase_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "TaksiCommon.h"
#include <stdarg.h>
#include <assert.h>

typedef char LOGCHAR;	// just use UTF8 for logs, dont bother with UNICODE.
#ifdef _WIN32
#define LOG_CR	"\r\n"	// CRLF for DOS/Windows format log files.
#else
#define LOG_CR	"\n"	// UNIX format carriage return.
#endif

enum LOGL_TYPE
{
	// criticalness/importance level of an logged event. similar to Log4J
	LOGL_ANY    = 0,	// Anything
	LOGL_TRACE	= 1,	// low level debug trace.
	LOGL_INFO	= 2,	// Misc major events.
	LOGL_WARN	= 3,	// strange.
	LOGL_ERROR	= 4, 	// non-fatal errors. can continue.
	LOGL_CRIT	= 5, 	// critical. might not continue.
	LOGL_FATAL	= 6, 	// fatal error ! cannot continue.
	LOGL_NONE	= 7,	// filter everything. i want to see nothing.
};

typedef DWORD LOG_GROUP_MASK;
enum LOG_GROUP_TYPE_
{
	// classify log event by subject matter. (bitmask so event can belong to several groups)

	LOG_GROUP_DEBUG		= 0x0001,	// Unclassified debug stuff.
	LOG_GROUP_TEMP		= 0x0002,	// Real time status (don't bother to log)
	LOG_GROUP_INTERNAL	= 0x0004,	// Do not echo this message as it may relate to my own internals (i.e.feedback loop)
	LOG_GROUP_NOTICE	= 0x0008,	// Public notice for all. result of user action.
	LOG_GROUP_CHEAT		= 0x0040,	// Hackers
	LOG_GROUP_ACCOUNTS	= 0x0080,
	LOG_GROUP_INIT		= 0x0100,	// startup/exit stuff.
	LOG_GROUP_SAVE		= 0x0200,	// world save status.
	LOG_GROUP_CLIENTS	= 0x0400,	// all clients as they log in and out.
	LOG_GROUP_SCRIPT	= 0x0800,	// this is from some sort of script exec.

	// ... Subject Groups may be added depending on the app.
	//

	LOG_GROUP_CRLF		= 0x80000000,	// Always add a CR NL to the end of this string.
	LOG_GROUP_ALL		= 0x7FFFFFFF,
};

class TAKSI_LINK CLogBase
{
	// A very generic sink for log events.
public:
	virtual bool IsLogged( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel ) const
	{
		// overload this to allow filtering of the messages.
		UNREFERENCED_PARAMETER(dwGroupMask);
		UNREFERENCED_PARAMETER(eLogLevel);
		return true;
	}

	// overload these to control where the messages go.
	virtual int EventStr( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszMsg );
	virtual int VEvent( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszFormat, va_list args );

	// Helpers.
	int _cdecl Event( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszFormat, ... );

	int _cdecl Debug_Error( const LOGCHAR* pszFormat, ... );
	int _cdecl Debug_Warn( const LOGCHAR* pszFormat, ... );
	int _cdecl Debug_Info( const LOGCHAR* pszFormat, ... );
	int _cdecl Debug_Trace( const LOGCHAR* pszFormat, ... );

	// Log caught exception events
	void Assert_CheckFail( const char* pExp, const char* pFile, unsigned uLine );
	void Debug_CheckFail( const char* pExp, const char* pFile, unsigned uLine );

protected:
	// default OutputDebugString event if no other handler.
	static int _stdcall VDebugEvent( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszFormat, va_list args );
};

//***********************************************************************************

class TAKSI_LINK CLogEventFilter
{
	// control flow of events. What events do I want to see?
public:
	CLogEventFilter( LOG_GROUP_MASK dwGroupMaskOr=LOG_GROUP_ALL, LOGL_TYPE eLogLevel=LOGL_INFO ) :
		m_dwGroupMaskOr(dwGroupMaskOr),	// Level of log detail messages. IsLogMsg()
		m_eLogLevel(eLogLevel)		// Importance level.
	{
	}

	LOG_GROUP_MASK get_LogGroupMask() const
	{
		return( m_dwGroupMaskOr );
	}
	void put_LogGroupMask( LOG_GROUP_MASK dwGroupMask )
	{
		// What types of info do we want to filter for.
		m_dwGroupMaskOr = dwGroupMask;
	}
	bool IsLoggedGroupMask( LOG_GROUP_MASK dwGroupMask ) const
	{
		return(( m_dwGroupMaskOr & dwGroupMask ) ? true : false );
	}

	LOGL_TYPE get_LogLevel() const
	{
		// Min level to show.
		return( m_eLogLevel );
	}
	void put_LogLevel( LOGL_TYPE eLogLevel )
	{
		// What level of importance do we want to filter for.
		m_eLogLevel = eLogLevel;
	}
	bool IsLoggedLevel( LOGL_TYPE eLogLevel ) const
	{
		// level = LOGL_INFO (higher is more important
		return( eLogLevel >= m_eLogLevel );
	}

	bool IsLogged( LOG_GROUP_MASK dwGroupMaskOr, LOGL_TYPE eLogLevel ) const
	{
		return( IsLoggedGroupMask(dwGroupMaskOr) || IsLoggedLevel(eLogLevel));
	}

protected:
	LOG_GROUP_MASK m_dwGroupMaskOr;	// show ALL messages in these catagories. (reguardless of level)
	LOGL_TYPE m_eLogLevel;			// Min Importance level to see
};

class TAKSI_LINK CLogFiltered : public CLogBase
{
	// Log event stream. (destination is independant)
	// Actual CLogEvent may be routed or filtered to multiple destinations from here.
	// May include __LINE__ or __FILE__ macro as well ?
public:
	CLogFiltered( LOG_GROUP_MASK dwGroupMask=LOG_GROUP_ALL, LOGL_TYPE eLogLevel=LOGL_INFO );
	virtual ~CLogFiltered();

	virtual bool IsLogged( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel ) const
	{
		return m_Filter.IsLogged(dwGroupMask,eLogLevel);
	}

public:
	CLogEventFilter m_Filter;	// filter what goes out to ALL viewers
};

extern TAKSI_LINK CLogBase* g_pLog;	// singleton - always access the base via pointer. (or g_Log)

//***********************************************************************************
// Stuff that should stay in release mode.
#define LOG_WARN(_x_)		g_pLog->Debug_Warn _x_
#define LOG_MSG(_x_)		g_pLog->Debug_Info _x_

#ifndef DEBUG_ERR
#define DEBUG_ERR(_x_)		g_pLog->Debug_Error _x_
#ifdef _DEBUG
#define DEBUG_WARN(_x_)		g_pLog->Debug_Warn _x_
#define DEBUG_MSG(_x_)		g_pLog->Debug_Info _x_
#define DEBUG_TRACE(_x_)	g_pLog->Debug_Trace _x_
#else
#define DEBUG_WARN(_x_)
#define DEBUG_MSG(_x_)
#define DEBUG_TRACE(_x_)				// this allows all the variable args to be compiled out
#endif
#endif

#if defined(_DEBUG) || defined(_DEBUG_FAST)
// In linux, if we get an access violation, an exception isn't thrown.
// Instead, we get a SIG_SEGV, and the process cores.  
#undef ASSERT
#define ASSERT(exp)				(void)((exp) || (g_pLog->Assert_CheckFail(#exp, __FILE__, __LINE__), 0))
#undef DEBUG_CHECK
#define DEBUG_CHECK(exp)		(void)((exp) || (g_pLog->Debug_CheckFail(#exp, __FILE__, __LINE__), 0))
#undef DEBUG_ASSERT
#define DEBUG_ASSERT(exp,sDesc)	(void)((exp) || (g_pLog->Debug_CheckFail(sDesc, __FILE__, __LINE__), 0))

#else	// _DEBUG

// NON DEBUG
#ifndef ASSERT
#define ASSERT(exp) ((void)0)
#endif	// ASSERT
#ifndef DEBUG_CHECK
#define DEBUG_CHECK(exp) ((void)0)
#endif	// DEBUG_CHECK
#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT(exp) ((void)0)
#endif // DEBUG_ASSERT

#endif	// ! _DEBUG

#endif	// _INC_CLogBase_H
