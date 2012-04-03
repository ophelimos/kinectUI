//------------------------------------------------------------------------------
// <copyright file="SkeletalViewer.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// Disable "conditional expression is constant" warning
#pragma warning( disable : 4127 )

// Note: 
//     Platform SDK lib path should be added before the VC lib
//     path, because uuid.lib in VC lib path may be older

#include "stdafx.h"
#include <strsafe.h>
#include <crtdbg.h>
#include "SkeletalViewer.h"
#include "resource.h"
#include "NuiImpl.h"

// Global Variables:
int activeSkeleton = -1;		// The skeleton we care about for gestures
extern BOOL showSkeletalViewer;		     // Whether or not we plan on starting up the skeletal viewer window in the first place
GestureDetector* gestureDetectors[NUI_SKELETON_COUNT];
BOOL                startWithDebugScreen = FALSE;
extern int distanceInMM;
CSkeletalViewerApp* skeletalViewer = NULL; // This is the skeletal viewer window.  If it's NULL, it doesn't exist.  If it's not, it does.
extern BOOL allowMagnifyGestures;
extern BOOL quit_properly;
NuiImpl* nui_impl;

// Variables used to deal with the problem that threads might be in a
// GUI section when the GUI exits, and so we need to preserve the GUI
// until all the events trying to use it have finished
// (readers-writers problem, with a writer preference)
BOOL GUI_On = FALSE;

#define INSTANCE_MUTEX_NAME L"SkeletalViewerInstanceCheck"

static const COLORREF g_JointColorTable[NUI_SKELETON_POSITION_COUNT] = 
{
	RGB(169, 176, 155), // NUI_SKELETON_POSITION_HIP_CENTER
	RGB(169, 176, 155), // NUI_SKELETON_POSITION_SPINE
	RGB(168, 230, 29),  // NUI_SKELETON_POSITION_SHOULDER_CENTER
	RGB(200, 0,   0),   // NUI_SKELETON_POSITION_HEAD
	RGB(79,  84,  33),  // NUI_SKELETON_POSITION_SHOULDER_LEFT
	RGB(84,  33,  42),  // NUI_SKELETON_POSITION_ELBOW_LEFT
	RGB(255, 126, 0),   // NUI_SKELETON_POSITION_WRIST_LEFT
	RGB(215,  86, 0),   // NUI_SKELETON_POSITION_HAND_LEFT
	RGB(33,  79,  84),  // NUI_SKELETON_POSITION_SHOULDER_RIGHT
	RGB(33,  33,  84),  // NUI_SKELETON_POSITION_ELBOW_RIGHT
	RGB(77,  109, 243), // NUI_SKELETON_POSITION_WRIST_RIGHT
	RGB(37,   69, 243), // NUI_SKELETON_POSITION_HAND_RIGHT
	RGB(77,  109, 243), // NUI_SKELETON_POSITION_HIP_LEFT
	RGB(69,  33,  84),  // NUI_SKELETON_POSITION_KNEE_LEFT
	RGB(229, 170, 122), // NUI_SKELETON_POSITION_ANKLE_LEFT
	RGB(255, 126, 0),   // NUI_SKELETON_POSITION_FOOT_LEFT
	RGB(181, 165, 213), // NUI_SKELETON_POSITION_HIP_RIGHT
	RGB(71, 222,  76),  // NUI_SKELETON_POSITION_KNEE_RIGHT
	RGB(245, 228, 156), // NUI_SKELETON_POSITION_ANKLE_RIGHT
	RGB(77,  109, 243)  // NUI_SKELETON_POSITION_FOOT_RIGHT
};

static const COLORREF g_SkeletonColors[NUI_SKELETON_COUNT] =
{
	RGB( 255, 0, 0),
	RGB( 0, 255, 0 ),
	RGB( 64, 255, 255 ),
	RGB( 255, 255, 64 ),
	RGB( 255, 64, 255 ),
	RGB( 128, 128, 255 )
};

//lookups for color tinting based on player index
static const int g_IntensityShiftByPlayerR[] = { 1, 2, 0, 2, 0, 0, 2, 0 };
static const int g_IntensityShiftByPlayerG[] = { 1, 2, 2, 0, 2, 0, 0, 1 };
static const int g_IntensityShiftByPlayerB[] = { 1, 0, 2, 2, 0, 2, 0, 2 };

