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

#if ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_VULKAN)

#include "OpenXRGraphicsBinding.h"

// openxr_platform.h declares the EGL graphics binding when XR_USE_PLATFORM_EGL is set (which it is
// regardless of the selected graphics API), so the EGL types it references must exist beforehand.
typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef unsigned EGLenum;
#if defined(XR_USE_PLATFORM_EGL)
typedef void (*(*PFNEGLGETPROCADDRESSPROC)(const char *))(void);
#endif

#if OS(ANDROID)
#include <jni.h>
#endif

#if defined(XR_USE_GRAPHICS_API_VULKAN)
#include <volk.h>
#endif
#include <openxr/openxr_platform.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class OpenXRSwapchain;

class OpenXRGraphicsBindingVulkan final : public OpenXRGraphicsBinding {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRGraphicsBindingVulkan);
public:
    static std::unique_ptr<OpenXRGraphicsBindingVulkan> create();

    ~OpenXRGraphicsBindingVulkan();

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
    OpenXRGraphicsBindingVulkan();

    // The runtime's own swapchain images cannot be directly exported as DMABufs (we do not control their allocation), so this
    // mirrors the GBM fallback path, the WebProcess renders into our own buffer, then we copy it into the swapchain image.
    struct ExportedImage {
        VkImage image { VK_NULL_HANDLE };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        uint32_t width { 0 };
        uint32_t height { 0 };
        VkFormat format { VK_FORMAT_UNDEFINED };
    };

    std::optional<PlatformXR::FrameData::ExternalTexture> exportTexture2D(uint64_t swapchainImage, const OpenXRSwapchain&, uint32_t width, uint32_t height);
    void destroyExportedImage(ExportedImage&);

    // All dispatch below loader level goes through these tables instead of volk's globals, so this binding does not clobber the
    // dispatch of any other volk user in the process. volkLoadDevice() is the one that really bites: it replaces the globals
    // with pointers that bypass the loader trampoline and are only valid for the VkDevice they came from, so a second user
    // would be misdirected to our device rather than merely sharing our dispatch. The loader-level entry points
    // (vkGetInstanceProcAddr, vkEnumerateInstance*) are still globals, but those are instance-independent and set by the
    // idempotent volkInitialize(), so sharing them is what volk intends.
    // One global does remain unavoidably shared: volkLoadInstanceTable() also assigns the global vkGetDeviceProcAddr, because
    // that is where volkLoadDeviceTable() reads it from. So the two must be called in that order, and a concurrent volk user
    // could leave us resolving device functions through their instance's copy. volk has no API that avoids this.
    // volkLoadInstanceTable() requires volk 341, which OptionsGTK/OptionsWPE enforce for WEBXR_GRAPHICS_API=VULKAN.
    VolkInstanceTable m_instanceTable { };
    VolkDeviceTable m_deviceTable { };

    XrGraphicsBindingVulkanKHR m_graphicsBinding { };
    VkInstance m_vkInstance { VK_NULL_HANDLE };
    VkPhysicalDevice m_vkPhysicalDevice { VK_NULL_HANDLE };
    VkDevice m_vkDevice { VK_NULL_HANDLE };
    uint32_t m_queueFamilyIndex { 0 };
    VkQueue m_vkQueue { VK_NULL_HANDLE };
    VkCommandPool m_commandPool { VK_NULL_HANDLE };

    // Binary semaphore used to GPU-wait on the web process' render-completion fence: waitFrameFence()
    // imports the sync-file fd into it as a temporary payload, and commitFrame()'s blit submission
    // waits on it so the copy observes the finished writes without stalling the CPU. Mirrors the
    // OpenGLES GLFence::importFD()/serverWait() path. m_acquireSemaphorePending tracks whether a
    // payload was imported for the current commit.
    VkSemaphore m_acquireSemaphore { VK_NULL_HANDLE };
    bool m_acquireSemaphorePending { false };

    HashMap<uint64_t, ExportedImage> m_exportedImages;
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_VULKAN)
