/**************************************************************************
 
   Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

   MayaInterface.cpp - MAYA-specific exporter scene interface code.

   Created by Erik de Neve

   Lots of fragments snipped from all over the Maya SDK sources.

   Todo:  - Move any remaining non-Maya dependent code to SceneIFC
          - Assertions for exact poly/vertex/wedge bounds (16 bit indices !) 
 	         (max 21840)
		   - OurSkins array not used -> clean up or use..

  
   Change log (incomplete)

   Feb 2001:
   See #ROOT: Added explicit root bone scaler into bone sizes - as this is where in Maya
   the skeleton is scaled by default (independently of the skin).

   Feb 2002
     - 2/08/02   Misc cleanups for stability; added dummy UVSet string to the MeshFunction.getPolygonUV (which should be optional?) solved strange crash...	 

   Feb 2003
     - Ascii export for static meshes.

   Mar 2003
     - Moved vertex exporter to own window and command [axvertex].
     - Experimental: non-joint hierarchy export: any geom objects that DON't have a true maya joint will be
	   allowed geometry-as-bones exporting.

   May 2003
     - Siginficant fix in smoothing group generation/output. (v 2.21)

   Aug 2003
     - Adds full skeletal export features in the shape of "axautoexport..." ( with parameters, to enable MEL-script batching. )	

   Sept 2003
    - Adds new  MeshProcessor::createNewSmoothingGroups()   which correctly allocates smoothing groups - multiple 
	   per face if necessary - to duplicate the exact polygon edge sharpness/softness from the Maya hard/soft edges 
	   into max-style per-face smoothing groups.

   Nov 2003
    - ClearScenePointers around all MEL commands to fix crash from invalid object arrays when changing scenes in between commands.
	- Still to be compiled with VC 6.0 since plugins made with .Net 2002 and 2003 are still less compatible with Maya 4.5 (and 5.0 ??)



 //////////////
   WORK NOTES:
 //////////////

     Non-joint hierarchy ? -> Special option needed....	

:::::::::::::::::::: Short Maya class SDK summary ::::::::::::::::::::::::::

     MDagPath:
		MDagPath ::node()  -> MObject node.
		MObject MDagPath:: transform ( MStatus *ReturnStatus) const  -> Retrieves the lowest Transform in the DAG Path.
		MMatrix MDagPath:: inclusiveMatrix ( MStatus *ReturnStatus) const  =>The matrix for all Transforms in the Path including the the end Node in the Path if it is a Transform 
		MMatrix MDagPath:: exclusiveMatrix ( MStatus * ReturnStatus) const => The matrix for all transforms in the Path excluding the end Node in the Path if it is Transform 
		M4 MDagPath:: inclusiveMatrixInverse ( MStatus * ReturnStatus) const 
		MMatrix MDagPath:: exclusiveMatrixInverse ( MStatus * ReturnStatus) const 
		MDagPath:: MDagPath ( const MDagPath & src )  => Copy constructor
		bool MDagPath:: hasFn ( MFn::Type type, MStatus *ReturnStatus ) const 
		MObject MDagPath:: node ( MStatus * ReturnStatus) const => Retrieves the DAG Node for this DAG Path.
		MString MDagPath:: fullPathName ( MStatus * ReturnStatus) const  => Full path name.
		
     MObject:
		const char * MObject:: apiTypeStr () const => Returns a string that gives the object's type is a readable form. This is useful for debugging when type of an object needs to be printed
		MFn::Type MObject:: apiType () const  =>  Determines the exact type of the Maya internal Object.

     MFnDagNode:
		MStatus  getPath ( MDagPath & path ) 

		MFnDagNode ( MObject & object, MStatus * ret = NULL )  => constructor
		MMatrix MFnDagNode:: transformationMatrix ( MStatus * ReturnStatus ) const 
		=>Returns the transformation matrix for this DAG node. In general, only 
		transform nodes have matrices associated with them. Nodes such as shapes 
		(geometry nodes) do not have transform matrices.
		The identity matrix will be returned if this node does not have a transformation matrix.
		If the entire world space transformation is required, then an MDagPath should be used.

		MStatus MFnDagNode:: getPath ( MDagPath & path )  
		=>Returns a DAG Path to the DAG Node attached 
		to the Function Set. The difference between this method 
		and the method dagPath below is that this one will not 
		fail if the function set is not attached to a dag path, 
		it will always return a path to the node. dagPath will 
		fail if the function set is not attached to a dag path.

		MDagPath MFnDagNode:: dagPath ( MStatus * ReturnStatus ) const 
        =>Returns the DagPath to which the Function Set is attached. 
		The difference between this method and the method getPath above 
		is that this one will fail if the function set is not attached 
		to a dag path. getPath will find a dag path if the function set is not attached to one.


	 MFnDependencyNode:
		MFnDependencyNode ( MObject & object, MStatus * ReturnStatus = NULL )  => constructor
		MString MFnDependencyNode:: classification ( const MString & nodeTypeName )
		=>  Retrieves the classification string for a node type. This is a string that is used 
		    in dependency nodes that are also shaders to provide more detailed type information 
			to the rendering system. See the documentation for the MEL commands getClassification 
			and listNodeTypes for information on the strings that can be provided.
			User-defined nodes set this value through a parameter to MFnPlugin::registerNode.

		MString  name ( MStatus * ReturnStatus = NULL ) const 
		MString  typeName ( MStatus * ReturnStatus = NULL ) const	
	    MTypeId  typeId ( MStatus * ReturnStatus = NULL ) const
      
***************************************************************************/
// Plugin DLLMAIN - included only once.
#include "SceneIfc.h"
#include <maya/MFnPlugin.h> 
#include "MayaInterface.h"
#include "BrushExport.h"

#include "ActorX.h"
#include "ActorY.h"
//#EDN // For the test1/test2 code
#include <process.h> 
#include <time.h> 



//PCF BEGIN
#ifdef NEWMAYAEXPORT
#include "mayaHelper.h"
#endif
//PCF END

#define DeclareCommandClass( _className )						\
class _className : public MPxCommand							\
{																\
public:															\
	_className() {};											\
	virtual MStatus	doIt ( const MArgList& );					\
	static void*	creator();									\
	static MSyntax newSyntax();									\
};																\
	void* _className::creator()									\
{																\
	return new _className;										\
}																\

DeclareCommandClass( ActorXTool0 );
DeclareCommandClass( ActorXTool1 );
DeclareCommandClass( ActorXTool2 );
DeclareCommandClass( ActorXTool3 );
DeclareCommandClass( ActorXTool4 );
DeclareCommandClass( ActorXTool5 );
DeclareCommandClass( ActorXTool6 );
DeclareCommandClass( ActorXTool7 );
DeclareCommandClass( ActorXTool8 );
DeclareCommandClass( ActorXTool10);
DeclareCommandClass( ActorXTool11);
DeclareCommandClass( ActorXTool12);
//#EDN - testing
#if TESTCOMMANDS
DeclareCommandClass( ActorXTool21);
DeclareCommandClass( ActorXTool22);
#endif

MObjectArray AxSceneObjects;
MObjectArray AxSkinClusters;
MObjectArray AxShaders;
MObjectArray AxBlendShapes;
//
#define NOJOINTTEST (0)

#define TESTCOMMANDS (0)

// Forwards.
void ShowVertExporter( HWND hWnd );
void SaveSceneSkin();
void SaveCurrentSceneSkin( const char * OutputFile, TArray<ANSICHAR*>& LogList );
void DoDigestAnimation();


void ClearScenePointers()
{
	AxSceneObjects.clear();
	AxSkinClusters.clear();	
	AxShaders.clear();
	AxBlendShapes.clear();
}

INT WhereInMobjectArray( MObjectArray& MArray, MObject& LocateObject )
{
	INT KnownIndex = -1;
	for( DWORD t=0; t < MArray.length(); t++)
	{
		if( MArray[t] == LocateObject ) // comparison legal ?
			KnownIndex = t;
	}
	return KnownIndex;
}

// Print a string to the Maya log window.
void MayaLog(char* LogString, ... )
{
	//if (!DEBUGMSGS) return;
	char TempStr[4096];
	GET_VARARGS(TempStr,4096,LogString,LogString);
	MStatus	stat;
	stat.perror(TempStr);		
}




void SceneIFC::StoreNodeTree(AXNode* node) 
{
	MStatus	stat;

	INT NodeNum = 0;
	INT ObjectCount = 0;

	// Global scene object array
	// #NOTE: Clusters Empty() doesn't work right in reproducable cases -> though runs fine in 'release' build.		 
	
	ClearScenePointers();
	
	//
	// Iterate over all nodes in the dependency graph. Only some dependency graph nodes are/have actual DAG nodes.
	// 	

	for( MItDependencyNodes DepIt(MFn::kDependencyNode);!DepIt.isDone();DepIt.next())
	{
		MFnDependencyNode	Node(DepIt.item());

		// Get object for this Node.
		MObject	Object = Node.object();

		// Store BlendShapes		
		if ( Object.apiType() == MFn::kBlendShape )
		{
			AxBlendShapes.append( Object );
		}

		// Maya SkinCluster modifier is not a DAG node - store independently.

		if ( Object.apiType() == MFn::kSkinClusterFilter ) 
		{
			AxSkinClusters.append( Object );
		}

		if(! DepIt.item().hasFn(MFn::kDagNode))
				continue;

		MFnDagNode DagNode = DepIt.item();
		
		if(DagNode.isIntermediateObject())
				continue;
				
		//
		// Simply record all relevant objects.
		// Includes skinclusterfilters(?) (and materials???)
		//
				
		if( Object != MObject::kNullObj  )
		{
			if( //( Object.hasFn(MFn::kMesh)) ||
				( Object.apiType() == MFn::kMesh ) ||
				( Object.apiType() == MFn::kJoint ) ||
				( Object.apiType() == MFn::kTransform ) )
			{
				AxSceneObjects.append( Object ); //AxSceneObjects.AddItem(Object); 
								
				//if ( ! Object.hasFn(MFn::kDagNode) )
				//	PopupBox("Object %i [%s] has no DAG node",NodeNum, Object.apiTypeStr() );

				if ( Object.apiType() == MFn::kSkinClusterFilter ) 
				{
					//PopupBox("Skincluster found, scenetree item %i name %s",AxSceneObjects.Num()-1, Node.name(&stat).asChar() );
				}

				NodeInfo NewNode;
				NewNode.node = (void*) ObjectCount;
				NewNode.NumChildren = 0;

				SerialTree.AddItem( NewNode ); // Store index rather than Object.

				ObjectCount++;
			}
			else // Not a mesh or skincluster....
			{
				//DLog.Logf("NonMesh item:%i  Type: [%s]  NodeName: [%s] \r\n",NodeNum, Object.apiTypeStr(), Node.name(&stat).asChar());
			}
			
		}
		else // Null objects...
		{		
			// DLog.Logf("Null object - Item number: %i   NodeName: [%s] \r\n",NodeNum, Node.name(&stat).asChar() );
		}

		NodeNum++;
	}
}


FLOAT GetMayaTimeStart()
{
	MTime start( MAnimControl::minTime().value(), MTime::uiUnit() );
	return start.value();
}

FLOAT GetMayaTimeEnd()
{
	MTime end( MAnimControl::maxTime().value(), MTime::uiUnit() );
	return end.value();
}

void SetMayaSceneTime( FLOAT CurrentTime)
{
	MTime NowTime;
	NowTime.setValue(CurrentTime);
	MGlobal::viewFrame( NowTime );
}

int DigestMayaMaterial( MObject& NewShader  )
{
	INT KnownIndex = -1;
	for( DWORD t=0; t < AxShaders.length(); t++)
	{
		if( AxShaders[t] == NewShader ) // comparison legal ?
			KnownIndex = t;
	}
	if( KnownIndex == -1)
	{
		KnownIndex = AxShaders.length();
		AxShaders.append( NewShader );		
	}
	//AxShaders.AddUniqueItem( NewShader );
	return( KnownIndex );
}


// Maya-specific Y-Z matrix reversal.
void MatrixSwapXYZ( FMatrix& ThisMatrix)
{
	for(INT i=0;i<4;i++)
	{
		FLOAT TempX = ThisMatrix.M[i][0];
		FLOAT TempY = ThisMatrix.M[i][1];
		FLOAT TempZ = ThisMatrix.M[i][2];
		
		ThisMatrix.M[i][0] = TempZ;
		ThisMatrix.M[i][1] = -TempX;
		ThisMatrix.M[i][2] = TempY;		
	}
	
	/*
	FPlane Temp = ThisMatrix.YPlane;
	ThisMatrix.YPlane = ThisMatrix.ZPlane;
	ThisMatrix.ZPlane = Temp;
	*/
}

void MatrixSwapYZTrafo( FMatrix& ThisMatrix)
{
		
	for(INT i=3;i<4;i++)
	{
		FLOAT TempZ = ThisMatrix.M[i][1];
		ThisMatrix.M[i][1] =    ThisMatrix.M[i][0];
		ThisMatrix.M[i][0] =    TempZ;
	}
	
	/*
	FPlane Temp = ThisMatrix.YPlane;
	ThisMatrix.YPlane = ThisMatrix.ZPlane;
	ThisMatrix.ZPlane = Temp;
	*/
}

//
//	Description:
//		return the type of the object
//
const char* objectType( MObject object )
{
	if( object.isNull() )
	{
		return NULL;
	}
	MStatus stat = MS::kSuccess;
	MFnDependencyNode dgNode;
	stat = dgNode.setObject( object );
	MString typeName = dgNode.typeName( &stat );
	if( MS::kSuccess != stat)
	{
		//cerr << "Error: can not get the type name of this object.\n";
		return NULL;
	}
	return typeName.asChar();
}


int GetMayaShaderTextureSize( MObject alShader, INT& xres, INT& yres )
{
    //MtlStruct	  *mtl;
	MString   shaderName;
	MString   textureName;
	MString   textureFile = "";
	MString	  projectionName = "";
	MString	  convertName;
	MString   command;
	MStringArray result;
	MStringArray arrayResult;

	MStatus   status;
	MStatus   statusT;

	xres=256;
	yres=256; //#debug
	
	bool		projectionFound = false;
	bool		layeredFound = false;
    bool		fileTexture = false;

    MFnDependencyNode dgNode;
    status = dgNode.setObject( alShader );
	shaderName = dgNode.name( &status );

	textureName = shaderName; //#debug default texturename...

    result.clear();

	MObject             currentNode;
	MObject 			thisNode;
	MObjectArray        nodePath;
	MFnDependencyNode   nodeFn,dgNodeFnSet;
	MObject             node;

	// Get the dependent texture for this shader by going over a dependencygraph ???
	MItDependencyGraph* dgIt; 
	dgIt = new MItDependencyGraph( alShader,
                               MFn::kFileTexture,
                               MItDependencyGraph::kUpstream, 
                               MItDependencyGraph::kBreadthFirst,
                               MItDependencyGraph::kNodeLevel, 
                               &status );
	//
	for ( ; ! dgIt->isDone(); dgIt->next() ) 
	{      
		thisNode = dgIt->thisNode();
        dgNodeFnSet.setObject( thisNode ); 
        status = dgIt->getNodePath( nodePath );
                                                                 
        if ( !status ) 
		{
			status.perror("getNodePath");
			continue;
        }

        //
        // append the starting node.
        //
        nodePath.append(node);

        //dumpInfo( thisNode, dgNodeFnSet, nodePath );
		//PopupBox("texture node found: [%s] type [%s] ",dgNodeFnSet.name(), objectType(thisNode) );

		textureName = dgNodeFnSet.name();
		fileTexture = true;

	}
    delete dgIt;

    // If we are doing file texture lets see how large the file is
    // else lets use our default sizes.  If we can't open the file
	// we will use the default size.

	if (fileTexture) // fileTexture)
	{
			//getTextureFileSize( textureName, xres, yres );

			// Lets try getting the size of the textures this way.
			MStatus		status;
			MString		command;
			MIntArray	sizes;

			command = MString( "getAttr " ) + textureName + MString( ".outSize;" );
			status = MGlobal::executeCommand( command, sizes );
			if ( MS::kSuccess == status )
			{
				xres = sizes[0];
				yres = sizes[1];
				return 1;
			}
			else
			{
				PopupBox("Unable to retrieve textures with .outSize command from [%s]",textureName);
			}
	}

	nodePath.clear(); 

	return 0;
}


//
// Retrieve joint trafo
//
void RetrieveTrafoForJoint( MObject& ThisJoint, FMatrix& ThisMatrix, FQuat& ThisQuat, INT RootCheck, FVector& BoneScale )
{
	// try to directly get local transformations only.

	static float	mtxI[4][4];
	static float	mtxIrot[4][4];
	static double   newQuat[4];

	MStatus stat;	
	// Retrieve dagnode and dagpath from object..
	MFnDagNode fnTransNode( ThisJoint, &stat );
	MFnDagNode fnTransNodeParent( ThisJoint, &stat );

	MDagPath dagPath;
	MDagPath dagPathParent;

	stat = fnTransNode.getPath( dagPath );
	stat = fnTransNodeParent.getPath( dagPathParent );

	MFnDagNode fnDagPath( dagPath, &stat );
	MFnDagNode fnDagPathParent( dagPathParent, &stat );

	MMatrix RotationMatrix;
	MMatrix localMatrix;
	MMatrix globalMatrix;
	MMatrix testMatrix;
	MMatrix globalParentMatrix;

	FMatrix MayaRotationMatrix;

	// RotationMatrix = dagPath.inclusiveMatrix();
	// testMatrix = dagPath.exclusiveMatrix();
	// Get global matrix if RootCheck indicates skeletal root

	if ( RootCheck )
	{		
		localMatrix = fnTransNode.transformationMatrix ( &stat );
		globalMatrix = dagPath.inclusiveMatrix();
		//globalMatrix.homogenize();

		globalMatrix.get( mtxI ); 
	}
	else
	{
		localMatrix = fnTransNode.transformationMatrix ( &stat );
		globalMatrix = dagPath.inclusiveMatrix();
		//localMatrix.homogenize();

		localMatrix.get( mtxI );
	}

	// UT bones are supposed to not have scaling; but we need the maya-specific per-bone scale
	// because this is sometimes used to scale up the entire skeleton & skin.
	
	// To get the proper scaler for the translation ( not fully animated scaling )
	double DoubleScale[3];
	if ( !RootCheck ) // -> (!Rootcheck) works - but still messes up root ANGLE
	{
		globalParentMatrix = dagPathParent.inclusiveMatrix();
		
		MTransformationMatrix   matrix( globalParentMatrix );
		matrix.getScale( DoubleScale, MSpace::kWorld);	
	}
	else
	{
		DoubleScale[0] = DoubleScale[1] = DoubleScale[2] = 1.0f;
	}
	BoneScale.X = DoubleScale[0];
	BoneScale.Y = DoubleScale[1];
	BoneScale.Z = DoubleScale[2];

	//
	//  localMatrix.homogenize(); //=> destroys rotation !
	//  #TODO - needs coordinate flipping ?  ( XYZ order in Maya different from Unreal )
	//
	//MQuaternion MayaQuat;
	//MayaQuat = localMatrix; // implied conversion to a normalized quaternion
	//MayaQuat.normalizeIt();
	//MayaQuat.get(newQuat);
	
	// Copy result
	
	ThisMatrix.XPlane.X = mtxI[0][0];
	ThisMatrix.XPlane.Y = mtxI[0][1];
	ThisMatrix.XPlane.Z = mtxI[0][2];
	ThisMatrix.XPlane.W = 0.0f;

	ThisMatrix.YPlane.X = mtxI[1][0];
	ThisMatrix.YPlane.Y = mtxI[1][1];
	ThisMatrix.YPlane.Z = mtxI[1][2];
	ThisMatrix.YPlane.W = 0.0f;

	ThisMatrix.ZPlane.X = mtxI[2][0];
	ThisMatrix.ZPlane.Y = mtxI[2][1];
	ThisMatrix.ZPlane.Z = mtxI[2][2];
	ThisMatrix.ZPlane.W = 0.0f;
	// Trafo..
	ThisMatrix.WPlane.X = mtxI[3][0];
	ThisMatrix.WPlane.Y = mtxI[3][1];
	ThisMatrix.WPlane.Z = mtxI[3][2];
	ThisMatrix.WPlane.W = 1.0f;
	
	// if( RootCheck )MatrixSwapXYZ(ThisMatrix); //#debug RootCheck

	// Extract quaternion rotations
	ThisQuat = FMatrixToFQuat(ThisMatrix);
	
	if (! RootCheck ) // Necessary
	{
		ThisQuat.W = - ThisQuat.W;	
	}
	ThisQuat.Normalize(); // 
}


//   
//   Check if the given object was selected..
//  
UBOOL isObjectSelected( const MDagPath & path )
{
	MStatus status;
	MDagPath sDagPath;

    MSelectionList activeList;
    MGlobal::getActiveSelectionList( activeList );

	MItSelectionList iter ( activeList, MFn::kDagNode );

	while ( !iter.isDone() )
	{
		if ( iter.getDagPath( sDagPath ) )
		{
			//MString PathOne = sDagPath.fullPathName(&status);
			//MString PathTwo = path.fullPathName(&status);
			//DLog.LogfLn(" Selection check: [%s] against <[%s]>",PathOne.asChar(), PathTwo.asChar() );

			if ( sDagPath == path )
				return true;
		}
		iter.next();
	}

	return false;
}





//
// Build the hierarchy in SerialTree
//
int SceneIFC::SerializeSceneTree() 
{
	OurSkin=NULL;
	OurRootBone=NULL;
	
	/*
	// Get/set MAX animation Timing info.
	Interval AnimInfo = TheMaxInterface->GetAnimRange();
	TimeStatic = AnimInfo.Start();
	TimeStatic = 0;

	Frame 0 for reference: 
	if( DoPoseZero ) TimeStatic = 0.0f;

	FrameStart = AnimInfo.Start(); // in ticks..
	FrameEnd   = AnimInfo.End();   // in ticks..
	FrameTicks = GetTicksPerFrame();
	FrameRate  = GetFrameRate();
	*/
	
	FrameStart = GetMayaTimeStart(); // in ticks..
	TimeStatic = max( FrameStart, 0.0f );
	FrameEnd   = GetMayaTimeEnd();   // in ticks..
	FrameTicks = 1.0f;
	FrameRate  = 30.0f;// TODO: Maya framerate retrieval. 

	if( DoForceRate ) FrameRate = PersistentRate; //Forced rate.
	
	SerialTree.Empty();

	int ChildCount = 0;

	//PopupBox("Start store node tree");

	// Store all (relevant) nodes into SerialTree (actually, indices into AXSceneObjects. )
    StoreNodeTree( NULL );
	
	// Update basic nodeinfo contents : parent/child relationships.
	// Beware (later) that in Maya there is alwas a transform object between geometry and joints.
	for(int  i=0; i<SerialTree.Num(); i++)
	{	
		int ParentIndex = -1;
		MFnDagNode DagNode = AxSceneObjects[(int)SerialTree[i].node]; // object to DagNode... 

		MObject ParentNode = DagNode.parent(0);
		// Match to AxSceneObjects
		//int InSceneIdx = AxSceneObjects.Where(ParentNode);
		int InSceneIdx = WhereInMobjectArray( AxSceneObjects, ParentNode );

		ParentIndex = MatchNodeToIndex( (void*) InSceneIdx ); // find parent
		SerialTree[i].ParentIndex = ParentIndex;


		int NumChildren = 0;
		NumChildren = DagNode.childCount();
		SerialTree[i].NumChildren = NumChildren;
		//#todo Note: this may be wrong when we ignoderd children in the selection.

		if( NumChildren > 0 ) ChildCount+=NumChildren;

		// Update selected status
		// SerialTree[i].IsSelected = ((INode*)SerialTree[i].node)->Selected();
		// MAX:  Establish our hierarchy by way of Serialtree indices... upward only.
		// SerialTree[i].ParentIndex = MatchNodeToIndex(((INode*)SerialTree[i].node)->GetParentNode());
		// SerialTree[i].NumChildren = ( ((INode*)SerialTree[i].node)->NumberOfChildren() );
		
		// Print tree node name... note: is a pointer to an inner name, don't change content !
		// char* NodeNamePtr;
		// DtShapeGetName( (int)SerialTree[i].node,  &NodeNamePtr );
		// MayaLog("Node#: %i  Name: [%s] NodeID: %i ",i,NodeNamePtr, (int)SerialTree[i].node );

		MDagPath ObjectPath;
		DagNode.getPath( ObjectPath );
		// stat = skinCluster.getPathAtIndex(index,skinPath);
		// Check whether selected:

		if( isObjectSelected ( ObjectPath ))
			SerialTree[i].IsSelected = 1;

	}

	// PopupBox("Serialtree num: %i children: %i",SerialTree.Num(), ChildCount );
	return SerialTree.Num();
}


//
//   See if there's a skincluster in this scene that influences MObject...
//
INT FindSkinCluster( MObject &TestObject, SceneIFC* ThisScene )
{
	INT SoftSkin = -1;

	for( DWORD t=0; t<AxSkinClusters.length(); t++ )
	{
		// 'physique / skincluster modifier' detection		
		
		// MFnDependencyNode SkinNode = (MFnDependencyNode) SkinObject;
		// PopupBox("Skin search: node name: [%s] ", SkinNode.typeName().asChar() );
		
		if( AxSkinClusters[t].apiType() == MFn::kSkinClusterFilter )
		{
			// For each skinCluster node, get the list of influenced objects			
			MFnSkinCluster skinCluster(AxSkinClusters[t]);
			MDagPathArray infs;
			MStatus stat;
		    int nInfs = skinCluster.influenceObjects(infs, &stat);

			//PopupBox("Skincluster found, looking for affected geometry");

			if( nInfs )
			{
				int nGeoms = skinCluster.numOutputConnections();
				for (int ii = 0; ii < nGeoms; ++ii) 
				{
					int index = skinCluster.indexForOutputConnection(ii,&stat);
					//CheckError(stat,"Error getting geometry index.");					

					// Get the dag path of the ii'th geometry.
					MDagPath skinPath;
					stat = skinCluster.getPathAtIndex(index,skinPath);						
					//CheckError(stat,"Error getting geometry path.");
					
					MObject SkinGeom = skinPath.node();

					//?? DagNode in skinPath ????					
					if ( SkinGeom == TestObject ) //TestObjectode == DagNode ) // compare node & node from path
					{
						//PopupBox("Skincluster influence found for -  %s \n", skinPath.fullPathName().asChar() );
						return t;
					}
					// DLog.Logf("Geometry pathname: # [%3i] -  %s \n",ii,skinPath.fullPathName().asChar() );
				}
			}
		}	
	}
	return -1;
}
			/*
			if (1)	{
						// For each skinCluster node, get the list of influencdoe objects
						//
						MFnSkinCluster skinCluster(Object);
						MDagPathArray infs;
						MStatus stat;
						unsigned int nInfs = skinCluster.influenceObjects(infs, &stat);
						//CheckError(stat,"Error getting influence objects.");

						if( 0 == nInfs ) 
						{
							stat = MS::kFailure;
							//CheckError(stat,"Error: No influence objects found.");
						}
						
						// loop through the geometries affected by this cluster
						//
						unsigned int nGeoms = skinCluster.numOutputConnections();
						for (size_t ii = 0; ii < nGeoms; ++ii) 
						{
							unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);
							//CheckError(stat,"Error getting geometry index.");

							// Get the dag path of the ii'th geometry.
							//
							MDagPath skinPath;
							stat = skinCluster.getPathAtIndex(index,skinPath);
							//CheckError(stat,"Error getting geometry path.");

							DLog.Logf("Geometry pathname: # [%3i] -  %s \n",ii,skinPath.fullPathName().asChar() );
						}
					}
			*/



