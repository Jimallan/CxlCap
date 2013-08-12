//
// CWaveACM.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
// Interface connection to Windows WaveACM
//
#include "stdafx.h"
#include "CWaveACM.h"
#include "CLogBase.h"
#include <windowsx.h>
#include <tchar.h>

#pragma comment( lib, "MsAcm32.lib" )

CWaveACMInit::CWaveACMInit()
	: CSingleton<CWaveACMInit>(this)
	, m_bGetVersion(false)
{
	ASSERT( ! IsACMAttached());
}

CWaveACMInit::~CWaveACMInit()
{
}

bool CWaveACMInit::StartupACM()
{
	//  if we have already linked to the API's, then just succeed...
	if ( IsACMAttached())
		return true;

	HRESULT hRes = LoadDll( _T("MsAcm32.DLL"));
	if ( IS_ERROR(hRes))
		return false;

	//
	//  if the version of the ACM is *NOT* V2.00 or greater, then
	//  all other API's are unavailable--so don't waste time trying
	//  to link to them.
	//
	DWORD dwVersion = acmGetVersion();
	if ( HIWORD(dwVersion) < 0x0200 ) // 0x5000000
	{
		ShutdownACM();
		return(false);
	}

	m_bGetVersion = true;
	ASSERT( IsACMAttached());
	return true;
}

void CWaveACMInit::ShutdownACM()
{
	if ( ! IsACMAttached())
		return;
	FreeDll();
	m_bGetVersion = false;
	ASSERT( !IsACMAttached());
}

int CWaveACMInit::FormatDlg( HWND hwnd, CWaveFormat& Form, const TCHAR* pszTitle, DWORD dwEnum )
{
	// RETURN: IDCANCEL, IDOK
	// dwEnum = ACM_FORMATENUMF_CONVERT
	// 

	if ( !StartupACM())
	{
		return IDIGNORE;
	}

	ACMFORMATCHOOSE fmtc;
	ZeroMemory( &fmtc, sizeof(fmtc));

	DWORD dwMaxSize = 0;
	MMRESULT mmRes = acmMetrics( NULL, ACM_METRIC_MAX_SIZE_FORMAT, &dwMaxSize );
	if ( mmRes )
	{
		return IDRETRY;
	}

	LPWAVEFORMATEX lpForm = (LPWAVEFORMATEX) GlobalAllocPtr( GHND, dwMaxSize + sizeof( WAVEFORMATEX ));
	if ( lpForm == NULL )
	{
		return IDRETRY;
	}

	int iSizeCur = Form.get_FormatSize();
	if ( dwMaxSize < (DWORD) iSizeCur )
	{
		DEBUG_ERR(( "acmMetrics Failed %d<%d" LOG_CR, dwMaxSize, iSizeCur ));
		return IDRETRY;
	}

	memcpy( lpForm, Form.get_WF(), iSizeCur );
	if ( lpForm->wFormatTag == WAVE_FORMAT_PCM )
	{
		// Last part (cbSize) is optional. because we know it is 0
		lpForm->cbSize = 0;
	}

	static TCHAR szName[256]; // static necessary ??? 
	szName[0] = '\0';

	fmtc.cbStruct = sizeof( fmtc );
	fmtc.fdwStyle = ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT | ACMFORMATCHOOSE_STYLEF_SHOWHELP;
	fmtc.hwndOwner = hwnd;
	fmtc.pwfx = lpForm;
	fmtc.cbwfx = dwMaxSize;
	fmtc.pszTitle = pszTitle;
	fmtc.szFormatTag[0] = '\0';
	fmtc.szFormat[0] = '\0';
	fmtc.pszName = szName;
	fmtc.cchName = sizeof( szName );
	fmtc.hInstance = NULL;	// we are not using a template.

	fmtc.fdwEnum = dwEnum; 
	if ( dwEnum & ACM_FORMATENUMF_CONVERT ) // There is data to be converted.
	{
		fmtc.pwfxEnum = Form;
	}
	else
	{
		fmtc.pwfxEnum = NULL;
	}

	mmRes = acmFormatChoose( &fmtc );
	if ( ! mmRes )
	{
		// Copy results to Form.
		Form.SetFormat( lpForm );
	}

	GlobalFreePtr( lpForm );

	if ( mmRes )
	{
		if ( mmRes == ACMERR_CANCELED ) 
			return( IDCANCEL );
		DEBUG_ERR(( "acmFORMATCHOOSE %d" LOG_CR, mmRes ));
		return IDRETRY;
	}

	return IDOK;
}

MMRESULT CWaveACMInit::GetFormatDetails( const WAVEFORMATEX FAR* pForm, ACMFORMATTAGDETAILS& details )
{
	ZeroMemory( &details, sizeof(details));
	if ( pForm == NULL )
	{
		return 0;
	}
	if ( !StartupACM())
	{
		return ACMERR_NOTPOSSIBLE;
	}
	details.cbStruct = sizeof(details);
	details.dwFormatTag = pForm->wFormatTag;
	MMRESULT mmRes = acmFormatTagDetails( NULL, &details, ACM_FORMATTAGDETAILSF_FORMATTAG );
	return mmRes;
}

//*****************************************************

CWaveACMStream::CWaveACMStream()
	: m_hStream(NULL)
	, m_hResStop(S_OK)
	, m_iBuffers(0)
{
}

CWaveACMStream::~CWaveACMStream()
{
}

MMRESULT CWaveACMStream::StreamOpen()
{
	if ( ! CWaveACMInit::I().StartupACM())
	{
		return E_FAIL;
	}
	if ( m_hStream != NULL )
	{
		ASSERT(m_hStream==NULL);
		return E_FAIL;	// MUST close this properly!
		// StreamClose();
	}
	MMRESULT mmRes = acmStreamOpen(
		&m_hStream, NULL,
		m_SrcForm, m_DstForm, NULL,
		0L, 0L,
		ACM_STREAMOPENF_NONREALTIME );
	if ( mmRes != S_OK )
	{
		return( mmRes );
	}
	m_iBuffers = 0;			// Total outstanding buffers. (For async mode only)
	m_hResStop = S_OK;		// Stop feeding. ERROR_CANCELLED
	return S_OK;
}

void CWaveACMStream::StreamClose()
{
	if ( m_hStream == NULL )
		return;
	if ( ! CWaveACMInit::I().IsACMAttached())
	{
		return;
	}
	acmStreamClose( m_hStream, 0 );
	m_hStream = NULL;
}

//***************************************************

#ifdef _DEBUG

bool TAKSICALL CWaveACMStream::UnitTest() // static
{
	if ( ! CWaveACMInit::I().StartupACM())
		return false;

	CWaveACMStream test;
	test.m_SrcForm.SetFormatPCM();
	test.m_DstForm.SetFormatPCM();

	return true;
}

#endif
