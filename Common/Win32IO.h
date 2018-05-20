// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**********************************************************************

	Win32IO.h   Misc file, logging & dialog functions.

	Created by Erik de Neve

**********************************************************************/

#ifndef __WIN32IO__H
#define __WIN32IO__H

// System includes.
#include <tchar.h>
#include <windows.h>
#include <shlobj.h>    // shell open box etc

// Standard includes.
#include <stdio.h>
#include <assert.h>

#define ARRAY_ARG(array)		array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)		(sizeof(array)/sizeof(array[0]))

// General includes.
#include "UnSkeletal.h"

// String support.
int GetNameFromPath(TCHAR* Dest, const TCHAR* Src, int MaxChars );
int GetExtensionFromPath(char* Dest, const char* Src, int MaxChars );
int GetFolderFromPath(char* Dest, const char* Src, int MaxChars );
int strcpysafe(TCHAR* Dest, const TCHAR* Src, int MaxChars );
int ResizeString(char* Src, int Size);

// Forwards.
INT appGetVarArgs( char* Dest, INT Count, const char* Fmt, va_list ArgPtr );
#if _UNICODE
INT appGetVarArgs( TCHAR* Dest, INT Count, const TCHAR* Fmt, va_list ArgPtr );
#endif
#define GET_VARARGS(msg,len,lastarg,fmt) {va_list ap; va_start(ap,lastarg);appGetVarArgs(msg,len,fmt,ap);}

int  PrintWindowNum(HWND hWnd,int IDC, int Num);
int  PrintWindowNum(HWND hWnd,int IDC, FLOAT Num);
int  PrintWindowString(HWND hWnd,int IDC, TCHAR* String);

void ErrorBox(TCHAR* PrintboxString, ... );
void WarningBox(TCHAR* PrintboxString, ... );
void PopupBox(TCHAR* PrintboxString, ... );
void DebugBox(char* PrintboxString, ... );
#if _UNICODE
void DebugBox(TCHAR* PrintboxString, ... );
#endif

int _GetCheckBox( HWND hWnd, int CheckID );
int _SetCheckBox( HWND hWnd, int CheckID, int Switch );
int _EnableCheckBox( HWND hWnd, int CheckID, int Switch );

int  GetFolderName(HWND hWnd, TCHAR* PathResult, size_t resultLen);
bool GetLoadName(HWND hWnd, TCHAR* filename, size_t filenameLen, TCHAR* workpath, const TCHAR* filterlist );
bool GetSaveName(HWND hWnd, TCHAR* filename, size_t filenameLen, TCHAR* workpath, const TCHAR* filterList, TCHAR* defaultextension );
void GetBatchFileName(HWND hWnd, TCHAR* filename, TCHAR* workpath );


inline void Memzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}

// Simple log file support.

class TextFile
{
	public:
	INT Tabs;
	FILE* LStream;
	INT LastError;
	UBOOL ObeyTabs;

	int Open(const TCHAR* LogToPath, const TCHAR* LogName, int Enabled)
	{
		if( Enabled && (LogName[0] != 0) )
		{
			TCHAR LogFileName[MAX_PATH];
			LogFileName[0] = 0;
			Tabs = 0;
			if( LogToPath ) _tcscat( LogFileName, LogToPath );
			if( LogName )   _tcscat( LogFileName, LogName ); // May contain path.
			LStream = _tfopen(LogFileName,_T("wt")); // Open for writing.
			if (!LStream)
			{
				return 0;
			}
			return 1;
		}
		return 0;
	}

	void Logf(const char* LogString, ... )
	{
		if( ObeyTabs ) doTabs();
		char TempStr[4096];
		GET_VARARGS(TempStr,4096,LogString,LogString);
		if( LStream )
		{
			fprintf(LStream, "%s", TempStr);
		}
	}

#if _UNICODE
	void Logf(const TCHAR* LogString, ... )
	{
		if( ObeyTabs ) doTabs();
		TCHAR TempStr[4096];
		GET_VARARGS(TempStr,4096,LogString,LogString);
		char TempStr2[4096];
		wcstombs(TempStr2, TempStr, 4096);
		if( LStream )
		{
			fprintf(LStream, "%s", TempStr);
		}
	}
#endif

