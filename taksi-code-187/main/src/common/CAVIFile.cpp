//
// CAVIFile.cpp
//
#include "stdafx.h"
#include "CAVIFile.h"
#include "Common.h"

// Header structure for an AVI file:
// with no audio stream, and one video stream, which uses bitmaps without palette.
// This is sort of cheating to mash it all together like this. might want to use mmio*() ??
// NOTE: Use RIFFpad at menasoft.com to check this format out.

#pragma pack(1)

struct AVI_FILE_HEADER
{
	FOURCC fccRiff;                 // "RIFF"
	DWORD  dwSizeRiff;          

	FOURCC fccForm;                 // "AVI "
	FOURCC fccList_0;				// "LIST"
	DWORD  dwSizeList_0;

	FOURCC fccHdrl;                 // "hdrl"
	FOURCC fccAvih;                 // "avih"
	DWORD  dwSizeAvih;			// sizeof(m_AviHeader)

	MainAVIHeader m_AviHeader;
};

struct AVI_VIDEO_HEADER
{
	// Video Format.
	FOURCC fccList_V;                  // "LIST"
	DWORD  dwSizeList_V;

	FOURCC fccStrl;                 // "strl"
	FOURCC fccStrh;                 // "strh"
	DWORD  dwSizeStrh;
	AVIStreamHeader m_StreamHeader;	// sizeof() = 64 ? Audio or video.

	FOURCC fccStrf;                 // "strf"
	DWORD  dwSizeStrf;
	BITMAPINFOHEADER m_biHeader;

	FOURCC fccStrn;                 // "strn"
	DWORD  dwSizeStrn;
	char m_Strn[13];				// "Video Stream"
	char m_StrnPadEven;
};

struct AVI_AUDIO_HEADER
{
	// Audio Format info stored in file. Only present if actually recording audio.
	FOURCC fccList_A;				// "LIST"
	DWORD  dwSizeList_A;

	FOURCC fccStrl;                 // "strl"
	FOURCC fccStrh;                 // "strh"
	DWORD  dwSizeStrh;
	AVIStreamHeader m_StreamHeader;	// sizeof() = 64 ? Audio or video.

	FOURCC fccStrf;                 // "strf"
	DWORD  dwSizeStrf;
	WAVEFORMATEX m_AudioFormat;		// Variable length!! m_AudioFormat.cbSize

	FOURCC fccStrn;                 // "strn"
	DWORD  dwSizeStrn;				// sizeof("Audio Stream")
	char m_Strn[13];				// "Audio Stream"
	char m_StrnPadEven;
};

// NOTE: this must match what is in AVI_VIDEO_HEADER
struct AVI_STRN_HEADER
{
	FOURCC fccStrn;                 // "strn"
	DWORD  dwSizeStrn;				// sizeof("Audio Stream")
	char m_Strn[13];				// "Audio Stream"
	char m_StrnPadEven;
};

// NOTE: this must match what is in AVI_VIDEO_HEADER
struct AVI_VIDEO_STRL_HEADER
{
	FOURCC fccList_V;               // "LIST"
	DWORD  dwSizeList_V;

	FOURCC fccStrl;                 // "strl"
	FOURCC fccStrh;                 // "strh"
	DWORD  dwSizeStrh;
	AVIStreamHeader m_StreamHeader;	// sizeof() = 64 ? Audio or video.

	FOURCC fccStrf;                 // "strf"
	DWORD  dwSizeStrf;
};

#define AVI_MOVILIST_OFFSET 0x800	// fixed offset to data. (not sure exactly why).

#pragma pack()

// end of AVI structures

void CVideoFrameForm::InitPadded( int cx, int cy, int iBPP, int iPad )
{
	// iPad = sizeof(DWORD)
	m_Size.cx = cx;
	m_Size.cy = cy;
	m_iBPP = iBPP;
	int iPitchUn = cx * iBPP;
	int iRem = iPitchUn % iPad;
	m_iPitch = (iRem == 0) ? iPitchUn : (iPitchUn + iPad - iRem);
}

HRESULT HRes_GetLastErrorDef( HRESULT hResDefault )
{
	// Something failed so find out why
	// hResDefault = E_FAIL or CONVERT10_E_OLESTREAM_BITMAP_TO_DIB
	DWORD dwLastError = ::GetLastError();
	if ( dwLastError == 0 )
		dwLastError = hResDefault; // no idea why this failed.
	return HRESULT_FROM_WIN32(dwLastError);
}

//**************************************************************** 

void CVideoFrame::FreeFrame()
{
	if ( !IsValidFrame()) 
		return;
	::HeapFree( ::GetProcessHeap(), 0, m_pPixels);
	m_pPixels = NULL;
}

bool CVideoFrame::AllocForm( const CVideoFrameForm& FrameForm )
{
	// allocate space for a frame of a certain format.
	if ( IsValidFrame())
	{
		if ( ! memcmp( &FrameForm, this, sizeof(FrameForm)))
			return true;
		FreeFrame();
	}
	((CVideoFrameForm&)*this) = FrameForm;
	m_pPixels = (BYTE*) ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, get_SizeBytes());
	if ( ! IsValidFrame())
		return false;
	return true;
}

bool CVideoFrame::AllocPadXY( int cx, int cy, int iBPP, int iPad )
{
	// iPad = sizeof(DWORD)
	CVideoFrameForm FrameForm;
	FrameForm.InitPadded(cx,cy,iBPP,iPad);
	return AllocForm(FrameForm);
}

void CVideoFrame::SetupBITMAPINFOHEADER( BITMAPINFOHEADER& bmih ) const
{
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = m_Size.cx;
	bmih.biHeight = m_Size.cy;
	bmih.biPlanes = 1;
	bmih.biBitCount = m_iBPP*8;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
}

HRESULT CVideoFrame::SaveAsBMP( const TCHAR* pszFileName ) const
{
	// Writes a BMP file
	ASSERT( pszFileName );

	// save to file
	CNTHandle File( ::CreateFile( pszFileName,            // file to create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL ));                        // no attr. template 
	if ( ! File.IsValidHandle()) 
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE));
		DEBUG_ERR(("CVideoFrame::SaveAsBMP: FAILED save to file. 0x%x" LOG_CR, hRes ));
		return hRes;	// 
	}

	DWORD dwSizeBytes = get_SizeBytes();

	// fill in the headers
	BITMAPFILEHEADER bmfh;
	bmfh.bfType = 0x4D42; // 'BM'
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwSizeBytes;
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	DWORD dwBytesWritten;
	::WriteFile(File, &bmfh, sizeof(bmfh), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(bmfh) )
	{
	}

	BITMAPINFOHEADER bmih;
	SetupBITMAPINFOHEADER(bmih);
	::WriteFile(File, &bmih, sizeof(bmih), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(bmih) )
	{
	}

	::WriteFile(File, m_pPixels, dwSizeBytes, &dwBytesWritten, NULL);
	if ( dwBytesWritten != dwSizeBytes )
	{
	}
	return S_OK;
}

//******************************************************************************

