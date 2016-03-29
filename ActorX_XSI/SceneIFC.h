// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

   SceneIFC.h	- File Exporter class definitions.

***************************************************************************/

#ifndef __SceneIFC__H
#define __SceneIFC__H

//===========================================
// Build-specific includes:

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#include "UnSkeletal.h"
#include "Vertebrate.h"

class SceneIFC;

extern VActor       TempActor;

extern TextFile DLog;
extern TextFile AuxLog;
extern TextFile MemLog;

extern char DestPath[MAX_PATH];
extern char LogPath[MAX_PATH];
extern char to_path[MAX_PATH];
extern char to_meshoutpath[MAX_PATH];
extern char to_animfile[MAX_PATH];
extern char to_skinfile[MAX_PATH];

extern char to_pathvtx[MAX_PATH];
extern char to_skinfilevtx[MAX_PATH];

extern char animname[MAX_PATH];
extern char framerangestring[MAXINPUTCHARS];
extern char classname[MAX_PATH];
extern char basename[MAX_PATH];
extern char batchfoldername[MAX_PATH];
extern char vertframerangestring[MAXINPUTCHARS];


//===========================================


class NodeInfo
{
public:
	AXNode*  node;
	int		IsBone;   // in the sense of a valid hierarchical node for physique...
	int     IsCulled;
	int		IsBiped;
	int		IsRoot;
	int     IsSmooth; // Any smooth-skinned mesh.
	int     IsSkin;
	int     IsDummy;
	int	  IsPointHelper;
	int	  IsMaxBone;
	int     LinksToSkin;
	int     HasMesh;
	int     HasTexture;
	int     IsSelected;
	int     IsRootBone;
	int     NoExport; // untie links from NoExport bones..

	int     InSkeleton;
	int     ModifierIdx; // Modifier for this node (Maya skincluster usage)
	int     IgnoreSubTree;

	// These are enough to recover the hierarchy.
	int  	ParentIndex;
	int     NumChildren;

	int     RepresentsBone;

	//ctor
	NodeInfo()
	{
		Memzero(this,sizeof(NodeInfo));
	}
	~NodeInfo()
	{
	}
};



// Frame index sequence

class FrameSeq
{
public:
	INT			StartFrame;
	INT			EndFrame;
	TArray<INT> Frames;

	FrameSeq()
	{
		Memzero(this,sizeof(FrameSeq));
	}

	void AddFrame(INT i)
	{
		if( i>=StartFrame && i<=EndFrame) Frames.AddItem(i);
	}

	INT GetFrame(INT index)
	{
		return Frames[index];
	}

	INT GetTotal()
	{
		return Frames.Num();
	}

	void Empty()
	{
		Frames.Empty();
	}
};


struct SkinInf
{
	AXNode*	Node;
	INT		IsSmoothSkinned;
	INT		IsTextured;
	INT		SeparateMesh;
	INT		SceneIndex;
	INT     IsVertexAnimated;
	INT     IsPhysique;
	INT     IsMaxSkin;
};

//
// Yet another dynamic array string implementation...
//
class XString
{
public:

	TArray<CHAR> StringArray;
	CHAR		 NullChar;

	INT Length()
	{
		return StringArray.Num();
	}

	void EmptyString()
	{
		StringArray.Empty();
	}

	INT CopyFrom( const CHAR* Source )
	{
		StringArray.Empty();
		INT Index = 0;
		NullChar = 0;
		if( Source != NULL)	while( Index < 1024 && ( Source[Index] != 0) )
		{
			StringArray.AddItem( Source[Index] );
			Index++;
		}
		return Length();
	}

	CHAR* StringPtr()
	{
		if( StringArray.Num() )
		{
			// Assure last char is a 0...
			if( StringArray[StringArray.Num()-1] != 0 )
			{
				StringArray.AddZeroed(1);
			}
			return &(StringArray[0]);
		}
		else
		{
			NullChar = 0;
			return &NullChar;
		}
	}

	// ctors
	XString()
	{
		Memzero( this, sizeof( XString ));
	}
	XString(CHAR* SourceString)
	{
		CopyFrom( SourceString );
	}
	~XString()
	{
		EmptyString();
	}

};

