/************************************************************************** 

    SceneIFC.cpp	- Scene Interface Class for the ActorX exporter.

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
	Created by Erik de Neve

    Note:this file ONLY contains those parts of SceneIFC that are parent-application independent.
    All Max/Maya specific code and parts of SceneIFC are to be found in MaxInterface.cpp / MayaInterface.cpp.

	TODO: plenty left to clean up, reorganize and make more platform-indepentent.

	Parent-platform independent SceneIFC functions:
	- LogAnimInfo
	- LogSkinInfo
	- WriteScriptFile
	- FixRootMotion

***************************************************************************/

//
// Max includes
//
#include "SceneIFC.h"
#include "ActorX.h"


int	SceneIFC::LogAnimInfo(VActor *Thing, char* AnimName)
{	
	char LogFileName[MAX_PATH]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");
	sprintf(LogFileName,("\\X_AnimInfo_%s%s"),AnimName,".LOG");	
	DLog.Open( LogPath, LogFileName,DOLOGFILE);
	
   	DLog.Logf("\n\n Unreal skeletal exporter for 3DS Max - Animation information for [%s.psa] \n\n",AnimName );	
	// DLog.Logf(" Frames: %i\n", Thing->BetaNumFrames);
	// DLog.Logf(" Time Total: %f\n seconds", Thing->FrameTimeInfo * 0.001 );
	DLog.Logf(" Bones: %i\n", Thing->RefSkeletonBones.Num() );	
	DLog.Logf(" \n\n");

	for( INT i=0; i< Thing->OutAnims.Num(); i++)
	{
		DLog.Logf(" * Animation sequence %s %4i  Tracktime: %6f rate: %6f \n", Thing->OutAnims[i].AnimInfo.Name, i, (FLOAT)Thing->OutAnims[i].AnimInfo.TrackTime, (FLOAT)Thing->OutAnims[i].AnimInfo.AnimRate);
		DLog.Logf("   First raw frame  %i  total raw frames %i  group: [%s]\n", Thing->OutAnims[i].AnimInfo.FirstRawFrame, Thing->OutAnims[i].AnimInfo.NumRawFrames,Thing->OutAnims[i].AnimInfo.Group);
		DLog.Logf("   Rot/translation track keys: %i   Animated scale keys: %i \n\n", Thing->OutAnims[i].KeyTrack.Num(), Thing->OutAnims[i].ScaleTrack.Num() );
	}
	DLog.Close();

	return 1;
}


