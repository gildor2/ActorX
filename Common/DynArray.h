// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/*

	DynArray.h - Dynamic Array (generic list) template class.

	Created by Erik de Neve

	Parts borrowed from the Game Developer Magazine 'Bunnylod' example source by Stan Melax.
	Cooler than Unreal's native dynamic arrays since it allows square bracket access :-)

	Usage:

	declare a dynamic array like:
	TArray <StructureOrClassType>  NewListName;

	Destruction is automatic for temp arrays.

	Todo:  - Make resizing and reallocation more like Unreal's TArray..
	       - Constructor/Destructor issues; verify use of new(TArrayName)ElementCtor()

    NOTE: there is unavoidable danger if you use TArrays like this:  MyArray( MyArray.Add() )=123

*/


#ifndef DYNARRAY_H
#define DYNARRAY_H

#define INITIALSIZE (128)

#include "Win32IO.h"

extern TextFile MemLog;

// #define debugassert(X,Y) {if(!(X))MemLog.Logf(" Assertion failed:  %s \n",Y);}

#define debugassert(X,Y) assert(X)


template <class Type> class TArray {
	public:

		//
		TArray(int s=0);
		~TArray();
		void	Empty();
		void	SetSize(int s);
		int    AddItem(Type);
		int     Add(int n=1);
		int     AddZeroed(int n=1);
		void	AddExactItem(Type);
		int     AddExact(int n=1);
		int     AddExactZeroed(int n=1);
		int		Num();

		void	AddUniqueItem(Type);
		int 	Contains(Type);
		int     Where(Type);   // Returns index of item.
		void	Remove(Type);  // Remove first and only occurrence of item (assumed unique - otherwise triggers assertion..)
		void    Shrink();      // Ensures only used entries are allocated.
		void    Reset(int n=INITIALSIZE); // Empties ArrayNum, sets ArrayMax to n and reallocates.
		void	DelIndex(int i);
		Type&   TopItem();
		Type	&operator[](int i)
		{
			debugassert( i>=0 && i<ArrayNum ,"");
			return Data[i];
		}
		const Type	&operator[](int i) const
		{
			debugassert( i>=0 && i<ArrayNum ,"");
			return Data[i];
		}
	// private:
	// Variables.
		Type*	Data;
		int     ArrayNum; // Number of used elements.
		int		ArrayMax; // Total amount of items allocated.

	protected:
		void	TRealloc(int s);
};


template <class Type>
Type& TArray<Type>::TopItem()
{
	return Data[ArrayNum-1];
}

template <class Type>
TArray<Type>::TArray(int s)
{
	ArrayNum=0;
	ArrayMax = 0;
	Data = NULL;
	if( s > 0 )
	{
		TRealloc(s);
	}
	else
	{
		ArrayNum = 0;
		ArrayMax = 0;
	}
}

template <class Type>
TArray<Type>::~TArray()
{
	Empty();
}


// Shrink: reallocate array to allocate only the used elements.
template <class Type>
void TArray<Type>::Shrink()
{
	if ( ArrayNum>0 && ArrayMax>ArrayNum )
		TRealloc( ArrayNum );
}


// Empties ArrayNum, sets ArrayMax to n and reallocates.

template <class Type>
void TArray<Type>::Reset(int n)
{
	INT NewArrayMax;

	if( n>=1 )
		NewArrayMax = n;
	else
		NewArrayMax = INITIALSIZE;

	if (ArrayNum>0)
	{
		ArrayNum = 0;
		TRealloc(NewArrayMax);
	}
}


// Expand Arraynum by n.
template <class Type>
int TArray<Type>::Add(int n)
{
	debugassert(ArrayNum<=ArrayMax,"!ArrayNum<=ArrayMax in Add()");
	debugassert(n>0,"n>0 in Add()");

	if ( (ArrayNum+n) >= ArrayMax)
	{
		INT NewArrayMax = (ArrayNum+n) + 3*(ArrayNum+n)/8 + INITIALSIZE;
		TRealloc(NewArrayMax);
	}
	int LastTopElementIndex = ArrayNum;
	ArrayNum += n;
	debugassert(ArrayNum<=ArrayMax,"ArrayNum<=ArrayMax in Add()");
	return LastTopElementIndex;
}

