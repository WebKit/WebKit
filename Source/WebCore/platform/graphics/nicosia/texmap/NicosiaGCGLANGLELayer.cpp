/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaGCGLANGLELayer.h"

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#include "ANGLEHeaders.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerDmabuf.h"
#include "TextureMapperPlatformLayerProxyGL.h"

namespace Nicosia {

using namespace WebCore;

void GCGLANGLELayer::swapBuffersIfNeeded()
{
    if (m_context.layerComposited())
        return;

    m_context.prepareTexture();

    auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(contentLayer().impl()).proxy();
    auto size = m_context.getInternalFramebufferSize();

    if (m_context.m_compositorTextureBacking) {
        if (m_context.m_compositorTextureBacking->isReleased())
            return;

        auto format = m_context.m_compositorTextureBacking->format();
        auto stride = m_context.m_compositorTextureBacking->stride();
        auto fd = m_context.m_compositorTextureBacking->fd();

        {
            Locker locker { proxy.lock() };
            ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
            downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(makeUnique<TextureMapperPlatformLayerDmabuf>(size, format, stride, fd));
        }

        m_context.markLayerComposited();
        return;
    }

    // Fallback path, read back texture to main memory
    RefPtr<WebCore::ImageBuffer> imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return;
    m_context.paintRenderingResultsToCanvas(*imageBuffer.get());

    auto flags = m_context.contextAttributes().alpha ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag;
    {
        Locker locker { proxy.lock() };
        ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
        std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = downcast<TextureMapperPlatformLayerProxyGL>(proxy).getAvailableBuffer(size, m_context.m_internalColorFormat);
        if (!layerBuffer) {
            auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get(), flags, m_context.m_internalColorFormat);
            layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture), flags);
        }

        layerBuffer->textureGL().setPendingContents(ImageBuffer::sinkIntoImage(WTFMove(imageBuffer)));
        downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
    }
    m_context.markLayerComposited();
}

const char* GCGLANGLELayer::ANGLEContext::errorString(int statusCode)
{
    static_assert(sizeof(int) >= sizeof(EGLint), "EGLint must not be wider than int");
    switch (statusCode) {
#define CASE_RETURN_STRING(name) case name: return #name
        // https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglGetError.xhtml
        CASE_RETURN_STRING(EGL_SUCCESS);
        CASE_RETURN_STRING(EGL_NOT_INITIALIZED);
        CASE_RETURN_STRING(EGL_BAD_ACCESS);
        CASE_RETURN_STRING(EGL_BAD_ALLOC);
        CASE_RETURN_STRING(EGL_BAD_ATTRIBUTE);
        CASE_RETURN_STRING(EGL_BAD_CONTEXT);
        CASE_RETURN_STRING(EGL_BAD_CONFIG);
        CASE_RETURN_STRING(EGL_BAD_CURRENT_SURFACE);
        CASE_RETURN_STRING(EGL_BAD_DISPLAY);
        CASE_RETURN_STRING(EGL_BAD_SURFACE);
        CASE_RETURN_STRING(EGL_BAD_MATCH);
        CASE_RETURN_STRING(EGL_BAD_PARAMETER);
        CASE_RETURN_STRING(EGL_BAD_NATIVE_PIXMAP);
        CASE_RETURN_STRING(EGL_BAD_NATIVE_WINDOW);
        CASE_RETURN_STRING(EGL_CONTEXT_LOST);
#undef CASE_RETURN_STRING
    default: return "Unknown EGL error";
    }
}

const char* GCGLANGLELayer::ANGLEContext::lastErrorString()
{
    return errorString(EGL_GetError());
}

