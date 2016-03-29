// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#include "ActorX.h"
#include "BrushExport.h"
#include "XSIInterface.h"
#include "SceneIfc.h"
#include ".\res\resource.h"

#include <xsi_polygonface.h>
#include <xsi_facet.h>
#include <xsi_longarray.h>
#include <xsi_vertex.h>
#include <xsi_cluster.h>
#include <xsi_polygonnode.h>
#include <xsi_comapihandler.h>
#include <xsi_color.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

extern WinRegistry PluginReg;
extern int  GetFolder ( char *out_szPath );
extern void W2AHelper( LPSTR out_sz, LPCWSTR in_wcs, int in_cch = -1);
extern SceneIFC OurScene;

void GetXSISaveName ( char* filename, char* workpath, char *filter, char *defaultextension );
bool GetNodeUV ( XSI::X3DObject in_obj, long in_NodeIndex, int UVSet, double &u, double &v );
bool GetNodeColor( XSI::X3DObject in_obj, long in_NodeIndex, XSI::CColor& color );

#define INVALID_ID              (-1)
#define NO_SMOOTHING_GROUP      (-1)
#define QUEUED_FOR_SMOOTHING (-2)

CSIBCArray<XSI::Material> g_GlobaMatList;
//
// Mesh-specific selection check.
//
UBOOL isMeshSelected( XSI::X3DObject in_obj )
{
	XSI::Application app;
	XSI::CRefArray l_pSelection = app.GetSelection().GetArray();


	for (int s=0;s<l_pSelection.GetCount();s++)
	{
		if ( l_pSelection[s] == in_obj )
		{
			return true;
		}
	}

	return false;
}




//
// Static Mesh export (universal) dialog procedure - can write brushes to separarte destination folder.
//

void ExportStaticMeshLocal()
{
	OurScene.ExportingStaticMesh = true;
	// First digest the scene into separately, named brush datastructures.
	OurScene.DigestStaticMeshes();

	// If anything to save:
	// see if we had DoGeomAsFilename -> use the main non-collision geometry name as filename
	// otherwise: prompt for name.

	//
	// Check whether we want to write directly or present a save-as menu..
	//
	char newname[MAX_PATH];
	_tcscpy(newname,("None"));

	// Anything to write ?
	if( OurScene.StaticPrimitives.Num() )
	{
		char filterlist[] = "ASE Files (*.ase)\0*.ase\0"\
			"ASE Files (*.ase)\0*.ase\0";

		char defaultextension[] = ".ase";

		if( ! OurScene.DoGeomAsFilename )
		{
			GetXSISaveName( newname, to_meshoutpath, filterlist, defaultextension );
		}
		else
		{
			// Use first/only name in scene - prefer the selected or biggest (selected) primitive.
			INT NameSakeIdx=0;
			INT MostFaces = 0;
			INT UniquelySelected = 0;
			INT TotalSelected = 0;
			for( int s=0; s<OurScene.StaticPrimitives.Num();s++)
			{
				INT NewFaceCount = OurScene.StaticPrimitives[s].Faces.Num();
				if( OurScene.StaticPrimitives[s].Selected  )
				{
					UniquelySelected = s;
					TotalSelected++;
				}
				if(  MostFaces < NewFaceCount )
				{

					NameSakeIdx = s;
					MostFaces = NewFaceCount;
				}
			}
			if( TotalSelected == 1)
				NameSakeIdx = UniquelySelected;

			if( strlen( OurScene.StaticPrimitives[NameSakeIdx].Name.StringPtr() ) > 0 )
			{
				sprintf( newname, "%s\\%s%s", to_meshoutpath, OurScene.StaticPrimitives[NameSakeIdx].Name.StringPtr(),defaultextension );
			}
			else
			{
				PopupBox("Staticmesh export: error deriving filename from scene primitive.");
			}
		}

		if( newname[0] != 0 )
			OurScene.SaveStaticMeshes( newname );
	}
	else
	{
		//if( !OurScene.DoSuppressPopups )
		{
			PopupBox("Staticmesh export: no suitable primitives found in the scene.");
		}
	}

	OurScene.Cleanup();

	OurScene.ExportingStaticMesh = false;
}

BOOL CALLBACK StaticMeshDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch (msg)
	{
		case WM_INITDIALOG:
		{

			_SetCheckBox( hWnd, IDC_CHECKSMOOTH,     OurScene.DoConvertSmooth );
			_SetCheckBox( hWnd, IDC_CHECKGEOMNAME,   OurScene.DoGeomAsFilename );
			_SetCheckBox( hWnd, IDC_CHECKSELECTEDSTATIC, OurScene.DoSelectedStatic );
			_SetCheckBox( hWnd, IDC_CHECKSUPPRESS,   OurScene.DoSuppressPopups );
			_SetCheckBox( hWnd, IDC_CHECKUNDERSCORE, OurScene.DoUnderscoreSpace );
			_SetCheckBox( hWnd, IDC_CHECKCONSOLIDATE, OurScene.DoConsolidateGeometry);

			if ( to_meshoutpath[0] )
			{
				PrintWindowString(hWnd,IDC_EDITOUTPATH,to_meshoutpath);
				_tcscpy(LogPath,to_meshoutpath);
			}
		}
		break;

		case WM_COMMAND:
		switch (LOWORD(wParam))
		{
			// Windows closure.
			case IDCANCEL:
			{
				EndDialog(hWnd, 0);
			}
			break;

			//  Browse for a destination path.
			case IDC_BROWSEOUT:
			{
				char  dir[MAX_PATH];
				if( GetFolder(dir) )
				{
					_tcscpy(to_meshoutpath,dir);
					_tcscpy(LogPath,dir); //
					PluginReg.SetKeyString("TOMESHOUTPATH", to_meshoutpath );
				}
				SetWindowText(GetDlgItem(hWnd,IDC_EDITOUTPATH ),to_meshoutpath);
			}
			break;

			case IDC_EDITOUTPATH:
			{
				switch( HIWORD(wParam) )
				{
					case EN_KILLFOCUS:
					{
						char  dir[MAX_PATH];
						GetWindowText(GetDlgItem(hWnd,IDC_EDITOUTPATH),dir,MAX_PATH);
						_tcscpy(to_meshoutpath,dir);
						_tcscpy(LogPath,dir); //
						PluginReg.SetKeyString("TOMESHOUTPATH", to_meshoutpath );
					}
					break;
				};
			}
			break;

			// Handle all checkboxes.
			case IDC_CHECKSMOOTH:
			{
				OurScene.DoConvertSmooth = _GetCheckBox(hWnd,IDC_CHECKSMOOTH);
				PluginReg.SetKeyValue("DOCONVERTSMOOTH",OurScene.DoConvertSmooth);
			}
			break;
			case IDC_CHECKUNDERSCORE:
			{
				OurScene.DoUnderscoreSpace = _GetCheckBox(hWnd,IDC_CHECKUNDERSCORE);
				PluginReg.SetKeyValue("DOUNDERSCORESPACE",OurScene.DoUnderscoreSpace);
			}
			break;
			case IDC_CHECKGEOMNAME:
			{
				OurScene.DoGeomAsFilename = _GetCheckBox(hWnd,IDC_CHECKGEOMNAME);
				PluginReg.SetKeyValue("DOGEOMASFILENAME",OurScene.DoGeomAsFilename);
			}
			break;
			case IDC_CHECKSELECTEDSTATIC:
			{
				OurScene.DoSelectedStatic = _GetCheckBox(hWnd,IDC_CHECKSELECTEDSTATIC);
				PluginReg.SetKeyValue("DOSELECTEDSTATIC",OurScene.DoSelectedStatic);
			}
			break;
			case IDC_CHECKSUPPRESS:
			{
				OurScene.DoSuppressPopups = _GetCheckBox(hWnd,IDC_CHECKSUPPRESS);
				PluginReg.SetKeyValue("DOSUPPRESSPOPUPS",OurScene.DoSuppressPopups);
			}
			break;
			case IDC_CHECKCONSOLIDATE:
			{
				OurScene.DoConsolidateGeometry = _GetCheckBox(hWnd,IDC_CHECKCONSOLIDATE);
				PluginReg.SetKeyValue("DOCONSOLIDATE",OurScene.DoConsolidateGeometry);
			}
			break;


			// EXPORT the mesh.
			case ID_EXPORTMESH:
			{
				ExportStaticMeshLocal();
			}

		}
		break;

		// Lose focus = exit window ??
		case WM_KILLFOCUS:
		{
			EndDialog(hWnd, 1);
		}
		break;


		// In case of no message to the window...
		default:
			return FALSE;
	}
	return TRUE;


}


