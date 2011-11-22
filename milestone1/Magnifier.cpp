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

// Ensure that the following definition is in effect before winuser.h is included.
#define _WIN32_WINNT 0x0501    

#include <windows.h>
#include <wincodec.h>
#include <magnification.h>
#include "SkeletalViewer.h"

#define RESTOREDWINDOWSTYLES WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CAPTION | WS_MAXIMIZEBOX

// Global variables and strings.
HINSTANCE           hInst;
float               MagFactor;
const TCHAR         WindowClassName[]= TEXT("MagnifierWindow");
const TCHAR         WindowTitle[]= TEXT("Screen Magnifier");
const UINT          timerInterval = 16; // close to the refresh rate @60hz
HWND                hwndMag;
HWND                hwndView;
HWND                hwndHost;
RECT                magWindowRect;
RECT                hostWindowRect;

// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK       UpdateMagWindow(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void                GoFullScreen();
int                 CaptureAnImage(HWND hwnd);
float               GetMagnificationFactor();
BOOL                isFullScreen = FALSE;

extern int distanceInMM;

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
	// Start up a separate thread that handles the Kinect stuff
	// StartSkeletalViewer(hInstance);
	CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
            StartSkeletalViewer,       // thread function name
            hInstance,          // argument to thread function 
            0,                      // use default creation flags 
            NULL);   // returns the thread identifier
    if (FALSE == MagInitialize())
    {
        return 0;
    }
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
    KillTimer(NULL, timerId);
    MagUninitialize();
    return (int) msg.wParam;
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
            CaptureAnImage(GetDesktopWindow());
            ShowCursor(true);
            PostQuitMessage(0);
            break;
        }        
        else if (wParam == VK_F1)
        {
            GoFullScreen();
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
        CaptureAnImage(GetDesktopWindow());
        ShowCursor(true);
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        if ( hwndMag != NULL )
        {
            GetClientRect(hWnd, &magWindowRect);
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
// FUNCTION: SetupMagnifier
//
// PURPOSE: Creates the windows and initializes magnification.
//
BOOL SetupMagnifier(HINSTANCE hinst)
{
    // Set bounds of host window according to screen size.
    hostWindowRect.top = 0;
    hostWindowRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    hostWindowRect.left = 0;
    hostWindowRect.right = GetSystemMetrics(SM_CXSCREEN);

    // Create the host window.
    RegisterHostWindowClass(hinst);
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
        WS_CHILD | MS_SHOWMAGNIFIEDCURSOR | WS_VISIBLE ,
        magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, hwndHost, NULL, hInst, NULL );
    if (!hwndMag)
    {
        return FALSE;
    }

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
// FUNCTION: UpdateMagWindow()
//
// PURPOSE: Sets the source rectangle and updates the window. Called by a timer.
//
void CALLBACK UpdateMagWindow(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/)
{
    POINT mousePoint;
    GetCursorPos(&mousePoint);

    int width = (int)((magWindowRect.right - magWindowRect.left) / MagFactor);
    int height = (int)((magWindowRect.bottom - magWindowRect.top) / MagFactor);
    RECT sourceRect;
    sourceRect.left = mousePoint.x - width / 2;
    sourceRect.top = mousePoint.y -  height / 2;

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

    // Set the source rectangle for the magnifier control.
    MagSetWindowSource(hwndMag, sourceRect);

    // Reclaim topmost status, to prevent unmagnified menus from remaining in view. 
    SetWindowPos(hwndHost, HWND_TOPMOST, 0, 0, 0, 0, 
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
}

int CaptureAnImage(HWND hWnd)
{
    HDC hdcScreen;
    HDC hdcWindow;
    HDC hdcMemDC = NULL;
    HBITMAP hbmScreen = NULL;
    BITMAP bmpScreen;

    // Retrieve the handle to a display device context for the client 
    // area of the window. 
    hdcScreen = GetDC(NULL);
    hdcWindow = GetDC(hWnd);

    // Create a compatible DC which is used in a BitBlt from the window DC
    hdcMemDC = CreateCompatibleDC(hdcWindow); 

    // Get the client area for size calculation
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    //This is the best stretch mode
    SetStretchBltMode(hdcWindow,HALFTONE);

    //The source DC is the entire screen and the destination DC is the current window (HWND)
    StretchBlt(hdcWindow, 
               0,0, 
               rcClient.right, rcClient.bottom, 
               hdcScreen, 
               0,0,
               GetSystemMetrics (SM_CXSCREEN),
               GetSystemMetrics (SM_CYSCREEN),
               SRCCOPY);

    // Create a compatible bitmap from the Window DC
    hbmScreen = CreateCompatibleBitmap(hdcWindow, rcClient.right-rcClient.left, rcClient.bottom-rcClient.top);

    // Select the compatible bitmap into the compatible memory DC.
    SelectObject(hdcMemDC,hbmScreen);
    
    // Bit block transfer into our compatible memory DC.
    BitBlt(hdcMemDC, 
               0,0, 
               rcClient.right-rcClient.left, rcClient.bottom-rcClient.top, 
               hdcWindow, 
               0,0,
               SRCCOPY);

    // Get the BITMAP from the HBITMAP
    GetObject(hbmScreen,sizeof(BITMAP),&bmpScreen);
     
    BITMAPFILEHEADER   bmfHeader;    
    BITMAPINFOHEADER   bi;
     
    bi.biSize = sizeof(BITMAPINFOHEADER);    
    bi.biWidth = bmpScreen.bmWidth;    
    bi.biHeight = bmpScreen.bmHeight;  
    bi.biPlanes = 1;    
    bi.biBitCount = 32;    
    bi.biCompression = BI_RGB;    
    bi.biSizeImage = 0;  
    bi.biXPelsPerMeter = 0;    
    bi.biYPelsPerMeter = 0;    
    bi.biClrUsed = 0;    
    bi.biClrImportant = 0;

    DWORD dwBmpSize = ((bmpScreen.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

    // Starting with 32-bit Windows, GlobalAlloc and LocalAlloc are implemented as wrapper functions that 
    // call HeapAlloc using a handle to the process's default heap. Therefore, GlobalAlloc and LocalAlloc 
    // have greater overhead than HeapAlloc.
    HANDLE hDIB = GlobalAlloc(GHND,dwBmpSize); 
    char *lpbitmap = (char *)GlobalLock(hDIB);    

    // Gets the "bits" from the bitmap and copies them into a buffer 
    // which is pointed to by lpbitmap.
    GetDIBits(hdcWindow, hbmScreen, 0,
        (UINT)bmpScreen.bmHeight,
        lpbitmap,
        (BITMAPINFO *)&bi, DIB_RGB_COLORS);

    // A file is created, this is where we will save the screen capture.
    HANDLE hFile = CreateFile("captureqwsx.bmp",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);   
    
    // Add the size of the headers to the size of the bitmap to get the total file size
    DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
 
    //Offset to where the actual bitmap bits start.
    bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER); 
    
    //Size of the file
    bmfHeader.bfSize = dwSizeofDIB; 
    
    //bfType must always be BM for Bitmaps
    bmfHeader.bfType = 0x4D42; //BM   
 
    DWORD dwBytesWritten = 0;
    WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);
    
    //Unlock and Free the DIB from the heap
    GlobalUnlock(hDIB);    
    GlobalFree(hDIB);

    //Close the handle for the file that was created
    CloseHandle(hFile);
     
    DeleteObject(hbmScreen);
    DeleteObject(hdcMemDC);
    ReleaseDC(NULL,hdcScreen);
    ReleaseDC(hWnd,hdcWindow);

    return 0;
}


float GetMagnificationFactor(){
    if (distanceInMM == 0)
	{
		return 2.0f;
	}
	else
	{
		float convertedDistance = (distanceInMM / 1000.0f) * 1.5f;
		return convertedDistance;
	}
}