int SceneIFC::GetSceneInfo()
{
	// DLog.Logf(" Total nodecount %i \n",SerialTree.Num());

	MStatus stat;

	PhysiqueNodes = 0;
	TotalBones    = 0;
	TotalDummies  = 0;
	TotalBipBones = 0;
	TotalMaxBones = 0;

	GeomMeshes = 0;
	INT MayaJointCount = 0;

	LinkedBones = 0;
	TotalSkinLinks = 0;

	if (SerialTree.Num() == 0) return 0;

	SetMayaSceneTime( TimeStatic );	

	OurSkins.Empty();	

	DebugBox("Go over serial nodes & set flags");

	for( int i=0; i<SerialTree.Num(); i++)
	{					
		MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
		MObject	Object = DagNode.object();

		// A mesh or geometry ? #TODO check against untextured geometry...
		SerialTree[i].HasMesh = ( Object.hasFn(MFn::kMesh) ? 1:0 );
		SerialTree[i].HasTexture = SerialTree[i].HasMesh;

		// Check for a bone candidate.
		SerialTree[i].IsBone = ( Object.apiType() == MFn::kJoint ) ? 1:0;

		if( SerialTree[i].IsBone ) 
			MayaJointCount++;
		
		// Check Mesh against all SerialTree items for a skincluster ('physique') modifier:		
		INT IsSkin = ( Object.apiType() == MFn::kMesh) ? 1: 0; //( Object.apiType() == MFn::kMesh ) ? 1 : 0;

		SerialTree[i].ModifierIdx = -1; // No cluster.			
		INT HasCluster = 0;
		if( IsSkin) 
		{
			INT Cluster = FindSkinCluster( Object, this );
			HasCluster = ( Cluster >= 0 ) ? 1 : 0;
			if ( Cluster >= 0 ) 
			{
				SerialTree[i].ModifierIdx = Cluster; //Skincluster index..
			}
		}
				
		UBOOL SkipThisMesh = false;
		
		GeomMeshes += SerialTree[i].HasMesh;

		//UBOOL SkipThisMesh = ( this->DoSkipSelectedGeom && ((INode*)SerialTree[i].node)->Selected() );

		if ( IsSkin && !SkipThisMesh  )  // && DoPhysGeom  (HasCluster > -1)
		{
			// SkinCluster/Physique skins dont act as bones.
			SerialTree[i].IsBone = 0; 
			SerialTree[i].IsSkin = 1;
			SerialTree[i].IsSmooth = HasCluster;

			// Multiple physique nodes...
			SkinInf NewSkin;			

			NewSkin.Node = SerialTree[i].node;
			NewSkin.IsSmoothSkinned = HasCluster;
			NewSkin.IsTextured = 1;
			NewSkin.SceneIndex = i;
			NewSkin.SeparateMesh = 0;

			OurSkins.AddItem(NewSkin);

			// Prefer the first or the selected skin if more than one encountered.
			/*
			if ((((INode*)SerialTree[i].node)->Selected()) || (PhysiqueNodes == 0))
			{
					OurSkin = SerialTree[i].node;
			}
			*/

			PhysiqueNodes++;
		}
		else if (0) // (SerialTree[i].HasTexture && SerialTree[i].HasMesh && DoTexGeom  && !SkipThisMesh )
		// Any nonphysique mesh object acts as its own bone ? => never the case for Maya.
		{						
			SerialTree[i].IsBone = 1; 
			SerialTree[i].IsSkin = 1;

			// Multiple physique nodes...
			SkinInf NewSkin;			

			NewSkin.Node = SerialTree[i].node;
			NewSkin.IsSmoothSkinned = 0;
			NewSkin.IsTextured = 1;
			NewSkin.SceneIndex = i;
			NewSkin.SeparateMesh = 1;

			OurSkins.AddItem(NewSkin);
			/*
			// Prefer the first or the selected skin if more than one encountered.
			TSTR NodeName(((INode*)SerialTree[i].node)->GetName());
			DebugBox("Textured geometry mesh: %s",NodeName);
			*/
		}
	}


	//#SKEL - doesn't work correct in Maya yet.	
	// IF no joints encountered, use parent-child relations as ersatz bones - a la 3DS MAX ...

	if( NOJOINTTEST )
	{
		INT NewJoints = 0;
		if( ( MayaJointCount == 0 ) && ( GeomMeshes > 0 ) )
		{		
			for( int i=0; i<SerialTree.Num(); i++)
			{
				if( SerialTree[i].HasMesh && !SerialTree[i].IsSmooth )
				{
					SerialTree[i].IsBone = 1;
					SerialTree[i].InSkeleton = 1;				
					NewJoints++;
				}
			}
		}
	}

	// #SKEL
	// PopupBox("-> Meshes: %i  Joint count: %i  New Joint Count: %i Ourskins: %i ",GeomMeshes,MayaJointCount,NewJoints,OurSkins.Num()); 


	/*
	//
	// Cull from skeleton all dummies that don't have any physique links to it and that don't have any children.
	//
	INT CulledDummies = 0;
	if (this->DoCullDummies)
	{
		for( int i=0; i<SerialTree.Num(); i++)
		{	
			if ( 
			   ( SerialTree[i].IsDummy ) &&  
			   ( SerialTree[i].LinksToSkin == 0)  && 
			   ( ((INode*)SerialTree[i].node)->NumberOfChildren()==0 ) && 
			   ( SerialTree[i].HasTexture == 0 ) && 
			   ( SerialTree[i].IsBone == 1 )
			   )
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].IsCulled = 1;
				CulledDummies++;
			}
		}
		PopupBox("Culled dummies: %i",CulledDummies);
	}
	*/
	
	if( DEBUGFILE )
	{
		char LogFileName[] = ("\\GetSceneInfo.LOG");
		DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	for(INT i=0; i<SerialTree.Num(); i++)
	{	
		// 'physique / skincluster modifier' detection
		MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 

		// No skeleton available yet!
		// INT IsRoot = ( TempActor.MatchNodeToSkeletonIndex( (void*) i ) == 0 );

		// If mesh, determine skin size.
		INT MeshFaces = 0;
		INT MeshVerts = 0;
		if( SerialTree[i].IsSkin )
		{
			MFnMesh MeshFunction( AxSceneObjects[ (int)SerialTree[i].node ], &stat );
			MeshVerts = MeshFunction.numVertices();
			MeshFaces = MeshFunction.numPolygons();
		}

		//char NodeName[300]; NodeName = (((INode*)SerialTree[i].node)->GetName());
		DLog.Logf("Node: %4i - %22s %22s Dummy: %2i Bone:%2i Skin:%2i  Smooth:%2i PhyLk:%4i NodeID %10i ModifierIdx: %3i Parent %4i Children %4i HasTexture %i Culled %i Sel %i \n",i,
			DagNode.name().asChar(), //NodeName
			DagNode.typeName().asChar(), //NodeName
			SerialTree[i].IsDummy,
			SerialTree[i].IsBone,
			SerialTree[i].IsSkin,
			SerialTree[i].IsSmooth,
			SerialTree[i].LinksToSkin,
			(INT)SerialTree[i].node,
			SerialTree[i].ModifierIdx,
			SerialTree[i].ParentIndex,
			SerialTree[i].NumChildren,			
			SerialTree[i].HasTexture,		
			SerialTree[i].IsCulled,
			SerialTree[i].IsSelected			
			);

			//DLog.Logf("Mesh stats:  Faces %4i  Verts %4i \n", MeshFaces, MeshVerts );
	}

	

	{for( DWORD i=0; i<AxSkinClusters.length(); i++)
	{
		MFnDependencyNode Node( AxSkinClusters[i]);
	
		MObject SkinObject = AxSkinClusters[i];

		MFnSkinCluster skinCluster(SkinObject);

		DLog.Logf("Node: %4i - name: [%22s] type: [%22s] \n",
			i,
			Node.name().asChar(), 
			Node.typeName().asChar()			
			);


		DWORD nGeoms = skinCluster.numOutputConnections();

		if ( nGeoms == 0 ) 
			DLog.LogfLn("  > No influenced objects for this skincluster.");

		for (DWORD ii = 0; ii < nGeoms; ++ii) 
		{
			unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);
			// get the dag path of the ii'th geometry
			MDagPath bonePath;
			stat = skinCluster.getPathAtIndex(index,bonePath);

			// MFnDependencyNode Node( skinPath );
			// MObject BoneObject = Node.object();
			// MObject BoneGeom = bonePath.node();

			DLog.LogfLn("  > Influenced object#: %4i - name: [%s] type: [%s] ",
			ii,
			bonePath.fullPathName().asChar(), 
			//BoneGeom.typeName().asChar()			
			" - "
			);

		}
	}}

        DLog.LogfLn(" ");
	DLog.Close();
	
	TreeInitialized = 1;	
	
	return 0;
}



// Skin exporting: from example plugin "ExportSkinClusterDataCmd"

#define CheckError(stat,msg)		\
	if ( MS::kSuccess != stat ) {	\
		MayaLog(msg);			\
		continue;					\
	}



/*
int DigestSoftSkin(VActor *Thing )
{
	
	size_t count = 0;
	
	// Iterate through graph and search for skinCluster nodes
	//
	MItDependencyNodes iter( MFn::kInvalid);
	MayaLog("Start Iterating graph....");

	for ( ; !iter.isDone(); iter.next() ) 
	{	

		MObject object = iter.item();
		if (object.apiType() == MFn::kSkinClusterFilter) 
		{
			count++;
			
			// For each skinCluster node, get the list of influencdoe objects
			//
			MFnSkinCluster skinCluster(object);
			MDagPathArray infs;
			MStatus stat;
			unsigned int nInfs = skinCluster.influenceObjects(infs, &stat);
			CheckError(stat,"Error getting influence objects.");

			if( 0 == nInfs ) 
			{
				stat = MS::kFailure;
				CheckError(stat,"Error: No influence objects found.");
			}
			
			// loop through the geometries affected by this cluster
			//
			unsigned int nGeoms = skinCluster.numOutputConnections();
			for (size_t ii = 0; ii < nGeoms; ++ii) 
			{
				unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);
				CheckError(stat,"Error getting geometry index.");

				// Get the dag path of the ii'th geometry.
				//
				MDagPath skinPath;
				stat = skinCluster.getPathAtIndex(index,skinPath);
				CheckError(stat,"Error getting geometry path.");

				// iterate through the components of this geometry (vertices)
				//
				MItGeometry gIter(skinPath);

				//
				// print out the path name of the skin, vertexCount & influenceCount
				//
				// #TODO
				// fprintf(file,"%s [vertexcount %d] [Influences %d] \r\n",skinPath.partialPathName().asChar(), gIter.count(),	nInfs);
				
				// print out the influence objects
				//
				for (size_t kk = 0; kk < nInfs; ++kk) 
				{
					// #TODO
					MayaLog("[%s] \r\n",infs[kk].partialPathName().asChar());
				}
				// fprintf(file,"\r\n");
			
				for (  ; !gIter.isDone(); gIter.next() ) 
				{
					MObject comp = gIter.component(&stat);
					CheckError(stat,"Error getting component.");

					// Get the weights for this vertex (one per influence object)
					//
					MFloatArray wts;
					unsigned int infCount;
					stat = skinCluster.getWeights(skinPath,comp,wts,infCount);
					CheckError(stat,"Error getting weights.");
					if (0 == infCount) 
					{
						stat = MS::kFailure;
						CheckError(stat,"Error: 0 influence objects.");
					}

					// Output the weight data for this vertex
					//
					float TotalWeight = 0.0f;

					// fprintf(file,"[%4d] ",gIter.index());

					for (unsigned int jj = 0; jj < infCount ; ++jj ) 
					{
						float BoneWeight = wts[jj];
						if( BoneWeight != 0.0f )
						{
							TotalWeight+=BoneWeight;
							// fprintf(file,"Bone: %4i Weight: %9f ",jj,wts[jj]);
						}
					}
					// fprintf(file," Total: %9.5f",TotalWeight);
					// fprintf(file,"\r\n");
				}
			}
		}
	}

	//if (0 == count) {
		//displayError("No skinClusters found in this scene.");
	//}
	//fclose(file);
	//return MS::kSuccess;

	return 1;
}
*/

//
// Get total number of blend curves
// Each blend shape can have multiple curves
// Looking for total number of curves 
//
INT GetTotalNumOfBlendCurves()
{
	INT TotalNumCurves=0;;
	for (INT I=0; I<AxBlendShapes.length(); ++I)
	{
		MFnBlendShapeDeformer fn(AxBlendShapes[I]);
		TotalNumCurves += fn.numWeights();
	}

	return TotalNumCurves;
}

// 
// Get alias name from the input attribute and copy to output
// 
int GetAliasName(const char * AttributeName, char * Output, INT OutputLen)
{
	if ( AttributeName && Output && OutputLen>0 )
	{
		MString ResultString;
		MString QueryString = "aliasAttr -q ";
		QueryString += AttributeName;
		Memzero(Output, OutputLen);
		
		MStatus Result = MGlobal::executeCommand(QueryString, ResultString);
		if (Result == MS::kSuccess)
		{
			// if there is colon, that's because of namespace
			const char* Colon = strrchr(ResultString.asChar(), ':');
			// remove namespace
			if (Colon)
			{
				strcpysafe( Output, Colon+sizeof(char), OutputLen );			
			}
			else
			{
				strcpysafe( Output, ResultString.asChar(), OutputLen );			
			}

			return (Output[0]!=0);
		}
	}

	return 0;
}

// 
// Initialize Curves
// Allocate memory for each curve
// And assign name for each curve
// Each curve for each blendshape, it gets one BlendCurve
// Each curve gets Frame count of weights with name
// 
int SceneIFC::InitializeCurve(VActor *Thing, INT TotalFrameCount )
{
	INT TotalNumCurves = GetTotalNumOfBlendCurves();

	// allocate memory for # of curves
	Thing->RawCurveKeys.Empty();
	Thing->RawCurveKeys.AddZeroed(TotalNumCurves);

	// allocate framecount for each curve
	for (INT I=0;I<TotalNumCurves; ++I)
	{
		Thing->RawCurveKeys[I].RawWeightKeys.Empty();
		Thing->RawCurveKeys[I].RawWeightKeys.AddZeroed(TotalFrameCount);
	}

	INT CurveIdx=0;
	// for each curve, initialize information, such as name
	for (INT I=0; I<AxBlendShapes.length(); ++I)
	{
		MObject Object = AxBlendShapes[I]; 
		// if blend shape exists
		if ( Object.hasFn(MFn::kBlendShape) )
		{
			MFnBlendShapeDeformer fn(Object);

			for (INT J=0; J<fn.numWeights(); ++J)
			{
				char AttributeName[MAX_PATH], OutputName[128];

				sprintf_s(AttributeName,"%s.weight[%d]",fn.name().asChar(),J);		

				if (GetAliasName(AttributeName, OutputName, 128))
				{
					strcpysafe(Thing->RawCurveKeys[CurveIdx].RawCurveName, OutputName, 128);
					CurveIdx++;
				}
				else
				{
					PopupBox("[BlendCurve] Failed to get alias name for [%s]", AttributeName);
				}
			}
		}
	}

	// # of the information I gathered for each curve does not match up TotalNumCurve
	// Some of information isn't right
	if (CurveIdx != TotalNumCurves)
	{
		return 0;
	}

	// verify if they have same name within the mesh
	for (INT I=0;I<TotalNumCurves; ++I)
	{
		for (INT J=I+1; J<TotalNumCurves; ++J)
		{
			// same name found, can't do it
			if (!strcmp(Thing->RawCurveKeys[I].RawCurveName, Thing->RawCurveKeys[J].RawCurveName))
			{
				PopupBox("[BlendCurve] Failed to export Curve - Same name found : [%s]", Thing->RawCurveKeys[I].RawCurveName);
				return 0;
			}
		}
	}

	return 1;
}
//
// Digest a single animation. called after hierarchy/nodelist digested.
//
int SceneIFC::DigestAnim(class VActor *Thing,char *SequenceName,char *RangeString)
{
	if( DEBUGFILE )
	{
		DLog.Close();//precaution
		char LogFileName[] = ("\\DigestAnim.LOG");
		DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	INT TimeSlider = 0;
	INT AnimEnabled = 1;

	FLOAT TimeStart, TimeEnd;
	TimeStart = 0.0f;
	TimeEnd   = 0.0f;

	TimeStart = GetMayaTimeStart();
	TimeEnd = GetMayaTimeEnd();

	//#TODO
	INT FirstFrame = (INT)TimeStart;
	INT LastFrame = (INT)TimeEnd;

	// Parse a framerange that's identical to or a subset (possibly including same frame multiple times) of the slider range.
	ParseFrameRange( RangeString, FirstFrame, LastFrame );
	OurTotalFrames = FrameList.GetTotal();	

	//PopupBox(" Animation range: start %f  end %f  frames %i ", TimeStart, TimeEnd, OurTotalFrames );

	Thing->RawAnimKeys.Empty();
	Thing->RawNumFrames = OurTotalFrames; 
	Thing->RawNumBones  = TotalBones;
	Thing->RawAnimKeys.SetSize( OurTotalFrames * TotalBones );
	Thing->RawNumBones  = TotalBones;
	Thing->FrameTotalTicks = OurTotalFrames; // * FrameTicks; 
	Thing->FrameRate = 30.0f; // GetFrameRate(); //FrameRate;  #TODO proper Maya framerate retrieval.

	// PopupBox(" Frame rate %f  Ticks per frame %f  Total frames %i FrameTimeInfo %f ",(FLOAT)GetFrameRate(), (FLOAT)FrameTicks, (INT)OurTotalFrames, (FLOAT)Thing->FrameTotalTicks );
	// PopupBox(" TotalFrames/BetaNumFrames: %i ",Thing->RawNumFrames);

	_tcscpy_s(Thing->RawAnimName,CleanString( SequenceName ));

	//
	// Rootbone is in tree starting from RootBoneIndex.
	//

	int KeyIndex  = 0;
	int FrameCount = 0;
	int TotalAnimNodes = SerialTree.Num()-RootBoneIndex; 
	INT TotalNumCurves = GetTotalNumOfBlendCurves();

	// initialize curve data
	if (InitializeCurve(Thing, OurTotalFrames) == 0)
	{
		// If failed, just clear RawCurveKeys, so that it's not used
		PopupBox("Failed to initialize Curve Data.");
		TotalNumCurves = 0;
		Thing->RawCurveKeys.Empty();
	}

	DebugBox("Start digesting the animation frames in the FrameList");
	
	for( INT t=0; t<FrameList.GetTotal(); t++)		
	{
		FLOAT TimeNow = FrameList.GetFrame(t); // * FrameTicks;
		SetMayaSceneTime( TimeNow );

		for (int i = 0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
		{
			if ( SerialTree[i].InSkeleton ) 
			{			
				MFnDagNode DagNode  = AxSceneObjects[ (int)SerialTree[i].node ]; 
				MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 

				// Check whether root bone
				INT RootCheck = ( Thing->MatchNodeToSkeletonIndex( (void*) i ) == 0) ? 1:0 ;			

				DLog.LogfLn("#DIGESTANIM Skeletal bone name [] num %i parent %i  NumChildren %i  Isroot %i RootCheck %i",i,(int)SerialTree[i].ParentIndex,SerialTree[i].NumChildren, (i != RootBoneIndex)?0:1,RootCheck );

				FMatrix LocalMatrix;
				FQuat   LocalQuat;
				FVector BoneScale(1.0f,1.0f,1.0f);

				// RootCheck
				// if( RootCheck && t==0) PopupBox(" Root in Anim digest: serialtree [%i] ",i);
				RetrieveTrafoForJoint( JointObject, LocalMatrix, LocalQuat, RootCheck, BoneScale );

				// Raw translation
				FVector Trans( LocalMatrix.WPlane.X, LocalMatrix.WPlane.Y, LocalMatrix.WPlane.Z );				
				// Incorporate matrix scaling - for translation only.
				Trans.X *= BoneScale.X;
				Trans.Y *= BoneScale.Y;
				Trans.Z *= BoneScale.Z;

				FQuat Orient;
				//if ( RootCheck ) 				
				Orient = FQuat(LocalQuat.X,LocalQuat.Y,LocalQuat.Z,LocalQuat.W);
			
				Thing->RawAnimKeys[KeyIndex].Orientation = Orient;
				Thing->RawAnimKeys[KeyIndex].Position = Trans;

				KeyIndex++;
			}			
		}

		if ( TotalNumCurves > 0 )
		{
			INT CurveIdx=0;
			// get the curve information out
			for (INT I=0; I<AxBlendShapes.length(); ++I)
			{
				MObject Object = AxBlendShapes[I]; 
				// if blend shape exists
				if ( Object.hasFn(MFn::kBlendShape) )
				{
					MFnBlendShapeDeformer fn(Object);

					for (INT J=0; J<fn.numWeights(); ++J)
					{
						Thing->RawCurveKeys[CurveIdx].RawWeightKeys[FrameCount] = fn.weight(J);
						DLog.LogfLn("#DIGESTANIM Curve name (%s), index(%i), framecount(%i), weight(%f)", Thing->RawCurveKeys[CurveIdx].RawCurveName, CurveIdx, FrameCount, Thing->RawCurveKeys[CurveIdx].RawWeightKeys[FrameCount]);
						++CurveIdx;
					}
				}
			}
		}
		
		FrameCount++;
	}	

	DLog.Close();

	return 1;
}


//
// Digest _and_ write vertex animation.
//

int SceneIFC::WriteVertexAnims(VActor *Thing, char* DestFileName, char* RangeString )
{

    //#define DOLOGVERT 0
	//if( DOLOGVERT )
	//{
	//	TSTR LogFileName = _T("\\ActXVert.LOG");
	//	AuxLog.Open(LogPath,LogFileName, 1 );//DEBUGFILE);
	//}

	INT FirstFrame = (INT)FrameStart / FrameTicks; // Convert ticks to frames. FrameTicks set in 
	INT LastFrame = (INT)FrameEnd / FrameTicks;

	ParseFrameRange( RangeString, FirstFrame, LastFrame );

	OurTotalFrames = FrameList.GetTotal();	
	Thing->RawNumFrames = OurTotalFrames; 
	Thing->RawAnimKeys.SetSize( OurTotalFrames * TotalBones );
	Thing->RawNumBones  = TotalBones;
	Thing->FrameTotalTicks = OurTotalFrames * FrameTicks; 
	Thing->FrameRate = 1.0f; //GetFrameRate(); //FrameRate - unused for vert anim....

	INT FrameCount = 0;
	INT AppendSeekLocation = 0;

	VertexMeshHeader MeshHeader;
	Memzero( &MeshHeader, sizeof( MeshHeader ) );
	
	VertexAnimHeader AnimHeader;
	Memzero( &AnimHeader, sizeof( AnimHeader ) );
	
	TArray<VertexMeshTri> VAFaces;
	TArray<FMeshVert> VAVerts;
	TArray<FVector> VARawVerts;

	// Get 'reference' triangles; write the _d.3d file. Scene time of sampling actually unimportant.
	OurScene.DigestSkin(&TempActor );  
	
	// Gather triangles
	for( INT t=0; t< TempActor.SkinData.Faces.Num(); t++)
	{
		VertexMeshTri NewTri;

		NewTri.Color = 0;
		NewTri.Flags = 0;

		NewTri.iVertex[0] = TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[0] ].PointIndex;
		NewTri.iVertex[1] = TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[1] ].PointIndex;
		NewTri.iVertex[2] = TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[2] ].PointIndex;

		FLOAT TexScaler=255.99f; //!

		for( INT v=0; v<3; v++)
		{
			NewTri.Tex[v].U = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].UV.U );
			NewTri.Tex[v].V = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].UV.V );

			NewTri.Tex[v].U1 = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].ExtraUVs[0].U );
			NewTri.Tex[v].V1 = (INT)( TexScaler * TempActor.SkinData.Wedges[ TempActor.SkinData.Faces[t].WedgeIndex[v] ].ExtraUVs[0].V );
		}
		
		NewTri.TextureNum = TempActor.SkinData.Faces[t].MatIndex;

		// #TODO - Verify that this is the proper flag assignment for things like alpha, masked, doublesided, etc...
		if( NewTri.TextureNum < Thing->SkinData.RawMaterials.Num() )
			NewTri.Flags = Thing->SkinData.Materials[NewTri.TextureNum].PolyFlags; 

		VAFaces.AddItem( NewTri );
	}

	INT RefSkinPoints = TempActor.SkinData.Points.Num();

	// Write.. to 'model' file DestFileName_d.3d
	if( VAFaces.Num() && (OurScene.DoAppendVertex == 0) )
	{
		FastFileClass OutFile;
		char OutPath[MAX_PATH];
		sprintf_s(OutPath,"%s\\%s_d.3d",(char*)to_path,(char*)DestFileName);		
		//PopupBox("OutPath for _d file: %s",OutPath); 
		if ( OutFile.CreateNewFile(OutPath) != 0) 
			ErrorBox( "Vertex Anim File creation error. ");

		MeshHeader.NumPolygons = VAFaces.Num();
		MeshHeader.NumVertices = TempActor.SkinData.Points.Num();
		MeshHeader.Unused[0] = 117; // "serial" number for ActorX-generated vertex data.

		// Write header.
		OutFile.Write( &MeshHeader, sizeof(MeshHeader));

		// Write triangle data.
		for( INT f=0; f< VAFaces.Num(); f++)
		{
			OutFile.Write( &(VAFaces[f]), sizeof(VertexMeshTri) );
		}		

		OutFile.CloseFlush();

		if( OutFile.GetError()) ErrorBox("Vertex skin save error.");

	}

    //
	// retrieve mesh for every frame & write to (to_skinfile_a.3d. Optionally append it to existing.
	//

	for( INT f=0; f<FrameList.GetTotal(); f++)		
	{
		FLOAT TimeNow = FrameList.GetFrame(f) * FrameTicks;

		// Advance progress bar; obey bailout. (MAX specific.)
		/*
		INT Progress = (INT) 100.0f * ( f/(FrameList.GetTotal()+0.01f) );
		TheMaxInterface->ProgressUpdate(Progress); 

		// give user opportunity to cancel
		if (TheMaxInterface->GetCancel()) 
		{
			INT retval = MessageBox(TheMaxInterface->GetMAXHWnd(), _T("Really Cancel"),
				_T("Question"), MB_ICONQUESTION | MB_YESNO);
			if (retval == IDYES)
				break;
			else if (retval == IDNO)
				TheMaxInterface->SetCancel(FALSE);
		}
		*/
	
		// Capture current mesh as a 'reference pose'  at 'TimeNow'.
		TimeStatic = TimeNow;		
		OurScene.DigestSkin(&TempActor ); 
		for( INT v=0; v< Thing->SkinData.Points.Num(); v++ )
		{			
			VARawVerts.AddItem( Thing->SkinData.Points[v].Point ); //Store full 3d point, since we may be rescaling stuff later.
		}

		// Add raw skin verts to buffer; we don't care about bones.		

		FrameCount++;
	} // Time.

	//
	// Convert into VAVerts (32-bit packed) and write out...
	//
	if( VARawVerts.Num() )
	{
		INT FileError = 0;
		
		if( OurScene.DoScaleVertex )
		{
			//
			// AUTO-scale (all 3 dimensions) if it's 0 ?
			//
			if( OurScene.VertexExportScale <= 0.f )
				OurScene.VertexExportScale = 1.0f;

			for( INT v=0; v<VARawVerts.Num(); v++)
			{
				VARawVerts[v] *= OurScene.VertexExportScale;
			}
		}

		VAVerts.Empty();

		// Convert.
		for( INT v=0; v<VARawVerts.Num(); v++)
		{
			FMeshVert VANew( VARawVerts[v]);
			VAVerts.AddItem( VANew );
		}

		//
		// Write.
		//
		FastFileClass OutFile;
		char OutPath[MAX_PATH];
		sprintf_s(OutPath,"%s\\%s_a.3d",(char*)to_path,(char*)DestFileName);				

		if( ! OurScene.DoAppendVertex )
		{
			if ( OutFile.CreateNewFile(OutPath) != 0) // Error!
			{
				ErrorBox( "Vertex Anim File creation error. ");
				FileError = 1;
			}
		}
		else
		{
			if ( OutFile.OpenExistingFile(OutPath) != 0) // Error!
			{
				ErrorBox( "File appending error. ");
				FileError = 1;
			}
			// Need to explicitly move file ptr to end for appending.
			AppendSeekLocation = OutFile.GetSize(); 
			if( AppendSeekLocation <= 0 )
			{
				ErrorBox(" File appending error: file on disk has no data.");
				FileError = 1;
			}
		}

		if( ! FileError )
		{
			AnimHeader.FrameSize = ( WORD )( RefSkinPoints * sizeof( DWORD ) );
			AnimHeader.NumFrames = FrameCount;

			// Write header if we're not appending.
			if( ! OurScene.DoAppendVertex)
			{
				OutFile.Write( &AnimHeader, sizeof(AnimHeader));
			}
			else
			{				
				VertexAnimHeader ExistingAnimHeader;
				ExistingAnimHeader.FrameSize = 0;
				ExistingAnimHeader.NumFrames = 0;

				// Overwrite header, but then skip existing data.
				OutFile.Seek( 0 );
				OutFile.Read( &ExistingAnimHeader, sizeof(ExistingAnimHeader));
				AnimHeader.NumFrames  += ExistingAnimHeader.NumFrames;
				if( AnimHeader.FrameSize != ExistingAnimHeader.FrameSize )
				{
					ErrorBox("File appending error: existing  per-frame data size differs (was %i, expected %i)" ,ExistingAnimHeader.FrameSize,  AnimHeader.FrameSize );
					FileError = 1;
					return 0;
				}
				OutFile.Seek( 0 );
				OutFile.Write( &AnimHeader, sizeof(AnimHeader));
				OutFile.Flush();
				// Move to end of file.
				OutFile.Seek( AppendSeekLocation );
			}

			// Write animated vertex data (all frames, all verts).
			for( INT f=0; f< VAVerts.Num(); f++)
			{
				OutFile.Write( &(VAVerts[f]), sizeof(FMeshVert));				
			}	

			OutFile.CloseFlush();

			if( OutFile.GetError()) ErrorBox("Vertex anim save error.");
		}

	}

	//if( DOLOGVERT )
	//{
	//   //PopupBox(" Digested %i vertex animation frames total.  Skin ponts total  %i -appended at : [%i]  ", FrameCount, RefSkinPoints, AppendSeekLocation ); //#EDN
	//	AuxLog.Logf(" Digested %i vertex animation frames total.  Skin ponts total  %i ", FrameCount, RefSkinPoints );
	//	AuxLog.Close();
	//}

	return ( RefSkinPoints > 0 ) ? FrameCount : 0 ;
}





			
//
// Digest points, weights, wedges, faces for a skeletal mesh.
//
// PCF START
//	maya triangulation
// PCF STOP
int	SceneIFC::ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin )
{
	MStatus	stat;
	MObject SkinObject = AxSceneObjects[ (INT)SkinNode ];
	MFnDagNode currentDagNode( SkinObject, &stat );



	cerr << "		process mesh (" << currentDagNode.name().asChar() << ")"<< endl;


	MFnMesh MeshFunction( SkinObject, &stat );
	// #todo act on SmoothSkin flag
	if ( stat != MS::kSuccess ) 
	{
			PopupBox(" Mesh methods could not be created for MESH %s",currentDagNode.name(&stat).asChar() );
			return 0;
	}

	MFnDependencyNode	Node(SkinObject);

	// Retrieve the first dag path.
    MDagPath ShapedagPath;
    stat = currentDagNode.getPath( ShapedagPath );



	// If we are doing world space, then we need to initialize the 
	// Mesh with a DagPath and not just the node

	MFloatPointArray	Points;	
	// Get points array & iterate vertices.
	// MeshFunction.getPoints( Points, MSpace::kWorld); // can also get other spaces!!				
	MeshFunction.setObject( ShapedagPath );
	MeshFunction.getPoints( Points, MSpace::kWorld ); // MSpace::kTransform);  //KWorld is wrong ?...

	int ParentJoint = TreeIndex;

	// Traverse hierarchy to find valid parent joint for this mesh. #DEBUG - make NoExport work !
	if( ParentJoint > -1) SerialTree[ParentJoint].NoExport = 0; //#SKEL
	
	while( (ParentJoint > 0)  &&  ((!SerialTree[ParentJoint].InSkeleton ) || ( SerialTree[ParentJoint].NoExport ) ) )
	{
		ParentJoint = SerialTree[ParentJoint].ParentIndex;
	}

	if( ParentJoint < 0) ParentJoint = 0;
	
	INT MatchedNode = Thing->MatchNodeToSkeletonIndex( (void*)ParentJoint ); // SkinNode ) ;

	// Not linked to a joint?
	if( (MatchedNode < 0) )
	{
		MatchedNode = 0;
	}

	INT SkinClusterVertexCount = 0;	

	if( !SmoothSkin )
	{
		for( int PointIndex = 0; PointIndex < MeshFunction.numVertices(); PointIndex++)
		{
			MFloatPoint	Vertex;
			// Get the vertex info.
			Vertex = Points[PointIndex];
			
			// Max: SkinLocal brings the skin 3d vertices into world space.
			// Point3 VxPos = OurTriMesh->getVert(i)*SkinLocalTM;

			VPoint NewPoint;
			// #inversion..
			NewPoint.Point.X =  Vertex.x; // -z
			NewPoint.Point.Y =  Vertex.y; // -x
			NewPoint.Point.Z =  Vertex.z; //  y

			LocalSkin.Points.AddItem(NewPoint); 		

			DLog.Logf(" # Skin vertex %i   Position: %f %f %f \n",PointIndex,NewPoint.Point.X,NewPoint.Point.Y,NewPoint.Point.Z);

			VRawBoneInfluence TempInf;
			TempInf.PointIndex = PointIndex; // Raw skinvertices index
			TempInf.BoneIndex  = MatchedNode; // Bone is it's own node.
			TempInf.Weight     = 1.0f;

			LocalSkin.RawWeights.AddItem(TempInf);  					
		}
	}
	else // Smooth skin (SkinCluster)
	{

		// First the points, then the weights ??????????? &
		for( int PointIndex = 0; PointIndex < MeshFunction.numVertices(); PointIndex++)
		{
				MFloatPoint	Vertex;
				// Get the vertex info.
				Vertex = Points[PointIndex];
				
				// Max: SkinLocal brings the skin 3d vertices into world space.
				// Point3 VxPos = OurTriMesh->getVert(i)*SkinLocalTM;

				VPoint NewPoint;
				// #inversion
				NewPoint.Point.X =  Vertex.x; // -z
				NewPoint.Point.Y =  Vertex.y; // -x
				NewPoint.Point.Z =  Vertex.z; //  y

				LocalSkin.Points.AddItem(NewPoint); 		
		}

		// Current object:"SkinObject/ShapedagPath".
		MFnSkinCluster skinCluster( AxSkinClusters[SerialTree[TreeIndex].ModifierIdx] );
		// SerialTree[TreeIndex].ModifierIdx holds the skincluster 'modifier', from which we can
		// obtain the proper weighted vertices.

		MDagPathArray infs;
		MStatus stat;
		unsigned int nInfs = skinCluster.influenceObjects(infs, &stat);

		// The array of nodes that influence the vertices should be traceable back through their bone names (or dagpaths?) into the skeletal array.

		//  Create translation array - influence index -> refskeleton index.
		TArray <INT> BoneIndices;
		// Match bone to influence
		for( DWORD b=0; b < nInfs; b++)
		{			
			INT BoneIndex = -1;
			for (int i=0; i<Thing->RefSkeletonNodes.Num(); i++)
			{
				// Object to Dagpath
				MObject TestBoneObj( AxSceneObjects[(INT)Thing->RefSkeletonNodes[i] ]  );
				MFnDagNode DNode( TestBoneObj, &stat );
				MDagPath BonePath;
				DNode.getPath( BonePath );
				
				if ( infs[b] == BonePath )
					BoneIndex = i;  // Skeletal index..
			}


			#if 0
			// See if we can find a valid parent if our node happens to not be a bone in the skeleton.
			if( BoneIndex = -1 )
			{
				INT LeadIndex = -1;
				// First find our node:
				for(int i=0; i<SerialTree.Num(); i++)
				{
					MObject TestBoneObj( AxSceneObjects[(INT)SerialTree[i].node ] );
					MFnDagNode DNode( TestBoneObj, &stat );
					MDagPath BonePath;
					DNode.getPath( BonePath );					
					if ( infs[b] == BonePath )
					{
						LeadIndex = i;
					}
				}
				//
				// See if SerialTree[i].node has known-bone parent...
				//				
				if( (LeadIndex > -1 ) && SerialTree[LeadIndex].NoExport ) // ??? //#SKEL: do this for ALL leadIndex>-1...
				{
					//PopupBox("Finding alternative BoneIndex = %i ",BoneIndex); 
					// Traverse until we find a bone parent... assumes all hierarchy up-chains end at -1 / 0
					while( LeadIndex > 0 && !SerialTree[LeadIndex].IsBone )
					{
						LeadIndex = SerialTree[LeadIndex].ParentIndex;
					}

					if( LeadIndex >= 0) // Found valid parent - now get the skeletal index of it..
					{
						INT AltParentNode = (INT)SerialTree[LeadIndex].node;
						for(int i=0; i<Thing->RefSkeletonNodes.Num(); i++)
						{
							if( Thing->RefSkeletonNodes[i] == (void*)AltParentNode )
							{
								BoneIndex = i; 
								//PopupBox(" Alternative bone parent found for noexport-bone: %i ",BoneIndex ); 
							}
						}						
					}
				}
		
			}
			#endif

			// Note:  noexport-bone's influences automatically revert to the closest valid parent in the hierarchy.
			BoneIndices.AddItem(BoneIndex > -1 ? BoneIndex: 0 );
		}

	

		// Also allow for things linked to bones 
		// SerialTree[i].NoExport = 1;
		
		unsigned int nGeoms = skinCluster.numOutputConnections();

		for (DWORD ii = 0; ii < nGeoms; ++ii) 
		{
			unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);

			// get the dag path of the ii'th geometry
			//
			MDagPath skinPath;
			stat = skinCluster.getPathAtIndex(index,skinPath);
			//CheckError(stat,"Error getting geometry path.");

			// Is this DagPath pointing to the current Mesh's path ??
			if ( skinPath == ShapedagPath )
			{
				// go ahead and save all weights and points....
				// iterate through the components of this geometry (vertices)
				//
				MItGeometry gIter(skinPath);
				//
				// PopupBox(" Found CLusterPath  %s [vertexcount %d] [Influences %d] \r\n",skinPath.partialPathName().asChar(),gIter.count(),nInfs) ;			
							
				INT VertexCount = 0;
				for ( ; !gIter.isDone(); gIter.next() ) 
				{
					MObject comp = gIter.component(&stat);
					CheckError(stat,"Error getting component.");

					// Get the weights for this vertex (one per influence object)
					//
					MFloatArray wts;
					unsigned int infCount;
					stat = skinCluster.getWeights(skinPath,comp,wts,infCount);
					//CheckError(stat,"Error getting weights.");
					//if (0 == infCount) {
					//	stat = MS::kFailure;
					//	CheckError(stat,"Error: 0 influence objects.");
					//}

					// Output the weight data for this vertex
					float TotalWeight = 0.0f;

					//fprintf(file,"[%4d] ",gIter.index());
					for (unsigned int jj = 0; jj < infCount ; ++jj ) 
					{
						float BoneWeight = wts[jj];
						if( BoneWeight != 0.0f )
						{
							VRawBoneInfluence TempInf;
							TempInf.PointIndex = VertexCount; // PointIndex.../ Raw skinvertices index
							TempInf.BoneIndex  = BoneIndices[jj];          //Translate bone index into skeleton bone index
							TempInf.Weight     = wts[jj];

							LocalSkin.RawWeights.AddItem(TempInf);  												
							//fprintf(file,"Bone: %4i Weight: %9f ",jj,wts[jj]);
						}
					}
					VertexCount++;
					SkinClusterVertexCount++;
				}				
			}
		}		
	}

	//
	// Digest any mesh triangle data -> into unique wedges (ie common texture UV'sand (later) smoothing groups.)
	//

	UBOOL NakedTri = false;
	int Handedness = 0;


	//DebugBox("Digesting Normals");
	//// Refresh normals.
	//OurTriMesh->buildNormals(); //#Handedness

	// Get mesh-to-worldspace matrix
	//Matrix3 Mesh2WorldTM = ((INode*)SkinNode)->GetObjectTM(TimeStatic);
	//Mesh2WorldTM.NoScale();		

	


	//MFnMesh:: getConnectedShaders ( unsigned instanceNumber, MObjectArray & shaders, MIntArray & indices ) const 
	//Arguments
	//instanceNumber The instance number of the mesh to query 
	//shaders Storage for set objects (shader objects) 
	//indices Storage for indices matching faces to shaders. For each face, this array contains the index into the shaders array for the shader assigned to the face.

	MObjectArray MayaShaders;
	MIntArray ShaderIndices;
	MeshFunction.getConnectedShaders( 0, MayaShaders, ShaderIndices);





