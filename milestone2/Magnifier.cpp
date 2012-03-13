/*************************************************************************************************
*
* File: Magnifier.cpp
*
* Description: Implements a simple control that magnifies the screen, using the 
* Magnification API.
*
* The magnification window is quarter-screen by default but can be resized.
* To make it full-screen, use the Maximize button or double-click the caption
* bar. To return to partial-screen mode, click on the application icon in the 
* taskbar and press ESC. 
*
* In full-screen mode, all keystrokes and mouse clicks are passed through to the
* underlying focused application. In partial-screen mode, the window can receive the 
* focus. 
*
* Multiple monitors are not supported.
*
* 
* Requirements: To compile, link to Magnification.lib. The sample must be run with 
* elevated privileges.
*
* The sample is not designed for multimonitor setups.
* 
*  This file is part of the Microsoft WinfFX SDK Code Samples.
* 
*  Copyright (C) Microsoft Corporation.  All rights reserved.
* 
* This source code is intended only as a supplement to Microsoft
* Development Tools and/or on-line documentation.  See these other
* materials for detailed information regarding Microsoft code samples.
* 
* THIS CODE AND INFORMATION ARE PROVIDED AS IS WITHOUT WARRANTY OF ANY
* KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
* 
*************************************************************************************************/
#include "Magnifier.h"
#include "GestureDetector.h"

// Make it easier to pick what part of the program you want to run.
enum ProgramMode
{
	MAGNIFIER_ONLY,
	KINECT_ONLY,
	KINECT_AND_MAGNIFIER
};

const ProgramMode mode = KINECT_AND_MAGNIFIER;
//const ProgramMode mode = MAGNIFIER_ONLY;

// Global variables and strings.
HINSTANCE           hInst;
float               MagFactor;
const TCHAR         WindowClassName[]= TEXT("MagnifierWindow");
const TCHAR         ViewfinderClassName[]= TEXT("ViewfinderWindow");
const TCHAR         LensClassName[]= TEXT("LensWindow");
const TCHAR         OverlayClassName[]= TEXT("GestureOverlayWindow");
const TCHAR         WindowTitle[]= TEXT("Screen Magnifier");
const TCHAR         ViewWindowTitle[]= TEXT("Viewfinder");
const TCHAR         LensWindowTitle[]= TEXT("Lens");
const TCHAR         OverlayWindowTitle[]= TEXT("Overlay");
const UINT          timerInterval = 16; // close to the refresh rate @60hz 16
HWND                hwndMag;
HWND                hwndViewfinder;
HWND                hwndLens;
HWND                hwndHost;
HWND                hwndOverlay;
RECT                magWindowRect;
RECT                lensWindowRect;
RECT                viewfinderWindowRect;
RECT                overlayWindowRect;
RECT                hostWindowRect;
BOOL                isMagnifierOff = FALSE;
BOOL                allowMagnifyGestures = FALSE;
BOOL				showOverlays = TRUE;
float               magnificationFloor = 0.0f;
int                 distanceInMM = 0;
BOOL                isFullScreen = FALSE;
int					xRes = GetSystemMetrics(SM_CXVIRTUALSCREEN);
int					yRes = GetSystemMetrics(SM_CYVIRTUALSCREEN);
extern int			activeSkeleton;

//
// FUNCTION: WinMain()
//
// PURPOSE: Entry point for the application.
//
int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE /*hPrevInstance*/,
	LPSTR     /*lpCmdLine*/,
	int       nCmdShow)
{
	if (mode == KINECT_ONLY)
	{
		StartSkeletalViewer(hInstance);
	} 
	else if (mode != MAGNIFIER_ONLY)
	{
		// Start up a separate thread that handles the Kinect stuff
		// StartSkeletalViewer(hInstance);
		CreateThread( 
			NULL,                   // default security attributes
			0,                      // use default stack size  
			StartSkeletalViewer,       // thread function name
			hInstance,          // argument to thread function 
			0,                      // use default creation flags 
			NULL);   // returns the thread identifier
	}
	if ((mode == MAGNIFIER_ONLY) || (mode == KINECT_AND_MAGNIFIER))
	{
		if (FALSE == MagInitialize())
		{
			return 0;
		}
        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR           gdiplusToken;
   
        // Initialize GDI+.
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		if (FALSE == SetupMagnifier(hInstance))
		{
			return 0;
		}        

		ShowWindow(hwndHost, nCmdShow);
		UpdateWindow(hwndHost);
		//ShowCursor(false);
		// Create a timer to update the control.
		UINT_PTR timerId = SetTimer(hwndHost, 0, timerInterval, UpdateMagWindow);

		// Main message loop.
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Shut down.
        GdiplusShutdown(gdiplusToken);
		KillTimer(NULL, timerId);
		MagUninitialize();
		return (int) msg.wParam;
	}
	return 0;
}

