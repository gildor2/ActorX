// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

   SceneIFC.h	- File Exporter class definitions.

***************************************************************************/

#ifndef __SceneIFC__H
#define __SceneIFC__H

//===========================================
// Build-specific includes:


#include "UnSkeletal.h"
#include "Vertebrate.h"

class SceneIFC;

extern SceneIFC		OurScene;
extern VActor       TempActor;

extern TextFile DLog;
extern TextFile AuxLog;
extern TextFile MemLog;

extern TCHAR PluginRegPath[];
extern TCHAR DestPath[MAX_PATH];
extern TCHAR LogPath[MAX_PATH];
extern TCHAR to_path[MAX_PATH];
extern TCHAR to_animfile[MAX_PATH];
extern TCHAR to_skinfile[MAX_PATH];

extern TCHAR to_pathvtx[MAX_PATH];
extern TCHAR to_skinfilevtx[MAX_PATH];

extern TCHAR newsequencename[MAX_PATH];
extern TCHAR framerangestring[MAXINPUTCHARS];
extern TCHAR classname[MAX_PATH];
extern TCHAR basename[MAX_PATH];
extern TCHAR batchfoldername[MAX_PATH];
extern TCHAR vertframerangestring[MAXINPUTCHARS];


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
	int         LogSkinInfo( VActor *Thing, TCHAR* SkinName);
	int			WriteScriptFile( VActor *Thing, TCHAR* ScriptName, TCHAR* BaseName, TCHAR* SkinFileName, TCHAR* AnimFileName );
	int         DigestAnim(VActor *Thing, TCHAR* AnimName, TCHAR* RangeString );
	int         LogAnimInfo( VActor *Thing, TCHAR* AnimName);
	void        FixRootMotion( VActor *Thing );
	int	        DoUnSmoothVerts(VActor *Thing, INT DoTangentVectorSeams );
	int			WriteVertexAnims( VActor *Thing, TCHAR* DestFileName, TCHAR* RangeString );

	int			MarkBonesOfSystem(int RIndex);
	int         RecurseValidBones(int RIndex, int &BoneCount);

    // Modifier*   FindPhysiqueModifier(AXNode* nodePtr);
	int			EvaluateBone(int RIndex);
	// int         HasMesh(AXNode* node);
	int         HasTexture(AXNode* node);
	int         EvaluateSkeleton(INT RequireBones);
	int         DigestMaterial(AXNode *node, INT matIndex, TArray<void*> &MatList);

	//
	// Data
	//
	int         TreeInitialized;

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
	UBOOL   DoTexPCX, DoQuadTex, InBatchMode;
	UBOOL   DoSmooth, DoSingleTex;

	UBOOL   DoAppendVertex, DoScaleVertex;

	UBOOL	DoSuppressAnimPopups;
	UBOOL   DoForceRate;

	FLOAT   PersistentRate;
	FLOAT   VertexExportScale;

	// ctor/dtor
	SceneIFC()
	{
		Memzero(this,sizeof(SceneIFC));
		TimeStatic = 1.0f; //0.0f ?!?!?
		DoLog = 1;
		DoUnSmooth = 1;
		DoTangents = 0;
		DoExportScale = 0;
		DoSkinGeom = 1;
		DoTexPCX = 1;
		DoForceRate = 0;
	}
	void Cleanup()
	{
		FrameList.Empty();
		SerialTree.Empty();
		OurSkins.Empty();
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
	void        ParseFrameRange(TCHAR* pString, INT startFrame,INT endFrame);

};


#endif // __SceneIFC__H
