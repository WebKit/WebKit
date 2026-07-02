/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "OpenXRGraphicsBindingOpenGLES.h"

#if ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "OpenXRExtensions.h"
#include "OpenXRSwapchain.h"
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif
#include <WebCore/FourCC.h>
#include <WebCore/GLContext.h>
#include <WebCore/GLDisplay.h>
#include <WebCore/GLFence.h>
#include <wtf/RunLoop.h>
#include <wtf/SafeStrerror.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if OS(ANDROID)
#include <android/hardware_buffer.h>
#endif

#if USE(GBM)
#include "DRMMainDevice.h"
#include <WebCore/DMABufBuffer.h>
#include <WebCore/DRMDevice.h>
#include <WebCore/GBMDevice.h>
#include <WebCore/GBMVersioning.h>
#include <drm_fourcc.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRGraphicsBindingOpenGLES);

std::unique_ptr<OpenXRGraphicsBindingOpenGLES> OpenXRGraphicsBindingOpenGLES::create()
{
    return std::unique_ptr<OpenXRGraphicsBindingOpenGLES>(new OpenXRGraphicsBindingOpenGLES());
}

OpenXRGraphicsBindingOpenGLES::OpenXRGraphicsBindingOpenGLES() = default;

OpenXRGraphicsBindingOpenGLES::~OpenXRGraphicsBindingOpenGLES()
{
    ASSERT(!m_glContext);
}

Vector<ASCIILiteral> OpenXRGraphicsBindingOpenGLES::requiredInstanceExtensions() const
{
    Vector<ASCIILiteral> extensions;
#if defined(XR_USE_PLATFORM_EGL)
    if (OpenXRExtensions::singleton().isExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_span))
        extensions.append(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_s);
#endif
    extensions.append(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME ""_s);
    return extensions;
}

bool OpenXRGraphicsBindingOpenGLES::initializeDisplay(bool isForTesting)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_glDisplay);

    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    auto tryCreateDisplay = [&](EGLenum platform, void *native) -> RefPtr<WebCore::GLDisplay> {
        if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
            return WebCore::GLDisplay::create(eglGetPlatformDisplayEXT(platform, native, nullptr));
        if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
            return WebCore::GLDisplay::create(eglGetPlatformDisplay(platform, native, nullptr));
        return nullptr;
    };

    RefPtr<WebCore::GLDisplay> glDisplay;

#if OS(ANDROID)
    if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_android")) {
        glDisplay = tryCreateDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY);
        if (!glDisplay)
            glDisplay = WebCore::GLDisplay::create(eglGetDisplay(EGL_DEFAULT_DISPLAY));
        if (glDisplay && !(glDisplay->extensions().ANDROID_get_native_client_buffer && glDisplay->extensions().ANDROID_image_native_buffer))
            glDisplay = nullptr;
    }
#endif // OS(ANDROID)

    if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_MESA_platform_surfaceless")) {
        glDisplay = tryCreateDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY);
        if (glDisplay && !isForTesting && !glDisplay->extensions().MESA_image_dma_buf_export)
            glDisplay = nullptr;
    }

#if USE(GBM)
    if (!glDisplay && WebCore::GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_gbm")) {
        const auto& mainDevice = drmMainDevice();
        if (!mainDevice.isNull()) {
            m_gbmDevice = WebCore::GBMDevice::create(!mainDevice.renderNode.isNull() ? mainDevice.renderNode : mainDevice.primaryNode);
            if (m_gbmDevice)
                glDisplay = tryCreateDisplay(EGL_PLATFORM_GBM_KHR, m_gbmDevice->device());
        }
    }
#endif

    m_glDisplay = WTF::move(glDisplay);
    return !!m_glDisplay;
}

bool OpenXRGraphicsBindingOpenGLES::initializeForSession(XrInstance instance, XrSystemId systemId)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_glDisplay);

