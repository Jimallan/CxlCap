//
// CWaveACM.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
// A structure to encapsulate/abstract the ACM functions in Windows.
// Audio Compression Manager MsAcm32.lib
//
#ifndef _INC_CWaveACM_H
#define _INC_CWaveACM_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _WIN32
#include "CWaveFormat.h"
#include "CSingleton.h"
#include "CDll.h"

// Forward declares.
#include <mmreg.h>
#include <msacm.h>

#ifndef acmFormatTagDetails
#define acmFormatTagDetails acmFormatTagDetailsA
#endif // acmFormatTagDetails

class TAKSI_LINK CWaveACMStream
{
	// A conversion state for convert from one format to another.
	// Keep track of a single conversion stream process.
	// acmStreamOpen() session
public:
	CWaveACMStream();
	~CWaveACMStream();

	MMRESULT StreamOpen();
	void StreamClose();

#ifdef _DEBUG
	static bool TAKSICALL UnitTest();
#endif

public:
	CWaveFormat m_SrcForm;
	CWaveFormat m_DstForm;

	HACMSTREAM	m_hStream;		// stream handle to compression conversion device

	// Async ACM Compression conversion.
	// Progress in the stream.
	WAVE_BLOCKS_t m_SrcIndex;
	WAVE_BLOCKS_t m_DstIndex;
	int     m_iBuffers;			// Total outstanding buffers. (For async mode only)
	HRESULT m_hResStop;			// Stop feeding. ERROR_CANCELLED
};

class TAKSI_LINK CWaveACMInit
	: public CSingleton<CWaveACMInit>
	, public CDllFile
{
	// Run time binding to the MsAcm32.DLL.
	// Use /DELAYLOAD:MsAcm32.dll for win2k ??
public:
	CWaveACMInit();
	~CWaveACMInit();

	bool IsACMAttached() const
	{
		return( m_bGetVersion );
	}

	bool StartupACM();
	void ShutdownACM();

	int FormatDlg( HWND hwnd, CWaveFormat& Form, const TCHAR* pszTitle, DWORD dwEnum );
	MMRESULT GetFormatDetails( const WAVEFORMATEX FAR* pForm, ACMFORMATTAGDETAILS& details );

private:
	bool m_bGetVersion;	// Found the proper version?
};

#endif	// _WIN32
#endif	// _INC_CWaveACM_H
