#include "CapturedTest_NoBinaryData_ES3_Vulkan.h"
#include "angle_trace_gl.h"

// Private Functions

void SetupReplayContext8(void)
{
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[0], gSurfaceMap2[0], gContextMap2[8]);
    UpdateCurrentContext(8);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void ReplayFrame1(void)
{
    eglGetError();
    glClearColor(0.1000000014901161, 0.2000000029802322, 0.300000011920929, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, 16, 16);
}

void ReplayFrame2(void)
{
    eglGetError();
    glClearColor(0.4000000059604645, 0.5, 0.6000000238418579, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 8, 8);
    glDisable(GL_SCISSOR_TEST);
}

void ReplayFrame3(void)
{
    eglGetError();
}

void ReplayFrame4(void)
{
    eglGetError();
}

void ResetReplayContextShared(void)
{
}

void ResetReplayContext8(void)
{
}

void ReplayFrame5(void)
{
    eglGetError();
}

// Public Functions

void SetupReplay(void)
{
    InitReplay();
    SetupReplayContextShared();
    if (gReplayResourceMode == angle::ReplayResourceMode::All)
    {
        SetupReplayContextSharedInactive();
    }
    SetCurrentContextID(8);
    SetupReplayContext8();

}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext8();

    // Reset main context state
}

