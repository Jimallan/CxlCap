//
// GAPI_dx8.cpp
// must be in seperate file from DX9
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include "GAPI_Base.h"

#ifdef USE_DIRECTX8
#include <d3d8types.h>
#include <d3d8.h>

CTaksiGAPI_DX8 g_DX8;

//**************************************************************************
static IRefPtr<IDirect3DSurface8> s_pSurfTemp;

static IRefPtr<IDirect3DVertexBuffer8> s_pVB[ TAKSI_INDICATE_QTY ];
static IRefPtr<IDirect3DVertexBuffer8> s_pVBborder;

static DWORD s_dwSaveState_Them = 0L;
static DWORD s_dwSaveState_Me = 0L;

static D3DFORMAT s_bbFormat;

//***************************************
// IDirect3DDevice8 method-types
// function pointers 
typedef ULONG   (STDMETHODCALLTYPE *PFN_DX8_ADDREF)(IDirect3DDevice8* pDevice);
static PFN_DX8_ADDREF	s_D3D8_AddRef = NULL;
typedef ULONG   (STDMETHODCALLTYPE *PFN_DX8_RELEASE)(IDirect3DDevice8* pDevice);
static PFN_DX8_RELEASE s_D3D8_Release = NULL;
typedef HRESULT (STDMETHODCALLTYPE *PFN_DX8_RESET)(IDirect3DDevice8* pDevice, LPVOID);
static PFN_DX8_RESET   s_D3D8_Reset = NULL;
typedef HRESULT (STDMETHODCALLTYPE *PFN_DX8_PRESENT)(IDirect3DDevice8* pDevice, CONST RECT*, CONST RECT*, HWND, LPVOID);
static PFN_DX8_PRESENT s_D3D8_Present = NULL;

//***************************************

struct CUSTOMVERTEX 
{ 
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
	FLOAT x,y,z,w;
	D3DCOLOR color;
};

//********************************************************************************************************************

static HRESULT CreateVB( IDirect3DDevice8* pDevice, IRefPtr<IDirect3DVertexBuffer8>& pVB, const CUSTOMVERTEX* pVertSrc, int iSizeSrc )
{
	// Create a vertex buffer to display my overlay info 
	if ( pVB )
	{
		// Already set so i must need to destroy the old one first?!
		pVB.ReleaseRefObj();
		g_DX8.m_iRefCountMe--;
	}
	HRESULT hRes = pDevice->CreateVertexBuffer( 
		iSizeSrc, D3DUSAGE_WRITEONLY, 
		D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, 
		IREF_GETPPTR(pVB,IDirect3DVertexBuffer8));
	if (FAILED(hRes))
	{
		LOG_WARN(( "CreateVertexBuffer() FAILED. 0%x" LOG_CR, hRes ));
		return hRes;
	}
	g_DX8.m_iRefCountMe++;

	BYTE* pVertices;
	hRes = pVB->Lock(0, iSizeSrc, &pVertices, 0);
	if (FAILED(hRes))
	{
		LOG_WARN(( "pVB->Lock() FAILED. 0%x" LOG_CR, hRes ));
		return hRes;
	}
	if ( pVertices == NULL )
	{
		ASSERT(0);
		return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
	}
	memcpy(pVertices, pVertSrc, iSizeSrc);
	pVB->Unlock();
	return S_OK;
}

//***************************************************************************

