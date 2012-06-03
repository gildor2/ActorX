/*****************************************************************
pcf ugly fix:
	- add suffix _Mat to texture path for autoimporting materials
			.. (int extensionDot)

******************************************************************/

/*****************************************************************
 	
	ActorX.cpp		Actor eXporter for Unreal.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
	Created by Erik de Neve 

	Exports smooth-skinned meshes and arbitrary hierarchies of regular textured meshes, 
	and their animation in offsetvector/quaternion timed-key format.
    
  Main structures:
	   SceneIFC OurScene  => Contains necessary current scene info i.e. the scene tree, current timings, etc.
	   VActor   TempActor => Contains reference skeleton+skin, accumulated animations.


  Revision list (  see top of MayaInterface.cpp & MaxInterface.cpp for more platform-specific items. )

  Rev 1.4  November 2000
     - Moved everything around to help code re-use in Maya exporter.
	 - Put in some bone-count checks to avoid creation of broken .PSA's.

  Rev 1.6  Jan 2001
     - Minor fixes

  Rev 1.7 Feb 2001
     - Minor stuff / safer dynamic arrays..(?!)
	 - Note: still gets pointer error related to memory deletion in the dyn. arrays in debug mode (known VC++ issue)   

  Rev 1.8 feb 2001 
    - Fixed base/class name getting erased between sessions.

  Rev 1.9 - 1.92   may 2001
    - Batch processing option (Max)	
	- Untextured vertices warning names specific mesh (max)
	- Persistent names & settings

  Rev 2.0 - 2.12   winter 2002-2003
    - Vertex animation export
	- Maya ASE brush export
	- Edge smoothing to smoothing group conversion
	- Automatic arbitrary-poly triangulation	
    - 'Scene Info' button works again.

   Rev 2.26
   Rev 2.27 - temp disabling of any root locking/start-to-end interpolation.
   Rev 2.28 - fixed the messed up  dofix/lock/posezero options..?!!


  Todo (MAYA)
	  - ? Maya scaler modifier before root: accept / warn ?
	  - Look into unification into single project with build options for various Max Maya versions ?
         The only project-specific files are now BrushExport, MayaInterface, MaxInterface ( and various 
	     resource files that may be in one but not the other )

  Todo (MAX/MAYA)

	- Clean up the header inclusion spaghetti
    - Debug the material-index override (the sorting & wedge remapping process )
  	- Fix 'get scene info'.
	- Fix the 'frame-0 reference pose must be accessible' requirement for non-biped(?) meshes.
	- Verify whether it's the somewhat arbitrary sign flips in the quats at '#inversion' cause the character to turn 90 degrees at export.
	- Verify mystery crash when editing/saving/loading multiple animation sequences are fixed...	
	- Enable more flexible 'nontextured' mesh export.	


  #ifdef MAX
  #ifdef MAYA 
  MAYAVER = 3, 4 etc
  MAXVER = 3,4 etc
  MAXCSVER=3.1, 3.2 etc

******************************************************************/


// Local includes
#include "ActorX.h"
#include "SceneIFC.h"


// General parent-application independent globals.
SceneIFC	 OurScene;
VActor       TempActor;

TextFile DLog;
TextFile AuxLog;
TextFile MemLog;

WinRegistry  PluginReg;

#ifdef MAYA
char    PluginRegPath[] = ("Software\\Epic Games\\ActorXMaya");
#endif

#ifdef MAX
char    PluginRegPath[] = ("Software\\Epic Games\\ActorXMax");
#endif

// Paths globals

char	DestPath[MAX_PATH],   // General  scratchpath :)
		LogPath[MAX_PATH],  // Log file destination.
  		to_path[MAX_PATH],   // Global output folder name.
		to_meshoutpath[MAX_PATH],  
		to_animfile[MAX_PATH],
		to_skinfile[MAX_PATH],
		newsequencename[MAX_PATH],
		framerangestring[MAXINPUTCHARS],
		vertframerangestring[MAXINPUTCHARS],
		classname[MAX_PATH],		
		basename[MAX_PATH],	
		batchfoldername[MAX_PATH];


INT		ExportBlendShapes;

