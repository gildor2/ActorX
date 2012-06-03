/*****************************************************************
 	
	ActorX.cpp		Actor eXporter for Unreal.

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
	Created by Erik de Neve

	Exports smooth-skinned meshes and arbitrary hierarchies of regular textured meshes, 
	and their animation in offsetvector/quaternion timed-key format.

    
  Main structures:
	   SceneIFC OurScene  => Contains necessary current scene info i.e. the scene tree, current timings, etc.
	   VActor   TempActor => Contains reference skeleton+skin, accumulated animations.

  Revision  1.4  November 2000
     - Moved everything around to help code re-use in Maya exporter.
	 - Put in some bone-count checks to avoid creation of broken .PSA's.
	 -  

  Rev 1.6  Jan 2001
     - Minor fixes

  Rev 1.7 Feb 2001
     - Minor stuff / safer dynamic arrays..(?!)
	 - Note: still gets pointer error related to memory deletion in the dyn. arrays in debug mode (known VC++ issue)

  Todo:
    - Debug the material-index override (the sorting & wedge remapping process )
  	- Fix 'get scene info'.
	- Fix the 'frame-0 reference pose must be accessible' requirement for non-biped(?) meshes.
	- Verify whether it's the somewhat arbitrary sign flips in the quats at '#inversion' cause the character to turn 90 degrees at export.
	- Verify mystery crash when editing/saving/loading multiple animation sequences are fixed...
	- Extend to export classic vertex mesh animations.
	- Enable more flexible 'nontextured' mesh export.
	- Extend to export textured level architecture brushes ?
	.....
	- Find out why Max exporter still makes character turn 90 degrees at export.	
	- Verify whether mystery crash on editing/saving/loading multiple animation sequences is fixed.	
	- Extend to export textured new level-architecture 'static-brushes', with smoothing groups

  Rev 1.8 feb 2001 
    - Fixed base/class name getting erased between sessions.

  Rev 1.9 - 1.92   may 2001
    - Batch processing option (Max)	
	- Untextured vertices warning names specific mesh (max)
	- Persistent names & settings
	Todo:
	- ? option for persistent options (max & maya...)
	- ? first-draft vertex exporting ?
	- ? 
	- ? Maya scaler modifier before root: accept / warn ?
	- ? 

   .....

   Rev    may 2003



  Todo - complete the unification into single project with build options for various Max Maya versions.
     The only project-specific files are now BrushExport, MayaInterface, MaxInterface ( and various 
	 resource files that may be in one but not the other )

  #ifdef MAX
  #ifdef MAYA 
  MAYAVER = 3, 4 etc
  MAXVER = 3,4 etc
  MAXCSVER=3.1, 3.2 etc

  	      
  	      
******************************************************************/

// Local includes
#include "ActorX.h"
#include "SceneIFC.h"
#include "MaxInterface.h"


// General parent-application independent globals.

SceneIFC	 OurScene;
VActor       TempActor;

TextFile DLog;
TextFile AuxLog;
TextFile MemLog;

WinRegistry  PluginReg;

MaxPluginClass* GlobalPluginObject;

#ifdef MAYA
TCHAR   PluginRegPath[] = _T("Software\\Epic Games\\ActorXMaya");
#endif

#ifdef MAX
TCHAR   PluginRegPath[] = _T("Software\\Epic Games\\ActorXMax");
#endif

// Path globals

TCHAR	DestPath[MAX_PATH];
TCHAR	LogPath[MAX_PATH];
TCHAR	to_path[MAX_PATH];
TCHAR	to_animfile[MAX_PATH];
TCHAR	to_skinfile[MAX_PATH];
//char	to_brushfile[MAX_PATH],
//		to_brushpath[MAX_PATH];
TCHAR	to_pathvtx[MAX_PATH];
TCHAR	to_skinfilevtx[MAX_PATH];
TCHAR	framerangestring[MAXINPUTCHARS];
TCHAR	vertframerangestring[MAXINPUTCHARS];
TCHAR	classname[MAX_PATH];
TCHAR	basename[MAX_PATH];
TCHAR	batchfoldername[MAX_PATH];
char	materialnames[8][MAX_PATH];
TCHAR	newsequencename[MAX_PATH];

