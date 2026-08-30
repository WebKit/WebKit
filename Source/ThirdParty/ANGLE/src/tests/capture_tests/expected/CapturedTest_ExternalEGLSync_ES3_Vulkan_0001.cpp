#include "CapturedTest_ExternalEGLSync_ES3_Vulkan.h"
#include "angle_trace_gl.h"

// Private Functions

void SetupReplayContext5(void)
{
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[0], gSurfaceMap2[0], gContextMap2[5]);
    UpdateCurrentContext(5);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glUseProgram(gShaderProgramMap[1]);
    UpdateCurrentProgramPerContext(1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void ReplayFrame1(void)
{
    eglGetError();
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[1], gSurfaceMap2[1], gContextMap2[5]);
    // Dropping eglWaitSyncKHR on possibly external EGL sync ID 1;
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)gReadBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)gReadBuffer);
    glGetAttribLocation(gShaderProgramMap[1], "a_position");
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gClientArrays[0]);
    glEnableVertexAttribArray(0);
    UpdateClientArrayPointer(0, (const GLubyte *)GetBinaryData(0), 72);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
}

void ReplayFrame2(void)
{
    eglGetError();
    // Dropping eglDestroySyncKHR on possibly external EGL sync ID 1;
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)gReadBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)gReadBuffer);
    glGetAttribLocation(gShaderProgramMap[1], "a_position");
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gClientArrays[0]);
    glEnableVertexAttribArray(0);
    UpdateClientArrayPointer(0, (const GLubyte *)GetBinaryData(80), 72);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
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

void ResetReplayContext5(void)
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
    SetCurrentContextID(5);
    SetupReplayContext5();

}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext5();

    // Reset main context state
}

