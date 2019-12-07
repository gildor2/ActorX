// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

	MaxInterface.cpp - MAX-specific exporter scene interface code.

	Created by Erik de Neve

   Lots of fragments snipped from all over the Max SDK sources.

   Examples of custom (Max-plugin specific) controls and usage are found in
   the CustCTRL plugin, in the Max 2.5/3.0 SDK example sources.

  Changelist:
          - Jan 20 2003: Finally properly passing TimeStatic to GetTriObjectFromNode (instead of forcing time to 0 )
		  - Mar    2003: Version 2.04 - fixed 'selection' skin feature, and skin-only / textured geometry / inverted selection
		                 selection bugs and cleanups on subsequent re-exporting.
		  - Mar    2003: Version 2.06 - adds ability to export animated scaling ( all-or-nothing; for licensee extensions;
		                 and for now only supported by the exporter(s), not by the standard Unreal Technology engine:
						 simply gets ignored/not loaded. )
		 - Jun     2003  V 2.10  fixed interface bugs in vertex anim export (append etc)
		 - 7/01/03 version 2.10c: fix for anyone experiencing the "plugin failed to initialize" error.
		 - 7/19/03 version 2.11: fixed a skeletal mesh smoothing-group vertex splitting bug.
		 - 9/30/03 version 2.12: fixed output path bug in vertex animation saving.
		 - 12/29/03 version 2.12b - recompiled Max 6 plugin to fix reported compatibility issues.
		 - 05/20/04 version 2.14  - now allows point helpers to function as full skeletal members, and culled, like dummies.

		 - 07/07 version 2.15 - experimental - rip out all unnecessary header and .lib includes to try and squash the
						'plugin failed to initialize' error several people reported (suddenly  - must be related to a windows update ?)
						=> because of VC7.1, just provide the MSVCR71.dll along with the plugin.
		 - 10/29/04 version 2.17 -  Max 7 build option.
		 - 11/19/04 version 2.18 -  Export functionality exposed to MaxScript through function publishing.
		 - 01/21/05 version 2.19 -  Vertex export: fixed  append-to-existing feature and added checks for animation data compatibility.
		 - 08/09/05 version 2.20 -  Fixed crash in VActor::LoadAnimation.
		 - 02/23/09 version 2.21 -  Fixed to keep material index order by default.

   Todo:
          - Move any remaining non-Max dependent code to SceneIFC
          - Assertions for exact poly/vertex/wedge bounds (16 bit indices !) (max 21840) ?

***************************************************************************/

#include "SceneIFC.h"
#include "MaxInterface.h"
#include "ActorX.h"

// Globals
INT FlippedFaces;

// PLUGIN interface code.
static MaxPluginClass ThisMaxPlugin;

// Forward declarations.
// UBOOL ExecuteBatchFile( char* batchfilename, SceneIFC* OurScene );
UBOOL BatchAnimationProcess( const TCHAR* batchfoldername, INT &NumDigested, INT &NumFound, MaxPluginClass *u );
void SaveCurrentSceneSkin();
void DoDigestAnimation();
void SetInterfaceFromScript();

// This is the Class Descriptor for the ActorX plug-in
class ActorXClassDesc:public ClassDesc {
	public:
	int 			IsPublic() {return 1;}
	void *			Create(BOOL loading = FALSE) {return &ThisMaxPlugin;}
	const TCHAR*	ClassName() {return GetString(IDS_CLASS_NAME);}
	SClass_ID		SuperClassID() {return UTILITY_CLASS_ID;}
	Class_ID		ClassID() {return ACTORX_CLASS_ID;}
	const TCHAR* 	Category() {return GetString(IDS_CATEGORY);}
	void			ResetClassParams (BOOL fileReset);
};

static ActorXClassDesc ActorXDesc;

//
// Max interface pointer
//
Interface* TheMaxInterface;

void SetOurMAXInterface(Interface *i)
{
	TheMaxInterface = i;
}


//
// Animation buffer cleanup.
//

void ClearOutAnims()
{
	// Clear out anims...
	TempActor.RawAnimKeys.Empty();
	TempActor.RefSkeletonNodes.Empty();
	TempActor.RefSkeletonBones.Empty();

	{for( INT c=0; c<TempActor.OutAnims.Num(); c++)
	{
		TempActor.OutAnims[c].KeyTrack.Empty();
	}}
	TempActor.OutAnims.Empty();

	{for( INT c=0; c<TempActor.Animations.Num(); c++)
	{
		TempActor.Animations[c].KeyTrack.Empty();
	}}
	TempActor.Animations.Empty();
}


//
// Function Publishing for MaxScript access.
//

#define AXMI_FP_INTERFACE_ID Interface_ID(0x4bc15c79, 0x2fa67b53)  // Randomly chosen hex 32 bit id #s using GenCid.exe

static int MaxInterfaceInitialized = 0;

class AXMaxInterfaceFunctionPublish : public FPStaticInterface {

	UBOOL BackupBakeSmoothing;
	UBOOL BackupCullUnusedDummies;
	UBOOL BackupDoSelectedGeometry;
	UBOOL BackupTangentUVSplit;
	UBOOL BackupAllTextured;
	UBOOL BackupAllSkinType;

	UBOOL BackupDoForceRate;
	INT        SavedAnimPopupState;

	UBOOL	AXDoSelectedGeometry;
	UBOOL	AXCullUnusedDummies;
	UBOOL	AXTangentUVSplit;
	UBOOL	AXAllTextured;
	UBOOL	AXBakeSmoothing;
	UBOOL	AXAllSkinType;

public:
	DECLARE_DESCRIPTOR(AXMaxInterfaceFunctionPublish);

	enum OpID
	{
		kAXSetOutputPath,
		kAXExportMesh,
		kAXExportAnimSet,
		kAXDigestAnim,
		kAXSetAllSkinType,
		kAXSetAllTextured,
		kAXSetTangentUVSplit,
		kAXSetBakeSmoothing,
		kAXSetCullUnusedDummies,
		kAXSetDoSelectedGeometry,
	};

	BEGIN_FUNCTION_MAP

		FN_1(kAXSetOutputPath, TYPE_INT, AXSetOutputPath, TYPE_STRING )
		FN_1(kAXExportMesh, TYPE_INT, AXExportMesh, TYPE_STRING )
		FN_1(kAXExportAnimSet, TYPE_INT, AXExportAnimSet, TYPE_STRING )
		FN_4(kAXDigestAnim, TYPE_INT, AXDigestAnim, TYPE_STRING, TYPE_INT, TYPE_INT, TYPE_FLOAT  )

		FN_1(kAXSetAllSkinType, TYPE_INT, AXSetAllSkinType, TYPE_INT )
		FN_1(kAXSetAllTextured, TYPE_INT, AXSetAllTextured, TYPE_INT )
		FN_1(kAXSetTangentUVSplit, TYPE_INT, AXSetTangentUVSplit, TYPE_INT )
		FN_1(kAXSetBakeSmoothing, TYPE_INT, AXSetBakeSmoothing, TYPE_INT )
		FN_1(kAXSetCullUnusedDummies, TYPE_INT, AXSetCullUnusedDummies, TYPE_INT )
		FN_1(kAXSetDoSelectedGeometry, TYPE_INT, AXSetDoSelectedGeometry, TYPE_INT )

	END_FUNCTION_MAP

	void InitValues()
	{
		if( MaxInterfaceInitialized == 1)
		{
			return;
		}
		MaxInterfaceInitialized = 1;

		// Link the interface to the global plugin's  "TheMaxInterface". May be re-linked later to the utility plug-in's interface.
		SetOurMAXInterface( GetCOREInterface() ); // Get MAX interface

		AXBakeSmoothing =false;
		AXCullUnusedDummies = false;
		AXDoSelectedGeometry = false;
		AXTangentUVSplit = false;
		AXAllTextured = true;
		AXAllSkinType = true;
	}

	// Forcing of script-specific settings.
	void InitOptions()
	{
		//Call pseudo-constructor.
		InitValues();

		// Safeguard GUI options and obey GUI-independent settings for script
		BackupBakeSmoothing =OurScene.DoUnSmooth;
		BackupCullUnusedDummies = OurScene.DoCullDummies;
		BackupDoSelectedGeometry = OurScene.DoSelectedGeom;
		BackupTangentUVSplit = OurScene.DoTangents;
		BackupAllTextured = OurScene.DoTexGeom;
		BackupAllSkinType = OurScene.DoSkinGeom;

		BackupDoForceRate = OurScene.DoForceRate;

		OurScene.DoUnSmooth = AXBakeSmoothing;
		OurScene.DoCullDummies = AXCullUnusedDummies;
		OurScene.DoSelectedGeom = AXDoSelectedGeometry;
		OurScene.DoTangents = AXTangentUVSplit;
		OurScene.DoTexGeom = AXAllTextured;
		OurScene.DoSkinGeom = AXAllSkinType;

		//  Temporarily suppress all window popups - we're dealing with script-initiated, possibly batching tasks.
		 SavedAnimPopupState = OurScene.DoSuppressAnimPopups;
		OurScene.DoSuppressAnimPopups = true;

	}

	// Restore settings to their custom values.
	void ClearOptions()
	{
		OurScene.DoForceRate = BackupDoForceRate;
		OurScene.DoSuppressAnimPopups = SavedAnimPopupState;

		OurScene.DoUnSmooth = BackupBakeSmoothing;
		OurScene.DoCullDummies = BackupCullUnusedDummies;
		OurScene.DoSelectedGeom = BackupDoSelectedGeometry;
		OurScene.DoTangents = BackupTangentUVSplit;
		OurScene.DoTexGeom = BackupAllTextured;
		OurScene.DoSkinGeom = BackupAllSkinType;
	}


	//
	// Set the main path for saving all files and logfiles.
	//
	INT AXSetOutputPath( const TCHAR* newDestinationPathName )
	{

		INT AXSuccess  = 0;
		// If a valid path was specified, change the global output path to it.
		if( _tcslen( newDestinationPathName) > 0 ) // TODO - could use a better validity criterium...
		{
			_tcscpy(to_path,newDestinationPathName);
			_tcscpy(LogPath,newDestinationPathName);
			AXSuccess = 1;
		}
		return AXSuccess;
	}

	//
	// Export the reference mesh and skeletal pose to disk.
	//
	INT AXExportMesh( const TCHAR* newMeshFileName )
	{
		InitOptions();

		INT AXSuccess  = 1;

		// If name as supplied is valid, use it..
		if( _tcslen( newMeshFileName ) > 0 ) // TODO - could use a better validity criterium...
		{
			_tcscpy(to_skinfile, newMeshFileName );
		}
		else
		{
			PopupBox(_T("Error: No valid reference pose mesh file name specified."));
			AXSuccess = 0;
		}

		if( AXSuccess )
		{
			// Write the PSK file to disk using the current to_skinfile and to_pathname
			SaveCurrentSceneSkin();
		}

		ClearOptions();

		return AXSuccess;
	};

	//
	// Digest an animation into the internal memory buffer.
	//
	INT AXDigestAnim( const TCHAR* inputSequenceName, INT inStartFrame, INT inEndFrame, FLOAT optionalForcedRate )
	{
		InitOptions();

		INT AXSuccess = 0;

		// Specified sequence name ?
		if( inputSequenceName[0] )
		{
			UBOOL Backup_DoForceRate = OurScene.DoForceRate;

			_tcscpy( newsequencename, inputSequenceName );
			if( optionalForcedRate !=0.0f )
			{
				OurScene.DoForceRate = true;
				OurScene.PersistentRate = optionalForcedRate;
			}

			// Specified animation range?
			framerangestring[0] = 0; // default: whole range.
			if( (inStartFrame !=0) ||  (inEndFrame != 0) )
			{
				TCHAR ExRangeString[1024];
				_stprintf( ExRangeString,_T("%i-%i"),inStartFrame,inEndFrame );
				_tcscpy( framerangestring, ExRangeString ); // Framerangestring is the plugin's default range text when exporting a sequence.
			}

			DoDigestAnimation();
			OurScene.DoForceRate = Backup_DoForceRate;

			AXSuccess = 1;
		}

		ClearOptions();
		return AXSuccess;
	}

	//
	// Export the complete animation set to disk.
	//
	INT AXExportAnimSet( const TCHAR* newAnimFileName )
	{
		InitOptions();

		INT AXSuccess  = 1;

		// If name as supplied is valid, use it..
		if( newAnimFileName[0] )
		{
			_tcscpy(to_animfile, newAnimFileName );
		}
		else
		{
			PopupBox(_T("Error: No valid animation file name specified."));
			AXSuccess = 0;
		}

		if( AXSuccess )
		{
			// Copy _all_ digested animations to output data.  Respect any existing output data....
			for( INT i=0; i<TempActor.Animations.Num(); i++)
			{
				INT NewOutIdx = TempActor.OutAnims.AddZeroed(1);
				TempActor.OutAnims[ NewOutIdx ].AnimInfo = TempActor.Animations[i].AnimInfo;

				for( INT j=0; j<TempActor.Animations[i].KeyTrack.Num(); j++ )
				{
					TempActor.OutAnims[ NewOutIdx].KeyTrack.AddItem( TempActor.Animations[i].KeyTrack[j] );
				}
			}

			// Save animation - as done in Dialogs.cpp
			_stprintf(DestPath,_T("%s\\%s.psa"),to_path,to_animfile);
			SaveAnimSet( DestPath );    // No error popups...

			// Erase all the animation buffers.
			ClearOutAnims();
		}

		ClearOptions();
		return AXSuccess;
	};

	//
	// Various options accessors.
	//
	INT AXSetBakeSmoothing( INT inSwitch )
	{
		INT AXSuccess = 1;
		InitOptions();
		AXBakeSmoothing = ( inSwitch != 0 );
		ClearOptions();
		return AXSuccess;
	};

	INT AXSetAllSkinType( INT inSwitch )
	{
		INT AXSuccess = 1;
		InitOptions();
		AXAllSkinType = ( inSwitch != 0 );
		ClearOptions();
		return AXSuccess;
	};

	INT AXSetAllTextured( INT inSwitch )
	{
		INT AXSuccess = 1;
		InitOptions();
		AXAllTextured = ( inSwitch != 0 );
		ClearOptions();
		return AXSuccess;
	};

	INT AXSetTangentUVSplit( INT inSwitch )
	{
		INT AXSuccess = 1;
		InitOptions();
		AXTangentUVSplit = ( inSwitch != 0 );
		ClearOptions();
		return AXSuccess;
	};

	INT AXSetCullUnusedDummies( INT inSwitch )
	{
		INT AXSuccess = 1;
		InitOptions();
		AXCullUnusedDummies = ( inSwitch != 0 );
		ClearOptions();
		return AXSuccess;
	};

	INT AXSetDoSelectedGeometry( INT inSwitch )
	{
		INT AXSuccess = 1;
		InitOptions();
		AXDoSelectedGeometry = ( inSwitch != 0 );
		ClearOptions();
		return AXSuccess;
	};

};


// Instantiate the object which publishes the functions to the Max API.   Calling from max script will then look like  " AX.exportmesh() "  (no quotes.)

static AXMaxInterfaceFunctionPublish theAXMaxInterfaceFunctionPublish(
	AXMI_FP_INTERFACE_ID, _T("AX"), -1, 0, FP_CORE,
	// Order of arguments:  constantID,  command string as called from script,  -1, type of return value, 0, number of input vars, then number  times [  input var name,  -1 (?!), var type ]

	AXMaxInterfaceFunctionPublish::kAXSetOutputPath, _T("setoutputpath"), -1, TYPE_INT,  0, 1,
	_T("newDestinationPathName"),-1,TYPE_STRING,

	AXMaxInterfaceFunctionPublish::kAXExportMesh, _T("exportmesh"), -1, TYPE_VOID, 0, 1,
	_T("newMeshFilename"),-1,TYPE_STRING,

	AXMaxInterfaceFunctionPublish::kAXExportAnimSet, _T("exportanimset"), -1, TYPE_INT,  0, 1,
	_T("newAnimFilename"),-1,TYPE_STRING,

	AXMaxInterfaceFunctionPublish::kAXDigestAnim, _T("digestanim"), -1, TYPE_INT,  0, 4,
	_T("inputSequenceName"),0,TYPE_STRING,
	_T("inStartFrame"),0,TYPE_INT,
	_T("inEndFrame"),0,TYPE_INT,
	_T("optionalForcedRate"),0,TYPE_FLOAT,

	AXMaxInterfaceFunctionPublish::kAXSetAllSkinType, _T("setallskintype"), -1, TYPE_INT,  0, 1,
	_T("inSwitch"),-1,TYPE_INT,
	AXMaxInterfaceFunctionPublish::kAXSetAllTextured, _T("setalltextured"), -1, TYPE_INT,  0, 1,
	_T("inSwitch"),-1,TYPE_INT,
	AXMaxInterfaceFunctionPublish::kAXSetTangentUVSplit, _T("settangentuvsplit"), -1, TYPE_INT,  0, 1,
	_T("inSwitch"),-1,TYPE_INT,
	AXMaxInterfaceFunctionPublish::kAXSetBakeSmoothing, _T("setbakesmoothing"), -1, TYPE_INT,  0, 1,
	_T("inSwitch"),-1,TYPE_INT,
	AXMaxInterfaceFunctionPublish::kAXSetCullUnusedDummies, _T("setcullunuseddummies"), -1, TYPE_INT,  0, 1,
	_T("inSwitch"),-1,TYPE_INT,
	AXMaxInterfaceFunctionPublish::kAXSetDoSelectedGeometry, _T("setselectedgeometry"), -1, TYPE_INT,  0, 1,
	_T("inSwitch"),-1,TYPE_INT,

#if MAX_PRODUCT_YEAR_NUMBER >= 2013
p_end
#else
end
#endif
);


//
//	DllEntry - the MAX Dll interface code.
//

INT_PTR CALLBACK SceneInfoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ActorManagerDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

extern ClassDesc* GetActorXDesc();
extern MaxPluginClass* GlobalPluginObject;
extern WinRegistry PluginReg;

HINSTANCE ParentApp_hInstance;
int controlsInit = FALSE;

TCHAR *GetString(int id)
{
	static TCHAR buf[256];

	if (ParentApp_hInstance)
		return LoadString(ParentApp_hInstance, id, buf, sizeof(buf)) ? buf : NULL;
	return NULL;
}


// This function is called by Windows when the DLL is loaded.  This
// function may also be called many times during time critical operations
// like rendering.  Therefore developers need to be careful what they
// do inside this function.  In the code below, note how after the DLL is
// loaded the first time only a few statements are executed.

BOOL WINAPI DllMain( HINSTANCE hinstDLL,ULONG fdwReason,LPVOID lpvReserved )
{
	ParentApp_hInstance = hinstDLL;				// Hang on to this DLL's instance handle.

#if !defined(MAX_PRODUCT_YEAR_NUMBER) || (MAX_PRODUCT_YEAR_NUMBER < 2011)
	if (!controlsInit) {
		controlsInit = TRUE;
		InitCustomControls(ParentApp_hInstance);	// Initialize MAX's custom controls
		InitCommonControls();			// Initialize Win95 controls
	}
#endif
	return (TRUE);
}

