#include "MoveAndMagnifyHandler.h"
#include "SkeletalViewer.h"

// Separated into a separate class, since there should only be one, 
// and throwing a ton of static variables into GestureDetector gets cumbersome

extern float magnificationFloor;
extern int hideWindowTimeout;

// Global variables, so GestureDetector can access them
LONG moveAmount_x;
LONG moveAmount_y;
float magnifyAmount;

// Stupid static declaration because TimerHandler needs to be static
HWND MoveAndMagnifyHandler::hwnd = NULL;

MoveAndMagnifyHandler::MoveAndMagnifyHandler(HWND assocHwnd)
{
	hwnd = assocHwnd;

	// Initialize movement amounts to initially zero
	magnifyAmount = 0.0;
	moveAmount_x = 0;
	moveAmount_y = 0;
	// We could use the default queue, but this is more compartmentalized
	hMovementTimerQueue = CreateTimerQueue();

	BOOL success = ::CreateTimerQueueTimer(
		&hTimerHandle,
		hMovementTimerQueue,
		MoveAndMagnifyHandler::TimerHandler,
		NULL,
		movementTimeoutInMs,
		movementTimeoutInMs,
		// Execute in the timer thread, since we never want two of these to happen at the same time, 
		// and it's better for the timing to be off than for that to happen
		WT_EXECUTEINTIMERTHREAD);

	if (! success)
	{
		exit(1);
	}
}

MoveAndMagnifyHandler::~MoveAndMagnifyHandler(void)
{
	// destroy the timer
	DeleteTimerQueueTimer(hMovementTimerQueue, hTimerHandle, NULL);
	DeleteTimerQueue(hMovementTimerQueue);
	//CloseHandle (hMovementTimerQueue);
	//CloseHandle (hTimerHandle);
}

// Short function to handle moving and magnifying every timer interval
void CALLBACK MoveAndMagnifyHandler::TimerHandler(void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	// View skeletal viewer handler
	// Stop us from immediately re-enabling after disabling
	if (hideWindowTimeout < 30)
	{
		hideWindowTimeout++;
	}
	else if (GetAsyncKeyState(VK_HOME))
	{
		if (IsWindowVisible(hwnd))
		{
			ShowWindow(hwnd, SW_HIDE);
		}
		else
		{
			ShowWindow(hwnd, SW_SHOW);
		}
		hideWindowTimeout = 0;
	}

	// Print the amounts
	::PostMessageW(hwnd, WM_USER_UPDATE_MOVEX, IDC_MOVEX, moveAmount_x);
	::PostMessageW(hwnd, WM_USER_UPDATE_MOVEY, IDC_MOVEY, moveAmount_y);
	// Unfortunately, it doesn't take floating-point values, so fix-point the thing
	FLOAT magnifyAmountx100 = magnifyAmount * 100;
	int magnifyAmountInt = (int) (magnifyAmountx100);
	::PostMessageW(hwnd, WM_USER_UPDATE_MAGNIFICATION, IDC_MAGNIFY, magnifyAmountInt);

	// Adjust magnification
	magnificationFloor += magnifyAmount;
	// Adjust position
	POINT curPos;
	GetCursorPos(&curPos);
	// Negative, because it feels more intuitive
	curPos.x += moveAmount_x;
	curPos.y += moveAmount_y;
	SetCursorPos(curPos.x, curPos.y);

	// Exponentially decrease amounts (friction)
	magnifyAmount /= 2;
	moveAmount_x /= 2;
	moveAmount_y /= 2;
}