	void LogfLn(const char* LogString, ... )
	{
		if( LStream )
		{
			if( ObeyTabs ) doTabs();
			char TempStr[4096];
			GET_VARARGS(TempStr,4096,LogString,LogString);
			fprintf(LStream, "%s\n", TempStr);
		}
	}

#if _UNICODE
	void LogfLn(const TCHAR* LogString, ... )
	{
		if( LStream )
		{
			if( ObeyTabs ) doTabs();
			TCHAR TempStr[4096];
			GET_VARARGS(TempStr,4096,LogString,LogString);
			char TempStr2[4096];
			wcstombs(TempStr2, TempStr, 4096);
			fprintf(LStream, "%s\n", TempStr2);
		}
	}
#endif

	void doTabs()
	{
		if( LStream )
		{
			for (INT i =0; i < Tabs; i++ )
			{
				_ftprintf(LStream, _T(" "));
			}
		}
	}

	int Error()
	{
		return ferror(LStream);
	}

	void doCR()
	{
		if ( LStream )
		{
			_ftprintf(LStream,_T("\r\n"));
		}
	}

	int Close()
	{
		if( LStream )
		{
			fclose(LStream);
			LStream = NULL;
			return 1;
		}
		return 0;
	}

	TextFile()
	{
		LStream = NULL;
		Tabs = 0;
		ObeyTabs = false;
	}

	~TextFile()
	{
		if( LStream )
		Close();
	}

};



//
// Quick-and-dirty simple file class. Reading is unbuffered. Fast buffered binary file writing.
//
class FastFileClass
{
private:

	HANDLE  FileHandle;
	BYTE*	Buffer;
	int		BufCount;
	int     Error;
	int     ReadPos;
	unsigned long BytesWritten;
	#define	BufSize (8192)

public:
	// Ctor
	FastFileClass()
	{
		Buffer = NULL;
		BufCount = 0;
		Error = 0;
		ReadPos = 0;
		Buffer = new BYTE[BufSize];
	}

	~FastFileClass()
	{
		if( Buffer )delete Buffer;
	}

	int CreateNewFile(TCHAR *FileName)
	{
		Error = 0;
		if ((FileHandle = CreateFile(FileName, GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))
		    == INVALID_HANDLE_VALUE)
		{
			Error = 1;
		}

		return Error;
	}

	void Write(void *Source,int ByteCount)
	{
		if ((BufCount+ByteCount) < BufSize)
		{
			memmove(&Buffer[BufCount],Source,ByteCount);
			BufCount += ByteCount;
		}
		else
		if (ByteCount <= BufSize)
		{
			if (BufCount !=0)
			{
				WriteFile(FileHandle,Buffer,BufCount,&BytesWritten, NULL);
				BufCount = 0;
			}
			memmove(Buffer,Source,ByteCount);
			BufCount += ByteCount;
		}
		else // Too big a chunk to be buffered.
		{
			if( BufCount )
			{
				WriteFile(FileHandle,Buffer,BufCount,&BytesWritten,NULL);
				BufCount = 0;
			}
			WriteFile(FileHandle,Source,ByteCount,&BytesWritten,NULL);
		}
	}

	// Write one byte. Flush when closed...
	inline void WriteByte(const BYTE OutChar)
	{
		if (BufCount < BufSize)
		{
			Buffer[BufCount] = OutChar;
			BufCount++;
			return;
		}
		else
		{
			BYTE OutExtra = OutChar;
			Write(&OutExtra, 1);
		}
	}

	//
	//int Read(int ByteCount, BYTE* Destination)
	//{
	//}
	//

