//
// config.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include <ctype.h>	// isspace()
#include "../common/CWaveDevice.h"

#ifndef __PLACEMENT_NEW_INLINE
#define __PLACEMENT_NEW_INLINE
inline void*__cdecl operator new(size_t, void*_P)
	{return (_P); }
#if     _MSC_VER >= 1200	// VS6
inline void __cdecl operator delete(void*, void*)
	{return; }
#endif
#endif

const char* CTaksiConfig::sm_Props[TAKSI_CFGPROP_QTY+1] = 
{
#define ConfigProp(a,b,c) #a,
#include "../ConfigProps.tbl"
#undef ConfigProp
	NULL,
};

static bool Str_GetQuoted( TCHAR* pszDest, int iLenMax, const char* pszSrc )
{
	// NOTE: Quoted string might contain UTF8 chars? so might have protected '"' ??
	char* pszStartQuote = strchr( const_cast<char*>(pszSrc), '\"');
	if (pszStartQuote == NULL) 
		return false;
	char* pszEndQuote = strchr( pszStartQuote + 1, '\"' );
	if (pszEndQuote == NULL)
		return false;
	int iLen = (int)( pszEndQuote - pszStartQuote ) - 1;
	if ( iLen > iLenMax-1 )
		iLen = iLenMax-1;
#ifdef _UNICODE
	// convert to unicode.
	iLen = ::MultiByteToWideChar( CP_UTF8, 0, pszStartQuote + 1, iLen, pszDest, iLen );
#else
	strncpy( pszDest, pszStartQuote + 1, iLen );
#endif
	pszDest[iLen] = '\0';
	return true;
}

static int Str_SetQuoted( char* pszDest, int iLenMax, const TCHAR* pszSrc )
{
#ifdef _UNICODE
	// convert from unicode.
	return _snprintf(pszDest, iLenMax, "\"%S\"", pszSrc);
#else
	return _snprintf(pszDest, iLenMax, "\"%s\"", pszSrc);
#endif
}

int Str_MakeFilePath( TCHAR* pszPath, int iMaxCount, const TCHAR* pszDir, const TCHAR* pszName )
{
	return _sntprintf( pszPath, iMaxCount-1, 
		_T("%s%s"), 
		pszDir, pszName );
}

//*****************************************************

const char* CTaksiConfigCustom::sm_Props[TAKSI_CUSTOM_QTY+1] = // TAKSI_CUSTOM_TYPE
{
	"FrameRate",
	"FrameWeight",
	"Pattern",		// TAKSI_CUSTOM_Pattern
	NULL,
};

int CTaksiConfigCustom::PropGet( int eProp, char* pszValue, int iSizeMax ) const
{
	// TAKSI_CUSTOM_TYPE 
	switch ( eProp )
	{
	case TAKSI_CUSTOM_FrameRate:
		return _snprintf(pszValue, iSizeMax, "%g", m_fFrameRate);
	case TAKSI_CUSTOM_FrameWeight:
		return _snprintf(pszValue, iSizeMax, "%g", m_fFrameWeight);
	case TAKSI_CUSTOM_Pattern:
		return Str_SetQuoted( pszValue, iSizeMax, m_szPattern );
	default:
		DEBUG_ERR(("CTaksiConfigCustom::PropGet bad code %d" LOG_CR, eProp ));
		ASSERT(0);
		break;
	}
	return -1;
}
bool CTaksiConfigCustom::PropSet( int eProp, const char* pszValue )
{
	// TAKSI_CUSTOM_TYPE 
	switch ( eProp )
	{
	case TAKSI_CUSTOM_FrameRate:
		m_fFrameRate = (float) atof(pszValue);
		break;
	case TAKSI_CUSTOM_FrameWeight:
		m_fFrameWeight = (float) atof(pszValue);
		break;
	case TAKSI_CUSTOM_Pattern:
		if (! Str_GetQuoted( m_szPattern, COUNTOF(m_szPattern), pszValue ))
			return false;
		_tcslwr( m_szPattern );
		break;
	default:
		DEBUG_ERR(("CTaksiConfigCustom::PropSet bad code %d" LOG_CR, eProp ));
		ASSERT(0);
		return false;
	}
#if 0 // def _DEBUG
	char szTmp[_MAX_PATH*2];
	if ( PropGet(eProp,szTmp,sizeof(szTmp)) < 0 )
	{
		strcpy( szTmp, "<NA>" );
	}
	LOG_MSG(( "PropSet[%s] '%s'='%s'" LOG_CR, m_szAppId, sm_Props[eProp], szTmp ));
#endif
	return true;
}