//
// FUNCTION: HostWndProc()
//
// PURPOSE: Window procedure for the window that hosts the magnifier control.
//
LRESULT CALLBACK HostWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			ShowCursor(true);
			PostQuitMessage(0);
			break;
		}        
		else if (wParam == VK_F1)
		{
			GoFullScreen();
		}
        else if (wParam == VK_F6)
		{
			// int size = 200;
			// drawRectangle ((xRes/2) - (size/2), (yRes/2) - (size/2), size, size, 0);
			// drawRectangle (50,50,200,100,0);
			drawTrapezoid(xRes*3/4 - (boxLarge/2), yRes/2 - (boxLarge/2), Q_TOP, 0);
			drawTrapezoid(xRes*3/4 - (boxLarge/2), yRes/2 - (boxLarge/2), Q_BOTTOM, 0);
			drawTrapezoid(xRes*3/4 - (boxLarge/2), yRes/2 - (boxLarge/2), Q_RIGHT, 1);
			drawTrapezoid(xRes*3/4 - (boxLarge/2), yRes/2 - (boxLarge/2), Q_LEFT, 1);
			drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
		}
        else if (wParam == VK_F7)
		{
			int sizeSmall = 100;

			int overlayCircleRadius = 150;
			
			// Center
			drawRectangle ((xRes*3/4) - (sizeSmall/2), (yRes/2) - (sizeSmall/2), sizeSmall, sizeSmall, 1);
			// Vert
			drawRectangle ((xRes*3/4) - (sizeSmall/2), (yRes/2) - (sizeSmall/2) - overlayCircleRadius, sizeSmall, sizeSmall, 1);
			drawRectangle ((xRes*3/4) - (sizeSmall/2), (yRes/2) - (sizeSmall/2) + overlayCircleRadius, sizeSmall, sizeSmall, 1);
			// Horiz
			drawRectangle ((xRes*3/4) - (sizeSmall/2) - overlayCircleRadius, (yRes/2) - (sizeSmall/2), sizeSmall, sizeSmall, 1);
			drawRectangle ((xRes*3/4) - (sizeSmall/2) + overlayCircleRadius, (yRes/2) - (sizeSmall/2), sizeSmall, sizeSmall, 1);

			// drawRectangle ((xRes*3/4) - (sizeLarge/2), (yRes/2) - (sizeLarge/2), sizeLarge, sizeLarge, 1);
		}
        else if (wParam == VK_F8)
		{
			clearOverlay();
		}

	case WM_SYSCOMMAND: 
		if (GET_SC_WPARAM(wParam) == SC_MAXIMIZE) 
		{ 
			GoFullScreen(); 
		} 
		else 
		{ 
			return DefWindowProc(hWnd, message, wParam, lParam); 
		} 
		break; 

	case WM_DESTROY:
		ShowCursor(true);
		PostQuitMessage(0);
		break;

	case WM_SIZE:
		if ( hwndMag != NULL )
		{
			GetClientRect(hwndHost, &magWindowRect);
			// Resize the control to fill the window.
			SetWindowPos(hwndMag, NULL, 
				magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, 0);
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;  
}

//
//  FUNCTION: RegisterHostWindowClass()
//
//  PURPOSE: Registers the window class for the window that contains the magnification control.
//
ATOM RegisterHostWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = {};

	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = HostWndProc;
	wcex.hInstance      = hInstance;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(1 + COLOR_BTNFACE);
	wcex.lpszClassName  = WindowClassName;

	return RegisterClassEx(&wcex);
}