//PCF BEGIN
// maya api triangulation
#ifdef NEWMAYAEXPORT
	if( MayaShaders.length() == 0) 
	{
		MGlobal::displayWarning(MString("Warning: possible shader-less geometry encountered for node  ") + Node.name());
	}
	triangulatedMesh triMesh;
	if (! triMesh.triangulate(ShapedagPath))
	{
		MGlobal::displayError(MString("triangulation failed for  ") + currentDagNode.name());
		return -1;

	}

	// Store the number of extra UV sets.  Subtract one because the first UV set is always taken
	LocalSkin.NumExtraUVSets = triMesh.NumUVSets - 1;

	triMesh.createEdgesPool();

	int NumFaces = ( int )triMesh.triangles.size();
	DebugBox("Digesting %i Triangles.",NumFaces);
	DLog.LogfLn("Digesting %i Triangles.",NumFaces);
	//
	// Any smoothing group conversion requested ?
	//
	UBOOL bSmoothingGroups = false;
	MeshProcessor TempTranslator;
	if( OurScene.DoUnSmooth )
	{
		// Construct smoothing groups using current mesh's DagPath.
		// TempTranslator.buildEdgeTable( ShapedagPath ); 	// OLD Maya-SDK code. 
		TempTranslator.createNewSmoothingGroupsFromTriangulatedMesh( triMesh );   // New max-compatible multiple smoothing groups converter.

		if( TempTranslator.PolySmoothingGroups.Num() >= NumFaces ) 
		{
			bSmoothingGroups = true;
		}
	}

	// Find out if this mesh has a color set and vertex colors
	// If it does, we will export them
	bool bHasColors = false;
	MString ColorSetName;
	MeshFunction.getCurrentColorSetName( ColorSetName );
	MColorArray ColorArray;
	if( MeshFunction.getCurrentColorSetName( ColorSetName) == MStatus::kSuccess &&
		ColorSetName.length() &&
		MeshFunction.getFaceVertexColors( ColorArray ) == MStatus::kSuccess )
	{
		bHasColors = true;
	}

	// Master list of vertex colors
	TArray<VColor> VertexColors;
	if( bHasColors )
	{
		// Iterate through all colors and convert them to bytes 
		for( INT ColorIdx = 0; ColorIdx < ColorArray.length(); ++ColorIdx )
		{
			VColor Color(255,255,255,255);
			if( !(ColorArray[ColorIdx].r == -1.f ||
				ColorArray[ColorIdx].g == -1.f ||
				ColorArray[ColorIdx].b == -1.f ) )
			{
				// Convert the colors to bytes to save space and for compatibility
				Color = VColor( BYTE(255.f*ColorArray[ColorIdx].b), BYTE(255.f*ColorArray[ColorIdx].g), BYTE(255.f*ColorArray[ColorIdx].r), BYTE(255.f*ColorArray[ColorIdx].a) );
			}

			VertexColors.AddItem( Color );
		}
	}

	LocalSkin.bHasVertexColors = bHasColors;
	for ( int i =0 ; i <  NumFaces; i++)
	{
		triangulatedMesh::Triangle & t = *(triMesh.triangles[i]);

		INT MaterialIndex = t.ShadingGroupIndex & 255;

		INT ThisMaterial;
		if( (INT) MayaShaders.length() <= MaterialIndex ) 
		{			
			ThisMaterial = 0;
		}
		else		
		{
			ThisMaterial = DigestMayaMaterial( MayaShaders[MaterialIndex]);
		}


		//// A brand new face.

		VTriangle NewFace;
		NewFace.SmoothingGroups = bSmoothingGroups ? TempTranslator.PolySmoothingGroups[i] : 1 ;

		for ( int j =0 ; j <3; j++)
		{
			int VertIdx = t.Indices[j].pointIndex;

			VVertex Wedge;
			NewFace.WedgeIndex[j] = LocalSkin.Wedges.Num()+j;
			Wedge.PointIndex = VertIdx; 
			FVector WV = LocalSkin.Points[ Wedge.PointIndex ].Point;
			Wedge.Reserved = 0;


			GWedge NewWedge;				
			//INT NewWedgeIdx = StaticPrimitives[PrimIndex].Wedges.AddZeroed(1);
			NewWedge.MaterialIndex = ThisMaterial; // Per-corner material index..
			NewWedge.PointIndex = VertIdx; // Maya's face vertices indices for this face.
			int uvIndex = t.Indices[j].tcIndex[0];
			Wedge.UV.U = triMesh.uArray[0][uvIndex];
			Wedge.UV.V = 1.0f - triMesh.vArray[0][uvIndex];

			// Read in extra UV sets
			for( INT UVSet = 0; UVSet < NUM_EXTRA_UV_SETS; ++UVSet )
			{
				if (t.Indices[j].hasExtraUVs[UVSet])
				{
					// Add one to skip the first uv set index
					uvIndex = t.Indices[j].tcIndex[UVSet+1];

					Wedge.ExtraUVs[UVSet].U = triMesh.uArray[UVSet+1][uvIndex];
					Wedge.ExtraUVs[UVSet].V = 1.0f - triMesh.vArray[UVSet+1][uvIndex];
				}
			}

			if( bHasColors )
			{
				// Find the vertex color for this wedge 
				// This has to be done after triangulation as vertex order 
				// may have changed
				INT colorIndex = t.Indices[j].vertexColor;
				Wedge.Color = VertexColors[ colorIndex ];
			}
		
			Wedge.MatIndex = ThisMaterial;
			LocalSkin.Wedges.AddItem(Wedge);

		}

		NewFace.MatIndex = ThisMaterial;
		LocalSkin.Faces.AddItem(NewFace);
	}

//PCF END

// old export code
#else 

	MStatus status;
	MFnDependencyN( &status );

	NumFaces = 0;
	if( MayaShaders.length() == 0) 
	{

		ErrorBox("Node without matching vertices encountered: [%s]", NodeName);
	}
	int NumFaces = MeshFunction.numPolygons(&stat);
	DebugBox("Digesting %i Triangles.",NumFaces);
	DLog.LogfLn("Digesting %i Triangles.",NumFaces);


	if( LocalSkin.Points.Num() ==  0 ) // Actual points allocated yet ? (Can be 0 if no matching bones were found)
	{
		MString   Nod	ode dgNode;
		status = dgNode.setObject( SkinObject );
		NodeName = dgNode.name
		ErrorBox("Warning: possible shader-less geometry encountered for node [%s] ",Node.name().asChar());
	}

	// Get Maya normals.. ?
	// MFloatVectorArray Normals;
	// stat = MeshFunction.getNormals( Normals, MSpace:kWorld );

	//
	// Any smoothing group conversion requested ?
	//
	UBOOL bSmoothingGroups = false;
	MeshProcessor TempTranslator;
	if( OurScene.DoUnSmooth )
	{
		// Construct smoothing groups using current mesh's DagPath.
		// TempTranslator.buildEdgeTable( ShapedagPath ); 	// OLD Maya-SDK code. 
		TempTranslator.createNewSmoothingGroups( ShapedagPath );   // New max-compatible multiple smoothing groups converter.

		if( TempTranslator.PolySmoothingGroups.Num() >= NumFaces ) 
		{
			bSmoothingGroups = true;
		}
	}

	MStringArray  UVSetsNames;
	MeshFunction.getUVSetNames   (UVSetsNames) ;
	bool secondUvSet =0;
	if (UVSetsNames.length() >1) secondUvSet =1;
	
	//if (secondUvSet)
	//	cerr << "	exporting 2 UVSets" << endl;
	//else
	//	cerr << "	zeroing second UVSet" << endl;
	for (int PolyIndex = 0; PolyIndex < NumFaces; PolyIndex++)
	{
		MIntArray	FaceVertices;

		// Get the vertex indices for this polygon.
		MeshFunction.getPolygonVertices(PolyIndex,FaceVertices);

		INT VertCount = FaceVertices.length();

		// Triangulate quads (and higher) but only if requested.
		if( ! OurScene.DoAutoTriangles )
		{
			VertCount = VertCount>3 ? 3 : VertCount;
		}

		while( VertCount >= 3 )
		{			
			// Fill vertex indices - breaks up face in triangle polygons.
			INT VertIdx[3];
			VertIdx[0] = 0;
			VertIdx[1] = VertCount-2;
			VertIdx[2] = VertCount-1;
			
			VVertex Wedge0;
			VVertex Wedge1;
			VVertex Wedge2;
			VTriangle NewFace;
			
			// Get SKELETAL skin face's smoothing mask from the TempTranslator.
			// When NO smoothing groups explicitly default to 1 !! (all facets SHARE smooth boundaries.)
			NewFace.SmoothingGroups = bSmoothingGroups ? TempTranslator.PolySmoothingGroups[PolyIndex] : 1 ;
			
			// Normals (determine handedness...
			// Point3 MaxNormal = OurTriMesh->getFaceNormal(PolyIndex);		
			// Transform normal vector into world space along with 3 vertices. 
			// MaxNormal = VectorTransform( Mesh2WorldTM, MaxNormal );

			// FVector FaceNormal = MeshNormals(PolyIndex );
			// Can't use MeshNormals -> vertex normals, we want face normals. 

			NewFace.WedgeIndex[0] = LocalSkin.Wedges.Num();
			NewFace.WedgeIndex[1] = LocalSkin.Wedges.Num()+1;
			NewFace.WedgeIndex[2] = LocalSkin.Wedges.Num()+2;		

			// Face's vertex indices ('point indices')
			
			Wedge0.PointIndex = FaceVertices[VertIdx[0]]; //MeshFunction.getPolygonVertices( 0,VertexIndex );
			Wedge1.PointIndex = FaceVertices[VertIdx[1]]; //MeshFunction.getPolygonVertices( 1,VertexIndex );
			Wedge2.PointIndex = FaceVertices[VertIdx[2]]; //MeshFunction.getPolygonVertices( 2,VertexIndex );

			// Vertex coordinates are in localskin.point, already in worldspace.
			FVector WV0 = LocalSkin.Points[ Wedge0.PointIndex ].Point;
			FVector WV1 = LocalSkin.Points[ Wedge1.PointIndex ].Point;
			FVector WV2 = LocalSkin.Points[ Wedge2.PointIndex ].Point;


			/*
			// #DEBUG 
			// Check for degenerate triangles...
			static INT FirstWarn = 0;
			if ( Wedge0.PointIndex == Wedge1.PointIndex || Wedge0.PointIndex == Wedge2.PointIndex || Wedge1.PointIndex == Wedge2.PointIndex 
				 || Wedge0.PointIndex >= LocalSkin.Points.Num() || Wedge1.PointIndex >= LocalSkin.Points.Num() || Wedge2.PointIndex >= LocalSkin.Points.Num() )
			{
				if( FirstWarn < 4) PopupBox("Degenerate triangle: (indices) %i  indices %i %i %i max index: %i", PolyIndex,Wedge0.PointIndex,Wedge1.PointIndex,Wedge2.PointIndex, LocalSkin.Points.Num() );
				FirstWarn++;
			}
			if ( WV0 == WV1 && WV0 == WV2 && WV1 == WV2 )
			{
				if( FirstWarn < 4) PopupBox("Degenerate triangle: %i  (points) ", PolyIndex);
				FirstWarn++;
			}
			*/
		
			// Figure out the handedness of the face by constructing a normal and comparing it to Maya's (??)s normal.		
			// FVector OurNormal = (WV0 - WV1) ^ (WV2 - WV0);
			// OurNormal /= sqrt(OurNormal.SizeSquared()+0.001f);// Normalize size (unnecessary ?);			
			// Handedness = ( ( OurNormal | FaceNormal ) < 0.0f); // normals anti-parallel ?

		
			//NewFace.MatIndex = MaterialIndex;
			Wedge0.Reserved = 0;
			Wedge1.Reserved = 0;
			Wedge2.Reserved = 0;

			//
			// Get UV texture coordinates: allocate a Wedge with for each and every 
			// UV+ MaterialIndex ID; then merge these, and adjust links in the Triangles 
			// that point to them.
			//
			
			// check mapped face
			//if( 1 ) // TriFace->flags & HAS_TVERTS )
			{
				//MString  UVSet1; // ? - Despite being optional, this helps prevent a weird crash...?				


				FLOAT U,V;
				int rr = 0;
				while (rr < 3 )
				{
					VVertex * we ;
					if (rr == 0)
							we= &Wedge0;
					else if (rr == 1)
							we= &Wedge1;
					else 
							we= &Wedge2;

					VVertex & wed = *we;

					cerr << "	uvs["<< rr << "]" << endl;

					stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[rr],U,V,&UVSetsNames[0]  );
					if (stat == MS::kSuccess)
					{
					
						//if ( stat != MS::kSuccess )
						//	DLog.Logf(" Invalid UV retrieval ");
						wed.U =  U;
						wed.V =  1.0f - V; // The Y-flip: an Unreal requirement..

						// second uv set
						wed.U1 =  Wedge0.U;
						wed.V1 =  Wedge0.V; // The Y-flip: an Unreal requirement..	
					}
					else
					{
						cerr << "	error: getPolygonUV(uvSet 0) failed"<< endl;
						wed.U =  0;
						wed.V =  0; // The Y-flip: an Unreal requirement..

						// second uv set
						wed.U1 =  0;
						wed.V1 =  0; // The Y-flip: an Unreal requirement..	
					}

					cerr << "	uvs0 " << Wedge0.U << "/"<< Wedge0.V<< endl;
					if (secondUvSet)
					{
						
						stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[rr],U,V,&UVSetsNames[1]  );
						cerr << "	secondUvSet "   << endl;

						if (stat == MS::kSuccess)
						{
							wed.U1 =  U;
							wed.V1 =  1.0f - V; // The Y-flip: an Unreal requirement..
						}
						else
						{
							cerr << "	error: getPolygonUV(uvSet 1) failed"<< endl;
							wed.U1 =  0;
							wed.V1 =  0; // The Y-flip: an Unreal requirement..
						}

					}
					cerr << "	uvs1 " << Wedge0.U1 << "/"<< Wedge0.V1<< endl;
					rr++;
				}

			}
			
			// PCF COMMENT - its wrong - totally messes uv sets, takes uv from any other mapped vert
			// if there is no uv - should zero it not mess

			//// Above may fail retrieving UV's - detect by comparing U/V's, if so retry old method:
			
			//if( (Wedge0.U == Wedge1.U) && (Wedge0.V == Wedge1.V) ) // TriFace->flags & HAS_TVERTS )
			//{
			//	FLOAT U,V;
			//	stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[0],U,V );

			//	//if ( stat != MS::kSuccess )
			//	//	DLog.Logf(" Invalid UV retrieval ");
			//	Wedge0.U =  U;
			//	Wedge0.V =  1.0f - V; // The Y-flip: an Unreal requirement...

			//	stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[1],U,V );
			//	//if ( stat != MS::kSuccess )
			//	//	DLog.Logf(" Invalid UV retrieval ");
			//	Wedge1.U =  U;
			//	Wedge1.V =  1.0f - V; 

			//	stat = MeshFunction.getPolygonUV( PolyIndex,VertIdx[2],U,V );
			//	//if ( stat != MS::kSuccess )
			//	//	DLog.Logf(" Invalid UV retrieval ");
			//	Wedge2.U =  U;
			//	Wedge2.V =  1.0f - V;
			//}

			
			// To verify: DigestMaterial should give a correct final material index 
			// even for multiple multi/sub materials on complex hierarchies of meshes.

			INT MaterialIndex = ShaderIndices[PolyIndex] & 255;

			INT ThisMaterial;
			if( (INT) MayaShaders.length() <= MaterialIndex ) 
			{			
				ThisMaterial = 0;
			}
			else		
			{
				ThisMaterial = DigestMayaMaterial( MayaShaders[MaterialIndex]);
			}

			NewFace.MatIndex = ThisMaterial;
			Wedge0.MatIndex = ThisMaterial;
			Wedge1.MatIndex = ThisMaterial;
			Wedge2.MatIndex = ThisMaterial;
			
			LocalSkin.Wedges.AddItem(Wedge0);
			LocalSkin.Wedges.AddItem(Wedge1);
			LocalSkin.Wedges.AddItem(Wedge2);
			
			LocalSkin.Faces.AddItem(NewFace);
			
			/*
			int PointIdx = LocalSkin.Wedges[ LocalSkin.Wedges.Num()-1 ].PointIndex; // last added 3d point
			FVector VertPoint = LocalSkin.Points[ PointIdx ].Point;
			DLog.LogfLn(" [tri %4i ] Wedge UV  %f  %f  MaterialIndex %i  Vertex %i Vertcount %i ",PolyIndex,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].U ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].V ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].MatIndex, LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].PointIndex,VertCount );
			DLog.LogfLn("            Vertex [%i] XYZ %6.6f  %6.6f  %6.6f", VertIdx, VertPoint.X,VertPoint.Y,VertPoint.Z );
			*/

			VertCount--;
		}
	}
	#endif

	MayaShaders.clear();

	TArray  <VVertex> NewWedges;
	TArray  <INT>     WedgeRemap;

	WedgeRemap.SetSize( LocalSkin.Wedges.Num() ); 

	DebugBox( "Digesting %i Wedges.", LocalSkin.Wedges.Num() );

	// Get wedge, see which others match and flag those,
	// then store wedge, get next nonflagged one. O(n^2/2)

	for( int t=0; t< LocalSkin.Wedges.Num(); t++ )
	{
		if( LocalSkin.Wedges[t].Reserved != 0xFF ) // not flagged ?
		{
			// Remember this wedge's unique new index.
			WedgeRemap[t] = NewWedges.Num();		
			NewWedges.AddItem( LocalSkin.Wedges[t] ); // then it's unique.

			// Any copies ?
			for (int d=t+1; d< LocalSkin.Wedges.Num(); d++)
			{
				if ( Thing->IsSameVertex( LocalSkin.Wedges[t], LocalSkin.Wedges[d], LocalSkin.NumExtraUVSets ) )
				{
					LocalSkin.Wedges[d].Reserved = 0xFF;
					WedgeRemap[d] = NewWedges.Num()-1;
				}
			}
		}
	}

	//
	// Re-point all indices from all Triangles to the proper unique-ified Wedges.
	// 

	for( INT FaceIndex = 0; FaceIndex < LocalSkin.Faces.Num(); FaceIndex++)
	{		
		LocalSkin.Faces[FaceIndex].WedgeIndex[0] = WedgeRemap[FaceIndex*3+0];
		LocalSkin.Faces[FaceIndex].WedgeIndex[1] = WedgeRemap[FaceIndex*3+1];
		LocalSkin.Faces[FaceIndex].WedgeIndex[2] = WedgeRemap[FaceIndex*3+2];

		// Verify we all mapped within new bounds.
		if (LocalSkin.Faces[FaceIndex].WedgeIndex[0] >= NewWedges.Num()) ErrorBox("Wedge Overflow 1");
		if (LocalSkin.Faces[FaceIndex].WedgeIndex[1] >= NewWedges.Num()) ErrorBox("Wedge Overflow 1");
		if (LocalSkin.Faces[FaceIndex].WedgeIndex[2] >= NewWedges.Num()) ErrorBox("Wedge Overflow 1");	

		// Flip faces?
		
		if (1) // (Handedness ) 
		{			
			//FlippedFaces++;
			INT Index1 = LocalSkin.Faces[FaceIndex].WedgeIndex[2];
			LocalSkin.Faces[FaceIndex].WedgeIndex[2] = LocalSkin.Faces[FaceIndex].WedgeIndex[1];
			LocalSkin.Faces[FaceIndex].WedgeIndex[1] = Index1;
		}
	}


	//  Replace old array with new.
	LocalSkin.Wedges.SetSize( 0 );
	LocalSkin.Wedges.SetSize( NewWedges.Num() );

	for(INT t=0; t<NewWedges.Num(); t++ )
	{
		LocalSkin.Wedges[t] = NewWedges[t];
		if (DEBUGFILE && LogPath[0]) DLog.Logf(" [ %4i ] Wedge UV  %f  %f  Material %i  Vertex %i  XYZ %f %f %f \n",
			t,
			NewWedges[t].UV.U,
			NewWedges[t].UV.V,
			NewWedges[t].MatIndex,
			NewWedges[t].PointIndex, 
			LocalSkin.Points[NewWedges[t].PointIndex].Point.X,
			LocalSkin.Points[NewWedges[t].PointIndex].Point.Y,
			LocalSkin.Points[NewWedges[t].PointIndex].Point.Z 
			);
	}

	DebugBox( " NewWedges %i SkinDataVertices %i ", NewWedges.Num(), LocalSkin.Wedges.Num() );

	if (DEBUGFILE)
	{
		DLog.Logf(" Digested totals: Wedges %i Points %i Faces %i \n", LocalSkin.Wedges.Num(),LocalSkin.Points.Num(),LocalSkin.Faces.Num());
	}

	return 1;
}