HRESULT CVideoCodec::GetHError( LRESULT lRes ) // static
{
	// convert the return code into a standard error code. or something reasonably close.
	switch(lRes)
	{
	case ICERR_OK:	return S_OK;
	case ICERR_UNSUPPORTED: return HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE);
	case ICERR_BADFORMAT: return HRESULT_FROM_WIN32(ERROR_COLORSPACE_MISMATCH); //?
	case ICERR_MEMORY: return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
	case ICERR_INTERNAL: return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
	case ICERR_BADFLAGS: return HRESULT_FROM_WIN32(ERROR_INVALID_FLAG_NUMBER);
	case ICERR_BADPARAM: return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
	case ICERR_BADSIZE: return HRESULT_FROM_WIN32(ERROR_INVALID_FORM_SIZE);
	case ICERR_BADHANDLE: return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
	case ICERR_CANTUPDATE: return CACHE_E_NOCACHE_UPDATED; //?
	case ICERR_ABORT: return HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED);
	case ICERR_ERROR: return HRESULT_FROM_WIN32(ERROR_INVALID_TRANSFORM);
	case ICERR_BADBITDEPTH: return HRESULT_FROM_WIN32(ERROR_INVALID_PIXEL_FORMAT);
	case ICERR_BADIMAGESIZE: return HRESULT_FROM_WIN32(ERROR_DS_MAX_OBJ_SIZE_EXCEEDED);
	}
	return ERROR_METAFILE_NOT_SUPPORTED;	// unknown error.
}

void CVideoCodec::InitCodec()
{
	m_bCompressing = false;
	ZeroMemory(&m_v,sizeof(m_v));
	// Use a default codec
	m_v.cbSize = sizeof(m_v); // 0x40;
	m_v.dwFlags = ICMF_COMPVARS_VALID;
	m_v.fccType = MAKEFOURCC('v','i','d','c');	
	m_v.fccHandler = MAKEFOURCC('D','I','B',' '); // no compress.
	// MAKEFOURCC('iyuv') = intel yuv format works.
	m_v.lKey = 0x01;
	m_v.lDataRate = 0x12c;
	//m_v.lpState = 0xsdfsdf;
	//m_v.cbState = 0x38;
}

void CVideoCodec::DestroyCodec()
{
	if ( m_v.cbSize != 0) 
	{
		CompEnd();
		DEBUG_MSG(( "CVideoCodec:DestroyCodec ICCompressorFree" LOG_CR ));
		// Must do this after ICCompressorChoose, ICSeqCompressFrameStart, ICSeqCompressFrame, and ICSeqCompressFrameEnd functions
		::ICCompressorFree(&m_v);
		m_v.cbSize = 0;
	}
	CloseCodec();
}

int CVideoCodec::GetStr( char* pszValue, int iSizeMax) const
{
	// serialize to a string format.
	// RETURN:
	//  length of the string.
	if ( m_v.cbSize <= 0 || iSizeMax <= 0 )
		return 0;
	CVideoCodec codec;
	codec.InitCodec();
	codec.CopyCodec( *this );
	return Mem_ConvertToString( pszValue, iSizeMax, (BYTE*) &codec.m_v, sizeof(codec.m_v));
}

bool CVideoCodec::put_Str( const char* pszValue )
{
	// serialize from a string format.
	DestroyCodec();
	ZeroMemory( &m_v, sizeof(m_v));
	int iSize = Mem_ReadFromString((BYTE*) &m_v, sizeof(m_v), pszValue );
	if ( iSize != sizeof(m_v))
	{
	}
	m_v.hic = NULL;
	m_v.lpbiIn = NULL;
	m_v.lpbiOut = NULL;
	m_v.lpBitsPrev = NULL;
	m_v.lpState = NULL;
	//m_v.cbState = 0;
	return true;
}

void CVideoCodec::CopyCodec( const CVideoCodec& codec )
{
	// cant copy the m_v.hic, etc.
	// DONT allow pointers to get copied! this will leak CloseCodec()!
	m_bCompressing = false;
	memcpy( &m_v, &codec.m_v, sizeof(m_v));
	m_v.hic = NULL;
	m_v.lpbiIn = NULL;
	m_v.lpbiOut = NULL;
	m_v.lpBitsPrev = NULL;
	m_v.lpState = NULL;
	//m_v.cbState = 0; 
}

bool CVideoCodec::CompChooseDlg( HWND hWnd, char* lpszTitle )
{
	// MUST call CloseCodec() after this.
	// dwFlags |= ICMF_COMPVARS_VALID ?
	m_v.cbSize = sizeof(m_v);	// Must use ICCompressorFree later.
	if ( ! ::ICCompressorChoose( hWnd, 
		ICMF_CHOOSE_ALLCOMPRESSORS|ICMF_CHOOSE_PREVIEW|ICMF_CHOOSE_KEYFRAME|ICMF_CHOOSE_DATARATE, // ICMF_CHOOSE_PREVIEW|ICMF_CHOOSE_KEYFRAME|ICMF_CHOOSE_DATARATE
		NULL, NULL,
		&m_v, (char*) lpszTitle ))
	{
		return false;	// cancelled.
	}
	// ICMF_COMPVARS_VALID
	return true; 
}

#ifdef _DEBUG
void CVideoCodec::DumpSettings()
{
	// Prints out COMPVARS 
	DEBUG_TRACE(("CVideoCodec::DumpSettings: cbSize = %d" LOG_CR, (DWORD)m_v.cbSize));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: dwFlags = %08x" LOG_CR, (DWORD)m_v.dwFlags));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: hic = %08x" LOG_CR, (UINT_PTR)m_v.hic));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: fccType = %08x" LOG_CR, (DWORD)m_v.fccType));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: fccHandler = %08x" LOG_CR, (DWORD)m_v.fccHandler));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lpbiOut = %08x" LOG_CR, (UINT_PTR)m_v.lpbiOut));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lKey = %d" LOG_CR, m_v.lKey));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lDataRate = %d" LOG_CR, m_v.lDataRate));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lQ = %d" LOG_CR, m_v.lQ));
}
#endif

HRESULT CVideoCodec::OpenCodec( WORD wMode )
{
	// Open a compressor or decompressor codec.
	// wMode = ICMODE_FASTCOMPRESS, ICMODE_QUERY 
	// fccType = 
	if ( m_v.hic )
	{
		// May not be the mode i want. so close and re-open
		CloseCodec();	
	}

	ASSERT( !m_bCompressing );
	ASSERT( m_v.hic == NULL );

	if ( m_v.fccHandler == MAKEFOURCC('D','I','B',' '))
	{
		// this just means NOT compressed! m_v.hic would be NULL anyhow.
		DEBUG_MSG(("CVideoCodec::OpenCodec DIB" LOG_CR ));
		return S_OK;
	}

	const char* pszFourCC = (const char*) &(m_v.fccHandler);

	m_v.hic = ::ICOpen( m_v.fccType, m_v.fccHandler, wMode );
	if ( m_v.hic == NULL)
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_CANNOT_DETECT_DRIVER_FAILURE));
		DEBUG_WARN(("CVideoCodec::Open: ICOpen(%c,%c,%c,%c) RET NULL (0x%x)" LOG_CR,
			pszFourCC[0], pszFourCC[1], pszFourCC[2], pszFourCC[3],
			hRes ));
		return hRes;
	}

	DEBUG_MSG(("CVideoCodec::OpenCodec:ICOpen(%c,%c,%c,%c) OK hic=0x%x" LOG_CR, 
		pszFourCC[0], pszFourCC[1], pszFourCC[2], pszFourCC[3],
		(UINT_PTR)m_v.hic ));
	return S_OK;
}

