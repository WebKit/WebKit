#include "CapturedTest_MECSurfaceRelease_ES3_Vulkan.h"
#include "angle_trace_gl.h"

// Private Functions

void ReplayFrame1(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[14]);
    UpdateCurrentContext(14);
    eglGetError();
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[0]);
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[14]);
}

void ReplayFrame2(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[14]);
    UpdateCurrentContext(14);
    eglGetError();
}

void ReplayFrame3(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[14]);
    UpdateCurrentContext(14);
    eglGetError();
}

void ReplayFrame4(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[14]);
    UpdateCurrentContext(14);
    eglGetError();
}

void ResetReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void ResetReplayContext14(void)
{
}

void ResetReplayContext15(void)
{
}

void ReplayFrame5(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[14]);
    UpdateCurrentContext(14);
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
    SetCurrentContextID(14);
    CreateContext(15);
    SetupReplayContext15();
    SetupReplayContext14();


    eglMakeCurrent(NULL, NULL, NULL, gContextMap2[14]);
    UpdateCurrentContext(14);
}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext14();
    ResetReplayContext15();

    // Reset main context state
}