// This function returns a string that describes the DLL and where the user
// could purchase the DLL if they don't have it.
__declspec( dllexport ) const TCHAR* LibDescription()
{
	return GetString(IDS_LIBDESCRIPTION);
}

// This function returns the number of plug-in classes this DLL
// TODO: Must change this number when adding a new class
__declspec( dllexport ) int LibNumberClasses()
{
	return 1;
}

// This function returns the number of plug-in classes this DLL
__declspec( dllexport ) ClassDesc* LibClassDesc(int i)
{
	switch(i) {
		case 0: return GetActorXDesc();
		default: return 0;
	}
}

// This function returns a pre-defined constant indicating the version of
// the system under which it was compiled.  It is used to allow the system
// to catch obsolete DLLs.
__declspec( dllexport ) ULONG LibVersion()
{
	return VERSION_3DSMAX;
}

// This function is called once, right after your plugin has been loaded by 3ds Max.
// Perform one-time plugin initialization in this method.
// Return TRUE if you deem your plugin successfully loaded, or FALSE otherwise. If
// the function returns FALSE, the system will NOT load the plugin, it will then call FreeLibrary
// on your DLL, and send you a message.
__declspec( dllexport ) int LibInitialize(void)
{
	return TRUE; // TODO: Perform initialization here.
}

// This function is called once, just before the plugin is unloaded.
// Perform one-time plugin un-initialization in this method."
// The system doesn't pay attention to a return value.
__declspec( dllexport ) int LibShutdown(void)
{
	return TRUE;// TODO: Perform un-initialization here.
}

// Dummy function necessary for status bar update
DWORD WINAPI fn(LPVOID arg)
{
	return(0);
}

ClassDesc* GetActorXDesc() {return &ActorXDesc;}

//TODO: Should implement this method to reset the plugin params when Max is reset
void ActorXClassDesc::ResetClassParams (BOOL fileReset)
{
	ResetPlugin();
}


static INT_PTR CALLBACK ActorXDlgProc(	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	// Get object ptr from this window's user area.
#pragma warning( suppress : 4312 )	// Avoid Windows SDK bug in 32-bit:  warning C4312: 'type cast' : conversion from 'LONG' to 'void *' of greater size
	MaxPluginClass *u = (MaxPluginClass *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!u && msg != WM_INITDIALOG ) return FALSE;
	GlobalPluginObject = u;

	switch (msg)
	{
		case WM_INITDIALOG:
			// Store the object pointer into window's user data area.
			u = (MaxPluginClass *)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, ( LONG_PTR )u);
			SetOurMAXInterface(u->ip);
			GlobalPluginObject = u;

			// Initialize this panel's custom controls if any.
			ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ATIME,IDC_SPIN_ATIME,0,0,1000);

			// Initialize all non-empty edit windows.
			if ( to_path[0]     ) SetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path);
			if ( to_skinfile[0] ) SetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile);
			if ( to_animfile[0] ) PrintWindowString(hWnd,IDC_ANIMFILENAME,to_animfile);
			if ( newsequencename[0]    ) PrintWindowString(hWnd,IDC_ANIMNAME,newsequencename);
			if ( framerangestring[0]  ) PrintWindowString(hWnd,IDC_ANIMRANGE,framerangestring);

			break;

		case WM_DESTROY:
			// Release this panel's custom controls.
			ThisMaxPlugin.DestroySpinner(hWnd,IDC_SPIN_ATIME);
			break;

		//case WM_MOUSEMOVE:
		//	ThisMaxPlugin.ip->RollupMouseMessage(hWnd,msg,wParam,lParam);
		//	break;

		case WM_COMMAND:

			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					EnableAccelerators();
					break;
			}

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam))
			{

			case IDC_ANIMNAME:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							//PopupBox("EDIT BOX KILLFOCUS");
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMNAME),newsequencename,100);
						}
						break;
					};
				}
				break;

			case IDC_ANIMRANGE:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							//PopupBox("EDIT BOX KILLFOCUS");
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMRANGE),framerangestring,500);
						}
						break;
					};
				}
				break;

			case IDC_PATH:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update path.
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path,300);
							_tcscpy(LogPath,to_path);
							PluginReg.SetKeyString(_T("TOPATH"), to_path );
						}
						break;
					};
				}
				break;

			case IDC_SKINFILENAME:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							//PopupBox("EDIT BOX KILLFOCUS");
							GetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile,300);
							PluginReg.SetKeyString(_T("TOSKINFILE"), to_skinfile );
						}
						break;
					};
				}
				break;

			case IDC_ANIMFILENAME:
				{
					switch( HIWORD(wParam) )
					{
						// whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMFILENAME),to_animfile,300);
							PluginReg.SetKeyString(_T("TOANIMFILE"), to_animfile );
						}
						break;
					};
				}
				break;


			case IDC_ANIMMANAGER: // Animation manager activated.
				{
					u->ShowActorManager(hWnd);
				}
				break;

			// Browse for a destination directory.
			case IDC_BROWSE:
				{
					TCHAR dir[MAX_PATH];
					TCHAR desc[MAX_PATH];
					_tcscpy(desc, _T("Target Directory"));
					u->ip->ChooseDirectory(u->ip->GetMAXHWnd(),_T("Choose Output"), dir, desc);
					SetWindowText(GetDlgItem(hWnd,IDC_PATH),dir);
					_tcscpy(to_path,dir);
					_tcscpy(LogPath,dir);
					PluginReg.SetKeyString(_T("TOPATH"), to_path );
				}
				break;


			// A click inside the logo.

			case IDC_LOGOUNREAL:
				{
					// Show our About box.
					//u->ShowAbout(hWnd);
					//CenterWindow(hWnd, GetParent(hWnd));
				}
			break;


			// DO it.
			case IDC_EVAL:
				{
					// Survey the scene, to conclude which items to export or evaluate, if any.
					// Logic: go for the deepest hierarchy; if it has a physique skin, we have
					// a full skeleton+skin, otherwise we assume a hierarchical object - which
					// can have max bones, though.
#if 1
					//DebugBox("Start surveying the scene.");
					OurScene.SurveyScene();

					// Initializes the scene tree.
						DebugBox("Start getting scene info");
					OurScene.GetSceneInfo();

						DebugBox("Start evaluating Skeleton");
					OurScene.EvaluateSkeleton(1);
					/////////////////////////////////////
					u->ShowSceneInfo(hWnd);
#endif
				}
				break;


			case IDC_SAVESKIN:
				{
					SaveCurrentSceneSkin();
				}
				break;

			case IDC_DIGESTANIM:
				{
					DoDigestAnimation();
				}
				break;
			}
			break;
			// END command

		default:
			return FALSE;
	}
	return TRUE;
}

static INT_PTR CALLBACK AuxDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Get our plugin object pointer from the window's user data area.
#pragma warning( suppress : 4312 )	// Avoid Windows SDK bug in 32-bit:  warning C4312: 'type cast' : conversion from 'LONG' to 'void *' of greater size
	MaxPluginClass *u = (MaxPluginClass *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!u && msg != WM_INITDIALOG ) return FALSE;
	GlobalPluginObject = u;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			// Store the object pointer into window's user data area.
			u = (MaxPluginClass *)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, ( LONG_PTR )u);
			SetOurMAXInterface(u->ip);
			GlobalPluginObject = u;

			// Optional persistence: retrieve setting from registry.
			INT SwitchPersistent;
			PluginReg.GetKeyValue(_T("PERSISTSETTINGS"), SwitchPersistent);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS,SwitchPersistent?1:0);

			INT SwitchPersistPaths;
			PluginReg.GetKeyValue(_T("PERSISTPATHS"), SwitchPersistPaths);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTPATHS,SwitchPersistPaths?1:0);


			// Make sure defaults are updated when boxes first appear.
			_SetCheckBox( hWnd, IDC_LOCKROOTX, OurScene.DoFixRoot);
			_SetCheckBox( hWnd, IDC_POSEZERO, OurScene.DoPoseZero);

			_SetCheckBox( hWnd, IDC_CHECKNOLOG, !OurScene.DoLog);
			_SetCheckBox( hWnd, IDC_CHECKDOUNSMOOTH, OurScene.DoUnSmooth);
			_SetCheckBox( hWnd, IDC_CHECKDOTANGENTS, OurScene.DoTangents);

			_SetCheckBox( hWnd, IDC_CHECKDOEXPORTSCALE, OurScene.DoExportScale);

			_SetCheckBox( hWnd, IDC_CHECKSKINALLT, OurScene.DoTexGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINALLP, OurScene.DoSkinGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINALLS, OurScene.DoSelectedGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINNOS,  OurScene.DoSkipSelectedGeom );
			_SetCheckBox( hWnd, IDC_CHECKTEXPCX,   OurScene.DoTexPCX );
			_SetCheckBox( hWnd, IDC_CHECKQUADTEX,  OurScene.DoQuadTex );
			_SetCheckBox( hWnd, IDC_CHECKCULLDUMMIES, OurScene.DoCullDummies );
			_SetCheckBox( hWnd, IDC_EXPLICIT, OurScene.DoExplicitSequences );

			if ( basename[0]  ) PrintWindowString(hWnd,IDC_EDITBASE,basename);
			if ( classname[0] ) PrintWindowString(hWnd,IDC_EDITCLASS,classname);

			ToggleControlEnable(hWnd,IDC_CHECKSKIN1,true);
			ToggleControlEnable(hWnd,IDC_CHECKSKIN2,true);

			// Start Lockroot disabled
			ToggleControlEnable(hWnd,IDC_LOCKROOTX,false);

			// Initialize this panel's custom controls, if any.
			ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ACOMP,IDC_SPIN_ACOMP,100,0,100);
		}
			break;

		case WM_DESTROY:
			// Release this panel's custom controls.
			ThisMaxPlugin.DestroySpinner(hWnd,IDC_SPIN_ACOMP);
			break;

		//case WM_LBUTTONDOWN:
		//case WM_LBUTTONUP:
		//case WM_MOUSEMOVE:
		//	ThisMaxPlugin.ip->RollupMouseMessage(hWnd,msg,wParam,lParam);
		//	break;

		case WM_COMMAND:
			// Needed to have keyboard focus on this window!
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					EnableAccelerators();
					break;
			};

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam))
			{
				// get the values of the toggles
				case IDC_CHECKPERSISTSETTINGS:
				{
					//Read out the value...
					OurScene.CheckPersistSettings = GetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS);
					PluginReg.SetKeyValue(_T("PERSISTSETTINGS"), OurScene.CheckPersistSettings);
				}
				break;

				case IDC_CHECKPERSISTPATHS:
				{
					//Read out the value...
					OurScene.CheckPersistPaths = GetCheckBox(hWnd,IDC_CHECKPERSISTPATHS);
					PluginReg.SetKeyValue(_T("PERSISTPATHS"), OurScene.CheckPersistPaths);
				}
				break;


				case IDC_FIXROOT:
				{
					OurScene.DoFixRoot = GetCheckBox(hWnd,IDC_FIXROOT);
					PluginReg.SetKeyValue(_T("DOFIXROOT"),OurScene.DoFixRoot );

					//ToggleControlEnable(hWnd,IDC_LOCKROOTX,OurScene.DoFixRoot);
					if (!OurScene.DoFixRoot)
					{
						_SetCheckBox(hWnd,IDC_LOCKROOTX,false);
						ToggleControlEnable(hWnd,IDC_LOCKROOTX,false);
					}
					else
						ToggleControlEnable(hWnd,IDC_LOCKROOTX,true);
					// HWND hDlg = GetDlgItem(hWnd,control);
					// EnableWindow(hDlg,FALSE);
					// see also MSDN  SDK info, Buttons/ using buttons etc
				}
				break;


				case IDC_POSEZERO:
				{
					OurScene.DoPoseZero = GetCheckBox(hWnd,IDC_POSEZERO);
					//ToggleControlEnable(hWnd,IDC_LOCKROOTX,OurScene.DoPOSEZERO);
					if (!OurScene.DoPoseZero)
					{
						_SetCheckBox(hWnd,IDC_LOCKROOTX,false);
						ToggleControlEnable(hWnd,IDC_LOCKROOTX,false);
					}
					else
						ToggleControlEnable(hWnd,IDC_LOCKROOTX,true);
					// HWND hDlg = GetDlgItem(hWnd,control);
					// EnableWindow(hDlg,FALSE);
					// see also MSDN  SDK info, Buttons/ using buttons etc
				}
				break;

				case IDC_LOCKROOTX:
				{
					//Read out the value...
					OurScene.DoLockRoot = GetCheckBox(hWnd,IDC_LOCKROOTX);
				}
				break;

				case IDC_CHECKNOLOG:
				{
					OurScene.DoLog = ! GetCheckBox(hWnd,IDC_CHECKNOLOG);
					PluginReg.SetKeyValue(_T("DOLOG"),OurScene.DoLog);
				}
				break;

				case IDC_CHECKDOUNSMOOTH:
				{
					OurScene.DoUnSmooth = GetCheckBox(hWnd,IDC_CHECKDOUNSMOOTH);
					PluginReg.SetKeyValue(_T("DOUNSMOOTH"),OurScene.DoUnSmooth);
				}
				break;

				case IDC_CHECKDOEXPORTSCALE:
				{
					OurScene.DoExportScale = GetCheckBox(hWnd,IDC_CHECKDOEXPORTSCALE);
					PluginReg.SetKeyValue(_T("DOEXPORTSCALE"),OurScene.DoExportScale);
				}
				break;


				case IDC_CHECKDOTANGENTS:
				{
					OurScene.DoTangents = GetCheckBox(hWnd,IDC_CHECKDOTANGENTS);
					PluginReg.SetKeyValue(_T("DOTANGENTS"),OurScene.DoTangents);
				}
				break;


				case IDC_CHECKSKINALLT: // all textured geometry skin checkbox
				{
					OurScene.DoTexGeom = GetCheckBox(hWnd,IDC_CHECKSKINALLT);
					PluginReg.SetKeyValue(_T("DOTEXGEOM"),OurScene.DoTexGeom );
					//if (!OurScene.DoTexGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLT,false);
				}
				break;

				case IDC_CHECKTEXPCX: // all textured geometry skin checkbox
				{
					OurScene.DoTexPCX = GetCheckBox(hWnd,IDC_CHECKTEXPCX);
					PluginReg.SetKeyValue(_T("DOPCX"),OurScene.DoTexPCX); // update registry
				}
				break;

				case IDC_CHECKQUADTEX: // all textured geometry skin checkbox
				{
					OurScene.DoQuadTex = GetCheckBox(hWnd,IDC_CHECKQUADTEX);
				}
				break;

				case IDC_CHECKSKINALLS: // all selected meshes skin checkbox
				{
					OurScene.DoSelectedGeom = GetCheckBox(hWnd,IDC_CHECKSKINALLS);
					PluginReg.SetKeyValue(_T("DOSELGEOM"),OurScene.DoSelectedGeom);
					//if (!OurScene.DoSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLS,false);
				}
				break;

				case IDC_CHECKSKINALLP: // all selected physique meshes checkbox
				{
					OurScene.DoSkinGeom = GetCheckBox(hWnd,IDC_CHECKSKINALLP);
					PluginReg.SetKeyValue(_T("DOPHYSGEOM"), OurScene.DoSkinGeom );
					//if (!OurScene.DoSkinGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLP,false);
				}
				break;

				case IDC_CHECKSKINNOS: // all none-selected meshes skin checkbox..
				{
					OurScene.DoSkipSelectedGeom = GetCheckBox(hWnd,IDC_CHECKSKINNOS);
					PluginReg.SetKeyValue(_T("DOSKIPSEL"),OurScene.DoSkipSelectedGeom );
					//if (!OurScene.DoSkipSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINNOS,false);
				}
				break;

				case IDC_CHECKCULLDUMMIES: // all selected physique meshes checkbox
				{
					OurScene.DoCullDummies = GetCheckBox(hWnd,IDC_CHECKCULLDUMMIES);
					PluginReg.SetKeyValue(_T("DOCULLDUMMIES"),OurScene.DoCullDummies );
					//ToggleControlEnable(hWnd,IDC_CHECKCULLDUMMIES,OurScene.DoCullDummies);
					//if (!OurScene.DoCullDummies) _SetCheckBox(hWnd,IDC_CHECKCULLDUMMIES,false);
				}
				break;

				case IDC_EXPLICIT:
				{
					OurScene.DoExplicitSequences = GetCheckBox(hWnd,IDC_EXPLICIT);
					PluginReg.SetKeyValue(_T("DOEXPSEQ"),OurScene.DoExplicitSequences);
				}
				break;

				case IDC_SAVESCRIPT: // save the script
				{
					if( TempActor.OutAnims.Num() == 0 )
					{
						PopupBox(_T("Warning: no animations present in output package.\n Aborting template generation."));
					}
					else
					{
						if( OurScene.WriteScriptFile( &TempActor, classname, basename, to_skinfile, to_animfile ) )
						{
							PopupBox(_T(" Script template file written: %s "),classname);
						}
						else
						{
							PopupBox(_T(" Error trying to write script template file [%s] to path [%s]"),classname,to_path) ;
						}
					}
				}
				break;

				// Load, parse and execute a batch-export list.
				case IDC_BATCHLIST:
				{
					// Load
					TCHAR batchfilename[300];
					_tcscpy(batchfilename,_T("XX"));
				    GetBatchFileName( hWnd, batchfilename, to_path);
					if( batchfilename[0] ) // Nonzero string = OK name.
					{
						// Parse and execute..
						// ExecuteBatchFile( batchfilename, OurScene );
						PopupBox(_T(" Batch file [%s] processed."),batchfilename );
					}
				}
				break;

				// Get all max files from selectable folder and digest them all into the animation manager.
				case IDC_BATCHMAX:
				{
					TCHAR dir[300];
					TCHAR desc[300];
					_tcscpy(desc, _T("Maxfiles source folder"));
					u->ip->ChooseDirectory(u->ip->GetMAXHWnd(),_T("Choose Folder"), dir, desc);
					_tcscpy(batchfoldername,dir);

					INT NumDigested = 0;
					INT NumFound = 0;

					BatchAnimationProcess( batchfoldername, NumDigested, NumFound, u );

					PopupBox(_T(" Digested %i animation files of %i from folder[ %s ]"),NumDigested,NumFound,batchfoldername);
				}
				break;

				case IDC_EDITCLASS:
				{
					switch( HIWORD(wParam) )
					{
						// whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							//PopupBox("EDIT BOX KILLFOCUS");
							GetWindowText( GetDlgItem(hWnd,IDC_EDITCLASS),classname,100 );
							PluginReg.SetKeyString(_T("CLASSNAME"), classname );
						}
						break;
					};
				}
				break;

				case IDC_EDITBASE:
				{
					switch( HIWORD(wParam) )
					{
						// whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							//PopupBox("EDIT BOX KILLFOCUS");
							GetWindowText(GetDlgItem(hWnd,IDC_EDITBASE),basename,100);
							PluginReg.SetKeyString(_T("BASENAME"), basename );
						}
						break;
					};
				}
				break;

			};
			break;

		default:
		return FALSE;
	}
	return TRUE;
}



