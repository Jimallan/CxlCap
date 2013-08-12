//
// CThreadLockedLong.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//

#ifndef _INC_CThreadLockedLong_H
#define _INC_CThreadLockedLong_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
#include "TaksiCommon.h"

#if defined(_MT) && defined(_WIN32)
extern "C"
{
   LONG  __cdecl _InterlockedIncrement(LONG volatile *Addend);
   LONG  __cdecl _InterlockedDecrement(LONG volatile *Addend);
   LONG  __cdecl _InterlockedCompareExchange(LONG volatile *Dest, LONG Exchange, LONG Comp);
   LONG  __cdecl _InterlockedExchange(LONG volatile *Target, LONG Value);
   LONG  __cdecl _InterlockedExchangeAdd(LONG volatile *Addend, LONG Value);
}
#pragma intrinsic( _InterlockedIncrement, _InterlockedDecrement, _InterlockedExchange, _InterlockedCompareExchange )
#endif

class TAKSI_LINK CThreadLockedLong
{
	// thread interlocked int
	// thread safe unitary actions on a LONG value
	// NOTE: These use inline _asm code and is VERY fast!
public:
	CThreadLockedLong( LONG lValue = 0 ) : m_lValue(lValue)
	{
	}

#if defined(_MT) && defined(_WIN32)
	LONG Inc()
	{
		return _InterlockedIncrement(&m_lValue);
	}
	LONG Dec()
	{
		return _InterlockedDecrement(&m_lValue);
	}
	LONG Exchange( LONG lValue )
	{
		return _InterlockedExchange(&m_lValue,lValue);
	}
	LONG CompareExchange( LONG lValue, LONG lComperand = 0 )
	{
		// only if current m_lValue is lComperand set the new m_lValue to lValue
		// RETURN: previous value.
		return _InterlockedCompareExchange( &m_lValue, lValue, lComperand);
	}
#else // Not sure how to make this work for LINUX.
	LONG Inc()
	{
		return ++m_lValue;
	}
	LONG Dec()
	{
		return --m_lValue;
	}
	LONG Exchange( LONG lValue )
	{
		LONG lValPrv = m_lValue;
		m_lValue = lValue;
		return lValPrv;
	}
	LONG CompareExchange( LONG lValue, LONG lComperand = 0 )
	{
		LONG lValPrv = m_lValue;
		if ( lComperand == lValPrv )
		{
			m_lValue = lValue;
		}
		return lValPrv;
	}
#endif

public:
//#pragma pack(push, 8)	// Add def for mingw32 or other non-ms compiler to align on 64-bit boundary
	LONG volatile m_lValue;
//#pragma pack(pop)
};

class TAKSI_LINK CThreadReentrant
{
	// A check for reentrancy. even on the same thread.
	// define an instance of this on the stack.
public:
	CThreadReentrant( CThreadLockedLong& count ) :
		m_count(count)
	{
		m_count.Inc();
	}
	~CThreadReentrant()
	{
		m_count.Dec();
	}
private:
	CThreadLockedLong& m_count;	// pointer to the 'static' count.
};

#endif