//
// Staticmesh digest.
//
int	SceneIFC::ProcessStaticMesh( int TreeIndex )
{

	XSI::CRef cref = ((x3dpointer*)SerialTree[TreeIndex].node)->m_ptr;

	SerialTree[TreeIndex].IsSelected = isMeshSelected ( cref );

	if( OurScene.DoSelectedStatic && !SerialTree[TreeIndex].IsSelected )
	{
		return 0; // Ignore unselected.
	}

	//
	// get some usefull stuff from the polymesh
	//

	XSI::X3DObject x3d ( cref );
	XSI::Geometry geom(x3d.GetActivePrimitive().GetGeometry());
	XSI::PolygonMesh polygonmesh(geom);
	XSI::CPointRefArray points = polygonmesh.GetPoints();
	XSI::CTriangleRefArray triarray = polygonmesh.GetTriangles();
	XSI::CPolygonFaceRefArray polyarray = polygonmesh.GetPolygons();

	XSI::CRefArray samplePoints;
	geom.GetClusters().Filter(XSI::siSampledPointCluster, XSI::CStringArray(), L"",
		samplePoints );

	XSI::Cluster cluster(samplePoints.GetItem(0));
	XSI::CRefArray properties = cluster.GetProperties();
	XSI::ClusterProperty clusterProp = properties[0];
	XSI::ClusterProperty prop ( properties[0] );
	XSI::CClusterPropertyElementArray uvwElementArray(prop.GetElements());

	XSI::CDoubleArray uvwArray1(uvwElementArray.GetArray());

	//XSI::Cluster cluster2(samplePoints.GetItem(1));
	//XSI::CRefArray properties2 = cluster2.GetProperties();
	XSI::ClusterProperty clusterProp2 = properties[1];
	XSI::ClusterProperty prop2 ( properties[1] );
	XSI::CClusterPropertyElementArray uvwElementArray2(prop2.GetElements());

	XSI::CDoubleArray uvwArray2(uvwElementArray2.GetArray());
	XSI::ClusterProperty clusterPropc = polygonmesh.GetCurrentVertexColor();
	XSI::CClusterPropertyElementArray colorElementArray(clusterPropc.GetElements());
	XSI::CDoubleArray colorArray(colorElementArray.GetArray());



	UBOOL MeshHasMapping = true;
	UBOOL bCollision = false;
	UBOOL bSmoothingGroups = false;

	INT NumVerts = points.GetCount();
	INT NumFaces = polyarray.GetCount();


	MeshHasMapping = x3d.GetMaterial().GetOGLTexture().IsValid();


	//
	//
	// Create cluster map to materials.
	//

	g_GlobaMatList.DisposeData();
	g_GlobaMatList.Extend(1);
	g_GlobaMatList[0] = x3d.GetMaterial();

	int DefaultMaterial = 0;
	CSIBCArray<long>	TriangleMaterialMap;
	TriangleMaterialMap.Extend( polyarray.GetCount() );

	for (long v=0;v<polyarray.GetCount();v++)
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

		g_GlobaMatList.Extend(1);
		g_GlobaMatList[g_GlobaMatList.GetUsed()-1] = l_pMat;
		int ClusterMaterialIndex = g_GlobaMatList.GetUsed() - 1;

		//
		// update the triangle map to point all tris using this material
		// to the correct index
		//

		XSI::CClusterElementArray clusterElementArray = Thecluster.GetElements();
		XSI::CLongArray values(clusterElementArray.GetArray());
		long countPolyIndices = values.GetCount();

		for (long v=0;v<countPolyIndices;v++)
		{
			TriangleMaterialMap[values[v]] = ClusterMaterialIndex;
		}
	}

	// Get name.
	// TODO XSI: DLog.Logf(" MESH NAME: [%s] Selected:[%i] mapping: %i \n" , DagNode.name().asChar(), SerialTree[TreeIndex].IsSelected, (INT)MeshHasMapping );

	// Recognize any collision primitive naming conventions ?
	// into separate MCD SP BX CY CX_name
	// MCDCX is the default for untextured geometry.
	CHAR PrimitiveName[MAX_PATH];

	char *strsz = new char [ x3d.GetName().Length() + 1 ];
	W2AHelper ( strsz, x3d.GetName().GetWideString() );

	strcpysafe( PrimitiveName, strsz, MAX_PATH );

	delete [] strsz;

	if( CheckSubString( PrimitiveName,_T("MCD")) )
	{
		bCollision = true; // collision-only architecture.
	}


	// New primitive.
	INT PrimIndex = StaticPrimitives.AddExactZeroed(1);

	StaticPrimitives[PrimIndex].Name.CopyFrom( PrimitiveName );
	StaticPrimitives[PrimIndex].Selected = SerialTree[TreeIndex].IsSelected;

	// Any smoothing group conversion requested ?
	MeshProcessor TempTranslator;

	if( OurScene.DoConvertSmooth && MeshHasMapping && ( !bCollision ) )
	{
		TempTranslator.createNewSmoothingGroups( x3d );

		// Copy smoothing groups over to the primitive's own.
		if( TempTranslator.FaceSmoothingGroups.Num()  )
		{
			bSmoothingGroups = true;
			StaticPrimitives[PrimIndex].FaceSmoothingGroups.Empty();
			StaticPrimitives[PrimIndex].FaceSmoothingGroups.AddExactZeroed(  TempTranslator.FaceSmoothingGroups.Num() );
			for( INT f=0; f< TempTranslator.FaceSmoothingGroups.Num(); f++)
			{
				for( INT s=0; s< TempTranslator.FaceSmoothingGroups[f].Groups.Num(); s++)
				{
					StaticPrimitives[PrimIndex].FaceSmoothingGroups[f].Groups.AddItem(  TempTranslator.FaceSmoothingGroups[f].Groups[s] );
				}
			}
		}
	}

	// If the mesh had no materials/mapping, if desired it will become 'convex' collision architecture automatically ?
	if( 0 ) // OurScene.DoUntexturedAsCollision && !MeshHasMapping && !bCollision )
	{
		sprintf( PrimitiveName, "MCDCX_%s",PrimitiveName );
		bCollision = true;
	}

	//
	// Now stash the entire thing in our StaticPrimitives, regardless of mapping/smoothing groups......
	// Accumulate shaders in order of occurrence in the triangles.
	//

	XSI::MATH::CTransformation xfo = x3d.GetKinematics().GetGlobal().GetTransform();
	for( int p=0; p< NumVerts; p++)
	{
		XSI::MATH::CVector3 thev ( ((XSI::Point)points[p]).GetPosition() );
		thev *= xfo;

		INT NewVertIdx = StaticPrimitives[PrimIndex].Vertices.AddZeroed(1);
		StaticPrimitives[PrimIndex].Vertices[NewVertIdx].X = - thev.GetX(); //#SKEL - needed for proper SHAPE...
		StaticPrimitives[PrimIndex].Vertices[NewVertIdx].Y =   thev.GetZ(); //
		StaticPrimitives[PrimIndex].Vertices[NewVertIdx].Z =   thev.GetY();	//
	}

	int uvSetCount = 1;
	if ( prop2.IsValid() )
	{
		uvSetCount = 2;
	}