std::unique_ptr<GCGLANGLELayer::ANGLEContext> GCGLANGLELayer::ANGLEContext::createContext(bool isForWebGL2)
{
    Vector<EGLint> displayAttributes {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, EGL_PLATFORM_SURFACELESS_MESA,
        EGL_NONE,
    };
    EGLDisplay display = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes.data());
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    EGLint majorVersion, minorVersion;
    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);

    const char* displayExtensions = EGL_QueryString(display, EGL_EXTENSIONS);
    LOG(WebGL, "Extensions: %s", displayExtensions);

    EGLConfig config;
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
    EGL_ChooseConfig(display, configAttributes, &config, 1, &numberConfigsReturned);
    if (numberConfigsReturned != 1) {
        LOG(WebGL, "EGLConfig Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "Got EGLConfig");

    EGL_BindAPI(EGL_OPENGL_ES_API);
    if (EGL_GetError() != EGL_SUCCESS) {
        LOG(WebGL, "Unable to bind to OPENGL_ES_API");
        return nullptr;
    }

    std::vector<EGLint> contextAttributes;
    if (isForWebGL2) {
        contextAttributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
        contextAttributes.push_back(3);
    } else {
        contextAttributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
        contextAttributes.push_back(2);
        // ANGLE will upgrade the context to ES3 automatically unless this is specified.
        contextAttributes.push_back(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
        contextAttributes.push_back(EGL_FALSE);
    }
    contextAttributes.push_back(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    contextAttributes.push_back(EGL_TRUE);

    // WebGL requires that all resources are cleared at creation.
    contextAttributes.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    contextAttributes.push_back(EGL_TRUE);

    // WebGL doesn't allow client arrays.
    contextAttributes.push_back(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    contextAttributes.push_back(EGL_FALSE);

    // WebGL doesn't allow implicit creation of objects on bind.
    contextAttributes.push_back(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    contextAttributes.push_back(EGL_FALSE);

    if (strstr(displayExtensions, "EGL_ANGLE_power_preference")) {
        contextAttributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        contextAttributes.push_back(EGL_LOW_POWER_ANGLE);
    }
    contextAttributes.push_back(EGL_NONE);

    EGLContext context = EGL_CreateContext(display, config, EGL_NO_CONTEXT, contextAttributes.data());
    if (context == EGL_NO_CONTEXT) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "Got EGLContext");

    return std::unique_ptr<ANGLEContext>(new ANGLEContext(display, config, context, EGL_NO_SURFACE));
}

GCGLANGLELayer::ANGLEContext::ANGLEContext(EGLDisplay display, EGLConfig config, EGLContext context, EGLSurface surface)
    : m_display(display)
    , m_config(config)
    , m_context(context)
    , m_surface(surface)
{
}

GCGLANGLELayer::ANGLEContext::~ANGLEContext()
{
    if (m_context) {
        GL_BindFramebuffer(GL_FRAMEBUFFER, 0);
        EGL_MakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_display, m_context);
    }

    if (m_surface)
        EGL_DestroySurface(m_display, m_surface);
}

bool GCGLANGLELayer::ANGLEContext::makeContextCurrent()
{
    ASSERT(m_context);

    if (EGL_GetCurrentContext() != m_context)
        return EGL_MakeCurrent(m_display, m_surface, m_surface, m_context);
    return true;
}

GCGLContext GCGLANGLELayer::ANGLEContext::platformContext() const
{
    return m_context;
}

GCGLDisplay GCGLANGLELayer::ANGLEContext::platformDisplay() const
{
    return m_display;
}

GCGLConfig GCGLANGLELayer::ANGLEContext::platformConfig() const
{
    return m_config;
}

GCGLANGLELayer::GCGLANGLELayer(GraphicsContextGLANGLE& context)
    : m_context(context)
    , m_angleContext(ANGLEContext::createContext(context.m_isForWebGL2))
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
}

GCGLANGLELayer::~GCGLANGLELayer()
{
    downcast<ContentLayerTextureMapperImpl>(m_contentLayer->impl()).invalidateClient();
}

bool GCGLANGLELayer::makeContextCurrent()
{
    ASSERT(m_angleContext);
    return m_angleContext->makeContextCurrent();

}

GCGLContext GCGLANGLELayer::platformContext() const
{
    ASSERT(m_angleContext);
    return m_angleContext->platformContext();
}

GCGLDisplay GCGLANGLELayer::platformDisplay() const
{
    ASSERT(m_angleContext);
    return m_angleContext->platformDisplay();
}

GCGLConfig GCGLANGLELayer::platformConfig() const
{
    ASSERT(m_angleContext);
    return m_angleContext->platformContext();
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
