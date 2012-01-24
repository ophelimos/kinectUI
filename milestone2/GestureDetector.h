/************************************************************************
*                                                                       *
*   GestureDetector.h -- Declaration of GestureDetector class           *
*                                                                       *
*   This is a class intended to detect when a user has made a           *
*   gesture and take an appropriate action.                             *
*                                                                       *
************************************************************************/

#pragma once
#include <windows.h>
#include "MSR_NuiApi.h"
#include "GestureState.h"

/* Magic constants */
const FLOAT detectRange = 0.2f;
// 10000000 is about one second
const long long timeout = 50000000;
const FLOAT saluteOver = 0.3f;
const FLOAT saluteUp = 0.2f;
const FLOAT bodyOver = 0.5f;
const LONG moveAmount = 50;
const float magnifyAmount = 0.1f;

enum Direction {
	RIGHT,
	LEFT,
	UP,
	DOWN,
};

class GestureDetector
{
private:

public:
	GestureDetector(HWND assocHwnd);
	~GestureDetector(void);

	/* Class Variables */
	// Gesture detection times out
	long long startTime;
	// Detection is single-handed, but the hand can change
	Direction hand;
	/* State */
	GestureState* state;
	/* Necessary for debugging */
	HWND hwnd;

	/* Functions */
	void detect(NUI_SKELETON_FRAME &SkeletonFrame, int skeletonNum);
	bool areClose(Vector4 &obj1, Vector4 &obj2, double range);
	long long getTimeIn100NSIntervals();
	void GestureDetector::moveCursor(Direction dir);
};