//
//  FUNCTION: RegisterViewfinderWindowClass()
//
//  PURPOSE: Registers the window class for the window that contains the magnification control.
//
ATOM RegisterViewfinderWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = {};

	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = HostWndProc;
	wcex.hInstance      = hInstance;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	//wcex.hbrBackground  = (HBRUSH)(1 + COLOR_MENU);
    wcex.hbrBackground  = CreateSolidBrush(RGB(100, 180, 65));
	wcex.lpszClassName  = ViewfinderClassName;

	return RegisterClassEx(&wcex);
}

//
//  FUNCTION: RegisterLensWindowClass()
//
//  PURPOSE: Registers the window class for the window that contains the magnification control.
//
ATOM RegisterLensWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = {};

	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = HostWndProc;
	wcex.hInstance      = hInstance;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(1 + COLOR_BTNFACE);
	wcex.lpszClassName  = LensClassName;

	return RegisterClassEx(&wcex);
}

//
//  FUNCTION: RegisterLensWindowClass()
//
//  PURPOSE: Registers the window class for the window that contains the magnification control.
//
ATOM RegisterOverlayWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = {};

	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = HostWndProc;
	wcex.hInstance      = hInstance;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = CreateSolidBrush(RGB(255, 255, 255));
	wcex.lpszClassName  = OverlayClassName;

	return RegisterClassEx(&wcex);
}


//
// FUNCTION: UpdateMagnificationFactor()
//
// PURPOSE: Change the amount the window is magnified
//
BOOL UpdateMagnificationFactor()
{
	MagFactor = GetMagnificationFactor();
	// Set the magnification factor.
	MAGTRANSFORM matrix;
	memset(&matrix, 0, sizeof(matrix));
	matrix.v[0][0] = MagFactor;
	matrix.v[1][1] = MagFactor;
	matrix.v[2][2] = 1.0f;

	BOOL ret = MagSetWindowTransform(hwndMag, &matrix);
	return ret;
}

//
// FUNCTION: SetupMagnifier
//
// PURPOSE: Creates the windows and initializes magnification.
//
BOOL SetupMagnifier(HINSTANCE hInst)
{
	// Set bounds of host window according to screen size.
	hostWindowRect.top = 0;
	hostWindowRect.bottom = GetSystemMetrics(SM_CYSCREEN);
	hostWindowRect.left = 0;
	hostWindowRect.right = GetSystemMetrics(SM_CXSCREEN);

	// Set the floor to 0
	magnificationFloor = 0;

	// Create the host and viewfinder windows.
	RegisterHostWindowClass(hInst);
	RegisterViewfinderWindowClass(hInst);
	RegisterLensWindowClass(hInst);
    RegisterOverlayWindowClass(hInst);

	hwndHost = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, 
		WindowClassName, WindowTitle, 
		RESTOREDWINDOWSTYLES,
		0, 0, hostWindowRect.right, hostWindowRect.bottom, NULL, NULL, hInst, NULL);
	if (!hwndHost)
	{
		return FALSE;
	}
	SetWindowLong(hwndHost, GWL_STYLE,  WS_POPUP);
	// Make the window opaque.
	SetLayeredWindowAttributes(hwndHost, 0, 255, LWA_ALPHA);

	// Create a magnifier control that fills the client area.
	GetClientRect(hwndHost, &magWindowRect);
	hwndMag = CreateWindow(WC_MAGNIFIER, TEXT("MagnifierWindow"), 
		WS_CHILD | WS_VISIBLE | MS_SHOWMAGNIFIEDCURSOR,
		magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, hwndHost, NULL, hInst, NULL );
	if (!hwndMag)
	{
		return FALSE;
	}
    
	UpdateMagnificationFactor();
	SetupViewfinder(hInst);
	SetupLens(hInst);
    SetupOverlay(hInst);
	GoFullScreen();

    HWND list[] = {hwndViewfinder, hwndLens, hwndOverlay};
	MagSetWindowFilterList(hwndMag, MW_FILTERMODE_EXCLUDE, 3, list);

	return UpdateMagnificationFactor();
}

