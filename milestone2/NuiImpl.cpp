//------------------------------------------------------------------------------
// <copyright file="NuiImpl.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// Implementation of CSkeletalViewerApp methods dealing with NUI processing

#include "stdafx.h"
#include "SkeletalViewer.h"
#include "resource.h"
#include "Magnifier.h"
#include <mmsystem.h>
#include <assert.h>
#include <strsafe.h>
#include "NuiImpl.h"

// Globals
extern int distanceInMM;
extern int activeSkeleton;
extern GestureDetector* gestureDetectors[NUI_SKELETON_COUNT];
extern BOOL allowMagnifyGestures;
extern FLOAT moveAmount_x;
extern FLOAT moveAmount_y;
// The important one for splitting the functionality
extern CSkeletalViewerApp* skeletalViewer;
extern BOOL showSkeletalViewer;

// Variables used to deal with the problem that threads might be in a
// GUI section when the GUI exits, and so we need to preserve the GUI
// until all the events trying to use it have finished
// (readers-writers problem, with a writer preference)
extern BOOL GUI_On;
extern int num_GUIers;
extern HANDLE num_GUIers_mutex;


//-------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------
NuiImpl::NuiImpl()
{
	m_pNuiSensor = NULL;
	// Even though this is a BSTR, you can treat it like a char*
	m_instanceId = NULL;
	Nui_Zero();
	NuiSetDeviceStatusCallback( &NuiImpl::Nui_StatusProcThunk, this );
	Nui_Init();
}

//-------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------
NuiImpl::~NuiImpl()
{
	Nui_UnInit();
	Nui_Zero();
	SysFreeString(m_instanceId);
}

//-------------------------------------------------------------------
// Nui_Zero
//
// Zero out member variables
//-------------------------------------------------------------------
void NuiImpl::Nui_Zero()
{
	if (m_pNuiSensor)
	{
		m_pNuiSensor->Release();
		m_pNuiSensor = NULL;
	}
	m_hNextDepthFrameEvent = NULL;
	m_hNextColorFrameEvent = NULL;
	m_hNextSkeletonEvent = NULL;
	m_pDepthStreamHandle = NULL;
	m_pVideoStreamHandle = NULL;
	m_hThNuiProcess = NULL;
	m_hEvNuiProcessStop = NULL;
	m_LastSkeletonFoundTime = 0;
	m_DepthFramesTotal = 0;
	m_LastDepthFPStime = 0;
	m_LastDepthFramesTotal = 0;
	// The ZeroMemory versions cause memory corruption.
	// The reason is that sizeof(m_SkeletonIds) is larger than NUI_SKELETON_MAX_TRACKED_COUNT.  (24, rather than 8)
	// It's not a problem with ZeroMemory
	// ZeroMemory(m_SkeletonIds,sizeof(m_SkeletonIds));
	for (int i = 0; i < NUI_SKELETON_COUNT; i++)
	{
		m_SkeletonIds[i] = 0;
	}
	// ZeroMemory(m_TrackedSkeletonIds,sizeof(m_SkeletonIds));
	for (int i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; i++)
	{
		m_TrackedSkeletonIds[i] = 0;
	}
}

void CALLBACK NuiImpl::Nui_StatusProcThunk( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void * pUserData )
{
	reinterpret_cast<NuiImpl *>(pUserData)->Nui_StatusProc( hrStatus, instanceName, uniqueDeviceName );
}

