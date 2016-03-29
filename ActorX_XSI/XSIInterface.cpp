// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

   XSIInterface.cpp - XSI-specific exporter scene interface code.

   Dominic Laflamme - SOFTIMAGE

  Changelist:
          - Nov 2004 - Initial check-in

***************************************************************************/
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#include "SceneIFC.h"
#include "XSIInterface.h"
#include "ActorX.h"
#include "BrushExport.h"

#include <malloc.h>	// for alloca

#include <xsi_application.h>
#include <xsi_command.h>
#include <xsi_argument.h>
#include <xsi_context.h>
#include <xsi_menuitem.h>
#include <xsi_menu.h>
#include <xsi_model.h>
#include <xsi_property.h>
#include <xsi_project.h>
#include <xsi_selection.h>
#include <xsi_polygonmesh.h>
#include <xsi_geometry.h>
#include <xsi_primitive.h>
#include <xsi_envelope.h>
#include <xsi_material.h>
#include <xsi_ogltexture.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_point.h>
#include <xsi_triangle.h>
#include <xsi_uitoolkit.h>
#include <xsi_progressbar.h>
#include <xsi_menuitem.h>
#include <xsi_menu.h>
#include <xsi_decl.h>

#include <winuser.h>
#include <xsi_context.h>

// uncomment this to add the vertex animation tab
//#define VERTEX_ANIMATION_SUPPORT

// Globals
INT FlippedFaces;
static XSIInterface*	g_XSIInterface = NULL;
void W2AHelper( LPSTR out_sz, LPCWSTR in_wcs, int in_cch = -1);
bool g_NoRecursion = false;

extern SceneIFC	OurScene;


// TODO XSI: Forward declarations.
//UBOOL ExecuteBatchFile( char* batchfilename, SceneIFC* OurScene );
//UBOOL BatchAnimationProcess( char* batchfoldername, INT &NumDigested, INT &NumFound, MaxPluginClass *u );

//////////////////////////////////////////////////////////////////////////////////
//
// dialog procs
//
//////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK SceneInfoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ActorManagerDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK AuxDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK VertexDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AboutBoxDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK XSIPanelDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void DoGlobalDataExchance ();

