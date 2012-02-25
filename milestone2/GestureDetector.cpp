#include "GestureDetector.h"
#include "SkeletalViewer.h"
#include <cmath>
#include <winuser.h>

extern int activeSkeleton;
extern LONG moveAmount_x;
extern LONG moveAmount_y;
extern float magnifyAmount;
// Including Magnifier.h is a *bad* idea, since it's not namespaced away in a class.  That really should be done at some point.
void HideMagnifier();
extern int hideWindowTimeout;

GestureDetector::GestureDetector(HWND assocHwnd, int userId)
{
	startTime = getTimeIn100NSIntervals();
	hwnd = assocHwnd;
	id = userId;
	state = new GestureState(hwnd, userId);
	state->set(OFF);
}

GestureDetector::~GestureDetector(void)
{
	delete state;
}

void GestureDetector::detect(NUI_SKELETON_FRAME &SkeletonFrame, NUI_SKELETON_FRAME &prevFrame, int skeletonNum)
{
	NUI_SKELETON_DATA SkeletonData = SkeletonFrame.SkeletonData[skeletonNum];
	NUI_SKELETON_DATA prevSkeletonData = prevFrame.SkeletonData[skeletonNum];
	/*** The compiler does not like initializing variables within case statements ***/
	// Temporary variables for actual body part points
	Vector4 headPoint;
	Vector4 rightHandPoint;
	Vector4 leftHandPoint;
	Vector4 handPoint;
	Vector4 spinePoint;
	Vector4 rightShoulderPoint;
	Vector4 leftShoulderPoint;
	// These are derived points used for magnification and movement
	Vector4 upPoint;
	Vector4 downPoint;
	Vector4 leftPoint;
	Vector4 rightPoint;
	Vector4 centerPoint;
	FLOAT displacement_x = 0;
	FLOAT displacement_y = 0;
	long long curTime = 0;

	// Do as much as we can before entering the state machine, because long states are confusing

	// Most states are only applicable if we're the active skeleton
	if ( (state->state != OFF || state->state != SALUTE1) && id != activeSkeleton)
	{
		state->set(OFF);
		return;
	}

	// Timeout any state other than OFF
	curTime = getTimeIn100NSIntervals();
	if ( (curTime - startTime) > timeout )
	{
		state->set(OFF);
	}

	// If they make the "stop" gesture, turn off gesture recognition and magnification period
	// In this case the "stop" gesture is hands _crossed_ and touching the shoulders
	rightHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	leftHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	rightShoulderPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
	leftShoulderPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
	if (areClose(leftShoulderPoint, rightHandPoint, detectRange) && areClose(rightShoulderPoint, leftHandPoint, detectRange))
	{
		// Stop us from immediately re-enabling after disabling
		if (hideWindowTimeout < 30)
		{
			hideWindowTimeout++;
		} 
		else
		{
			HideMagnifier();
			state->set(OFF);
		}
	}

	// If they make the "cancel" gesture, stop recognizing gestures
	// Cancel gesture here is both hands touching.
	if (areClose(leftHandPoint, rightHandPoint, handsTogether))
	{
		state->set(OFF);
	}

	switch (state->state)
	{
	case OFF:
		// Check if a hand is close to the head
		headPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
		rightHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
		leftHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];

		if (areClose(headPoint, rightHandPoint, detectRange))
		{
			state->set(SALUTE1);
			startTime = getTimeIn100NSIntervals();
			hand = RIGHT;
			return;
		}
		if (areClose(headPoint, leftHandPoint, detectRange))
		{
			state->set(SALUTE1);
			startTime = getTimeIn100NSIntervals();
			hand = LEFT;
			return;
		}
		break;
	case SALUTE1:
		// Check for saluting action (box up and away)
		headPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
		headPoint.y += saluteUp;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			headPoint.x += saluteOver;
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			headPoint.x -= saluteOver;
		}

		if (areClose(headPoint, handPoint, detectRange))
		{
			state->set(SALUTE2);
			// Right now, an alert to let me know gesture tracking is working
			//MessageBox(NULL, "Salute detected", "Gesture Detection", NULL);
			// Change the active user
			// This is a race condition, but what it's doing is also inherently one
			activeSkeleton = skeletonNum;
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case SALUTE2:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
		}
		if (areClose(spinePoint, handPoint, detectRange))
		{
			state->set(BODYCENTER);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case BODYCENTER:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerOver;
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerOver;
		}
		if (areClose(centerPoint, handPoint, detectRange))
		{
			state->set(MOVECENTER);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case MOVECENTER:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
		}
		// Place the direction arrows
		upPoint = centerPoint;
		upPoint.y += directionRadius;
		if (areClose(upPoint, handPoint, detectRange))
		{
			state->set(MOVE);
			moveAmount_y += 500*displacement_y;
			startTime = getTimeIn100NSIntervals();
			return;
		}
		downPoint = centerPoint;
		downPoint.y -= directionRadius;
		if (areClose(downPoint, handPoint, detectRange))
		{
			state->set(MOVE);
			moveAmount_y += 500*displacement_y;
			startTime = getTimeIn100NSIntervals();
			return;
		}
		rightPoint = centerPoint;
		rightPoint.x += directionRadius;
		if (areClose(rightPoint, handPoint, detectRange))
		{
			state->set(MOVE);
			moveAmount_x -= 500*displacement_x;
			startTime = getTimeIn100NSIntervals();
			return;
		}
		leftPoint = centerPoint;
		leftPoint.x -= directionRadius;
		if (areClose(leftPoint, handPoint, detectRange))
		{
			state->set(MOVE);
			moveAmount_x -= 500*displacement_x;
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case MOVE:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerOver;
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerOver;
		}
		// Back to MOVECENTER
		if (areClose(centerPoint, handPoint, detectRange))
		{
			state->set(MOVECENTER);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
	// case MAGNIFY:
	// 	spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
	// 	magnifyPoint = spinePoint;
	// 	if (hand == RIGHT)
	// 	{
	// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	// 		magnifyPoint.x += magnifyOver;
	// 		// Add velocity
	// 		getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
	// 	}
	// 	else
	// 	{
	// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	// 		magnifyPoint.x -= magnifyOver;
	// 		// Add velocity
	// 		getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
	// 	}
	// 	// Back to BODYCENTER (can switch to MOVE)
	// 	if (areClose(spinePoint, handPoint, detectRange))
	// 	{
	// 		state->set(BODYCENTER);
	// 		startTime = getTimeIn100NSIntervals();
	// 		return;
	// 	}
	// 	// Keep magnifying, adjust the magnification amount
	// 	if (areClose(magnifyPoint, handPoint, detectRange))
	// 	{
	// 		startTime = getTimeIn100NSIntervals();
	// 		// Up is less magnification, Down is more magnification
	// 		magnifyAmount -= 2*displacement_y;
	// 		return;
	// 	}
	// 	// Otherwise, keep looking (until the timeout)
	// 	break;
	}
}