/*
	// Vertex-color detection.
	MColorArray fColorArray;
	 if (MStatus::kFailure == MeshFunction.getFaceVertexColors(fColorArray))
	{
		//MGlobal::displayError("MFnMesh::getFaceVertexColors");
	}
	INT vertColorCount =fColorArray.length();
	INT realColorCount = 0;
	// Any vertex for which color is not defined will have -1 in all its components.
	for( INT i=0; i<vertColorCount;i++)
	{
		if( ! (
			(fColorArray[i].r == -1.f ) ||
			(fColorArray[i].g == -1.f ) ||
			(fColorArray[i].b == -1.f )
			) )
			realColorCount++;
	}
	// PopupBox(" Vertex colors - bulk  %i  - real: %i  for (sub) mesh %s ",vertColorCount,realColorCount,MeshFunction.name().asChar());

	*/

	TArray< GroupList > NewSmoothingGroups;
	UBOOL bHasSmoothingGroups = (StaticPrimitives[PrimIndex].FaceSmoothingGroups.Num() > 0);

	// Faces & Wedges & Materials & Smoothing groups, all in the same run......
	for (int PolyIndex = 0; PolyIndex < NumFaces; PolyIndex++)
	{
		// Get the vertex indices for this polygon.
		XSI::PolygonFace polyface = polyarray[PolyIndex];
		XSI::CVertexRefArray FaceVertices = polyface.GetVertices();
		INT VertCount = FaceVertices.GetCount();


		// Assumed material the same for all facets of a poly.
		// Material on this face - encountered before ? -> DigestMayaMaterial..
		INT MaterialIndex = 0;// TODO XSI: REmopve hardcoded mat ID
		INT ThisMaterial = TriangleMaterialMap[PolyIndex];


/*
		// Only count material on valid polygons.

		if( VertCount >= 3)
		{
			if( (INT)MayaShaders.length() <= MaterialIndex )
			{
				ThisMaterial = 0;
			}
			else
			{
				INT OldShaderNum = AxShaders.length();
				ThisMaterial = DigestMayaMaterial( MayaShaders[MaterialIndex] );

				// New material ?
				if( (INT)AxShaders.length() > OldShaderNum )
					NewMaterials++;
			}
			//DLog.LogfLn(" Material for poly %i is %i total %i ",PolyIndex,ThisMaterial,AxShaders.length());
		}
*/


		// Handle facets of single polygon.
		while( VertCount >= 3 )
		{
			// A brand new face.
			INT NewFaceIdx = StaticPrimitives[PrimIndex].Faces.AddZeroed(1);
			StaticPrimitives[PrimIndex].Faces[NewFaceIdx].MaterialIndices.AddExactZeroed(1);
			StaticPrimitives[PrimIndex].Faces[NewFaceIdx].MaterialIndices[0] = ThisMaterial;

		if( bHasSmoothingGroups )
				{
				//  Necessary for redistribution of smoothing groups for faces with more than 3 vertices being auto-triangulated.
				NewSmoothingGroups.AddZeroed(1);
				for( INT s=0; s< StaticPrimitives[PrimIndex].FaceSmoothingGroups[PolyIndex].Groups.Num(); s++)
				{
					NewSmoothingGroups[NewFaceIdx].Groups.AddItem( StaticPrimitives[PrimIndex].FaceSmoothingGroups[PolyIndex].Groups[s] );
				}
			}

			 // DLog.LogfLn(" Material on facet %i Face %i Primitive %i is %i NumMaterialsForface %i DATA %i ",VertCount,NewFaceIdx,PrimIndex,ThisMaterial,StaticPrimitives[PrimIndex].Faces[NewFaceIdx].MaterialIndices.Num(), (INT)((BYTE*)&StaticPrimitives[PrimIndex].Faces[NewFaceIdx].MaterialIndices[0]) );

			// Fill vertex indices (breaks up face in triangle polygons.
			INT VertIdx[3];
		VertIdx[0] = 0;
		VertIdx[1] = VertCount-2;
		VertIdx[2] = VertCount-1;

			for( int i=0; i<3; i++)
			{
				//Retrieve wedges for first UV set.

				//XSI::Vertex vertx = ((XSI::Vertex)( polyface.GetVertices()[VertIdx[i]])).GetUVs();
				//stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[i],U,V,&firstUVSetName);

				XSI::PolygonNode pnode = polyface.GetNodes()[VertIdx[i]];

				double u,v;

				long l_clusterIndex;
				cluster.FindIndex(pnode.GetIndex(), l_clusterIndex);
				u = uvwArray1 [ l_clusterIndex * 3 ];
				v = uvwArray1 [ (l_clusterIndex * 3) + 1 ];


				//DLog.Logf(" UV logging: Face: %6i Index %6i (%6i) U %6f  V %6f \n",NewFaceIdx,i,VertIdx[i],U,V);

				GWedge NewWedge;
				//INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddZeroed(1);

				NewWedge.MaterialIndex = ThisMaterial; // Per-corner material index..
				NewWedge.U = u;
				NewWedge.V = v;
				NewWedge.PointIndex = XSI::Vertex(FaceVertices[VertIdx[i]]).GetIndex(); // Maya's face vertices indices for this face.

				// Should we merge identical wedges here instead of counting on editor ?  With the way ASE is imported it may make no difference.
				INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddItem( NewWedge);
				// New wedge on every corner of a face.
				StaticPrimitives[PrimIndex].Faces[NewFaceIdx].WedgeIndex[i] = NewWedgeIdx;

				// Any second UV set data ?
				if( uvSetCount > 1)
				{
					double u,v;

					XSI::PolygonNode pnode = polyface.GetNodes()[VertIdx[i]];

					long l_clusterIndex;
					cluster.FindIndex(pnode.GetIndex(), l_clusterIndex);
					u = uvwArray2 [ l_clusterIndex * 3 ];
					v = uvwArray2 [ (l_clusterIndex * 3) + 1 ];

					GWedge NewWedge2;
					NewWedge2.MaterialIndex = ThisMaterial;
					NewWedge2.U = u;
					NewWedge2.V = v;
					NewWedge2.PointIndex = FaceVertices[VertIdx[i]];

					INT NewWedge2Idx = StaticPrimitives[PrimIndex].Wedges2.AddItem( NewWedge2);
					StaticPrimitives[PrimIndex].Faces[NewFaceIdx].Wedge2Index[i] = NewWedge2Idx;
				}
				// Store per-vertex color (for this new wedge) only if any were actually defined.

				XSI::CColor color;
				color.r = color.g = color.b = 0.0;
				color.a = 1.0;

				if ( clusterPropc.IsValid() )
				{
					long in_NodeIndex = pnode.GetIndex();
					color.r = colorArray[ in_NodeIndex * 4 ];
					color.g = colorArray[ (in_NodeIndex * 4) + 1 ];
					color.b = colorArray[ (in_NodeIndex * 4) + 2 ];
					color.a = colorArray[ (in_NodeIndex * 4) + 3 ];

					GColor NewVertColor;
					NewVertColor.A = color.a;
					NewVertColor.R = color.r;
					NewVertColor.G = color.g;
					NewVertColor.B = color.b;
					StaticPrimitives[PrimIndex].VertColors.AddItem(NewVertColor);

				}

			}

			VertCount--;
		}

	}// For each polygon (possibly multiple triangles per poly.)


	if( bHasSmoothingGroups )
	{
		// Copy back the newly distributed smoothing groups.
		StaticPrimitives[PrimIndex].FaceSmoothingGroups.Empty();
		StaticPrimitives[PrimIndex].FaceSmoothingGroups.AddExactZeroed(NewSmoothingGroups.Num());
		for( INT f=0; f< NewSmoothingGroups.Num(); f++)
		{
			for( INT s=0; s<NewSmoothingGroups[f].Groups.Num(); s++)
			{
				StaticPrimitives[PrimIndex].FaceSmoothingGroups[f].Groups.AddItem( NewSmoothingGroups[f].Groups[s] );
			}
		}
	}

	return 1;
}



//
// Digest static meshes, filling the StaticMesh array (some may be collision geometry, or simply destined to be ignored.)
//
int SceneIFC::DigestStaticMeshes()
{
	//
	// Digest primitives list
	// Go over all nodes in scene, retain the one that have possible geometry.
	//
	SurveyScene();
	GetSceneInfo();

	if( DEBUGFILE )
	{
		char LogFileName[] = ("\\PrepareStaticMeshInfo.LOG");
		DLog.Open(LogPath,LogFileName,1);
		DLog.Logf("STATICMESH EXTRACTION DEBUGGING\n\n" );
	}

	if( DEBUGMEM )
	{
		char LogFileName[] = ("\\MemDebugging.LOG");
		MemLog.Open(LogPath,LogFileName, 1 );
		//MemLog = DLog; //#SKEL!!!!!!!
	}


	//
	// Go over the nodes list and digest each of them into the StaticPrimitives GeometryPrimitive array
	//
	INT NumMeshes = 0;
	for( INT i=0; i<SerialTree.Num(); i++)
	{
		XSI::CRef cref = ((x3dpointer*)SerialTree[i].node)->m_ptr;
		INT IsRoot = ( TempActor.MatchNodeToSkeletonIndex( (void*) i ) == 0 );

		// If mesh, determine
		INT MeshFaces = 0;
		INT MeshVerts = 0;
		if( SerialTree[i].IsSkin )
		{
			XSI::X3DObject x3d ( cref );
			XSI::Geometry geom(x3d.GetActivePrimitive().GetGeometry());
			XSI::PolygonMesh polygonmesh(geom);
			XSI::CPointRefArray points = polygonmesh.GetPoints();
			XSI::CTriangleRefArray triarray = polygonmesh.GetTriangles();

			MeshVerts = points.GetCount();
			MeshFaces = triarray.GetCount();
		}

		if( MeshFaces )
		{
			NumMeshes++;
			//DLog.Logf("Mesh stats:  Faces %4i  Verts %4i \n\n", MeshFaces, MeshVerts );
			if ( SerialTree[i].IsSkin )
			{
				// Process this primitive.
				ProcessStaticMesh(i);
			}
		}
	} //Serialtree

	DLog.Close();

	if( DEBUGMEM )
	{
		MemLog.Close();
	}

	// Digest materials. See also 'fixmaterials'.
	//
	for( INT m=0; m < (INT)g_GlobaMatList.GetUsed(); m++ )
	{
		XSI::Material mat ( g_GlobaMatList[m]);

		StaticMeshMaterials.AddExactZeroed(1);

		char *strsz = new char [ mat.GetName().Length() + 1 ];
		W2AHelper ( strsz, mat.GetName().GetWideString() );
		StaticMeshMaterials[m].Name.CopyFrom( strsz );
		delete [] strsz;


		XSI::OGLTexture ogl = mat.GetOGLTexture();

		if ( !ogl.IsValid() )
		{
			StaticMeshMaterials[m].BitmapName.CopyFrom( "None" );
		} else {

			char *strsz = new char [ ogl.GetFullName().Length() + 1 ];
			W2AHelper ( strsz, ogl.GetFullName().GetWideString() );
			StaticMeshMaterials[m].BitmapName.CopyFrom( strsz );
			delete [] strsz;


		}

	}

	return NumMeshes;

}