	// Fast string writing.
	inline void Print(char* OutString, ... )
	{
		char TempStr[4096];
		GET_VARARGS(TempStr,4096,OutString,OutString);
		Write(&TempStr, ( int )strlen(TempStr));
	}

#if _UNICODE
	inline void Print(TCHAR* OutString, ... )
	{
		TCHAR TempStr[4096];
		GET_VARARGS(TempStr,4096,OutString,OutString);
		char TempStr2[4096];
		wcstombs(TempStr2, TempStr, 4096);
		Write(&TempStr, ( int )strlen(TempStr2));
	}
#endif

	int GetError()
	{
		return Error;
	}

	// Close for reading.
	int Close()
	{
		CloseHandle(FileHandle);
		return Error;
	}

	// Flush for writing.
	int Flush()
	{
		Error = 0;
		// flush
		if( BufCount )
		{
			WriteFile(FileHandle, Buffer, BufCount, &BytesWritten, NULL);
			BufCount = 0;
		}
		return Error;
	}

	// Close for writing.
	int CloseFlush()
	{
		ReadPos = 0;
		// flush
		if( BufCount )
		{
			WriteFile(FileHandle, Buffer, BufCount, &BytesWritten, NULL);
			BufCount = 0;
		}
		CloseHandle(FileHandle);
		return Error;
	}