//////////////////////////////////////////////////////////////////////////////////
//
// fwd decl
//
// TODO XSI: Move this into .h
//
//////////////////////////////////////////////////////////////////////////////////
void ToggleControlEnable( HWND hWnd, int control, BOOL state ) ;
bool GetCheckBox ( HWND hWnd, int in_iControl ) ;
static BOOL CALLBACK ActorXDlgProc(	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
void RemoveBorder ( HWND in_hWnd );
int  GetFolder ( char *out_szPath );
bool IsSelected ( XSI::X3DObject in_obj );
void LogThis ( XSI::CString in_cstr );
XSI::MATH::CVector3 GetTriangleNormal ( XSI::Triangle in_tri );
void CalcMaterialFlags(XSI::Material mat, VMaterial& Stuff );
int DigestLocalMaterial(XSI::Material, TArray<void*> &RawMaterials);
bool FindCustomPSet(XSI::X3DObject &node, const XSI::CString& name, XSI::CustomProperty &prop);

//////////////////////////////////////////////////////////////////////////////////
//
// globals
//
//////////////////////////////////////////////////////////////////////////////////
extern WinRegistry PluginReg;
HINSTANCE ParentApp_hInstance;
int controlsInit = FALSE;

//////////////////////////////////////////////////////////////////////////////////
//
// The following was copied from MAXInterface
//
//////////////////////////////////////////////////////////////////////////////////

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


// This function is called by Windows when the DLL is loaded.  This
// function may also be called many times during time critical operations
// like rendering.  Therefore developers need to be careful what they
// do inside this function.  In the code below, note how after the DLL is
// loaded the first time only a few statements are executed.

BOOL WINAPI DllMain( HINSTANCE hinstDLL,ULONG fdwReason,LPVOID lpvReserved )
{
	ParentApp_hInstance = hinstDLL;				// Hang on to this DLL's instance handle.


	return (TRUE);
}

//********************************************************************
//
// @mfunc	XSILoadPlugin | Called by XSI to register the DLL and
//							the various plugin it hosts.
//							See the SDK documentation for more info.
//
//
//********************************************************************
XSI::CStatus XSILoadPlugin( XSI::PluginRegistrar& in_reg )
{

	in_reg.PutAuthor( L"Epic" );
	in_reg.PutName( L"ActorX" );
	in_reg.PutVersion( 1, 0 );

	//
	// register commands
	//

	in_reg.RegisterCommand( L"ActorX", L"ActorX" );
	in_reg.RegisterCommand( L"ActorManager", L"ActorManager" );
	in_reg.RegisterCommand( L"ActorXSetup", L"ActorXSetup" );
	in_reg.RegisterCommand( L"ActorXAbout", L"ActorXAbout" );
	in_reg.RegisterCommand( L"ActorXVertex", L"ActorXVertex" );
	in_reg.RegisterCommand( L"ActorXSceneInfo", L"ActorXSceneInfo" );
	in_reg.RegisterCommand( L"ActorXSaveMesh", L"ActorXSaveMesh" );
	in_reg.RegisterCommand( L"ActorXDigestAnimation", L"ActorXDigestAnimation" );
	in_reg.RegisterCommand( L"ActorXSetValue", L"ActorXSetValue" );
	in_reg.RegisterCommand( L"ActorXExportStaticMesh", L"ActorXExportStaticMesh" );

	//
	// register menu entries
	//

	in_reg.RegisterMenu(XSI::siMenuMainFileExportID, L"ActorXStaticMesh", false, false);
	in_reg.RegisterMenu(XSI::siMenuMainFileExportID, L"ActorXMenu", false, false);

	return XSI::CStatus::OK;
}

//********************************************************************
//
// @mfunc	XSILoadPlugin | Called by XSI when it needs to unload the
//							plugin.
//
//
//********************************************************************
XSI::CStatus XSIUnloadPlugin( const XSI::PluginRegistrar& in_reg )
{

	if( g_XSIInterface != NULL )
	{
		//PopupBox("Deleting g_XSIInterface !");
		delete g_XSIInterface;
	}
	else
	{
		//PopupBox("g_XSIinterface is NULL on exit");
	}

	return XSI::CStatus::OK;
}

//********************************************************************
//
//
//
// XSI Command Entry Points
//
//
//
//********************************************************************
XSI::CStatus ActorX_Init( const XSI::CRef& in_context )
{

	//#EDN - is this the right place ?
	// Reset: mainly to initialize the persistent settings through registry paths.
	OurScene.DoLog = 1;
	ResetPlugin();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorX_Execute( XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowActorX ();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorManager_Init( const XSI::CRef& in_context ) {	return XSI::CStatus::OK;}
XSI::CStatus ActorManager_Execute( XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowActorManager ();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXSetup_Init( const XSI::CRef& in_context ) {	return XSI::CStatus::OK; }
XSI::CStatus ActorXSetup_Execute( XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowActorXSetup();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXVertex_Init( const XSI::CRef& in_context ) { return XSI::CStatus::OK; }
XSI::CStatus ActorXVertex_Execute( const XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowVertex();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXAbout_Init( const XSI::CRef& in_context ) { return XSI::CStatus::OK; }
XSI::CStatus ActorXAbout_Execute( const XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowAbout();

	return XSI::CStatus::OK;
}


XSI::CStatus ActorXSceneInfo_Init( const XSI::CRef& in_context ) { return XSI::CStatus::OK;}
XSI::CStatus ActorXSceneInfo_Execute( const XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowSceneInfo();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXStaticMesh_Init( XSI::CRef& in_ref )
{
	using namespace XSI;
	Context ctxt = in_ref;
	Menu menu = ctxt.GetSource();

	CStatus st;
	MenuItem item;
	menu.AddCallbackItem(L"Export ASE...", L"OnExportASE", item);


	return CStatus::OK;
}

XSI::CStatus ActorXMenu_Init( XSI::CRef& in_ref )
{
	using namespace XSI;
	Context ctxt = in_ref;
	Menu menu = ctxt.GetSource();

	CStatus st;
	MenuItem item;
	menu.AddCallbackItem(L"ActorX...", L"OnActorX", item);


	return CStatus::OK;
}

XSI::CStatus OnActorX( XSI::CRef& in_ref )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ShowActorX ();


	return XSI::CStatus::OK;
}


XSI::CStatus OnExportASE( XSI::CRef& in_ref )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ExportStaticMesh ();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXSetValue_Init( const XSI::CRef& in_context )
{
	XSI::Context ctx(in_context);
	XSI::Command cmd(ctx.GetSource());

	cmd.EnableReturnValue( true ) ;

	XSI::ArgumentArray args = cmd.GetArguments();
	args.Add( L"arg0", L"\0" );	// parameter name
	args.Add( L"arg1", XSI::CValue() );

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXSetValue_Execute( const XSI::CRef& in_context )
{

	XSI::Context ctxt(in_context);
	XSI::CValueArray args = ctxt.GetAttribute( L"Arguments" );

	XSI::CString l_szParameterName(args[0]);
	XSI::CValue l_vValue (args[1]);

	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	ActorXParam* l_pParam = g_XSIInterface->GetParameter ( l_szParameterName );
	if ( l_pParam != NULL )
	{
		switch ( l_vValue.m_t )
		{
			case XSI::siFloat:
			case XSI::siDouble:
				{
					float fval = (float)l_vValue;
					char valuesz[256];
					sprintf ( valuesz, "%f", fval );
					SetDlgItemText ( l_pParam->m_hParent, l_pParam->m_iID, valuesz );
					break;
				}

			case XSI::siBool:
				{
					bool bval = (bool)l_vValue;
					SendDlgItemMessage(l_pParam->m_hParent, l_pParam->m_iID, BM_SETCHECK , bval ? BST_CHECKED:BST_UNCHECKED,0);
					break;
				}
			case XSI::siString:
			case XSI::siWStr:
				{
					XSI::CString strval = (XSI::CString)l_vValue;

					char *strsz = new char [ strval.Length() + 1 ];
					W2AHelper ( strsz, strval.GetWideString() );
					SetDlgItemText ( l_pParam->m_hParent, l_pParam->m_iID, strsz );
					delete [] strsz;

					break;
				}
		}

		g_XSIInterface->SaveParameters ();
		g_XSIInterface->DoGlobalDataExchange ();
	}

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXDigestAnimation_Init( const XSI::CRef& in_context ) { return XSI::CStatus::OK;}
XSI::CStatus ActorXDigestAnimation_Execute( const XSI::CRef& in_context )
{
	// Scene stats
	DebugBox("Start serializing scene tree");
	OurScene.SurveyScene();

	if( OurScene.SerialTree.Num() > 1 ) // Anything detected ?
	{
		// TODO XSI
		// u->ip->ProgressStart(_T(" Digesting Animation... "), true, fn, NULL );

		DebugBox("Start getting scene info; Scene tree size: %i",OurScene.SerialTree.Num() );
		OurScene.GetSceneInfo();

		DebugBox("Start evaluating Skeleton");
		OurScene.EvaluateSkeleton(1);

		DebugBox("Start digest Skeleton");
		OurScene.DigestSkeleton(&TempActor);

		g_XSIInterface->SaveParameters ();
		g_XSIInterface->DoGlobalDataExchange ();

		DebugBox("Start digest Anim");
		OurScene.DigestAnim(&TempActor, animname, framerangestring );

		OurScene.FixRootMotion( &TempActor );

		// PopupBox("Digested animation:%s RawAnimKeys: %i Current animation # %i", animname, TempActor.RawAnimKeys.Num(), TempActor.Animations.Num() );
		INT DigestedKeys = TempActor.RecordAnimation();



		//PopupBox( " Animation sequence %s digested. Total anims: %i ",animname, TempActor.Animations.Num() );
	}
	// TODO XSI
	// u->ip->ProgressEnd();

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXSaveMesh_Init( const XSI::CRef& in_context ) { return XSI::CStatus::OK;}
XSI::CStatus ActorXSaveMesh_Execute( const XSI::CRef& in_context )
{
	// Scene stats. #debug: surveying should be done only once ?
	DebugBox("Start serializing scene tree");
	OurScene.SurveyScene();

	DebugBox("Start getting scene info");
	OurScene.GetSceneInfo();

	DebugBox("Start evaluating Skeleton");
	OurScene.EvaluateSkeleton(1);

	// Skeleton+skin gathering
	DebugBox("Start digest Skeleton");
	OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

	DebugBox("Start digest Skin");
	OurScene.DigestSkin(&TempActor );

	if( OurScene.DoUnSmooth || OurScene.DoTangents )
	{
		INT VertsDuplicated = OurScene.DoUnSmoothVerts( &TempActor, OurScene.DoTangents ); // Optionally splits on UV tangent space handedness, too.
		PopupBox("Unsmooth groups processing: [%i] vertices added.", VertsDuplicated );
	}

	if( OurScene.DuplicateBones > 0 )
	{
		WarningBox(" Non-unique bone names detected - model not saved. Please check your skeletal setup.");
	}
	else if( TempActor.SkinData.Points.Num() == 0)
	{
		WarningBox(" Warning: No valid skin triangles digested (is the 'all textured geometry' box checked?) - there may be invalid mesh linkups or texture mapping.");
	}
	else
	{
		// Save.

		// TCHAR MessagePopup[512];
		TCHAR to_ext[32];
		_tcscpy(to_ext, _T("PSK"));
		sprintf(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_skinfile,to_ext);
		FastFileClass OutFile;
		DebugBox("DestPath: %s",DestPath);
		if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
			ErrorBox( "File creation error. ");


		DebugBox("Start serializing...");
						  TempActor.SerializeActor(OutFile);
						  DebugBox("End serializing...");

						  DebugBox("Closing....");
						  // Close.
						  OutFile.CloseFlush();
						  if( OutFile.GetError()) ErrorBox("Skin Save Error");



						  DebugBox("Logging skin info:");
						  // Log materials and skin stats.
						  if( OurScene.DoLog )
						  {
							  OurScene.LogSkinInfo( &TempActor, to_skinfile );
							  DebugBox("Logged skin info:");
						  }

						  // sprintf(MessagePopup, "Skin file %s.%s written.",to_skinfile,to_ext);
						  // MessageBox(GetActiveWindow(),MessagePopup, "Saved OK", MB_OK);
						  PopupBox(" Skin file %s.%s written.",to_skinfile,to_ext );
	}

	return XSI::CStatus::OK;
}

XSI::CStatus ActorXExportStaticMesh_Init( const XSI::CRef& in_context ) {	return XSI::CStatus::OK;}
XSI::CStatus ActorXExportStaticMesh_Execute( XSI::CRef& in_context )
{
	if ( !g_XSIInterface )
	{
		g_XSIInterface = new XSIInterface();
	}

	g_XSIInterface->ExportStaticMesh ();

	return XSI::CStatus::OK;
}

//********************************************************************
//
// @mclass	XSIInterface | XSI interface to ActorX functions
//
//
//
//********************************************************************
XSIInterface::XSIInterface()
: m_hActorX ( NULL )
, m_hActorManager ( NULL )
, m_hActorXSceneInfo ( NULL )
, m_hXSI ( NULL )
, m_hActorXVertex ( NULL )
, m_hXSIPanel ( NULL )
{
}

XSIInterface::~XSIInterface()
{
	EndDialog ( m_hXSIPanel, 0 );
	EndDialog ( m_hActorManager, 0 );
	EndDialog ( m_hAbout, 0 );
	EndDialog ( m_hActorXSceneInfo, 0 );

	long n;
	for (n=0;n<m_pCachedScene.GetUsed();n++)
	{
		delete m_pCachedScene[n];
	}
	m_pCachedScene.DisposeData();

	for (n=0;n<m_pCachedMaterials.GetUsed();n++)
	{
		delete m_pCachedMaterials[n];
	}
	m_pCachedMaterials.DisposeData();

	for (n=0;n<m_pHRLevels.GetUsed();n++)
	{
		delete m_pHRLevels[n];
	}
	m_pHRLevels.DisposeData();



}

//********************************************************************
//
// @mfunc	ShowActorX | Calls up the ActorX dialog
//
//********************************************************************
void XSIInterface::ShowActorX()
{
	ActivatePanel();
	HideAll();

	ShowWindow ( m_hActorX, SW_SHOWNORMAL);

}

//********************************************************************
//
// @mfunc	ShowActorManager | Calls up the ActorX Actor Manager
//								dialog
//
//********************************************************************
void XSIInterface::ShowActorManager()
{
	if ( !IsWindow(m_hActorManager))
	{
		m_hActorManager = CreateDialog(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_ACTORMANAGER), GetXSIWindow(), (DLGPROC)ActorManagerDlgProc);
	}

	ShowWindow ( m_hActorManager, SW_SHOWNORMAL );
	UpdateWindow ( m_hActorManager );

}

//********************************************************************
//
// @mfunc	ShowSceneInfo | Calls up the ActorX Scene Info dialog
//
//********************************************************************
void	XSIInterface::ShowSceneInfo()
{
	DialogBox(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_SCENEINFO), GetXSIWindow(), (DLGPROC)SceneInfoDlgProc);


}

//********************************************************************
//
// @mfunc	ShowActorXSetup | Calls up the ActorX Setup dialog
//
//********************************************************************
void	XSIInterface::ShowActorXSetup( )
{
	ActivatePanel();
	HideAll();
	ShowWindow ( m_hActorXSetup, SW_SHOWNORMAL);
}

//********************************************************************
//
// @mfunc	ShowActorXSetup | Calls up the ActorX Setup dialog
//
//********************************************************************
void	XSIInterface::ShowActorXStaticMesh( )
{
	ActivatePanel();
	HideAll();
	ShowWindow ( m_hActorXStaticMesh, SW_SHOWNORMAL);
}



//********************************************************************
//
// @mfunc	ShowVertex | Calls up the ActorX Vertex dialog
//
//********************************************************************
void	XSIInterface::ShowVertex( )
{
#ifdef VERTEX_ANIMATION_SUPPORT
	ActivatePanel();
	HideAll();
	ShowWindow ( m_hActorXVertex, SW_SHOWNORMAL);
#endif

}

//********************************************************************
//
// @mfunc	ShowAbout | Calls up the ActorX about dialog
//
//********************************************************************
void	XSIInterface::ShowAbout( )
{
	if ( !IsWindow(m_hAbout))
	{
		m_hAbout  = CreateDialog(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_ABOUTBOX), GetXSIWindow(), (DLGPROC)AboutBoxDlgProc);
	}

	ShowWindow ( m_hAbout , SW_SHOWNORMAL );
	UpdateWindow ( m_hAbout  );
}

//********************************************************************
//
// @mfunc	ExportStaticMesh | Calls up the Static Mesh Export dlg
//
//********************************************************************
void	XSIInterface::ExportStaticMesh()
{
	ExportStaticMeshLocal();
}
//********************************************************************
//
// @mfunc	ActivatePanel | This function creates a window that
//							acts as a placeholder for ActorX dialogs
//							It first creates the placeholder window
//							which contains a tab-control then
//							creates the ActorX dialogs. It modifies
//							the styles of these dialogs so it looks
//							better integrated into XSI's layout.
//
//********************************************************************
void	XSIInterface::ActivatePanel()
{
	if ( !IsWindow(m_hXSIPanel) )
	{
		m_hXSIPanel = CreateDialog ( ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_XSIPANEL), GetXSIWindow(), (DLGPROC)XSIPanelDlgProc);
#ifdef _WIN64
		SetWindowLong ( m_hXSIPanel, GWLP_USERDATA, (long)this);
#else
		SetWindowLong ( m_hXSIPanel, GWL_USERDATA, (long)this);
#endif

		m_hActorX = CreateDialog(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_PANEL), (HWND)GetDlgItem ( m_hXSIPanel, IDC_TAB1 ), (DLGPROC)ActorXDlgProc);
		m_hActorXSetup = CreateDialog(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_AUXPANEL), (HWND)GetDlgItem ( m_hXSIPanel, IDC_TAB1 ), (DLGPROC)AuxDlgProc);
		m_hActorXStaticMesh = CreateDialog(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_STATICMESH), (HWND)GetDlgItem ( m_hXSIPanel, IDC_TAB1 ), (DLGPROC)StaticMeshDlgProc);
#ifdef VERTEX_ANIMATION_SUPPORT
		m_hActorXVertex  = CreateDialog(  ParentApp_hInstance ,  MAKEINTRESOURCE(IDD_VERTEX), (HWND)GetDlgItem ( m_hXSIPanel, IDC_TAB1 ), (DLGPROC)VertexDlgProc);
		RemoveBorder ( m_hActorXVertex );
		SetWindowPos ( m_hActorXVertex, HWND_TOP, 5,25,0,0,SWP_NOSIZE | SWP_NOZORDER);
#endif

		RemoveBorder ( m_hActorX );
		RemoveBorder ( m_hActorXSetup );
		RemoveBorder ( m_hActorXStaticMesh );

		SetWindowPos ( m_hActorX,		HWND_TOP, 5,25,0,0,SWP_NOSIZE | SWP_NOZORDER);
		SetWindowPos ( m_hActorXSetup,	HWND_TOP, 5,25,0,0,SWP_NOSIZE | SWP_NOZORDER);
		SetWindowPos ( m_hActorXStaticMesh, HWND_TOP, 5,25,0,0,SWP_NOSIZE | SWP_NOZORDER);

		HideAll();

		//
		// connect the parameters here
		//

		m_vParameters.DisposeData();

		AddParameter ( IDC_DEFOUTPATH, m_hActorX, L"", L"outputfolder" );
		AddParameter ( IDC_SKINFILENAME, m_hActorX, L"", L"meshfilename" );
		AddParameter ( IDC_ANIMFILENAME, m_hActorX, L"", L"animationfilename" );
		AddParameter ( IDC_ANIMNAME, m_hActorX, L"", L"animationsequencename" );
		AddParameter ( IDC_ANIMRANGE, m_hActorX, L"", L"animationrange" );


		AddParameter ( IDC_CHECKPERSISTSETTINGS, m_hActorXSetup, false, L"persistentsettings" );
		AddParameter ( IDC_CHECKPERSISTPATHS, m_hActorXSetup, false, L"persistentpaths" );
		AddParameter ( IDC_CHECKSKINALLP, m_hActorXSetup, false, L"allskintype" );
		AddParameter ( IDC_CHECKSKINALLT, m_hActorXSetup, false, L"alltextured" );
		AddParameter ( IDC_CHECKSKINALLS, m_hActorXSetup, false, L"allselected" );
		AddParameter ( IDC_CHECKSKINNOS, m_hActorXSetup, false, L"invert" );
		AddParameter ( IDC_POSEZERO, m_hActorXSetup, false, L"forcereferencepose" );
		AddParameter ( IDC_CHECKDOTANGENTS, m_hActorXSetup, false, L"tangentuvsplits" );
		AddParameter ( IDC_CHECKDOUNSMOOTH, m_hActorXSetup, false, L"bakesmoothinggroups" );
		AddParameter ( IDC_CHECKCULLDUMMIES, m_hActorXSetup, false, L"cullunuseddummies" );
		AddParameter ( IDC_CHECKDOEXPORTSCALE, m_hActorXSetup, false, L"exportanimatedscale" );
		AddParameter ( IDC_FIXROOT, m_hActorXSetup, false, L"fixrootnetmotion" );
		AddParameter ( IDC_LOCKROOTX, m_hActorXSetup, false, L"hardlock" );
		AddParameter ( IDC_EDITGLOBALSCALE, m_hActorXSetup, (float)1.0f, L"globalscalefactor" );
		AddParameter ( IDC_CHECKNOLOG, m_hActorXSetup, false, L"nologfiles" );
		AddParameter ( IDC_EDITCLASS, m_hActorXSetup, L"", L"class" );
		AddParameter ( IDC_EDITBASE, m_hActorXSetup, L"", L"base" );
		AddParameter ( IDC_EXPLICIT, m_hActorXSetup, false, L"explicitsequencelist" );
		AddParameter ( IDC_CHECKTEXPCX, m_hActorXSetup, false, L"assumepcxtextures" );

		AddParameter ( IDC_PATHVTX, m_hActorXVertex, L"", L"vertexoutputfolder" );
		AddParameter ( IDC_FILENAMEVTX, m_hActorXVertex, L"", L"vertexmeshoutputfilename" );
		AddParameter ( IDC_EDITVERTEXRANGE, m_hActorXVertex, L"", L"vertexframerange" );
		AddParameter ( IDC_CHECKFIXEDSCALE, m_hActorXVertex, false, L"vertexfixedscale" );
		AddParameter ( IDC_EDITFIXEDSCALE, m_hActorXVertex, (float)1.0f, L"vertexfixedscaleedit" );
		AddParameter ( IDC_CHECKAPPENDVERTEX, m_hActorXVertex, false, L"vertexappendtoexisting" );

		//
		// static mesh
		//

		AddParameter ( IDC_CHECKSUPPRESS, m_hActorXStaticMesh, false, L"nopopups" );
		AddParameter ( IDC_CHECKUNDERSCORE, m_hActorXStaticMesh, false, L"convertunderscores" );
		AddParameter ( IDC_CHECKUNDERSCORE, m_hActorXStaticMesh, false, L"selectedonly" );
		AddParameter ( IDC_CHECKGEOMNAME, m_hActorXStaticMesh, false, L"geometrynameasfilename" );
		AddParameter ( IDC_CHECKSMOOTH, m_hActorXStaticMesh, false, L"obeyhardedges" );
		AddParameter ( IDC_CHECKCONSOLIDATE, m_hActorXStaticMesh, false, L"consolidateoutputgeometry" );
		AddParameter ( IDC_EDITOUTPATH, m_hActorXStaticMesh, L"", L"defaultpath" );

		UpdateParameters();

	}

	ShowWindow ( m_hXSIPanel, SW_SHOWNORMAL );
	UpdateWindow ( m_hXSIPanel );

}

void	XSIInterface::UpdateParameters()
{
	XSI::CustomProperty prop;
	XSI::Parameter param ;
	XSI::Application app;
	XSI::CValue out;
	XSI::CValueArray args(1);

	if(!FindCustomPSet(app.GetActiveSceneRoot(), L"ActorXSettings", prop))
	{
		app.GetActiveSceneRoot().AddProperty(L"Custom_parameter_list", false, L"ActorXSettings", prop);

		for (int p=0;p<m_vParameters.GetUsed();p++)
		{
			ActorXParam* l_pParam = &m_vParameters[p];

			switch ( l_pParam->m_cValue.m_t )
			{
			case XSI::siFloat:
			case XSI::siDouble:
				prop.AddParameter( l_pParam->m_cName,l_pParam->m_cValue.m_t,XSI::siPersistable, L"", L"", (float)0,(float)-FLT_MAX,(float)FLT_MAX,(float)-200,(float)200,param);
				break;
			case XSI::siBool:
				prop.AddParameter( l_pParam->m_cName,l_pParam->m_cValue.m_t,XSI::siPersistable, L"", L"", (bool)0,(bool)0,(bool)1,(bool)0,(bool)1,param);
				break;
			case XSI::siString:
			case XSI::siWStr:
				prop.AddParameter( l_pParam->m_cName,l_pParam->m_cValue.m_t,XSI::siPersistable, L"", L"", L"",L"",L"",L"",L"",param);
				break;
			}
		}
	}

	for (int p=0;p<m_vParameters.GetUsed();p++)
	{
		ActorXParam* l_pParam = &m_vParameters[p];

		XSI::Parameter param = prop.GetParameter (l_pParam->m_cName);
		XSI::CValue value = param.GetValue();

		switch ( value.m_t )
		{
			case XSI::siFloat:
			case XSI::siDouble:
				{
					float fval = (float)value;
					char valuesz[256];
					sprintf ( valuesz, "%f", fval );
					SetDlgItemText ( l_pParam->m_hParent, l_pParam->m_iID, valuesz );
					break;
				}

			case XSI::siBool:
				{
					bool bval = (bool)value;
					SendDlgItemMessage(l_pParam->m_hParent, l_pParam->m_iID, BM_SETCHECK , bval ? BST_CHECKED:BST_UNCHECKED,0);
					break;
				}
			case XSI::siString:
			case XSI::siWStr:
				{
					XSI::CString strval = (XSI::CString)value;

					char *strsz = new char [ strval.Length() + 1 ];
					W2AHelper ( strsz, strval.GetWideString() );
					SetDlgItemText ( l_pParam->m_hParent, l_pParam->m_iID, strsz );
					delete [] strsz;

					break;
				}
		}
	}

}

void XSIInterface::DoGlobalDataExchange ()
{
	GetWindowText(GetDlgItem(m_hActorX,IDC_ANIMNAME),animname,100);
	GetWindowText(GetDlgItem(m_hActorX,IDC_ANIMRANGE),framerangestring,500);
	GetWindowText(GetDlgItem(m_hActorX,IDC_DEFOUTPATH),to_path,300);
	GetWindowText(GetDlgItem(m_hActorX,IDC_SKINFILENAME),to_skinfile,300);
	GetWindowText(GetDlgItem(m_hActorX,IDC_ANIMFILENAME),to_animfile,300);

	OurScene.CheckPersistSettings = GetCheckBox(m_hActorXSetup,IDC_CHECKPERSISTSETTINGS);
	OurScene.CheckPersistPaths = GetCheckBox(m_hActorXSetup,IDC_CHECKPERSISTPATHS);
	OurScene.DoFixRoot = GetCheckBox(m_hActorXSetup,IDC_FIXROOT);
	OurScene.DoPoseZero = GetCheckBox(m_hActorXSetup,IDC_POSEZERO);
	OurScene.DoLockRoot = GetCheckBox(m_hActorXSetup,IDC_LOCKROOTX);
	OurScene.DoLog = ! GetCheckBox(m_hActorXSetup,IDC_CHECKNOLOG);

	OurScene.DoExportScale = GetCheckBox(m_hActorXSetup,IDC_CHECKDOEXPORTSCALE);
	OurScene.DoTangents = GetCheckBox(m_hActorXSetup,IDC_CHECKDOTANGENTS);
	OurScene.DoUnSmooth = GetCheckBox(m_hActorXSetup,IDC_CHECKDOUNSMOOTH);

	OurScene.DoTexGeom = GetCheckBox(m_hActorXSetup,IDC_CHECKSKINALLT);
	OurScene.DoTexPCX = GetCheckBox(m_hActorXSetup,IDC_CHECKTEXPCX);
	OurScene.DoSelectedGeom = GetCheckBox(m_hActorXSetup,IDC_CHECKSKINALLS);
	OurScene.DoSkinGeom = GetCheckBox(m_hActorXSetup,IDC_CHECKSKINALLP);
	OurScene.DoSkipSelectedGeom = GetCheckBox(m_hActorXSetup,IDC_CHECKSKINNOS);
	OurScene.DoCullDummies = GetCheckBox(m_hActorXSetup,IDC_CHECKCULLDUMMIES);
	OurScene.DoExplicitSequences = GetCheckBox(m_hActorXSetup,IDC_EXPLICIT);
	GetWindowText( GetDlgItem(m_hActorXSetup,IDC_EDITCLASS),classname,100 );
	GetWindowText(GetDlgItem(m_hActorXSetup,IDC_EDITBASE),basename,100);

}

void	XSIInterface::SaveParameters()
{
	XSI::CustomProperty prop;
	XSI::Parameter param ;
	XSI::Application app;

	if(!FindCustomPSet(app.GetActiveSceneRoot(), L"ActorXSettings", prop))
	{
		return;
	}

	for (int p=0;p<m_vParameters.GetUsed();p++)
	{
		ActorXParam* l_pParam = &m_vParameters[p];
		XSI::Parameter param = prop.GetParameter (l_pParam->m_cName);
		switch ( l_pParam->m_cValue.m_t )
		{
			case XSI::siFloat:
			case XSI::siDouble:
				{
					char mess[256];
					GetDlgItemText ( l_pParam->m_hParent, l_pParam->m_iID, mess, 256 );
					param.PutValue ( atof ( mess ) );
					break;
				}
			case XSI::siBool:
				{
					param.PutValue ( GetCheckBox(l_pParam->m_hParent, l_pParam->m_iID ));
					break;
				}
			case XSI::siString:
			case XSI::siWStr:
				{
					char mess[MAX_PATH];
					GetDlgItemText ( l_pParam->m_hParent, l_pParam->m_iID, mess, MAX_PATH );
					wchar_t*l_wszString = NULL;
					A2W(&l_wszString, mess);
					param.PutValue ( XSI::CString(l_wszString));
					break;
				}
		}


	}


}

ActorXParam*	XSIInterface::GetParameter ( XSI::CString name )
{
	for (int c=0;c<m_vParameters.GetUsed();c++)
	{
		if ( m_vParameters[c].m_cName == name )
		{
			return &m_vParameters[c];
		}
	}

	return NULL;
}

bool FindCustomPSet(XSI::X3DObject &node, const XSI::CString& name, XSI::CustomProperty &prop)
{
	XSI::CRefArray properties = node.GetProperties();
	int j;

	if(properties.GetCount() > 0)
	{
		for(j = 0; j < properties.GetCount(); j++)
		{
			prop = properties[j];

			if(prop.GetName() == name)
				return true;
		}
	}

	return false;
}

//********************************************************************
//
// @mfunc	HideAll | Hides all ActorX window
//
//********************************************************************
void	XSIInterface::HideAll()
{
	ShowWindow ( m_hActorX, SW_HIDE );
	ShowWindow ( m_hActorXSetup, SW_HIDE );

	ShowWindow ( m_hActorXStaticMesh, SW_HIDE );

#ifdef VERTEX_ANIMATION_SUPPORT
	ShowWindow ( m_hActorXVertex, SW_HIDE );
#endif

}


//********************************************************************
//
// @mfunc	GetTabHWND | Returns the HWND for one of the ActorX dlg
//
//********************************************************************
HWND	XSIInterface::GetTabHWND ( int in_iNum )
{
	switch (in_iNum)
	{
	case 0: return m_hActorX;
	case 1: return m_hActorXSetup;
	case 2: return m_hActorXStaticMesh;
#ifdef VERTEX_ANIMATION_SUPPORT
	case 3: return m_hActorXVertex;
#endif
	}

	return NULL;
}

//********************************************************************
//
// @mfunc	IsDeformer | This functions returns true if the x3dobject
//							passed in is an envelope deformer (bone)
//
//********************************************************************
bool	XSIInterface::IsDeformer ( XSI::CRef in_obj )
{

	XSI::CRef potentialbone = in_obj;

	for( int i=0; i<m_pCachedScene.GetUsed(); i++)
	{

		XSI::X3DObject node = m_pCachedScene[i]->m_ptr;
		XSI::SceneItem item ( node );

		if ( item.IsValid())
		{
			XSI::CRefArray l_pEnvelopes = item.GetEnvelopes();
			if(l_pEnvelopes.GetCount())
			{
				bool bSelected = IsSelected(node);
				if(( OurScene.DoSelectedGeom && !OurScene.DoSkipSelectedGeom )  && !bSelected)
				{
					continue;
				}
				if(( OurScene.DoSelectedGeom && OurScene.DoSkipSelectedGeom )  && bSelected)
				{
					continue;
				}
			}

			for (int e=0;e<l_pEnvelopes.GetCount();e++)
			{
				XSI::Envelope env ( l_pEnvelopes[e] );

				XSI::CRefArray l_deformers = env.GetDeformers();

				for (int d=0;d<l_deformers.GetCount();d++)
				{

					if ( l_deformers[d] == potentialbone )
					{
						return true;
					}

				}

			}


		}

	}

	return false;

}


//********************************************************************
//
// @mfunc	AnalyzeSkeleton | This function analyzes the entire scene
//								to figure out which node should be
//								considered the root bone.
//								When found, everything under this node
//								will be considered a "potential" bone.
//
//								Some potential bones may be optimized
//								out later if they do not act as a
//								deformer. But to the artist, these
//								bones remain totally normal and can
//								be animated, constrained, etc.
//
//********************************************************************
void	XSIInterface::AnalyzeSkeleton()
{
	//
	// first start building a list of all deformers and
	// find a common root for all these bones.
	// Everything lower in the hierarchy than that bone
	// will be considered a bone.
	//

	XSI::Application app;
	XSI::Model root = app.GetActiveSceneRoot();

	int highest = 9999;
	XSI::SceneItem highestobject;

	int n;
	for (n=0;n<m_pCachedScene.GetUsed();n++)
	{
		XSI::SceneItem item ( m_pCachedScene[n]->m_ptr );

		if ( IsDeformer ( item ) )
		{

			int levels = 0;

			while ( item.GetParent() != root )
			{
				item = item.GetParent();
				levels++;
			}

			if ( levels < highest )
			{
				highest = levels;
				highestobject = m_pCachedScene[n]->m_ptr;
			}
			m_pHRLevels.Extend(1);
			m_pHRLevels[m_pHRLevels.GetUsed()-1] = new HierarchicalLevel;
			m_pHRLevels[m_pHRLevels.GetUsed()-1]->m_iLevelsDeep = levels;
			m_pHRLevels[m_pHRLevels.GetUsed()-1]->m_cref = m_pCachedScene[n]->m_ptr;
		}
	}

	//
	// count the number of bones that share the highest hierarhical level
	//

	int l_iCount = 0;

	CSIBCArray<HierarchicalLevel*> l_pHighest;
	for (n=0;n<m_pHRLevels.GetUsed();n++)
	{
		if ( m_pHRLevels[n]->m_iLevelsDeep == highest )
		{
			l_iCount++;

			LogThis ( XSI::SceneItem ( m_pHRLevels[n]->m_cref ).GetName() + L" is at highest level" );

			l_pHighest.Extend(1);
			l_pHighest[l_pHighest.GetUsed()-1] = m_pHRLevels[n];
		}
	}

	if ( l_iCount == 0 )
	{
		//
		// no bones in scene...
		//

		return;
	}

	if ( !l_iCount )
	{
		LogThis ( L"ActorX: Could not find any envelope deformers!");
		return;
	}

	if ( l_iCount == 1 )
	{
		// one common root for every bone in the hierarchy
		//
		m_cSkeletonRoot = highestobject;
	} else {

		//
		// multiple bones have the same hierarchical level.
		// so they share a common root.
		//

		//
		// Find the first common grandparent
		//

		XSI::SceneItem firstparent = XSI::SceneItem( l_pHighest[0]->m_cref ).GetParent();

		while ( firstparent != root )
		{
			int sharecount = 0;
			for (int i=1;i<l_pHighest.GetUsed();i++)
			{
				if ( IsChildOf ( XSI::SceneItem ( l_pHighest[i]->m_cref ), firstparent ))
				{
					sharecount++;
				}

			}

			if ( sharecount == l_pHighest.GetUsed() -1 )
			{
				// found our guy
				//

				m_cSkeletonRoot = firstparent;
				break;
			}

			firstparent = firstparent.GetParent();
		}



		m_cSkeletonRoot = firstparent;
	}


	LogThis ( XSI::CString(L"Setting ") + XSI::SceneItem ( m_cSkeletonRoot ).GetName() + L" as top level bone" );




}


//********************************************************************
//
// @mfunc	IsChildOf | This function returns true if children is
//						part of obj's hierarchy
//
//********************************************************************
bool	XSIInterface::IsChildOf ( XSI::SceneItem children, XSI::SceneItem obj )
{
	XSI::Application app;
	XSI::Model root = app.GetActiveSceneRoot();

	while ( children != XSI::SceneItem(root) )
	{

		if ( children.GetParent() == obj )
		{
			return true;
		}

		children = children.GetParent();
	}

	return false;
}

//********************************************************************
//
// @mfunc	GetRootPointer | Get the node pointer of the skeleton root
//
//********************************************************************
x3dpointer* XSIInterface::GetRootPointer()
{
	x3dpointer* p = NULL;
	for (int n=0;n<m_pCachedScene.GetUsed();n++)
	{
		if ( m_pCachedScene[n]->m_ptr == g_XSIInterface->m_cSkeletonRoot )
		{
			p = m_pCachedScene[n];
			break;
		}
	}

	return p;

}

bool	XSIInterface::IsBone ( XSI::SceneItem obj )
{

	return IsChildOf ( obj, m_cSkeletonRoot );
}


void	XSIInterface::AddParameter ( int ID, HWND parent, XSI::CValue value, XSI::CString name )
{
	m_vParameters.Extend(1);
	m_vParameters[m_vParameters.GetUsed()-1].m_iID = ID;
	m_vParameters[m_vParameters.GetUsed()-1].m_hParent = parent;
	m_vParameters[m_vParameters.GetUsed()-1].m_cValue = value;
	m_vParameters[m_vParameters.GetUsed()-1].m_cName = name;
}


//********************************************************************
//
// @mfunc	GetFolder | Uses ShellFolder to popup a folder browser
//						dialog.
//						returns 1 on success, 0 otherwise.
//
//********************************************************************
int GetFolder ( char *out_szPath )
{
	BROWSEINFO      bi;
    LPMALLOC        pMalloc =   NULL;
    LPITEMIDLIST    pidl;
    char        acSrc   [MAX_PATH];
	int retval = 0;

    if  (SHGetMalloc (&pMalloc)!=  NOERROR)
	{
		return 0;
	}

    ZeroMemory  (&bi,sizeof(BROWSEINFO));

    bi.hwndOwner        =   GetActiveWindow();
    bi.pszDisplayName   =   acSrc;
    bi.ulFlags          =   BIF_RETURNONLYFSDIRS;
    bi.lpszTitle        =   "Please select a folder";

    if  (pidl=SHBrowseForFolder(&bi))
	{
		if  ( !SHGetPathFromIDList(pidl, acSrc))
			return 0;

		pMalloc->Free   (pidl);

		retval = 1;
	} else {
		retval = 0;
	}

    if  (   pMalloc)
		pMalloc->Release();

	strcpy ( out_szPath, acSrc );

	return retval;

}


//********************************************************************
//
// @mfunc	GetXSIWindow | Hack to get XSI's main window handle.
//
//********************************************************************
HWND	XSIInterface::GetXSIWindow()
{
	if ( m_hXSI == NULL )
	{
		m_hXSI = GetActiveWindow();
	}

	return m_hXSI;
}

//********************************************************************
//
// @mfunc	RemoveBorder | Edit's a window's style by removing it's
//							border
//
//********************************************************************
void RemoveBorder ( HWND in_hWnd )
{
	long l_Style;
	l_Style = GetWindowLong ( in_hWnd, GWL_STYLE );
	l_Style &= ~WS_BORDER;
	SetWindowLong ( in_hWnd, GWL_STYLE, l_Style);
}

static BOOL CALLBACK ActorXDlgProc(	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{

	switch (msg)
	{
		case WM_INITDIALOG:
			{
			// Store the object pointer into window's user data area.
			// TODO XSI
			// u = (MaxPluginClass *)lParam;
			//SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);

			PrintWindowString(hWnd,IDC_STATIC2,"Version XSI beta \n © 2007  Epic Games");


			// Initialize all non-empty edit windows.
			if ( to_path[0]   != 0  )
			{
				PrintWindowString( hWnd,IDC_DEFOUTPATH, to_path );
			}

			if ( to_skinfile[0] )  PrintWindowString( hWnd,IDC_SKINFILENAME,to_skinfile );
			if ( to_animfile[0] ) PrintWindowString(hWnd,IDC_ANIMFILENAME,to_animfile );
			if ( animname[0]    ) PrintWindowString(hWnd,IDC_ANIMNAME,animname );
			if ( framerangestring[0]  ) PrintWindowString(hWnd,IDC_ANIMRANGE,framerangestring);

			}
			break;

		case WM_CLOSE:
		case WM_DESTROY:
			// TODO XSI
			// Release this panel's custom controls.
			//ThisMaxPlugin.DestroySpinner(hWnd,IDC_SPIN_ATIME);
			EndDialog( hWnd, 0 );
			break;

		//case WM_MOUSEMOVE:
		//	ThisMaxPlugin.ip->RollupMouseMessage(hWnd,msg,wParam,lParam);
		//	break;

		case WM_COMMAND:

			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					// TODO XSI
					//DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					// TODO XSI
					//EnableAccelerators();
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
							//g_XSIInterface->DoGlobalDataExchange();
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
							//g_XSIInterface->DoGlobalDataExchange();
						}
						break;
					};
				}
				break;

			case IDC_DEFOUTPATH:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update path.
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_DEFOUTPATH),to_path,300);
							_tcscpy(LogPath,to_path);
							PluginReg.SetKeyString("TOPATH", to_path );
							//g_XSIInterface->DoGlobalDataExchange();
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
							GetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile,300);
							// PluginReg.SetKeyString("TOSKINFILE", to_skinfile );
							//g_XSIInterface->DoGlobalDataExchange();
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
							//PluginReg.SetKeyString("TOANIMFILE", to_animfile );
							//g_XSIInterface->DoGlobalDataExchange();
						}
						break;
					};
				}
				break;


			case IDC_ANIMMANAGER: // Animation manager activated.
				{
					XSI::Application app;
					XSI::CValue arg;
					arg = 0L;
					XSI::CValue retVal;
					app.ExecuteCommand ( L"ActorManager", arg, retVal);

				}
				break;

			// Browse for a destination directory.
			case IDC_BROWSE:
				{
					char  dir[MAX_PATH];
					TCHAR desc[MAX_PATH];
					_tcscpy(desc, _T("Target Directory"));

					if ( GetFolder ( dir ) )
					{
						SetWindowText(GetDlgItem(hWnd,IDC_DEFOUTPATH),dir);
						_tcscpy(to_path,dir);
						_tcscpy(LogPath,dir);
						PluginReg.SetKeyString("TOPATH", to_path );
					}
				}
				break;


			// A click inside the logo.

			case IDC_LOGOUNREAL:
				{
					XSI::Application app;
					XSI::CValue arg;
					arg = 0L;
					XSI::CValue retVal;
					app.ExecuteCommand ( L"ActorXAbout", arg, retVal);

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

					XSI::Application app;
					XSI::CValue arg;
					arg = 0L;
					XSI::CValue retVal;
					app.ExecuteCommand ( L"ActorXSceneInfo", arg, retVal);

#endif
				}
				break;


			case IDC_SAVESKIN:
				{

					XSI::Application app;
					XSI::CValue arg;
					arg = 0L;
					XSI::CValue retVal;
					app.ExecuteCommand ( L"ActorXSaveMesh", arg, retVal);

				}
				break;

			case IDC_DIGESTANIM:
				{
					XSI::Application app;
					XSI::CValue arg;
					arg = 0L;
					XSI::CValue retVal;
					app.ExecuteCommand ( L"ActorXDigestAnimation", arg, retVal);

				}
				break;
			}
			if ( g_XSIInterface )
				g_XSIInterface->SaveParameters ();

			break;
			// END command

		default:
			return FALSE;
	}
	return TRUE;
}