void CVideoCodec::CloseCodec()
{
	CompEnd();
	ASSERT( !m_bCompressing );
	if ( m_v.hic != NULL )
	{
		::ICClose(m_v.hic);
		m_v.hic = NULL;
		LOG_MSG(("CVideoCodec:CloseCodec ICClose" LOG_CR));
	}
}

bool CVideoCodec::CompSupportsConfigure() const
{
	if ( m_v.hic == NULL )
		return false;
	return ICQueryConfigure(m_v.hic);
}
LRESULT CVideoCodec::CompConfigureDlg( HWND hWndApp )
{
	// a config dialog specific to the chosen codec.
	if ( m_v.hic == NULL )
		return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	return ICConfigure(m_v.hic,hWndApp);
}

bool CVideoCodec::GetCodecInfo( ICINFO& icInfo ) const
{
	// Get name etc info on the m_v.hic codec.
	// RETURN:
	//  false = (and icInfo.dwSize=0), to indicate the codec cant be loaded.
	if ( m_v.hic == NULL )
	{
		// open a temporary version of this.
		CVideoCodec codec;
		codec.InitCodec();
		codec.CopyCodec( *this );
		HRESULT hRes = codec.OpenCodec(ICMODE_QUERY);
		if ( IS_ERROR(hRes) || codec.m_v.hic == NULL )
		{
			icInfo.dwSize = 0;
			return false;
		}
		bool bRet = codec.GetCodecInfo(icInfo);
		codec.DestroyCodec();
		return bRet;
	}

	ZeroMemory(&icInfo, sizeof(ICINFO));
	icInfo.dwSize = sizeof(ICINFO);
	if ( ! ::ICGetInfo(m_v.hic, &icInfo, sizeof(ICINFO)))
	{
		icInfo.dwSize = 0;
		DEBUG_ERR(("CVideoCodec::GetCodecInfo: ICGetInfo FAILED." LOG_CR));
		return false;
	}

	DEBUG_MSG(("CVideoCodec::GetCodecInfo: fccType: {%08x}" LOG_CR, icInfo.fccType));
	const char* pszFourcc = (const char*)&(icInfo.fccHandler);
	DEBUG_MSG(("CVideoCodec::GetCodecInfo: fccHandler: {%c,%c,%c,%c}={%08x}" LOG_CR, pszFourcc[0], pszFourcc[1], pszFourcc[2], pszFourcc[3], icInfo.fccHandler));
	DEBUG_MSG(("CVideoCodec::GetCodecInfo: Name: {%S}" LOG_CR, icInfo.szName));
	DEBUG_MSG(("CVideoCodec::GetCodecInfo: Desc: {%S}" LOG_CR, icInfo.szDescription));
	DEBUG_MSG(("CVideoCodec::GetCodecInfo: Driver: {%S}" LOG_CR, icInfo.szDriver));
	return true;
}

LRESULT CVideoCodec::GetCompFormat( const BITMAPINFO* lpbiIn, BITMAPINFO* lpbiOut ) const
{
	// Query compressor to determine its output format structure size.
	// NOTE: lpbiOut = NULL, returns the size required for the output format.
	// RETURN: 
	//  -2 = ICERR_BADFORMAT;

	ASSERT(lpbiIn);
	if ( m_v.hic == NULL )
	{
		if ( lpbiOut == NULL )	// just ret the size.
			return sizeof(BITMAPINFO);
		memcpy( lpbiOut, lpbiIn, sizeof(BITMAPINFO));
		return S_OK;
	}

	LRESULT lRes = ::ICCompressGetFormat( m_v.hic, lpbiIn, lpbiOut );
	if (lRes != ICERR_OK)
	{
		if ( lpbiOut == NULL )	// just ret the size.
		{
			if ( lRes <= 0 || lRes >= 0x10000 )
			{
				DEBUG_ERR(( "CVideoCodec::ICCompressGetFormat BAD RET=%d" LOG_CR, lRes ));
			}
			return lRes;
		}
		DEBUG_ERR(("CVideoCodec:ICCompressGetFormat FAILED %d." LOG_CR, lRes ));
		return lRes;
	}

	if ( lpbiOut == NULL )	// size = 0. this is bad.
		return ICERR_BADFORMAT;

	DEBUG_TRACE(("CVideoCodec:GetCompFormat: lpbiOut.bmiHeader.biWidth = %d" LOG_CR, 
		lpbiOut->bmiHeader.biWidth));
	DEBUG_TRACE(("CVideoCodec:GetCompFormat: lpbiOut.bmiHeader.biHeight = %d" LOG_CR, 
		lpbiOut->bmiHeader.biHeight));
	DEBUG_TRACE(("CVideoCodec:GetCompFormat: lpbiOut.bmiHeader.biCompression = %08x" LOG_CR, 
		lpbiOut->bmiHeader.biCompression));

	return ICERR_OK;
}

HRESULT CVideoCodec::CompStart( BITMAPINFO* lpbiIn )
{
	// open the compression stream.
	// MUST call CompEnd() and CloseCodec() after this.
	// NOTE: it seems we keep a pointer to lpbiIn so it MUST PERSIST!

	ASSERT(lpbiIn);
	if ( m_bCompressing )	// already started?!
	{
		ASSERT(m_v.lpbiOut);
		return S_FALSE;
	}
	if ( m_v.hic == NULL )
	{
		m_bCompressing = false;
		m_v.lpbiOut = lpbiIn;
		return S_OK;
	}

	DEBUG_MSG(( "CVideoCodec::CompStart" LOG_CR ));

	if ( ! ::ICSeqCompressFrameStart( &m_v, lpbiIn ))
	{	
		// m_v.lpbiOut = trash ??
		// NOTE: may fail because of odd frame sizes ??
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_TRANSFORM_NOT_SUPPORTED));
		DEBUG_ERR(("CVideoCodec::CompStart: ICSeqCompressFrameStart (%d x %d) FAILED (0x%x)." LOG_CR, 
			lpbiIn->bmiHeader.biWidth, lpbiIn->bmiHeader.biHeight, hRes ));
		return hRes;
	}

	ASSERT(m_v.lpbiOut);
	m_bCompressing = true;
	DEBUG_MSG(("CVideoCodec:CompStart: ICSeqCompressFrameStart success." LOG_CR));
	return S_OK;
}

void CVideoCodec::CompEnd()
{
	// End the compression stream.
	// ASSUME: CompStart() was called.
	if ( ! m_bCompressing )
		return;
	//ASSERT(m_v.lpbiOut);
	DEBUG_MSG(( "CVideoCodec:CompEnd ICSeqCompressFrameEnd" LOG_CR ));
	m_bCompressing = false;
	::ICSeqCompressFrameEnd(&m_v);
}

