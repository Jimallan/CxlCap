//
// CFileDirDlg.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "stdafx.h"
#include "CFileDirDlg.h"
#include "IRefPtr.h"
#include "CLogBase.h"
#include <tchar.h>
#include <shlobj.h>		// The documentation says this nowhere but this is the header file for SHGetPathFromIDList

CFileDirDlg::CFileDirDlg( HWND hWndOwner, const TCHAR* pszDir ) :
	m_hWndOwner(hWndOwner)
{
	if ( pszDir )
		lstrcpyn( m_szDir, pszDir, sizeof(m_szDir)-1 );
	else
		m_szDir[0] = '\0';
}

CFileDirDlg::~CFileDirDlg()
{
}

int CALLBACK CFileDirDlg::BrowseCallbackProc( HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData )	// static
{
	//Dialog box event that generated the message. One of the following values:
	CFileDirDlg* pThis = (CFileDirDlg*) lpData;
	ASSERT(pThis);
	switch(uMsg)
	{
	case BFFM_INITIALIZED:
		// The dialog box has finished initializing.
		pThis->m_hWnd = hWnd;
		if ( pThis->m_szDir[0] != '\0' )
		{
			// BFFM_SETSELECTION is converted to A or W
			::SendMessage(hWnd,BFFM_SETSELECTION,(WPARAM)true,(LPARAM) pThis->m_szDir );
		}
		break;
	case BFFM_IUNKNOWN:
		// An IUnknown interface is available to the dialog box.
		{
			// IRefPtr<IUnknown> pIUnk = (IUnknown*)(lParam); // LPARAM parameter contains a pointer to an instance of IUnknown
		}
		break;
	case BFFM_SELCHANGED:
		// The selection has changed in the dialog box.
		break;
	case BFFM_VALIDATEFAILED:
		// Version 4.71. The user typed an invalid name into the dialog's edit box. A nonexistent folder is considered an invalid name.
		break;
	}
	//DEBUG_MSG(( "CFileDirDlg::BrowseCallbackProc 0%x" LOG_CR, uMsg ));
	return 0;
}

HRESULT CFileDirDlg::DoModal( const TCHAR* pszPrompt )
{
	// NOTE: BIF_NEWDIALOGSTYLE must have called OleInitialize
	IRefPtr<IMalloc> pMalloc;	// COM object.
	HRESULT hRes = SHGetMalloc( IREF_GETPPTR(pMalloc,IMalloc));
	if ( hRes )		// failed ?
		return hRes;

	// buffer - a place to hold the file system pathname
	_TCHAR szBuffer[MAX_PATH+1];
	lstrcpy( szBuffer, m_szDir );	// NOTE: Setting this does nothing ?!!!

	// This struct holds the various options for the dialog
	BROWSEINFO bi;		// BROWSEINFOA
 	bi.hwndOwner = m_hWndOwner;
	bi.pidlRoot = NULL;					// The root is the true current root of the system by default.
	bi.pszDisplayName = szBuffer;		// Return display name of item selected.
	bi.lpszTitle = pszPrompt;			// text to go in the banner over the tree.
	bi.ulFlags = BIF_DONTGOBELOWDOMAIN | 
		BIF_RETURNONLYFSDIRS | 
		BIF_EDITBOX | 
		BIF_VALIDATE | 
		BIF_NEWDIALOGSTYLE; // Flags that control the return stuff
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM) this;		// extra info that's passed back in callbacks
    bi.iImage = 0;					// output var: where to return the Image index.

	// Now cause the dialog to appear. Modal
	m_hWnd = NULL;
	LPITEMIDLIST pidlRoot = SHBrowseForFolder(&bi);
	m_hWnd = NULL;	// now closed.
	if ( pidlRoot== NULL)
	{
		// User hit cancel i assume
		return E_ABORT;
	}

	// get a ASCII pathname from the LPITEMIDLIST struct.
	if ( ! SHGetPathFromIDList( pidlRoot, szBuffer ))
	{
		// *m_szDir = '\0';
		// return E_FAIL;
	}
	else
	{
		// Do something with the converted string.
		lstrcpyn( m_szDir, szBuffer, sizeof(m_szDir)-1 );
	}

	// Free the returned item identifier list using the shell's allocator!
	pMalloc->Free(pidlRoot);
	return S_OK;
}