static BOOL CALLBACK AuxDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Get our plugin object pointer from the window's user data area.
	// TODO XSI
	//MaxPluginClass *u = (MaxPluginClass *)GetWindowLong(hWnd, GWL_USERDATA);
	//if (!u && msg != WM_INITDIALOG ) return FALSE;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			// Store the object pointer into window's user data area.
			// TODO XSI
			//u = (MaxPluginClass *)lParam;
			//SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);
			//SetInterface(u->ip);


			// Update boxes with the internal settings.
			_SetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS,OurScene.CheckPersistSettings);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTPATHS,OurScene.CheckPersistPaths);

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
			_SetCheckBox( hWnd, IDC_CHECKCULLDUMMIES, OurScene.DoCullDummies );
			_SetCheckBox( hWnd, IDC_EXPLICIT, OurScene.DoExplicitSequences );

			if ( basename[0]  ) PrintWindowString(hWnd,IDC_EDITBASE,basename);
			if ( classname[0] ) PrintWindowString(hWnd,IDC_EDITCLASS,classname);

			ToggleControlEnable(hWnd,IDC_CHECKSKIN1,true);
			ToggleControlEnable(hWnd,IDC_CHECKSKIN2,true);

			// Start Lockroot disabled
			ToggleControlEnable(hWnd,IDC_LOCKROOTX,false);

		}
			break;

		case WM_CLOSE:
		case WM_DESTROY:
			EndDialog( hWnd, 0 );
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
					// TODO XSI
					// DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					// TODO XSI
					// EnableAccelerators();
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
					PluginReg.SetKeyValue("PERSISTSETTINGS", OurScene.CheckPersistSettings);
				}
				break;

				case IDC_CHECKPERSISTPATHS:
				{
					//Read out the value...
					OurScene.CheckPersistPaths = GetCheckBox(hWnd,IDC_CHECKPERSISTPATHS);
					PluginReg.SetKeyValue("PERSISTPATHS", OurScene.CheckPersistPaths);
				}
				break;

				case IDC_FIXROOT:
				{
					OurScene.DoFixRoot = GetCheckBox(hWnd,IDC_FIXROOT);
					PluginReg.SetKeyValue("DOFIXROOT",OurScene.DoFixRoot );

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
					PluginReg.SetKeyValue("DOLOG",OurScene.DoLog);
				}
				break;

				case IDC_CHECKDOUNSMOOTH:
				{
					OurScene.DoUnSmooth = GetCheckBox(hWnd,IDC_CHECKDOUNSMOOTH);
					PluginReg.SetKeyValue("DOUNSMOOTH",OurScene.DoUnSmooth);
				}
				break;

				case IDC_CHECKDOEXPORTSCALE:
				{
					OurScene.DoExportScale = GetCheckBox(hWnd,IDC_CHECKDOEXPORTSCALE);
					PluginReg.SetKeyValue("DOEXPORTSCALE",OurScene.DoExportScale);
				}
				break;


				case IDC_CHECKDOTANGENTS:
				{
					OurScene.DoTangents = GetCheckBox(hWnd,IDC_CHECKDOTANGENTS);
					PluginReg.SetKeyValue("DOTANGENTS",OurScene.DoTangents);
				}
				break;


				case IDC_CHECKSKINALLT: // all textured geometry skin checkbox
				{
					OurScene.DoTexGeom = GetCheckBox(hWnd,IDC_CHECKSKINALLT);
					PluginReg.SetKeyValue("DOTEXGEOM",OurScene.DoTexGeom );
					//if (!OurScene.DoTexGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLT,false);
				}
				break;

				case IDC_CHECKTEXPCX: // all textured geometry skin checkbox
				{
					OurScene.DoTexPCX = GetCheckBox(hWnd,IDC_CHECKTEXPCX);
					PluginReg.SetKeyValue("DOPCX",OurScene.DoTexPCX); // update registry
				}
				break;

				case IDC_CHECKSKINALLS: // all selected meshes skin checkbox
				{
					OurScene.DoSelectedGeom = GetCheckBox(hWnd,IDC_CHECKSKINALLS);
					PluginReg.SetKeyValue("DOSELGEOM",OurScene.DoSelectedGeom);
					//if (!OurScene.DoSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLS,false);
				}
				break;

				case IDC_CHECKSKINALLP: // all selected physique meshes checkbox
				{
					OurScene.DoSkinGeom = GetCheckBox(hWnd,IDC_CHECKSKINALLP);
					PluginReg.SetKeyValue("DOPHYSGEOM", OurScene.DoSkinGeom );
					//if (!OurScene.DoSkinGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLP,false);
				}
				break;

				case IDC_CHECKSKINNOS: // all none-selected meshes skin checkbox..
				{
					OurScene.DoSkipSelectedGeom = GetCheckBox(hWnd,IDC_CHECKSKINNOS);
					PluginReg.SetKeyValue("DOSKIPSEL",OurScene.DoSkipSelectedGeom );
					//if (!OurScene.DoSkipSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINNOS,false);
				}
				break;

				case IDC_CHECKCULLDUMMIES: // all selected physique meshes checkbox
				{
					OurScene.DoCullDummies = GetCheckBox(hWnd,IDC_CHECKCULLDUMMIES);
					PluginReg.SetKeyValue("DOCULLDUMMIES",OurScene.DoCullDummies );
					//ToggleControlEnable(hWnd,IDC_CHECKCULLDUMMIES,OurScene.DoCullDummies);
					//if (!OurScene.DoCullDummies) _SetCheckBox(hWnd,IDC_CHECKCULLDUMMIES,false);
				}
				break;

				case IDC_EXPLICIT:
				{
					OurScene.DoExplicitSequences = GetCheckBox(hWnd,IDC_EXPLICIT);
					PluginReg.SetKeyValue("DOEXPSEQ",OurScene.DoExplicitSequences);
				}
				break;

				case IDC_SAVESCRIPT: // save the script
				{
					if( TempActor.OutAnims.Num() == 0 )
					{
						PopupBox("Warning: no animations present in output package.\n Aborting template generation.");
					}
					else
					{
						if( OurScene.WriteScriptFile( &TempActor, classname, basename, to_skinfile, to_animfile ) )
						{
							PopupBox(" Script template file written: %s ",classname);
						}
						else
							PopupBox(" Error trying to write script template file [%s] to path [%s]",classname,to_path) ;
					}
				}
				break;

				// Load, parse and execute a batch-export list.
				case IDC_BATCHLIST:
				{
					// Load
					char batchfilename[300];
					_tcscpy(batchfilename,("XX"));
				    GetBatchFileName( hWnd, batchfilename, to_path);
					if( batchfilename[0] ) // Nonzero string = OK name.
					{
						// Parse and execute..
						// ExecuteBatchFile( batchfilename, OurScene );
						PopupBox(" Batch file [%s] processed.",batchfilename );
					}
				}
				break;

				// Get all XSI files from selectable folder and digest them all into the animation manager.
				case IDC_BATCHMAX:
				{
					TCHAR dir[300];
					TCHAR desc[300];
					_tcscpy(desc, _T("Maxfiles source folder"));

					if ( GetFolder ( dir ) )
					{
						_tcscpy(batchfoldername,dir);
					}

					INT NumDigested = 0;
					INT NumFound = 0;

					// TODO XSI
					// BatchAnimationProcess( batchfoldername, NumDigested, NumFound, u );

					PopupBox(" Digested %i animation files of %i from folder[ %s ]",NumDigested,NumFound,batchfoldername);
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
							PluginReg.SetKeyString("CLASSNAME", classname );
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
							PluginReg.SetKeyString("BASENAME", basename );
						}
						break;
					};
				}
				break;

			};


			if ( g_XSIInterface )
				g_XSIInterface->SaveParameters ();

			break;


		default:
		return FALSE;
	}
	return TRUE;
}