HRESULT CVideoCodec::CompFrame( const CVideoFrame& frame, const void*& rpCompRet, LONG& nSize, BOOL& bIsKey )
{
	// Compress the frame in the stream.
	// ASSUME: CompStart() was called.
	// ASSUME: frame.m_pPixels is not altered.
	// ASSUME frame == m_FrameForm
	// RETURN:
	//  bIsKey = this was used as the key frame?

	if ( ! m_bCompressing || m_v.hic == NULL )
	{
		// DEBUG_TRACE(("CVideoCodec::CompFrame: NOT ACTIVE!." LOG_CR));
		rpCompRet = frame.m_pPixels;
		nSize = frame.get_SizeBytes();
		return S_FALSE;
	}
	void* pCompRet = ::ICSeqCompressFrame( &m_v, 0, (LPVOID) frame.m_pPixels, &bIsKey, &nSize );
	if (pCompRet == NULL)
	{
		// Just use the raw frame? NOT COMPRESSED.
		DEBUG_ERR(("CVideoCodec::CompFrame: ICSeqCompressFrame FAILED."));
		nSize = 0;
		return E_FAIL;
	}
	ASSERT( (DWORD) nSize <= 2*frame.get_SizeBytes());
	rpCompRet = pCompRet; // ASSUME m_v owns this buffer and it will get cleaned up on CompEnd()
	return S_OK;
}

//******************************************************************************

CAVIIndex::CAVIIndex()
	: m_dwFramesTotal(0)
	, m_dwIndexBlocks(0)
	, m_pIndexFirst(NULL)
	, m_pIndexCur(NULL)
	, m_dwIndexCurFrames(0)
{
}

CAVIIndex::~CAVIIndex()
{
}

CAVIIndexBlock* CAVIIndex::CreateIndexBlock()
{
	HANDLE hHeap = ::GetProcessHeap();
	ASSERT(hHeap);
	CAVIIndexBlock* pIndex = (CAVIIndexBlock*) HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(CAVIIndexBlock));
	ASSERT(pIndex);
	pIndex->m_pEntry = (AVIINDEXENTRY*) HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(AVIINDEXENTRY)*AVIINDEX_QTY );
	ASSERT(pIndex->m_pEntry);
	pIndex->m_pNext = NULL;
	m_dwIndexBlocks++;
	return pIndex;
}

void CAVIIndex::DeleteIndexBlock( CAVIIndexBlock* pIndex )
{
	HANDLE hHeap = ::GetProcessHeap();
	::HeapFree(hHeap, 0, pIndex->m_pEntry); // release index
	::HeapFree(hHeap, 0, pIndex); // release index
}

void CAVIIndex::AddFrame( const AVIINDEXENTRY& indexentry )
{
	// Add a single index to the array.
	// update index
	if ( m_pIndexCur == NULL )
	{
		// First frame.
		ASSERT( m_dwIndexCurFrames == 0 );
		m_pIndexFirst = CreateIndexBlock();
		m_pIndexCur = m_pIndexFirst;	// initialize index pointer
	}
	if (m_dwIndexCurFrames >= AVIINDEX_QTY)
	{
		// allocate new index, if this one is full.
		CAVIIndexBlock* pIndexNew = CreateIndexBlock();
		ASSERT(pIndexNew);
		m_pIndexCur->m_pNext = pIndexNew;
		m_pIndexCur = pIndexNew;
		m_dwIndexCurFrames = 0;
	}

	m_pIndexCur->m_pEntry[m_dwIndexCurFrames] = indexentry;
	m_dwIndexCurFrames++;
	m_dwFramesTotal++;
}

void CAVIIndex::FlushIndexChunk( HANDLE hFile )
{
	// release memory that index took
	// m_pIndexCur = Last;

	DWORD dwBytesWritten;
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		// write index
		DWORD dwTags[2];
		dwTags[0] = ckidAVINEWINDEX;	// MAKEFOURCC('i', 'd', 'x', '1')
		dwTags[1] = get_ChunkSize();
		::WriteFile(hFile, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
	}

	int iCount = 0;
	CAVIIndexBlock* p = m_pIndexFirst;
	if (p)
	{
		DWORD dwTotal = m_dwFramesTotal;
		while (p != NULL)
		{
			DWORD dwFrames = ( p == m_pIndexCur ) ? 
				( m_dwFramesTotal % AVIINDEX_QTY ) : AVIINDEX_QTY;
			dwTotal -= dwFrames;
			if ( hFile != INVALID_HANDLE_VALUE )
			{
				::WriteFile( hFile, p->m_pEntry, dwFrames*sizeof(AVIINDEXENTRY),
					&dwBytesWritten, NULL);
			}
			CAVIIndexBlock* q = p->m_pNext;
			DeleteIndexBlock(p);
			p = q;
			iCount++;
		}
		DEBUG_MSG(( "CAVIIndex::FlushIndex() %d" LOG_CR, iCount ));
		ASSERT( dwTotal == 0 );
	}
	ASSERT( iCount == m_dwIndexBlocks );

	m_dwFramesTotal = 0;
	m_pIndexFirst = NULL;
	m_pIndexCur = NULL;
	m_dwIndexCurFrames = 0;
	m_dwIndexBlocks = 0;
}

//******************************************************************************

CAVIFile::CAVIFile() 
	: m_pJunkData(NULL)
	, m_dwMoviChunkSize( sizeof(DWORD))
	, m_dwTotalFrames(0)
	, m_dwAudioBytes(0)
	, m_pVideoHeader(NULL)
	, m_dwVideoHeaderSize(0)
{
	m_VideoCodec.InitCodec();
	m_AudioFormat.InitFormatEmpty();
}

CAVIFile::~CAVIFile() 
{
	CloseAVI();
	m_Index.FlushIndexChunk(m_File);
	// m_VideoCodec.DestroyCodec();
	if ( m_pVideoHeader != NULL )
		::HeapFree( ::GetProcessHeap(), 0, m_pVideoHeader );
}

void CAVIFile::InitBitmapIn( BITMAPINFO& biIn ) const
{
	// prepare BITMAPINFO for compressor
	ZeroMemory( &biIn, sizeof(biIn));
	biIn.bmiHeader.biSize = sizeof(biIn.bmiHeader);
	biIn.bmiHeader.biWidth = m_FrameForm.m_Size.cx;
	biIn.bmiHeader.biHeight = m_FrameForm.m_Size.cy;
	biIn.bmiHeader.biPlanes = 1;
	biIn.bmiHeader.biBitCount = m_FrameForm.m_iBPP*8;	// always write 24-bit AVIs.
	biIn.bmiHeader.biCompression = BI_RGB;
}

