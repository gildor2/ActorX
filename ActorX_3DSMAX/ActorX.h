/**********************************************************************
 	
	ActorX.h     Unreal Actor Exporter
	
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

**********************************************************************/

#ifndef __ACTORX__H
#define __ACTORX__H

void ResetPlugin();

// Compile-time defines for debugging popups and logfiles
#define DEBUGMSGS (0)  // set to 0 to avoid debug popups
#define DEBUGFILE (0)  // set to 0 to suppress misc. log files being written from SceneIFC.cpp 
#define DOLOGFILE (1)  // Log extra animation/skin info while writing a .PSA or .PSK file
#define DEBUGMEM  (0)  // Memory debugging

#endif // __ACTORX__H
