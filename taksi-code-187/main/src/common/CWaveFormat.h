//
// CWaveFormat.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
// Audio format def.
//
#ifndef _INC_CWaveFormat_H
#define _INC_CWaveFormat_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
#include "CHeapBlock.h"

#ifdef _WIN32 
#include <mmsystem.h>

#if(WINVER >= 0x0400)
#include <mmreg.h>
#else
#define _WAVEFORMATEX_
typedef struct tWAVEFORMATEX
{
	WORD    wFormatTag;        // format type
	WORD    nChannels;         // number of channels (i.e. mono, stereo...)
	DWORD   nSamplesPerSec;    // sample rate
	DWORD   nAvgBytesPerSec;   // for buffer estimation
	WORD    nBlockAlign;       // block size of data
	WORD    wBitsPerSample;    // Number of bits per sample of mono data
	WORD    cbSize;            // The count in bytes of the size of
	// extra information (after cbSize)
} WAVEFORMATEX;
#endif // _WAVEFORMATEX_

typedef DWORD WAVE_BLOCKS_t;     // Index in blocks to the sound buffer.

//***********************************************************************
// -CWaveFormat

class TAKSI_LINK CWaveFormat
{
	// Base container to contain WAVEFORMATEX (variable length)
public:

	void InitFormatEmpty()
	{
		m_iAllocSize = 0;
	}

	int get_FormatSize( void ) const
	{
		return( m_iAllocSize );
	}

	bool SetFormatBytes( const BYTE* pFormData, int iSize );
	bool SetFormat( const WAVEFORMATEX FAR* pForm );
	bool SetFormatPCM( WORD nChannels=1, DWORD nSamplesPerSec=11025, WORD wBitsPerSample=8 );

	bool IsValidFormat() const;
	bool IsValidBasic() const
	{
		return( get_BlockSize() != 0 );
	}
	void ReCalc( void );

	bool IsSameAs( const WAVEFORMATEX FAR* pForm ) const;

	WAVE_BLOCKS_t CvtSrcToDstSize( WAVE_BLOCKS_t SrcSize, const WAVEFORMATEX* pDstForm ) const;
	DWORD GetRateDiff( const WAVEFORMATEX* pDstForm ) const
	{
		// Get a sample rate conversion factor.
		return(( get_SamplesPerSec() << 16 ) / pDstForm->nSamplesPerSec );
	}

	//
	// Get info about the format.
	//
	WAVEFORMATEX* get_WF() const
	{
		return( (WAVEFORMATEX*)m_cbBytes );
	}
	operator WAVEFORMATEX*()
	{
		return( (WAVEFORMATEX*)m_cbBytes );
	}
	operator const WAVEFORMATEX*() const
	{
		return( (const WAVEFORMATEX*)m_cbBytes );
	}
	bool IsPCM() const
	{
		ASSERT(get_WF());
		return( get_WF()->wFormatTag == WAVE_FORMAT_PCM );
	}
	WORD get_BlockSize() const
	{
		// This is the smallest possible useful unit size.
		ASSERT(get_WF());
		return( get_WF()->nBlockAlign );
	}
	DWORD get_SamplesPerSec() const
	{
		ASSERT(get_WF());
		return( get_WF()->nSamplesPerSec );
	}
	DWORD get_BytesPerSec() const
	{
		ASSERT(get_WF());
		return( get_WF()->nAvgBytesPerSec );
	}
	WORD get_Channels() const
	{
		ASSERT(get_WF());
		return( get_WF()->nChannels );
	}
	WORD get_SampleSize() const  // valid for PCM only.
	{
		// In Bytes (only valid for PCM really.)
		ASSERT(get_WF());
		ASSERT(IsPCM());
		return(( get_WF()->wBitsPerSample == 8 ) ? 1 : 2 );	// sizeof(WORD)
	}
	DWORD get_SampleRangeHalf( void ) const    // valid for PCM only.
	{
		// Get half the max dynamic range available
		ASSERT(get_WF());
		ASSERT(IsPCM());
		int iBits = ( get_WF()->wBitsPerSample ) - 1;
		return((1<<iBits) - 1 );
	}

	//
	// Convert Samples, Time(ms), and Bytes to Blocks and back.
	//

	DWORD CvtBlocksToBytes( WAVE_BLOCKS_t index ) const
	{
		return( index * get_BlockSize());
	}
	WAVE_BLOCKS_t CvtBytesToBlocks( DWORD sizebytes ) const
	{
		return( sizebytes / get_BlockSize());
	}

	DWORD CvtBlocksToSamples( WAVE_BLOCKS_t index ) const;
	WAVE_BLOCKS_t CvtSamplesToBlocks( DWORD dwSamples ) const;

	TIMESYS_t CvtBlocksTomSec( WAVE_BLOCKS_t index ) const;
	WAVE_BLOCKS_t CvtmSecToBlocks( TIMESYS_t mSec ) const;

	static int __stdcall FormatCalcSize( const WAVEFORMATEX FAR* pForm );
	bool ReAllocFormatSize( int iSize );

protected:
	int get_CalcSize( void ) const;

protected:
	int m_iAllocSize;		// sizeof(WAVEFORMATEX) + cbSize;
	char m_cbBytes[256];	// Note: this must be shared memory since it lives in sg_Config
};

#endif
#endif // _INC_CWaveFormat_H
