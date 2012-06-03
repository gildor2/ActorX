/**************************************************************************

	MaxInterface.h - MAX-specific exporter scene interface code.

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
	Created by Erik de Neve

   Lots of fragments snipped from all over the Max SDK sources.

***************************************************************************/

#ifndef __MaxInterface__H
#define __MaxInterface__H


// Misc includes
#include <assert.h>

// MAX specific includes

#pragma warning (push)
#pragma warning (disable : 4002) // "too many parameters for macro".

#include "Max.h"
#include "stdmat.h"
#include "decomp.h"
#include "shape.h"
#include "utilapi.h"  // Utility plugin parent class.
#include "IParamb2.h" // Needed for Max' native skin modifier.
#include "ISkin.h"    // Native skin modifier (since Max 4.2 )
#include "iFnPub.h" // Function publishing (script exposure.)

#pragma warning (pop)


// Character studio 2.x or 3 ??
#define CS32 1  //CS 3.2 (comes with Max 4.2 patch)
#define CS3  0
#define CS2  0

// Local includes
#if CS2
#include "Phyexp.h"  // Max' PHYSIQUE 2.X interface
#endif

#if CS3
#include "Phyexp3.h"  // Max' PHYSIQUE 3.0 interface
#endif

#if CS32
#include "Phyexp32.h"  // Max' PHYSIQUE 3.2 interface
#endif

#include "Vertebrate.h"
#include "Win32IO.h"

// Resources
#include ".\res\resource.h"


// Minimum bone influence below which the physique weight link is ignored.

#define SMALLESTWEIGHT (0.0001f)
#define MAXSKININDEX 64


// Misc: platform independent.
UBOOL SaveAnimSet( TCHAR* DestPath );


// MAX PLUGIN interface code
void ToggleControlEnable( HWND hWnd, int control, BOOL state );

// The unique ClassID
#define ACTORX_CLASS_ID	Class_ID( 0xaadac57c, 0x95526bf7 )

// Forward decl.
TCHAR *GetString(int id);

// Derive utility plugin class from UtilityObj
class MaxPluginClass : public UtilityObj
{
	public:
		IUtil *iu;
		Interface *ip;
		HWND hMainPanel, hAuxPanel, hVertPanel;
		void InitSpinner( HWND hWnd, int ed, int sp, int v, int min, int max );
		void DestroySpinner( HWND hWnd, int sp );
		int  GetSpinnerValue( HWND hWnd, int sp );

		// Inherited Virtual Methods from UtilityObj
		void BeginEditParams(Interface *ip, IUtil *iu);
		void EndEditParams(Interface *ip, IUtil *iu);
		void DeleteThis() {};
		//void ShowAbout(HWND hWnd);
		void ShowActorManager(HWND hWnd);
		// void ShowBoneTree(HWND hWnd);
		void ShowSceneInfo(HWND hWnd);


		// Methods
		MaxPluginClass();
		~MaxPluginClass();
};





























// UNIFORM_MATRIX REMOVES
// NON-UNIFORM SCALING FROM MATRIX

inline Matrix3 Uniform_Matrix(Matrix3 orig_cur_mat)
{
    AffineParts   parts;
    Matrix3       mat;
 	///Remove  scaling  from orig_cur_mat
	//1) Decompose original and get decomposition info
    decomp_affine(orig_cur_mat, &parts);

	//2) construct 3x3 rotation from quaternion parts.q
    parts.q.MakeMatrix(mat);

	//3) construct position row from translation parts.t
    mat.SetRow(3,  parts.t);

   return(mat);
}

// From the Biped sdk documentation ( BipExp.rtf )
inline Matrix3 Get_Relative_Matrix(INode *node, int t)
{
     /* Note: This function removes the non-uniform scaling
     from MAX node transformations. before multiplying the
     current node by  the inverse of its parent. The
     removal  must be done on both nodes before the
     multiplication and Inverse are applied. This is especially
     useful for Biped export (which uses non-uniform scaling on
     its body parts.) */

      INode *p_node  =    node->GetParentNode();

      Matrix3     orig_cur_mat;  // for current and parent
      Matrix3     orig_par_mat;  // original matrices

      Matrix3     cur_mat;       // for current and parent
      Matrix3     par_mat;       // decomposed matrices

      //Get transformation matrices
      orig_cur_mat   =      node->GetNodeTM(t);
      orig_par_mat   =    p_node->GetNodeTM(t);

      //Decompose each matrix
      cur_mat =  Uniform_Matrix(orig_cur_mat);
      par_mat =  Uniform_Matrix(orig_par_mat);

      //then return relative matrix in coordinate space of parent
      return(cur_mat * Inverse( par_mat));
}


// Make Max strings safe.
TCHAR* CleanString(const TCHAR* name);







#endif // _MaxInterface__H
