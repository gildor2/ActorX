// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/*=============================================================================

	UnSkeletal.h: General ActorXporter support functions, misc math classes ripped from Unreal.

	Created by Erik de Neve

=============================================================================*/

// Dirty subset of Unreal's skeletal math routines and type defs.

#ifndef UNSKELMATH_H
#define UNSKELMATH_H

#include <tchar.h>
#include <math.h>

#define  AXNode void
//#debug TODO - eliminate CStr dependecy from Max-specific code ?
//#define  TSTR CStr
/*#ifdef _UNICODE
#define TSTR WStr
#else
#define TSTR CStr
#endif
*/

// UW codebase defines
#define VARARGS     __cdecl					/* Functions with variable arguments */
#define STDCALL		__stdcall				/* Standard calling convention */
#define FORCEINLINE __forceinline			/* Force code to be inline */
#define ZEROARRAY                           /* Zero-length arrays in structs */


/*-----------------------------------------------------------------------------
	Types, 	pragmas, etc snipped from the Unreal Engine 3 codebase
-----------------------------------------------------------------------------*/

#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4127) // Conditional expression is constant
#pragma warning(disable : 4200) // Zero-length array item at end of structure, a VC-specific extension
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable : 4244) // conversion to float, possible loss of data
#pragma warning(disable : 4245) // conversion from 'enum ' to 'unsigned long', signed/unsigned mismatch
#pragma warning(disable : 4291) // typedef-name '' used as synonym for class-name ''
#pragma warning(disable : 4324) // structure was padded due to __declspec(align())
#pragma warning(disable : 4355) // this used in base initializer list
#pragma warning(disable : 4389) // signed/unsigned mismatch
#pragma warning(disable : 4511) // copy constructor could not be generated
#pragma warning(disable : 4512) // assignment operator could not be generated
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#pragma warning(disable : 4699) // creating precompiled header
#pragma warning(disable : 4702) // unreachable code in inline expanded function
#pragma warning(disable : 4710) // inline function not expanded
#pragma warning(disable : 4711) // function selected for autmatic inlining
#pragma warning(disable : 4714) // __forceinline function not expanded
#pragma warning(disable : 4482) // nonstandard extension used: enum 'enum' used in qualified name (having hte enum name helps code readability and should be part of TR1 or TR2)
#pragma warning(disable : 4748)	// /GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function
#pragma warning(disable : 4996)	// unsafe CRT functions


// some of these types are actually typedef'd in windows.h.
// we redeclare them anyway so we will know if/when the basic
// typedefs may change.

// Unsigned base types.
typedef unsigned __int8		BYTE;		// 8-bit  unsigned.
typedef unsigned __int16	WORD;		// 16-bit unsigned.
typedef unsigned __int32	UINT;		// 32-bit unsigned.
typedef unsigned long		DWORD;		// defined in windows.h

typedef unsigned __int64	QWORD;		// 64-bit unsigned.

// Signed base types.
typedef	signed __int8		SBYTE;		// 8-bit  signed.
typedef signed __int16		SWORD;		// 16-bit signed.
typedef signed __int32 		INT;		// 32-bit signed.
typedef long				LONG;		// defined in windows.h

typedef signed __int64		SQWORD;		// 64-bit signed.

// Character types.
typedef char				ANSICHAR;	// An ANSI character. normally a signed type.
typedef wchar_t				UNICHAR;	// A unicode character. normally a signed type.
typedef wchar_t				WCHAR;		// defined in windows.h

// Other base types.
typedef UINT				UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float				FLOAT;		// 32-bit IEEE floating point.
typedef double				DOUBLE;		// 64-bit IEEE double.
// SIZE_T is defined in windows.h based on WIN64 or not.
// Plus, it uses ULONG instead of size_t, so we can't use size_t.
#ifdef _WIN64
typedef SQWORD				PTRINT;		// Integer large enough to hold a pointer.
#else
typedef INT					PTRINT;		// Integer large enough to hold a pointer.
#endif

// Bitfield type.
typedef unsigned long       BITFIELD;	// For bitfields.


// Math constants.
#define INV_PI			(0.31830988618)
#define HALF_PI			(1.57079632679)

// Magic numbers for numerical precision.
#define DELTA			(0.00001)
#define SLERP_DELTA		(0.0001)   // Todo: test if it can be bigger (=faster code).

// Unreal's Constants.
// #define PI 					(3.1415926535897932)
#define SMALL_NUMBER		(1.e-8)
#define KINDA_SMALL_NUMBER	(1.e-4)



#define MAXPATHCHARS (255)
#define MAXINPUTCHARS (600)

// String support.

UBOOL FindSubString(const TCHAR* CheckString, const TCHAR* Fragment, INT& Pos);
UBOOL CheckSubString(const TCHAR* CheckString, const TCHAR* Fragment);
INT FindValueTag(const TCHAR* CheckString, const TCHAR* Fragment, INT DigitNum);
TCHAR* CleanString(const TCHAR* name);
UBOOL RemoveExtString(const TCHAR* CheckString, const TCHAR* Fragment, TCHAR* OutString);
UBOOL CheckStringsEqual( const TCHAR* FirstString, const TCHAR* SecondString );


// Forward declarations.
class  FVector;
class  FPlane;
class  FCoords;
class  FMatrix;
class  FQuat;
class  FAngAxis;



// Fixed point quaternion.
//
// Floating point 4-element vector, for use with KNI 4x[4x3] matrix
// Warning: not fully or properly implemented all the FVector equivalent methods yet.
//

// Floating point vector.
//
class FVector
{
public:
	// Variables.
	union
	{
		struct {FLOAT X,Y,Z;};
		struct {FLOAT R,G,B;};
	};

	// Constructors.
	FVector()
	{}
	FVector( FLOAT InX, FLOAT InY, FLOAT InZ )
	:	X(InX), Y(InY), Z(InZ)
	{}

