//
// CWaveDevice.H
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//

#ifndef _INC_CWaveDevice_H
#define _INC_CWaveDevice_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CRefPtr.h"
#include <mmsystem.h>

typedef UINT WAVE_DEVICEID_TYPE;
#define WAVE_DEVICE_NONE		((WAVE_DEVICEID_TYPE)( WAVE_MAPPER-1 ))  // no device at all.

typedef UINT WAVE_PRODUCTID_TYPE;
#define WAVE_GET_PID(caps)		((WAVE_PRODUCTID_TYPE) MAKELONG((caps).wPid,(caps).wMid)) // get full product id = manfacturer + product

class CWaveDevice;
class CWaveFormat;

class TAKSI_LINK CWaveHeaderBase
{
	// Wrapper for WAVEHDR. 
	// base class for any app specific stuff.
	// Used to pass a buffer to the wave driver.
	// pHdr->dwUser = this.
	// NOTE: dwBufferLength should be a multiple of the current format get_BlockSize() !
	
public:
	CWaveHeaderBase( LPSTR lpData=NULL, DWORD dwBufferLength=0 );
	~CWaveHeaderBase();

	void UnprepareDevice();  // release from device. (NOT WHILE WHDR_INQUEUE !)
	MMRESULT PrepareDevice( CWaveDevice* pDevice );

	void DetachData();
	void AttachData( LPSTR lpData, DWORD dwBufferLength );

	LPWAVEHDR get_wh() const
	{
		return m_pwh;
	}
	bool IsValid() const
	{
		if ( m_pwh == NULL )
			return false;
		if ( m_pwh->lpData == NULL )
			return false;
		ASSERT( m_pwh->dwUser == (DWORD_PTR) this );
		return true;
	}
	bool IsValidCheck()
	{
		return(IsValid());
	}
	bool IsInQueue() const
	{
		if ( ! IsValid())
			return false;
		if ( m_pwh->dwFlags & WHDR_INQUEUE )
			return true;
		return false;
	}

public:
	CWaveDevice *m_pDevice;

private:
	LPWAVEHDR m_pwh;	// Specially global allocated buffer.
	bool m_bManageDataBuffer;	// do i locally allocate the buffer?
};

// No longer need CRefObjBase
class TAKSI_LINK CWaveDevice /*: public CRefObjBase */
{
	// A wave device we could open and send/recieve sound data to/from
	// CRefObjBase = all WAVEHDR s that are WHDR_PREPARED for this device.
	friend class CWaveHeaderBase;

protected:
	CWaveDevice( WAVE_DEVICEID_TYPE uDeviceID = WAVE_MAPPER );
public:
	~CWaveDevice();

	static WAVE_DEVICEID_TYPE __stdcall FindDeviceByProductID( bool bInput, WAVE_PRODUCTID_TYPE dwPid );

	operator HWAVE() const
	{
		return( m_hDevice );
	}
	HWAVE get_Handle() const
	{
		return( m_hDevice );
	}
	void DetachHandle()
	{
		// Don't close this device on destroy ?
		m_hDevice = NULL;
	}
 
	WAVE_DEVICEID_TYPE get_DeviceID() const
	{
		// default = WAVE_MAPPER
		return m_uDeviceID;
	}

	virtual MMRESULT put_DeviceID( WAVE_DEVICEID_TYPE uDeviceID ) = 0;
	virtual WAVE_DEVICEID_TYPE QueryDeviceID() const = 0; // if WAVE_MAPPER, what device are we mapped to ?

	bool IsActive() const
	{
		return( m_fActive );
	}
	int get_HeadersInQueue() const
	{
		return m_iHeadersInQueue;
	}
	int& ref_HeadersInQueue()
	{
		return m_iHeadersInQueue;
	}

	virtual MMRESULT Open( const CWaveFormat& format, UINT_PTR dwCallback, UINT_PTR dwInstanceData, DWORD fdwOpenFlags ) = 0;
	virtual void Close() = 0;

	virtual MMRESULT Start() = 0;
	virtual MMRESULT Stop() = 0;
	virtual MMRESULT Reset() = 0;

	virtual MMRESULT GetPosition( MMTIME &Info ) const = 0;
	virtual MMRESULT WriteHeader( CWaveHeaderBase* pHdr ) = 0;