//
// FUNCTION: SetupViewfinder
//
// PURPOSE: Creates the Viewfinder window
//
BOOL SetupViewfinder(HINSTANCE hInst)
{
	HDC hDC = GetDC(NULL);
	int xRes = GetSystemMetrics(SM_CXSCREEN);
	int yRes = GetSystemMetrics(SM_CYSCREEN);
	ReleaseDC(NULL, hDC);
    
	// Set bounds of viewfinder window according to screen size.
	viewfinderWindowRect.top = yRes-(yRes/5);        
	//viewfinderWindowRect.bottom = yRes;
	viewfinderWindowRect.bottom = yRes/5;
	viewfinderWindowRect.left = 0;
	viewfinderWindowRect.right = xRes/5;
    
	hwndViewfinder = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, 
		ViewfinderClassName, ViewWindowTitle, 
		WS_VISIBLE | WS_POPUP,
		viewfinderWindowRect.left, viewfinderWindowRect.top, viewfinderWindowRect.right, viewfinderWindowRect.bottom, NULL, NULL, hInst, NULL);
	if (!hwndViewfinder)
	{
		return FALSE;
	}
	SetLayeredWindowAttributes(hwndViewfinder, 0, 150, LWA_ALPHA);      

	return TRUE;
}

void ApplyLensRestrictions (RECT sourceRect){

	lensWindowRect.left = viewfinderWindowRect.left + (sourceRect.left/5);
	lensWindowRect.top = viewfinderWindowRect.top + (sourceRect.top/5);
	lensWindowRect.right = (LONG) ((sourceRect.right-sourceRect.left)/5);
	lensWindowRect.bottom = (LONG) ((sourceRect.bottom-sourceRect.top)/5);

	if (lensWindowRect.left + lensWindowRect.right > viewfinderWindowRect.left + viewfinderWindowRect.right){
		lensWindowRect.left = viewfinderWindowRect.left + viewfinderWindowRect.right - lensWindowRect.right;
	}

	if (lensWindowRect.top + lensWindowRect.bottom > viewfinderWindowRect.top + viewfinderWindowRect.bottom){
		lensWindowRect.top = viewfinderWindowRect.top + viewfinderWindowRect.bottom - lensWindowRect.bottom;
	}
	return;
}

//
// FUNCTION: SetupViewfinder
//
// PURPOSE: Creates the Viewfinder window
//
BOOL SetupLens(HINSTANCE hInst)
{       
	ApplyLensRestrictions (GetSourceRect());
    
	hwndLens = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, 
		LensClassName, LensWindowTitle, 
		WS_VISIBLE | WS_POPUP | WS_BORDER | WS_THICKFRAME,
		lensWindowRect.left, lensWindowRect.top, lensWindowRect.right, lensWindowRect.bottom, NULL, NULL, hInst, NULL);

	if (!hwndLens)
	{
		return FALSE;
	}
	SetLayeredWindowAttributes(hwndLens, 0, 180, LWA_ALPHA);  

	return TRUE;
}

//
// FUNCTION: SetupOverlay
//
// PURPOSE: Creates the Gesture Overlay window
//
BOOL SetupOverlay(HINSTANCE hInst)
{
    int xRes = GetSystemMetrics(SM_CXSCREEN);
	int yRes = GetSystemMetrics(SM_CYSCREEN);
        
	overlayWindowRect.top = 0;        	
	overlayWindowRect.bottom = yRes;
	overlayWindowRect.left = 0;
	overlayWindowRect.right = xRes;

	hwndOverlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, 
		OverlayClassName, OverlayWindowTitle, 
		WS_VISIBLE | WS_POPUP,
		overlayWindowRect.left, overlayWindowRect.top, overlayWindowRect.right, overlayWindowRect.bottom, NULL, NULL, hInst, NULL);

	if (!hwndOverlay)
	{
		return FALSE;
	}
	SetLayeredWindowAttributes(hwndOverlay, 0, 180, LWA_ALPHA);  

    ShowWindow(hwndOverlay, SW_HIDE);

	return TRUE;
}