static INT_PTR CALLBACK VertexDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Get our plugin object pointer from the window's user data area.
#pragma warning( suppress : 4312 )	// Avoid Windows SDK bug in 32-bit:  warning C4312: 'type cast' : conversion from 'LONG' to 'void *' of greater size
	MaxPluginClass *u = (MaxPluginClass *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!u && msg != WM_INITDIALOG ) return FALSE;
	GlobalPluginObject = u;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			// Store the object pointer into window's user data area.
			u = (MaxPluginClass *)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)u);
			SetOurMAXInterface(u->ip);
			GlobalPluginObject = u;

			_SetCheckBox( hWnd, IDC_CHECKAPPENDVERTEX, OurScene.DoAppendVertex );
			_SetCheckBox( hWnd, IDC_CHECKFIXEDSCALE,   OurScene.DoScaleVertex );

			if ( OurScene.DoScaleVertex && (OurScene.VertexExportScale != 0.f) )
			{
				TCHAR ScaleString[64];
				_stprintf(ScaleString,_T("%8.5f"),OurScene.VertexExportScale );
				PrintWindowString(hWnd, IDC_EDITFIXEDSCALE, ScaleString );
			}
			else
			{
				OurScene.VertexExportScale = 1.0f;
			}


			if ( vertframerangestring[0]  ) PrintWindowString(hWnd,IDC_EDITVERTEXRANGE,vertframerangestring);

			// Initialize this panel's custom controls, if any.
			// ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ACOMP,IDC_SPIN_ACOMP,100,0,100);
		}
			break;

		case WM_DESTROY:
			// Release this panel's custom controls.
			ThisMaxPlugin.DestroySpinner(hWnd,IDC_SPIN_ACOMP);
			break;


		case WM_COMMAND:
			// Needed to have keyboard focus on this window!
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					EnableAccelerators();
					break;
			};

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam))
			{

				// Browse for a destination directory.
				case IDC_BROWSEVTX:
				{
					TCHAR dir[MAX_PATH];
					TCHAR desc[MAX_PATH];
					_tcscpy(desc, _T("Target Directory"));
					u->ip->ChooseDirectory(u->ip->GetMAXHWnd(),_T("Choose Output"), dir, desc);
					SetWindowText(GetDlgItem(hWnd,IDC_PATHVTX),dir);
					_tcscpy(to_pathvtx,dir);
					_tcscpy(LogPath,dir);
					PluginReg.SetKeyString(_T("TOPATHVTX"), to_pathvtx );
				}
				break;


				case IDC_PATHVTX:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update path.
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_PATHVTX),to_pathvtx,300);
							_tcscpy(LogPath,to_pathvtx);
							PluginReg.SetKeyString(_T("TOPATHVTX"), to_pathvtx );
						}
						break;
					};
				}
				break;

				case IDC_FILENAMEVTX:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_FILENAMEVTX),to_skinfilevtx,300);
							PluginReg.SetKeyString(_T("TOSKINFILEVTX"), to_skinfilevtx );
						}
						break;
					};
				}
				break;

				case IDC_EDITVERTEXRANGE:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_EDITVERTEXRANGE),vertframerangestring,500);
						}
						break;
					};
				}
				break;

				case IDC_EXPORTVERTEX: // export vertex-animated data (raw discretized data, doesn't do auto-scaling )
				{
					OurScene.SurveyScene();
					OurScene.GetSceneInfo();

					OurScene.EvaluateSkeleton(1);
					OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

					INT WrittenFrameCount = OurScene.WriteVertexAnims( &TempActor, to_skinfilevtx, vertframerangestring );

					if( WrittenFrameCount ) // Calls DigestSkin
					{

						if( ! OurScene.DoAppendVertex )
								PopupBox(_T(" Vertex animation export successful. \n Written %i frames to file [%s]. "),WrittenFrameCount,to_skinfilevtx);
							else
								PopupBox(_T(" Vertex animation export successful. \n Appended %i frames to file [%s]. "),WrittenFrameCount,to_skinfilevtx);
					}
					else
					{
						PopupBox(_T(" Error encountered writing vertex animation data. "));
					}
				}
				break;

				case IDC_CHECKAPPENDVERTEX:
				{
					OurScene.DoAppendVertex = _GetCheckBox(hWnd,IDC_CHECKAPPENDVERTEX);
					PluginReg.SetKeyValue(_T("DOAPPENDVERTEX"), OurScene.DoAppendVertex);
				}
				break;

				case IDC_CHECKFIXEDSCALE:
				{
					OurScene.DoScaleVertex = _GetCheckBox(hWnd,IDC_CHECKFIXEDSCALE);
					PluginReg.SetKeyValue(_T("DOSCALEVERTEX"), OurScene.DoScaleVertex);
				}
				break;


			};
			break;

		default:
		return FALSE;
	}
	return TRUE;
}



static INT_PTR CALLBACK AboutBoxDlgProc(HWND hWnd, UINT msg,
	WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		//CenterWindow(hWnd, GetParent(hWnd));
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDOK:
			EndDialog(hWnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
		break;

		}
		break;
		default:
			return FALSE;
	}
	return TRUE;
}


/*
void MaxPluginClass::ShowAbout(HWND hWnd)
{
	DialogBoxParam(ParentApp_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutBoxDlgProc, 0);
	//CenterWindow(hWnd, GetParent(hWnd));
}
*/

void MaxPluginClass::ShowActorManager( HWND hWnd )
{
	DialogBoxParam(ParentApp_hInstance, MAKEINTRESOURCE(IDD_ACTORMANAGER), hWnd, ActorManagerDlgProc, 0);
}

//
// ActorX utility class
//

// Constructor
MaxPluginClass::MaxPluginClass()
{
	iu = NULL;
	ip = NULL;
	hMainPanel = NULL;
	hAuxPanel  = NULL;
	hVertPanel = NULL;

	ResetPlugin();
}

// Destructor
MaxPluginClass::~MaxPluginClass()
{
	GlobalPluginObject = NULL;
}

void MaxPluginClass::BeginEditParams(Interface *ip,IUtil *iu)
{
	this->iu = iu;
	this->ip = ip;

	// Create the Main Rollup page in the utility panel.
	hMainPanel = ip->AddRollupPage(
		ParentApp_hInstance,
		MAKEINTRESOURCE(IDD_PANEL),
		ActorXDlgProc,
		_T("Actor X - Epic Games"),//GetString(IDS_PARAMS),
		(LPARAM)this);

	// Create auxiliary rollup page (exporter settings)
	hAuxPanel = ip->AddRollupPage(
		ParentApp_hInstance,
		MAKEINTRESOURCE(IDD_AUXPANEL),
		AuxDlgProc,
		_T("Actor X - Setup"),//GetString(IDS_PARAMS),
		(LPARAM)this);

	// Create vertex rollup page
	hVertPanel = ip->AddRollupPage(
		ParentApp_hInstance,
		MAKEINTRESOURCE(IDD_VERTEX),
		VertexDlgProc,
		_T("Actor X - Vertex Export"),//GetString(IDS_PARAMS),
		(LPARAM)this);


	/*  - Outcommented - as suggested by Steve Cordon - crashes on certain Max4.2/cs32 setups

	// Start the aux panel in closed state.
	HWND MainPanel = GetParent(GetParent(GetParent(hMainPanel)));
	IRollupWindow *rw = GetIRollup(MainPanel);
	rw->SetPanelOpen(2, false); //2 *appears* to be the aux rollup, 1 the main rollup.
	ReleaseIRollup(rw);

	*/
}

void MaxPluginClass::EndEditParams(Interface *ip,IUtil *iu)
{
	this->iu = NULL;
	this->ip = NULL;
	ip->DeleteRollupPage(hMainPanel);
	ip->DeleteRollupPage(hAuxPanel);
	ip->DeleteRollupPage(hVertPanel);
	hMainPanel = NULL;
	hAuxPanel = NULL;
	hVertPanel = NULL;
}

//
// False = greyed out.
//
void ToggleControlEnable( HWND hWnd, int control, BOOL state )
{
	HWND hDlg = GetDlgItem(hWnd,control);
	EnableWindow(hDlg,state);
}

void MaxPluginClass::InitSpinner( HWND hWnd, int ed, int sp, int v, int minv, int maxv )
{
	ISpinnerControl	*blendspin = GetISpinner(GetDlgItem(hWnd,sp));
	if (blendspin) {
		blendspin->LinkToEdit(GetDlgItem(hWnd,ed),EDITTYPE_INT);
		blendspin->SetLimits(minv,maxv,FALSE);
		blendspin->SetValue(v,FALSE);
	}
}

void MaxPluginClass::DestroySpinner( HWND hWnd, int sp )
{
	ISpinnerControl	*blendspin = GetISpinner(GetDlgItem(hWnd,sp));
	if (blendspin)
		ReleaseISpinner(blendspin);
}

int MaxPluginClass::GetSpinnerValue( HWND hWnd, int sp )
{
	ISpinnerControl	*blendspin = GetISpinner(GetDlgItem(hWnd,sp));
	if (blendspin)
		return (blendspin->GetIVal());
	else
		return 0;
}


void MaxPluginClass::ShowSceneInfo(HWND hWnd)
{
	DialogBoxParam(ParentApp_hInstance, MAKEINTRESOURCE(IDD_SCENEINFO), hWnd, SceneInfoDlgProc, 0);
}


//
// Unreal flags from materials (original material tricks by Steve Sinclair, Digital Extremes )
// - uses classic Unreal/UT flag conventions for now.
// Determine the polyflags, in case a multimaterial is present
// reference the matID value (pass in the face's matID).
//

void CalcMaterialFlags(Mtl* std, VMaterial& Stuff )
{
	BOOL	two=FALSE;
	BOOL	translucent=FALSE;
	BOOL	weapon=FALSE;
	BOOL	unlit=FALSE;
	BOOL	enviro=FALSE;
	BOOL	nofiltering=FALSE;
	BOOL	modulate=FALSE;
	BOOL	masked=FALSE;
	BOOL	flat=FALSE;
	BOOL    alpha=FALSE;

	// char*	buf;

	// Material properties from Max' material properties: enable later ? Confused people when using tags also.

	/*
	//float	selfIllum=0.0f;
	//float	opacity=1.0f;
	//float	utile, vtile;
	// See if it's a true standard material

	if (std->ClassID() == Class_ID(DMTL_CLASS_ID, 0))
	{
		StdMat* m = (StdMat *)std;
		two = m->GetTwoSided();
		// Access the Diffuse map and see if it's a Bitmap texture
		Texmap *tmap = m->GetSubTexmap(ID_DI);
		if (tmap && tmap->ClassID() == Class_ID(BMTEX_CLASS_ID, 0))
		{
			// It is -- Access the UV tiling settings at time 0.
			BitmapTex *bmt = (BitmapTex*) tmap;
			if ( bmt->GetFilterType()==FILTER_NADA )
				nofiltering = TRUE;
			StdUVGen *uv = bmt->GetUVGen();

			// if (uv->GetCoordMapping(0)!=UVMAP_EXPLICIT)
			// 	enviro = TRUE;
			utile = uv->GetUScl(0);
			vtile = uv->GetVScl(0);
		}

		Color ambColor = m->GetAmbient(0);
		//if (color.r==color.g==color.b
		opacity = m->GetOpacity(0);
		if (opacity<1.0f)
			translucent=TRUE;

		selfIllum = m->GetSelfIllum(0);
		if (selfIllum>0.0f)
			unlit=TRUE;
	}
	*/

	TSTR pName = std->GetName();

	// Check skin index
	INT skinIndex = FindValueTag( pName, _T("skin") ,2 ); // Double (or single) digit skin index.......
	if ( skinIndex < 0 )
	{
		skinIndex = 0;
		Stuff.AuxFlags = 0;
	}
	else
		Stuff.AuxFlags = 1; // Indicates an explicitly defined skin index.

	Stuff.TextureIndex = skinIndex;

	// Lodbias
	INT LodBias = FindValueTag( pName, _T("lodbias"),2);
	if ( LodBias < 0 ) LodBias = 5; // Default value when tag not found.
	Stuff.LodBias = LodBias;

	// LodStyle
	INT LodStyle = FindValueTag( pName, _T("lodstyle"),2);
	if ( LodStyle < 0 ) LodStyle = 0; // Default LOD style.
	Stuff.LodStyle = LodStyle;

	// Substrings in arbitrary positions enforce flags.
	if (CheckSubString(pName,_T("twosid")))    two=true;
	if (CheckSubString(pName,_T("doublesid"))) two=true;
	if (CheckSubString(pName,_T("weapon"))) weapon=true;
	if (CheckSubString(pName,_T("modul"))) modulate=true;
	if (CheckSubString(pName,_T("mask")))  masked=true;
	if (CheckSubString(pName,_T("flat")))  flat=true;
	if (CheckSubString(pName,_T("envir"))) enviro=true;
	if (CheckSubString(pName,_T("mirro")))  enviro=true;
	if (CheckSubString(pName,_T("nosmo")))  nofiltering=true;
	if (CheckSubString(pName,_T("unlit")))  unlit=true;
	if (CheckSubString(pName,_T("bright"))) unlit=true;
	if (CheckSubString(pName,_T("trans")))  translucent=true;
	if (CheckSubString(pName,_T("opaque"))) translucent=false;
	if (CheckSubString(pName,_T("alph"))) alpha=true;

	BYTE MatFlags= MTT_Normal;

	if (two)
		MatFlags|= MTT_NormalTwoSided;

	if (translucent)
		MatFlags|= MTT_Translucent;

	if (masked)
		MatFlags|= MTT_Masked;

	if (modulate)
		MatFlags|= MTT_Modulate;

	if (unlit)
		MatFlags|= MTT_Unlit;

	if (flat)
		MatFlags|= MTT_Flat;

	if (enviro)
		MatFlags|= MTT_Environment;

	if (nofiltering)
		MatFlags|= MTT_NoSmooth;

	if (alpha)
		MatFlags|= MTT_Alpha;

	if (weapon)
		MatFlags= MTT_Placeholder;

	Stuff.PolyFlags = MatFlags;


	/*
	// Triangle types. Pick ONE AND ONLY ONE of these.
	MTT_Normal				= 0x00,	// Normal one-sided.
	MTT_NormalTwoSided      = 0x01, // Normal but two-sided.
	MTT_Translucent			= 0x02,	// Translucent two-sided.
	MTT_Masked				= 0x03,	// Masked two-sided.
	MTT_Modulate			= 0x04,	// Modulation blended two-sided.
	MTT_Placeholder			= 0x08,	// Placeholder triangle for positioning weapon. Invisible.

	// Bit flags. Add any of these you want.
	MTT_Unlit				= 0x10,	// Full brightness, no lighting.
	MTT_Flat				= 0x20,	// Flat surface, don't do bMeshCurvy thing.
	MTT_Environment			= 0x40,	// Environment mapped.
	MTT_NoSmooth			= 0x80,	// No bilinear filtering on this poly's texture

  	// Regular in-game flags.
	PF_Invisible		= 0x00000001,	// Poly is invisible.
	PF_Masked			= 0x00000002,	// Poly should be drawn masked.
	PF_Translucent	 	= 0x00000004,	// Poly is transparent.
	PF_NotSolid			= 0x00000008,	// Poly is not solid, doesn't block.
	PF_Environment   	= 0x00000010,	// Poly should be drawn environment mapped.
	PF_ForceViewZone	= 0x00000010,	// Force current iViewZone in OccludeBSP (reuse Environment flag)
	PF_Semisolid	  	= 0x00000020,	// Poly is semi-solid = collision solid, Csg nonsolid.
	PF_Modulated 		= 0x00000040,	// Modulation transparency.
	PF_FakeBackdrop		= 0x00000080,	// Poly looks exactly like backdrop.
	PF_TwoSided			= 0x00000100,	// Poly is visible from both sides.
	PF_AutoUPan		 	= 0x00000200,	// Automatically pans in U direction.
	PF_AutoVPan 		= 0x00000400,	// Automatically pans in V direction.
	PF_NoSmooth			= 0x00000800,	// Don't smooth textures.
	PF_BigWavy 			= 0x00001000,	// Poly has a big wavy pattern in it.
	PF_SpecialPoly		= 0x00001000,	// Game-specific poly-level render control (reuse BigWavy flag)
	PF_SmallWavy		= 0x00002000,	// Small wavy pattern (for water/enviro reflection).
	PF_Flat				= 0x00004000,	// Flat surface.
	PF_LowShadowDetail	= 0x00008000,	// Low detaul shadows.
	PF_NoMerge			= 0x00010000,	// Don't merge poly's nodes before lighting when rendering.
	PF_CloudWavy		= 0x00020000,	// Polygon appears wavy like clouds.
	PF_DirtyShadows		= 0x00040000,	// Dirty shadows.
	PF_BrightCorners	= 0x00080000,	// Brighten convex corners.
	PF_SpecialLit		= 0x00100000,	// Only speciallit lights apply to this poly.
	PF_Gouraud			= 0x00200000,	// Gouraud shaded.
	PF_NoBoundRejection = 0x00200000,	// Disable bound rejection in OccludeBSP (reuse Gourard flag)
	PF_Unlit			= 0x00400000,	// Unlit.
	PF_HighShadowDetail	= 0x00800000,	// High detail shadows.
	PF_Portal			= 0x04000000,	// Portal between iZones.
	PF_Mirrored			= 0x08000000,	// Reflective surface.
	*/
}



