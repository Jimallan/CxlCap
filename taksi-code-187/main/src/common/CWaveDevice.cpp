//
// CWaveDevice.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//

#include "stdafx.h"
#ifdef _WIN32
#include <windowsx.h>
#include "CWaveDevice.h"
#include "CWaveFormat.h"
#include "CLogBase.h"

//***************************************************************
// -CWaveHeaderBase

CWaveHeaderBase::CWaveHeaderBase( LPSTR lpData, DWORD dwBufferLength ) : 
	m_pwh(NULL),
	m_bManageDataBuffer(false)
{
	// NOTE: dwBufferLength should be a multiple of the current format get_BlockSize() !
	if (dwBufferLength)
	{
		AttachData( lpData,dwBufferLength);
	}
}

CWaveHeaderBase::~CWaveHeaderBase()
{
	DetachData();
}

void CWaveHeaderBase::UnprepareDevice()
{
	// Make sure the header is unlinked from the device.
	if ( m_pwh == NULL )
		return;
	ASSERT( m_pwh->dwUser == (DWORD_PTR) this );
	ASSERT( ! ( m_pwh->dwFlags & WHDR_INQUEUE ));
	if ( m_pDevice != NULL )
	{
		m_pDevice->UnprepareHeader( m_pwh );
		ASSERT( ! ( m_pwh->dwFlags & WHDR_PREPARED ));
		m_pDevice = NULL;
	}
}

MMRESULT CWaveHeaderBase::PrepareDevice( CWaveDevice* pDevice )
{
	if ( ! IsValid())
	{
		return MMSYSERR_NOMEM;
	}

	ASSERT( ! ( m_pwh->dwFlags & WHDR_INQUEUE ));
	if ( m_pwh->dwFlags & WHDR_PREPARED )
	{
		ASSERT( m_pDevice == pDevice );
		return MMSYSERR_NOERROR;
	}

	// Prepare the buffer header. (if not already preped)
	MMRESULT mmRes = pDevice->PrepareHeader( m_pwh );
	if ( mmRes != MMSYSERR_NOERROR )
	{
		DEBUG_ERR(( "PrepareHeader %d", mmRes ));
		return mmRes;
	}

	ASSERT( m_pwh->dwFlags & WHDR_PREPARED );
	m_pDevice = pDevice;
	return MMSYSERR_NOERROR;
}

void CWaveHeaderBase::DetachData()
{
	if ( m_pwh == NULL )
		return;
	UnprepareDevice();
	if ( m_bManageDataBuffer )
	{
		GlobalFreePtr( m_pwh->lpData );
		m_bManageDataBuffer = false;
	}
	GlobalFreePtr( m_pwh );
	m_pwh = NULL;
}

void CWaveHeaderBase::AttachData( LPSTR lpData, DWORD dwBufferLength )
{
	// A sound buffer to be passed to the wave device.
	ASSERT(dwBufferLength);
	ASSERT(m_pwh==NULL);
	m_pwh = (LPWAVEHDR) GlobalAllocPtr( GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_SHARE, sizeof( WAVEHDR ));
	ASSERT(m_pwh);
	if ( m_pwh == NULL )
	{
		DEBUG_ERR(( "CWaveHdr GlobalAlloc" ));
		return;
	}

	//
	// Set up the control header.
	// Length in bytes.
	//
	if ( lpData == NULL )
	{
		m_bManageDataBuffer = true;
		lpData = (LPSTR) GlobalAllocPtr( GMEM_MOVEABLE | GMEM_SHARE, dwBufferLength );
	}

	m_pwh->lpData = lpData;
	m_pwh->dwBufferLength = dwBufferLength;
	m_pwh->dwUser = (DWORD_PTR) this;
}

//***************************************************************
// -CWaveDevice

CWaveDevice::CWaveDevice( WAVE_DEVICEID_TYPE uDeviceID ) :
	m_uDeviceID(uDeviceID),
	m_hDevice(NULL),
	m_iHeadersInQueue(0),	// The device has possesion of this # of headers. Cannot close til this is 0.
	m_fActive(false)		// Is the device actively playing or recording ?
{
	//StaticConstruct();
}

CWaveDevice::~CWaveDevice()
{
	//StaticDestruct();
	ASSERT(m_hDevice==NULL);	// must already be closed.
}