//*****************************************************************************

void CTaksiConfigData::InitConfig()
{
	m_szCaptureDir[0] = '\0';
	m_bDebugLog = false;
	m_szFileNamePostfix[0] = '\0';

	// Video Format
	m_fFrameRateTarget=10;
	m_VideoCodec.InitCodec();
	m_bVideoCodecMsg = false;
	m_bVideoHalfSize = true;

	m_AudioFormat.InitFormatEmpty();
	m_iAudioDevice = WAVE_DEVICE_NONE;

	m_wHotKey[TAKSI_HOTKEY_ConfigOpen]=0x0;
	m_wHotKey[TAKSI_HOTKEY_HookModeToggle]=0x75;
	m_wHotKey[TAKSI_HOTKEY_IndicatorToggle]=0x74;
	m_wHotKey[TAKSI_HOTKEY_RecordBegin]=0x7b;
	m_wHotKey[TAKSI_HOTKEY_RecordPause]=0;
	m_wHotKey[TAKSI_HOTKEY_RecordStop]=0x7b;
	m_wHotKey[TAKSI_HOTKEY_Screenshot]=0x77;
	m_wHotKey[TAKSI_HOTKEY_SmallScreenshot]=0x76;
	m_bUseDirectInput = true;
	m_bUseKeyboard = true;

	memset( m_abUseGAPI, true, sizeof(m_abUseGAPI));	// set all to true.
	m_abUseGAPI[TAKSI_GAPI_DESKTOP] = false;
	m_bGDIFrame = true;
	m_bUseOverheadCompensation = false;
	m_bGDICursor = false;

	lstrcpy( m_szImageFormatExt, _T("png") );	// vs png,jpeg or bmp
	m_bShowIndicator = true;
	m_ptMasterWindow.x = 0;
	m_ptMasterWindow.y = 0;
	m_bMasterTopMost = false;

	DEBUG_MSG(("CTaksiConfig::InitConfig" LOG_CR ));
}

void CTaksiConfigData::CopyConfig( const CTaksiConfigData& config )
{
	_tcsncpy( m_szCaptureDir, config.m_szCaptureDir, COUNTOF(m_szCaptureDir)-1);
	m_bDebugLog = config.m_bDebugLog;
	_tcsncpy( m_szFileNamePostfix, config.m_szFileNamePostfix, COUNTOF(m_szFileNamePostfix)-1 );

	m_fFrameRateTarget = config.m_fFrameRateTarget;
	m_VideoCodec.CopyCodec( config.m_VideoCodec );
	m_bVideoHalfSize = config.m_bVideoHalfSize;
	m_bVideoCodecMsg = config.m_bVideoCodecMsg;

	m_iAudioDevice = config.m_iAudioDevice;	// audio input device. WAVE_MAPPER = -1, WAVE_DEVICE_NONE = -2. CWaveRecorder
	m_AudioFormat.SetFormat( config.m_AudioFormat );

	memcpy( m_wHotKey, config.m_wHotKey, sizeof(m_wHotKey));
	m_bUseDirectInput = config.m_bUseDirectInput;
	m_bUseKeyboard = config.m_bUseKeyboard;

	memcpy( m_abUseGAPI, config.m_abUseGAPI, sizeof(m_abUseGAPI));	// hook GDI mode at all?
	m_bGDIFrame = config.m_bGDIFrame;	// record the frame of GDI windows or not ?
	m_bUseOverheadCompensation = config.m_bUseOverheadCompensation;
	m_bGDICursor = config.m_bGDICursor;

	// CAN NOT be set from CGuiConfig directly
	m_bShowIndicator = config.m_bShowIndicator;
	m_ptMasterWindow = config.m_ptMasterWindow;	// previous position of the master EXE window.
	m_bMasterTopMost = config.m_bMasterTopMost;

	// CANT COPY m_pCustomList for interproces access ??
	DEBUG_MSG(("CTaksiConfig::CopyConfig '%s'" LOG_CR, m_szCaptureDir ));
}

