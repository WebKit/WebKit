/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER) && !USE(NICOSIA)

#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "Logging.h"
#include "PixelBuffer.h"
#include "PlatformLayerDisplayDelegate.h"

#include "GLContext.h"
#include "PlatformDisplay.h"
#include "TextureMapperGCGLPlatformLayer.h"

#if USE(GSTREAMER) && ENABLE(MEDIA_STREAM)
#include "VideoFrameGStreamer.h"
#endif

namespace WebCore {

GraphicsContextGLANGLE::GraphicsContextGLANGLE(GraphicsContextGLAttributes attributes)
    : GraphicsContextGL(attributes)
{
}

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
    } else if (attributes.stencil || attributes.depth) {
        if (m_depthStencilBuffer)
            GL_DeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    GL_DeleteFramebuffers(1, &m_fbo);
}

GCGLDisplay GraphicsContextGLANGLE::platformDisplay() const
{
    return m_displayObj;
}

GCGLConfig GraphicsContextGLANGLE::platformConfig() const
{
    return m_configObj;
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
    if (EGL_GetCurrentContext() == m_contextObj)
        return true;
    return !!EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj);
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
}

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
}

RefPtr<PixelBuffer> GraphicsContextGLANGLE::readCompositedResults()
{
    return readRenderingResults();
}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes, SerialFunctionDispatcher*)
{
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
#if USE(COORDINATED_GRAPHICS)
    if (m_intermediateTexture)
        GL_DeleteTextures(1, &m_intermediateTexture);
#endif
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLTextureMapperANGLE::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate;
}

#if ENABLE(VIDEO)
bool GraphicsContextGLTextureMapperANGLE::copyTextureFromMedia(MediaPlayer&, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool)
{
    // FIXME: Implement copy-free (or at least, software copy-free) texture transfer via dmabuf.
    return false;
}
#endif

#if ENABLE(MEDIA_STREAM)
RefPtr<VideoFrame> GraphicsContextGLTextureMapperANGLE::paintCompositedResultsToVideoFrame()
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
#if ENABLE(WEBGL2)
    m_isForWebGL2 = contextAttributes().webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
#endif

    Vector<EGLint> displayAttributes {
#if !OS(WINDOWS)
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, EGL_PLATFORM_SURFACELESS_MESA,
#endif
        EGL_NONE,
    };

    m_displayObj = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes.data());
    if (m_displayObj == EGL_NO_DISPLAY)
        return false;

    EGLint majorVersion, minorVersion;
    if (EGL_Initialize(m_displayObj, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return false;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);

    const char* displayExtensions = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
    LOG(WebGL, "Extensions: %s", displayExtensions);

    EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
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

    if (strstr(displayExtensions, "EGL_ANGLE_power_preference")) {
        eglContextAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        eglContextAttributes.append(EGL_LOW_POWER_ANGLE);
    }
    eglContextAttributes.append(EGL_NONE);

    auto sharingContext = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->platformContext();
    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, sharingContext, eglContextAttributes.data());
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
#if ENABLE(WEBGL2)
    if (m_isForWebGL2)
        GL_Enable(GraphicsContextGL::PRIMITIVE_RESTART_FIXED_INDEX);
#endif

    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this);
    m_layerContentsDisplayDelegate = PlatformLayerDisplayDelegate::create(m_texmapLayer.get());

    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);

    // We require this extension to render into the dmabuf-backed EGLImage.
    RELEASE_ASSERT(supportsExtension("GL_OES_EGL_image"_s));
    GL_RequestExtensionANGLE("GL_OES_EGL_image");

    Vector<ASCIILiteral, 4> requiredExtensions;
#if ENABLE(WEBGL2)
    if (m_isForWebGL2) {
        // For WebGL 2.0 occlusion queries to work.
        requiredExtensions.append("GL_EXT_occlusion_query_boolean"_s);
        requiredExtensions.append("GL_ANGLE_framebuffer_multisample"_s);
    }
#endif

    for (auto& extension : requiredExtensions) {
        if (!supportsExtension(extension)) {
            LOG(WebGL, "Missing required extension. %s", extension.characters());
            return false;
        }
        ensureExtensionEnabled(extension);
    }

    validateAttributes();
    auto attributes = contextAttributes(); // They may have changed during validation.

    GLenum textureTarget = drawingBufferTextureTarget();
    // Create a texture to render into.
    GL_GenTextures(1, &m_texture);
    GL_BindTexture(textureTarget, m_texture);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindTexture(textureTarget, 0);

    // Create an FBO.
    GL_GenFramebuffers(1, &m_fbo);
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    GL_GenTextures(1, &m_compositorTexture);
    GL_BindTexture(textureTarget, m_compositorTexture);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if USE(COORDINATED_GRAPHICS)
    GL_GenTextures(1, &m_intermediateTexture);
    GL_BindTexture(textureTarget, m_intermediateTexture);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL_BindTexture(textureTarget, 0);
#endif

    // Create a multisample FBO.
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        GL_GenFramebuffers(1, &m_multisampleFBO);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        GL_GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
        if (attributes.stencil || attributes.depth)
            GL_GenRenderbuffers(1, &m_depthStencilBuffer);
    }

    GL_ClearColor(0, 0, 0, 0);
    return GraphicsContextGLANGLE::platformInitialize();
}

void GraphicsContextGLTextureMapperANGLE::prepareTexture()
{
    ASSERT(!m_layerComposited);

    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

    std::swap(m_texture, m_compositorTexture);
#if USE(COORDINATED_GRAPHICS)
    std::swap(m_texture, m_intermediateTexture);
#endif

    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
    GL_Flush();

    if (m_state.boundDrawFBO != m_fbo)
        GL_BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
}

void GraphicsContextGLTextureMapperANGLE::setContextVisibility(bool)
{
}

bool GraphicsContextGLTextureMapperANGLE::reshapeDisplayBufferBacking()
{
    auto attrs = contextAttributes();
    const auto size = getInternalFramebufferSize();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;
    GLenum textureTarget = drawingBufferTextureTarget();
    GLuint internalColorFormat = textureTarget == GL_TEXTURE_2D ? colorFormat : m_internalColorFormat;
    ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQueryForDrawingTarget(textureTarget), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);

    GL_BindTexture(textureTarget, m_compositorTexture);
    GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
#if USE(COORDINATED_GRAPHICS)
    GL_BindTexture(textureTarget, m_intermediateTexture);
    GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
#endif
    GL_BindTexture(textureTarget, m_texture);
    GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, m_texture, 0);

    return true;
}

void GraphicsContextGLTextureMapperANGLE::prepareForDisplay()
{
    if (m_layerComposited || !makeContextCurrent())
        return;

    prepareTexture();
    markLayerComposited();
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER) && !USE(NICOSIA)