WAVE_DEVICEID_TYPE CWaveDevice::FindDeviceByProductID( bool bInput, WAVE_PRODUCTID_TYPE uProductID ) // static
{
	//
	// PURPOSE:
	//  Search through the CAPS structures for the Manufacturer and Product ID.
	// RETURN:
	//  the Device ID = found it.
	//  WAVE_MAPPER if not found.
	//

	DEBUG_TRACE(( "Dev:FindProductID(%d,%d)", bInput, uProductID ));

	if ( uProductID == WAVE_DEVICE_NONE )
		return( WAVE_DEVICE_NONE );

	// WAVE_PRODUCTID_TYPE uProductID = WAVE_GET_PID(MM_WAVE_MAPPER,MM_MICROSOFT);
	if ( uProductID == MAKELONG(MM_WAVE_MAPPER,MM_MICROSOFT))	// WAVE_GET_PID
		return( WAVE_MAPPER );

	UINT uQty;
	WAVE_DEVICEID_TYPE uDeviceID = WAVE_MAPPER;

	if ( bInput )
	{
		uQty = ::waveInGetNumDevs();
	}
	else
	{
		uQty = ::waveOutGetNumDevs();
	}

	for ( ; uQty-- ; uDeviceID ++ )
	{
		WAVE_PRODUCTID_TYPE uProductID2;
		if ( bInput )
		{
			WAVEINCAPS Caps;
			if ( ::waveInGetDevCaps( uDeviceID, &Caps, sizeof( WAVEINCAPS )))
				continue;
			uProductID2 = WAVE_GET_PID(Caps);
		}
		else
		{
			WAVEOUTCAPS Caps;
			if ( ::waveOutGetDevCaps( uDeviceID, &Caps, sizeof( WAVEOUTCAPS )))
				continue;
			uProductID2 = WAVE_GET_PID(Caps);
		}
		if ( uProductID2 == uProductID )
			return( uDeviceID );
	}

	return((uQty) ? WAVE_MAPPER : WAVE_DEVICE_NONE );  // Not found.
}

CWaveHeaderBase* CWaveDevice::OnHeaderReturn( LPWAVEHDR pwh ) // static
{
	// A header has been processed by the system.
	// Maybe it was played/recorded to or maybe it was not. (ie. Reset)
	// from MM_WOM_DONE or MM_WIM_DATA message if window callback. (or WOM_DONE,WIM_DATA if function)
	ASSERT(pwh);
	ASSERT( ! ( pwh->dwFlags & WHDR_INQUEUE )); // just came out of the queue of course.
	ASSERT( pwh->dwFlags & WHDR_PREPARED );

	CWaveHeaderBase* pHdr = (CWaveHeaderBase*) ( pwh->dwUser );
	ASSERT( pHdr->IsValidCheck()); 

	//CRefPtr<CWaveDevice> pDev = pHdr->m_pDevice;
	CWaveDevice *pDev = pHdr->m_pDevice;
	ASSERT(pDev);
	if ( pDev )
	{
		// Header is not in queue by the device anymore.
		// Though it is still prepared for the device.
		ASSERT(pDev->m_iHeadersInQueue > 0);
		-- ( pDev->m_iHeadersInQueue );
	}

	return( pHdr );
}

//***************************************************************
// -CWaveRecorder

CWaveRecorder::CWaveRecorder( WAVE_DEVICEID_TYPE uDeviceID ) :
CWaveDevice(uDeviceID)
{
	put_DeviceID(uDeviceID);
}

CWaveRecorder::~CWaveRecorder()
{
	Close();
}

MMRESULT CWaveRecorder::put_DeviceID( WAVE_DEVICEID_TYPE uDeviceID )
{
	Close();
	UINT uQty = ::waveInGetNumDevs();
	if ( uDeviceID >= uQty )
		m_uDeviceID = ( uQty ) ? WAVE_MAPPER : WAVE_DEVICE_NONE;
	else
		m_uDeviceID = uDeviceID;

	ZeroMemory( &m_Caps, sizeof( m_Caps ));
	if ( ! uQty || m_uDeviceID == WAVE_DEVICE_NONE )
	{
		return( MMSYSERR_NOERROR );
	}
	if ( m_uDeviceID == WAVE_MAPPER )
	{
		m_Caps.wMid = MM_MICROSOFT;
		m_Caps.wPid = MM_WAVE_MAPPER;
		return MMSYSERR_NOERROR;
	}

	MMRESULT mmRes = ::waveInGetDevCaps( m_uDeviceID, &m_Caps, sizeof( m_Caps ));
	if ( mmRes != MMSYSERR_NOERROR )
	{
		if ( m_uDeviceID == WAVE_MAPPER )
		{
			m_Caps.wMid = MM_MICROSOFT;
			m_Caps.wPid = MM_WAVE_MAPPER;
			return MMSYSERR_NOERROR;
		}
	}
	return( mmRes );
}

