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
const long long timeout = 20000000;
const double saluteOver = 0.3;
const double saluteUp = 0.2;

enum Hand {
	RIGHT,
	LEFT
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
	Hand hand;
	/* State */
	GestureState* state;
	/* Necessary for debugging */
	HWND hwnd;

	/* Functions */
	void detect(NUI_SKELETON_FRAME &SkeletonFrame, int skeletonNum);
	bool areClose(Vector4 &obj1, Vector4 &obj2, double range);
	long long getTimeIn100NSIntervals();
};