#if !OS(ANDROID)
    if (!OpenXRExtensions::singleton().isExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_span)) {
        LOG(XR, "OpenXR MNDX_EGL_ENABLE extension is not supported.");
        return false;
    }
#endif

    auto requirements = createOpenXRStruct<XrGraphicsRequirementsOpenGLESKHR, XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR>();
    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrGetOpenGLESGraphicsRequirementsKHR(instance, systemId, &requirements));

    if (!m_glContext) {
#if USE(GBM)
        const WebCore::GLContext::Target target = m_gbmDevice ? WebCore::GLContext::Target::Default : WebCore::GLContext::Target::Surfaceless;
#else
        static const WebCore::GLContext::Target target = WebCore::GLContext::Target::Surfaceless;
#endif
        m_glContext = WebCore::GLContext::create(*m_glDisplay, target);
        if (!m_glContext) {
            LOG(XR, "Failed to create the GL context for OpenXR.");
            return false;
        }
        if (!m_glContext->makeContextCurrent()) {
            LOG(XR, "Failed to make the GL context current.");
            return false;
        }
    }

#if OS(ANDROID)
    m_graphicsBinding = createOpenXRStruct<XrGraphicsBindingOpenGLESAndroidKHR, XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR>();
#else
    m_graphicsBinding = createOpenXRStruct<XrGraphicsBindingEGLMNDX, XR_TYPE_GRAPHICS_BINDING_EGL_MNDX>();
    m_graphicsBinding.getProcAddress = OpenXRExtensions::singleton().methods().getProcAddressFunc;
#endif
    m_graphicsBinding.display = m_glDisplay->eglDisplay();
    m_graphicsBinding.context = m_glContext->platformContext();
    m_graphicsBinding.config = m_glContext->config();

    return true;
}

const void* OpenXRGraphicsBindingOpenGLES::sessionGraphicsBinding() const
{
    return &m_graphicsBinding;
}

int64_t OpenXRGraphicsBindingOpenGLES::selectColorFormat(const Vector<int64_t>& supportedFormats, bool) const
{
    // Even if alpha is false we always ask for the RGBA8 format, as the DRM_FORMAT_RGB8 is not supported by ANGLE.
    // In this case we ignore the alpha channel by using DRM_FORMAT_XRGB8888 when exporting the texture.
    int64_t preferredFormat = GL_RGBA8;
    return supportedFormats.contains(preferredFormat) ? preferredFormat : supportedFormats.first();
}

Vector<uint64_t> OpenXRGraphicsBindingOpenGLES::enumerateSwapchainImages(XrSwapchain swapchain) const
{
    uint32_t imageCount;
    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
    if (!imageCount) {
        LOG(XR, "xrEnumerateSwapchainImages(): no images\n");
        return { };
    }

    Vector<XrSwapchainImageOpenGLESKHR> imageBuffers(FillWith { }, imageCount, [] {
        return createOpenXRStruct<XrSwapchainImageOpenGLESKHR, XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR>();
    }());

    Vector<XrSwapchainImageBaseHeader*> imageHeaders = imageBuffers.map([](auto& image) {
        return (XrSwapchainImageBaseHeader*) &image;
    });

    // Get images from an XrSwapchain
    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, imageHeaders[0]));

    return imageBuffers.map([](auto& image) -> uint64_t {
        return image.image;
    });
}

void OpenXRGraphicsBindingOpenGLES::waitFrameFence(WTF::UnixFileDescriptor&& fenceFD)
{
    ASSERT(m_glDisplay);
    if (auto fence = WebCore::GLFence::importFD(*m_glDisplay, WTF::move(fenceFD)))
        fence->serverWait();
}

