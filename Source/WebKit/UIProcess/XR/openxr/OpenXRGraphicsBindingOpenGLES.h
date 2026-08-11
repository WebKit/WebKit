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

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "OpenXRGraphicsBinding.h"

typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef unsigned EGLenum;
#if defined(XR_USE_PLATFORM_EGL)
typedef void (*(*PFNEGLGETPROCADDRESSPROC)(const char *))(void);
#endif

// The JNI types need to be defined before including openxr_platform.h
#if OS(ANDROID)
#include <jni.h>
#endif

#include <WebCore/GraphicsTypesGL.h>
#include <array>
#include <openxr/openxr_platform.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class GLContext;
class GLDisplay;
#if USE(GBM)
class GBMDevice;
#endif
}

namespace WebKit {

class OpenXRSwapchain;

class OpenXRGraphicsBindingOpenGLES final : public OpenXRGraphicsBinding {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRGraphicsBindingOpenGLES);
public:
    static std::unique_ptr<OpenXRGraphicsBindingOpenGLES> create();

    ~OpenXRGraphicsBindingOpenGLES();

    Vector<ASCIILiteral> requiredInstanceExtensions() const final;
    bool initializeDisplay(bool isForTesting) final;
    bool initializeForSession(XrInstance, XrSystemId) final;
    const void* sessionGraphicsBinding() const final;
    int64_t selectColorFormat(const Vector<int64_t>& supportedFormats, bool alpha) const final;
    Vector<uint64_t> enumerateSwapchainImages(XrSwapchain) const final;
    std::optional<PlatformXR::FrameData::ExternalTexture> exportTexture(uint64_t image, const OpenXRSwapchain&, TextureType, uint32_t width, uint32_t height) final;
    void commitFrame(uint64_t keyImage, const OpenXRSwapchain&, TextureType, const Vector<uint64_t>& images) final;
    void waitFrameFence(WTF::UnixFileDescriptor&&) final;
    void releaseSessionGraphics() final;

private:
    OpenXRGraphicsBindingOpenGLES();

#if OS(ANDROID)
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTextureAndroid(const OpenXRSwapchain&, PlatformGLObject, uint32_t width, uint32_t height);
#else
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTextureDMABuf(const OpenXRSwapchain&, PlatformGLObject);
#endif
#if USE(GBM)
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTextureGBM(const OpenXRSwapchain&, PlatformGLObject, uint32_t width, uint32_t height);
#endif
    std::optional<PlatformXR::FrameData::ExternalTexture> exportTexture2D(PlatformGLObject, const OpenXRSwapchain&, uint32_t width, uint32_t height);
    void blitTextureIfNeeded(const OpenXRSwapchain&);

#if defined(XR_KHR_composition_layer_cube)
    std::optional<PlatformXR::FrameData::ExternalTexture> exportCubeBuffer(uint64_t keyImage, const OpenXRSwapchain&, uint32_t width, uint32_t height);
    void reconstructCubeFaces(uint64_t keyImage, const Vector<uint64_t>& cubeImages, uint32_t faceSize);
#endif

    RefPtr<WebCore::GLDisplay> m_glDisplay;
    std::unique_ptr<WebCore::GLContext> m_glContext;
#if USE(GBM)
    RefPtr<WebCore::GBMDevice> m_gbmDevice;
#endif
#if OS(ANDROID)
    XrGraphicsBindingOpenGLESAndroidKHR m_graphicsBinding;
#else
    XrGraphicsBindingEGLMNDX m_graphicsBinding;
#endif
#if USE(GBM) || OS(ANDROID)
    // Exported texture per swapchain image (exported once, then reused). Shared across the session's
    // swapchains, which assumes images live until session end (layers are add-only); if per-layer
    // removal is added, evict here or a recycled GL name would alias a stale texture.
    HashMap<PlatformGLObject, PlatformGLObject> m_exportedTexturesMap;
    std::array<PlatformGLObject, 2> m_fbosForBlitting { 0, 0 };
#endif
#if defined(XR_KHR_composition_layer_cube)
    HashMap<PlatformGLObject, PlatformGLObject> m_sideBySideTextures;
    std::array<PlatformGLObject, 2> m_reconstructionFBOs { 0, 0 };
#endif
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_OPENGL_ES)
