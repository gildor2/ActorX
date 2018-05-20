// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**********************************************************************

	Vertebrate.h - Binary structures for digesting and exporting skeleton, skin & animation data.

	Created by Erik de Neve

	New model limits (imposed by the binary format and/or engine)
		256		materials.
		65535  'wedges' = amounts to approx 20000 triangles/vertices, depending on texturing complexity.
	A realistic upper limit, with LOD in mind, is 10000 wedges.

	Todo:
	 Max smoothing group support ?
	-> Unreal's old non-lod code had structures in place for all adjacent triangles-to-a-vertex,
	   to compute the vertex normal.
    -> LODMeshes only needs the connectivity of each triangle to its three vertices.
	-> Facet-shading as opposed to gouraud/vertex shading would be a very nice option.
	-> Normals should be exported per influence & transformed along with vertices for faster drawing/lighting pipeline in UT ?!

************************************************************************/

#ifndef VERTHDR_H
#define VERTHDR_H

#include "DynArray.h"
#include "Win32IO.h"

#include <assert.h>

// version 1: 1999801 "1 august 99" _or_ 0 means Version 1.0.
// version 2: "21  march 03" means version 2.0
// versino 3: "27 jan 09" is version 3.0  - Added 0 before 1 (so 01 not 1 for january) for future concern
// Maya has higher version, but not max as max version does not support blend shape yet.
#define PSA_VERSION	2003321


// Enforce the default packing, entrenched in the exporter/importer code for PSA & PSK files...
#pragma pack( push, 4)

// Bitflags describing effects for a (classic Unreal & Unreal Tournament) mesh triangle.
enum EJSMeshTriType
{
	// Triangle types. Pick ONE AND ONLY ONE of these.
	MTT_Normal				= 0x00,	// Normal one-sided.
	MTT_NormalTwoSided      = 0x01,    // Normal but two-sided.
	MTT_Translucent			= 0x02,	// Translucent two-sided.
	MTT_Masked				= 0x03,	// Masked two-sided.
	MTT_Modulate			= 0x04,	// Modulation blended two-sided.
	MTT_Placeholder			= 0x08,	// Placeholder triangle for positioning weapon. Invisible.

	// Bit flags. Add any of these you want.
	MTT_Unlit				= 0x10,	// Full brightness, no lighting.
	MTT_Flat				= 0x20,	// Flat surface, don't do bMeshCurvy thing.
	MTT_Alpha				= 0x20,	// This material has per-pixel alpha.
	MTT_Environment			= 0x40,	// Environment mapped.
	MTT_NoSmooth			= 0x80,	// No bilinear filtering on this poly's texture.

};

// Unreal engine internal / T3D polyflags
enum EPolyFlags
{
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
	PF_AlphaTexture		= 0x00001000,	// Honor texture alpha (reuse BigWavy and SpecialPoly flags)
	PF_SmallWavy		= 0x00002000,	// Small wavy pattern (for water/enviro reflection).
	PF_DetailTexture	= 0x00002000,	// Render as detail texture.
	PF_Flat				= 0x00004000,	// Flat surface.
	PF_LowShadowDetail	= 0x00008000,	// Low detail shadows.
	PF_NoMerge			= 0x00010000,	// Don't merge poly's nodes before lighting when rendering.
	PF_SpecularBump		= 0x00020000,	// Poly is specular bump mapped.
	PF_DirtyShadows		= 0x00040000,	// Dirty shadows.
	PF_BrightCorners	= 0x00080000,	// Brighten convex corners.
	PF_SpecialLit		= 0x00100000,	// Only speciallit lights apply to this poly.
	PF_Gouraud			= 0x00200000,	// Gouraud shaded.
	PF_NoBoundRejection = 0x00200000,	// Disable bound rejection in OccludeBSP (reuse Gourard flag)
	PF_Wireframe		= 0x00200000,	// Render as wireframe
	PF_Unlit			= 0x00400000,	// Unlit.
	PF_HighShadowDetail	= 0x00800000,	// High detail shadows.
	PF_Portal			= 0x04000000,	// Portal between iZones.
	PF_Mirrored			= 0x08000000,	// Reflective surface.

	// Editor flags.
	PF_Memorized     	= 0x01000000,	// Editor: Poly is remembered.
	PF_Selected      	= 0x02000000,	// Editor: Poly is selected.
	PF_Highlighted      = 0x10000000,	// Editor: Poly is highlighted.
	PF_FlatShaded		= 0x40000000,	// FPoly has been split by SplitPolyWithPlane.

	// Internal.
	PF_EdProcessed 		= 0x40000000,	// FPoly was already processed in editorBuildFPolys.
	PF_EdCut       		= 0x80000000,	// FPoly has been split by SplitPolyWithPlane.
	PF_RenderFog		= 0x40000000,	// Render with fogmapping.
	PF_Occlude			= 0x80000000,	// Occludes even if PF_NoOcclude.
	PF_RenderHint       = 0x01000000,   // Rendering optimization hint.

	// Combinations of flags.
	PF_NoOcclude		= PF_Masked | PF_Translucent | PF_Invisible | PF_Modulated | PF_AlphaTexture,
	PF_NoEdit			= PF_Memorized | PF_Selected | PF_EdProcessed | PF_NoMerge | PF_EdCut,
	PF_NoImport			= PF_NoEdit | PF_NoMerge | PF_Memorized | PF_Selected | PF_EdProcessed | PF_EdCut,
	PF_AddLast			= PF_Semisolid | PF_NotSolid,
	PF_NoAddToBSP		= PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized,
	PF_NoShadows		= PF_Unlit | PF_Invisible | PF_Environment | PF_FakeBackdrop,
	PF_Transient		= PF_Highlighted,
};