DWORD FlagsFromName(char* pName )
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


//
// Unreal flags from materials (original material tricks by Steve Sinclair, Digital Extremes ) 
// - uses classic Unreal/UT flag conventions for now.
// Determine the polyflags, in case a multimaterial is present
// reference the matID value (pass in the face's matID).
//

void CalcMaterialFlags( MObject& MShaderObject, VMaterial& Stuff )
{
	
	char pName[MAX_PATH];
	MFnDependencyNode MFnShader( MShaderObject );		
	strcpysafe( pName, MFnShader.name().asChar(), MAX_PATH );

	// Check skin index 
	INT skinIndex = FindValueTag( pName, _T("skin") ,2 ); // Double (or single) digit skin index.......
	if ( skinIndex < 0 ) 
	{
		skinIndex = 0;
		Stuff.AuxFlags = 0;  
	}
	else
		Stuff.AuxFlags = 1; // Indicates an explicitly defined skin index.

	Stuff.TextureIndex = skinIndex;
	
	// Lodbias
	INT LodBias = FindValueTag( pName, _T("lodbias"),2); 
	if ( LodBias < 0 ) LodBias = 5; // Default value when tag not found.
	Stuff.LodBias = LodBias;

	// LodStyle
	INT LodStyle = FindValueTag( pName, _T("lodstyle"),2);
	if ( LodStyle < 0 ) LodStyle = 0; // Default LOD style.
	Stuff.LodStyle = LodStyle;

	Stuff.PolyFlags = FlagsFromName(pName); //MatFlags;

}



int	SceneIFC::DigestSkin(VActor *Thing )
{
	//cerr << endl<< "	DigestSkin()"  << endl;
	//
	// if (OurSkins.Num() == 0) return 0; // no physique skin detected !!
	//	
	// We need a digest of all objects in the scene that are meshes belonging
	// to the skin; then go over these and use the parent node for non-physique ones,
	// and physique bones digestion where possible.
	//

	SetMayaSceneTime( TimeStatic );

	// Maya shaders
	AxShaders.clear(); //Empty();

	Thing->SkinData.Points.Empty();
	Thing->SkinData.Wedges.Empty();
	Thing->SkinData.Faces.Empty();
	Thing->SkinData.RawWeights.Empty();
	Thing->SkinData.RawMaterials.Empty();
	Thing->SkinData.Materials.Empty();

	if( DEBUGFILE )
	{
		char LogFileName[MAX_PATH]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");	
		sprintf_s(LogFileName,"\\ActXSkin.LOG");
		DLog.Open( LogPath,LogFileName, DEBUGFILE );
	}

	DLog.Logf(" Starting skin digestion logfile \n");


	// Digest multiple meshes.

	for( INT i=0; i< SerialTree.Num(); i++ )
	{
		INT HasRootAsParent = 0;

		// If not a skincluster subject, figure out whether the skeletal root is this mesh's parent somehow,
		// so that it is definitely part of the skin. 
		if( SerialTree[i].IsSkin )
		{			
			MObject Object = AxSceneObjects[ (INT)SerialTree[i].node ];
			MObject RootBoneObj( AxSceneObjects[(INT)Thing->RefSkeletonNodes[0]] );
			MFnDagNode RootNode( RootBoneObj );
			if( RootNode.hasChild(Object) )
				HasRootAsParent = 1;
		}

		// Skin or mesh; if mesh, parent must be a joint unless it's a skincluster

		// #SKEL - crucial requirement:  && ( SerialTree[i].IsSmooth || HasRootAsParent ) ?? #SKEL!
		if( ( SerialTree[i].IsSkin ) && ( SerialTree[i].IsSmooth || HasRootAsParent ) )
		{

			VSkin LocalSkin;

			// Smooth-skinned or regular?
			if( SerialTree[i].IsSmooth)
			{								
				// Skincluster deformable mesh.
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 1 );
			}
			else
			{				
				// Regular mesh - but throw away if its parent is ROOT !
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 0 );
			}

			DLog.LogfLn(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );
			
			// Add points, wedges, faces, RAW WEIGHTS from localskin to Thing->Skindata.
			// Indices need to be shifted to create a single mesh soup!

			INT PointBase = Thing->SkinData.Points.Num();
			INT WedgeBase = Thing->SkinData.Wedges.Num();

			if( LocalSkin.NumExtraUVSets > Thing->SkinData.NumExtraUVSets )
			{
				Thing->SkinData.NumExtraUVSets = LocalSkin.NumExtraUVSets;
			}

			Thing->SkinData.bHasVertexColors = LocalSkin.bHasVertexColors;

			{for(INT j=0; j<LocalSkin.Points.Num(); j++)
			{
				Thing->SkinData.Points.AddItem(LocalSkin.Points[j]);
			}}

			{for(INT j=0; j<LocalSkin.Wedges.Num(); j++)
			{
				VVertex Wedge = LocalSkin.Wedges[j];
				//adapt wedge's POINTINDEX...
				Wedge.PointIndex += PointBase;
				Thing->SkinData.Wedges.AddItem(Wedge);
			}}

			{for(INT j=0; j<LocalSkin.Faces.Num(); j++)
			{
				//WedgeIndex[3] needs replacing
				VTriangle Face = LocalSkin.Faces[j];
				// adapt faces indices into wedges...
				Face.WedgeIndex[0] += WedgeBase;
				Face.WedgeIndex[1] += WedgeBase;
				Face.WedgeIndex[2] += WedgeBase;
				Thing->SkinData.Faces.AddItem(Face);
			}}

			{for(INT j=0; j<LocalSkin.RawWeights.Num(); j++)
			{
				VRawBoneInfluence RawWeight = LocalSkin.RawWeights[j];
				// adapt RawWeights POINTINDEX......
				RawWeight.PointIndex += PointBase;
				Thing->SkinData.RawWeights.AddItem(RawWeight);
			}}
		}
	}


	FixMaterials(Thing);

	DLog.Close();	
	return 1;
}



//
// DigestBrush for UnrealEd2-compatible T3D export.
//

int SceneIFC::DigestBrush( VActor *Thing )
{
	//cerr << endl<< "	DigestSkin()"  << endl;
	// Similar to digestskin but - digest all _selected_ meshes regardless of their linkup-state...
	SetMayaSceneTime( TimeStatic );

	// Maya shaders
	AxShaders.clear(); //.Empty();

	Thing->SkinData.Points.Empty();
	Thing->SkinData.Wedges.Empty();
	Thing->SkinData.Faces.Empty();
	Thing->SkinData.RawWeights.Empty();
	Thing->SkinData.RawMaterials.Empty();
	Thing->SkinData.MaterialVSize.Empty();
	Thing->SkinData.MaterialUSize.Empty();
	Thing->SkinData.Materials.Empty();

	// Digest multiple meshes.

	for( INT i=0; i< SerialTree.Num(); i++ )
	{
		INT HasRootAsParent = 0;


		// If not a skincluster subject, Figure out whether the skeletal root is this mesh's parent somehow,
		// so that it is definitely part of the skin.
		if( SerialTree[i].IsSkin )
		{			
			MObject Object = AxSceneObjects[ (INT)SerialTree[i].node ];

			// Allow for zero-bone mesh digesting
			if( Thing->RefSkeletonNodes.Num() )
			{
				MObject RootBoneObj( AxSceneObjects[(INT)Thing->RefSkeletonNodes[0]] );
				MFnDagNode RootNode( RootBoneObj );
				if( RootNode.hasChild(Object) )
					HasRootAsParent = 1;
			}


			// Selection in Maya: the TRANSFORM-parent of a mesh got selected, not the actual mesh.
			if( SerialTree[i].ParentIndex >= 0 )
			{
				if( SerialTree[SerialTree[i].ParentIndex].IsSelected)
				{
					SerialTree[i].IsSelected = 1;
				}
			}
		}


		// Skin or mesh; if mesh, parent must be a joint unless it's a skincluster <- Restriction removed - exporting brushes here.
		if( SerialTree[i].IsSelected && SerialTree[i].IsSkin  ) // && ( SerialTree[i].IsSmooth || HasRootAsParent ) )
		{
			VSkin LocalSkin;

			// Smooth-skinned or regular?
			if( SerialTree[i].IsSmooth )
			{								
				// Skincluster deformable mesh.
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 1 );
			}
			else
			{				
				// Regular mesh - but throw away if its parent is ROOT !
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 0 );
			}

			// PopupBox(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );

			// DLog.LogfLn(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );
			
			// Add points, wedges, faces, RAW WEIGHTS from localskin to Thing->Skindata.
			// Indices need to be shifted to create a single mesh soup!


			INT PointBase = Thing->SkinData.Points.Num();
			INT WedgeBase = Thing->SkinData.Wedges.Num();

			{for(INT i=0; i<LocalSkin.Points.Num(); i++)
			{
				Thing->SkinData.Points.AddItem(LocalSkin.Points[i]);
			}}

			// MAYA Z/Y reversal...
			{for(INT v=0;v<Thing->SkinData.Points.Num(); v++)
			{
				FLOAT Z = Thing->SkinData.Points[v].Point.Z;
				FLOAT Y = Thing->SkinData.Points[v].Point.Y;
				Thing->SkinData.Points[v].Point.Z =  Y;
				Thing->SkinData.Points[v].Point.Y = -Z;
			}}

			{for( i=0; i<LocalSkin.Wedges.Num(); i++)
			{
				VVertex Wedge = LocalSkin.Wedges[i];
				//adapt wedge's POINTINDEX...
				Wedge.PointIndex += PointBase;
				Thing->SkinData.Wedges.AddItem(Wedge);
			}}

			{for( i=0; i<LocalSkin.Faces.Num(); i++)
			{
				//WedgeIndex[3] needs replacing
				VTriangle Face = LocalSkin.Faces[i];
				// adapt faces indices into wedges...
				Face.WedgeIndex[0] += WedgeBase;
				Face.WedgeIndex[1] += WedgeBase;
				Face.WedgeIndex[2] += WedgeBase;
				Thing->SkinData.Faces.AddItem(Face);
			}}

			{for( i=0; i<LocalSkin.RawWeights.Num(); i++)
			{
				VRawBoneInfluence RawWeight = LocalSkin.RawWeights[i];
				// adapt RawWeights POINTINDEX......
				RawWeight.PointIndex += PointBase;
				Thing->SkinData.RawWeights.AddItem(RawWeight);
			}}

		}
	}

	FixMaterials(Thing);

	//DLog.Close();	
	return 1;
}



//
//  Bitmap path/name/extension extraction from shader node.
//
void GetBitmapName(TCHAR* Name, size_t NameLen, TCHAR* Path, MObject& ThisShader )
{
	char TextureSourceString[MAX_PATH];
	char BmpPathname[MAX_PATH];
	char BmpFilename[MAX_PATH];
	char BmpExtension[MAX_PATH];

	Name[0]=0;
	Path[0]=0;

	// Get path and bitmap name from shader.		
	GetTextureNameFromShader( ThisShader, TextureSourceString, MAX_PATH );

	// extract bitmap/path name from path...
	if( strlen( TextureSourceString ) >0 )
	{
		GetFolderFromPath( BmpPathname, TextureSourceString, MAX_PATH );
		GetNameFromPath( BmpFilename, TextureSourceString, MAX_PATH );	
		GetExtensionFromPath( BmpExtension, TextureSourceString, MAX_PATH );

		sprintf_s( Name,NameLen,"%s.%s",BmpFilename,BmpExtension);
		strcpysafe(Path,CleanString(BmpPathname),MAX_PATH);	
	}
	else
	{
		sprintf_s( Name,NameLen,"unknown_bitmap.bmp");
	}
}
	



int	SceneIFC::FixMaterials(VActor *Thing )
{
	//
	// Digest the accumulated Maya materials, and if necessary re-sort materials and update material indices.
	//

	UBOOL IndexOverride = false;

	for( DWORD i=0; i< AxShaders.length(); i++ )
	{
		// Get materials from the collected objects.
		VMaterial Stuff;

		//
		char pName[MAX_PATH];
		MFnDependencyNode MFnShader ( AxShaders[i] );

		strcpysafe( pName, MFnShader.name().asChar(), MAX_PATH );

		Stuff.SetName(pName);


		// Calc material flags
		CalcMaterialFlags( AxShaders[i], Stuff );

		// Add to materials
		Thing->SkinData.Materials.AddItem( Stuff );

		// Reserved - for material sorting.
		Stuff.AuxMaterial = 0;

		// Skin index override detection.
		if (Stuff.AuxFlags ) IndexOverride = true;

		// Add texture source path		
		VBitmapOrigin TextureSource;
		GetBitmapName( TextureSource.RawBitmapName, _countof(TextureSource.RawBitmapName), TextureSource.RawBitmapPath, AxShaders[i] );		
		Thing->SkinData.RawBMPaths.AddItem( TextureSource );

		// Retrieve texture size from MFnShader somehow..
		INT USize = 256;
		INT VSize = 256;
		MObject Object = MFnShader.object();
		GetMayaShaderTextureSize( Object, USize, VSize );

		//PopupBox("Shader [%i]  USize [%i] VSize [%i] ",i,USize,VSize); //#DEBUG

		Thing->SkinData.MaterialUSize.AddItem( USize );
		Thing->SkinData.MaterialVSize.AddItem( VSize );

	}


	//
	// Sort Thing->materials in order of 'skin' assignment.
	// Auxflags = 1: explicitly defined material -> these have be sorted in their assigned positions;
	// if they are incomplete (e,g, skin00, skin02 ) then the 'skin02' index becomes 'skin01'...
	// Wedge indices  then have to be remapped also.
	//
	if ( IndexOverride)
	{
		INT NumMaterials = Thing->SkinData.Materials.Num();
		// AuxMaterial will hold original index.
		for (int t=0; t<NumMaterials;t++)
		{
			Thing->SkinData.Materials[t].AuxMaterial = t;
		}

		// Bubble sort.		
		int Sorted = 0;
		while (Sorted==0)
		{
			Sorted=1;
			for (int t=0; t<(NumMaterials-1);t++)
			{	
				UBOOL SwapNeeded = false;
				if( Thing->SkinData.Materials[t].AuxFlags && (!Thing->SkinData.Materials[t+1].AuxFlags) )
				{
					SwapNeeded = true;
				}
				else
				{
					if( ( Thing->SkinData.Materials[t].AuxFlags && Thing->SkinData.Materials[t+1].AuxFlags ) &&
						( Thing->SkinData.Materials[t].TextureIndex > Thing->SkinData.Materials[t+1].TextureIndex ) )
						SwapNeeded = true;
				}				
				if( SwapNeeded )
				{
					Sorted=0;

					VMaterial VMStore = Thing->SkinData.Materials[t];
					Thing->SkinData.Materials[t]=Thing->SkinData.Materials[t+1];
					Thing->SkinData.Materials[t+1]=VMStore;					
										
					VBitmapOrigin BOStore = Thing->SkinData.RawBMPaths[t];
					Thing->SkinData.RawBMPaths[t] =Thing->SkinData.RawBMPaths[t+1];
					Thing->SkinData.RawBMPaths[t+1] = BOStore;					

					INT USize = Thing->SkinData.MaterialUSize[t];
					INT VSize = Thing->SkinData.MaterialVSize[t];
					Thing->SkinData.MaterialUSize[t] = Thing->SkinData.MaterialUSize[t+1];
					Thing->SkinData.MaterialVSize[t] = Thing->SkinData.MaterialVSize[t+1];
					Thing->SkinData.MaterialUSize[t+1] = USize;
					Thing->SkinData.MaterialVSize[t+1] = VSize;
				}			
			}
		}

		DebugBox("Re-sorted material order for skin-index override.");	

		// Remap wedge material indices.
		if (1)
		{
			INT RemapArray[256];
			Memzero(RemapArray,256*sizeof(INT));
			{for( int t=0; t<NumMaterials;t++)
			{
				RemapArray[Thing->SkinData.Materials[t].AuxMaterial] = t;
			}}
			{for( int i=0; i<Thing->SkinData.Wedges.Num(); i++)
			{
				INT MatIdx = Thing->SkinData.Wedges[i].MatIndex;
				if ( MatIdx >=0 && MatIdx < Thing->SkinData.Materials.Num() )
					Thing->SkinData.Wedges[i].MatIndex =  RemapArray[MatIdx];
			}}		
		}
	}

	return 1;
}


int SceneIFC::DigestMaterial(AXNode *node,  INT matIndex,  TArray<void*>& RawMaterials)
{
	void* ThisMtl = (void*) matIndex;
	
	// The different material properties can be investigated at the end, for now we care about uniqueness only.
	RawMaterials.AddUniqueItem( (void*) ThisMtl );

	return( RawMaterials.Where( (void*) ThisMtl ) );
}


int SceneIFC::RecurseValidBones(const int RIndex, int &BoneCount)
{
	if ( SerialTree[RIndex].IsBone == 0) return 0; // Exit hierarchy for non-bones...

	SerialTree[RIndex].InSkeleton = 1; //part of our skeleton.
	BoneCount++;

	for (int c = 0; c < SerialTree[RIndex].NumChildren; c++) 
	{
		if( GetChildNodeIndex(RIndex,c) <= 0 ) ErrorBox("GetChildNodeIndex assertion failed");

		RecurseValidBones( GetChildNodeIndex(RIndex,c), BoneCount); 
	}
	return 1;
}


int	SceneIFC::MarkBonesOfSystem(int RIndex)
{
	int BoneCount = 0;

	for( int i=0; i<SerialTree.Num(); i++)
	{
		SerialTree[i].InSkeleton = 0; // Erase skeletal flags.
	}

	RecurseValidBones( RIndex, BoneCount );

	// How to get the hierarchy with support of our 
	// flags, too:
	// - ( a numchildren, and look-for-nth-child routine for our array !)

	return BoneCount;
};


//
// Evaluate the skeleton & set node flags...
//


int SceneIFC::EvaluateSkeleton(INT RequireBones)
{
	// Our skeleton: always the one hierarchy
	// connected to the scene root which has either the most
	// members, or the most SELECTED members, or a physique
	// mesh attached...
	// For now, a hack: find all root bones; then choose the one
	// which is selected.

	SetMayaSceneTime( TimeStatic );

	RootBoneIndex = 0;
	if (SerialTree.Num() == 0) return 0;

	int BonesMax = 0;

	RootBoneIndex = -1;

	for( int i=0; i<SerialTree.Num(); i++)
	{
		// First bone assumed root bone //#TODO unsafe
		if( SerialTree[i].IsBone )
		{
			if (RootBoneIndex == -1)
				RootBoneIndex = i; 

			// All bones assumed to be in skeleton.
			SerialTree[i].InSkeleton = 1 ;
			BonesMax++;
		}
	}
	
	if (RootBoneIndex == -1)
	{
		if ( RequireBones ) PopupBox("ERROR: Failed to find a root bone.");

		TotalBipBones = TotalBones = TotalMaxBones = TotalDummies = 0;
		return 0;// failed to find any.
	}

	//PopupBox("ROOTBONE is: [%i]", RootBoneIndex );

	OurRootBone = SerialTree[RootBoneIndex].node;
	OurBoneTotal = BonesMax;

	/*
	for( int i=0; i<SerialTree.Num(); i++)
	{	
		if( (SerialTree[i].IsBone != 0 ) && (SerialTree[i].ParentIndex == 0) )
		{
			// choose one with the biggest number of bones, that must be the
			// intended skeleton/hierarchy.
			int TotalBones = MarkBonesOfSystem(i);

			if (TotalBones>BonesMax)
			{
				RootBoneIndex = i;
				BonesMax = TotalBones;
			}
		}
	}

	if (RootBoneIndex == 0) return 0;// failed to find any.

	// Real node
	OurRootBone = SerialTree[RootBoneIndex].node;

	// Total bones
	OurBoneTotal = MarkBonesOfSystem(RootBoneIndex);
   */

	//
	// Digest bone-name functional tag substrings - 'noexport' means don't export a bone,
	// 'ignore' means don't do this bone nor any that are its children.
	//
	for (INT i=0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
	{
		if ( SerialTree[i].InSkeleton ) 
		{
			MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
			MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 

			// Grab the name of this bone.  If the name has a prefix because it came from a reference
			// file, we'll strip that off first.
			MString MayaDagNodeName = GetCleanNameForNode( DagNode );
			char BoneName[MAX_PATH];
			sprintf_s( BoneName, MayaDagNodeName.asChar() );

			SerialTree[i].NoExport = 0;
			// noexport: simply force this node not to be a bone
			if (CheckSubString(BoneName,_T("noexport")))  
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].InSkeleton = 0;	// DAVE: for hiding joints in hierarchy.  DigestSkeleton looks at InSkeleton, so zero it out.
				SerialTree[i].NoExport = 1;
			}

			// Ignore: make sure anything down the hierarchy is not exported.
			if (CheckSubString(BoneName,_T("ignore")))  
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].InSkeleton = 0;	// DAVE: for hiding joints in hierarchy.  DigestSkeleton looks at InSkeleton, so zero it out.
				SerialTree[i].IgnoreSubTree = 1;
			}

			// Todo? any exported geometry/skincluster references  that refer to such a non-exported
			// bone should default to bones higher in the hierarchy...			
		}
	}

	// IgnoreSubTree 
	for (INT i=0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
	{
		INT pIdx = SerialTree[i].ParentIndex;
		if( pIdx>=0 && SerialTree[pIdx].IgnoreSubTree )
		{
			SerialTree[i].IsBone = 0;
			SerialTree[i].InSkeleton = 0;	// DAVE: for hiding joints in hierarchy.  DigestSkeleton looks at InSkeleton, so zero it out.
			SerialTree[i].IgnoreSubTree = 1;
		}		
	}


	//
	// Tally the elements of the current skeleton - obsolete except for dummy culling.
	//
	for(INT i=0; i<SerialTree.Num(); i++)
	{
		if (SerialTree[i].InSkeleton )
		{
			TotalDummies  += SerialTree[i].IsDummy;
			TotalMaxBones += SerialTree[i].IsMaxBone;
			TotalBones    += SerialTree[i].IsBone;
		}
	}
	TotalBipBones = TotalBones - TotalMaxBones - TotalDummies;

	return 1;
};



/**
 * Removes the file path and the file extension from a full file path.
 * The only thing returned is the name of the file.
 * This is used to remove the reference name from nodes in a scene
 *
 * @param	InFullFilePath		The path and file name to clean
 *
 * @return	File name with no path or extension
 */
MString SceneIFC::StripFilePathAndExtension( const MString& InFullFilePath )
{
	MString FilePathOnly = InFullFilePath;

	// Finds the first occurrence of a '/' from the right of the string.
	// Assigns the location of the first '/' as an index of FilePathOnly.
	int SlashIndex = FilePathOnly.rindex( '/' );
	
	// As long as a '/' is found
	if( SlashIndex != -1 )
	{
		// Removes all characters from that first '/' to the beginning of the string.
		// This leaves us with <file name>.<extension>
		FilePathOnly = FilePathOnly.substring( SlashIndex + 1 , FilePathOnly.numChars() - 1 );
	}

	// Finds the first occurrence of a '.' from the left of the string.
	// Assigns the location of the first '.' as an index of FilePathOnly.
	int DotIndex = FilePathOnly.index( '.' );
	
	// As long as a '.' is found
	if( DotIndex != -1 )
	{
		// Removes all characters from that first '.' to the end of the string.
		// This leaves us with just <file name>
		FilePathOnly = FilePathOnly.substring( 0 , DotIndex - 1 );
	}
	
	// Returns only the file name
	return FilePathOnly;
}



/**
 * Returns the name of the specified node, minus any reference prefix (if configured to strip that)
 *
 * @param	DagNode		The node to return the 'clean' name of
 *
 * @return	The node name, minus any reference prefix
 */
MString SceneIFC::GetCleanNameForNode( MFnDagNode& DagNode )
{
	// By default we'll just use the node name by itself
	MString MayaDagNodeName = DagNode.name();

	// For referenced nodes
	// Removes the reference namespace from the current node name if a reference exists
	if( DagNode.isFromReferencedFile() )
	{
		// Does the user want us to strip the reference prefix from the node name?
		if( bCheckStripRef )
		{
			// Gets an array of all references in the scene
			MStringArray MayaReferenceList;
			MFileIO::getReferences( MayaReferenceList );				
//			MayaLog( "Found %i references.\n", MayaReferenceList.length() );

			for( int ReferenceNumber = 0 ; ReferenceNumber < MayaReferenceList.length() ; ReferenceNumber++ )
			{
				// Separate current reference into it's own variable
				MString FullReferencePath = MayaReferenceList[ ReferenceNumber ];
//				MayaLog( "Reference number %i: '%s'\n", ReferenceNumber, FullReferencePath.asChar() );

				// Get the reference file name only (no path)
				MString FileNameOnly = StripFilePathAndExtension( FullReferencePath );

				// Checks to see if the first character after the reference file name is a "_"
				// If so, removes the reference file name plus the "_" from the name of the current node
				// This handles the case where the file was referenced without using namespaces
				if( MayaDagNodeName.indexW( FileNameOnly + "_" ) == 0 )
				{
					// Rename the current node
					MayaDagNodeName = MayaDagNodeName.substring( FileNameOnly.numChars() + 1 , MayaDagNodeName.numChars() - 1 );

					// If the code gets to this point, no further action should be taken with the current node, even if 
					// there are other references in the scene.
					break;
				}

				// Checks to see if the first character after the reference file name is a ":"
				// If so, removes the reference file name plus the ":" from the name of the current node
				// This handles the case where the file was referenced using namespaces
				if( MayaDagNodeName.indexW( FileNameOnly + ":" ) == 0 )
				{
					// Rename the current node
					MayaDagNodeName = MayaDagNodeName.substring( FileNameOnly.numChars() + 1 , MayaDagNodeName.numChars() - 1 );

					// If the code gets to this point, no further action should be taken with the current node, even if 
					// there are other references in the scene.
					break;
				}
			}
		}
	}
	return MayaDagNodeName;
}


