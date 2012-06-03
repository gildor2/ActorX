#ifndef _XSIINTERFACE_H_
#define _XSIINTERFACE_H_
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE 
// Misc includes
#include <windows.h>
#include <assert.h>

#include <xsi_pluginregistrar.h>
#include <xsi_decl.h>
#include <xsi_viewcontext.h>

#include <xsi_ref.h>
#include <xsi_decl.h>

#include <xsi_x3dobject.h>

#include <SIBCArray.h>

// copied over from MaxInterface.h
#define SMALLESTWEIGHT (0.0001f)
#define MAXSKININDEX 64

typedef struct _x3dpointer
{
	_x3dpointer(XSI::CRef in_ptr) { m_ptr = in_ptr;};

	XSI::CRef m_ptr;
} x3dpointer;

typedef struct _singleweight
{
	int		m_iBoneIndex;
	float	m_fWeight;
	x3dpointer* m_ptr;
} singleweight;

typedef struct _weightedvertex
{
	CSIBCArray<singleweight>	m_pWeights;
} weightedvertex;

typedef struct _HierarchicalLevel
{
	int m_iLevelsDeep;
	XSI::CRef m_cref;
} HierarchicalLevel;

typedef struct _ActorXParam
{
	int				m_iID;
	HWND			m_hParent;
	XSI::CValue		m_cValue;
	XSI::CString	m_cName;
} ActorXParam;

BOOL CALLBACK StaticMeshDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Resources
#include ".\res\resource.h"

//
// interface class
//

class XSIInterface
{
public:

	XSIInterface();
	virtual ~XSIInterface();

	void	ActivatePanel();

	void	ShowActorX( );
	void	ShowActorManager(  );
	void	ShowSceneInfo();

	void	ExportStaticMesh();

	void	ShowActorXSetup( );
	void	ShowVertex( );
	void	ShowAbout( );
	void	ShowActorXStaticMesh();

	void	ShowTab ( int );

	HWND	GetXSIWindow();

	HWND	GetTabHWND ( int );

	void	AnalyzeSkeleton();
	bool	IsDeformer ( XSI::CRef in_obj );

	CSIBCArray<x3dpointer*>	m_pCachedScene;
	CSIBCArray<x3dpointer*>	m_pCachedMaterials;
	CSIBCArray<HierarchicalLevel*> m_pHRLevels;	

	bool	IsBone ( XSI::SceneItem obj );
	bool	IsChildOf ( XSI::SceneItem children, XSI::SceneItem obj );
	x3dpointer*	GetRootPointer();

	void	UpdateParameters();
	void	SaveParameters();
	void	DoGlobalDataExchange();

	ActorXParam*	GetParameter ( XSI::CString name );

	HWND	m_hXSIPanel;

private:

	void	AddParameter ( int ID, HWND parent, XSI::CValue value, XSI::CString );
	

	void	HideAll();

	HWND	m_hXSI;

	

	HWND	m_hActorX;
	HWND	m_hActorXSetup;
	HWND	m_hActorManager;
	HWND	m_hActorXSceneInfo;
	HWND	m_hActorXVertex;
	HWND	m_hActorXStaticMesh;
	HWND	m_hAbout;

	XSI::CRef m_cSkeletonRoot;

	CSIBCArray<ActorXParam>	m_vParameters;
};

//
// dialog callbacks
//

#endif