//-------------------------------------------------------------------
// StartKinectProcessing
//
// Do two things:
// 1.  Start up Kinect processing
// 2.  Start the Skeletal Viewer window
//-------------------------------------------------------------------
DWORD WINAPI StartKinectProcessing(LPVOID lpParam)
{
	// Start up portions that should be started up with or without
	// the actual skeletal viewer window starting
	
	// Initialize KinectUI variables
	distanceInMM = 0;
	// There's no need to NULL out these things, since we're just always initializing them anyways.
	// Start up gesture detectors
	for (int ii = 0; ii < NUI_SKELETON_COUNT; ii++)
	{
		gestureDetectors[ii] = new GestureDetector(ii);
	}

	// Start up movement timer and handler
	MoveAndMagnifyHandler* movementHandler = new MoveAndMagnifyHandler();
	// Start up the NUI implementation
	nui_impl = new NuiImpl();

	DWORD returnval = 0;
	if (skeletalViewer != NULL)
	{
		// Start up its window
		HINSTANCE hInstance = static_cast<HINSTANCE>(lpParam);
		returnval = skeletalViewer->DisplayWindow(hInstance, /* start maximized */ 3);
		// Now that it's finished, release the memory
		delete skeletalViewer;		
		skeletalViewer = NULL;
	}

	// Don't quit until you're told to
//	Sleep(INFINITE);
	while (! quit_properly)
	{
		Sleep(1000);
	}

	// Free gesture detectors
	for (int ii = 0; ii < NUI_SKELETON_COUNT; ii++)
	{
		delete gestureDetectors[ii];
	}
	// And the timer
	delete movementHandler;
	// And the NUI implementation
	delete nui_impl;
	nui_impl = NULL;
	
	return returnval;
}

//-------------------------------------------------------------------
// DisplayWindow
//
// Converted version of the entry point of the application, so that I
// can instead start it from Magnifier
//-------------------------------------------------------------------
int CSkeletalViewerApp::DisplayWindow(HINSTANCE hInstance, int nCmdShow)
{
	MSG       msg;
	WNDCLASS  wc;

	// unique mutex, if it already exists there is already an instance of this app running
	// in that case we want to show the user an error dialog
	HANDLE hMutex = CreateMutex( NULL, FALSE, (LPCSTR) INSTANCE_MUTEX_NAME );
	if ( (hMutex != NULL) && (GetLastError() == ERROR_ALREADY_EXISTS) ) 
	{
		TCHAR szAppTitle[256] = { 0 };
		TCHAR szRes[512] = { 0 };

		//load the app title
		LoadString( hInstance, IDS_APPTITLE, szAppTitle, _countof(szAppTitle) );

		//load the error string
		LoadString( hInstance, IDS_ERROR_APP_INSTANCE, szRes, _countof(szRes) );

		MessageBox( NULL, szRes, szAppTitle, MB_OK | MB_ICONHAND );

		CloseHandle(hMutex);
		return -1;
	}

	// Store the instance handle
	m_hInstance = hInstance;

	// Dialog custom window class
	ZeroMemory( &wc,sizeof(wc) );
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_SKELETALVIEWER));
	wc.lpfnWndProc = DefDlgProc;
	wc.lpszClassName = SZ_APPDLG_WINDOW_CLASS;
	if( !RegisterClass(&wc) )
	{
		return 0;
	}

	// Create main application window
	HWND hWndApp = CreateDialogParam(
		hInstance,
		MAKEINTRESOURCE(IDD_APP),
		NULL,
		(DLGPROC) CSkeletalViewerApp::MessageRouter, 
		reinterpret_cast<LPARAM>(this));

	// Show window
	ShowWindow(hWndApp,nCmdShow);

	// Main message loop:
	while( GetMessage( &msg, NULL, 0, 0 ) ) 
	{
		// If a dialog message will be taken care of by the dialog proc
		if ( (hWndApp != NULL) && IsDialogMessage(hWndApp, &msg) )
		{
			continue;
		}

		// otherwise do our window processing
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CloseHandle( hMutex );

	return static_cast<int>(msg.wParam);
}

//-------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------
CSkeletalViewerApp::CSkeletalViewerApp(NuiImpl* nui_impl) : m_hInstance(NULL)
{
	nui = nui_impl;
	ZeroMemory(m_szAppTitle, sizeof(m_szAppTitle));
	LoadString(m_hInstance, IDS_APPTITLE, m_szAppTitle, _countof(m_szAppTitle));

	m_fUpdatingUi = false;
	SV_Zero();

	// Init Direct2D
	D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory );
}

//-------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------
CSkeletalViewerApp::~CSkeletalViewerApp()
{
	// Clean up Direct2D
	SafeRelease( m_pD2DFactory );

	SV_Zero();
}

void CSkeletalViewerApp::ClearComboBox()
{
	for ( long i = 0; i < SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_GETCOUNT, 0, 0); i++ )
	{
		SysFreeString( reinterpret_cast<BSTR>( SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_GETITEMDATA, i, 0) ) );
	}
	SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_RESETCONTENT, 0, 0);
}

