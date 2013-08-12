//
// stdafx.h
// included in both EXE and DLL
//
#ifndef _INC_STDAFX_H
#define _INC_STDAFX_H
#if _MSC_VER > 1000
#pragma once
#endif

#define _WIN32_IE 0x0500
#define STRICT 

#define USE_DIRECTI
//#define USE_DIRECTX8			// remove this to compile if u dont have the DirectX 8 SDK
#define USE_DIRECTX9			// remove this to compile if u dont have the DirectX 9 SDK
#define USE_LOGFILE

// This causes us not to load under Win2k, so use /DELAYLOAD:gdiplus.dll
#define USE_GDIP		// use Gdiplus::Bitmap to save images as PNG (or JPEG)
#define USE_AUDIO

// System includes always go first.
#include <windows.h>
#include <windef.h>
#include <tchar.h>
#include <rpc.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#ifdef TAKSIDLL_EXPORTS
#define LIBSPEC __declspec(dllexport)
#else
#define LIBSPEC __declspec(dllimport)
#endif // TAKSIDLL_EXPORTS

#include "../common/Common.h"
#include "../common/IRefPtr.h"
#include "../common/CLogBase.h"
#include "../common/CAVIFile.h"	// CAVIFile
#include "../common/CWaveACM.h"

#include "TaksiVersion.h"
#include "Config.h"
#include "TaksiShared.h"

#endif // _INC_STDAFX_H