//
// Consolidate primitves into single structure to comply with UE2/UE3's static mesh import code.
//

INT SceneIFC::ConsolidateStaticPrimitives( GeometryPrimitive* ResultPrimitive )
{

	INT PointIndexBase = 0;
	INT WedgeIndexBase = 0;

	if( StaticPrimitives.Num() ==1 )
	{
		ResultPrimitive->Name =  StaticPrimitives[0].Name;
	}
	else
	{
		ResultPrimitive->Name.CopyFrom("ConsolidatedObject");
	}
	ResultPrimitive->Selected = 1;

	UBOOL DoMultiUV = false;
	UBOOL DoColoredVerts = false;
	UBOOL HasSmoothing = false;


	// Detect if any additional UV channels have been digested; if so, the second UV channel needs to be defined for the entire mesh.
	{for(INT PrimIdx=0; PrimIdx<StaticPrimitives.Num(); PrimIdx++)
	{
		if( StaticPrimitives[PrimIdx].Wedges2.Num() > 0)
			DoMultiUV = true;
	}}
	// Detect if any additional vertex colors.
	{for(INT PrimIdx=0; PrimIdx<StaticPrimitives.Num(); PrimIdx++)
	{
		if( StaticPrimitives[PrimIdx].VertColors.Num() > 0)
			DoColoredVerts = true;
	}}

	// Detect if any smoothing data.
	{for(INT PrimIdx=0; PrimIdx<StaticPrimitives.Num(); PrimIdx++)
	{
		if( StaticPrimitives[PrimIdx].FaceSmoothingGroups.Num() > 0)
			HasSmoothing = true;
	}}


	for(INT PrimIdx=0; PrimIdx<StaticPrimitives.Num(); PrimIdx++)
	{

		// Main: add verts, faces, tverts
		for( INT VertIdx=0; VertIdx< StaticPrimitives[PrimIdx].Vertices.Num(); VertIdx++)
		{
			ResultPrimitive->Vertices.AddItem( StaticPrimitives[PrimIdx].Vertices[VertIdx] );
		}

		for( INT WedgeIdx=0; WedgeIdx< StaticPrimitives[PrimIdx].Wedges.Num(); WedgeIdx++)
		{
			GWedge NewWedge = StaticPrimitives[PrimIdx].Wedges[WedgeIdx];
			NewWedge.PointIndex += PointIndexBase;
			ResultPrimitive->Wedges.AddItem( NewWedge );
		}

		for( INT FaceIdx=0; FaceIdx< StaticPrimitives[PrimIdx].Faces.Num(); FaceIdx++)
		{
			INT NewFaceIndex = ResultPrimitive->Faces.AddZeroed( 1 );

			ResultPrimitive->Faces[NewFaceIndex] = StaticPrimitives[PrimIdx].Faces[FaceIdx];

			ResultPrimitive->Faces[NewFaceIndex].WedgeIndex[0] += WedgeIndexBase;
			ResultPrimitive->Faces[NewFaceIndex].WedgeIndex[1] += WedgeIndexBase;
			ResultPrimitive->Faces[NewFaceIndex].WedgeIndex[2] += WedgeIndexBase;

			ResultPrimitive->Faces[NewFaceIndex].Wedge2Index[0] += WedgeIndexBase;
			ResultPrimitive->Faces[NewFaceIndex].Wedge2Index[1] += WedgeIndexBase;
			ResultPrimitive->Faces[NewFaceIndex].Wedge2Index[2] += WedgeIndexBase;
		}


		 // Ensure as many face smoothing groups as faces exist.
		if( HasSmoothing)
		{
			if( StaticPrimitives[PrimIdx].FaceSmoothingGroups.Num() )
			{				INT NewSmoothIndex = ResultPrimitive->FaceSmoothingGroups.Num();

				ResultPrimitive->FaceSmoothingGroups.AddExactZeroed( StaticPrimitives[PrimIdx].FaceSmoothingGroups.Num() );

				for( INT s=0; s<StaticPrimitives[PrimIdx].FaceSmoothingGroups.Num(); s++)
				{
					for( INT g = 0; g<  StaticPrimitives[PrimIdx].FaceSmoothingGroups[s].Groups.Num(); g++)
					{
						ResultPrimitive->FaceSmoothingGroups[NewSmoothIndex+s].Groups.AddItem( StaticPrimitives[PrimIdx].FaceSmoothingGroups[s].Groups[g] );
					}
				}
			}
			else
			{
				ResultPrimitive->FaceSmoothingGroups.AddExactZeroed( StaticPrimitives[PrimIdx].Faces.Num());
			}
		}


		if( DoMultiUV )
		{
			if( StaticPrimitives[PrimIdx].Wedges2.Num() )
			{
				for( INT WedgeIdx=0; WedgeIdx< StaticPrimitives[PrimIdx].Wedges2.Num(); WedgeIdx++)
				{
					GWedge NewWedge =StaticPrimitives[PrimIdx].Wedges2[WedgeIdx];
					NewWedge.PointIndex += PointIndexBase;
					ResultPrimitive->Wedges2.AddItem( NewWedge );
				}
			}
			else
			{
				// Add 'empty' wedges2 indentical to Wedges content.
				for( INT WedgeIdx=0; WedgeIdx< StaticPrimitives[PrimIdx].Wedges.Num(); WedgeIdx++)
				{
					GWedge EmptyWedge =StaticPrimitives[PrimIdx].Wedges[WedgeIdx];
					EmptyWedge.PointIndex += PointIndexBase;
					ResultPrimitive->Wedges2.AddItem( EmptyWedge );
				}
			}
		}

		if( DoColoredVerts )
		{
			if( StaticPrimitives[PrimIdx].VertColors.Num() )
			{
				for( INT VertIdx=0; VertIdx< StaticPrimitives[PrimIdx].VertColors.Num(); VertIdx++)
				{
					ResultPrimitive->VertColors.AddItem( StaticPrimitives[PrimIdx].VertColors[VertIdx] );
				}
			}
			else
			{
				// Add (wedge) number of empty vertcolors.
				for( INT WedgeIdx=0; WedgeIdx< StaticPrimitives[PrimIdx].Wedges.Num(); WedgeIdx++)
				{
					GColor BlackVertColor( 0,0,0,0);
					ResultPrimitive->VertColors.AddItem( BlackVertColor );
				}
			}
		}

		// Adjust bases for vertex and wedge indices in subsequent consolidated primitives.
		PointIndexBase = ResultPrimitive->Vertices.Num();
		WedgeIndexBase = ResultPrimitive->Wedges.Num();

	}


	return 1;
}