int CAVIFile::InitFileHeader( AVI_FILE_HEADER& afh, bool bInitVideo, AVI_AUDIO_HEADER* pAudio )
{
	// build the AVI file header structure
	ASSERT(m_VideoCodec.m_v.lpbiOut);
	LOG_MSG(("CAVIFile:InitFileHeader framerate=%g" LOG_CR, (float)m_fFrameRate ));
	ZeroMemory( &afh, sizeof(afh));

	afh.fccRiff = FOURCC_RIFF; // "RIFF"
	afh.dwSizeRiff = 0;	// re-calc later.

	afh.fccForm = formtypeAVI; // "AVI "
	afh.fccList_0  = FOURCC_LIST; // "LIST"
	afh.dwSizeList_0 = sizeof(FOURCC) 
		+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(afh.m_AviHeader);	// re-calc later.

	afh.fccHdrl = listtypeAVIHEADER; // "hdrl"
	afh.fccAvih = ckidAVIMAINHDR; // "avih"
	afh.dwSizeAvih = sizeof(afh.m_AviHeader);

	// fill-in MainAVIHeader
	afh.m_AviHeader.dwMicroSecPerFrame = (DWORD)( 1000000.0/m_fFrameRate );
	afh.m_AviHeader.dwMaxBytesPerSec = (DWORD)( m_FrameForm.get_SizeBytes() * m_fFrameRate );
	afh.m_AviHeader.dwTotalFrames = m_dwTotalFrames;
	afh.m_AviHeader.dwStreams = pAudio ? 2 : 1;
	afh.m_AviHeader.dwFlags = AVIF_HASINDEX; // uses index 0x10
	afh.m_AviHeader.dwSuggestedBufferSize = m_FrameForm.get_SizeBytes();
	afh.m_AviHeader.dwWidth = m_FrameForm.m_Size.cx;
	afh.m_AviHeader.dwHeight = m_FrameForm.m_Size.cy;	// ?? was /2 ?

	AVI_VIDEO_HEADER* pVideo = (AVI_VIDEO_HEADER*)m_pVideoHeader;

	// AVI_VIDEO_HEADER - Video Format
	if ( bInitVideo )
	{
		// The video header can't be a fixed size since the format size (biSize)
		// may be larger than a BITMAPINFOHEADER.  Also, biSizeImage is assumed
		// to be the the proper size after ICSeqCompressFrameStart is called.
		// Alloc a strl which contains a strh, strf (with size biSize), and strn
		// TODO: what if the Alloc fails? need to return failure
		if( IS_ERROR(AllocVideoHeader(sizeof(AVI_VIDEO_STRL_HEADER) + sizeof(AVI_STRN_HEADER) + m_VideoCodec.m_v.lpbiOut->bmiHeader.biSize)) )
			return 0;

		pVideo = (AVI_VIDEO_HEADER*)m_pVideoHeader;
		ZeroMemory( pVideo, m_dwVideoHeaderSize);

		pVideo->fccList_V  = FOURCC_LIST; // "LIST"

		pVideo->fccStrl = listtypeSTREAMHEADER; // "strl" - stream list
		pVideo->fccStrh = ckidSTREAMHEADER; // "strh"
		pVideo->dwSizeStrh = sizeof(pVideo->m_StreamHeader);
		
		// populate stream header
		pVideo->m_StreamHeader.fccType = streamtypeVIDEO; // 'vids' - NOT m_VideoCodec.m_v.fccType; = 'vidc'
		pVideo->m_StreamHeader.fccHandler = m_VideoCodec.m_v.fccHandler; // video codec fourcc
		// for video: rate / scale == frame rate, we'll use 1000 (and add .49 to round up, this allows exactly 29.97)
		pVideo->m_StreamHeader.dwScale = 1000;
		pVideo->m_StreamHeader.dwRate = ( m_fFrameRate * 1000.0 < 1.0 ) ? 1 : (DWORD)(m_fFrameRate * 1000.0 + 0.49);
		pVideo->m_StreamHeader.dwQuality = m_VideoCodec.m_v.lQ;
		pVideo->m_StreamHeader.dwSuggestedBufferSize = m_FrameForm.get_SizeBytes();
		pVideo->m_StreamHeader.rcFrame.right = m_FrameForm.m_Size.cx;
		pVideo->m_StreamHeader.rcFrame.bottom = m_FrameForm.m_Size.cy;

		// populate stream format
		pVideo->fccStrf = ckidSTREAMFORMAT; // "strf"
		pVideo->dwSizeStrf = m_VideoCodec.m_v.lpbiOut->bmiHeader.biSize;
		memcpy(&pVideo->m_biHeader, &m_VideoCodec.m_v.lpbiOut->bmiHeader, pVideo->dwSizeStrf);

		// populate stream name
		AVI_STRN_HEADER* pStrnHeader = (AVI_STRN_HEADER*)(m_pVideoHeader + sizeof(AVI_VIDEO_STRL_HEADER) + pVideo->dwSizeStrf);
		pStrnHeader->fccStrn = ckidSTREAMNAME; // "strn" - stream name
		pStrnHeader->dwSizeStrn = sizeof(pVideo->m_Strn);
		ASSERT( pStrnHeader->dwSizeStrn == 13 );
		strcpy( pStrnHeader->m_Strn, "Video Stream" );

		// calculate video list size
		pVideo->dwSizeList_V = sizeof(FOURCC) 
			+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(pVideo->m_StreamHeader)
			+ sizeof(FOURCC) + sizeof(DWORD) + pVideo->dwSizeStrf
			+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(pVideo->m_Strn);
		if ( pVideo->dwSizeList_V & 1 )	// always even size.
			pVideo->dwSizeList_V ++;
	}
	else
	{
		// update total frames
		pVideo->m_StreamHeader.dwLength = m_dwTotalFrames;
	}

	// add video size to avi list size
	afh.dwSizeList_0 +=  
		+ sizeof(FOURCC) + sizeof(DWORD) + pVideo->dwSizeList_V;

	// AVI_AUDIO_HEADER - Audio Format (if it has one)
	// if pAudio is not null m_AudioFormat should be valid, but there's a potential divide by 0 below
	if ( pAudio && HasAudioFormat() )
	{
		ZeroMemory( pAudio, sizeof(*pAudio));

		pAudio->fccList_A  = FOURCC_LIST; // "LIST"

		pAudio->fccStrl = listtypeSTREAMHEADER; // "strl"
		pAudio->fccStrh = ckidSTREAMHEADER; // "strh"
		pAudio->dwSizeStrh = sizeof(pAudio->m_StreamHeader);

		pAudio->fccStrf = ckidSTREAMFORMAT; // "strf"
		pAudio->dwSizeStrf = sizeof(pAudio->m_AudioFormat); // variable size.

		pAudio->fccStrn = ckidSTREAMNAME; // "strn"
		pAudio->dwSizeStrn = sizeof(pAudio->m_Strn);
		ASSERT( pAudio->dwSizeStrn == 13 );
		strcpy( pAudio->m_Strn, "Audio Stream" );

		// calculate audio list size
		pAudio->dwSizeList_A = sizeof(FOURCC) 
			+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(pAudio->m_StreamHeader)
			+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(pAudio->m_AudioFormat)
			+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(pAudio->m_Strn);
		if ( pAudio->dwSizeList_A & 1 )	// always even size.
			pAudio->dwSizeList_A ++;

		// fill-in Audio AVIStreamHeader and m_AudioFormat
		pAudio->m_StreamHeader.fccType = streamtypeAUDIO; // 'auds' - NOT m_VideoCodec.m_v.fccType; = 'vidc'
		pAudio->m_StreamHeader.fccHandler = 0;	// 0 for PCM?
		pAudio->m_StreamHeader.dwScale = 1;	// PCM is always 1, so rate is simply the sample rate
		pAudio->m_StreamHeader.dwRate = m_AudioFormat.get_SamplesPerSec();	// I think this will only be valid for PCM
		pAudio->m_StreamHeader.dwLength = m_dwAudioBytes / m_AudioFormat.get_BlockSize();	// number of audio blocks
		pAudio->m_StreamHeader.dwQuality = (DWORD)-1;	// default quality
		pAudio->m_StreamHeader.dwSuggestedBufferSize = 0;	// this should ideally be the largest AVI audio chunk size
		pAudio->m_StreamHeader.dwSampleSize = m_AudioFormat.get_BlockSize();	// for audio use nBlockAlign, get_BlockSize()

		// fill in m_AudioFormat header
		memcpy(&pAudio->m_AudioFormat, m_AudioFormat.get_WF(), sizeof(pAudio->m_AudioFormat) );
		// TODO: ??? Variable size !! m_AudioFormat.cbSize, m_AudioFormat.get_FormatSize()

		// add audio size to avi list size
		afh.dwSizeList_0 +=  
			+ sizeof(FOURCC) + sizeof(DWORD) + pAudio->dwSizeList_A;
		
		// add audio rate to overall
		afh.m_AviHeader.dwMaxBytesPerSec += m_AudioFormat.get_BytesPerSec();
	}

	// compute final sizes.
	if ( afh.dwSizeList_0 & 1 )	// always even size.
		afh.dwSizeList_0 ++;

	// update RIFF chunk size
	int iPadFile = ( m_dwMoviChunkSize ) & 1; // make even
	afh.dwSizeRiff = AVI_MOVILIST_OFFSET + 8 + m_dwMoviChunkSize +
		iPadFile + m_Index.get_ChunkSize();

	return iPadFile;
}