//
// Mapping element
//
class GMap
{
	INT    MaterialIndex;
	FLOAT  U;
	FLOAT  V;
};



//
// Static mesh triangle.
//
class GFace
{
	public:

	INT     WedgeIndex[3];	     			// point to three vertices in the vertex list.
	INT     Wedge2Index[3];				// point to three vertices in the (second UV set) vertex list.
	DWORD   SmoothingGroups;			// 32-bit flag for smoothing groups AND Lod-bias calculation.
	DWORD   AuxSmoothingGroups;         //
	TArray<INT> MaterialIndices;	    // Materials can be anything.

	GFace()
	{
		Memzero( this, sizeof( GFace));
	}

	~GFace()
	{
		MaterialIndices.Empty();
	}

	void operator=( GFace& G )
	{
		WedgeIndex[0] = G.WedgeIndex[0];
		WedgeIndex[1] = G.WedgeIndex[1];
		WedgeIndex[2] = G.WedgeIndex[2];

		Wedge2Index[0] = G.Wedge2Index[0];
		Wedge2Index[1] = G.Wedge2Index[1];
		Wedge2Index[2] = G.Wedge2Index[2];

		SmoothingGroups = G.SmoothingGroups;
		AuxSmoothingGroups = G.AuxSmoothingGroups;

		MaterialIndices.Empty();
		for( INT mIndex = 0; mIndex<G.MaterialIndices.Num(); mIndex++)
		{
			MaterialIndices.AddItem( G.MaterialIndices[mIndex] );
		}
	}
};
//
// Static mesh vertex with 'unlimited' texturing info.
//
class GWedge
{
	public:
	INT				PointIndex;	 // Index to a point.
	FLOAT U;
	FLOAT V;
	INT   MaterialIndex;
};

//
// Per-vertex color property.
//
class GColor
{
	public:
	FLOAT R,G,B,A;
	GColor()
	{
		R=0.0f;
		G=0.0f;
		B=0.0f;
		A=1.0f;
	}
	GColor( FLOAT ColorR, FLOAT ColorG, FLOAT ColorB, FLOAT ColorA)
	{
		R = ColorR;
		G= ColorG;
		B = ColorB;
		A = ColorA;
	}
};

// Static mesh material.
//
class GMaterial
{
	public:
	XString Name;       // Name we'll export it with.
	XString BitmapName; //
	FLOAT   Opacity;    //
	FLOAT   R,G,B;      // possible color

	GMaterial()
	{
		Memzero( this, sizeof(GMaterial) );
	}
	~GMaterial()
	{
		Name.EmptyString();
		BitmapName.EmptyString();
	}
};

class PrimGroupList
{
	public:
	TArray<INT> Groups;

	PrimGroupList()
	{
		Memzero( this, sizeof( PrimGroupList));
	}
	~PrimGroupList()
	{
		Groups.Empty();
	}

	void operator=( PrimGroupList& P )
	{
		Groups.Empty();
		for( INT i=0; i<P.Groups.Num(); i++)
		{
			Groups.AddItem( P.Groups[i] );
		}
	}

};

// Static mesh export.
class GeometryPrimitive
{

public:
	AXNode* node;         // Original scene node.
	XString   Name;       //
	INT  Selected;

	TArray<FVector>	Vertices;
	TArray<GWedge>	Wedges;
	TArray<GFace>	Faces;
	TArray<GWedge>	Wedges2;
	TArray<GColor>		VertColors;

	TArray<PrimGroupList> FaceSmoothingGroups; // Local smoothing groups.

	// ctor
	GeometryPrimitive()
	{
		Memzero( this, sizeof( GeometryPrimitive ));
	}
	// dtor
	~GeometryPrimitive()
	{
		for(INT i=0; i< FaceSmoothingGroups.Num(); i++)
		{
			FaceSmoothingGroups[i].Groups.Empty();
		}
		FaceSmoothingGroups.Empty();

		for(INT f=0; f< Faces.Num(); f++)
		{
			Faces[f].MaterialIndices.Empty();
		}
		Faces.Empty();

		Wedges.Empty();
		Vertices.Empty();
		VertColors.Empty();
		Wedges2.Empty();

		Name.EmptyString();
	}
};

class SceneIFC
{
public:

	void		SurveyScene();
	int			GetSceneInfo();

