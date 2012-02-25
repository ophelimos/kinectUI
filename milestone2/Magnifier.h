/************************************************************************
*                                                                       *
*   Magnifier.h -- Header for Magnifier.cpp                             *
*                                                                       *
*                                                                       *
************************************************************************/

#pragma once

// Ensure that the following definition is in effect before winuser.h is included.
#define _WIN32_WINNT 0x0501    

#include <windows.h>
#include <wincodec.h>
#include <objidl.h>
#include <gdiplus.h>
#include <magnification.h>
#include "SkeletalViewer.h"

using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

#define RESTOREDWINDOWSTYLES WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CAPTION | WS_MAXIMIZEBOX

// Global variables and strings.
HINSTANCE           hInst;
float               MagFactor;
const TCHAR         WindowClassName[]= TEXT("MagnifierWindow");
const TCHAR         ViewfinderClassName[]= TEXT("ViewfinderWindow");
const TCHAR         LensClassName[]= TEXT("LensWindow");
const TCHAR         WindowTitle[]= TEXT("Screen Magnifier");
const TCHAR         ViewWindowTitle[]= TEXT("Viewfinder");
const TCHAR         LensWindowTitle[]= TEXT("Lens");
const UINT          timerInterval = 16; // close to the refresh rate @60hz 16
HWND                hwndMag;
HWND                hwndViewfinder;
HWND                hwndLens;
HWND                hwndHost;
RECT                magWindowRect;
RECT                lensWindowRect;
RECT                viewfinderWindowRect;
RECT                hostWindowRect;
BOOL				isMagnifierOff = FALSE;
int					hideWindowTimeout = 0;

// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
ATOM                RegisterViewfinderWindowClass(HINSTANCE hInstance);
ATOM                RegisterLensWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
BOOL                SetupViewfinder(HINSTANCE hinst);
BOOL                SetupLens(HINSTANCE hinst);
LRESULT CALLBACK    HostWndProc(HWND, UINT, WPARAM, LPARAM);
BOOL                UpdateLens();
void CALLBACK       UpdateMagWindow(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void                GoFullScreen();
void                ApplyLensRestrictions (RECT sourceRect);
float               GetMagnificationFactor();
RECT                GetSourceRect ();
BOOL                isFullScreen = FALSE;
void				HideMagnifier();
int                 drawRectangle(int x1, int y1, int width, int height, int c);

extern int distanceInMM;