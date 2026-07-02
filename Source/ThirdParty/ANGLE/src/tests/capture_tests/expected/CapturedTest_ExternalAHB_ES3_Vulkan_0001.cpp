#include "CapturedTest_ExternalAHB_ES3_Vulkan.h"
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
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)gReadBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)gReadBuffer);
    glGetAttribLocation(gShaderProgramMap[1], "a_position");
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gClientArrays[0]);
    glEnableVertexAttribArray(0);
    UpdateClientArrayPointer(0, (const GLubyte *)GetBinaryData(0), 72);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
}

void ReplayFrame2(void)
{
    eglGetError();
    CreateEGLImageKHR(gEGLDisplay, gContextMap2[0], 12608, 0, 0, 2, 2, 2);
    eglGetError();
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, gTextureMap[1]);
    UpdateEGLImageData(2, 2, 2, (const GLubyte *)GetBinaryData(80));
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, gEGLImageMap2[2]);
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)gReadBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)gReadBuffer);
    glGetAttribLocation(gShaderProgramMap[1], "a_position");
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gClientArrays[0]);
    glEnableVertexAttribArray(0);
    UpdateClientArrayPointer(0, (const GLubyte *)GetBinaryData(96), 72);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
}

void ReplayFrame3(void)
{
    eglGetError();
    CreateEGLImageKHR(gEGLDisplay, gContextMap2[0], 12608, 0, 0, 2, 2, 3);
    eglGetError();
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, gTextureMap[1]);
    UpdateEGLImageData(3, 2, 2, (const GLubyte *)GetBinaryData(176));
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, gEGLImageMap2[3]);
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)gReadBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)gReadBuffer);
    glGetAttribLocation(gShaderProgramMap[1], "a_position");
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gClientArrays[0]);
    glEnableVertexAttribArray(0);
    UpdateClientArrayPointer(0, (const GLubyte *)GetBinaryData(192), 72);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
}

void ReplayFrame4(void)
{
    eglGetError();
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)gReadBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)gReadBuffer);
    glGetAttribLocation(gShaderProgramMap[1], "a_position");
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gClientArrays[0]);
    glEnableVertexAttribArray(0);
    UpdateClientArrayPointer(0, (const GLubyte *)GetBinaryData(272), 72);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
}

void ResetReplayContextShared(void)
{
    DestroyEGLImageKHR(gEGLDisplay, gEGLImageMap2[3], 3);
}

void ResetReplayContext5(void)
{
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, gTextureMap[1]);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, gEGLImageMap2[1]);
}

void ReplayFrame5(void)
{
    eglGetError();
    DestroyEGLImageKHR(gEGLDisplay, gEGLImageMap2[2], 2);
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
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, gTextureMap[1]);
}