int SceneIFC::DigestSkeleton(VActor *Thing)
{
	SetMayaSceneTime( TimeStatic );

	if( DEBUGFILE )
	{
		char LogFileName[] = ("\\DigestSkeleton.LOG");
		DLog.Open(LogPath,LogFileName,DEBUGFILE);
	}

	// Digest bones and their transformations
	DLog.Logf(" Starting DigestSkeleton log \n\n");                                 
	
	Thing->RefSkeletonBones.SetSize(0);  // SetSize( SerialNodes ); //#debug  'ourbonetotal ?'
	Thing->RefSkeletonNodes.SetSize(0);  // SetSize( SerialNodes );

	//PopupBox("DigestSkeleton start - our bone total: %i Serialnodes: %i  RootBoneIndex %i \n",OurBoneTotal,SerialTree.Num()); //#TODO

	// for reassigning parents...
	TArray <INT> NewParents;

	// Assign all bones & store names etc.
	// Rootbone is in tree starting from RootBoneIndex.

	if( NOJOINTTEST )
	{	
		// MAYA specific: meshes aren't the hierarchy, TRANSFORMS are - so,
		// make sure that transforms point back to the 'mesh bones'.
		for (int c = 0; c<SerialTree.Num(); c++)
		{
			SerialTree[c].RepresentsBone = -1;
		}
		for (int j = 0; j<SerialTree.Num(); j++)
		{	
			if( SerialTree[j].InSkeleton )
			{
				SerialTree[j].RepresentsBone = j;
				INT ThisParent= SerialTree[j].ParentIndex;
				if( (SerialTree.Num() > ThisParent)  && (ThisParent > 0) )
				{
					// Point parent transform back to this 'bone' mesh.
					SerialTree[ThisParent].RepresentsBone = j;
				}
			}
		}

		// If a node has multiple children but does not represent a mesh(representsbone >-1) then it
		// should become its own bone to patch up a (jointless) hierarchy.	
		for (int k = 0; k<SerialTree.Num(); k++)
		{	
			if( (SerialTree[k].RepresentsBone == -1 ) && (SerialTree[k].NumChildren > 1 ) && ! SerialTree[k].HasMesh )
			{
				SerialTree[k].InSkeleton = 1;
				SerialTree[k].IsBone = 1;			
			}
		}
	}

	
	/*
	// Move mesh 'represents' status linearly down any nodes.		
	// Alternative: 

	INT AssignedMore = 0;
	do
	{
		AssignedMore = 0;
		TArray <INT> NewAssignments;
		NewAssignments.AddZeroed( SerialTree.Num() );
				
		for( int b=0; b<SerialTree.Num(); b++)
		{
			INT ParentIndex = SerialTree[b].ParentIndex;
			if( ( ParentIndex < 0 ) || (ParentIndex  >= SerialTree.Num())  )
				ParentIndex = 0;
			
			if(( SerialTree[b].RepresentsBone == -1) && ( SerialTree[ParentIndex].RepresentsBone != -1 ))
			{
				// Only allowed to progress one step down the hierachy each cycle.
				if( NewAssignments[ SerialTree[ParentIndex].RepresentsBone ] == 0 )
				{
					SerialTree[b].RepresentsBone = SerialTree[ParentIndex].RepresentsBone;
					AssignedMore++;
					// mark as having propagated once..
					NewAssignments[ SerialTree[ParentIndex].RepresentsBone ] = 1;
				}
			}
		}	
	}
	while( AssignedMore );
	*/
	
	
	
	//for (int i = RootBoneIndex; i<SerialNodes; i++)
	for (int i = 0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
	{
		if ( SerialTree[i].InSkeleton ) 
		{ 
			//PopupBox("Bone in skeleton: %i",i);

			// DLog.LogfLn(" Adding index [%i] into NewParents..",i); 

			// construct:
			//Thing->RefSkeletonBones.
			//Thing->RefSkeletonNodes.

			NewParents.AddItem(i);
			FNamedBoneBinary ThisBone; //= &Thing->RefSkeletonBones[BoneIndex];
			Memzero( &ThisBone, sizeof( ThisBone ) );

			//INode* ThisNode = (INode*) SerialTree[i].node;	

			MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
			MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 

			INT RootCheck = 0; // Thing->MatchNodeToSkeletonIndex( (void*) i );- skeletal hierarchy not yet digested.

			
			// Grab the name of this bone.  If the name has a prefix because it came from a reference
			// file, we'll strip that off first.
			MString MayaDagNodeName = GetCleanNameForNode( DagNode );

			// Copy name. Truncate/pad for safety. 
			char BoneName[MAX_PATH];
			sprintf_s(BoneName,MayaDagNodeName.asChar());

			ResizeString( BoneName,31); // will be padded if needed.

			// If necessary, change all underscores in name to spaces.
			if( this->DoReplaceUnderscores )
			{
				for( INT i=0; i<32; i++)
				{
					if( BoneName[i] == 95 )
					{
						BoneName[i] = 32; //replace all _ with " ".
					}
				}
			}

			_tcscpy_s((ThisBone.Name),BoneName);

			//PopupBox("Bone name: [%s] ",BoneName);

			ThisBone.Flags = 0;//ThisBone.Info.Flags = 0;
			ThisBone.NumChildren = SerialTree[i].NumChildren;//ThisBone.Info.NumChildren = SerialTree[i].NumChildren;


			// Find this bone's local parent, if we're not the root. 

			if (i != RootBoneIndex)
			{
				//#debug needs to be Local parent if we're not the root.
				int OldParent = SerialTree[i].ParentIndex;
				int LocalParent = -1;

				// Find what new index this corresponds to. Parent must already be in set.
				for (int f=0; f<NewParents.Num(); f++)
				{
					if( NewParents[f] == OldParent ) LocalParent=f;
				}

				if (LocalParent == -1 )
				{
					// ErrorBox(" Local parent lookup failed."); >> may be a transform rather than the root scene.
					LocalParent = 0;
				}

				ThisBone.ParentIndex = LocalParent;//ThisBone.Info.ParentIndex = LocalParent;
			}
			else
			{
				ThisBone.ParentIndex = 0;  // root links to itself...? .Info.ParentIndex
			}

			/* #SKEL! - non-bone hierarchy experiments
			if (i != RootBoneIndex)
			{
				int LocalParent = -1;				
				int TryParent = SerialTree[i].ParentIndex; // This is a SERIALTREE index, _not_ a NewParents( bone-only array ) index ! 				
				int TryGrandParent = -1;
				int TryGrandParentMesh = -1;				
				if( (SerialTree.Num() > TryParent)  && (TryParent > 0) )
					TryGrandParent = SerialTree[TryParent].ParentIndex;

				if( (SerialTree.Num() > TryGrandParent)  && (TryGrandParent > 0) )
					TryGrandParentMesh = SerialTree[TryGrandParent].RepresentsBone;
				
				DLog.LogfLn(" SerialTree[%i] parent: %i  Parentmesh: %i - looking for it in NewParents..(num: %i)...",i,TryParent,TryGrandParentMesh,NewParents.Num() );
				
				// Find what new index this corresponds to. Parent (or grandparent) must already be in set..
				INT Runaway=0;
				INT GrandParent = -1;

				for (int f=0; f<NewParents.Num(); f++)
				{
					if( (NewParents[f] == TryParent) || (NewParents[f] == TryGrandParentMesh) || (NewParents[f] == TryGrandParent)  ) 
					{
						LocalParent=f;
					}
				}
				if (LocalParent == -1 )
				{
					// ErrorBox(" Local parent lookup failed."); >> may be a transform rather than the root scene.
					LocalParent = 0;					
				}

				ThisBone.ParentIndex = LocalParent; // ThisBone.Info.ParentIndex = LocalParent;
			}
			else
			{
				ThisBone.ParentIndex = 0; // root links to itself...? .Info.ParentIndex
			}
			*/
			
			RootCheck = ( Thing->RefSkeletonBones.Num() == 0 ) ? 1:0; //#debug

			DLog.LogfLn(" Skeletal bone name %s num %i parent %i skelparent %i  NumChildren %i  Isroot %i RootCheck %i",BoneName,i,SerialTree[i].ParentIndex,ThisBone.ParentIndex,SerialTree[i].NumChildren, (i != RootBoneIndex)?0:1,RootCheck );

			FMatrix LocalMatrix;
			FQuat   LocalQuat;
			FVector BoneScale(1.0f,1.0f,1.0f);

			// Get trafo in 'scene' space, relative to root of the scene ?
			RetrieveTrafoForJoint( JointObject, LocalMatrix, LocalQuat, RootCheck, BoneScale );
			{
				DLog.LogfLn("Bone scale for  joint [%i]  %f %f %f ",i, BoneScale.X,BoneScale.Y,BoneScale.Z ); //********
				MStatus stat;	
				MFnDagNode fnTransNode( JointObject, &stat );
				MDagPath dagPath;
				stat = fnTransNode.getPath( dagPath );
				if(stat == MS::kSuccess) DLog.LogfLn("Path for joint [%i] [%s]",i,dagPath.fullPathName().asChar()); //********
			}
			
			// Clean up matrix scaling.

			// Raw translation.
			FVector Trans( LocalMatrix.WPlane.X, LocalMatrix.WPlane.Y, LocalMatrix.WPlane.Z );
			// Incorporate matrix scaling - for translation only.
			Trans.X *= BoneScale.X;
			Trans.Y *= BoneScale.Y;
			Trans.Z *= BoneScale.Z;

			// PopupBox("Local Bone offset for %s is %f %f %f ",DagNode.name().asChar(),Trans.X,Trans.Y,Trans.Z );			

			// Affine decomposition ?
			ThisBone.BonePos.Length = 0.0f ; //#TODO yet to be implemented

			// Is this node the root (first bone) of the skeleton ?

			// if ( RootCheck ) 
			//LocalQuat =  FQuat( LocalQuat.X,LocalQuat.Y,LocalQuat.Z, LocalQuat.W );

			ThisBone.BonePos.Orientation = LocalQuat; // FQuat( LocalQuat.X,LocalQuat.Y,LocalQuat.Z,LocalQuat.W );
			ThisBone.BonePos.Position = Trans;

			//ThisBone.BonePos.Orientation.Normalize();

			Thing->RefSkeletonNodes.AddItem( (void*) (int)SerialTree[i].node ); //index
			Thing->RefSkeletonBones.AddItem(ThisBone);			
		}
	}

	// Bone/node duplicate name check
	DuplicateBones = 0;
	{for(INT i=0; i< Thing->RefSkeletonBones.Num(); i++)
	{
		for(INT j=0; j< Thing->RefSkeletonBones.Num(); j++)
		{
			if( (i!=j) && ( strcmp( Thing->RefSkeletonBones[i].Name, Thing->RefSkeletonBones[j].Name)==0)  ) 
			{
				DuplicateBones++;
				WarningBox( " Duplicate bone name encountered in skeleton: %s ", Thing->RefSkeletonBones[j].Name );
			}
		}
	}}
	if (DuplicateBones)
	{
		WarningBox( "%i total duplicate bone name(s) detected.", DuplicateBones );		
	}

	DLog.Close();

	return 1;
}

void SceneIFC::SurveyScene()
{
	
	// Serialize scene tree 
	SerializeSceneTree();

}


///////////////////////////////////////////////
//
// Mel Command line driven export features.
//
///////////////////////////////////////////////


void ClearOutAnims()
{
	// Clear out anims...
	TempActor.RawAnimKeys.Empty();
	TempActor.RefSkeletonNodes.Empty();
	TempActor.RefSkeletonBones.Empty();

	{for( INT c=0; c<TempActor.OutAnims.Num(); c++)
	{
		TempActor.OutAnims[c].KeyTrack.Empty();
	}}
	TempActor.OutAnims.Empty();
	
	{for( INT c=0; c<TempActor.Animations.Num(); c++)
	{
		TempActor.Animations[c].KeyTrack.Empty();
	}}
	TempActor.Animations.Empty();
		
}


//
// "AXWriteSequence" - Write this complete scene sequence out as single file using current output path and scene name.
//
MStatus AXWriteSequence(BOOL bUseSourceOutPath)
{

	char OutPath[MAX_PATH];
	char OutFolder[MAX_PATH];
	char SceneName[MAX_PATH];

	GetNameFromPath(SceneName, MFileIO::currentFile().asChar(), MAX_PATH );

	if( strlen(SceneName) < 1 )
	{
		return MS::kFailure;
	}

	INT TotalKeys = 0;
	
	framerangestring[0] = 0; //  Clear framerange to default.
	
	// Digest full scene anim..
	OurScene.SurveyScene(); 
	
	if( (OurScene.SerialTree.Num() > 1) && (strlen(SceneName) > 0) ) // Anything detected; valid anim name ?
	{
			OurScene.GetSceneInfo();
			OurScene.EvaluateSkeleton(1);
			ClearOutAnims();  // Tempactor animations discard.
			OurScene.DigestSkeleton(&TempActor);
			OurScene.DigestAnim(&TempActor, SceneName, framerangestring );  
			OurScene.FixRootMotion( &TempActor );	
			TotalKeys = TempActor.RecordAnimation();  
	}
	
	// Move to OutAnims
	if( TempActor.Animations.Num() == 1 )
	{
		TempActor.OutAnims.AddZeroed(1); // Add a single element.
		TempActor.CopyToOutputAnims(0, 0);
		TempActor.Animations[0].KeyTrack.Empty(); 
	}
	
	//
	// Save it.
	//
	char to_ext[32];
	_tcscpy_s(to_ext, ("PSA"));
	if( !bUseSourceOutPath )
	{
		strcpysafe( OutFolder, to_path, MAX_PATH );
	}
	else // Use source art location as destination path.
	{
		GetFolderFromPath( OutFolder,MFileIO::currentFile().asChar(), MAX_PATH );
	}
	sprintf_s(OutPath,"%s\\%s.%s", OutFolder, SceneName, to_ext);

	if( SaveAnimSet( OutPath ) )
	{		
		return MS::kSuccess;
	}
	else
		return MS::kFailure;
}




//
// Write EVERY single frame out as a special pose PSA, using the current output path
// and scene name. 
//
MStatus AXWritePoses()
{
	char OutPath[MAX_PATH];
	char SceneName[MAX_PATH];

	GetNameFromPath( SceneName, MFileIO::currentFile().asChar(), MAX_PATH );

	INT TotalPoses = 0;
	INT ErrorTally = 0;
	INT TotalKeys = 0;	

	FLOAT TimeStart = GetMayaTimeStart();
	FLOAT TimeEnd = GetMayaTimeEnd();

	INT FrameSweepCounter = 0;

	// Start frame sweep.
	for( INT t=(INT)TimeStart; t<=(INT)TimeEnd; t++)
	{
		ClearOutAnims();
			
		// Put single number into framerange string.				
		char NumberedSequenceName[512];
		char SingleFrameString[63];
		NumberedSequenceName[0] = 0;
		sprintf_s(SingleFrameString,"%i",t);
		sprintf_s(NumberedSequenceName,"%s%i",SceneName,t);

		// Digest full scene anim..
		OurScene.SurveyScene(); 		
		if( (OurScene.SerialTree.Num() > 1) && (strlen(NumberedSequenceName) > 0) ) // Anything detected; valid anim name ?
		{
				OurScene.GetSceneInfo();
				OurScene.EvaluateSkeleton(1);
				OurScene.DigestSkeleton(&TempActor);
				OurScene.DigestAnim(&TempActor, NumberedSequenceName, SingleFrameString );  
				OurScene.FixRootMotion( &TempActor );	
				TotalKeys = TempActor.RecordAnimation();  
		}

		//
		// Move to OutAnims
		//

		if( TempActor.Animations.Num() == 1 )
		{
			TempActor.OutAnims.AddZeroed(1); // Add a single element.
			TempActor.CopyToOutputAnims(0, 0);
		}

		//
		// Save it.
		//

		char to_ext[32];
		_tcscpy_s(to_ext, ("PSA"));
		sprintf_s( OutPath,"%s\\%s%i.%s",(char*)to_path,(char*)SceneName,FrameSweepCounter,to_ext);					

		if ( TempActor.OutAnims.Num() == 0)
		{
			//PopupBox(" No stored animations available. ");
			ErrorTally++;
		}
		else
		{											
			FastFileClass OutFile;


			// Tally errors but no msg yet..
			if ( OutFile.CreateNewFile( OutPath ) != 0) // Error!
			{
				ErrorTally++;
			}
			else
			{
				// PopupBox(" Bones in first track: %i",TempActor.RefSkeletonBones.Num());
				TempActor.SerializeAnimation( OutFile );
				OutFile.CloseFlush();
				if( OutFile.GetError()) 
				{
					ErrorTally++;
				}
				else
				{
					TotalPoses++;
				}
			}
		}

		FrameSweepCounter++;

	}// End frame sweep.


	if( !OurScene.DoSuppressAnimPopups )
	{
		if( TotalPoses > 0 )
		{
			PopupBox( "Saved [%i] single-frame pose files, named [%s??] into folder [%s]", TotalPoses, SceneName, to_path );
		}

		if( ErrorTally > 0 )
		{
			PopupBox( "Error: [%i] single-frame animation save errors encountered.",ErrorTally);
		}

	}

	return MS::kSuccess;
}







//	
// Take care of command definitions. 
//





//
// Main MEL-type command implementations
//
//	Arguments:
//		args - the argument list that was passes to the command from MEL
//
//	Return Value:
//		MS::kSuccess - command succeeded
//		MS::kFailure - command failed (returning this value will cause the 
//                     MEL script that is being run to terminate unless the
//                     error is caught using a "catch" statement.
//


// "About"
MStatus ActorXTool0::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowAbout(hWnd);
	
	setResult( "command About  executed.\n" );
	return stat;
}

//"AXmain"
MStatus ActorXTool1::doIt( const MArgList& args )
{
	ClearScenePointers();

	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowPanelOne(hWnd);

	ClearScenePointers();
	
	setResult( " command AxMain executed.\n" );
	return stat;
}


//"AXOptions"
MStatus ActorXTool2::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowPanelTwo(hWnd);

	setResult( "command AXOptions executed.\n" );
	return stat;
}

 
//
//"utexportmesh [arglist]" mel command.
//
MStatus ActorXTool3::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;

	// Command line interface-free (selected) mesh exporting.
	UTExportMesh(args);

	setResult( "utexportmesh command executed.\n" );
	return stat;
}


//"AXAnim"
MStatus ActorXTool4::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowActorManager(hWnd);

	setResult( "command AxAnim executed.\n" );
	return stat;
}


// "axmesh"
MStatus ActorXTool5::doIt( const MArgList& args )
{
	ClearScenePointers(); 
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	// Interpret PARAMETERS - if none, call the panel.	
	if( args.length() > 0 )
	{
		//for (unsigned int i = 1; i < args.length(); ++i) 
		// Interpret arguments and export
		// the mesh immediately with
	    // the given name/filenames, don't pop up any windows.
	}	
	else
	{
		ShowStaticMeshPanel(hWnd);
		ClearScenePointers(); 
	}

	setResult( "command axmesh executed.\n" );
	return stat;
}


// "AXProcessSequence"
MStatus ActorXTool6::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	ClearScenePointers();

	HWND hWnd = GetActiveWindow();	
	AXWriteSequence( true ); // Switch to indicate output path is scene location.

	ClearScenePointers();
	setResult( "command axprocesssequence executed.\n" );

	return stat;
}


//"AXwritePoses"
MStatus ActorXTool7::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	ClearScenePointers(); 

	HWND hWnd = GetActiveWindow();
	
	AXWritePoses();

	ClearScenePointers(); 
	setResult( "command axwriteposes executed.\n" );
	return stat;
}

//"AXwritesequence"
MStatus ActorXTool8::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	ClearScenePointers();

	HWND hWnd = GetActiveWindow();	
	AXWriteSequence( false );

	ClearScenePointers();
	setResult( "command axwritesequence executed.\n" );
	return stat;
}


//"AXVertex"
MStatus ActorXTool10::doIt( const MArgList& args )
{

	ClearScenePointers(); 
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();
	
	ShowVertExporter( hWnd );
	ClearScenePointers(); 

	setResult( "command AXVertex executed.\n" );
	return stat;
}


INT ParseSpecifier(const MArgList& args, const char* specifier )
{
	INT SpecIdx = -1;
	for( INT i=0; i< (INT)args.length(); i++)
	{
		char TempArg[1024];
		strcpy_s( TempArg, args.asString(i).asChar() );
		if( CheckStringsEqual( TempArg, specifier ) )
		{
			SpecIdx = i;
		}
	}
	return SpecIdx;
}

UBOOL ParseSwitch( const MArgList& args, const char* specifier )
{	
	return( ParseSpecifier( args, specifier ) > -1 );
}

// 
// Find if specifier is in the argument list; if so, return the next argument .
//
UBOOL ParseCommand( const MArgList& args, const char* specifier, char* Output, size_t OutputLen )
{
	INT SpecIdx = ParseSpecifier( args, specifier );

	if( (SpecIdx == -1) ||  ((SpecIdx+1) >= (INT)args.length())  )
		return false;

	char TempOut[1024];
	TempOut[0]=0;
	strcpy_s( TempOut, args.asString(SpecIdx+1).asChar() );

	if( CheckSubString(  TempOut, "-") ) // UNLESS that starts with a - (another specifier...)
		return false;

	// Flatten the case...
	_strlwr_s(TempOut); 

	strcpy_s( Output, OutputLen, TempOut);
	return true;
}


// 
// Find if specifier is in the argument list; if so, return the next argument .
//
UBOOL ParseCommandKeepCase( const MArgList& args, const char* specifier, char* Output, size_t OutputLen )
{
	INT SpecIdx = ParseSpecifier( args, specifier );

	if( (SpecIdx == -1) ||  ((SpecIdx+1) >= (INT)args.length())  )
		return false;

	char TempOut[1024];
	TempOut[0]=0;
	strcpy_s( TempOut, args.asString(SpecIdx+1).asChar() );

	if( CheckSubString(  TempOut, "-") ) // UNLESS that starts with a - (another specifier...)
		return false;

	// Do not flatten the case.	

	strcpy_s( Output, OutputLen, TempOut);
	return true;
}


// Return all next arguments up to end or another "-xxx" switch inside single string.
UBOOL ParseMultiArgCommand( const MArgList& args, const char* specifier, char* Output, size_t OutputLen )
{
	INT SpecIdx = ParseSpecifier( args, specifier );

	if( (SpecIdx == -1) ||  ((SpecIdx+1) >= (INT)args.length())  )
		return false;

	char AllArguments[1024];
	AllArguments[0]=0;

	INT ArgCount = 0;

	while( (SpecIdx +1 ) < (INT)args.length() )  // more arguments left ?
	{
		char TempOut[1024];
		TempOut[0]=0;

		strcpy_s( TempOut, args.asString(SpecIdx+1).asChar() );

		if(  CheckSubString(  TempOut, "-") && (strlen(TempOut) != 1)  ) // UNLESS that starts with a - (another specifier...) - but single dashes/dots etc  are okay.
		{
			break; // Assume end of argument list.
		}
		else
		{ // append to existing with a space in between..
			if( ArgCount == 0) // First argument to be added?
			{
				strcpy_s( AllArguments, TempOut);
			}
			else
			{							
				if( CheckSubString( TempOut, "," ) || CheckSubString( TempOut,".")  )// Comma/dot part of argument?  just append literally, with space....
					sprintf_s(AllArguments,"%s %s",AllArguments,TempOut);
				else // any non-comma will be assumed part of a range and get a dash.
					sprintf_s(AllArguments,"%s-%s",AllArguments,TempOut);
			}
			ArgCount++;
		}		
		SpecIdx++;
	}

	// Flatten the case...
	_strlwr_s(AllArguments); 

	strcpy_s( Output, OutputLen, AllArguments );
	return true;
}


//
//"AXExecute" - uses command line arguments to enable automation via MEL script.
//
// - Digest argument list, and perform the requested actions (save PSA/PSK, digest an animation of a certain range etc).	
//

// PCF BEGIN
//	ActorXTool11() - command parsing changed to maya for robust support of file paths (old style doesn`t support  "-" i dir string)


MSyntax ActorXTool11::newSyntax()
{
	MSyntax syntax;


	syntax.addFlag("-p", "-path", MSyntax::kString   );
	syntax.addFlag("-sf", "-skinfile", MSyntax::kString   );
	syntax.addFlag("-af", "-animfile", MSyntax::kString   );
	syntax.addFlag("-sa", "-saveanim", MSyntax::kNoArg  );
	syntax.addFlag("-seq", "-sequence", MSyntax::kString  );
	syntax.addFlag("-r", "-range", MSyntax::kDouble,  MSyntax::kDouble  );
	syntax.addFlag("-tri", "-autotri", MSyntax::kNoArg   );
	syntax.addFlag("-smo", "-unsmooth", MSyntax::kNoArg   );
	syntax.addFlag("-rt", "-rate", MSyntax::kDouble   );
	//// MAKE COMMAND QUERYABLE AND NON-EDITABLE:
	//syntax.enableQuery(true);
	//syntax.enableEdit(true);

	return syntax;
}
static bool getStringFlagMayaCorrect(MArgDatabase & argData, const char* specifier, char * Output  )
{
	if (argData.isFlagSet(specifier))
	{
		MString _out;
		argData.getFlagArgument(specifier, 0, _out);
		_tcscpy_s( Output, 1024, _out.asChar() );
		return 1;

	}
	return 0;
}

MStatus ActorXTool11::doIt( const MArgList& args )
{

	ClearScenePointers(); 
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	char ExSkinFileName[1024];
	char ExAnimFileName[1024];
	char ExPathName[1024];
	char ExSequenceName[1024];
	char ExRangeString[1024];
	char ExRateString[1024];

	ExSkinFileName[0]=0;
	ExAnimFileName[0]=0;
	ExPathName[0]=0;
	ExSequenceName[0]=0;
	ExRangeString[0]=0;
	ExRateString[0]=0;

	MArgDatabase argData(syntax(), args);

	// problem  - in paths there are multiple "-" Maya Arg parser do it correct
	UBOOL NewPath =getStringFlagMayaCorrect(argData, "-path", ExPathName);

	UBOOL DoDigestAnim = getStringFlagMayaCorrect(argData, "-sequence", ExSequenceName);
		

	//UBOOL NewPath = ParseCommand( args, "-path", ExPathName );  // Path for all PSA / PSK output.


	//UBOOL DoSkin = ParseCommandKeepCase( args, "-skinfile", ExSkinFileName);	// PSK name. Will be used for PSA if no separate -animname  is specified.
	UBOOL DoSkin =getStringFlagMayaCorrect(argData, "-skinfile", ExSkinFileName);
	//ParseCommandKeepCase( args, "-animfile", ExAnimFileName);	// Will be used for PSA.
	getStringFlagMayaCorrect(argData, "-animfile", ExAnimFileName);

	UBOOL DoAppendAnim = argData.isFlagSet("-appendfile") ;
	UBOOL DoSaveAnim = argData.isFlagSet("-saveanim") ;

	UBOOL BackupDoAutoTriangles = OurScene.DoAutoTriangles;
	UBOOL BackupDoUnSmooth =  OurScene.DoUnSmooth;

	OurScene.DoAutoTriangles = argData.isFlagSet("-autotri");  // On by default, though ?
	OurScene.DoUnSmooth = argData.isFlagSet("-unsmooth"); 
	// Specified animation range?
	

	if (argData.isFlagSet("-range"))
	{
		double range0 = 0;
		double range1 = 0;
		argData.getFlagArgument("-range", 0, range0);
		argData.getFlagArgument("-range", 1, range1);
		MString aa ;
		aa = range0;
		aa += " ";
		aa += range1;
		strcpy_s( framerangestring, aa.asChar() );
	}
	
	FLOAT ForcedRate = 0.0f;
	if (argData.isFlagSet("-rate"))
	{
		double rte = 0;

		argData.getFlagArgument("-rate", 0, rte);
		ForcedRate = float(rte);
	}
	//if( ParseMultiArgCommand( args, "-range", ExRangeString) ) // Rangestring will contain any and all arguments (separated by spaces) until the next "-xxx" argument.
	//{
	//	strcpy_s( framerangestring, ExRangeString ); // Framerangestring is the plugin's default range text. when exporting a sequence.	
	//}

	//FLOAT ForcedRate = 0.0f;
	//if( ParseCommand( args, "-rate", ExRateString ) )
	//{
	//	ForcedRate = atof( ExRateString );
	//}
	
	//
	// Actions taken depend on arguments.  Multiple file actions can take place.
	//   -skin specified:   digest+save skin.
	//   -sequence :         digest animation.
	//   -path: set (global) path for psk/psa file.
	//   -saveanims:        save animations (overwrite existing PSA) - erase any sequences in the buffer.
	//   -appendanims:   save animations( append to existing PSA if compatible...)- erase any sequences in the buffer.
	//

	//  Temporarily suppress all window popups - we're dealing with script-initiated actions...	
	INT SavedAnimPopupState = OurScene.DoSuppressAnimPopups;
	INT SavedPopupState = OurScene.DoSuppressPopups;

	OurScene.DoSuppressAnimPopups = true;
	OurScene.DoSuppressPopups = true;

	// If a valid path was specified, change the global output path to it.	
	if( strlen( ExPathName) > 0 ) // TODO - could use a better validity criterium...
	{
		_tcscpy_s(to_path,ExPathName); 
		_tcscpy_s(LogPath,ExPathName);	
	}

	if( DoSkin )
	{
		// If name as supplied is valid, use it..
		if( strlen( ExSkinFileName ) > 0 ) // TODO - could use a better validity criterium...
		{
			_tcscpy_s(to_skinfile, ExSkinFileName );
		}
		SaveSceneSkin();
	}

	if( DoDigestAnim )
	{		
		// Specified sequence name ?
		if( strlen( ExSequenceName ) )
		{
			UBOOL Backup_DoForceRate = OurScene.DoForceRate;

			_tcscpy_s( newsequencename, ExSequenceName );
			if( ForcedRate !=0.0f )
			{
				OurScene.DoForceRate = true;
				OurScene.PersistentRate = ForcedRate;
			}
			
			DoDigestAnimation();			

			OurScene.DoForceRate = Backup_DoForceRate;
		}		
	}

	if( DoAppendAnim )
	{		
		//TODO see: IDC_QUICKSAVE..  - load the specified PSA if exists in the output path, and then let regular save code do the rest....
	}
	

	if( DoSaveAnim  )
	{
		// If name as supplied is valid, use it..
		if( strlen( ExAnimFileName ) > 0 ) 
		{
			_tcscpy_s(to_animfile, ExAnimFileName );
		}
		else // go with skin name if no valid anim name specified;.
		if( strlen( ExSkinFileName ) > 0 ) 
		{
			_tcscpy_s(to_animfile, ExSkinFileName );
		}
		
		// Copy _all_ digested animations to output data.  Respect any existing output data....
		for( INT i=0; i<TempActor.Animations.Num(); i++)
		{
			INT NewOutIdx = TempActor.OutAnims.AddZeroed(1);
			TempActor.CopyToOutputAnims(i, NewOutIdx);
		}

		// Save animation - as done in Dialogs.cpp		
		char to_ext[32];
		_tcscpy_s(to_ext, ("PSA"));
		sprintf_s(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_animfile,to_ext);
		SaveAnimSet( DestPath );    // No error popups...

		// Erase all the animation buffers.
		 ClearOutAnims();
	}

		
	/////////////////////
	OurScene.DoSuppressAnimPopups = SavedAnimPopupState;
	OurScene.DoSuppressPopups = SavedPopupState;
	
	char ResultString[1024];
	sprintf_s( ResultString,  "command AXExecute executed. ");//  [%s] [%s] [%s] [%s] [%s] \n", ExPathName,ExSkinFileName,ExSequenceName,ExRangeString, ExRateString );  
	setResult( ResultString );	
	
	OurScene.DoAutoTriangles = BackupDoAutoTriangles;
	OurScene.DoUnSmooth = BackupDoUnSmooth;

	return stat;
}
// PCF END