void CSkeletalViewerApp::UpdateComboBox()
{
	m_fUpdatingUi = true;
	ClearComboBox();

	int numDevices = 0;
	HRESULT hr = NuiGetSensorCount(&numDevices);

	if ( FAILED(hr) )
	{
		return;
	}

	EnableWindow(GetDlgItem(m_hWnd, IDC_APPTRACKING), numDevices > 0);

	long selectedIndex = 0;
	for ( int i = 0; i < numDevices; i++ )
	{
		INuiSensor *pNui = NULL;
		HRESULT hr = NuiCreateSensorByIndex(i,  &pNui);
		if (SUCCEEDED(hr))
		{
			HRESULT status = pNui ? pNui->NuiStatus() : E_NUI_NOTCONNECTED;
			if (status == E_NUI_NOTCONNECTED)
			{
				pNui->Release();
				continue;
			}

			WCHAR kinectName[MAX_PATH];
			StringCchPrintfW( kinectName, _countof(kinectName), L"Kinect %d", i);
			long index = static_cast<long>( SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kinectName)) );
			SendDlgItemMessage( m_hWnd, IDC_CAMERAS, CB_SETITEMDATA, index, reinterpret_cast<LPARAM>(pNui->NuiUniqueId()) );
			if (nui->m_pNuiSensor && pNui == nui->m_pNuiSensor)
			{
				selectedIndex = index;
			}
			pNui->Release();
		}
	}

	SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_SETCURSEL, selectedIndex, 0);
	m_fUpdatingUi = false;
}

void CSkeletalViewerApp::UpdateTrackingComboBoxes()
{
	SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_RESETCONTENT, 0, 0);

	SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"0"));
	SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"0"));

	SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_SETITEMDATA, 0, 0);
	SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_SETITEMDATA, 0, 0);

	bool setCombo0 = false;
	bool setCombo1 = false;

	for ( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
	{
		if ( nui->m_SkeletonIds[i] != 0 )
		{
			WCHAR trackingId[MAX_PATH];
			StringCchPrintfW(trackingId, _countof(trackingId), L"%d", nui->m_SkeletonIds[i]);
			LRESULT pos0 = SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(trackingId));
			LRESULT pos1 = SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(trackingId));

			SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_SETITEMDATA, pos0, nui->m_SkeletonIds[i]);
			SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_SETITEMDATA, pos1, nui->m_SkeletonIds[i]);

			if ( nui->m_TrackedSkeletonIds[0] == nui->m_SkeletonIds[i] )
			{
				SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_SETCURSEL, pos0, 0);
				setCombo0 = true;
			}

			if ( nui->m_TrackedSkeletonIds[1] == nui->m_SkeletonIds[i] )
			{
				SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_SETCURSEL, pos1, 0);
				setCombo1 = true;
			}
		}
	}

	if ( !setCombo0 )
	{
		SendDlgItemMessage( m_hWnd, IDC_TRACK0, CB_SETCURSEL, 0, 0 );
	}

	if ( !setCombo1 )
	{
		SendDlgItemMessage( m_hWnd, IDC_TRACK1, CB_SETCURSEL, 0, 0 );
	}
}

void CSkeletalViewerApp::UpdateTrackingFromComboBoxes()
{
	LRESULT trackingIndex0 = SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_GETCURSEL, 0, 0);
	LRESULT trackingIndex1 = SendDlgItemMessage(m_hWnd, IDC_TRACK1, CB_GETCURSEL, 0, 0);

	LRESULT trackingId0 = SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_GETITEMDATA, trackingIndex0, 0);
	LRESULT trackingId1 = SendDlgItemMessage(m_hWnd, IDC_TRACK0, CB_GETITEMDATA, trackingIndex1, 0);

	nui->Nui_SetTrackedSkeletons(static_cast<int>(trackingId0), static_cast<int>(trackingId1));
}

