//
// GAPI_dx9.cpp
// must be in seperate file from DX8
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include "GAPI_Base.h"

#ifdef USE_DIRECTX9
#include <d3d9types.h>
#include <d3d9.h>

CTaksiGAPI_DX9 g_DX9;

//**************************************************************************

static IRefPtr<IDirect3DVertexBuffer9> s_pVB[ TAKSI_INDICATE_QTY ];
static IRefPtr<IDirect3DVertexBuffer9> s_pVBborder;

static IRefPtr<IDirect3DStateBlock9> s_pSavedState_Them;
static IRefPtr<IDirect3DStateBlock9> s_pSavedState_Me;

static D3DFORMAT s_bbFormat;

//***************************************
// IDirect3DDevice9 method-types 
// function pointers 
typedef ULONG   (STDMETHODCALLTYPE *PFN_DX9_ADDREF)(IDirect3DDevice9* pDevice);
static PFN_DX9_ADDREF s_D3D9_AddRef = NULL;
typedef ULONG   (STDMETHODCALLTYPE *PFN_DX9_RELEASE)(IDirect3DDevice9* pDevice);
static PFN_DX9_RELEASE s_D3D9_Release = NULL;
typedef HRESULT (STDMETHODCALLTYPE *PFN_DX9_RESET)(IDirect3DDevice9* pDevice, LPVOID);
static PFN_DX9_RESET   s_D3D9_Reset = NULL;
typedef HRESULT (STDMETHODCALLTYPE *PFN_DX9_PRESENT)(IDirect3DDevice9* pDevice, const RECT*, const RECT*, HWND, LPVOID);
static PFN_DX9_PRESENT s_D3D9_Present = NULL;
typedef HRESULT (STDMETHODCALLTYPE *PFN_DX9_SCPRESENT)(IDirect3DSwapChain9* pSwapChain, const RECT*,const RECT*, HWND, LPVOID, DWORD);
static PFN_DX9_SCPRESENT s_D3D9_SCPresent = NULL;

//********************************

struct CUSTOMVERTEX 
{ 
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
	FLOAT x,y,z,w;
	DWORD color;
};

//*********************************************************************************

