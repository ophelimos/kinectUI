#pragma once

#include "NuiApi.h"

class NuiImpl
{
	/* Since the classes are already far too linked */
	friend class CSkeletalViewerApp;
public:
	NuiImpl();
	~NuiImpl();
	HRESULT                 Nui_Init( );
	HRESULT                 Nui_Init( OLECHAR * instanceName );
	void                    Nui_UnInit( );
	void                    Nui_GotDepthAlert( );
	void                    Nui_GotColorAlert( );
	void                    Nui_GotSkeletonAlert( );
	void                    Nui_Zero();
	/* void                    Nui_BlankSkeletonScreen( HWND hWnd, bool getDC ); */
	/* void                    Nui_DoDoubleBuffer(HWND hWnd,HDC hDC); */
	/* void                    Nui_DrawSkeleton( NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor ); */
	/* void                    Nui_DrawSkeletonId( NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor ); */

	/* void                    Nui_DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... ); */
	/* void                    Nui_EnableSeatedTracking(bool seated); */
	void                    Nui_SetApplicationTracking(bool applicationTracks);
	void                    Nui_SetTrackedSkeletons(int skel1, int skel2);

	/* RGBQUAD                 Nui_ShortToQuad_Depth( USHORT s ); */

	static void CALLBACK    Nui_StatusProcThunk(HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void* pUserData);
	void CALLBACK           Nui_StatusProc( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName );

private:
	static DWORD WINAPI     Nui_ProcessThread(LPVOID pParam);
	DWORD WINAPI            Nui_ProcessThread();
	
	// Current kinect
	INuiSensor *            m_pNuiSensor;
	BSTR                    m_instanceId;

	// thread handling
	HANDLE        m_hThNuiProcess;
	HANDLE        m_hEvNuiProcessStop;

	HANDLE        m_hNextDepthFrameEvent;
	HANDLE        m_hNextColorFrameEvent;
	HANDLE        m_hNextSkeletonEvent;
	HANDLE        m_pDepthStreamHandle;
	HANDLE        m_pVideoStreamHandle;
	/* HFONT         m_hFontFPS; */
	/* HFONT		  m_smallFontFPS; */
	/* HFONT         m_hFontSkeletonId; */
	/* HPEN          m_Pen[NUI_SKELETON_COUNT]; */
	/* HDC           m_SkeletonDC; */
	/* HBITMAP       m_SkeletonBMP; */
	/* HGDIOBJ       m_SkeletonOldObj; */
	/* int           m_PensTotal; */
	/* POINT         m_Points[NUI_SKELETON_POSITION_COUNT]; */
	/* RGBQUAD       m_rgbWk[640*480]; */
	DWORD         m_LastSkeletonFoundTime;
	/* bool          m_bScreenBlanked; */
	bool          m_bAppTracking;
	int           m_DepthFramesTotal;
	DWORD         m_LastDepthFPStime;
	int           m_LastDepthFramesTotal;
	DWORD         m_SkeletonIds[NUI_SKELETON_COUNT];
	DWORD         m_TrackedSkeletonIds[NUI_SKELETON_MAX_TRACKED_COUNT];
	/* ULONG_PTR     m_GdiplusToken; */
};
