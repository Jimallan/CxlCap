//
// TaksiCommon.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "TaksiCommon.h"
#include "CImageGDIP.h"

#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

