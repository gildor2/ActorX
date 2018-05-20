// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/*=============================================================================

  UnSkeletal.cpp: General ActorXporter support functions, misc math classes ripped-out from Unreal.

  Created by Erik de Neve

  Notes:
		- Most math code mirrored from Unreal Tournament's UnMath.cpp/.h

=============================================================================*/

#include "UnSkeletal.h"
#include "Win32IO.h"

#pragma warning(push)
#pragma warning(disable : 4701) // silly non-init warning


//
// General string support
//

// Case insensitive substring search
UBOOL FindSubString(const TCHAR* CheckString, const TCHAR* Fragment, INT& Pos)
{
	TCHAR *LString = new TCHAR[_tcslen(CheckString)+1];
	_tcscpy(LString,CheckString);
	_tcslwr(LString); //make lower case

	TCHAR *LFragment = new TCHAR[_tcslen(Fragment)+1];
	_tcscpy(LFragment,Fragment);
	_tcslwr(LFragment); //make lower case

	TCHAR* PosPtr = _tcsstr(LString,LFragment);

	UBOOL Result = (PosPtr != NULL);  // check if it contains LString

	if (PosPtr != NULL) Pos = (INT)(PosPtr-LString); // start position
	else Pos = 0;

	if (LString) delete LString;
	if (LFragment) delete LFragment;
	return Result;
}

UBOOL CheckSubString(const TCHAR* CheckString, const TCHAR* Fragment)
{
	INT DummyPos;
	return FindSubString(CheckString,Fragment,DummyPos);
}


// Check for equalit strings but ignore case.
UBOOL CheckStringsEqual( const TCHAR* FirstString, const TCHAR* SecondString )
{
	if( _tcslen(FirstString) != _tcslen(SecondString) )
		return false;
	INT DummyPos;
	return FindSubString( FirstString, SecondString, DummyPos );
}



//
// Try to find a DigitNum digits sized number after a certain substring in a string and return the numerical value.
//
INT FindValueTag(const TCHAR* CheckString, const TCHAR* Fragment, INT DigitNum)
{
	INT SubPos;
	FindSubString(CheckString,Fragment,SubPos);
	if( !FindSubString(CheckString,Fragment,SubPos) ) return -1;

	INT N=0;
	SubPos += ( INT )_tcslen(Fragment); // move beyond fragment.
	INT NumPos = 0;

	while( (CheckString[NumPos+SubPos] >= 0x30) && (CheckString[NumPos+SubPos] <= 0x39)  && (NumPos < DigitNum) )
	{
		INT A=CheckString[NumPos+SubPos]-0x30; //0x30..0x39
		N= 10*N + A;
		NumPos++;
	}
	return N;
}



#define CTL_CHARS  31
#define SINGLE_QUOTE 39

// Replace some characters we don't care for.
TCHAR* CleanString(const TCHAR* name)
{
	static TCHAR buffer[256];
	TCHAR* cPtr;

    _tcscpy(buffer, name);
    cPtr = buffer;

    while(*cPtr) {
		if (*cPtr == '"')
			*cPtr = SINGLE_QUOTE;
        else if (*cPtr <= CTL_CHARS)
			*cPtr = ('_');
        cPtr++;
    }

	// clean out tail
	while( cPtr <= &buffer[255] )
	{
		*cPtr = 0;
		cPtr++;
	}

	return buffer;
}


// Removes the 'Fragment' (e.g.  .max ) and puts result in OutString.
UBOOL RemoveExtString(const TCHAR* CheckString, const TCHAR* Fragment, TCHAR* OutString)
{
	INT Pos=0;
	FindSubString( CheckString, Fragment, Pos);
	if(Pos == 0)
	{
		return 0;
		OutString[0] = 0;
	}

	for(INT t=0; t<Pos; t++)
	{
		OutString[t]=CheckString[t];
	}
	OutString[Pos] = 0;
	return 1;
}


// Todo: make faster by assuming last row zero and ortho(normal) translation...
FMatrix CombineTransforms(const FMatrix& M, const FMatrix& N)
{
	FMatrix Q;
	for( int i=0; i<4; i++ ) // row
		for( int j=0; j<3; j++ ) // column
			Q.M[i][j] = M.M[i][0]*N.M[0][j] + M.M[i][1]*N.M[1][j] + M.M[i][2]*N.M[2][j] + M.M[i][3]*N.M[3][j];

	return (Q);
}
#pragma warning(pop)


/*-----------------------------------------------------------------------------
	FQuaternion and FMatrix support functions
-----------------------------------------------------------------------------*/

FMatrix	FMatrix::Identity(FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,1,0),FPlane(0,0,0,1));