void ResetPlugin()
{
	// Clear all digested scenedata/animation/path settings etc.

	// Ensure global file/path strings are reset.
	to_path[0]=0;
	to_meshoutpath[0];
	LogPath[0]=0;
	to_skinfile[0]=0;
	to_animfile[0]=0;
	newsequencename[0]=0;
	DestPath[0]=0;
	framerangestring[0]=0;
	vertframerangestring[0]=0;
	ExportBlendShapes=0;

	// Set access to our registry path
	PluginReg.SetRegistryPath( (char*) &PluginRegPath );	

	// Get all relevant ones from the registry....
	INT SwitchPersistent;
	PluginReg.GetKeyValue("PERSISTSETTINGS", SwitchPersistent);
	
	if( SwitchPersistent )
	{
		// Skeletal export options.
		PluginReg.GetKeyValue("DOPCX", OurScene.DoTexPCX);
		PluginReg.GetKeyValue("DOFIXROOT",OurScene.DoFixRoot );
		PluginReg.GetKeyValue("DOLOCKROOT",OurScene.DoLockRoot );
		PluginReg.GetKeyValue("DOPOSEZERO",OurScene.DoPoseZero);

		PluginReg.GetKeyValue("DOCULLDUMMIES",OurScene.DoCullDummies );
		PluginReg.GetKeyValue("DOTEXGEOM",OurScene.DoTexGeom );
		PluginReg.GetKeyValue("DOPHYSGEOM",OurScene.DoPhysGeom );
		PluginReg.GetKeyValue("DOSELGEOM",OurScene.DoSelectedGeom);
		PluginReg.GetKeyValue("DOEXPSEQ",OurScene.DoExplicitSequences);
		PluginReg.GetKeyValue("DOLOG",OurScene.DoLog);
		PluginReg.GetKeyValue("DOHIERBONES",OurScene.DoHierBones);
		PluginReg.GetKeyValue("DOBAKEANIMS",OurScene.DoBakeAnims);

		PluginReg.GetKeyValue("QUICKSAVEDISK",OurScene.QuickSaveDisk);
		PluginReg.GetKeyValue("DOFORCERATE",OurScene.DoForceRate);
		PluginReg.GetKeyValue("PERSISTENTRATE",OurScene.PersistentRate);
		PluginReg.GetKeyValue("DOREPLACEUNDERSCORES",OurScene.DoReplaceUnderscores);
		PluginReg.GetKeyValue("SKELAUTOTRI",OurScene.DoAutoTriangles);
		PluginReg.GetKeyValue("STRIPREFERENCEPREFIX",OurScene.bCheckStripRef);

		//PCF BEGIN
		PluginReg.GetKeyValue("DOSECONDUVSETSKINNED",OurScene.DoSecondUVSetSkinned);
		//PCF END

		PluginReg.GetKeyValue("DOUNSMOOTH",OurScene.DoUnSmooth);
		PluginReg.GetKeyValue("DOTANGENTS",OurScene.DoTangents);
		PluginReg.GetKeyValue("DOAPPENDVERTEX",OurScene.DoAppendVertex);
		PluginReg.GetKeyValue("DOSCALEVERTEX",OurScene.DoScaleVertex);
		PluginReg.GetKeyValue("DOSUPPRESSANIMPOPUPS", OurScene.DoSuppressAnimPopups );

		// Static mesh export options.		
		PluginReg.GetKeyValue("DOCONVERTSMOOTH", OurScene.DoConvertSmooth );
		PluginReg.GetKeyValue("DOFORCETRIANGLES", OurScene.DoForceTriangles );
		PluginReg.GetKeyValue("DOUNDERSCORESPACE", OurScene.DoUnderscoreSpace);
		PluginReg.GetKeyValue("DOGEOMASFILENAME", OurScene.DoGeomAsFilename);
		PluginReg.GetKeyValue("DOSELECTEDSTATIC", OurScene.DoSelectedStatic );
		PluginReg.GetKeyValue("DOSUPPRESSPOPUPS", OurScene.DoSuppressPopups );	
		PluginReg.GetKeyValue("DOCONSOLIDATE", OurScene.DoConsolidateGeometry );

		PluginReg.GetKeyString( "TOMESHOUTPATH", to_meshoutpath );		
	}

	INT SwitchPersistPaths;
	PluginReg.GetKeyValue("PERSISTPATHS", SwitchPersistPaths);

	if( SwitchPersistPaths )
	{
		PluginReg.GetKeyString("BASENAME", basename );
		PluginReg.GetKeyString("CLASSNAME", classname );

		PluginReg.GetKeyString("TOPATH", to_path );
		_tcscpy_s(LogPath,to_path);		
		PluginReg.GetKeyString("TOANIMFILE", to_animfile );
		PluginReg.GetKeyString("TOSKINFILE", to_skinfile );
		
	}

}