//-------------------------------------------------------------------
// Nui_StatusProc
//
// Callback to handle Kinect status changes
//-------------------------------------------------------------------
void CALLBACK NuiImpl::Nui_StatusProc( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* /*uniqueDeviceName*/ )
{
	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		// Update UI
		PostMessageW( skeletalViewer->m_hWnd, WM_USER_UPDATE_COMBO, 0, 0 );
		skeletalViewer->decrement_num_GUIers();
	}

	if( SUCCEEDED(hrStatus) )
	{
		if ( S_OK == hrStatus )
		{
			if ( m_instanceId && 0 == wcscmp(instanceName, m_instanceId) )
			{
				Nui_Init(m_instanceId);
			}
			else if ( !m_pNuiSensor )
			{
				Nui_Init();
			}
		}
	}
	else
	{
		if ( m_instanceId && 0 == wcscmp(instanceName, m_instanceId) )
		{
			Nui_UnInit();
			Nui_Zero();
			if (GUI_On && skeletalViewer->increment_num_GUIers())
			{
				skeletalViewer->SV_UnInit();				
				skeletalViewer->SV_Zero();
				skeletalViewer->decrement_num_GUIers();
			}
		}
	}
}

//-------------------------------------------------------------------
// Nui_Init
//
// Initialize Kinect by instance name
//-------------------------------------------------------------------
HRESULT NuiImpl::Nui_Init( OLECHAR *instanceName )
{
	// Generic creation failure
	if ( NULL == instanceName )
	{
		if (GUI_On && skeletalViewer->increment_num_GUIers())
		{
			skeletalViewer->MessageBoxResource( IDS_ERROR_NUICREATE, MB_OK | MB_ICONHAND );
			skeletalViewer->decrement_num_GUIers();
		}
		return E_FAIL;
	}

	HRESULT hr = NuiCreateSensorById( instanceName, &m_pNuiSensor );

	// Generic creation failure
	if ( FAILED(hr) )
	{
		if (GUI_On && skeletalViewer->increment_num_GUIers())
		{
			skeletalViewer->MessageBoxResource( IDS_ERROR_NUICREATE, MB_OK | MB_ICONHAND );
			skeletalViewer->decrement_num_GUIers();
		}
		return hr;
	}

	SysFreeString(m_instanceId);

	m_instanceId = m_pNuiSensor->NuiDeviceConnectionId();

	return Nui_Init();
}