//
// "axpskfixup" command.  Undocumented function for quick fixup of PSK with body/face material assignments out of order..
// Load arbitrary PSK; swap order of first 2 materials; ( need to swap indices on mesh faces too !) and  write 
// back with modified (or input..) name.
// 

MStatus ActorXTool12::doIt( const MArgList& args )
{

	char PSKpath[MAX_PATH] = "";	
	char SAVEDpath[MAX_PATH] = "";

	ClearScenePointers(); 
	TempActor.Cleanup();

	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();	

	FastFileClass PSKInFile;

	// Parse input filename & PSK loading.
	char filterList[] = "PSK Files (*.psk)\0*.psk\0PSK Files (*.psk)\0*.psk\0";	

	GetLoadName( hWnd, PSKpath, _countof(PSKpath), to_path, filterList);
	//GetNameFromPath(to_pskfile,newname, MAX_PATH );	
	
	_tcscpy_s( SAVEDpath, PSKpath );

	
	if ( PSKInFile.OpenExistingFileForReading( PSKpath ) != 0 ) // Error!
	{
		ErrorBox("File [%s] does not exist yet. ", PSKpath );
		PSKInFile.Close();
	}
	else // Load all relevant chunks into TempActor and its arrays.
	{
		//PopupBox(" Starting to load file : %s ",PSKpath);
		INT DebugBytes = TempActor.LoadActor(PSKInFile);		
		// Log input 
		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox("Reference mesh loaded, %i faces and %i bones.",TempActor.SkinData.Faces.Num(),TempActor.RefSkeletonBones.Num());
		}
		PSKInFile.Close();
	
		_tcscat_s( PSKpath,("_fixed")); //#SKEL
		
		FastFileClass PSKOutFile;
		
		if( PSKOutFile.CreateNewFile( PSKpath ) == 0 )
		{
			// Swap indices - swap the indices on the faces, effectively swapping body/head for most player models.
			{for( INT i=0; i< TempActor.SkinData.Faces.Num(); i++)
			{
				TempActor.SkinData.Faces[i].MatIndex = ((DWORD)TempActor.SkinData.Faces[i].MatIndex) ^ 1; 
			}}
			{for( INT i=0; i< TempActor.SkinData.Wedges.Num(); i++)
			{
				TempActor.SkinData.Wedges[i].MatIndex = ((DWORD)TempActor.SkinData.Wedges[i].MatIndex) ^ 1; 
			}}
	
			// Write PSK back to the SAME file name; use fixed indicator
			TempActor.SerializeActor( PSKOutFile );			
		}
		else
		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox("Reference mesh saving failed - name [%s] ",PSKpath );
		}


		PSKOutFile.Close();
	}

	TempActor.Cleanup();
	ClearScenePointers(); 	

	return stat;
}



/////////////////////////////////////////////////////////////////////////
#if TESTCOMMANDS
//	getToolsPath  #TODO - adapt for maya ! #EDN!

MString getToolsPath()
{
	char	DirName[MAX_PATH];
	GetModuleFileNameA( MhInstPlugin,DirName,MAX_PATH);  // MhInstPlugin; #EDN - see Instance in MAX equivalent #TODO check .

	// Remove final "\" ??
	char*	LastBackslash = strrchr(DirName,'\\') + 1;
	*LastBackslash = 0;

	MString Result( DirName );	
	return Result;
}


// Commands for test purposes.
MStatus ActorXTool21::doIt( const MArgList& args ) // "axtest1"
{
	MStatus stat = MS::kSuccess;
	MayaLog("- Test 1b -");


	//stringToC<char>( toString("MeshProcess.exe") ).Data,	//#EDN - arg0 pathname not needed (arg0 is formality - though MeshProcess.exe itself might use it )
	// Spawning test....
	//intptr_t spawnResult = _spawnl( _P_NOWAIT, (getToolsPath()+MString("MeshProcess")).asChar(), (getToolsPath()+MString("MeshProcess") ).asChar(), 		
	intptr_t spawnResult = _spawnl( _P_NOWAIT, (getToolsPath()+MString("cmd")).asChar(), (getToolsPath()+MString("cmd") ).asChar(),"/c","doit.bat",		
		 (getToolsPath()+MString("ProcessTemp.dat")).asChar(),
		NULL );

	if(( spawnResult == -1))
	{
		if( errno == 2 )
			MayaLog(" Error - MeshProcess.exe not found in plugin path.");
		else
			MayaLog("Unknown error while spawning MeshProcess.exe. ");
	}

	/*	 
	selected  'errno' system error message values:
	E2BIG Argument list too long		7 			
	EINVAL Invalid argument		22 
	EMFILE Too many open files	24 
	ENOENT No such file or directory		2
	ENOMEM          12
	ENOEXEC 8 
	? 1024 + 8  
	*/

	return stat;
}


MStatus ActorXTool22::doIt( const MArgList& args ) // "axtest2"
{
	MStatus stat = MS::kSuccess;
	MayaLog("- Test 2 -");

	return stat;
}

#endif // TESTCOMMANDS
///////////////////////////////////////////////////////////////


//
// Maya-specific SceneIFC implementations.
//
static INT_PTR CALLBACK AboutBoxDlgProc(HWND hWnd, UINT msg, 
	WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{

	case WM_INITDIALOG:
		//CenterWindow(hWnd, GetParent(hWnd)); 
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) 
		{
		case IDOK:
			EndDialog(hWnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
		break;
		}
		break;

		default:
			return FALSE;
	}
	return TRUE;	
}       



void ShowAbout(HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutBoxDlgProc, 0 );
}

void ShowInfo(HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_SCENEINFO), hWnd, SceneInfoDlgProc, 0 );
}

void DoDigestAnimation()
{
	// Bake animation if requested.
	if( OurScene.DoBakeAnims )
	{
		INT BakedChannels = 0;
		MString commandString = MString( "bakeResults -t \":\" -simulation true \"joint*\"" );  //bakeResults -t ":" -simulation true "joint*"    == bake all joints for all times with full scene evaluation.
		// #EDN - error: with -simulation , it won't accept : any more...
		// Also - it doesn't seem to make a difference for the test character....
		MStatus	status = MGlobal::executeCommand( commandString, BakedChannels );
		if ( MS::kSuccess == status )
		{
			// Baked channels # : in BakedChannels.
		}
		else
		{
			// PopupBox("Unable to bake animations - unknown error.");
		}
		//PopupBox(" Command text for baking:  [%s] ",commandString.asChar());
	}

	// Scene stats
	DebugBox("Start serializing scene tree; serialtree elements: %i ",  OurScene.SerialTree.Num() );
	OurScene.SurveyScene();

	if( OurScene.SerialTree.Num() > 1 ) // Anything detected ?
	{
		//u->ip->ProgressStart((" Digesting Animation... "), true, fn, NULL );

		DebugBox("Start getting scene info; Scene tree size: %i",OurScene.SerialTree.Num() );
		OurScene.GetSceneInfo();

			DebugBox("Start evaluating Skeleton.");
		OurScene.EvaluateSkeleton(1);

			DebugBox("Start digesting of Skeleton.");
		OurScene.DigestSkeleton(&TempActor);

			DebugBox("Start digest Anim.");
		OurScene.DigestAnim(&TempActor, newsequencename, framerangestring );  

		OurScene.FixRootMotion( &TempActor );

		// PopupBox("Digested animation:%s Betakeys: %i Current animation # %i", animname, TempActor.BetaKeys.Num(), TempActor.Animations.Num() );
		if(OurScene.DoForceRate )
			TempActor.FrameRate = OurScene.PersistentRate;

			DebugBox("Start recording animation");
		INT DigestedKeys = TempActor.RecordAnimation();			
		
		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox("Animation digested: [%s] total frames: %i  total keys: %i  ", newsequencename, OurScene.FrameList.GetTotal(), DigestedKeys);
		}
	}
}

// Save all weights of current frame, so that we can go back afterwards
struct VCurrentCurve
{
	INT			BlendShapeIndex; // blend shape index
	INT			BlendCurveIndex; // blend curve index
	ANSICHAR	CurveName[128]; // curvename
	FLOAT 		Weight;			// weight of current frame
};

// Now this is main function to save scene skin
// This determines if it needs to export more than just base skin or not
void SaveSceneSkin()
{
	// Save all logs
	TArray<ANSICHAR*> LogList;
	
	// save current ref mesh
	SaveCurrentSceneSkin(to_skinfile, LogList);

	// If export blend shape is checked
	if ( ExportBlendShapes )
	{
		// save all blend shapes current value;
		// After SaveCurrentSceneSkin, AxBlendShapes information is saved
		MObjectArray BlendShapes = AxBlendShapes;
		TArray<VCurrentCurve> Curves;

		for (INT I=0; I<BlendShapes.length(); ++I)
		{
			MObject Object = BlendShapes[I]; 
			// if blend shape exists
			if ( Object.hasFn(MFn::kBlendShape) )
			{
				MFnBlendShapeDeformer fn(Object);
				VCurrentCurve NewItem;
				NewItem.BlendShapeIndex = I;

				for (INT J=0; J<fn.numWeights(); ++J)
				{
					char AttributeName[MAX_PATH], OutputName[128];

					sprintf_s(AttributeName,"%s.weight[%d]",fn.name().asChar(),J);		

					if (GetAliasName(AttributeName, OutputName, 128))
					{
						strcpysafe(NewItem.CurveName, OutputName, 128);
					}
					else
					{
						WarningBox("[BlendCurve] Failed to get alias name for [%s]", AttributeName);
					}

					NewItem.Weight = fn.weight(J);
					NewItem.BlendCurveIndex = J;

					// add new item
					Curves.AddItem(NewItem);
				}
			}
		}

		// check duplicated name - then rename to [name]_#

		// now go through all of them and modify and save current skin for each
		for (INT I=0; I<Curves.Num(); ++I)
		{
			const VCurrentCurve& Current = Curves[I];
			MFnBlendShapeDeformer fn(BlendShapes[Current.BlendShapeIndex]);
			fn.setWeight(Current.BlendCurveIndex, 1.f);

			// now set everybody else to 0
			for (INT J=0; J<Curves.Num(); ++J)
			{
				if ( I!=J )
				{
					const VCurrentCurve& Other = Curves[J];
					MFnBlendShapeDeformer fn(BlendShapes[Other.BlendShapeIndex]);
					fn.setWeight(Other.BlendCurveIndex, 0.f);
				}
			}

			SaveCurrentSceneSkin(Current.CurveName, LogList);
		}
		
		// now return to previous values, so that it doesn't screwn up the data
		for (INT I=0; I<Curves.Num(); ++I)
		{
			const VCurrentCurve& Current = Curves[I];
			MFnBlendShapeDeformer fn(BlendShapes[Current.BlendShapeIndex]);
			fn.setWeight(Current.BlendCurveIndex, Current.Weight);
		}
	}

	if( !OurScene.DoSuppressAnimPopups )
	{
		char MESSAGE[4096];
		MESSAGE[0] = 0;

		for ( INT I=0; I<LogList.Num(); ++I )
		{
			char BUFFER[512];
			sprintf(BUFFER, "%s\r\n", LogList[I]);
			strcat(MESSAGE, BUFFER);
		}

		PopupBox(" [SUMMARY] \r\n %s ",MESSAGE );

		for ( INT I=0; I<LogList.Num(); ++I )
		{
			delete[] LogList[I];
		}
	}
}

void AddSkinLog(const char * OutputFile, TArray<char*>& LogList, char* Log, ... )
{
	if ( Log && Log[0]!=0 && OutputFile && OutputFile[0]!=0 )
	{
		char TempStr[512];
		GET_VARARGS(TempStr,512,Log,Log);

		char * NewLog = new char[sizeof(TempStr) + sizeof(OutputFile) + 10];
		sprintf(NewLog, "[%s] %s", OutputFile, TempStr);
		LogList.AddItem(NewLog);
	}
}

void SaveCurrentSceneSkin(const char * OutputFile, TArray<char*>& LogList)
{
	//cerr << endl<< "	SaveCurrentSceneSkin()"  << endl;
	// Scene stats. #debug: surveying should be done only once ?
	OurScene.SurveyScene();					
	OurScene.GetSceneInfo();
	OurScene.EvaluateSkeleton(1);

	//PopupBox("Start digest Skeleton");
	OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

	// Skin
	OurScene.DigestSkin( &TempActor );

	if( OurScene.DoUnSmooth || OurScene.DoTangents )
	{
		INT VertsDuplicated = OurScene.DoUnSmoothVerts( &TempActor, OurScene.DoTangents ); // Optionally splits on UV tangent space handedness, too.

		if( !OurScene.DoSuppressAnimPopups )
		{
			AddSkinLog(OutputFile, LogList, "Unsmooth groups processing: [%i] vertices added.", VertsDuplicated );
		}
	}

	//DLog.Logf(" Skin vertices: %6i\n",Thing->SkinData.Points.Num());

	if( OurScene.DuplicateBones > 0 )
	{
		WarningBox(" Non-unique bone names detected - model not saved. Please check your skeletal setup.");
	}
	else if( TempActor.SkinData.Points.Num() == 0)
	{
		WarningBox(" Warning: No valid skin triangles digested (mesh may lack proper mapping or valid linkups)");
	}
	else
	{
		// WarningBox(" Warning: Skin triangles found: %i ",TempActor.SkinData.Points.Num() ); 

		// TCHAR MessagePopup[512];
		TCHAR to_ext[32];
		_tcscpy_s(to_ext, ("PSK"));
		sprintf_s(DestPath,"%s\\%s.%s",(char*)to_path,(char*)OutputFile,to_ext);
		FastFileClass OutFile;

		if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
			ErrorBox( "Skin File creation error. ");

		TempActor.SerializeActor(OutFile); //save skin
	
		 // Close.
		OutFile.CloseFlush();
		if( OutFile.GetError()) ErrorBox("Skin Save Error");

		DebugBox("Logging skin info:");
		// Log materials and skin stats.
		if( OurScene.DoLog )
		{
			OurScene.LogSkinInfo( &TempActor, OutputFile );
			//DebugBox("Logged skin info:");
		}
				
		if( !OurScene.DoSuppressAnimPopups )
		{
			AddSkinLog(OutputFile, LogList, " Skin file %s.%s written.",OutputFile,to_ext );
		}
	}
}


// Maya panel 1 (like Max Utility Panel)

static INT_PTR CALLBACK PanelOneDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
		case WM_INITDIALOG:
			// Store the object pointer into window's user data area.
			// u = (MaxPluginClass *)lParam;
			// SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);
			// SetInterface(u->ip);
			// Initialize this panel's custom controls, if any.
			// ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ATIME,IDC_SPIN_ATIME,0,0,1000);

			// Initialize all non-empty edit windows.
			_SetCheckBox(hWnd, IDC_INCLUDEBLENDSHAPES, ExportBlendShapes);

			if ( to_path[0]     ) SetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path);
				_tcscpy_s(LogPath,to_path); 		

			if ( to_skinfile[0] ) SetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile);
			if ( to_animfile[0] ) PrintWindowString(hWnd,IDC_ANIMFILENAME,to_animfile);
			if ( newsequencename[0]    ) PrintWindowString(hWnd,IDC_ANIMNAME,newsequencename);
			if ( framerangestring[0]  ) PrintWindowString(hWnd,IDC_ANIMRANGE,framerangestring);
			if ( basename[0]    ) PrintWindowString(hWnd,IDC_EDITBASE,basename);
			if ( classname[0]   ) PrintWindowString(hWnd,IDC_EDITCLASS,classname);		
			break;
	
		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
				case IDOK:
					EndDialog(hWnd, 1);
					break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;
			}

			/*
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					EnableAccelerators();
					break;
			}
			*/

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam)) 
			{
				case IDC_ANIMNAME:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMNAME),newsequencename,100);
						}
						break;
					};
				}
				break;		

				case IDC_ANIMRANGE:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMRANGE),framerangestring,500);
						}
						break;
					};
				}
				break;				

				case IDC_PATH:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path,300);										
							_tcscpy_s(LogPath,to_path);
							PluginReg.SetKeyString("TOPATH", to_path );
						}
						break;
					};
				}
				break;

				case IDC_SKINFILENAME:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile,300);
							PluginReg.SetKeyString("TOSKINFILE", to_skinfile );
						}
						break;
					};
				}
				break;

				case IDC_ANIMFILENAME:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMFILENAME),to_animfile,300);
							PluginReg.SetKeyString("TOANIMFILE", to_animfile );
						}
						break;
					};
				}
				break;
				
				case IDC_INCLUDEBLENDSHAPES:
				{
					case EN_KILLFOCUS:
					{
						ExportBlendShapes = _GetCheckBox(hWnd,IDC_INCLUDEBLENDSHAPES);
					}
				}
				break;

				case IDC_ANIMMANAGER: // Animation manager activated.
				{
					HWND hWnd = GetActiveWindow();
					ShowActorManager(hWnd);
				}
				break;
	
				// Browse for a destination directory.
				case IDC_BROWSE:
				{					
					char  dir[MAX_PATH];
					if( GetFolderName(hWnd, dir, _countof(dir)) )
					{
						_tcscpy_s(to_path,dir); 
						_tcscpy_s(LogPath,dir);					
						PluginReg.SetKeyString("TOPATH", to_path );
					}
					SetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path);															
				}	
				break;
			

				// Quickly save all digested anims to the animation file, overwriting whatever's there...
				case IDC_QUICKSAVE:
				{						
					if( TempActor.Animations.Num() <= 0 )
					{
						PopupBox("Error: no digested animations present to quicksave.");
						break;
					}

					INT ExistingSequences = 0;
					INT FreshSequences = TempActor.Animations.Num();

					// If 'QUICKDISK' is on, load to output package, move/overwrite, and save all at once.
					char to_ext[32];
					_tcscpy_s(to_ext, ("PSA"));
					sprintf_s(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_animfile,to_ext);					
 
					// Optionally load them from the existing on disk..
					if( OurScene.QuickSaveDisk )
					{
						// Clear outanims...
						{for( INT c=0; c<TempActor.OutAnims.Num(); c++)
						{
							TempActor.OutAnims[c].KeyTrack.Empty();
						}}
						TempActor.OutAnims.Empty();

						// Quick load; silent if file doesn't exist yet.
						{							
							FastFileClass InFile;
							if ( InFile.OpenExistingFileForReading(DestPath) != 0) // Error!
							{
								//ErrorBox("File [%s] does not exist yet.", DestPath);
							}
							else // Load all relevant chunks into TempActor and its arrays.
							{
								INT DebugBytes = TempActor.LoadAnimation(InFile);								
								// Log input 
								// PopupBox("Total animation sequences loaded:  %i", TempActor.OutAnims.Num());
							}
							InFile.Close();
						}				
						
						ExistingSequences = TempActor.OutAnims.Num();
					}

					int NameUnique = 1;
					int ReplaceIndex = -1;
					int ConsistentBones = 1;

					// Go over animations in 'inbox' and copy or add to outbox.
					for( INT c=0; c<TempActor.Animations.Num(); c++)
					{
					    INT AnimIdx = c;

						// Name uniqueness check as well as bonenum-check for every sequence; _overwrite_ if a name already exists...
						for( INT t=0;t<TempActor.OutAnims.Num(); t++)
						{
							if( strcmp( TempActor.OutAnims[t].AnimInfo.Name, TempActor.Animations[AnimIdx].AnimInfo.Name ) == 0)
							{
								NameUnique = 0;
								ReplaceIndex = t;
							}

							if( TempActor.OutAnims[t].AnimInfo.TotalBones != TempActor.Animations[AnimIdx].AnimInfo.TotalBones)
								ConsistentBones = 0;
						}

						// Add or replace.
						if( ConsistentBones )
						{
							INT NewIdx = 0;
							if( NameUnique ) // Add -
							{
								NewIdx = TempActor.OutAnims.Num();
								TempActor.OutAnims.AddZeroed(1); // Add a single element.
							}
							else // Replace.. delete existing.
							{
								NewIdx = ReplaceIndex;
								TempActor.OutAnims[NewIdx].KeyTrack.Empty();
							}

							TempActor.CopyToOutputAnims(AnimIdx, NewIdx);
						}
						else 
						{							
							PopupBox("ERROR !! Aborting the quicksave/move, inconsistent bone counts detected.");
							break;
						}						
					}

					// Delete 'left-box' items.
					if( ConsistentBones )
					{
						// Clear 'left pane' anims..
						{for( INT c=0; c<TempActor.Animations.Num(); c++)
						{
							TempActor.Animations[c].KeyTrack.Empty();
						}}
						TempActor.Animations.Empty();
					}
										
										
					// Optionally SAVE them _over_ the existing on disk (we added/overwrote stuff)
					if( OurScene.QuickSaveDisk )
					{
						if ( TempActor.OutAnims.Num() == 0)
						{
							PopupBox(" No digested animations available. ");
						}
						else
						{											
							FastFileClass OutFile;
							if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
							{
								ErrorBox("Anim QuickSave File creation error.");
							}
							else
							{
								// PopupBox(" Bones in first track: %i",TempActor.RefSkeletonBones.Num());
								TempActor.SerializeAnimation(OutFile);
								OutFile.CloseFlush();
								if( OutFile.GetError()) ErrorBox("Animation Save Error.");
							}

							INT WrittenBones = TempActor.RefSkeletonBones.Num();

							if( OurScene.DoLog )
							{
								OurScene.LogAnimInfo(&TempActor, to_animfile ); 												
							}

							if( !OurScene.DoSuppressAnimPopups )
							{
								PopupBox( "OK: Animation sequence appended to [%s.%s] total: %i sequences.", to_animfile, to_ext, TempActor.OutAnims.Num() );
							}
						}			
					}
				}
				break;
				
				// A click inside the logo.
				case IDC_LOGOUNREAL: 
				{
					// Show our About box.
				}
				break;
			
				// Show the 'scene info' box.
				case IDC_EVAL:
				{
					// Survey the scene, to conclude which items to export or evaluate, if any.
					OurScene.SurveyScene();
					// Initializes the scene tree.												
					OurScene.GetSceneInfo();
					OurScene.EvaluateSkeleton(1);					
					/////////////////////////////
					HWND hWnd = GetActiveWindow();
					ShowInfo(hWnd);
				}
				break;
			
				case IDC_SAVESKIN:
				{
					SaveSceneSkin();
				}
				break;

				case IDC_DIGESTANIM:
				{									
					DoDigestAnimation();
				}
				break;			
			}
			break;
			// END command

		default:
			return FALSE;
	}
	return TRUE;
}       


