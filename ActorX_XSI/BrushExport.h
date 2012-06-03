/**************************************************************************
 
	BrushExport.h - XSI specific, helper functions for static mesh digestion

***************************************************************************/
#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_selection.h>
#include <xsi_scene.h>
#include <xsi_polygonmesh.h>
#include <xsi_geometry.h>
#include <xsi_primitive.h>
#include <xsi_point.h>
#include <xsi_triangle.h>
#include <xsi_project.h>
#include <xsi_material.h>
#include <xsi_ogltexture.h>
#include <xsi_library.h>
#include <xsi_edge.h>
#include "actorx.h"
#include "SceneIfc.h"
#define INT int
#define UBOOL bool
#define DWORD long

void ExportStaticMeshLocal();

// Edge info to go in a TArray for each indiv. vertex.
struct XSIEdge
{
	int     polyIds[2];  // ID's of polygons that reference this edge.
	int		vertId; // Second vertex
	bool	smooth; // is this edge smooth ?
};

// For a vertex, a TArray with all edges.
struct VertEdges
{
	TArray<XSIEdge> Edges;
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
    void            buildEdgeTable( XSI::X3DObject in_obj );
	void            createNewSmoothingGroups( XSI::X3DObject in_obj );	
    void            addEdgeInfo( INT v1, INT v2, bool smooth );
	XSIEdge*       findEdgeInfo( INT v1, INT v2);
	void            fillSmoothFaces( INT polyid,XSI::X3DObject in_obj );

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
