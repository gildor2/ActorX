// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

/**********************************************************************

 	Win32IO.cpp misc IO & dialog functions

	Created by Erik de Neve

**********************************************************************/

#include "Vertebrate.h"
#include "ActorX.h"

int _GetCheckBox( HWND hWnd, int CheckID )
{
	HWND HBox = GetDlgItem(hWnd, CheckID);
	int TempResult = ( (SendMessage( HBox, BM_GETCHECK, 0 , 0 )==BST_CHECKED) ? 1 : 0 ) ;
	//PopupBox("GetCheckbox: %i",TempResult);
	return TempResult;
}

int _SetCheckBox( HWND hWnd,int CheckID, int Switch ) // Switch: 0 off, 1 marked, 2 greyed...
{
	HWND HBox = GetDlgItem(hWnd, CheckID);
	//PopupBox("SetCheckbox: %i",Switch);
	return( SendMessage( HBox, BM_SETCHECK, Switch, 0 ) );
}

int _EnableCheckBox( HWND hWnd, int CheckID, int Switch )
{
	HWND HBox = GetDlgItem(hWnd, CheckID);
	return( SendMessage( HBox, BM_SETCHECK, Switch ? BST_UNCHECKED:BST_INDETERMINATE, 0 ) );
}

int GetNameFromPath(TCHAR* Dest, const TCHAR* Src, int MaxChars )
{
	// Process filename: get position of last "\" or "/" and . then copy what's between.
	const TCHAR* ChStart  = _tcsrchr(Src,char(92));    // \ 92
	const TCHAR* ChStart2 = _tcsrchr(Src,char(47));    // / 47
	const TCHAR* ChEnd    = _tcsrchr(Src,char(46));    // . 46
	if( ChStart < ChStart2) ChStart = ChStart2;

	if( !ChEnd || !ChStart)
	{
		Dest[0] = 0;
		return 0;
	}

	INT NameEnd   = ChEnd   - Src;
	INT NameStart = ChStart - Src;

	INT ChunkSize = (NameEnd-NameStart-1);
	if( ChunkSize >= MaxChars )
		ChunkSize = MaxChars > 0 ? MaxChars-1 : 0;

	INT t;
	for( t=0;t<ChunkSize; t++)
	{
		Dest[t] = Src[t+NameStart+1];
	}
	Dest[t] = 0;

	return ( int )_tcslen(Dest);
}

int GetFolderFromPath(char* Dest, const char* Src, int MaxChars)
{
	// Process filename: get position of last "\" or "/" and . then copy what's between.
	char* ChStart  = (char*)strrchr(Src,char(92));    // \ 92
	char* ChStart2 = (char*)strrchr(Src,char(47));    // / 47
	if( ChStart < ChStart2) ChStart = ChStart2;

	if( !ChStart)
	{
		Dest[0] = 0;
		return 0;
	}

	INT NameStart = ( ChStart - Src) ;

	if( NameStart >= MaxChars )
		NameStart = MaxChars > 0 ? MaxChars-1 : 0;

	INT t;
	for( t=0; t<NameStart; t++ )
	{
		Dest[t] = Src[t];
	}

	Dest[t] = 0;
	return ( int )strlen(Dest);
}


int GetExtensionFromPath(char* Dest, const char* Src, int MaxChars )
{
	char* ChEnd    = (char*)strrchr(Src,char(0));
	char* ChStart  = (char*)strrchr(Src,char(46));
	if( !ChEnd )
	{
		Dest[0] = 0;
		return 0;
	}

	INT NameEnd   = ChEnd - Src;
	INT NameStart = ChStart - Src;

	INT ChunkSize = (NameEnd-NameStart-1);
	if( ChunkSize >= MaxChars )
		ChunkSize = MaxChars > 0 ? MaxChars-1 : 0;

	INT t;
	for( t=0; t<ChunkSize; t++)
	{
		Dest[t] = Src[t+NameStart+1];
	}

	Dest[t] = 0;
	return ( int )strlen(Dest);
}

// Truncate or pad string with spaces to become Size.
int ResizeString(char* Src, int Size)
{
	int Length = ( int )strlen(Src);
	if( Length > Size)
	{
		Src[Size] = 0;
	}
	else
	if( (Length < Size) && (Size < MAX_PATH ) )
	{
		for( int t= Length; t<Size; t++)
		{
			Src[t] = 32;
		}
		Src[Size] = 0;
	}
	return 1;
}

