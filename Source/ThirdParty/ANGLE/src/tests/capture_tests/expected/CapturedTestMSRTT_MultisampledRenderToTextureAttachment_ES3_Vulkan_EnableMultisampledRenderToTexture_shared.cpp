#include "angle_trace_gl.h"
#include "CapturedTestMSRTT_MultisampledRenderToTextureAttachment_ES3_Vulkan_EnableMultisampledRenderToTexture.h"

// Private Functions

void SetupReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, (GLuint *)gReadBuffer);
    UpdateTextureID(1, 0);
    glBindTexture(GL_TEXTURE_2D, gTextureMap[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, (const GLubyte *)GetBinaryData(1024));
    glGenRenderbuffers(1, (GLuint *)gReadBuffer);
    UpdateRenderbufferID(1, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, gRenderbufferMap[1]);
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, 16, 16);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void SetupReplayContextSharedInactive(void)
{
}

// Public Functions