void OpenXRGraphicsBindingOpenGLES::releaseSessionGraphics()
{
    // Deleting the GL objects below requires the context that owns them to be current. The context may be null if the session graphics
    // were never initialized, in which case there is nothing to delete.
    ASSERT(!m_glContext || WebCore::GLContext::current() == m_glContext.get());

#if USE(GBM) || OS(ANDROID)
    if (m_fbosForBlitting[0])
        glDeleteFramebuffers(m_fbosForBlitting.size(), m_fbosForBlitting.data());
    m_fbosForBlitting = { 0, 0 };
    for (auto texture : m_exportedTexturesMap.values())
        glDeleteTextures(1, &texture);
    m_exportedTexturesMap.clear();
#endif

#if defined(XR_KHR_composition_layer_cube)
    for (auto texture : m_sideBySideTextures.values())
        glDeleteTextures(1, &texture);
    m_sideBySideTextures.clear();
    if (m_reconstructionFBOs[0])
        glDeleteFramebuffers(m_reconstructionFBOs.size(), m_reconstructionFBOs.data());
    m_reconstructionFBOs = { 0, 0 };
#endif

    m_glContext = nullptr;
}

#if OS(ANDROID)
std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingOpenGLES::exportOpenXRTextureAndroid(const OpenXRSwapchain& swapchain, PlatformGLObject openxrTexture, uint32_t width, uint32_t height)
{
    static constexpr auto kHardwareBufferUsage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    RefPtr<AHardwareBuffer> hardwareBuffer;
    {
        RELEASE_ASSERT(width > 0);
        RELEASE_ASSERT(height > 0);

        AHardwareBuffer_Desc bufferDesc = { };
        bufferDesc.width = width;
        bufferDesc.height = height;
        bufferDesc.usage = kHardwareBufferUsage;
        bufferDesc.layers = 1;

        switch (swapchain.format()) {
        case GL_RGBA8:
            bufferDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
            break;
        case GL_RGB8:
            bufferDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
            if (!AHardwareBuffer_isSupported(&bufferDesc))
                bufferDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
            break;
        case GL_RGB565:
            bufferDesc.format = AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
            break;
        case GL_RGBA16F:
            bufferDesc.format = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
            break;
        case GL_RGB10_A2:
            bufferDesc.format = AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
            break;
        }

        if (!bufferDesc.format || !AHardwareBuffer_isSupported(&bufferDesc)) {
            RELEASE_LOG_INFO(XR, "AHardwareBuffer format %#" PRIX32 " not supported, using"
                " RGBA8888 fallback that may result in slow blits", bufferDesc.format);
            bufferDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        }

        AHardwareBuffer* buffer { nullptr };
        if (auto error = AHardwareBuffer_allocate(&bufferDesc, &buffer)) {
            if (error < 0)
                RELEASE_LOG_ERROR(XR, "Failed to allocate AHardwareBuffer for OpenXR texture: %s", safeStrerror(-error).data());
            else
                RELEASE_LOG_ERROR(XR, "Failed to allocate AHardwareBuffer for OpenXR texture: %" PRIi32, error);
            return { };
        }
        hardwareBuffer = adoptRef(buffer);
    }

    static PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC s_eglGetNativeClientBufferANDROID { nullptr };
    if (!s_eglGetNativeClientBufferANDROID) [[unlikely]] {
        s_eglGetNativeClientBufferANDROID = reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(eglGetProcAddress("eglGetNativeClientBufferANDROID"));
        RELEASE_ASSERT(s_eglGetNativeClientBufferANDROID);
    }

    static const Vector<EGLAttrib> attributes = { EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE };
    auto clientBuffer = s_eglGetNativeClientBufferANDROID(hardwareBuffer.get());
    auto image = m_glDisplay->createImage(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attributes);
    if (image == EGL_NO_IMAGE_KHR) {
        RELEASE_LOG(XR, "Failed to create EGL image for OpenXR texture (%#06x)", eglGetError());
        return { };
    }

    GLint boundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    PlatformGLObject exportedTexture;
    glGenTextures(1, &exportedTexture);
    glBindTexture(GL_TEXTURE_2D, exportedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, boundTexture);

    m_glDisplay->destroyImage(image);

    m_exportedTexturesMap.add(openxrTexture, exportedTexture);

    return hardwareBuffer;
}
#else
std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingOpenGLES::exportOpenXRTextureDMABuf(const OpenXRSwapchain& swapchain, PlatformGLObject openxrTexture)
{
    UNUSED_PARAM(swapchain);

    // Texture must be bound to be exported.
    glBindTexture(GL_TEXTURE_2D, openxrTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    auto image = m_glDisplay->createImage(m_glContext->platformContext(), EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)openxrTexture, { });

    auto releaseImageOnError = makeScopeExit([&] {
        if (image)
            m_glDisplay->destroyImage(image);
    });

    if (!image) {
        RELEASE_LOG(XR, "Failed to create EGL image from OpenXR texture");
        return std::nullopt;
    }

    int fourcc, planeCount;
    uint64_t modifier;
    if (!eglExportDMABUFImageQueryMESA(m_glDisplay->eglDisplay(), image, &fourcc, &planeCount, &modifier)) {
        RELEASE_LOG(XR, "eglExportDMABUFImageQueryMESA failed");
        return std::nullopt;
    }

    Vector<int> fdsOut(planeCount);
    Vector<int> stridesOut(planeCount);
    Vector<int> offsetsOut(planeCount);
    if (!eglExportDMABUFImageMESA(m_glDisplay->eglDisplay(), image, fdsOut.mutableSpan().data(), stridesOut.mutableSpan().data(), offsetsOut.mutableSpan().data())) {
        RELEASE_LOG(XR, "eglExportDMABUFImageMESA failed");
        return std::nullopt;
    }

    m_glDisplay->destroyImage(image);

    releaseImageOnError.release();

    Vector<WTF::UnixFileDescriptor> fds = fdsOut.map([](int fd) {
        return WTF::UnixFileDescriptor(fd, WTF::UnixFileDescriptor::Adopt);
    });
    Vector<uint32_t> strides = stridesOut.map([](int stride) {
        return static_cast<uint32_t>(stride);
    });
    Vector<uint32_t> offsets = offsetsOut.map([](int offset) {
        return static_cast<uint32_t>(offset);
    });

    return PlatformXR::FrameData::ExternalTexture {
        .fds = WTF::move(fds),
        .strides = WTF::move(strides),
        .offsets = WTF::move(offsets),
        .fourcc = static_cast<uint32_t>(fourcc),
        .modifier = modifier,
    };
}
#endif // !OS(ANDROID)

