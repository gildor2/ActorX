/**************************************************************************
 
   Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

   BrushExport.h - Maya specific, helper functions for static mesh digestion

   Created by Erik de Neve

***************************************************************************/

#ifdef MAYA

#ifndef BRUSHEXPORT_H
#define BRUSHEXPORT_H

#include "MayaInclude.h"

MStatus UTExportMesh( const MArgList& args );
MStatus AXWriteSequence(BOOL bUseSourceOutPath);
MStatus AXWritePoses();

#endif
#endif