WAVE_DEVICEID_TYPE CWaveRecorder::QueryDeviceID() const
{
	// If we are the WAVE_MAPPER, what device are we actually using ?
	//
	if ( m_hDevice != NULL && m_uDeviceID == WAVE_MAPPER )
	{
		WAVE_DEVICEID_TYPE uDeviceID;
		if ( ::waveInGetID( get_Handle(), &uDeviceID ) == MMSYSERR_NOERROR )
			return( uDeviceID );
	}
	return( m_uDeviceID );
}

void CWaveRecorder::Close()
{
	// Remeber to Reset() and wait for m_iHeadersInQueue to return before Close() !
	if ( m_hDevice )
	{
		Reset(); // return all buffers or the close will just fail.
		ASSERT(m_iHeadersInQueue==0);
		// ASSERT(get_RefCount()==CRefObjBase_STATIC_VAL);
		MMRESULT mRes = ::waveInClose( get_Handle());
		m_hDevice = NULL;
	}
}

MMRESULT CWaveRecorder::Open( const CWaveFormat& format, UINT_PTR dwCallback, UINT_PTR dwInstanceData, DWORD fdwOpen )
{
	// fdwOpen = CALLBACK_WINDOW, WAVE_ALLOWSYNC, WAVE_MAPPED etc.
	Close();
	return ::waveInOpen((LPHWAVEIN) &m_hDevice, m_uDeviceID,
#if(WINVER >= 0x0400)
		(LPWAVEFORMATEX) format.get_WF(),
#else
		(LPWAVEFORMAT) format.get_WF(),
#endif
		dwCallback, dwInstanceData, fdwOpen );
}

MMRESULT CWaveRecorder::GetPosition( MMTIME &Info ) const
{
	Info.wType = TIME_BYTES;             // Get the number of bytes gone by.
	return ::waveInGetPosition( get_Handle(), &Info, sizeof( Info ));
}

MMRESULT CWaveRecorder::SetLowPri()
{
	// Low priority record means it may be stopped by higher pri if they want.
#ifndef WIDM_LOW_PRIORITY
#define WIDM_LOW_PRIORITY 0x4093    // waveInMessage
#endif
	return (MMRESULT) ::waveInMessage( get_Handle(), WIDM_LOW_PRIORITY, 0, 0 );
}

MMRESULT CWaveRecorder::Start()
{
	MMRESULT mmRes = ::waveInStart( get_Handle());
	if ( mmRes == MMSYSERR_NOERROR )
	{
		m_fActive = true;
	}
	return mmRes;
}

MMRESULT CWaveRecorder::Stop()
{
	// just the current header will be returned.
	m_fActive = false;
	return ::waveInStop( get_Handle());
}

MMRESULT CWaveRecorder::Reset()
{
	// All headers will be returned now.
	// Note: this function can hang if the waveIn device/handle is in a bad state
	// or if called during DLL_PROCESS_DETATCH
	m_fActive = false;
	return ::waveInReset( get_Handle());
}

MMRESULT CWaveRecorder::WriteHeader( CWaveHeaderBase* pHdr )
{
	// Write the record data buffer to the device.
	ASSERT(pHdr);
	MMRESULT mmRes = pHdr->PrepareDevice(this);
	if ( mmRes != MMSYSERR_NOERROR )
		return mmRes;

	mmRes = ::waveInAddBuffer( get_Handle(), pHdr->get_wh(), sizeof(WAVEHDR));
	if ( mmRes != MMSYSERR_NOERROR )
		return( mmRes );

	// NOTE: The pHdr will always be returned via callback after this.
	// Even if Reset() is called.
	// ASSERT( pwh->dwFlags & WHDR_INQUEUE ); // WAVE_ALLOWSYNC/WAVECAPS_SYNC device will already be done !?
	m_iHeadersInQueue++;
	return( MMSYSERR_NOERROR );
}