//-------------------------------------------------------------------
// Nui_Init
//
// Initialize Kinect
//-------------------------------------------------------------------
HRESULT NuiImpl::Nui_Init( )
{
	// Clean state the class
	Nui_Zero();

	HRESULT  hr;

	if ( !m_pNuiSensor )
	{
		HRESULT hr = NuiCreateSensorByIndex(0, &m_pNuiSensor);

		if ( FAILED(hr) )
		{
			return hr;
		}

		// Why free a string if we haven't got an instanceId yet?
		SysFreeString(m_instanceId);

		m_instanceId = m_pNuiSensor->NuiDeviceConnectionId();
	}

	// Start up the skeletal viewer at this point
	if (skeletalViewer == NULL && showSkeletalViewer)
	{
		// Make a CSkeletalViewerApp object
		skeletalViewer = new CSkeletalViewerApp(this);
		// The object itself will call SV_Init() when it starts (runs DisplayWindow)
	}

	m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hNextColorFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	DWORD nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON |  NUI_INITIALIZE_FLAG_USES_COLOR;
	hr = m_pNuiSensor->NuiInitialize( nuiFlags );
	if ( E_NUI_SKELETAL_ENGINE_BUSY == hr )
	{
		nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH |  NUI_INITIALIZE_FLAG_USES_COLOR;
		hr = m_pNuiSensor->NuiInitialize( nuiFlags) ;
	}

	if ( FAILED( hr ) )
	{
		if ( E_NUI_DEVICE_IN_USE == hr )
		{
			if (GUI_On && skeletalViewer->increment_num_GUIers())
			{
				skeletalViewer->MessageBoxResource( IDS_ERROR_IN_USE, MB_OK | MB_ICONHAND );
				skeletalViewer->decrement_num_GUIers();
			}
		}
		else
		{
			if (GUI_On && skeletalViewer->increment_num_GUIers())
			{
				skeletalViewer->MessageBoxResource( IDS_ERROR_NUIINIT, MB_OK | MB_ICONHAND );
				skeletalViewer->decrement_num_GUIers();
			}
		}
		return hr;
	}

	hr = m_pNuiSensor->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_COLOR,
		NUI_IMAGE_RESOLUTION_640x480,
		0,
		2,
		m_hNextColorFrameEvent,
		&m_pVideoStreamHandle );

	if ( FAILED( hr ) )
	{
		if (GUI_On && skeletalViewer->increment_num_GUIers())
		{
			skeletalViewer->MessageBoxResource( IDS_ERROR_VIDEOSTREAM, MB_OK | MB_ICONHAND );
			skeletalViewer->decrement_num_GUIers();
		}
		return hr;
	}

	hr = m_pNuiSensor->NuiImageStreamOpen(
		HasSkeletalEngine(m_pNuiSensor) ? NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX : NUI_IMAGE_TYPE_DEPTH,
		NUI_IMAGE_RESOLUTION_320x240,
		0,
		2,
		m_hNextDepthFrameEvent,
		&m_pDepthStreamHandle );

	if ( FAILED( hr ) )
	{
		if (GUI_On && skeletalViewer->increment_num_GUIers())
		{
			skeletalViewer->MessageBoxResource(IDS_ERROR_DEPTHSTREAM, MB_OK | MB_ICONHAND);
			skeletalViewer->decrement_num_GUIers();
		}
		return hr;
	}

	if ( HasSkeletalEngine( m_pNuiSensor ) )
	{
		hr = m_pNuiSensor->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, 0 );
		if( FAILED( hr ) )
		{
			if (GUI_On && skeletalViewer->increment_num_GUIers())
			{
				skeletalViewer->MessageBoxResource( IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND );
				skeletalViewer->decrement_num_GUIers();
			}
			return hr;
		}
	}

	// Start the Nui processing thread
	m_hEvNuiProcessStop = CreateEvent( NULL, FALSE, FALSE, NULL );
	m_hThNuiProcess = CreateThread( NULL, 0, Nui_ProcessThread, this, 0, NULL );

	return hr;
}

//-------------------------------------------------------------------
// Nui_UnInit
//
// Uninitialize Kinect
//-------------------------------------------------------------------
void NuiImpl::Nui_UnInit( )
{
	// Stop the Nui processing thread
	if ( NULL != m_hEvNuiProcessStop )
	{
		// Signal the thread
		SetEvent(m_hEvNuiProcessStop);

		// Wait for thread to stop
		if ( NULL != m_hThNuiProcess )
		{
			WaitForSingleObject( m_hThNuiProcess, INFINITE );
			CloseHandle( m_hThNuiProcess );
		}
		CloseHandle( m_hEvNuiProcessStop );
	}

	if ( m_pNuiSensor )
	{
		m_pNuiSensor->NuiShutdown( );
	}
	if ( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_hNextSkeletonEvent );
		m_hNextSkeletonEvent = NULL;
	}
	if ( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_hNextDepthFrameEvent );
		m_hNextDepthFrameEvent = NULL;
	}
	if ( m_hNextColorFrameEvent && ( m_hNextColorFrameEvent != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_hNextColorFrameEvent );
		m_hNextColorFrameEvent = NULL;
	}

	if ( m_pNuiSensor )
	{
		m_pNuiSensor->Release();
		m_pNuiSensor = NULL;
	}

	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		skeletalViewer->SV_UnInit();
		skeletalViewer->decrement_num_GUIers();
	}
}

DWORD WINAPI NuiImpl::Nui_ProcessThread(LPVOID pParam)
{
	NuiImpl *pthis = (NuiImpl *) pParam;
	return pthis->Nui_ProcessThread();
}