static BOOL CALLBACK VertexDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{


	switch (msg)
	{
		case WM_INITDIALOG:
		{

			_SetCheckBox( hWnd, IDC_CHECKAPPENDVERTEX, OurScene.DoAppendVertex );
			_SetCheckBox( hWnd, IDC_CHECKFIXEDSCALE,   OurScene.DoScaleVertex );

			if ( OurScene.DoScaleVertex && (OurScene.VertexExportScale != 0.f) )
			{
				TCHAR ScaleString[64];
				sprintf(ScaleString,"%8.5f",OurScene.VertexExportScale );
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

		case WM_CLOSE:
		case WM_DESTROY:
			EndDialog( hWnd, 0 );
			break;


		case WM_COMMAND:
			// Needed to have keyboard focus on this window!
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					// TODO XSI
					// DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					// TODO XSI
					// EnableAccelerators();
					break;
			};

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam))
			{

				// Browse for a destination directory.
				case IDC_BROWSEVTX:
				{
					char  dir[MAX_PATH];
					TCHAR desc[MAX_PATH];
					_tcscpy(desc, _T("Target Directory"));

					if ( GetFolder ( dir ) )
					{
						SetWindowText(GetDlgItem(hWnd,IDC_PATHVTX),dir);
						_tcscpy(to_pathvtx,dir);
						_tcscpy(LogPath,dir);
						PluginReg.SetKeyString("TOPATHVTX", to_pathvtx );
					}
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
							PluginReg.SetKeyString("TOPATHVTX", to_pathvtx );
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
							PluginReg.SetKeyString("TOSKINFILEVTX", to_skinfilevtx );
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

					INT WrittenFrameCount;

					if( WrittenFrameCount = OurScene.WriteVertexAnims( &TempActor, to_skinfilevtx, vertframerangestring ) ) // Calls DigestSkin
					{

						if( ! OurScene.DoAppendVertex )
								PopupBox(" Vertex animation export successful. \n Written %i frames to file [%s]. ",WrittenFrameCount,to_skinfilevtx);
							else
								PopupBox(" Vertex animation export successful. \n Appended %i frames to file [%s]. ",WrittenFrameCount,to_skinfilevtx);
					}
					else
					{
						PopupBox(" Error encountered writing vertex animation data. ");
					}
				}
				break;

				case IDC_CHECKAPPENDVERTEX:
				{
					OurScene.DoAppendVertex = _GetCheckBox(hWnd,IDC_CHECKAPPENDVERTEX);
					PluginReg.SetKeyValue("DOAPPENDVERTEX", OurScene.DoAppendVertex);
				}
				break;

				case IDC_CHECKFIXEDSCALE:
				{
					OurScene.DoScaleVertex = _GetCheckBox(hWnd,IDC_CHECKFIXEDSCALE);
					PluginReg.SetKeyValue("DOSCALEVERTEX", OurScene.DoScaleVertex);
				}
				break;


			};

			if ( g_XSIInterface )
				g_XSIInterface->SaveParameters ();

			break;


		default:
		return FALSE;
	}
	return TRUE;
}



static BOOL CALLBACK AboutBoxDlgProc(HWND hWnd, UINT msg,
	WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		{
			SetWindowText(GetDlgItem(hWnd,IDC_STATIC2),"Unreal Actor Exporter\n\n Version: XSI beta \n\n (c) 2007  Epic Games, Inc.\n\n All Rights Reserved.");
		}
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

static BOOL CALLBACK XSIPanelDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
	XSIInterface*	l_pInterface = (XSIInterface*)GetWindowLong ( hWnd, GWLP_USERDATA );
#else
	XSIInterface*	l_pInterface = (XSIInterface*)GetWindowLong ( hWnd, GWL_USERDATA );
#endif

	switch (msg) {
	case WM_INITDIALOG:
		{
			//
			// Pre-initialize persistent settings
			//
			INT SwitchPersistent;
			PluginReg.GetKeyValue("PERSISTSETTINGS", SwitchPersistent);
			// Get all relevant settings from the registry.
			if( SwitchPersistent )
			{
				OurScene.GetAuxSwitchesFromRegistry( PluginReg );
				OurScene.GetStaticMeshSwitchesFromRegistry( PluginReg );
			}

			//
			// build the tab control
			//

			TC_ITEM	item;
			memset ( &item, 0, sizeof(TC_ITEM));
			item.mask = TCIF_TEXT ;
			item.pszText = "ActorX";
			item.iImage = -1;

			int a = TabCtrl_InsertItem( GetDlgItem (hWnd, IDC_TAB1),0,&item);

			item.pszText = "Setup";
			a = TabCtrl_InsertItem( GetDlgItem ( hWnd, IDC_TAB1),1,&item);
			item.pszText = "StaticMesh";
			a = TabCtrl_InsertItem( GetDlgItem ( hWnd, IDC_TAB1),2,&item);
#ifdef VERTEX_ANIMATION_SUPPORT
			item.pszText = "Vertex";
			a = TabCtrl_InsertItem( GetDlgItem ( hWnd, IDC_TAB1),1,&item);
#endif


		}
		break;

		case WM_NOTIFY:
			{
				NMHDR FAR *tem=(NMHDR FAR *)lParam;

				if (tem->code== TCN_SELCHANGE)
				{
					int num=TabCtrl_GetCurSel(tem->hwndFrom);

					switch ( num )
					{
					case 0: l_pInterface->ShowActorX (); break;
					case 1: l_pInterface->ShowActorXSetup(); break;
					case 2: l_pInterface->ShowActorXStaticMesh(); break;
#ifdef VERTEX_ANIMATION_SUPPORT
					case 3: l_pInterface->ShowVertex (); break;
#endif
					}

					if ( l_pInterface )
					{
						l_pInterface->UpdateParameters ();
					}

				}
				break;
			}

	case WM_DESTROY:
	case WM_CLOSE:

		if ( l_pInterface != NULL )
		{
			for (int t=0;t<4;t++)
			{
				EndDialog ( l_pInterface->GetTabHWND(t), 0 );
			}

			l_pInterface->m_hXSIPanel = NULL;
		}

		EndDialog(hWnd, 1);

		break;

		default:
			return FALSE;
	}
	return TRUE;

}