	// Binary math operators.
	FVector operator^( const FVector& V ) const
	{
		return FVector
		(
			Y * V.Z - Z * V.Y,
			Z * V.X - X * V.Z,
			X * V.Y - Y * V.X
		);
	}
	FLOAT operator|( const FVector& V ) const
	{
		return X*V.X + Y*V.Y + Z*V.Z;
	}
	friend FVector operator*( FLOAT Scale, const FVector& V )
	{
		return FVector( V.X * Scale, V.Y * Scale, V.Z * Scale );
	}
	FVector operator+( const FVector& V ) const
	{
		return FVector( X + V.X, Y + V.Y, Z + V.Z );
	}
	FVector operator-( const FVector& V ) const
	{
		return FVector( X - V.X, Y - V.Y, Z - V.Z );
	}
	FVector operator*( FLOAT Scale ) const
	{
		return FVector( X * Scale, Y * Scale, Z * Scale );
	}
	FVector operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.0f/Scale;
		return FVector( X * RScale, Y * RScale, Z * RScale );
	}
	FVector operator*( const FVector& V ) const
	{
		return FVector( X * V.X, Y * V.Y, Z * V.Z );
	}

	// Binary comparison operators.
	UBOOL operator==( const FVector& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z;
	}
	UBOOL operator!=( const FVector& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z;
	}

	// Unary operators.
	FVector operator-() const
	{
		return FVector( -X, -Y, -Z );
	}

	// Assignment operators.
	FVector operator+=( const FVector& V )
	{
		X += V.X; Y += V.Y; Z += V.Z;
		return *this;
	}
	FVector operator-=( const FVector& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z;
		return *this;
	}
	FVector operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale;
		return *this;
	}
	FVector operator/=( FLOAT V )
	{
		FLOAT RV = 1.0f/V;
		X *= RV; Y *= RV; Z *= RV;
		return *this;
	}
	FVector operator*=( const FVector& V )
	{
		X *= V.X; Y *= V.Y; Z *= V.Z;
		return *this;
	}
	FVector operator/=( const FVector& V )
	{
		X /= V.X; Y /= V.Y; Z /= V.Z;
		return *this;
	}

	// Simple functions.
	FLOAT Size() const
	{
		return (FLOAT) sqrt( X*X + Y*Y + Z*Z );
	}
	FLOAT SizeSquared() const
	{
		return X*X + Y*Y + Z*Z;
	}
	FLOAT Size2D() const
	{
		return (FLOAT) sqrt( X*X + Y*Y );
	}
	FLOAT SizeSquared2D() const
	{
		return X*X + Y*Y;
	}
	UBOOL IsZero() const
	{
		return X==0.0 && Y==0.0 && Z==0.0;
	}
	UBOOL Normalize()
	{
		FLOAT SquareSum = X*X+Y*Y+Z*Z;
		if( SquareSum >= SMALL_NUMBER )
		{
			FLOAT Scale = 1.0f/(FLOAT)sqrt(SquareSum);
			X *= Scale; Y *= Scale; Z *= Scale;
			return 1;
		}
		else return 0;
	}
	FVector Projection() const
	{
		FLOAT RZ = 1.0f/Z;
		return FVector( X*RZ, Y*RZ, 1 );
	}
	FVector UnsafeNormal() const
	{
		FLOAT Scale = 1.0f/(FLOAT)sqrt(X*X+Y*Y+Z*Z);
		return FVector( X*Scale, Y*Scale, Z*Scale );
	}
	FLOAT& Component( INT Index )
	{
		return (&X)[Index];
	}

	FVector TransformVectorBy( const FCoords& Coords ) const;

	// Return a boolean that is based on the vector's direction.
	// When      V==(0.0.0) Booleanize(0)=1.
	// Otherwise Booleanize(V) <-> !Booleanize(!B).
	UBOOL Booleanize()
	{
		return
			X >  0.0 ? 1 :
			X <  0.0 ? 0 :
			Y >  0.0 ? 1 :
			Y <  0.0 ? 0 :
			Z >= 0.0 ? 1 : 0;
	}

	FVector SafeNormal() const; //warning: Not inline because of compiler bug.
};



/*-----------------------------------------------------------------------------
	FPlane.
-----------------------------------------------------------------------------*/

class FPlane : public FVector
{
public:
	// Variables.
	FLOAT W;

	// Constructors.
	FPlane()
	{}
	FPlane( const FPlane& P )
	:	FVector(P)
	,	W(P.W)
	{}
	FPlane( const FVector& V )
	:	FVector(V)
	,	W(0)
	{}
	FPlane( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW )
	:	FVector(InX,InY,InZ)
	,	W(InW)
	{}
	FPlane( FVector InNormal, FLOAT InW )
	:	FVector(InNormal), W(InW)
	{}
	FPlane( FVector InBase, const FVector &InNormal )
	:	FVector(InNormal)
	,	W(InBase | InNormal)
	{}

	FPlane( FVector A, FVector B, FVector C )
	:	FVector( ((B-A)^(C-A)).SafeNormal() )
	,	W( A | ((B-A)^(C-A)).SafeNormal() )
	{}


	// Functions.
	FLOAT PlaneDot( const FVector &P ) const
	{
		return X*P.X + Y*P.Y + Z*P.Z - W;
	}
	FPlane Flip() const
	{
		return FPlane(-X,-Y,-Z,-W);
	}
	UBOOL operator==( const FPlane& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z && W==V.W;
	}
	UBOOL operator!=( const FPlane& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z || W!=V.W;
	}

};

class FCoords
{
public:
	FVector	Origin;
	FVector	XAxis;
	FVector YAxis;
	FVector ZAxis;

	// Constructors.
	FCoords() {}
	FCoords( const FVector &InOrigin )
	:	Origin(InOrigin), XAxis(1,0,0), YAxis(0,1,0), ZAxis(0,0,1) {}
	FCoords( const FVector &InOrigin, const FVector &InX, const FVector &InY, const FVector &InZ )
	:	Origin(InOrigin), XAxis(InX), YAxis(InY), ZAxis(InZ) {}

	FCoords FCoords::ApplyPivot(const FCoords& CoordsB) const;

	FCoords Inverse() const;

	FMatrix Matrix() const;