// Log skin data including materials to file.
int	SceneIFC::LogSkinInfo( VActor *Thing, char* SkinName ) // char* ModelName )
{
	if( !LogPath[0] )
	{
		return 1; // Prevent logs getting lost in root.
	}

	char LogFileName[MAX_PATH];
	sprintf(LogFileName,"\\X_ModelInfo_%s%s",SkinName,(".LOG"));
	
	DLog.Open(LogPath,LogFileName,DOLOGFILE);

	if( DLog.Error() ) return 0;

   	DLog.Logf("\n\n Unreal skeletal exporter for 3DS MAX - Model information for [%s.psk] \n\n",SkinName );	   	

	DLog.Logf(" Skin faces: %6i\n",Thing->SkinData.Faces.Num());
	DLog.Logf(" Skin vertices: %6i\n",Thing->SkinData.Points.Num());
	DLog.Logf(" Skin wedges (vertices with unique U,V): %6i\n",Thing->SkinData.Wedges.Num());
	DLog.Logf(" Total bone-to-vertex linkups: %6i\n",Thing->SkinData.RawWeights.Num());
	DLog.Logf(" Reference bones: %6i\n",Thing->RefSkeletonBones.Num());
	DLog.Logf(" Unique materials: %6i\n",Thing->SkinData.Materials.Num());
	DLog.Logf("\n = materials =\n");	

	// Print out smoothing groups ?
	if(0)
	for( INT f=0; f< Thing->SkinData.Faces.Num(); f++)
	{
		DLog.Logf(" Smoothing group for face %i is  [ %d ]  [%X] \n",f,Thing->SkinData.Faces[f].SmoothingGroups,Thing->SkinData.Faces[f].SmoothingGroups );
	}

	DebugBox("Start printing skins");

	for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{
		DebugBox("Start printing skin [%i]",i);

		char MaterialName[256];
		_tcscpy(MaterialName,CleanString( Thing->SkinData.Materials[i].MaterialName ));

		DebugBox("Start printing bitmap");

		// Find bitmap:		
		char BitmapName[MAX_PATH];
		char BitmapPath[MAX_PATH];

		if( (Thing->SkinData.RawMaterials.Num()>i) && Thing->SkinData.RawMaterials[i] )
		{
			_tcscpy( BitmapName,Thing->SkinData.RawBMPaths[i].RawBitmapName );  
			_tcscpy( BitmapPath,Thing->SkinData.RawBMPaths[i].RawBitmapPath );  
		}
		else
		{
			sprintf(BitmapName,"[Internal material]");
			BitmapPath[MAX_PATH];
		}

		DebugBox("Retrieved bitmapname");
		// Log.
		DLog.Logf(" * Index: [%2i]  name: %s  \n",i, MaterialName);
									 if( BitmapName[0] ) DLog.Logf("   Original bitmap: %s  Path: %s\n ",BitmapName,BitmapPath);
			                         DLog.Logf("   - Skin Index: %2i\n",Thing->SkinData.Materials[i].TextureIndex );
									 

		DebugBox("End printing bitmap");

		DWORD Flags = Thing->SkinData.Materials[i].PolyFlags;
		DLog.Logf("   - render mode flags:\n");
		if( Flags & MTT_NormalTwoSided ) DLog.Logf("		- Twosided\n");
		if( Flags & MTT_Unlit          ) DLog.Logf("		- Unlit\n");
		if( Flags & MTT_Translucent    ) DLog.Logf("		- Translucent\n");
		if( (Flags & MTT_Masked) == MTT_Masked ) 
										 DLog.Logf("		- Masked\n");
		if( Flags & MTT_Modulate       ) DLog.Logf("		- Modulated\n");
		if( Flags & MTT_Placeholder    ) DLog.Logf("		- Invisible\n");
		if( Flags & MTT_Alpha          ) DLog.Logf("		- Per-pixel alpha.\n");
		//if( Flags & MTT_Flat           ) DLog.Logf("		- Flat\n");
		if( Flags & MTT_Environment    ) DLog.Logf("		- Environment mapped\n");
		if( Flags & MTT_NoSmooth       ) DLog.Logf("		- Nosmooth\n");
	}


    //#DEBUG print out smoothing groups
	/*
	for( INT f=0; f < Thing->SkinData.Faces.Num(); f++ )
	{
		DLog.Logf(" Face [%4i] Smoothing group: %4i  mat: %4i \n ", f, Thing->SkinData.Faces[f].SmoothingGroups, Thing->SkinData.Faces[f].MatIndex );
	}
	*/

	// Print out bones list
	DLog.Logf(" \n\n");
	DLog.Logf(" BONE LIST        [%i] total bones in reference skeleton. \n",Thing->RefSkeletonBones.Num());

	    DLog.Logf("  number     name                           (parent) \n");

	for( INT b=0; b < Thing->RefSkeletonBones.Num(); b++)
	{
		char BoneName[65];
		_tcscpy( BoneName, Thing->RefSkeletonBones[b].Name );
		DLog.Logf("  [%3i ]    [%s]      (%3i ) \n",b,BoneName, Thing->RefSkeletonBones[b].ParentIndex );
	}
	
	DLog.Logf(" \n\n");
	DLog.Close();
	return 1;
}



