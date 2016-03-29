// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/*
	PCF BEGIN
		New static mesh export function (codeneme ActorY) with mell support and exporting per mesh
*/
#include "SceneIFC.h"
#include "ActorY.h"

void* ActorY::creator()
{
	return new ActorY;
}

MStatus ActorY::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;

	MGlobal::displayInfo("actory command executed.\n");

	UTExportMeshY(args);
	OurScene.Cleanup();
	setResult( "actory command executed.\n" );
	return stat;
}


MStatus ActorY::UTExportMeshY( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	cerr<< endl<< "--------------------------" << endl;
	cerr<< endl<< "actorY start" << endl;

	// #TODO -  interpret command line !


	OurScene.Cleanup();

	INT SavedPopupState = OurScene.DoSuppressPopups;
	OurScene.DoSuppressPopups = true;
	//OurScene.DoSelectedStatic = true;
	OurScene.DoConsolidateGeometry =  false;
	OurScene.DoConvertSmooth =  false;
	OurScene.DoForceTriangles = false;
	OurScene.DoExportInObjectSpace =false;
	OurScene.VertexExportScale =1;
	OurScene.DoSelectedStatic =true;
	OurScene.DoExportTextureSuffix =true;



	MArgDatabase argData(syntax(), args);

	if (argData.isFlagSet("-scale"))
	{
		double globalScale  =1;
		argData.getFlagArgument("-scale", 0,  globalScale) ;
		OurScene.VertexExportScale =float(globalScale);
	}

	// First digest the scene into separately, named brush datastructures.


	if (argData.isFlagSet("-DoExportInObjectSpace"))
		OurScene.DoExportInObjectSpace =true;
	if (argData.isFlagSet("-all"))
		OurScene.DoSelectedStatic = false;
	if (argData.isFlagSet("-consolidateGeometry"))
		OurScene.DoConsolidateGeometry = true;
	if (argData.isFlagSet("-convertSmooth"))
		OurScene.DoConvertSmooth = true;


	if (OurScene.DigestStaticMeshes() == -1)
		return MS::kFailure;

	// If anything to save:
	// see if we had DoGeomAsFilename -> use the main non-collision geometry name as filename
	// otherwise: prompt for name.


	//
	// Check whether we want to write directly or present a save-as menu..
	//
	char newname[MAX_PATH];
	_tcscpy_s(newname,("None"));

	// Anything to write ?

	MString dir  ;
	if (argData.isFlagSet("-exportFile"))
	{
		argData.getFlagArgument("-exportFile", 0, dir)	  ;
	}
	else
	{
		MGlobal::displayError("no export path");
		return MS::kFailure;
	}

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
		if (argData.isFlagSet("-sep"))
		{
			for( int s=0; s<OurScene.StaticPrimitives.Num();s++)
			{
				MString outp = dir + MString("/") + MString( OurScene.StaticPrimitives[s].Name.StringPtr());
				outp += ".ase";

				cerr << (s+1)<< "/" << OurScene.StaticPrimitives.Num() << " saving \"" <<	 outp.asChar() << "\" ... " ;

				if (OurScene.SaveStaticMesh((char *)(outp.asChar()), s ) == 1)
				{
					cerr << " done! "<< endl;
				}
				else
				{
					cerr << " failed! "<< endl;
				}
			}
		}
		else
		{

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
			{
				MString outp = dir + MString("/")+ MString( newname);
				OurScene.SaveStaticMeshes( (char *)(outp.asChar()) );
			}
		}

	}
	else
	{
		//
	}

	OurScene.Cleanup();

	OurScene.DoSuppressPopups = SavedPopupState;

	cerr<< endl<< "actorY finished" << endl;

	return stat;
};



MSyntax ActorY::newSyntax()
{
	MSyntax syntax;

	syntax.addFlag("-sep", "-separateFiles", MSyntax::kNoArg   );
	syntax.addFlag("-all", "-allSelected", MSyntax::kNoArg   );

	syntax.addFlag("-cons", "-consolidateGeometry", MSyntax::kNoArg   );
	syntax.addFlag("-sm", "-convertSmooth", MSyntax::kNoArg   );


	syntax.addFlag("-f", "-exportFile", MSyntax::kString   );
	syntax.addFlag("-os", "-DoExportInObjectSpace", MSyntax::kNoArg   );
	syntax.addFlag("-s", "-scale", MSyntax::kDouble   );


	syntax.enableQuery(false);
	syntax.enableEdit(false);

	return syntax;
}