LRESULT CALLBACK CSkeletalViewerApp::MessageRouter( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	CSkeletalViewerApp *pThis = NULL;

	if ( WM_INITDIALOG == uMsg )
	{
		pThis = reinterpret_cast<CSkeletalViewerApp*>(lParam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
	}
	else
	{
		pThis = reinterpret_cast<CSkeletalViewerApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if ( NULL != pThis )
	{
		return pThis->WndProc( hwnd, uMsg, wParam, lParam );
	}

	return 0;
}

//-------------------------------------------------------------------
// WndProc
//
// Handle windows messages
//-------------------------------------------------------------------
LRESULT CALLBACK CSkeletalViewerApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		{
			// Clean state the class
			SV_Zero();
			
			LOGFONT lf;
			
			// Bind application window handle
			m_hWnd = hWnd;

			// Set the font for Frames Per Second display
			GetObject( (HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf );
			lf.lfHeight *= 4;
			m_hFontFPS = CreateFontIndirect(&lf);
			SendDlgItemMessage( hWnd, IDC_FPS, WM_SETFONT, (WPARAM) m_hFontFPS, 0 );

			// Use the same font for everything else as well
			SendDlgItemMessage(hWnd,IDC_DISTANCE,WM_SETFONT,(WPARAM) m_hFontFPS,0);
			SendDlgItemMessage(hWnd,IDC_USER,WM_SETFONT,(WPARAM) m_hFontFPS,0);
			SendDlgItemMessage(hWnd,IDC_MAGNIFY,WM_SETFONT,(WPARAM) m_hFontFPS,0);
			SendDlgItemMessage(hWnd,IDC_MOVEX,WM_SETFONT,(WPARAM) m_hFontFPS,0);
			SendDlgItemMessage(hWnd,IDC_MOVEY,WM_SETFONT,(WPARAM) m_hFontFPS,0);

			// Want a smaller font for the state, since the words are longer
			LOGFONT smallfont;
			GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT),sizeof(smallfont),&smallfont);
			smallfont.lfHeight*=2;
			m_smallFontFPS = CreateFontIndirect(&smallfont);
			SendDlgItemMessage(hWnd,IDC_STATE,WM_SETFONT,(WPARAM) m_smallFontFPS,0);

			UpdateComboBox();
			SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_SETCURSEL, 0, 0);

			// Re-init the class
			SV_Init();
		}
		break;

	case WM_USER_UPDATE_FPS:
		{
			// Last param is whether or not the value is signed
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;

	case WM_USER_UPDATE_DISTANCE:
		{
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;
	case WM_USER_UPDATE_USER:
		{
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;
	case WM_USER_UPDATE_STATE:
		{
			// The LPARAM needs to be a char pointer in this case, so let's poke some holes
			void* tmp = (void*)lParam;
			char* charptr = (char*)tmp;
			::SetDlgItemText( m_hWnd, static_cast<int>(wParam), static_cast<LPCTSTR>(charptr));
		}
		break;
	case WM_USER_UPDATE_MAGNIFICATION:
		{
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;
	case WM_USER_UPDATE_MOVEX:
		{
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;
	case WM_USER_UPDATE_MOVEY:
		{
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;

	case WM_USER_UPDATE_COMBO:
		{
			UpdateComboBox();
		}
		break;

	case WM_COMMAND:
		{
			if( HIWORD( wParam ) == CBN_SELCHANGE )
			{
				switch (LOWORD(wParam))
				{
				case IDC_CAMERAS:
					{
						LRESULT index = ::SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_GETCURSEL, 0, 0);

						// Don't reconnect as a result of updating the combo box
						if ( !m_fUpdatingUi )
						{
							nui->Nui_UnInit();
							SV_UnInit();
							nui->Nui_Zero();
							SV_Zero();
							nui->Nui_Init(reinterpret_cast<BSTR>(::SendDlgItemMessage(m_hWnd, IDC_CAMERAS, CB_GETITEMDATA, index, 0)));
							SV_Init();
						}
					}
					break;

				case IDC_TRACK0:
				case IDC_TRACK1:
					{
						UpdateTrackingFromComboBoxes();
					}
					break;
				}
			}
			else if ( HIWORD( wParam ) == BN_CLICKED )
			{
				switch (LOWORD(wParam))
				{
				case IDC_APPTRACKING:
					{
						bool checked = IsDlgButtonChecked(m_hWnd, IDC_APPTRACKING) == BST_CHECKED;
						nui->m_bAppTracking = checked;

						EnableWindow( GetDlgItem(m_hWnd, IDC_TRACK0), checked );
						EnableWindow( GetDlgItem(m_hWnd, IDC_TRACK1), checked );

						if ( checked )
						{
							UpdateTrackingComboBoxes();
						}

						nui->Nui_SetApplicationTracking(checked);

						if ( checked )
						{
							UpdateTrackingFromComboBoxes();
						}
					}
					break;
				}
			}
		}
		break;

		// If the titlebar X is clicked destroy app
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		// nui->Nui_UnInit();
		SV_UnInit();

		// Other cleanup
		ClearComboBox();
		DeleteObject(m_hFontFPS);
		DeleteObject(m_smallFontFPS);

		// Quit the main message pump
		PostQuitMessage(0);
		break;
	}

	return FALSE;
}

//-------------------------------------------------------------------
// MessageBoxResource
//
// Display a MessageBox with a string table table loaded string
//-------------------------------------------------------------------
int CSkeletalViewerApp::MessageBoxResource( UINT nID, UINT nType )
{
	static TCHAR szRes[512];

	LoadString( m_hInstance, nID, szRes, _countof(szRes) );
	return MessageBox( m_hWnd, szRes, m_szAppTitle, nType );
}

/////////////////////////////////////////////////////////////////////////
//
// Functions moved over from NuiImpl.cpp because they only pertain to
// the Skeletal Viewer moved here.
//
/////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------
// Nui_ShortToQuad_Depth
//
// Get the player colored depth value
//-------------------------------------------------------------------
RGBQUAD CSkeletalViewerApp::Nui_ShortToQuad_Depth( USHORT s )
{
	USHORT RealDepth = NuiDepthPixelToDepth(s);
	USHORT Player    = NuiDepthPixelToPlayerIndex(s);

	// transform 13-bit depth information into an 8-bit intensity appropriate
	// for display (we disregard information in most significant bit)
	BYTE intensity = (BYTE)~(RealDepth >> 4);

	// tint the intensity by dividing by per-player values
	RGBQUAD color;
	color.rgbRed   = intensity >> g_IntensityShiftByPlayerR[Player];
	color.rgbGreen = intensity >> g_IntensityShiftByPlayerG[Player];
	color.rgbBlue  = intensity >> g_IntensityShiftByPlayerB[Player];

	return color;
}

void CSkeletalViewerApp::Nui_BlankSkeletonScreen (HWND hWnd, bool getDC )
{
	HDC hdc = getDC ? GetDC( hWnd ) : m_SkeletonDC;

	RECT rct;
	GetClientRect( hWnd, &rct );
	PatBlt( hdc, 0, 0, rct.right, rct.bottom, BLACKNESS );

	if ( getDC )
	{
		ReleaseDC( hWnd, hdc );
	}
}

void CSkeletalViewerApp::Nui_DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... )
{
	va_list vl;
	va_start(vl,numJoints);

	POINT segmentPositions[NUI_SKELETON_POSITION_COUNT];
	int segmentPositionsCount = 0;

	DWORD polylinePointCounts[NUI_SKELETON_POSITION_COUNT];
	int numPolylines = 0;
	int currentPointCount = 0;

	// Note the loop condition: We intentionally run one iteration beyond the
	// last element in the joint list, so we can properly end the final polyline.
	for ( int iJoint = 0; iJoint <= numJoints; iJoint++ )
	{
		if ( iJoint < numJoints )
		{
			NUI_SKELETON_POSITION_INDEX jointIndex = va_arg( vl, NUI_SKELETON_POSITION_INDEX );

			if ( pSkel->eSkeletonPositionTrackingState[jointIndex] != NUI_SKELETON_POSITION_NOT_TRACKED )
			{
				// This joint is tracked: add it to the array of segment positions.            
				segmentPositions[segmentPositionsCount] = m_Points[jointIndex];
				segmentPositionsCount++;
				currentPointCount++;

				// Fully processed the current joint; move on to the next one
				continue;
			}
		}

		// If we fall through to here, we're either beyond the last joint, or
		// the current joint is not tracked: end the current polyline here.
		if ( currentPointCount > 1 )
		{
			// Current polyline already has at least two points: save the count.
			polylinePointCounts[numPolylines++] = currentPointCount;
		}
		else if ( currentPointCount == 1 )
		{
			// Current polyline has only one point: ignore it.
			segmentPositionsCount--;
		}
		currentPointCount = 0;
	}

#ifdef _DEBUG
	// We should end up with no more points in segmentPositions than the
	// original number of joints.
	assert(segmentPositionsCount <= numJoints);

	int totalPointCount = 0;
	for (int i = 0; i < numPolylines; i++)
	{
		// Each polyline should contain at least two points.
		assert(polylinePointCounts[i] > 1);

		totalPointCount += polylinePointCounts[i];
	}

	// Total number of points in all polylines should be the same as number
	// of points in segmentPositions.
	assert(totalPointCount == segmentPositionsCount);
#endif

	if (numPolylines > 0)
	{
		PolyPolyline( m_SkeletonDC, segmentPositions, polylinePointCounts, numPolylines );
	}

	va_end(vl);
}

void CSkeletalViewerApp::Nui_DrawSkeleton( NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor )
{
	HGDIOBJ hOldObj = SelectObject( m_SkeletonDC, m_Pen[WhichSkeletonColor % m_PensTotal] );

	RECT rct;
	GetClientRect(hWnd, &rct);
	int width = rct.right;
	int height = rct.bottom;

	if ( m_Pen[0] == NULL )
	{
		for (int i = 0; i < m_PensTotal; i++)
		{
			m_Pen[i] = CreatePen( PS_SOLID, width / 80, g_SkeletonColors[i] );
		}
	}

	int i;
	USHORT depth;
	for (i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
	{
		NuiTransformSkeletonToDepthImage( pSkel->SkeletonPositions[i], &m_Points[i].x, &m_Points[i].y, &depth );

		m_Points[i].x = (m_Points[i].x * width) / 320;
		m_Points[i].y = (m_Points[i].y * height) / 240;
	}

	SelectObject(m_SkeletonDC,m_Pen[WhichSkeletonColor%m_PensTotal]);

	Nui_DrawSkeletonSegment(pSkel,4,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD);
	Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);
	Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);
	Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);
	Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);

	// Draw the joints in a different color
	for ( i = 0; i < NUI_SKELETON_POSITION_COUNT ; i++ )
	{
		if ( pSkel->eSkeletonPositionTrackingState[i] != NUI_SKELETON_POSITION_NOT_TRACKED )
		{
			HPEN hJointPen;

			hJointPen = CreatePen( PS_SOLID, 9, g_JointColorTable[i] );
			hOldObj = SelectObject( m_SkeletonDC, hJointPen );

			MoveToEx( m_SkeletonDC, m_Points[i].x, m_Points[i].y, NULL );
			LineTo( m_SkeletonDC, m_Points[i].x, m_Points[i].y );

			SelectObject( m_SkeletonDC, hOldObj );
			DeleteObject( hJointPen );
		}

	}

	if (nui->m_bAppTracking)
	{
		Nui_DrawSkeletonId(pSkel, hWnd, WhichSkeletonColor);
	}

	// Draw the gesture hitboxes
	if (gestureDetectors[WhichSkeletonColor] != NULL)
	{
		// This ensures the proper per-skeleton state.
		GestureDetector* gestureDetector = gestureDetectors[WhichSkeletonColor];
		//// Cancel hitboxes not needed, since hands on head is both self-apparent and hitboxes could be confused with the salute hitboxes
		// Always draw the 'cancel' hitboxes
		// HPEN hCancelPen;
		// hCancelPen = CreatePen(PS_DOT, 1, RGB(0,0,255));
		// hOldObj = SelectObject(m_SkeletonDC, hCancelPen);
		// DrawBox(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT], detectRange/2);
		// DrawBox(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT], detectRange/2);
		// SelectObject( m_SkeletonDC, hOldObj );
		// DeleteObject(hCancelPen);

		// Make a new pen for the gesture hitboxes
		HPEN hGesturePen;
		// Hard-code to red for now
		hGesturePen = CreatePen(PS_DASH, 1, RGB(255,0,0));
		hOldObj = SelectObject(m_SkeletonDC, hGesturePen);

		// Where we draw the box is going to depend on what gesture state we're in
		Vector4 headPoint;
		Vector4 spinePoint;
		// Vector4 magnifyPoint;
		// Vector4 movePoint;
		Vector4 centerPoint;
		Vector4 upPoint, downPoint, leftPoint, rightPoint;

		switch (gestureDetector->state->state)
		{
		case OFF:
			// Draw a detectRange box around the head
			DrawBox(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HEAD], detectRange/2);
			break;
		case SALUTE1:
			// Up and away from the head, both hands
			headPoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
			headPoint.y += saluteUp;
			if (gestureDetector->hand == RIGHT)
			{
				headPoint.x += saluteOver;
			}
			else
			{
				headPoint.x -= saluteOver;
			}
			DrawBox(headPoint, detectRange/2);
			break;
		case SALUTE2:
			spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			centerPoint = spinePoint;
			if (gestureDetector->hand == RIGHT)
			{
				centerPoint.x += centerRightOver;
			} 
			else 
			{
				centerPoint.x -= centerLeftOver;
			}
			if (allowMagnifyGestures)
			{
				DrawBox(spinePoint, detectRange/2);			
				DrawBox(centerPoint, detectRange/2);
			}
			else
			{
				DrawBox(centerPoint, detectRange/2);
			}
			break;
		//case BODYCENTER:
		//	spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		//	centerPoint = spinePoint;
		//	if (gestureDetector->hand == RIGHT)
		//	{
		//		centerPoint.x += centerRightOver;
		//	} 
		//	else 
		//	{
		//		centerPoint.x -= centerLeftOver;
		//	}
		//	DrawBox(centerPoint, detectRange/2);
		//	break;
		case MOVECENTER:
		case MOVEUP:
		case MOVEDOWN:
		case MOVERIGHT:
		case MOVELEFT:
			spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			centerPoint = spinePoint;
			if (gestureDetector->hand == RIGHT)
			{
				centerPoint.x += centerRightOver;
			} 
			else 
			{
				centerPoint.x -= centerLeftOver;
			}
			DrawBox(centerPoint, centerBoxSize/2);
			DrawX(centerPoint);
			break;
			//case MOVE:
			//	spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			//	centerPoint = spinePoint;
			//	if (gestureDetector->hand == RIGHT)
			//	{
			//		centerPoint.x += centerRightOver;
			//	} 
			//	else 
			//	{
			//		centerPoint.x -= centerLeftOver;
			//	}
			//	DrawBox(centerPoint, detectRange/2);
			//	break;
		// case MAGNIFYCENTER:
		// 	spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		// 	centerPoint = spinePoint;
		// 	if (gestureDetector->hand == RIGHT)
		// 	{
		// 		centerPoint.x += centerRightOver;
		// 	} 
		// 	else 
		// 	{
		// 		centerPoint.x -= centerLeftOver;
		// 	}
		// 	upPoint = centerPoint;
		// 	upPoint.y += directionRadius;
		// 	downPoint = centerPoint;
		// 	downPoint.y -= directionRadius;
		// 	rightPoint = centerPoint;
		// 	rightPoint.x += directionRadius;
		// 	leftPoint = centerPoint;
		// 	leftPoint.x -= directionRadius;
		// 	DrawBox(upPoint, detectRange/2);
		// 	DrawBox(downPoint, detectRange/2);
		// 	DrawBox(leftPoint, detectRange/2);
		// 	DrawBox(rightPoint, detectRange/2);
		// 	break;
		case MAGNIFYUP:
		case MAGNIFYDOWN:
			spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			centerPoint = spinePoint;
			if (gestureDetector->hand == RIGHT)
			{
				centerPoint.x += centerRightOver;
			} 
			else 
			{
				centerPoint.x -= centerLeftOver;
			}
			rightPoint = centerPoint;
			rightPoint.x += directionRadius;
			leftPoint = centerPoint;
			leftPoint.x -= directionRadius;
			DrawBox(leftPoint, detectRange/2);
			DrawBox(rightPoint, detectRange/2);
			break;
		case MAGNIFYLEFT:
		case MAGNIFYRIGHT:
			spinePoint = pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			centerPoint = spinePoint;
			if (gestureDetector->hand == RIGHT)
			{
				centerPoint.x += centerRightOver;
			} 
			else 
			{
				centerPoint.x -= centerLeftOver;
			}
			upPoint = centerPoint;
			upPoint.y += directionRadius;
			downPoint = centerPoint;
			downPoint.y -= directionRadius;
			DrawBox(upPoint, detectRange/2);
			DrawBox(downPoint, detectRange/2);
			break;
		}

		// Cleanup
		SelectObject( m_SkeletonDC, hOldObj );
		DeleteObject(hGesturePen);
	}
}

