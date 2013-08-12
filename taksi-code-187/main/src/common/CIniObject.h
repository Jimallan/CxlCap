//
// CIniObject.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#pragma once
#include "TaksiCommon.h"
#include <stdio.h>

#define INI_CR "\n"

struct TAKSI_LINK CIniObject
{
	// Generic object with props in an INI file.
public:
	void PropsInit()
	{
		ASSERT( get_PropQty() <= 32 );
		m_dwWrittenMask = 0;
	}

	bool PropSetName( const char* pszProp, const char* pszValue );
	bool PropWrite( FILE* pFile, int eProp ) const;
	bool PropWriteName( FILE* pFile, const char* pszProp );
	void PropsWrite( FILE* pFile );

	virtual const char** get_Props() const = 0;
	virtual int get_PropQty() const = 0;
	virtual bool PropSet( int eProp, const char* pszValue ) = 0;
	virtual int PropGet( int eProp, char* pszValue, int iSizeMax ) const = 0;

public:
	// ASSUME get_PropQty() <= 32 
	DWORD  m_dwWrittenMask;	// bitmask of props already written.
};
