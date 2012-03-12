/************************************************************************
*                                                                       *
*   Magnifier.h -- Header for Magnifier.cpp                             *
*                                                                       *
*                                                                       *
************************************************************************/

#pragma once

// Ensure that the following definition is in effect before winuser.h is included.
// This ensures that WM_INPUT definitions can be used.
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

// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
ATOM                RegisterViewfinderWindowClass(HINSTANCE hInstance);
ATOM                RegisterLensWindowClass(HINSTANCE hInstance);
ATOM                RegisterOverlayWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
BOOL                SetupViewfinder(HINSTANCE hinst);
BOOL                SetupLens(HINSTANCE hinst);
BOOL                SetupOverlay(HINSTANCE hinst);
LRESULT CALLBACK    HostWndProc(HWND, UINT, WPARAM, LPARAM);
BOOL                UpdateLens();
void CALLBACK       UpdateMagWindow(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void                GoFullScreen();
void                ApplyLensRestrictions (RECT sourceRect);
float               GetMagnificationFactor();
RECT                GetSourceRect ();
void				HideMagnifier();
void                drawRectangle(int ulx, int uly, int width, int height, int c);
void                drawText(float x1, float y1, WCHAR string[]);
void                drawRectangle(int x1, int y1, int width, int height, int c);
void                clearOverlay();