// Alloc the video header buffer if the size has changed
HRESULT CAVIFile::AllocVideoHeader( DWORD dwSize )
{
	if ( m_dwVideoHeaderSize != dwSize )
	{
		m_dwVideoHeaderSize = dwSize;
		if ( m_pVideoHeader != NULL )
			::HeapFree( ::GetProcessHeap(), 0, m_pVideoHeader );
		m_pVideoHeader = (LPBYTE)::HeapAlloc( ::GetProcessHeap(), 0, dwSize );
		if ( m_pVideoHeader == NULL )
			return E_OUTOFMEMORY;
	}
	return S_OK;
}

HRESULT CAVIFile::OpenAVICodec( CVideoFrameForm& FrameForm, double fFrameRate, const CVideoCodec& VideoCodec, const CWaveFormat* pAudioFormat )
{
	// NOTE:
	//  FrameForm = could be modified to match a format i can use.
	// NOTE: 
	//  Attempt to pick a different codec if this one fails ?

	if ( fFrameRate <= 0 )
	{
		return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
	}

	// Store my params i'm going to use.
	m_FrameForm = FrameForm;
	m_fFrameRate = fFrameRate;
	// prepare compressor. 
	m_VideoCodec.CopyCodec( VideoCodec );

	// Set the audio codec format desired.
	if ( pAudioFormat && m_AudioFormat.SetFormat( *pAudioFormat ))
	{
		// Open the Audio compression codec. 
		ASSERT(HasAudioFormat());
	}
	else
	{
		// No audio desired. 
		m_AudioFormat.SetFormat( NULL );
		ASSERT( ! HasAudioFormat());
	}

	// open compressor
	HRESULT hRes = m_VideoCodec.OpenCodec(ICMODE_FASTCOMPRESS);
	if ( IS_ERROR(hRes))
	{
		DEBUG_ERR(( "CAVIFile:OpenCodec FAILED 0x%x" LOG_CR, hRes ));
		return hRes;
	}

	// prepare BITMAPINFO for compressor
	InitBitmapIn(m_biIn);

/* NOTE: This code could be useful if ffdshow ever supports resizing when using
ICSeqCompressFrame(), in which case the below code asks the compressor for the
output format.  As of this writing it's interesting (with resize enabled in
ffdshow) how ICCompressGetFormat() returns the proper format but
ICCompressQuery() and ICSeqCompressFrame() will fail. */
#if 0 //def _DEBUG
	// print compressor settings
	// m_VideoCodec.DumpSettings();

	LRESULT lFormatSize = m_VideoCodec.GetCompFormat(&m_biIn,NULL);
	if ( lFormatSize <= 0 )
	{
		// dwFormatSize = -2 = ICERR_BADFORMAT 
		DEBUG_ERR(("CAVIFile:GetCompFormat=%d BAD FORMAT?!" LOG_CR, lFormatSize));
		//ASSERT(0);
		return m_VideoCodec.GetHError(lFormatSize);
	}

	DEBUG_MSG(("CAVIFile:GetCompFormatSize=%d" LOG_CR, lFormatSize));

	BITMAPINFO* pbiOut = (BITMAPINFO*)GlobalAlloc(GPTR, lFormatSize);
	if ( !pbiOut )
		return E_OUTOFMEMORY;
	pbiOut->bmiHeader.biSize = lFormatSize;
	LRESULT lRes = m_VideoCodec.GetCompFormat(&m_biIn,pbiOut);
	if ( lRes != 0 )
	{	
		// lRes = -2 = ICERR_BADFORMAT, should be 0x28 = sizeof(m_biIn.bmiHeader)
		DEBUG_ERR(( "CAVIFile:GetCompFormat FAILED %d" LOG_CR, lRes ));
		//ASSERT(0);
		GlobalFree(pbiOut);
		return m_VideoCodec.GetHError(lRes);
	}

	// The codec doesnt like odd sizes !!
	if ( m_biIn.bmiHeader.biHeight != pbiOut->bmiHeader.biHeight 
		|| m_biIn.bmiHeader.biWidth != pbiOut->bmiHeader.biWidth )
	{
		DEBUG_MSG(( "Codec changes the size! from (%d x %d) to (%d x %d)" LOG_CR,
			m_biIn.bmiHeader.biHeight, m_biIn.bmiHeader.biWidth,
			pbiOut->bmiHeader.biWidth, pbiOut->bmiHeader.biHeight ));
	}
	GlobalFree(pbiOut);
#endif

	int nAlign = 1;	// initial re-try alignment of 2
	// initialize compressor
do_retry:
	hRes = m_VideoCodec.CompStart(&m_biIn);
	if ( IS_ERROR(hRes))
	{
		// re-try the aligned size (up to 16) (ran into YUVsoft that actually needs 16 byte alignment) 
		while ( nAlign <= 16 )
		{
			nAlign <<= 1;
			if ( ((FrameForm.m_Size.cx & (nAlign - 1)) || (FrameForm.m_Size.cy & (nAlign - 1))) )
			{
				FrameForm.m_Size.cx &= ~(nAlign - 1);
				FrameForm.m_Size.cy &= ~(nAlign - 1);
				DEBUG_WARN(( "CAVIFile:CompStart retry at (%d x %d)" LOG_CR, FrameForm.m_Size.cx, FrameForm.m_Size.cy ));
				m_biIn.bmiHeader.biWidth = FrameForm.m_Size.cx;
				m_biIn.bmiHeader.biHeight = FrameForm.m_Size.cy;
				m_FrameForm = FrameForm;
				goto do_retry;
			}
		}

		DEBUG_ERR(( "CAVIFile:CompStart FAILED (%d x %d) (0x%x)" LOG_CR, FrameForm.m_Size.cx, FrameForm.m_Size.cy, hRes ));
		return hRes;
	}

	ASSERT(m_VideoCodec.m_v.lpbiOut);
	return S_OK;
}

