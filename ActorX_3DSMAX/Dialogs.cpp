/**************************************************************************

	Dialogs.cpp - Host-independent exporter windows interface .

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
	Created by Erik de Neve

   TODO
   - move all non-GUI (SceneIFC) stuff to SceneIFC...

***************************************************************************/

// Local includes
#include "ActorX.h"
#include "SceneIFC.h"
#include ".\res\resource.h"


INT_PTR CALLBACK SceneInfoDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
INT_PTR CALLBACK ActorManagerDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
//INT_PTR CALLBACK BrushPanelDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );


/*

// The bone tree selection popup window.
static INT_PTR CALLBACK BoneTreeDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	UBOOL ReturnWnd = true;
	//HWND  hwndList  = GetDlgItem(hWnd, IDC_TREE1);

	switch (msg) 
	{
		//
		// Initialization : just fill tree once with the current skeleton.
		//
		case WM_INITDIALOG:
		{
			CenterWindow(hWnd, GetParent(hWnd)); 
			// PrintWindowString(hWnd, IDC_PACKAGENAME, stringvar );
			// Initialize the tree control to allow single-bone selection 
		}
		break;

		// Commands.
		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				case IDOK:
					EndDialog(hWnd, 1);
				break;
			}
		break;

		default:
			ReturnWnd = false;
	}
	return ReturnWnd;
}

void ShowBoneTree(HWND hWnd)
{
	DialogBoxParam( hInstance, MAKEINTRESOURCE(IDD_BONETREE), hWnd, BoneTreeDlgProc, 0);
}

*/


static int comparez=0;
int AnimCompare( const void *arg1, const void *arg2 )
{
   /* Compare all of both VAnimation strings: */
	comparez++;
	INT Compare = 0;

	//Name: ANSICHAR Name[64];
	INT C=0;
	while( C<64 )
	{
		ANSICHAR Ch1,Ch2;
		Ch1=(*(VAnimation*)arg1).AnimInfo.Name[C];
		Ch2=(*(VAnimation*)arg2).AnimInfo.Name[C];
		if( Ch1 < Ch2 )
		{
			Compare = -1;
			break;
		}
		if( Ch1 > Ch2 )
		{
			Compare = 1;
			break;
		}
		C++;
	}

	return Compare;
}



//
// Save the animation set to DestPath (complete path+filename)
//

UBOOL SaveAnimSet( char* DestPath )
{
	if ( TempActor.OutAnims.Num() == 0)
	{
		PopupBox(" No stored animations available. ");
		return false;
	}
	else
	{											
		FastFileClass OutFile;
		if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
		{
			ErrorBox("AnimSet File creation error for [%s]",DestPath);
			return false;
		}
		else
		{
			// PopupBox(" Bones in first track: %i",TempActor.RefSkeletonBones.Num());
			TempActor.SerializeAnimation(OutFile);
			OutFile.CloseFlush();
			if( OutFile.GetError())
			{
				ErrorBox("Animation Save Error.");
				return false;
			}
		}

		INT WrittenBones = TempActor.RefSkeletonBones.Num();

		if( OurScene.DoLog )
		{
			OurScene.LogAnimInfo(&TempActor, to_animfile );
		}

		if( !OurScene.DoSuppressAnimPopups )
		{
			PopupBox( "OK: Animation file %s.PSA written. Bones total: %i  Sequences: %i", to_animfile, WrittenBones, TempActor.OutAnims.Num() );				
		}
	}

	return true;
}




// The animation manager window stuff