int SceneIFC::DigestAnim(VActor *Thing, char* AnimName, char* RangeString )
{
	if( DEBUGFILE )
	{
		//TODO XSI: TSTR LogFileName = _T("\\ActXAni.LOG");
		//DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	INT FirstFrame = (INT)FrameStart / FrameTicks; // Convert ticks to frames
	INT LastFrame = (INT)FrameEnd / FrameTicks;

	ParseFrameRange( RangeString, FirstFrame, LastFrame );

	OurTotalFrames = FrameList.GetTotal();
	Thing->RawNumFrames = OurTotalFrames;
	Thing->RawAnimKeys.SetSize( OurTotalFrames * OurBoneTotal );
	Thing->RawNumBones  = OurBoneTotal;
	Thing->FrameTotalTicks = OurTotalFrames * FrameTicks;

	XSI::Application app;
	XSI::Property playControl = app.GetActiveProject().GetProperties().GetItem(L"Play Control");

	Thing->FrameRate = playControl.GetParameterValue(L"rate");

	if( DoExportScale )
	{
		Thing->RawScaleKeys.SetSize( OurTotalFrames * OurBoneTotal );
	}

	// PopupBox(" Frame rate %f  Ticks per frame %f  Total frames %i FrameTimeInfo %f ",(FLOAT)GetFrameRate(), (FLOAT)FrameTicks, (INT)OurTotalFrames, (FLOAT)Thing->FrameTotalTicks );

	_tcscpy(Thing->RawAnimName,CleanString( AnimName ));

	//
	// Rootbone is in tree starting from RootBoneIndex.
	//

	int BoneIndex  = 0;
	int AnimIndex  = 0;
	int FrameCount = 0;
	int TotalAnimNodes = SerialTree.Num()-RootBoneIndex;

	//for ( TimeValue TimeNow = FrameStart; TimeNow <= FrameEnd; TimeNow += FrameTicks )

	XSI::UIToolkit kit = app.GetUIToolkit();
	XSI::ProgressBar	l_pBar = kit.GetProgressBar();

	l_pBar.PutMaximum( 100 );
	l_pBar.PutMinimum( 1 );
	l_pBar.PutStep( 1 );
	l_pBar.PutValue( 1 );

	XSI::CString l_szCaption = L"Processing animation: ";
	wchar_t*l_wszString = NULL;
	A2W(&l_wszString, AnimName);
	l_szCaption += l_wszString;

	l_pBar.PutCaption( l_szCaption );
	l_pBar.PutStatusText( L"" );
	l_pBar.PutVisible( true );

	for( INT t=0; t<FrameList.GetTotal(); t++)
	{
		double TimeNow = FrameList.GetFrame(t) * FrameTicks;

		// Advance progress bar; obey bailout.
		INT Progress = (INT) 100.0f * ( t/(FrameList.GetTotal()+0.01f) );

		l_pBar.PutValue ( Progress );

		if ( l_pBar.IsCancelPressed() )
		{
			if ( MessageBox ( NULL, "Really cancel?", "Cancel Export", MB_YESNO|MB_ICONWARNING ) == IDYES )
			{
				break;
			} else {

				l_pBar.PutVisible( true );
			}

		}

		if (DEBUGFILE && to_path[0])
		{
			DLog.Logf(" Frame time: %f   frameticks %f  \n", (FLOAT)TimeNow, (FLOAT)FrameTicks);
		}

		for (int i = RootBoneIndex; i<SerialTree.Num(); i++)
		{
			if ( SerialTree[i].InSkeleton )
			{
				XSI::X3DObject ThisNode = ((x3dpointer*)SerialTree[i].node)->m_ptr;

				Thing->RawAnimKeys[AnimIndex].Time = 1.0f;

				XSI::MATH::CTransformation xfo;

				if (  i == RootBoneIndex )
				{
					xfo = ThisNode.GetKinematics().GetGlobal().GetTransform(TimeNow);

					// convert coordinate systems
					//
					XSI::MATH::CTransformation flip;
					flip.SetRotationFromXYZAnglesValues ( 90.0 * 0.01745329251994329547, 0.0, 0.0);
					xfo.MulInPlace ( flip );
				}
				else
				{
					//
					// do hierarchical compensation
					//

					xfo = ThisNode.GetKinematics().GetLocal().GetTransform(TimeNow);

					NodeInfo* ParentInfo = &SerialTree[ SerialTree[i].ParentIndex ];

					//
					// special case for effectors in XSI chains ( effector parented to the chain root)
					//

					if ( ThisNode.GetType().IsEqualNoCase(XSI::siChainEffPrimType) )
					{
						//
						// export it's global xfo
						//
						xfo = ThisNode.GetKinematics().GetGlobal().GetTransform(TimeNow);

					}

					//
					// If this bone's parent was optimized out, do
					// hierarchical compensation. (fancy term for
					// saying that we concatenate the transform for
					// each node upwards in the hierarchy that were
					// optimized out.)
					//

					while ( !ParentInfo->InSkeleton )
					{
						XSI::X3DObject l_pParent = ((x3dpointer*)ParentInfo->node)->m_ptr;
						XSI::MATH::CTransformation parent_xfo;

						parent_xfo = l_pParent.GetKinematics().GetLocal().GetTransform(TimeNow);

						if ( MatchNodeToIndex( (x3dpointer*)ParentInfo->node ) == RootBoneIndex )
						{
							XSI::MATH::CTransformation flip;
							flip.SetRotationFromXYZAnglesValues ( 90.0 * 0.01745329251994329547, 0.0, 0.0);
							parent_xfo.MulInPlace ( flip );
						}

						xfo.MulInPlace ( parent_xfo );

						if (( ParentInfo->ParentIndex == 0 ) || ( ParentInfo->ParentIndex == -1 ))
						{
							break;
						}

						ParentInfo = &SerialTree[ ParentInfo->ParentIndex ];

					}
				}


				XSI::MATH::CQuaternion quat = xfo.GetRotationQuaternion();
				double rx,ry,rz;
				xfo.GetRotationFromXYZAnglesValues ( rx, ry, rz );

				if ( i == RootBoneIndex ) //#check whether root
				{
					Thing->RawAnimKeys[AnimIndex].Orientation = FQuat(quat.GetX(),quat.GetY(),quat.GetZ(),quat.GetW());
				}
				else
				{
					Thing->RawAnimKeys[AnimIndex].Orientation = FQuat(quat.GetX(),quat.GetY(),quat.GetZ(),-quat.GetW());
					Thing->RawAnimKeys[AnimIndex].Orientation.Normalize ();
				}

				double tx, ty, tz;
				xfo.GetTranslationValues(tx,ty,tz);
				Thing->RawAnimKeys[AnimIndex].Position = FVector(tx,ty,tz);

				if( DoExportScale )
				{
					double sx, sy, sz;
					xfo.GetScalingValues(sx,sy,sz);

					Thing->RawScaleKeys[AnimIndex].Time = 1.0f;
					Thing->RawScaleKeys[AnimIndex].ScaleVector = FVector(sx,sy,sz);
				}

				AnimIndex++;
			}
		}
		FrameCount++;
	} // Time

	DLog.Close();


	if (AnimIndex != (OurBoneTotal * OurTotalFrames))
	{
		TCHAR OutString[500];
		sprintf(OutString," Anim: digested %i Expected %i FrameCalc %i FramesProc %i \n",AnimIndex, OurBoneTotal*OurTotalFrames, OurTotalFrames,FrameCount);
		MessageBox(GetActiveWindow(),OutString, "err", MB_OK);
		return 0;
	};

	return 1;
};

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
		char LogFileName[MAXPATHCHARS]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");
		sprintf(LogFileName,"\\ActXSkin.LOG");
		DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	if (OurSkins.Num() == 0) return 0; // No Physique, Max- or simple geometry 'skin' detected. Return with empty SkinData.

	DLog.Logf(" Starting skin digestion logfile \n");

	// Digest multiple meshes...
	INT i;
	for(i=0; i<OurSkins.Num(); i++)
	{
		DebugBox("SubMesh %i Points %i Wedges %i Faces %i",i,Thing->SkinData.Points.Num(),Thing->SkinData.Wedges.Num(),Thing->SkinData.Faces.Num());

		VSkin LocalSkin;
		LocalSkin.Points.Empty();
		LocalSkin.Wedges.Empty();
		LocalSkin.Faces.Empty();
		LocalSkin.RawWeights.Empty();

		FlippedFaces = 0;

		if( OurSkins[i].IsSmoothSkinned  )
		{
			// Skinned mesh.
			ProcessMesh( OurSkins[i].Node, OurSkins[i].SceneIndex, Thing, LocalSkin, 1, &OurSkins[i] );
		}
		else if ( OurSkins[i].IsTextured  )
		{
			// Textured geometry; assumed to be its own bone.
			ProcessMesh( OurSkins[i].Node, OurSkins[i].SceneIndex, Thing, LocalSkin, 0, &OurSkins[i] );
		}

		if( FlippedFaces )
		{
		// TODO XSI	DebugBox(" Flipped faces: %i on node: %s",FlippedFaces,((INode*)OurSkins[i].Node)->GetName() );
		}

		DebugBox(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );

		// Add points, wedges, faces, RAW WEIGHTS from localskin to Thing->Skindata...
		// Indices need to be shifted to create a single mesh soup!

		INT PointBase = Thing->SkinData.Points.Num();
		INT WedgeBase = Thing->SkinData.Wedges.Num();
		INT j;

		for(j=0; j<LocalSkin.Points.Num(); j++)
		{
			Thing->SkinData.Points.AddItem(LocalSkin.Points[j]);
		}

		for(INT j=0; j<LocalSkin.Wedges.Num(); j++)
		{
			VVertex Wedge = LocalSkin.Wedges[j];
			//adapt wedge's POINTINDEX...
			Wedge.PointIndex += PointBase;
			Thing->SkinData.Wedges.AddItem(Wedge);
		}

		for(INT j=0; j<LocalSkin.Faces.Num(); j++)
		{
			//WedgeIndex[3] needs replacing
			VTriangle Face = LocalSkin.Faces[j];
			// adapt faces indices into wedges...
			Face.WedgeIndex[0] += WedgeBase;
			Face.WedgeIndex[1] += WedgeBase;
			Face.WedgeIndex[2] += WedgeBase;
			Thing->SkinData.Faces.AddItem(Face);
		}

		for(j=0; j<LocalSkin.RawWeights.Num(); j++)
		{
			VRawBoneInfluence RawWeight = LocalSkin.RawWeights[j];
			// adapt RawWeights POINTINDEX......
			RawWeight.PointIndex += PointBase;
			Thing->SkinData.RawWeights.AddItem(RawWeight);
		}
	}

	DebugBox( "Total raw materials [%i] ", Thing->SkinData.RawMaterials.Num() ); //#debug

	UBOOL IndexOverride = false;
	// Now digest all the raw materials we encountered for export, and build appropriate flags.
	for( i=0; i < Thing->SkinData.RawMaterials.Num(); i++)
	{

		VMaterial Stuff;
		XSI::Material ThisMtl = XSI::Material( ((x3dpointer*)Thing->SkinData.RawMaterials[i])->m_ptr );

		char *MatNameStr = new char [ ThisMtl.GetName().Length() + 1 ];
		W2AHelper ( MatNameStr, ThisMtl.GetName().GetWideString() );


		// Material name.
		Stuff.SetName( MatNameStr );

		delete [] MatNameStr;

		//TODO XSI : DebugBox( "Material: %s  [%i] ", ThisMtl->GetName(), i ); //#debug

		// Rendering flags.
		CalcMaterialFlags( ThisMtl, Stuff );
		// Reserved - for material sorting.
		Stuff.AuxMaterial = 0;

		// Skin index override detection.
		if (Stuff.AuxFlags ) IndexOverride = true;

		// Add material. Uniqueness already guaranteed.
		Thing->SkinData.Materials.AddItem( Stuff );

		VBitmapOrigin TextureSource;

		XSI::OGLTexture tex = ThisMtl.GetOGLTexture();
		XSI::CString XSIPath = tex.GetFullName();
		char *asciipath = new char [ XSIPath.Length() + 1 ];
		W2AHelper ( asciipath, XSIPath.GetWideString() );

		char drive[MAX_PATH];
		char folder[MAX_PATH];
		char filename[MAX_PATH];
		char extension[MAX_PATH];

		_splitpath ( asciipath, drive, folder, filename, extension );

		sprintf ( TextureSource.RawBitmapName, "%s%s", filename, extension );
		sprintf ( TextureSource.RawBitmapPath, "%s\\%s", drive, folder );


		Thing->SkinData.RawBMPaths.AddItem( TextureSource );

	}

	DebugBox( "Total unique materials [%i] ", Thing->SkinData.Materials.Num() );

	//
	// Sort Thing->materials in order of 'skin' assignment.
	// Auxflags = 1: explicitly defined material -> these have be sorted in their assigned positions;
	// if they are incomplete (e,g, skin00, skin02 ) then the 'skin02' index becomes 'skin01'...
	// Wedge indices  then have to be remapped also.
	//
	if ( IndexOverride)
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


int	SceneIFC::ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin, SkinInf* SkinHandle )
{
	// We need a digest of all objects in the scene that are meshes belonging
	// to the skin. Use each meshes' parent node if it is a non-physiqued one;
	// do physique bones digestion where possible.

	// Get vertexes (=points)
	// Get faces + allocate wedge for each face vertex.

	if( 0 )
	{
		char LogFileName[MAX_PATH];
		sprintf(LogFileName,"\\Aaaa.LOG");
		DLog.Open(LogPath,LogFileName,DOLOGFILE);
	}

	if( SkinNode == NULL)
	{
		return 0;
	}

	// Get the selection state (should match SerialTree[TreeIndex].IsSelected. )
	XSI::X3DObject x3d = ((x3dpointer*)SkinNode)->m_ptr;
	UBOOL bMeshSelected = IsSelected ( x3d );

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

	XSI::Geometry geom(x3d.GetActivePrimitive().GetGeometry());
	XSI::PolygonMesh polygonmesh(geom);


	if (!polygonmesh.IsValid())
	{
		ErrorBox(" Invalid TriMesh Error. ");
		return 0;
	}

	XSI::MATH::CTransformation SkinLocalTM = x3d.GetKinematics().GetLocal().GetTransform(TimeStatic);

	XSI::MATH::CTransformation flip;
	flip.SetRotationFromXYZAnglesValues ( 90.0 * 0.01745329251994329547, 0.0, 0.0);
	SkinLocalTM.MulInPlace ( flip );

	XSI::CPointRefArray points = polygonmesh.GetPoints();

	XSI::CTriangleRefArray triarray = polygonmesh.GetTriangles();

	//
	//
	// Create cluster map to materials.
	//
	//

	//
	// initialize all tris to default material for now
	//

	int DefaultMaterial = DigestMaterial(  SkinNode,  0,  Thing->SkinData.RawMaterials );
	CSIBCArray<long>	TriangleMaterialMap;
	TriangleMaterialMap.Extend( triarray.GetCount() );

	for (long v=0;v<triarray.GetCount();v++)
	{
		TriangleMaterialMap[v] = DefaultMaterial;
	}

	// get all polygon clusters on mesh
	//
	XSI::CRefArray		allClusters;
	polygonmesh.GetClusters().Filter(L"poly",XSI::CStringArray(),L"",allClusters);

	for (int c=0;c<allClusters.GetCount();c++)
	{
		//
		// get this cluster's material
		//

		XSI::Cluster Thecluster = allClusters[c];
		XSI::Material l_pMat = Thecluster.GetMaterial();

		//
		// digest it
		//

		int ClusterMaterialIndex = DigestLocalMaterial ( l_pMat, Thing->SkinData.RawMaterials );

		//
		// update the triangle map to point all tris using this material
		// to the correct index
		//

		XSI::CClusterElementArray clusterElementArray = Thecluster.GetElements();
		XSI::CLongArray values(clusterElementArray.GetArray());
		long countPolyIndices = values.GetCount();

		for (long v=0;v<countPolyIndices;v++)
		{
			for (long w=0;w<triarray.GetCount();w++)
			{
				XSI::Triangle tri = triarray[w];

				if(tri.GetPolygonIndex() == values[v])
				{
					TriangleMaterialMap[w] = ClusterMaterialIndex;
				}
			}
		}
	}

	if (!SmoothSkin) // normal mesh
	{

		INT NumberVertices = points.GetCount();

		DebugBox("Vertices total: %i",NumberVertices);

		if (DEBUGFILE && to_path[0])
		{
			DLog.Logf(" Mesh totals:  Points %i Faces %i \n", NumberVertices,polygonmesh.GetTriangles().GetCount());
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
			XSI::Point point(points.GetItem(i) );
			XSI::MATH::CVector3 pos( point.GetPosition() );
			pos *= SkinLocalTM;

			VPoint NewPoint;
			NewPoint.Point = FVector(pos.GetX(),pos.GetY(),pos.GetZ());
			LocalSkin.Points.AddItem(NewPoint);

			VRawBoneInfluence TempInf;
			TempInf.PointIndex = i; // raw skinvertices index
			TempInf.BoneIndex  =  MatchedNode; // Bone is it's own node.
			TempInf.Weight     = 1.0f; // Single influence by definition.

			LocalSkin.RawWeights.AddItem(TempInf);
		}

		DebugBox("Digested geometry mesh - Points %",LocalSkin.Points.Num() );
	}

	int NodeMatchWarnings = 0;

	//
	// Process a skin geometry object.
	//
	if ( SmoothSkin )
	{
		if ( SkinHandle->IsPhysique )
		{

			//
			// Create an inverted list of weights->bone so we can easily populate
			// the ActorX list after
			//

			INT NumberVertices = points.GetCount();

			CSIBCArray<weightedvertex>	XSIEnvelope;
			XSIEnvelope.Extend(NumberVertices);

			XSI::SceneItem item(x3d);
			XSI::CRefArray l_Envelope = item.GetEnvelopes();

			assert ( l_Envelope.GetCount() == 1 );	// what do we do for multiple envelopes??


			XSI::Envelope l_env = l_Envelope[0];

			XSI::CRefArray	l_Deformers = l_env.GetDeformers();

			for (int d=0;d<l_Deformers.GetCount();d++)
			{

				XSI::X3DObject l_pCurrentDeformer = l_Deformers[d];

				XSI::CDoubleArray	myWeightArray = l_env.GetDeformerWeights( l_pCurrentDeformer, 0.0 );
				XSI::CClusterElementArray l_cElements = l_env.GetElements(0);
				XSI::CLongArray indexArray = l_cElements.GetArray();


				for (int w=0;w<myWeightArray.GetCount();w++)
				{
					if ( myWeightArray[w] > 0.00001f ) // TODO XSI: use min??
					{
						int VertIndex = indexArray[w];

						//
						//  find the ActorX index for this deformer
						//
						x3dpointer *p=NULL;
						XSI::CRef CRefDeformer(l_pCurrentDeformer);
						for (int n=0;n<g_XSIInterface->m_pCachedScene.GetUsed();n++)
						{
							if ( g_XSIInterface->m_pCachedScene[n]->m_ptr == CRefDeformer)
							{
								p = g_XSIInterface->m_pCachedScene[n];
								break;
							}

						}

						int BoneIndex = MatchNodeToIndex( p );

						weightedvertex* l_pVert = &XSIEnvelope[VertIndex];
						l_pVert->m_pWeights.Extend(1);
						l_pVert->m_pWeights[l_pVert->m_pWeights.GetUsed()-1].m_iBoneIndex = BoneIndex;
						l_pVert->m_pWeights[l_pVert->m_pWeights.GetUsed()-1].m_fWeight = myWeightArray[w];
						l_pVert->m_pWeights[l_pVert->m_pWeights.GetUsed()-1].m_ptr = p;


					}

				}

			}

			// Get vertices + weights.
			for (int VertexIndex=0; VertexIndex< NumberVertices ; VertexIndex++)
			{
				weightedvertex* l_pVert = &XSIEnvelope[VertexIndex];

				INT NumberBones = l_pVert->m_pWeights.GetUsed();

				TArray <SmoothVertInfluence> RawInfluences;

					for (int BoneIndex=0; BoneIndex<NumberBones; BoneIndex++)
					{
						singleweight* sweight = &l_pVert->m_pWeights[BoneIndex];

						SmoothVertInfluence NewInfluence( 0.0f, -1 );

						NewInfluence.Weight = sweight->m_fWeight;


						// Now find the bone and add the influence only if there was a nonzero weight.
						if( NewInfluence.Weight > 0.0f ) // SMALLESTWEIGHT constant instead ?
						{
							NewInfluence.BoneIndex = sweight->m_iBoneIndex;

							// Node not found ?
							if( NewInfluence.BoneIndex < 0 )
							{
								NewInfluence.Weight = 0.0f;
								NodeMatchWarnings++;
							}

							SerialTree[  sweight->m_iBoneIndex ].LinksToSkin++; // Count number of vertices !

							NewInfluence.BoneIndex = Thing->MatchNodeToSkeletonIndex(sweight->m_ptr);

							RawInfluences.AddItem( NewInfluence );
						}
						else //#SKEL!!!!!!!!!!!!!
						{
							TCHAR OutString[500];
							sprintf(OutString,"Weight invalid: [%f] - fixing it up....", NewInfluence.Weight);
							ErrorBox(OutString);


						}
					}



				// Verify we found at least one influence for this vert.
				static INT PhysiqueTotalErrors = 0;
				if( RawInfluences.Num() == 0 ) //&& PhysiqueTotalErrors == 0 ) //#SKEL
				{
					TCHAR OutString[500];
					sprintf(OutString,"Invalid number of physique bone influences (%i) for skin vertex %i",NumberBones,VertexIndex );
					ErrorBox(OutString);
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

				XSI::Point point(points.GetItem(VertexIndex) );
				XSI::MATH::CVector3 pos( point.GetPosition() );
				pos *= SkinLocalTM;

				VPoint NewPoint;
				NewPoint.Point = FVector(pos.GetX(),pos.GetY(),pos.GetZ());
				LocalSkin.Points.AddItem(NewPoint);


			}


		}

	}

	if( NodeMatchWarnings )
	{
		ErrorBox("Warning: %8i 'Unmatched node ID' Physique skin vertex errors encountered.",(INT)NodeMatchWarnings);
		NodeMatchWarnings=0;
	}



	//
	// Digest any mesh triangle data -> into unique wedges (ie common texture UV'sand (later) smoothing groups.)
	//

	UBOOL NakedTri = false;
	int Handedness = 0;
	int NumFaces = polygonmesh.GetTriangles().GetCount();

	DebugBox("Digesting Normals");

	XSI::MATH::CTransformation Mesh2WorldTM = x3d.GetKinematics().GetLocal().GetTransform(TimeStatic);
	Mesh2WorldTM.SetScalingFromValues ( 1.0, 1.0, 1.0);	// TODO: Really??

	DebugBox("Digesting %i Triangles.",NumFaces);

	if( LocalSkin.Points.Num() ==  0 ) // Actual points allocated yet ? (Can be 0 if no matching bones were found)
	{
		NumFaces = 0;
		// TODO XSI: ErrorBox("Node without matching vertices encountered for mesh: [%s]", ((INode*)SkinNode)->GetName());
	}

	INT CountNakedTris = 0;
	INT CountTexturedTris = 0;
	INT TotalTVNum = points.GetCount();

	MeshProcessor TempTranslator;
	UBOOL bSmoothingGroups = false;
	if( OurScene.DoUnSmooth )
	{
		TempTranslator.createNewSmoothingGroups( x3d );

		if( TempTranslator.FaceSmoothingGroups.Num()  )
		{
			bSmoothingGroups = true;
		}
	}


	INT TriIndex;
	for (TriIndex = 0; TriIndex < NumFaces; TriIndex++)
	{
		XSI::Triangle TriFace = triarray[TriIndex];

		VVertex Wedge0;
		VVertex Wedge1;
		VVertex Wedge2;

		VTriangle NewFace;

		// Note that because GetPolygonIndex() is used, the correct smoothing group data will be retrieved for each triangle regardless of the smoothing group array having been extracted from potentially non-triangular polygons.
		NewFace.SmoothingGroups = bSmoothingGroups ? TempTranslator.PolySmoothingGroups[TriFace.GetPolygonIndex()] : 1 ;

		XSI::MATH::CVector3 XSINormal = GetTriangleNormal ( TriFace );

		XSINormal *= Mesh2WorldTM;

		FVector FaceNormal = FVector( XSINormal.GetX(), XSINormal.GetY(), XSINormal.GetZ());

		NewFace.WedgeIndex[0] = LocalSkin.Wedges.Num();
		NewFace.WedgeIndex[1] = LocalSkin.Wedges.Num()+1;
		NewFace.WedgeIndex[2] = LocalSkin.Wedges.Num()+2;

		// TODO XSI: if (&LocalSkin.Points[0] == NULL )PopupBox("Triangle digesting 1: [%i]  %i %i",TriIndex,(DWORD)TriFace, (DWORD)&LocalSkin.Points[0]); //#! LocalSkin.Points[9] failed;==0..

		// Face's vertex indices ('point indices')
		Wedge0.PointIndex = TriFace.GetIndexArray()[0];  // TriFace->v[0];
		Wedge1.PointIndex = TriFace.GetIndexArray()[1];
		Wedge2.PointIndex = TriFace.GetIndexArray()[2];

		// Vertex coordinates are in localskin.point, already in worldspace.
		FVector WV0 = LocalSkin.Points[ Wedge0.PointIndex ].Point;
		FVector WV1 = LocalSkin.Points[ Wedge1.PointIndex ].Point;
		FVector WV2 = LocalSkin.Points[ Wedge2.PointIndex ].Point;

		// Figure out the handedness of the face by constructing a normal and comparing it to Max's normal.
		FVector OurNormal = (WV0 - WV1) ^ (WV2 - WV0);
		// OurNormal /= sqrt(OurNormal.SizeSquared()+0.001f);// Normalize size (unnecessary ?);

		Handedness = ( ( OurNormal | FaceNormal ) < 0.0f); // normals anti-parallel ?

		BYTE MaterialIndex = TriangleMaterialMap[TriIndex];


		//NewFace.MatIndex = MaterialIndex;

		Wedge0.Reserved = 0;
		Wedge1.Reserved = 0;
		Wedge2.Reserved = 0;

		//
		// Get UV texture coordinates: allocate a Wedge with for each and every
		// UV+ MaterialIndex ID; then merge these, and adjust links in the Triangles
		// that point to them.
		//

		Wedge0.U = 0; // bogus UV's.
		Wedge0.V = 0;
		Wedge1.U = 1;
		Wedge1.V = 0;
		Wedge2.U = 0;
		Wedge2.V = 1;

		// Jul 12 '01: -> Changed to uses alternate indicator - the HAS_TVERTS flag apparently not supported correctly in Max 4.X
		if( TotalTVNum > 0 )   // (TriFace->flags & HAS_TVERTS)
		{
			DWORD UVidx0 = TriFace.GetIndexArray()[0];
			DWORD UVidx1 = TriFace.GetIndexArray()[1];
			DWORD UVidx2 = TriFace.GetIndexArray()[2];

			XSI::CUV UV0 = TriFace.GetUVArray()[0];
			XSI::CUV UV1 = TriFace.GetUVArray()[1];
			XSI::CUV UV2 = TriFace.GetUVArray()[2];

			Wedge0.U =  UV0.u;
			Wedge0.V =  1.0f-UV0.v; //#debug flip Y always ?
			Wedge1.U =  UV1.u;
			Wedge1.V =  1.0f-UV1.v;
			Wedge2.U =  UV2.u;
			Wedge2.V =  1.0f-UV2.v;

			CountTexturedTris++;
		}
		else
		{
			CountNakedTris++;
			NakedTri = true;
		}

		//
		// This should give a correct final material index EVEN for multiple multi/sub materials on complex hierarchies of meshes.
		//

		INT ThisMaterial = DigestMaterial(  SkinNode,  MaterialIndex,  Thing->SkinData.RawMaterials );

		NewFace.MatIndex = ThisMaterial;

		Wedge0.MatIndex = ThisMaterial;
		Wedge1.MatIndex = ThisMaterial;
		Wedge2.MatIndex = ThisMaterial;

		LocalSkin.Wedges.AddItem(Wedge0);
		LocalSkin.Wedges.AddItem(Wedge1);
		LocalSkin.Wedges.AddItem(Wedge2);

		LocalSkin.Faces.AddItem(NewFace);
		if (DEBUGFILE && to_path[0]) DLog.Logf(" [tri %4i ] Wedge UV  %f  %f  MaterialIndex %i  Vertex %i \n",TriIndex,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].U ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].V ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].MatIndex, LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].PointIndex );
	}

	if (NakedTri)
	{
		// TODO XSI:
		//TSTR NodeName( ((INode*)SkinNode)->GetName() );
		//int numTexVerts = OurTriMesh->getNumTVerts();
		//WarningBox("Triangles [%i] without UV mapping detected for mesh object: %s ", CountNakedTris, NodeName );
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
				if ( Thing->IsSameVertex( LocalSkin.Wedges[t], LocalSkin.Wedges[d] ) )
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

	for (TriIndex = 0; TriIndex < NumFaces; TriIndex++)
	{
		LocalSkin.Faces[TriIndex].WedgeIndex[0] = WedgeRemap[TriIndex*3+0];
		LocalSkin.Faces[TriIndex].WedgeIndex[1] = WedgeRemap[TriIndex*3+1];
		LocalSkin.Faces[TriIndex].WedgeIndex[2] = WedgeRemap[TriIndex*3+2];

		// Verify we all mapped within new bounds.
		if (LocalSkin.Faces[TriIndex].WedgeIndex[0] >= NewWedges.Num()) ErrorBox("Wedge Overflow 1");
		if (LocalSkin.Faces[TriIndex].WedgeIndex[1] >= NewWedges.Num()) ErrorBox("Wedge Overflow 1");
		if (LocalSkin.Faces[TriIndex].WedgeIndex[2] >= NewWedges.Num()) ErrorBox("Wedge Overflow 1");

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
			DLog.Logf(" [ %4i ] Wedge UV  %f  %f  Material %i  Vertex %i \n",t,NewWedges[t].U,NewWedges[t].V,NewWedges[t].MatIndex,NewWedges[t].PointIndex );
		}
	}}

	if (DEBUGFILE && to_path[0])
	{
		DLog.Logf(" Digested totals: Wedges %i Points %i Faces %i \n", LocalSkin.Wedges.Num(),LocalSkin.Points.Num(),LocalSkin.Faces.Num());
	}


	return 1;
};