//
// Output a template .uc file.....
//
//int	SceneIFC::WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName ) 
int	SceneIFC::WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName ) 
{		
	if( !LogPath[0] )
	{
		return 1; // Prevent logs getting lost in root. 
	}
	
	char LogFileName[MAX_PATH];
	sprintf(LogFileName,"\\%s%s",ScriptName,".uc");

	DLog.Open(LogPath,LogFileName,1);
	if( DLog.Error() ) return 0;

	//PopupBox("Assigning script names...");
	char MeshName[200];
	char AnimName[200];
	sprintf(MeshName,"%s%s",ScriptName,"Mesh");
	sprintf(AnimName,"%s%s",ScriptName,"Anims");

	//PopupBox(" Writing header lines.");

	DLog.Logf("//===============================================================================\n");
   	DLog.Logf("//  [%s] \n",ScriptName );	   	
	DLog.Logf("//===============================================================================\n\n");

	//PopupBox(" Writing import #execs.");

	DLog.Logf("class %s extends %s;\n",ScriptName,BaseName );

	DLog.Logf("#exec MESH  MODELIMPORT MESH=%s MODELFILE=models\\%s.PSK LODSTYLE=10\n", MeshName, SkinFileName );
	DLog.Logf("#exec MESH  ORIGIN MESH=%s X=0 Y=0 Z=0 YAW=0 PITCH=0 ROLL=0\n", MeshName );
	DLog.Logf("#exec ANIM  IMPORT ANIM=%s ANIMFILE=models\\%s.PSA COMPRESS=1 MAXKEYS=999999", AnimName, AnimFileName );
	if (!DoExplicitSequences) DLog.Logf(" IMPORTSEQS=1" );
	DLog.Logf("\n");
	DLog.Logf("#exec MESHMAP   SCALE MESHMAP=%s X=1.0 Y=1.0 Z=1.0\n", MeshName );
	DLog.Logf("#exec MESH  DEFAULTANIM MESH=%s ANIM=%s\n",MeshName,AnimName );
	
	// Explicit sequences?
	if( DoExplicitSequences )
	{
		// PopupBox(" Writing explicit sequence info.");
		DLog.Logf("\n// Animation sequences. These can replace or override the implicit (exporter-defined) sequences.\n");
		for( INT i=0; i< Thing->OutAnims.Num(); i++)
		{
			// Prepare strings from implicit sequence info; default strings when none present.
			DLog.Logf("#EXEC ANIM  SEQUENCE ANIM=%s SEQ=%s STARTFRAME=%i NUMFRAMES=%i RATE=%.4f COMPRESS=%.2f GROUP=%s \n",
				AnimName, 
				Thing->OutAnims[i].AnimInfo.Name, 
				Thing->OutAnims[i].AnimInfo.FirstRawFrame, 
				Thing->OutAnims[i].AnimInfo.NumRawFrames,
				(FLOAT)Thing->OutAnims[i].AnimInfo.AnimRate,
				(FLOAT)Thing->OutAnims[i].AnimInfo.KeyReduction,
				Thing->OutAnims[i].AnimInfo.Group );
		}
	}

	// PopupBox(" Writing Digest #exec.");
	// Digest
	DLog.Logf("\n");
	DLog.Logf("// Digest and compress the animation data. Must come after the sequence declarations.\n");
	DLog.Logf("// 'VERBOSE' gives more debugging info in UCC.log \n");
	DLog.Logf("#exec ANIM DIGEST ANIM=%s %s VERBOSE", AnimName, DoExplicitSequences ? "" : "USERAWINFO" );
	DLog.Logf("\n\n");

	//
	// TEXTURES/SETTEXTURES. 
	//

	//
	// - Determine unique textures - these are linked up with a number of (max) materials which may be larger.	
	// - extract the main names from textures and append PCX names
	//
	// - SkinIndex: what's it do exactly: force right index into the textures, for a certain material.
	//   Not to be confused with a material index.
	//

	// Determine unique textures, update pointers to them.
	// Order determined by skin indices; let these rearrange the textures if at all possible.
	TArray<VMaterial> UniqueMaterials;

	{for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{				
		char BitmapName[MAX_PATH];
		char BitmapPath[MAX_PATH];
		_tcscpy(BitmapName,Thing->SkinData.RawBMPaths[i].RawBitmapName);
		_tcscpy(BitmapPath,Thing->SkinData.RawBMPaths[i].RawBitmapPath);

		if( DoTexPCX ) // force PCX texture extension
		{
			// Not elegant...
			int j = ( int )strlen(BitmapName);
			if( (j>4) && (BitmapName[j-4] == char(".")))
			{								
				BitmapName[j-3] = 112;//char("p"); // 112
				BitmapName[j-2] =  99;//char("c"); //  99
				BitmapName[j-1] = 120;//char("x"); // 120
			}			
		}

		char NumStr[10];
		char TexName[MAX_PATH];
		_itoa(i,NumStr,10);
		sprintf(TexName,"%s%s%s",ScriptName,"Tex",NumStr);
			
		DLog.Logf("#EXEC TEXTURE IMPORT NAME=%s  FILE=TEXTURES\\%s  GROUP=Skins\n",TexName,BitmapName);
	}}
	DLog.Logf("\n");

	{for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{
		char NumStr[10];
		_itoa(i,NumStr,10);
		// Override default skin number with TextureIndex:
		char TexName[100];
		sprintf(TexName,"%s%s%s",ScriptName,"Tex",NumStr);
		DLog.Logf("#EXEC MESHMAP SETTEXTURE MESHMAP=%s NUM=%i TEXTURE=%s\n",MeshName,i,TexName);
	}}
	DLog.Logf("\n");

	// Full material list for debugging purposes.
	{for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{
		char BitmapName[MAX_PATH];
		char BitmapPath[MAX_PATH];
		_tcscpy(BitmapName,Thing->SkinData.RawBMPaths[i].RawBitmapName);
		_tcscpy(BitmapPath,Thing->SkinData.RawBMPaths[i].RawBitmapPath);		
		DLog.Logf("// Original material [%i] is [%s] SkinIndex: %i Bitmap: %s  Path: %s \n",i,Thing->SkinData.Materials[i].MaterialName,Thing->SkinData.Materials[i].TextureIndex,BitmapName,BitmapPath );
		
	}}
	DLog.Logf("\n\n");

	// defaultproperties
	DLog.Logf("defaultproperties\n{\n");
	DLog.Logf("    Mesh=%s\n",MeshName);
	DLog.Logf("    DrawType=DT_Mesh\n");
	DLog.Logf("    bStatic=False\n");
	DLog.Logf("}\n\n");

	DLog.Close();
	return 1;
}