//
// Return a pointer to a TriObject given an AXNode or return NULL
// if the node cannot be converted to a TriObject
//
TriObject* GetTriObjectFromNode(AXNode *node, TimeValue t, UBOOL &deleteIt)
{

	deleteIt = FALSE;
	Object *obj = ((INode*)node)->EvalWorldState(t).obj;
	if (obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
	{
		TriObject *tri = (TriObject *) obj->ConvertToType(t,
			Class_ID(TRIOBJ_CLASS_ID, 0));
		// Note that the TriObject should only be deleted
		// if the pointer to it is not equal to the object
		// pointer that called ConvertToType()
		if (obj != tri) deleteIt = TRUE;
		return tri;
	}
	else
	{
		return NULL;
	}
}

// Recursive tree storage
void SceneIFC::StoreNodeTree(AXNode* node)
{
	NodeInfo I;
	I.node = node;
	SerialTree.AddItem(I);

	for (int c = 0; c < ((INode*)node)->NumberOfChildren(); c++)
	{
		StoreNodeTree(((INode*)node)->GetChildNode(c));
	}

}


//
// Recurse all nodes of the entire scene tree, putting node pointers into an array for faster scene evaluation.
//

int SceneIFC::SerializeSceneTree()
{
	//TheMaxInterface = Itf;

	OurSkin=NULL;
	OurRootBone=NULL;

	// Get animation Timing info.
	Interval AnimInfo = TheMaxInterface->GetAnimRange();
	TimeStatic = AnimInfo.Start();

	// Frame 0 for reference:
	if( DoPoseZero ) TimeStatic = 0.0f;

	FrameStart = AnimInfo.Start(); // in ticks
	FrameEnd   = AnimInfo.End(); // in ticks
	FrameTicks = GetTicksPerFrame();
	FrameRate  = DoForceRate ? PersistentRate : GetFrameRate();

	SerialTree.Empty();

    StoreNodeTree(TheMaxInterface->GetRootNode());

	// Update basic nodeinfo contents
	for(int  i=0; i<SerialTree.Num(); i++)
	{
		// Update selected status

		SerialTree[i].IsSelected = ((INode*)SerialTree[i].node)->Selected();
		INode* ParentNode;
		ParentNode = ((INode*)SerialTree[i].node)->GetParentNode();

		// Establish our hierarchy by way of Serialtree indices... upward only.
		SerialTree[i].ParentIndex = MatchNodeToIndex( ParentNode );
		SerialTree[i].NumChildren = ( ((INode*)SerialTree[i].node)->NumberOfChildren() );

		/*
		if ( SerialTree[i].ParentIndex >0 )
		{
			if (SerialTree[SerialTree[i]]
		}
		*/
	}

	//DebugBox("end of SNT2 -nodes: %i",SerialTree.Num());

	return SerialTree.Num();
}

Modifier* FindPhysiqueModifier(AXNode* nodePtr)
{
	// Get object from node. Abort if no object.
	Object* ObjectPtr =((INode*)nodePtr)->GetObjectRef();

	//DebugBox("* Find Physiqye Modifier");

	if (!ObjectPtr) return NULL;

	// Is derived object ?
	if (ObjectPtr->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		// Yes -> Cast.
		IDerivedObject* DerivedObjectPtr = static_cast<IDerivedObject*>(ObjectPtr);

		// Iterate over all entries of the modifier stack.
		int ModStackIndex = 0;
		while (ModStackIndex < DerivedObjectPtr->NumModifiers())
		{
			// Get current modifier.
			Modifier* ModifierPtr = DerivedObjectPtr->GetModifier(ModStackIndex);

			DebugBox("Modifier Testing: %i \n",ModStackIndex);

			// Is this Physique ?
			if (ModifierPtr->ClassID() == Class_ID(PHYSIQUE_CLASS_ID_A, PHYSIQUE_CLASS_ID_B))
			{
				DebugBox("physique modifier found");
				// Yes -> Exit.
				return ModifierPtr;
			}

			// Next modifier stack entry.
			ModStackIndex++;
		}
	}
	// Not found.
	return NULL;
}


Modifier* FindMaxSkinModifier( AXNode* nodePtr)
{
	// Get object from node. Abort if no object.
	Object* ObjectPtr =((INode*)nodePtr)->GetObjectRef();

	if (!ObjectPtr) return NULL;

	// Is derived object ?
	if (ObjectPtr->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		// Yes -> Cast.
		IDerivedObject* DerivedObjectPtr = static_cast<IDerivedObject*>(ObjectPtr);

		// Iterate over all entries of the modifier stack.
		int ModStackIndex = 0;
		while (ModStackIndex < DerivedObjectPtr->NumModifiers())
		{
			// Get current modifier.
			Modifier* ModifierPtr = DerivedObjectPtr->GetModifier(ModStackIndex);

			//DebugBox("Modifier Testing: %i \n",ModStackIndex);

			// Is this Physique ?
			if( ModifierPtr->ClassID() == Class_ID(SKIN_CLASSID) ) //PHYSIQUE_CLASS_ID_A, PHYSIQUE_CLASS_ID_B))
			{
				DebugBox("Skin modifier found");
				// Yes -> Exit.
				return ModifierPtr;
			}
			// Next modifier stack entry.
			ModStackIndex++;
		}
	}
	// Not found.
	return NULL;
}


int SceneIFC::EvaluateBone(int RIndex)
{

	INode* nodePtr = (INode*)SerialTree[RIndex].node;
	// Ignore the root of the scene.
	if (((INode*)nodePtr)->IsRootNode()) return 0;

	// By calling EvalWorldState(TimeStatic) we tell max to eveluate the
	// object at end of the pipeline at the requested reference-pose time.
	ObjectState os =((INode*)nodePtr)->EvalWorldState(TimeStatic);

	// The obj member of ObjectState
	if (os.obj)
	{
		// We look at the super class ID to determine the type of the object.
		switch(os.obj->SuperClassID())
		{

			// ALL geometry objects can act as bones for physique, or be a part
			// of a hierarchy... even if their mesh is the skin itself.
			//
			case GEOMOBJECT_CLASS_ID:
			{
				// Ignore (Biped) footsteps.
				TSTR NodeName = ( ((INode*)nodePtr)->GetName() );
				if( CheckSubString(NodeName, _T("footsteps")) )
				{
					return 0;
				}

				// Classify any geom object acting as bone a GeoBone (==biped, mostly)
				// #debug  Not right ! Will get meshes counted as biped bones.
				// TotalBipBones++;
				return 1;
			}
			break;

			case HELPER_CLASS_ID:
				// if classname is Bone or Dummy (or  it can affect the Physique skin !
				Object* helperObj =((INode*)nodePtr)->EvalWorldState(TimeStatic).obj;
				if (helperObj)
				{
					TSTR className;
					helperObj->GetClassName(className);
					if( CheckSubString(className, _T("Dummy")) )
					{
						SerialTree[RIndex].IsDummy = 1;
						return 1;
					}
					if( CheckSubString(className, _T("POINTHELP")) )
					{
						SerialTree[RIndex].IsPointHelper = 1;
						return 1;
					}
					if( CheckSubString(className, _T("Bone")) )
					{
						SerialTree[RIndex].IsMaxBone = 1;
						return 1;
					}
				}
				return 0;
			break;

			// Other unused classes can include CAMERA_CLASS_ID, LIGHT_CLASS_ID, SHAPE_CLASS_ID:
		}
	}
	return 0;
}


int HasMesh(INode* node, FLOAT TimeStatic)
{
	ObjectState os = ((INode*)node)->EvalWorldState(TimeStatic);
	if (!os.obj || os.obj->SuperClassID()!=GEOMOBJECT_CLASS_ID)
	{
		return 0; // not a geom object anyway
	}

	Object *obj = os.obj;

	if (obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
	{
		return 1; // Can convert to triangle mesh.
	}
	return 0;
}


int SceneIFC::HasTexture(AXNode* node)
{
	// Conclude it's a geometry mesh whenever we can convert it to a TRIOBJ and it has a material assigned to it..

	ObjectState os = ((INode*)node)->EvalWorldState(TimeStatic);
	if (!os.obj || os.obj->SuperClassID()!=GEOMOBJECT_CLASS_ID)
	{
		return 0; // not a geom object anyway
	}

	Object *obj = os.obj;

	if (obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
	{
		UBOOL deleteIt = false;
		//INT TVertsDetected = 0;
		INT MaterialDetected = 0;
		// get tri object, traverse triangles, see if any have 'Textured vertices'

		TriObject *tri = (TriObject *) obj->ConvertToType( 0, Class_ID(TRIOBJ_CLASS_ID, 0));
		// Note that the TriObject should only be deleted
		// if the pointer to it is not equal to the object
		// pointer that called ConvertToType()
		if (obj != tri) deleteIt = TRUE;

		// Get the material from the node
		Mtl* nodeMtl = ((INode*)node)->GetMtl();
		if (nodeMtl) MaterialDetected = 1;

		/*
		Mesh* OurTriMesh = &tri->mesh;
		int NumFaces = OurTriMesh->getNumFaces();

		for (int TriIndex = 0; +TriIndex < NumFaces; TriIndex++)
		{
			Face*	TriFace			= &OurTriMesh->faces[TriIndex];
			if (TriFace->flags & HAS_TVERTS)
			{
				TVertsDetected = 1;
				break;
			}
		}
		*/

		if( deleteIt )
		{
			delete tri;
		}

		//return TVertsDetected; // Can convert to triangle mesh.

		return MaterialDetected;
	}
	return 0;
}



//
// Scene information probe. Uses the serialized scene tree.
//

int SceneIFC::GetSceneInfo()
{

	// PopupBox("Get Scene Info");
	if (!TheMaxInterface)
	{
		DebugBox("* ERROR TheMaxInterface not defined ");
		return 0;
	}

	DLog.Logf(" Total nodecount %i \n",SerialTree.Num());

	TotalSkinNodeNum = 0;
	TotalBones    = 0;
	TotalDummies  = 0;
	TotalBipBones = 0;
	TotalMaxBones = 0;

	GeomMeshes = 0;

	LinkedBones = 0;
	TotalSkinLinks = 0;

	DebugBox("* Total serial nodes to check: %i \n",SerialTree.Num());

	if (SerialTree.Num() == 0) return 0;

	DebugBox("Go over serial nodes & set flags");

	OurSkins.Empty();

	for( int i=0; i<SerialTree.Num(); i++)
	{
		// Any meshes at all ?
		SerialTree[i].HasMesh = HasMesh( (INode*)SerialTree[i].node, TimeStatic );

		// Check for a bone candidate.
		SerialTree[i].IsBone = EvaluateBone(i);

		// Check for a physique modifier
		Modifier *ThisPhysique = FindPhysiqueModifier( SerialTree[i].node );
		Modifier *ThisMaxSkin  = FindMaxSkinModifier( SerialTree[i].node );

		SerialTree[i].HasTexture = 0;

		// Any texture ?
		if (SerialTree[i].HasMesh)
			SerialTree[i].HasTexture = HasTexture(SerialTree[i].node);

		GeomMeshes += SerialTree[i].HasMesh;

		if ( (ThisPhysique || ThisMaxSkin ) && DoSkinGeom )
		{
			// Physique/MaxSkin skins dont act as bones themselves. Any other mesh object can act as a bone, though.
			SerialTree[i].IsBone = 0;
			SerialTree[i].IsSkin = 1;

			// OurSkins array can hold handles for multile physique/maxskin nodes.
			SkinInf NewSkin;

			NewSkin.Node = SerialTree[i].node;
			NewSkin.IsSmoothSkinned = 1;
			NewSkin.IsTextured = 1;
			NewSkin.SceneIndex = i;
			NewSkin.SeparateMesh = 0;

			NewSkin.IsPhysique = ( ThisPhysique != NULL );
			NewSkin.IsMaxSkin =  ( ThisMaxSkin  != NULL );

			OurSkins.AddItem(NewSkin);

			// #TODO - A primitive with a Max skin modifier could theoretically be its own bone too... take that into account ??
			if( NewSkin.IsMaxSkin )
			{
				// SerialTree[i].IsBone = 1;
			}

			// Prefer the first or the selected skin if more than one encountered.
			if ((((INode*)SerialTree[i].node)->Selected()) || (TotalSkinNodeNum == 0))
			{
					OurSkin = SerialTree[i].node;
			}
			TotalSkinNodeNum++;
		}
		else if (SerialTree[i].HasTexture && SerialTree[i].HasMesh && DoTexGeom )
		{
			// Any nonphysique mesh object acts as its own bone ?
			SerialTree[i].IsBone = 1;
			SerialTree[i].IsSkin = 1;

			// Multiple physique nodes...
			SkinInf NewSkin;

			NewSkin.Node = SerialTree[i].node;
			NewSkin.IsSmoothSkinned = 0;
			NewSkin.IsTextured = 1;
			NewSkin.SceneIndex = i;
			NewSkin.SeparateMesh = 1;

			NewSkin.IsPhysique = 0;
			NewSkin.IsMaxSkin = 0;

			OurSkins.AddItem(NewSkin);
			// Prefer the first or the selected skin if more than one encountered.
			TSTR NodeName(((INode*)SerialTree[i].node)->GetName());
			DebugBox(_T("Textured geometry mesh: %s"),NodeName);
		}
	}

	DebugBox("Number of skinned nodes: %i",OurSkins.Num());

	DebugBox("Add skinlink stats");

	// Accumulate some skin linking stats.
	for( int i=0; i<SerialTree.Num(); i++)
	{
		if (SerialTree[i].LinksToSkin) LinkedBones++;
		TotalSkinLinks += SerialTree[i].LinksToSkin;
	}

	DebugBox(_T("Tested main physique skin, bone links: %i total links %i"),LinkedBones, TotalSkinLinks );


	//
	// Cull from skeleton all dummies (and point helpers) that don't have any physique or MaxSkin links to them and that don't have any children.
	//
	INT CulledDummies = 0;
	if (this->DoCullDummies)
	{
		for( int i=0; i<SerialTree.Num(); i++)
		{
			if (
			   ( ( SerialTree[i].IsDummy ) || SerialTree[i].IsPointHelper ) &&
			   ( SerialTree[i].LinksToSkin == 0)  &&
			   ( ((INode*)SerialTree[i].node)->NumberOfChildren()==0 ) &&
			   ( SerialTree[i].HasTexture == 0 ) &&
			   ( SerialTree[i].IsBone == 1 )
			   )
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].IsCulled = 1;
				CulledDummies++;
			}
		}
		if( ( !OurScene.InBatchMode)  && (!OurScene.DoSuppressAnimPopups) )
		{
			PopupBox(_T("Culled dummies: %i"),CulledDummies);
		}
	}

	// Log for debugging purposes...
	if (DEBUGFILE)
	{
		for( int i=0; i<SerialTree.Num(); i++)
		{
			TSTR NodeName(((INode*)SerialTree[i].node)->GetName());
			DLog.Logf("Node: %4i %-23s Dummy: %2i PHelper %2i Bone: %2i Skin: %2i PhyLinks: %4i Parent %4i Isroot %4i HasTexture %i Childrn %i Culld %i Parent %3i \n",i,
				NodeName,
				SerialTree[i].IsDummy,
				SerialTree[i].IsPointHelper,
				SerialTree[i].IsBone,
				SerialTree[i].IsSkin,
				SerialTree[i].LinksToSkin,
				SerialTree[i].ParentIndex,
				(INT)((INode*)SerialTree[i].node)->IsRootNode(),
				SerialTree[i].HasTexture,
				((INode*)SerialTree[i].node)->NumberOfChildren(),
				SerialTree[i].IsCulled,
				SerialTree[i].ParentIndex
				);
		}
	}
	TreeInitialized = 1;
	return 1;
}


int SceneIFC::RecurseValidBones(const int RIndex, int &BoneCount)
{
	if ( SerialTree[RIndex].IsBone == 0) return 0; // Exit hierarchy for non-bones...

	SerialTree[RIndex].InSkeleton = 1; //part of our skeleton.
	BoneCount++;

	for (int c = 0; c < SerialTree[RIndex].NumChildren; c++)
	{
		if( GetChildNodeIndex(RIndex,c) <= 0 ) ErrorBox(_T("GetChildNodeIndex assertion failed"));

		RecurseValidBones( GetChildNodeIndex(RIndex,c), BoneCount);
	}
	return 1;
}


int	SceneIFC::MarkBonesOfSystem(int RIndex)
{
	int BoneCount = 0;

	for( int i=0; i<SerialTree.Num(); i++)
	{
		SerialTree[i].InSkeleton = 0; // Erase skeletal flags.
	}

	RecurseValidBones( RIndex, BoneCount );

	// DebugBox("Recursed valid bones: %i",BoneCount);

	// How to get the hierarchy with support of our
	// flags, too:
	// - ( a numchildren, and look-for-nth-child routine for our array !)

	return BoneCount;
};


//
// Evaluate the skeleton & set node flags...
//

int SceneIFC::EvaluateSkeleton( INT RequireBones )
{
	// Our skeleton: always the one hierarchy
	// connected to the scene root which has either the most
	// members, or the most SELECTED members, or a physique
	// mesh attached...
	// For now, a hack: find all root bones; then choose the one
	// which is selected.

	RootBoneIndex = 0;
	if (SerialTree.Num() == 0) return 0;

	int BonesMax = 0;

	for( int i=0; i<SerialTree.Num(); i++)
	{
		if( (SerialTree[i].IsBone != 0 ) && (SerialTree[i].ParentIndex == 0) )
		{
			// choose one with the biggest number of bones, that must be the
			// intended skeleton/hierarchy.
			int TotalBones = MarkBonesOfSystem(i);

			if (TotalBones>BonesMax)
			{
				RootBoneIndex = i;
				BonesMax = TotalBones;
			}
		}
	}


	if (RootBoneIndex == 0) return 0;// failed to find any.

	// Real node
	OurRootBone = SerialTree[RootBoneIndex].node;

	// Total bones
	OurBoneTotal = MarkBonesOfSystem(RootBoneIndex);

	// Tally the elements of the current skeleton
	for( int i=0; i<SerialTree.Num(); i++)
	{
		if (SerialTree[i].InSkeleton )
		{
			TotalDummies  += SerialTree[i].IsDummy;
			TotalPointHelpers +=SerialTree[i].IsPointHelper;
			TotalMaxBones += SerialTree[i].IsMaxBone;
			TotalBones    += SerialTree[i].IsBone;
		}
	}

	TotalBipBones = TotalBones - TotalMaxBones - TotalDummies - TotalPointHelpers;

	return 1;
};


//
// Digest a *reference* skeleton into a VActor.
// ( EvaluateSkeleton assumed.)
// Reference time is always 0th frame.
//

int SceneIFC::DigestSkeleton(VActor *Thing)
{

	TextFile SkelLog;
	if (DEBUGFILE)
	{
		TSTR LogFileName = _T("\\ActXSKL.LOG");
		SkelLog.Open( to_path, LogFileName, DEBUGFILE );
   		SkelLog.Logf(" Starting DigestSkeleton logfile \n\n");
	}


	Thing->RefSkeletonBones.SetSize(0);  //SetSize( SerialNodes ); //#debug  'ourbonetotal ?'
	Thing->RefSkeletonNodes.SetSize(0);  //SetSize( SerialNodes );

	DebugBox("Our bone total: %i Serialnodes: %i \n",OurBoneTotal,SerialTree.Num());

	// for reassigning parents...
	TArray <int> NewParents;

	// Assign all bones & store names etc.
	// Rootbone is in tree starting from RootBoneIndex.

	int BoneIndex = 0;

	// Fill:
	// Thing->RefSkeletonBones.
	// Thing->RefSkeletonNodes.

	for (int i = RootBoneIndex; i<SerialTree.Num(); i++)
	{
		if ( SerialTree[i].InSkeleton )
		{
			NewParents.AddItem( i );

			FNamedBoneBinary ThisBone; //= &Thing->RefSkeletonBones[BoneIndex];
			Memzero( &ThisBone, sizeof( ThisBone ) );

			ThisBone.Flags = 0;
			ThisBone.NumChildren = 0;
			ThisBone.ParentIndex = -1;

			INode* ThisNode = (INode*) SerialTree[i].node;

			//Copy name. Truncate/pad to 19 chars for safety.
			TSTR BoneName = (((INode*)SerialTree[i].node)->GetName());

			BoneName.Resize(31); // will be padded if needed.
		#if _UNICODE
			wcstombs(ThisBone.Name, BoneName, 32);
		#else
			strcpy(ThisBone.Name,BoneName);
		#endif

			ThisBone.Flags = 0;//ThisBone.Info.Flags = 0;
			ThisBone.NumChildren = SerialTree[i].NumChildren; //ThisBone.Info.NumChildren = SerialTree[i].NumChildren;

			if (i != RootBoneIndex)
			{
				//#debug needs to be Local parent if we're not the root.
				int OldParent = SerialTree[i].ParentIndex;
				int LocalParent = -1;

				// Find what new index this corresponds to. Parent must already be in set.
				for (int f=0; f<NewParents.Num(); f++)
				{
					if( NewParents[f] == OldParent ) LocalParent=f;
				}

				if (LocalParent == -1 )
				{
					ErrorBox(_T(" Local parent lookup failed."));
					LocalParent = 0;
				}

				ThisBone.ParentIndex = LocalParent; //ThisBone.Info.ParentIndex = LocalParent;
			}
			else
			{
				ThisBone.ParentIndex = 0; // root links to itself...? .Info.ParentIndex
			}


			INode *parent;
			Matrix3 parentTM, nodeTM, localTM;
			nodeTM = ((INode*)ThisNode)->GetNodeTM(TimeStatic);
			parent = ((INode*)ThisNode)->GetParentNode();
			parentTM = parent->GetNodeTM(TimeStatic);

			Quat LocalQuat = ((INode*)ThisNode)->GetObjOffsetRot();

			// clean up scaling ?
			// parentTM.NoScale();
			// nodeTM.NoScale();

			nodeTM = Uniform_Matrix(nodeTM);
			parentTM = Uniform_Matrix(parentTM);

			// undo parent transformation to obtain local trafo
			// localTM = nodeTM*Inverse(parentTM);

			// Obtain local matrix unless this is the root of the skeleton.
			if ( SerialTree[i].ParentIndex == 0 )
			{
				localTM = nodeTM;
			}
			else
			{
				localTM = nodeTM*Inverse(parentTM);
			}

			Point3 Trans = localTM.GetTrans();
			AffineParts localAff;
			decomp_affine(localTM, &localAff);

			Point3 Row;
			TSTR NodeName(((INode*)ThisNode)->GetName());

			ThisBone.BonePos.Length = 0.0f ;
			if ( SerialTree[i].ParentIndex == 0 ) //#check whether root
			{
				// #debug #inversion
				// Root XYZ needs to be negated to match the handedness of the reference skin, and space in Unreal in general.
				ThisBone.BonePos.Orientation = FQuat(-localAff.q.x,-localAff.q.y,-localAff.q.z,localAff.q.w);
			}
			else
			{
				ThisBone.BonePos.Orientation = FQuat(localAff.q.x,localAff.q.y,localAff.q.z,localAff.q.w);
			}
			ThisBone.BonePos.Position = FVector(Trans.x,Trans.y,Trans.z);

			Thing->RefSkeletonNodes.AddItem(ThisNode);
			Thing->RefSkeletonBones.AddItem(ThisBone);
		}
	}

	// Bone/node duplicate name check
	DuplicateBones = 0;
	{for(INT i=0; i< Thing->RefSkeletonBones.Num(); i++)
	{
		for(INT j=0; j< Thing->RefSkeletonBones.Num(); j++)
		{
			if( (i!=j) && ( strcmp( Thing->RefSkeletonBones[i].Name, Thing->RefSkeletonBones[j].Name)==0)  )
			{
				DuplicateBones++;
			#if _UNICODE
				TCHAR BoneName[64];
				mbstowcs(BoneName, Thing->RefSkeletonBones[j].Name, 64);
			#else
				const char *BoneName = Thing->RefSkeletonBones[j].Name;
			#endif
				WarningBox( _T(" Duplicate bone name encountered: %s "), BoneName );
			}
		}
	}}
	if( DuplicateBones > 0 )
	{
		WarningBox( _T("%i total duplicate bone name(s) detected."), DuplicateBones );
	}


	if (DEBUGFILE)
	{
		for( INT i=0; i<SerialTree.Num(); i++)
		{
			TSTR NodeName(((INode*)SerialTree[i].node)->GetName());

			SkelLog.Logf("Node: %4i %-23s Dummy: %2i PointHelper %2i Bone: %2i Skin: %2i PhyLinks: %4i Tex: %i Culld: %2i Parent:%2i\n",i,
				NodeName,
				SerialTree[i].IsDummy,
				SerialTree[i].IsPointHelper,
				SerialTree[i].IsBone,
				SerialTree[i].IsSkin,
				SerialTree[i].LinksToSkin,
				SerialTree[i].HasTexture,
				SerialTree[i].IsCulled,
				SerialTree[i].ParentIndex
				);
		}

        // Print out refskeletonbones too.
		for( INT b=0; b<Thing->RefSkeletonBones.Num(); b++)
		{
			SkelLog.Logf(" RefSkel bone: %4i  %-23s  Fl: %3i  Children# %3i Parent: %3i \n",
				b,
				Thing->RefSkeletonBones[b].Name,
				Thing->RefSkeletonBones[b].Flags,
				Thing->RefSkeletonBones[b].NumChildren,
				Thing->RefSkeletonBones[b].ParentIndex
				);
		}

		SkelLog.Close();
	}

	DebugBox("total bone nodes: %i",Thing->RefSkeletonNodes.Num());

	DLog.Close();

	return 1;
};




/*
	3ds MAX frame rate conventions:

	GetFrameRate() :  returns frame rate in frames per second;
	GetTicksPerFrame() : returns ticks per frame;
	AnimInfo.Start() :
	AnimInfo.End():

	Time is stored internally in MAX as an integer number of ticks.  Each second of an animation is divided into 4800 ticks.
	This value is chosen in part because it is evenly divisible by the standard frame per second settings in common use
	(24 -- Film, 25 -- PAL, and 30 -- NTSC).

*/


//
// Retrieve (animated) scale for node at time Now
//
FVector GetAnimatedScale(INode* node, TimeValue timeNow )
{
	Matrix3 tm;
	AffineParts ap;

	tm = node->GetNodeTM(timeNow) * Inverse(node->GetParentTM(timeNow));
    decomp_affine(tm, &ap);

    return FVector( ap.k.x, ap.k.y, ap.k.z );
}


//
// Digest the animations into a VActor.
// reference skeleton need not be present.
//
int SceneIFC::DigestAnim(VActor *Thing, TCHAR* AnimName, TCHAR* RangeString )
{
	if( DEBUGFILE )
	{
		TSTR LogFileName = _T("\\ActXAni.LOG");
		DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	INT FirstFrame = (INT)FrameStart / FrameTicks; // Convert ticks to frames
	INT LastFrame = (INT)FrameEnd / FrameTicks;

	ParseFrameRange( RangeString, FirstFrame, LastFrame );

	OurTotalFrames = FrameList.GetTotal();
	Thing->RawNumFrames = OurTotalFrames;
	Thing->RawAnimKeys.SetSize( OurTotalFrames * OurBoneTotal );
	Thing->RawNumBones  = OurBoneTotal;
	Thing->FrameTotalTicks = OurTotalFrames * FrameTicks;
	Thing->FrameRate = GetFrameRate();

	if( DoExportScale )
	{
		Thing->RawScaleKeys.SetSize( OurTotalFrames * OurBoneTotal );
	}

	// PopupBox(" Frame rate %f  Ticks per frame %f  Total frames %i FrameTimeInfo %f ",(FLOAT)GetFrameRate(), (FLOAT)FrameTicks, (INT)OurTotalFrames, (FLOAT)Thing->FrameTotalTicks );

#if _UNICODE
	wcstombs(Thing->RawAnimName, CleanString( AnimName ), 64);
#else
	strcpy(Thing->RawAnimName,CleanString( AnimName ));
#endif

	//
	// Rootbone is in tree starting from RootBoneIndex.
	//

	int BoneIndex  = 0;
	int AnimIndex  = 0;
	int FrameCount = 0;
	int TotalAnimNodes = SerialTree.Num()-RootBoneIndex;

	//for ( TimeValue TimeNow = FrameStart; TimeNow <= FrameEnd; TimeNow += FrameTicks )
	for( INT t=0; t<FrameList.GetTotal(); t++)
	{
		TimeValue TimeNow = FrameList.GetFrame(t) * FrameTicks;

		// Advance progress bar; obey bailout.
		INT Progress = (INT) 100.0f * ( t/(FrameList.GetTotal()+0.01f) );
		TheMaxInterface->ProgressUpdate(Progress);

		if (TheMaxInterface->GetCancel())
		{
			INT retval = MessageBox(TheMaxInterface->GetMAXHWnd(), _T("Really Cancel"),
				_T("Question"), MB_ICONQUESTION | MB_YESNO);
			if (retval == IDYES)
				break;
			else if (retval == IDNO)
				TheMaxInterface->SetCancel(FALSE);
		}

		if (DEBUGFILE && to_path[0])
		{
			DLog.Logf(" Frame time: %f   frameticks %f  \n", (FLOAT)TimeNow, (FLOAT)FrameTicks);
		}

		for (int i = RootBoneIndex; i<SerialTree.Num(); i++)
		{
			if ( SerialTree[i].InSkeleton )
			{
				INode* ThisNode = (INode*) SerialTree[i].node;

				// Store local rotation/translation.
				// Local matrix extraction according to Max SDK
				INode *parent;
				Matrix3 parentTM, nodeTM, localTM;
				nodeTM = ((INode*)ThisNode)->GetNodeTM(TimeNow);
				parent = ((INode*)ThisNode)->GetParentNode();
				parentTM = parent->GetNodeTM(TimeNow);

				// clean up scaling - necessary for Biped skeletons in particular ?
				/*
				parentTM.NoScale();
				nodeTM.NoScale();
				*/

				nodeTM = Uniform_Matrix(nodeTM);
				parentTM = Uniform_Matrix(parentTM);

				//
				// Obtain local matrix unless this is the root of the skeleton.
				//
				if ( SerialTree[i].ParentIndex == 0 ) //#check for root
				{
					localTM = nodeTM;
				}
				else
				{
					localTM = nodeTM*Inverse(parentTM);
				}

				// Row = nodeTM.GetTrans();
				// DLog.Logf(" tr)  %9f %9f %9f  ",Row.x,Row.y,Row.z);

				// Dump local matrix to log.
				/*
				if (DEBUGFILE && to_path[0])
				{
					Point3 Row;
					TSTR NodeName(((INode*)ThisNode)->GetName());

					DLog.Logf(" Global matrix number %i  name %s  ParentIndex %i \n",i,NodeName,SerialTree[i].ParentIndex);
					Row = nodeTM.GetRow(0);
					DLog.Logf("  x)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = nodeTM.GetRow(1);
					DLog.Logf("  y)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = nodeTM.GetRow(2);
					DLog.Logf("  z)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = nodeTM.GetRow(3);
					DLog.Logf("  t)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));

					DLog.Logf(" Local matrix number %i  name %s  ParentIndex %i \n",i,NodeName,SerialTree[i].ParentIndex);
					Row = localTM.GetRow(0);
					DLog.Logf("  x)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = localTM.GetRow(1);
					DLog.Logf("  y)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = localTM.GetRow(2);
					DLog.Logf("  z)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = localTM.GetRow(3);
					DLog.Logf("  t)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
				}
				*/

				Point3 Trans = localTM.GetTrans(); // Return translation row of matrix
				AffineParts localAff;
				decomp_affine(localTM, &localAff);

				Thing->RawAnimKeys[AnimIndex].Time = 1.0f;

				if( DoExportScale )
				{
					Thing->RawScaleKeys[AnimIndex].Time = 1.0f;
					Thing->RawScaleKeys[AnimIndex].ScaleVector = GetAnimatedScale( ThisNode, TimeNow );
				}

				if ( SerialTree[i].ParentIndex == 0 ) //#check whether root
				{
					// #debug #inversion
					// Root XYZ needs to be negated to match the handedness of the reference skin, and space in Unreal in general.
					Thing->RawAnimKeys[AnimIndex].Orientation = FQuat(-localAff.q.x,-localAff.q.y,-localAff.q.z,localAff.q.w);
				}
				else
				{
					Thing->RawAnimKeys[AnimIndex].Orientation = FQuat(localAff.q.x,localAff.q.y,localAff.q.z,localAff.q.w);
				}
				Thing->RawAnimKeys[AnimIndex].Position = FVector(Trans.x,Trans.y,Trans.z);



				/*
				if (DEBUGFILE && to_path[0])
				{
					DLog.Logf(" Bone %3i Pos  %f %f %f    \n",i, Thing->RawAnimKeys[AnimIndex].Position.X,Thing->RawAnimKeys[AnimIndex].Position.Y,Thing->RawAnimKeys[AnimIndex].Position.Z);
					DLog.Logf("          Quat %f %f %f %f  \n", Thing->RawAnimKeys[AnimIndex].Orientation.X,Thing->RawAnimKeys[AnimIndex].Orientation.Y,Thing->RawAnimKeys[AnimIndex].Orientation.Z,Thing->RawAnimKeys[AnimIndex].Orientation.W);

					// What the GLOBAL quaternion would look like:
					Point3 Trans = nodeTM.GetTrans();
					AffineParts GlobalAff;
					decomp_affine(nodeTM, &GlobalAff );

					FQuat GlobQ = FQuat(GlobalAff.q.x, GlobalAff.q.y, GlobalAff.q.z, GlobalAff.q.w);
					FVector GlobT =  FVector(Trans.x,Trans.y,Trans.z);
					DLog.Logf("GBone %3i Pos  %f %f %f    \n",i, GlobT.X,GlobT.Y,GlobT.Z);
					DLog.Logf("G         Quat %f %f %f %f  \n", GlobQ.X, GlobQ.Y, GlobQ.Z, GlobQ.W);
					//
				}
				*/

				AnimIndex++;
			}
			else if (DEBUGFILE && to_path[0]) // Also dump non-skeletal nodes for debugging here only
			{
				INode* ThisNode = (INode*)SerialTree[i].node;

				//
				// Store local rotation/translation.
				// Local matrix extraction according to Max SDK
				//

				INode *parent;
				Matrix3 parentTM, nodeTM, localTM;
				nodeTM = ((INode*)ThisNode)->GetNodeTM(TimeNow);

				if (!((INode*)ThisNode)->IsRootNode())
				{
					parent = ((INode*)ThisNode)->GetParentNode();
					parentTM = parent->GetNodeTM(TimeNow);
					parentTM.NoScale();
				}

				// clean up scaling
				nodeTM.NoScale();

				if ( SerialTree[i].ParentIndex == 0 )
				{
					localTM = nodeTM;
				}
				else
				{
					localTM = nodeTM*Inverse(parentTM);
				}

				/*
				if (DEBUGFILE && to_path[0])
				{
					Point3 Row;
					TSTR NodeName(((INode*)ThisNode)->GetName());

					DLog.Logf(" Local matrix number %i  name %s  ParentIndex %i \n",i,NodeName,SerialTree[i].ParentIndex);
					Row = localTM.GetRow(0);
					DLog.Logf("  x)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = localTM.GetRow(1);
					DLog.Logf("  y)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = localTM.GetRow(2);
					DLog.Logf("  z)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
					Row = localTM.GetRow(3);
					DLog.Logf("  t)  %9f %9f %9f  size %9f \n",Row.x,Row.y,Row.z,sqrt( (Row.x*Row.x)+(Row.y*Row.y)+(Row.z*Row.z) ));
				}
				*/
			}
		}
		FrameCount++;
	} // Time

	DLog.Close();


	if (AnimIndex != (OurBoneTotal * OurTotalFrames))
	{
		ErrorBox(_T(" Anim: digested %i Expected %i FrameCalc %i FramesProc %i \n"),AnimIndex, OurBoneTotal*OurTotalFrames, OurTotalFrames,FrameCount);
		return 0;
	};

	return 1;
};



//
// Digest _and_ write vertex animation.
//

int SceneIFC::WriteVertexAnims(VActor *Thing, TCHAR* DestFileName, TCHAR* RangeString )
{

    //#define DOLOGVERT 0
	//if( DOLOGVERT )
	//{
	//	TSTR LogFileName = _T("\\ActXVert.LOG");
	//	AuxLog.Open(LogPath,LogFileName, 1 );//DEBUGFILE);
	//}

	INT FirstFrame = (INT)FrameStart / FrameTicks; // Convert ticks to frames. FrameTicks set in
	INT LastFrame = (INT)FrameEnd / FrameTicks;

	ParseFrameRange( RangeString, FirstFrame, LastFrame );

	OurTotalFrames = FrameList.GetTotal();
	Thing->RawNumFrames = OurTotalFrames;
	Thing->RawAnimKeys.SetSize( OurTotalFrames * OurBoneTotal );
	Thing->RawNumBones  = OurBoneTotal;
	Thing->FrameTotalTicks = OurTotalFrames * FrameTicks;
	Thing->FrameRate = GetFrameRate(); //FrameRate - unused for vert anim....

	INT FrameCount = 0;
	INT AppendSeekLocation = 0;

	VertexMeshHeader MeshHeader;
	Memzero( &MeshHeader, sizeof( MeshHeader ) );

	VertexAnimHeader AnimHeader;
	Memzero( &AnimHeader, sizeof( AnimHeader ) );

	TArray<VertexMeshTri> VAFaces;
	TArray<FMeshVert> VAVerts;
	TArray<FVector> VARawVerts;

	// Get 'reference' triangles; write the _d.3d file. Scene time of sampling here unimportant.
	OurScene.DigestSkin(&TempActor );

	// NOTE: handedness-flips all triangles - to be consistent with Unreal engine self-normal generation dependent on winding.
	{for( INT f=0; f< TempActor.SkinData.Faces.Num(); f++)
	{
		INT WIndex = TempActor.SkinData.Faces[f].WedgeIndex[1];
		TempActor.SkinData.Faces[f].WedgeIndex[1] = TempActor.SkinData.Faces[f].WedgeIndex[2];
		TempActor.SkinData.Faces[f].WedgeIndex[2] = WIndex;
	}}

	// Gather triangles
	for( INT t=0; t< TempActor.SkinData.Faces.Num(); t++)
	{
		VertexMeshTri NewTri;

		NewTri.Color = 0;
		NewTri.Flags = 0;


		NewTri.iVertex[0] = TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[0] ].PointIndex;
		NewTri.iVertex[1] = TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[1] ].PointIndex;
		NewTri.iVertex[2] = TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[2] ].PointIndex;

		FLOAT TexScaler=255.99f; //!

		for( INT v=0; v<3; v++)
		{
			NewTri.Tex[v].U = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].UV.U );
			NewTri.Tex[v].V = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].UV.V );
		}

		NewTri.TextureNum = TempActor.SkinData.Faces[t].MatIndex;

		// #TODO - Verify that this is the proper flag assignment for things like alpha, masked, doublesided, etc...
		if( NewTri.TextureNum < Thing->SkinData.RawMaterials.Num() )
			NewTri.Flags = Thing->SkinData.Materials[NewTri.TextureNum].PolyFlags;

		VAFaces.AddItem( NewTri );
	}

	INT RefSkinPoints = TempActor.SkinData.Points.Num();

	// Write.. to 'model' file DestFileName_d.3d
	if( VAFaces.Num() && (OurScene.DoAppendVertex == 0) )
	{
		FastFileClass OutFile;
		TCHAR OutPath[MAX_PATH];
		_stprintf(OutPath,_T("%s\\%s_d.3d"),to_pathvtx,DestFileName);
		//PopupBox("OutPath for _d file: %s",OutPath);
		if ( OutFile.CreateNewFile(OutPath) != 0)
			ErrorBox( _T("File creation error. "));

		MeshHeader.NumPolygons = VAFaces.Num();
		MeshHeader.NumVertices = TempActor.SkinData.Points.Num();
		MeshHeader.Unused[0] = 117; // "serial" number for ActorX-generated vertex data.

		// Write header.
		OutFile.Write( &MeshHeader, sizeof(MeshHeader));

		// Write triangle data.
		for( INT f=0; f< VAFaces.Num(); f++)
		{
			OutFile.Write( &(VAFaces[f]), sizeof(VertexMeshTri) );
		}

		OutFile.CloseFlush();

		if( OutFile.GetError()) ErrorBox(_T("Vertex skin save error."));

	}

    //
	// retrieve mesh for every frame & write to (to_skinfile_a.3d. Optionally append it to existing.
	//

	for( INT f=0; f<FrameList.GetTotal(); f++)
	{
		TimeValue TimeNow = FrameList.GetFrame(f) * FrameTicks;

		// Advance progress bar; obey bailout.
		INT Progress = (INT) 100.0f * ( f/(FrameList.GetTotal()+0.01f) );
		TheMaxInterface->ProgressUpdate(Progress);

		// give user opportunity to cancel
		if (TheMaxInterface->GetCancel())
		{
			INT retval = MessageBox(TheMaxInterface->GetMAXHWnd(), _T("Really Cancel"),
				_T("Question"), MB_ICONQUESTION | MB_YESNO);
			if (retval == IDYES)
				break;
			else if (retval == IDNO)
				TheMaxInterface->SetCancel(FALSE);
		}

		// Capture current mesh as a 'reference pose'  at 'TimeNow'.
		TimeStatic = TimeNow;
		OurScene.DigestSkin(&TempActor );
		for( INT v=0; v< Thing->SkinData.Points.Num(); v++ )
		{
			VARawVerts.AddItem( Thing->SkinData.Points[v].Point ); //Store full 3d point, since we may be rescaling stuff later.
		}

		// Add raw skin verts to buffer; we don't care about bones.

		FrameCount++;
	} // Time.

	//
	// Convert into VAVerts (32-bit packed) and write out...
	//
	if( VARawVerts.Num() )
	{
		INT FileError = 0;

		if( OurScene.DoScaleVertex )
		{
			//
			// AUTO-scale (all 3 dimensions) if it's 0 ?
			//
			if( OurScene.VertexExportScale <= 0.f )
				OurScene.VertexExportScale = 1.0f;

			for( INT v=0; v<VARawVerts.Num(); v++)
			{
				VARawVerts[v] *= OurScene.VertexExportScale;
			}
		}

		VAVerts.Empty();

		// Convert.
		for( INT v=0; v<VARawVerts.Num(); v++)
		{
			FMeshVert VANew( VARawVerts[v]);
			VAVerts.AddItem( VANew );
		}

		//
		// Write.
		//
		FastFileClass OutFile;
		TCHAR OutPath[MAX_PATH];
		_stprintf(OutPath,_T("%s\\%s_a.3d"),to_pathvtx,DestFileName);

		if( ! OurScene.DoAppendVertex )
		{
			if ( OutFile.CreateNewFile(OutPath) != 0) // Error!
			{
				ErrorBox( _T("File creation error. "));
				FileError = 1;
			}
		}
		else
		{
			if ( OutFile.OpenExistingFile(OutPath) != 0) // Error!
			{
				ErrorBox( _T("File appending error. "));
				FileError = 1;
			}
			// Need to explicitly move file ptr to end for appending.
			AppendSeekLocation = OutFile.GetSize();
			if( AppendSeekLocation <= 0 )
			{
				ErrorBox(_T(" File appending error: file on disk has no data."));
				FileError = 1;
			}
		}

		if( ! FileError )
		{
			AnimHeader.FrameSize = ( WORD )( RefSkinPoints * sizeof( DWORD ) );
			AnimHeader.NumFrames = FrameCount;

			// Write header if we're not appending.
			if( ! OurScene.DoAppendVertex)
			{
				OutFile.Write( &AnimHeader, sizeof(AnimHeader));
			}
			else
			{
				VertexAnimHeader ExistingAnimHeader;
				ExistingAnimHeader.FrameSize = 0;
				ExistingAnimHeader.NumFrames = 0;

				// Overwrite header, but then skip existing data.
				OutFile.Seek( 0 );
				OutFile.Read( &ExistingAnimHeader, sizeof(ExistingAnimHeader));
				AnimHeader.NumFrames  += ExistingAnimHeader.NumFrames;
				if( AnimHeader.FrameSize != ExistingAnimHeader.FrameSize )
				{
					ErrorBox(_T("File appending error: existing  per-frame data size differs (was %i, expected %i)") ,ExistingAnimHeader.FrameSize,  AnimHeader.FrameSize );
					FileError = 1;
					return 0;
				}
				OutFile.Seek( 0 );
				OutFile.Write( &AnimHeader, sizeof(AnimHeader));
				OutFile.Flush();
				// Move to end of file.
				OutFile.Seek( AppendSeekLocation );
			}

			// Write animated vertex data (all frames, all verts).
			for( INT f=0; f< VAVerts.Num(); f++)
			{
				OutFile.Write( &(VAVerts[f]), sizeof(FMeshVert));
			}

			OutFile.CloseFlush();

			if( OutFile.GetError()) ErrorBox(_T("Vertex anim save error."));
		}

	}

	//if( DOLOGVERT )
	//{
	//   //PopupBox(" Digested %i vertex animation frames total.  Skin ponts total  %i -appended at : [%i]  ", FrameCount, RefSkinPoints, AppendSeekLocation ); //#EDN
	//	AuxLog.Logf(" Digested %i vertex animation frames total.  Skin ponts total  %i ", FrameCount, RefSkinPoints );
	//	AuxLog.Close();
	//}

	return ( RefSkinPoints > 0 ) ? FrameCount : 0 ;
}





