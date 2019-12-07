// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

   SceneIFC.h	- File Exporter class definitions.

***************************************************************************/

#ifndef __SceneIFC__H
#define __SceneIFC__H

//===========================================
// Build-specific includes:

#include "MayaInclude.h"
#include "UnSkeletal.h"
#include "Vertebrate.h"

class SceneIFC;

extern SceneIFC		OurScene;
extern VActor       TempActor;

extern TextFile DLog;
extern TextFile AuxLog;
extern TextFile MemLog;

//extern char PluginRegPath[];
extern char DestPath[MAX_PATH];
extern char LogPath[MAX_PATH];
extern char to_path[MAX_PATH];
extern char to_meshoutpath[MAX_PATH];
extern char to_animfile[MAX_PATH];
extern char to_skinfile[MAX_PATH];

extern char newsequencename[MAX_PATH];
extern char framerangestring[MAXINPUTCHARS];
extern char classname[MAX_PATH];
extern char basename[MAX_PATH];
extern char batchfoldername[MAX_PATH];
extern char vertframerangestring[MAXINPUTCHARS];

extern INT 	ExportBlendShapes;

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
	int     IsMaxBone;
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

	double _rotation[3];
	double _translation[3];
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


class SkinInf
{
	public:
	AXNode*	Node;
	INT		IsSmoothSkinned;
	INT		IsTextured;
	INT		SeparateMesh;
	INT		SceneIndex;
	INT     IsVertexAnimated;
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
	int	        ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin);
	int         LogSkinInfo( VActor *Thing, const char* SkinName);
	int			WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName );
	int         DigestAnim(VActor *Thing, char* AnimName, char* RangeString );
	int 		InitializeCurve(VActor *Thing, INT TotalFrameCount );
	int         LogAnimInfo( VActor *Thing, char* AnimName);
	void        FixRootMotion( VActor *Thing );
	int	        DoUnSmoothVerts(VActor *Thing, INT DoTangentVectorSeams );
	int			WriteVertexAnims( VActor *Thing, char* DestFileName, char* RangeString );

	int			MarkBonesOfSystem(int RIndex);
	int         RecurseValidBones(int RIndex, int &BoneCount);

	int			EvaluateBone(int RIndex);
	int         EvaluateSkeleton(INT RequireBones);

	// Static mesh export
	int         DigestStaticMeshes();
	int         ProcessStaticMesh( int TreeIndex);
	int         SaveStaticMeshes( char* OutFileName );
	//PCF BEGIN
	int         SaveStaticMesh( char* OutFileName, INT );
	// //PCF END
	int         ConsolidateStaticPrimitives( GeometryPrimitive* ResultPrimitive );

	//
	// Data
	//
	int         TreeInitialized;

	// Static meshes.
	TArray< GeometryPrimitive >  StaticPrimitives;
	TArray< GMaterial >          StaticMeshMaterials;

	// Skeletal meshes.
//	int			OurSkin;
	TArray <SkinInf> OurSkins;
	AXNode*      OurRootBone;
	int         RootBoneIndex;
	// LH: deprecating this variable -
	// This is almost identical as TotalBones except
	// this does not include noexport/ignored parameter
	// but used to calculate wrong size of data for keytrack.
	// This will be replaced with TotalBones
	int         OurBoneTotal;

	// Simple array of nodes for the whole scene tree.
	TArray< NodeInfo >  SerialTree;

	// Physique skin data
	int         PhysiqueNodes;

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
	int		  DuplicateBones;

	// Animation
	//TimeValue TimeStatic; // for evaluating any objects at '(0)' reference.
	//TimeValue	FrameRange;
	//TimeValue	FrameStart;
	//TimeValue	FrameEnd;

	FLOAT	TimeStatic; // for evaluating any objects at '(0)' reference.
	FLOAT	FrameRange;
	FLOAT	FrameStart;
	FLOAT	FrameEnd;

	int			FrameTicks;
	int         OurTotalFrames;
	FLOAT       FrameRate; // frames per second !

	FrameSeq    FrameList; // Total frames == FrameList.Num()

	// Export switches from 'aux' panel.
	UBOOL	DoFixRoot, DoLockRoot, DoExplicitSequences, DoLog, DoUnSmooth, DoPoseZero, CheckPersistSettings, CheckPersistPaths, DoTangents;
	UBOOL   DoTexGeom, DoSelectedGeom, DoSkipSelectedGeom, DoPhysGeom, DoCullDummies, DoHierBones;
	UBOOL   DoTexPCX, DoQuadTex, DoVertexOut, InBatchMode;
	UBOOL   DoSmoothBrush, DoSingleTex;
	UBOOL   DoAutoTriangles, DoSuppressAnimPopups, DoBakeAnims;

	/** User preference that controls whether or not we strip reference file prefixes from node names */
	UBOOL	bCheckStripRef;

	// Static mesh export panel
	UBOOL   DoConvertSmooth, DoForceTriangles, DoUnderscoreSpace, DoGeomAsFilename, DoUntexturedAsCollision, DoSuppressPopups, DoSelectedStatic, DoConsolidateGeometry;

	UBOOL   QuickSaveDisk;
	UBOOL   DoForceRate;

	UBOOL   DoAppendVertex, DoScaleVertex, DoReplaceUnderscores;

	FLOAT   PersistentRate;
	FLOAT   VertexExportScale;

	//PCF BEGIN

	UBOOL DoExportInObjectSpace ; //object space export for static mesh

	UBOOL DoExportTextureSuffix; //"_Mat" suffix for correct auto import

	UBOOL DoSecondUVSetSkinned;// export second uvset for skinned

	//PCF END

	// ctor/dtor
	SceneIFC()
	{
		Memzero(this,sizeof(SceneIFC));
		TimeStatic = 1.0f;   //0.0f ?!?!?
		DoLog = 1;
		DoUnSmooth = 1;
		DoTangents = 0;
		DoTexPCX = 0;
		DoPhysGeom = 1;
		DoConsolidateGeometry = 0;
		VertexExportScale = 1.0;
		DoExportInObjectSpace =0;
		DoExportTextureSuffix =0;
		DoSecondUVSetSkinned =0;

	}

	void Cleanup()
	{
		FrameList.Empty();
		SerialTree.Empty();
		OurSkins.Empty();

		StaticPrimitives.Empty();
		StaticMeshMaterials.Empty();
	}

	~SceneIFC()
	{
		Cleanup();
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

	/**
	 * Removes the file path and the file extension from a full file path.
	 * The only thing returned is the name of the file.
	 * This is used to remove the reference name from nodes in a scene
	 *
	 * @param	InFullFilePath		The path and file name to clean
	 *
	 * @return	File name with no path or extension
	 */
	MString	StripFilePathAndExtension( const MString& InFullFilePath );

	/**
	 * Returns the name of the specified node, minus any reference prefix (if configured to strip that)
	 *
	 * @param	DagNode		The node to return the 'clean' name of
	 *
	 * @return	The node name, minus any reference prefix
	 */
	MString GetCleanNameForNode( MFnDagNode& DagNode );


};


#endif // __SceneIFC__H