bool CTaksiConfigData::SetHotKey( TAKSI_HOTKEY_TYPE eHotKey, WORD wHotKey )
{
	// Signal to unhook from IDirect3DDeviceN methods 
	// wHotKey = LOBYTE(virtual key code) + HIBYTE(HOTKEYF_ALT)
	if ( eHotKey < 0 || eHotKey >= TAKSI_HOTKEY_QTY )
		return false;
	if ( m_wHotKey[eHotKey] == wHotKey )
		return false;
	m_wHotKey[eHotKey] = wHotKey;
	return true;
}

WORD CTaksiConfigData::GetHotKey( TAKSI_HOTKEY_TYPE eHotKey ) const
{
	// wHotKey = LOBYTE(virtual key code) + HIBYTE(HOTKEYF_ALT)
	if ( eHotKey < 0 || eHotKey >= TAKSI_HOTKEY_QTY )
		return 0;
	return m_wHotKey[eHotKey];
}

HRESULT CTaksiConfigData::FixCaptureDir()
{
	// RETURN: 
	//  <0 = bad name.
	//  S_FALSE = no change.
	//  S_OK = changed!

	int iLen = lstrlen(m_szCaptureDir);
	if (iLen<=0)
	{
		// If capture directory is still unknown at this point
		// set it to the current APP directory then.
		if ( ::GetModuleFileName(NULL,m_szCaptureDir, sizeof(m_szCaptureDir)-1 ) <= 0 )
		{
			return HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
		}
		// strip off file name
		TCHAR* pTitle = GetFileTitlePtr(m_szCaptureDir);
		if(pTitle==NULL)
			return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
		*pTitle = '\0';
		return S_OK; // changed
	}

	// make sure it ends with backslash.
	if ( ! FILE_IsDirSep( m_szCaptureDir[iLen-1] )) 
	{
		lstrcat(m_szCaptureDir, _T("\\"));
		return S_OK;	// changed
	}

	// make sure it actually exists! try to create it if not.
	HRESULT hRes = CreateDirectoryX( m_szCaptureDir );
	if ( FAILED(hRes))
	{
		return hRes;
	}
	return S_FALSE;	// no change.
}

//************************************************************

CTaksiConfigCustom* CTaksiConfig::CustomConfig_Alloc() // static
{
	// NOTE: This should be in process shared memory ?
	CTaksiConfigCustom* pConfig = (CTaksiConfigCustom*) ::HeapAlloc( 
		::GetProcessHeap(), 
		HEAP_ZERO_MEMORY, sizeof(CTaksiConfigCustom));
	return new((void*) pConfig ) CTaksiConfigCustom();	// init the struct properly
}

void CTaksiConfig::CustomConfig_Delete( CTaksiConfigCustom* pConfig ) // static
{
	if ( pConfig == NULL )
		return;
	::HeapFree( ::GetProcessHeap(), 0, pConfig);
}

CTaksiConfigCustom* CTaksiConfig::CustomConfig_FindPattern( const TCHAR* pszProcessFile ) const
{
	// must always be all lower case. _tcslwr
	for (CTaksiConfigCustom* p=m_pCustomList; p!=NULL; p=p->m_pNext )
	{
		// try to match the pattern with processfile
		if ( _tcsstr(pszProcessFile, p->m_szPattern)) 
			return p;
	}
	return NULL;
}

CTaksiConfigCustom* CTaksiConfig::CustomConfig_FindAppId( const TCHAR* pszAppId ) const
{
	// Looks up a custom config in a list. If not found return NULL
	for (CTaksiConfigCustom* p=m_pCustomList; p!=NULL; p=p->m_pNext )
	{
		if ( ! lstrcmp(p->m_szAppId, pszAppId))
			return p;
	}
	return NULL;
}

CTaksiConfigCustom* CTaksiConfig::CustomConfig_Lookup( const TCHAR* pszAppId, bool bCreate )
{
	// Looks up a custom config in a list. If not found, creates one
	// and inserts at the beginning of the list.
	//
	CTaksiConfigCustom* p = CustomConfig_FindAppId(pszAppId);
	if (p)
		return p;
	if ( ! bCreate)
		return NULL;

	// not found. Therefore, insert a new one
	p = CustomConfig_Alloc();
	if (p == NULL)
		return NULL;

	_tcsncpy(p->m_szAppId, pszAppId, COUNTOF(p->m_szAppId)-1);
	p->m_pNext = m_pCustomList;
	m_pCustomList = p;
	return p;
}

