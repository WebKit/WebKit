#include "CapturedTest_MECSurfaceRelease_ES3_Vulkan.h"
#include "angle_trace_gl.h"

// Private Functions

void SetupReplayContext14(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[14]);
    UpdateCurrentContext(14);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glClearColor(0.000000000000000, 0.000000000000000, 1, 1);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
}

void SetupReplayContext15(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[15]);
    UpdateCurrentContext(15);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glViewport(0, 0, 1, 1);
    glScissor(0, 0, 1, 1);
}

// Public Functions