// The number of additional supported UV sets besides the manditory 1
#define NUM_EXTRA_UV_SETS 3

//
// Most of these structs are mirrored in Unreal's "UnSkeletal.h" and need to stay binary compatible with Unreal's
// skeletal data import routines.
//

// File header structure.
struct VChunkHdr
{
	char		ChunkID[20];  // string ID of up to 19 chars (usually zero-terminated?)
	int			TypeFlag;     // Flags/reserved/Version number...
    int         DataSize;     // size per struct following;
	int         DataCount;    // number of structs/
};


// A Material
struct VMaterial
{
	char		MaterialName[64]; // Straightforward ascii array, for binary input.
	int         TextureIndex;     // multi/sub texture index
	DWORD		PolyFlags;        // all poly's with THIS material will have this flag.
	int         AuxMaterial;      // index into another material, eg. alpha/detailtexture/shininess/whatever
	DWORD		AuxFlags;		  // reserved: auxiliary flags
	INT			LodBias;          // material-specific lod bias
	INT			LodStyle;         // material-specific lod style


	VMaterial()
	{
		Memzero( this, sizeof( VMaterial ) );
	}

	// Necessary to determine material uniqueness
	UBOOL operator==( const VMaterial& M ) const
	{
		UBOOL Match = true;
		if (TextureIndex != M.TextureIndex) Match = false;
		if (PolyFlags != M.PolyFlags) Match = false;
		if (AuxMaterial != M.AuxMaterial) Match = false;
		if (AuxFlags != M.AuxFlags) Match = false;
		if (LodBias != M.LodBias) Match = false;
		if (LodStyle != M.LodStyle) Match = false;

		for(INT c=0; c<64; c++)
		{
			if( MaterialName[c] != M.MaterialName[c] )
			{
				Match = false;
				break;
			}
		}
		return Match;
	}

	// Copy a name and properly zero-terminate it.
	void SetName( const TCHAR* NewName)
	{
		memset(MaterialName, 0, 64);
	#if _UNICODE
		wcstombs(MaterialName, NewName, 64);
	#else
		strncpy(MaterialName, NewName, 64);
	#endif
		MaterialName[63] = 0;
	}
};

struct VBitmapOrigin
{
	TCHAR RawBitmapName[MAX_PATH];
	TCHAR RawBitmapPath[MAX_PATH];

	VBitmapOrigin()
	{
		Memzero(RawBitmapName, MAX_PATH);
		Memzero(RawBitmapPath, MAX_PATH);
	}
};

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat   	Orientation;  //
	FVector		Position;     //  needed or not ?

	FLOAT       Length;       //  For collision testing / debugging drawing...
	FLOAT       XSize;	      //
	FLOAT       YSize;        //
	FLOAT       ZSize;        //
};

struct FNamedBoneBinary
{
	char	   Name[64];     // ANSICHAR   Name[64];	// Bone's name
	DWORD      Flags;		 // reserved
	INT        NumChildren;  //
	INT		   ParentIndex;	 // 0/NULL if this is the root bone.
	VJointPos  BonePos;	     //


	FNamedBoneBinary()
	{
		Memzero( this, sizeof( FNamedBoneBinary ) );
	}
};

struct AnimInfoBinary
{
	ANSICHAR Name[64];     // Animation's name
	ANSICHAR Group[64];    // Animation's group name

	INT TotalBones;           // TotalBones * NumRawFrames is number of animation keys to digest.

	INT ScaleInclude;         // if 1, scaling keys will follow in the scale key chunk for this sequence.  Old name: "RootInclude".
	INT KeyCompressionStyle;  // Reserved: variants in tradeoffs for compression.
	INT KeyQuotum;            // Max key quotum for compression
	FLOAT KeyReduction;       // desired
	FLOAT TrackTime;          // explicit - can be overridden by the animation rate
	FLOAT AnimRate;           // frames per second.
	INT StartBone;            // - Reserved: for partial animations.
	INT FirstRawFrame;        //
	INT NumRawFrames;         //


	AnimInfoBinary()
	{
		Memzero( this, sizeof( AnimInfoBinary ) );
	}
};

struct VQuatAnimKey
{
	FVector		Position;           // relative to parent.
	FQuat       Orientation;        // relative to parent.
	FLOAT       Time;				// The duration until the next key (end key wraps to first...)
};


struct VScaleAnimKey
{
	FVector ScaleVector;   // If uniform scaling is required, just use the X component..
	FLOAT   Time;          // disregarded
};

struct VBoneInfIndex // ,, ,, contains Index, number of influences per bone (+ N detail level sizers! ..)
{
	WORD WeightIndex;
	WORD Detail0;  // how many to process if we only handle 1 master bone per vertex.
	WORD Detail1;  // how many to process if we're up to 2 max influences
	WORD Detail2;  // how many to process if we're up to full 3 max influences

};

struct VBoneInfluence // Weight and vertex number
{
	WORD PointIndex; // 3d vertex
	WORD BoneWeight; // 0..1 scaled influence
};

struct VRawBoneInfluence // Just weight, vertex, and Bone, sorted later.
{
	FLOAT Weight;
	INT   PointIndex;
	INT   BoneIndex;
};

//
// Points: regular FVectors (for now..)
//
struct VPoint
{
	FVector			Point;             //  change into packed integer later IF necessary, for 3x size reduction...
};

//
// A simple color struct that matches FColor in UE3
//
struct VColor
{
	BYTE B,G,R,A;