//-------------------------------------------------------------------
// Nui_ProcessThread
//
// Thread to handle Kinect processing
//-------------------------------------------------------------------
DWORD WINAPI NuiImpl::Nui_ProcessThread()
{
	const int numEvents = 4;
	HANDLE hEvents[numEvents] = { m_hEvNuiProcessStop, m_hNextDepthFrameEvent, m_hNextColorFrameEvent, m_hNextSkeletonEvent };
	int    nEventIdx;
	DWORD  t;

	m_LastDepthFPStime = timeGetTime( );

	//blank the skeleton display on startup
	m_LastSkeletonFoundTime = 0;

	// Main thread loop
	bool continueProcessing = true;
	while ( continueProcessing )
	{
		// Wait for any of the events to be signalled
		nEventIdx = WaitForMultipleObjects( numEvents, hEvents, FALSE, 100 );

		// Process signal events
		switch ( nEventIdx )
		{
		case WAIT_TIMEOUT:
			continue;

			// If the stop event, stop looping and exit
		case WAIT_OBJECT_0:
			continueProcessing = false;
			continue;

		case WAIT_OBJECT_0 + 1:
			Nui_GotDepthAlert();
			++m_DepthFramesTotal;
			break;

		case WAIT_OBJECT_0 + 2:
			Nui_GotColorAlert();
			break;

		case WAIT_OBJECT_0 + 3:
			Nui_GotSkeletonAlert( );
			break;
		}

		// Once per second, display the depth FPS
		if (GUI_On && skeletalViewer->increment_num_GUIers())
		{
			t = timeGetTime( );
			if ( (t - m_LastDepthFPStime) > 1000 )
			{
				int fps = ((m_DepthFramesTotal - m_LastDepthFramesTotal) * 1000 + 500) / (t - m_LastDepthFPStime);
				PostMessageW( skeletalViewer->m_hWnd, WM_USER_UPDATE_FPS, IDC_FPS, fps );
				m_LastDepthFramesTotal = m_DepthFramesTotal;
				m_LastDepthFPStime = t;
			}

			// Blank the skeleton panel if we haven't found a skeleton recently
			if ( (t - m_LastSkeletonFoundTime) > 250 )
			{
				if ( ! skeletalViewer->m_bScreenBlanked )
				{
					skeletalViewer->Nui_BlankSkeletonScreen( GetDlgItem( skeletalViewer->m_hWnd, IDC_SKELETALVIEW ), true );
					skeletalViewer->m_bScreenBlanked = true;
				}
			}
			skeletalViewer->decrement_num_GUIers();
		}
	}

	return 0;
}

//-------------------------------------------------------------------
// Nui_GotColorAlert
//
// Handle new color data
//-------------------------------------------------------------------
void NuiImpl::Nui_GotColorAlert( )
{
	NUI_IMAGE_FRAME imageFrame;

	HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame( m_pVideoStreamHandle, 0, &imageFrame );

	if ( FAILED( hr ) )
	{
		return;
	}

	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
		NUI_LOCKED_RECT LockedRect;
		pTexture->LockRect( 0, &LockedRect, NULL, 0 );
		if ( LockedRect.Pitch != 0 )
		{
			skeletalViewer->m_pDrawColor->Draw( static_cast<BYTE *>(LockedRect.pBits), LockedRect.size );
		}
		else
		{
			OutputDebugString( "Buffer length of received texture is bogus\r\n" );
		}

		pTexture->UnlockRect( 0 );
		skeletalViewer->decrement_num_GUIers();
	}

	m_pNuiSensor->NuiImageStreamReleaseFrame( m_pVideoStreamHandle, &imageFrame );
}