HRESULT CTaksiGAPI_DX8::RestoreDeviceObjects()
{
	// create vertex buffer for black border
	static const CUSTOMVERTEX s_VertBorder[] =
	{
		{INDICATOR_X,        INDICATOR_Y,         0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
		{INDICATOR_X,        INDICATOR_Y+INDICATOR_Height, 0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
		{INDICATOR_X+INDICATOR_Width, INDICATOR_Y+INDICATOR_Height, 0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
		{INDICATOR_X+INDICATOR_Width, INDICATOR_Y,         0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
		{INDICATOR_X,        INDICATOR_Y,         0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
	};
	HRESULT hRes = CreateVB(m_pDevice,s_pVBborder,s_VertBorder,sizeof(s_VertBorder));
    if (FAILED(hRes))
    {
		LOG_WARN(( "InitVB() FAILED. 0x%x" LOG_CR, hRes ));
        return hRes;
    }

	// create vertex buffer for TAKSI_INDICATE_QTY
	for ( int i=0; i<COUNTOF(s_pVB); i++ )
	{
		static CUSTOMVERTEX s_Vert[4] =
		{
			{INDICATOR_X,        INDICATOR_Y,         0.0f, 1.0f, 0 }, // x, y, z, rhw, color
			{INDICATOR_X,        INDICATOR_Y+INDICATOR_Height, 0.0f, 1.0f, 0 }, // x, y, z, rhw, color
			{INDICATOR_X+INDICATOR_Width, INDICATOR_Y,         0.0f, 1.0f, 0 }, // x, y, z, rhw, color
			{INDICATOR_X+INDICATOR_Width, INDICATOR_Y+INDICATOR_Height, 0.0f, 1.0f, 0 }, // x, y, z, rhw, color
		};
		for ( int j=0; j<COUNTOF(s_Vert); j++ )
		{
			s_Vert[j].color = sm_IndColors[i];
		}
		hRes = CreateVB(m_pDevice,s_pVB[i],s_Vert,sizeof(s_Vert));
		if ( FAILED(hRes))
		{
			LOG_WARN(( "CreateVB(%d) FAILED. (0x%x)" LOG_CR, i, hRes ));
		    return hRes;
		}
	}

	D3DVIEWPORT8 vp;
	vp.X      = 0;
	vp.Y      = 0;
	vp.Width  = (UINT)( INDICATOR_X*2 + INDICATOR_Width );
	vp.Height = (UINT)( INDICATOR_Y*2 + INDICATOR_Height );
	vp.MinZ   = 0.0f;
	vp.MaxZ   = 1.0f;

	// Create the state blocks for rendering text
	for( UINT which=0; which<2; which++ )
	{
		m_pDevice->BeginStateBlock();

		m_pDevice->SetViewport( &vp );
		m_pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
		m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, false );
		m_pDevice->SetRenderState( D3DRS_FILLMODE,   D3DFILL_SOLID );
		m_pDevice->SetRenderState( D3DRS_CULLMODE,   D3DCULL_CW );
		m_pDevice->SetRenderState( D3DRS_STENCILENABLE,    false );
		m_pDevice->SetRenderState( D3DRS_CLIPPING, true );
		m_pDevice->SetRenderState( D3DRS_CLIPPLANEENABLE, false );
		m_pDevice->SetRenderState( D3DRS_FOGENABLE,        false );

		m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_DISABLE );
		m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

		m_pDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
		m_pDevice->SetPixelShader ( NULL );
		m_pDevice->SetStreamSource( 0, s_pVB[0], sizeof(CUSTOMVERTEX));

		if( which==0 )
			m_pDevice->EndStateBlock( &s_dwSaveState_Them );
		else
			m_pDevice->EndStateBlock( &s_dwSaveState_Me );
	}

    return S_OK;
}

HRESULT CTaksiGAPI_DX8::InvalidateDeviceObjects(bool bDetaching)
{
	// Desc: Destroys all device-dependent objects
	DEBUG_MSG(("CTaksiGAPI_DX8::InvalidateDeviceObjects: called." LOG_CR));
	//if (bDetaching) return S_OK;

    // Delete the state blocks
    if( m_pDevice && !bDetaching )
    {
		m_pDevice->DeleteStateBlock( s_dwSaveState_Them );
		m_pDevice->DeleteStateBlock( s_dwSaveState_Me );
		LOG_MSG(( "InvalidateDeviceObjects: DeleteStateBlock(s) done." LOG_CR));
    }

	if (s_pSurfTemp.IsValidRefObj())
	{
		s_pSurfTemp.ReleaseRefObj();
		m_iRefCountMe--;
	}

	for ( int i=0; i<COUNTOF(s_pVB); i++ )
	{
		if (s_pVB[i].IsValidRefObj())
		{
			s_pVB[i].ReleaseRefObj();
			m_iRefCountMe--;
		}
	}
	if (s_pVBborder.IsValidRefObj())
	{
		s_pVBborder.ReleaseRefObj();
		m_iRefCountMe--;
	}

	DEBUG_MSG(( "CTaksiGAPI_DX8::InvalidateDeviceObjects: done." LOG_CR));
    return S_OK;
}

//*********************************************************************

HWND CTaksiGAPI_DX8::GetFrameInfo( SIZE& rSize ) // virtual
{
	// Determine format of back buffer and its dimensions.
	if ( m_pDevice == NULL )
		return NULL;

	// get the 0th backbuffer.
	IRefPtr<IDirect3DSurface8> pBackBuffer;
	HRESULT hRes = m_pDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, 
		IREF_GETPPTR(pBackBuffer,IDirect3DSurface8));
	if (FAILED(hRes))
	{
		DEBUG_ERR(( "DX8:GetFrameInfo:m_pDevice->GetBackBuffer FAIL 0x%x" LOG_CR, hRes ));
		return NULL;
	}

	D3DSURFACE_DESC desc;
	hRes = pBackBuffer->GetDesc(&desc);

	s_bbFormat = desc.Format;
	rSize.cx = desc.Width;
	rSize.cy = desc.Height;

	// check dest.surface format
	int s_bpp = 0;
	switch(s_bbFormat)
	{
	case D3DFMT_R8G8B8:
		s_bpp = 3;
		break;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
		s_bpp = 2;
		break;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		s_bpp = 4;
		break;
	default:
		s_bpp = 0;
		return NULL;
	}

	// determine window handle 
	D3DDEVICE_CREATION_PARAMETERS params;	
	hRes = m_pDevice->GetCreationParameters(&params);
	if (FAILED(hRes))
	{
		DEBUG_ERR(( "DX8:GetFrameInfo:m_pDevice->GetCreationParameters FAIL 0x%x" LOG_CR, hRes ));
		return NULL;
	}

	return params.hFocusWindow;
}

//*********************************************************************************

static HRESULT GetFrameHalfSize( D3DLOCKED_RECT& lockedSrcRect, CVideoFrame& frame )
{
	// Copies pixel's RGB data into dest buffer. Alpha information is discarded. 

	DEBUG_TRACE(( "GetFrameHalfSize: called." LOG_CR ));

	int width = g_Proc.m_Stats.m_SizeWnd.cx;
	int height = g_Proc.m_Stats.m_SizeWnd.cy;
	int dHeight = frame.m_Size.cx;
	int dWidth = frame.m_Size.cy;

	// copy data
	BYTE* pSrcRow = (BYTE*)lockedSrcRect.pBits;
	int iSrcPitch = lockedSrcRect.Pitch;

	//DEBUG_TRACE(( "iSrcPitch = %d" LOG_CR, iSrcPitch));
	//DEBUG_TRACE(( "frame.m_pPixels = %d" LOG_CR, frame.m_pPixels));

	int sbpp = 0;
	int i, k;

	switch (s_bbFormat)
	{
	case D3DFMT_X1R5G5B5:
		//DEBUG_TRACE(( "GetFrameHalfSize: source s_bbFormat = D3DFMT_X1R5G5B5" LOG_CR));
		// 16-bit: some conversion needed.
		for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
		{
			int m=0;
			for ( int j=0; j<dWidth, m<width-1; j++, m+=2)
			{
				//  memory layout 16 bit:
				//  ---------------------------------
				//  Lo            Hi Lo            Hi
				//  b3b4b5b6b7g3g4g5 g6g7r3r4r5r6r700
				//  ---------------------------------
				//    blue      green      red
				//
				//                turns into:->
				//
				//  memory layout 24 bit:
				//  --------------------------------------------------
				//  Lo            Hi Lo            Hi Lo            Hi
				//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
				//  --------------------------------------------------
				//       blue            green              red

				/*
				// fetch 1 pixel: nearest neigbour
				b0 = pSrcRow[k*iSrcPitch + m*2];
				b1 = pSrcRow[k*iSrcPitch + m*2 + 1];

				frame.m_pPixels[i*frame.m_iPitch + j*3] = (b0 << 3) & 0xf8;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (b1 << 1) & 0xf8;
				*/

				// fetch 4 pixels: bilinear interpolation
				BYTE b0 = pSrcRow[k*iSrcPitch + m*2];
				BYTE b1 = pSrcRow[k*iSrcPitch + m*2 + 1];
				BYTE b00 = (b0 << 3) & 0xf8;
				BYTE b01 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				BYTE b02 = (b1 << 1) & 0xf8;
				b0 = pSrcRow[k*iSrcPitch + (m+1)*2];
				b1 = pSrcRow[k*iSrcPitch + (m+1)*2 + 1];
				BYTE b10 = (b0 << 3) & 0xf8;
				BYTE b11 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				BYTE b12 = (b1 << 1) & 0xf8;
				b0 = pSrcRow[(k+1)*iSrcPitch + m*2];
				b1 = pSrcRow[(k+1)*iSrcPitch + m*2 + 1];
				BYTE b20 = (b0 << 3) & 0xf8;
				BYTE b21 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				BYTE b22 = (b1 << 1) & 0xf8;
				b0 = pSrcRow[(k+1)*iSrcPitch + (m+1)*2];
				b1 = pSrcRow[(k+1)*iSrcPitch + (m+1)*2 + 1];
				BYTE b30 = (b0 << 3) & 0xf8;
				BYTE b31 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				BYTE b32 = (b1 << 1) & 0xf8;

				BYTE temp = (b00 + b10 + b20 + b30) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3] = (BYTE)temp;
				temp = (b01 + b11 + b21 + b31) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = (BYTE)temp; 
				temp = (b02 + b12 + b22 + b32) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (BYTE)temp;
			}
		}
		break;

	case D3DFMT_R5G6B5:
		//DEBUG_TRACE(( "GetFrameHalfSize: source s_bbFormat = D3DFMT_R5G6B5" LOG_CR));
		// 16-bit: some conversion needed.
		for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
		{
			int m=0;
			for ( int j=0; j<dWidth, m<width-1; j++, m+=2)
			{
				//  memory layout 16 bit:
				//  ---------------------------------
				//  Lo            Hi Lo            Hi
				//  b3b4b5b6b7g2g3g4 g5g6g7r3r4r5r6r7
				//  ---------------------------------
				//    blue      green      red
				//
				//                turns into:->
				//
				//  memory layout 24 bit:
				//  --------------------------------------------------
				//  Lo            Hi Lo            Hi Lo            Hi
				//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
				//  --------------------------------------------------
				//       blue            green              red

				/*
				// fetch 1 pixel: nearest neigbour
				b0 = pSrcRow[k*iSrcPitch + m*2];
				b1 = pSrcRow[k*iSrcPitch + m*2 + 1];

				frame.m_pPixels[i*frame.m_iPitch + j*3] = (b0 << 3) & 0xf8;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (b1 << 1) & 0xf8;
				*/

				// fetch 4 pixels: bilinear interpolation
				BYTE b0 = pSrcRow[k*iSrcPitch + m*2];
				BYTE b1 = pSrcRow[k*iSrcPitch + m*2 + 1];
				BYTE b00 = (b0 << 3) & 0xf8;
				BYTE b01 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
				BYTE b02 = b1 & 0xf8;
				b0 = pSrcRow[k*iSrcPitch + (m+1)*2];
				b1 = pSrcRow[k*iSrcPitch + (m+1)*2 + 1];
				BYTE b10 = (b0 << 3) & 0xf8;
				BYTE b11 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
				BYTE b12 = b1 & 0xf8;
				b0 = pSrcRow[(k+1)*iSrcPitch + m*2];
				b1 = pSrcRow[(k+1)*iSrcPitch + m*2 + 1];
				BYTE b20 = (b0 << 3) & 0xf8;
				BYTE b21 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
				BYTE b22 = b1 & 0xf8;
				b0 = pSrcRow[(k+1)*iSrcPitch + (m+1)*2];
				b1 = pSrcRow[(k+1)*iSrcPitch + (m+1)*2 + 1];
				BYTE b30 = (b0 << 3) & 0xf8;
				BYTE b31 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
				BYTE b32 = b1 & 0xf8;

				BYTE temp = (b00 + b10 + b20 + b30) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3] = (BYTE)temp;
				temp = (b01 + b11 + b21 + b31) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = (BYTE)temp; 
				temp = (b02 + b12 + b22 + b32) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (BYTE)temp;
			}
		}
		break;

	case D3DFMT_R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		//DEBUG_TRACE(( "GetFrameHalfSize: source s_bbFormat = D3DFMT_R8G8B or D3DFMT_A8R8G8B8 or D3DFMT_X8R8G8B8" LOG_CR));
		sbpp = (s_bbFormat == D3DFMT_R8G8B8)? 3 : 4;
		for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
		{
			int m=0;
			for (int j=0; j<dWidth, m<width-1; j++, m+=2)
			{
				/*
				// fetch 1 pixel: nearest neigbour
				frame.m_pPixels[i*frame.m_iPitch + j*3] = pSrcRow[k*iSrcPitch + m*sbpp];
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = pSrcRow[k*iSrcPitch + m*sbpp + 1];
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = pSrcRow[k*iSrcPitch + m*sbpp + 2];
				*/

				// fetch 4 pixels: bilinear interpolation
				BYTE temp = (pSrcRow[k*iSrcPitch + m*sbpp] + 
					pSrcRow[k*iSrcPitch + (m+1)*sbpp] + 
					pSrcRow[(k+1)*iSrcPitch + m*sbpp] +
					pSrcRow[(k+1)*iSrcPitch + (m+1)*sbpp]) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3] = (BYTE)temp;
				temp = (pSrcRow[k*iSrcPitch + m*sbpp + 1] + 
					pSrcRow[k*iSrcPitch + (m+1)*sbpp + 1] + 
					pSrcRow[(k+1)*iSrcPitch + m*sbpp + 1] +
					pSrcRow[(k+1)*iSrcPitch + (m+1)*sbpp + 1]) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = (BYTE)temp;
				temp = (pSrcRow[k*iSrcPitch + m*sbpp + 2] + 
					pSrcRow[k*iSrcPitch + (m+1)*sbpp + 2] + 
					pSrcRow[(k+1)*iSrcPitch + m*sbpp + 2] +
					pSrcRow[(k+1)*iSrcPitch + (m+1)*sbpp + 2]) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (BYTE)temp;
			}
		}
		break;
	default:
		ASSERT(0);
		return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
	}

	return S_OK;
}