//
// Extract root motion... to be called AFTER DigestAnim has been called.
// 'Default' =  make end and start line up linearly;
// 'Locked' =  set it all to the exact position of frame 0.
//
// 'Rotation' => should be used to S-lerp the rotation so that that matches up automatically also ->
// do this later, part of an automatic kind of 'welding' of looping anims...>> 3dsmax should probably
// be used for this though.
//

void SceneIFC::FixRootMotion(VActor *Thing) //, BOOL Fix, BOOL Lock )
{

	if( ! DoFixRoot ) 
		return;
	
	// Regular linear motion fixup
	if( ! DoLockRoot )
	{
		FVector StartPos = Thing->RawAnimKeys[0].Position;
		FVector EndPos   = Thing->RawAnimKeys[(OurTotalFrames-1)*OurBoneTotal].Position;

		for(int n=0; n<OurTotalFrames; n++)
		{
			float Alpha = ((float)n)/(float(OurTotalFrames-1));
			Thing->RawAnimKeys[n*OurBoneTotal].Position += Alpha*(StartPos - EndPos); // aligns end position onto start.
		}
	}

	// Motion LOCK
	if( DoLockRoot )
	{
		FVector StartPos = Thing->RawAnimKeys[0].Position;
		FVector EndPos   = Thing->RawAnimKeys[(OurTotalFrames-1)*OurBoneTotal].Position;

		for(int n=0; n<OurTotalFrames; n++)
		{
			//float Alpha = ((float)n)/(float(OurTotalFrames-1));
			Thing->RawAnimKeys[n*OurBoneTotal].Position = StartPos; //Locked
		}
	}
}



