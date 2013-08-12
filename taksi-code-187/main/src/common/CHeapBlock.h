//
// CHeapBlock.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
// a dynamically allocated block of heap memory.

#ifndef _INC_CHeapBlock_H
#define _INC_CHeapBlock_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
#include "TaksiCommon.h"

#include <stdlib.h>	
#include <malloc.h>	// malloc()

#if defined(_WIN32) && !defined(UNDER_CE)
#include <crtdbg.h>
#endif

#define CHeapBlock_IsValid(p)			((p)!=NULL)
#define CHeapBlock_IsValidOffset(p,i)	((p)!=NULL)
#define CHeapBlock_Alloc(i)				malloc(i)
#define CHeapBlock_Free(p)				free(p)
#define CHeapBlock_ReAlloc(p,i)			realloc(p,i)

#endif	// _INC_CHeapBlock_H