	/*
	// Functions.
	FCoords MirrorByVector( const FVector& MirrorNormal ) const;
	FCoords MirrorByPlane( const FPlane& MirrorPlane ) const;
	FCoords	Transpose() const;
	FCoords Inverse() const;
	*/
};




/*-----------------------------------------------------------------------------
	FMatrix classes.
-----------------------------------------------------------------------------*/

/*
	FMatrix
	Floating point 4x4 matrix
*/

class FMatrix
{
public:

	static FMatrix	Identity;

	// Variables.
	union
	{
		FLOAT M[4][4];
		struct
		{
			FPlane XPlane; // each plane [x,y,z,w] is a *column* in the matrix.
			FPlane YPlane;
			FPlane ZPlane;
			FPlane WPlane;
		};
	};

	// Constructors.

	FMatrix()
	{
	}

	FMatrix(FPlane InX,FPlane InY,FPlane InZ,FPlane InW)
	{
		M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = InX.W;
		M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = InY.W;
		M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = InZ.W;
		M[3][0] = InW.X; M[3][1] = InW.Y;  M[3][2] = InW.Z;  M[3][3] = InW.W;
	}

	FMatrix( FVector InX, FVector InY, FVector InZ )
	{
		M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = 0;
		M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = 0;
		M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = 0;
		M[3][0] = 0;	 M[3][1] = 0;      M[3][2] = 0;      M[3][3] = 0;
	}

	// Destructor.

	~FMatrix()
	{
	}

	void SetIdentity()
	{
		M[0][0] = 1; M[0][1] = 0;  M[0][2] = 0;  M[0][3] = 0;
		M[1][0] = 0; M[1][1] = 1;  M[1][2] = 0;  M[1][3] = 0;
		M[2][0] = 0; M[2][1] = 0;  M[2][2] = 1;  M[2][3] = 0;
		M[3][0] = 0; M[3][1] = 0;  M[3][2] = 0;  M[3][3] = 1;
	}



	FLOAT Determinant3x3()
	{
		FLOAT Det  = M[0][0] * ( M[1][1] * M[2][2] ) - ( M[2][1] * M[1][2] );
		      Det += M[1][0] * ( M[2][1] * M[0][2] ) - ( M[0][1] * M[2][2] );
		      Det += M[2][0] * ( M[0][1] * M[1][2] ) - ( M[1][1] * M[0][2] );
		return( Det );
	}

	// Concatenation operator.

	FORCEINLINE FMatrix operator*(FMatrix Other) const
	{
		FMatrix	Result;

#if defined(__MWERKS__) && defined(__PSX2_EE__)
		asm (
		"lqc2 vf4, 0x00(%0)\n"
		"lqc2 vf5, 0x10(%0)\n"
		"lqc2 vf6, 0x20(%0)\n"
		"lqc2 vf7, 0x30(%0)\n"

		"lqc2 vf8, 0x00(%1)\n"

		"vmulax.xyzw ACC, vf4, vf8\n"
		"vmadday.xyzw ACC, vf5, vf8\n"
		"vmaddaz.xyzw ACC, vf6, vf8\n"
		"vmaddw.xyzw vf8, vf7, vf8\n"

		"sqc2 vf8, 0x00(%2)\n"

		"lqc2 vf10, 0x10(%1)\n"

		"vmulax.xyzw ACC, vf4, vf10\n"
		"vmadday.xyzw ACC, vf5, vf10\n"
		"vmaddaz.xyzw ACC, vf6, vf10\n"
		"vmaddw.xyzw vf10, vf7, vf10\n"

		"sqc2 vf10, 0x10(%2)\n"

		"lqc2 vf9, 0x20(%1)\n"

		"vmulax.xyzw ACC, vf4, vf9\n"
		"vmadday.xyzw ACC, vf5, vf9\n"
		"vmaddaz.xyzw ACC, vf6, vf9\n"
		"vmaddw.xyzw vf9, vf7, vf9\n"

		"sqc2 vf9, 0x20(%2)\n"

		"lqc2 vf11, 0x30(%1)\n"

		"vmulax.xyzw ACC, vf4, vf11\n"
		"vmadday.xyzw ACC, vf5, vf11\n"
		"vmaddaz.xyzw ACC, vf6, vf11\n"
		"vmaddw.xyzw vf11, vf7, vf11\n"

		"sqc2 vf11, 0x30(%2)\n"

		: : "r" (&Other.M[0][0]) , "r" (&M[0][0]), "r" (&Result.M[0][0]) : "memory");

#else

		Result.M[0][0] = M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0] + M[0][3] * Other.M[3][0];
		Result.M[0][1] = M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1] + M[0][3] * Other.M[3][1];
		Result.M[0][2] = M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2] + M[0][3] * Other.M[3][2];
		Result.M[0][3] = M[0][0] * Other.M[0][3] + M[0][1] * Other.M[1][3] + M[0][2] * Other.M[2][3] + M[0][3] * Other.M[3][3];

		Result.M[1][0] = M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0] + M[1][3] * Other.M[3][0];
		Result.M[1][1] = M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1] + M[1][3] * Other.M[3][1];
		Result.M[1][2] = M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2] + M[1][3] * Other.M[3][2];
		Result.M[1][3] = M[1][0] * Other.M[0][3] + M[1][1] * Other.M[1][3] + M[1][2] * Other.M[2][3] + M[1][3] * Other.M[3][3];

		Result.M[2][0] = M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0] + M[2][3] * Other.M[3][0];
		Result.M[2][1] = M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1] + M[2][3] * Other.M[3][1];
		Result.M[2][2] = M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2] + M[2][3] * Other.M[3][2];
		Result.M[2][3] = M[2][0] * Other.M[0][3] + M[2][1] * Other.M[1][3] + M[2][2] * Other.M[2][3] + M[2][3] * Other.M[3][3];

		Result.M[3][0] = M[3][0] * Other.M[0][0] + M[3][1] * Other.M[1][0] + M[3][2] * Other.M[2][0] + M[3][3] * Other.M[3][0];
		Result.M[3][1] = M[3][0] * Other.M[0][1] + M[3][1] * Other.M[1][1] + M[3][2] * Other.M[2][1] + M[3][3] * Other.M[3][1];
		Result.M[3][2] = M[3][0] * Other.M[0][2] + M[3][1] * Other.M[1][2] + M[3][2] * Other.M[2][2] + M[3][3] * Other.M[3][2];
		Result.M[3][3] = M[3][0] * Other.M[0][3] + M[3][1] * Other.M[1][3] + M[3][2] * Other.M[2][3] + M[3][3] * Other.M[3][3];

