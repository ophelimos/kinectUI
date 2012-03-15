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
#include "NuiApi.h"
#include "GestureState.h"

/* Mode selection, because Karan likes one method and I like another */
enum Movement_Style {
	Velocity_Style,
	Constant_Style,
};
const enum Movement_Style MOVEMENT_STYLE = Constant_Style;

/* Magic constants */
const FLOAT detectRange = 0.20f;
// A little bit smaller, since otherwise accidental hits are too easy
const FLOAT handsTogether = 0.1f;
// 10000000 is about one second
const long long oneSecondTimeout = 10000000;
const long long timeout = 10*oneSecondTimeout;
const FLOAT saluteOver = 0.25f;
const FLOAT saluteUp = 0.15f;
const FLOAT centerRightOver = 0.30f;
const FLOAT centerLeftOver = 0.25f;
const FLOAT directionRadius = 0.30f;
const int boxSmall = 100;
const int boxLarge = 200;
const int overlayCircleRadius = 150;
/* const FLOAT magnifyOver = 0.5f; */
/* const FLOAT moveDown = 0.2f; */
//const LONG moveAmount = 50;
//const float magnifyAmount = 0.1f;
const FLOAT centerBoxSize = 0.15f;
const int constantMovement = 10;
const FLOAT clickDistance = 0.50;

enum Direction {
	RIGHT,
	LEFT,
	UP,
	DOWN,
};

enum Quadrant {
	Q_TOP,
	Q_BOTTOM,
	Q_LEFT,
	Q_RIGHT,
	Q_CENTER,
};

class GestureDetector
{
private:

public:
	GestureDetector(HWND assocHwnd, int userId);
	~GestureDetector(void);

	/* Class Variables */
	// Gesture detection times out
	long long startTime;
	// Detection is single-handed, but the hand can change
	Direction hand;
	/* State */
	GestureState* state;
	// Now that we have multiple detectors, we need to know which user we refer to
	int id;

	/* Functions */
	void detect(NUI_SKELETON_FRAME &SkeletonFrame, NUI_SKELETON_FRAME &prevFrame);
	bool areClose(Vector4 &obj1, Vector4 &obj2, double range);
	long long getTimeIn100NSIntervals();
	void moveCursor(Direction dir);
	void GestureDetector::getDifference(Vector4 now, Vector4 prev, FLOAT& displacement_x, FLOAT& displacement_y);
	bool GestureDetector::areClose3D(Vector4 &obj1, Vector4 &obj2, double range);
	Quadrant GestureDetector::findQuadrant(Vector4 center, Vector4 point);

	/* Necessary for debugging */
	HWND hwnd;
};