void GetBitmapName(TCHAR* Name, TCHAR* Path, Mtl* ThisMtl)
{
	Name[0] = 0;
	Texmap *tmap = ThisMtl->GetSubTexmap(ID_DI);
	if (tmap && tmap->ClassID() == Class_ID(BMTEX_CLASS_ID, 0))
	{
		// A bitmapped texture - access the UV tiling settings at time 0.
		BitmapTex *bmt = (BitmapTex*) tmap;
		//_tcscpy(Name,CleanString( bmt->GetMapName()) );

		TSTR BmpPath, BmpFile;
		SplitPathFile( TSTR(bmt->GetMapName()), &BmpPath, &BmpFile);

		_tcscpy(Name,CleanString(BmpFile));
		_tcscpy(Path,CleanString(BmpPath));
	}
}



//
// Digest a physique skin into a VActor. May consist of multiple meshes.
// Assumed GetPhysiqueStats already done.
// and also the reference skeleton present from DigestSkeleton.
//

int	SceneIFC::DigestSkin(VActor *Thing )
{

	// We need a digest of all objects in the scene that are meshes belonging
	// to the skin; then go over these and use the parent node for non-physique ones,
	// and physique bones digestion where possible.

	Thing->SkinData.Points.Empty();
	Thing->SkinData.Wedges.Empty();
	Thing->SkinData.Faces.Empty();
	Thing->SkinData.RawWeights.Empty();
	Thing->SkinData.RawMaterials.Empty();
	Thing->SkinData.Materials.Empty();
	Thing->SkinData.RawBMPaths.Empty();


	// Digest total of materials encountered
	if (DEBUGFILE)
	{
		TCHAR LogFileName[MAXPATHCHARS]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");
		_stprintf(LogFileName,_T("\\ActXSkin.LOG"));
		DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	if (OurSkins.Num() == 0) return 0; // No Physique, Max- or simple geometry 'skin' detected. Return with empty SkinData.

	DLog.Logf(" Starting skin digestion logfile \n");

	// Digest multiple meshes...
	for(INT i=0; i<OurSkins.Num(); i++)
	{
		DebugBox("SubMesh %i Points %i Wedges %i Faces %i",i,Thing->SkinData.Points.Num(),Thing->SkinData.Wedges.Num(),Thing->SkinData.Faces.Num());

		VSkin LocalSkin;
		LocalSkin.Points.Empty();
		LocalSkin.Wedges.Empty();
		LocalSkin.Faces.Empty();
		LocalSkin.RawWeights.Empty();
		LocalSkin.NumExtraUVSets = 0;

		FlippedFaces = 0;

		if( OurSkins[i].IsSmoothSkinned  )
		{
			// Skinned mesh.
			ProcessMesh( OurSkins[i].Node, OurSkins[i].SceneIndex, Thing, LocalSkin, 1, &OurSkins[i] );
		}
		else if ( OurSkins[i].IsTextured  )
		{
			// Textured geometry; assumed to be its own bone.
			// ProcessGeometryMesh( OurSkins[i].Node, Thing, LocalSkin );
			ProcessMesh( OurSkins[i].Node, OurSkins[i].SceneIndex, Thing, LocalSkin, 0, &OurSkins[i] );
		}

		if( FlippedFaces )
		{
			DebugBox(_T(" Flipped faces: %i on node: %s"),FlippedFaces,((INode*)OurSkins[i].Node)->GetName() );
		}

		DebugBox(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );

		// Add points, wedges, faces, RAW WEIGHTS from localskin to Thing->Skindata...
		// Indices need to be shifted to create a single mesh soup!

		INT PointBase = Thing->SkinData.Points.Num();
		INT WedgeBase = Thing->SkinData.Wedges.Num();

		if( LocalSkin.NumExtraUVSets > Thing->SkinData.NumExtraUVSets )
		{
			Thing->SkinData.NumExtraUVSets = LocalSkin.NumExtraUVSets;
		}

		Thing->SkinData.bHasVertexColors = LocalSkin.bHasVertexColors;

		for(INT i=0; i<LocalSkin.Points.Num(); i++)
		{
			Thing->SkinData.Points.AddItem(LocalSkin.Points[i]);
		}

		{for(INT i=0; i<LocalSkin.Wedges.Num(); i++)
		{
			VVertex Wedge = LocalSkin.Wedges[i];
			//adapt wedge's POINTINDEX...
			Wedge.PointIndex += PointBase;
			Thing->SkinData.Wedges.AddItem(Wedge);
		}}

		{for(INT i=0; i<LocalSkin.Faces.Num(); i++)
		{
			//WedgeIndex[3] needs replacing
			VTriangle Face = LocalSkin.Faces[i];
			// adapt faces indices into wedges...
			Face.WedgeIndex[0] += WedgeBase;
			Face.WedgeIndex[1] += WedgeBase;
			Face.WedgeIndex[2] += WedgeBase;
			Thing->SkinData.Faces.AddItem(Face);
		}}

		{for(INT i=0; i<LocalSkin.RawWeights.Num(); i++)
		{
			VRawBoneInfluence RawWeight = LocalSkin.RawWeights[i];
			// adapt RawWeights POINTINDEX......
			RawWeight.PointIndex += PointBase;
			Thing->SkinData.RawWeights.AddItem(RawWeight);
		}}
	}

	DebugBox( "Total raw materials [%i] ", Thing->SkinData.RawMaterials.Num() ); //#debug

	// before using RawMaterials, make sure it does not have NULL member
	// if NULL is found, please delete it
	for( INT i=0; i < Thing->SkinData.RawMaterials.Num(); i++)
	{
		if (Thing->SkinData.RawMaterials[i] == NULL)
		{
			Thing->SkinData.RawMaterials.DelIndex(i);
			--i;
		}
	}

	UBOOL IndexOverride = false;
	// Now digest all the raw materials we encountered for export, and build appropriate flags.

	for( INT i=0; i < Thing->SkinData.RawMaterials.Num(); i++)
	{
		VMaterial Stuff;
		Mtl* ThisMtl = (Mtl*) Thing->SkinData.RawMaterials[i];
		// Material name.
		Stuff.SetName( ThisMtl->GetName() );
		DebugBox( _T("Material: %s  [%i] "), ThisMtl->GetName(), i ); //#debug

		// Rendering flags.
		CalcMaterialFlags( ThisMtl, Stuff );
		// Reserved - for material sorting.
		Stuff.AuxMaterial = 0;

		// Skin index override detection.
		if (Stuff.AuxFlags ) IndexOverride = true;

		// Add material. Uniqueness already guaranteed.
		Thing->SkinData.Materials.AddItem( Stuff );

		VBitmapOrigin TextureSource;
		GetBitmapName( TextureSource.RawBitmapName, TextureSource.RawBitmapPath, (Mtl*)Thing->SkinData.RawMaterials[i] );
		Thing->SkinData.RawBMPaths.AddItem( TextureSource );

	}

	DebugBox( "Total unique materials [%i] ", Thing->SkinData.Materials.Num() );

	if ( DoQuadTex )
	{
		// Cut texture UV's for 1 texture into 4 numbered skins and stretch the UV's accordingly.
		// Add 4 new materials...

		INT SkinIndex[MAXSKININDEX];
		for( INT i=0; i<MAXSKININDEX; i++)
			SkinIndex[i]=0;

		// Gather material indices:
		for( INT i=0; i<Thing->SkinData.Wedges.Num(); i++)
		{
			INT Skin = min(Thing->SkinData.Wedges[i].MatIndex, MAXSKININDEX);
			SkinIndex[Skin]++;
		}

		// What's the main referenced skin ?
		INT MainMaterial = 0;
		INT MainCount = 0;
		{
			for( INT i=0; i<MAXSKININDEX; i++)
			{
				if( MainCount < SkinIndex[i])
				{
					MainCount = SkinIndex[i];
					MainMaterial = i;
				}
			}
		}

		// Quadruple the [MainMaterial] material and assing 0,1,2,4 textureindices
		INT NewIndex = Thing->SkinData.Materials.Num();
		VMaterial QuadTex = Thing->SkinData.Materials[MainMaterial];
		for( INT i=0; i<4; i++ )
		{
			QuadTex.TextureIndex = i;
			Thing->SkinData.Materials.AddItem(QuadTex);
		}

		// Re-index all the ones pointing to MainMaterial to NewIndex[0/1/2/3]
		for( INT i=0; i<Thing->SkinData.Wedges.Num(); i++)
		{
			INT Skin  = Thing->SkinData.Wedges[i].MatIndex;
			if( Skin == MainMaterial )
			{
				INT QuadIndex = 0;
				// rescale the U/V coordinates -> [0..0.5>,[0.5..1>  =  [0..1>,[0..1>
				FLOAT Uf = Thing->SkinData.Wedges[i].UV.U * 2.f;
				FLOAT Vf = Thing->SkinData.Wedges[i].UV.V * 2.f;

				// Assumed
				if ( Uf >= 1.0f)
				{
					Uf -= 1.f;
					QuadIndex +=1;
				}
				if ( Vf >= 1.0f)
				{
					Vf -= 1.f;
					QuadIndex +=2;
				}
				Thing->SkinData.Wedges[i].UV.U  = Uf;
				Thing->SkinData.Wedges[i].UV.V  = Vf;
				Thing->SkinData.Wedges[i].MatIndex = NewIndex + QuadIndex;
			}
		}
		DebugBox( "Automatic Quad texture cutting of [%s] complete.",Thing->SkinData.Materials[MainMaterial].MaterialName );
	}

	//
	// Sort Thing->materials in order of 'skin' assignment.
	// Auxflags = 1: explicitly defined material -> these have be sorted in their assigned positions;
	// if they are incomplete (e,g, skin00, skin02 ) then the 'skin02' index becomes 'skin01'...
	// Wedge indices  then have to be remapped also.
	//
	if ( !DoQuadTex && IndexOverride)
	{
		INT NumMaterials = Thing->SkinData.Materials.Num();
		// AuxMaterial will hold original index.
		for (int t=0; t<NumMaterials;t++)
		{
			Thing->SkinData.Materials[t].AuxMaterial = t;
		}

		// Bubble sort.
		int Sorted = 0;
		while (Sorted==0)
		{
			Sorted=1;
			for (int t=0; t<(NumMaterials-1);t++)
			{
				UBOOL SwapNeeded = false;
				if( Thing->SkinData.Materials[t].AuxFlags && (!Thing->SkinData.Materials[t+1].AuxFlags) )
				{
					SwapNeeded = true;
				}
				else
				{
					if( ( Thing->SkinData.Materials[t].AuxFlags && Thing->SkinData.Materials[t+1].AuxFlags ) &&
						( Thing->SkinData.Materials[t].TextureIndex > Thing->SkinData.Materials[t+1].TextureIndex ) )
						SwapNeeded = true;
				}
				if( SwapNeeded )
				{
					Sorted=0;

					VMaterial VMStore = Thing->SkinData.Materials[t];
					Thing->SkinData.Materials[t]   = Thing->SkinData.Materials[t+1];
					Thing->SkinData.Materials[t+1] = VMStore;

					VBitmapOrigin BOStore = Thing->SkinData.RawBMPaths[t];
					Thing->SkinData.RawBMPaths[t]   = Thing->SkinData.RawBMPaths[t+1];
					Thing->SkinData.RawBMPaths[t+1] = BOStore;
				}
			}
		}

		DebugBox("Re-sorted material order for skin-index override.");

		// Remap wedge material indices.
		if (1)
		{
			INT RemapArray[256];
			Memzero(RemapArray,256*sizeof(INT));
			{for( int t=0; t<NumMaterials;t++)
			{
				RemapArray[Thing->SkinData.Materials[t].AuxMaterial] = t;
			}}
			{for( int i=0; i<Thing->SkinData.Wedges.Num(); i++)
			{
				INT MatIdx = Thing->SkinData.Wedges[i].MatIndex;
				if ( MatIdx >=0 && MatIdx < Thing->SkinData.Materials.Num() )
					Thing->SkinData.Wedges[i].MatIndex =  RemapArray[MatIdx];
			}}
		}
	}

	DLog.Close();
	return 1;
}

struct SmoothVertInfluence
{
	float Weight;
	int   BoneIndex;

	//Ctors.
	SmoothVertInfluence()
	{}

	SmoothVertInfluence( float InWeight, int InBoneIndex )
	{
		Weight = InWeight;
		BoneIndex = InBoneIndex;
	}

};

void WeightsSortAndNormalize( TArray<SmoothVertInfluence>& RawInfluences )
{
	// Bubble sort weights.
	int Sorted = 0;
	while( Sorted==0 )
	{
		Sorted=1;
		for( int t=0; t<RawInfluences.Num()-1; t++ )
		{
			if ( RawInfluences[t].Weight < RawInfluences[t+1].Weight )
			{
				// Swap.
				SmoothVertInfluence TempInfluence = RawInfluences[t];
				RawInfluences[t] = RawInfluences[t+1];
				RawInfluences[t+1] = TempInfluence;
				Sorted=0;
			}
		}
	}
	// Renormalize weights.
	float WNorm = 0.0;
	{for( int t=0; t<RawInfluences.Num(); t++ )
	{
		WNorm += RawInfluences[t].Weight;
	}}
	{for( int t=0; t<RawInfluences.Num(); t++ )
	{
		RawInfluences[t].Weight *= 1.0f/WNorm;
	}}
}





int	SceneIFC::ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin, SkinInf* SkinHandle )
{
	// We need a digest of all objects in the scene that are meshes belonging
	// to the skin. Use each meshes' parent node if it is a non-physiqued one;
	// do physique bones digestion where possible.

	// Get vertexes (=points)
	// Get faces + allocate wedge for each face vertex.

	if( 0 )
	{
		DLog.Open(LogPath,_T("\\Aaaa.LOG"),DOLOGFILE);
	}

	if( SkinNode == NULL)
	{
		return 0;
	}

	// Get the selection state (should match SerialTree[TreeIndex].IsSelected. )
	UBOOL bMeshSelected = ((INode*)SkinNode)->Selected();

	// If requested ignore all but selected geometry.
	if( ( OurScene.DoSelectedGeom && !DoSkipSelectedGeom ) && !bMeshSelected )
	{
		return 0;
	}

	// If requested ignore all selected geometry.
	if( ( OurScene.DoSelectedGeom && DoSkipSelectedGeom ) && bMeshSelected  )
	{
		return 0;
	}

	// Get a tri object - at time TimeStatic.
	UBOOL needDel;
	TriObject* tri = GetTriObjectFromNode( SkinNode, TimeStatic, needDel);

	if (!tri)
	{
		return 0;
	}

	Mesh* OurTriMesh = &tri->mesh;
	if( !OurTriMesh )
	{
		ErrorBox(_T(" Invalid TriMesh Error. "));
		return 0;
	}

	// Get local node TM:
	Matrix3 SkinLocalTM = ((INode*)SkinNode)->GetObjectTM(TimeStatic);

	if (!SmoothSkin) // normal mesh
	{
		// GetNumberVertices returns the number of vertices for the given modContext's Object.
		INT NumberVertices = OurTriMesh->getNumVerts();

		DebugBox("Vertices total: %i",NumberVertices);

		if (DEBUGFILE && to_path[0])
		{
			DLog.Logf(" Mesh totals:  Points %i Faces %i \n", OurTriMesh->getNumVerts(),OurTriMesh->getNumFaces() );
		}

		DebugBox("Digesting geometry mesh - vertices %",NumberVertices);

		INT MatchedNode = Thing->MatchNodeToSkeletonIndex(SkinNode);

		// Is this node part of the hierarchy we digested earlier ? This should never occur.
		if( MatchedNode < 0 )
		{
			MatchedNode = 0;
		}

		// Get vertices + weights.
		for (int i=0; i< NumberVertices ; i++)
		{
			// SkinLocal brings the skin 3d vertices into world space.
			Point3 VxPos = OurTriMesh->getVert(i)*SkinLocalTM;
			VPoint NewPoint;
			NewPoint.Point = FVector(VxPos.x,VxPos.y,VxPos.z);
			LocalSkin.Points.AddItem(NewPoint);

			VRawBoneInfluence TempInf;
			TempInf.PointIndex = i; // raw skinvertices index
			TempInf.BoneIndex  =  MatchedNode; // Bone is it's own node.
			TempInf.Weight     = 1.0f; // Single influence by definition.

			LocalSkin.RawWeights.AddItem(TempInf);
		}

		DebugBox("Digested geometry mesh - Points %d",LocalSkin.Points.Num() );
	}

	int NodeMatchWarnings = 0;

	//
	// Process a smooth (Max- or Physique) skin geometry object.
	//
	if ( SmoothSkin )
	{

		//
		// Physique node encountered.
		//
		if ( SkinHandle->IsPhysique )
		{

			// Dump skin localizer matrix -> puts local vertices into world space.
			// But, - apparently puts physique vertices into 'parent' space only.
			// Get the physique modifier for this mesh.
			Modifier* PhyMod = FindPhysiqueModifier(SkinNode);

			if (!PhyMod)
			{
				ErrorBox(_T("NONPHYSIQUE error for mesh."));
				return 0;
			}

			// Given Physique Modifier Modifier *phyMod get the Physique Export Interface:
			IPhysiqueExport *phyExport = (IPhysiqueExport *)PhyMod->GetInterface(I_PHYINTERFACE);

			// For a given Object's INode get this ModContext Interface from the Physique Export Interface:
			IPhyContextExport *contextExport = (IPhyContextExport *)phyExport->GetContextInterface((INode*)SkinNode);

			// Allow multiple bone influences per vertex.
			contextExport->AllowBlending(true);

			// Sets BONES to rigid - instead of splines. Not to be confused
			// with the exporting of rigid (non-blended) vertices.
			contextExport->ConvertToRigid(true);

			// GetNumberVertices returns the number of vertices for the given modContext's Object.
			INT NumberVertices = contextExport->GetNumberVertices();

			// Get vertices + weights.
			for (int VertexIndex=0; VertexIndex< NumberVertices ; VertexIndex++)
			{

				IPhyVertexExport *vertexExport = (IPhyVertexExport *)contextExport->GetVertexInterface(VertexIndex);

				if (DEBUGFILE && to_path[0])
				{
					DLog.Logf(" Physique mesh totals:  Points %i Faces %i \n", NumberVertices, OurTriMesh->getNumFaces() );
				}

				//
				// Get the vertex type: in our game's case (rigid blended)
				// we only export the 2 cases: RIGID_NON_BLENDED_TYPE and RIGID_BLENDED_TYPE.
				//
				INT BlendType = vertexExport->GetVertexType();

				// GetNumberNodes() returns the number of nodes assigned to the given VertexInterface.
				UBOOL MultiInfluences = false;
				INT NumberBones = 1;

				if ( BlendType == RIGID_BLENDED_TYPE)
				{
					NumberBones = ((IPhyBlendedRigidVertex*)vertexExport)->GetNumberNodes();
					MultiInfluences = true;
				}

				TArray <SmoothVertInfluence> RawInfluences;

				if( NumberBones > 0 )
				{
					for (int BoneIndex=0; BoneIndex<NumberBones; BoneIndex++)
					{
						// GetNode() will return the INode pointer of the link of the given VertexInterface.
						// Get the INode for each Rigid Vertex Interface:

						SmoothVertInfluence NewInfluence( 0.0f, -1 );

						INode *nodePtr;
						if (!MultiInfluences)
							nodePtr = ((IPhyRigidVertex*)vertexExport)->GetNode();
						else
							nodePtr = ((IPhyBlendedRigidVertex*)vertexExport)->GetNode(BoneIndex);

						if (!MultiInfluences)
							NewInfluence.Weight =  1.0f;
						else
							NewInfluence.Weight = ((IPhyBlendedRigidVertex*) vertexExport)->GetWeight(BoneIndex);

						// Now find the bone and add the influence only if there was a nonzero weight.
						if( NewInfluence.Weight > 0.0f ) // SMALLESTWEIGHT constant instead ?
						{
							int NodeIndex = -1;
							for (int t=0; t<SerialTree.Num(); t++)
							{
								if ( (AXNode*)SerialTree[t].node == (AXNode*)nodePtr )
								{
									NodeIndex = t;
								}
							}

							if ( NodeIndex >= 0 )
							{
								SerialTree[ NodeIndex ].LinksToSkin++; // Count number of vertices !
							}

							NewInfluence.BoneIndex = Thing->MatchNodeToSkeletonIndex(nodePtr);

							// Node not found ?
							if( NewInfluence.BoneIndex < 0 )
							{
								NewInfluence.Weight = 0.0f;
								NodeMatchWarnings++;
							}
							RawInfluences.AddItem( NewInfluence );
						}
						else
						{
							ErrorBox(_T("Weight invalid: [%f] - fixing it up...."), NewInfluence.Weight);

							/*
							NewInfluence.Weight = 0.25f;

							int NodeIndex = -1;
							for (int t=0; t<SerialTree.Num(); t++)
							{
								if ( (AXNode*)SerialTree[t].node == (AXNode*)nodePtr )
								{
									NodeIndex = t;
								}
							}

							if ( NodeIndex >= 0 )
							{
								SerialTree[ NodeIndex ].LinksToSkin++; // Count number of vertices !
							}

							NewInfluence.BoneIndex = Thing->MatchNodeToSkeletonIndex(nodePtr);

							// Node not found ?
							if( NewInfluence.BoneIndex < 0 )
							{
								NewInfluence.Weight = 0.0f;
								NodeMatchWarnings++;
							}

							RawInfluences.AddItem( NewInfluence );
							*/
						}
					}
				}

				// Verify we found at least one influence for this vert.
				static INT PhysiqueTotalErrors = 0;
				if( RawInfluences.Num() == 0 )
				{
					ErrorBox(_T("Invalid number of physique bone influences (%i) for skin vertex %i"),NumberBones,VertexIndex);
					PhysiqueTotalErrors ++;
				}

				WeightsSortAndNormalize( RawInfluences );

				// Store all nonzero weights
				for( INT t=0; t<RawInfluences.Num(); t++ )
				{
					// Skip zero-weights and invalid bone indices.
					if( ( RawInfluences[t].Weight > 0.0f ) && ( RawInfluences[t].BoneIndex >= 0) )
					{
						VRawBoneInfluence TempInf;

						TempInf.PointIndex = VertexIndex;          // raw skinvertices index
						TempInf.BoneIndex  = RawInfluences[t].BoneIndex;
						TempInf.Weight     = RawInfluences[t].Weight;

						LocalSkin.RawWeights.AddItem(TempInf);
					}
				}

				//DebugBox("Total weights added %i for vert %i ",LocalSkin.RawWeights.Num(),i);

				// Digest vertex position.
				// Assumes the physique ordering equals the regular mesh ordering.
				// GetVertexInterface(i) -> i is the vertex index, should be same as getVert(i) !

				// SkinLocal brings the skin 3d vertices into world space
				Point3 VxPos = OurTriMesh->getVert(VertexIndex)*SkinLocalTM;
				VPoint NewPoint;
				NewPoint.Point = FVector(VxPos.x,VxPos.y,VxPos.z);
				LocalSkin.Points.AddItem(NewPoint);

				// You must call ReleaseVertexInterface to delete the VertexInterface when finished with it.
				contextExport->ReleaseVertexInterface( vertexExport );

			}// NumberVertices loop


			// New - finally using the proper cleanup here.
			phyExport->ReleaseContextInterface( contextExport );
			PhyMod->ReleaseInterface(I_PHYINTERFACE, phyExport );


		}
		else if( SkinHandle->IsMaxSkin )
		{
			//
			// Process a Max native skin.
			//

			Modifier* SkinMod = FindMaxSkinModifier(SkinNode);

			if( !SkinMod )
			{
				ErrorBox(_T("MAXSKIN not found - error for mesh."));
				return 0;
			}

			ISkin* MaxSkin = (ISkin*)SkinMod->GetInterface(I_SKIN);
			ISkinContextData* MaxSkinContext = MaxSkin->GetContextInterface( (INode*)SkinNode);

			// Digest.
			INT NumberVertices = MaxSkinContext->GetNumPoints();
			INT TotalBones     = MaxSkin->GetNumBones(); // Total for skin.
			INT TotalFaces     = OurTriMesh->getNumFaces();


			// Get vertices + weights.
			// Assumes MaxSkin's vertex ordering identical to final mesh export ordering.
			for (int VertexIndex=0; VertexIndex< NumberVertices ; VertexIndex++)
			{
				TArray <SmoothVertInfluence> RawInfluences;

				// Total bones influencing this vertex.
				INT NumberBones = MaxSkinContext->GetNumAssignedBones( VertexIndex );

				if( NumberBones > 0 )
				{
					//PopupBox(" BonesForVertex: %i ",NumberBones);

					for (int BoneIndex=0; BoneIndex<NumberBones; BoneIndex++)
					{
						// Retrieve the INode for this particular influence.

						SmoothVertInfluence NewInfluence( 0.0f, -1 );

						NewInfluence.Weight = MaxSkinContext->GetBoneWeight( VertexIndex, BoneIndex );
						INT LocalBoneIndex  = MaxSkinContext->GetAssignedBone( VertexIndex, BoneIndex );


						//DLog.Logf(" BoneLocalIdx, VertIdx, GlobalIdx: %i %i %i  W: %f \n",BoneIndex,VertexIndex,LocalBoneIndex, NewInfluence.Weight );

						INode *nodePtr;
						nodePtr = MaxSkin->GetBone( LocalBoneIndex );

						//
						// Now find the bone and add the influence only if there was a nonzero weight.
						//
						if( NewInfluence.Weight > 0.0f ) // SMALLESTWEIGHT constant instead ?
						{
							int NodeIndex = -1;
							for (int t=0; t<SerialTree.Num(); t++)
							{
								if ( (AXNode*)SerialTree[t].node == (AXNode*)nodePtr )
								{
									NodeIndex = t;
								}
							}

							// Keep track of number of links.
							if ( NodeIndex >= 0 )
							{
								SerialTree[ NodeIndex ].LinksToSkin++;
							}

							NewInfluence.BoneIndex = Thing->MatchNodeToSkeletonIndex(nodePtr);

							// Node not found ?
							if( NewInfluence.BoneIndex < 0 )
							{
								NewInfluence.Weight = 0.0f;
								NodeMatchWarnings++;
							}

							RawInfluences.AddItem( NewInfluence );
						}
					}
				}


				//
				// Verify we found at least one influence for this vert.
				//
				static INT MaxSkinTotalErrors = 0;
				if( (RawInfluences.Num() == 0) && (MaxSkinTotalErrors == 0) )
				{
					ErrorBox(_T("Invalid number of max skin influences (%i) for skin vertex %i"),NumberBones,VertexIndex);
					MaxSkinTotalErrors ++;
				}

				WeightsSortAndNormalize( RawInfluences );

				// Store all nonzero weights
				for( int t=0; t<RawInfluences.Num(); t++ )
				{
					// Skip zero-weights and invalid bone indices.
					if( ( RawInfluences[t].Weight > 0.0f ) && ( RawInfluences[t].BoneIndex >= 0) )
					{
						VRawBoneInfluence TempInf;

						TempInf.PointIndex = VertexIndex;          // raw skinvertices index
						TempInf.BoneIndex  = RawInfluences[t].BoneIndex;
						TempInf.Weight     = RawInfluences[t].Weight;

						LocalSkin.RawWeights.AddItem(TempInf);
					}
				}

				//
				// Native Max Skin modifier : when no matched node found whatsoever, the object itself is assumed the single influence parent..
				//

				if( RawInfluences.Num() == 0 )
				{
					INT MatchedNode = Thing->MatchNodeToSkeletonIndex(SkinNode);

					// If no match found, assume vertices welded to 'root' of scene.
					if( MatchedNode < 0 )
					{
						MatchedNode = 0;
					}

					VRawBoneInfluence TempInf;
					TempInf.PointIndex = VertexIndex;
					TempInf.BoneIndex = MatchedNode;
					TempInf.Weight = 1.0f;
					LocalSkin.RawWeights.AddItem(TempInf);
				}

				// Digest vertex position.
				// Assumes the physique ordering equals the regular mesh ordering.
				// GetVertexInterface(i) -> i is the vertex index, should be same as getVert(i) !
				// SkinLocal brings the skin 3d vertices into world space
				Point3 VxPos =  OurTriMesh->getVert(VertexIndex)*SkinLocalTM;
				VPoint NewPoint;
				NewPoint.Point = FVector(VxPos.x,VxPos.y,VxPos.z);
				LocalSkin.Points.AddItem(NewPoint);

			}// NumberVertices loop

			// Release.
			// MaxSkin->ReleaseContextInterface( MaxSkinContext ); // No context release function defined for ISkins themselves...
			SkinMod->ReleaseInterface( I_SKIN, MaxSkin );
		}
	}

	if( NodeMatchWarnings )
	{
		ErrorBox(_T("Warning: %8i 'Unmatched node ID' Physique skin vertex errors encountered."),(INT)NodeMatchWarnings);
		NodeMatchWarnings=0;
	}



	//
	// Digest any mesh triangle data -> into unique wedges (ie common texture UV'sand (later) smoothing groups.)
	//

	UBOOL NakedTri = false;
	int Handedness = 0;
	int NumFaces = OurTriMesh->getNumFaces();

	DebugBox("Digesting Normals");

	// Refresh normals.
	OurTriMesh->buildNormals(); //#Handedness

	// Get mesh-to-worldspace matrix
	Matrix3 Mesh2WorldTM = ((INode*)SkinNode)->GetObjectTM(TimeStatic);
	Mesh2WorldTM.NoScale();

	DebugBox("Digesting %i Triangles.",NumFaces);

	if( LocalSkin.Points.Num() ==  0 ) // Actual points allocated yet ? (Can be 0 if no matching bones were found)
	{
		NumFaces = 0;
		ErrorBox(_T("Node without matching vertices encountered for mesh: [%s])"), ((INode*)SkinNode)->GetName());
	}

	INT CountNakedTris = 0;
	INT CountTexturedTris = 0;
	INT TotalTVNum = OurTriMesh->getNumTVerts();


	// Get vertex color information.  This info will be used to determine if we have vertex colors
	const int NumVertexColors = OurTriMesh->getNumVertCol();
	VertColor* VertexColors = OurTriMesh->vertCol;

	for (int TriIndex = 0; TriIndex < NumFaces; TriIndex++)
	{
		Face*	TriFace			= &OurTriMesh->faces[TriIndex];
		TVFace*	TexturedTriFace	= &OurTriMesh->tvFace[TriIndex];

		VVertex Wedge0;
		VVertex Wedge1;
		VVertex Wedge2;

		VTriangle NewFace;

		NewFace.SmoothingGroups = TriFace->getSmGroup();

		// Normals (determine handedness...
		Point3 MaxNormal = OurTriMesh->getFaceNormal(TriIndex);
		// Transform normal vector into world space along with 3 vertices.
		MaxNormal = VectorTransform( Mesh2WorldTM, MaxNormal );

		FVector FaceNormal = FVector( MaxNormal.x, MaxNormal.y, MaxNormal.z);

		NewFace.WedgeIndex[0] = LocalSkin.Wedges.Num();
		NewFace.WedgeIndex[1] = LocalSkin.Wedges.Num()+1;
		NewFace.WedgeIndex[2] = LocalSkin.Wedges.Num()+2;

		//if (&LocalSkin.Points[0] == NULL )PopupBox("Triangle digesting 1: [%i]  %i %i",TriIndex,(DWORD)TriFace, (DWORD)&LocalSkin.Points[0]); //#! LocalSkin.Points[9] failed;==0..

		// Face's vertex indices ('point indices')
		Wedge0.PointIndex = TriFace->getVert(0);  // TriFace->v[0];
		Wedge1.PointIndex = TriFace->getVert(1);
		Wedge2.PointIndex = TriFace->getVert(2);

		// Vertex coordinates are in localskin.point, already in worldspace.
		FVector WV0 = LocalSkin.Points[ Wedge0.PointIndex ].Point;
		FVector WV1 = LocalSkin.Points[ Wedge1.PointIndex ].Point;
		FVector WV2 = LocalSkin.Points[ Wedge2.PointIndex ].Point;

		// Figure out the handedness of the face by constructing a normal and comparing it to Max's normal.
		FVector OurNormal = (WV0 - WV1) ^ (WV2 - WV0);
		// OurNormal /= sqrt(OurNormal.SizeSquared()+0.001f);// Normalize size (unnecessary ?);

		Handedness = ( ( OurNormal | FaceNormal ) < 0.0f); // normals anti-parallel ?

		BYTE MaterialIndex = TriFace->getMatID() & 255; // 255 & (TriFace->flags >> 32);  ???


		//NewFace.MatIndex = MaterialIndex;

		Wedge0.Reserved = 0;
		Wedge1.Reserved = 0;
		Wedge2.Reserved = 0;

		//
		// Get UV texture coordinates: allocate a Wedge with for each and every
		// UV+ MaterialIndex ID; then merge these, and adjust links in the Triangles
		// that point to them.
		//

		Wedge0.UV.U = 0; // bogus UV's.
		Wedge0.UV.V = 0;
		Wedge1.UV.U = 1;
		Wedge1.UV.V = 0;
		Wedge2.UV.U = 0;
		Wedge2.UV.V = 1;

		memset( Wedge2.ExtraUVs, 0, sizeof( FUVCoord ) * NUM_EXTRA_UV_SETS );

		// Jul 12 '01: -> Changed to uses alternate indicator - the HAS_TVERTS flag apparently not supported correctly in Max 4.X
		if( TotalTVNum > 0 )   // (TriFace->flags & HAS_TVERTS)
		{
			//  TVertex indices
			DWORD UVidx0 = TexturedTriFace->getTVert(0);
			DWORD UVidx1 = TexturedTriFace->getTVert(1);
			DWORD UVidx2 = TexturedTriFace->getTVert(2);

			// TVertices
			UVVert UV0 = OurTriMesh->getTVert(UVidx0);
			UVVert UV1 = OurTriMesh->getTVert(UVidx1);
			UVVert UV2 = OurTriMesh->getTVert(UVidx2);

			Wedge0.UV.U =  UV0.x;
			Wedge0.UV.V =  1.0f-UV0.y; //#debug flip Y always ?
			Wedge1.UV.U =  UV1.x;
			Wedge1.UV.V =  1.0f-UV1.y;
			Wedge2.UV.U =  UV2.x;
			Wedge2.UV.V =  1.0f-UV2.y;

			CountTexturedTris++;
		}
		else
		{
			CountNakedTris++;
			NakedTri = true;
		}

		INT LocalNumExtraUVs = 0;
		INT NumMapChannels = OurTriMesh->getNumMaps();
		// Skip map index 0 which is reserved for vertex colors
		// Skip map index 1 which is reserved for the first uv set which we already have.
		for( INT MapIndex = 2; MapIndex < NumMapChannels; ++MapIndex )
		{
			if( OurTriMesh->mapSupport( MapIndex ) )
			{
				TVFace* TvFaceArray = OurTriMesh->mapFaces(MapIndex);
				UVVert* TVertArray  = OurTriMesh->mapVerts(MapIndex);


				if( TvFaceArray && TVertArray )
				{

					TVFace& Face = TvFaceArray[ TriIndex ];
					const DWORD NumMapVerts = OurTriMesh->getNumMapVerts( MapIndex );

					DWORD UVidx0 = Face.getTVert(0);
					if( UVidx0 < NumMapVerts )
					{
						UVVert UV0 = TVertArray[UVidx0];
						Wedge0.ExtraUVs[ LocalNumExtraUVs ].U = UV0.x;
						Wedge0.ExtraUVs[ LocalNumExtraUVs ].V = 1.0f-UV0.y;
					}

					DWORD UVidx1 = Face.getTVert(1);
					if( UVidx1 < NumMapVerts )
					{
						UVVert UV1 = TVertArray[UVidx1];
						Wedge1.ExtraUVs[ LocalNumExtraUVs ].U = UV1.x;
						Wedge1.ExtraUVs[ LocalNumExtraUVs ].V = 1.0f-UV1.y;
					}

					DWORD UVidx2 = Face.getTVert(2);
					if( UVidx2 < NumMapVerts )
					{
						UVVert UV2 = TVertArray[UVidx2];
						Wedge2.ExtraUVs[ LocalNumExtraUVs ].U = UV2.x;
						Wedge2.ExtraUVs[ LocalNumExtraUVs ].V = 1.0f-UV2.y;
					}

				}

				if( LocalNumExtraUVs + 1 == NUM_EXTRA_UV_SETS )
				{
					break;
				}

				++LocalNumExtraUVs;
			}

		}

		if( LocalSkin.NumExtraUVSets < LocalNumExtraUVs )
		{
			LocalSkin.NumExtraUVSets = LocalNumExtraUVs;
		}

		//
		// This should give a correct final material index EVEN for multiple multi/sub materials on complex hierarchies of meshes.
		//

		INT ThisMaterial = DigestMaterial(  SkinNode,  MaterialIndex,  Thing->SkinData.RawMaterials );

		NewFace.MatIndex = ThisMaterial;

		Wedge0.MatIndex = ThisMaterial;
		Wedge1.MatIndex = ThisMaterial;
		Wedge2.MatIndex = ThisMaterial;

		if( VertexColors && NumVertexColors > 0 )
		{
			// There are vertex colors in this mesh.
			// Get the face containing vertex color index information from the current index
			TVFace& VertexColorFace = OurTriMesh->vcFace[TriIndex];

			// Get the index of the vertex color at each vertex in the current face
			int VertexColorIndex = VertexColorFace.t[0];
			Point3& VertexColor = VertexColors[VertexColorIndex];
			Wedge0.Color = VColor( BYTE( VertexColor.z * 255.f ), BYTE( VertexColor.y * 255.f ), BYTE( VertexColor.x * 255.f ), 255 );

			VertexColorIndex = VertexColorFace.t[1];
			VertexColor = VertexColors[VertexColorIndex];
			Wedge1.Color = VColor( BYTE( VertexColor.z * 255.f ), BYTE( VertexColor.y * 255.f ), BYTE( VertexColor.x * 255.f ), 255 );

			VertexColorIndex = VertexColorFace.t[2];
			VertexColor = VertexColors[VertexColorIndex];
			Wedge2.Color = VColor( BYTE( VertexColor.z * 255.f ), BYTE( VertexColor.y * 255.f ), BYTE( VertexColor.x * 255.f ), 255 );
			LocalSkin.bHasVertexColors = true;

		}
		else
		{
			// The mesh has no vertex colors
			LocalSkin.bHasVertexColors = false;
		}

		LocalSkin.Wedges.AddItem(Wedge0);
		LocalSkin.Wedges.AddItem(Wedge1);
		LocalSkin.Wedges.AddItem(Wedge2);

		LocalSkin.Faces.AddItem(NewFace);
		if (DEBUGFILE && to_path[0]) DLog.Logf(" [tri %4i ] Wedge UV  %f  %f  MaterialIndex %i  Vertex %i \n",TriIndex,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].UV.U ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].UV.V ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].MatIndex, LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].PointIndex );
	}

	if (NakedTri)
	{
		TSTR NodeName( ((INode*)SkinNode)->GetName() );
		int numTexVerts = OurTriMesh->getNumTVerts();
		WarningBox(_T("Triangles [%i] without UV mapping detected for mesh object: %s "), CountNakedTris, NodeName );
	}

	DebugBox( "Digested %i Triangles.", NumFaces );

	// DIGEST the wedges. Merge _all_ non-unique ones, but keep most of the current ordering intact..\
	// Wedges: special vertices that have the
	// texture UV enclosed. (Material index: in Unreal, implied, = that of the triangle pointing to 'em.)

	DebugBox("Merging Wedges: ");

	TArray  <VVertex> NewWedges;
	TArray  <int>     WedgeRemap;

	WedgeRemap.SetSize( LocalSkin.Wedges.Num() );

	DebugBox( "Digesting %i Wedges.", LocalSkin.Wedges.Num() );

	// Get wedge, see which others match and flag those,
	// then store wedge, get next nonflagged one. O(n^2/2)

	for( int t=0; t< LocalSkin.Wedges.Num(); t++ )
	{
		if( LocalSkin.Wedges[t].Reserved != 0xFF ) // not flagged ?
		{
			// Remember this wedge's unique new index.
			WedgeRemap[t] = NewWedges.Num();
			NewWedges.AddItem( LocalSkin.Wedges[t] ); // then it's unique.

			// Any copies ?
			for (int d=t+1; d< LocalSkin.Wedges.Num(); d++)
			{
				if ( Thing->IsSameVertex( LocalSkin.Wedges[t], LocalSkin.Wedges[d], LocalSkin.NumExtraUVSets  ) )
				{
					LocalSkin.Wedges[d].Reserved = 0xFF;
					WedgeRemap[d] = NewWedges.Num()-1;
				}
			}
		}
	}

	//
	// Re-point all indices from all Triangles to the proper unique-ified Wedges.
	//

	for (INT TriIndex = 0; TriIndex < NumFaces; TriIndex++)
	{
		LocalSkin.Faces[TriIndex].WedgeIndex[0] = WedgeRemap[TriIndex*3+0];
		LocalSkin.Faces[TriIndex].WedgeIndex[1] = WedgeRemap[TriIndex*3+1];
		LocalSkin.Faces[TriIndex].WedgeIndex[2] = WedgeRemap[TriIndex*3+2];

		// Verify we all mapped within new bounds.
		if (LocalSkin.Faces[TriIndex].WedgeIndex[0] >= NewWedges.Num()) ErrorBox(_T("Wedge Overflow 1"));
		if (LocalSkin.Faces[TriIndex].WedgeIndex[1] >= NewWedges.Num()) ErrorBox(_T("Wedge Overflow 1"));
		if (LocalSkin.Faces[TriIndex].WedgeIndex[2] >= NewWedges.Num()) ErrorBox(_T("Wedge Overflow 1"));

		// Flip faces?
		if( Handedness )
		{
			FlippedFaces++;
			INT Index1 = LocalSkin.Faces[TriIndex].WedgeIndex[2];
			LocalSkin.Faces[TriIndex].WedgeIndex[2] = LocalSkin.Faces[TriIndex].WedgeIndex[1];
			LocalSkin.Faces[TriIndex].WedgeIndex[1] = Index1;
		}
	}


	//  Replace old array with new.
	LocalSkin.Wedges.SetSize( 0 );
	LocalSkin.Wedges.SetSize( NewWedges.Num() );

	DebugBox("WedgeUV start");

	{for( int t=0; t<NewWedges.Num(); t++ )
	{
		LocalSkin.Wedges[t] = NewWedges[t];
		if (DEBUGFILE && to_path[0])
		{
			DLog.Logf(" [ %4i ] Wedge UV  %f  %f  Material %i  Vertex %i \n",t,NewWedges[t].UV.U,NewWedges[t].UV.V,NewWedges[t].MatIndex,NewWedges[t].PointIndex );
		}
	}}

	if (DEBUGFILE && to_path[0])
	{
		DLog.Logf(" Digested totals: Wedges %i Points %i Faces %i \n", LocalSkin.Wedges.Num(),LocalSkin.Points.Num(),LocalSkin.Faces.Num());
	}

	if (needDel)
	{
		delete tri;
	}

	if( 0 )
	{
		DLog.Close();
	}

	return 1;
};