#endif

		return Result;
	}

	FORCEINLINE void operator*=(FMatrix Other)
	{

#if defined(__MWERKS__) && defined(__PSX2_EE__)
		asm (
		"lqc2 vf4, 0x00(%0)\n"
		"lqc2 vf5, 0x10(%0)\n"
		"lqc2 vf6, 0x20(%0)\n"
		"lqc2 vf7, 0x30(%0)\n"

		"lqc2 vf8, 0x00(%1)\n"

		"vmulax.xyzw ACC, vf4, vf8\n"
		"vmadday.xyzw ACC, vf5, vf8\n"
		"vmaddaz.xyzw ACC, vf6, vf8\n"
		"vmaddw.xyzw vf8, vf7, vf8\n"

		"sqc2 vf8, 0x00(%1)\n"

		"lqc2 vf10, 0x10(%1)\n"

		"vmulax.xyzw ACC, vf4, vf10\n"
		"vmadday.xyzw ACC, vf5, vf10\n"
		"vmaddaz.xyzw ACC, vf6, vf10\n"
		"vmaddw.xyzw vf10, vf7, vf10\n"

		"sqc2 vf10, 0x10(%1)\n"

		"lqc2 vf9, 0x20(%1)\n"

		"vmulax.xyzw ACC, vf4, vf9\n"
		"vmadday.xyzw ACC, vf5, vf9\n"
		"vmaddaz.xyzw ACC, vf6, vf9\n"
		"vmaddw.xyzw vf9, vf7, vf9\n"

		"sqc2 vf9, 0x20(%1)\n"

		"lqc2 vf11, 0x30(%1)\n"

		"vmulax.xyzw ACC, vf4, vf11\n"
		"vmadday.xyzw ACC, vf5, vf11\n"
		"vmaddaz.xyzw ACC, vf6, vf11\n"
		"vmaddw.xyzw vf11, vf7, vf11\n"

		"sqc2 vf11, 0x30(%1)\n"

		: : "r" (&Other.M[0][0]) , "r" (&M[0][0]) );

#else //__PSX2_EE__

		FMatrix Result;
		Result.M[0][0] = M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0] + M[0][3] * Other.M[3][0];
		Result.M[0][1] = M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1] + M[0][3] * Other.M[3][1];
		Result.M[0][2] = M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2] + M[0][3] * Other.M[3][2];
		Result.M[0][3] = M[0][0] * Other.M[0][3] + M[0][1] * Other.M[1][3] + M[0][2] * Other.M[2][3] + M[0][3] * Other.M[3][3];

		Result.M[1][0] = M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0] + M[1][3] * Other.M[3][0];
		Result.M[1][1] = M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1] + M[1][3] * Other.M[3][1];
		Result.M[1][2] = M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2] + M[1][3] * Other.M[3][2];
		Result.M[1][3] = M[1][0] * Other.M[0][3] + M[1][1] * Other.M[1][3] + M[1][2] * Other.M[2][3] + M[1][3] * Other.M[3][3];

		Result.M[2][0] = M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0] + M[2][3] * Other.M[3][0];
		Result.M[2][1] = M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1] + M[2][3] * Other.M[3][1];
		Result.M[2][2] = M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2] + M[2][3] * Other.M[3][2];
		Result.M[2][3] = M[2][0] * Other.M[0][3] + M[2][1] * Other.M[1][3] + M[2][2] * Other.M[2][3] + M[2][3] * Other.M[3][3];

		Result.M[3][0] = M[3][0] * Other.M[0][0] + M[3][1] * Other.M[1][0] + M[3][2] * Other.M[2][0] + M[3][3] * Other.M[3][0];
		Result.M[3][1] = M[3][0] * Other.M[0][1] + M[3][1] * Other.M[1][1] + M[3][2] * Other.M[2][1] + M[3][3] * Other.M[3][1];
		Result.M[3][2] = M[3][0] * Other.M[0][2] + M[3][1] * Other.M[1][2] + M[3][2] * Other.M[2][2] + M[3][3] * Other.M[3][2];
		Result.M[3][3] = M[3][0] * Other.M[0][3] + M[3][1] * Other.M[1][3] + M[3][2] * Other.M[2][3] + M[3][3] * Other.M[3][3];
		*this = Result;

