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

float magnificationFloor;

// Make it easier to pick what part of the program you want to run.
enum ProgramMode
{
	MAGNIFIER_ONLY,
	KINECT_ONLY,
	KINECT_AND_MAGNIFIER
};

const ProgramMode mode = MAGNIFIER_ONLY;

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
	else if (mode == KINECT_AND_MAGNIFIER)
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
		else if (wParam == VK_F5)
		{
			isMagnifierOff = TRUE;
			HideMagnifier();
		}
        else if (wParam == VK_F6)
		{
			drawRectangle (50,50,200,100,0);			
		}
        else if (wParam == VK_F7)
		{
			drawRectangle (100, 100, 300, 150, 1);
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

	HWND list[] = {hwndViewfinder, hwndLens};
	MagSetWindowFilterList(hwndMag, MW_FILTERMODE_EXCLUDE, 2, list);

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
	if (isMagnifierOff)
	{
		// Stop us from immediately re-enabling after disabling
		if (hideWindowTimeout < 30)
		{
			hideWindowTimeout++;
			return;
		}
		if (GetAsyncKeyState(VK_F5))
		{
			isMagnifierOff = FALSE;
			HideMagnifier();
		}
		else
		{
			// Don't bother processing if magnification is off
			return;
		}
	}
	UpdateMagnificationFactor();
	RECT sourceRect = GetSourceRect();
	// Set the source rectangle for the magnifier control.
	MagSetWindowSource(hwndMag, sourceRect);
        
	UpdateLens();    
    
	// Reclaim topmost status, to prevent unmagnified menus from remaining in view. 
	SetWindowPos(hwndHost, HWND_TOPMOST, 0, 0, 0, 0, 
		     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
	// Make viewfinder topmost window. 
	SetWindowPos(hwndViewfinder, HWND_TOPMOST, NULL, NULL, NULL, NULL, 
		     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
	// Make lens topmost window. 
	SetWindowPos(hwndLens, HWND_TOPMOST, NULL, NULL, NULL, NULL, 
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
	int windowState = ShowWindow(hwndMag, SW_HIDE);
	if (windowState == 0) // Previously hidden
	{
		ShowWindow(hwndMag, SW_SHOW);
		ShowWindow(hwndViewfinder, SW_SHOW);
		ShowWindow(hwndLens, SW_SHOW);
		ShowWindow(hwndHost, SW_SHOW);
	}
	else  // Previously visible
	{
		ShowWindow(hwndViewfinder, SW_HIDE);
		ShowWindow(hwndLens, SW_HIDE);
		ShowWindow(hwndHost, SW_HIDE);
	}
	hideWindowTimeout = 0;
}

void clearOverlay(){
    Graphics g(hwndOverlay);    
    SolidBrush brush(Color(255, 255, 255, 255));
    
    g.FillRectangle( &brush, overlayWindowRect.left, overlayWindowRect.top, overlayWindowRect.right, overlayWindowRect.bottom );        
    ShowWindow(hwndOverlay, SW_HIDE);
    isOverlayOff = TRUE;
    return;
}

int drawRectangle(int x1, int y1, int width, int height, int c)
{
    if (isOverlayOff){
        clearOverlay();
        ShowWindow(hwndOverlay, SW_SHOW);
        isOverlayOff = FALSE;
    }
    Graphics g(hwndOverlay);
    int green = c*255;
    int red = abs(green-255);
    Pen pen(Color(255, red, green, 0), 10);

    Rect rectangle(x1, y1, width, height);    
    g.DrawRectangle( &pen, rectangle );        
    return 1;
}

//
// FUNCTION: GetMagnificationFactor()
//
// PURPOSE: Determine how much to magnify the screen by.
//
float GetMagnificationFactor(){
	if (distanceInMM < 1000)
	{
		return 1.0f;
	}
	else
	{
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
}