	// Driver returns headers when it is done with it.
	static CWaveHeaderBase* __stdcall OnHeaderReturn( LPWAVEHDR pHdr );

private:
	virtual MMRESULT PrepareHeader( LPWAVEHDR pwh ) = 0;
	virtual MMRESULT UnprepareHeader( LPWAVEHDR pwh ) = 0;

protected:
	WAVE_DEVICEID_TYPE m_uDeviceID;	// Device number on the current system. NOTE: use ProductID for serialize. (device id can change)
	HWAVE	m_hDevice;		// Handle to the open input or output device. HWAVEIN or HWAVEOUT
	bool	m_fActive;			// Is the device actively playing or recording ? waveInStart()
	int		m_iHeadersInQueue;	// device has possesion of this # of headers. Cannot close til this is 0.
};

typedef CRefPtr<CWaveDevice> CWaveDevicePtr;

class TAKSI_LINK CWaveRecorder : public CWaveDevice
{
	// A device for recording wave/sound data.
public:
	CWaveRecorder( WAVE_DEVICEID_TYPE uDeviceID = WAVE_MAPPER );
	~CWaveRecorder();

	operator HWAVEIN() const
	{
		return((HWAVEIN) m_hDevice );
	}
	HWAVEIN get_Handle() const
	{
		return((HWAVEIN) m_hDevice );
	}
	WAVE_PRODUCTID_TYPE get_ProductID() const
	{
		// return 0 for WAVE_DEVICE_NONE
		return WAVE_GET_PID(m_Caps);
	}

	virtual MMRESULT put_DeviceID( WAVE_DEVICEID_TYPE uDeviceID );
	virtual WAVE_DEVICEID_TYPE QueryDeviceID() const;

	virtual MMRESULT Open( const CWaveFormat& format, UINT_PTR dwCallback, UINT_PTR dwInstanceData, DWORD fdwOpenFlags );
	virtual void Close();

	MMRESULT SetLowPri();	// background recorder.
	virtual MMRESULT Start();
	virtual MMRESULT Stop();
	virtual MMRESULT Reset();

	virtual MMRESULT GetPosition( MMTIME &Info ) const;
	virtual MMRESULT WriteHeader( CWaveHeaderBase* pHdr );

private:
	virtual MMRESULT PrepareHeader( LPWAVEHDR pwh );
	virtual MMRESULT UnprepareHeader( LPWAVEHDR pwh );
public:
	WAVEINCAPS  m_Caps;       // Capabilities of the input device.
};

class TAKSI_LINK CWavePlayer : public CWaveDevice
{
	// A device for playing wave/sound data.
public:
	CWavePlayer( WAVE_DEVICEID_TYPE uDeviceID = WAVE_MAPPER );
	~CWavePlayer();

	operator HWAVEOUT() const
	{
		return((HWAVEOUT) m_hDevice );
	}
	HWAVEOUT get_Handle() const
	{
		return((HWAVEOUT) m_hDevice );
	}
	WAVE_PRODUCTID_TYPE get_ProductID() const
	{
		// return 0 for WAVE_DEVICE_NONE
		return WAVE_GET_PID(m_Caps);
	}
	virtual MMRESULT put_DeviceID( WAVE_DEVICEID_TYPE uDeviceID );
	virtual WAVE_DEVICEID_TYPE QueryDeviceID() const;

	virtual MMRESULT Open( const CWaveFormat& format, UINT_PTR dwCallback, UINT_PTR dwInstanceData, DWORD fdwOpenFlags );
	virtual void Close();

	virtual MMRESULT GetPosition( MMTIME &Info ) const;
	virtual MMRESULT Start();
	virtual MMRESULT Stop();
	virtual MMRESULT Reset();
	virtual MMRESULT WriteHeader( CWaveHeaderBase* pHdr );

	// 0 to 0xffff per stereo channel volume
	MMRESULT get_Volume( DWORD& dwVolume );
	MMRESULT put_Volume( DWORD dwVolume );
	MMRESULT put_VolumeStereo( WORD wVolumeLeft, WORD wVolumeRight );

	// Very rarely supported feature.
	MMRESULT get_PlaybackRate( DWORD& dwRate );
	MMRESULT put_PlaybackRate( DWORD dwRate );

private:
	virtual MMRESULT PrepareHeader( LPWAVEHDR pwh );
	virtual MMRESULT UnprepareHeader( LPWAVEHDR pwh );
public:
	WAVEOUTCAPS m_Caps;      // Capabilities of the output device.
};

#endif //_INC_CWaveDevice_H