#endif //__PSX2_EE__
	}

	// Comparison operators.

	inline UBOOL operator==(FMatrix& Other) const
	{
		for(INT X = 0;X < 4;X++)
			for(INT Y = 0;Y < 4;Y++)
				if(M[X][Y] != Other.M[X][Y])
					return 0;

		return 1;
	}

	inline UBOOL operator!=(FMatrix& Other) const
	{
		return !(*this == Other);
	}

	// Homogeneous transform.

	FORCEINLINE FPlane TransformFPlane(const FPlane &P) const
	{
		FPlane Result;

#if ASM && !_DEBUG
		__asm
		{
			// Setup.

			mov esi, P
			mov edx, [this]
			lea edi, Result

			fld dword ptr [esi + 0]		//	X
			fmul dword ptr [edx + 0]	//	Xx
			fld dword ptr [esi + 0]		//	X		Xx
			fmul dword ptr [edx + 4]	//	Xy		Xx
			fld dword ptr [esi + 0]		//	X		Xy		Xx
			fmul dword ptr [edx + 8]	//	Xz		Xy		Xx
			fld dword ptr [esi + 0]		//	X		Xz		Xy		Xx
			fmul dword ptr [edx + 12]	//	Xw		Xz		Xy		Xx

			fld dword ptr [esi + 4]		//	Y		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 16]	//	Yx		Xw		Xz		Xy		Xx
			fld dword ptr [esi + 4]		//	Y		Yx		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 20]	//	Yy		Yx		Xw		Xz		Xy		Xx
			fld dword ptr [esi + 4]		//	Y		Yy		Yx		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 24]	//	Yz		Yy		Yx		Xw		Xz		Xy		Xx
			fld dword ptr [esi + 4]		//	Y		Yz		Yy		Yx		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 28]	//	Yw		Yz		Yy		Yx		Xw		Xz		Xy		Xx

			fxch st(3)					//	Yx		Yz		Yy		Yw		Xw		Xz		Xy		Xx
			faddp st(7), st(0)			//	Yz		Yy		Yw		Xw		Xz		Xy		XYx
			faddp st(4), st(0)			//	Yy		Yw		Xw		XYz		Xy		XYx
			faddp st(4), st(0)			//	Yw		Xw		XYz		XYy		XYx
			faddp st(1), st(0)			//	XYw		XYz		XYy		XYx

			fld dword ptr [esi + 8]		//	Z		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 32]	//	Zx		XYw		XYz		XYy		XYx
			fld dword ptr [esi + 8]		//	Z		Zx		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 36]	//	Zy		Zx		XYw		XYz		XYy		XYx
			fld dword ptr [esi + 8]		//	Z		Zy		Zx		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 40]	//	Zz		Zy		Zx		XYw		XYz		XYy		XYx
			fld dword ptr [esi + 8]		//	Z		Zz		Zy		Zx		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 44]	//	Zw		Zz		Zy		Zx		XYw		XYz		XYy		XYx

			fxch st(3)					//	Zx		Zz		Zy		Zw		XYw		XYz		XYy		XYx
			faddp st(7), st(0)			//	Zz		Zy		Zw		XYw		XYz		XYy		XYZx
			faddp st(4), st(0)			//	Zy		Zw		XYw		XYZz	XYy		XYZx
			faddp st(4), st(0)			//	Zw		XYw		XYZz	XYZy	XYZx
			faddp st(1), st(0)			//	XYZw	XYZz	XYZy	XYZx

			fld dword ptr [esi + 12]	//	W		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 48]	//	Wx		XYZw	XYZz	XYZy	XYZx
			fld dword ptr [esi + 12]	//	W		Wx		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 52]	//	Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fld dword ptr [esi + 12]	//	W		Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 56]	//	Wz		Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fld dword ptr [esi + 12]	//	W		Wz		Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 60]	//	Ww		Wz		Wy		Wx		XYZw	XYZz	XYZy	XYZx

			fxch st(3)					//	Wx		Wz		Wy		Ww		XYZw	XYZz	XYZy	XYZx
			faddp st(7), st(0)			//	Wz		Wy		Ww		XYZw	XYZz	XYZy	XYZWx
			faddp st(4), st(0)			//	Wy		Ww		XYZw	XYZWz	XYZy	XYZWx
			faddp st(4), st(0)			//	Ww		XYZw	XYZWz	XYZWy	XYZWx
			faddp st(1), st(0)			//	XYZWw	XYZWz	XYZWy	XYZWx

			fxch st(3)					//	XYZWx	XYZWz	XYZWy	XYZWw
			fstp dword ptr [edi + 0]	//	XYZWz	XYZWy	XYZWw
			fxch st(1)					//	XYZWy	XYZWz	XYZWw
			fstp dword ptr [edi + 4]	//	XYZWz	XYZWw
			fstp dword ptr [edi + 8]	//	XYZWw
			fstp dword ptr [edi + 12]
		}
#else
		Result.X = P.X * M[0][0] + P.Y * M[1][0] + P.Z * M[2][0] + P.W * M[3][0];
		Result.Y = P.X * M[0][1] + P.Y * M[1][1] + P.Z * M[2][1] + P.W * M[3][1];
		Result.Z = P.X * M[0][2] + P.Y * M[1][2] + P.Z * M[2][2] + P.W * M[3][2];
		Result.W = P.X * M[0][3] + P.Y * M[1][3] + P.Z * M[2][3] + P.W * M[3][3];
#endif

		return Result;
	}

	// Regular transform.

	FORCEINLINE FVector TransformFVector(const FVector &V) const
	{
		return TransformFPlane(FPlane(V.X,V.Y,V.Z,1.0f));
	}

	// Normal transform.

	FORCEINLINE FPlane TransformNormal(const FVector& V) const
	{
		return TransformFPlane(FPlane(V.X,V.Y,V.Z,0.0f));
	}

	// Transpose.

	FORCEINLINE FMatrix Transpose()
	{
		FMatrix	Result;

		Result.M[0][0] = M[0][0];
		Result.M[0][1] = M[1][0];
		Result.M[0][2] = M[2][0];
		Result.M[0][3] = M[3][0];

		Result.M[1][0] = M[0][1];
		Result.M[1][1] = M[1][1];
		Result.M[1][2] = M[2][1];
		Result.M[1][3] = M[3][1];

		Result.M[2][0] = M[0][2];
		Result.M[2][1] = M[1][2];
		Result.M[2][2] = M[2][2];
		Result.M[2][3] = M[3][2];

		Result.M[3][0] = M[0][3];
		Result.M[3][1] = M[1][3];
		Result.M[3][2] = M[2][3];
		Result.M[3][3] = M[3][3];

		return Result;
	}

	// Determinant.

	inline FLOAT Determinant() const
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)
	    float a1,a2,a3,a4,b1,b2,b3,b4,c1,c2,c3,c4,d1,d2,d3,d4;

		// Assign to individual variable names to aid selecting
		// correct elements
		a1 = M[0][0]; b1 = M[0][1];
		c1 = M[0][2]; d1 = M[0][3];

		a2 = M[1][0]; b2 = M[1][1];
		c2 = M[1][2]; d2 = M[1][3];

		a3 = M[2][0]; b3 = M[2][1];
		c3 = M[2][2]; d3 = M[2][3];

		a4 = M[3][0]; b4 = M[3][1];
		c4 = M[3][2]; d4 = M[3][3];

	    return Float_Mac4_VU0(
			a1,  Determinant3_VU0( b2, b3, b4, c2, c3, c4, d2, d3, d4),
			-b1, Determinant3_VU0( a2, a3, a4, c2, c3, c4, d2, d3, d4),
			c1,  Determinant3_VU0( a2, a3, a4, b2, b3, b4, d2, d3, d4),
			-d1, Determinant3_VU0( a2, a3, a4, b2, b3, b4, c2, c3, c4)
		);