void CSkeletalViewerApp::Nui_DrawSkeletonId( NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor )
{
	RECT rct;
	GetClientRect( hWnd, &rct );

	float fx = 0, fy = 0;

	NuiTransformSkeletonToDepthImage( pSkel->Position, &fx, &fy );

	int skelPosX = (int)( fx * rct.right + 0.5f );
	int skelPosY = (int)( fy * rct.bottom + 0.5f );

	WCHAR number[20];
	size_t length;

	if ( FAILED(StringCchPrintfW(number, ARRAYSIZE(number), L"%d", pSkel->dwTrackingID)) )
	{
		return;
	}

	if ( FAILED(StringCchLengthW(number, ARRAYSIZE(number), &length)) )
	{
		return;
	}

	if ( m_hFontSkeletonId == NULL )
	{
		LOGFONT lf;
		GetObject( (HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf );
		lf.lfHeight *= 2;
		m_hFontSkeletonId = CreateFontIndirect( &lf );
	}

	HGDIOBJ hLastFont = SelectObject( m_SkeletonDC, m_hFontSkeletonId );
	SetTextAlign( m_SkeletonDC, TA_CENTER );
	SetTextColor( m_SkeletonDC, g_SkeletonColors[WhichSkeletonColor] );
	SetBkColor( m_SkeletonDC, RGB(0, 0, 0) );

	TextOutW( m_SkeletonDC, skelPosX, skelPosY, number, (int)length);

	SelectObject( m_SkeletonDC, hLastFont );
}

// Draw a box around a skeletal position
BOOL CSkeletalViewerApp::DrawBox(Vector4& s_point, FLOAT radius)
{
	RECT rct;
	GetClientRect(GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rct);
	int scaleX = rct.right;
	int scaleY = rct.bottom;

	// s_ denotes skeleton-space point
	// Translate it to an image-space point
	Vector4 s_ul = s_point;
	Vector4 s_ur = s_point;
	Vector4 s_ll = s_point;
	Vector4 s_lr = s_point;

	s_ul.x -= radius;
	s_ul.y += radius;
	s_ur.x += radius;
	s_ur.y += radius;
	s_ll.x -= radius;
	s_ll.y -= radius;
	s_lr.x += radius;
	s_lr.y -= radius;

	// Yes, 5 points in a rectangle since we want to join the start and end
	const unsigned int NUM_POINTS = 5;
	Vector4* s_points[NUM_POINTS] = {&s_ul, &s_ur, &s_lr, &s_ll, &s_ul};
	POINT i_points[NUM_POINTS];

	// Convert skeleton-space points to image-space points (i_points)
	LONG lx=0, ly=0;
	for (int i = 0; i < NUM_POINTS; i++)
	{
		USHORT dummy;
		NuiTransformSkeletonToDepthImage( *(s_points[i]), &lx, &ly, &dummy );
		i_points[i].x = (int) ( lx * scaleX) / 320;
		i_points[i].y = (int) ( ly * scaleY) / 240;
	}

	// Actually draw the rectangle from lines
	return Polyline(m_SkeletonDC, i_points, NUM_POINTS);
}

// Draw an X to the corners from the given position
void CSkeletalViewerApp::DrawX(Vector4& s_point)
{
	RECT rct;
	GetClientRect(GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rct);
	int scaleX = rct.right;
	int scaleY = rct.bottom;

	// Convert the point's X and Y to image coordinates
	LONG lx=0, ly=0;
	USHORT dummy;
	NuiTransformSkeletonToDepthImage( s_point, &lx, &ly, &dummy );
	int pointX = (int) ( lx * scaleX) / 320;
	int pointY = (int) ( ly * scaleY) / 240;

	MoveToEx(m_SkeletonDC, pointX, pointY, NULL);
	LineTo(m_SkeletonDC, pointX - scaleX, pointY - scaleY);
	MoveToEx(m_SkeletonDC, pointX, pointY, NULL);
	LineTo(m_SkeletonDC, pointX + scaleX, pointY - scaleY);
	MoveToEx(m_SkeletonDC, pointX, pointY, NULL);
	LineTo(m_SkeletonDC, pointX - scaleX, pointY + scaleY);
	MoveToEx(m_SkeletonDC, pointX, pointY, NULL);
	LineTo(m_SkeletonDC, pointX + scaleX, pointY + scaleY);
}

void CSkeletalViewerApp::Nui_DoDoubleBuffer( HWND hWnd, HDC hDC )
{
	RECT rct;
	GetClientRect(hWnd, &rct);

	HDC hdc = GetDC( hWnd );

	BitBlt( hdc, 0, 0, rct.right, rct.bottom, hDC, 0, 0, SRCCOPY );

	ReleaseDC( hWnd, hdc );
}

//-------------------------------------------------------------------
// SV_Zero
//
// Zero out member variables that pertain only to the skeletal viewer
//-------------------------------------------------------------------
void CSkeletalViewerApp::SV_Zero()
{
	ZeroMemory(m_Pen,sizeof(m_Pen));
	m_SkeletonDC = NULL;
	m_SkeletonBMP = NULL;
	m_SkeletonOldObj = NULL;
	m_PensTotal = 6;
	ZeroMemory(m_Points,sizeof(m_Points));
	m_bScreenBlanked = false;
	m_pDrawDepth = NULL;
	m_pDrawColor = NULL;
}

//-------------------------------------------------------------------
// SV_Init
//
// Perform skeletal-viewer only intialization
//-------------------------------------------------------------------
HRESULT CSkeletalViewerApp::SV_Init()
{
	_ASSERTE(!GUI_On);
	
	HRESULT  hr = 0;
	RECT     rc;
	bool     result;

	GetWindowRect( GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rc );  
	HDC hdc = GetDC( GetDlgItem( m_hWnd, IDC_SKELETALVIEW) );

	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;

	m_SkeletonBMP = CreateCompatibleBitmap( hdc, width, height );
	m_SkeletonDC = CreateCompatibleDC( hdc );

	ReleaseDC(GetDlgItem(m_hWnd,IDC_SKELETALVIEW), hdc );
	m_SkeletonOldObj = SelectObject( m_SkeletonDC, m_SkeletonBMP );

	m_pDrawDepth = new DrawDevice( );
	result = m_pDrawDepth->Initialize( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ), m_pD2DFactory, 320, 240, 320 * 4 );
	if ( !result )
	{
		MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
		return E_FAIL;
	}

	m_pDrawColor = new DrawDevice( );
	result = m_pDrawColor->Initialize( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), m_pD2DFactory, 640, 480, 640 * 4 );
	if ( !result )
	{
		MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
		return E_FAIL;
	}

	// Set up the mutex to allow clean shutdown (FALSE means don't start off with ownership)
	num_GUIers_mutex = CreateMutex(NULL, FALSE, NULL);
	num_GUIers = 0;
	GUI_On = TRUE;

	return hr;
}

