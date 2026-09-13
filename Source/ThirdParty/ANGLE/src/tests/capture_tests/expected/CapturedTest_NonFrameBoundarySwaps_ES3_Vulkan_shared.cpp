#include "angle_trace_gl.h"
#include "CapturedTest_NonFrameBoundarySwaps_ES3_Vulkan.h"

// Private Functions

void SetupReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void SetupReplayContextSharedInactive(void)
{
}

// Public Functions