#else //__PSX2_EE__
		return	M[0][0] * (
					M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
					M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
					M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
					) -
				M[1][0] * (
					M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
					M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
					M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
					) +
				M[2][0] * (
					M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
					M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
					M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
					) -
				M[3][0] * (
					M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
					M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
					M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
					);
#endif
	}

	// Inverse.

#if defined(__MWERKS__) && defined(__PSX2_EE__)
	void Adjoint_VU0( FMatrix & matrix )
	{
	    float a1, a2, a3, a4, b1, b2, b3, b4;
	    float c1, c2, c3, c4, d1, d2, d3, d4;

		// Assign to individual variable names to aid
		// selecting correct values
		a1 = M[0][0]; b1 = M[0][1];
		c1 = M[0][2]; d1 = M[0][3];

		a2 = M[1][0]; b2 = M[1][1];
		c2 = M[1][2]; d2 = M[1][3];

		a3 = M[2][0]; b3 = M[2][1];
		c3 = M[2][2]; d3 = M[2][3];

		a4 = M[3][0]; b4 = M[3][1];
		c4 = M[3][2]; d4 = M[3][3];

	    /* row column labeling reversed since we transpose rows & columns */
	    matrix.M[0][0] =  Determinant3_VU0( b2, b3, b4, c2, c3, c4, d2, d3, d4);
	    matrix.M[1][0] = -Determinant3_VU0( a2, a3, a4, c2, c3, c4, d2, d3, d4);
	    matrix.M[2][0] =  Determinant3_VU0( a2, a3, a4, b2, b3, b4, d2, d3, d4);
	    matrix.M[3][0] = -Determinant3_VU0( a2, a3, a4, b2, b3, b4, c2, c3, c4);

	    matrix.M[0][1] = -Determinant3_VU0( b1, b3, b4, c1, c3, c4, d1, d3, d4);
	    matrix.M[1][1] =  Determinant3_VU0( a1, a3, a4, c1, c3, c4, d1, d3, d4);
	    matrix.M[2][1] = -Determinant3_VU0( a1, a3, a4, b1, b3, b4, d1, d3, d4);
	    matrix.M[3][1] =  Determinant3_VU0( a1, a3, a4, b1, b3, b4, c1, c3, c4);

	    matrix.M[0][2] =  Determinant3_VU0( b1, b2, b4, c1, c2, c4, d1, d2, d4);
	    matrix.M[1][2] = -Determinant3_VU0( a1, a2, a4, c1, c2, c4, d1, d2, d4);
	    matrix.M[2][2] =  Determinant3_VU0( a1, a2, a4, b1, b2, b4, d1, d2, d4);
	    matrix.M[3][2] = -Determinant3_VU0( a1, a2, a4, b1, b2, b4, c1, c2, c4);

	    matrix.M[0][3] = -Determinant3_VU0( b1, b2, b3, c1, c2, c3, d1, d2, d3);
	    matrix.M[1][3] =  Determinant3_VU0( a1, a2, a3, c1, c2, c3, d1, d2, d3);
	    matrix.M[2][3] = -Determinant3_VU0( a1, a2, a3, b1, b2, b3, d1, d2, d3);
	    matrix.M[3][3] =  Determinant3_VU0( a1, a2, a3, b1, b2, b3, c1, c2, c3);
	}