HRESULT CAVIFile::OpenAVIFile( const TCHAR* pszFileName )
{
	// Opens a new file, and writes the AVI header 
	// prepare filename
	// we could use mmioOpen() ??

	ASSERT(pszFileName);
	LOG_MSG(("OpenAVIFile: szFileName: %s" LOG_CR, pszFileName ));
	if ( ! m_FrameForm.IsFrameFormInit())
		return OLE_E_BLANK;
	if ( m_fFrameRate <= 0 )
		return OLE_E_BLANK;
	ASSERT(m_VideoCodec.m_v.lpbiOut);

	// reset frames counter and chunk size
	m_dwMoviChunkSize = sizeof(DWORD);
	m_dwTotalFrames = 0;
	m_dwAudioBytes = 0;
	m_Index.FlushIndexChunk();	// kill any previous index.

	//***************************************************
	m_File.AttachHandle( ::CreateFile( pszFileName, // file to create 
		GENERIC_READ | GENERIC_WRITE,  // open for reading and writing 
		0,                             // do not share 
		NULL,                          // default security 
		CREATE_ALWAYS,                 // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,         // normal file 
		NULL));                         // no attr. template 
	if ( ! m_File.IsValidHandle()) 
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE));
		DEBUG_ERR(( "CAVIFile:OpenAVIFile CreateFile FAILED 0x%x" LOG_CR, hRes ));
		return hRes;
	}

	int iJunkChunkSize = AVI_MOVILIST_OFFSET;

	AVI_FILE_HEADER afh;  // needs m_VideoCodec.m_v.lpbiOut
	AVI_AUDIO_HEADER aah;
	InitFileHeader(afh, 
		true,
		(HasAudioFormat())?(&aah):NULL);

	DWORD dwBytesWritten = 0;
	::WriteFile(m_File, &afh, sizeof(afh), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(afh))
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
		DEBUG_ERR(( "CAVIFile:OpenAVIFile WriteFile Header FAILED %d" LOG_CR, hRes ));
		return hRes;
	}
	iJunkChunkSize -= dwBytesWritten;

	if (HasVideo())
	{
		::WriteFile(m_File, m_pVideoHeader, m_dwVideoHeaderSize, &dwBytesWritten, NULL);
		if ( dwBytesWritten != m_dwVideoHeaderSize)
		{
			HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
			DEBUG_ERR(( "CAVIFile:OpenAVIFile WriteFile Video FAILED %d" LOG_CR, hRes ));
			return hRes;
		}
		iJunkChunkSize -= dwBytesWritten;
	}
	if (HasAudioFormat())
	{
		::WriteFile(m_File, &aah, sizeof(aah), &dwBytesWritten, NULL);
		if ( dwBytesWritten != sizeof(aah))
		{
			HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
			DEBUG_ERR(( "CAVIFile:OpenAVIFile WriteFile Audio FAILED %d" LOG_CR, hRes ));
			return hRes;
		}
		iJunkChunkSize -= dwBytesWritten;
	}

	{
	// add "JUNK" chunk to get the 2K-alignment (AVI_MOVILIST_OFFSET)
	HANDLE hHeap = ::GetProcessHeap();
	DWORD* pChunkJunk = (DWORD*) ::HeapAlloc( hHeap, HEAP_ZERO_MEMORY, iJunkChunkSize);
	ASSERT(pChunkJunk);
	pChunkJunk[0] = ckidAVIPADDING; // "JUNK"
	pChunkJunk[1] = iJunkChunkSize - 8;

	// Put some possibly useful id stuff in the junk area. why not
	if ( m_pJunkData != NULL )
	{
		_snprintf( (char*)( &pChunkJunk[2] ), pChunkJunk[1], m_pJunkData );
	}
	else
	{
		// No junk specified so write the version (mediainfo will actually display this)
		_snprintf( (char*)&pChunkJunk[2], pChunkJunk[1], "Taksi v" TAKSI_VERSION_S " built: " __DATE__ );
	}

	::WriteFile(m_File, pChunkJunk, iJunkChunkSize, &dwBytesWritten, NULL);
	::HeapFree(hHeap,0,pChunkJunk);
	}
	if ( dwBytesWritten != iJunkChunkSize )
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
		return hRes;
	}

	// ASSERT( SetFilePointer(m_hFile, 0, NULL, FILE_CURRENT) == AVI_MOVILIST_OFFSET );

	DWORD dwTags[3];
	dwTags[0] = FOURCC_LIST;	// "LIST"
	dwTags[1] = m_dwMoviChunkSize;	// length to be filled later.
	dwTags[2] = listtypeAVIMOVIE;	// "movi"
	::WriteFile(m_File, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(dwTags))
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
		return hRes;
	}

	return S_OK;
}

#if 0
HRESULT CAVIFile::OpenAVI( const TCHAR* pszFileName, CVideoFrameForm& FrameForm, double fFrameRate, const CVideoCodec& VideoCodec, const CWaveFormat* pAudioFormat )
{
	HRESULT hRes = OpenAVICodec( FrameForm, fFrameRate, VideoCodec, pAudioFormat );

	hRes = OpenAVIFile( pszFileName );

	return hRes;
}
#endif

HRESULT CAVIFile::CloseAVI( bool bCloseCodec )
{
	// finish sequence compression
	if ( ! m_File.IsValidHandle())
	{
		return S_FALSE;
	}
	if ( m_VideoCodec.m_v.lpbiOut == NULL )
	{
		return E_FAIL;
	}

	// flush the buffers
	::FlushFileBuffers(m_File);

	// read the avi-header. So i can make my changes to it.
	AVI_FILE_HEADER afh;	// needs m_VideoCodec.m_v.lpbiOut
	AVI_AUDIO_HEADER aah;
	int iPadFile = InitFileHeader(afh, 
		false,
		(HasAudioFormat())?(&aah):NULL);

	// save update header back with my changes.
	DWORD dwBytesWritten = 0;
	::SetFilePointer(m_File, 0, NULL, FILE_BEGIN);
	::WriteFile(m_File, &afh, sizeof(afh), &dwBytesWritten, NULL);
	if ( HasVideo())
	{
		::WriteFile(m_File, m_pVideoHeader, m_dwVideoHeaderSize, &dwBytesWritten, NULL);
	}
	if ( HasAudioFormat())
	{
		::WriteFile(m_File, &aah, sizeof(aah), &dwBytesWritten, NULL);
	}

	// update movi chunk size
	::SetFilePointer(m_File, AVI_MOVILIST_OFFSET + 4, NULL, FILE_BEGIN);
	::WriteFile(m_File, &m_dwMoviChunkSize, sizeof(m_dwMoviChunkSize), &dwBytesWritten, NULL);

	// write some padding (if necessary) so that index always align at 16 bytes boundary
	::SetFilePointer(m_File, 0, NULL, FILE_END); 
	if (iPadFile > 0) 
	{ 
		BYTE zero = 0; 
		::WriteFile(m_File, &zero, 1, &dwBytesWritten, NULL);
	}

	m_Index.FlushIndexChunk(m_File);
	if ( bCloseCodec )
		m_VideoCodec.DestroyCodec();	// leave m_v.lpbiOut->bmiHeader.biCompression til now

	// close file
	m_File.CloseHandle();
	return S_OK;
}