// Copy a string up to MaxChars, zero-terminate if necessary. MaxChars includes the zero terminator.
int strcpysafe(TCHAR* Dest, const TCHAR* Src, int MaxChars )
{
	INT CopySize = ( INT )_tcslen(Src);
	if( CopySize >= MaxChars )
	{
		CopySize = MaxChars <= 0 ? 0 : MaxChars-1;
	}
	for( INT c=0; c<CopySize; c++ )
	{
		Dest[c] = Src[c];
	}
	Dest[CopySize]=0;
	return CopySize; //return the number of characters excluding terminator.
}


int appGetVarArgs( char* Dest, INT Count, const char* Fmt, va_list ArgPtr )
{
	INT Result = _vsnprintf( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}

#if _UNICODE
int appGetVarArgs( TCHAR* Dest, INT Count, const TCHAR* Fmt, va_list ArgPtr )
{
	INT Result = _vsntprintf( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}
#endif

int PrintWindowNum(HWND hWnd,int IDC, int Num)
{
	TCHAR strbuf[80];
	int CharCount = _stprintf(strbuf, _T("%i"), Num);
	SetWindowText(GetDlgItem(hWnd,IDC),strbuf );
	return CharCount;
};

int PrintWindowNum(HWND hWnd,int IDC, FLOAT Num)
{
	TCHAR strbuf[80];
	int CharCount = _stprintf(strbuf, _T("%5.3f"), Num);
	SetWindowText(GetDlgItem(hWnd,IDC),strbuf );
	return CharCount;
};

int PrintWindowString(HWND hWnd,int IDC, TCHAR* String)
{
	// char strbuf[80];
	// int CharCount = sprintf(strbuf, "%i", Num);
	SetWindowText(GetDlgItem(hWnd,IDC), String );
	return 1;
};


void PopupBox(TCHAR* PrintboxString, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr,4096,PrintboxString,PrintboxString);
	MessageBox(GetActiveWindow(),TempStr, _T(" Note: "), MB_OK);
}

void WarningBox(TCHAR* PrintboxString, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr,4096,PrintboxString,PrintboxString);
	MessageBox(GetActiveWindow(),TempStr, _T(" Warning: "), MB_OK);
}

void ErrorBox(TCHAR* PrintboxString, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr,4096,PrintboxString,PrintboxString);
	MessageBox(GetActiveWindow(),TempStr, _T(" Error: "), MB_OK);
}

void DebugBox(char* PrintboxString, ... )
{
	if (!DEBUGMSGS) return;
	char TempStr[4096];
	GET_VARARGS(TempStr,4096,PrintboxString,PrintboxString);
	MessageBoxA(GetActiveWindow(),TempStr, " Debug: ", MB_OK);
}

#if _UNICODE
void DebugBox(TCHAR* PrintboxString, ... )
{
	if (!DEBUGMSGS) return;
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr,4096,PrintboxString,PrintboxString);
	MessageBox(GetActiveWindow(),TempStr, _T(" Debug: "), MB_OK);
}
#endif

void GetBatchFileName(HWND hWnd, TCHAR* filename, TCHAR* workpath )
{
	static TCHAR filterlist[] = _T("txt Files (*.txt)\0*.txt\0txt Files (*.txt)\0*.txt\0");
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));

	TCHAR tempfilename[MAXPATHCHARS];
	tempfilename[0] = 0;
	ofn.lpstrFile		= tempfilename;
	ofn.nMaxFile		= MAXPATHCHARS;

	ofn.lpstrFilter		= filterlist;
	ofn.lpstrTitle		= _T("Load batch file: ");
	ofn.Flags			= OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	ofn.lStructSize		= sizeof(OPENFILENAME);
	ofn.hwndOwner		= hWnd;

	if (workpath[0])
		ofn.lpstrInitialDir	= workpath;
	else
		ofn.lpstrInitialDir = _T("");
	/*
	// Max-specific...
	else
		ofn.lpstrInitialDir	= ip->GetMapDir(0);
	*/

	while (1) {

		if (GetOpenFileName(&ofn)) {

			//-- Make sure there is an extension ----------
			/*
			int l = _tcslen(ofn.lpstrFile);
			if (!l)
				return;

			if (l==ofn.nFileExtension || !ofn.nFileExtension)
			_tcscat(ofn.lpstrFile,(".txt"));
			*/
		}
		break;

	}

	_tcscpy(filename,ofn.lpstrFile);
}


static TCHAR Skeletalfilterlist[] = _T("PSA Files (*.psa)\0*.psa\0PSA Files (*.psa)\0*.psa\0");