#endif //PSX2_EE
	FMatrix Inverse()
	{
		FMatrix Result;

#if defined(__MWERKS__) && defined(__PSX2_EE__)
		int i, j;
	    	float det;
	    	float rdet;

		// Calculate the adjoint tmatrix
		Adjoint_VU0( Result );

		// Calculate 4x4 determinant
		// If the determinant is 0,
		// the inverse is not unique.
		det = Determinant();

		if( (det < 0.0001) && (det > -0.0001) )
			return FMatrix::Identity;

		// Scale adjoint matrix to get inverse
		rdet = Float_Rcp_VU0(det);

	    	for (i=0; i<4; i++)
	        	for(j=0; j<4; j++)
				Result.M[i][j] = Result.M[i][j] * rdet;
#else
		FLOAT	Det = Determinant();

		if(Det == 0.0f)
			return FMatrix::Identity;

		FLOAT	RDet = 1.0f / Det;

		Result.M[0][0] = RDet * (
				M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
				M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
				);
		Result.M[0][1] = -RDet * (
				M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
				);
		Result.M[0][2] = RDet * (
				M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
				M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);
		Result.M[0][3] = -RDet * (
				M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
				M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
				M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);

		Result.M[1][0] = -RDet * (
				M[1][0] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][0] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
				M[3][0] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
				);
		Result.M[1][1] = RDet * (
				M[0][0] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][0] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][0] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
				);
		Result.M[1][2] = -RDet * (
				M[0][0] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
				M[1][0] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][0] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);
		Result.M[1][3] = RDet * (
				M[0][0] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
				M[1][0] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
				M[2][0] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);

		Result.M[2][0] = RDet * (
				M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
				M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
				M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
		Result.M[2][1] = -RDet * (
				M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
				M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
				M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
		Result.M[2][2] = RDet * (
				M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
				M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
				M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
		Result.M[2][3] = -RDet * (
				M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);

		Result.M[3][0] = -RDet * (
				M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
				M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
				M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
		Result.M[3][1] = RDet * (
				M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
				M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
				M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
		Result.M[3][2] = -RDet * (
				M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
				M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
				M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
		Result.M[3][3] = RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
#endif
		return Result;
	}

	FMatrix TransposeAdjoint() const
	{
		FMatrix ta;

		ta.M[0][0] = this->M[1][1] * this->M[2][2] - this->M[1][2] * this->M[2][1];
		ta.M[0][1] = this->M[1][2] * this->M[2][0] - this->M[1][0] * this->M[2][2];
		ta.M[0][2] = this->M[1][0] * this->M[2][1] - this->M[1][1] * this->M[2][0];
		ta.M[0][3] = 0.f;

		ta.M[1][0] = this->M[2][1] * this->M[0][2] - this->M[2][2] * this->M[0][1];
		ta.M[1][1] = this->M[2][2] * this->M[0][0] - this->M[2][0] * this->M[0][2];
		ta.M[1][2] = this->M[2][0] * this->M[0][1] - this->M[2][1] * this->M[0][0];
		ta.M[1][3] = 0.f;

		ta.M[2][0] = this->M[0][1] * this->M[1][2] - this->M[0][2] * this->M[1][1];
		ta.M[2][1] = this->M[0][2] * this->M[1][0] - this->M[0][0] * this->M[1][2];
		ta.M[2][2] = this->M[0][0] * this->M[1][1] - this->M[0][1] * this->M[1][0];
		ta.M[2][3] = 0.f;

		ta.M[3][0] = 0.f;
		ta.M[3][1] = 0.f;
		ta.M[3][2] = 0.f;
		ta.M[3][1] = 1.f;

		return ta;
	}

	// Conversions.

	FCoords Coords()
	{
		FCoords	Result;

		Result.XAxis = FVector(M[0][0],M[1][0],M[2][0]);
		Result.YAxis = FVector(M[0][1],M[1][1],M[2][1]);
		Result.ZAxis = FVector(M[0][2],M[1][2],M[2][2]);
		Result.Origin = FVector(M[3][0],M[3][1],M[3][2]);

		return Result;
	}

};


// Conversions for Unreal1 coordinate system class.

inline FMatrix FMatrixFromFCoords(const FCoords& FC)
{
	FMatrix M;
	M.XPlane = FPlane( FC.XAxis.X, FC.XAxis.Y, FC.XAxis.Z, FC.Origin.X );
	M.YPlane = FPlane( FC.YAxis.X, FC.YAxis.Y, FC.YAxis.Z, FC.Origin.Y );
	M.ZPlane = FPlane( FC.ZAxis.X, FC.ZAxis.Y, FC.ZAxis.Z, FC.Origin.Z );
	M.WPlane = FPlane( 0.0f,     0.0f,    0.0f,    1.0f    );
	return M;
}

inline FCoords FCoordsFromFMatrix(const FMatrix& FM)
{
	FCoords C;
	C.Origin = FVector( FM.XPlane.W, FM.YPlane.W, FM.ZPlane.W );
	C.XAxis  = FVector( FM.XPlane.X, FM.XPlane.Y, FM.XPlane.Z );
	C.YAxis  = FVector( FM.YPlane.X, FM.YPlane.Y, FM.YPlane.Z );
	C.ZAxis  = FVector( FM.ZPlane.X, FM.ZPlane.Y, FM.ZPlane.Z );
	return C;
}


/*-----------------------------------------------------------------------------
	FQuat.
-----------------------------------------------------------------------------*/

//
// floating point quaternion.
//
class FQuat
{
	public:
	// Variables.
	struct {FLOAT X,Y,Z,W;};

	// Constructors.
	FQuat()
	{}

	FQuat( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InA )
	:	X(InX), Y(InY), Z(InZ), W(InA)
	{}


	// Binary operators.
	FQuat operator+( const FQuat& Q ) const
	{
		return FQuat( X + Q.X, Y + Q.Y, Z + Q.Z, W + Q.W );
	}

	FQuat operator-( const FQuat& Q ) const
	{
		return FQuat( X - Q.X, Y - Q.Y, Z - Q.Z, W - Q.W );
	}

	FQuat operator*( const FQuat& Q ) const
	{
		return FQuat(
			X*Q.X - Y*Q.Y - Z*Q.Z - W*Q.W,
			X*Q.Y + Y*Q.X + Z*Q.W - W*Q.Z,
			X*Q.Z - Y*Q.W + Z*Q.X + W*Q.Y,
			X*Q.W + Y*Q.Z - Z*Q.Y + W*Q.X
			);
	}

	FQuat operator*( const FLOAT& Scale ) const
	{
		return FQuat( Scale*X, Scale*Y, Scale*Z, Scale*W);
	}

	// Unary operators.
	FQuat operator-() const
	{
		return FQuat( X, Y, Z, -W );
	}

    // Misc operators
	UBOOL operator!=( const FQuat& Q ) const
	{
		return X!=Q.X || Y!=Q.Y || Z!=Q.Z || W!=Q.W;
	}

	UBOOL Normalize()
	{
		//
		FLOAT SquareSum = (FLOAT)(X*X+Y*Y+Z*Z+W*W);
		if( SquareSum >= DELTA )
		{
			FLOAT Scale = 1.0f/(FLOAT)sqrt(SquareSum);
			X *= Scale;
			Y *= Scale;
			Z *= Scale;
			W *= Scale;
			return true;
		}
		else
		{
			X = 0.0f;
			Y = 0.0f;
			Z = 0.1f;
			W = 0.0f;
			return false;
		}
	}

	/*
    // Angle scaling.
	FQuat operator*( FLOAT Scale, const FQuat& Q )
	{
		return FQuat( Q.X, Q.Y, Q.Z, Q.W * Scale );
	}
	*/

	// friends
	// friend FAngAxis	FQuatToFAngAxis(const FQuat& Q);
	// friend void SlerpQuat(const FQuat& Q1, const FQuat& Q2, FLOAT slerp, FQuat& Result)
	// friend FQuat	BlendQuatWith(const FQuat& Q1, FQuat& Q2, FLOAT Blend)
	// friend  FLOAT FQuatDot(const FQuat&1, FQuat& Q2);
};

//
// Misc conversions
//

FQuat FMatrixToFQuat(const FMatrix& M);


//
// Dot product of axes to get cos of angle  #Warning some people use .W component here too !
//
inline FLOAT FQuatDot(const FQuat& Q1,const FQuat& Q2)
{
	return( Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z );
};

//
// Floating point Angle-axis rotation representation.
//
class FAngAxis
{
public:
	// Variables.
	struct {FLOAT X,Y,Z,A;};
};


// Inlines

inline FLOAT Square(const FLOAT F1)
{
	return F1*F1;
}

// Error measure (angle) between two quaternions, ranged [0..1]
inline FLOAT FQuatError(FQuat& Q1,FQuat& Q2)
{
	// Returns the hypersphere-angle between two quaternions; alignment shouldn't matter, though
	// normalized input is expected.
	FLOAT cosom = Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z + Q1.W*Q2.W;
	return ( abs(cosom) < 0.9999999f) ? acos(cosom)*(1.f/3.1415926535f) : 0.0f;
}

// Ensure quat1 points to same side of the hypersphere as quat2
inline void AlignFQuatWith(FQuat &quat1, const FQuat &quat2)
{
	FLOAT Minus  = Square(quat1.X-quat2.X) + Square(quat1.Y-quat2.Y) + Square(quat1.Z-quat2.Z) + Square(quat1.W-quat2.W);
	FLOAT Plus   = Square(quat1.X+quat2.X) + Square(quat1.Y+quat2.Y) + Square(quat1.Z+quat2.Z) + Square(quat1.W+quat2.W);

	if (Minus > Plus)
	{
		quat1.X = - quat1.X;
		quat1.Y = - quat1.Y;
		quat1.Z = - quat1.Z;
		quat1.W = - quat1.W;
	}
}

// No-frills spherical interpolation. Assumes aligned quaternions, and the output is not normalized.
inline FQuat SlerpQuat(const FQuat &quat1,const FQuat &quat2, float slerp)
{
	FQuat result;
	float omega,cosom,sininv,scale0,scale1;

	// Get cosine of angle betweel quats.
	cosom = quat1.X * quat2.X +
			quat1.Y * quat2.Y +
			quat1.Z * quat2.Z +
			quat1.W * quat2.W;

	if( cosom < 0.99999999f )
	{
		omega = acos(cosom);
		sininv = 1.f/sin(omega);
		scale0 = sin((1.f - slerp) * omega) * sininv;
		scale1 = sin(slerp * omega) * sininv;

		result.X = scale0 * quat1.X + scale1 * quat2.X;
		result.Y = scale0 * quat1.Y + scale1 * quat2.Y;
		result.Z = scale0 * quat1.Z + scale1 * quat2.Z;
		result.W = scale0 * quat1.W + scale1 * quat2.W;
		return result;
	}
	else
	{
		return quat1;
	}

}

inline FVector FVector::TransformVectorBy( const FCoords &Coords ) const
{
	return FVector(	*this | Coords.XAxis, *this | Coords.YAxis, *this | Coords.ZAxis );
}


//
// Triple product of three vectors.
//
inline FLOAT FTriple( const FVector& X, const FVector& Y, const FVector& Z )
{
	return
	(	(X.X * (Y.Y * Z.Z - Y.Z * Z.Y))
	+	(X.Y * (Y.Z * Z.X - Y.X * Z.Z))
	+	(X.Z * (Y.X * Z.Y - Y.Y * Z.X)) );
}

/*-----------------------------------------------------------------------------
	Texturing.
-----------------------------------------------------------------------------*/

// Accepts a triangle (XYZ and ST values for each point) and returns a poly base and UV vectors
inline void FTexCoordsToVectors( FVector V0, FVector ST0, FVector V1, FVector ST1, FVector V2, FVector ST2, FVector* InBaseResult, FVector* InUResult, FVector* InVResult )
{
	// Create polygon normal.
	FVector PN = FVector((V0-V1) ^ (V2-V0));
	PN = PN.SafeNormal();

	// Fudge UV's to make sure no infinities creep into UV vector math, whenever we detect identical U or V's.
	if( ( ST0.X == ST1.X ) || ( ST2.X == ST1.X ) || ( ST2.X == ST0.X ) ||
		( ST0.Y == ST1.Y ) || ( ST2.Y == ST1.Y ) || ( ST2.Y == ST0.Y ) )
	{
		ST1 += FVector(0.004173f,0.004123f,0.0f);
		ST2 += FVector(0.003173f,0.003123f,0.0f);
	}

	//
	// Solve the equations to find our texture U/V vectors 'TU' and 'TV' by stacking them
	// into a 3x3 matrix , one for  u(t) = TU dot (x(t)-x(o) + u(o) and one for v(t)=  TV dot (.... ,
	// then the third assumes we're perpendicular to the normal.
	//
	FCoords TexEqu;
	TexEqu.XAxis = FVector(	V1.X - V0.X, V1.Y - V0.Y, V1.Z - V0.Z );
	TexEqu.YAxis = FVector( V2.X - V0.X, V2.Y - V0.Y, V2.Z - V0.Z );
	TexEqu.ZAxis = FVector( PN.X,        PN.Y,        PN.Z        );
	TexEqu.Origin =FVector( 0.0f, 0.0f, 0.0f );
	TexEqu = TexEqu.Inverse();

	FVector UResult( ST1.X-ST0.X, ST2.X-ST0.X, 0.0f );
	FVector TUResult = UResult.TransformVectorBy( TexEqu );

	FVector VResult( ST1.Y-ST0.Y, ST2.Y-ST0.Y, 0.0f );
	FVector TVResult = VResult.TransformVectorBy( TexEqu );

	//
	// Adjust the BASE to account for U0 and V0 automatically, and force it into the same plane.
	//
	FCoords BaseEqu;
	BaseEqu.XAxis = TUResult;
	BaseEqu.YAxis = TVResult;
	BaseEqu.ZAxis = FVector( PN.X, PN.Y, PN.Z );
	BaseEqu.Origin = FVector( 0.0f, 0.0f, 0.0f );

	FVector BResult = FVector( ST0.X - ( TUResult|V0 ), ST0.Y - ( TVResult|V0 ),  0.0f );

	*InBaseResult = - 1.0f *  BResult.TransformVectorBy( BaseEqu.Inverse() );
	*InUResult = TUResult;
	*InVResult = TVResult;

}



/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

#endif // UNSKELMATH_H