#include "CapturedTest_NonFrameBoundarySwaps_ES3_Vulkan.h"
#include "angle_trace_gl.h"

// Private Functions

void SetupReplayContext12(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[12]);
    UpdateCurrentContext(12);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void ReplayFrame1(void)
{
    eglGetError();
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[2], gSurfaceMap2[2], gContextMap2[12]);
    glClear(GL_COLOR_BUFFER_BIT);
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[12]);
    glClearColor(0.000000000000000, 0.000000000000000, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ReplayFrame2(void)
{
    eglGetError();
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[2], gSurfaceMap2[2], gContextMap2[12]);
    glClear(GL_COLOR_BUFFER_BIT);
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[12]);
    glClearColor(0.000000000000000, 0.000000000000000, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ReplayFrame3(void)
{
    eglGetError();
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[2], gSurfaceMap2[2], gContextMap2[12]);
    glClear(GL_COLOR_BUFFER_BIT);
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[12]);
    glClearColor(0.000000000000000, 0.000000000000000, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ReplayFrame4(void)
{
    eglGetError();
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[2], gSurfaceMap2[2], gContextMap2[12]);
    glClear(GL_COLOR_BUFFER_BIT);
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[12]);
    glClearColor(0.000000000000000, 0.000000000000000, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ResetReplayContextShared(void)
{
}

void ResetReplayContext12(void)
{
}

void ReplayFrame5(void)
{
    eglGetError();
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[2], gSurfaceMap2[2], gContextMap2[12]);
    glClear(GL_COLOR_BUFFER_BIT);
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[12]);
    glClearColor(0.000000000000000, 0.000000000000000, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
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
    SetCurrentContextID(12);
    SetupReplayContext12();

}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext12();

    // Reset main context state
}

