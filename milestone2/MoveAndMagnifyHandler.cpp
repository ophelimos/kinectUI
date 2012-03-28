#include "MoveAndMagnifyHandler.h"
#include "SkeletalViewer.h"

// Separated into a separate class, since there should only be one, 
// and throwing a ton of static variables into GestureDetector gets cumbersome

extern float magnificationFloor;
extern CSkeletalViewerApp* skeletalViewer;

// Another method of quitting
HANDLE quitMutex;

// Global variables, so GestureDetector can access them
LONG moveAmount_x;
LONG moveAmount_y;
float magnifyAmount;

// Mutex
extern BOOL GUI_On;

MoveAndMagnifyHandler::MoveAndMagnifyHandler()
{
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

	// Create the mutex for waiting, and initially own it.
	quitMutex = CreateMutex(NULL, FALSE, NULL);
	WaitForSingleObject(quitMutex, INFINITE);
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
void CALLBACK MoveAndMagnifyHandler::TimerHandler(void* /*lpParameter*/, BOOLEAN /*TimerOrWaitFired*/)
{
	// Debug processing only necessary if the skeletal viewer exists
	if (GUI_On && skeletalViewer->increment_num_GUIers())
	{
		// View skeletal viewer handler
		// Stop us from immediately re-enabling after disabling
		static BOOL hideSVButtonReleased = TRUE;
		if (GetAsyncKeyState(VK_HOME))
		{
			if (hideSVButtonReleased)
			{
				hideSVButtonReleased = FALSE;
				if (IsWindowVisible(skeletalViewer->m_hWnd))
				{
					ShowWindow(skeletalViewer->m_hWnd, SW_HIDE);
				}
				else
				{
					ShowWindow(skeletalViewer->m_hWnd, SW_SHOW);
				}
			}
		}
		else
		{
			hideSVButtonReleased = TRUE;
		}

		// Print the amounts
		::PostMessageW(skeletalViewer->m_hWnd, WM_USER_UPDATE_MOVEX, IDC_MOVEX, moveAmount_x);
		::PostMessageW(skeletalViewer->m_hWnd, WM_USER_UPDATE_MOVEY, IDC_MOVEY, moveAmount_y);
		// Unfortunately, it doesn't take floating-point values, so fix-point the thing
		FLOAT magnifyAmountx100 = magnifyAmount * 100;
		int magnifyAmountInt = (int) (magnifyAmountx100);
		::PostMessageW(skeletalViewer->m_hWnd, WM_USER_UPDATE_MAGNIFICATION, IDC_MAGNIFY, magnifyAmountInt);
		skeletalViewer->decrement_num_GUIers();
	}

	if (GetAsyncKeyState(VK_F4))
	{
		ReleaseMutex(quitMutex);
	}

	// Adjust magnification
	magnificationFloor += magnifyAmount;
	// Adjust position
	POINT curPos;
	GetCursorPos(&curPos);
	curPos.x += moveAmount_x;
	curPos.y += moveAmount_y;
	SetCursorPos(curPos.x, curPos.y);

	// Exponentially decrease amounts (friction)
	magnifyAmount /= 2;
	if (MOVEMENT_STYLE == Velocity_Style)
	{
		moveAmount_x /= 2;
		moveAmount_y /= 2;
	}
}