void ResetPlugin()
{

	// Clear all digested scenedata/animation/path settings etc.

	GlobalPluginObject = NULL;

	// Ensure global file/path strings are reset.
	to_path[0]=0;
	LogPath[0]=0;
	to_skinfile[0]=0;
	to_animfile[0]=0;
	newsequencename[0]=0;
	DestPath[0]=0;
	framerangestring[0]=0;
	vertframerangestring[0]=0;
	newsequencename[0]=0;

	for(INT t=0; t<8; t++)
		memset(materialnames[t], 0 ,MAX_PATH);

	// Set access to our registry path
	PluginReg.SetRegistryPath( PluginRegPath );
	

	// Get all relevant ones from the registry....
	INT SwitchPersistent;
	PluginReg.GetKeyValue(_T("PERSISTSETTINGS"), SwitchPersistent);
	
	if( SwitchPersistent )
	{
		PluginReg.GetKeyValue(_T("DOPCX"), OurScene.DoTexPCX);
		PluginReg.GetKeyValue(_T("DOFIXROOT"),OurScene.DoFixRoot );
		PluginReg.GetKeyValue(_T("DOCULLDUMMIES"),OurScene.DoCullDummies );
		PluginReg.GetKeyValue(_T("DOTEXGEOM"),OurScene.DoTexGeom );
		PluginReg.GetKeyValue(_T("DOPHYSGEOM"),OurScene.DoSkinGeom );
		PluginReg.GetKeyValue(_T("DOSELGEOM"),OurScene.DoSelectedGeom);
		PluginReg.GetKeyValue(_T("DOSKIPSEL"),OurScene.DoSkipSelectedGeom);
		PluginReg.GetKeyValue(_T("DOEXPSEQ"),OurScene.DoExplicitSequences);
		PluginReg.GetKeyValue(_T("DOLOG"),OurScene.DoLog);
		
		// Maya specific ?
		//PluginReg.GetKeyValue(_T("QUICKSAVEDISK"),OurScene.QuickSaveDisk);
		//PluginReg.GetKeyValue(_T("DOFORCERATE"),OurScene.DoForceRate);
		//PluginReg.GetKeyValue(_T("PERSISTENTRATE"),OurScene.PersistentRate);
		//PluginReg.GetKeyValue(_T("DOREPLACEUNDERSCORES"),OurScene.DoReplaceUnderscores);

		PluginReg.GetKeyValue(_T("DOUNSMOOTH"),OurScene.DoUnSmooth);
		PluginReg.GetKeyValue(_T("DOEXPORTSCALE"),OurScene.DoExportScale);
		PluginReg.GetKeyValue(_T("DOTANGENTS"),OurScene.DoTangents);
		PluginReg.GetKeyValue(_T("DOAPPENDVERTEX"),OurScene.DoAppendVertex);
		PluginReg.GetKeyValue(_T("DOSCALEVERTEX"),OurScene.DoScaleVertex);

	}

	INT SwitchPersistPaths;
	PluginReg.GetKeyValue(_T("PERSISTPATHS"), SwitchPersistPaths);

	if( SwitchPersistPaths )
	{
		PluginReg.GetKeyString(_T("BASENAME"), basename );
		PluginReg.GetKeyString(_T("CLASSNAME"), classname );

		PluginReg.GetKeyString(_T("TOPATH"), to_path );
		_tcscpy(LogPath,to_path);
		PluginReg.GetKeyString(_T("TOANIMFILE"), to_animfile );
		PluginReg.GetKeyString(_T("TOSKINFILE"), to_skinfile );

		PluginReg.GetKeyString(_T("TOPATHVTX"), to_pathvtx );
		PluginReg.GetKeyString(_T("TOSKINFILEVTX"), to_skinfilevtx );
	}

}







