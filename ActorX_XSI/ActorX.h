// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**********************************************************************

	ActorX.h     Unreal Actor Exporter

**********************************************************************/

#ifndef __ACTORX__H
#define __ACTORX__H

void ResetPlugin();

// To make the ActorX code parts common to all platforms aware of which plug-in we're building:
#define AXPLATFORM_XSI (1)
#define AXPLATFORM_MAYA 0
#define AXPLATFORM_MAX 0

// Compile-time defines for debugging popups and logfiles
#define DEBUGMSGS (0)  // set to 0 to avoid debug popups
#define DEBUGFILE (0)  // set to 0 to suppress misc. log files being written from SceneIFC.cpp
#define DOLOGFILE (0)  // Log extra animation/skin info while writing a .PSA or .PSK file
#define DEBUGMEM  (0)  // Memory debugging

#include <windows.h>
#include <assert.h>
#include <malloc.h>	// for alloca




void A2WHelper( wchar_t* out_wcs, const char* in_sz, int in_cch = -1 );


#define A2W(out_wcs, in_sz) \
	if (NULL == (const char *)(in_sz)) \
		*(out_wcs) = NULL; \
	else \
	{ \
		*(out_wcs) = (wchar_t*)alloca((strlen((in_sz)) + 1) * sizeof(wchar_t)); \
		A2WHelper(*(out_wcs), (in_sz)); \
	}



#endif // __ACTORX__H
