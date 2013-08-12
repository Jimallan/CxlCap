//
// Test_DX9.cpp
// must be in seperate file from DX8
//
#include "../stdafx.h"
#include "Taksi.h"

#ifdef USE_DIRECTX9
#include <d3d9types.h>
#include <d3d9.h>

typedef IDirect3D9* (STDMETHODCALLTYPE *DIRECT3DCREATE9)(UINT);

bool Test_DirectX9( HWND hWnd )
{
	// get the offset from the start of the DLL to the interface element we want.
	// step 1: Load d3d9.dll
	CDllFile _DX9;
	HRESULT hRes = _DX9.LoadDll( TEXT("d3d9"));
	if (IS_ERROR(hRes))
	{
		LOG_MSG(("Test_DirectX9 Failed to load d3d9.dll (0x%x)" LOG_CR, hRes ));
		return false;
	}

	// step 2: Get IDirect3D9
	DIRECT3DCREATE9 pDirect3DCreate9 = (DIRECT3DCREATE9)_DX9.GetProcAddress("Direct3DCreate9");
	if (pDirect3DCreate9 == NULL) 
	{
		LOG_MSG(("Test_DirectX9 Unable to find Direct3DCreate9" LOG_CR));
		return false;
	}
	IRefPtr<IDirect3D9> pD3D = pDirect3DCreate9(D3D_SDK_VERSION);
	if ( !pD3D.IsValidRefObj())
	{
		LOG_MSG(("Test_DirectX9 Direct3DCreate9(%d) call FAILED" LOG_CR, D3D_SDK_VERSION ));
		return false;
	}

	// step 3: Get IDirect3DDevice9
    D3DDISPLAYMODE d3ddm;
	hRes = pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm );
    if (FAILED(hRes))
	{
		LOG_MSG(("Test_DirectX9 GetAdapterDisplayMode failed. 0x%x" LOG_CR, hRes));
        return false;
	}

    D3DPRESENT_PARAMETERS d3dpp; 
    ZeroMemory( &d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = true;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

	IRefPtr<IDirect3DDevice9> pD3DDevice;
	hRes = pD3D->CreateDevice( 
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,	// the device we suppose any app would be using.
		hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
		&d3dpp, IREF_GETPPTR(pD3DDevice,IDirect3DDevice9));
    if (FAILED(hRes))
    {
		LOG_MSG(("Test_DirectX9 CreateDevice failed. 0x%x" LOG_CR, hRes ));
        return false;
    }

	// step 4: store method addresses in out vars
	UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)pD3DDevice.get_RefObj()));
	ASSERT(pVTable);
	sg_Shared.m_nDX9_Present = ( pVTable[TAKSI_INTF_DX9_Present] - _DX9.get_DllInt());
	sg_Shared.m_nDX9_Reset = ( pVTable[TAKSI_INTF_DX9_Reset] - _DX9.get_DllInt());

	LOG_MSG(("Test_DirectX9: %08x, Present=0%x, Reset=0%x " LOG_CR,
		(UINT_PTR)pD3DDevice.get_RefObj(),
		sg_Shared.m_nDX9_Present, sg_Shared.m_nDX9_Reset ));

	IRefPtr<IDirect3DSwapChain9> pD3DSwapChain;
	hRes = pD3DDevice->GetSwapChain( 0, IREF_GETPPTR(pD3DSwapChain,IDirect3DSwapChain9) );
    if (FAILED(hRes))
    {
		LOG_MSG(("Test_DirectX9 GetSwapChain failed. 0x%x" LOG_CR, hRes ));
    }
	else
	{
		pVTable = (UINT_PTR*)(*((UINT_PTR*)pD3DSwapChain.get_RefObj()));
		ASSERT(pVTable);
		sg_Shared.m_nDX9_SCPresent = ( pVTable[TAKSI_INTF_DX9_SCPresent] - _DX9.get_DllInt());
		LOG_MSG(("Test_DirectX9: %08x, GetSwapChain=0%x" LOG_CR,
			(UINT_PTR)pD3DSwapChain.get_RefObj(),
			sg_Shared.m_nDX9_SCPresent ));
	}

	return true;
}

#endif // USE_DIRECTX9
