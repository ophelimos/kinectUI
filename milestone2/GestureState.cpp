#include "GestureState.h"
#include "SkeletalViewer.h"
#include "resource.h"
#include <string.h>

GestureState::GestureState(HWND assocHwnd)
{
	state = OFF;
	hwnd = assocHwnd;
	name = NULL;
	updateDebug();
}

GestureState::~GestureState(void)
{
	if (name != NULL)
	{
		free(name);
	}
}

// Very well might not be used, but avoids confusion with the other
// assignment operator
GestureState& GestureState::operator=(GestureState& other)
{
	state = other.state;
	updateDebug();

	return *this;
}


void GestureState::set(GestureStateEnum newState)
{
	state = newState;
	updateDebug();
}

// Send the state to the output window, which unfortunately is an absolute pain to do.
void GestureState::updateDebug()
{
	// Transform the state name into a string
	char* statename;
	switch(state)
	{
	case OFF:
		statename = _strdup("OFF");
		break;
	case SALUTE1:
		statename = _strdup("SALUTE1");
		break;
	case SALUTE2:
		statename = _strdup("SALUTE2");
		break;
	case MOVELENS:
		statename = _strdup("MOVELENS");
		break;
	case MAGNIFY:
		statename = _strdup("MAGNIFY");
		break;
	default:
		statename = _strdup("UNKNOWN");
	}

	// I want to pass a char*.  The function wants an int*.  Solve the problem by casting.
	// Note, the other uses are casting an int* to an int anyway, so this isn't anything particularly out of the ordinary
	void* tmp = (void*)statename;
	LONG_PTR longptr = (LONG_PTR) tmp;
	// Actually post the message to the output
	::PostMessageW(hwnd, WM_USER_UPDATE_STATE, IDC_STATE, longptr);

	// Delete the old string, keep the new one
	if (name != NULL)
	{
		free(name);
	}
	name = statename;
}