	VColor( BYTE InB = 0, BYTE InG = 0, BYTE InR = 0, BYTE InA = 0 )
		: B(InB),G(InG),R(InR),A(InA)
	{

	}
};

//
// Simple struct for storing UV coords
//
struct FUVCoord
{
	float U,V;

	FUVCoord() : U(0.0f),V(0.0f)
	{

	}

	UBOOL operator==( const FUVCoord& Other ) const
	{
		return U == Other.U && V == Other.V;
	}

	UBOOL operator!=( const FUVCoord& Other ) const
	{
		return U != Other.U || V != Other.V;
	}
};


//
// Vertex with texturing info, akin to Hoppe's 'Wedge' concept.
//
struct VVertex
{
	WORD		PointIndex;	 // Index to a point.
	FUVCoord	UV;         // Engine may choose to store these as floats, words,bytes - but raw PSK file has full floats.
	// Vertex colors.  This will be exported as a separate list at the bottom of the PSK file to maintain backwards compatibility with different combinations of ActorX and UE3.
	// We store it here for now as wedges get re-arranged and moved around during the export process.  It is easier to keep track of them this way
	//support for 3 additional uv sets.  This will be exported as a separate list at the bottom of the PSK file to maintain backwards compatibility with different
	// combinations of ActorX and UE3
	FUVCoord	ExtraUVs[NUM_EXTRA_UV_SETS];
	VColor		Color;
	BYTE		MatIndex;    // At runtime, this one will be implied by the vertex that's pointing to us.
	BYTE		Reserved;    // Top secret.
};

// This is the VVertex struct that is exported to the PSK file.  It omits vertex color data and extra UV data since old versions of UE3 would size missmatch since they are expecting the vertex struct to be a specific size.
// We will export vertex colors and extra UV sets at the bottom of the file.
struct VVertexExport
{
	WORD		PointIndex;	 // Index to a point.
	FUVCoord    UV;          // Engine may choose to store these as floats, words,bytes - but raw PSK file has full floats.
	BYTE		MatIndex;    // At runtime, this one will be implied by the vertex that's pointing to us.
	BYTE		Reserved;    // Top secret.
};

//
// Textured triangle.
//
struct VTriangle
{
	WORD    WedgeIndex[3];	 // point to three vertices in the vertex list.
	BYTE    MatIndex;	     // Materials can be anything.
	BYTE    AuxMatIndex;     // Second material (eg. damage skin, shininess, detail texture / detail mesh...
	DWORD   SmoothingGroups; // 32-bit flag for smoothing groups AND Lod-bias calculation.
};


//
// Physique (or other) skin.
//
struct VSkin
{
	TArray <VMaterial>			Materials; // Materials
	TArray <VPoint>				Points;    // 3D Points
	TArray <VVertex>			Wedges;  // Wedges
	TArray <VTriangle>			Faces;       // Faces
	TArray <FNamedBoneBinary>   RefBones;   // reference skeleton
	TArray <void*>				RawMaterials; // alongside Materials - to access extra data //Mtl*
	TArray <VRawBoneInfluence>	RawWeights;  // Raw bone weights..
	TArray <VBitmapOrigin>      RawBMPaths;  // Full bitmap Paths for logging

	// Brushes: UV must embody the material size...
	TArray <INT>				MaterialUSize;
	TArray <INT>				MaterialVSize;

	int NumExtraUVSets; // Number of extra uv sets
	int NumBones; // Explicit number of bones (?)

	bool bHasVertexColors; // if we have vertex colors
};



//
// Complete Animation for a skeleton.
// Independent-sized key tracks for each part of the skeleton.
//
struct VAnimation
{
	AnimInfoBinary         AnimInfo;   //  Generic animation info as saved for the engine
	TArray <VQuatAnimKey>  KeyTrack;   //  Animation track - keys * bones
	TArray <VScaleAnimKey> ScaleTrack; //  Animated scaling track - keys * bones

	VAnimation()
	{
		Memzero(&AnimInfo,sizeof(AnimInfoBinary));
		//PopupBox("VAnimation constructor call... ");
	}

	// Copy.
	void operator=( VAnimation& V )
	{
		AnimInfo = V.AnimInfo;

		KeyTrack.Empty();
		for( INT k=0;k<V.KeyTrack.Num(); k++)
		{
			KeyTrack.AddItem( V.KeyTrack[k] );
		}

		ScaleTrack.Empty();
		for( INT s=0;s<V.ScaleTrack.Num(); s++)
		{
			ScaleTrack.AddItem( V.ScaleTrack[s] );
		}
	}

};



//
// Added 11/11/2002 - Classic Unreal ".3d" Vertex animation export support.
//
//  _d.3d  mesh file: VertexMeshHeader followed by NumPolygons VertexMeshTris.
//  _a.3d  anim file: VertexAnimHeader followed by raw ( Frames * FrameSize/sizeof(DWORD) ) vertices,
//                    where FrameSize matches the NumVertices from the _d.3d file.
//
//

struct FMeshByteUV
{
	BYTE U;
	BYTE V;
};

struct VertexMeshHeader
{
   WORD  NumPolygons;  // Polygon count.
   WORD  NumVertices;  // Vertex count.
   WORD  BogusRot;
   WORD  BogusFrame;
   DWORD   BogusNormX;
   DWORD   BogusNormY;
   DWORD   BogusNormZ;
   DWORD   FixScale;
   DWORD   Unused[3];
   BYTE    Unknown[12];
   // Padding issues ?
};

// James animation info.
struct VertexAnimHeader
{
	WORD	NumFrames;		// Number of animation frames.
	WORD	FrameSize;		// Size of one frame of animation data.( number of vertices * sizeof(DWORD) )
};