// Check if two Vector4 objects are within a certain rectangular (switched to 2-d, since it's more intuititve) range of each other
bool GestureDetector::areClose(Vector4 &obj1, Vector4 &obj2, double range)
{
	//if ( 
	//	(abs(obj1.x - obj2.x) < range)
	//	&& (abs(obj1.y - obj2.y) < range)
	//	&& (abs(obj1.z - obj2.z) < range)
	//	)
	//{
	//	return true;
	//}

	if ( 
		(abs(obj1.x - obj2.x) < range)
		&& (abs(obj1.y - obj2.y) < range)
		)
	{
		return true;
	}

	return false;
}

// As usual, this is much uglier than it needs to be.  Blame Microsoft.
long long GestureDetector::getTimeIn100NSIntervals()
{
	SYSTEMTIME st;
	FILETIME ft;
	LARGE_INTEGER li;    
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	return li.QuadPart;
}

//void GestureDetector::moveCursor(Direction dir)
//{
//	POINT curPos;
//	GetCursorPos(&curPos);
//	switch (dir)
//	{
//	case RIGHT:
//		curPos.x += moveAmount;
//		break;	
//	case LEFT:
//		curPos.x -= moveAmount;
//		break;
//	case UP:
//		curPos.y += moveAmount;
//		break;
//	case DOWN:
//		curPos.y -= moveAmount;
//		break;
//	}
//	SetCursorPos(curPos.x, curPos.y);
//}

// Figure out the distance between the two points, and therefore, how
// quickly the point has moved
void GestureDetector::getDifference(Vector4 now, Vector4 prev, FLOAT& displacement_x, FLOAT& displacement_y)
{
	// Manhattan distance.  I could do Euclidean, but I don't
	// see the point, and this is faster.

	// Signed, since we need to determine which direction the velocity is in.

	// Only 2-D, since 3-D ends up being non-intuitive.
	displacement_x = prev.x - now.x;
	displacement_y = prev.y - now.y;
}