static INT_PTR CALLBACK PanelTwoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{	
	switch (msg) 
	{
		case WM_INITDIALOG:
			// Store the object pointer into window's user data area.
			//u = (MaxPluginClass *)lParam;
			//SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);

			// Optional persistence: retrieve setting from registry.						
			INT SwitchPersistent;
			PluginReg.GetKeyValue("PERSISTSETTINGS", SwitchPersistent);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS,SwitchPersistent?1:0);

			INT SwitchPersistPaths;
			PluginReg.GetKeyValue("PERSISTPATHS", SwitchPersistPaths);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTPATHS,SwitchPersistPaths?1:0);

			//if ( OurScene.DoForceRate ) 
			{
				char RateString[MAX_PATH];				
				sprintf_s(RateString,"%5.3f",OurScene.PersistentRate);
				PrintWindowString(hWnd,IDC_PERSISTRATE,RateString);		
			}
					
			// Make sure defaults are updated when boxes first appear.
			_SetCheckBox( hWnd, IDC_QUICKSAVEDISK, OurScene.QuickSaveDisk );
			_SetCheckBox( hWnd, IDC_CHECK_STRIP_REFERENCE_PREFIX, OurScene.bCheckStripRef );

			_SetCheckBox( hWnd, IDC_FIXROOT, OurScene.DoFixRoot);
			_SetCheckBox( hWnd, IDC_LOCKROOTX, OurScene.DoLockRoot);
			_SetCheckBox( hWnd, IDC_POSEZERO, OurScene.DoPoseZero);

			_SetCheckBox( hWnd, IDC_CHECKNOLOG, !OurScene.DoLog);			
			_SetCheckBox( hWnd, IDC_CHECKDOUNSMOOTH, OurScene.DoUnSmooth);
			_SetCheckBox( hWnd, IDC_CHECKDOTANGENTS, OurScene.DoTangents);			
			_SetCheckBox( hWnd, IDC_CHECKAUTOTRI, OurScene.DoAutoTriangles);

			_SetCheckBox( hWnd, IDC_CHECKSKINALLT, OurScene.DoTexGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINALLP, OurScene.DoPhysGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINALLS, OurScene.DoSelectedGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINNOS,  OurScene.DoSkipSelectedGeom );
			_SetCheckBox( hWnd, IDC_CHECKTEXPCX,   OurScene.DoTexPCX );
			_SetCheckBox( hWnd, IDC_CHECKQUADTEX,  OurScene.DoQuadTex );
			_SetCheckBox( hWnd, IDC_CHECKCULLDUMMIES, OurScene.DoCullDummies );
			_SetCheckBox( hWnd, IDC_CHECKHIERBONES, OurScene.DoHierBones );
			_SetCheckBox( hWnd, IDC_CHECKFORCERATE, OurScene.DoForceRate );
			_SetCheckBox( hWnd, IDC_UNDERSCORETOSPACE, OurScene.DoReplaceUnderscores );
			//PCF BEGIN
			_SetCheckBox( hWnd, IDC_DOSECONDUVSETSKINNED, OurScene.DoSecondUVSetSkinned );
			//PCF END

			_SetCheckBox( hWnd, IDC_CHECKFIXEDSCALE,   OurScene.DoScaleVertex );

			_SetCheckBox( hWnd, IDC_CHECKNOPOPUP, OurScene.DoSuppressAnimPopups );
			_SetCheckBox( hWnd, IDC_EXPLICIT,     OurScene.DoExplicitSequences );

			_SetCheckBox( hWnd, IDC_DOBAKEANIMS,  OurScene.DoBakeAnims );
			

			if ( OurScene.DoScaleVertex && (OurScene.VertexExportScale != 0.f) ) 
			{
				TCHAR ScaleString[64];						
				sprintf_s(ScaleString,"%8.5f",OurScene.VertexExportScale );
				PrintWindowString(hWnd, IDC_EDITFIXEDSCALE, ScaleString );
			}
			else
			{
				OurScene.VertexExportScale = 1.0f;
			}

			if ( vertframerangestring[0]  ) PrintWindowString(hWnd,IDC_EDITVERTEXRANGE,vertframerangestring);			


			//if ( batchfoldername[0] ) SetWindowText(GetDlgItem(hWnd,IDC_BATCHPATH),batchfoldername);
		
			//ToggleControlEnable(hWnd,IDC_CHECKSKIN1,true);
			//ToggleControlEnable(hWnd,IDC_CHECKSKIN2,true);
			
			// Initialize this panel's custom controls, if any.
			// ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ACOMP,IDC_SPIN_ACOMP,100,0,100);
			break;

		case WM_DESTROY:
			// Release this panel's custom controls, if any.
			// ThisMaxPlugin.DestroySpinner(hWnd,IDC_SPIN_ACOMP);
			break;

		//case WM_LBUTTONDOWN:
		//case WM_LBUTTONUP:
		//case WM_MOUSEMOVE:
		//	ThisMaxPlugin.ip->RollupMouseMessage(hWnd,msg,wParam,lParam); 
		//	break;

		case WM_COMMAND:
		{
			// Needed to have keyboard focus on this window!
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					//DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					//EnableAccelerators();
					break;
			};

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam)) 
			{
				// Window-closing
				case IDOK:
					EndDialog(hWnd, 1);
				break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				// get the values of the toggles
				case IDC_CHECKPERSISTSETTINGS:
				{
					//Read out the value...
					OurScene.CheckPersistSettings = _GetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS);
					PluginReg.SetKeyValue("PERSISTSETTINGS", OurScene.CheckPersistSettings);
				}
				break;

				//
				case IDC_CHECKAUTOTRI:
				{
					OurScene.DoAutoTriangles = _GetCheckBox(hWnd,IDC_CHECKAUTOTRI);
					PluginReg.SetKeyValue("SKELAUTOTRI", OurScene.DoAutoTriangles );
				}
				break;

				case IDC_CHECKPERSISTPATHS:
				{
					//Read out the value...
					OurScene.CheckPersistPaths = _GetCheckBox(hWnd,IDC_CHECKPERSISTPATHS);
					PluginReg.SetKeyValue("PERSISTPATHS", OurScene.CheckPersistPaths);
				}
				break;

				case IDC_CHECK_STRIP_REFERENCE_PREFIX:
					{
						OurScene.bCheckStripRef = _GetCheckBox( hWnd, IDC_CHECK_STRIP_REFERENCE_PREFIX );
						PluginReg.SetKeyValue( "STRIPREFERENCEPREFIX", OurScene.bCheckStripRef );
					}
					break;

				// Switch to indicate QuickSave is to save/append/overwrite sequences immediately to disk (off by default)...			
				case IDC_QUICKSAVEDISK:
				{
					OurScene.QuickSaveDisk = _GetCheckBox( hWnd, IDC_QUICKSAVEDISK );
					PluginReg.SetKeyValue("QUICKSAVEDISK",OurScene.QuickSaveDisk );
				}
				break;

				case IDC_FIXROOT:
				{
					OurScene.DoFixRoot = _GetCheckBox(hWnd,IDC_FIXROOT);
																		
					if ( !OurScene.DoFixRoot )
					{
						//_SetCheckBox( hWnd,IDC_LOCKROOTX, false );
						//_EnableCheckBox( hWnd,IDC_LOCKROOTX,0 );			// ? ToggleControlEnable(hWnd,IDC_LOCKROOTX,OurScene.DoFixRoot); ? see also MSDN  SDK info, Buttons/ using buttons etc
						//OurScene.DoLockRoot = false;
						//PluginReg.SetKeyValue("DOLOCKROOT",OurScene.DoLockRoot );
					}
					else 
					{
						//_EnableCheckBox( hWnd,IDC_LOCKROOTX,1 );
						//_SetCheckBox( hWnd,IDC_LOCKROOTX, OurScene.DoLockRoot );
					}					

					PluginReg.SetKeyValue("DOFIXROOT",OurScene.DoFixRoot );					
				}
				break;

				case IDC_CHECKHIERBONES:
				{
					OurScene.DoHierBones = _GetCheckBox( hWnd, IDC_CHECKHIERBONES );
					PluginReg.SetKeyValue("DOHIERBONES",OurScene.DoHierBones );
				}
				break;

				/*
				case IDC_POSEZERO:
				{
					OurScene.DoPoseZero = _GetCheckBox(hWnd,IDC_POSEZERO);
					//ToggleControlEnable(hWnd,IDC_LOCKROOTX,OurScene.DoPOSEZERO);
					if (!OurScene.DoPoseZero)
					{
						_SetCheckBox(hWnd,IDC_LOCKROOTX,false);
						_EnableCheckBox( hWnd,IDC_LOCKROOTX, 0);			
					}
					else 
					{
						_EnableCheckBox( hWnd,IDC_LOCKROOTX, 1 );			
					}
					// HWND hDlg = GetDlgItem(hWnd,control);
					// EnableWindow(hDlg,FALSE);					
					// see also MSDN  SDK info, Buttons/ using buttons etc

					PluginReg.SetKeyValue( "DOPOSEZERO",OurScene.DoPoseZero );
				}
				break;
				*/

				case IDC_LOCKROOTX:
				{
					//Read out the value...
					OurScene.DoLockRoot =_GetCheckBox(hWnd,IDC_LOCKROOTX);
					PluginReg.SetKeyValue("DOLOCKROOT",OurScene.DoLockRoot );
				}
				break;

				case IDC_CHECKNOLOG:
				{
					OurScene.DoLog = !_GetCheckBox(hWnd,IDC_CHECKNOLOG);	
					PluginReg.SetKeyValue("DOLOG",OurScene.DoLog);
				}
				break;

				case IDC_CHECKDOUNSMOOTH:
				{
					OurScene.DoUnSmooth = _GetCheckBox(hWnd,IDC_CHECKDOUNSMOOTH);					
					PluginReg.SetKeyValue("DOUNSMOOTH",OurScene.DoUnSmooth);
				}
				break;

				case IDC_CHECKDOTANGENTS:
				{
					OurScene.DoTangents = _GetCheckBox(hWnd,IDC_CHECKDOTANGENTS);					
					PluginReg.SetKeyValue("DOTANGENTS",OurScene.DoTangents);
				}
				break;


				case IDC_CHECKFORCERATE:
				{
					OurScene.DoForceRate =_GetCheckBox(hWnd,IDC_CHECKFORCERATE);
					PluginReg.SetKeyValue("DOFORCERATE",OurScene.DoForceRate );
					if (!OurScene.DoForceRate) _SetCheckBox(hWnd,IDC_CHECKFORCERATE,false);
				}
				break;

				case IDC_PERSISTRATE:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							char RateString[MAX_PATH];
							GetWindowText( GetDlgItem(hWnd,IDC_PERSISTRATE),RateString,100 );
							OurScene.PersistentRate = atof(RateString);
							PluginReg.SetKeyValue("PERSISTENTRATE", OurScene.PersistentRate );
						}
						break;
					};
				}
				break;
				
				case IDC_CHECKSKINALLT: // all textured geometry skin checkbox
				{
					OurScene.DoTexGeom =_GetCheckBox(hWnd,IDC_CHECKSKINALLT);
					PluginReg.SetKeyValue("DOTEXGEOM",OurScene.DoTexGeom );
					if (!OurScene.DoTexGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLT,false);
				}
				break;

				case IDC_DOBAKEANIMS: // bake animations checkbox
					{
						OurScene.DoBakeAnims =_GetCheckBox(hWnd,IDC_DOBAKEANIMS);
						PluginReg.SetKeyValue("DOBAKEANIMS",OurScene.DoBakeAnims );
						if (!OurScene.DoBakeAnims) _SetCheckBox(hWnd,IDC_DOBAKEANIMS,false);
					}
					break;

				case IDC_CHECKTEXPCX: // all textured geometry skin checkbox
				{
					OurScene.DoTexPCX =_GetCheckBox(hWnd,IDC_CHECKTEXPCX);
					PluginReg.SetKeyValue("DOPCX",OurScene.DoTexPCX); // update registry					
				}
				break;

				case IDC_VERTEXOUT: // Classic vertex animation export
				{
					OurScene.DoVertexOut =_GetCheckBox(hWnd,IDC_VERTEXOUT);					
				}

				case IDC_CHECKSKINALLS: // all selected meshes skin checkbox
				{
					OurScene.DoSelectedGeom =_GetCheckBox(hWnd,IDC_CHECKSKINALLS);
					PluginReg.SetKeyValue("DOSELGEOM",OurScene.DoSelectedGeom);					
					if (!OurScene.DoSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLS,false);
				}
				break;

				case IDC_CHECKSKINALLP: // all selected physique meshes checkbox
				{
					OurScene.DoPhysGeom =_GetCheckBox(hWnd,IDC_CHECKSKINALLP);
					PluginReg.SetKeyValue("DOPHYSGEOM", OurScene.DoPhysGeom );
					if (!OurScene.DoPhysGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLP,false);
				}
				break;

				case IDC_CHECKSKINNOS: // all none-selected meshes skin checkbox..
				{
					OurScene.DoSkipSelectedGeom = _GetCheckBox(hWnd,IDC_CHECKSKINNOS);
					if (!OurScene.DoSkipSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINNOS,false);
				}
				break;								

				case IDC_CHECKCULLDUMMIES: // all selected physique meshes checkbox
				{
					OurScene.DoCullDummies =_GetCheckBox(hWnd,IDC_CHECKCULLDUMMIES);
					PluginReg.SetKeyValue("DOCULLDUMMIES",OurScene.DoCullDummies );
					//ToggleControlEnable(hWnd,IDC_CHECKCULLDUMMIES,OurScene.DoCullDummies);
					if (!OurScene.DoCullDummies) _SetCheckBox(hWnd,IDC_CHECKCULLDUMMIES,false);					
				}
				break;

				case IDC_UNDERSCORETOSPACE:
				{
					OurScene.DoReplaceUnderscores =_GetCheckBox(hWnd,IDC_UNDERSCORETOSPACE);
					PluginReg.SetKeyValue("DOREPLACEUNDERSCORES",OurScene.DoReplaceUnderscores );
					if (!OurScene.DoReplaceUnderscores) _SetCheckBox(hWnd,IDC_UNDERSCORETOSPACE,false);					
				}
				break;
				//PCF BEGIN
				case IDC_DOSECONDUVSETSKINNED:
					{
						OurScene.DoSecondUVSetSkinned =_GetCheckBox(hWnd,IDC_DOSECONDUVSETSKINNED);
						PluginReg.SetKeyValue("DOSECONDUVSETSKINNED",OurScene.DoSecondUVSetSkinned );
						if (!OurScene.DoSecondUVSetSkinned) _SetCheckBox(hWnd,IDC_DOSECONDUVSETSKINNED,false);					
					}
				break;
				//PCF END

				case IDC_EXPLICIT:
				{
					OurScene.DoExplicitSequences =_GetCheckBox(hWnd,IDC_EXPLICIT);
					PluginReg.SetKeyValue("DOEXPSEQ",OurScene.DoExplicitSequences);
				}
				break;

				case IDC_SAVESCRIPT: // save the script
				{
					if( 0 ) // TempActor.OutAnims.Num() == 0 ) 
					{
						PopupBox("Warning: no animations present in output package.\n Aborting template generation.");
					}
					else
					{
						OurScene.WriteScriptFile( &TempActor, classname, basename, to_skinfile, to_animfile );
						PopupBox(" Script template file written: %s",classname);
					}
				}
				break;

				case IDC_BATCHPATH:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText( GetDlgItem(hWnd,IDC_EDITCLASS),batchfoldername,300 );
						}
						break;
					};
				}
				break;		

				case IDC_CHECKNOPOPUP:
				{
					OurScene.DoSuppressAnimPopups = _GetCheckBox(hWnd,IDC_CHECKNOPOPUP);					
					PluginReg.SetKeyValue("DOSUPPRESSANIMPOPUPS",OurScene.DoSuppressAnimPopups);
				}
				break;


				case IDC_BATCHSRC: // Browse for batch source folder.
				{
					// TCHAR dir[MAX_PATH];
					// TCHAR desc[MAX_PATH];
					//_tcscpy_s(desc, ("Batch Source Folder"));
					// u->ip->ChooseDirectory(u->ip->GetMAXHWnd(),("Choose Folder"), dir, desc);					
					//_tcscpy_s(batchfoldername,dir); 
					// SetWindowText(GetDlgItem(hWnd,IDC_BATCHPATH),batchfoldername);					
				}
				break;

				case IDC_BATCHMAX: // process the batch files.
				{	
					// Get all max files from folder '' and digest them.
					PopupBox(" Digested %i animation files, total of %i frames",0,0);
				}
				break;

				case IDC_EDITCLASS:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText( GetDlgItem(hWnd,IDC_EDITCLASS),classname,100 );
							PluginReg.SetKeyString("CLASSNAME", classname );
						}
						break;
					};
				}
				break;		

				case IDC_EDITBASE:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_EDITBASE),basename,100);
							PluginReg.SetKeyString("BASENAME", basename );
						}
						break;
					};
				}
				break;		

			};
			break;
		
		default:
		return FALSE;
		}
	}
	return TRUE;
}       



//
// Vertex-animation exporter (mostly package-independent.)
//

static INT_PTR CALLBACK VertExpDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{	
	switch (msg) 
	{
		case WM_INITDIALOG:
		{
			_SetCheckBox( hWnd, IDC_CHECKAPPENDVERTEX, OurScene.DoAppendVertex );
			_SetCheckBox( hWnd, IDC_CHECKFIXEDSCALE,   OurScene.DoScaleVertex );			

			if ( OurScene.DoScaleVertex && (OurScene.VertexExportScale != 0.f) ) 
			{
				TCHAR ScaleString[64];						
				sprintf_s(ScaleString,"%8.5f",OurScene.VertexExportScale );
				PrintWindowString(hWnd, IDC_EDITFIXEDSCALE, ScaleString );
			}
			else
			{
				OurScene.VertexExportScale = 1.0f;
			}

			if ( vertframerangestring[0]  ) PrintWindowString(hWnd,IDC_EDITVERTEXRANGE,vertframerangestring);			
		}
		break;

		case WM_COMMAND:
		{
			// Needed to have keyboard focus on this window!
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					//DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					//EnableAccelerators();
					break;
			};

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam)) 
			{
				// Window-closing
				case IDOK:
					EndDialog(hWnd, 1);
				break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				case IDC_EDITVERTEXRANGE:
				{
					switch( HIWORD(wParam) )
					{
						// Whenenver focus is lost, update the skin file name.
						case EN_KILLFOCUS:
						{							
							GetWindowText(GetDlgItem(hWnd,IDC_EDITVERTEXRANGE),vertframerangestring,500);
						}
						break;
					};
				}
				break;		

				case IDC_EXPORTVERTEX: // export vertex-animated data (raw discretized data, doesn't do auto-scaling )
				{
					OurScene.SurveyScene();
					OurScene.GetSceneInfo();	
					
					OurScene.EvaluateSkeleton(1);
					OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

					INT WrittenFrameCount;

					if( WrittenFrameCount = OurScene.WriteVertexAnims( &TempActor, to_animfile, vertframerangestring ) ) // Calls DigestSkin
					{
						if( !OurScene.DoSuppressAnimPopups )
						{
							if( ! OurScene.DoAppendVertex )
								PopupBox(" Vertex animation export successful. Written %i frames. ",WrittenFrameCount);
							else
								PopupBox(" Vertex animation export successful. Appended %i frames. ",WrittenFrameCount);
						}
					}
					else
					{
						if( !OurScene.DoSuppressAnimPopups )
							PopupBox(" Error encountered writing vertex animation data.");
					}
				}
				break;

				case IDC_CHECKAPPENDVERTEX: 
				{
					OurScene.DoAppendVertex = _GetCheckBox(hWnd,IDC_CHECKAPPENDVERTEX);
					PluginReg.SetKeyValue("DOAPPENDVERTEX", OurScene.DoAppendVertex);
				}
				break;

				case IDC_CHECKFIXEDSCALE: 
				{
					OurScene.DoScaleVertex = _GetCheckBox(hWnd,IDC_CHECKFIXEDSCALE);
					PluginReg.SetKeyValue("DOSCALEVERTEX", OurScene.DoScaleVertex);
				}
				break;


			}//switch (LOWORD(wParam)) 

			default:
			return FALSE;
		}
		break;
	}; //switch (msg) 

	return TRUE;
}


//
// Old (Unrealed2) .T3D level brush exporter.
//
//
// Brush export dialog procedure - can write brushes to separarte destination folder.
//
// BRUSH export support
char materialnames[8][MAX_PATH];
char to_brushfile[MAX_PATH];
char to_brushpath[MAX_PATH];

static INT MatIDC[16];
static INT DoSmoothBrush;

void UpdateSlotNames(HWND hWnd)
{
	for(INT m=0; m<8; m++)
	{
		if ( materialnames[m][0] ) PrintWindowString( hWnd, MatIDC[m], materialnames[m] );
	}   
}

INT_PTR CALLBACK BrushPanelDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
		case WM_INITDIALOG:

			MatIDC[0] = IDC_EDITTEX0;
			MatIDC[1] = IDC_EDITTEX1;
			MatIDC[2] = IDC_EDITTEX2;
			MatIDC[3] = IDC_EDITTEX3;
			MatIDC[4] = IDC_EDITTEX4;
			MatIDC[5] = IDC_EDITTEX5;
			MatIDC[6] = IDC_EDITTEX6;
			MatIDC[7] = IDC_EDITTEX7;

			LogPath[0] = 0; // disable logging.

			_SetCheckBox( hWnd, IDC_CHECKSMOOTH, DoSmoothBrush );
			_SetCheckBox( hWnd, IDC_CHECKSINGLE, OurScene.DoSingleTex );

			// Initialize all non-empty edit windows.
			if ( to_brushpath[0] ) SetWindowText(GetDlgItem(hWnd,IDC_BRUSHPATH),to_brushpath);
			if ( to_brushfile[0] ) SetWindowText(GetDlgItem(hWnd,IDC_BRUSHFILENAME),to_brushfile);			

			// See if there are materials from the mesh to write back to the window..
			{for(INT m=0; m < TempActor.SkinData.Materials.Num(); m++)
			{
				// Override:
				if(! materialnames[m][0] )
				{
					strcpy_s( materialnames[m], TempActor.SkinData.Materials[m].MaterialName );
				}
			}}

			{for(INT m=0; m<8; m++)
			{
				if ( materialnames[m][0] ) PrintWindowString( hWnd, MatIDC[m], materialnames[m] );
			}}
			break;

	
		case WM_COMMAND:

			switch (LOWORD(wParam)) 
			{
				case IDOK:
					EndDialog(hWnd, 1);
					break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				case IDC_READMAT:  // Read materials from the skin...
				{
					// Digest the skin & read materials...

					// Clear scene
					OurScene.Cleanup();
					TempActor.Cleanup();


					// Digest everything...
					OurScene.SurveyScene();
					OurScene.GetSceneInfo();


					// Skeleton & hierarchy unneeded for brushes.
					OurScene.EvaluateSkeleton(0);
					OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

					OurScene.DigestBrush( &TempActor );

					// See if there are materials from the mesh to write back to the window..
					{for(INT m=0; m < TempActor.SkinData.Materials.Num(); m++)
					{
						// Override:
						if( 1) // ! materialnames[m][0] )
						{
							strcpy_s( materialnames[m], TempActor.SkinData.Materials[m].MaterialName );
						}
					}}

					UpdateSlotNames(hWnd);
				}

				case IDC_CHECKSMOOTH: // all textured geometry skin checkbox
				{
					DoSmoothBrush =_GetCheckBox(hWnd,IDC_CHECKSMOOTH);
				}
				break;

				case IDC_CHECKSINGLE: // all textured geometry skin checkbox
				{
					OurScene.DoSingleTex =_GetCheckBox(hWnd,IDC_CHECKSINGLE);
				}
				break;

				// Retrieve material name updates.
				case IDC_EDITTEX0:
				case IDC_EDITTEX1:
				case IDC_EDITTEX2:
				case IDC_EDITTEX3:
				case IDC_EDITTEX4:
				case IDC_EDITTEX5:
				case IDC_EDITTEX6:
				case IDC_EDITTEX7:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							INT MatIdx = -1;
							for(INT m=0;m<8; m++)
							{
								if(MatIDC[m] == LOWORD(wParam))
									MatIdx = m;
							}
							if( MatIdx > -1)
							{
								GetWindowText(GetDlgItem(hWnd,MatIDC[MatIdx]),materialnames[MatIdx],300);
							}
						}
					}
					//UpdateSlotNames(hWnd);
				}
			

				case IDC_BRUSHPATH:
					{
						switch( HIWORD(wParam) )
						{
							case EN_KILLFOCUS:
							{
								GetWindowText(GetDlgItem(hWnd,IDC_BRUSHPATH),to_brushpath,300);										
								//_tcscpy_s(LogPath,to_brushpath);
							}
							break;
						};
					}
					break;

				case IDC_BRUSHFILENAME:
					{
						switch( HIWORD(wParam) )
						{
							case EN_KILLFOCUS:
							{
								GetWindowText(GetDlgItem(hWnd,IDC_BRUSHFILENAME),to_brushfile,300);
							}
							break;
						};
					}
					break;


				// Browse for a destination directory.
				case IDC_BRUSHBROWSE:
					{					
						char  dir[MAX_PATH];
						if( GetFolderName(hWnd, dir, _countof(dir)) )
						{
							_tcscpy_s(to_brushpath,dir); 
							//_tcscpy_s(LogPath,dir);					
						}
						SetWindowText(GetDlgItem(hWnd,IDC_BRUSHPATH),to_brushpath);															
					}	
					break;

				// 
				case IDC_EXPORTBRUSH:
					{
						//
						// Special digest-brush command - 
						// Switches
						// - selectedonly: only selected (textured) objects
						// - otherwise, all geometry nodes will be exported.
						// 

						// Clear scene
						OurScene.Cleanup();
						TempActor.Cleanup();

						// Scene stats. #debug: surveying should be done only once ?
						OurScene.SurveyScene();
						OurScene.GetSceneInfo();

						// Skeleton & hierarchy unneeded for brushes....				
						OurScene.EvaluateSkeleton(0);
						DebugBox("End EvalSK");
					#if 1
						OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor
						DebugBox("End DigestSk");
					#endif

						OurScene.DigestBrush( &TempActor );
						DebugBox("End DigestBrush");

						PopupBox("Processed brush; points:[%i]",TempActor.SkinData.Points.Num());

						//
						// See if there are materials to override
						//
						for(INT m=0; m < TempActor.SkinData.Materials.Num(); m++)
						{
							// Override:
							if( materialnames[m][0] )
							{								
								TempActor.SkinData.Materials[m].SetName(materialnames[m]);
							}
							else // copy it back...
							{
								strcpy_s( materialnames[m], TempActor.SkinData.Materials[m].MaterialName );
							}
						}
						
						if( TempActor.SkinData.Points.Num() == 0 )
							WarningBox(" Warning: no valid geometry digested (missing mapping or selection?)");
						else
						{
							TCHAR to_ext[32];
							_tcscpy_s(to_ext,("T3D"));
							sprintf_s(DestPath,"%s\\%s.%s", (char*)to_brushpath, (char*)to_brushfile, to_ext);
							FastFileClass OutFile;

							if ( OutFile.CreateNewFile(DestPath) != 0) // Error !
								ErrorBox("ExportBrush File creation error. ");
					
							TempActor.WriteBrush( OutFile, DoSmoothBrush, OurScene.DoSingleTex ); // Save brush.
						
							 // Close.
							OutFile.CloseFlush();

							INT MatCount =  OurScene.DoSingleTex ? 1 : TempActor.SkinData.Materials.Num();

							if( OutFile.GetError()) ErrorBox("Brush Save Error");
								else
							PopupBox(" Brush  %s\\%s.%s written; %i material(s).",to_brushpath,to_brushfile,to_ext, MatCount);		

							// Log materials and stats.
							// if( OurScene.DoLog ) OurScene.LogSkinInfo( &TempActor, to_brushfile );

							UpdateSlotNames(hWnd);
						}
					}
					break;
				}
				break;

		default:
		return FALSE;
	}
	return TRUE;
}       

void ShowPanelOne( HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_PANEL), hWnd, PanelOneDlgProc, 0 );
}

void ShowPanelTwo( HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_AUXPANEL), hWnd, PanelTwoDlgProc, 0 );
}

void ShowActorManager( HWND hWnd )
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_ACTORMANAGER), hWnd, ActorManagerDlgProc, 0 );	
}

void ShowStaticMeshPanel( HWND hWnd )
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_STATICMESH), hWnd, StaticMeshDlgProc, 0 );	
}

