#include "CapturedTestMSRTT_MultisampledRenderToTextureAttachment_ES3_Vulkan_EnableMultisampledRenderToTexture.h"
#include "angle_trace_gl.h"

// Private Functions

void SetupReplayContext18(void)
{
    eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContextMap2[18]);
    UpdateCurrentContext(18);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenFramebuffers(1, (GLuint *)gReadBuffer);
    UpdateFramebufferID2(18, 3, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][3]);
    glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gTextureMap[1], 0, 4);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gRenderbufferMap[1]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gRenderbufferMap[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
}

void ReplayFrame1(void)
{
    eglGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][3]);
    glClearColor(0.000000000000000, 1, 0.000000000000000, 1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
}

void ReplayFrame2(void)
{
    eglGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][3]);
    glClearColor(0.000000000000000, 1, 0.000000000000000, 1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
}

void ReplayFrame3(void)
{
    eglGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][3]);
    glClearColor(0.000000000000000, 1, 0.000000000000000, 1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
}

void ReplayFrame4(void)
{
    eglGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][3]);
    glClearColor(0.000000000000000, 1, 0.000000000000000, 1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
}

void ResetReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, gTextureMap[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, (const GLubyte *)GetBinaryData(0));
}

void ResetReplayContext18(void)
{
}

void ReplayFrame5(void)
{
    eglGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][3]);
    glClearColor(0.000000000000000, 1, 0.000000000000000, 1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
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
    SetCurrentContextID(18);
    SetupReplayContext18();

}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext18();

    // Reset main context state
    glBindFramebuffer(GL_FRAMEBUFFER, gFramebufferMapPerContext[18][0]);
    glBindTexture(GL_TEXTURE_2D, gTextureMap[1]);
}