static HRESULT GetFrameFullSize( D3DLOCKED_RECT& lockedSrcRect, CVideoFrame& frame )
{
	// Copies pixel's RGB data into dest buffer. Alpha information is discarded. 
	// DEBUG_TRACE(( "GetFrameFullSize: called." LOG_CR ));

	int width = frame.m_Size.cx;
	int height = frame.m_Size.cy;

	// copy data
	BYTE* pSrcRow = (BYTE*)lockedSrcRect.pBits;
	int iSrcPitch = lockedSrcRect.Pitch;

	//DEBUG_TRACE(( "width = %d" LOG_CR, width));
	//DEBUG_TRACE(( "height = %d" LOG_CR, width));
	//DEBUG_TRACE(( "iSrcPitch = %d" LOG_CR, iSrcPitch));
	//DEBUG_TRACE(( "frame.m_pPixels = %d" LOG_CR, frame.m_pPixels));

	int i, k;
	switch (s_bbFormat)
	{
	case D3DFMT_R8G8B8:
		DEBUG_TRACE(( "GetFrameFullSize: source format = D3DFMT_R8G8B8" LOG_CR));
		// 24-bit: straight copy
		for (i=0, k=height-1; i<height, k>=0; i++, k--)
		{
			memcpy(frame.m_pPixels + i*frame.m_iPitch, pSrcRow + k*iSrcPitch, 3*width);
		}
		break;

	case D3DFMT_X1R5G5B5:
		DEBUG_TRACE(( "GetFrameFullSize: source format = D3DFMT_X1R5G5B5" LOG_CR));
		// 16-bit: some conversion needed.
		for (i=0, k=height-1; i<height, k>=0; i++, k--)
		{
			for (int j=0; j<width; j++)
			{
				BYTE b0 = pSrcRow[k*iSrcPitch + j*2];
				BYTE b1 = pSrcRow[k*iSrcPitch + j*2 + 1];

				//  memory layout 16 bit:
				//  ---------------------------------
				//  Lo            Hi Lo            Hi
				//  b3b4b5b6b7g3g4g5 g6g7r3r4r5r6r700
				//  ---------------------------------
				//    blue      green      red
				//
				//                turns into:->
				//
				//  memory layout 24 bit:
				//  --------------------------------------------------
				//  Lo            Hi Lo            Hi Lo            Hi
				//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
				//  --------------------------------------------------
				//       blue            green              red

				frame.m_pPixels[i*frame.m_iPitch + j*3] = (b0 << 3) & 0xf8;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (b1 << 1) & 0xf8;
			}
		}
		break;

	case D3DFMT_R5G6B5:
		DEBUG_TRACE(( "GetFrameFullSize: source format = D3DFMT_R5G6B5" LOG_CR));
		// 16-bit: some conversion needed.
		for (i=0, k=height-1; i<height, k>=0; i++, k--)
		{
			for (int j=0; j<width; j++)
			{
				BYTE b0 = pSrcRow[k*iSrcPitch + j*2];
				BYTE b1 = pSrcRow[k*iSrcPitch + j*2 + 1];

				//  memory layout 16 bit:
				//  ---------------------------------
				//  Lo            Hi Lo            Hi
				//  b3b4b5b6b7g2g3g4 g5g6g7r3r4r5r6r7
				//  ---------------------------------
				//    blue      green      red
				//
				//                turns into:->
				//
				//  memory layout 24 bit:
				//  --------------------------------------------------
				//  Lo            Hi Lo            Hi Lo            Hi
				//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
				//  --------------------------------------------------
				//       blue            green              red

				frame.m_pPixels[i*frame.m_iPitch + j*3] = (b0 << 3) & 0xf8;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = b1 & 0xf8;
			}
		}
		break;

	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		DEBUG_TRACE(( "GetFrameFullSize: source format = D3DFMT_A8R8G8B8 or D3DFMT_X8R8G8B8" LOG_CR));
		// 32-bit entries: discard alpha
		for (i=0, k=height-1; i<height, k>=0; i++, k--)
		{
			for (int j=0; j<width; j++)
			{
				frame.m_pPixels[i*frame.m_iPitch + j*3] = pSrcRow[k*iSrcPitch + j*4];
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = pSrcRow[k*iSrcPitch + j*4 + 1];
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = pSrcRow[k*iSrcPitch + j*4 + 2];
			}
		}
		break;
	default:
		ASSERT(0);
		return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
	}

	return S_OK;
}