void ShowVertExporter( HWND hWnd )
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_VERTMESH), hWnd, VertExpDlgProc, 0 );	
}
// PCF BEGIN
// smoothing groups from traingulatedMesh
void MeshProcessor::createNewSmoothingGroupsFromTriangulatedMesh( triangulatedMesh& mesh )
{
	// Create our edge lookup table and initialize all entries to NULL.

	INT edgeTableSize = mesh.numVerts;
	INT numPolygons = ( INT )mesh.triangles.size();
	if( edgeTableSize < 1)
		return; //SafeGuard.

	VertEdgePools.Empty();
	VertEdgePools.AddZeroed( edgeTableSize );  

	// Add entries, for each edge, to the lookup table
	for (int i =0; i <  mesh.edges->_edges.size(); i++ ) // Iterate over edges.
	{

		edgesPool::edge * e =  mesh.edges->_edges[i];
		bool smooth =e->smooth;  // Smoothness retrieval for edge.
		addEdgeInfo( e->v1, e->v2, smooth );  // Adds info for edge running from vertex elt.index(0) to elt.index(1) into VertEdgePools.
		MayaEdge* elem = findEdgeInfo( e->v1, e->v2 );
		if ( elem )  // Null if no edges found for vertices a and b. 
		{

				elem->polyIds[0] = e->f1; // Add poly index to poly table for this edge.
				elem->polyIds[1] = e->f2; // If first slot already filled, fill the second face.  Assumes each edge only has 2 faces max.
                                  
		}
	}
	// Fill FacePools table: for each face, a pool of smoothly touching faces and a pool of nonsmoothly touching ones.	

	FacePools.AddZeroed( numPolygons );
	for (int p =0; p <  mesh.triangles.size(); p++ ) // Iterate over edges.
	{

		triangulatedMesh::Triangle * t =  mesh.triangles[p];
		for ( int vid=0; vid<3;vid++ ) 
		{
			int a = t->Indices[vid].pointIndex;
			int b = t->Indices[ vid==(2) ? 0 : vid+1 ].pointIndex;

			MayaEdge* Edge = findEdgeInfo( a, b );
			if ( Edge )
			{
				INT FaceA = Edge->polyIds[0];
				INT FaceB = Edge->polyIds[1];
				INT TouchingFaceIdx = -1;

				if( FaceA == p )  
					TouchingFaceIdx = FaceB;
				else 
					if( FaceB == p ) 
						TouchingFaceIdx = FaceA;

				if( TouchingFaceIdx >= 0)
				{
					if( Edge->smooth )
					{
						FacePools[p].SmoothFaces.AddUniqueItem(TouchingFaceIdx);							
					}
					else
					{
						FacePools[p].HardFaces.AddUniqueItem(TouchingFaceIdx);					
					}
				}
			}
		}
	}



	PolyProcessFlags.Empty();
	PolyProcessFlags.AddZeroed( numPolygons );
	{for ( INT i=0; i< numPolygons; i++ ) 
	{ 
		PolyProcessFlags[i] = NO_SMOOTHING_GROUP; 
	}}    

	//
	// Convert all from edges into smoothing groups.  Essentially flood fill it. but als check each poly if it's been processed yet.
	//
	FaceSmoothingGroups.AddZeroed( numPolygons );
	INT SmoothParts = 0;
	CurrentGroup = 0;
	{for ( INT pid=0; pid<numPolygons; pid++ ) 
	{
		if( PolyProcessFlags[pid] == NO_SMOOTHING_GROUP )
		{			
			fillSmoothFaces( pid );
			SmoothParts++;
		}
	}}

	//
	// Fill GroupTouchPools: for each group, this notes which other groups touch it or overlap it in any way.
	//
	GroupTouchPools.AddZeroed( CurrentGroup ); 
	for( INT f=0; f< numPolygons; f++)
	{
		TArray<INT> TempTouchPool;

		// This face's own groups..
		{for( INT i=0; i< FaceSmoothingGroups[f].Groups.Num(); i++)
		{
			TempTouchPool.AddUniqueItem( FaceSmoothingGroups[f].Groups[i] );
		}}

		// And those of the neighbours, whether touching or not..
		{for( INT n=0; n< FacePools[f].HardFaces.Num(); n++)
		{
			INT FaceIdx = FacePools[f].HardFaces[n];
			for( INT i=0; i< FaceSmoothingGroups[ FaceIdx].Groups.Num(); i++)
			{
				TempTouchPool.AddUniqueItem( FaceSmoothingGroups[FaceIdx].Groups[i] );
			}
		}}
		{for( INT n=0; n< FacePools[f].SmoothFaces.Num(); n++)
		{
			INT FaceIdx = FacePools[f].SmoothFaces[n];
			for( INT i=0; i< FaceSmoothingGroups[ FaceIdx].Groups.Num(); i++)
			{
				TempTouchPool.AddUniqueItem( FaceSmoothingGroups[FaceIdx].Groups[i] );
			}
		}}

		// Then for each element add all other elements uniquely to its GroupTouchPool (includes its own..)
		for( INT g=0; g< TempTouchPool.Num(); g++)
		{
			for( INT t=0; t< TempTouchPool.Num(); t++ )
			{
				GroupTouchPools[ TempTouchPool[g] ].Groups.AddUniqueItem( TempTouchPool[t] );
			}
		}
	}


	// Distribute final smoothing groups by carefully checking against already assigned groups in
	// each group's touchpools. Note faces can have many multiple touching groups so the
	// 4-colour theorem doesn't apply, but it should at least help us limit the smoothing groups to 32.

	INT numRawSmoothingGroups = GroupTouchPools.Num();
	INT HardClashes = 0;
	INT MaxGroup = 0;
	for( INT gInit =0; gInit <GroupTouchPools.Num(); gInit++)
	{
		GroupTouchPools[gInit].FinalGroup = -1;
	}

	for( INT g=0; g<GroupTouchPools.Num(); g++ )
	{		
		INT FinalGroupCycle = 0;
		for( INT SGTries=0; SGTries<32; SGTries++ )
		{
			INT Clashes = 0;
			// Check all touch groups. If none has been assigned this FinalGroupCycle number, we're done otherwise, try another one.
			for( INT t=0; t< GroupTouchPools[g].Groups.Num(); t++ )
			{
				Clashes += ( GroupTouchPools[ GroupTouchPools[g].Groups[t] ].FinalGroup == FinalGroupCycle ) ? 1: 0;
			}
			if( Clashes == 0 )
			{
				// Done for this smoothing group.
				GroupTouchPools[g].FinalGroup = FinalGroupCycle;
				break; 
			}			
			FinalGroupCycle = (FinalGroupCycle+1)%32;
		}
		if( GroupTouchPools[g].FinalGroup == -1)
		{
			HardClashes++;			
			GroupTouchPools[g].FinalGroup = 0; // Go to default group.
		}		
		MaxGroup = max( MaxGroup, GroupTouchPools[g].FinalGroup );
		if( (g!=0) && (( g & 65535 )==0) )
		{
			MayaLog(" Busy merging smoothing groups... %i ",g);
		}
	}

	if( HardClashes > 0) 
	{
		MayaLog(" Warning: [%i] smoothing group reassignment errors.",HardClashes );
	}


	// Resulting face smoothing groups remapping to final groups.
	{for( INT p=0; p< numPolygons; p++)
	{
		// This face's own groups..
		for( INT i=0; i< FaceSmoothingGroups[p].Groups.Num(); i++)
		{			
			//FaceSmoothingGroups[p].Groups[i] = RemapperArray[ FaceSmoothingGroups[p].Groups[i]  ];
			FaceSmoothingGroups[p].Groups[i] = GroupTouchPools[ FaceSmoothingGroups[p].Groups[i] ].FinalGroup;
		}
	}}

	// Finally, let's copy them into PolySmoothingGroups.
	PolySmoothingGroups.Empty();
	PolySmoothingGroups.AddExactZeroed( FaceSmoothingGroups.Num() );
	{for( INT i=0; i< PolySmoothingGroups.Num(); i++)
	{
		// OR-in all the smoothing bits.
		for( INT s=0; s< FaceSmoothingGroups[i].Groups.Num(); s++)
		{ 
			PolySmoothingGroups[i]  |=  ( 1 << (  FaceSmoothingGroups[i].Groups[s]  ) );
		}
	}}

	//MayaLog(" Faces %i  - Smoothing groups before reduction:  %i   after reduction: %i  ",numPolygons,  numRawSmoothingGroups, MaxGroup );

}
// PCF END


//
// New smoothing group extraction. Rewritten to eliminate hard-to-track-down anomalies inherent in the Maya SDK's mesh processor smoothing group extraction code.
//
void MeshProcessor::createNewSmoothingGroups( MDagPath& mesh )
{
	// Create our edge lookup table and initialize all entries to NULL.
    MFnMesh fnMesh( mesh );
    INT edgeTableSize = fnMesh.numVertices(); 
	INT numPolygons = fnMesh.numPolygons();
	if( edgeTableSize < 1)
		return; //SafeGuard.

	VertEdgePools.Empty();
	VertEdgePools.AddZeroed( edgeTableSize );  
    
	// Add entries, for each edge, to the lookup table
    MItMeshEdge eIt( mesh );
    for ( ; !eIt.isDone(); eIt.next() ) // Iterate over edges.
    {
        bool smooth = eIt.isSmooth();  // Smoothness retrieval for edge.
        addEdgeInfo( eIt.index(0), eIt.index(1), smooth );  // Adds info for edge running from vertex elt.index(0) to elt.index(1) into VertEdgePools.
    }

    MItMeshPolygon pIt( mesh );
    for ( ; !pIt.isDone(); pIt.next() )
    {
        INT pvc = pIt.polygonVertexCount();
        for ( INT v=0; v<pvc; v++ )
        {
			// Circle around polygons assigning the edge IDs  to edgeinfo's 			
            INT a = pIt.vertexIndex( v );
            INT b = pIt.vertexIndex( v==(pvc-1) ? 0 : v+1 ); // wrap

            MayaEdge* elem = findEdgeInfo( a, b );
            if ( elem )  // Null if no edges found for vertices a and b. 
			{
                INT edgeId = pIt.index();
                
                if ( INVALID_ID == elem->polyIds[0] ) 
				{
                    elem->polyIds[0] = edgeId; // Add poly index to poly table for this edge.
                }
                else 
				{
                    elem->polyIds[1] = edgeId; // If first slot already filled, fill the second face.  Assumes each edge only has 2 faces max.
                }                                    
            }
        }
    }

	// Fill FacePools table: for each face, a pool of smoothly touching faces and a pool of nonsmoothly touching ones.	
	FacePools.AddZeroed( numPolygons );
	{for ( INT p=0; p< numPolygons; p++ ) 
	{		
		MIntArray vertexList; 
		fnMesh.getPolygonVertices( p, vertexList );
		int vcount = vertexList.length();		

		// Walk around this polygon. accumulate all smooth and sharp bounding faces..
		for ( int vid=0; vid<vcount;vid++ ) 
		{
			int a = vertexList[ vid ];
			int b = vertexList[ vid==(vcount-1) ? 0 : vid+1 ];
			MayaEdge* Edge = findEdgeInfo( a, b );
			if ( Edge )
			{
				INT FaceA = Edge->polyIds[0];
				INT FaceB = Edge->polyIds[1];
				INT TouchingFaceIdx = -1;

				if( FaceA == p )  
					TouchingFaceIdx = FaceB;
				else 
				if( FaceB == p ) 
					TouchingFaceIdx = FaceA;

				if( TouchingFaceIdx >= 0)
				{
					if( Edge->smooth )
					{
						FacePools[p].SmoothFaces.AddUniqueItem(TouchingFaceIdx);							
					}
					else
					{
						FacePools[p].HardFaces.AddUniqueItem(TouchingFaceIdx);					
					}
				}								
			}
		}
	}}
	
	PolyProcessFlags.Empty();
	PolyProcessFlags.AddZeroed( numPolygons );
	{for ( INT i=0; i< numPolygons; i++ ) 
	{ 
		PolyProcessFlags[i] = NO_SMOOTHING_GROUP; 
	}}    

	//
	// Convert all from edges into smoothing groups.  Essentially flood fill it. but als check each poly if it's been processed yet.
	//
	FaceSmoothingGroups.AddZeroed( numPolygons );
	INT SmoothParts = 0;
	CurrentGroup = 0;
	{for ( INT pid=0; pid<numPolygons; pid++ ) 
	{
		if( PolyProcessFlags[pid] == NO_SMOOTHING_GROUP )
		{			
			fillSmoothFaces( pid );
			SmoothParts++;
		}
	}}

	//
	// Fill GroupTouchPools: for each group, this notes which other groups touch it or overlap it in any way.
	//
	GroupTouchPools.AddZeroed( CurrentGroup ); 
	for( INT f=0; f< numPolygons; f++)
	{
		TArray<INT> TempTouchPool;

		// This face's own groups..
		{for( INT i=0; i< FaceSmoothingGroups[f].Groups.Num(); i++)
		{
			TempTouchPool.AddUniqueItem( FaceSmoothingGroups[f].Groups[i] );
		}}

		// And those of the neighbours, whether touching or not..
		{for( INT n=0; n< FacePools[f].HardFaces.Num(); n++)
		{
			INT FaceIdx = FacePools[f].HardFaces[n];
			for( INT i=0; i< FaceSmoothingGroups[ FaceIdx].Groups.Num(); i++)
			{
				TempTouchPool.AddUniqueItem( FaceSmoothingGroups[FaceIdx].Groups[i] );
			}
		}}
		{for( INT n=0; n< FacePools[f].SmoothFaces.Num(); n++)
		{
			INT FaceIdx = FacePools[f].SmoothFaces[n];
			for( INT i=0; i< FaceSmoothingGroups[ FaceIdx].Groups.Num(); i++)
			{
				TempTouchPool.AddUniqueItem( FaceSmoothingGroups[FaceIdx].Groups[i] );
			}
		}}

		// Then for each element add all other elements uniquely to its GroupTouchPool (includes its own..)
		for( INT g=0; g< TempTouchPool.Num(); g++)
		{
			for( INT t=0; t< TempTouchPool.Num(); t++ )
			{
				GroupTouchPools[ TempTouchPool[g] ].Groups.AddUniqueItem( TempTouchPool[t] );
			}
		}
	}
	

	// Distribute final smoothing groups by carefully checking against already assigned groups in
	// each group's touchpools. Note faces can have many multiple touching groups so the
	// 4-colour theorem doesn't apply, but it should at least help us limit the smoothing groups to 32.

	INT numRawSmoothingGroups = GroupTouchPools.Num();
	INT HardClashes = 0;
	INT MaxGroup = 0;
	for( INT gInit =0; gInit <GroupTouchPools.Num(); gInit++)
	{
		GroupTouchPools[gInit].FinalGroup = -1;
	}

	for( INT g=0; g<GroupTouchPools.Num(); g++ )
	{		
		INT FinalGroupCycle = 0;
		for( INT SGTries=0; SGTries<32; SGTries++ )
		{
			INT Clashes = 0;
			// Check all touch groups. If none has been assigned this FinalGroupCycle number, we're done otherwise, try another one.
			for( INT t=0; t< GroupTouchPools[g].Groups.Num(); t++ )
			{
				Clashes += ( GroupTouchPools[ GroupTouchPools[g].Groups[t] ].FinalGroup == FinalGroupCycle ) ? 1: 0;
			}
			if( Clashes == 0 )
			{
				// Done for this smoothing group.
				GroupTouchPools[g].FinalGroup = FinalGroupCycle;
				break; 
			}			
			FinalGroupCycle = (FinalGroupCycle+1)%32;
		}
		if( GroupTouchPools[g].FinalGroup == -1)
		{
			HardClashes++;			
			GroupTouchPools[g].FinalGroup = 0; // Go to default group.
		}		
		MaxGroup = max( MaxGroup, GroupTouchPools[g].FinalGroup );
		if( (g!=0) && (( g & 65535 )==0) )
		{
			MayaLog(" Busy merging smoothing groups... %i ",g);
		}
	}

	if( HardClashes > 0) 
	{
		MayaLog(" Warning: [%i] smoothing group reassignment errors.",HardClashes );
	}


	// Resulting face smoothing groups remapping to final groups.
	{for( INT p=0; p< numPolygons; p++)
	{
		// This face's own groups..
		for( INT i=0; i< FaceSmoothingGroups[p].Groups.Num(); i++)
		{			
			//FaceSmoothingGroups[p].Groups[i] = RemapperArray[ FaceSmoothingGroups[p].Groups[i]  ];
			FaceSmoothingGroups[p].Groups[i] = GroupTouchPools[ FaceSmoothingGroups[p].Groups[i] ].FinalGroup;
		}
	}}

	// Finally, let's copy them into PolySmoothingGroups.
	PolySmoothingGroups.Empty();
	PolySmoothingGroups.AddExactZeroed( FaceSmoothingGroups.Num() );
	{for( INT i=0; i< PolySmoothingGroups.Num(); i++)
	{
		// OR-in all the smoothing bits.
		for( INT s=0; s< FaceSmoothingGroups[i].Groups.Num(); s++)
		{ 
			PolySmoothingGroups[i]  |=  ( 1 << (  FaceSmoothingGroups[i].Groups[s]  ) );
		}
	}}

	//MayaLog(" Faces %i  - Smoothing groups before reduction:  %i   after reduction: %i  ",numPolygons,  numRawSmoothingGroups, MaxGroup );
	
}



//
// Flood-fills mesh with placeholder smoothing group numbers.
//  - Start from a face and reach all faces smoothly connected with "CurrentGroup"
//
//  
//   ==> General rule: new triangle: get set of all groups bounding it across a sharp boundary; 
//             if CurrentGroup is not in the sharpboundgroups,  just assign CurrentGroup; store surrounding unprocessed faces on stack; pick top of stack to process. 
//             if CurrentGroup is in there -> see if our pre-filled Groups  have none in common with SharpBoundGroups;  
//             if none in common, we can just use the Groups
//             if any in common, we assign a NEW ++LatestGroup, discard Groups, and put the LatestGroup into all our surrounding tris' Groups.
//
//      - This should propagate  edge groups, and keep everything smooth that's smooth.
//
//

// Count common elements between two TArrays of INTs. Assumes the elements are unique (otherwise the result may be too big ).
INT CountCommonElements( TArray<INT>& ArrayOne, TArray<INT>& ArrayTwo )
{
	INT Counter = 0;
	for(INT i=0;i<ArrayOne.Num(); i++)
	{
		if( ArrayTwo.Contains( ArrayOne[i] ) )
			Counter++;
	}
	return Counter;
}

// See if there are any common elements at all.
INT AnyCommonElements( TArray<INT>& ArrayOne, TArray<INT>& ArrayTwo )
{
	INT LastTested = -9999999;
	for(INT i=0;i<ArrayOne.Num(); i++)
	{
		if( ArrayOne[i] != LastTested ) // Avoid unnecessary testing of same item.
		{
			if( ArrayTwo.Contains( ArrayOne[i] ) )
			{
				return 1;
			}
			LastTested = ArrayOne[i];
		}		
	}
	return 0;
}


// PCF BEGIN - not using MFnMesh& fnMesh
void MeshProcessor::fillSmoothFaces( INT polyid  )
//PCF END
{		
	TArray<INT> TodoFaceStack;
	TodoFaceStack.AddItem( polyid ); // Guaranteed to have been a NO_SMOOTHING_GROUP marked face.

	INT LatestGroup = CurrentGroup;
	
	while( TodoFaceStack.Num() )
	{
		// Get top of stack.
		INT StackTopIdx = TodoFaceStack.Num() -1;
		INT ThisFaceIdx = TodoFaceStack[ StackTopIdx ];		
		TodoFaceStack.DelIndex( StackTopIdx );
		
		PolyProcessFlags[ThisFaceIdx] = 1; // Mark as processed.
		
		// CurrentGroup, our first choice, is not in any of the groups assigned to any of the faces of the 'sharp' connecting face set ? Then we'll try to use it.
		INT SharpSetCurrMatches = 0;
		{for( INT t=0; t< FacePools[ ThisFaceIdx].HardFaces.Num(); t++ )
		{
			INT HardFaceAcross = FacePools[ ThisFaceIdx].HardFaces[t] ;
			SharpSetCurrMatches += FaceSmoothingGroups[ HardFaceAcross  ].Groups.Contains( CurrentGroup );				
		}}

		// Do our "pre-filled" groups match any sharply connected groups ? 
		INT SharpSetGroupMatches = 0;
		{for( INT t=0; t< FacePools[ ThisFaceIdx].HardFaces.Num(); t++ )
		{
			INT HardFaceAcross = FacePools[ ThisFaceIdx].HardFaces[t] ;
			//SharpSetGroupMatches += CountCommonElements(  FaceSmoothingGroups[ThisFaceIdx].Groups, FaceSmoothingGroups[HardFaceAcross].Groups );
			SharpSetGroupMatches += AnyCommonElements(  FaceSmoothingGroups[ThisFaceIdx].Groups, FaceSmoothingGroups[HardFaceAcross].Groups );
		}}
		
		UBOOL NewGroup = true;
		INT SplashGroup = CurrentGroup;		
						
		// No conflicts, then we can assign the default 'currentgroup' and move on. 
		if( SharpSetCurrMatches == 0  && SharpSetGroupMatches == 0 )
		{			
			FaceSmoothingGroups[ ThisFaceIdx ].Groups.AddItem( CurrentGroup );
			NewGroup = false;			
		}
		else
		{				
			// If CurrentGroup _does_ match across sharp, but none of our pre-setgroups match AND there are more than 0 - we have a definite candidate already.			
			if( ( SharpSetCurrMatches > 0 )  && (SharpSetGroupMatches == 0) &&  (FaceSmoothingGroups[ThisFaceIdx].Groups.Num() > 0 ) ) 
			{
				NewGroup=false;
				// Plucked  our existing, first pre-set group as a candidate to splash over our environment as we progress - always ensures smoothess with our smooth neighbors.
				SplashGroup = FaceSmoothingGroups[ThisFaceIdx].Groups[0];
			}
		}
		
		if( NewGroup )
		{
			LatestGroup++;
			SplashGroup = LatestGroup;
			FaceSmoothingGroups[ThisFaceIdx].Groups.Empty();  // We don't need any pre-set groups for smoothness, since we'll saturate our environment.
			FaceSmoothingGroups[ThisFaceIdx].Groups.AddItem( SplashGroup ); // Set group in this face.
		}

		// If we added ANY single new group that's not our default group, we splash it all over our _non-processed_ environment, because we need to 
		// smoothly mesh with our default environment,  always.  This way, 'odd' groups progress themselves as necessary. 
		if( SplashGroup != CurrentGroup )
		{
			for( INT t=0; t< FacePools[ ThisFaceIdx ].SmoothFaces.Num(); t++ )
			{
				INT SmoothFaceAcross = FacePools[ ThisFaceIdx ].SmoothFaces[t];			
				// Only do it in case of == LatestGroup - ensures no corruption with anything else around.. - or when come from pre-loaded Groups; only forward to un-processed faces.
				if(  (SplashGroup == LatestGroup) || ( PolyProcessFlags[ SmoothFaceAcross ] < 0 ) ) 
				{
					FaceSmoothingGroups[ SmoothFaceAcross ].Groups.AddUniqueItem( SplashGroup );
				}
			}
		}

		// Now add all unprocessed, unqueued,  smoothly touching ones to our todo stack.
		for( INT t=0; t< FacePools[ ThisFaceIdx ].SmoothFaces.Num(); t++ )
		{
			INT SmoothFaceAcross = FacePools[ ThisFaceIdx ].SmoothFaces[t];
			if( PolyProcessFlags[ SmoothFaceAcross ]  == NO_SMOOTHING_GROUP ) // ONLY unprocessed ones are added, and setting QUEUED  will ensure they're uniquely added.
			{
				TodoFaceStack.AddItem( SmoothFaceAcross );
				PolyProcessFlags[ SmoothFaceAcross ]  = QUEUED_FOR_SMOOTHING; 
			}
		}
	} // While still faces to process..

	// Currentgroup up to Latestgroup encompass all new unique groups assigned in this run for this floodfilled area.

	// When done, make CurrentGroup higher than any we've used.
	CurrentGroup = ++LatestGroup;

}



//
// Adds a new edge info element to the vertex table.
//
void MeshProcessor::addEdgeInfo( int v1, int v2, bool smooth )
{
	MayaEdge NewEdge;
	NewEdge.vertId = v2;
	NewEdge.smooth = smooth;
	NewEdge.polyIds[0] = INVALID_ID;
	NewEdge.polyIds[1] = INVALID_ID;
	// Add it always.
	VertEdgePools[v1].Edges.AddItem( NewEdge );
}

//
// Find an edge connecting vertex 1 and vertex 2.
//
MayaEdge* MeshProcessor::findEdgeInfo( int v1, int v2 )
{
	// Look in VertEdgePools[v1] for v2..
	//   or in VertEdgePools[v2] for v1..
	INT EdgeIndex;

	EdgeIndex = 0;
    while( EdgeIndex < VertEdgePools[v1].Edges.Num() ) 
	{
        if( VertEdgePools[v1].Edges[EdgeIndex].vertId == v2 )
		{
            return &(VertEdgePools[v1].Edges[EdgeIndex]);
        }
        EdgeIndex++;
    }

    EdgeIndex = 0;
	while( EdgeIndex < VertEdgePools[v2].Edges.Num() ) 
	{
        if( VertEdgePools[v2].Edges[EdgeIndex].vertId == v1 )
		{
            return &(VertEdgePools[v2].Edges[EdgeIndex]);
        }
        EdgeIndex++;
    }

    return NULL;
}



//
//  Description:
//      Find the shading node for the given shading group set node.
//
MObject findShader(const MObject& setNode )
{
	MFnDependencyNode fnNode(setNode);
	MPlug shaderPlug = fnNode.findPlug("surfaceShader");
			
	if (!shaderPlug.isNull()) {			
		MPlugArray connectedPlugs;
		bool asSrc = false;
		bool asDst = true;
		shaderPlug.connectedTo( connectedPlugs, asDst, asSrc );

		if (connectedPlugs.length() != 1)
		{
			//cerr << "Error getting shader\n";
		}
		else 
			return connectedPlugs[0].node();
	}			
	
	return MObject::kNullObj;
}





const char* GetNodeName( const MObject& node )
{
	MStatus status;
	
	
	// Retrieve dagnode and dagpath from object..
	//MFnDagNode fnDagNode( node, &status );

	MFnDependencyNode shaderInfo( node, &status);

	static char OutName[1024];
	strcpysafe( OutName, "GetNodeNameUninitialized", 1024 );

	if( status == MS::kFailure )
	{
			return( OutName );
	}	

	return( shaderInfo.name(&status).asChar() );
}


//
// Find full bitmap and path name for the (first) texture associated with this shader (or rather, Shading Group )
//
INT GetTextureNameFromShader( const MObject& shaderNode, char* BitmapFileName, int MaxChars )
{
    // Get the selection and choose the first path on the selection list.
	MStatus status;
	MObject FoundShader = findShader( shaderNode );
	BitmapFileName[0]=0;

	MPlug colorPlug = MFnDependencyNode( FoundShader ).findPlug("color", &status);
	if (status == MS::kFailure)
	{
		//DLog.LogfLn("Color-find-failure for shader [%s]",GetNodeName( FoundShader ));
		return 0;
	}

	MItDependencyGraph dgIt(colorPlug, MFn::kFileTexture,
					   MItDependencyGraph::kUpstream, 
					   MItDependencyGraph::kBreadthFirst,
					   MItDependencyGraph::kNodeLevel, 
					   &status);

	if (status == MS::kFailure)
	{
		//DLog.LogfLn("Failed constructing dependency graph iterator for shadernode [%s]",GetNodeName(  FoundShader ));
		return 0;
	}
		
	dgIt.disablePruningOnFilter();

    // Found any ?
	if (dgIt.isDone())
	{
		//DLog.LogfLn("Failed to find a texture attached to shadernode [%s]",GetNodeName(  FoundShader ));			
		return 0;
	}	  

	MObject textureNode = dgIt.thisNode();
    MPlug filenamePlug = MFnDependencyNode(textureNode).findPlug("fileTextureName");
    MString textureName;
    filenamePlug.getValue(textureName);

	
	// 
	//	pcf:
	//  renaming texture name with suffix "_Mat" to auto load materials in 
	//	this should do code in unrealed - look for materials with texture name + "_Mat" - as is renamed when textures are imported 
	//	with create material flag
	//
	if (OurScene.DoExportTextureSuffix)
	{
	
		int extensionDot = textureName.rindex('.');
		int pathDot= textureName.rindex('/'); 	


		textureName=  textureName.substring(0,extensionDot-1 ) + MString("_Mat") +  textureName.substring(extensionDot ,textureName.length() );
	}




	strcpysafe( BitmapFileName, textureName.asChar(), MaxChars );

	// Convert Maya's forward slashes to backward.
	for( INT t=0; t<(INT)strlen(BitmapFileName); t++ )
	{
		if( BitmapFileName[t]==47 )
			 BitmapFileName[t]=92;
	}

	return ( INT )strlen( BitmapFileName );
}


//
// Conform data *about* to move to the output list to the animation data already there; issue a warning if the
// output !=0 (means reordered/removed/patched some data....
//

INT ConformAnimDataForOutput()
{
	// OurScene
	return 0;
}





MStatus initializePlugin( MObject _obj )						 
{

	MFnPlugin	plugin( _obj, "Epic Games, Inc.", "2.49", "Any"  ); // VERSION
	MStatus		stat;

	// Register all commands
	stat = plugin.registerCommand( "axabout", ActorXTool0::creator ); 
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXAbout");

	stat = plugin.registerCommand( "axmain", ActorXTool1::creator ); 
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXMain");

	stat = plugin.registerCommand( "axoptions", ActorXTool2::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXOptions");

	stat = plugin.registerCommand( "utexportmesh", ActorXTool3::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXDigestAnim");

	stat = plugin.registerCommand( "axanim", ActorXTool4::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXAnimations");

	stat = plugin.registerCommand( "axmesh", ActorXTool5::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXMeshExport");	

	stat = plugin.registerCommand( "axprocesssequence", ActorXTool6::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: AXProcessSequence");		

	stat = plugin.registerCommand( "axwriteposes", ActorXTool7::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: AXWritePoses");	

	stat = plugin.registerCommand( "axwritesequence", ActorXTool8::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: AXWriteSequence");	

	stat = plugin.registerCommand( "axvertex", ActorXTool10::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: axvertex");	

	// AX-Execute for MEL command-line usage.
	stat = plugin.registerCommand( "axexecute", ActorXTool11::creator,ActorXTool11::newSyntax  );
	if (stat != MS::kSuccess)
		stat.perror("registerCommand error: axexecute");

	stat = plugin.registerCommand( "axpskfixup", ActorXTool12::creator );
	if (stat != MS::kSuccess)
		stat.perror("registerCommand error: axpskfixup");

	stat = plugin.registerCommand( "actory", ActorY::creator, ActorY::newSyntax );
	if (stat != MS::kSuccess)
		stat.perror("registerCommand error: ActorY");	

	//#EDN
#if TESTCOMMANDS
	stat = plugin.registerCommand( "axtest1", ActorXTool21::creator );
	if (stat != MS::kSuccess)
		stat.perror("registerCommand error: axtest1");
	stat = plugin.registerCommand( "axtest2", ActorXTool22::creator );
	if (stat != MS::kSuccess)
		stat.perror("registerCommand error: axtest2");
#endif

	// Reset plugin: mainly to initialize registry paths & settings..
	OurScene.DoLog = 1;
	ResetPlugin();

	// Todo - combine all the errors.
	stat = MS::kSuccess;
	return stat;												 
}												


//	 
// Deregister the commands.
//
MStatus uninitializePlugin( MObject _obj )				 
{							

	MFnPlugin	plugin( _obj );									 
	MStatus		stat;					

	stat = plugin.deregisterCommand( "axabout" ); 	 			 
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axmain" ); 	 			 
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axoptions" );	
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "utexportmesh" );
	if (stat != MS::kSuccess)
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axanim" );	
	if (stat != MS::kSuccess)
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axmesh" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");	

	stat = plugin.deregisterCommand( "axprocesssequence" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axwriteposes" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axwritesequence" );
	if (stat != MS::kSuccess)
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axbrush" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axvertex" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axexecute" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "axpskfixup" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

	stat = plugin.deregisterCommand( "actory" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");

#if TESTCOMMANDS //#EDN

	stat = plugin.deregisterCommand( "axtest1" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand axtest1 error");

	stat = plugin.deregisterCommand( "axtest2" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand axtest2 error");

#endif


	// Todo - combine all the errors.
	stat = MS::kSuccess;
	return stat;												 

}
