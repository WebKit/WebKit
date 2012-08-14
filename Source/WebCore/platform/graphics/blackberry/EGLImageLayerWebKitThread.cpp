/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "EGLImageLayerWebKitThread.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerCompositingThread.h"
#include "LayerRenderer.h"
#include "SharedGraphicsContext3D.h"
#include <BlackBerryPlatformGLES2ContextState.h>
#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformLog.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

using BlackBerry::Platform::Graphics::GLES2Program;
using BlackBerry::Platform::Graphics::GLES2ContextState;

namespace WebCore {

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 0;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 0;

EGLImageLayerWebKitThread::EGLImageLayerWebKitThread(LayerType type)
    : LayerWebKitThread(type, 0)
    , m_client(EGLImageLayerCompositingThreadClient::create())
    , m_needsDisplay(false)
    , m_frontBufferTexture(0)
    , m_fbo(0)
    , m_shader(0)
    , m_image(0)
{
    layerCompositingThread()->setClient(m_client.get());
    setLayerProgramShader(LayerProgramShaderRGBA);
}

EGLImageLayerWebKitThread::~EGLImageLayerWebKitThread()
{
    // The subclass is responsible for calling deleteFrontBuffer()
    // before we get this far.
    ASSERT(!m_frontBufferTexture);
    ASSERT(!m_fbo);
    ASSERT(!m_shader);
}

void EGLImageLayerWebKitThread::setNeedsDisplay()
{
    m_needsDisplay = true;
    setNeedsCommit();
}

void EGLImageLayerWebKitThread::updateFrontBuffer(const IntSize& size, unsigned backBufferTexture)
{
    if (!m_needsDisplay)
        return;

    if (size.isEmpty()) {
        if (size != m_size) {
            deleteFrontBuffer();
            if (m_image)
                m_garbage.append(m_image);
            m_image = 0;
            m_size = size;
        }

        m_needsDisplay = false;
        return;
    }

    if (!eglCreateImageKHR) {
        eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
        eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    }

    ASSERT(eglCreateImageKHR && eglDestroyImageKHR);
    if (!eglCreateImageKHR || !eglDestroyImageKHR) {
        using namespace BlackBerry::Platform;
        logAlways(LogLevelWarn, "eglGetProcAddress for eglCreate/DestroyImageKHR FAILED");
        m_needsDisplay = false;
        return;
    }

    GLenum currentActiveTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<GLint*>(&currentActiveTexture));
    glActiveTexture(GL_TEXTURE0);

    {
        GLES2ContextState::TextureAndFBOStateSaver textureAndFBOStateSaver;

        if (!createImageIfNeeded(size))
            return;

        m_needsDisplay = false;

        blitToFrontBuffer(backBufferTexture);
    }

    glActiveTexture(currentActiveTexture);
}

void EGLImageLayerWebKitThread::deleteFrontBuffer()
{
    glDeleteTextures(1, &m_frontBufferTexture);
    m_frontBufferTexture = 0;
    glDeleteFramebuffers(1, &m_fbo);
    m_fbo = 0;
    glDeleteShader(m_shader);
    m_shader = 0;
}

void EGLImageLayerWebKitThread::commitPendingTextureUploads()
{
    // This call is serialized with the compositing thread, so it's safe to update the
    // image and destroy any old images.

    m_client->setImage(m_image);

    // Destroy the garbage images in a thread-safe way
    for (size_t i = 0; i < m_garbage.size(); ++i)
        eglDestroyImageKHR(BlackBerry::Platform::Graphics::eglDisplay(), m_garbage[i]);
}