MMRESULT CWaveRecorder::PrepareHeader( LPWAVEHDR pwh ) // virtual
{
	// Protected function.
	ASSERT(pwh);
	if ( pwh->dwFlags & WHDR_PREPARED )
		return MMSYSERR_NOERROR;
	return ::waveInPrepareHeader( get_Handle(), pwh, sizeof( WAVEHDR ));
}

MMRESULT CWaveRecorder::UnprepareHeader( LPWAVEHDR pwh ) // virtual
{
	// Protected function.
	ASSERT(pwh);
	if ( ! ( pwh->dwFlags & WHDR_PREPARED ) || !get_Handle() )
		return( MMSYSERR_NOERROR );
	return ::waveInUnprepareHeader( get_Handle(), pwh, sizeof( WAVEHDR ));
}

//***************************************************************
// -CWavePlayer

CWavePlayer::CWavePlayer( WAVE_DEVICEID_TYPE uDeviceID ) :
CWaveDevice(uDeviceID)
{
	put_DeviceID(uDeviceID);
	m_fActive = true;	// active by default. (if it has beuffers to play)
}

CWavePlayer::~CWavePlayer()
{
	Close();
}

MMRESULT CWavePlayer::put_DeviceID( WAVE_DEVICEID_TYPE uDeviceID )
{
	//@------------------------------------------------------------------------
	// PURPOSE:
	//  Set to use an output device byt its id.
	// RETURN:
	//  0 = OK = MMSYSERR_NOERROR.
	//  else MMSYSTEM error code.
	// NOTE:
	//  Caps is not supported by the WAVE_MAPPER !!!
	//@------------------------------------------------------------------------

	Close();

	UINT uQty = ::waveOutGetNumDevs();
	if ( uDeviceID >= uQty )
		m_uDeviceID = ( uQty ) ? WAVE_MAPPER : WAVE_DEVICE_NONE;    // Out of range must be the mapper.
	else
		m_uDeviceID = uDeviceID;

	ZeroMemory( &m_Caps, sizeof( m_Caps ));
	if ( ! uQty || m_uDeviceID == WAVE_DEVICE_NONE )
	{
		return( MMSYSERR_NOERROR );
	}

	MMRESULT mmRes = ::waveOutGetDevCaps( m_uDeviceID, &m_Caps, sizeof( m_Caps ));
	if ( mmRes != MMSYSERR_NOERROR )
	{
		if ( m_uDeviceID == WAVE_MAPPER )
		{
			m_Caps.wMid = MM_MICROSOFT;
			m_Caps.wPid = MM_WAVE_MAPPER;
			return MMSYSERR_NOERROR;
		}
	}
	return( mmRes );
}

WAVE_DEVICEID_TYPE CWavePlayer::QueryDeviceID() const
{
	// If we are the WAVE_MAPPER, what device are we actually using ?
	//
	if ( m_hDevice != NULL && m_uDeviceID == WAVE_MAPPER )
	{
		WAVE_DEVICEID_TYPE uDeviceID;
		if ( ::waveOutGetID( get_Handle(), &uDeviceID ) == MMSYSERR_NOERROR )
			return( uDeviceID );
	}
	return( m_uDeviceID );
}

void CWavePlayer::Close()
{
	// Remeber to Reset() and wait for m_iHeadersInQueue to return before Close() !
	if ( m_hDevice )
	{
		Reset();
		ASSERT(m_iHeadersInQueue==0);
		// ASSERT(get_RefCount()==CRefObjBase_STATIC_VAL);
		::waveOutClose( get_Handle());
		m_hDevice = NULL;
	}
}

MMRESULT CWavePlayer::Open( const CWaveFormat& format, UINT_PTR dwCallback, UINT_PTR dwInstanceData, DWORD fdwOpen )
{
	// fdwOpen = CALLBACK_WINDOW, WAVE_ALLOWSYNC etc.
	Close();	// kill any previous open first.
	MMRESULT mmRes = ::waveOutOpen((LPHWAVEOUT) &m_hDevice, m_uDeviceID,
#if(WINVER >= 0x0400)
		(LPWAVEFORMATEX) format.get_WF(),
#else
		(LPWAVEFORMAT) format.get_WF(),
#endif
		dwCallback, dwInstanceData, fdwOpen );
	if ( mmRes == MMSYSERR_NOERROR )
	{
		// Players are active by default.
		m_fActive = true;
	}
	return( mmRes );
}