INT_PTR CALLBACK ActorManagerDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	/*
	Listbox: An application should monitor and process the following list box notification messages. 
	Notification message Description 
	LBN_DBLCLK The user double-clicks an item in the list box. 
	LBN_ERRSPACE The list box cannot allocate enough memory to fulfill a request. 
	LBN_KILLFOCUS The list box loses the keyboard focus. 
	LBN_SELCANCEL The user cancels the selection of an item in the list box. 
	LBN_SELCHANGE The selection in a list box is about to change. 
	LBN_SETFOCUS The list box receives the keyboard focus. 
	*/		

    HWND hwndList; 
	UBOOL ReturnWnd = true;
	UBOOL UpdateAnimParams = false;

	UBOOL UpdateInBox  = false;
	UBOOL UpdateOutBox = false;
	UBOOL DoDeleteIn   = false;
	UBOOL DoDeleteOut  = false;

	UBOOL UpdateStartBone = false;
	UBOOL UpdateRate = false;
	UBOOL UpdateName = false;
	UBOOL UpdateKeyReduction = false;
	UBOOL UpdateGroup = false;
	UBOOL UpdateCheckRoot = false;

	INT InSelector = 0;
	INT InSelBufferNum = 0;
	INT InSelBuffer[1024];
	INT OutSelBufferNum = 0;
	INT OutSelBuffer[1024];

	
	// Refresh in-list/out-list selections.
	hwndList = GetDlgItem(hWnd, IDC_LISTOUT);
	OutSelBufferNum = SendMessage(hwndList, LB_GETSELITEMS, 512, (LPARAM) OutSelBuffer); 

	// Multiple selection: only if one is selected will we have i > -1.
	hwndList = GetDlgItem(hWnd, IDC_LISTIN);
	InSelBufferNum = SendMessage(hwndList, LB_GETSELITEMS, 512, (LPARAM) InSelBuffer); 
	if( InSelBufferNum >= 1)
	{
		InSelector = InSelBuffer[0];
	}

	// Main message switch for Actor Manager dialog
	switch (msg) 
	{
		// Initialization time
		case WM_INITDIALOG:
		{
			
			

			//INITIALIZE
			UpdateInBox = true;
			UpdateOutBox = true;
			//     Center window on init only.				
			// hwndList = GetDlgItem(hWnd, IDC_LISTIN);
			//CenterWindow(hWnd, GetParent(hWnd)); 

			PrintWindowString(hWnd, IDC_PACKAGENAME, to_animfile );

			switch (LOWORD(wParam)) 
			{
				case IDC_LISTIN: 
				{					
				}
				break;

				case IDC_LISTOUT:
				{					
				}
				break;
			}
		}
		break;

		case WM_VSCROLL:
		{
			//PopupBox("VScroll");
			HWND hwndScrollBar = (HWND)lParam;      // handle to scroll bar 
			//PopupBox("# hwnd%i shuf:[%i]  switch: %i  SBLineup: [%i] npos: %i", (INT)hwndScrollBar, (INT)IDC_OUTSHUF,(INT)LOWORD(wParam),(INT)SB_LINEUP,(short int)HIWORD(wParam));

			switch((int) LOWORD(wParam)) // scroll bar value 
			{
				case SB_LINEUP:
				{
					//PopupBox("Up, hwnd%i shuf:%i", (INT)hwndScrollBar, (INT)IDC_OUTSHUF);
				}
				break;
				case SB_LINEDOWN:
				{
					//PopupBox("Up, hwnd%i shuf:%i", (INT)hwndScrollBar, (INT)IDC_INSHUF);
				}
				break;
			}
		}	
		break;

		/*
		WM_VSCROLL 
		nScrollCode = (int) LOWORD(wParam); // scroll bar value 
		nPos = (short int) HIWORD(wParam);  // scroll box position 
		hwndScrollBar = (HWND) lParam;      // handle to scroll bar 
		*/
 
		// Commands
		case WM_COMMAND:
			switch( LOWORD(wParam) ) 
			{
				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				// Sense any focus loss in the animation-parameter update boxes to update the parameters.
				case IDC_EDITSTARTBONE:
				case IDC_EDITRATE:
				case IDC_EDITNAME:
				case IDC_EDITKEYREDUCTION:
				case IDC_EDITGROUP:
				case IDC_CHECKROOT:
				{
					switch (HIWORD(wParam))
					{		
					case EN_KILLFOCUS:
						{
							UpdateAnimParams=true;
							// Special multi-selection update tags:
							switch(LOWORD(wParam))
							{
								case IDC_EDITSTARTBONE:
									UpdateStartBone=true;
								break;
								case IDC_EDITRATE:
									UpdateRate=true;
								break;
								case IDC_EDITNAME:
									UpdateName=true;
								break;
								case IDC_EDITKEYREDUCTION:
									UpdateKeyReduction=true;
								break;
								case IDC_EDITGROUP:
									UpdateGroup=true;
								break;
								case IDC_CHECKROOT:
									UpdateCheckRoot=true;
								break;
							}
						}
						break;
					}
				}
				break;

				case IDC_LISTIN: 
				{
					// Set handle for easy access.
					hwndList = GetDlgItem( hWnd, IDC_LISTIN );
					switch( HIWORD(wParam) )
					{							
						case LBN_KILLFOCUS:
						case LBN_SELCANCEL:    // Only _SELCHANGE needed ??
						case LBN_SETFOCUS:
						case LBN_SELCHANGE:						
						{
							// UpdateInBox = true; 							

							// Simply print values if single one is selected.							

							if( InSelBufferNum )
							{
							
								// Uneditable stats: accumulate for multiple.
								INT NumRawFrames = 0;
								for( INT c=0; c<InSelBufferNum; c++)
								{
									INT AnimIdx=InSelBuffer[c];									
									NumRawFrames += TempActor.Animations[AnimIdx].AnimInfo.NumRawFrames;
								}
								PrintWindowNum( hWnd, IDC_ANIMFRAMES, NumRawFrames );
								
								PrintWindowNum( hWnd, IDC_TOTALSIZE, TempActor.Animations[InSelBuffer[0]].KeyTrack.Num() );

								PrintWindowNum( hWnd, IDC_TOTALSCALERS, TempActor.Animations[InSelBuffer[0]].ScaleTrack.Num() );

								// Animation seconds accumulated
								{								
									
									FLOAT TotalSeconds = 0.f;						
									for( INT c=0; c<InSelBufferNum; c++ )
									{
										INT AnimIdx=InSelBuffer[c];
										if( TempActor.Animations[AnimIdx].AnimInfo.AnimRate > 0.f )
											TotalSeconds += ( TempActor.Animations[AnimIdx].AnimInfo.NumRawFrames / TempActor.Animations[AnimIdx].AnimInfo.AnimRate );
									}
									PrintWindowNum( hWnd, IDC_ANIMSECONDS, TotalSeconds);
								}
								
								/*
								// Editable ones:
								PrintWindowNum( hWnd, IDC_EDITRATE, TempActor.Animations[InSelector].AnimInfo.AnimRate );
								PrintWindowNum( hWnd, IDC_EDITKEYREDUCTION, TempActor.Animations[InSelector].AnimInfo.KeyReduction );
								PrintWindowString( hWnd,IDC_EDITGROUP, TempActor.Animations[InSelector].AnimInfo.Group );
								PrintWindowNum( hWnd, IDC_EDITSTARTBONE, TempActor.Animations[InSelector].AnimInfo.StartBone );								
								PrintWindowString( hWnd,IDC_EDITNAME, TempActor.Animations[InSelector].AnimInfo.Name );
								_SetCheckBox( hWnd, IDC_CHECKROOT, TempActor.Animations[InSelector].AnimInfo.RootInclude );
								*/							

								// Kludgy elaborate way of detecting different property values for multi-selected animations.
								{
									char instr1[255];
									char instr2[255];

									UBOOL SameAnimGroups = true;
									for( INT c=0; c<InSelBufferNum; c++)
									{
										INT AnimIdx=InSelBuffer[c];									
										_tcscpy(instr1, TempActor.Animations[AnimIdx].AnimInfo.Group );									
										if( c>0 ) 
										{
											if( strcmp(instr1,instr2) != 0)
												SameAnimGroups = false;
										}
										_tcscpy(instr2,instr1);
									}								
									if( SameAnimGroups )
										PrintWindowString( hWnd,IDC_EDITGROUP, TempActor.Animations[InSelBuffer[0]].AnimInfo.Group );
									else
										PrintWindowString( hWnd,IDC_EDITGROUP,(""));
								}
								//////////			
								{
									char instr1[255];
									char instr2[255];    

									UBOOL SameAnimNames = true;
									for( INT c=0; c<InSelBufferNum; c++)
									{
										INT AnimIdx=InSelBuffer[c];									
										_tcscpy(instr1, TempActor.Animations[AnimIdx].AnimInfo.Name );									
										if( c>0 ) 
										{
											if( strcmp(instr1,instr2) != 0)
											{
												SameAnimNames = false;
												//PopupBox("Diffferent names: [%s] [%s]",instr1,instr2);
											}

										}
										_tcscpy(instr2,instr1);
									}
									if( SameAnimNames )
										PrintWindowString( hWnd, IDC_EDITNAME, TempActor.Animations[InSelBuffer[0]].AnimInfo.Name );
									else
										PrintWindowString( hWnd, IDC_EDITNAME,(""));
								}

								////////////////////////////////////////

								UBOOL SameAnimRates = true;
								{
									FLOAT Inf1=0.f,Inf2=0.f;
									for( INT c=0; c<InSelBufferNum; c++)
									{
										INT AnimIdx=InSelBuffer[c];									
										Inf1 = TempActor.Animations[AnimIdx].AnimInfo.AnimRate;									
										if( c>0 ) 
										{
											if( Inf1 != Inf2 )
											{
												SameAnimRates = false;												
											}
										}
										Inf2 = Inf1;
									}
								}
								if( SameAnimRates )
									PrintWindowNum( hWnd, IDC_EDITRATE, TempActor.Animations[InSelBuffer[0]].AnimInfo.AnimRate );
								else
									PrintWindowString( hWnd, IDC_EDITRATE,(""));

								//////////////////////////////
								UBOOL SameKeyReductions = true;								
								FLOAT Inf1=0.f,Inf2=0.f;
								for( INT c=0; c<InSelBufferNum; c++)
								{
									INT AnimIdx=InSelBuffer[c];									
									Inf1 = TempActor.Animations[AnimIdx].AnimInfo.KeyReduction;									
									if( c>0 ) 
									{
										if( Inf1 != Inf2 )
										{
											SameKeyReductions = false;												
										}
									}
									Inf2 = Inf1;
								}
								if( SameKeyReductions )
									PrintWindowNum( hWnd, IDC_EDITKEYREDUCTION, TempActor.Animations[InSelBuffer[0]].AnimInfo.KeyReduction );
								else
									PrintWindowString( hWnd, IDC_EDITKEYREDUCTION,(""));

								//////////////////////////////
								UBOOL SameStartBones = true;								
								{ INT Inf1=0,Inf2=0;
								for( INT c=0; c<InSelBufferNum; c++)
								{
									INT AnimIdx=InSelBuffer[c];									
									Inf1 = TempActor.Animations[AnimIdx].AnimInfo.StartBone;									
									if( c>0 ) 
									{
										if( Inf1 != Inf2 )
										{
											SameStartBones = false;												
										}
									}
									Inf2 = Inf1;
								}
								}

								if( SameStartBones )
									PrintWindowNum( hWnd, IDC_EDITSTARTBONE, TempActor.Animations[InSelBuffer[0]].AnimInfo.StartBone );
								else
									PrintWindowString( hWnd, IDC_EDITSTARTBONE,(""));

								//////////////////////////////
								/*
								UBOOL SameCheckRoots = true;								
								{ 
									UBOOL Inf1,Inf2;
									for( INT c=0; c<InSelBufferNum; c++)
									{
										INT AnimIdx=InSelBuffer[c];									
										Inf1 = TempActor.Animations[AnimIdx].AnimInfo.RootInclude;									
										if( c>0 ) 
										{
											if( Inf1 != Inf2 )
											{
												SameCheckRoots = false;
											}
										}
										Inf2 = Inf1;
									}
								}
								if( SameCheckRoots )
									_SetCheckBox( hWnd, IDC_CHECKROOT, TempActor.Animations[InSelBuffer[0]].AnimInfo.RootInclude );
								else
									_SetCheckBox( hWnd, IDC_CHECKROOT, 0 );
								*/
								///////////////////////
							}
							else
							{ 
								// None selected; clear everything.
								PrintWindowString( hWnd, IDC_EDITSTARTBONE,(""));
								PrintWindowString( hWnd, IDC_EDITGROUP,(""));
								PrintWindowString( hWnd, IDC_EDITKEYREDUCTION,(""));
								PrintWindowString( hWnd, IDC_EDITRATE,(""));
								PrintWindowString( hWnd, IDC_EDITNAME,(""));
								_SetCheckBox( hWnd, IDC_CHECKROOT, 0);
							}
						}
						break;						
					}
					// end of messages for IDC_LISTIN box
				}
				break;


				case IDC_LISTOUT: 
				{
					// Set handle for easy access
					switch (HIWORD(wParam))
					{	
						case LBN_KILLFOCUS:
						case LBN_SELCANCEL: // only _SELCHANGE needed ??
						case LBN_SETFOCUS:
						case LBN_SELCHANGE:
						{
							//UpdateOutBox = true;
						}
						break;
					}
				}
				break;				

				/*
				case IDC_BONESELECT:
				{
					// Show it & wait until it returns.
					//ShowBoneTree(hWnd);
				}
				break;
				*/
		
				/* 
				// Apply animation parameters from the edit-properties-window.
				case IDC_UPDATEANIM:
				{
					UpdateAnimParams=true; // update the relevant					
				}
				break;
				*/

				// Buttons which move items back and forth and load/store the whole animation.
				case IDC_MOVEOUT:
				case IDC_COPYOUT:
				{
					// Tracks of dynamic arrays need to be copied explicitly.
					for( INT c=0; c<InSelBufferNum; c++)
					{
					    INT AnimIdx=InSelBuffer[c];
						// The Actual add to OutItems
						
						if( (AnimIdx > -1) && (AnimIdx < TempActor.Animations.Num()) )
						{

							// Double-NAME-check: refuse if a name already exists..
							int NameUnique = 1;
							int ConsistentBones = 1;
							for( INT t=0;t<TempActor.OutAnims.Num(); t++)
							{
								if( strcmp( TempActor.OutAnims[t].AnimInfo.Name, TempActor.Animations[AnimIdx].AnimInfo.Name ) == 0)
									NameUnique = 0;
								if( TempActor.OutAnims[t].AnimInfo.TotalBones != TempActor.Animations[AnimIdx].AnimInfo.TotalBones)
									ConsistentBones = 0;
							}

							if( NameUnique && ConsistentBones )
							{
								INT i = TempActor.OutAnims.Num();
								TempActor.OutAnims.AddZeroed(1); // Add a single element.
								TempActor.OutAnims[i].AnimInfo = TempActor.Animations[AnimIdx].AnimInfo;
								//PopupBox("Copying Keytracks now, total %i...",TempActor.Animations[InSelect].KeyTrack.Num()); //#debug
								for( INT j=0; j<TempActor.Animations[AnimIdx].KeyTrack.Num(); j++ )
								{								
									//if (j<3) PopupBox("j: %i total: %i",j,TempActor.OutAnims[i].KeyTrack.Num());//#debug
									TempActor.OutAnims[i].KeyTrack.AddItem( TempActor.Animations[AnimIdx].KeyTrack[j] );
								}

								// Handle possible animated scale data.
								for( INT s=0; s<TempActor.Animations[AnimIdx].ScaleTrack.Num(); s++ )
								{																
									TempActor.OutAnims[i].ScaleTrack.AddItem( TempActor.Animations[AnimIdx].ScaleTrack[s] );
								}
								
								// Completely update listbox contents:
								UpdateOutBox = true;
								UpdateInBox = true;

								// Queue source for deletion if we are moving:
								if( LOWORD(wParam) == IDC_MOVEOUT )
									DoDeleteIn = true;
							}
							else
							{
								if(! NameUnique )
								    PopupBox("ERROR !! Aborting the move, duplicate name detected.");
								if(! ConsistentBones )
									PopupBox("ERROR !! Aborting the move, inconsistent bone counts detected.");
							}
						}					
					}
				}
				break;

				case IDC_MOVEIN:
				case IDC_COPYIN:
				{
					// Tracks of dynamic arrays need to be copied explicitly.
					// PopupBox( "CopyIn: from %i to %i total Out: %i", OutSelect, TempActor.Animations.Num(), TempActor.OutAnims.Num() ); //#debug
					for( INT c=0; c<OutSelBufferNum; c++)
					{
					    INT AnimIdx=OutSelBuffer[c];
						// Actual add to InItems.
						if( (AnimIdx > -1) && (AnimIdx < TempActor.OutAnims.Num()) )
						{
							INT i = TempActor.Animations.Num();
							TempActor.Animations.AddZeroed(1); // Add a single element.
							//new(TempActor.Animations)VAnimation(); // Add one element to a dynamic array, and properly call its constructor.

							TempActor.Animations[i].AnimInfo = TempActor.OutAnims[AnimIdx].AnimInfo;

							//PopupBox("Copying Keytracks now, total %i...",TempActor.OutAnims[AnimIdx].KeyTrack.Num()); //#debug
							for( INT j=0; j<TempActor.OutAnims[AnimIdx].KeyTrack.Num(); j++ )
							{								
								//if (j<3) PopupBox("j: %i total: %i",j,TempActor.Animations[i].KeyTrack.Num()); //#debug
								TempActor.Animations[i].KeyTrack.AddItem( TempActor.OutAnims[AnimIdx].KeyTrack[j] );
							}

							// Copy Scale tracks if present..
							for( INT s=0; s<TempActor.OutAnims[AnimIdx].ScaleTrack.Num(); s++ )
							{																
								TempActor.Animations[i].ScaleTrack.AddItem( TempActor.OutAnims[AnimIdx].ScaleTrack[s] );
							}

							// Completely update listbox contents:
							UpdateInBox = true;
							UpdateOutBox = true;
							// Queue source for deletion:
							if( LOWORD(wParam) == IDC_MOVEIN )
								DoDeleteOut = true;
						}					
					}
				}
				break;

				// Delete from in-list.
				case IDC_DELIN:
				{						
					{
						DoDeleteIn  = true;
						UpdateInBox = true;
					}					
				}
				break;

				// Delete from out-list.
				case IDC_DELOUT:
				{					
					{
						DoDeleteOut  = true;
						UpdateOutBox = true;
					}					
				}
				break;

				// Name-sort (selected?) entries in the in-list..
				case IDC_ANIMSORT:
				{
					// Quicksort all Animations
					//by: TempActor.Animations[i].AnimInfo.Name;					
					qsort ( &(TempActor.Animations[0]), TempActor.Animations.Num(), sizeof(VAnimation), AnimCompare );
					UpdateInBox = true;
					
				}
				break;


				// Load the same name and folder as specified. 
				case IDC_ANIMLOAD:
				{	
					char to_ext[32];
					_tcscpy(to_ext, ("PSA"));
					sprintf(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_animfile,to_ext);
					FastFileClass InFile;
					if ( InFile.OpenExistingFileForReading(DestPath) != 0) // Error!
						ErrorBox("File [%s] does not exist yet.", DestPath);
					else // Load all relevant chunks into TempActor and its arrays.
					{
						INT DebugBytes = TempActor.LoadAnimation(InFile);
						UpdateOutBox = true;
						// Log input 
						if( !OurScene.DoSuppressAnimPopups )
						{
							PopupBox("Total animation sequences loaded:  %i", TempActor.OutAnims.Num());
						}
					}
					InFile.Close();
				}
				break;

				case IDC_LOADANIMAS:
				{	
					//char to_ext[32];
					//_tcscpy(to_ext, ("PSA"));
					//sprintf(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_animfile,to_ext);

					char filterList[] = "PSA Files (*.psa)\0*.psa\0PSA Files (*.psa)\0*.psa\0";

					char newname[MAX_PATH];
					newname[0] = 0;
					GetLoadName( hWnd, newname, to_path, filterList);
					GetNameFromPath(to_animfile,newname, MAX_PATH );
					_tcscpy( DestPath, newname );
					
					// TEST
					//char ExtName[MAX_PATH],FolderName[MAX_PATH];
					//GetFolderFromPath( FolderName,newname);
					//GetExtensionFromPath( ExtName,newname);
					//PopupBox(" Folder: [%s] name: [%s] ext: [%s] ",FolderName,to_animfile,ExtName);
					//					

					FastFileClass InFile;
					if ( InFile.OpenExistingFileForReading(DestPath) != 0) // Error!
						ErrorBox("File [%s] does not exist yet.", DestPath);
					else // Load all relevant chunks into TempActor and its arrays.
					{
						//PopupBox(" Starting to load file : %s ",DestPath);

						INT DebugBytes = TempActor.LoadAnimation(InFile);

						UpdateOutBox = true;
						// Log input 
						if( !OurScene.DoSuppressAnimPopups )
						{
							PopupBox("Total animation sequences loaded:  %i", TempActor.OutAnims.Num());
						}
					}
					InFile.Close();
				}
				break;



				// Interactive browsing for destination path.
				case IDC_ANIMSAVEAS:
					{						
						char newname[MAX_PATH];
						_tcscpy(newname,("XX"));
						char defaultextension[]=".psa";
						GetSaveName( hWnd, newname, to_path, NULL, defaultextension );

						SaveAnimSet( newname );					
					}
					break;

					// Save with interface-specified name & output path.
				case IDC_ANIMSAVE:
					{
						char to_ext[32];
						_tcscpy(to_ext, ("PSA"));
						sprintf(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_animfile,to_ext);

						SaveAnimSet( DestPath ); 
					}
					break;



			}
		break;
			
		default:
		ReturnWnd = FALSE;
	}


	if( UpdateAnimParams )
	{
		

		char in_string[MAX_PATH];
		// Multiple selection changes:
		if( UpdateStartBone )
		{
			GetWindowText( GetDlgItem( hWnd, IDC_EDITSTARTBONE ), in_string, 300 );
			INT SetStartBone = atoi( in_string );
			if(  SetStartBone >= 0 )
			for( INT c=0; c<InSelBufferNum; c++)
			{
				INT AnimIdx=InSelBuffer[c];
				TempActor.Animations[AnimIdx].AnimInfo.StartBone = SetStartBone;
			}
		}

		if( UpdateRate )
		{
			GetWindowText( GetDlgItem( hWnd, IDC_EDITRATE ), in_string, 300 );
			FLOAT SetRate = atof( in_string );
			if(  SetRate > 0.f )
			for( INT c=0; c<InSelBufferNum; c++)
			{
				INT AnimIdx=InSelBuffer[c];
				TempActor.Animations[AnimIdx].AnimInfo.AnimRate = SetRate;
			}
		}

		if( UpdateKeyReduction )
		{
			GetWindowText( GetDlgItem( hWnd, IDC_EDITKEYREDUCTION ), in_string, 300 );
			FLOAT SetKeyReduction = atof( in_string );
			if(  SetKeyReduction > 0.f )
			for( INT c=0; c<InSelBufferNum; c++)
			{
				INT AnimIdx=InSelBuffer[c];
				TempActor.Animations[AnimIdx].AnimInfo.KeyReduction = SetKeyReduction;
			}
		}

		if( UpdateGroup )
		{
			GetWindowText( GetDlgItem( hWnd, IDC_EDITGROUP ), in_string, 300 );
			if( strlen(in_string) > 0 )			
			for( INT c=0; c<InSelBufferNum; c++)
			{
				INT AnimIdx=InSelBuffer[c];
				_tcscpy( TempActor.Animations[AnimIdx].AnimInfo.Group, in_string );
			}
		}

		if( UpdateName )
		{
			GetWindowText( GetDlgItem( hWnd, IDC_EDITNAME ), in_string, 300 );			
			if( strlen(in_string) > 0  && (InSelBufferNum == 1) )		 // Necessary: cannot rename more than 1 animation !	
			for( INT c=0; c<InSelBufferNum; c++)
			{
				INT AnimIdx=InSelBuffer[c];
				_tcscpy( TempActor.Animations[AnimIdx].AnimInfo.Name, in_string );
			}
			UpdateInBox = true; // Necessary for names to be updated :-)
		}

		/*
		if( UpdateCheckRoot )
		{
			UBOOL RootCheck = _GetCheckBox( hWnd, IDC_CHECKROOT );
			for( INT c=0; c<InSelBufferNum; c++)
			{
				INT AnimIdx=InSelBuffer[c];
				TempActor.Animations[AnimIdx].AnimInfo.ScaleInclude = ScaleInclude;
				//PopupBox(" updatecheckroot ");
			}
		}
		*/
	}

	// Delete all selected in out-box.
	if ( DoDeleteOut  )
	{
		for( INT c = OutSelBufferNum-1; c >= 0; c-- )
		{
		    INT AnimIdx=OutSelBuffer[c];
			if( AnimIdx<TempActor.OutAnims.Num() )
			{
				// Deleting an animation: empty its animation track.
				TempActor.OutAnims[AnimIdx].KeyTrack.Empty();
				TempActor.OutAnims[AnimIdx].ScaleTrack.Empty();

				// Delete.
				TempActor.OutAnims.DelIndex(AnimIdx); 
			}
		}
	}	

	// Delete all selected in in-box.
	if ( DoDeleteIn  )
	{
		for( INT c = InSelBufferNum-1; c >= 0; c-- )
		{
		    INT AnimIdx=InSelBuffer[c];
			if( AnimIdx<TempActor.Animations.Num() )
			{
				// Deleting an animation: empty its animation track.
				TempActor.Animations[AnimIdx].KeyTrack.Empty();
				TempActor.Animations[AnimIdx].ScaleTrack.Empty();
				// Delete.
				TempActor.Animations.DelIndex(AnimIdx); 
			}
		}
	}	
	

	if( UpdateInBox )
	{
		hwndList = GetDlgItem( hWnd, IDC_LISTIN );
		// Empty animations list.
		SendMessage( hwndList, LB_RESETCONTENT, 0, 0 );
		// Fill the animations list.
		for( INT i=0; i< TempActor.Animations.Num(); i++ )
		{
			char* String1 = TempActor.Animations[i].AnimInfo.Name;
			SendMessage( hwndList, LB_INSERTSTRING, i, (LPARAM) String1);
		}
	}


	if( UpdateOutBox )
	{
		hwndList = GetDlgItem( hWnd, IDC_LISTOUT );
		// Empty animations list.
		SendMessage( hwndList, LB_RESETCONTENT, 0, 0 );
		// Fill the animations list.
		for( INT i=0; i< TempActor.OutAnims.Num(); i++ )
		{
			char* String1 = TempActor.OutAnims[i].AnimInfo.Name;
			SendMessage( hwndList, LB_INSERTSTRING, i, (LPARAM)String1 );
		}		
	}

	return ReturnWnd;
}    




  
INT_PTR CALLBACK SceneInfoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
		case WM_INITDIALOG:
			//CenterWindow(hWnd, GetParent(hWnd)); 
			{
				PrintWindowNum(hWnd,IDC_MESHES,     OurScene.GeomMeshes);
				PrintWindowNum(hWnd,IDC_PHYMESHES,  OurScene.TotalSkinNodeNum);
				PrintWindowNum(hWnd,IDC_TOTALBONES, OurScene.TotalBones);
				PrintWindowNum(hWnd,IDC_DUMMIES,    OurScene.TotalDummies);
				PrintWindowNum(hWnd,IDC_MAXBONES,   OurScene.TotalMaxBones);
				PrintWindowNum(hWnd,IDC_BIPBONES,   OurScene.TotalBipBones);

				PrintWindowNum(hWnd,IDC_VERTS,    OurScene.TotalVerts);
				PrintWindowNum(hWnd,IDC_FACES,    OurScene.TotalFaces);

				PrintWindowNum(hWnd,IDC_LINKEDBONES, OurScene.LinkedBones);
				PrintWindowNum(hWnd,IDC_TOTALLINKS,  OurScene.TotalSkinLinks);

				PrintWindowNum(hWnd,IDC_FRAMES,  1 +  (OurScene.FrameEnd - OurScene.FrameStart)/OurScene.FrameTicks);
				PrintWindowNum(hWnd,IDC_FRAMES,   OurScene.FrameStart);
				
				PrintWindowNum(hWnd,IDC_TICKSFRAME,  OurScene.FrameTicks);
				PrintWindowNum(hWnd,IDC_TOTALTIME,   OurScene.FrameEnd);

				PrintWindowNum(hWnd,IDC_BONESCURRENT, OurScene.OurBoneTotal);

				

			}
		break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				case IDOK:
					EndDialog(hWnd, 1);
				break;
			}
		break;

		default:
		return FALSE;

	}
	return TRUE;
}       



