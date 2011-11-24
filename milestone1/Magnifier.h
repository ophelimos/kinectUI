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
const TCHAR         ViewfinderClassName[]= TEXT("ViewfinderWindow");
const TCHAR         WindowTitle[]= TEXT("Screen Magnifier");
const TCHAR         ViewWindowTitle[]= TEXT("Viewfinder");
const UINT          timerInterval = 16; // close to the refresh rate @60hz
HWND                hwndMag;
HWND                hwndViewfinder;
HWND                hwndHost;
RECT                magWindowRect;
RECT                viewfinderWindowRect;
RECT                hostWindowRect;

// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
ATOM                RegisterViewfinderWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
BOOL                SetupViewfinder(HINSTANCE hinst);
LRESULT CALLBACK    HostWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ViewWndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK       UpdateMagWindow(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void                GoFullScreen();
int                 CaptureAnImage(HWND hwnd);
float               GetMagnificationFactor();
BOOL                isFullScreen = FALSE;

extern int distanceInMM;
