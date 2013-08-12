//
// CSingleton.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
//	A singleton is a type of class of which only one instance may 
//  exist. This is commonly used for management classes used to 
//	control system-wide resources. 

#ifndef _INC_CSingleton_H
#define _INC_CSingleton_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CLogBase.h"

// disable the warning regarding 'this' pointers being used in 
// base member initializer list. Singletons rely on this action
#pragma warning(disable : 4355)
#pragma warning(disable : 4275)

class TAKSI_LINK CNonCopyable 
{
protected:
	//! Create a NonCopyable object
	CNonCopyable()
	{}

    //! Destroy a NonCopyable object
    ~CNonCopyable() 
	{}

private:
    //! Restrict the copy constructor
    CNonCopyable(const CNonCopyable&);

    //! Restrict the assignment operator
    const CNonCopyable& operator=(const CNonCopyable&);
};

template <class _T>
class CSingletonBase : public CNonCopyable
{
	// NOTE: _T = CSingletonBase based class = this
	// Statically created singleton.
public:
    CSingletonBase(_T* pObject)
    {
	    // the singleton must be constructed
		// with a reference to the controlled object
		ASSERT( pObject != NULL );
		if ( sm_pThe != NULL )
		{
			// THIS SHOULD NEVER HAPPEN!
			ASSERT( sm_pThe == NULL );
			return;
		}
		sm_pThe = pObject;
		ASSERT(sm_pThe==this);
    }
    ~CSingletonBase()
    {
	    // the singleton accessor
		if (sm_pThe!=NULL)
		{
			ASSERT(sm_pThe==this);
			sm_pThe = NULL;
		}
    }

    // the singleton accessor
    static _T* TAKSICALL get_Instance()
    {
		// This is a complex or abstract type that we cannot just create automatically.
		ASSERT(sm_pThe!=NULL);
        return(sm_pThe);
    }
    static _T& TAKSICALL Instance()
    {
        return(*get_Instance());
    }
	static _T& TAKSICALL I()
	{
        return(*get_Instance());
	}
	static inline bool IsInstanceCreated()
	{
		return( sm_pThe != NULL );
	}

protected:
    // Data...
    static _T* sm_pThe;
};

template <class _T> _T* CSingletonBase<_T>::sm_pThe = NULL;

template <class _T>
class CSingleton : public CSingletonBase<_T>
{
	// Dynamic singleton is created (on demand) if it does not yet exist.
	// see CSingletonMT for the thread safe version of this.
public:
	CSingleton(_T* pObject) : CSingletonBase<_T>(pObject)
	{
	}
	static _T* TAKSICALL get_Instance();
    static _T& TAKSICALL Instance()
    {
        return(*get_Instance());
    }
	static _T& TAKSICALL I()
	{
        return(*get_Instance());
	}
};

template <class _T> _T* TAKSICALL CSingleton<_T>::get_Instance()
{
	if ( sm_pThe == NULL )
	{
		// NOTE: This ensures proper creation order for singletons (Statics) that ref each other!
		//  during static init constructors.
		sm_pThe = new _T;	// not created yet ? then create it !
	}
    return(sm_pThe);
}

#endif