BOOL UpdateLens()
{
	RECT source;
	MagGetWindowSource(hwndMag,&source);
	ApplyLensRestrictions (source);

	SetWindowPos(hwndLens, NULL, 
		lensWindowRect.left, 
		lensWindowRect.top, 
		lensWindowRect.right, 
		lensWindowRect.bottom, 
		SWP_NOACTIVATE|SWP_NOREDRAW);

	return TRUE;
}

RECT GetSourceRect (){

	int width = (int)((magWindowRect.right - magWindowRect.left) / MagFactor);
	int height = (int)((magWindowRect.bottom - magWindowRect.top) / MagFactor);
	POINT mousePoint;
	GetCursorPos(&mousePoint);
	RECT sourceRect;
	sourceRect.left = mousePoint.x - width / 2;
	sourceRect.top = mousePoint.y - height / 2;

	// Don't scroll outside desktop area.
	if (sourceRect.left < 0)
	{
		sourceRect.left = 0;
	}
	if (sourceRect.left > GetSystemMetrics(SM_CXSCREEN) - width)
	{
		sourceRect.left = GetSystemMetrics(SM_CXSCREEN) - width;
	}
	sourceRect.right = sourceRect.left + width;

	if (sourceRect.top < 0)
	{
		sourceRect.top = 0;
	}
	if (sourceRect.top > GetSystemMetrics(SM_CYSCREEN) - height)
	{
		sourceRect.top = GetSystemMetrics(SM_CYSCREEN) - height;
	}        
	sourceRect.bottom = sourceRect.top + height;

	return sourceRect;
}