// Mesh triangle.
struct VertexMeshTri
{
	WORD		iVertex[3];		// Vertex indices.
	BYTE		Type;			// James Schmalz' mesh type.( 0 by default ?)
	BYTE		Color;			// Color for flat and Gouraud shaded - unused
	FMeshByteUV	Tex[3];			// Texture UV coordinates.
	BYTE		TextureNum;		// Source texture offset. ("material index")
	BYTE		Flags;			// Unreal mesh flags.
};


#pragma pack( pop )


// Compressed 3d data type.
#define GET_MESHVERT_DWORD(mv) (*(DWORD*)&(mv))
struct FMeshVert
{
	// Components (INTEL byte order assumed.)
	INT X:11; INT Y:11; INT Z:10;

	// Constructor.
	FMeshVert()
	{}

	FMeshVert( const FVector& In )
	: X((INT)In.X), Y((INT)In.Y), Z((INT)In.Z)
	{}

	// Functions.
	FVector Vector() const
	{
		return FVector( X, Y, Z );
	}
};



//
// The internal animation class, which can contain various amounts of
// (mesh+reference skeleton), and (skeletal animation sequences).
//
// Knows how to save a subset of the data as a valid Unreal2
// model/skin/skeleton/animation file.
//

class VActor
{
public:

	//
	// Some globals from the scene probe.
	//
	INT		NodeCount;
	INT     MeshCount;

	FLOAT	FrameTotalTicks;
	FLOAT   FrameRate;

	// file stuff
	char*	LogFileName;
	char*   OutFileName;

	// Multiple skins: because not all skins have to be Physique skins; and
	// we also want hierarchical actors/animation (robots etc.)

	VSkin             SkinData;

	// Single skeleton digestion (reference)
	TArray <FNamedBoneBinary>    RefSkeletonBones;
	TArray <AXNode*>		         RefSkeletonNodes;     // The node* array for each bone index....


	// Raw animation, 'RawAnimKeys' structure
	int                   RawNumBones;
	int                   RawNumFrames; //#debug
	TArray<VQuatAnimKey>  RawAnimKeys;
	TArray<VScaleAnimKey> RawScaleKeys;
	char                  RawAnimName[64];


	// Array of digested Animations - all same bone count, variable frame count.
	TArray  <VAnimation> Animations; //digested;
	TArray  <VAnimation> OutAnims;   //queued for output.
	int		             AnimationBoneNumber; // Bone number consistency check..


	int	MatchNodeToSkeletonIndex(AXNode* ANode)
	{
		for (int t=0; t<RefSkeletonBones.Num(); t++)
		{
			if ( RefSkeletonNodes[t] == ANode) return t;
		}
		return -1; // no matching node found.
	}

	int IsSameVertex( VVertex &A, VVertex &B, INT NumExtraUVSets )
	{
		if (A.PointIndex != B.PointIndex) return 0;
		if (A.MatIndex != B.MatIndex) return 0;

		if (A.UV != B.UV) return 0;

		for( INT UVSetIndex = 0; UVSetIndex < NumExtraUVSets; ++UVSetIndex )
		{
			if ( A.ExtraUVs[UVSetIndex] != B.ExtraUVs[UVSetIndex] )
			{
				return 0;
			}
		}

		return 1;
	}


	// Ctor
	VActor()
	{
		// Memzero(this,sizeof(VActor));
		// NumFrames = 0;
		NodeCount = 0;
		MeshCount = 0;
		AnimationBoneNumber = 0;

	};

	void Cleanup()
	{
		RawAnimKeys.Empty();
		RefSkeletonNodes.Empty();
		RefSkeletonBones.Empty();

		// Clean up all dynamic-array-allocated ones - can't count on automatic destructor calls for multi-dimension arrays...
		{for(INT i=0; i<Animations.Num(); i++)
		{
			Animations[i].KeyTrack.Empty();
			Animations[i].ScaleTrack.Empty();
		}}
		Animations.Empty();

		{for(INT i=0; i<OutAnims.Num(); i++)
		{
			OutAnims[i].KeyTrack.Empty();
			OutAnims[i].ScaleTrack.Empty();
		}}
		OutAnims.Empty();
	}

	~VActor()
	{
		Cleanup();
	}

	//
	// Save the actor: Physique mesh, plus reference skeleton.
	//

	bool SerializeActor(FastFileClass &OutFile)
	{
		if (!RefSkeletonBones.Num())
		{
			ErrorBox(_T("Actor does not have bones"));
			return false;
		}
		// Header
		VChunkHdr ChunkHdr;
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,"ACTRHEAD");
		ChunkHdr.DataCount = 0;
		ChunkHdr.DataSize  = 0;
		ChunkHdr.TypeFlag  = PSA_VERSION;
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		////////////////////////////////////////////

		//TCHAR MessagePopup[512];
		//sprintf(MessagePopup, "Writing Skin file, 3d vertices : %i",SkinData.Points.Num());
		//PopupBox(GetActiveWindow(),MessagePopup, "Saving", MB_OK);

