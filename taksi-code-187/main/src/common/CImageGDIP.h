//
// CImageGDIP.h
//
#ifndef _INC_CImageGDIP_H
#define _INC_CImageGDIP_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CDll.h"
#include <gdiplus.h>

class TAKSI_LINK CImageGDIPInt : public CDllFile
{
	// binding to the gdiplus.dll.
	// Use /DELAYLOAD:gdiplus.dll for win2k !!
public:
	CImageGDIPInt();
	~CImageGDIPInt();

	bool AttachGDIPInt();
	void DetachGDIPInt();

	HRESULT GetEncoderClsid( const char* pszFormatExt, CLSID* pClsid );

public:
	bool m_bFailedLoad;
	ULONG_PTR m_Token;
};

extern TAKSI_LINK CImageGDIPInt g_gdiplus;
#endif	// _INC_CImageGDIP_H
