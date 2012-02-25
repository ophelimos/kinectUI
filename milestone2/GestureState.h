#pragma once
#include <Windows.h>

// Size of the name string
const int NAMESIZE = 16;

enum GestureStateEnum {
	OFF,
	SALUTE1,
	SALUTE2,
	BODYCENTER,
	/* MOVEUP, */
	/* MOVEDOWN, */
	/* MOVELEFT, */
	/* MOVERIGHT, */
	MOVE,
	MOVECENTER,
	/* add more as necessary */
};

class GestureState
{
public:
	GestureState(HWND assocHwnd, int userId);
	~GestureState(void);
	
	GestureStateEnum state;
	HWND hwnd;
	// This needs to be passed to the SkeletalViewer window, so it needs to be persistent
	char* name;
	GestureState& operator=(GestureState& other);
	void set(GestureStateEnum newState);
	void updateDebug();

	// Since we have multiple gesture detectors, we need to know who we are.
	int id;
};