//
//	Write out a single ASE-GeomObject
//    01-13-05 EdN  - Added explicit tabs and some spaces-to-tabs replacements to mirror strict original 3DSMax ase output conventions (however arbitrary those may be...)
//
INT WriteStaticPrimitive( GeometryPrimitive& StaticPrimitive, TextFile& OutFile )
{
	// GeomObject wrapper.
	OutFile.LogfLn("*GEOMOBJECT {");
	OutFile.LogfLn("\t*NODE_NAME \"%s\"",StaticPrimitive.Name.StringPtr() );
	OutFile.LogfLn("\t*NODE_TM {");
	OutFile.LogfLn("\t*NODE_NAME \"%s\"",StaticPrimitive.Name.StringPtr() );
	OutFile.LogfLn("\t}");

	// Mesh block.
	OutFile.LogfLn("\t*MESH {");
	OutFile.LogfLn("\t\t*TIMEVALUE 0");
	OutFile.LogfLn("\t\t*MESH_NUMVERTEX %i",StaticPrimitive.Vertices.Num());
	OutFile.LogfLn("\t\t\t*MESH_NUMFACES %i",StaticPrimitive.Faces.Num());

	// Vertices.
	OutFile.LogfLn("\t\t*MESH_VERTEX_LIST {");
	for( INT v=0; v< StaticPrimitive.Vertices.Num(); v++ )
	{
		OutFile.LogfLn("\t\t\t*MESH_VERTEX %4i\t%f\t%f\t%f",
			v,
			StaticPrimitive.Vertices[v].X,
			StaticPrimitive.Vertices[v].Y,
			StaticPrimitive.Vertices[v].Z
			);
	}
	OutFile.LogfLn("\t\t}");

	INT UniqueGroup = 15;

	// Faces.
	OutFile.LogfLn("\t\t*MESH_FACE_LIST {");
	for( INT f=0; f<StaticPrimitive.Faces.Num();f++ )
	{
		// Face's vertex indices.
		OutFile.Logf("\t\t\t*MESH_FACE %4i: A: %4i B: %4i C: %4i",
			f,
			StaticPrimitive.Wedges[ StaticPrimitive.Faces[f].WedgeIndex[0] ].PointIndex,
			StaticPrimitive.Wedges[ StaticPrimitive.Faces[f].WedgeIndex[1] ].PointIndex,
			StaticPrimitive.Wedges[ StaticPrimitive.Faces[f].WedgeIndex[2] ].PointIndex
			);

		OutFile.Logf("\t *MESH_SMOOTHING ");  // Preceded by: TAB+SPACE. trailed by 1 space.

		if( StaticPrimitive.FaceSmoothingGroups.Num() <= f ) // Undefined or out of range somehow.
		{
			OutFile.Logf("0"); // Number + 1 space.
		}

		// Print max-style multiple smoothing groups. Separated by spaces...
		if( StaticPrimitive.FaceSmoothingGroups.Num() > f )
		{
			if( ! StaticPrimitive.FaceSmoothingGroups[f].Groups.Num() )
				OutFile.Logf("0");

			for( INT s=0; s<StaticPrimitive.FaceSmoothingGroups[f].Groups.Num(); s++ )
			{
				OutFile.Logf("%i",  StaticPrimitive.FaceSmoothingGroups[f].Groups[s] + 1  );// Groups 1-32 ... ?
				if( s+1 < StaticPrimitive.FaceSmoothingGroups[f].Groups.Num() )
					OutFile.Logf(",");
			}
			}
		OutFile.Logf(" "); // 1 space at end of whatever space-free smoothing groups there were.

		// Material index.
		if( StaticPrimitive.Faces[f].MaterialIndices.Num() > 0 )
		{
			OutFile.Logf("\t*MESH_MTLID %i", StaticPrimitive.Faces[f].MaterialIndices[0]);  // Preceded by: TAB.
		}
		// EOL
		OutFile.LogfLn("");
	}
	OutFile.LogfLn("\t\t}");

	// TVerts - stand-alone UV pairs.
	OutFile.LogfLn("\t\t*MESH_NUMTVERTEX %i",StaticPrimitive.Wedges.Num() );
	OutFile.LogfLn("\t\t*MESH_TVERTLIST {");
	for( INT t=0; t<StaticPrimitive.Wedges.Num();t++ )
	{
		// Face's vertex indices.
		OutFile.LogfLn("\t\t\t*MESH_TVERT %i\t%f\t%f\t%f",
			t,
			StaticPrimitive.Wedges[t].U,
			StaticPrimitive.Wedges[t].V,
			0
			);
	}
	OutFile.LogfLn("\t\t}");

	// TvFaces - for each face, three pointers into the TVerts.
	OutFile.LogfLn("\t\t*MESH_NUMTVFACES %i",StaticPrimitive.Faces.Num() );
	OutFile.LogfLn("\t\t*MESH_TFACELIST {");
	for( INT w=0; w<StaticPrimitive.Faces.Num(); w++ )
	{
		// Face's vertex indices.
		OutFile.LogfLn("\t\t\t*MESH_TFACE %i\t%i\t%i\t%i",
			w,
			StaticPrimitive.Faces[w].WedgeIndex[0],
			StaticPrimitive.Faces[w].WedgeIndex[1],
			StaticPrimitive.Faces[w].WedgeIndex[2]
			);
	}
	OutFile.LogfLn("\t\t}");

	//
	// Save second UV channel verts and indices - only if data was present in the scene.
	//
	if( StaticPrimitive.Wedges2.Num() )
	{
		OutFile.LogfLn("\t\t*MESH_MAPPINGCHANNEL 2 {");

		// Write out all 2nd channel TVerts and faces' vert indices - in Wedges2

		// TVerts - stand-alone UV pairs.
		OutFile.LogfLn("\t\t\t*MESH_NUMTVERTEX %i",StaticPrimitive.Wedges2.Num() );
		OutFile.LogfLn("\t\t\t*MESH_TVERTLIST {");
		for( INT t=0; t<StaticPrimitive.Wedges2.Num();t++ )
		{
			OutFile.LogfLn("\t\t\t\t*MESH_TVERT %i\t%f\t%f\t%f",
				t,
				StaticPrimitive.Wedges2[t].U,
				StaticPrimitive.Wedges2[t].V,
				0
				);
		}
		OutFile.LogfLn("\t\t\t}");

		// TFaces's second set of Wedge indices -  indices into preceding "TVerts".
		OutFile.LogfLn("\t\t\t*MESH_NUMTVFACES %i",StaticPrimitive.Faces.Num() );
		OutFile.LogfLn("\t\t\t*MESH_TFACELIST {");

		for( INT w=0; w<StaticPrimitive.Faces.Num(); w++ )
		{
			// Face's vertex indices.
			OutFile.LogfLn("\t\t\t\t*MESH_TFACE %i\t%i\t%i\t%i",
				w,
				StaticPrimitive.Faces[w].Wedge2Index[0],
				StaticPrimitive.Faces[w].Wedge2Index[1],
				StaticPrimitive.Faces[w].Wedge2Index[2]
				);
		}
		OutFile.LogfLn("\t\t\t}");
		OutFile.LogfLn("\t\t}");// End of  "MAPPINGCHANNEL 2" subpart.
	}

	//
	// Save  per-vertex coloring if data was present in the scene.
	//
	if( StaticPrimitive.VertColors.Num() )
	{
		OutFile.LogfLn("\t\t*MESH_NUMCVERTEX %i",StaticPrimitive.VertColors.Num() );
		OutFile.LogfLn("\t\t*MESH_CVERTLIST {");
		for( INT t=0; t< StaticPrimitive.VertColors.Num(); t++)
		{
			OutFile.LogfLn("\t\t\t*MESH_VERTCOL %i\t%f\t%f\t%f",
				t,
				StaticPrimitive.VertColors[t].R,
				StaticPrimitive.VertColors[t].G,
				StaticPrimitive.VertColors[t].B
				);
		}
		OutFile.LogfLn("\t\t}");

		// TFaces's second set of Wedge indices -  indices into preceding CVERTs.
		OutFile.LogfLn("\t\t\t*MESH_NUMCVFACES %i",StaticPrimitive.Faces.Num() );
		OutFile.LogfLn("\t\t\t*MESH_CFACELIST {");
		for( INT w=0; w<StaticPrimitive.Faces.Num(); w++ )
		{
			// Face's vertex indices.  Since the VertColors are always the same layout as the Wedges, we can use WedgeIndex here too.
			OutFile.LogfLn("\t\t\t\t*MESH_CFACE %i\t%i\t%i\t%i",
				w,
				StaticPrimitive.Faces[w].WedgeIndex[0],
				StaticPrimitive.Faces[w].WedgeIndex[1],
				StaticPrimitive.Faces[w].WedgeIndex[2]
				);
		}
		OutFile.LogfLn("\t\t}");
	}

	OutFile.LogfLn("\t}"); // End of "mesh" chunk.
	OutFile.LogfLn("\t*MATERIAL_REF 0"); // All materials refer to the one big multi-sub material.
	OutFile.LogfLn("}");	//End of "GeomObject".

	return 1;
}