//
// Animation range parser - based on code by Steve Sinclair, Digital Extremes
//
void SceneIFC::ParseFrameRange(char *pString,INT startFrame,INT endFrame )
{
	char*	pSeek;
	char*	pPos;
	int		lastPos = 0;
	int		curPos = 0;
	UBOOL	rangeStart=false;
	int		start=0;
	int		val;
	char    LocalString[600];

	FrameList.StartFrame = startFrame;
	FrameList.EndFrame   = endFrame;

	_tcscpy(LocalString, pString);

	FrameList.Empty();

	pPos = pSeek = LocalString;
	
	if (!LocalString || LocalString[0]==0) // export it all if nothing was specified.
	{
		for(INT i=startFrame; i<=endFrame; i++)
		{
			FrameList.AddFrame(i);
		}
		return;
	}

	while (1)
	{
		if (*pSeek=='-')
		{
			*pSeek=0; // terminate this position for atoi
			pSeek++;
			// Eliminate multiple -'s
			while(*pSeek=='-')
			{
				*pSeek=0;
				pSeek++;
			}
			start = atoi( pPos );							
			pPos = pSeek;			
			rangeStart=true;
		}
		
		if (*pSeek==',')
		{
			*pSeek=0; // terminate this position for atoi
			pSeek++;			
			val = atoi( pPos );			
			pPos = pSeek;			
			// Single frame or end of range?
			if( rangeStart )
			{
				// Allow reversed ranges.
				if( val<start)
					for( INT i=start; i>=val; i--)
					{
						FrameList.AddFrame(i);								
					}
				else
					for( INT i=start; i<=val; i++)
					{
						FrameList.AddFrame(i);												
					}
			}
			else
			{
				FrameList.AddFrame(val); 
			}

			rangeStart=false;
		}
		
		if (*pSeek==0) // Real end of string.
		{
			if (pSeek!=pPos)
			{
				val = atoi ( pPos );
				if( rangeStart )
				{	
					// Allow reversed ranges.
					if( val<start)
						for( INT i=start; i>=val; i--)
						{
							FrameList.AddFrame(i);								
						}
					else
						for( INT i=start; i<=val; i++)
						{
							FrameList.AddFrame(i);												
						}
				}
				else
				{
					FrameList.AddFrame(val); 
				}
			}
			break;
		}

		pSeek++;
	}
}





//
// Apply smoothing groups as saved per face to the end result by raw-power-vertex duplication.
//


struct tFaceRecord
{
	INT FaceIndex;
	INT HoekIndex;
	INT WedgeIndex;
	DWORD SmoothFlags;
	DWORD FanFlags;
};

struct VertsFans
{	
	TArray<tFaceRecord> FaceRecord;
	INT FanGroupCount;
};

struct tInfluences
{
	TArray<INT> RawInfIndices;
};

struct tWedgeList
{
	TArray<INT> WedgeList;
};

struct tFaceSet
{
	TArray<INT> Faces;
};


TArray <FLOAT> FaceDeterminants;


// Check whether faces have at least two vertices in common. These must be POINTS - don't care about wedges.
UBOOL FacesAreSmoothlyConnected( VActor *Thing, INT Face1, INT Face2 )
{
	
	//if( ( Face1 >= Thing->SkinData.Faces.Num()) || ( Face2 >= Thing->SkinData.Faces.Num()) ) return false;

	if( Face1 == Face2 )
		return true;

	// Smoothing groups match at least one bit in binary AND ?
	if( ( Thing->SkinData.Faces[Face1].SmoothingGroups & Thing->SkinData.Faces[Face2].SmoothingGroups ) == 0  ) 
	{
		return false;
	}

	if( FaceDeterminants.Num() )  // Ignore if not prepared ( i.e. smoothing groop splitting only. )
	{
		// Are the determinants of different sign ? 
		// Then there is 'handedness' flip in the UV mapping spatial vector, and we have to maintain a seam.
		if( FaceDeterminants[Face1] * FaceDeterminants[Face2] < 0.f )
			return false;
	}

	INT VertMatches = 0;
	for( INT i=0; i<3; i++)
	{
		INT Point1 = Thing->SkinData.Wedges[ Thing->SkinData.Faces[Face1].WedgeIndex[i] ].PointIndex;

		for( INT j=0; j<3; j++)
		{
			INT Point2 = Thing->SkinData.Wedges[ Thing->SkinData.Faces[Face2].WedgeIndex[j] ].PointIndex;
			if( Point2 == Point1 )
			{
				VertMatches ++;
			}
		}
	}

	return ( VertMatches >= 2 );
}