void SceneIFC::SurveyScene()
{
	// Scene tree fill.
	SerializeSceneTree();
}



//
// See if material is unique, add to material list if so.
// See the Max SDK: Asciiexp : export.cpp : Material and Texture Export
//
int SceneIFC::DigestMaterial(AXNode *node,  INT matIndex,  TArray<void*>& RawMaterials)
{
	// Get the material from the node
	Mtl* nodeMtl = ((INode*)node)->GetMtl();

	// This should be checked for in advance.
	if (!nodeMtl)
	{
		return -1; //255; // No material assigned
	}

	// We know the Standard material, so we can get some extra info
	if (nodeMtl->ClassID() == Class_ID(DMTL_CLASS_ID, 0))
	{
		StdMat* std = (StdMat*)nodeMtl;
	}

	Mtl* ThisMtl;

	// Any sub materials ??
	int NumSubs = nodeMtl->NumSubMtls();

	if (NumSubs > 0)
	{
		ThisMtl = nodeMtl->GetSubMtl(matIndex % NumSubs);
	}
	else
	{
		ThisMtl = nodeMtl;
	}

	// The different material properties can be investigated at the end, for now we care about uniqueness only.
	// need to enter to the array of the matIndex to make sure it exports in the right order
	// resize if it's smaller
	if ( RawMaterials.Num()-1 < matIndex)
	{
		RawMaterials.AddZeroed(matIndex-RawMaterials.Num()+1);
	}

	// if still not set, please set
	if (RawMaterials[matIndex] == NULL)
	{
		RawMaterials[matIndex] = ThisMtl;
	}

	return matIndex;
}