//-------------------------------------------------------------------
// Nui_GotDepthAlert
//
// Handle new depth data
//-------------------------------------------------------------------
void NuiImpl::Nui_GotDepthAlert( )
{
	NUI_IMAGE_FRAME imageFrame;

	HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame(
		m_pDepthStreamHandle,
		0,
		&imageFrame );

	if ( FAILED( hr ) )
	{
		return;
	}

	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
		NUI_LOCKED_RECT LockedRect;
		pTexture->LockRect( 0, &LockedRect, NULL, 0 );
		if ( 0 != LockedRect.Pitch )
		{
			DWORD frameWidth, frameHeight;

			NuiImageResolutionToSize( imageFrame.eResolution, frameWidth, frameHeight );

			// draw the bits to the bitmap
			RGBQUAD * rgbrun = skeletalViewer->m_rgbWk;
			USHORT * pBufferRun = (USHORT *)LockedRect.pBits;

			// end pixel is start + width*height - 1
			USHORT * pBufferEnd = pBufferRun + (frameWidth * frameHeight);

			assert( frameWidth * frameHeight <= ARRAYSIZE(skeletalViewer->m_rgbWk) );

			while ( pBufferRun < pBufferEnd )
			{
				*rgbrun = skeletalViewer->Nui_ShortToQuad_Depth( *pBufferRun );
				++pBufferRun;
				++rgbrun;
			}

			skeletalViewer->m_pDrawDepth->Draw( (BYTE*) skeletalViewer->m_rgbWk, frameWidth * frameHeight * 4 );
		}
		else
		{
			OutputDebugString( "Buffer length of received texture is bogus\r\n" );
		}

		pTexture->UnlockRect( 0 );
		skeletalViewer->decrement_num_GUIers();
	}

	m_pNuiSensor->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, &imageFrame );
}