bool EGLImageLayerWebKitThread::createImageIfNeeded(const IntSize& size)
{
    // We use a texture rather than a renderbuffer as the basis of the FBO and EGLImage,
    // since the EGLImage will be used as a texture on the compositing thread in the end.

    if (!m_frontBufferTexture) {
        glGenTextures(1, &m_frontBufferTexture);
        glBindTexture(GL_TEXTURE_2D, m_frontBufferTexture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (!m_fbo)
        glGenFramebuffers(1, &m_fbo);

    if (!m_image || size != m_size) {
        IntSize fboSize = size;

        // Imagination-specific fix
        static bool isImaginationHardware = std::strstr(reinterpret_cast<const char*>(glGetString(GL_RENDERER)), "PowerVR SGX");
        if (isImaginationHardware)
            fboSize = fboSize.expandedTo(IntSize(16, 16));

        glBindTexture(GL_TEXTURE_2D, m_frontBufferTexture);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fboSize.width(), fboSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frontBufferTexture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return false;
        if (glGetError() != GL_NO_ERROR)
            return false;

        // The sibling is orphaned after glTexImage2D, recreate the image.
        if (m_image)
            m_garbage.append(m_image);
        m_image = eglCreateImageKHR(BlackBerry::Platform::Graphics::eglDisplay(), eglGetCurrentContext(),
                EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)m_frontBufferTexture, 0);
        if (!m_image)
            return false;

        m_size = size;
    }

    return true;
}

void EGLImageLayerWebKitThread::createShaderIfNeeded()
{
    // Shaders for drawing the layer contents.
    static char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = a_position;  \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";

    static char fragmentShaderStringRGBA[] =
        "varying mediump vec2 v_texCoord;                   \n"
        "uniform lowp sampler2D s_texture;                  \n"
        "void main()                                        \n"
        "{                                                  \n"
        "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
        "}                                                  \n";

    if (!m_shader) {
        m_shader = LayerRenderer::loadShaderProgram(vertexShaderString, fragmentShaderStringRGBA);
        if (!m_shader)
            return;
        glBindAttribLocation(m_shader, GLES2Program::PositionAttributeIndex, "a_position");
        glBindAttribLocation(m_shader, GLES2Program::TexCoordAttributeIndex, "a_texCoord");
        glLinkProgram(m_shader);
        unsigned samplerLocation = glGetUniformLocation(m_shader, "s_texture");
        glUseProgram(m_shader);
        glUniform1i(samplerLocation, 0);
    }
}

void EGLImageLayerWebKitThread::blitToFrontBuffer(unsigned backBufferTexture)
{
    if (!backBufferTexture)
        return;

    GLuint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&currentProgram));

    createShaderIfNeeded();

    GLuint currentArrayBufferBinding = 0, currentElementArrayBufferBinding = 0;
    GLint currentViewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint*>(&currentArrayBufferBinding));
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint*>(&currentElementArrayBufferBinding));
    glGetIntegerv(GL_VIEWPORT, &currentViewport[0]);

    GLES2ContextState::GlobalFlagStateSaver flagStateSaver(GLES2ContextState::GlobalFlagState::DontRestoreBlendFunc);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glViewport(0, 0, m_size.width(), m_size.height());
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glUseProgram(m_shader);
    glBindTexture(GL_TEXTURE_2D, backBufferTexture);

    {
        GLES2ContextState::VertexAttributeStateSaver vertexAttribStateSaver;

        static float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };
        static float vertices[] = { -1, -1, -1, 1, 1, 1, 1, -1 };
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(GLES2Program::PositionAttributeIndex);
        glEnableVertexAttribArray(GLES2Program::TexCoordAttributeIndex);
        glVertexAttribPointer(GLES2Program::PositionAttributeIndex, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(GLES2Program::TexCoordAttributeIndex, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // If we don't flush, the EGLImage may never update its appearance
        glFlush();

        glDisableVertexAttribArray(GLES2Program::PositionAttributeIndex);
        glDisableVertexAttribArray(GLES2Program::TexCoordAttributeIndex);

        glBindBuffer(GL_ARRAY_BUFFER, currentArrayBufferBinding);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, currentElementArrayBufferBinding);
    }

    // The previous program might have been deleted by refcount destruction
    // with another one now in use. Don't try to restore it later.
    if (currentProgram && !glIsProgram(currentProgram))
        currentProgram = 0;

    glUseProgram(currentProgram);
    glViewport(currentViewport[0], currentViewport[1], currentViewport[2], currentViewport[3]);
}

}

#endif // USE(ACCELERATED_COMPOSITING) && ENABLE(ACCELERATED_2D_CANVAS)
