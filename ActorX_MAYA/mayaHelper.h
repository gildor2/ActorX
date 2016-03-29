// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#ifndef __MayaHelper__H
#define __MayaHelper__H

// PCF BEGIN
/*
	maya triangulation helpers:
		triangulatedMesh - triangulates dagPath
		edgesPool - builds edges structure for smoothing groups of triangulated polys

	by Sebastian Woldanski


*/


#include "MayaInclude.h"

#include "UnSkeletal.h"

using namespace std;
#include <vector>


/*
	edgesPool - builds edges structure for smoothing groups of triangulated polys
*/
class edgesPool
{
public:
	edgesPool (int numVerts);

	~edgesPool()
	{
		destroy();
	};



	void destroy()
	{
		for (INT i =0; i< _edges.size(); i++)
			if (_edges[i]) delete _edges[i];
		for (INT i =0; i< _vertsPairs.size(); i++)
			if (_vertsPairs[i]) delete _vertsPairs[i];
		for (INT i =0; i< _vertEdges.size(); i++)
			if (_vertEdges[i]) delete _vertEdges[i];
	}
	typedef struct edge
	{
		int	v1;
		int v2;
		int f1;
		int f2;
		bool smooth;
		edge()
		{
			v1=-1;
			v2=-1;
			f1=-1;
			f2=-1;
			smooth =true;
		};

	};


	bool add (int v1, int v2, int faceIndex);
	void setEdgeSmoothing (MDagPath & );	// sets smoothing flag via maya polygon edge iterator
	bool findFaces (int v1, int v2, int fases[2]  ); // returns edge faces from verts
	edge * find (int v1, int v2  ); // returns valid edge from verts
	vector  <edge *>	_edges;
	vector  <MIntArray *>	_vertsPairs; // size of numVerts
	vector  <MIntArray *>	_vertEdges; // size of numVerts

private:



};

// The number of additional supported UV sets besides the manditory 1
#define NUM_EXTRA_UV_SETS 3

/*
triangulatedMesh
	triangulates maya dag path and builds tructure of indexes

*/
class triangulatedMesh
{
public:
	triangulatedMesh():edges(NULL)
	{
		numVerts =0;
	};
	~triangulatedMesh()
	{
		destroy();
	};

	void destroy()
	{
		if (edges)
			delete edges;
		for (INT i =0; i< triangles.size(); i++)
		{
			if(triangles[i]) delete triangles[i];

		}
	}

	struct Index
	{
		int	pointIndex;
		int tcIndex[NUM_EXTRA_UV_SETS+1];
		bool hasExtraUVs[NUM_EXTRA_UV_SETS];
		int vertexColor;
		Index()
		{

			memset( hasExtraUVs, 0, sizeof( bool ) * NUM_EXTRA_UV_SETS );
			vertexColor =-1;

		}


	};
	typedef struct Triangle
	{
		Index	Indices[3];
		int		ShadingGroupIndex;	// index for ShadingGroups
	};

	bool triangulate(MDagPath & _d);
	bool createEdgesPool();

	vector <Triangle *>	triangles;

	MFloatArray uArray[NUM_EXTRA_UV_SETS+1];
	MFloatArray vArray[NUM_EXTRA_UV_SETS+1];

	int numVerts;

	int NumUVSets;

	edgesPool * edges;

	bool exportNormals;




private:
	MDagPath  dag;

};
// PCF END
#endif