//-------------------------------------------------------------------
// Nui_GotSkeletonAlert
//
// Handle new skeleton data
//-------------------------------------------------------------------
void NuiImpl::Nui_GotSkeletonAlert( )
{
	NUI_SKELETON_FRAME SkeletonFrame = {0};

	bool bFoundSkeleton = false;

	if ( SUCCEEDED(m_pNuiSensor->NuiSkeletonGetNextFrame( 0, &SkeletonFrame )) )
	{
		for ( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
		{
			// If we're no longer tracking the active skeleton, we don't have an active skeleton
			if ((i == activeSkeleton) && (SkeletonFrame.SkeletonData[i].eTrackingState != NUI_SKELETON_TRACKED))
			{
				clearAndHideOverlay();
				moveAmount_x = 0;
				moveAmount_y = 0;
				gestureDetectors[activeSkeleton]->state->state = OFF;
				activeSkeleton = -1;
				
			}

			if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED ||
				(SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_POSITION_ONLY
				 && GUI_On && m_bAppTracking))
			{
				bFoundSkeleton = true;
				// If we don't have an active skeleton, whatever's tracked becomes the active one
				if (activeSkeleton == -1)
				{
					clearAndHideOverlay();
					moveAmount_x = 0;
					moveAmount_y = 0;
					activeSkeleton = i;
				}
			}
		}
	}

	// no skeletons!
	if( !bFoundSkeleton )
	{
		return;
	}

	// smooth out the skeleton data
	HRESULT hr = m_pNuiSensor->NuiTransformSmooth(&SkeletonFrame,NULL);
	if ( FAILED(hr) )
	{
		return;
	}

	// we found a skeleton, re-start the skeletal timer
	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		skeletalViewer->m_bScreenBlanked = false;
		skeletalViewer->decrement_num_GUIers();
	}
	m_LastSkeletonFoundTime = timeGetTime( );

	// Save the velocities via comparison with the previous skeleton frame
	static NUI_SKELETON_FRAME prevFrame = SkeletonFrame;

	// draw each skeleton color according to the slot within they are found.
	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		skeletalViewer->Nui_BlankSkeletonScreen( GetDlgItem( skeletalViewer->m_hWnd, IDC_SKELETALVIEW ), false );
		skeletalViewer->decrement_num_GUIers();
	}

	bool bSkeletonIdsChanged = false;
	for ( int i = 0 ; i < NUI_SKELETON_COUNT; i++ )
	{
		if ( m_SkeletonIds[i] != SkeletonFrame.SkeletonData[i].dwTrackingID )
		{
			m_SkeletonIds[i] = SkeletonFrame.SkeletonData[i].dwTrackingID;
			bSkeletonIdsChanged = true;
		}

		// Show skeleton only if it is tracked, and the center-shoulder joint is at least inferred.
		if ( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED &&
			SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] != NUI_SKELETON_POSITION_NOT_TRACKED)
		{
			if (GUI_On && skeletalViewer->increment_num_GUIers())
			{
				skeletalViewer->Nui_DrawSkeleton( &SkeletonFrame.SkeletonData[i], GetDlgItem( skeletalViewer->m_hWnd, IDC_SKELETALVIEW ), i );
				skeletalViewer->decrement_num_GUIers();
			}
			if (i == activeSkeleton)
			{
				// Update distance data
				Vector4 headPoint = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
				LONG xcoord, ycoord;
				USHORT depthValue;
				NuiTransformSkeletonToDepthImage(headPoint, &xcoord, &ycoord, &depthValue);
				int depthInMM = (depthValue >> 3);
				distanceInMM = depthInMM;

				if (GUI_On && skeletalViewer->increment_num_GUIers())
				{
					::PostMessageW(skeletalViewer->m_hWnd, WM_USER_UPDATE_DISTANCE, IDC_DISTANCE, depthInMM);

					// Update user data
					::PostMessageW(skeletalViewer->m_hWnd, WM_USER_UPDATE_USER, IDC_USER, i);
					skeletalViewer->decrement_num_GUIers();
				}
			}
			// Remember, gesture detectors are now per-skeleton, but we still only want to detect gestures for tracked skeletons

			// TODO: Don't try to detect gestures for messed-up skeletons
			gestureDetectors[i]->detect(SkeletonFrame, prevFrame);
		}
		else if ( GUI_On && m_bAppTracking && SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_POSITION_ONLY )
		{
			skeletalViewer->increment_num_GUIers();
			skeletalViewer->Nui_DrawSkeletonId( &SkeletonFrame.SkeletonData[i], GetDlgItem( skeletalViewer->m_hWnd, IDC_SKELETALVIEW ), i );
			skeletalViewer->decrement_num_GUIers();
		}
	}

	if ( bSkeletonIdsChanged && GUI_On)
	{
		skeletalViewer->increment_num_GUIers();
		skeletalViewer->UpdateTrackingComboBoxes();
		skeletalViewer->decrement_num_GUIers();
	}

	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		skeletalViewer->Nui_DoDoubleBuffer(GetDlgItem(skeletalViewer->m_hWnd,IDC_SKELETALVIEW), skeletalViewer->m_SkeletonDC);
		skeletalViewer->decrement_num_GUIers();
	}

	prevFrame = SkeletonFrame;
}

void NuiImpl::Nui_SetApplicationTracking(bool applicationTracks)
{
	if ( HasSkeletalEngine(m_pNuiSensor) )
	{
		HRESULT hr = m_pNuiSensor->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, applicationTracks ? NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS : 0);
		if ( FAILED( hr ) )
		{
			if (GUI_On && skeletalViewer->increment_num_GUIers())
			{
				skeletalViewer->MessageBoxResource(IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND);
				skeletalViewer->decrement_num_GUIers();
			}
		}
	}
}

void NuiImpl::Nui_SetTrackedSkeletons(int skel1, int skel2)
{
	m_TrackedSkeletonIds[0] = skel1;
	m_TrackedSkeletonIds[1] = skel2;
	DWORD tracked[NUI_SKELETON_MAX_TRACKED_COUNT] = { skel1, skel2 };
	if ( FAILED(m_pNuiSensor->NuiSkeletonSetTrackedSkeletons(tracked)) )
	{
		if (GUI_On && skeletalViewer->increment_num_GUIers())
		{
			skeletalViewer->MessageBoxResource(IDS_ERROR_SETTRACKED, MB_OK | MB_ICONHAND);
			skeletalViewer->decrement_num_GUIers();
		}
	}
}