//
// Write to an ASE-type text file, with individual sections for the materials, and for each primitive.
//
int SceneIFC::SaveStaticMeshes( char* OutFileName )
{

	XSI::Application app;


   TextFile OutFile;	 // Output text file
   OutFile.Open( NULL, OutFileName, 1);

	// Standard header.....
	OutFile.Logf("*3DSMAX_ASCIIEXPORT\n");

	OutFile.LogfLn("*COMMENT \"AxTool ASE output, extracted from scene: [%s]\"", "TODO: XSI" );

	//
	// Materials
	//

	// For simplicity, all materials become part of one big artificial multi-sub material.

	OutFile.LogfLn("*MATERIAL_LIST {");
	OutFile.LogfLn("*MATERIAL_COUNT 1");
   	OutFile.LogfLn("\t*MATERIAL 0 {");
	OutFile.LogfLn("\t\t*MATERIAL_NAME \"AxToolMultiSubMimicry\"");
	OutFile.LogfLn("\t\t*MATERIAL_CLASS \"Multi/Sub-Object\"");
	OutFile.LogfLn("\t\t*NUMSUBMTLS %i",StaticMeshMaterials.Num());
	for( INT m=0; m<StaticMeshMaterials.Num(); m++)
	{
		OutFile.LogfLn("\t\t*SUBMATERIAL %i {",m);
		OutFile.LogfLn("\t\t\t*MATERIAL_NAME \"%s\"",StaticMeshMaterials[m].Name.StringPtr());
		OutFile.LogfLn("\t\t\t*MATERIAL_CLASS \"Standard\"");
		OutFile.LogfLn("\t\t\t*MAP_DIFFUSE {");
		OutFile.LogfLn("\t\t\t\t*MAP_CLASS \"Bitmap\"");
		OutFile.LogfLn("\t\t\t\t*BITMAP \"%s\"", StaticMeshMaterials[m].BitmapName.StringPtr() );
		OutFile.LogfLn("\t\t\t\t*UVW_U_OFFSET 0.0");
		OutFile.LogfLn("\t\t\t\t*UVW_V_OFFSET 0.0");
		OutFile.LogfLn("\t\t\t\t*UVW_U_TILING 1.0");
		OutFile.LogfLn("\t\t\t\t*UVW_V_TILING 1.0"); // The line that triggers a material to be added in the Unrealed ASE reader & ends the Diffuse section..
		OutFile.LogfLn("\t\t\t}");
		OutFile.LogfLn("\t\t}");
	}
	OutFile.LogfLn("\t}");
	OutFile.LogfLn("}");


	// IF consolidation is requested, and there are multiple primitives present, put everything in a new single static primitive and save that one instead.
	if( OurScene.DoConsolidateGeometry && (StaticPrimitives.Num()>1)  )
	{
		GeometryPrimitive ConsolidatedGeometry;
		Memzero( &ConsolidatedGeometry, sizeof( GeometryPrimitive) );

		// Group the StaticPrimitives array into ConsolidatedGeometry.
		ConsolidateStaticPrimitives( &ConsolidatedGeometry );
		 WriteStaticPrimitive( ConsolidatedGeometry, OutFile );
	}
	else
	{
		// Otherwise, loop over all primitives.
		for(INT PrimIdx=0; PrimIdx<StaticPrimitives.Num(); PrimIdx++)
		{
			WriteStaticPrimitive( StaticPrimitives[PrimIdx], OutFile );
		}
	}

   // Ready.
   OutFile.Close();

   // Report.
   if( !OurScene.DoSuppressPopups )
   {
	   // Tally total faces.
	   INT TotalFaces = 0;
	   INT MultiMappings = 0;
	   for( INT p=0; p< StaticPrimitives.Num(); p++ )
	   {
		   TotalFaces += StaticPrimitives[p].Faces.Num();
		   MultiMappings += ( StaticPrimitives[p].Wedges2.Num() > 0);
	   }
	   PopupBox("Staticmesh [%s] exported: %i parts, %i polygons, %i materials, %i double UV maps.", OutFileName, StaticPrimitives.Num(), TotalFaces, StaticMeshMaterials.Num(), MultiMappings );
   }

   return 1;

}



//
// Static mesh MEL command line exporting.
//
//  Command line arguments optionally include the geometry name(s) (can be multiple)  to export,
//  destination file, all interface options (NCSUAO+ or - )
//
//  When none given: all meshes in the scene are exported with the name of the biggest(in face count) one.
//
//
/*
MStatus UTExportMesh( const MArgList& args )
{
	MStatus stat = MS::kSuccess;

	// #TODO -  interpret command line !

	INT SavedPopupState = OurScene.DoSuppressPopups;
	OurScene.DoSuppressPopups = true;

	// First digest the scene into separately, named brush datastructures.
	OurScene.DigestStaticMeshes();

	// If anything to save:
	// see if we had DoGeomAsFilename -> use the main non-collision geometry name as filename
	// otherwise: prompt for name.


	//
	// Check whether we want to write directly or present a save-as menu..
	//
	char newname[MAX_PATH];
	_tcscpy(newname,("None"));

	// Anything to write ?
	if( OurScene.StaticPrimitives.Num() )
	{
		char filterlist[] = "ASE Files (*.ase)\0*.ase\0"\
					        "ASE Files (*.ase)\0*.ase\0";

		char defaultextension[] = ".ase";

		// Use first/only name in scene - prefer the selected or biggest (selected) primitive...

		INT NameSakeIdx=0;
		INT MostFaces = 0;
		INT UniquelySelected = 0;
		INT TotalSelected = 0;
		for( int s=0; s<OurScene.StaticPrimitives.Num();s++)
		{
			INT NewFaceCount = OurScene.StaticPrimitives[s].Faces.Num();
			if( OurScene.StaticPrimitives[s].Selected  )
			{
				UniquelySelected = s;
				TotalSelected++;
			}
			if(  MostFaces < NewFaceCount )
			{

				NameSakeIdx = s;
				MostFaces = NewFaceCount;
			}
		}
		if( TotalSelected == 1)
			NameSakeIdx = UniquelySelected;

		if( strlen( OurScene.StaticPrimitives[NameSakeIdx].Name.StringPtr() ) > 0 )
		{
			sprintf( newname, "%s\\%s%s", to_meshoutpath, OurScene.StaticPrimitives[NameSakeIdx].Name.StringPtr(),defaultextension );
		}
		else
		{
		   //PopupBox("Staticmesh export: error deriving filename from scene primitive.");
		}

		if( newname[0] != 0 )
			OurScene.SaveStaticMeshes( newname );

	}
	else
	{
		//
	}

	OurScene.Cleanup();

	OurScene.DoSuppressPopups = SavedPopupState;

	return stat;
};
*/

void GetXSISaveName ( char* filename, char* workpath, char *filter, char *defaultextension )
{
	using namespace XSI;

	Application app;

	CComAPIHandler tk;

	tk.CreateInstance( L"XSI.UIToolkit");
	CValue retval = tk.GetProperty( L"FileBrowser" );

	CComAPIHandler browser( retval );

	wchar_t*l_wszPath = NULL;
	A2W(&l_wszPath, workpath);

	CValue rtn;
	browser.PutProperty (  L"Filter", L"ASE Files (*.ase)|*.ase||" );
	browser.PutProperty (  L"DialogTitle", L"Save to ASE" );
	browser.PutProperty (  L"InitialDirectory", l_wszPath);
	browser.Call ( L"ShowSave", rtn);

	CValue cfilename = browser.GetProperty( L"FilePathName");

	W2AHelper ( filename, XSI::CString(cfilename).GetWideString() );

}