// Reset AVI recording to a new file.
// If pszFileName is NULL the a query is done to see if the reset should be done, returns:
//    S_OK - file size threshold not yet reached
//    S_FALSE - file size threshold reached, a new file should be created ASAP to avoid corruping the AVI
// If pszFileName is not NULL then a new file is opened with the given filename
HRESULT CAVIFile::ResetAVIFile( const TCHAR* pszFileName )
{
	HRESULT hRes = S_OK;
	if ( m_dwMoviChunkSize > AVI_MOVI_SIZE_RESET_THRESHOLD || pszFileName != NULL )
	{
		// if no filename then indicate the reset needs to be done
		if ( pszFileName == NULL )
			return S_FALSE;

		// end compressor, close AVI, start compressor, open a new file
		m_VideoCodec.CompEnd();
		CloseAVI( false );
		// NOTE: unfortunately the file handle is used to detect recording so frames may
		// be dropped between the close and open.
		hRes = m_VideoCodec.CompStart(&m_biIn);
		if ( SUCCEEDED(hRes) )
			hRes = OpenAVIFile( pszFileName );
	}
	return hRes;
}

HRESULT CAVIFile::WriteVideoBlocks( CVideoFrame& frame, int nTimes )
{
	// compress and write a single frame.
	// ARGS:
	//  nTimes = FrameDups = duplicate this frame a few times. to make up for missed frames.
	// RETURN:
	//  bytes written
	// ASSUME: 
	//  This frame is at the correct frame rate.
	// NOTE: 
	//  This can be pretty slow. May be on background thread.

	if ( ! m_File.IsValidHandle() || ! HasVideo()) 
		return 0;
	// Make sure the codec is open?
	if ( nTimes <= 0 ) 
		return 0;
	ASSERT( ! memcmp( &frame, &m_FrameForm, sizeof(m_FrameForm)));
	DEBUG_TRACE(("CAVIFile:WriteVideoBlocks: called. writing frame %d time(s)" LOG_CR, nTimes ));

	HRESULT hCompressed;
	const void* pCompBuf;
	LONG nSizeComp;
	BOOL bIsKey = false;

	DWORD dwBytesWrittenTotal = 0;
	for (int i=0; i<nTimes; i++)
	{
		// compress the frame, using chosen compressor.
		// NOTE: we need to recompress the frame even tho it may be the same as last time!
		if ( i < 2 )
		{
			hCompressed = m_VideoCodec.CompFrame( frame, pCompBuf, nSizeComp, bIsKey );
			if ( FAILED(hCompressed))
			{
				DEBUG_ERR(("CAVIBuilder:m_VideoCodec.CompFrame FAIL %d" LOG_CR, hCompressed ));
				return hCompressed;
			}
		}
		else
		{
			// no change. so dont bother compessing it yet again.
			// ??? since we know this is not changed, can i just write a blank frame?
		}

		DEBUG_TRACE(("CAVIFile:WriteVideoBlocks: size=%d, bIsKey=%d" LOG_CR, nSizeComp, bIsKey ));

		DWORD dwTags[2];
		dwTags[0] = ( hCompressed == S_OK ) ? MAKEFOURCC('0', '0', 'd', 'c') : MAKEFOURCC('0', '0', 'd', 'b');
		dwTags[1] = nSizeComp;

		// NOTE: every frame needs indexed (since the AVI uses one)
		AVIINDEXENTRY indexentry;
		indexentry.ckid = dwTags[0]; 
		indexentry.dwFlags = (bIsKey) ? AVIIF_KEYFRAME : 0;
		indexentry.dwChunkOffset = m_dwMoviChunkSize;
		indexentry.dwChunkLength = dwTags[1];
		m_Index.AddFrame( indexentry );

		// write video frame
		DWORD dwBytesWritten = 0;
		::WriteFile(m_File, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
		if ( dwBytesWritten != sizeof(dwTags))
		{
			HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
			DEBUG_ERR(("CAVIFile:WriteVideoBlocks:WriteFile FAIL=0x%x" LOG_CR, hRes ));
			return hRes;
		}
		dwBytesWrittenTotal += dwBytesWritten;

		::WriteFile(m_File, pCompBuf, (DWORD) nSizeComp, &dwBytesWritten, NULL);
		if ( dwBytesWritten != nSizeComp )
		{
			HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
			DEBUG_ERR(("CAVIFile:WriteVideoBlocks:WriteFile FAIL=0x%x" LOG_CR, hRes ));
			return hRes;
		}
		dwBytesWrittenTotal += dwBytesWritten;

		if ( nSizeComp & 1 ) // pad to even size.
		{
			BYTE bPad;
			::WriteFile(m_File, &bPad, 1, &dwBytesWritten, NULL);
			m_dwMoviChunkSize++;
			dwBytesWrittenTotal ++;
		}

		m_dwTotalFrames ++;
		m_dwMoviChunkSize += sizeof(dwTags) + nSizeComp;
	}

	return dwBytesWrittenTotal;	// ASSUME not negative -> error
}

HRESULT CAVIFile::WriteAudioBlock( const BYTE* pWaveData, DWORD dwLength )
{
	// Write out the audio blocks associated with the video frames.
	if ( ! m_File.IsValidHandle() || !dwLength ) 
	{
		return 0;
	}
	if ( ! HasAudioFormat())	// this shouldnt happen!
	{
		return E_FAIL;
	}

	DWORD dwTags[2];
	dwTags[0] = MAKEFOURCC('0', '1', 'w', 'b');
	dwTags[1] = dwLength;
	
	// Add audio index
	// NOTE: audio would not play unless the audio chunks are indexed, all chunks need indexed
	AVIINDEXENTRY indexentry;
	indexentry.ckid = dwTags[0]; 
	indexentry.dwFlags = 0;
	indexentry.dwChunkOffset = m_dwMoviChunkSize;
	indexentry.dwChunkLength = dwTags[1];
	m_Index.AddFrame( indexentry );

	DWORD dwBytesWritten = 0;
	::WriteFile( m_File, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(dwTags))
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
		DEBUG_ERR(("CAVIFile:WriteAudioBlock:WriteFile FAIL=0x%x" LOG_CR, hRes ));
		return hRes;
	}

	::WriteFile( m_File, pWaveData, dwLength, &dwBytesWritten, NULL);
	if ( dwBytesWritten != dwLength )
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
		DEBUG_ERR(("CAVIFile:WriteAudioBlock:WriteFile FAIL=0x%x" LOG_CR, hRes ));
		return hRes;
	}

	m_dwMoviChunkSize += sizeof(dwTags) + dwLength;
	m_dwAudioBytes += dwLength;

	return( dwLength + sizeof(dwTags));
}

#ifdef _DEBUG
bool CAVIFile::UnitTest()
{
	// a unit test for the AVIFile.
	return true;
}
#endif
