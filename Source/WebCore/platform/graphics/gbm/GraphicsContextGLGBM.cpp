/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContextGLGBM.h"

#if ENABLE(WEBGL) && USE(LIBGBM)

#include "ANGLEHeaders.h"
#include "DMABufEGLUtilities.h"
#include "Logging.h"
#include "PixelBuffer.h"

#if ENABLE(MEDIA_STREAM)
#include "VideoFrame.h"
#endif

namespace WebCore {

static inline bool isDMABufSupportedByANGLEPlatform(const GraphicsContextGLGBM::EGLExtensions& eglExtensions)
{
    return eglExtensions.KHR_image_base && eglExtensions.KHR_surfaceless_context
        && eglExtensions.EXT_image_dma_buf_import;
}

RefPtr<GraphicsContextGLGBM> GraphicsContextGLGBM::create(GraphicsContextGLAttributes&& attributes)
{
    auto context = adoptRef(*new GraphicsContextGLGBM(WTFMove(attributes)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLGBM::GraphicsContextGLGBM(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLANGLE(WTFMove(attributes))
{ }

GraphicsContextGLGBM::~GraphicsContextGLGBM() = default;

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLGBM::layerContentsDisplayDelegate()
{
    return { };
}

#if ENABLE(MEDIA_STREAM)
RefPtr<VideoFrame> GraphicsContextGLGBM::paintCompositedResultsToVideoFrame()
{
    return { };
}
#endif

#if ENABLE(VIDEO)
bool GraphicsContextGLGBM::copyTextureFromMedia(MediaPlayer&, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool)
{
    return false;
}
#endif

void GraphicsContextGLGBM::setContextVisibility(bool)
{
}

void GraphicsContextGLGBM::prepareForDisplay()
{
    if (m_layerComposited || !makeContextCurrent())
        return;

    prepareTexture();
    markLayerComposited();

    m_swapchain.displayBO = WTFMove(m_swapchain.drawBO);
    allocateDrawBufferObject();
}

bool GraphicsContextGLGBM::platformInitializeContext()
{
#if ENABLE(WEBGL2)
    m_isForWebGL2 = contextAttributes().webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
#endif

    Vector<EGLint> displayAttributes {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, EGL_PLATFORM_SURFACELESS_MESA,
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

    {
        const char* extensionsString = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
        LOG(WebGL, "Extensions: %s\n", extensionsString);

        auto displayExtensions = StringView::fromLatin1(extensionsString).split(' ');
        auto findExtension =
            [&](auto extensionName) {
                return std::any_of(displayExtensions.begin(), displayExtensions.end(),
                    [&](auto extensionEntry) {
                        return extensionEntry == extensionName;
                    });
            };

        m_eglExtensions.KHR_image_base = findExtension("EGL_KHR_image_base"_s);
        m_eglExtensions.KHR_surfaceless_context = findExtension("EGL_KHR_surfaceless_context"_s);
        m_eglExtensions.EXT_image_dma_buf_import = findExtension("EGL_EXT_image_dma_buf_import"_s);
        m_eglExtensions.EXT_image_dma_buf_import_modifiers = findExtension("EGL_EXT_image_dma_buf_import_modifiers"_s);
        m_eglExtensions.ANGLE_power_preference = findExtension("EGL_ANGLE_power_preference"_s);
    }

    if (!isDMABufSupportedByANGLEPlatform(m_eglExtensions)) {
        LOG(WebGL, "Warning: GL images could not be created using DMABuf buffers backend, we fallback to common GL images, they require a copy, that causes a performance penalty.");
        return false;
    }

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

    if (m_eglExtensions.ANGLE_power_preference) {
        eglContextAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        eglContextAttributes.append(EGL_LOW_POWER_ANGLE);
    }
    eglContextAttributes.append(EGL_NONE);

    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, EGL_NO_CONTEXT, eglContextAttributes.data());
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

bool GraphicsContextGLGBM::platformInitialize()
{
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);

    // We require this extension to render into the dmabuf-backed EGLImage.
    RELEASE_ASSERT(supportsExtension("GL_OES_EGL_image"_s));
    GL_RequestExtensionANGLE("GL_OES_EGL_image");

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

void GraphicsContextGLGBM::prepareTexture()
{
    ASSERT(!m_layerComposited);

    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

    GL_Flush();
}

bool GraphicsContextGLGBM::reshapeDisplayBufferBacking()
{
    m_swapchain = Swapchain(platformDisplay());
    allocateDrawBufferObject();

    {
        GLenum textureTarget = drawingBufferTextureTarget();
        ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQueryForDrawingTarget(textureTarget), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);

        GL_BindTexture(textureTarget, m_texture);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, m_texture, 0);
    }

    return true;
}

void GraphicsContextGLGBM::allocateDrawBufferObject()
{
    if (!m_swapchain.swapchain || m_swapchain.drawBO)
        return;

    auto size = getInternalFramebufferSize();
    m_swapchain.drawBO = m_swapchain.swapchain->getBuffer(
        GBMBufferSwapchain::BufferDescription {
            .format = DMABufFormat::create(uint32_t(contextAttributes().alpha ? DMABufFormat::FourCC::ARGB8888 : DMABufFormat::FourCC::XRGB8888)),
            .width = std::clamp<uint32_t>(size.width(), 0, UINT_MAX),
            .height = std::clamp<uint32_t>(size.height(), 0, UINT_MAX),
            .flags = GBMBufferSwapchain::BufferDescription::NoFlags,
        });

    GLenum textureTarget = drawingBufferTextureTarget();
    ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQueryForDrawingTarget(textureTarget), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);

    auto result = m_swapchain.images.ensure(m_swapchain.drawBO->handle(),
        [&] {
            auto dmabufObject = m_swapchain.drawBO->createDMABufObject(0);
            auto attributes = DMABufEGLUtilities::constructEGLCreateImageAttributes(dmabufObject, 0,
                DMABufEGLUtilities::PlaneModifiersUsage { static_cast<GraphicsContextGLGBM&>(*this).eglExtensions().EXT_image_dma_buf_import_modifiers });
            return EGL_CreateImageKHR(m_swapchain.platformDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)nullptr, attributes.data());
        });

    GL_BindTexture(textureTarget, m_texture);
    GL_EGLImageTargetTexture2DOES(textureTarget, result.iterator->value);
}

GraphicsContextGLGBM::Swapchain::Swapchain(GCGLDisplay platformDisplay)
    : platformDisplay(platformDisplay)
    , swapchain(adoptRef(new GBMBufferSwapchain(GBMBufferSwapchain::BufferSwapchainSize::Four)))
{ }

GraphicsContextGLGBM::Swapchain::~Swapchain()
{
    for (EGLImageKHR image : images.values()) {
        if (image != EGL_NO_IMAGE_KHR)
            EGL_DestroyImageKHR(platformDisplay, image);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(LIBGBM)
