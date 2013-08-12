//
// TaksiCommon.h
//

#pragma once

#ifndef TAKSI_LINK
#if defined(_AFXDLL) 	// can be defined to make this all a static lib
#define TAKSI_LINK
#else
#define TAKSI_LINK __declspec(dllimport)	// default is to include from a DLL
#endif
#endif

#define TAKSICALL _cdecl

#ifndef ASSERT
#define ASSERT assert
#endif

typedef DWORD TIMESYS_t;		// GetTickCount() = mSec
#define TIMESYS_FREQ 1000
