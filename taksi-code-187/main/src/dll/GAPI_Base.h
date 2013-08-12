//
// GAPI_Base.h
//
#pragma once
#include "../common/Common.h"

// indicator dimensions/position
#define INDICATOR_X 8
#define INDICATOR_Y 8
#define INDICATOR_Width 16
#define INDICATOR_Height 16

struct CTaksiGAPIBase : public CDllFile
{
	// a generic graphics mode base class.
	// Base for: CTaksiGAPI_DX8, CTaksiGAPI_DX9, CTaksiGAPI_GDI etc,
public:
	CTaksiGAPIBase()
		: m_bHookedFunctions(false)
		, m_bGotFrameInfo(false)
	{
	}

	virtual const TCHAR* get_DLLName() const = 0;
	virtual TAKSI_GAPI_TYPE get_GAPIType() const = 0;

	HRESULT AttachGAPI();
	virtual void FreeDll()
	{
		__super::FreeDll();
	}

	void PresentFrameBegin( bool bDrawIndicator );
	void PresentFrameEnd();
	void RecordAVI_Reset();

protected:
	virtual HRESULT HookFunctions()
	{
		// ONLY CALLED FROM AttachGAPI()
		ASSERT( IsValidDll());
		if ( m_bHookedFunctions )
		{
			return S_FALSE;
		}
		DEBUG_MSG(( "CTaksiGAPIBase::HookFunctions done." LOG_CR ));
		m_bHookedFunctions = true;
		return S_OK;
	}
	virtual void UnhookFunctions()
	{
		DEBUG_MSG(( "CTaksiGAPIBase::UnhookFunctions" LOG_CR ));
		m_bHookedFunctions = false;
	}

	virtual HWND GetFrameInfo( SIZE& rSize ) = 0;
	virtual HRESULT GetFrame( CVideoFrame& frame, bool bHalfSize ) = 0;
	virtual HRESULT DrawIndicator( TAKSI_INDICATE_TYPE eIndicate ) = 0;

private:

	static HRESULT MakeScreenShotGDIP( TCHAR* pszFileName, CVideoFrame& frame );

	HRESULT RecordAVI_Start();
	void RecordAVI_Stop();
	bool RecordAVI_Frame();

	HRESULT MakeScreenShot( bool bHalfSize );

	void ProcessHotKey( TAKSI_HOTKEY_TYPE eHotKey );

public:
	static const COLORREF sm_IndColors[TAKSI_INDICATE_QTY];
	bool m_bHookedFunctions;	// HookFunctions() called and returned true. may not have called PresentFrameBegin() yet tho.
	bool m_bGotFrameInfo;		// GetFrameInfo() called. set to false to re-read the info. PresentFrameBegin success.
};

#ifdef USE_DIRECTX8

interface IDirect3DDevice8;
struct CTaksiGAPI_DX8 : public CTaksiGAPIBase
{
	// TAKSI_GAPI_DX8
public:
	CTaksiGAPI_DX8();
	~CTaksiGAPI_DX8();

	virtual const TCHAR* get_DLLName() const
	{
		return TEXT("d3d8.dll");
	}
	virtual TAKSI_GAPI_TYPE get_GAPIType() const
	{
		return TAKSI_GAPI_DX8;
	}

	virtual HRESULT HookFunctions();
	virtual void UnhookFunctions();
	virtual void FreeDll();

	virtual HWND GetFrameInfo( SIZE& rSize );
	virtual HRESULT DrawIndicator( TAKSI_INDICATE_TYPE eIndicate );
	virtual HRESULT GetFrame( CVideoFrame& frame, bool bHalfSize );

	HRESULT RestoreDeviceObjects();
	HRESULT InvalidateDeviceObjects(bool bDetaching);

public:
	IDirect3DDevice8* m_pDevice;	// use this carefully, it is not IRefPtr locked
	ULONG m_iRefCount;	// Actual RefCounts i see by hooking the AddRef()/Release() methods.
	ULONG m_iRefCountMe;	// RefCounts that i know i have made.

	CHookJump m_Hook_Present;
	CHookJump m_Hook_Reset;
	UINT_PTR* m_Hook_AddRef;
	UINT_PTR* m_Hook_Release;
};
extern CTaksiGAPI_DX8 g_DX8;

#endif // USE_DIRECTX8

#ifdef USE_DIRECTX9

interface IDirect3DDevice9;
struct CTaksiGAPI_DX9 : public CTaksiGAPIBase
{
	// TAKSI_GAPI_DX9
public:
	CTaksiGAPI_DX9();
	~CTaksiGAPI_DX9();

