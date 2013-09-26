
/**************************************************************************

   Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

   BrushExport - brush export support for Unreal Technology

   Created by Erik de Neve
   Smoothing group extraction algorithm from the Maya SDK's  MayaTranslator sources / rewritten to allow multiple per poly.

   .........................

   Takes a 'static' mesh including any smoothing groups, material names, and
   writes it to a text based format which the Unreal Editor can read.


    -> Collision primitives: these 5-character tags enable Karma to interpret them as 'platonic' primtives (not polygon-based)
	     - MCDCX_name  -> Convex geometry (defined by its verts )  => not necessarily a KARMA primitive...
		 - MCDSP_name  -> Sphere
		 - MCDCY_name  -> Cylinder
		 - MCDBX_name  -> box


   TODO:
    -> Extracting Multiple UV's :



***************************************************************************/

#include "ActorX.h"
#include "MayaInterface.h"
#include "SceneIfc.h"
#include ".\res\resource.h"

//PCF BEGIN
#ifdef NEWMAYAEXPORT
#include "mayaHelper.h"
#endif
//PCF END

extern WinRegistry PluginReg;
extern MObjectArray AxSceneObjects;
extern MObjectArray AxSkinClusters;
extern MObjectArray AxShaders;
extern MObjectArray AxBlendShapes;


//
// Mesh-specific selection check.
//
UBOOL isMeshSelected( const MDagPath & path )
{
	MStatus status;

	//create an iterator for the selected mesh components of the DAG
	MSelectionList selectionList;
	if (MStatus::kFailure == MGlobal::getActiveSelectionList(selectionList))
	{
		return false;
	}

	MItSelectionList itSelectionList(selectionList, MFn::kMesh, &status);
	if (MStatus::kFailure == status)
	{
		return false;
	}

	for (itSelectionList.reset(); !itSelectionList.isDone(); itSelectionList.next())
	{
		MDagPath dagPath;
		//get the current dag path
		if( MStatus::kFailure != itSelectionList.getDagPath(dagPath))
		{
			//MString PathOne = dagPath.fullPathName(&status);
			//MString PathTwo = path.fullPathName(&status);
			//DLog.LogfLn(" Selection check: [%s] against <[%s]>",PathOne.asChar(), PathTwo.asChar() );

			if( path == dagPath )
				return true;
		}
	}
	return false;
}




//
// Static Mesh export (universal) dialog procedure - can write brushes to separarte destination folder.
//

