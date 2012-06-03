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
char    PluginRegPath[] = ("Software\\Epic Games\\ActorXMaya");
#endif

#ifdef MAX
char    PluginRegPath[] = ("Software\\Epic Games\\ActorXMax");
#endif

// Path globals

char	DestPath[MAX_PATH], 
		LogPath[MAX_PATH],
  		to_path[MAX_PATH],
		to_animfile[MAX_PATH],
		to_skinfile[MAX_PATH],
		to_brushfile[MAX_PATH],
		to_brushpath[MAX_PATH],
		to_pathvtx[MAX_PATH],
		to_skinfilevtx[MAX_PATH],
		framerangestring[MAXINPUTCHARS],
		vertframerangestring[MAXINPUTCHARS],
		classname[MAX_PATH],		
		basename[MAX_PATH],	
		batchfoldername[MAX_PATH],
		materialnames[8][MAX_PATH],
		newsequencename[MAX_PATH];

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
	PluginReg.SetRegistryPath( (char*) &PluginRegPath );
	

	// Get all relevant ones from the registry....
	INT SwitchPersistent;
	PluginReg.GetKeyValue("PERSISTSETTINGS", SwitchPersistent);
	
	if( SwitchPersistent )
	{
		PluginReg.GetKeyValue("DOPCX", OurScene.DoTexPCX);
		PluginReg.GetKeyValue("DOFIXROOT",OurScene.DoFixRoot );
		PluginReg.GetKeyValue("DOCULLDUMMIES",OurScene.DoCullDummies );
		PluginReg.GetKeyValue("DOTEXGEOM",OurScene.DoTexGeom );
		PluginReg.GetKeyValue("DOPHYSGEOM",OurScene.DoSkinGeom );
		PluginReg.GetKeyValue("DOSELGEOM",OurScene.DoSelectedGeom);
		PluginReg.GetKeyValue("DOSKIPSEL",OurScene.DoSkipSelectedGeom);
		PluginReg.GetKeyValue("DOEXPSEQ",OurScene.DoExplicitSequences);
		PluginReg.GetKeyValue("DOLOG",OurScene.DoLog);
		
		// Maya specific ?
		//PluginReg.GetKeyValue("QUICKSAVEDISK",OurScene.QuickSaveDisk);
		//PluginReg.GetKeyValue("DOFORCERATE",OurScene.DoForceRate);
		//PluginReg.GetKeyValue("PERSISTENTRATE",OurScene.PersistentRate);
		//PluginReg.GetKeyValue("DOREPLACEUNDERSCORES",OurScene.DoReplaceUnderscores);

		PluginReg.GetKeyValue("DOUNSMOOTH",OurScene.DoUnSmooth);
		PluginReg.GetKeyValue("DOEXPORTSCALE",OurScene.DoExportScale);
		PluginReg.GetKeyValue("DOTANGENTS",OurScene.DoTangents);
		PluginReg.GetKeyValue("DOAPPENDVERTEX",OurScene.DoAppendVertex);
		PluginReg.GetKeyValue("DOSCALEVERTEX",OurScene.DoScaleVertex);

	}

	INT SwitchPersistPaths;
	PluginReg.GetKeyValue("PERSISTPATHS", SwitchPersistPaths);

	if( SwitchPersistPaths )
	{
		PluginReg.GetKeyString("BASENAME", basename );
		PluginReg.GetKeyString("CLASSNAME", classname );

		PluginReg.GetKeyString("TOPATH", to_path );
		_tcscpy(LogPath,to_path);
		PluginReg.GetKeyString("TOANIMFILE", to_animfile );
		PluginReg.GetKeyString("TOSKINFILE", to_skinfile );

		PluginReg.GetKeyString("TOPATHVTX", to_pathvtx );
		PluginReg.GetKeyString("TOSKINFILEVTX", to_skinfilevtx );
	}

}