HRESULT CTaksiGAPI_DX8::GetFrame( CVideoFrame& frame, bool bHalfSize )
{
	// check dest.surface format
	switch(s_bbFormat)
	{
	case D3DFMT_R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
		break;
	default:
		LOG_WARN(( "GetFrame: surface s_bbFormat=0x%x not supported." LOG_CR, s_bbFormat ));
		return HRESULT_FROM_WIN32(ERROR_CTX_BAD_VIDEO_MODE);
	}

	IRefPtr<IDirect3DSurface8> pBackBuffer;
	HRESULT hRes = m_pDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, 
		IREF_GETPPTR(pBackBuffer,IDirect3DSurface8));
	if (FAILED(hRes))
	{
		LOG_WARN(( "GetFrame: FAILED to get back-pBackBuffer 0x%x" LOG_CR, hRes ));
		return hRes;
	}

	// copy rect to a new surface and lock the rect there.
	// NOTE: apparently, this works faster than locking a rect on the back buffer itself.
	// LOOKS LIKE: reading from the backbuffer is faster via CopyRects, but writing
	// is faster if done directly.
	if (s_pSurfTemp == NULL)
	{
		hRes = m_pDevice->CreateImageSurface(g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy, s_bbFormat, 
			IREF_GETPPTR(s_pSurfTemp,IDirect3DSurface8));
		if (FAILED(hRes))
		{
			LOG_WARN(( "GetFrame: FAILED to create image surface. 0x%x" LOG_CR, hRes ));
			return hRes;
		}
		m_iRefCountMe++;
	}

	RECT Rect = {0, 0, g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy};
	POINT points[] = {{0, 0}};

	CLOCK_START(a);
	hRes = m_pDevice->CopyRects(pBackBuffer, &Rect, 1, s_pSurfTemp, points);
	if (FAILED(hRes))
	{
		LOG_WARN(( "GetFrame: CopyRects() FAILED for image surface 0x%x." LOG_CR, hRes ));
		return hRes;
	}
	CLOCK_STOP(a,"CTaksiGAPI_DX8:CopyRects: clock=%10d");

	D3DLOCKED_RECT lockedSrcRect;
	RECT newRect = {0, 0, g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy};

	CLOCK_START(b);
	hRes = s_pSurfTemp->LockRect(&lockedSrcRect, &newRect, 0);
	if (FAILED(hRes))
	{
		LOG_WARN(( "GetFrame: FAILED to lock source rect. 0x%x" LOG_CR, hRes ));
		return hRes;
	}
	CLOCK_STOP(b,"CTaksiGAPI_DX8::LockRect: clock=%10d");

	CLOCK_START(c);
	if (bHalfSize)
		hRes = GetFrameHalfSize(lockedSrcRect, frame );
	else
		hRes = GetFrameFullSize(lockedSrcRect, frame );
	CLOCK_STOP(c,"CTaksiGAPI_DX8::GetFrame*Size clock=%10d");

	if (s_pSurfTemp)
	{
		s_pSurfTemp->UnlockRect();
	}

	return hRes;
}

