#pragma once
#include <Windows.h>

// Size of the name string
const int NAMESIZE = 16;

enum GestureStateEnum {
	OFF,
	SALUTE1,
	SALUTE2,
	MOVELENS,
	MAGNIFY
	/* add more as necessary */
};

class GestureState
{
public:
	GestureState(HWND assocHwnd);
	~GestureState(void);
	
	GestureStateEnum state;
	HWND hwnd;
	// This needs to be passed to the SkeletalViewer window, so it needs to be persistent
	char* name;
	GestureState& operator=(GestureState& other);
	void set(GestureStateEnum newState);
	void updateDebug();
};