int	SceneIFC::DoUnSmoothVerts(VActor *Thing, INT DoTangentVectorSeams  )
{
	//
	// Special Spherical-harmonics Tangent-space criterium -> unsmooth boundaries where there is
	// 

	FaceDeterminants.Empty();
		
	// Compute face tangents. Face tangent determinants whose signs don't match should not have smoothing between them (i.e. require
	// duplicated vertices.

	if( DoTangentVectorSeams )
	{
		FaceDeterminants.AddZeroed( Thing->SkinData.Faces.Num());

		for( INT FaceIndex=0; FaceIndex< Thing->SkinData.Faces.Num(); FaceIndex++)
		{

			FVector	P1 = Thing->SkinData.Points[ Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[0] ].PointIndex ].Point;
			FVector	P2 = Thing->SkinData.Points[ Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[1] ].PointIndex ].Point;
			FVector	P3 = Thing->SkinData.Points[ Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[2] ].PointIndex ].Point;

			FVector	TriangleNormal = FPlane(P3,P2,P1);
			FMatrix	ParameterToLocal(
				FPlane(	P2.X - P1.X,	P2.Y - P1.Y,	P2.Z - P1.Z,	0	),
				FPlane(	P3.X - P1.X,	P3.Y - P1.Y,	P3.Z - P1.Z,	0	),
				FPlane(	P1.X,			P1.Y,			P1.Z,			0	),
				FPlane(	0,				0,				0,				1	)
				);


			FLOAT T1U= Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[0] ].UV.U;
			FLOAT T1V= Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[0] ].UV.V;

			FLOAT T2U= Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[1] ].UV.U;
			FLOAT T2V= Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[1] ].UV.V;

			FLOAT T3U= Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[2] ].UV.U;
			FLOAT T3V= Thing->SkinData.Wedges[ Thing->SkinData.Faces[FaceIndex].WedgeIndex[2] ].UV.V;
					

			FMatrix	ParameterToTexture(
				FPlane(	T2U - T1U,	T2V - T1V,	0,	0	),
				FPlane(	T3U - T1U,	T3V - T1V,	0,	0	),
				FPlane(	T1U,		T1V,		1,	0	),
				FPlane(	0,			0,			0,	1	)
				);

			FMatrix	TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;
			FVector	TangentX = TextureToLocal.TransformNormal(FVector(1,0,0)).SafeNormal(),
					TangentY = TextureToLocal.TransformNormal(FVector(0,1,0)).SafeNormal(),
					TangentZ;

			TangentX = TangentX - TriangleNormal * (TangentX | TriangleNormal);
			TangentY = TangentY - TriangleNormal * (TangentY | TriangleNormal);
			TangentZ = TriangleNormal;

			TangentX.SafeNormal();
			TangentY.SafeNormal();
			TangentZ.SafeNormal();

			FMatrix TangentBasis( TangentX, TangentY, TangentZ );

			TangentBasis.Transpose(); //

			FaceDeterminants[FaceIndex] = TangentBasis.Determinant3x3();
		}
	}
		

	//
	// Connectivity: triangles with non-matching smoothing groups will be physically split.
	//
	// -> Splitting involves: the UV+material-contaning vertex AND the 3d point.
	//
	// -> Tally smoothing groups for each and every (textured) vertex.
	//
	// -> Collapse: 
	// -> start from a vertex and all its adjacent triangles - go over
	// each triangle - if any connecting one (sharing more than one vertex) gives a smoothing match,
	// accumulate it. Then IF more than one resulting section, 
	// ensure each boundary 'vert' is split _if not already_ to give each smoothing group
	// independence from all others.
	//

	int DuplicatedVertCount = 0;
	int RemappedHoeks = 0;

	int TotalSmoothMatches = 0;
	int TotalConnexChex = 0;

	// Link _all_ faces to vertices.	
	TArray<VertsFans>  Fans;
	TArray<tInfluences> PointInfluences;	
	TArray<tWedgeList>  PointWedges;

	Fans.AddExactZeroed(            Thing->SkinData.Points.Num() );
	PointInfluences.AddExactZeroed( Thing->SkinData.Points.Num() );
	PointWedges.AddExactZeroed(     Thing->SkinData.Points.Num() );

	{for(INT i=0; i< Thing->SkinData.RawWeights.Num(); i++)
	{
		PointInfluences[ Thing->SkinData.RawWeights[i].PointIndex ].RawInfIndices.AddItem( i );
	}}

	{for(INT i=0; i< Thing->SkinData.Wedges.Num(); i++)
	{
		PointWedges[ Thing->SkinData.Wedges[i].PointIndex ].WedgeList.AddItem( i );
	}}

	for(INT f=0; f< Thing->SkinData.Faces.Num(); f++ )
	{
		// For each face, add a pointer to that face into the Fans[vertex].
		for( INT i=0; i<3; i++)
		{
			INT WedgeIndex = Thing->SkinData.Faces[f].WedgeIndex[i];			
			INT PointIndex = Thing->SkinData.Wedges[ WedgeIndex ].PointIndex;
		    tFaceRecord NewFR;
			
			NewFR.FaceIndex = f;
			NewFR.HoekIndex = i;			
			NewFR.WedgeIndex = WedgeIndex; // This face touches the point courtesy of Wedges[Wedgeindex].
			NewFR.SmoothFlags = Thing->SkinData.Faces[f].SmoothingGroups;
			NewFR.FanFlags = 0;
			Fans[ PointIndex ].FaceRecord.AddItem( NewFR );
			Fans[ PointIndex ].FanGroupCount = 0;
		}		
	}

	// Investigate connectivity and assign common group numbers (1..+) to the fans' individual FanFlags.
	for( INT p=0; p< Fans.Num(); p++) // The fan of faces for each 3d point 'p'.
	{
		// All faces connecting.
		if( Fans[p].FaceRecord.Num() > 0 ) 
		{		
			INT FacesProcessed = 0;
			TArray<tFaceSet> FaceSets; // Sets with indices INTO FANS, not into face array.			

			// Digest all faces connected to this vertex (p) into one or more smooth sets. only need to check 
			// all faces MINUS one..
			while( FacesProcessed < Fans[p].FaceRecord.Num()  )
			{
				// One loop per group. For the current ThisFaceIndex, tally all truly connected ones
				// and put them in a new Tarray. Once no more can be connected, stop.

				INT NewSetIndex = FaceSets.Num(); // 0 to start
				FaceSets.AddZeroed(1);			            // first one will be just ThisFaceIndex.

				// Find the first non-processed face. There will be at least one.
				INT ThisFaceFanIndex = 0;
				{
					INT SearchIndex = 0;
					while( Fans[p].FaceRecord[SearchIndex].FanFlags == -1 ) // -1 indicates already  processed. 
					{
						SearchIndex++;
					}
					ThisFaceFanIndex = SearchIndex; //Fans[p].FaceRecord[SearchIndex].FaceIndex; 
				}

				// Inital face.
				FaceSets[ NewSetIndex ].Faces.AddItem( ThisFaceFanIndex );   // Add the unprocessed Face index to the "local smoothing group" [NewSetIndex].
				Fans[p].FaceRecord[ThisFaceFanIndex].FanFlags = -1;              // Mark as processed.
				FacesProcessed++; 

				// Find all faces connected to this face, and if there's any
				// smoothing group matches, put it in current face set and mark it as processed;
				// until no more match. 
				INT NewMatches = 0;
				do
				{
					NewMatches = 0;
					// Go over all current faces in this faceset and set if the FaceRecord (local smoothing groups) has any matches.
					// there will be at least one face already in this faceset - the first face in the fan.
					for( INT n=0; n< FaceSets[NewSetIndex].Faces.Num(); n++)
					{				
						INT HookFaceIdx = Fans[p].FaceRecord[ FaceSets[NewSetIndex].Faces[n] ].FaceIndex;

						//Go over the fan looking for matches.
						for( INT s=0; s< Fans[p].FaceRecord.Num(); s++)
						{
							// Skip if same face, skip if face already processed.
							if( ( HookFaceIdx != Fans[p].FaceRecord[s].FaceIndex )  && ( Fans[p].FaceRecord[s].FanFlags != -1  ))
							{
								TotalConnexChex++;
								// Process if connected with more than one vertex, AND smooth..
								if( FacesAreSmoothlyConnected( Thing, HookFaceIdx, Fans[p].FaceRecord[s].FaceIndex ) )
								{									
									TotalSmoothMatches++;
									Fans[p].FaceRecord[s].FanFlags = -1; // Mark as processed.
									FacesProcessed++;
									// Add 
									FaceSets[NewSetIndex].Faces.AddItem( s ); // Store FAN index of this face index into smoothing group's faces. 
									// Tally
									NewMatches++;
								}
							} // not the same...
						}// all faces in fan
					} // all faces in FaceSet
				}while( NewMatches );	
								
			}// Repeat until all faces processed.

			// For the new non-initialized  face sets, 
			// Create a new point, influences, and uv-vertex(-ices) for all individual FanFlag groups with an index of 2+ and also remap
			// the face's vertex into those new ones.
			if( FaceSets.Num() > 1 )
			{
				for( INT f=1; f<FaceSets.Num(); f++ )
				{				
					// We duplicate the current vertex. (3d point)
					INT NewPointIndex = Thing->SkinData.Points.Num();
					Thing->SkinData.Points.AddItem( Thing->SkinData.Points[p] );
	
					DuplicatedVertCount++;
					
					// Duplicate all related weights.
					for( INT t=0; t< PointInfluences[p].RawInfIndices.Num(); t++ )
					{
						// Add new weight
						INT NewWeightIndex = Thing->SkinData.RawWeights.Num();
						Thing->SkinData.RawWeights.AddItem( Thing->SkinData.RawWeights[ PointInfluences[p].RawInfIndices[t] ] );
						Thing->SkinData.RawWeights[NewWeightIndex].PointIndex = NewPointIndex;
					}
					
					// Duplicate any and all Wedges associated with it; and all Faces' wedges involved.					
					for( INT w=0; w< PointWedges[p].WedgeList.Num(); w++)
					{						
						INT OldWedgeIndex = PointWedges[p].WedgeList[w];
						INT NewWedgeIndex = Thing->SkinData.Wedges.Num();
						Thing->SkinData.Wedges.AddItem( Thing->SkinData.Wedges[ OldWedgeIndex ] );
						Thing->SkinData.Wedges[ NewWedgeIndex ].PointIndex = NewPointIndex; 

						//  Update relevant face's Wedges. Inelegant: just check all associated faces for every new wedge.
						for( INT s=0; s< FaceSets[f].Faces.Num(); s++)
						{
							INT FanIndex = FaceSets[f].Faces[s];
							if( Fans[p].FaceRecord[ FanIndex ].WedgeIndex == OldWedgeIndex )
							{
								// Update just the right one for this face (HoekIndex!) 
								Thing->SkinData.Faces[ Fans[p].FaceRecord[ FanIndex].FaceIndex ].WedgeIndex[ Fans[p].FaceRecord[ FanIndex ].HoekIndex ] = NewWedgeIndex;
								RemappedHoeks++;
							}
						}
					}
				}
			} //  if FaceSets.Num(). -> duplicate stuff
		}//	while( FacesProcessed < Fans[p].FaceRecord.Num() )
	} // Fans for each 3d point

	return DuplicatedVertCount; 
}