MMRESULT CWavePlayer::GetPosition( MMTIME &Info ) const
{
	Info.wType = TIME_BYTES;             // Get the number of bytes gone by.
	return ::waveOutGetPosition( get_Handle(), &Info, sizeof( Info ));
}

MMRESULT CWavePlayer::Start()
{
	// Player is started by default. Only do this after a Stop().
	MMRESULT mmRes = ::waveOutRestart( get_Handle());
	if ( mmRes == MMSYSERR_NOERROR )
	{
		m_fActive = true;
	}
	return mmRes;
}

MMRESULT CWavePlayer::Stop()
{
	// just the current header will be returned.
	m_fActive = false;
	return ::waveOutPause( get_Handle());
}

MMRESULT CWavePlayer::Reset()
{
	// Stop and Return all the outstanding headers.
	m_fActive = false;
	return ::waveOutReset( get_Handle());
}

MMRESULT CWavePlayer::WriteHeader( CWaveHeaderBase* pHdr )
{
	// Write the play data to the device.
	ASSERT(pHdr);
	MMRESULT mmRes = pHdr->PrepareDevice( this );
	if ( mmRes != MMSYSERR_NOERROR )
		return( mmRes );

	// Write the play data to the device.
	mmRes = ::waveOutWrite( get_Handle(), pHdr->get_wh(), sizeof( WAVEHDR ));
	if ( mmRes != MMSYSERR_NOERROR )
		return( mmRes );

	// NOTE: The pHdr will always be returned via callback after this.
	// Even if Reset() is called.
	// ASSERT(( m_Caps.dwSupport & WAVECAPS_SYNC ) || ( pwh->dwFlags & WHDR_INQUEUE )); // WAVE_ALLOWSYNC/WAVECAPS_SYNC device will already be done !?

	m_iHeadersInQueue++;
	return( MMSYSERR_NOERROR );
}

MMRESULT CWavePlayer::PrepareHeader( LPWAVEHDR pwh ) // virtual
{
	// Protected function.
	ASSERT(pwh);
	if ( pwh->dwFlags & WHDR_PREPARED )
		return MMSYSERR_NOERROR;
	return ::waveOutPrepareHeader( get_Handle(), pwh, sizeof( WAVEHDR ));
}

MMRESULT CWavePlayer::UnprepareHeader( LPWAVEHDR pwh ) // virtual
{
	// Protected function.
	ASSERT(pwh);
	if ( ! ( pwh->dwFlags & WHDR_PREPARED ))
		return( MMSYSERR_NOERROR );
	return ::waveOutUnprepareHeader( get_Handle(), pwh, sizeof( WAVEHDR ));
}

//******************************************************************
// Optional stuff.

MMRESULT CWavePlayer::get_PlaybackRate( DWORD& dwPlayRate )
{
	// I've never actually seen this supported.
	return ::waveOutGetPlaybackRate( get_Handle(), &dwPlayRate );
}

MMRESULT CWavePlayer::put_PlaybackRate( DWORD dwPlayRate )
{
	if ( ! ( m_Caps.dwSupport & WAVECAPS_PLAYBACKRATE ))
		return( MMSYSERR_NOTSUPPORTED );
	return( ::waveOutSetPlaybackRate( get_Handle(), dwPlayRate ));
}

MMRESULT CWavePlayer::get_Volume( DWORD& dwVolume )
{
#if (WINVER >= 0x0400) || defined( _WIN32 )
	// win32 now requires the device to be open !
	return( ::waveOutGetVolume( get_Handle(), & dwVolume ));
#else
	return( ::waveOutGetVolume( m_uDeviceID, & dwVolume ));
#endif
}

MMRESULT CWavePlayer::put_Volume( DWORD dwVolume )
{
	// Volume = range from 0 to 0xFFFF for each channel
#if (WINVER >= 0x0400) || defined( _WIN32 )
	// win32 now requires the device to be open !
	return( ::waveOutSetVolume( get_Handle(), dwVolume ));
#else
	return( ::waveOutSetVolume( m_uDeviceID, dwVolume ));
#endif
}

MMRESULT CWavePlayer::put_VolumeStereo( WORD wVolumeLeft, WORD wVolumeRight )
{
	// Left is always first.
	DWORD dwVolume = MAKELONG(wVolumeLeft,wVolumeRight);
	return put_Volume(dwVolume);
}

#endif 	// _WIN32