#if USE(GBM)
std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingOpenGLES::exportOpenXRTextureGBM(const OpenXRSwapchain& swapchain, PlatformGLObject openxrTexture, uint32_t width, uint32_t height)
{
    static constexpr std::array<WebCore::FourCC, 3> preferredAlphaDRMFormats = { DRM_FORMAT_ARGB8888, DRM_FORMAT_RGBA8888, DRM_FORMAT_ABGR8888 };
    static constexpr std::array<WebCore::FourCC, 3> preferredNoAlphaDRMFormats = { DRM_FORMAT_XRGB8888, DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888 };
    const auto& preferredDRMFormats = swapchain.hasAlpha() == OpenXRSwapchain::HasAlpha::Yes ? preferredAlphaDRMFormats : preferredNoAlphaDRMFormats;
    WebCore::GLDisplay::BufferFormat format;
    const auto& supportedFormats = m_glDisplay->bufferFormats();
    for (const auto& preferredFormat : preferredDRMFormats) {
        auto matchIndex = supportedFormats.findIf([preferredFormat](const auto& supportedFormat) {
            return supportedFormat.fourcc == preferredFormat;
        });
        if (matchIndex != notFound) {
            format = supportedFormats[matchIndex];
            break;
        }
    }

    if (!format.fourcc.value) {
        RELEASE_LOG(XR, "OpenXR texture format not supported");
        return std::nullopt;
    }

    auto* buffer = gbm_bo_create_with_modifiers2(m_gbmDevice->device(), width, height, format.fourcc.value, format.modifiers.span().data(), format.modifiers.size(), GBM_BO_USE_RENDERING);
    if (!buffer)
        buffer = gbm_bo_create(m_gbmDevice->device(), width, height, format.fourcc.value, GBM_BO_USE_RENDERING);
    if (!buffer) {
        RELEASE_LOG(XR, "Failed to allocate GBM buffer for OpenXR texture");
        return std::nullopt;
    }

    auto dmaBufAttributes = WebCore::DMABufBufferAttributes::fromGBMBufferObject(buffer);
    if (!dmaBufAttributes) {
        gbm_bo_destroy(buffer);
        RELEASE_LOG(XR, "Failed to extract DMA-buf attributes from GBM buffer for OpenXR texture");
        return std::nullopt;
    }

    auto image = WebCore::DMABufBuffer::createEGLImage(*m_glDisplay, *dmaBufAttributes);
    gbm_bo_destroy(buffer);
    if (!image) {
        RELEASE_LOG(XR, "Failed to create EGL image from OpenXR texture");
        return std::nullopt;
    }

    GLint boundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    PlatformGLObject exportedTexture;
    glGenTextures(1, &exportedTexture);
    glBindTexture(GL_TEXTURE_2D, exportedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, boundTexture);

    m_glDisplay->destroyImage(image);

    m_exportedTexturesMap.add(openxrTexture, exportedTexture);

    return PlatformXR::FrameData::ExternalTexture {
        .fds = WTF::move(dmaBufAttributes->fds),
        .strides = WTF::move(dmaBufAttributes->strides),
        .offsets = WTF::move(dmaBufAttributes->offsets),
        .fourcc = dmaBufAttributes->fourcc.value,
        .modifier = dmaBufAttributes->modifier
    };
}
#endif // USE(GBM)

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingOpenGLES::exportTexture(uint64_t image, const OpenXRSwapchain& swapchain, TextureType type, uint32_t width, uint32_t height)
{
    ASSERT(m_glContext);
    ASSERT(m_glDisplay);

    switch (type) {
    case TextureType::Texture2D:
        return exportTexture2D(static_cast<PlatformGLObject>(image), swapchain, width, height);
    case TextureType::Cubemap:
#if defined(XR_KHR_composition_layer_cube)
        return exportCubeBuffer(image, swapchain, width, height);
#else
        break;
#endif
    }
    return std::nullopt;
}

