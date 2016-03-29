// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**************************************************************************

   MaxInterface.h - MAX-specific exporter scene interface code.

   Created by Erik de Neve

   Lots of fragments snipped from all over the Max SDK sources.

***************************************************************************/

#ifndef __MayaInterface__H
#define __MayaInterface__H

// Local includes
//#include "Phyexp.h"  // Max' PHYSIQUE interface
#include "MayaInclude.h"
#include "Vertebrate.h"
#include "Win32IO.h"
#include "mayaHelper.h"

// Minimum bone influence below which the physique weight link is ignored.
#define SMALLESTWEIGHT (0.0001f)
#define MAXSKININDEX (64)
#define NO_SMOOTHING_GROUP      (-1)
#define QUEUED_FOR_SMOOTHING (-2)
#define INVALID_ID              (-1)

// Dialogs.
void ShowAbout(HWND hWnd);
void ShowPanelOne(HWND hWnd);
void ShowPanelTwo(HWND hWnd);
void ShowActorManager(HWND hWnd);
void ShowStaticMeshPanel(HWND hWnd);


INT_PTR CALLBACK SceneInfoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ActorManagerDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
INT_PTR CALLBACK StaticMeshDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Misc: platform independent..
UBOOL SaveAnimSet( char* DestPath );

// Misc  Maya specific.
FLOAT GetMayaTimeStart();
FLOAT GetMayaTimeEnd();
// Get an actual shader, from a higher (shading group) node.
MObject findShader( MObject& setNode );
// Get a texture name (forward slashes converted to backward) from a shader.
INT GetTextureNameFromShader( const MObject& shaderNode, char* BitmapFileName, int MaxChars );

INT DigestMayaMaterial( MObject& NewShader  );


//
// HINSTANCE ParentApp_hInstance; // Parent instance
//
extern struct HINSTANCE__ * MhInstPlugin;  // Link to parent app
extern WinRegistry PluginReg;






// Edge info to go in a TArray for each indiv. vertex.
struct MayaEdge
{
	int     polyIds[2];  // ID's of polygons that reference this edge.
	int		vertId; // Second vertex
	bool	smooth; // is this edge smooth ?
};

// For a vertex, a TArray with all edges.
struct VertEdges
{
	TArray<MayaEdge> Edges;
};

struct GroupList
{
	TArray<INT> Groups;
	INT FinalGroup;
};

// Face environment for each face - pools of all touching smooth- and hard-bounded face indices.
class FaceEnviron
{
	public:
	TArray<INT> SmoothFaces;
	TArray<INT> HardFaces;
};

// Helper class to convert edge smoothing properties to face smoothing groups.
class MeshProcessor
{
public:

    // Edge lookup methods
    void            buildEdgeTable( MDagPath& mesh );
	void            createNewSmoothingGroups( MDagPath& mesh );
	//PCF BEGIN
	void            createNewSmoothingGroupsFromTriangulatedMesh( triangulatedMesh& );
	//PCF END
    void            addEdgeInfo( INT v1, INT v2, bool smooth );
    MayaEdge*       findEdgeInfo( INT v1, INT v2);
    UBOOL		smoothingAlgorithm( INT, MFnMesh& fnMesh );
// PCF BEGIN - not using MFnMesh& fnMesh
	void            fillSmoothFaces( INT polyid );
//PCF END

	// Structors
	MeshProcessor()
	{
		CurrentGroup = 1;
	}

	~MeshProcessor()
	{
		// Deallocate the multi-level-TArrays properly.
		for ( int v=0; v<VertEdgePools.Num(); v++ )
		{
			VertEdgePools[v].Edges.Empty();
		}
		VertEdgePools.Empty();

		for( int g=0; g<GroupTouchPools.Num(); g++)
		{
			GroupTouchPools[g].Groups.Empty();
		}
		GroupTouchPools.Empty();

		for( int f=0; f<FaceSmoothingGroups.Num(); f++)
		{
			FaceSmoothingGroups[f].Groups.Empty();
		}
		FaceSmoothingGroups.Empty();

		for( int h=0; h<FacePools.Num(); h++)
		{
			FacePools[h].SmoothFaces.Empty();
			FacePools[h].HardFaces.Empty();
		}
		FacePools.Empty();

	}

	void RemapGroupTouchTools( INT source, INT dest )
	{
		// Remap all elements;
		for( INT i=0; i< GroupTouchPools.Num(); i++)
		{
			INT Changes = 0;
			for( INT j=0; j< GroupTouchPools[i].Groups.Num(); j++)
			{
				if ( GroupTouchPools[i].Groups[j] == source )
				{
					GroupTouchPools[i].Groups[j] = dest;
					Changes++;
				}
			}

			// Eliminate possible duplicates by copying it into a unique array.
			if( Changes )
			{
				TArray<INT> NewGroups;
				for( INT j=0; j<GroupTouchPools[i].Groups.Num(); j++)
				{
					NewGroups.AddUniqueItem( GroupTouchPools[i].Groups[j] );
				}
				GroupTouchPools[i].Groups.Empty();
				for( INT n=0; n<NewGroups.Num(); n++)
				{
					GroupTouchPools[i].Groups.AddItem( NewGroups[n] );
				}
			}
		}

		// Merge source Pool elements into dest pool.
		for( INT j=0; j< GroupTouchPools[source].Groups.Num(); j++)
		{
			INT item = GroupTouchPools[source].Groups[j];
			GroupTouchPools[dest].Groups.AddUniqueItem( item );
		}

		// Delete 'dest' element lager..
		GroupTouchPools[source].Groups.Empty();
	}


	public:

	TArray< DWORD >			PolySmoothingGroups;
	TArray< GroupList >     FaceSmoothingGroups;

	private:

	TArray< INT >					PolyProcessFlags;
	TArray<FaceEnviron>       FacePools;
	TArray< VertEdges >			VertEdgePools;

	TArray< GroupList >			GroupTouchPools;
	TArray< INT >					FacesTodo;	 // Bookmarks for flood filling group assignment.
	TArray< INT >					SmoothEnvironmentFaces; // Smoothly joined neighbor faces for current face.
	TArray< INT >					HardEnvironmentFaces; // Non-smoothly joined neighbor faces for current face.

	INT					CurrentGroup;

    DWORD           nextSmoothingGroup;
    DWORD           currSmoothingGroup;
    bool				 newSmoothingGroup;
};




#endif // _MayaInterface__H
