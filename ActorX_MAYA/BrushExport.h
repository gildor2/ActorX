// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

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
