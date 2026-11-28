/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "GraphicsContextGLANGLEWebGPU.h"

#if ENABLE(WEBGL) && ENABLE(WEBGL_WEBGPU) && PLATFORM(COCOA)

#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "IOSurface.h"
#include "Logging.h"
#include "PixelBuffer.h"
#include "WebGPU.h"
#include "WebGPUCompositorIntegration.h"
#include "WebGPUPresentationContextDescriptor.h"
#include <WebGPU/WebGPUCAPIHandles.h>
#include <wtf/Borrow.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsContextGLANGLEWebGPU);

// The format of the render buffers. ANGLE imports a Bgra8unorm texture as GL_BGRA8_EXT, which it can
// render to only with GL_EXT_texture_format_BGRA8888, and the WebGPU backend does not have it.
static constexpr auto renderBufferFormat = ::WebGPU::TextureFormat::Rgba8unorm;

static ::WebGPU::PredefinedColorSpace predefinedColorSpace(const ColorSpace& colorSpace)
{
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    if (colorSpace == ColorSpace::DisplayP3())
        return ::WebGPU::PredefinedColorSpace::DisplayP3;
#else
    UNUSED_PARAM(colorSpace);
#endif
    return ::WebGPU::PredefinedColorSpace::SRGB;
}

static Lock displayKeysLock;
static intptr_t lastDisplayKey WTF_GUARDED_BY_LOCK(displayKeysLock);

static Vector<intptr_t>& unusedDisplayKeys() WTF_REQUIRES_LOCK(displayKeysLock)
{
    static NeverDestroyed<Vector<intptr_t>> keys;
    return keys;
}

static intptr_t takeDisplayKey()
{
    Locker locker { displayKeysLock };
    if (!unusedDisplayKeys().isEmpty())
        return unusedDisplayKeys().takeLast();
    return ++lastDisplayKey;
}

// Called when the display of the key is terminated, or was not initialized.
static void returnDisplayKey(intptr_t displayKey)
{
    Locker locker { displayKeysLock };
    unusedDisplayKeys().append(displayKey);
}

RefPtr<GraphicsContextGLANGLEWebGPU> GraphicsContextGLANGLEWebGPU::create(const GraphicsContextGLAttributes& attributes, Ref<WebGPUIntegration>&& gpu, Ref<::WebGPU::Device>&& device)
{
    RefPtr compositorIntegration = gpu->createCompositorIntegration();
    if (!compositorIntegration)
        return nullptr;
    RefPtr presentationContext = gpu->createPresentationContext(WebGPUPresentationContextDescriptor { *compositorIntegration });
    if (!presentationContext)
        return nullptr;
    Ref context = adoptRef(*new GraphicsContextGLANGLEWebGPU(attributes, WTF::move(gpu), WTF::move(device), compositorIntegration.releaseNonNull(), presentationContext.releaseNonNull()));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLANGLEWebGPU::GraphicsContextGLANGLEWebGPU(const GraphicsContextGLAttributes& attributes, Ref<WebGPUIntegration>&& gpu, Ref<::WebGPU::Device>&& device, Ref<WebGPUCompositorIntegration>&& compositorIntegration, Ref<::WebGPU::PresentationContext>&& presentationContext)
    : GraphicsContextGLANGLE(attributes)
    , m_gpu(WTF::move(gpu))
    , m_device(WTF::move(device))
    , m_compositorIntegration(WTF::move(compositorIntegration))
    , m_presentationContext(WTF::move(presentationContext))
    , m_drawingBufferColorSpace(ColorSpace::SRGB())
{
}

GraphicsContextGLANGLEWebGPU::~GraphicsContextGLANGLEWebGPU()
{
    freeDrawingBuffers();
    destroyContext();
    if (m_displayObj) {
        EGL_Terminate(std::exchange(m_displayObj, nullptr));
        returnDisplayKey(m_displayKey);
    }
}

GCGLDisplay GraphicsContextGLANGLEWebGPU::initializeEGLDisplay(intptr_t displayKey)
{
    if (!platformIsANGLEAvailable()) {
        RELEASE_LOG_ERROR(WebGL, "Failed to load ANGLE shared library.");
        return EGL_NO_DISPLAY;
    }
    RefPtr instance = m_device->instance();
    if (!instance)
        return EGL_NO_DISPLAY;
    // ANGLE calls the C API of WebGPU.framework, its default procs, on the handles. Its display takes
    // references of its own when it is initialized.
    WGPUInstance instanceHandle = ::WebGPU::retainCAPIHandle(*instance);
    WGPUDevice deviceHandle = ::WebGPU::retainCAPIHandle(m_device);
    auto releaseHandles = makeScopeExit([&] {
        wgpuDeviceRelease(deviceHandle);
        wgpuInstanceRelease(instanceHandle);
    });
    // ANGLE shares a display between the contexts that create it with the same key, whatever their
    // WebGPU device. Each context has a device of its own, so it has a display of its own.
    std::array displayAttributes {
        static_cast<EGLAttrib>(EGL_PLATFORM_ANGLE_TYPE_ANGLE), static_cast<EGLAttrib>(EGL_PLATFORM_ANGLE_TYPE_WEBGPU_ANGLE),
        static_cast<EGLAttrib>(EGL_PLATFORM_ANGLE_WEBGPU_INSTANCE_ANGLE), reinterpret_cast<EGLAttrib>(instanceHandle),
        static_cast<EGLAttrib>(EGL_PLATFORM_ANGLE_WEBGPU_DEVICE_ANGLE), reinterpret_cast<EGLAttrib>(deviceHandle),
        static_cast<EGLAttrib>(EGL_PLATFORM_ANGLE_DISPLAY_KEY_ANGLE), static_cast<EGLAttrib>(displayKey),
        static_cast<EGLAttrib>(EGL_NONE),
    };
    EGLDisplay display = EGL_GetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY), displayAttributes.data());
    if (display == EGL_NO_DISPLAY)
        return EGL_NO_DISPLAY;
    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return EGL_NO_DISPLAY;
    }
    return display;
}