		// Skin: 3D Points
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("PNTS0000"));
		ChunkHdr.DataCount = SkinData.Points.Num();
		ChunkHdr.DataSize  = sizeof ( VPoint );
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

		/////////////////////////////////////////////
		for( int i=0; i < (SkinData.Points.Num()); i++ )
		{
			OutFile.Write( &SkinData.Points[i], sizeof (VPoint));
		}

		// List of vertex colors to export
		TArray<VColor> VertexColors;
		TArray<FUVCoord> ExtraUVCoords[NUM_EXTRA_UV_SETS];

		// Skin: VERTICES (wedges)
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("VTXW0000"));
		ChunkHdr.DataCount = SkinData.Wedges.Num();
		ChunkHdr.DataSize  = sizeof (VVertexExport);
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		// Copy wedges to the export struct and put the color data in its own array for exporting later
		for( int i=0; i<SkinData.Wedges.Num(); i++)
		{
			VVertex& Vert = SkinData.Wedges[i];

			VVertexExport ExportVert;
			ExportVert.MatIndex = Vert.MatIndex;
			ExportVert.PointIndex = Vert.PointIndex;
			ExportVert.Reserved = Vert.Reserved;
			ExportVert.UV = Vert.UV;

			if( SkinData.bHasVertexColors )
			{
				VertexColors.AddItem( Vert.Color );
			}

			for( INT UVIndex = 0; UVIndex < SkinData.NumExtraUVSets ; ++UVIndex )
			{
				ExtraUVCoords[UVIndex].AddItem( Vert.ExtraUVs[UVIndex] );
			}


			OutFile.Write( &ExportVert, sizeof (VVertexExport));
		}

		// Skin: TRIANGLES (faces)
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("FACE0000"));
		ChunkHdr.DataCount = SkinData.Faces.Num();
		ChunkHdr.DataSize  = sizeof( VTriangle );
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		//
		for( int i=0; i<SkinData.Faces.Num(); i++)
		{
			OutFile.Write( &SkinData.Faces[i], sizeof (VTriangle));
		}

		// Skin: Materials
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("MATT0000"));
		ChunkHdr.DataCount = SkinData.Materials.Num();
		ChunkHdr.DataSize  = sizeof( VMaterial );
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		//
		for( int i=0; i<SkinData.Materials.Num(); i++)
		{
			OutFile.Write( &SkinData.Materials[i], sizeof (VMaterial));
		}

		// Reference Skeleton: Refskeleton.TotalBones times a VBone.
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("REFSKELT"));
		ChunkHdr.DataCount = RefSkeletonBones.Num();
		ChunkHdr.DataSize  = sizeof ( FNamedBoneBinary ) ;
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		//
		for( int i=0; i<RefSkeletonBones.Num(); i++)
		{
			OutFile.Write( &RefSkeletonBones[i], sizeof (FNamedBoneBinary));
		}

		// Reference Skeleton: Refskeleton.TotalBones times a VBone.
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("RAWWEIGHTS"));
		ChunkHdr.DataCount = SkinData.RawWeights.Num();
		ChunkHdr.DataSize  = sizeof ( VRawBoneInfluence ) ;
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

		for( int i=0; i< SkinData.RawWeights.Num(); i++)
		{
			OutFile.Write( &SkinData.RawWeights[i], sizeof (VRawBoneInfluence));
		}

		// SKIN: Vertex colors and extra UV's
		// NOTE: These *MUST* be the last thing written to the PSK file to maintain backwards compatibility with other versions of UE3 not importing vertex colors or extra UV sets
		// When older versions of UE3 read the psk file they will not read this data since they dont know its here.
		if( VertexColors.Num() > 0 )
		{
			Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
			strcpy( ChunkHdr.ChunkID, ("VERTEXCOLOR"));
			ChunkHdr.DataCount = VertexColors.Num();
			ChunkHdr.DataSize = sizeof( VColor );
			OutFile.Write( &ChunkHdr, sizeof(ChunkHdr) );

			for( int i=0; i< VertexColors.Num(); i++)
			{
				OutFile.Write( &VertexColors[i], sizeof (VColor));
			}
		}

		for( INT UVIndex = 0; UVIndex < SkinData.NumExtraUVSets; ++UVIndex )
		{
			Memzero( &ChunkHdr, sizeof( ChunkHdr ) );

			sprintf( ChunkHdr.ChunkID, ("EXTRAUVS%d"), UVIndex );
			ChunkHdr.DataCount = ExtraUVCoords[UVIndex].Num();
			ChunkHdr.DataSize = sizeof( FUVCoord );
			OutFile.Write( &ChunkHdr, sizeof(ChunkHdr) );

			for( int i=0; i< ExtraUVCoords[UVIndex].Num(); i++)
			{
				OutFile.Write( &ExtraUVCoords[UVIndex][i], sizeof (FUVCoord));
			}
		}

		return OutFile.GetError() == 0;
	};


	// Save the Output animations. ( 'Reference' skeleton is just all the bone names.)
	bool SerializeAnimation(FastFileClass &OutFile)
	{
		if (!RefSkeletonBones.Num())
		{
			ErrorBox(_T("Actor does not have bones"));
			return false;
		}
		// Header :
		VChunkHdr ChunkHdr;
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("ANIMHEAD"));
		ChunkHdr.DataCount = 0;
		ChunkHdr.DataSize  = 0;
		ChunkHdr.TypeFlag  = 2003321; // "21  march 03"
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

		// Bone names (+flags) list:
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("BONENAMES"));
		ChunkHdr.DataCount = RefSkeletonBones.Num();
		ChunkHdr.DataSize  = sizeof ( FNamedBoneBinary );
		OutFile.Write(&ChunkHdr, sizeof (ChunkHdr));
		for(int b = 0; b < RefSkeletonBones.Num(); b++)
		{
			OutFile.Write( &RefSkeletonBones[b], sizeof (FNamedBoneBinary) );
		}

		INT TotalAnimKeys = 0;
		INT TotalAnimFrames = 0;
		INT TotalScaleKeys = 0;
		// Add together all frames to get the count.
		for(INT i = 0; i<OutAnims.Num(); i++)
		{
			OutAnims[i].AnimInfo.FirstRawFrame = TotalAnimKeys / RefSkeletonBones.Num();
			TotalAnimKeys   += OutAnims[i].KeyTrack.Num();
			TotalAnimFrames += OutAnims[i].AnimInfo.NumRawFrames;
			TotalScaleKeys  += OutAnims[i].ScaleTrack.Num();
		}

		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("ANIMINFO"));
	    ChunkHdr.DataCount = OutAnims.Num();
		ChunkHdr.DataSize  = sizeof( AnimInfoBinary  ); // heap of angaxis/pos/length, 8 floats #debug
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		for( INT i = 0; i<OutAnims.Num(); i++)
		{
			OutFile.Write( &OutAnims[i].AnimInfo, sizeof( AnimInfoBinary ) );
		}

		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("ANIMKEYS"));
	    ChunkHdr.DataCount = TotalAnimKeys;            // RefSkeletonBones.Num() * RawNumFrames;
		ChunkHdr.DataSize  = sizeof( VQuatAnimKey );   // Heap of angaxis/pos/length, 8 floats #debug
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

		// Save out all in our 'digested' array.
		for( INT a = 0; a<OutAnims.Num(); a++ )
		{
			// Raw keys chunk....
			for( INT i=0; i<OutAnims[a].KeyTrack.Num(); i++)
			{
				OutFile.Write( &OutAnims[a].KeyTrack[i], sizeof ( VQuatAnimKey ) );
			}
		}

		// Scalers chunk.
		Memzero( &ChunkHdr, sizeof( ChunkHdr ) );
		strcpy(ChunkHdr.ChunkID,("SCALEKEYS"));
		ChunkHdr.DataCount = TotalScaleKeys;            // RefSkeletonBones.Num() * RawNumFrames;
		ChunkHdr.DataSize  = sizeof( VScaleAnimKey );   // Heap of angaxis/pos/length, 8 floats #debug
		OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

		// Optional: separate chunk with scaler keys.
		if( TotalScaleKeys )
		{
			// Save out all scaler keys.
			for( INT a = 0; a<OutAnims.Num(); a++ )
			{
				// Raw keys chunks will be written only for those sequences that have TempInfo.ScaleInclude == 1.
				for( INT i=0; i<OutAnims[a].ScaleTrack.Num(); i++)
				{
					OutFile.Write( &OutAnims[a].ScaleTrack[i], sizeof ( VScaleAnimKey ) );
				}
			}
		}

		return true;
	};

	// Load the Output animations. ( 'Reference' skeleton is just all the bone names.)
	int LoadAnimation(FastFileClass &InFile)
	{
		//
		// Animation layout:
		//
		// name        variable										type
		//
		// ANIMHEAD
		// BONENAMES   RefSkeletonBones								FNamedBoneBinary
		// ANIMINFO    OutAnims										AnimInfoBinary
		// ANIMKEYS    OutAnims[0-.KeyTrack.Num()].KeyTrack[i]....  VQuatAnimKey
		//

		// Animation header.
		VChunkHdr ChunkHdr;
		// Output error message if not found.
		INT ReadBytes = InFile.Read(&ChunkHdr,sizeof(ChunkHdr));

		INT PSAVersion = ChunkHdr.TypeFlag;

		UBOOL ScalerChunkExpected = ( PSAVersion >= 2003321 );

		// Bones
		InFile.Read(&ChunkHdr,sizeof(ChunkHdr));


		// SKIP the bones - relying on our scene to have consistent (number of) bones... #TODO: add error message if not consistent ?
#if 0
		RefSkeletonBones.Empty();
		RefSkeletonBones.Add( ChunkHdr.DataCount );
		for( INT i=0; i<RefSkeletonBones.Num(); i++ )
		{
			InFile.Read( &RefSkeletonBones[i], sizeof(FNamedBoneBinary) );
		}
		RefSkeletonBones.Empty();
#else
		// See https://udn.epicgames.com/lists/showpost.php?list=unprog&id=38289
		TArray<FNamedBoneBinary> TempBones;
		TempBones.Add( ChunkHdr.DataCount );
		for( INT i = 0 ; i < TempBones.Num() ; ++i )
		{
			InFile.Read( &TempBones[i], sizeof(FNamedBoneBinary) );

		}
#endif
		// Animation info
		InFile.Read(&ChunkHdr,sizeof(ChunkHdr));

		// Proper cleanup: de-linking of tracks! -> because they're 'an array of arrays' and our
		// dynamic array stuff is dumb and doesn't call individual element-destructors, which would usually carry this responsibility.
		for( INT i=0; i<OutAnims.Num();i++)
		{
			VAnimation& TempOutAnim = OutAnims[i];
			TempOutAnim.KeyTrack.Empty(); // Empty should really unlink any data !
			TempOutAnim.ScaleTrack.Empty();
		}
		OutAnims.Empty();
		OutAnims.AddExactZeroed( ChunkHdr.DataCount ); // Zeroed... necessary, the KeyTrack is a dynamic array.

		// AnimInfo chunks - per-sequence information.
		for( INT i = 0; i<OutAnims.Num(); i++)
		{
			InFile.Read( &OutAnims[i].AnimInfo, sizeof(AnimInfoBinary));
		}

		// Key tracks.
		InFile.Read(&ChunkHdr,sizeof(ChunkHdr));
		// verify if total matches read keys...
		INT TotalKeys = ChunkHdr.DataCount;
		INT ReadKeys = 0;

		//PopupBox(" Start loading Keytracks, number: %i OutAnims: %i ", ChunkHdr.DataCount , OutAnims.Num() );
		for( INT i = 0; i<OutAnims.Num(); i++)
		{
			INT TrackKeys = OutAnims[i].AnimInfo.NumRawFrames * OutAnims[i].AnimInfo.TotalBones;
			OutAnims[i].KeyTrack.Empty();
			OutAnims[i].KeyTrack.Add(TrackKeys);
			InFile.Read( &(OutAnims[i].KeyTrack[0]), TrackKeys * sizeof(VQuatAnimKey) );

			ReadKeys += TrackKeys;
		}

		// Scaler tracks.. if present -> checks main chunk type number for backward compatibility....
		if( ScalerChunkExpected )
		{
			InFile.Read(&ChunkHdr,sizeof(ChunkHdr));
			// verify if total matches read keys...
			INT TotalScaleKeys = ChunkHdr.DataCount;
			INT ReadScaleKeys = 0;

			if( TotalScaleKeys )
			{
				for( INT s = 0; s<OutAnims.Num(); s++)
				{
					OutAnims[s].ScaleTrack.Empty();
					if( OutAnims[s].AnimInfo.ScaleInclude ) // ONLY for those sequences with scaling.
					{
						INT TrackScaleKeys = OutAnims[s].AnimInfo.NumRawFrames * OutAnims[s].AnimInfo.TotalBones;
						if( ReadScaleKeys + TrackScaleKeys <= TotalScaleKeys ) // Enough on disk left to read ?!
						{
							OutAnims[s].ScaleTrack.Add( TrackScaleKeys );
							InFile.Read( &(OutAnims[s].ScaleTrack[0]), TrackScaleKeys * sizeof(VScaleAnimKey) );
							ReadScaleKeys += TrackScaleKeys;
						}
						else
						{
							TCHAR SeqName[65];
						#if _UNICODE
							mbstowcs( SeqName, OutAnims[s].AnimInfo.Name, 65 );
						#else
							_tcscpy( SeqName, OutAnims[s].AnimInfo.Name );
						#endif
							PopupBox(_T(" Error: more animation scaling keys were expected for sequence [%s]"), SeqName );
						}
					}
				}
			}
		}


		if( OutAnims.Num() &&  (AnimationBoneNumber > 0) && (AnimationBoneNumber != OutAnims[0].AnimInfo.TotalBones ) )
			PopupBox(_T(" ERROR !! Loaded animation bone number [%i] \n inconsistent with digested bone number [%i] "),OutAnims[0].AnimInfo.TotalBones,AnimationBoneNumber);

		return 1;
	}

	// Add a current 'RawAnimKeys' animation data to our TempActor's in-memory repertoire.
	int RecordAnimation()
	{
		AnimInfoBinary TempInfo;
		TempInfo.FirstRawFrame = 0; // Fixed up at write time
		TempInfo.NumRawFrames =  RawNumFrames; // RawAnimKeys.Num()
		TempInfo.StartBone = 0; //
		TempInfo.TotalBones = RawNumBones; // OurBoneTotal;
		TempInfo.TrackTime = RawNumFrames; // FrameRate;
		TempInfo.AnimRate =  FrameRate; // RawNumFrames;
		TempInfo.KeyReduction = 1.0;
		TempInfo.KeyQuotum = RawAnimKeys.Num(); // NumFrames * RawNumBones; //Set to full size...
		TempInfo.KeyCompressionStyle = 0;
		TempInfo.ScaleInclude = RawScaleKeys.Num() == ( RawNumFrames * RawNumBones ) ? 1:0; // Proper number of RawScaleKeys ? then digest

		if( RawNumFrames && RawNumBones )
		{
			INT ThisIndex = Animations.Num(); // Add to top of Animations (sequences) array.

			// Very necessary - see if we're adding inconsistent skeletons to the (raw) frame memory !
			if( ThisIndex==0) // First one to add...
			{
				AnimationBoneNumber = RawNumBones;
			}
			else if ( AnimationBoneNumber != RawNumBones )
			{
				PopupBox(_T("ERROR !! Inconsistent number of bones detected: %i instead of %i"),RawNumBones,AnimationBoneNumber );
				return 0;
			}

			Animations.AddExactZeroed(1);
			Animations[ThisIndex].AnimInfo = TempInfo;

			for(INT t=0; t< RawAnimKeys.Num(); t++)
			{
				Animations[ThisIndex].KeyTrack.AddItem( RawAnimKeys[t] );
			}

			if( TempInfo.ScaleInclude )
			{
				for(INT s=0; s< RawScaleKeys.Num(); s++)
				{
					Animations[ThisIndex].ScaleTrack.AddItem( RawScaleKeys[s] );
				}
			}

			// get name
			strcpy( Animations[ThisIndex].AnimInfo.Name, RawAnimName );
			// get group name
			strcpy( Animations[ThisIndex].AnimInfo.Group, ("None") );

			INT TotalKeys = Animations[ThisIndex].KeyTrack.Num();
			RawNumFrames = 0;

			RawAnimKeys.Empty();
			RawScaleKeys.Empty();

			return TotalKeys;
		}
		else
		{
			RawNumFrames = 0;
			RawAnimKeys.Empty();
			RawScaleKeys.Empty();
			return 0;
		}
	}

	DWORD FlagsFromName(const TCHAR* pName )
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
		BOOL	alpha=FALSE;

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

		return (DWORD) MatFlags;
	}


	int ReflagMaterials()
	{
		for(INT t=0; SkinData.Materials.Num(); t++)
		{
#if _UNICODE
			TCHAR MaterialName[64];
			mbstowcs(MaterialName, SkinData.Materials[t].MaterialName, 64);
#else
			char *MaterialName = SkinData.Materials[t].MaterialName;
#endif
			SkinData.Materials[t].PolyFlags = FlagsFromName(MaterialName);
		}
		return 1;
	}

	// Write out a brush to the file. Everything's in SkinData - ignore any weights etc.
	int WriteBrush(FastFileClass &OutFile, INT DoSmooth, INT OneTexture )
	{
		//for(INT m=0; m<SkinData.Materials.Num(); m++)
		//	PopupBox("MATERIAL OUTPUT SIZES: [%i] %i %i",m,SkinData.MaterialUSize[m],SkinData.MaterialVSize[m]);


		OutFile.Print(_T("Begin PolyList\r\n"));
		// Write all faces.
		for(INT i=0; i<SkinData.Faces.Num(); i++)
		{

			FVector Base;
			FVector Normal;
			FVector TextureU;
			FVector TextureV;
			INT PointIdx[3];
			FVector Vertex[3];
			FLOAT U[3],V[3];

			INT TexIndex = SkinData.Faces[i].MatIndex;
			if ( OneTexture ) TexIndex = 0;

			for(INT v=0; v<3; v++)
			{
				PointIdx[v]= SkinData.Wedges[ SkinData.Faces[i].WedgeIndex[v] ].PointIndex;
				Vertex[v] = SkinData.Points[ PointIdx[v]].Point;
				U[v] = SkinData.Wedges[ SkinData.Faces[i].WedgeIndex[v] ].UV.U;
				V[v] = SkinData.Wedges[ SkinData.Faces[i].WedgeIndex[v] ].UV.V;
			}

			// Compute Unreal-style texture coordinate systems:
			//FLOAT MaterialWidth  = 1024; //SkinData.MaterialUSize[TexIndex]; //256.0f
			//FLOAT MaterialHeight = -1024; //SkinData.MaterialVSize[TexIndex]; //256.0f

			FLOAT MaterialWidth  = SkinData.MaterialUSize[TexIndex];
			FLOAT MaterialHeight = -SkinData.MaterialVSize[TexIndex];

			FTexCoordsToVectors
			(
				Vertex[0], FVector( U[0],V[0],0.0f) * FVector( MaterialWidth, MaterialHeight, 1),
				Vertex[1], FVector( U[1],V[1],0.0f) * FVector( MaterialWidth, MaterialHeight, 1),
				Vertex[2], FVector( U[2],V[2],0.0f) * FVector( MaterialWidth, MaterialHeight, 1),
				&Base, &TextureU, &TextureV
			);

			// Need to flip the one texture vector ?
			TextureV *= -1;
			// Pre-flip ???
			FVector Flip(-1,1,1);
			Vertex[0] *= Flip;
			Vertex[1] *= Flip;
			Vertex[2] *= Flip;
			Base *= Flip;
			TextureU *= Flip;
			TextureV *= Flip;

			// Maya: need to flip everything 'upright' -Y vs Z?

			// Write face
			OutFile.Print("	Begin Polygon");

			OutFile.Print("	Texture=%s", SkinData.Materials[TexIndex].MaterialName );
			if( DoSmooth )
				OutFile.Print(" Smooth=%u", SkinData.Faces[i].SmoothingGroups);
			// Flags = NOTE-translate to Unreal in-engine style flags

			DWORD PolyFlags = 0;
			// SkinData.Materials[TexIndex].PolyFlags
			DWORD TriFlags = SkinData.Materials[TexIndex].PolyFlags;
			if( TriFlags)
			{
				// Set style based on triangle type.
				if     ( (TriFlags&15)==MTT_Normal         ) PolyFlags |= 0;
				else if( (TriFlags&15)==MTT_NormalTwoSided ) PolyFlags |= PF_TwoSided;
				else if( (TriFlags&15)==MTT_Modulate       ) PolyFlags |= PF_Modulated;
				else if( (TriFlags&15)==MTT_Translucent    ) PolyFlags |= PF_Translucent;
				else if( (TriFlags&15)==MTT_Masked         ) PolyFlags |= PF_Masked;
				else if( (TriFlags&15)==MTT_Placeholder    ) PolyFlags |= PF_Invisible;

				// Handle effects.
				if     ( TriFlags&MTT_Unlit             ) PolyFlags |= PF_Unlit;
				if     ( TriFlags&MTT_Flat              ) PolyFlags |= PF_Flat;
				if     ( TriFlags&MTT_Environment       ) PolyFlags |= PF_Environment;
				if     ( TriFlags&MTT_NoSmooth          ) PolyFlags |= PF_NoSmooth;

				// per-pixel Alpha flag ( Reuses Flatness triangle tag and PF_AlphaTexture engine tag...)
				if     ( TriFlags&MTT_Flat				) PolyFlags |= PF_AlphaTexture;
			}

			OutFile.Print(" Flags=%i",PolyFlags );

			OutFile.Print(" Link=%u", i);
			OutFile.Print("\r\n");

			OutFile.Print("		Origin   %+013.6f,%+013.6f,%+013.6f\r\n", Base.X, Base.Y, Base.Z );
			OutFile.Print("		TextureU %+013.6f,%+013.6f,%+013.6f\r\n", TextureU.X, TextureU.Y, TextureU.Z );
			OutFile.Print("		TextureV %+013.6f,%+013.6f,%+013.6f\r\n", TextureV.X, TextureV.Y, TextureV.Z );
			for( INT v=0; v<3; v++ )
			{
				OutFile.Print("		Vertex   %+013.6f,%+013.6f,%+013.6f\r\n", Vertex[v].X, Vertex[v].Y, Vertex[v].Z );
			}
			OutFile.Print("	End Polygon\r\n");
		}
		OutFile.Print("End PolyList\r\n");

		return 0;
	}
};

#endif