static HRESULT CreateVB( IDirect3DDevice9* pDevice, IRefPtr<IDirect3DVertexBuffer9>& pVB, const CUSTOMVERTEX* pVertSrc, int iSizeSrc )
{
	// Create a vertex buffer to display my overlay info 
	ASSERT(pDevice);
	if ( pVB )
	{
		// Already set so i must need to destroy the old one first?!
		pVB.ReleaseRefObj();
		g_DX9.m_iRefCountMe--;
	}

	HRESULT hRes = pDevice->CreateVertexBuffer( 
		iSizeSrc, D3DUSAGE_WRITEONLY, 
		D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, 
		IREF_GETPPTR(pVB,IDirect3DVertexBuffer9), NULL );
	if (FAILED(hRes))
	{
		LOG_WARN(( "CreateVertexBuffer() FAILED. 0x%x" LOG_CR, hRes ));
		return hRes;
	}
	g_DX9.m_iRefCountMe++;

	void* pVertices;
	hRes = pVB->Lock(0, iSizeSrc, &pVertices, 0);
	if (FAILED(hRes))
	{
		LOG_WARN(( "s_pVB->Lock() FAILED. 0x%x" LOG_CR, hRes ));
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

//*********************************************************************************

HRESULT CTaksiGAPI_DX9::RestoreDeviceObjects()
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
		LOG_WARN(( "CreateVB() FAILED. (0x%x)" LOG_CR, hRes ));
        return hRes;
    }
	LOG_MSG(( "CreateVB() done." LOG_CR));

	// create vertex buffer for TAKSI_INDICATE_QTY
	for ( int i=0; i<COUNTOF(s_pVB); i++ )
	{
		static CUSTOMVERTEX s_Vert[4] =
		{
			{INDICATOR_X,        INDICATOR_Y,         0.0f, 1.0f, 0x0 }, // x, y, z, rhw, color
			{INDICATOR_X,        INDICATOR_Y+INDICATOR_Height, 0.0f, 1.0f, 0x0 }, // x, y, z, rhw, color
			{INDICATOR_X+INDICATOR_Width, INDICATOR_Y,         0.0f, 1.0f, 0x0 }, // x, y, z, rhw, color
			{INDICATOR_X+INDICATOR_Width, INDICATOR_Y+INDICATOR_Height, 0.0f, 1.0f, 0x0 }, // x, y, z, rhw, color
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

	D3DVIEWPORT9 vp;
	vp.X      = 0;
	vp.Y      = 0;
	vp.Width  = (UINT)( INDICATOR_X*2 + INDICATOR_Width );
	vp.Height = (UINT)( INDICATOR_Y*2 + INDICATOR_Height );
	vp.MinZ   = 0.0f;
	vp.MaxZ   = 1.0f;

	// Create the state blocks for rendering text
	for( UINT which=0; which<2; which++ )
	{
		hRes = m_pDevice->BeginStateBlock();

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

		m_pDevice->SetVertexShader( NULL );
		m_pDevice->SetFVF( D3DFVF_CUSTOMVERTEX );
		m_pDevice->SetPixelShader( NULL );
		m_pDevice->SetStreamSource( 0, s_pVB[0], 0, sizeof(CUSTOMVERTEX));

		if( which==0 )
		{
			m_pDevice->EndStateBlock( IREF_GETPPTR(s_pSavedState_Them,IDirect3DStateBlock9));
			m_iRefCountMe++;
		}
		else
		{
			m_pDevice->EndStateBlock( IREF_GETPPTR(s_pSavedState_Me,IDirect3DStateBlock9));
			m_iRefCountMe++;
		}
	}
    return S_OK;
}

HRESULT CTaksiGAPI_DX9::InvalidateDeviceObjects()
{
	// Desc: Release all device-dependent objects

	int iRefCountPrev = m_iRefCountMe;
	DEBUG_TRACE(( "CTaksiGAPI_DX9:InvalidateDeviceObjects: called." LOG_CR));

    // Delete the state blocks
	if (s_pSavedState_Them.IsValidRefObj())
	{
		s_pSavedState_Them.ReleaseRefObj();
		m_iRefCountMe--;
	}
	if (s_pSavedState_Me.IsValidRefObj())
	{
		s_pSavedState_Me.ReleaseRefObj();
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

	if ( iRefCountPrev != m_iRefCountMe ) // some work was done.
	{
		DEBUG_MSG(( "CTaksiGAPI_DX9:InvalidateDeviceObjects: done." LOG_CR));
	}
    return S_OK;
}

//****************************************************************

HWND CTaksiGAPI_DX9::GetFrameInfo( SIZE& rSize ) // virtual
{
	// Determine format of back buffer and its dimensions.
	// will set m_bGotFrameInfo
	if ( m_pDevice == NULL )	// PresentFrameBegin not called yet!
	{
		return NULL;
	}

	// get the 0th backbuffer.
	IRefPtr<IDirect3DSurface9> backBuffer;
	HRESULT hRes = m_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, 
		IREF_GETPPTR(backBuffer,IDirect3DSurface9));
	if (FAILED(hRes))
	{
		DEBUG_ERR(( "DX9:GetFrameInfo:m_pDevice->GetBackBuffer FAIL 0x%x" LOG_CR, hRes ));
		return NULL;
	}

	D3DSURFACE_DESC desc;
	backBuffer->GetDesc(&desc);

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
		DEBUG_ERR(( "DX9:GetFrameInfo:m_pDevice->GetCreationParameters FAIL 0x%x" LOG_CR, hRes ));
		return NULL;
	}

	return params.hFocusWindow;
}

//*********************************************************************************

static HRESULT GetFrameFullSize( D3DLOCKED_RECT& lockedSrcRect, CVideoFrame& frame )
{
	// Copies pixel's RGB data into dest buffer. Alpha information is discarded. 

	ASSERT(frame.m_pPixels);
	int width = frame.m_Size.cx;
	int height = frame.m_Size.cy;

	// copy data
	BYTE* pSrcRow = (BYTE*)lockedSrcRect.pBits;
	ASSERT(pSrcRow);
	int iSrcPitch = lockedSrcRect.Pitch;

	//DEBUG_TRACE(( "iSrcPitch = %d" LOG_CR, iSrcPitch));
	//DEBUG_TRACE(( "frame.m_iPitch = %d" LOG_CR, frame.m_iPitch));

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

static HRESULT GetFrameHalfSize( D3DLOCKED_RECT& lockedSrcRect, CVideoFrame& frame )
{
	// Copies pixel's RGB data into dest buffer. Alpha information is discarded. 
	//DEBUG_TRACE(( "GetFrameHalfSize: called." LOG_CR));

	ASSERT(frame.m_pPixels);
	int width = g_Proc.m_Stats.m_SizeWnd.cx;
	int height = g_Proc.m_Stats.m_SizeWnd.cy;
	int dHeight = frame.m_Size.cx;
	int dWidth = frame.m_Size.cy;

	// copy data
	BYTE* pSrcRow = (BYTE*)lockedSrcRect.pBits;
	ASSERT(pSrcRow);
	INT iSrcPitch = lockedSrcRect.Pitch;
	ASSERT(iSrcPitch);

	//DEBUG_TRACE(( "iSrcPitch = %d" LOG_CR, iSrcPitch));
	//DEBUG_TRACE(( "frame.m_iPitch = %d" LOG_CR, frame.m_iPitch));

	int sbpp = 0;
	int i, k;

	switch (s_bbFormat)
	{
	case D3DFMT_X1R5G5B5:
		//DEBUG_TRACE(( "GetFrameHalfSize: source format = D3DFMT_X1R5G5B5" LOG_CR));
		// 16-bit: some conversion needed.
		for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
		{
			int m=0;
			for (int j=0; j<dWidth, m<width-1; j++, m+=2)
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

				UINT temp = (b00 + b10 + b20 + b30) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3] = (BYTE)temp;
				temp = (b01 + b11 + b21 + b31) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 1] = (BYTE)temp; 
				temp = (b02 + b12 + b22 + b32) >> 2;
				frame.m_pPixels[i*frame.m_iPitch + j*3 + 2] = (BYTE)temp;
			}
		}
		break;

	case D3DFMT_R5G6B5:
		//DEBUG_TRACE(( "GetFrameHalfSize: source format = D3DFMT_R5G6B5" LOG_CR));
		// 16-bit: some conversion needed.
		for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
		{
			int m=0;
			for (int j=0; j<dWidth, m<width-1; j++, m+=2)
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

				UINT temp = (b00 + b10 + b20 + b30) >> 2;
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
		//DEBUG_TRACE(( "GetFrameHalfSize: source format = D3DFMT_R8G8B or D3DFMT_A8R8G8B8 or D3DFMT_X8R8G8B8" LOG_CR));
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
				UINT temp = (pSrcRow[k*iSrcPitch + m*sbpp] + 
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

HRESULT CTaksiGAPI_DX9::GetFrame( CVideoFrame& frame, bool bHalfSize )
{
	// check dest.surface format
	switch (s_bbFormat)
	{
	case D3DFMT_R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
		break;
	default:
		LOG_WARN(( "GetFrameFullSize: surface format=0x%x not supported." LOG_CR, s_bbFormat ));
		return HRESULT_FROM_WIN32(ERROR_CTX_BAD_VIDEO_MODE);
	}

	IRefPtr<IDirect3DSurface9> pBackBuffer;
	HRESULT hRes = m_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, 
		IREF_GETPPTR(pBackBuffer,IDirect3DSurface9));
	if (FAILED(hRes))
	{
		LOG_WARN(( "GetFramePrep: FAILED to get back-buffer 0x%x" LOG_CR, hRes ));
		return hRes;
	}

	// copy rect to a new surface and lock the rect there.
	// NOTE: apparently, this works faster than locking a rect on the back buffer itself.
	// LOOKS LIKE: reading from the backbuffer is faster via GetRenderTargetData, but writing
	// is faster if done directly.
	IRefPtr<IDirect3DSurface9> pSurfTemp;
	hRes = m_pDevice->CreateOffscreenPlainSurface(
		g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy, 
		s_bbFormat, D3DPOOL_SYSTEMMEM, 
		IREF_GETPPTR(pSurfTemp,IDirect3DSurface9), NULL );
	if (FAILED(hRes))
	{
		LOG_WARN(( "GetFramePrep: FAILED to create image surface. 0x%x" LOG_CR, hRes ));
		return hRes;
	}

	//DEBUG_TRACE(( "GetFrameFullSize: created image surface." LOG_CR));
	//DEBUG_TRACE(( "GetFrameFullSize: pD3DDevice = %d" LOG_CR, (UINT_PTR) m_pDevice ));
	//DEBUG_TRACE(( "GetFrameFullSize: pBackBuffer = %d" LOG_CR, (UINT_PTR)pBackBuffer));
	//DEBUG_TRACE(( "GetFrameFullSize: rect = %d" LOG_CR, (DWORD)rect));
	//DEBUG_TRACE(( "GetFrameFullSize: imgSurf = %d" LOG_CR, (UINT_PTR)imgSurf));

	hRes = m_pDevice->GetRenderTargetData(pBackBuffer, pSurfTemp);
	if (FAILED(hRes))
	{
		// This method will fail if:
		// The render target is multisampled.
		// The source render target is a different size than the destination surface.
		// The source render target and destination surface formats do not match.
		LOG_WARN(( "GetFramePrep: GetRenderTargetData() FAILED for image surface.(0x%x)" LOG_CR, hRes));
		return hRes;
	}
	
	CLOCK_START(b);

	D3DLOCKED_RECT lockedSrcRect;
	RECT newRect = {0, 0, g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy};
	hRes = pSurfTemp->LockRect( &lockedSrcRect, &newRect, 0);
	if (FAILED(hRes))
	{
		LOG_WARN(( "GetFramePrep: FAILED to lock source rect. (0x%x)" LOG_CR, hRes ));
		return hRes;
	}

	CLOCK_STOP( b, "CTaksiGAPI_DX9::LockRect clock=%d" );

	CLOCK_START(c);
	if (bHalfSize)
		hRes = GetFrameHalfSize( lockedSrcRect, frame );
	else
		hRes = GetFrameFullSize( lockedSrcRect, frame );
	CLOCK_STOP(c, "CTaksiGAPI_DX9::GetFrame* clock=%d" );

	if ( pSurfTemp )
	{
		 pSurfTemp->UnlockRect();
	}
	return hRes;
}

//*********************************************************************************

HRESULT CTaksiGAPI_DX9::DrawIndicator( TAKSI_INDICATE_TYPE eIndicate )
{
	ASSERT(m_pDevice);
	ASSERT( eIndicate >= 0 && eIndicate < COUNTOF(s_pVB) );
	if (s_pVB[eIndicate] == NULL) 
	{
		HRESULT hRes = RestoreDeviceObjects();
		if (FAILED(hRes))
		{
			LOG_WARN(( "RestoreDeviceObjects() FAILED 0x%x." LOG_CR, hRes ));
			return hRes;
		}
		DEBUG_TRACE(( "RestoreDeviceObjects() done." LOG_CR));
	}

	// setup renderstate
	HRESULT hRes = s_pSavedState_Them->Capture();
	if ( FAILED(hRes))
	{
		return hRes;
	}

	hRes = s_pSavedState_Me->Apply();

	// render
	hRes = m_pDevice->BeginScene();
	if ( SUCCEEDED(hRes))
	{
		m_pDevice->SetStreamSource( 0, s_pVB[eIndicate], 0, sizeof(CUSTOMVERTEX));
		m_pDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

		m_pDevice->SetStreamSource( 0, s_pVBborder, 0, sizeof(CUSTOMVERTEX));
		m_pDevice->DrawPrimitive( D3DPT_LINESTRIP, 0, 4 );

		m_pDevice->EndScene();
	}

	// restore the modified renderstates
	s_pSavedState_Them->Apply();
	return hRes;
}

//****************************************************

EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE DX9_AddRef(IDirect3DDevice9* pDevice)
{
	// New AddRef function 
	g_DX9.m_iRefCount = s_D3D9_AddRef(pDevice);
	//DEBUG_TRACE(("DX9_AddRef: called (m_iRefCount = %d)." LOG_CR, g_DX9.m_iRefCount));
	return g_DX9.m_iRefCount;
}

EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE DX9_Release(IDirect3DDevice9* pDevice)
{
	// New Release function
	// a "fall-through" case
	if ((g_DX9.m_iRefCount > g_DX9.m_iRefCountMe + 1) && s_D3D9_Release) 
	{
		g_DX9.m_iRefCount = s_D3D9_Release(pDevice);
		// DEBUG_TRACE(("DX9_Release: called (m_iRefCount = %d)." LOG_CR, g_DX9.m_iRefCount));
		return g_DX9.m_iRefCount;
	}

	DEBUG_TRACE(("+++++++++++++++++++++++++++++++++++++" LOG_CR ));
	DEBUG_MSG(("DX9_Release: called." LOG_CR));
	DEBUG_TRACE(("DX9_Release: pDevice = %08x" LOG_CR, (UINT_PTR)pDevice));
	DEBUG_TRACE(("DX9_Release: VTABLE[0] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[0]));
	DEBUG_TRACE(("DX9_Release: VTABLE[1] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[1]));
	DEBUG_TRACE(("DX9_Release: VTABLE[2] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[2]));

	g_DX9.m_pDevice = pDevice;

	// if in recording mode, close the AVI file,
	g_DX9.RecordAVI_Reset();
	g_DX9.UnhookFunctions();	// unhook device methods

	// reset the pointers
	g_DX9.m_Hook_AddRef = NULL;
	g_DX9.m_Hook_Release = NULL;

	// call the real Release()
	DEBUG_MSG(( "DX9_Release: about to call real Release." LOG_CR));

	g_DX9.m_iRefCount = s_D3D9_Release(pDevice);

	DEBUG_MSG(( "DX9_Release: UNHOOK m_iRefCount = %d" LOG_CR, g_DX9.m_iRefCount));

	return g_DX9.m_iRefCount;
}

static void DX9_HooksInit( IDirect3DDevice9* pDevice )
{
	UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)pDevice));
	ASSERT(pVTable);
	g_DX9.m_Hook_AddRef = pVTable + 1;
	g_DX9.m_Hook_Release = pVTable + 2;

	DEBUG_TRACE(("*m_Hook_AddRef  = %08x" LOG_CR, *g_DX9.m_Hook_AddRef));
	DEBUG_TRACE(("*m_Hook_Release  = %08x" LOG_CR, *g_DX9.m_Hook_Release));

	// hook AddRef method
	s_D3D9_AddRef = (PFN_DX9_ADDREF)(*g_DX9.m_Hook_AddRef);
	*g_DX9.m_Hook_AddRef = (UINT_PTR)DX9_AddRef;

	// hook Release method
	s_D3D9_Release = (PFN_DX9_RELEASE)(*g_DX9.m_Hook_Release);
	*g_DX9.m_Hook_Release = (UINT_PTR)DX9_Release;
}

static void DX9_HooksVerify( IDirect3DDevice9* pDevice )
{
	// It looks like at certain points, vtable entries get restored to its original values.
	// If that happens, we need to re-assign them to our functions again.
	// NOTE: we don't want blindly re-assign, because there can be other programs
	// hooking on the same methods. Therefore, we only re-assign if we see that
	// original addresses are restored by the system.

	if ( ! g_DX9.m_bHookedFunctions )	// no intent to hook.
		return;

	UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)pDevice));
	ASSERT(pVTable);
	if (pVTable[TAKSI_INTF_AddRef] == (UINT_PTR)s_D3D9_AddRef)
	{
		pVTable[TAKSI_INTF_AddRef] = (UINT_PTR)DX9_AddRef;
		DEBUG_MSG(( "DX9_HooksVerify: pDevice->AddRef() re-hooked." LOG_CR));
	}
	if (pVTable[TAKSI_INTF_Release] == (UINT_PTR)s_D3D9_Release)
	{
		pVTable[TAKSI_INTF_Release] = (UINT_PTR)DX9_Release;
		DEBUG_MSG(( "DX9_HooksVerify: pDevice->Release() re-hooked." LOG_CR));
	}
}

EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE DX9_Reset(
	IDirect3DDevice9* pDevice, LPVOID params )
{
	// New Reset function
	// m_nDX9_Reset
	// put back saved code fragment
	g_DX9.m_Hook_Reset.SwapOld(s_D3D9_Reset);

	LOG_MSG(( "DX9_Reset: called." LOG_CR));

	g_DX9.m_pDevice = pDevice;

	g_DX9.RecordAVI_Reset();
	g_DX9.InvalidateDeviceObjects();

	//call real Reset() 
	HRESULT hRes = s_D3D9_Reset(pDevice, params);

	DX9_HooksVerify(pDevice);

	DEBUG_MSG(( "DX9_Reset: done." LOG_CR));

	// put JMP instruction again
	g_DX9.m_Hook_Reset.SwapReset(s_D3D9_Reset);
	return hRes;
}

EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE DX9_Present(
	IDirect3DDevice9* pDevice, const RECT* src, const RECT* dest, HWND hWnd, LPVOID unused)
{
	// New Present function 
	// m_nDX9_Present
	g_DX9.m_Hook_Present.SwapOld(s_D3D9_Present);

	DEBUG_TRACE(( "--------------------------------" LOG_CR ));
	DEBUG_TRACE(( "DX9_Present: called." LOG_CR ));

	g_DX9.m_pDevice = pDevice;

	// if swap chain present is hooked it needs removed since device present will
	// eventually call swap chain present.
	if (g_DX9.m_Hook_SCPresent.IsHookInstalled())
	{
		g_DX9.m_Hook_SCPresent.RemoveHook(s_D3D9_SCPresent);
	}

	// rememeber IDirect3DDevice9::Release pointer so that we can clean-up properly.
	if (g_DX9.m_Hook_AddRef == NULL || g_DX9.m_Hook_Release == NULL)
	{
		DX9_HooksInit(pDevice);
	}

	g_DX9.PresentFrameBegin(true);

	// call real Present() 
	HRESULT hRes = s_D3D9_Present(pDevice, src, dest, hWnd, unused);

	g_DX9.PresentFrameEnd();

	DX9_HooksVerify(pDevice);
	DEBUG_TRACE(( "DX9_Present: done." LOG_CR ));

	g_DX9.m_Hook_Present.SwapReset(s_D3D9_Present);
	return hRes;
}

EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE DX9_SCPresent(
	IDirect3DSwapChain9* pSwapChain, const RECT* src, const RECT* dest, HWND hWnd, LPVOID unused, DWORD dwFlags)
{
	// New IDirect3DSwapChain9::Present function 
	// m_nDX9_SCPresent
	IRefPtr<IDirect3DDevice9> pDevice;
	if ( FAILED( pSwapChain->GetDevice( IREF_GETPPTR(pDevice,IDirect3DDevice9) ) ) )
	{
		return s_D3D9_SCPresent(pSwapChain, src, dest, hWnd, unused, dwFlags);
	}

	g_DX9.m_Hook_SCPresent.SwapOld(s_D3D9_SCPresent);

	DEBUG_TRACE(( "--------------------------------" LOG_CR ));
	DEBUG_TRACE(( "DX9_SCPresent: called (%08x)." LOG_CR, dwFlags ));

	g_DX9.m_pDevice = pDevice;

	// rememeber IDirect3DDevice9::Release pointer so that we can clean-up properly.
	if (g_DX9.m_Hook_AddRef == NULL || g_DX9.m_Hook_Release == NULL)
	{
		DX9_HooksInit(pDevice);
	}

	g_DX9.PresentFrameBegin(true);

	// call real IDirect3DSwapChain9::Present() 
	HRESULT hRes = s_D3D9_SCPresent(pSwapChain, src, dest, hWnd, unused, dwFlags);

	g_DX9.PresentFrameEnd();

	DX9_HooksVerify(pDevice);
	DEBUG_TRACE(( "DX9_SCPresent: done." LOG_CR ));

	g_DX9.m_Hook_SCPresent.SwapReset(s_D3D9_SCPresent);
	return hRes;
}

//************************************************************************

CTaksiGAPI_DX9::CTaksiGAPI_DX9() 
	: m_pDevice( NULL )
	, m_iRefCount( 1 )
	, m_iRefCountMe( 0 )
	, m_Hook_AddRef(NULL)
	, m_Hook_Release(NULL)
{
}
CTaksiGAPI_DX9::~CTaksiGAPI_DX9()
{
}

HRESULT CTaksiGAPI_DX9::HookFunctions()
{
	// ONLY CALLED FROM AttachGAPI()
	// This function hooks two IDirect3DDevice9 methods, using code overwriting technique. 
	// hook IDirect3DDevice9::Present(), using code modifications at run-time.
	// ALGORITHM: we overwrite the beginning of real IDirect3DDevice9::Present
	// with a JMP instruction to our routine (DX9_Present).
	// When our routine gets called, first thing that it does - it restores 
	// the original bytes of IDirect3DDevice9::Present, then performs its pre-call tasks, 
	// then calls IDirect3DDevice9::Present, then does post-call tasks, then writes
	// the JMP instruction back into the beginning of IDirect3DDevice9::Present, and
	// returns.

	ASSERT( IsValidDll());
	if ( m_bHookedFunctions )
	{
		return S_FALSE;
	}
	if (!sg_Shared.m_nDX9_Present || !sg_Shared.m_nDX9_Reset)
	{
		LOG_WARN(( "CTaksiGAPI_DX9::HookFunctions: No info on 'Present' and/or 'Reset'." LOG_CR));
		return HRESULT_FROM_WIN32(ERROR_INVALID_HOOK_HANDLE);
	}
	
	s_D3D9_Present = (PFN_DX9_PRESENT)(get_DllInt() + sg_Shared.m_nDX9_Present);
	s_D3D9_Reset = (PFN_DX9_RESET)(get_DllInt() + sg_Shared.m_nDX9_Reset);

	DEBUG_MSG(( "CTaksiGAPI_DX9::HookFunctions: checking JMP-implants..." LOG_CR));

	if ( ! m_Hook_Present.InstallHook(s_D3D9_Present,DX9_Present))
	{
		LOG_WARN(( "CTaksiGAPI_DX9::HookFunctions: FAILED to InstallHook for IDirect3DDevice9::Present!" LOG_CR));
		return HRESULT_FROM_WIN32(ERROR_INVALID_HOOK_HANDLE);
	}
	m_Hook_Reset.InstallHook(s_D3D9_Reset,DX9_Reset);

	if (sg_Shared.m_nDX9_SCPresent)
	{
		// Also hook IDirect3DSwapChain9::Present in case the app calls it directly
		s_D3D9_SCPresent = (PFN_DX9_SCPRESENT)(get_DllInt() + sg_Shared.m_nDX9_SCPresent);
		if ( ! m_Hook_SCPresent.InstallHook(s_D3D9_SCPresent,DX9_SCPresent))
		{
			LOG_WARN(( "CTaksiGAPI_DX9::HookFunctions: FAILED to InstallHook for IDirect3DSwapChain9::Present!" LOG_CR));
		}
	}

	return __super::HookFunctions();
}

void CTaksiGAPI_DX9::UnhookFunctions()
{
	// Restore original Reset() and Present()
	
	if ( ! IsValidDll())
		return;

	if (m_Hook_AddRef != NULL && s_D3D9_AddRef != NULL)
	{
		*m_Hook_AddRef = (UINT_PTR)s_D3D9_AddRef;
	}
	if (m_Hook_Release != NULL && s_D3D9_Release != NULL)
	{
		*m_Hook_Release = (UINT_PTR)s_D3D9_Release;
	}

	// restore IDirect3D9Device methods
	m_Hook_SCPresent.RemoveHook(s_D3D9_SCPresent);
	m_Hook_Present.RemoveHook(s_D3D9_Present);
	m_Hook_Reset.RemoveHook(s_D3D9_Reset);

	InvalidateDeviceObjects();

	__super::UnhookFunctions();
}

void CTaksiGAPI_DX9::FreeDll()
{
	// This function is called by the main DLL thread, when
	// the DLL is about to detach. All the memory clean-ups should be done here.
	// invalidate all our D3D objects 
	if ( ! IsValidDll())
		return;

	// restore hooked functions if DLL is still loaded
	if ( ::GetModuleHandle(get_DLLName()))
		UnhookFunctions();

	DEBUG_MSG(( "CTaksiGAPI_DX9::FreeDll: done." LOG_CR));
	__super::FreeDll();
}

#endif