bool GraphicsContextGLANGLEWebGPU::platformInitializeContext()
{
    m_displayKey = takeDisplayKey();
    m_displayObj = initializeEGLDisplay(m_displayKey);
    if (!m_displayObj) {
        returnDisplayKey(m_displayKey);
        return false;
    }
    if (initializeConfigAndContext({ }) != EGL_SUCCESS || !makeCurrent(m_displayObj, m_contextObj)) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return false;
    }
    return true;
}

bool GraphicsContextGLANGLEWebGPU::platformInitializeExtensions()
{
    // For binding the render buffers to the drawing buffer texture.
    if (!enableExtensionsImpl({ "GL_OES_EGL_image"_s }))
        return false;
#if ENABLE(WEBXR)
    // The WebXR layers need the IOSurface and MTLSharedEvent imports of the Metal backend.
    if (contextAttributes().xrCompatible)
        return false;
#endif
    return true;
}

bool GraphicsContextGLANGLEWebGPU::platformInitialize()
{
    // The render buffers are IOSurfaces.
    m_maxInternalFramebufferSize.clampToMaximumSize(IOSurface::maximumSize());
    return true;
}

void GraphicsContextGLANGLEWebGPU::freeDrawingBuffers()
{
    for (auto& buffer : borrow(m_drawingBuffers)) {
        if (buffer.image)
            EGL_DestroyImageKHR(m_displayObj, buffer.image);
    }
    m_drawingBuffers.clear();
}

bool GraphicsContextGLANGLEWebGPU::reshapeDrawingBuffer()
{
    auto size = getInternalFramebufferSize();
    ASSERT(!size.isEmpty());
    freeDrawingBuffers();

    auto attributes = contextAttributes();
    m_presentationContext->unconfigure();
    auto renderBufferCount = m_compositorIntegration->recreateRenderBuffers(size.width(), size.height(), ColorSpace { m_drawingBufferColorSpace }, attributes.alpha ? AlphaPremultiplication::Premultiplied : AlphaPremultiplication::Unpremultiplied, renderBufferFormat, m_device);
    if (!renderBufferCount)
        return false;
    // ANGLE renders to an imported texture with CopySource and RenderAttachment, and samples and
    // uploads to one with CopyDestination and TextureBinding.
    m_presentationContext->configure({
        .device = m_device,
        .format = renderBufferFormat,
        .usage = { ::WebGPU::TextureUsage::RenderAttachment, ::WebGPU::TextureUsage::CopySource, ::WebGPU::TextureUsage::CopyDestination, ::WebGPU::TextureUsage::TextureBinding },
        .viewFormats = { },
        .colorSpace = predefinedColorSpace(m_drawingBufferColorSpace),
        .toneMappingMode = ::WebGPU::CanvasToneMappingMode::Standard,
        .compositingAlphaMode = attributes.alpha ? ::WebGPU::CanvasAlphaMode::Premultiplied : ::WebGPU::CanvasAlphaMode::Opaque,
        .reportValidationErrors = true,
        .width = static_cast<uint32_t>(size.width()),
        .height = static_cast<uint32_t>(size.height()),
    });
    m_drawingBuffers.grow(renderBufferCount);
    return bindDrawingBuffer(0);
}