	int OpenExistingFile(TCHAR *FileName)
	{
		ReadPos = 0;
		Error = 0;
		//Note: will return with error if file is write-protected.
		if ((FileHandle = CreateFile(FileName, GENERIC_WRITE | GENERIC_READ , FILE_SHARE_WRITE,NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
		    == INVALID_HANDLE_VALUE)
		{
			Error = 1;
		}
		return Error;
	}

	// Open for read only: will succeeed with write-protected files.
	int OpenExistingFileForReading(TCHAR *FileName)
	{
		ReadPos = 0;
		Error = 0;

		if ((FileHandle = CreateFile(FileName, GENERIC_READ , FILE_SHARE_WRITE,NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
		    == INVALID_HANDLE_VALUE)
		{
			Error = 1;
		}
		return Error;
	}

	// Get current size of a file opened for reading/writing...
	int GetSize()
	{
		DWORD Size;
		Size = GetFileSize(FileHandle, NULL);
		if( Size == 0xFFFFFFFF ) Size = 0;
		return (INT)Size;
 	}

	// Directly maps onto windows' filereading. Return number of bytes read.
	int Read(void *Dest,int ByteCount) // int InFilePos ? )
	{
		ULONG BytesRead = 0;

		if (! ReadFile( FileHandle, Dest, ByteCount, &BytesRead, NULL) )
			BytesRead = 0;

		ReadPos +=BytesRead;

		return BytesRead;
	}

	int Seek( INT InPos )
	{
		assert(InPos>=0);
		//assert(InPos<=Size);

		if( SetFilePointer( FileHandle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
		{
			return 0;
		}
		ReadPos     = InPos;
		return InPos;
	}

	// Quick and dirty read-from-position..
	int ReadFromPos(void *Dest, int ByteCount, int Position)
	{
		Seek( Position );
		Read( Dest, ByteCount);
	}

};


//
// Simple registry interface for strings and ints.
// a clone of the registry code in the 3ds2unr
// exporter by Mike Fox/Legend Entertainment.
//

class WinRegistry
{

public:

    HKEY    hKey;
	TCHAR   RegPath[400];

	WinRegistry()
	{
		hKey = NULL;
	}

	// Set path
	void SetRegistryPath( TCHAR* NewPath)
	{
		strcpysafe( RegPath, NewPath, 400 );
	}

	void SetKeyString( TCHAR* KeyName, TCHAR* ValString )
	{
		DWORD ValSize = ( DWORD )strlen( (char*)ValString ) + 1;

		DWORD Res;
		LONG KeyError;
		KeyError = ::RegCreateKeyEx( HKEY_CURRENT_USER, RegPath, 0L, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &hKey, &Res );
		if( KeyError == ERROR_SUCCESS )
		{
			::RegSetValueEx( hKey, KeyName, 0L, REG_SZ, (CONST BYTE*)ValString, ValSize ); //#DEBUG size
			::RegCloseKey( hKey );
			hKey = 0;
		}
		else
		{
			//PopupBox("SetKeyError: %i", KeyError);
		}
	}

	void GetKeyString( TCHAR* KeyName, TCHAR* ValString )
	{
		*(char*)ValString = '\0';

		LONG  KeyError;
		KeyError = ::RegOpenKeyEx( HKEY_CURRENT_USER, RegPath, 0L, KEY_READ, &hKey );
		if( KeyError == ERROR_SUCCESS )
		{
			DWORD Type;
			DWORD DataLen = 300; //strlen( (char*)ValString )+1;
			::RegQueryValueEx( hKey,
				               KeyName,
				               0,
				               &Type,
							   (byte*) ValString, //reinterpret_cast<unsigned char*>( ValString ),
							   &DataLen );
			::RegCloseKey( hKey );
			hKey = 0;

			//PopupBox("string length: %i datalen: %i string: [%s]", strlen((char*)ValString), DataLen, ValString);
		}
		else
		{
			//PopupBox("GetKeyError: %i",KeyError);
		}
	}

	void SetKeyValue( TCHAR* KeyName, DWORD Value ) // set any 32-bit value...
	{
		DWORD Res;
		LONG KeyError;
		DWORD Val = Value;
		KeyError = ::RegCreateKeyEx( HKEY_CURRENT_USER, RegPath, 0L, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &hKey, &Res );
		if( KeyError == ERROR_SUCCESS )
		{
			::RegSetValueEx( hKey, KeyName, 0L, REG_DWORD, (CONST BYTE*)&Val, sizeof(DWORD) ); //#DEBUG size
			::RegCloseKey( hKey );
			hKey = 0;
		}
		else
		{
			//PopupBox("SetKeyError: %i",KeyError);
		}
	}

	void SetKeyValue( TCHAR* KeyName, FLOAT FloatValue ) // FLOAT overload
	{
		SetKeyValue( KeyName, *((DWORD*)&FloatValue) );
	}

	void SetKeyValue( TCHAR* KeyName, UBOOL UBOOLValue ) // UBOOL overload
	{
		SetKeyValue( KeyName, *((DWORD*)&UBOOLValue) );
	}

	void GetKeyValue( TCHAR* KeyName, INT& Value )    // set any 32-bit value...
	{

		Value = 0;
		LONG  KeyError;
		KeyError = ::RegOpenKeyEx( HKEY_CURRENT_USER, RegPath, 0L, KEY_READ, &hKey );
		if( KeyError == ERROR_SUCCESS )
		{
			DWORD Type;
			DWORD DataLen = sizeof(DWORD);
			::RegQueryValueEx( hKey,
				               KeyName,
				               0,
				               &Type,
							   (byte*) &Value,
							   &DataLen );
			::RegCloseKey( hKey );
			hKey = 0;
		}
		else
		{
			//PopupBox("GetKeyError: %i",KeyError);
		}
	}

	void GetKeyValue( TCHAR* KeyName, FLOAT& FloatValue ) // FLOAT overload
	{
		GetKeyValue( KeyName, *((INT*)&FloatValue) );
	}

	void GetKeyValue( TCHAR* KeyName, UBOOL& UBOOLValue ) // UBOOL overload
	{
		GetKeyValue( KeyName, *((INT*)&UBOOLValue) );
	}


	// Remove settings
	void DeleteRegistryKey( const TCHAR* KeyName )
	{
		::RegDeleteKey( hKey, KeyName );
		::RegCloseKey( hKey );
	}

};



#endif // __WIN32IO__H