int SceneIFC::DigestMaterial(AXNode *node,  INT matIndex,  TArray<void*>& RawMaterials)
{
	// Get the material from the node
	XSI::Material mat;
	if(RawMaterials.Num() > matIndex)
	{
		 mat = ((x3dpointer*)RawMaterials[matIndex])->m_ptr;
	}
	else
	{
		XSI::X3DObject x3d = ((x3dpointer*)node)->m_ptr;
		mat = x3d.GetMaterial();
	}


	// This should be checked for in advance.
	if (!mat.IsValid())
	{
		return -1; //255; // No material assigned
	}

	for (int m=0;m<g_XSIInterface->m_pCachedMaterials.GetUsed();m++)
	{
		if ( g_XSIInterface->m_pCachedMaterials[m]->m_ptr == mat )
		{
			return m;
		}
	}

	x3dpointer* ptr = new x3dpointer(mat);

	g_XSIInterface->m_pCachedMaterials.Extend(1);
	g_XSIInterface->m_pCachedMaterials[g_XSIInterface->m_pCachedMaterials.GetUsed()-1] = ptr;
	// The different material properties can be investigated at the end, for now we care about uniqueness only.
	RawMaterials.AddUniqueItem( (void*)ptr);

	return( RawMaterials.Where( (void*)ptr) );
}