void CTaksiConfig::CustomConfig_DeleteAppId(const TCHAR* pszAppId)
{
	// Deletes a custom config from the list and frees its memory. 
	// If not found, nothing is done.
	//
	CTaksiConfigCustom* pPrev = NULL;
	CTaksiConfigCustom* pCur = m_pCustomList;
	while (pCur != NULL)
	{
		CTaksiConfigCustom* pNext = pCur->m_pNext;
		if ( pszAppId == NULL || ! lstrcmp(pCur->m_szAppId, pszAppId)) 
		{
			if ( pPrev == NULL )
				m_pCustomList = pNext;
			else
				pPrev->m_pNext = pNext;
			CustomConfig_Delete(pCur);
		}
		else
		{
			pPrev = pCur;
		}
		pCur = pNext;
	}
}

//************************************************************

CTaksiConfig::CTaksiConfig()
	: m_pCustomList(NULL)
{
	PropsInit();
	InitConfig();
}

CTaksiConfig::~CTaksiConfig()
{
	CustomConfig_DeleteAppId(NULL);		// Free ALL memory taken by custom configs
	m_VideoCodec.DestroyCodec();	// cleanup VFW resources. 
}

static const char* GetBoolStr( bool bVal )
{
	return bVal? "1" : "0";
}

int CTaksiConfig::PropGet( int eProp, char* pszValue, int iSizeMax ) const
{
	// TAKSI_CFGPROP_TYPE
	switch (eProp)
	{
	case TAKSI_CFGPROP_DebugLog:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bDebugLog));
	case TAKSI_CFGPROP_CaptureDir:
		return Str_SetQuoted(pszValue, iSizeMax, m_szCaptureDir );
	case TAKSI_CFGPROP_FileNamePostfix:
		return Str_SetQuoted(pszValue, iSizeMax, m_szFileNamePostfix );
	case TAKSI_CFGPROP_ImageFormatExt:
		return Str_SetQuoted(pszValue, iSizeMax, m_szImageFormatExt );
	case TAKSI_CFGPROP_MovieFrameRateTarget:
		return _snprintf(pszValue, iSizeMax, "%g", m_fFrameRateTarget);
	case TAKSI_CFGPROP_VKey_ConfigOpen:
	case TAKSI_CFGPROP_VKey_HookModeToggle:
	case TAKSI_CFGPROP_VKey_IndicatorToggle:
	case TAKSI_CFGPROP_VKey_RecordBegin:
	case TAKSI_CFGPROP_VKey_RecordPause:
	case TAKSI_CFGPROP_VKey_RecordStop:
	case TAKSI_CFGPROP_VKey_Screenshot:
	case TAKSI_CFGPROP_VKey_SmallScreenshot:
		{
		int iKey = TAKSI_HOTKEY_ConfigOpen + ( eProp - TAKSI_CFGPROP_VKey_ConfigOpen );
		return _snprintf(pszValue, iSizeMax, "%d", m_wHotKey[iKey] );
		}
	case TAKSI_CFGPROP_KeyboardUseDirectInput:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bUseDirectInput));
	case TAKSI_CFGPROP_VideoHalfSize:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bVideoHalfSize));
	case TAKSI_CFGPROP_VideoCodecMsg:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bVideoCodecMsg));

	case TAKSI_CFGPROP_VideoCodecInfo:
		return m_VideoCodec.GetStr(pszValue, iSizeMax);
	case TAKSI_CFGPROP_AudioFormat:
		return Mem_ConvertToString( pszValue, iSizeMax, (BYTE*) m_AudioFormat.get_WF(), m_AudioFormat.get_FormatSize());
	case TAKSI_CFGPROP_AudioDevice:
		return _snprintf(pszValue, iSizeMax, "%d", m_iAudioDevice );
	case TAKSI_CFGPROP_ShowIndicator:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bShowIndicator));
	case TAKSI_CFGPROP_PosMasterWindow:
		return _snprintf(pszValue, iSizeMax, "%d,%d", m_ptMasterWindow.x, m_ptMasterWindow.y );
	case TAKSI_CFGPROP_PosMasterTopMost:
		return _snprintf(pszValue, iSizeMax, "%d", m_bMasterTopMost );

	case TAKSI_CFGPROP_GDIFrame:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bGDIFrame));
	case TAKSI_CFGPROP_GDICursor:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bGDICursor));

	case TAKSI_CFGPROP_GDIDesktop:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_abUseGAPI[TAKSI_GAPI_DESKTOP]));
	case TAKSI_CFGPROP_UseGDI:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_abUseGAPI[TAKSI_GAPI_GDI]));
	case TAKSI_CFGPROP_UseOGL:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_abUseGAPI[TAKSI_GAPI_OGL]));