//-------------------------------------------------------------------
// SV_UnInit
//
// Uninitialize Kinect
//-------------------------------------------------------------------
void CSkeletalViewerApp::SV_UnInit( )
{
	// Wait until we have no GUIers before unIniting
	GUI_On = FALSE;
	unsigned int timesWaiting = 0;
	unsigned int MAX_TIMES_TO_WAIT = 65536;
	while (1)
	{
		WaitForSingleObject(num_GUIers_mutex, INFINITE);
		if (num_GUIers == 0 || (timesWaiting > MAX_TIMES_TO_WAIT))
		{
			ReleaseMutex(num_GUIers_mutex);
			break;
		}
		else
		{
			timesWaiting++;
		}
	}
	CloseHandle(num_GUIers_mutex);
	
	SelectObject( m_SkeletonDC, m_SkeletonOldObj );
	DeleteDC( m_SkeletonDC );
	DeleteObject( m_SkeletonBMP );
	
	if ( NULL != m_Pen[0] )
	{
		for ( int i = 0; i < NUI_SKELETON_COUNT; i++ )
		{
			DeleteObject( m_Pen[i] );
		}
		ZeroMemory( m_Pen, sizeof(m_Pen) );
	}

	if ( NULL != m_hFontSkeletonId )
	{
		DeleteObject( m_hFontSkeletonId );
		m_hFontSkeletonId = NULL;
	}

	// clean up graphics
	delete m_pDrawDepth;
	m_pDrawDepth = NULL;

	delete m_pDrawColor;
	m_pDrawColor = NULL;    
}

//-------------------------------------------------------------------
// increment_num_GUIers
//
// Increment the num_GUIers variable, mutex-protected
//-------------------------------------------------------------------
int CSkeletalViewerApp::increment_num_GUIers( )
{
	WaitForSingleObject(num_GUIers_mutex, INFINITE);
	num_GUIers++;
	ReleaseMutex(num_GUIers_mutex);

	return 1;
}

//-------------------------------------------------------------------
// decrement_num_GUIers
//
// Decrement the num_GUIers variable, mutex-protected
//-------------------------------------------------------------------
int CSkeletalViewerApp::decrement_num_GUIers( )
{
	WaitForSingleObject(num_GUIers_mutex, INFINITE);
	num_GUIers--;
	ReleaseMutex(num_GUIers_mutex);

	return 1;
}
