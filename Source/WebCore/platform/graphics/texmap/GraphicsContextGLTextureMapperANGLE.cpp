/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContextGLTextureMapperANGLE.h"

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "GLContext.h"
#include "Logging.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include "PlatformLayerDisplayDelegate.h"

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
#include "VideoFrame.h"
#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif
#endif

#if USE(NICOSIA)
#include "NicosiaGCGLANGLELayer.h"
#else
#include "TextureMapperGCGLPlatformLayer.h"
#endif

#if USE(ANGLE_GBM)
#include "GraphicsContextGLGBMTextureMapper.h"
#endif

namespace WebCore {

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    if (!makeContextCurrent())
        return;

    if (m_texture)
        GL_DeleteTextures(1, &m_texture);

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        GL_DeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        GL_DeleteFramebuffers(1, &m_multisampleFBO);
    } else {
        if (attributes.stencil || attributes.depth) {
            if (m_depthStencilBuffer)
                GL_DeleteRenderbuffers(1, &m_depthStencilBuffer);
        }

        if (m_preserveDrawingBufferTexture)
            GL_DeleteTextures(1, &m_preserveDrawingBufferTexture);
        if (m_preserveDrawingBufferFBO)
            GL_DeleteFramebuffers(1, &m_preserveDrawingBufferFBO);
    }
    GL_DeleteFramebuffers(1, &m_fbo);

    if (m_contextObj) {
        EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_displayObj, m_contextObj);
    }

    if (m_surfaceObj)
        EGL_DestroySurface(m_displayObj, m_surfaceObj);
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
    return !!EGL_MakeCurrent(m_displayObj, m_surfaceObj, m_surfaceObj, m_contextObj);
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
}

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
}

RefPtr<PixelBuffer> GraphicsContextGLTextureMapperANGLE::readCompositedResults()
{
    return readRenderingResults();
}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes, SerialFunctionDispatcher*)
{
#if USE(ANGLE_GBM)
    auto& eglExtensions = PlatformDisplay::sharedDisplayForCompositing().eglExtensions();
    if (eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import)
        return GraphicsContextGLGBMTextureMapper::create(GraphicsContextGLAttributes { attributes });
#endif
    return GraphicsContextGLTextureMapperANGLE::create(GraphicsContextGLAttributes { attributes });
}

