#ifndef __ACTORY__H
#define __ACTORY__H

#include <maya/MPxCommand.h>
#include <maya/MArgList.h>


//PCF BEGIN
// Compile-time defines for debugging popups and logfiles
#define DEBUGMSGS (0)  // set to 0 to avoid debug popups
#define DEBUGFILE (0)  // set to 0 to suppress misc. log files being written from SceneIFC.cpp                        
#define DOLOGFILE (1)  // Log extra animation/skin info while writing a .PSA or .PSK file
#define DEBUGMEM  (0)  // Memory debugging

class MArgList;

class ActorY : public MPxCommand						
{															
public:														
	ActorY() {};										
	virtual MStatus	doIt ( const MArgList& );				
	static void*	creator();								
	static MSyntax newSyntax();
	MStatus UTExportMeshY( const MArgList& args );

};															

//PCF END

#endif // __ACTORY__H