int SceneIFC::DigestSkeleton(VActor *Thing)
{

	TextFile SkelLog;


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

	OurBoneTotal = 0;
	for (int i = RootBoneIndex; i<SerialTree.Num(); i++)
	{
		if ( SerialTree[i].InSkeleton )
		{
			if (i != RootBoneIndex)
			{
				XSI::X3DObject obj ( ((x3dpointer*)SerialTree[i].node)->m_ptr );

				if ( ( obj.GetType().IsEqualNoCase(XSI::siChainEffPrimType) ) ||
					( obj.GetType().IsEqualNoCase(XSI::siChainRootPrimType) ) )
				{
					SerialTree[i].InSkeleton = false;
					continue;
				}

				if ( !g_XSIInterface->IsDeformer ( obj ))
				{
					SerialTree[i].InSkeleton = false;
					continue;
				}
			}

			OurBoneTotal++;
			NewParents.AddItem( i );

			FNamedBoneBinary ThisBone; //= &Thing->RefSkeletonBones[BoneIndex];
			ThisBone.Flags = 0;
			ThisBone.NumChildren = 0;
			ThisBone.ParentIndex = -1;

			XSI::X3DObject ThisNode = ((x3dpointer*)SerialTree[i].node)->m_ptr;
			char *BoneNameStr = new char [ ThisNode.GetName().Length() + 1 ];
			W2AHelper ( BoneNameStr, ThisNode.GetName().GetWideString() );


			//Copy name. Truncate/pad to 19 chars for safety.
			//TSTR BoneName = BoneNameStr;

			//BoneName.Resize(31); // will be padded if needed.
			_tcscpy((ThisBone.Name),BoneNameStr);

			delete [] BoneNameStr;

			ThisBone.Flags = 0;//ThisBone.Info.Flags = 0;
			ThisBone.NumChildren = SerialTree[i].NumChildren; //ThisBone.Info.NumChildren = SerialTree[i].NumChildren;

			if (i != RootBoneIndex)
			{
				//#debug needs to be Local parent if we're not the root.
				int OldParent = SerialTree[i].ParentIndex;
				int LocalParent = -1;

				//
				// it is possible that we have removed the parent from the hierarchy.
				//

				if ( !SerialTree[OldParent].InSkeleton )
				{
					bool inSkeleton = false;

					while (!inSkeleton)
					{
						OldParent = SerialTree[OldParent].ParentIndex;
						inSkeleton = SerialTree[OldParent].InSkeleton ? true : false;
					}

				}

				// Find what new index this corresponds to. Parent must already be in set.
				for (int f=0; f<NewParents.Num(); f++)
				{
					if( NewParents[f] == OldParent ) LocalParent=f;
				}

				if (LocalParent == -1 )
				{
					ErrorBox(" Local parent lookup failed.");
					LocalParent = 0;
				}

				ThisBone.ParentIndex = LocalParent; //ThisBone.Info.ParentIndex = LocalParent;
			}
			else
			{
				ThisBone.ParentIndex = 0; // root links to itself...? .Info.ParentIndex
			}
/*

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
*/
			// Obtain local matrix unless this is the root of the skeleton.

			XSI::MATH::CTransformation xfo;

			if ( /*SerialTree[i].ParentIndex == 0*/ i == RootBoneIndex )
			{
				xfo = ThisNode.GetKinematics().GetGlobal().GetTransform();


				// convert coordinate systems

				XSI::MATH::CTransformation flip;
				flip.SetRotationFromXYZAnglesValues ( 90.0 * 0.01745329251994329547, 0.0, 0.0);
				xfo.MulInPlace ( flip );
			}
			else
			{
				//
				// do hierarchical compensation
				//

				xfo = ThisNode.GetKinematics().GetLocal().GetTransform();

				NodeInfo* ParentInfo = &SerialTree[ SerialTree[i].ParentIndex ];

				//
				// special case for effectors in XSI chains ( effector parented to the chain root)
				//

				if ( ThisNode.GetType().IsEqualNoCase(XSI::siChainEffPrimType) )
				{
					//
					// export it's global xfo
					//
					xfo = ThisNode.GetKinematics().GetGlobal().GetTransform();

				}

				//
				// If this bone's parent was optimized out, do
				// hierarchical compensation. (fancy term for
				// saying that we concatenate the transform for
				// each node upwards in the hierarchy that were
				// optimized out.)
				//

				while ( !ParentInfo->InSkeleton )
				{
					XSI::X3DObject l_pParent = ((x3dpointer*)ParentInfo->node)->m_ptr;
					XSI::MATH::CTransformation parent_xfo;

					parent_xfo = l_pParent.GetKinematics().GetLocal().GetTransform();

					if ( MatchNodeToIndex( (x3dpointer*)ParentInfo->node ) == RootBoneIndex )
					{
						XSI::MATH::CTransformation flip;
						flip.SetRotationFromXYZAnglesValues ( 90.0 * 0.01745329251994329547, 0.0, 0.0);
						parent_xfo.MulInPlace ( flip );
					}

					xfo.MulInPlace ( parent_xfo );


					if (( ParentInfo->ParentIndex == 0 ) || ( ParentInfo->ParentIndex == -1 ))
					{
						break;
					}

					ParentInfo = &SerialTree[ ParentInfo->ParentIndex ];

				}
			}

			ThisBone.BonePos.Length = 0.0f ;

			XSI::MATH::CQuaternion quat = xfo.GetRotationQuaternion();
			double rx,ry,rz;
			xfo.GetRotationFromXYZAnglesValues ( rx, ry, rz );

			if ( /*SerialTree[i].ParentIndex == 0*/ i == RootBoneIndex ) //#check whether root
			{
				//XSI::MATH::CTransformation flip;
				//flip.SetRotationFromXYZAnglesValues ( -90.0 * 0.01745329251994329547, 0.0, 0.0);
				//xfo.MulInPlace ( flip );

				//quat = xfo.GetRotationQuaternion();

				ThisBone.BonePos.Orientation = FQuat(quat.GetX(),quat.GetY(),quat.GetZ(),quat.GetW());
			}
			else
			{
			//	XSI::MATH::CTransformation flip;
			//	flip.SetRotationFromXYZAnglesValues ( 0.0, 0.0, -90.0 * 0.01745329251994329547);
			//	xfo.MulInPlace ( flip );

			//	quat = xfo.GetRotationQuaternion();
				ThisBone.BonePos.Orientation = FQuat(quat.GetX(),quat.GetY(),quat.GetZ(),-quat.GetW());
				ThisBone.BonePos.Orientation.Normalize ();
			}

			double tx, ty, tz;
			xfo.GetTranslationValues(tx,ty,tz);
			ThisBone.BonePos.Position = FVector(tx,ty,tz);

			Thing->RefSkeletonNodes.AddItem(((void*)SerialTree[i].node));
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
				WarningBox( " Duplicate bone name encountered in skeleton: %s ", Thing->RefSkeletonBones[j].Name );
			}
		}
	}}
	if (DuplicateBones)
	{
		WarningBox( "%i total duplicate bone name(s) detected.", DuplicateBones );
	}

	DLog.Close();

	return 1;
}

int SceneIFC::EvaluateSkeleton( INT RequireBones )
{
		// Our skeleton: always the one hierarchy
	// connected to the scene root which has either the most
	// members, or the most SELECTED members, or a physique
	// mesh attached...
	// For now, a hack: find all root bones; then choose the one
	// which is selected.

	OurBoneTotal = 0;
	RootBoneIndex = 0;
	TotalBipBones = 0;
	if (SerialTree.Num() == 0) return 0;

	int BonesMax = 0;
	int i;

	for( i=0; i<SerialTree.Num(); i++)
	{
		if( (SerialTree[i].IsBone != 0 )/* && (SerialTree[i].ParentIndex == 0)*/ )
		{
			// choose one with the biggest number of bones, that must be the
			// intended skeleton/hierarchy.

			BonesMax += MarkBonesOfSystem(i);
		}
	}

	//
	// establish the RootBoneIndex
	//
		// Establish our hierarchy by way of Serialtree indices... upward only.
	RootBoneIndex = MatchNodeToIndex( g_XSIInterface->GetRootPointer() );

	if (RootBoneIndex <= 0) return 0;// failed to find any.

	SerialTree[RootBoneIndex].IsBone = true;

	// Real node
	OurRootBone = SerialTree[RootBoneIndex].node;

	// Total bones
	OurBoneTotal = MarkBonesOfSystem(RootBoneIndex);

	// Tally the elements of the current skeleton
	for( i=0; i<SerialTree.Num(); i++)
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
}


int SceneIFC::RecurseValidBones(const int RIndex, int &BoneCount)
{
	if ( SerialTree[RIndex].IsBone == 0) return 0; // Exit hierarchy for non-bones...

	SerialTree[RIndex].InSkeleton = 1; //part of our skeleton.
	BoneCount++;

	for (int c = 0; c < SerialTree[RIndex].NumChildren; c++)
	{
		if( GetChildNodeIndex(RIndex,c) <= 0 ) ErrorBox("GetChildNodeIndex assertion failed");

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


int SceneIFC::GetSceneInfo()
{

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
	int i;
	for( i=0; i<SerialTree.Num(); i++)
	{
		XSI::X3DObject x3d = ((x3dpointer*)SerialTree[i].node)->m_ptr;
		XSI::SceneItem item ( x3d );

		XSI::Geometry geom(x3d.GetActivePrimitive().GetGeometry());
		XSI::PolygonMesh polygonmesh(geom);
		// Any meshes at all ?
		SerialTree[i].HasMesh = polygonmesh.IsValid();

		// Check for a bone candidate.
		//SerialTree[i].IsBone = EvaluateBone(i);
		SerialTree[i].IsBone = g_XSIInterface->IsBone(x3d);

		if ( SerialTree[i].IsBone )
		{
			LogThis ( XSI::CString(L"Adding bone: ") + XSI::SceneItem ( x3d ).GetName() );
		}

		XSI::CRefArray l_Envelope = item.GetEnvelopes();

		SerialTree[i].HasTexture = 0;

		// Any texture ?
		if (SerialTree[i].HasMesh)
			SerialTree[i].HasTexture = x3d.GetMaterial().GetOGLTexture().IsValid();

		GeomMeshes += SerialTree[i].HasMesh;

		if ( (l_Envelope.GetCount() ) && DoSkinGeom )
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

			NewSkin.IsPhysique = 1;
			NewSkin.IsMaxSkin =  0;

			OurSkins.AddItem(NewSkin);


			// Prefer the first or the selected skin if more than one encountered.
			if (( IsSelected( ((x3dpointer*)SerialTree[i].node)->m_ptr ) ) || (TotalSkinNodeNum == 0))
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
			//TSTR NodeName(((INode*)SerialTree[i].node)->GetName());
			//DebugBox("Textured geometry mesh: %s",NodeName);

		}
		else if (SerialTree[i].HasMesh && ExportingStaticMesh )
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
			//TSTR NodeName(((INode*)SerialTree[i].node)->GetName());
			//DebugBox("Textured geometry mesh: %s",NodeName);

		}

	}

	DebugBox("Number of skinned nodes: %i",OurSkins.Num());

	DebugBox("Add skinlink stats");

	// Accumulate some skin linking stats.
	for( i=0; i<SerialTree.Num(); i++)
	{
		if (SerialTree[i].LinksToSkin) LinkedBones++;
		TotalSkinLinks += SerialTree[i].LinksToSkin;
	}

	DebugBox("Tested main physique skin, bone links: %i total links %i",LinkedBones, TotalSkinLinks );


	//
	// Cull from skeleton all dummies (and point helpers) that don't have any physique or MaxSkin links to them and that don't have any children.
	//
	INT CulledDummies = 0;
	if (this->DoCullDummies)
	{
		for( i=0; i<SerialTree.Num(); i++)
		{
			XSI::CRefArray l_children = XSI::X3DObject(((x3dpointer*)SerialTree[i].node)->m_ptr).FindChildren ( L"", L"", XSI::CStringArray(), false );

			if (
			   ( SerialTree[i].LinksToSkin == 0)  &&
			   ( l_children.GetCount() ==0 ) &&
			   ( SerialTree[i].HasTexture == 0 ) &&
			   ( SerialTree[i].IsBone == 1 )
			   )
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].IsCulled = 1;
				CulledDummies++;
			}
		}
		if( !OurScene.InBatchMode )
		{
			PopupBox("Culled dummies: %i",CulledDummies);
		}
	}
	/*
	// Log for debugging purposes...
	if (DEBUGFILE)
	{
		for( i=0; i<SerialTree.Num(); i++)
		{
			TSTR NodeName(((INode*)SerialTree[i].node)->GetName());
			DLog.Logf("Node: %4i %-23s Dummy: %2i PHelper %2i Bone: %2i Skin: %2i PhyLinks: %4i NodeID %10i Parent %4i Isroot %4i HasTexture %i Childrn %i Culld %i Parent %3i \n",i,
				NodeName,
				SerialTree[i].IsDummy,
				SerialTree[i].IsPointHelper,
				SerialTree[i].IsBone,
				SerialTree[i].IsSkin,
				SerialTree[i].LinksToSkin,
				(INT)SerialTree[i].node,
				SerialTree[i].ParentIndex,
				(INT)((INode*)SerialTree[i].node)->IsRootNode(),
				SerialTree[i].HasTexture,
				((INode*)SerialTree[i].node)->NumberOfChildren(),
				SerialTree[i].IsCulled,
				SerialTree[i].ParentIndex
				);
		}
	}

  */
	TreeInitialized = 1;

	return 1;
}

int SceneIFC::EvaluateBone(int RIndex)
{

	XSI::CRef potentialbone = ((x3dpointer*)SerialTree[RIndex].node)->m_ptr;

	for( int i=0; i<SerialTree.Num(); i++)
	{

		XSI::X3DObject node = ((x3dpointer*)SerialTree[i].node)->m_ptr;
		XSI::SceneItem item ( node );

		if ( item.IsValid() )
		{
			XSI::CRefArray l_pEnvelopes = item.GetEnvelopes();

			for (int e=0;e<l_pEnvelopes.GetCount();e++)
			{
				XSI::Envelope env ( l_pEnvelopes[e] );

				XSI::CRefArray l_deformers = env.GetDeformers();

				for (int d=0;d<l_deformers.GetCount();d++)
				{
					/*
					XSI::CString name = XSI::SceneItem(l_deformers[d]).GetName();
					XSI::CString name2 = XSI::SceneItem(potentialbone).GetName();
					XSI::CString l_ctrs = L"Comparing ";
					l_ctrs += name;
					l_ctrs += L" and ";
					l_ctrs += name2;
					LogThis ( l_ctrs );
					*/
					if ( l_deformers[d] == potentialbone )
					{
					//	LogThis ( XSI::CString(L"Adding bone: ") + XSI::SceneItem ( potentialbone ).GetName() );
						return 1;
					}

				}

			}


		}

	}

	/*
	if ( !g_NoRecursion )
	{
		//
		// see if it's the parent of a bone... if so, we consider it a bone
		// and we will let ActorX optimize it out... I think...
		//

		XSI::X3DObject theParentBone (potentialbone);

		XSI::CRefArray l_allObjects = theParentBone.FindChildren ( L"", L"", XSI::CStringArray());

		g_NoRecursion = true;

		for (int n=0;n<l_allObjects.GetCount();n++)
		{
			XSI::X3DObject child = l_allObjects[n];

			x3dpointer* p = NULL;
			for (int c=0;c<g_XSIInterface->m_pCachedScene.GetUsed();c++)
			{
				if ( g_XSIInterface->m_pCachedScene[c]->m_ptr == child )
				{
					p = g_XSIInterface->m_pCachedScene[c];
					break;
				}

			}

			if ( EvaluateBone ( MatchNodeToIndex( p ) ) )
			{
				return 1;
			}
		}

		g_NoRecursion = false;
	}
*/
	return 0;
}