RefPtr<GraphicsContextGLTextureMapperANGLE> GraphicsContextGLTextureMapperANGLE::create(GraphicsContextGLAttributes&& attributes)
{
    auto context = adoptRef(*new GraphicsContextGLTextureMapperANGLE(WTFMove(attributes)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLTextureMapperANGLE::GraphicsContextGLTextureMapperANGLE(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLANGLE(WTFMove(attributes))
{
}

GraphicsContextGLTextureMapperANGLE::~GraphicsContextGLTextureMapperANGLE()
{
    if (!makeContextCurrent())
        return;

    if (m_compositorTexture)
        GL_DeleteTextures(1, &m_compositorTexture);
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLTextureMapperANGLE::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate;
}

#if ENABLE(VIDEO)
bool GraphicsContextGLTextureMapperANGLE::copyTextureFromMedia(MediaPlayer&, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool)
{
    // FIXME: Implement copy-free (or at least, software copy-free) texture transfer.
    return false;
}
#endif

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<VideoFrame> GraphicsContextGLTextureMapperANGLE::surfaceBufferToVideoFrame(SurfaceBuffer)
{
#if USE(GSTREAMER)
    if (auto pixelBuffer = readCompositedResults())
        return VideoFrameGStreamer::createFromPixelBuffer(pixelBuffer.releaseNonNull(), VideoFrameGStreamer::CanvasContentType::WebGL, VideoFrameGStreamer::Rotation::UpsideDown, MediaTime::invalidTime(), { }, 30, true, { });
#endif
    return nullptr;
}
#endif

bool GraphicsContextGLTextureMapperANGLE::platformInitializeContext()
{
    m_isForWebGL2 = contextAttributes().isWebGL2;

    auto& sharedDisplay = PlatformDisplay::sharedDisplayForCompositing();
    m_displayObj = sharedDisplay.angleEGLDisplay();
    if (m_displayObj == EGL_NO_DISPLAY)
        return false;

    const char* displayExtensions = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
    LOG(WebGL, "Extensions: %s", displayExtensions);

    bool isSurfacelessContextSupported = GLContext::isExtensionSupported(displayExtensions, "EGL_KHR_surfaceless_context");

    EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#if USE(NICOSIA)
        EGL_SURFACE_TYPE, !isSurfacelessContextSupported || sharedDisplay.type() == PlatformDisplay::Type::Surfaceless ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT,
#endif
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLint numberConfigsReturned = 0;
    EGL_ChooseConfig(m_displayObj, configAttributes, &m_configObj, 1, &numberConfigsReturned);
    if (numberConfigsReturned != 1) {
        LOG(WebGL, "EGLConfig Initialization failed.");
        return false;
    }
    LOG(WebGL, "Got EGLConfig");

    if (!isSurfacelessContextSupported) {
        static const int pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
        m_surfaceObj = EGL_CreatePbufferSurface(m_displayObj, m_configObj, pbufferAttributes);
        if (m_surfaceObj == EGL_NO_SURFACE) {
            LOG(WebGL, "Surfaceless context is not supported and we failed to create a pbuffer surface");
            return false;
        }
    }

    EGL_BindAPI(EGL_OPENGL_ES_API);
    if (EGL_GetError() != EGL_SUCCESS) {
        LOG(WebGL, "Unable to bind to OPENGL_ES_API");
        return false;
    }

    Vector<EGLint> eglContextAttributes;
    if (m_isForWebGL2) {
        eglContextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
        eglContextAttributes.append(3);
    } else {
        eglContextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
        eglContextAttributes.append(2);
        // ANGLE will upgrade the context to ES3 automatically unless this is specified.
        eglContextAttributes.append(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
        eglContextAttributes.append(EGL_FALSE);
    }
    eglContextAttributes.append(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    eglContextAttributes.append(EGL_TRUE);
    // WebGL requires that all resources are cleared at creation.
    eglContextAttributes.append(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    eglContextAttributes.append(EGL_TRUE);
    // WebGL doesn't allow client arrays.
    eglContextAttributes.append(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    eglContextAttributes.append(EGL_FALSE);
    // WebGL doesn't allow implicit creation of objects on bind.
    eglContextAttributes.append(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    eglContextAttributes.append(EGL_FALSE);
#if USE(NICOSIA)
    eglContextAttributes.append(EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE);
    eglContextAttributes.append(0);
#endif

    if (strstr(displayExtensions, "EGL_ANGLE_power_preference")) {
        eglContextAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        eglContextAttributes.append(EGL_LOW_POWER_ANGLE);
    }
    eglContextAttributes.append(EGL_NONE);

    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, sharedDisplay.angleSharingGLContext(), eglContextAttributes.data());
    if (m_contextObj == EGL_NO_CONTEXT) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return false;
    }
    if (!makeContextCurrent()) {
        LOG(WebGL, "ANGLE makeContextCurrent failed.");
        return false;
    }
    LOG(WebGL, "Got EGLContext");
    return true;
}

bool GraphicsContextGLTextureMapperANGLE::platformInitialize()
{
#if USE(NICOSIA)
    m_nicosiaLayer = makeUnique<Nicosia::GCGLANGLELayer>(*this);
    m_layerContentsDisplayDelegate = PlatformLayerDisplayDelegate::create(&m_nicosiaLayer->contentLayer());
#else
    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this);
    m_layerContentsDisplayDelegate = PlatformLayerDisplayDelegate::create(m_texmapLayer.get());
#endif

    GLenum textureTarget = drawingBufferTextureTarget();
#if USE(NICOSIA)
    GL_BindTexture(textureTarget, m_texture);
    m_textureID = setupCurrentTexture();
#endif

    GL_GenTextures(1, &m_compositorTexture);
    GL_BindTexture(textureTarget, m_compositorTexture);
    GL_TexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if USE(NICOSIA)
    m_compositorTextureID = setupCurrentTexture();
#endif
    GL_BindTexture(textureTarget, 0);

    return true;
}

void GraphicsContextGLTextureMapperANGLE::swapCompositorTexture()
{
    std::swap(m_texture, m_compositorTexture);
#if USE(NICOSIA)
    std::swap(m_textureID, m_compositorTextureID);
#endif

    if (m_preserveDrawingBufferTexture) {
        // The context requires the use of an intermediate texture in order to implement preserveDrawingBuffer:true without antialiasing.
        // m_fbo is bound at this point.
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_preserveDrawingBufferTexture, 0);
        // Attach m_texture to m_preserveDrawingBufferFBO for later blitting.
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_preserveDrawingBufferFBO);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    } else {
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
    }

    GL_Flush();

    if (m_state.boundDrawFBO != m_fbo)
        GL_BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
}

void GraphicsContextGLTextureMapperANGLE::setContextVisibility(bool)
{
}

bool GraphicsContextGLTextureMapperANGLE::reshapeDrawingBuffer()
{
    auto attrs = contextAttributes();
    const auto size = getInternalFramebufferSize();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;
    auto [textureTarget, textureBinding] = drawingBufferTextureBindingPoint();
    GLuint internalColorFormat = textureTarget == GL_TEXTURE_2D ? colorFormat : m_internalColorFormat;
    ScopedRestoreTextureBinding restoreBinding(textureBinding, textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);

    GL_BindTexture(textureTarget, m_compositorTexture);
    GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    GL_BindTexture(textureTarget, m_texture);
    GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);

    return true;
}

void GraphicsContextGLTextureMapperANGLE::prepareForDisplay()
{
    if (!makeContextCurrent())
        return;

    prepareTexture();
    swapCompositorTexture();
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)