//*********************************************************************************

HRESULT CTaksiGAPI_DX8::DrawIndicator( TAKSI_INDICATE_TYPE eIndicate )
{
	ASSERT(m_pDevice);
	ASSERT( eIndicate >= 0 && eIndicate < COUNTOF(s_pVB));
	if (s_pVB[eIndicate] == NULL) 
	{
		HRESULT hRes = RestoreDeviceObjects();
		if (FAILED(hRes))
		{
			LOG_WARN(( "RestoreDeviceObjects() FAILED 0x%x" LOG_CR, hRes ));
			return hRes;
		}
		DEBUG_TRACE(( "RestoreDeviceObjects() done." LOG_CR));
	}

	// setup renderstate
	HRESULT hRes = m_pDevice->CaptureStateBlock(s_dwSaveState_Them);
	if (FAILED(hRes))
	{
		return hRes;
	}
	hRes = m_pDevice->ApplyStateBlock(s_dwSaveState_Me);

	// render
	hRes = m_pDevice->BeginScene();
	if ( SUCCEEDED(hRes))
	{
		m_pDevice->SetStreamSource( 0, s_pVB[eIndicate], sizeof(CUSTOMVERTEX));
		m_pDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

		m_pDevice->SetStreamSource( 0, s_pVBborder, sizeof(CUSTOMVERTEX));
		m_pDevice->DrawPrimitive( D3DPT_LINESTRIP, 0, 4 );

		m_pDevice->EndScene();
	}

	// restore the modified renderstates
	m_pDevice->ApplyStateBlock(s_dwSaveState_Them);
	return hRes;
}