//
// TODO: can be greatly optimized with a few tables (eg a 1/sin table!!)
// Warning : assumes normalized quaternions.
//

FAngAxis FQuatToFAngAxis(const FQuat& Q)
{
	FLOAT scale = (FLOAT)sin(Q.W);
	FAngAxis N;

	if (scale >= DELTA)
	{
		N.X = Q.X / scale;
		N.Y = Q.Y / scale;
		N.Z = Q.Z / scale;

		// TODO: can change to factor facilitating fixed-point representation.
		N.A = (2.0f * acos (Q.W));

		// Degrees: N.A = ((FLOAT)acos(Q.W) * 360.0f) * INV_PI;
	}
	else
	{
		N.X = 0.0f;
		N.Y = 0.0f;
		N.Z = 1.0f;
		N.A = 0.0f;
	}

	return N;
};


//
// Angle-Axis to Quaternion. No normalized axis assumed.
//
FQuat FAngAxisToFQuat(const FAngAxis& N)
{
	FLOAT scale = N.X*N.X + N.Y*N.Y + N.Z*N.Z;

	FQuat Q;

	if (scale >= DELTA)
	{
		FLOAT invscale = 1.0f /(FLOAT)sqrt(scale);
		Q.X = N.X * invscale;
		Q.Y = N.Y * invscale;
		Q.Z = N.Z * invscale;
		Q.W = cos( N.A * 0.5f); //Radians assumed.
	}
	else
	{
		Q.X = 0.0f;
		Q.Y = 0.0f;
		Q.Z = 1.0f;
		Q.W = 0.0f;
	}

	return Q;
}

// SafeNormal
FVector FVector::SafeNormal() const
{
	FLOAT SquareSum = X*X + Y*Y + Z*Z;
	if( SquareSum < SMALL_NUMBER )
		return FVector( 0.f, 0.f, 0.f );

	FLOAT Size = sqrt(SquareSum);
	FLOAT Scale = 1.f/Size;
	return FVector( X*Scale, Y*Scale, Z*Scale );
}

//
// Matrix-to-Quaternion code. From Bobick.
//
FQuat FMatrixToFQuat(const FMatrix& M)
{
	FQuat Q;

	INT nxt[3] = {1, 2, 0};

	FLOAT tr = M.M[0][0] + M.M[1][1] + M.M[2][2];

	// Check the diagonal

	if (tr > 0.0)
	{
		FLOAT s = sqrt (tr + 1.0f);
		Q.W = s / 2.0f;
  		s = 0.5f / s;

	    Q.X = (M.M[1][2] - M.M[2][1]) * s;
	    Q.Y = (M.M[2][0] - M.M[0][2]) * s;
	    Q.Z = (M.M[0][1] - M.M[1][0]) * s;
	}
	else
	{
		// diagonal is negative
		FLOAT  q[4];

		INT i = 0;

		if (M.M[1][1] > M.M[0][0]) i = 1;
		if (M.M[2][2] > M.M[i][i]) i = 2;

		INT j = nxt[i];
		INT k = nxt[j];

		FLOAT s = sqrt ((M.M[i][i] - (M.M[j][j] + M.M[k][k])) + 1.0f);

		q[i] = s * 0.5f;

		//if (s != 0.0f) s = 0.5f / s;
		if (s > DELTA) s = 0.5f / s;

		q[3] = (M.M[j][k] - M.M[k][j]) * s;
		q[j] = (M.M[i][j] + M.M[j][i]) * s;
		q[k] = (M.M[i][k] + M.M[k][i]) * s;

		Q.X = q[0];
		Q.Y = q[1];
		Q.Z = q[2];
		Q.W = q[3];
	}

	return Q;
}


//
// Angles to Quaternion. Vector components must be angles.
//

FQuat EulerToFQuat( const FVector Angle)
{
	FLOAT ar = Angle.X * 0.5f;  // roll ?
	FLOAT ap = Angle.Y * 0.5f;  // pitch ?
	FLOAT ay = Angle.Z * 0.5f;  // yaw ?

	FLOAT cx = cos(ar);
	FLOAT sx = sin(ar);

	FLOAT cy = cos(ap);
	FLOAT sy = sin(ap);

	FLOAT cz = cos(ay);
	FLOAT sz = sin(ay);

	FLOAT cc = cx * cz;
	FLOAT cs = cx * sz;
	FLOAT sc = sx * cz;
	FLOAT ss = sx * sz;

	FQuat Q;
	Q.X = (cy * sc) - (sy * cs);  // Warning- flipped ?
	Q.Y = (cy * ss) + (sy * cc);
	Q.Z = (cy * cs) - (sy * sc);
	Q.W = (cy * cc) + (sy * ss);

	return Q;
}