void SceneIFC::SurveyScene()
{
	//
	// Delete old cache list
	//

	long n;
	for (n=0;n<g_XSIInterface->m_pCachedScene.GetUsed();n++)
	{
		delete g_XSIInterface->m_pCachedScene[n];
	}
	g_XSIInterface->m_pCachedScene.DisposeData();

	for (n=0;n<g_XSIInterface->m_pCachedMaterials.GetUsed();n++)
	{
		delete g_XSIInterface->m_pCachedMaterials[n];
	}
	g_XSIInterface->m_pCachedMaterials.DisposeData();

	for (n=0;n<g_XSIInterface->m_pHRLevels.GetUsed();n++)
	{
		delete g_XSIInterface->m_pHRLevels[n];
	}
	g_XSIInterface->m_pHRLevels.DisposeData();




	//
	// Cache x3d object
	//

	XSI::Application app;
	XSI::Model root = app.GetActiveSceneRoot();

	XSI::X3DObject rootx3d = root;
	g_XSIInterface->m_pCachedScene.Extend(1);
	g_XSIInterface->m_pCachedScene[g_XSIInterface->m_pCachedScene.GetUsed()-1] = new x3dpointer(rootx3d);

	XSI::CRefArray l_allObjects = root.FindChildren ( L"", L"", XSI::CStringArray());

	for (n=0;n<l_allObjects.GetCount();n++)
	{
		g_XSIInterface->m_pCachedScene.Extend(1);
		g_XSIInterface->m_pCachedScene[g_XSIInterface->m_pCachedScene.GetUsed()-1] = new x3dpointer(l_allObjects[n]);

	}

	OurSkin=NULL;
	OurRootBone=NULL;

	XSI::Property playControl = app.GetActiveProject().GetProperties().GetItem(L"Play Control");

	float startframe = playControl.GetParameterValue(L"in");
	float endframe = playControl.GetParameterValue(L"out" );
	float framerate = playControl.GetParameterValue(L"rate");

	TimeStatic = startframe;

	// Frame 0 for reference:
	if( DoPoseZero ) TimeStatic = 0.0f;

	FrameStart = startframe;
	FrameEnd   = endframe;
	FrameTicks = 1.0f;
	FrameRate  = framerate;

	SerialTree.Empty();

	for (n=0;n<g_XSIInterface->m_pCachedScene.GetUsed();n++)
	{
		NodeInfo I;
		I.node = g_XSIInterface->m_pCachedScene[n];

   		SerialTree.AddItem(I);
	}

	XSI::CRefArray l_pSelection = app.GetSelection().GetArray();
	// Update basic nodeinfo contents
	for(int  i=0; i<SerialTree.Num(); i++)
	{
		// Update selected status

		SerialTree[i].IsSelected = false;
		for (int s=0;s<l_pSelection.GetCount();s++)
		{
			XSI::CRef cref = ((x3dpointer*)SerialTree[i].node)->m_ptr;
			if ( cref == l_pSelection[s] )
			{
				SerialTree[i].IsSelected = true;
			}

		}

		XSI::X3DObject m_parent = XSI::X3DObject(((x3dpointer*)SerialTree[i].node)->m_ptr).GetParent();

		x3dpointer *p=NULL;

		for (n=0;n<g_XSIInterface->m_pCachedScene.GetUsed();n++)
		{
			if ( g_XSIInterface->m_pCachedScene[n]->m_ptr == m_parent )
			{
				p = g_XSIInterface->m_pCachedScene[n];
				break;
			}

		}


		// Establish our hierarchy by way of Serialtree indices... upward only.
		SerialTree[i].ParentIndex = MatchNodeToIndex( p );

		XSI::CRefArray children = XSI::X3DObject(((x3dpointer*)SerialTree[i].node)->m_ptr).FindChildren(L"", L"", XSI::CStringArray(), false );

		SerialTree[i].NumChildren = ( children.GetCount() );

	}

	g_XSIInterface->AnalyzeSkeleton();

}

int SceneIFC::WriteVertexAnims(VActor *Thing, char* DestFileName, char* RangeString )
{

    //#define DOLOGVERT 0
	//if( DOLOGVERT )
	//{
	//	TSTR LogFileName = _T("\\ActXVert.LOG");
	//	AuxLog.Open(LogPath,LogFileName, 1 );//DEBUGFILE);
	//}

	XSI::Application app;
	XSI::Property playControl = app.GetActiveProject().GetProperties().GetItem(L"Play Control");

	Thing->FrameRate = playControl.GetParameterValue(L"rate");

	INT FirstFrame = (INT)FrameStart / FrameTicks; // Convert ticks to frames. FrameTicks set in
	INT LastFrame = (INT)FrameEnd / FrameTicks;

	ParseFrameRange( RangeString, FirstFrame, LastFrame );

	OurTotalFrames = FrameList.GetTotal();
	Thing->RawNumFrames = OurTotalFrames;
	Thing->RawAnimKeys.SetSize( OurTotalFrames * OurBoneTotal );
	Thing->RawNumBones  = OurBoneTotal;
	Thing->FrameTotalTicks = OurTotalFrames * FrameTicks;
	Thing->FrameRate = playControl.GetParameterValue(L"rate");

	INT FrameCount = 0;
	INT RefSkinPoints = 0;

	VertexMeshHeader MeshHeader;
	VertexAnimHeader AnimHeader;

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
	INT t;
	for( t=0; t< TempActor.SkinData.Faces.Num(); t++)
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
			NewTri.Tex[v].U = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].U );
			NewTri.Tex[v].V = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].V );
		}

		NewTri.TextureNum = TempActor.SkinData.Faces[t].MatIndex;

		// #TODO - Verify that this is the proper flag assignment for things like alpha, masked, doublesided, etc...
		if( NewTri.TextureNum < Thing->SkinData.RawMaterials.Num() )
			NewTri.Flags = Thing->SkinData.Materials[NewTri.TextureNum].PolyFlags;

		VAFaces.AddItem( NewTri );
	}

	// Write.. to 'model' file DestFileName_d.3d
	if( VAFaces.Num() && (OurScene.DoAppendVertex == 0) )
	{
		FastFileClass OutFile;
		char OutPath[MAX_PATH];
		sprintf(OutPath,"%s\\%s_d.3d",(char*)to_pathvtx,(char*)DestFileName);
		//PopupBox("OutPath for _d file: %s",OutPath);
		if ( OutFile.CreateNewFile(OutPath) != 0)
			ErrorBox( "File creation error. ");

		MeshHeader.NumPolygons = VAFaces.Num();
		MeshHeader.NumVertices = TempActor.SkinData.Points.Num();
		MeshHeader.Unused[0] = 117; // "serial" number for ActorX-generated vertex data.

		RefSkinPoints = MeshHeader.NumVertices;

		// Write header.
		OutFile.Write( &MeshHeader, sizeof(MeshHeader));

		// Write triangle data.
		for( INT f=0; f< VAFaces.Num(); f++)
		{
			OutFile.Write( &(VAFaces[f]), sizeof(VertexMeshTri) );
		}

		OutFile.CloseFlush();
						if( OutFile.GetError()) ErrorBox("Vertex skin save error.");

	}

    //
	// retrieve mesh for every frame & write to (to_skinfile_a.3d. Optionally append it to existing.
	//

	XSI::UIToolkit kit = app.GetUIToolkit();
	XSI::ProgressBar	l_pBar = kit.GetProgressBar();

	l_pBar.PutMaximum( 100 );
	l_pBar.PutMinimum( 1 );
	l_pBar.PutStep( 1 );
	l_pBar.PutValue( 1 );

	for( INT f=0; f<FrameList.GetTotal(); f++)
	{
		float TimeNow = FrameList.GetFrame(f) * FrameTicks;

		// Advance progress bar; obey bailout.
		INT Progress = (INT) 100.0f * ( t/(FrameList.GetTotal()+0.01f) );

		l_pBar.PutValue ( Progress );

		if ( l_pBar.IsCancelPressed() )
		{
			if ( MessageBox ( NULL, "Really cancel?", "Cancel Export", MB_YESNO|MB_ICONWARNING ) == IDYES )
			{
				break;
			} else {

				l_pBar.PutVisible( true );
			}

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
		char OutPath[MAX_PATH];
		sprintf(OutPath,"%s\\%s_a.3d",(char*)to_pathvtx,(char*)DestFileName);

		if( ! OurScene.DoAppendVertex )
		{
			if ( OutFile.CreateNewFile(OutPath) != 0) // Error!
			{
				ErrorBox( "File creation error. ");
				FileError = 1;
			}
		}
		else
		{
			if ( OutFile.OpenExistingFile(OutPath) != 0) // Error!
			{
				ErrorBox( "File appending error. ");
				FileError = 1;
			}
			// Need to explicitly move file ptr to end for appending.
			INT SeekLoc = OutFile.Seek(OutFile.GetSize());
		}

		if( ! FileError )
		{
			AnimHeader.FrameSize = RefSkinPoints * sizeof( DWORD );
			AnimHeader.NumFrames = FrameCount;

			// Write header if we're not appending.
			if( ! OurScene.DoAppendVertex)
				OutFile.Write( &AnimHeader, sizeof(AnimHeader));

			// Write animated vertex data (all frames, all verts).
			for( INT f=0; f< VAVerts.Num(); f++)
			{
				OutFile.Write( &(VAVerts[f]), sizeof(FMeshVert));
			}

			OutFile.CloseFlush();
				if( OutFile.GetError()) ErrorBox("Vertex anim save error.");
		}

	}

	//if( DOLOGVERT )
	//{
	//	AuxLog.Logf(" Digested %i vertex animation frames total.", FrameCount);
	//	AuxLog.Close();
	//}

	return ( RefSkinPoints > 0 ) ? FrameCount : 0 ;
}

//
// Unreal flags from materials (original material tricks by Steve Sinclair, Digital Extremes )
// - uses classic Unreal/UT flag conventions for now.
// Determine the polyflags, in case a multimaterial is present
// reference the matID value (pass in the face's matID).
//

void CalcMaterialFlags(XSI::Material mat, VMaterial& Stuff )
{
	//
	// TODO XSI: We will need to create a custom Unreal Realtime Shader.
	//

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

	// Check skin index
	// TODO XSI: INT skinIndex = FindValueTag( pName, _T("skin") ,2 ); // Double (or single) digit skin index.......
	INT skinIndex = 0;
	if ( skinIndex < 0 )
	{
		skinIndex = 0;
		Stuff.AuxFlags = 0;
	}
	else
		Stuff.AuxFlags = 1; // Indicates an explicitly defined skin index.

	Stuff.TextureIndex = skinIndex;

	// Lodbias
	// TODO XSI: INT LodBias = FindValueTag( pName, _T("lodbias"),2);
	INT LodBias = 5;
	// END TODO XSI
	if ( LodBias < 0 ) LodBias = 5; // Default value when tag not found.
	Stuff.LodBias = LodBias;

	// LodStyle
	// TODO XSI: INT LodStyle = FindValueTag( pName, _T("lodstyle"),2);
	INT LodStyle = 0;
	// END TODO XSI
	if ( LodStyle < 0 ) LodStyle = 0; // Default LOD style.
	Stuff.LodStyle = LodStyle;
/*
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
*/

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

UBOOL ExecuteBatchFile( char* batchfilename, SceneIFC* OurScene )
{

	return true;
}

//
// False = greyed out.
//
void ToggleControlEnable( HWND hWnd, int control, BOOL state )
{
	HWND hDlg = GetDlgItem(hWnd,control);
	EnableWindow(hDlg,state);
}

bool GetCheckBox ( HWND hWnd, int in_iControl )
{

	return ( SendDlgItemMessage(hWnd,in_iControl, BM_GETCHECK ,0,0) == BST_CHECKED ) ? true : false;

}

bool IsSelected ( XSI::X3DObject in_obj )
{
	XSI::Application app;
	XSI::CRefArray l_pSelection = app.GetSelection().GetArray();

	for (int s=0;s<l_pSelection.GetCount();s++)
	{
		XSI::CRef cref = in_obj;
		if ( l_pSelection[s] == in_obj )
		{
			return true;
		}
	}

	return false;
}

int DigestLocalMaterial(XSI::Material mat, TArray<void*> &RawMaterials)
{
	for (int m=0;m<g_XSIInterface->m_pCachedMaterials.GetUsed();m++)
	{
		if ( g_XSIInterface->m_pCachedMaterials[m]->m_ptr == mat )
		{
			return m;
		}
	}

	x3dpointer* ptr = new x3dpointer(mat);

	g_XSIInterface->m_pCachedMaterials.Extend(1);
	g_XSIInterface->m_pCachedMaterials[g_XSIInterface->m_pCachedMaterials.GetUsed()-1] = ptr;
	// The different material properties can be investigated at the end, for now we care about uniqueness only.
	RawMaterials.AddUniqueItem( (void*)ptr);

	return( RawMaterials.Where( (void*)ptr) );

}

//
// the following is to shut up the linker complaining about missing MAX specific exports
//
__declspec( dllexport ) const TCHAR* LibDescription()
{
	return NULL;
}
__declspec( dllexport ) int LibNumberClasses()
{
	return 0;
}
#define ClassDesc void
__declspec( dllexport ) ClassDesc* LibClassDesc(int i)
{
	return NULL;
}
__declspec( dllexport ) ULONG LibVersion()
{
	return 0;
}


void LogThis ( XSI::CString in_cstr )
{
	XSI::Application app;
	app.LogMessage ( in_cstr, XSI::siWarningMsg );
}

void W2AHelper( LPSTR out_sz, LPCWSTR in_wcs, int in_cch )
{
	if ( out_sz != NULL && 0 != in_cch )
	{
		out_sz[0] = '\0' ;

		if ( in_wcs != NULL )
		{
			size_t l_iLen = 0;
			l_iLen = ::wcstombs( out_sz, in_wcs, ( in_cch < 0 ) ? ::wcslen(in_wcs) + 1 : (size_t) in_cch ) ;

			if ( in_cch > 0 ) { out_sz[ in_cch - 1 ] = '\0'; }
		}
	}
}

XSI::MATH::CVector3 GetTriangleNormal ( XSI::Triangle in_tri )
{
	XSI::MATH::CVector3 normal;

	for (int v=0;v<3;v++)
	{
		normal += in_tri.GetPolygonNodeNormalArray()[v];
	}

	normal *= (1.0 / 3.0f);

	return normal;
}

void A2WHelper( wchar_t* out_wcs, const char* in_sz, int in_cch )
{
	if ( out_wcs != NULL && 0 != in_cch )
	{
		out_wcs[0] = L'\0' ;

		if ( in_sz != NULL )
		{
			size_t l_iLen = 0;
			l_iLen = ::mbstowcs( out_wcs, in_sz, ( in_cch < 0 ) ? ::strlen(in_sz) + 1 : (size_t) in_cch ) ;
			assert( (int)l_iLen != -1 );

        		if ( in_cch > 0 ) { out_wcs[ in_cch - 1 ] = L'\0'; }
		}
	}
}