// Expand Arraynum by n.
template <class Type>
int TArray<Type>::AddExact(int n)
{
	debugassert(ArrayNum<=ArrayMax,"ArrayNum<=ArrayMax in AddExact");
	debugassert(n>0,"n>0 in AddExact");
	if ( (ArrayNum+n) > ArrayMax)
	{
		INT NewArrayMax = ArrayNum+n;
		TRealloc(NewArrayMax);
	}
	int LastTopElementIndex = ArrayNum;
	ArrayNum += n;
	debugassert(ArrayNum<=ArrayMax,"ArrayNum<=ArrayMax in AddExact");
	return LastTopElementIndex;
}



template <class Type>
int TArray<Type>::AddZeroed(int n)
{
	int OldArrayNum = ArrayNum;

	debugassert(ArrayNum<=ArrayMax,"");
	debugassert(n>0,"");

	if ( (ArrayNum+n) >= ArrayMax ) // need to allocate more ?
	{
		INT NewArrayMax = (ArrayNum+n) + 3*(ArrayNum+n)/8 + INITIALSIZE;
		TRealloc(NewArrayMax);
	}
	int NewElementIndex = ArrayNum;
	ArrayNum += n;

	// Zero content.
	for(int i=OldArrayNum; i<ArrayNum; i++)
	{
			memset( ((BYTE*)Data) + (i*sizeof(Type)), 0, sizeof(Type) );
	}

	return NewElementIndex;
}

// AddExact: does not allocate ANY slack, typically for arrays that are small, numerous and aren't expected to grow.
template <class Type>
int TArray<Type>::AddExactZeroed(int n)
{
	int OldArrayNum = ArrayNum;
	debugassert(ArrayNum<=ArrayMax,"!ArrayNum<=ArrayMax");
	debugassert(n>0,"!n>0");

	if ( (ArrayNum+n) > ArrayMax )
	{
		INT NewArrayMax = ArrayNum+n;
		TRealloc(NewArrayMax);
	}
	int NewElementIndex = ArrayNum;
	ArrayNum += n;

	debugassert(ArrayNum<=ArrayMax,"!ArrayNum<=ArrayMax");

	// Zero content.
	for(int i=OldArrayNum; i<ArrayNum; i++)
	{
			memset( ((BYTE*)Data) + (i*sizeof(Type)), 0, sizeof(Type) );
	}

	return NewElementIndex;
}


template <class Type>
void TArray<Type>::Empty()
{
	if( (Data != NULL) && ( ArrayMax > 0 ) )
	{
		if( ArrayNum > 0 ) // Why: destructors allowed only if there are valid array members present.
			delete Data;
		else
			delete (BYTE*)Data;

		Data = NULL;

	}
	ArrayMax = 0;
	ArrayNum = 0;
}

template <class Type>
void TArray<Type>::SetSize(int s)
{
	debugassert( s >= 0,"! s>=0 " );
	if( s==0 )
	{
		Empty();
	}
	else
	{
		if( ArrayNum > s )
		{
			ArrayNum = s;
		}
		TRealloc(s);
		ArrayNum=s;
	}

}

template <class Type>
int TArray<Type>::AddItem(Type t)
{
	INT NewIdx = ArrayNum;
	Add(1);
	Data[NewIdx] = t;
	return NewIdx;
}

template <class Type>
void TArray<Type>::AddExactItem(Type t)
{
	INT NewIdx = ArrayNum;
	AddExact(1);
	Data[NewIdx] = t;
}


// Return total number of instances of 't' present in the array.
template <class Type>
int TArray<Type>::Contains(Type t)
{
	int i;
	int count=0;
	for(i=0;i<ArrayNum;i++)
	{
		if(Data[i] == t) count++;
	}
	return count;
}


