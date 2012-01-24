#include "GestureDetector.h"
#include <cmath>
#include <winuser.h>

extern int activeSkeleton;
extern float magnificationFloor;

GestureDetector::GestureDetector(HWND assocHwnd)
{
	startTime = getTimeIn100NSIntervals();
	hwnd = assocHwnd;
	state = new GestureState(hwnd);
	state->set(OFF);
}


GestureDetector::~GestureDetector(void)
{
  delete state;
}

void GestureDetector::detect(NUI_SKELETON_FRAME &SkeletonFrame, int skeletonNum)
{
	NUI_SKELETON_DATA SkeletonData = SkeletonFrame.SkeletonData[skeletonNum];
	// The compiler does not like initializing variables within case statements
	Vector4 headPoint;
	Vector4 rightHandPoint;
	Vector4 leftHandPoint;
	Vector4 handPoint;
	Vector4 spinePoint;
	Vector4 upPoint, downPoint, leftPoint, rightPoint;
	long long curTime = 0;
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
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
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
			  activeSkeleton = skeletonNum;
			  startTime = getTimeIn100NSIntervals();
			  return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case SALUTE2:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
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
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			rightPoint = spinePoint;
			rightPoint.x += bodyOver;
			leftPoint = spinePoint;
			leftPoint.x -= bodyOver;
			upPoint = spinePoint;
			upPoint.y += bodyOver;
			downPoint = spinePoint;
			downPoint.y -= bodyOver;
			if (hand == RIGHT)
			{
				handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			}
			else
			{
				handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			}
			if (areClose(rightPoint, handPoint, detectRange))
			{
				state->set(MOVERIGHT);
				startTime = getTimeIn100NSIntervals();
				return;
			}
			if (areClose(leftPoint, handPoint, detectRange))
			{
				state->set(MOVELEFT);
				startTime = getTimeIn100NSIntervals();
				return;
			}
			if (areClose(upPoint, handPoint, detectRange))
			{
				state->set(MOVEUP);
				startTime = getTimeIn100NSIntervals();
				return;
			}
			if (areClose(downPoint, handPoint, detectRange))
			{
				state->set(MOVEDOWN);
				startTime = getTimeIn100NSIntervals();
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MOVERIGHT:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			upPoint = spinePoint;
			upPoint.y += bodyOver;
			downPoint = spinePoint;
			downPoint.y -= bodyOver;
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
				moveCursor(RIGHT);
				return;
			}
			if (areClose(upPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYUP);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			if (areClose(downPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYDOWN);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MOVELEFT:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			upPoint = spinePoint;
			upPoint.y += bodyOver;
			downPoint = spinePoint;
			downPoint.y -= bodyOver;
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
				moveCursor(LEFT);
				return;
			}
			if (areClose(upPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYUP);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			if (areClose(downPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYDOWN);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MOVEUP:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			rightPoint = spinePoint;
			rightPoint.x += bodyOver;
			leftPoint = spinePoint;
			leftPoint.x -= bodyOver;
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
				moveCursor(UP);
				return;
			}
			if (areClose(rightPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYRIGHT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			if (areClose(leftPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYLEFT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MOVEDOWN:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			rightPoint = spinePoint;
			rightPoint.x += bodyOver;
			leftPoint = spinePoint;
			leftPoint.x -= bodyOver;
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
				moveCursor(DOWN);
				return;
			}
			if (areClose(leftPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYLEFT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			if (areClose(rightPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYRIGHT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MAGNIFYRIGHT:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			upPoint = spinePoint;
			upPoint.y += bodyOver;
			downPoint = spinePoint;
			downPoint.y -= bodyOver;
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
			if (areClose(upPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYUP);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			if (areClose(downPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYDOWN);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MAGNIFYLEFT:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			upPoint = spinePoint;
			upPoint.y += bodyOver;
			downPoint = spinePoint;
			downPoint.y -= bodyOver;
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
			if (areClose(upPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYUP);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			if (areClose(downPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYDOWN);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MAGNIFYUP:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			rightPoint = spinePoint;
			rightPoint.x += bodyOver;
			leftPoint = spinePoint;
			leftPoint.x -= bodyOver;
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
			if (areClose(rightPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYRIGHT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			if (areClose(leftPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYLEFT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case MAGNIFYDOWN:
			curTime = getTimeIn100NSIntervals();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
			rightPoint = spinePoint;
			rightPoint.x += bodyOver;
			leftPoint = spinePoint;
			leftPoint.x -= bodyOver;
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
			if (areClose(leftPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYLEFT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor -= magnifyAmount;
				return;
			}
			if (areClose(rightPoint, handPoint, detectRange))
			{
				state->set(MAGNIFYRIGHT);
				startTime = getTimeIn100NSIntervals();
				magnificationFloor += magnifyAmount;
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
	}
}

// Check if two Vector4 objects are within a certain rectangular range of each other
bool GestureDetector::areClose(Vector4 &obj1, Vector4 &obj2, double range)
{
	if ( 
		(abs(obj1.x - obj2.x) < range)
		&& (abs(obj1.y - obj2.y) < range)
		&& (abs(obj1.z - obj2.z) < range)
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

void GestureDetector::moveCursor(Direction dir)
{
	POINT curPos;
	GetCursorPos(&curPos);
	switch (dir)
	{
	case RIGHT:
		curPos.x += moveAmount;
		break;	
	case LEFT:
		curPos.x -= moveAmount;
		break;
	case UP:
		curPos.y += moveAmount;
		break;
	case DOWN:
		curPos.y -= moveAmount;
		break;
	}
	SetCursorPos(curPos.x, curPos.y);
}
