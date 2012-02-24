#pragma once
#include <windows.h>

// Time in milliseconds between instances of movement happening
const DWORD movementTimeoutInMs = 100;

class MoveAndMagnifyHandler
{
public:
	MoveAndMagnifyHandler(HWND assocHwnd);
	~MoveAndMagnifyHandler(void);

	static void CALLBACK TimerHandler(void* lpParameter, BOOLEAN TimerOrWaitFired);
	HANDLE hMovementTimerQueue;
	HANDLE hTimerHandle;
	static HWND hwnd;
};