// Return first occurrence of t, -1 if not found.
template <class Type>
int TArray<Type>::Where(Type t)
{
	int i;
	int count=0;
	for(i=0;i<ArrayNum;i++)
	{
		if(Data[i] == t) return i;
	}
	return -1;
}

template <class Type>
int TArray<Type>::Num()
{
	return ArrayNum;
}

template <class Type>
void TArray<Type>::AddUniqueItem(Type t)
{
	if(!Contains(t)) AddItem(t);
}

template <class Type>
void TArray<Type>::DelIndex(int i)
{
	debugassert(i<ArrayNum,"!i<ArrayNum");
	ArrayNum--;

	//#debug: properly call destructors if available !
	while(i<ArrayNum)
	{
		Data[i] = Data[i+1];
		i++;
	}
}

template <class Type>
void TArray<Type>::Remove(Type t)
{
	int i;
	for(i=0;i<ArrayNum;i++)
	{
		if(Data[i] == t)
		{
			break;
		}
	}
	DelIndex(i);
	for(i=0;i<ArrayNum;i++)
	{
		debugassert(Data[i] != t,"! Data[i] != t");
	}
}

template <class Type>
void TArray<Type>::TRealloc(int s)
{
	debugassert( s > 0 ," !s>0 in TRealloc");
	debugassert( s >= ArrayNum ,"!s >= ArrayNum in TRealloc");
	debugassert( ArrayMax >= ArrayNum,"!ArrayMax >= ArrayNum in TRealloc" );

	if( 1 )// new/delete method
	{
		BYTE* OldData = (BYTE*) Data;
		INT OldSize = ArrayMax; // old ArrayMax may well be 0.

		ArrayMax = s;

		// DUMB allocation/deletion - we don't want any destructors or constructors to be called, not of
		// classes in the arrays and certainly not of inner TArrays - since we'll be copying over raw data
		// instead. We DO allow ctors/dtors everywhere else.

		Data = (Type*) new BYTE[ ArrayMax * sizeof(Type) ];

		debugassert(Data,"! Data zero in TRealloc");

		if( OldData != NULL )
		{
			//  Don't copy per element - risks special operator= interfering with byte-for-byte copying
			memcpy( (BYTE*)Data, (BYTE*)OldData, ArrayNum * sizeof(Type) );

			// Delete it as a raw byte pointer.
			delete OldData;
		}
		else
		{
			MemLog.LogfLn(" First allocation for data ptr at [%i] is %i elements of size %i.",(BYTE*)Data, ArrayMax, sizeof(Type) );
		}
	}

	if( 0 ) // Realloc method - less wasteful of cpu and memory  ? -> defective.
	{
		BYTE* OldData = (BYTE*) Data;
		INT OldSize = ArrayMax; // old ArrayMax may well be 0.
		ArrayMax = s;	// New size.

		if( OldSize == 0 ) // Initial allocation.
		{
			if( OldData )
				delete OldData;
			Data = NULL;
			OldData = NULL;

			realloc( (BYTE*)Data, ArrayMax * sizeof(Type) );
			//Data = (Type*) new BYTE[ ArrayMax*sizeof(Type) ];
		}
		else if( (Data != NULL) && ( ArrayMax > 0 ) )
		{
			realloc( (BYTE*)Data, ArrayMax * sizeof(Type) );
		}
		else if ( ArrayMax == 0 && ArrayNum == 0 )
		{
			// Nothing in array. Delete without dtors.
			if( OldData )
				delete OldData;
		}
		else debugassert(0,"Realloc anomaly");
	}
}

//
// Array operator,   "new(TArrayToAddTo)ClassConstructor()"
//
template <class T> void* operator new( size_t Size, TArray<T>& Array )
{
	INT Index = Array.AddZeroed(1);
	return &Array[Index];
}

/*
template <class T> void* operator new( size_t Size, TArray<T>& Array, INT Index )
{
	Array.Insert(Index,1,sizeof(T));
	return &Array(Index);
}
*/


#endif