/*
UBOOL ExecuteBatchFile( char* batchfilename, SceneIFC* OurScene )
{
	return true;
}
*/


UBOOL BatchAnimationProcess( const TCHAR* batchfoldername, INT &NumDigested, INT &NumFound, MaxPluginClass *u )
{
	// Process all Max animation files from the batchfoldername into the animation manager with default
	// properties, and use the Max names as animation names.
	// (use OurScene for digestion.)
	WIN32_FIND_DATA data;

	TSTR search;
	search.printf(_T("%s%s"), batchfoldername, _T("\\*.max"));
	HANDLE h = FindFirstFile( search.data(), &data);

	UBOOL FoundFile = ( h != INVALID_HANDLE_VALUE);

	NumFound = 0;
	NumDigested = 0;
	INT Errors = 0;

	OurScene.InBatchMode = 1; // Set batch mode flag to suppress notifications like 'dummies culled' popups during batch.

	while( FoundFile && !Errors )
	{
		NumFound++;

		TSTR fname;
		fname.printf(_T("%s%s%s"),batchfoldername,_T("\\"),data.cFileName);

		// PopupBox("Processing Max file:  [%s] intf: %i", fname.data(), (DWORD)TheMaxInterface );

		// LOAD max file.
		if( ! TheMaxInterface->LoadFromFile( fname ),false )
		{
			PopupBox(_T("Error: trouble loading file: %s - aborting batch load."), fname.data() );
			Errors++;
			break;
		}

		TCHAR rawfilename[400];
		// Now put name into newsequencename..
		_stprintf(rawfilename, data.cFileName );
		RemoveExtString(rawfilename, _T(".max"), newsequencename );
		//PopupBox("Raw anim name: [%s]",newsequencename );

		// Scene stats
		// DebugBox("Start serializing scene tree");

#if 1
		OurScene.SurveyScene();

		if( OurScene.SerialTree.Num() > 1 && newsequencename[0]) // Anything detected; valid anim name ?
		{
			//TheMaxInterface->ProgressStart(_T(" Digesting Animation... "), true, fn, NULL );

			DebugBox("Start getting scene info; Scene tree size: %i",OurScene.SerialTree.Num() );
			OurScene.GetSceneInfo();

			DebugBox("Start evaluating Skeleton");
			OurScene.EvaluateSkeleton(1);

			DebugBox("Start digest Skeleton");
			OurScene.DigestSkeleton(&TempActor);

			DebugBox("Start digest Anim");
			OurScene.DigestAnim(&TempActor, newsequencename, framerangestring );

			OurScene.FixRootMotion( &TempActor );

			// PopupBox("Digested animation:%s RawAnimKeys: %i Current animation # %i", newsequencename, TempActor.RawAnimKeys.Num(), TempActor.Animations.Num() );
			INT DigestedKeys = TempActor.RecordAnimation();

			//PopupBox( " Animation sequence %s digested. Total anims: %i ",newsequencename,  TempActor.Animations.Num() );
			if (DigestedKeys>0) NumDigested++;

		}
#endif

		FoundFile = FindNextFile(h, &data);

	}

	if( h != INVALID_HANDLE_VALUE )
		FindClose(h);

	OurScene.InBatchMode = 0;

	return true;
}