bool GetSaveName(HWND hWnd, TCHAR* filename, size_t filenameLen, TCHAR* workpath, const TCHAR* filterlist, TCHAR* defaultextension )
{
	if( filterlist == NULL )
		filterlist = &Skeletalfilterlist[0];

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));

	TCHAR tempfilename[MAX_PATH];
	tempfilename[0] = 0;
	ofn.lpstrFile		= tempfilename;
	ofn.nMaxFile		= MAX_PATH;

	ofn.lpstrFilter		= filterlist;
	ofn.lpstrTitle		= _T("Save file As");
	ofn.Flags			= OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	ofn.lStructSize		= sizeof(OPENFILENAME);
	ofn.hwndOwner		= hWnd;

	if (workpath[0])
		ofn.lpstrInitialDir	= workpath;
	else
		ofn.lpstrInitialDir = _T("");
	/*
	// Max-specific...
	else
		ofn.lpstrInitialDir	= ip->GetMapDir(0);
	*/

	while (1) {

		if (GetSaveFileName(&ofn))
		{

			// Refuse empty filename.
			int FileNameLength = ( int )_tcslen(ofn.lpstrFile);
			if (FileNameLength == 0)
				return false;

			//-- Make sure there is an extension ----------
			if (1==ofn.nFileExtension || 0==ofn.nFileExtension)
			_tcscat(ofn.lpstrFile,defaultextension );

			//-- Check for file overwrite -----------------
			// Does it exist ? -> for now ignore overwriting.
			if (0) // (BMMIsFile(ofn.lpstrFile))
			{
				TCHAR text[MAX_PATH];
				wsprintf(text,_T("Overwrite ? "),ofn.lpstrFile);

				TCHAR* tit = _T("Overwrite ? "); //GetString(IDS_OVERWRITE_TIT);
				if (MessageBox(hWnd,text,tit,MB_APPLMODAL | MB_ICONQUESTION | MB_YESNO) != IDYES)
				{
					ofn.lpstrFile[0]=0;
					continue;
				}
			}
		}
		else
			return false;
		break;

	}
	_tcscpy_s(filename, filenameLen, ofn.lpstrFile);
	return true;
}


bool GetLoadName(HWND hWnd, TCHAR* filename, size_t filenameLen, TCHAR* workpath, const TCHAR* filterlist )
{

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAMEA));

	TCHAR tempfilename[MAX_PATH];
	tempfilename[0] = 0;
	ofn.lpstrFile		= tempfilename;
	ofn.nMaxFile		= MAX_PATH;

	ofn.lpstrFilter		= filterlist;
	ofn.lpstrTitle		= _T("Load file from :");
	ofn.Flags			= OFN_HIDEREADONLY | OFN_NOCHANGEDIR; //OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	ofn.lStructSize		= sizeof(OPENFILENAMEA);
	ofn.hwndOwner		= hWnd;

	if (workpath[0])
		ofn.lpstrInitialDir	= workpath;
	else
		ofn.lpstrInitialDir = _T("");
	/*
	// Max-specific...
	else
		ofn.lpstrInitialDir	= ip->GetMapDir(0);
	*/

	if (GetOpenFileName(&ofn))
	{
		//-- Make sure there is an extension ----------
		int l = ( int )_tcslen(ofn.lpstrFile);
		if (!l)
			return false;
		if (l==ofn.nFileExtension || !ofn.nFileExtension)
		_tcscat(ofn.lpstrFile,_T(".psa"));
		_tcscpy_s(filename, filenameLen, ofn.lpstrFile);
		return true;
	}

	return false;
}

int GetFolderName(HWND hWnd, TCHAR* PathResult, size_t resultLen)
{
    LPMALLOC pMalloc;
    /* Gets the Shell's default allocator */
    if (::SHGetMalloc(&pMalloc) == NOERROR)
    {
        BROWSEINFO bi;
        TCHAR pszBuffer[MAX_PATH];
        LPITEMIDLIST pidl;
        // Get help on BROWSEINFO struct - it's got all the bit settings.
        bi.hwndOwner = hWnd;
        bi.pidlRoot = NULL;
        bi.pszDisplayName = pszBuffer;
        bi.lpszTitle = _T("Select a folder:");
        bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
        bi.lpfn = NULL;
        bi.lParam = 0;
        // This next call issues the dialog box.
        if ((pidl = ::SHBrowseForFolder(&bi)) != NULL)
        {
            if (::SHGetPathFromIDList(pidl, pszBuffer))
            {
            // At this point pszBuffer contains the selected path */.
                _tcscpy_s(PathResult, resultLen, pszBuffer); //DoingSomethingUseful(pszBuffer);
				return 1;
            }
            // Free the PIDL allocated by SHBrowseForFolder.
            pMalloc->Free(pidl);
        }
        // Release the shell's allocator.
        pMalloc->Release();
    }
	return 0;
}