	virtual const TCHAR* get_DLLName() const
	{
		return TEXT("d3d9.dll");
	}
	virtual TAKSI_GAPI_TYPE get_GAPIType() const
	{
		return TAKSI_GAPI_DX9;
	}

	virtual HRESULT HookFunctions();
	virtual void UnhookFunctions();
	virtual void FreeDll();

	virtual HWND GetFrameInfo( SIZE& rSize );
	virtual HRESULT DrawIndicator( TAKSI_INDICATE_TYPE eIndicate );
	virtual HRESULT GetFrame( CVideoFrame& frame, bool bHalfSize );

	HRESULT RestoreDeviceObjects();
	HRESULT InvalidateDeviceObjects();

public:
	IDirect3DDevice9* m_pDevice;	// use this carefully, it is not IRefPtr locked
	ULONG m_iRefCount;	// Actual RefCounts i see by hooking the AddRef()/Release() methods.
	ULONG m_iRefCountMe;	// RefCounts that i know i have made.

	CHookJump m_Hook_Present;
	CHookJump m_Hook_SCPresent;
	CHookJump m_Hook_Reset;
	UINT_PTR* m_Hook_AddRef;
	UINT_PTR* m_Hook_Release;
};
extern CTaksiGAPI_DX9 g_DX9;

#endif // USE_DIRECTX9

struct CTaksiGAPI_OGL : public CTaksiGAPIBase
{
	// TAKSI_GAPI_OGL
public:
	CTaksiGAPI_OGL() 
		: m_bTestTextureCaps(false)
	{
	}

	virtual const TCHAR* get_DLLName() const
	{
		return TEXT("opengl32.dll");
	}
	virtual TAKSI_GAPI_TYPE get_GAPIType() const
	{
		return TAKSI_GAPI_OGL;
	}

	virtual HRESULT HookFunctions();
	virtual void UnhookFunctions();
	virtual void FreeDll();

	virtual HWND GetFrameInfo( SIZE& rSize );
	virtual HRESULT DrawIndicator( TAKSI_INDICATE_TYPE eIndicate );
	virtual HRESULT GetFrame( CVideoFrame& frame, bool bHalfSize );

private:
	void GetFrameFullSize(CVideoFrame& frame);
	void GetFrameHalfSize(CVideoFrame& frame);

public:
	bool m_bTestTextureCaps;
	CHookJump m_Hook;
	CHookJump m_Hook_Delete;
	CVideoFrame m_SurfTemp;	// temporary full size frame if we are halfing the image.
};
extern CTaksiGAPI_OGL g_OGL;

struct CTaksiGAPI_GDI : public CTaksiGAPIBase
{
	// TAKSI_GAPI_DESKTOP HWND_DESKTOP
	// TAKSI_GAPI_GDI
public:
	CTaksiGAPI_GDI()
		: m_hWnd(HWND_DESKTOP)
		, m_WndProcOld(NULL)
		, m_iReentrant(0)
	{
	}
	
	static bool IsDesktop();
	virtual const TCHAR* get_DLLName() const
	{
		return TEXT("user32.dll");
	}
	virtual TAKSI_GAPI_TYPE get_GAPIType() const
	{
		if ( IsDesktop())
			return TAKSI_GAPI_DESKTOP;
		return TAKSI_GAPI_GDI;
	}

	virtual HRESULT HookFunctions();
	virtual void UnhookFunctions();
	virtual void FreeDll();

	virtual HWND GetFrameInfo( SIZE& rSize );
	virtual HRESULT DrawIndicator( TAKSI_INDICATE_TYPE eIndicate );
	virtual HRESULT GetFrame( CVideoFrame& frame, bool bHalfSize );

	LRESULT OnTick( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
	void DrawMouse( HDC hMemDC, bool bHalfSize );
	static LRESULT CALLBACK WndProcHook( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

public:
	HWND m_hWnd;	// best window for the current process.
	RECT m_WndRect;	// system rectangle for the window.
	WNDPROC m_WndProcOld;	// the old handler before i hooked it.
	int m_iReentrant;
	TIMESYS_t m_dwTimeLastDrawIndicator;	// GetTickCount() of last.
	// TIMESYS_t m_dwTimeLastPaint;
	UINT_PTR m_uTimerId;	// did we set up the timer ?
};
extern CTaksiGAPI_GDI g_GDI;

inline void SwapColorsRGB( BYTE* pPixel )
{
	//ASSUME pixel format is in 8bpp mode.
	BYTE r = pPixel[0];
	pPixel[0] = pPixel[2];
	pPixel[2] = r;
}