INT_PTR CALLBACK StaticMeshDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch (msg)
	{
		case WM_INITDIALOG:

		_SetCheckBox( hWnd, IDC_CHECKTRIANGLE,   OurScene.DoForceTriangles );
		_SetCheckBox( hWnd, IDC_CHECKSMOOTH,     OurScene.DoConvertSmooth );
		_SetCheckBox( hWnd, IDC_CHECKGEOMNAME,   OurScene.DoGeomAsFilename );
		_SetCheckBox( hWnd, IDC_CHECKSELECTEDSTATIC, OurScene.DoSelectedStatic );
		_SetCheckBox( hWnd, IDC_CHECKSUPPRESS,   OurScene.DoSuppressPopups );
		_SetCheckBox( hWnd, IDC_CHECKUNDERSCORE, OurScene.DoUnderscoreSpace );
		_SetCheckBox( hWnd, IDC_CHECKCONSOLIDATE, OurScene.DoConsolidateGeometry);


		if ( to_meshoutpath[0] )
		{
			SetWindowText(GetDlgItem(hWnd,IDC_EDITOUTPATH),to_meshoutpath);
			_tcscpy_s(LogPath,to_meshoutpath);
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
				if( GetFolderName(hWnd, dir, _countof(dir)) )
				{
					_tcscpy_s(to_meshoutpath,dir);
					_tcscpy_s(LogPath,dir); //
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
						_tcscpy_s(to_meshoutpath,dir);
						_tcscpy_s(LogPath,dir); //
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
			case IDC_CHECKTRIANGLE:
			{
				OurScene.DoForceTriangles = _GetCheckBox(hWnd,IDC_CHECKTRIANGLE);
				PluginReg.SetKeyValue("DOFORCETRIANGLES",OurScene.DoForceTriangles);
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
				// First digest the scene into separately, named brush datastructures.
				if (OurScene.DigestStaticMeshes() == -1) return false;

				// If anything to save:
				// see if we had DoGeomAsFilename -> use the main non-collision geometry name as filename
				// otherwise: prompt for name.

				//
				// Check whether we want to write directly or present a save-as menu..
				//
				char newname[MAX_PATH];
				_tcscpy_s(newname,("None"));

				// Anything to write ?
				if( OurScene.StaticPrimitives.Num() )
				{
					char filterlist[] = "ASE Files (*.ase)\0*.ase\0"\
					        			"ASE Files (*.ase)\0*.ase\0";

					char defaultextension[] = ".ase";

					if( ! OurScene.DoGeomAsFilename )
					{
						if (!GetSaveName( hWnd, newname, _countof(newname), to_meshoutpath, filterlist, defaultextension ))
							newname[0] = 0;
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
							sprintf_s( newname, "%s\\%s%s", to_meshoutpath, OurScene.StaticPrimitives[NameSakeIdx].Name.StringPtr(),defaultextension );
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
    // <Path>.fullPathName().asChar()
	MStatus	stat;
	MObject MeshObject = AxSceneObjects[ (int)SerialTree[TreeIndex].node ];
	MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[TreeIndex].node ];
	MDagPath  DagPath;
	DagNode.getPath( DagPath );





	//SerialTree[TreeIndex].IsSelected = isObjectSelected ( DagPath );
	SerialTree[TreeIndex].IsSelected = isMeshSelected ( DagPath );

	if( OurScene.DoSelectedStatic && !SerialTree[TreeIndex].IsSelected )
	{
		return 0; // Ignore unselected.
	}

	//PCF BEGIN

	////	dirty valid maya triangulation via mel command - soon replaced by api
	////	Suppouse that we need always to triangulate (true?)

	//MStringArray triangNode;
	//MString triangulateCmd = "polyTriangulate -ch 1 " + DagNode.name();
	//MGlobal::executeCommand(triangulateCmd, triangNode, 0, 0);
	//PCF END

	UBOOL MeshHasMapping = true;
	UBOOL bCollision = false;
	UBOOL bSmoothingGroups = false;

	// Decide if it's a nontextured OR collision-only mesh - look at material.
	MFnMesh MeshFunction( DagPath, &stat );




	INT NumVerts = MeshFunction.numVertices();
	INT NumFaces = MeshFunction.numPolygons();

	MObjectArray MayaShaders;
	MIntArray ShaderIndices;

	MeshFunction.getConnectedShaders( 0, MayaShaders, ShaderIndices);
	// Get the shader 'index; during triangle processing as with BYTE MaterialIndex = ShaderIndices[TriIndex] & 255;

	INT NewMaterials = 0;

	if( MayaShaders.length() == 0)
	{
		MeshHasMapping = false;

	}

	// Get name.
	DLog.Logf(" MESH NAME: [%s] Selected:[%i] mapping: %i \n" , DagNode.name().asChar(), SerialTree[TreeIndex].IsSelected, (INT)MeshHasMapping );

	// Recognize any collision primitive naming conventions ?
	// into separate MCD SP BX CY CX_name
	// MCDCX is the default for untextured geometry.
	CHAR PrimitiveName[MAX_PATH];
	strcpysafe( PrimitiveName, DagNode.name().asChar(), MAX_PATH );
    if( CheckSubString( PrimitiveName,_T("MCD")) )
	{
		bCollision = true; // collision-only architecture.
	}

	// New primitive.
	INT PrimIndex = StaticPrimitives.AddExactZeroed(1);

	StaticPrimitives[PrimIndex].Name.CopyFrom( PrimitiveName );
	StaticPrimitives[PrimIndex].Selected = SerialTree[TreeIndex].IsSelected;

	//
	//	PCF BEGIN
	//	export in object space 	for t3d file
	//	t3d file for current mesh is exported via mel
	//
	if (OurScene.DoExportInObjectSpace)
	{

		MFnTransform meshTrans (DagPath.transform());
		//MGlobal::displayInfo(meshTrans.name())   ;


		double scals[3] ;
		meshTrans.getScale(scals);

		// pcf: can`t export with scale
		if (scals[0] != 1 ||scals[1] != 1 ||scals[2] != 1)
		{
			MGlobal::displayError("Object " +meshTrans.name() + " with scale other than 1 1 1 can`t be exported in object space mode. Please freeze scale transformation" );
			return -1;

		}

		MVector trans = meshTrans.getTranslation(MSpace::kPostTransform);


		MEulerRotation  rotation ;
		meshTrans.getRotation(rotation );


		//X = - Points[p].x;
		//Y =   Points[p].z;
		//Z =   Points[p].y;

		StaticPrimitives[PrimIndex]._rotation[0] = rotation.x;
		StaticPrimitives[PrimIndex]._rotation[1] = rotation.y;
		StaticPrimitives[PrimIndex]._rotation[2] = rotation.z;


		StaticPrimitives[PrimIndex]._translation[0] = -trans.x * OurScene.VertexExportScale;
		StaticPrimitives[PrimIndex]._translation[1] = -trans.z * OurScene.VertexExportScale;
		StaticPrimitives[PrimIndex]._translation[2] = trans.y * OurScene.VertexExportScale;

	}
	// PCF END



	// Ensure our dagPathMesh is valid..
	if( MS::kSuccess != stat )
		return 0;

	// If the mesh had no materials/mapping, if desired it will become 'convex' collision architecture automatically ?
	if( 0 ) // OurScene.DoUntexturedAsCollision && !MeshHasMapping && !bCollision )
	{
		sprintf_s( PrimitiveName, "MCDCX_%s",PrimitiveName );
		bCollision = true;
	}

	//
	// Now stash the entire thing in our StaticPrimitives, regardless of mapping/smoothing groups......
	// Accumulate shaders in order of occurrence in the triangles.
	//


	// Get points array & iterate vertices.
    MDagPath ShapedagPath;
    stat = DagNode.getPath( ShapedagPath );

	// Ensure our dagPathMesh is valid..
	if( MS::kSuccess != stat )
		return 0;

	//PCF BEGIN - object space export + global scale
	MFloatPointArray	Points;
	MeshFunction.setObject( ShapedagPath );
	if (OurScene.DoExportInObjectSpace)
		MeshFunction.getPoints( Points, MSpace::kObject );
	else
		MeshFunction.getPoints( Points, MSpace::kWorld );

	UBOOL bFlipAxii = MGlobal::isYAxisUp();

	// 3D Vertices.
	if( bFlipAxii )
	{
		for( int p=0; p< NumVerts; p++)
		{
			INT NewVertIdx = StaticPrimitives[PrimIndex].Vertices.AddZeroed(1);
			StaticPrimitives[PrimIndex].Vertices[NewVertIdx].X = - Points[p].x * OurScene.VertexExportScale; //#SKEL - needed for proper SHAPE...
			StaticPrimitives[PrimIndex].Vertices[NewVertIdx].Y =   Points[p].z* OurScene.VertexExportScale; //
			StaticPrimitives[PrimIndex].Vertices[NewVertIdx].Z =   Points[p].y* OurScene.VertexExportScale;	//
		}
	}
	else
	{
		for( int p=0; p< NumVerts; p++)
		{
			INT NewVertIdx = StaticPrimitives[PrimIndex].Vertices.AddZeroed(1);
			StaticPrimitives[PrimIndex].Vertices[NewVertIdx].X = Points[p].x* OurScene.VertexExportScale;
			StaticPrimitives[PrimIndex].Vertices[NewVertIdx].Y = Points[p].y* OurScene.VertexExportScale;
			StaticPrimitives[PrimIndex].Vertices[NewVertIdx].Z = Points[p].z* OurScene.VertexExportScale;
		}
	}
	//PCF END


	// Vertex-color detection.

	bool hasColors = false;
	MString colorSetName;
	MeshFunction.getCurrentColorSetName ( colorSetName);
	MColorArray fColorArray;
	if ( MeshFunction.getCurrentColorSetName ( colorSetName) == MStatus::kSuccess &&
			colorSetName.length() &&
			MeshFunction.getFaceVertexColors(fColorArray) == MStatus::kSuccess)
	{
		hasColors = true;
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





//PCF BEGIN
// maya api triangulation
#ifdef NEWMAYAEXPORT
	if( MayaShaders.length() == 0)
	{
		MGlobal::displayWarning(MString("possible shader-less geometry encountered for node  ") + DagNode.name());
	}

	triangulatedMesh triMesh;
	if (! triMesh.triangulate(DagPath))
	{
		MGlobal::displayError(MString("triangulation failed for  ") + DagNode.name());
		return -1;

	}
	triMesh.createEdgesPool();

	// Any smoothing group conversion requested ?
	MeshProcessor TempTranslator;
	if( OurScene.DoConvertSmooth && MeshHasMapping && ( !bCollision ) )
	{
		TempTranslator.createNewSmoothingGroupsFromTriangulatedMesh( triMesh );  // NEW 3dsmax-type-smoothing group creation.

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
	TArray< GroupList > NewSmoothingGroups;
	UBOOL bHasSmoothingGroups = (StaticPrimitives[PrimIndex].FaceSmoothingGroups.Num() > 0);


	for ( int i =0 ; i <  triMesh.triangles.size(); i++)
	{
		triangulatedMesh::Triangle & t = *(triMesh.triangles[i]);
		int PolyIndex = i;


		INT MaterialIndex = t.ShadingGroupIndex & 255;

		INT ThisMaterial = 0;
		if(MaterialIndex == -1)
		{
			MGlobal::displayError(DagPath.partialPathName() + MString(".f[") + i + MString("doesn`t have material assigned!" ));
			return  -1 ;
		}
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


		//// A brand new face.

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

		for ( int j =0 ; j <3; j++)
		{
			int VertIdx = t.Indices[j].pointIndex;

			GWedge NewWedge;
			//INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddZeroed(1);
			NewWedge.MaterialIndex = ThisMaterial; // Per-corner material index..
			NewWedge.PointIndex = VertIdx; // Maya's face vertices indices for this face.

			int indexuv =0;

			int uvIndex = t.Indices[j].tcIndex[0];
			NewWedge.U = triMesh.uArray[0][uvIndex];
			NewWedge.V = triMesh.vArray[0][uvIndex];



			// Should we merge identical wedges here instead of counting on editor ?  With the way ASE is imported it may make no difference.
			INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddItem( NewWedge);
			// New wedge on every corner of a face.
			StaticPrimitives[PrimIndex].Faces[NewFaceIdx].WedgeIndex[j] = NewWedgeIdx;



			if (t.Indices[j].hasExtraUVs[0])
			{
				uvIndex = t.Indices[j].tcIndex[1];

				GWedge NewWedge2;
				NewWedge2.MaterialIndex = ThisMaterial;
				NewWedge2.U = triMesh.uArray[1][uvIndex];
				NewWedge2.V = triMesh.vArray[1][uvIndex];
				NewWedge2.PointIndex = VertIdx;

				INT NewWedge2Idx = StaticPrimitives[PrimIndex].Wedges2.AddItem( NewWedge2);
				StaticPrimitives[PrimIndex].Faces[NewFaceIdx].Wedge2Index[j] = NewWedge2Idx;


			}

			// Store per-vertex color (for this new wedge) only if any were actually defined.
			if( hasColors &&  realColorCount > 0 )
			{
				GColor NewVertColor; // Defaults to RGBA = 0.0 0.0 0.0 1.0
				INT colorIndex = t.Indices[j].vertexColor;

				if( (colorIndex >= 0) && (colorIndex < (INT)fColorArray.length()) )
				{
					NewVertColor.A = fColorArray[colorIndex].a;
					NewVertColor.R = fColorArray[colorIndex].r;
					NewVertColor.G = fColorArray[colorIndex].g;
					NewVertColor.B = fColorArray[colorIndex].b;
				}

				// Store in VertColors - which will be either empty OR contain the same number of elements as this primitive's Wedges array.
				StaticPrimitives[PrimIndex].VertColors.AddItem(NewVertColor);
			}

		}
	}

//PCF END

#else
	// old export code

	// Any smoothing group conversion requested ?

	MeshProcessor TempTranslator;

	if( OurScene.DoConvertSmooth && MeshHasMapping && ( !bCollision ) )
	{
		MFnDagNode fnMeshNode( MeshObject, &stat); // Intermediate to get at the DAGPATH..
		if( MS::kSuccess == stat)
		{
			MDagPath dagPathMesh;
			stat = fnMeshNode.getPath( dagPathMesh );  // Puts DagPath in dagPathMesh.
			//TempTranslator.buildEdgeTable( dagPathMesh ); 				 OLD smoothing group conversion..
			TempTranslator.createNewSmoothingGroups( dagPathMesh );  // NEW 3dsmax-type-smoothing group creation.
		}

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
	TArray< GroupList > NewSmoothingGroups;
	UBOOL bHasSmoothingGroups = (StaticPrimitives[PrimIndex].FaceSmoothingGroups.Num() > 0);

	// UV set tally.
	MStringArray uvSetNames;
	MeshFunction.getUVSetNames(uvSetNames);
	INT uvSetCount = uvSetNames.length();
	MString firstUVSetName = MString("");
	MString secondUVSetName = MString("");
	if( uvSetCount > 0)
	{
		firstUVSetName = uvSetNames[0];
	}
	if( uvSetCount > 1)
	{
		secondUVSetName = uvSetNames[1];
	}

	// Faces & Wedges & Materials & Smoothing groups, all in the same run......
	for (int PolyIndex = 0; PolyIndex < NumFaces; PolyIndex++)
	{
		// Get the vertex indices for this polygon.
		MIntArray	FaceVertices;
		MeshFunction.getPolygonVertices(PolyIndex,FaceVertices);
		INT VertCount = FaceVertices.length();

		// Assumed material the same for all facets of a poly.
		// Material on this face - encountered before ? -> DigestMayaMaterial..
		INT MaterialIndex = ShaderIndices[PolyIndex]; // & 255;
		if(MaterialIndex == -1)
		{
			MString _aaa = "Some polys don`t have materials on object \"";
			_aaa += DagNode.name();
			_aaa += "\"";


			MGlobal::displayError(_aaa );
			cerr << "ERROR: " << _aaa.asChar() << endl;

			return  -1 ;

		}
		INT ThisMaterial = 0;


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

			 //DLog.LogfLn(" Material on facet %i Face %i Primitive %i is %i NumMaterialsForface %i DATA %i ",VertCount,NewFaceIdx,PrimIndex,ThisMaterial,StaticPrimitives[PrimIndex].Faces[NewFaceIdx].MaterialIndices.Num(), (INT)((BYTE*)&StaticPrimitives[PrimIndex].Faces[NewFaceIdx].MaterialIndices[0]) );

			// Fill vertex indices (breaks up face in triangle polygons.
			INT VertIdx[3];
			VertIdx[0] = 0;
			VertIdx[1] = VertCount-2;
			VertIdx[2] = VertCount-1;

			for( int i=0; i<3; i++)
			{
				//Retrieve wedges for first UV set.
				FLOAT U,V;
				stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[i],U,V,&firstUVSetName);
				if ( stat != MS::kSuccess )
				{
					DLog.Logf(" Invalid UV retrieval, index [%i] for face [%i]", VertIdx[i], PolyIndex );
				}

				//DLog.Logf(" UV logging: Face: %6i Index %6i (%6i) U %6f  V %6f \n",NewFaceIdx,i,VertIdx[i],U,V);

				GWedge NewWedge;
				//INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddZeroed(1);

				NewWedge.MaterialIndex = ThisMaterial; // Per-corner material index..
				NewWedge.U = U;
				NewWedge.V = V;
				NewWedge.PointIndex = FaceVertices[VertIdx[i]]; // Maya's face vertices indices for this face.

				// Should we merge identical wedges here instead of counting on editor ?  With the way ASE is imported it may make no difference.
				INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddItem( NewWedge);
				// New wedge on every corner of a face.
				StaticPrimitives[PrimIndex].Faces[NewFaceIdx].WedgeIndex[i] = NewWedgeIdx;

				// Any second UV set data ?
				if( uvSetCount > 1)
				{
					FLOAT U,V;
					stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[i],U,V,&secondUVSetName);
					if ( stat != MS::kSuccess )
					{
						DLog.Logf(" Invalid secondary UV retrieval, index [%i] for face [%i]", VertIdx[i], PolyIndex );
					}
					GWedge NewWedge2;
					NewWedge2.MaterialIndex = ThisMaterial;
					NewWedge2.U = U;
					NewWedge2.V = V;
					NewWedge2.PointIndex = FaceVertices[VertIdx[i]];

					INT NewWedge2Idx = StaticPrimitives[PrimIndex].Wedges2.AddItem( NewWedge2);
					StaticPrimitives[PrimIndex].Faces[NewFaceIdx].Wedge2Index[i] = NewWedge2Idx;
				}

				// Store per-vertex color (for this new wedge) only if any were actually defined.
				if( realColorCount > 0 )
				{
					GColor NewVertColor; // Defaults to RGBA = 0.0 0.0 0.0 1.0
					INT colorIndex = -1;
					if (MStatus::kFailure == MeshFunction.getFaceVertexColorIndex( PolyIndex,VertIdx[i], colorIndex))
					{
						//MGlobal::displayError("MFnMesh::getFaceVertexColorIndex");
						//return MStatus::kFailure;
					}
					else
					{
						if( (colorIndex >= 0) && (colorIndex < (INT)fColorArray.length()) )
						{
							NewVertColor.A = fColorArray[colorIndex].a;
							NewVertColor.R = fColorArray[colorIndex].r;
							NewVertColor.G = fColorArray[colorIndex].g;
							NewVertColor.B = fColorArray[colorIndex].b;
						}
					}
					// Store in VertColors - which will be either empty OR contain the same number of elements as this primitive's Wedges array.
					StaticPrimitives[PrimIndex].VertColors.AddItem(NewVertColor);
				}
			}

			VertCount--;
		}

	}// For each primitive..


#endif
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

	DLog.Logf(" Primitive [%s] processed - Faces %i verts %i UVpairs %i  New Materials %i \n",
		StaticPrimitives[PrimIndex].Name.StringPtr(),
		StaticPrimitives[PrimIndex].Faces.Num(),
		StaticPrimitives[PrimIndex].Vertices.Num(),
	    StaticPrimitives[PrimIndex].Wedges.Num(),
		NewMaterials
		);

	////pcf: remove triangulations
	//for (int q =0; q< triangNode.length() ; q++)
	//{
	//	triangulateCmd = "catchQuiet(`delete " + triangNode[q] + "`)";
	//	MGlobal::executeCommand(triangulateCmd, triangNode[q], 0, 0);
	//}
	return 1;
}



//
// Digest static meshes, filling the StaticMesh array (some may be collision geometry, or simply destined to be ignored.)
//
int SceneIFC::DigestStaticMeshes()
{
	MStatus	stat;

	//
	// Digest primitives list
	// Go over all nodes in scene, retain the one that have possible geometry.
	//
	SerializeSceneTree();
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

	// Maya shader array.
	AxShaders.clear();

	//
	// Go over the nodes list and digest each of them into the StaticPrimitives GeometryPrimitive array
	//
	INT NumMeshes = 0;
	for( INT i=0; i<SerialTree.Num(); i++)
	{
		MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ];
		INT IsRoot = ( TempActor.MatchNodeToSkeletonIndex( (void*) i ) == 0 );

		// If mesh, determine
		INT MeshFaces = 0;
		INT MeshVerts = 0;
		if( SerialTree[i].IsSkin )
		{
			MFnMesh MeshFunction( AxSceneObjects[ (int)SerialTree[i].node ], &stat );
			MeshVerts = MeshFunction.numVertices();
			MeshFaces = MeshFunction.numPolygons();
		}

		if( MeshFaces )
		{
			NumMeshes++;
			//DLog.Logf("Mesh stats:  Faces %4i  Verts %4i \n\n", MeshFaces, MeshVerts );
			if ( SerialTree[i].IsSkin )
			{
				// Process this primitive.
				if (ProcessStaticMesh(i)== -1)
					return -1;
			}
		}
	} //Serialtree

	DLog.Close();

	if( DEBUGMEM )
	{
		MemLog.Close();
	}

	//
	// Digest materials. See also 'fixmaterials'.
	//
	for( INT m=0; m < (INT)AxShaders.length(); m++ )
	{
		MFnDependencyNode MFnShader ( AxShaders[m] );
		StaticMeshMaterials.AddExactZeroed(1);

		// Copy name and bitmap name.   Other attributes necessary ?
		StaticMeshMaterials[m].Name.CopyFrom( MFnShader.name().asChar() );

		// OR get it from the actual material instead of the Shading Group ?

		CHAR BitmapFileName[MAX_PATH];
		GetTextureNameFromShader( AxShaders[m], BitmapFileName, MAX_PATH );

		if( strlen( BitmapFileName) )
			StaticMeshMaterials[m].BitmapName.CopyFrom( BitmapFileName );
		else
			StaticMeshMaterials[m].BitmapName.CopyFrom( "None" );

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
//PCF BEGIN
//
//	Write out a single ASE-GeomObject
//    01-13-05 EdN  - Added explicit tabs and some spaces-to-tabs replacements to mirror strict original 3DSMax ase output conventions (however arbitrary those may be...)
//
INT WriteStaticPrimitiveUniqueMats( GeometryPrimitive& StaticPrimitive, TextFile& OutFile, MIntArray & uniqueMaterialIndices )
{
	// GeomObject wrapper.
	OutFile.LogfLn("*GEOMOBJECT {");

	OutFile.LogfLn("\t*COMMENT \"TRANSLATION %f %f %f\"", StaticPrimitive._translation[0], StaticPrimitive._translation[1], StaticPrimitive._translation[2]);
	OutFile.LogfLn("\t*COMMENT \"ROTATION %f %f %f\"", StaticPrimitive._rotation[0], StaticPrimitive._rotation[1], StaticPrimitive._rotation[2]);
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
		//cerr << "#face " << f << endl;
		//cerr << "	mat" << endl;
		if( StaticPrimitive.Faces[f].MaterialIndices.Num() > 0 )
		{

		//	cerr << "		" << StaticPrimitive.Faces[f].MaterialIndices[0] << " / " << uniqueMaterialIndices[f] << endl;
			//OutFile.Logf("\t*MESH_MTLID %i", StaticPrimitive.Faces[f].MaterialIndices[0]);  // Preceded by: TAB.
			OutFile.Logf("\t*MESH_MTLID %i", uniqueMaterialIndices[f]);  // Preceded by: TAB.
		}
		//else
		//	cerr << "		NULL" << endl;
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
//PCF END
//
// Write to an ASE-type text file, with individual sections for the materials, and for each primitive.
//
int SceneIFC::SaveStaticMeshes( char* OutFileName )
{

   TextFile OutFile;	 // Output text file
   OutFile.Open( NULL, OutFileName, 1);

	// Standard header.....
	OutFile.Logf("*3DSMAX_ASCIIEXPORT\n");

	OutFile.LogfLn("*COMMENT \"AxTool ASE output, extracted from scene: [%s]\"", MFileIO::currentFile().asChar() );

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
	   for( INT p=0; p< StaticPrimitives.Num(); p++ )
	   {
		   TotalFaces += StaticPrimitives[p].Faces.Num();
	   }
	   PopupBox("Staticmesh [%s] exported: %i parts, %i polygons, %i materials", OutFileName, StaticPrimitives.Num(), TotalFaces, StaticMeshMaterials.Num());
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
MStatus UTExportMesh( const MArgList& args )
{
	MStatus stat = MS::kSuccess;

	// #TODO -  interpret command line !

	INT SavedPopupState = OurScene.DoSuppressPopups;
	OurScene.DoSuppressPopups = true;

	// First digest the scene into separately, named brush datastructures.
	if (OurScene.DigestStaticMeshes() == -1 ) return  MS::kFailure ;

	// If anything to save:
	// see if we had DoGeomAsFilename -> use the main non-collision geometry name as filename
	// otherwise: prompt for name.


	//
	// Check whether we want to write directly or present a save-as menu..
	//
	char newname[MAX_PATH];
	_tcscpy_s(newname,("None"));

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
			sprintf_s( newname, "%s\\%s%s", to_meshoutpath, OurScene.StaticPrimitives[NameSakeIdx].Name.StringPtr(),defaultextension );
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



//PCF BEGIN
//
// Write to an ASE-type text file for each mesh, with individual sections for the materials, and for each primitive.
//
int SceneIFC::SaveStaticMesh( char* OutFileName, INT m )
{
	if (!StaticPrimitives.Num()) return 0;
	if ( m < 0) return 0;
	if ( m >= StaticPrimitives.Num()) return 0;


	TextFile OutFile;	 // Output text file
	OutFile.Open( NULL, OutFileName, 1);

	//cerr << "saving file \"" << OutFileName << "\"" << endl;

	// Standard header.....
	OutFile.Logf("*3DSMAX_ASCIIEXPORT\n");

	OutFile.LogfLn("*COMMENT \"AxTool ASE output, extracted from scene: [%s]\"", MFileIO::currentFile().asChar() );

	//
	// Materials
	//

	// For simplicity, all materials become part of one big artificial multi-sub material.


	// pcf: reindex material list for this mesh
	//	otherwise it will spit all materials of selectd meshes witch leads to empty mesh material slots in UnrealEd

	MIntArray globalMaterialsUsedByMesh(StaticMeshMaterials.Num(), -1);

	int FacesNum = StaticPrimitives[m].Faces.Num();

	for( INT f=0; f<FacesNum;f++ )
	{
		if( StaticPrimitives[m].Faces[f].MaterialIndices.Num() > 0 )
		{
			globalMaterialsUsedByMesh[StaticPrimitives[m].Faces[f].MaterialIndices[0]] = 1 ;
		}

	}
	MIntArray uniqueMats;
	for( int f=0; f<globalMaterialsUsedByMesh.length();f++ )
	{
		if(globalMaterialsUsedByMesh[f] > -1)
		{
			uniqueMats.append(f);
//			cerr << " uniqueMats["<<uniqueMats.length()-1 << "] = " << f << endl;
		}

	}

	MIntArray uniqueMaterialIndices(FacesNum, 0);
	for( INT f=0; f<FacesNum;f++ )
	{
		if( StaticPrimitives[m].Faces[f].MaterialIndices.Num() > 0 )
		{
			int globmatInd = StaticPrimitives[m].Faces[f].MaterialIndices[0];
			bool foundd =0;
			for(int j =0 ; j < uniqueMats.length(); j++)
			{
				if (globmatInd == uniqueMats[j])
				{
					uniqueMaterialIndices[f] =j;
					break;
				}

			}
		}

	}


	OutFile.LogfLn("*MATERIAL_LIST {");
	OutFile.LogfLn("*MATERIAL_COUNT 1");
	OutFile.LogfLn("\t*MATERIAL 0 {");
	OutFile.LogfLn("\t\t*MATERIAL_NAME \"AxToolMultiSubMimicry\"");
	OutFile.LogfLn("\t\t*MATERIAL_CLASS \"Multi/Sub-Object\"");
	OutFile.LogfLn("\t\t*NUMSUBMTLS %i",uniqueMats.length());
	for( int _m=0; _m<uniqueMats.length(); _m++)
	{

				OutFile.LogfLn("\t\t*SUBMATERIAL %i {",_m);
				OutFile.LogfLn("\t\t\t*MATERIAL_NAME \"%s\"",StaticMeshMaterials[uniqueMats[_m]].Name.StringPtr());
				OutFile.LogfLn("\t\t\t*MATERIAL_CLASS \"Standard\"");
				OutFile.LogfLn("\t\t\t*MAP_DIFFUSE {");
				OutFile.LogfLn("\t\t\t\t*MAP_CLASS \"Bitmap\"");
				OutFile.LogfLn("\t\t\t\t*BITMAP \"%s\"", StaticMeshMaterials[uniqueMats[_m]].BitmapName.StringPtr() );
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
		WriteStaticPrimitiveUniqueMats( StaticPrimitives[m], OutFile, uniqueMaterialIndices );
	}
	else
	{
		// Otherwise, loop over all primitives.
		//for(INT PrimIdx=0; PrimIdx<StaticPrimitives.Num(); PrimIdx++)
	//	{
			WriteStaticPrimitiveUniqueMats( StaticPrimitives[m], OutFile, uniqueMaterialIndices );
	//	}
	}

	// Ready.
	OutFile.Close();

	// Report.
	//if( !OurScene.DoSuppressPopups )
	//{
	//	// Tally total faces.
	//	INT TotalFaces = 0;
	//	for( INT p=0; p< StaticPrimitives.Num(); p++ )
	//	{
	//		TotalFaces += StaticPrimitives[p].Faces.Num();
	//	}
	//	PopupBox("Staticmesh [%s] exported: %i parts, %i polygons, %i materials", OutFileName, StaticPrimitives.Num(), TotalFaces, StaticMeshMaterials.Num());
	//}

	return 1;

}
//PCF END