bool GraphicsContextGLANGLEWebGPU::bindDrawingBuffer(uint32_t index)
{
    auto drawingBuffers = borrow(m_drawingBuffers);
    auto& buffer = drawingBuffers[index];
    // Getting the texture starts a new frame in it.
    RefPtr texture = m_presentationContext->getCurrentTexture(index);
    if (!texture)
        return false;
    if (texture != buffer.texture) {
        if (buffer.image)
            EGL_DestroyImageKHR(m_displayObj, std::exchange(buffer.image, nullptr));
        // Without alpha, ANGLE keeps the alpha of the RGB8 image at one, as for an RGB drawing buffer.
        Vector<EGLint, 3> imageAttributes;
        if (!contextAttributes().alpha)
            imageAttributes.appendList({ EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_RGB8 });
        imageAttributes.append(EGL_NONE);
        // The image takes a reference of its own to the texture.
        WGPUTexture textureHandle = ::WebGPU::retainCAPIHandle(*texture);
        buffer.image = EGL_CreateImageKHR(m_displayObj, EGL_NO_CONTEXT, EGL_WEBGPU_TEXTURE_ANGLE, static_cast<EGLClientBuffer>(textureHandle), borrow(imageAttributes)->span().data());
        wgpuTextureRelease(textureHandle);
        if (!buffer.image) {
            buffer.texture = nullptr;
            return false;
        }
        buffer.texture = WTF::move(texture);
    }
    ScopedRestoreTextureBinding restoreBinding(TEXTURE_BINDING_2D, TEXTURE_2D);
    GL_BindTexture(TEXTURE_2D, m_texture);
    GL_EGLImageTargetTexture2DOES(TEXTURE_2D, buffer.image);
    return true;
}

void GraphicsContextGLANGLEWebGPU::prepareForDisplay()
{
    if (!makeContextCurrent())
        return;
    checkGPUStatus();
    if (isContextLost() || m_drawingBuffers.isEmpty())
        return;
    // FIXME: Present the frame through the compositor integration.
    GL_Flush();
}

RefPtr<PixelBuffer> GraphicsContextGLANGLEWebGPU::readCompositedResults()
{
    // FIXME: Read the presented frame.
    return nullptr;
}

void GraphicsContextGLANGLEWebGPU::setDrawingBufferColorSpace(const ColorSpace& colorSpace)
{
    if (!makeContextCurrent())
        return;
    if (m_drawingBufferColorSpace == colorSpace)
        return;
    m_drawingBufferColorSpace = colorSpace;
    if (getInternalFramebufferSize().isEmpty())
        return;
    if (!reshapeDrawingBuffer())
        forceContextLost();
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLANGLEWebGPU::layerContentsDisplayDelegate()
{
    return nullptr;
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<VideoFrame> GraphicsContextGLANGLEWebGPU::surfaceBufferToVideoFrame(SurfaceBuffer)
{
    // FIXME: Capture the canvas from the render buffers.
    return nullptr;
}
#endif

#if ENABLE(VIDEO)
bool GraphicsContextGLANGLEWebGPU::copyTextureFromVideoFrame(VideoFrame&, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, bool, bool)
{
    // The uploads take the path of the other images.
    return false;
}
#endif

#if ENABLE(WEBXR)
bool GraphicsContextGLANGLEWebGPU::addFoveation(IntSize, IntSize, IntSize, std::span<const GCGLfloat>, std::span<const GCGLfloat>, std::span<const GCGLfloat>)
{
    return false;
}

void GraphicsContextGLANGLEWebGPU::enableFoveation(GCGLuint)
{
}

void GraphicsContextGLANGLEWebGPU::disableFoveation()
{
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL) && ENABLE(WEBGL_WEBGPU) && PLATFORM(COCOA)
