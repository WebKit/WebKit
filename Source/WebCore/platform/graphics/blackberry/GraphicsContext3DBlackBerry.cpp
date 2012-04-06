/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

#include "BitmapImageSingleFrameSkia.h"
#include "Extensions3DOpenGL.h"
#include "GraphicsContext.h"
#include "WebGLLayerWebKitThread.h"

#include <BlackBerryPlatformGraphics.h>

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(Attributes attribs, HostWindow* hostWindow, RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to a window.
    if (renderStyle == RenderDirectlyToHostWindow)
        return 0;

    return adoptRef(new GraphicsContext3D(attribs, hostWindow, false));
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_hostWindow(hostWindow)
    , m_context(BlackBerry::Platform::Graphics::createWebGLContext())
    , m_extensions(adoptPtr(new Extensions3DOpenGL(this)))
    , m_attrs(attrs)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_boundFBO(0)
    , m_activeTexture(GL_TEXTURE0)
    , m_boundTexture0(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
{
    if (!renderDirectlyToHostWindow) {
#if USE(ACCELERATED_COMPOSITING)
        m_compositingLayer = WebGLLayerWebKitThread::create();
#endif
        makeContextCurrent();

        Extensions3D* extensions = getExtensions();
        if (!extensions->supports("GL_IMG_multisampled_render_to_texture"))
            m_attrs.antialias = false;

        // Create a texture to render into.
        ::glGenTextures(1, &m_texture);
        ::glBindTexture(GL_TEXTURE_2D, m_texture);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ::glBindTexture(GL_TEXTURE_2D, 0);

        // Create an FBO.
        ::glGenFramebuffers(1, &m_fbo);
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffers(1, &m_depthStencilBuffer);
        m_boundFBO = m_fbo;

#if USE(ACCELERATED_COMPOSITING)
        static_cast<WebGLLayerWebKitThread*>(m_compositingLayer.get())->setWebGLContext(this);
#endif
    }

    // FIXME: If GraphicsContext3D is created with renderDirectlyToHostWindow == true,
    // makeContextCurrent() will make the shared context current.
    makeContextCurrent();

    // FIXME: Do we need to enable GL_VERTEX_PROGRAM_POINT_SIZE with GL ES2? See PR #120141.

    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    if (m_texture) {
        makeContextCurrent();
        ::glDeleteTextures(1, &m_texture);
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
        ::glDeleteFramebuffers(1, &m_fbo);
    }

    BlackBerry::Platform::Graphics::destroyWebGLContext(m_context);
}

bool GraphicsContext3D::paintsIntoCanvasBuffer() const
{
    // See PR #120141.
    return true;
}

bool GraphicsContext3D::makeContextCurrent()
{
    BlackBerry::Platform::Graphics::useWebGLContext(m_context);
    return true;
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return true;
}

bool GraphicsContext3D::isGLES2NPOTStrict() const
{
    return true;
}

bool GraphicsContext3D::isErrorGeneratedOnOutOfBoundsAccesses() const
{
    return false;
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_texture;
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return m_compositingLayer.get();
}
#endif

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight, int canvasWidth, int canvasHeight,
       GraphicsContext* context, bool flipY)
{
    // Reorder pixels into BGRA format.
    unsigned char* tempPixels = new unsigned char[imageWidth * imageHeight * 4];

    if (flipY) {
        for (int y = 0; y < imageHeight; y++) {
            const unsigned char *srcRow = imagePixels + (imageWidth * 4 * y);
            unsigned char *destRow = tempPixels + (imageWidth * 4 * (imageHeight - y - 1));
            for (int i = 0; i < imageWidth * 4; i += 4) {
                destRow[i + 0] = srcRow[i + 2];
                destRow[i + 1] = srcRow[i + 1];
                destRow[i + 2] = srcRow[i + 0];
                destRow[i + 3] = srcRow[i + 3];
            }
        }
    } else {
        for (int i = 0; i < imageWidth * imageHeight * 4; i += 4) {
            tempPixels[i + 0] = imagePixels[i + 2];
            tempPixels[i + 1] = imagePixels[i + 1];
            tempPixels[i + 2] = imagePixels[i + 0];
            tempPixels[i + 3] = imagePixels[i + 3];
        }
    }

    SkBitmap canvasBitmap;

    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, canvasWidth, canvasHeight);
    canvasBitmap.allocPixels(0, 0);
    canvasBitmap.lockPixels();
    memcpy(canvasBitmap.getPixels(), tempPixels, imageWidth * imageHeight * 4);
    canvasBitmap.unlockPixels();
    delete [] tempPixels;

    FloatRect src(0, 0, canvasWidth, canvasHeight);
    FloatRect dst(0, 0, imageWidth, imageHeight);

    RefPtr<BitmapImageSingleFrameSkia> bitmapImage = BitmapImageSingleFrameSkia::create(canvasBitmap, false);
    context->drawImage(bitmapImage.get(), ColorSpaceDeviceRGB, dst, src, CompositeCopy, DoNotRespectImageOrientation, false);
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGL)