#ifdef USE_DIRECTX8
	case TAKSI_CFGPROP_UseDX8:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_abUseGAPI[TAKSI_GAPI_DX8]));
#endif
#ifdef USE_DIRECTX9
	case TAKSI_CFGPROP_UseDX9:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_abUseGAPI[TAKSI_GAPI_DX9]));
#endif

	case TAKSI_CFGPROP_UseOverheadCompensation:
		return _snprintf(pszValue, iSizeMax, GetBoolStr(m_bUseOverheadCompensation));

	default:
		DEBUG_ERR(("CTaksiConfig::PropGet bad code %d" LOG_CR, eProp ));
		ASSERT(0);
		break;
	}
	return -1;
}

static bool GetStrBool( const char* pszVal )
{
	return atoi(pszVal)? true : false;
}

bool CTaksiConfig::PropSet( int eProp, const char* pszValue )
{
	// TAKSI_CFGPROP_TYPE
	switch (eProp)
	{
	case TAKSI_CFGPROP_DebugLog:
		m_bDebugLog = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_CaptureDir:
		if (! Str_GetQuoted( m_szCaptureDir, COUNTOF(m_szCaptureDir), pszValue))
			return false;
		break;
	case TAKSI_CFGPROP_FileNamePostfix:
		if (! Str_GetQuoted( m_szFileNamePostfix, COUNTOF(m_szFileNamePostfix), pszValue))
			return false;
		break;
	case TAKSI_CFGPROP_ImageFormatExt:
		if (! Str_GetQuoted( m_szImageFormatExt, COUNTOF(m_szImageFormatExt), pszValue))
			return false;
		break;
	case TAKSI_CFGPROP_MovieFrameRateTarget:
		m_fFrameRateTarget = (float) atof(pszValue);
		break;

	case TAKSI_CFGPROP_PosMasterWindow:
		sscanf( pszValue, "%d,%d", &m_ptMasterWindow.x, &m_ptMasterWindow.y );
		break;
	case TAKSI_CFGPROP_PosMasterTopMost:
		m_bMasterTopMost = GetStrBool(pszValue);
		break;
		
	case TAKSI_CFGPROP_VKey_ConfigOpen:
	case TAKSI_CFGPROP_VKey_HookModeToggle:
	case TAKSI_CFGPROP_VKey_IndicatorToggle:
	case TAKSI_CFGPROP_VKey_RecordBegin:
	case TAKSI_CFGPROP_VKey_RecordPause:
	case TAKSI_CFGPROP_VKey_RecordStop:
	case TAKSI_CFGPROP_VKey_Screenshot:
	case TAKSI_CFGPROP_VKey_SmallScreenshot:
		{
		int iKey = TAKSI_HOTKEY_ConfigOpen + ( eProp - TAKSI_CFGPROP_VKey_ConfigOpen );
		ASSERT( iKey >= 0 && iKey < TAKSI_HOTKEY_QTY );
		char* pszEnd;
		m_wHotKey[iKey] = (WORD) strtol(pszValue,&pszEnd,0);
		}
		break;
	case TAKSI_CFGPROP_KeyboardUseDirectInput:
		m_bUseDirectInput = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_VideoHalfSize:
		m_bVideoHalfSize = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_VideoCodecMsg:
		m_bVideoCodecMsg = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_VideoCodecInfo:
		if ( ! m_VideoCodec.put_Str(pszValue))
			return false;
		break;
	case TAKSI_CFGPROP_AudioFormat:
		{
		BYTE bTmp[1024];
		ZeroMemory( &bTmp, sizeof(bTmp));
		int iSize = Mem_ReadFromString( bTmp, sizeof(bTmp)-1, pszValue );
		if ( iSize < sizeof(PCMWAVEFORMAT))
		{
			//ASSERT( iSize >= sizeof(PCMWAVEFORMAT));
			return false;
		}
		if ( ! m_AudioFormat.SetFormatBytes( bTmp, iSize ))
		{
			//ASSERT(0);
			return false;
		}
		}
		break;
	case TAKSI_CFGPROP_AudioDevice:
		m_iAudioDevice = atoi(pszValue);
		break;
	case TAKSI_CFGPROP_ShowIndicator:
		m_bShowIndicator = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_GDIFrame:
		m_bGDIFrame = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_GDICursor:
		m_bGDICursor = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_GDIDesktop:
		m_abUseGAPI[TAKSI_GAPI_DESKTOP] = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_UseGDI:
		m_abUseGAPI[TAKSI_GAPI_GDI] = GetStrBool(pszValue);
		break;
	case TAKSI_CFGPROP_UseOGL:
		m_abUseGAPI[TAKSI_GAPI_OGL] = GetStrBool(pszValue);
		break;
#ifdef USE_DIRECTX8
	case TAKSI_CFGPROP_UseDX8:
		m_abUseGAPI[TAKSI_GAPI_DX8] = GetStrBool(pszValue);
		break;
#endif
#ifdef USE_DIRECTX9
	case TAKSI_CFGPROP_UseDX9:
		m_abUseGAPI[TAKSI_GAPI_DX9] = GetStrBool(pszValue);
		break;
#endif
	case TAKSI_CFGPROP_UseOverheadCompensation:
		m_bUseOverheadCompensation = GetStrBool(pszValue);
		break;
	default:
		DEBUG_ERR(("CTaksiConfig::PropSet bad code %d" LOG_CR, eProp ));
		ASSERT(0);
		return false;
	}
#if 0 // def _DEBUG
	char szTmp[_MAX_PATH*2];
	if ( PropGet(eProp,szTmp,sizeof(szTmp)) < 0 )
	{
		strcpy( szTmp, "<NA>" );
	}
	LOG_MSG(( "PropSet '%s'='%s'" LOG_CR, sm_Props[eProp], szTmp));
#endif
	return true;
}