//************************************************************************

EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE DX8_Reset(
	IDirect3DDevice8* pDevice, LPVOID params )
{
	// Hook Reset function 
	DEBUG_TRACE(( "#################################" LOG_CR));
	DEBUG_MSG(( "DX8_Reset: called." LOG_CR));

	g_DX8.m_pDevice = pDevice;
	g_DX8.m_Hook_Reset.SwapOld(s_D3D8_Reset);
	g_DX8.RecordAVI_Reset();
	g_DX8.InvalidateDeviceObjects(false);

	// call real Present()
	HRESULT hRes = s_D3D8_Reset(pDevice, params);

	DEBUG_MSG(( "DX8_Reset: Reset() is done. About to return." LOG_CR));

	g_DX8.m_Hook_Reset.SwapReset(s_D3D8_Reset);
	return hRes;
}

EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE DX8_AddRef(IDirect3DDevice8* pDevice)
{
	// Hook AddRef function
	g_DX8.m_iRefCount = s_D3D8_AddRef(pDevice);
	DEBUG_TRACE(( "DX8_AddRef: called (m_iRefCount = %d)." LOG_CR, g_DX8.m_iRefCount));
	return g_DX8.m_iRefCount;
}

EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE DX8_Release(IDirect3DDevice8* pDevice)
{
	// New Release function
	// a "fall-through" case
	if ((g_DX8.m_iRefCount > g_DX8.m_iRefCountMe + 1) && s_D3D8_Release) 
	{
		g_DX8.m_iRefCount = s_D3D8_Release(pDevice);
		DEBUG_TRACE(( "DX8_Release: called (m_iRefCount = %d)." LOG_CR, g_DX8.m_iRefCount));
		return g_DX8.m_iRefCount;
	}

	g_DX8.m_pDevice = pDevice;

	DEBUG_TRACE(( "+++++++++++++++++++++++++++++++++++++" LOG_CR));
	DEBUG_TRACE(( "DX8_Release: called." LOG_CR));
	DEBUG_TRACE(( "DX8_Release: pDevice = %08x" LOG_CR, (UINT_PTR)pDevice));
	DEBUG_TRACE(( "DX8_Release: VTABLE[0] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[0]));
	DEBUG_TRACE(( "DX8_Release: VTABLE[1] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[1]));
	DEBUG_TRACE(( "DX8_Release: VTABLE[2] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[2]));

	// unhook device methods
	g_DX8.RecordAVI_Reset();
	g_DX8.UnhookFunctions();
	g_DX8.InvalidateDeviceObjects(false);

	// reset the pointers
	g_DX8.m_Hook_AddRef = NULL;
	g_DX8.m_Hook_Release = NULL;

	// call the real Release()
	DEBUG_MSG(( "DX8_Release: about to call real Release." LOG_CR));
	g_DX8.m_iRefCount = s_D3D8_Release(pDevice);

	DEBUG_MSG(( "DX8_Release: UNHOOK m_iRefCount = %d" LOG_CR, g_DX8.m_iRefCount));

	return g_DX8.m_iRefCount;
}

EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE DX8_Present(
	IDirect3DDevice8* pDevice, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	// New Present function 
	DEBUG_TRACE(( "--------------------------------" LOG_CR));
	DEBUG_TRACE(( "DX8_Present: called." LOG_CR));
	
	g_DX8.m_pDevice = pDevice;
	g_DX8.m_Hook_Present.SwapOld(s_D3D8_Present);

	// rememeber IDirect3DDevice8::Release pointer so that we can clean-up properly.
	if (g_DX8.m_Hook_AddRef == NULL || g_DX8.m_Hook_Release == NULL)
	{
		DEBUG_TRACE(( "DX8_Present:pDevice  = %08x", (UINT_PTR)pDevice));

		UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)pDevice));
		ASSERT(pVTable);
		g_DX8.m_Hook_AddRef = pVTable + 1;
		g_DX8.m_Hook_Release = pVTable + 2;

		DEBUG_TRACE(( "DX8_Present:*m_Hook_AddRef  = %08x" LOG_CR, *g_DX8.m_Hook_AddRef));
		DEBUG_TRACE(( "DX8_Present:*m_Hook_Release  = %08x" LOG_CR, *g_DX8.m_Hook_Release));

		// hook AddRef method
		s_D3D8_AddRef = (PFN_DX8_ADDREF)(*g_DX8.m_Hook_AddRef);
		*g_DX8.m_Hook_AddRef = (UINT_PTR)DX8_AddRef;

		// hook Release method
		s_D3D8_Release = (PFN_DX8_RELEASE)(*g_DX8.m_Hook_Release);
		*g_DX8.m_Hook_Release = (UINT_PTR)DX8_Release;
	}
	
	g_DX8.PresentFrameBegin(true);

	// call real Present()
	HRESULT hRes = s_D3D8_Present(pDevice, src, dest, hWnd, unused);

	g_DX8.PresentFrameEnd();
	g_DX8.m_Hook_Present.SwapReset(s_D3D8_Present);
	return hRes;
}

//************************************************************************

CTaksiGAPI_DX8::CTaksiGAPI_DX8()
	: m_pDevice( NULL )
	, m_iRefCount( 1 )
	, m_iRefCountMe( 0 )
	, m_Hook_AddRef(NULL)
	, m_Hook_Release(NULL)
{
}

CTaksiGAPI_DX8::~CTaksiGAPI_DX8()
{
}

HRESULT CTaksiGAPI_DX8::HookFunctions()
{
	// ONLY CALLED FROM AttachGAPI()
	// This function hooks two IDirect3DDevice8 methods
	// hook IDirect3DDevice8::Present(), using code modifications at run-time.
	// ALGORITHM: we overwrite the beginning of real IDirect3DDevice8::Present
	// with a JMP instruction to our routine (DX8_Present).
	// When our routine gets called, first thing that it does - it restores 
	// the original bytes of IDirect3DDevice8::Present, then performs its pre-call tasks, 
	// then calls IDirect3DDevice8::Present, then does post-call tasks, then writes
	// the JMP instruction back into the beginning of IDirect3DDevice8::Present, and
	// returns.
	
	ASSERT( IsValidDll());
	if ( m_bHookedFunctions )
	{
		return S_FALSE;
	}
	if (!sg_Shared.m_nDX8_Present || !sg_Shared.m_nDX8_Reset)
	{
		LOG_WARN(( "CTaksiGAPI_DX8::HookFunctions: No info on 'Present' and/or 'Reset'." LOG_CR ));
		return HRESULT_FROM_WIN32(ERROR_INVALID_HOOK_HANDLE);
	}

	s_D3D8_Present = (PFN_DX8_PRESENT)(get_DllInt() + sg_Shared.m_nDX8_Present);
	s_D3D8_Reset = (PFN_DX8_RESET)(get_DllInt() + sg_Shared.m_nDX8_Reset);

	DEBUG_MSG(( "CTaksiGAPI_DX8::HookFunctions:checking JMP-implants..." LOG_CR));

	if ( ! m_Hook_Present.InstallHook(s_D3D8_Present,DX8_Present))
	{
		LOG_WARN(( "CTaksiGAPI_DX8::HookFunctions: FAILED to InstallHook!" LOG_CR));
		return HRESULT_FROM_WIN32(ERROR_INVALID_HOOK_HANDLE);
	}
	m_Hook_Reset.InstallHook(s_D3D8_Reset,DX8_Reset);
	return __super::HookFunctions();
}

void CTaksiGAPI_DX8::UnhookFunctions()
{
	// Restore original Reset() and Present()
	if ( !IsValidDll())
		return;

	if (m_Hook_AddRef != NULL && s_D3D8_AddRef != NULL)
		*m_Hook_AddRef = (UINT_PTR)s_D3D8_AddRef;
	if (m_Hook_Release != NULL && s_D3D8_Release != NULL)
		*m_Hook_Release = (UINT_PTR)s_D3D8_Release;

	// restore IDirect3D8Device methods
	m_Hook_Present.RemoveHook(s_D3D8_Present);
	m_Hook_Reset.RemoveHook(s_D3D8_Reset);

	__super::UnhookFunctions();
}

void CTaksiGAPI_DX8::FreeDll()
{
	// This function is called by the main DLL thread, when
	// the DLL is about to detach. All the memory clean-ups should be done here.
	// invalidate all our D3D objects
	if ( ! IsValidDll())
		return;

	InvalidateDeviceObjects(true);
	
	// restore hooked functions if DLL is still loaded
	if ( ::GetModuleHandle(get_DLLName()))
		UnhookFunctions();

	DEBUG_MSG(( "CTaksiGAPI_DX8::FreeDll:done." LOG_CR));
	__super::FreeDll();
}

#endif