	int         DigestSkeleton(VActor *Thing);
	int         DigestSkin( VActor *Thing );
	int         DigestBrush( VActor *Thing );
	int         FixMaterials( VActor *Thing ); // internal
	int	        ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin, SkinInf* SkinHandle );
	int         LogSkinInfo( VActor *Thing, char* SkinName);
	int			WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName );
	int         DigestAnim(VActor *Thing, char* AnimName, char* RangeString );
	int         LogAnimInfo( VActor *Thing, char* AnimName);
	void        FixRootMotion( VActor *Thing );
	int	        DoUnSmoothVerts(VActor *Thing, INT DoTangentVectorSeams );
	int			WriteVertexAnims( VActor *Thing, char* DestFileName, char* RangeString );

	int			MarkBonesOfSystem(int RIndex);
	int         RecurseValidBones(int RIndex, int &BoneCount);

    // Modifier*   FindPhysiqueModifier(AXNode* nodePtr);
	int			EvaluateBone(int RIndex);
	// int         HasMesh(AXNode* node);
	int         HasTexture(AXNode* node);
	int         EvaluateSkeleton(INT RequireBones);
	int         DigestMaterial(AXNode *node, INT matIndex, TArray<void*> &MatList);

	// Static mesh export
	int         DigestStaticMeshes();
	int         ProcessStaticMesh( int TreeIndex );
	int         SaveStaticMeshes( char* OutFileName );
	int         ConsolidateStaticPrimitives( GeometryPrimitive* ResultPrimitive );

	//
	// Data
	//
	int         TreeInitialized;

		// Static meshes.
	TArray< GeometryPrimitive >  StaticPrimitives;
	TArray< GMaterial >          StaticMeshMaterials;

	// Skeletal meshes.
	AXNode*			 OurSkin;
	TArray <SkinInf> OurSkins;
	AXNode*      OurRootBone;
	int         RootBoneIndex;
	int         OurBoneTotal;

	// Simple array of nodes for the whole scene tree.
	TArray< NodeInfo > SerialTree;

	// Physique skin data
	int         TotalSkinNodeNum;

	// Skeletal links to skin:
	int         LinkedBones;
	int         TotalSkinLinks;

	// Misc evaluation counters
	int         GeomMeshes;
	int         Hierarchies;
	int         TotalBones;
	int         TotalDummies;
	int         TotalBipBones;
	int         TotalMaxBones;
	// More stats:
	int         TotalVerts;
	int         TotalFaces;
	int         TotalPointHelpers;
	int         DuplicateBones;

	// Animation
	//TimeValue   TimeStatic; // for evaluating any objects at '(0)' reference.
	//TimeValue	FrameRange;
	//TimeValue	FrameStart;
	//TimeValue	FrameEnd;

	FLOAT   TimeStatic; // for evaluating any objects at '(0)' reference.
	FLOAT	FrameRange;
	FLOAT	FrameStart;
	FLOAT	FrameEnd;

	int			FrameTicks;
	int         OurTotalFrames;
	FLOAT       FrameRate; // frames per second !

	FrameSeq    FrameList; // Total frames == FrameList.Num()

	// Export switches from 'aux' panel.
	UBOOL	DoFixRoot, DoLockRoot, DoExplicitSequences, DoLog, DoUnSmooth, DoPoseZero, CheckPersistSettings, CheckPersistPaths, DoTangents;
	UBOOL   DoTexGeom, DoSelectedGeom, DoSkipSelectedGeom, DoSkinGeom, DoCullDummies, DoExportScale;
	UBOOL   DoTexPCX, InBatchMode;
	UBOOL   DoSmooth, DoSingleTex;

	UBOOL   DoAppendVertex, DoScaleVertex;

	FLOAT   VertexExportScale;

		// Static mesh export panel
	UBOOL   DoConvertSmooth, DoUnderscoreSpace, DoGeomAsFilename, DoUntexturedAsCollision, DoSuppressPopups, DoSelectedStatic, DoConsolidateGeometry;

	bool ExportingStaticMesh;

	// ctor/dtor
	SceneIFC()
	{
		Memzero(this,sizeof(SceneIFC));
		TimeStatic = 1.0f; //0.0f ?!?!?
		DoLog = 1;
		DoUnSmooth = 1;
		DoExportScale = 0;
		DoSkinGeom = 1;
		DoTexPCX = 1;
		ExportingStaticMesh = false;
	}
	void Cleanup()
	{
		FrameList.Empty();
		SerialTree.Empty();
		OurSkins.Empty();

		StaticPrimitives.Empty();
		StaticMeshMaterials.Empty();
		ExportingStaticMesh = false;
	}
	~SceneIFC()
	{
		Cleanup();
	}


	void GetAuxSwitchesFromRegistry(  WinRegistry& PluginReg )
	{

		PluginReg.GetKeyValue("PERSISTSETTINGS",CheckPersistSettings);
		PluginReg.GetKeyValue("PERSISTPATHS",CheckPersistPaths );

		PluginReg.GetKeyValue("DOPCX", DoTexPCX);
		PluginReg.GetKeyValue("DOFIXROOT",DoFixRoot );
		PluginReg.GetKeyValue("DOCULLDUMMIES",DoCullDummies );
		PluginReg.GetKeyValue("DOTEXGEOM",DoTexGeom );
		PluginReg.GetKeyValue("DOPHYSGEOM",DoSkinGeom );
		PluginReg.GetKeyValue("DOSELGEOM",DoSelectedGeom);
		PluginReg.GetKeyValue("DOSKIPSEL",DoSkipSelectedGeom);
		PluginReg.GetKeyValue("DOEXPSEQ",DoExplicitSequences);
		PluginReg.GetKeyValue("DOLOG",DoLog);
		PluginReg.GetKeyValue("DOUNSMOOTH",DoUnSmooth);
		PluginReg.GetKeyValue("DOEXPORTSCALE",DoExportScale);
		PluginReg.GetKeyValue("DOTANGENTS",DoTangents);
		PluginReg.GetKeyValue("DOAPPENDVERTEX",DoAppendVertex);
		PluginReg.GetKeyValue("DOSCALEVERTEX",DoScaleVertex);

		if( CheckPersistPaths )
		{
			PluginReg.GetKeyString("TOPATH", to_path );
			_tcscpy(LogPath,to_path);
		}

	}

	void GetStaticMeshSwitchesFromRegistry(  WinRegistry& PluginReg )
	{

		PluginReg.GetKeyValue("DOCONVERTSMOOTH",DoConvertSmooth);
		PluginReg.GetKeyValue("DOUNDERSCORESPACE",DoUnderscoreSpace);
		PluginReg.GetKeyValue("DOGEOMASFILENAME",DoGeomAsFilename);
		PluginReg.GetKeyValue("DOSELECTEDSTATIC",DoUntexturedAsCollision);
		PluginReg.GetKeyValue("DOSUPPRESSPOPUPS",DoSuppressPopups);
		PluginReg.GetKeyValue("DOSELECTEDSTATIC",DoSelectedStatic);
		PluginReg.GetKeyValue("DOCONSOLIDATE",DoConsolidateGeometry);

		PluginReg.GetKeyValue("PERSISTPATHS",CheckPersistPaths );
		if( CheckPersistPaths )
		{
			PluginReg.GetKeyString("TOMESHOUTPATH", to_meshoutpath );
		}
	}

	int	MatchNodeToIndex(AXNode* ANode)
	{
		for (int t=0; t<SerialTree.Num(); t++)
		{
			if ( (void*)(SerialTree[t].node) == (void*)ANode) return t;
		}
		return -1; // no matching node found.
	}

	int GetChildNodeIndex(int PIndex,int ChildNumber)
	{
		int Sibling = 0;
		for (int t=0; t<SerialTree.Num(); t++)
		{
			if (SerialTree[t].ParentIndex == PIndex)
			{
				if( Sibling == ChildNumber )
				{
					return t; // return Nth child.
				}
				Sibling++;
			}
		}
		return -1; // no matching node found.
	}

	int	    GetParentNodeIndex(int CIndex)
	{
		return SerialTree[CIndex].ParentIndex; //-1 if none
	}

	private:
	void		StoreNodeTree(AXNode* node);
	int			SerializeSceneTree();
	void        ParseFrameRange(char* pString, INT startFrame,INT endFrame);

};


#endif // __SceneIFC__H