//***************************************************************************

bool CTaksiConfig::ReadIniFile()
{
	// Read an INI file in standard INI file format.
	// Returns true if successful.

	TCHAR szIniFileName[_MAX_PATH];
	Str_MakeFilePath( szIniFileName, COUNTOF(szIniFileName), 
		sg_Shared.m_szIniDir, _T(TAKSI_INI_FILE) );

#ifdef _UNICODE
	// convert from UNICODE. fopen() is multibyte only.
	FILE* pFile = NULL;	ASSERT(0);
#else
	FILE* pFile = fopen(szIniFileName, _T("rt"));
#endif
	if (pFile == NULL) 
	{
		// ASSUME constructor has already set this to all defaults.
		return false;
	}

	LOG_MSG(( "Reading INI file '%s'" LOG_CR, szIniFileName ));

	CIniObject* pObj = NULL;
	while (true)
	{
		char szBuffer[_MAX_PATH*2];
		fgets(szBuffer, sizeof(szBuffer)-1, pFile);
		if (feof(pFile))
			break;

		// skip/trim comments
		char* pszComment = strstr(szBuffer, ";");
		if (pszComment != NULL) 
			pszComment[0] = '\0';

		// parse the line
		char* pszName = Str_SkipSpace(szBuffer);	// skip leading spaces.
		if ( *pszName == '\0' )
			continue;

		if ( *pszName == '[' )
		{
			if ( ! strnicmp( pszName, "[" TAKSI_SECTION "]", 7 ))
			{
				pObj = this;
			}
			else if ( ! strnicmp( pszName, "[" TAKSI_CUSTOM_SECTION " ", sizeof(TAKSI_CUSTOM_SECTION)+1 ))
			{
				TCHAR szSection[ _MAX_PATH ];
#ifdef _UNICODE
				ASSERT(0);
#else
				strncpy( szSection, pszName+sizeof(TAKSI_CUSTOM_SECTION)+1, sizeof(szSection));
#endif
				TCHAR* pszEnd = _tcschr(szSection, ']');
				if (pszEnd)
					*pszEnd = '\0';
				pObj = CustomConfig_Lookup(szSection,true);
			}
			else
			{
				pObj = NULL;
				LOG_MSG(("INI Bad Section %s" LOG_CR, pszName ));
			}
			continue;
		}

		if ( pObj == NULL )	// skip
			continue;

		char* pszEq = strstr(pszName, "=");
		if (pszEq == NULL) 
			continue;
		pszEq[0] = '\0';

		char* pszValue = Str_SkipSpace( pszEq + 1 );	// skip leading spaces.

		if ( ! pObj->PropSetName( pszName, pszValue ))
		{
			LOG_MSG(("INI Bad Prop %s" LOG_CR, pszName ));
		}
	}

	fclose(pFile);
	FixCaptureDir();
	return true;
}