//
// New smoothing group extraction. Rewritten to eliminate hard-to-track-down anomalies inherent in the Maya SDK's mesh processor smoothing group extraction code.
//
void MeshProcessor::createNewSmoothingGroups( XSI::X3DObject in_obj )
{
	XSI::Geometry geom(in_obj.GetActivePrimitive().GetGeometry());
	XSI::PolygonMesh polygonmesh(geom);
	XSI::CPointRefArray points = polygonmesh.GetPoints();
	XSI::CPolygonFaceRefArray polyarray = polygonmesh.GetPolygons();
	XSI::CEdgeRefArray edgearray = polygonmesh.GetEdges();

    INT edgeTableSize = points.GetCount();
	INT numPolygons = polyarray.GetCount();
	if( edgeTableSize < 1)
		return; //SafeGuard.

	VertEdgePools.Empty();
	VertEdgePools.AddZeroed( edgeTableSize );

	// Add entries, for each edge, to the lookup table

	long e;

	for (e=0;e<edgearray.GetCount();e++)
	{
		XSI::Edge edge = edgearray[e];

		bool smooth = !edge.GetIsHard();  // Smoothness retrieval for edge.
		XSI::Vertex v1 = edge.GetVertices()[0];
		XSI::Vertex v2 = edge.GetVertices()[1];

        addEdgeInfo( v1.GetIndex(), v2.GetIndex(), smooth );  // Adds info for edge running from vertex elt.index(0) to elt.index(1) into VertEdgePools.
    }

	for ( e=0;e<polyarray.GetCount();e++)
    {
		XSI::PolygonFace polyface = polyarray[e];

        INT pvc = polyface.GetVertices().GetCount();

        for ( INT v=0; v<pvc; v++ )
        {
			XSI::Vertex XSIVertex = polyface.GetVertices()[v];
			// Circle around polygons assigning the edge IDs  to edgeinfo's
            INT a = XSIVertex.GetIndex();
			XSI::Vertex CircleAround = polyface.GetVertices()[v==(pvc-1) ? 0 : v+1];
            INT b = CircleAround.GetIndex();

            XSIEdge* elem = findEdgeInfo( a, b );
            if ( elem )  // Null if no edges found for vertices a and b.
			{
                INT edgeId = polyface.GetIndex();

                if ( INVALID_ID == elem->polyIds[0] )
				{
                    elem->polyIds[0] = edgeId; // Add poly index to poly table for this edge.
                }
                else
				{
                    elem->polyIds[1] = edgeId; // If first slot already filled, fill the second face.  Assumes each edge only has 2 faces max.
                }
            }
        }
    }

	// Fill FacePools table: for each face, a pool of smoothly touching faces and a pool of nonsmoothly touching ones.
	FacePools.AddZeroed( numPolygons );
	{for ( INT p=0; p< numPolygons; p++ )
	{
		XSI::PolygonFace polyface = polyarray[p];
		XSI::CVertexRefArray vertexList = polyface.GetVertices();

		int vcount = vertexList.GetCount();

		// Walk around this polygon. accumulate all smooth and sharp bounding faces..
		for ( int vid=0; vid<vcount;vid++ )
		{
			int a = XSI::Vertex(vertexList[ vid ]).GetIndex();
			int b = XSI::Vertex(vertexList[ vid==(vcount-1) ? 0 : vid+1 ]).GetIndex();
			XSIEdge* Edge = findEdgeInfo( a, b );
			if ( Edge )
			{
				INT FaceA = Edge->polyIds[0];
				INT FaceB = Edge->polyIds[1];
				INT TouchingFaceIdx = -1;

				if( FaceA == p )
					TouchingFaceIdx = FaceB;
				else
				if( FaceB == p )
					TouchingFaceIdx = FaceA;

				if( TouchingFaceIdx >= 0)
				{
					if( Edge->smooth )
					{
						FacePools[p].SmoothFaces.AddUniqueItem(TouchingFaceIdx);
					}
					else
					{
						FacePools[p].HardFaces.AddUniqueItem(TouchingFaceIdx);
					}
				}
			}
		}
	}}

	PolyProcessFlags.Empty();
	PolyProcessFlags.AddZeroed( numPolygons );
	{for ( INT i=0; i< numPolygons; i++ )
	{
		PolyProcessFlags[i] = NO_SMOOTHING_GROUP;
	}}

	//
	// Convert all from edges into smoothing groups.  Essentially flood fill it. but als check each poly if it's been processed yet.
	//
	FaceSmoothingGroups.AddZeroed( numPolygons );
	INT SmoothParts = 0;
	CurrentGroup = 0;
	{for ( INT pid=0; pid<numPolygons; pid++ )
	{
		if( PolyProcessFlags[pid] == NO_SMOOTHING_GROUP )
		{
			fillSmoothFaces( pid, in_obj );
			SmoothParts++;
		}
	}}

   // PopupBox(" FaceSmoothingGroups %i  - Smooth sections: %i", FaceSmoothingGroups.Num(), SmoothParts );

	// Fill GroupTouchPools: for each group, this notes which other groups touch it or overlap it in any way.
	GroupTouchPools.AddZeroed( CurrentGroup );
	for( INT f=0; f< numPolygons; f++)
	{
		TArray<INT> TempTouchPool;

		// This face's own groups..
		{for( INT i=0; i< FaceSmoothingGroups[f].Groups.Num(); i++)
		{
			TempTouchPool.AddUniqueItem( FaceSmoothingGroups[f].Groups[i] );
		}}

		// And those of the neighbours, whether touching or not..
		{for( INT n=0; n< FacePools[f].HardFaces.Num(); n++)
		{
			INT FaceIdx = FacePools[f].HardFaces[n];
			for( INT i=0; i< FaceSmoothingGroups[ FaceIdx].Groups.Num(); i++)
			{
				TempTouchPool.AddUniqueItem( FaceSmoothingGroups[FaceIdx].Groups[i] );
			}
		}}
		{for( INT n=0; n< FacePools[f].SmoothFaces.Num(); n++)
		{
			INT FaceIdx = FacePools[f].SmoothFaces[n];
			for( INT i=0; i< FaceSmoothingGroups[ FaceIdx].Groups.Num(); i++)
			{
				TempTouchPool.AddUniqueItem( FaceSmoothingGroups[FaceIdx].Groups[i] );
			}
		}}

		// Then for each element add all other elements uniquely to its GroupTouchPool (includes its own..)
		for( INT g=0; g< TempTouchPool.Num(); g++)
		{
			for( INT t=0; t< TempTouchPool.Num(); t++ )
			{
				GroupTouchPools[ TempTouchPool[g] ].Groups.AddUniqueItem( TempTouchPool[t] );
			}
		}
	}

	// Distribute final smoothing groups by carefully checking against already assigned groups in
	// each group's touchpools. Note faces can have many multiple touching groups so the
	// 4-colour theorem doesn't apply, but it should at least help us limit the smoothing groups to 32.

	INT numRawSmoothingGroups = GroupTouchPools.Num();
	INT HardClashes = 0;
	INT MaxGroup = 0;
	for( INT gInit =0; gInit <GroupTouchPools.Num(); gInit++)
	{
		GroupTouchPools[gInit].FinalGroup = -1;
	}

	for( INT g=0; g<GroupTouchPools.Num(); g++ )
	{
		INT FinalGroupCycle = 0;
		for( INT SGTries=0; SGTries<32; SGTries++ )
		{
			INT Clashes = 0;
			// Check all touch groups. If none has been assigned this FinalGroupCycle number, we're done otherwise, try another one.
			for( INT t=0; t< GroupTouchPools[g].Groups.Num(); t++ )
			{
				Clashes += ( GroupTouchPools[ GroupTouchPools[g].Groups[t] ].FinalGroup == FinalGroupCycle ) ? 1: 0;
			}
			if( Clashes == 0 )
			{
				// Done for this smoothing group.
				GroupTouchPools[g].FinalGroup = FinalGroupCycle;
				break;
			}
			FinalGroupCycle = (FinalGroupCycle+1)%32;
		}
		if( GroupTouchPools[g].FinalGroup == -1)
		{
			HardClashes++;
			GroupTouchPools[g].FinalGroup = 0; // Go to default group.
		}
		MaxGroup = max( MaxGroup, GroupTouchPools[g].FinalGroup );
	}

	if( HardClashes > 0)
	{
		//PopupBox(" Warning: [%i] smoothing group reassignment errors.",HardClashes );
	}

	// Resulting face smoothing groups remapping to final groups.
	{for( INT p=0; p< numPolygons; p++)
	{
		// This face's own groups..
		for( INT i=0; i< FaceSmoothingGroups[p].Groups.Num(); i++)
		{
			FaceSmoothingGroups[p].Groups[i] = GroupTouchPools[ FaceSmoothingGroups[p].Groups[i] ].FinalGroup;
		}
	}}

	// Finally, let's copy them into PolySmoothingGroups.
	PolySmoothingGroups.Empty();
	PolySmoothingGroups.AddExactZeroed( FaceSmoothingGroups.Num() );
	{for( INT i=0; i< PolySmoothingGroups.Num(); i++)
	{
		// OR-in all the smoothing bits.
		for( INT s=0; s< FaceSmoothingGroups[i].Groups.Num(); s++)
		{
			PolySmoothingGroups[i]  |=  ( 1 << (  FaceSmoothingGroups[i].Groups[s]  ) );
		}
	}}

}



//
// Flood-fills mesh with placeholder smoothing group numbers.
//  - Start from a face and reach all faces smoothly connected with "CurrentGroup"
//
//
//   ==> General rule: new triangle: get set of all groups bounding it across a sharp boundary;
//             if CurrentGroup is not in the sharpboundgroups,  just assign CurrentGroup; store surrounding unprocessed faces on stack; pick top of stack to process.
//             if CurrentGroup is in there -> see if our pre-filled Groups  have none in common with SharpBoundGroups;
//             if none in common, we can just use the Groups
//             if any in common, we assign a NEW ++LatestGroup, discard Groups, and put the LatestGroup into all our surrounding tris' Groups.
//
//      - This should propagate  edge groups, and keep everything smooth that's smooth.
//
//