//
//  Spherical linear interpolation
//  Source: Watt & Watt (as fixed by Jeff Lander)
//
//  Todo: - Write a variation to handle multi-revolution spins ?
//        - Check if cosom calculation is correct.

void SlerpQuat(const FQuat& Q1, const FQuat& Q2, FLOAT slerp, FQuat& Result)
{
	FLOAT scale0,scale1;

	/* !!!!! Lander uses W in this !

	FLOAT cosom = Q1.X * Q2.X +
				  Q1.Y * Q2.Y +
				  Q1.Z * Q2.Z +
				  Q1.W * Q2.W;
	*/

	// Use dot to get cosine of angle betweek quaternions.
	FLOAT cosom = FQuatDot(Q1,Q2);

	// Use the shortest route over the sphere.
	if ((1.0f + cosom) > SLERP_DELTA)
	{
		// Slerp unless the angle is small enough to lerp.
		if ((1.0f - cosom) > DELTA)
		{
			FLOAT omega = acos(cosom);
			FLOAT sinom  = sin(omega);
			scale0 = sin((1.0f - slerp) * omega) / sinom;
			scale1 = sin(slerp * omega) / sinom;
		}
		else
		{
			scale0 = 1.0f - slerp;
			scale1 = slerp;
		}

		// Interpolate result.
		Result = Q1 * scale0 + Result * scale1;
	}
	else
	{
		// Use the shorter route.
		Result.X = - Q2.Y;
		Result.Y =   Q2.X;
		Result.Z = - Q2.W;
		Result.W =   Q2.Z;

		scale0 = sin((1.0f - slerp) * (FLOAT)HALF_PI);
		scale1 = sin(slerp * (FLOAT)HALF_PI);

		// Slerp result.
		Result = Q1 * scale0 + Result * scale1;
	}
};

//
// Combine two 'pivot' transforms.
//
FCoords FCoords::ApplyPivot(const FCoords& CoordsB) const
{
	// Fast solution assuming orthogonal coordinate system
	FCoords Temp;
	Temp.Origin = CoordsB.Origin + Origin.TransformVectorBy(CoordsB);
	Temp.XAxis = CoordsB.XAxis.TransformVectorBy( *this );
	Temp.YAxis = CoordsB.YAxis.TransformVectorBy( *this );
	Temp.ZAxis = CoordsB.ZAxis.TransformVectorBy( *this );
	return Temp;
}

//
// Coordinate system inverse.
//
FCoords FCoords::Inverse() const
{
	FLOAT Div = FTriple( XAxis, YAxis, ZAxis );
	if( !Div ) Div = 1.f;

	FLOAT RDet = 1.f / Div;
	return FCoords
	(	-Origin.TransformVectorBy(*this)
	,	RDet * FVector
		(	(YAxis.Y * ZAxis.Z - YAxis.Z * ZAxis.Y)
		,	(ZAxis.Y * XAxis.Z - ZAxis.Z * XAxis.Y)
		,	(XAxis.Y * YAxis.Z - XAxis.Z * YAxis.Y) )
	,	RDet * FVector
		(	(YAxis.Z * ZAxis.X - ZAxis.Z * YAxis.X)
		,	(ZAxis.Z * XAxis.X - XAxis.Z * ZAxis.X)
		,	(XAxis.Z * YAxis.X - XAxis.X * YAxis.Z))
	,	RDet * FVector
		(	(YAxis.X * ZAxis.Y - YAxis.Y * ZAxis.X)
		,	(ZAxis.X * XAxis.Y - ZAxis.Y * XAxis.X)
		,	(XAxis.X * YAxis.Y - XAxis.Y * YAxis.X) )
	);
}





// Convert this FCoords to a 4x4 matrix
FMatrix FCoords::Matrix() const
{
	FMatrix M;

	M.M[0][0] = XAxis.X;
	M.M[0][1] = YAxis.X;
	M.M[0][2] = ZAxis.X;
	M.M[0][3] = 0.0f;
	M.M[1][0] = XAxis.Y;
	M.M[1][1] = YAxis.Y;
	M.M[1][2] = ZAxis.Y;
	M.M[1][3] = 0.0f;
	M.M[2][0] = XAxis.Z;
	M.M[2][1] = YAxis.Z;
	M.M[2][2] = ZAxis.Z;
	M.M[2][3] = 0.0f;
	M.M[3][0] = XAxis | -Origin;
	M.M[3][1] = YAxis | -Origin;
	M.M[3][2] = ZAxis | -Origin;
	M.M[3][3] = 1.0f;

	return M;
}