void OpenXRGraphicsBindingOpenGLES::commitFrame(uint64_t keyImage, const OpenXRSwapchain& swapchain, TextureType type, const Vector<uint64_t>& images)
{
    switch (type) {
    case TextureType::Texture2D:
        blitTextureIfNeeded(swapchain);
        break;
    case TextureType::Cubemap:
#if defined(XR_KHR_composition_layer_cube)
        reconstructCubeFaces(keyImage, images, static_cast<uint32_t>(swapchain.width()));
#endif
        break;
    }
#if !defined(XR_KHR_composition_layer_cube)
    UNUSED_PARAM(keyImage);
    UNUSED_PARAM(images);
#endif
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingOpenGLES::exportTexture2D(PlatformGLObject openxrTexture, const OpenXRSwapchain& swapchain, [[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height)
{
#if OS(ANDROID)
    return exportOpenXRTextureAndroid(swapchain, openxrTexture, width, height);
#else
    if (m_glDisplay->extensions().MESA_image_dma_buf_export)
        return exportOpenXRTextureDMABuf(swapchain, openxrTexture);
#endif

#if USE(GBM)
    if (m_gbmDevice)
        return exportOpenXRTextureGBM(swapchain, openxrTexture, width, height);
#endif

    RELEASE_LOG(XR, "Failed to export OpenXR texture");
    return std::nullopt;
}

void OpenXRGraphicsBindingOpenGLES::blitTextureIfNeeded(const OpenXRSwapchain& swapchain)
{
#if OS(ANDROID) || USE(GBM)
#if !OS(ANDROID)
    // Only the GBM path renders into a separate exported buffer that has to be blitted back; the
    // MESA dma-buf path exports the swapchain image directly, so there is nothing to blit.
    if (!m_gbmDevice)
        return;
#endif
    if (!m_fbosForBlitting[0])
        glGenFramebuffers(m_fbosForBlitting.size(), m_fbosForBlitting.data());

    auto openxrTexture = static_cast<PlatformGLObject>(swapchain.acquiredTexture());
    ASSERT(openxrTexture);

    auto exportedTexture = m_exportedTexturesMap.get(openxrTexture);
    ASSERT(exportedTexture);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbosForBlitting[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, exportedTexture, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbosForBlitting[1]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, openxrTexture, 0);

    auto width = swapchain.width();
    auto height = swapchain.height();
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    UNUSED_PARAM(swapchain);
#endif
}

#if defined(XR_KHR_composition_layer_cube)
std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingOpenGLES::exportCubeBuffer(uint64_t keyImage, const OpenXRSwapchain& swapchain, uint32_t width, uint32_t height)
{
    ASSERT(m_glContext);
    ASSERT(m_glDisplay);

    auto key = static_cast<PlatformGLObject>(keyImage);
    PlatformGLObject sideBySideTexture = 0;
#if OS(ANDROID)
    auto externalTexture = exportOpenXRTextureAndroid(swapchain, key, width, height);
    if (!externalTexture)
        return std::nullopt;
    sideBySideTexture = m_exportedTexturesMap.take(key);
#else
    GLint boundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    glGenTextures(1, &sideBySideTexture);
    glBindTexture(GL_TEXTURE_2D, sideBySideTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glBindTexture(GL_TEXTURE_2D, boundTexture);

    std::optional<PlatformXR::FrameData::ExternalTexture> externalTexture;
    if (m_glDisplay->extensions().MESA_image_dma_buf_export)
        externalTexture = exportOpenXRTextureDMABuf(swapchain, sideBySideTexture);
#if USE(GBM)
    else if (m_gbmDevice)
        externalTexture = exportOpenXRTextureGBM(swapchain, sideBySideTexture, width, height);
#endif
    if (!externalTexture) {
        glDeleteTextures(1, &sideBySideTexture);
        return std::nullopt;
    }
#endif
    if (!sideBySideTexture)
        return std::nullopt;

    m_sideBySideTextures.set(key, sideBySideTexture);
    return externalTexture;
}

void OpenXRGraphicsBindingOpenGLES::reconstructCubeFaces(uint64_t keyImage, const Vector<uint64_t>& cubeImages, uint32_t faceSize)
{
    // A cube swapchain always has the 6 cube-map faces.
    static constexpr uint32_t faceCount = 6;

    auto sideBySideTexture = m_sideBySideTextures.get(static_cast<PlatformGLObject>(keyImage));
    if (!sideBySideTexture)
        return;

    if (!m_reconstructionFBOs[0])
        glGenFramebuffers(m_reconstructionFBOs.size(), m_reconstructionFBOs.data());

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_reconstructionFBOs[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sideBySideTexture, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_reconstructionFBOs[1]);

    // Side-by-side region (cube * faceCount + face) maps to GL_TEXTURE_CUBE_MAP_POSITIVE_X + face.
    for (uint32_t cube = 0; cube < cubeImages.size(); ++cube) {
        auto cubeTexture = static_cast<PlatformGLObject>(cubeImages[cube]);
        if (!cubeTexture)
            continue;
        for (uint32_t face = 0; face < faceCount; ++face) {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubeTexture, 0);
            int srcX = static_cast<int>(cube * faceCount + face) * static_cast<int>(faceSize);
            glBlitFramebuffer(srcX, 0, srcX + static_cast<int>(faceSize), static_cast<int>(faceSize), 0, 0, static_cast<int>(faceSize), static_cast<int>(faceSize), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
#endif // XR_KHR_composition_layer_cube

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_OPENGL_ES)