//
// FUNCTION: UpdateMagWindow()
//
// PURPOSE: Sets the source rectangle and updates the window. Called by a timer.
//
void CALLBACK UpdateMagWindow(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/)
{
	static BOOL hideWindowButtonReleased = TRUE;
	if (GetAsyncKeyState(VK_END))
	{
		if (hideWindowButtonReleased)
		{
			hideWindowButtonReleased = FALSE;
			HideMagnifier();
			if (isMagnifierOff)
			{
				return;
			}
		}
	}
	else
	{
		hideWindowButtonReleased = TRUE;
	}

	static BOOL hideOverlaysButtonReleased = TRUE;
	if (GetAsyncKeyState(VK_F10))
	{
		if (hideOverlaysButtonReleased)
		{
			hideOverlaysButtonReleased = FALSE;
			if (showOverlays)
			{
				clearOverlay();
				showOverlays = FALSE;
			}
			else
			{
				showOverlays = TRUE;
			}
		}
	}
	else
	{
		hideOverlaysButtonReleased = TRUE;
	}

	static BOOL allowMagnifyGesturesButtonReleased = TRUE;
	if (GetAsyncKeyState(VK_F9)) // Requested magnify gestures toggle
	{
		if (allowMagnifyGesturesButtonReleased)
		{
			allowMagnifyGesturesButtonReleased = FALSE;
			if (allowMagnifyGestures)
			{
				allowMagnifyGestures = FALSE;
			}
			else
			{
				allowMagnifyGestures = TRUE;
			}
		}
	}
	else
	{
		allowMagnifyGesturesButtonReleased = TRUE;
	}

	// Kill switch
	if (GetAsyncKeyState(VK_HOME) && GetAsyncKeyState(VK_END))
	{
		exit(0);
	}

	UpdateMagnificationFactor();
	RECT sourceRect = GetSourceRect();
	// Set the source rectangle for the magnifier control.
	MagSetWindowSource(hwndMag, sourceRect);

	UpdateLens();
	//UpdateOverlay();

	// Reclaim topmost status, to prevent unmagnified menus from remaining in view. 
	SetWindowPos(hwndHost, HWND_TOPMOST, 0, 0, 0, 0, 
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
	// Make viewfinder topmost window. 
	SetWindowPos(hwndViewfinder, HWND_TOPMOST, NULL, NULL, NULL, NULL, 
		     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
	// Make lens topmost window. 
	SetWindowPos(hwndLens, HWND_TOPMOST, NULL, NULL, NULL, NULL, 
		     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );    
    // Make lens topmost window. 
	SetWindowPos(hwndOverlay, HWND_TOPMOST, NULL, NULL, NULL, NULL, 
		     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );    


	// Force redraw.
	InvalidateRect(hwndMag, NULL, TRUE);   
    
}


//
// FUNCTION: GoFullScreen()
//
// PURPOSE: Makes the host window full-screen by placing non-client elements outside the display.
//
void GoFullScreen()
{
	isFullScreen = TRUE;
	//ShowCursor(false);
	// The window must be styled as layered for proper rendering. 
	// It is styled as transparent so that it does not capture mouse clicks.
	SetWindowLong(hwndHost, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT);
	// Give the window a system menu so it can be closed on the taskbar.
	//SetWindowLong(hwndHost, GWL_STYLE,  WS_CAPTION | WS_SYSMENU);

	// Calculate the span of the display area.
	HDC hDC = GetDC(NULL);
	int xSpan = GetSystemMetrics(SM_CXSCREEN);
	int ySpan = GetSystemMetrics(SM_CYSCREEN);
	ReleaseDC(NULL, hDC);

	// Calculate the size of system elements.
	int xBorder = GetSystemMetrics(SM_CXFRAME);
	// int yCaption = GetSystemMetrics(SM_CYCAPTION);
	int yBorder = GetSystemMetrics(SM_CYFRAME);

	// Calculate the window origin and span for full-screen mode.
	int xOrigin = 0;
	int yOrigin = 0;

	xSpan += xBorder;
	ySpan += yBorder;

	SetWindowPos(hwndHost, HWND_TOP, xOrigin, yOrigin, xSpan, ySpan, 
		SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);

	GetClientRect(hwndHost, &magWindowRect);
}

//
// FUNCTION: HideMagnifier()
//
// PURPOSE: Hide all the magnification windows to allow users to "turn off magnification"
//
void HideMagnifier()
{
  if (IsWindowVisible(hwndMag))
    {
      ShowWindow(hwndMag, SW_HIDE);
      ShowWindow(hwndViewfinder, SW_HIDE);
      ShowWindow(hwndLens, SW_HIDE);
      ShowWindow(hwndHost, SW_HIDE);
      isMagnifierOff = FALSE;
    }
  else
    {
      ShowWindow(hwndMag, SW_SHOW);
      ShowWindow(hwndViewfinder, SW_SHOW);
      ShowWindow(hwndLens, SW_SHOW);
      ShowWindow(hwndHost, SW_SHOW);
      isMagnifierOff = TRUE;
    }
}

void clearOverlay()
{
	if (IsWindowVisible(hwndOverlay))
	{
		Graphics g(hwndOverlay);    
		SolidBrush brush(Color(255, 255, 255, 255));
    
		g.FillRectangle( &brush, overlayWindowRect.left, overlayWindowRect.top, overlayWindowRect.right, overlayWindowRect.bottom );        
		ShowWindow(hwndOverlay, SW_HIDE);
	}

	return;
}

Status drawText(int x1, int y1, WCHAR string[], int size)
{
	if (! IsWindowVisible(hwndOverlay))
	{
		ShowWindow(hwndOverlay, SW_SHOW);
	}
	Graphics g(hwndOverlay);
	FontFamily  fontFamily(L"Arial");
	Font        font(&fontFamily, (float) size, FontStyleRegular, UnitPixel);
	PointF      pointF((float) x1, (float) y1);
	SolidBrush  solidBrush(Color(255, 128, 255, 128));

	// Create a solid background so the text is visible
	SolidBrush backgroundBrush(Color(255, 0, 0, 0));
	Rect backgroundRect(x1 - size, y1, size * strlen( (const char*) string) * 13, size);
	g.FillRectangle(&backgroundBrush, backgroundRect);

	Status result = g.DrawString(string, -1, &font, pointF, &solidBrush);
    
	return result;
}

void drawRectangle(int ulx, int uly, int width, int height, int c)
{
	if (! IsWindowVisible(hwndOverlay))
	{
		ShowWindow(hwndOverlay, SW_SHOW);
	}
	Graphics g(hwndOverlay);
	int green = c*255;
	int red = abs(green-255);
	Pen pen(Color(255, (BYTE) red, (BYTE) green, 0), 10.0f);

	Rect rectangle(ulx, uly, width, height);    
	g.DrawRectangle( &pen, rectangle );        

	return;
}

Status drawTrapezoid(int ulx, int uly, Quadrant quad, int on)
{
	const int pointsInTrapezoid = 5;

	if (! IsWindowVisible(hwndOverlay))
	{
		ShowWindow(hwndOverlay, SW_SHOW);
	}

	Graphics g(hwndOverlay);
	BYTE red, green;
	if (on)
	{
		red = 0;
		green = 255;
	}
	else
	{
		red = 255;
		green = 0;
	}
	Pen pen( Color(255, red, green, 0), 10.0f);

	int maxDist = 0;
	if (xRes > yRes)
	{
		maxDist = xRes;
	}
	else
	{
		maxDist = yRes;
	}

	if (quad == Q_TOP)
	{
		Point trapezoidPoints[pointsInTrapezoid] = {
			Point(ulx, uly),
			Point(ulx - maxDist, uly - maxDist),
			Point(ulx + boxLarge + maxDist, uly - maxDist),
			Point(ulx + boxLarge, uly),
			Point(ulx, uly)};
		return g.DrawPolygon(&pen, trapezoidPoints, pointsInTrapezoid);
	}
	else if (quad == Q_BOTTOM)
	{
		Point trapezoidPoints[pointsInTrapezoid] = {
			Point(ulx, uly + boxLarge),
			Point(ulx - maxDist, uly + boxLarge + maxDist),
			Point(ulx + boxLarge + maxDist, uly + boxLarge + maxDist),
			Point(ulx + boxLarge, uly + boxLarge),
			Point(ulx, uly + boxLarge)};
		return g.DrawPolygon(&pen, trapezoidPoints, pointsInTrapezoid);
	}
	else if (quad == Q_RIGHT)
	{
		Point trapezoidPoints[pointsInTrapezoid] = {
			Point(ulx + boxLarge, uly),
			Point(ulx + boxLarge + maxDist, uly - maxDist),
			Point(ulx + boxLarge + maxDist, uly + boxLarge + maxDist),
			Point(ulx + boxLarge, uly + boxLarge),
			Point(ulx + boxLarge, uly)};
		return g.DrawPolygon(&pen, trapezoidPoints, pointsInTrapezoid);
	}
	else if (quad == Q_LEFT)
	{
		Point trapezoidPoints[pointsInTrapezoid] = {
			Point(ulx, uly),
			Point(ulx - maxDist, uly - maxDist),
			Point(ulx - maxDist, uly + boxLarge + maxDist),
			Point(ulx, uly + boxLarge),
			Point(ulx, uly)};
		return g.DrawPolygon(&pen, trapezoidPoints, pointsInTrapezoid);
	}

	return GenericError;
}

//
// FUNCTION: GetMagnificationFactor()
//
// PURPOSE: Determine how much to magnify the screen by.
//
float GetMagnificationFactor()
{
	// No skeleton, no magnification
	if (activeSkeleton == -1)
	{
		return 1.0f;
	}

	// Make sure the floor isn't a ridiculous value
	if (magnificationFloor > 8.0f)
	{
		magnificationFloor = 8.0f;
	}
	if (magnificationFloor < -8.0f)
	{
		magnificationFloor = -8.0f;
	}

	float convertedDistance = (distanceInMM / 1000.0f) + magnificationFloor;

	// No going nuts with the magnification
	if (convertedDistance < 1.0f)
	{
		magnificationFloor = 1.0f - (distanceInMM / 1000.0f);
		return 1.0f;
	}

	// No going nuts with the magnification
	if (convertedDistance > 32.0f)
	{
		magnificationFloor = 32.0f - (distanceInMM / 1000.0f);
		return 32.0f;
	}

	return convertedDistance;
}
