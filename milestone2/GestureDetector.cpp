#include "GestureDetector.h"
#include "SkeletalViewer.h"
#include <cmath>
#include <winuser.h>
#include "Magnifier.h"

// Disable "conditional expression is constant" warning
#pragma warning( disable : 4127 )

extern int activeSkeleton;
extern FLOAT moveAmount_x;
extern FLOAT moveAmount_y;
extern float magnifyAmount;
BOOL hideWindowOn = FALSE;
extern float magnificationFloor;
extern BOOL allowMagnifyGestures;
extern BOOL showOverlays;
extern int xRes;
extern int yRes;
extern HWND hwndMag;

GestureDetector::GestureDetector(int userId)
{
	startTime = getTimeIn100NSIntervals();
	id = userId;
	state = new GestureState(userId);
	state->set(OFF);
	lockingOn_move = FALSE;
	lockingOn_magnify = FALSE;
	lockonStartTime = 0;
	// killGesturesStartTime = 0;
}

GestureDetector::~GestureDetector(void)
{
	delete state;
}

void GestureDetector::detect(NUI_SKELETON_FRAME &SkeletonFrame, NUI_SKELETON_FRAME &prevFrame)
{
	NUI_SKELETON_DATA SkeletonData = SkeletonFrame.SkeletonData[id];
	NUI_SKELETON_DATA prevSkeletonData = prevFrame.SkeletonData[id];
	/*** The compiler does not like initializing variables within case statements ***/
	// Temporary variables for actual body part points
	Vector4 headPoint;
	Vector4 rightHandPoint;
	Vector4 leftHandPoint;
	Vector4 handPoint;
	Vector4 spinePoint;
	// Vector4 rightShoulderPoint;
	// Vector4 leftShoulderPoint;
	// These are derived points used for magnification and movement
	Vector4 upPoint;
	Vector4 downPoint;
	Vector4 leftPoint;
	Vector4 rightPoint;
	Vector4 centerPoint;
	FLOAT displacement_x = 0;
	FLOAT displacement_y = 0;
	long long curTime = 0;
	Quadrant curQuadrant;

	// Do as much as we can before entering the state machine, because long states are confusing

	// If they make the "stop" gesture, turn off gesture recognition and magnification period

	// // In this case the "stop" gesture is hands _crossed_ and touching the shoulders
	// rightHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	// leftHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	// rightShoulderPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
	// leftShoulderPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
	// if (areClose(leftShoulderPoint, rightHandPoint, detectRange) && areClose(rightShoulderPoint, leftHandPoint, detectRange))

	// Stop gesture is hands on head
	rightHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	leftHandPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	headPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
	if (id == activeSkeleton) // Only if we're the active skeleton - nobody else should be able to kill it
	{
		if (
			areClose3D(headPoint, rightHandPoint, detectRange) 
			&& areClose3D(headPoint, leftHandPoint, detectRange)
			)
		{
			moveAmount_y = 0;
			moveAmount_x = 0;
			// Stop us from immediately re-enabling after disabling (cycling)
			if (! hideWindowOn)
			{
				hideWindowOn = TRUE;
				// Reset magnification value
				magnificationFloor = 0;
				HideMagnifier();
				clearAndHideOverlay();
				state->set(OFF);
			}
		}
		else
		{
			hideWindowOn = FALSE;
		}
	}

	// If the magnifier is off now, don't bother detecting gestures, just stop.
	if (id == activeSkeleton && (! IsWindowVisible(hwndMag)))
	{
		return;
	}

	// Most states are only applicable if we're the active skeleton
	if (id != activeSkeleton)
	{
		// Setting an already OFF state to OFF won't cause issues.
		if (state->state != SALUTE1)
		{
			// Adding a 'return' here will prevent anyone from stealing focus while a gesture is happening
			state->set(OFF);
		}
	}

	// Timeout any state other than OFF
	curTime = getTimeIn100NSIntervals();
	if ( (curTime - startTime) > timeout )
	{
		if (id == activeSkeleton)
		{
			moveAmount_y = 0;
			moveAmount_x = 0;
			clearAndHideOverlay();
			state->set(OFF);
		}
		state->set(OFF);
	}

	// Same thing if they make the "cancel" gesture
	// The cancel gesture is to start the salute again
	static BOOL cancelling = FALSE;
	if (cancelling || (id == activeSkeleton && state->state != SALUTE1 && state->state != OFF && state->state != SALUTE2))
	{
		// Headpoint already initialized above for the stop gesture
		// headPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
		}
		if (areClose3D(headPoint, handPoint, detectRange))
		{
			if (! cancelling)
			{
				cancelling = TRUE;
				moveAmount_y = 0;
				moveAmount_x = 0;
				clearAndHideOverlay();
				state->set(OFF);
			}
			return;
		}
		else
		{
			cancelling = FALSE;
		}
	}

	// If they make the "cancel" gesture, stop recognizing gestures
	// Cancel gesture here is both hands touching.
	// static BOOL cancelling = FALSE;
	// if (id == activeSkeleton
	// 	&& areClose3D(leftHandPoint, rightHandPoint, handsTogether))
	// {
	// 	if ( ! cancelling)
	// 	{
	// 		cancelling = TRUE;
	// 		killGesturesStartTime = getTimeIn100NSIntervals();
	// 	}
	// 	else
	// 	{
	// 		if ( (curTime - killGesturesStartTime) > killGesturesTime)
	// 		{
	// 			moveAmount_y = 0;
	// 			moveAmount_x = 0;
	// 			clearAndHideOverlay();
	// 			state->set(OFF);
	// 		}
	// 	}
	// }
	// else
	// {
	// 	cancelling = FALSE;
	// }

	// Click gesture
	spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
	static BOOL amClicking = FALSE;
	if (id == activeSkeleton && state->state == MOVECENTER
		&&
		(
		((spinePoint.z - rightHandPoint.z) > clickDistance)
		|| ((spinePoint.z - leftHandPoint.z) > clickDistance)
		)
		)
	{
		if (showOverlays)
		{
			int ulx_small;
			int ulx_ss;
			int uly_small = (yRes/2) - (boxSmall/2);
			int uly_ss = (yRes/2) - (boxSuperSmall/2);
			if (hand == RIGHT)
			{
				ulx_small = (xRes*3/4) - (boxSmall/2);
				ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
			}
			else
			{
				ulx_small = (xRes/4) - (boxSmall/2);
				ulx_ss = (xRes/4) - (boxSuperSmall/2);
			}
			// Make the center swirls go red
			drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 0);
			drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 0);
		}
		if (! amClicking)
		{
			// It'd be nice if the screen could flash at this
			// point or something, but that doesn't seem entirely
			// trivial with our current concept of overlays
			mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			amClicking = TRUE;
		}
	}
	else
	{
		amClicking = FALSE;
	}

	switch (state->state)
	{
	case OFF:
		if (id == activeSkeleton)
		{
			moveAmount_y = 0;
			moveAmount_x = 0;
		}
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
			if (showOverlays)
			{
				if (allowMagnifyGestures)
				{
					if (hand == RIGHT)
					{
						drawRectangle ((xRes*3/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
					}
					else
					{
						drawRectangle ((xRes/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
					}
					drawRectangle ((xRes/2) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
				}
				else
				{
					if (hand == RIGHT)
					{
						drawRectangle ((xRes*3/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
					}
					else
					{
						drawRectangle ((xRes/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
					}
				}
			}
			state->set(SALUTE2);
			// Right now, an alert to let me know gesture tracking is working
			//MessageBox(NULL, "Salute detected", "Gesture Detection", NULL);
			// Change the active user
			// This is a race condition, but what it's doing is also inherently one
			if (activeSkeleton != id)
			{
				activeSkeleton = id;
				clearOverlay();
				moveAmount_x = 0;
				moveAmount_y = 0;
			}
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case SALUTE2:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerRightOver;
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerLeftOver;
		}

		// Only allow user magnification if it's turned on
		if (allowMagnifyGestures)
		{
			if (areClose(spinePoint, handPoint, detectRange))
			{
				// Lock-on
				if (! lockingOn_magnify)
				{
					lockingOn_magnify = TRUE;
					lockonStartTime = getTimeIn100NSIntervals();
					if (showOverlays)
					{
						clearOverlay();
						drawLockOn(xRes/2, yRes/2);
						drawText ((xRes/3), (yRes/10), L"Locking on to Magnification Mode", 56);
					}
				}
				else // We're locking on already
				{
					curTime = getTimeIn100NSIntervals();
					if ((curTime - lockonStartTime) > lockonTime)
					{
						state->set(MAGNIFYLEFT);
						startTime = getTimeIn100NSIntervals();
						if (showOverlays)
						{
							clearOverlay();
							drawRectangle ((xRes/2) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 1);
							drawText ((xRes/3), (yRes/10), L"Magnification Gesture Mode", 56);
						}
					}
				}
				return;
			}
			else if (areClose(centerPoint, handPoint, detectRange))
			{
				// Don't do anything during the dead time
				if (! lockingOn_move)
				{
					lockingOn_move = TRUE;
					lockonStartTime = getTimeIn100NSIntervals();
					if (showOverlays)
					{
						clearOverlay();
						if (hand == RIGHT)
						{
							drawLockOn(xRes*3/4, yRes/2);
						}
						else
						{
							drawLockOn(xRes/4, yRes/2);
						}
						drawText ((xRes/3), (yRes/10), L"Locking on to Movement Mode", 56);
					}
				}
				else
				{
					curTime = getTimeIn100NSIntervals();
					if ((curTime - lockonStartTime) > lockonTime)
					{
						state->set(MOVECENTER);
						startTime = getTimeIn100NSIntervals();
						if (showOverlays)
						{
							clearOverlay();

							int ulx;
							int ulx_small;
							int ulx_ss;
							int uly = (yRes/2) - (boxLarge/2);
							int uly_small = (yRes/2) - (boxSmall/2);
							int uly_ss = (yRes/2) - (boxSuperSmall/2);
							if (hand == RIGHT)
							{
								ulx = (xRes*3/4) - (boxLarge/2);
								ulx_small = (xRes*3/4) - (boxSmall/2);
								ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
							}
							else
							{
								ulx = (xRes/4) - (boxLarge/2);
								ulx_small = (xRes/4) - (boxSmall/2);
								ulx_ss = (xRes/4) - (boxSuperSmall/2);
							}
							drawTrapezoid(ulx, uly, Q_TOP, 0);
							drawTrapezoid(ulx, uly, Q_BOTTOM, 0);
							// drawTrapezoid(ulx, uly, Q_RIGHT, 0);
							// drawTrapezoid(ulx, uly, Q_LEFT, 0);
							drawRectangle(ulx, uly, boxLarge, boxLarge, 1);
							drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 1);
							drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 1);
							drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
						}
					}
				}
				return;
			}
			// Nothing hit, so we're not locking on
			lockingOn_move = FALSE;
			lockingOn_magnify = FALSE;
			if (showOverlays)
			{
				clearOverlay();
				if (hand == RIGHT)
				{
					drawRectangle ((xRes*3/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
				}
				else
				{
					drawRectangle ((xRes/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
				}
				drawRectangle ((xRes/2) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
			}
		}
		else 		// Only movement gestures
		{
			if (areClose(centerPoint, handPoint, detectRange))
			{
				if (! lockingOn_move)
				{
					lockingOn_move = TRUE;
					lockonStartTime = getTimeIn100NSIntervals();
					if (showOverlays)
					{
						clearOverlay();
						if (hand == RIGHT)
						{
							drawLockOn(xRes*3/4, yRes/2);
						}
						else
						{
							drawLockOn(xRes/4, yRes/2);
						}
						drawText ((xRes/3), (yRes/10), L"Locking on to Movement Mode", 56);
					}
				}
				else
				{ // We're locking on
					curTime = getTimeIn100NSIntervals();
					if ((curTime - lockonStartTime) > lockonTime)
					{
						state->set(MOVECENTER);
						startTime = getTimeIn100NSIntervals();
						if (showOverlays)
						{
							clearOverlay();

							int ulx;
							int ulx_small;
							int ulx_ss;
							int uly = (yRes/2) - (boxLarge/2);
							int uly_small = (yRes/2) - (boxSmall/2);
							int uly_ss = (yRes/2) - (boxSuperSmall/2);
							if (hand == RIGHT)
							{
								ulx = (xRes*3/4) - (boxLarge/2);
								ulx_small = (xRes*3/4) - (boxSmall/2);
								ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
							}
							else
							{
								ulx = (xRes/4) - (boxLarge/2);
								ulx_small = (xRes/4) - (boxSmall/2);
								ulx_ss = (xRes/4) - (boxSuperSmall/2);
							}
							drawTrapezoid(ulx, uly, Q_TOP, 0);
							drawTrapezoid(ulx, uly, Q_BOTTOM, 0);
							// drawTrapezoid(ulx, uly, Q_RIGHT, 0);
							// drawTrapezoid(ulx, uly, Q_LEFT, 0);
							drawRectangle(ulx, uly, boxLarge, boxLarge, 1);
							drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 1);
							drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 1);
							drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
						}
					}
				}
				return;
			}
			// We're not close to anything, so turn off the lockon
			lockingOn_move = FALSE;
			if (showOverlays)
			{
				clearOverlay();
				if (hand == RIGHT)
				{
					drawRectangle ((xRes*3/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
				}
				else
				{
					drawRectangle ((xRes/4) - (boxLarge/2), (yRes/2) - (boxLarge/2), boxLarge, boxLarge, 0);
				}
			}
		}
		break;
	// case BODYCENTER:
	// 	spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
	// 	centerPoint = spinePoint;
	// 	if (hand == RIGHT)
	// 	{
	// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	// 		centerPoint.x += centerRightOver;
	// 	}
	// 	else
	// 	{
	// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	// 		centerPoint.x -= centerLeftOver;
	// 	}
	// 	if (areClose(centerPoint, handPoint, detectRange))
	// 	{
	// 		if (hand == RIGHT)
	// 		{
	// 			// Center
	// 			drawRectangle ((xRes*3/4) - (boxSmall/2), (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 1);
	// 			// Vert
	// 			drawRectangle ((xRes*3/4) - (boxSmall/2), (yRes/2) - (boxSmall/2) - overlayCircleRadius, boxSmall, boxSmall, 0);
	// 			drawRectangle ((xRes*3/4) - (boxSmall/2), (yRes/2) - (boxSmall/2) + overlayCircleRadius, boxSmall, boxSmall, 0);
	// 			// Horiz
	// 			drawRectangle ((xRes*3/4) - (boxSmall/2) - overlayCircleRadius, (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 			drawRectangle ((xRes*3/4) - (boxSmall/2) + overlayCircleRadius, (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 		}
	// 		else
	// 		{
	// 			// Center
	// 			drawRectangle ((xRes/4) - (boxSmall/2), (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 1);
	// 			// Vert
	// 			drawRectangle ((xRes/4) - (boxSmall/2), (yRes/2) - (boxSmall/2) - overlayCircleRadius, boxSmall, boxSmall, 0);
	// 			drawRectangle ((xRes/4) - (boxSmall/2), (yRes/2) - (boxSmall/2) + overlayCircleRadius, boxSmall, boxSmall, 0);
	// 			// Horiz
	// 			drawRectangle ((xRes/4) - (boxSmall/2) - overlayCircleRadius, (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 			drawRectangle ((xRes/4) - (boxSmall/2) + overlayCircleRadius, (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 		}
	// 		state->set(MOVECENTER);
	// 		startTime = getTimeIn100NSIntervals();
	// 		return;
	// 	}
	// 	// Otherwise, keep looking (until the timeout)
	// 	break;
	case MOVECENTER:
	case MOVEUP:
	case MOVEDOWN:
	case MOVERIGHT:
	case MOVELEFT:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerRightOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerLeftOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
		}
		// Specific direction
		curQuadrant = findQuadrant(centerPoint, handPoint);

		// Place the direction arrows
		if (curQuadrant == Q_TOP)
		{
			if (showOverlays)
			{
				int ulx;
				int ulx_small;
				int ulx_ss;
				int uly = (yRes/2) - (boxLarge/2);
				int uly_small = (yRes/2) - (boxSmall/2);
				int uly_ss = (yRes/2) - (boxSuperSmall/2);
				if (hand == RIGHT)
				{
					ulx = (xRes*3/4) - (boxLarge/2);
					ulx_small = (xRes*3/4) - (boxSmall/2);
					ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
				}
				else
				{
					ulx = (xRes/4) - (boxLarge/2);
					ulx_small = (xRes/4) - (boxSmall/2);
					ulx_ss = (xRes/4) - (boxSuperSmall/2);
				}
				// Overwrite old center stuff
				drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 2);
				drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 2);

				drawRectangle(ulx, uly, boxLarge, boxLarge, 0);
				drawTrapezoid(ulx, uly, Q_BOTTOM, 0);
				/*drawTrapezoid(ulx, uly, Q_RIGHT, 0);
				drawTrapezoid(ulx, uly, Q_LEFT, 0);*/
				drawTrapezoid(ulx, uly, Q_TOP, 1);				
				drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
			}
			state->set(MOVEUP);
			if (MOVEMENT_STYLE == Velocity_Style)
			{
				if (displacement_y < 0)
				{
					moveAmount_y += 500*displacement_y;
				}
			}
			else if (MOVEMENT_STYLE == Constant_Style)
			{
				moveAmount_x = 0;
				moveAmount_y = -constantMovement;
			}
			startTime = getTimeIn100NSIntervals();
			return;
		}
		else if (curQuadrant == Q_BOTTOM)
		{
			if (showOverlays)
			{
				int ulx;
				int ulx_small;
				int ulx_ss;
				int uly = (yRes/2) - (boxLarge/2);
				int uly_small = (yRes/2) - (boxSmall/2);
				int uly_ss = (yRes/2) - (boxSuperSmall/2);
				if (hand == RIGHT)
				{
					ulx = (xRes*3/4) - (boxLarge/2);
					ulx_small = (xRes*3/4) - (boxSmall/2);
					ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
				}
				else
				{
					ulx = (xRes/4) - (boxLarge/2);
					ulx_small = (xRes/4) - (boxSmall/2);
					ulx_ss = (xRes/4) - (boxSuperSmall/2);
				}
				// Overwrite old center stuff
				drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 2);
				drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 2);

				drawRectangle(ulx, uly, boxLarge, boxLarge, 0);
				drawTrapezoid(ulx, uly, Q_TOP, 0);
				/*drawTrapezoid(ulx, uly, Q_RIGHT, 0);
				drawTrapezoid(ulx, uly, Q_LEFT, 0);*/
				drawTrapezoid(ulx, uly, Q_BOTTOM, 1);				
				drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
			}
			state->set(MOVEDOWN);
			if (MOVEMENT_STYLE == Velocity_Style)
			{
				if (displacement_y > 0)
				{
					moveAmount_y += 500*displacement_y;
				}
			}
			else if (MOVEMENT_STYLE == Constant_Style)
			{
				moveAmount_x = 0;
				moveAmount_y = constantMovement;
			}
			startTime = getTimeIn100NSIntervals();
			return;
		}
		else if (curQuadrant == Q_RIGHT)
		{
			if (showOverlays)
			{
				int ulx;
				int ulx_small;
				int ulx_ss;
				int uly = (yRes/2) - (boxLarge/2);
				int uly_small = (yRes/2) - (boxSmall/2);
				int uly_ss = (yRes/2) - (boxSuperSmall/2);
				if (hand == RIGHT)
				{
					ulx = (xRes*3/4) - (boxLarge/2);
					ulx_small = (xRes*3/4) - (boxSmall/2);
					ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
				}
				else
				{
					ulx = (xRes/4) - (boxLarge/2);
					ulx_small = (xRes/4) - (boxSmall/2);
					ulx_ss = (xRes/4) - (boxSuperSmall/2);
				}
				// Overwrite old center stuff
				drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 2);
				drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 2);
				
				drawRectangle(ulx, uly, boxLarge, boxLarge, 0);
				/*drawTrapezoid(ulx, uly, Q_TOP, 0);
				drawTrapezoid(ulx, uly, Q_BOTTOM, 0);*/
				drawTrapezoid(ulx, uly, Q_LEFT, 0);
				drawTrapezoid(ulx, uly, Q_RIGHT, 1);				
				drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
			}
			state->set(MOVERIGHT);
			if (MOVEMENT_STYLE == Velocity_Style)
			{
				if (displacement_x > 0)
				{
					moveAmount_x += 500*displacement_x;
				}
			}
			else if (MOVEMENT_STYLE == Constant_Style)
			{
				moveAmount_y = 0;
				moveAmount_x = constantMovement;
			}
			startTime = getTimeIn100NSIntervals();
			return;
		}
		else if (curQuadrant == Q_LEFT)
		{
			if (showOverlays)
			{
				int ulx;
				int ulx_small;
				int ulx_ss;
				int uly = (yRes/2) - (boxLarge/2);
				int uly_small = (yRes/2) - (boxSmall/2);
				int uly_ss = (yRes/2) - (boxSuperSmall/2);
				if (hand == RIGHT)
				{
					ulx = (xRes*3/4) - (boxLarge/2);
					ulx_small = (xRes*3/4) - (boxSmall/2);
					ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
				}
				else
				{
					ulx = (xRes/4) - (boxLarge/2);
					ulx_small = (xRes/4) - (boxSmall/2);
					ulx_ss = (xRes/4) - (boxSuperSmall/2);
				}
				// Overwrite old center stuff
				drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 2);
				drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 2);
				
				drawRectangle(ulx, uly, boxLarge, boxLarge, 0);
				/*drawTrapezoid(ulx, uly, Q_TOP, 0);
				drawTrapezoid(ulx, uly, Q_BOTTOM, 0);*/
				drawTrapezoid(ulx, uly, Q_RIGHT, 0);
				drawTrapezoid(ulx, uly, Q_LEFT, 1);
				drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
			}
			state->set(MOVELEFT);
			if (MOVEMENT_STYLE == Velocity_Style)
			{
				if (displacement_x < 0)
				{
					moveAmount_x += 500*displacement_x;
				}
			}
			else if (MOVEMENT_STYLE == Constant_Style)
			{
				moveAmount_y = 0;
				moveAmount_x = -constantMovement;
			}
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Back to MOVECENTER
		else if (curQuadrant == Q_CENTER)
		{
			if (MOVEMENT_STYLE == Constant_Style)
			{
				moveAmount_y = 0;
				moveAmount_x = 0;
			}
			if (showOverlays)
			{
				int ulx;
				int ulx_small;
				int ulx_ss;
				int uly = (yRes/2) - (boxLarge/2);
				int uly_small = (yRes/2) - (boxSmall/2);
				int uly_ss = (yRes/2) - (boxSuperSmall/2);
				if (hand == RIGHT)
				{
					ulx = (xRes*3/4) - (boxLarge/2);
					ulx_small = (xRes*3/4) - (boxSmall/2);
					ulx_ss = (xRes*3/4) - (boxSuperSmall/2);
				}
				else
				{
					ulx = (xRes/4) - (boxLarge/2);
					ulx_small = (xRes/4) - (boxSmall/2);
					ulx_ss = (xRes/4) - (boxSuperSmall/2);
				}
				if (! amClicking)
				{
					drawRectangle(ulx_small, uly_small, boxSmall, boxSmall, 1);
					drawRectangle(ulx_ss, uly_ss, boxSuperSmall, boxSuperSmall, 1);
				}
				
				drawTrapezoid(ulx, uly, Q_TOP, 0);
				drawTrapezoid(ulx, uly, Q_BOTTOM, 0);
				/*drawTrapezoid(ulx, uly, Q_RIGHT, 0);
				drawTrapezoid(ulx, uly, Q_LEFT, 0);*/
				drawRectangle(ulx, uly, boxLarge, boxLarge, 1);
				drawText ((xRes/3), (yRes/10), L"Movement Gesture Mode", 56);
			}
			state->set(MOVECENTER);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// Otherwise, keep looking (until the timeout)
		break;
		// case MOVE:
		// 	spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		// 	centerPoint = spinePoint;
		// 	if (hand == RIGHT)
		// 	{
		// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
		// 		centerPoint.x += centerRightOver;
		// 	}
		// 	else
		// 	{
		// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
		// 		centerPoint.x -= centerLeftOver;
		// 	}
		// 	// Back to MOVECENTER
		// 	if (areClose(centerPoint, handPoint, detectRange))
		// 	{
		// 		state->set(MOVECENTER);
		// 		startTime = getTimeIn100NSIntervals();
		// 		return;
		// 	}
		// 	// Otherwise, keep looking (until the timeout)
		// 	break;
	// case MAGNIFYCENTER:
	// 	spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
	// 	centerPoint = spinePoint;
	// 	if (hand == RIGHT)
	// 	{
	// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	// 		centerPoint.x += centerRightOver;
	// 	}
	// 	else
	// 	{
	// 		handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	// 		centerPoint.x -= centerLeftOver;
	// 	}
	// 	// Place the direction arrows
	// 	upPoint = centerPoint;
	// 	upPoint.y += directionRadius;
	// 	if (areClose(upPoint, handPoint, detectRange))
	// 	{
	// 		state->set(MAGNIFYUP);
	// 		return;
	// 	}
	// 	downPoint = centerPoint;
	// 	downPoint.y -= directionRadius;
	// 	if (areClose(downPoint, handPoint, detectRange))
	// 	{
	// 		state->set(MAGNIFYDOWN);
	// 		startTime = getTimeIn100NSIntervals();
	// 		return;
	// 	}
	// 	rightPoint = centerPoint;
	// 	rightPoint.x += directionRadius;
	// 	if (areClose(rightPoint, handPoint, detectRange))
	// 	{
	// 		state->set(MAGNIFYRIGHT);
	// 		startTime = getTimeIn100NSIntervals();
	// 		return;
	// 	}
	// 	leftPoint = centerPoint;
	// 	leftPoint.x -= directionRadius;
	// 	if (areClose(leftPoint, handPoint, detectRange))
	// 	{
	// 		state->set(MAGNIFYLEFT);
	// 		startTime = getTimeIn100NSIntervals();
	// 		return;
	// 	}
	// 	if (showOverlays)
	// 	{
	// 		clearOverlay();
	// 		drawText ((xRes/3), (yRes/10), L"Rotate clockwise to decrease magnification and vice-versa to increase", 56);
	// 		// Center
	// 		drawRectangle ((xRes*3/4) - (boxSmall/2), (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 		// Vert
	// 		drawRectangle ((xRes*3/4) - (boxSmall/2), (yRes/2) - (boxSmall/2) - overlayCircleRadius, boxSmall, boxSmall, 0);
	// 		drawRectangle ((xRes*3/4) - (boxSmall/2), (yRes/2) - (boxSmall/2) + overlayCircleRadius, boxSmall, boxSmall, 0);
	// 		// Horiz
	// 		drawRectangle ((xRes*3/4) - (boxSmall/2) - overlayCircleRadius, (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 		drawRectangle ((xRes*3/4) - (boxSmall/2) + overlayCircleRadius, (yRes/2) - (boxSmall/2), boxSmall, boxSmall, 0);
	// 	}
	// 	// Otherwise, keep looking (until the timeout)
	// 	break;
	case MAGNIFYUP:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerRightOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerLeftOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
		}
		// Place the direction arrows
		// upPoint = centerPoint;
		// upPoint.y += directionRadius;
		// if (areClose(upPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYUP);
		// 	return;
		// }
		// downPoint = centerPoint;
		// downPoint.y -= directionRadius;
		// if (areClose(downPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYDOWN);
		// 	startTime = getTimeIn100NSIntervals();
		// 	return;
		// }
		rightPoint = centerPoint;
		rightPoint.x += directionRadius;
		if (areClose(rightPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYRIGHT);
			// Clockwise is increase magnification
			magnifyAmount += abs(displacement_x + displacement_y);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		leftPoint = centerPoint;
		leftPoint.x -= directionRadius;
		if (areClose(leftPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYLEFT);
			// Counterclockwise is decrease magnification
			magnifyAmount -= abs(displacement_x + displacement_y);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		if (showOverlays)
		{
			clearOverlay();
			drawText ((xRes/3), (yRes/10), L"Clockwise = zoom in", 56);
			drawText ((xRes/3), (yRes*9/10), L"Counter-Clockwise = zoom out", 56);
			int ulx;
			int uly = (yRes/2) - (boxSmall/2);
			if (hand == RIGHT)
			{
				ulx = (xRes*3/4) - (boxSmall/2);
			}
			else
			{
				ulx = (xRes/4) - (boxSmall/2);
			}
			// Vert
			drawRectangle (ulx, uly - overlayCircleRadius, boxSmall, boxSmall, 1);
			drawRectangle (ulx, uly + overlayCircleRadius, boxSmall, boxSmall, 0);
			// Horiz
			drawRectangle (ulx - overlayCircleRadius, uly, boxSmall, boxSmall, 0);
			drawRectangle (ulx + overlayCircleRadius, uly, boxSmall, boxSmall, 0);
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case MAGNIFYDOWN:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerRightOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerLeftOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
		}
		// Place the direction arrows
		// upPoint = centerPoint;
		// upPoint.y += directionRadius;
		// if (areClose(upPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYUP);
		// 	return;
		// }
		// downPoint = centerPoint;
		// downPoint.y -= directionRadius;
		// if (areClose(downPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYDOWN);
		// 	startTime = getTimeIn100NSIntervals();
		// 	return;
		// }
		rightPoint = centerPoint;
		rightPoint.x += directionRadius;
		if (areClose(rightPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYRIGHT);
			// Counterclockwise is increase magnification
			magnifyAmount -= abs(displacement_x + displacement_y);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		leftPoint = centerPoint;
		leftPoint.x -= directionRadius;
		if (areClose(leftPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYLEFT);
			// Clockwise is decrease magnification
			magnifyAmount += abs(displacement_x + displacement_y);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		if (showOverlays)
		{
			clearOverlay();
			drawText ((xRes/3), (yRes/10), L"Clockwise = zoom in", 56);
			drawText ((xRes/3), (yRes*9/10), L"Counter-Clockwise = zoom out", 56);
			int ulx;
			int uly = (yRes/2) - (boxSmall/2);
			if (hand == RIGHT)
			{
				ulx = (xRes*3/4) - (boxSmall/2);
			}
			else
			{
				ulx = (xRes/4) - (boxSmall/2);
			}
			// Vert
			drawRectangle (ulx, uly - overlayCircleRadius, boxSmall, boxSmall, 0);
			drawRectangle (ulx, uly + overlayCircleRadius, boxSmall, boxSmall, 1);
			// Horiz
			drawRectangle (ulx - overlayCircleRadius, uly, boxSmall, boxSmall, 0);
			drawRectangle (ulx + overlayCircleRadius, uly, boxSmall, boxSmall, 0);
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case MAGNIFYLEFT:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerRightOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerLeftOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
		}
		// Place the direction arrows
		upPoint = centerPoint;
		upPoint.y += directionRadius;
		if (areClose(upPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYUP);
			// Clockwise is increase magnification
			magnifyAmount += abs(displacement_x + displacement_y);
			return;
		}
		downPoint = centerPoint;
		downPoint.y -= directionRadius;
		if (areClose(downPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYDOWN);
			// Counterclockwise is decrease magnification
			magnifyAmount -= abs(displacement_x + displacement_y);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// rightPoint = centerPoint;
		// rightPoint.x += directionRadius;
		// if (areClose(rightPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYRIGHT);
		// 	// Clockwise is increase magnification
		// 	magnifyAmount += abs(displacement_x + displacement_y);
		// 	startTime = getTimeIn100NSIntervals();
		// 	return;
		// }
		// leftPoint = centerPoint;
		// leftPoint.x -= directionRadius;
		// if (areClose(leftPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYLEFT);
		// 	// Counterclockwise is decrease magnification
		// 	magnifyAmount -= abs(displacement_x + displacement_y);
		// 	startTime = getTimeIn100NSIntervals();
		// 	return;
		// }
		if (showOverlays)
		{
			clearOverlay();
			drawText ((xRes/3), (yRes/10), L"Clockwise = zoom in", 56);
			drawText ((xRes/3), (yRes*9/10), L"Counter-Clockwise = zoom out", 56);
			int ulx;
			int uly = (yRes/2) - (boxSmall/2);
			if (hand == RIGHT)
			{
				ulx = (xRes*3/4) - (boxSmall/2);
			}
			else
			{
				ulx = (xRes/4) - (boxSmall/2);
			}
			// Vert
			drawRectangle (ulx, uly - overlayCircleRadius, boxSmall, boxSmall, 0);
			drawRectangle (ulx, uly + overlayCircleRadius, boxSmall, boxSmall, 0);
			// Horiz
			drawRectangle (ulx - overlayCircleRadius, uly, boxSmall, boxSmall, 1);
			drawRectangle (ulx + overlayCircleRadius, uly, boxSmall, boxSmall, 0);
		}
		// Otherwise, keep looking (until the timeout)
		break;
	case MAGNIFYRIGHT:
		spinePoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_SPINE];
		centerPoint = spinePoint;
		if (hand == RIGHT)
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			centerPoint.x += centerRightOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], displacement_x, displacement_y);
		}
		else
		{
			handPoint = SkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			centerPoint.x -= centerLeftOver;
			// Add velocity
			getDifference(handPoint, prevSkeletonData.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], displacement_x, displacement_y);
		}
		// Place the direction arrows
		upPoint = centerPoint;
		upPoint.y += directionRadius;
		if (areClose(upPoint, handPoint, detectRange))
		{
			// Counterclockwise is decrease magnification
			magnifyAmount -= abs(displacement_x + displacement_y);
			state->set(MAGNIFYUP);
			return;
		}
		downPoint = centerPoint;
		downPoint.y -= directionRadius;
		if (areClose(downPoint, handPoint, detectRange))
		{
			state->set(MAGNIFYDOWN);
			// Clockwise is increase magnification
			magnifyAmount += abs(displacement_x + displacement_y);
			startTime = getTimeIn100NSIntervals();
			return;
		}
		// rightPoint = centerPoint;
		// rightPoint.x += directionRadius;
		// if (areClose(rightPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYRIGHT);
		// 	// Clockwise is increase magnification
		// 	magnifyAmount += abs(displacement_x + displacement_y);
		// 	startTime = getTimeIn100NSIntervals();
		// 	return;
		// }
		// leftPoint = centerPoint;
		// leftPoint.x -= directionRadius;
		// if (areClose(leftPoint, handPoint, detectRange))
		// {
		// 	state->set(MAGNIFYLEFT);
		// 	// Counterclockwise is decrease magnification
		// 	magnifyAmount -= abs(displacement_x + displacement_y);
		// 	startTime = getTimeIn100NSIntervals();
		// 	return;
		// }
		if (showOverlays)
		{
			clearOverlay();
			drawText ((xRes/3), (yRes/10), L"Clockwise = zoom in", 56);
			drawText ((xRes/3), (yRes*9/10), L"Counter-Clockwise = zoom out", 56);
			int ulx;
			int uly = (yRes/2) - (boxSmall/2);
			if (hand == RIGHT)
			{
				ulx = (xRes*3/4) - (boxSmall/2);
			}
			else
			{
				ulx = (xRes/4) - (boxSmall/2);
			}
			// Vert
			drawRectangle (ulx, uly - overlayCircleRadius, boxSmall, boxSmall, 0);
			drawRectangle (ulx, uly + overlayCircleRadius, boxSmall, boxSmall, 0);
			// Horiz
			drawRectangle (ulx - overlayCircleRadius, uly, boxSmall, boxSmall, 0);
			drawRectangle (ulx + overlayCircleRadius, uly, boxSmall, boxSmall, 1);
		}
		// Otherwise, keep looking (until the timeout)
		break;
	}
}

// Check if two Vector4 objects are within a certain 2-D rectangular range of each other
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

// Check if two Vector4 objects are within a certain 3-D rectangular range of each other
// This is less intuitive, so default to 2D, but sometimes we do want things to be 3D
bool GestureDetector::areClose3D(Vector4 &obj1, Vector4 &obj2, double range)
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

// Figure out if a point is in the top, bottom, left, right, or center
// areas of the screen.  The given argument is the center of the
// "center" box.
Quadrant GestureDetector::findQuadrant(Vector4 center, Vector4 point)
{
	// If it's in the center box, handle that right away using are already-existing areClose function
	if (areClose(center, point, centerBoxSize))
	{
		return Q_CENTER;
	}

	// Make the point centered from the center, rather than whatever (0,0) is.
	// Verified that point - center is right (as opposed to center - point)
	point.x = point.x - center.x;
	point.y = point.y - center.y;

	// Cut the screen into two diagonal halves
	if (point.y > point.x) 	// Top left
	{
		if (point.y > -point.x)
		{
			return Q_TOP;
		}
		else
		{
			return Q_LEFT;
		}
	}
	else 			// Bottom right
	{
		if (point.y > -point.x)
		{
			return Q_RIGHT;
		}
		else
		{
			return Q_BOTTOM;
		}
	}
}
