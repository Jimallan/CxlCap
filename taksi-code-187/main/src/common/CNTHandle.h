//
// CNTHandle.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#ifndef _INC_CNTHandle_H
#define _INC_CNTHandle_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "TaksiCommon.h"

struct TAKSI_LINK CNTHandle
{
	// Wrap ownership of a Windows HANDLE
	CNTHandle( HANDLE h=INVALID_HANDLE_VALUE ) 
		: m_h(h)
	{}
	~CNTHandle()
	{
		CloseHandleLast();
	}
	bool IsValidHandle() const
	{
		return( m_h != NULL && m_h != INVALID_HANDLE_VALUE );
	}
	bool CloseHandle()
	{
		if ( ! IsValidHandle())
			return true;
		HANDLE h = m_h;
		m_h=INVALID_HANDLE_VALUE;
		return( ::CloseHandle(h) ? true : false );
	}
	void AttachHandle( HANDLE h )
	{
		CloseHandleLast();
		m_h = h;
	}
	HANDLE DetachHandle()
	{
		HANDLE h = m_h;
		m_h = INVALID_HANDLE_VALUE;
		return h;
	}
	operator HANDLE () const
	{
		return m_h;
	}
private:
	void CloseHandleLast()
	{
		if ( ! IsValidHandle())
			return;
		::CloseHandle(m_h);
	}
public:
	HANDLE m_h;
};

class CNTEvent : public CNTHandle
{
	// Like MFC class CEvent
	// used for multi thread synchronization.
public:
	bool CreateEvent( LPSECURITY_ATTRIBUTES lpEventAttributes = NULL,
		IN bool bManualReset = true,	// requires use of the ResetEvent function set the state to nonsignaled.
		IN bool bInitialState = false,	// false = wait.
		IN LPCTSTR lpName = NULL )
	{
		AttachHandle( ::CreateEvent(lpEventAttributes, bManualReset, bInitialState, lpName ));
		return( IsValidHandle());
	}
	bool SetEvent() const
	{
		// Sets the state of the event to signaled, releasing any waiting threads.
		ASSERT(IsValidHandle());
		return ::SetEvent(m_h) ? true : false;
	}
	bool PulseEvent() const
	{
		// Sets the state of the event to signaled (available), releases any waiting threads, and resets it to nonsignaled (unavailable) automatically.
		ASSERT(IsValidHandle());
		return ::PulseEvent(m_h) ? true : false;
	}
	bool ResetEvent() const
	{
		// Sets the state of the event to nonsignaled until explicitly set to signaled by the SetEvent member function.
		// This causes all threads wishing to access this event to wait.
		// This member function is not used by automatic events.
		ASSERT(IsValidHandle());
		return ::ResetEvent(m_h) ? true : false;
	}
	DWORD WaitForSingleObject( DWORD dwMilliseconds ) const
	{
		// dwMilliseconds = -1 = wait forever.
		// RETURN: WAIT_ABANDONED, WAIT_TIMEOUT or WAIT_OBJECT_0
		ASSERT(IsValidHandle());
		return ::WaitForSingleObject( m_h, dwMilliseconds );
	}
#if 0
	bool IsSignaled() const
	{
		ASSERT(IsValidHandle());
		// no good way to do this!
		return ??;
	}
#endif
};

#endif