// Count common elements between two TArrays of INTs. Assumes the elements are unique (otherwise the result may be too big ).
INT CountCommonElements( TArray<INT>& ArrayOne, TArray<INT>& ArrayTwo )
{
	INT Counter = 0;
	for(INT i=0;i<ArrayOne.Num(); i++)
	{
		if( ArrayTwo.Contains( ArrayOne[i] ) )
			Counter++;
	}
	return Counter;
}

// See if there are any common elements at all.
INT AnyCommonElements( TArray<INT>& ArrayOne, TArray<INT>& ArrayTwo )
{
	INT LastTested = -9999999;
	for(INT i=0;i<ArrayOne.Num(); i++)
	{
		if( ArrayOne[i] != LastTested ) // Avoid unnecessary testing of same item.
		{
			if( ArrayTwo.Contains( ArrayOne[i] ) )
			{
				return 1;
			}
			LastTested = ArrayOne[i];
		}
	}
	return 0;
}



void MeshProcessor::fillSmoothFaces( INT polyid,  XSI::X3DObject in_obj )
{
	TArray<INT> TodoFaceStack;
	TodoFaceStack.AddItem( polyid ); // Guaranteed to have been a NO_SMOOTHING_GROUP marked face.

	INT LatestGroup = CurrentGroup;

	while( TodoFaceStack.Num() )
	{
		// Get top of stack.
		INT StackTopIdx = TodoFaceStack.Num() -1;
		INT ThisFaceIdx = TodoFaceStack[ StackTopIdx ];
		TodoFaceStack.DelIndex( StackTopIdx );

		PolyProcessFlags[ThisFaceIdx] = 1; // Mark as processed.

		// CurrentGroup, our first choice, is not in any of the groups assigned to any of the faces of the 'sharp' connecting face set ? Then we'll try to use it.
		INT SharpSetCurrMatches = 0;
		{for( INT t=0; t< FacePools[ ThisFaceIdx].HardFaces.Num(); t++ )
		{
			INT HardFaceAcross = FacePools[ ThisFaceIdx].HardFaces[t] ;
			SharpSetCurrMatches += FaceSmoothingGroups[ HardFaceAcross  ].Groups.Contains( CurrentGroup );
		}}

		// Do our "pre-filled" groups match any sharply connected groups ?
		INT SharpSetGroupMatches = 0;
		{for( INT t=0; t< FacePools[ ThisFaceIdx].HardFaces.Num(); t++ )
		{
			INT HardFaceAcross = FacePools[ ThisFaceIdx].HardFaces[t] ;
			//SharpSetGroupMatches += CountCommonElements(  FaceSmoothingGroups[ThisFaceIdx].Groups, FaceSmoothingGroups[HardFaceAcross].Groups );
			SharpSetGroupMatches += AnyCommonElements(  FaceSmoothingGroups[ThisFaceIdx].Groups, FaceSmoothingGroups[HardFaceAcross].Groups );
		}}

		UBOOL NewGroup = true;
		INT SplashGroup = CurrentGroup;

		// No conflicts, then we can assign the default 'currentgroup' and move on.
		if( SharpSetCurrMatches == 0  && SharpSetGroupMatches == 0 )
		{
			FaceSmoothingGroups[ ThisFaceIdx ].Groups.AddItem( CurrentGroup );
			NewGroup = false;
		}
		else
		{
			// If CurrentGroup _does_ match across sharp, but none of our pre-setgroups match AND there are more than 0 - we have a definite candidate already.
			if( ( SharpSetCurrMatches > 0 )  && (SharpSetGroupMatches == 0) &&  (FaceSmoothingGroups[ThisFaceIdx].Groups.Num() > 0 ) )
			{
				NewGroup=false;
				// Plucked  our existing, first pre-set group as a candidate to splash over our environment as we progress - always ensures smoothess with our smooth neighbors.
				SplashGroup = FaceSmoothingGroups[ThisFaceIdx].Groups[0];
			}
		}

		if( NewGroup )
		{
			LatestGroup++;
			SplashGroup = LatestGroup;
			FaceSmoothingGroups[ThisFaceIdx].Groups.Empty();  // We don't need any pre-set groups for smoothness, since we'll saturate our environment.
			FaceSmoothingGroups[ThisFaceIdx].Groups.AddItem( SplashGroup ); // Set group in this face.
		}

		// If we added ANY single new group that's not our default group, we splash it all over our _non-processed_ environment, because we need to
		// smoothly mesh with our default environment,  always.  This way, 'odd' groups progress themselves as necessary.
		if( SplashGroup != CurrentGroup )
		{
			for( INT t=0; t< FacePools[ ThisFaceIdx ].SmoothFaces.Num(); t++ )
			{
				INT SmoothFaceAcross = FacePools[ ThisFaceIdx ].SmoothFaces[t];
				// Only do it in case of == LatestGroup - ensures no corruption with anything else around.. - or when come from pre-loaded Groups; only forward to un-processed faces.
				if(  (SplashGroup == LatestGroup) || ( PolyProcessFlags[ SmoothFaceAcross ] < 0 ) )
				{
					FaceSmoothingGroups[ SmoothFaceAcross ].Groups.AddUniqueItem( SplashGroup );
				}
			}
		}

		// Now add all unprocessed, unqueued,  smoothly touching ones to our todo stack.
		for( INT t=0; t< FacePools[ ThisFaceIdx ].SmoothFaces.Num(); t++ )
		{
			INT SmoothFaceAcross = FacePools[ ThisFaceIdx ].SmoothFaces[t];
			if( PolyProcessFlags[ SmoothFaceAcross ]  == NO_SMOOTHING_GROUP ) // ONLY unprocessed ones are added, and setting QUEUED  will ensure they're uniquely added.
			{
				TodoFaceStack.AddItem( SmoothFaceAcross );
				PolyProcessFlags[ SmoothFaceAcross ]  = QUEUED_FOR_SMOOTHING;
			}
		}
	} // While still faces to process..

	// Currentgroup up to Latestgroup encompass all new unique groups assigned in this run for this floodfilled area.

	// When done, make CurrentGroup higher than any we've used.
	CurrentGroup = ++LatestGroup;

}



//
// Adds a new edge info element to the vertex table.
//
void MeshProcessor::addEdgeInfo( int v1, int v2, bool smooth )
{
	XSIEdge NewEdge;
	NewEdge.vertId = v2;
	NewEdge.smooth = smooth;
	NewEdge.polyIds[0] = INVALID_ID;
	NewEdge.polyIds[1] = INVALID_ID;
	// Add it always.
	VertEdgePools[v1].Edges.AddItem( NewEdge );
}

//
// Find an edge connecting vertex 1 and vertex 2.
//
XSIEdge* MeshProcessor::findEdgeInfo( int v1, int v2 )
{
	// Look in VertEdgePools[v1] for v2..
	//   or in VertEdgePools[v2] for v1..
	INT EdgeIndex;

	EdgeIndex = 0;
    while( EdgeIndex < VertEdgePools[v1].Edges.Num() )
	{
        if( VertEdgePools[v1].Edges[EdgeIndex].vertId == v2 )
		{
            return &(VertEdgePools[v1].Edges[EdgeIndex]);
        }
        EdgeIndex++;
    }

    EdgeIndex = 0;
	while( EdgeIndex < VertEdgePools[v2].Edges.Num() )
	{
        if( VertEdgePools[v2].Edges[EdgeIndex].vertId == v1 )
		{
            return &(VertEdgePools[v2].Edges[EdgeIndex]);
        }
        EdgeIndex++;
    }

    return NULL;
}


bool GetNodeColor( XSI::X3DObject in_obj, long in_NodeIndex, XSI::CColor& color )
{


	XSI::Geometry geom(in_obj.GetActivePrimitive().GetGeometry());
	XSI::PolygonMesh polygonmesh(geom);


	XSI::ClusterProperty clusterProp = polygonmesh.GetCurrentVertexColor();


	if ( !clusterProp.IsValid() )
		return false;

	XSI::CClusterPropertyElementArray colorElementArray(clusterProp.GetElements());

	XSI::CDoubleArray colorArray(colorElementArray.GetArray());

	color.r = colorArray[ in_NodeIndex * 4 ];
	color.g = colorArray[ (in_NodeIndex * 4) + 1 ];
	color.b = colorArray[ (in_NodeIndex * 4) + 2 ];
	color.a = colorArray[ (in_NodeIndex * 4) + 3 ];

	return true;
}