bool CTaksiConfig::WriteIniFile()
{
	// RETURN: true = success
	//  false = cant save!
	//
	char* pFileOld = NULL;
	DWORD nSizeOld = 0;

	TCHAR szIniFileName[_MAX_PATH];
	Str_MakeFilePath( szIniFileName, COUNTOF(szIniFileName), 
		sg_Shared.m_szIniDir, _T(TAKSI_INI_FILE) );

	// first read all lines
	CNTHandle FileOld( ::CreateFile( szIniFileName, 
		GENERIC_READ, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ));
	if ( FileOld.IsValidHandle())
	{
		nSizeOld = ::GetFileSize(FileOld, NULL);
		pFileOld = (char*) ::HeapAlloc( g_Proc.m_hHeap, HEAP_ZERO_MEMORY, nSizeOld);
		if (pFileOld == NULL) 
			return false;

		DWORD dwBytesRead = 0;
		::ReadFile(FileOld, pFileOld, nSizeOld, &dwBytesRead, NULL);
		if (dwBytesRead != nSizeOld) 
		{
			::HeapFree( g_Proc.m_hHeap, 0, pFileOld );
			return false;
		}
		FileOld.CloseHandle();
	}

	// create new file
	FILE* pFile = fopen( TAKSI_INI_FILE, "wt");
	if ( pFile == NULL )
		return false;

	// loop over every line from the old file, and overwrite it in the new file
	// if necessary. Otherwise - copy the old line.
	
	CIniObject* pObj = NULL;
	char* pszLine = pFileOld; 

	if (pFileOld)
	while (true)
	{
		if ( pszLine >= pFileOld + nSizeOld )
			break;

		if ( *pszLine == '[' )
		{
			if ( pObj ) // finish previous section.
			{
				pObj->PropsWrite(pFile);
			}
			if ( ! strnicmp( pszLine, "[" TAKSI_SECTION "]", sizeof(TAKSI_SECTION)+1 ))
			{
				pObj = this;
			}
			else if ( ! strnicmp( pszLine, "[" TAKSI_CUSTOM_SECTION " ", sizeof(TAKSI_CUSTOM_SECTION)+1 ))
			{
				TCHAR szSection[ _MAX_PATH ];
#ifdef _UNICODE
				ASSERT(0);
#else
				strncpy( szSection, pszLine+14, sizeof(szSection));
#endif
				TCHAR* pszEnd = _tcschr(szSection, ']');
				if (pszEnd)
					*pszEnd = '\0';
				pObj = CustomConfig_FindAppId(szSection);
			}
			else
			{
				pObj = NULL;
			}
		}

		char* pszEndLine = strchr(pszLine, '\n' ); // INI_CR
		if (pszEndLine) 
		{
			// covers \n or \r\n
			char* pszTmp = pszEndLine;
			for ( ; pszTmp >= pszLine && Str_IsSpace(*pszTmp); pszTmp-- )
				pszTmp[0] = '\0'; 
			pszEndLine++;
		}

		// it's a custom setting.
		bool bReplaced;
		if (pObj)
		{
			bReplaced = pObj->PropWriteName( pFile, pszLine );
		}
		else
		{
			bReplaced = false;
		}
		if (!bReplaced)
		{
			// take the old line as it was, might be blank or comment.
			fprintf(pFile, "%s" INI_CR, pszLine);
		}
		if (pszEndLine == NULL)
			break;
		pszLine = pszEndLine;
	}

	// release buffer
	if(pFileOld)
	{
		::HeapFree( g_Proc.m_hHeap, 0, pFileOld );
	}

	if ( pObj ) // finish previous section.
	{
		pObj->PropsWrite(pFile);
	}

	// if wasn't written, make sure we write it.
	if  (!m_dwWrittenMask)
	{
		fprintf( pFile, "[" TAKSI_SECTION "]" INI_CR );
		PropsWrite(pFile);
	}
	PropsInit();

	// same goes for NEW custom configs
	CTaksiConfigCustom* p = m_pCustomList;
	while (p != NULL)
	{
		if ( ! p->m_dwWrittenMask )
		{
			fprintf( pFile, "[" TAKSI_CUSTOM_SECTION " %s]" INI_CR, p->m_szAppId );
			p->PropsWrite(pFile);
		}
		p->PropsInit();
		p = p->m_pNext;
	}

	fclose(pFile);
	return true;
}
