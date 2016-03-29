// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#include "mayaHelper.h"
// PCF BEGIN
/*
	maya triangulation helpers triangulatedMesh, edgesPool classes
	by Sebastian Woldanski
*/
edgesPool::edgesPool (int numVerts)
{
	for (int i =0; i< numVerts; i++)
	{
		MIntArray * ar =new MIntArray;
		_vertsPairs.push_back( ar);

		ar =new MIntArray;
		_vertEdges.push_back( ar);
	}
}


// ads edge from verts
bool edgesPool::add (int v1, int v2, int face)
{

	if (!_vertsPairs[v1] || !_vertsPairs[v2])
		return  false;

	edgesPool::edge * e= find(v1,  v2) ;
		//// look for opposite vert , if found edge already added
		//MIntArray * ar = _vertsPairs[v1];
		//int found =0;
		//for (int i =0; i< ar->length(); i++)
		//{
		//	if ((*ar)[i] == v2)
		//	{
		//		found =1;
		//		break;
		//	}
		//
		//}

		//if (!found)
		//{
		//	ar = _vertsPairs[v2];
		//	for (int i =0; i< ar->length(); i++)
		//	{
		//		if ((*ar)[i] == v2)
		//		{
		//			found =1;
		//			break;
		//		}
		//	}

		//}
	if (e)
	{
		// check if face on edge exists
		if (e->f1 == -1 && e->f2 != face)
			e->f1 = face;
		else if (e->f2 == -1 && e->f1 != face)
			e->f2 = face;
		else
			return false;

		return false;
	}
	else
	{
		MIntArray * ar = _vertsPairs[v1];
		ar->append(v2);

		ar = _vertsPairs[v2];
		ar->append(v1);



		edge * e = new edge;
		e->v1 = v1;
		e->v2 = v2;
		if (e->f1 == -1   )
			e->f1  =face;
		else
			e->f2  = face;
		_edges.push_back(e);

		int newEdgeind = ( int )_edges.size() -1;
		ar = _vertEdges[v1];
		ar->append(newEdgeind);

		ar = _vertEdges[v2];
		ar->append(newEdgeind);

		return true;

	}

}
bool edgesPool::findFaces (int v1, int v2, int fases[2] )
{
	fases[0] = -1;
	fases[1] = -1;


		if (_vertsPairs[v1])
		{

			MIntArray & ar = *_vertsPairs[v1];
			for (int i =0; i< ar.length(); i++)
			{
				if (ar[i] == v2)
				{

					edge * e = _edges[ar[i]];
					if (e )
					{
						fases[0] = e->f1;
						fases[1] = e->f2;

					}
					return true;

				}

			}
		}
		return false;
}
edgesPool::edge * edgesPool::find (int v1, int v2  )
{
	if (_vertsPairs.size() > v1  )
	{
		int ind =-1;
		MIntArray & ar = *_vertsPairs[v1];
		for (int i = 0 ; i < ar.length(); i++)
		{
			if (ar[i] == v2)
			{
				ind = i;
				break;

			}
		}
		if (ind > -1)
		{
			MIntArray & ar = *_vertEdges[v1];
			int eind = ar[ind];
			edge * e = _edges[(*_vertEdges[v1])[ind]];
			return e;

		}
	}
	return NULL;
}
void edgesPool::setEdgeSmoothing (MDagPath& mesh  )
{
	// Add entries, for each edge, to the lookup table
	MItMeshEdge eIt( mesh );
	for ( ; !eIt.isDone(); eIt.next() ) // Iterate over edges.
	{
		bool smooth = eIt.isSmooth();  // Smoothness retrieval for edge.
		edge * e= find(eIt.index(0), eIt.index(1));
		if (e)
			e->smooth = smooth;
	}
}
bool triangulatedMesh::createEdgesPool()
{
	if (edges)
		delete edges;

	edges =new edgesPool(numVerts);
	if (edges)
	{


		for ( int i =0 ; i < triangles.size(); i++)
		{
			Triangle & t = *triangles[i];
			edges->add(t.Indices[0].pointIndex,t.Indices[1].pointIndex , i);
			edges->add(t.Indices[1].pointIndex,t.Indices[2].pointIndex , i);
			edges->add(t.Indices[2].pointIndex,t.Indices[0].pointIndex , i);

		}
		edges->setEdgeSmoothing(dag);
		return true;
	}
	return false;
}
bool triangulatedMesh::triangulate(MDagPath & dagPath)
{
	MStatus stat;
	//dag.set(_d);
	//mesh = new	MFnMesh(_d);
	dag =  dagPath;
	MFnMesh fnMesh(dag);

	numVerts = 	fnMesh.numVertices();

	MStringArray uvSetNames;
	stat = fnMesh.getUVSetNames(uvSetNames);
	NumUVSets = uvSetNames.length();
	for( int UVIndex = 0; UVIndex < NumUVSets; ++UVIndex )
	{
		fnMesh.getUVs (uArray[UVIndex], vArray[UVIndex] , &uvSetNames[UVIndex] );
	}

	MObjectArray shaders;
	MIntArray shaderIndicesPerPoly;
	stat = fnMesh.getConnectedShaders(0, shaders, shaderIndicesPerPoly);
	if (!stat)
	{
		return 0;
	}
	bool hasColors = false;
	MString colorSetName;
	fnMesh.getCurrentColorSetName ( colorSetName);
	if (fnMesh.getCurrentColorSetName ( colorSetName) && colorSetName.length() )
		hasColors = true;

	MItMeshPolygon polyIter ( dagPath);

	int c = 0;

	MPointArray _pt_temp;
	MIntArray  tris_vertexListGlobal;
	MIntArray  tris_vertexListLocal;
	MIntArray   verticesIndices;
	// stworzmy liste - face reletive tris verts indexes
	int tangentOffset=0;
	int u=0;

	bool bHasUVSet[NUM_EXTRA_UV_SETS+1];
	for ( ; ! polyIter.isDone(); polyIter.next() )
	{
		if (! polyIter.hasValidTriangulation())
		{
			return 0;
		}

		for( int UVIndex = 0; UVIndex < NumUVSets; ++UVIndex )
		{
			if (! polyIter.hasUVs(uvSetNames[UVIndex]))
			{
				bHasUVSet[UVIndex] = false;
			}
			else
			{
				bHasUVSet[UVIndex] = true;
			}
		}


		u =polyIter.index();

		// process triangles
		int triNum;
		polyIter.numTriangles(triNum);



		//	cerr << " poly " << u<< endl;

		//// cerr << " poly " << faceID<< endl;


		//	cerr << "	dupa 1"<<endl;

		polyIter.getTriangles( _pt_temp, tris_vertexListGlobal );
		polyIter.getVertices ( verticesIndices);
		tris_vertexListLocal.clear();
		tris_vertexListLocal.setLength(tris_vertexListGlobal.length());

		for ( int i =0 ; i <  tris_vertexListGlobal.length(); i++)
		{

			for ( int j =0 ; j <  verticesIndices.length(); j++)
			{
				//	cerr << "		" << tris_vertexListGlobal[i] << " ? " << verticesIndices[j]<<endl;
				if (tris_vertexListGlobal[i] == verticesIndices[j] )
				{
					tris_vertexListLocal[i] = j;
					break;
				}
			}
		}
		//cerr << "	dupa 2"<<endl;

		int c=0;
		for ( int i =0 ; i <  triNum; i++)
		{
			triangulatedMesh::Triangle * pt = new triangulatedMesh::Triangle;
			triangulatedMesh::Triangle & t = * pt;
			//cerr << "		gowno 1"<<endl;


			for ( int j =0 ; j <  3; j++)
			{
				t.Indices[j].pointIndex = tris_vertexListGlobal[c];
				//t.Indices[j].normalIndex =polyIter.normalIndex(tris_vertexListLocal[c]);
				int indexuv;

				for( int UVIndex = 0; UVIndex < NUM_EXTRA_UV_SETS; ++UVIndex )
				{
					if ( bHasUVSet[ UVIndex ] )
					{
						stat = polyIter.getUVIndex(tris_vertexListLocal[c], indexuv, &uvSetNames[UVIndex]);
						if (stat)
						{
							t.Indices[j].tcIndex[UVIndex] = indexuv;

							if( UVIndex > 0 )
							{
								t.Indices[j].hasExtraUVs[ UVIndex - 1 ] = true;
							}
						}
					}
					else
					{
						t.Indices[j].tcIndex[UVIndex] =0;
					}
				}

				int colorIndex = -1;
				if (hasColors && fnMesh.getFaceVertexColorIndex( u,tris_vertexListLocal[c], colorIndex, &colorSetName) == MS::kSuccess)
				{
					t.Indices[j].vertexColor = colorIndex;
				}



				c++;

			}
			//cerr << "		gowno 2"<<endl;

			if (shaderIndicesPerPoly[u] == -1)
			{

				t.ShadingGroupIndex =-1;
			}
			else
				t.ShadingGroupIndex =shaderIndicesPerPoly[u];

			triangles.push_back(pt);

		}


	}

	return true;


}
//PCF END
