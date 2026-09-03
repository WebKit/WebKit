#include "CapturedTest_ClientWaitSync_ES3_Vulkan.h"
#include "angle_trace_gl.h"

// Private Functions

void SetupReplayContext10(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[10]);
    UpdateCurrentContext(10);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void ReplayFrame1(void)
{
    eglGetError();
    FenceSync2(GL_SYNC_GPU_COMMANDS_COMPLETE, 0, 1);
    glFlush();
    glFinish();
    ClientWaitSync(gSyncMap2[1], 0, 0, GL_ALREADY_SIGNALED);
    glDeleteSync(gSyncMap2[1]);
}

void ReplayFrame2(void)
{
    eglGetError();
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

void ResetReplayContext10(void)
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
    SetCurrentContextID(10);
    SetupReplayContext10();

}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext10();

    // Reset main context state
}

