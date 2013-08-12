//
// CIniObject.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "stdafx.h"
#include "CIniObject.h"
#include "Common.h"

static int Str_FindTableHead( const char* pszFind, const char** pszTable )
{
	ASSERT(pszFind);
	pszFind = Str_SkipSpace(pszFind);
	int iLen = 0;
	for (;pszFind[iLen];iLen++)
	{
		char ch = toupper(pszFind[iLen]);
		if ( ! isalnum(ch) && ch !='.' && ch !='_' )
			break;
	}
	if ( iLen <= 0 )
		return -1;
	for (int i=0; pszTable[i]; i++ )
	{
		if ( ! _strnicmp( pszFind, pszTable[i], iLen ))
			return i;
	}
	return -1;
}

bool CIniObject::PropSetName( const char* pszProp, const char* pszValue )
{
	int eProp = Str_FindTableHead(pszProp,get_Props());
	if ( eProp<0)
		return false;
	return PropSet(eProp,pszValue); 
}

bool CIniObject::PropWrite( FILE* pFile, int eProp ) const
{
	char szTmp[_MAX_PATH*2];
	szTmp[0] = '\0';
	if ( PropGet(eProp,szTmp,sizeof(szTmp)) < 0 )
		return false;
	const char* pszProp = get_Props()[eProp]; 
	ASSERT(pszProp);
	char szTmp2[_MAX_PATH*2];
	_snprintf( szTmp2, sizeof(szTmp2)-1, "%s=%s" INI_CR, pszProp, szTmp );
	fputs( szTmp2, pFile );
	return true;
}

bool CIniObject::PropWriteName( FILE* pFile, const char* pszProp )
{
	// Overwrite this prop.
	int eProp = Str_FindTableHead(pszProp,get_Props());
	if ( eProp < 0 )
		return false;
	if (m_dwWrittenMask & (1<<eProp))	// already written
		return false;
	PropWrite(pFile,eProp);
	m_dwWrittenMask |= (1<<eProp);
	return true;
}

void CIniObject::PropsWrite( FILE* pFile )
{
	// Write out all that are not already written.
	// Assume [HEADER] already written.
	int iQty = get_PropQty();
	for ( int i=0; i<iQty; i++ )
	{
		if (m_dwWrittenMask & (1<<i))	// was already written
			continue;
		PropWrite(pFile,i);
		m_dwWrittenMask |= (1<<i);
	}
}