void DoDigestAnimation()
{
	// Scene stats
	DebugBox("Start serializing scene tree; serialtree elements: %i ",  OurScene.SerialTree.Num() );
	OurScene.SurveyScene();

	if( OurScene.SerialTree.Num() > 1 ) // Anything detected ?
	{
		if( GlobalPluginObject )
		{
			GlobalPluginObject->ip->ProgressStart(_T(" Digesting Animation... "), true, fn, NULL ); // Max-specific progress metering.
		}

		DebugBox("Start getting scene info; Scene tree size: %i",OurScene.SerialTree.Num() );
		OurScene.GetSceneInfo();

		DebugBox("Start evaluating Skeleton.");
		OurScene.EvaluateSkeleton(1);

		DebugBox("Start digesting of Skeleton.");
		OurScene.DigestSkeleton(&TempActor);

		DebugBox("Start digest Anim.");
		OurScene.DigestAnim(&TempActor, newsequencename, framerangestring );

		OurScene.FixRootMotion( &TempActor );

		// PopupBox("Digested animation:%s Betakeys: %i Current animation # %i", newsequencename, TempActor.BetaKeys.Num(), TempActor.Animations.Num() );

		if( OurScene.DoForceRate )
		{
			TempActor.FrameRate = OurScene.PersistentRate;
		}

		DebugBox("Start recording animation");
		INT DigestedKeys = TempActor.RecordAnimation();

		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox(_T("Animation digested: [%s] total frames: %i  total keys: %i  "), newsequencename, OurScene.FrameList.GetTotal(), DigestedKeys);
		}

		if( GlobalPluginObject )
		{
			GlobalPluginObject->ip->ProgressEnd();
		}
	}
}



void SaveCurrentSceneSkin()
{
	// Scene stats. #debug: surveying should be done only once ?
	OurScene.SurveyScene();
	OurScene.GetSceneInfo();
	OurScene.EvaluateSkeleton(1);

	//PopupBox("Start digest Skeleton");
	OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

	// Skin
	OurScene.DigestSkin( &TempActor );

	if( OurScene.DoUnSmooth || OurScene.DoTangents )
	{
		INT VertsDuplicated = OurScene.DoUnSmoothVerts( &TempActor, OurScene.DoTangents ); // Optionally splits on UV tangent space handedness, too.

		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox(_T("Unsmooth groups processing: [%i] vertices added."), VertsDuplicated );
		}
	}

	//DLog.Logf(" Skin vertices: %6i\n",Thing->SkinData.Points.Num());

	if( OurScene.DuplicateBones > 0 )
	{
		WarningBox(_T(" Non-unique bone names detected - model not saved. Please check your skeletal setup."));
	}
	else if( TempActor.SkinData.Points.Num() == 0)
	{
		WarningBox(_T(" Warning: No valid skin triangles digested (mesh may lack proper mapping or valid linkups)"));
	}
	else
	{
		// WarningBox(" Warning: Skin triangles found: %i ",TempActor.SkinData.Points.Num() );

		// TCHAR MessagePopup[512];
		const TCHAR* to_ext = _T("psk");
		_stprintf(DestPath,_T("%s\\%s.%s"), to_path,  to_skinfile, to_ext);
		FastFileClass OutFile;

		if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
			ErrorBox( _T("Skin File creation error. "));

		bool ok = TempActor.SerializeActor(OutFile); //save skin

		// Close.
		OutFile.CloseFlush();
		if (!ok) return;
		if( OutFile.GetError()) ErrorBox(_T("Skin Save Error"));

		// Log materials and skin stats.
		if( OurScene.DoLog )
		{
			OurScene.LogSkinInfo( &TempActor, to_skinfile );
		}

		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox(_T(" Skin file %s.%s written."), to_skinfile, to_ext );
		}

	}
}
