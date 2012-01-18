#include "GestureDetector.h"
#include <cmath>

GestureDetector::GestureDetector(HWND assocHwnd)
{
	startTime = 0;
	hwnd = assocHwnd;
	state = new GestureState(hwnd);
	state->set(OFF);
}


GestureDetector::~GestureDetector(void)
{
  delete state;
}

void GestureDetector::detect(NUI_SKELETON_DATA SkeletonData)
{
	// The compiler does not like initializing variables within case statements
	Vector4 headPoint;
	Vector4 rightHandPoint;
	Vector4 leftHandPoint;
	Vector4 handPoint;
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
				startTime = getTime();
				hand = RIGHT;
				return;
			}
			if (areClose(headPoint, leftHandPoint, detectRange))
			{
				state->set(SALUTE1);
				startTime = getTime();
				hand = LEFT;
				return;
			}
			break;
		case SALUTE1:
			curTime = getTime();
			if ( (curTime - startTime) > timeout )
			{
				state->set(OFF);
				return;
			}
			// Check for saluting action (basically, up and away)
			headPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
			if (hand == RIGHT)
			{
				handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			}
			else
			{
				handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			}
			// Needs to be forward (negative z from the perspective of the kinect), and above (positive y)
			if (
				((handPoint.z - headPoint.z) < saluteForward)
				&& ((handPoint.y - headPoint.y) > saluteUp)
				)
			{
			  state->set(SALUTE2);
				// Insert code for changing the current user.  Right now though, just an alert to let me know gesture tracking is working
				MessageBox(NULL, "Salute detected", "Gesture Detection", NULL);
				return;
			}
			// Otherwise, keep looking (until the timeout)
			break;
		case SALUTE2:
		  state->set(OFF);
		  return;
	}
}

// Check if two Vector4 objects are within a certain rectangular range of each other
bool GestureDetector::areClose(Vector4 obj1, Vector4 obj2, double range)
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
long long GestureDetector::getTime()
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
