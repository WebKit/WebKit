/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "PlatformDisplayAndroid.h"

#if OS(ANDROID)

#include "GLContext.h"
#include "Logging.h"
#include <android/hardware_buffer.h>
#include <epoxy/egl.h>
#include <wtf/android/RefPtrAndroid.h>

namespace WebCore {

static bool supportsAndroidNativeBufferImport(const PlatformDisplayAndroid& display)
{
    const auto& extensions = display.eglExtensions();
    auto supportsEGLImage = display.eglCheckVersion(1, 5) || extensions.KHR_image_base;
    if (!(supportsEGLImage && extensions.ANDROID_get_native_client_buffer && extensions.ANDROID_image_native_buffer)) {
        RELEASE_LOG_INFO(Compositing, "Android EGL display does not support required capabilities for AHardwareBuffer import (EGLImage=%d, ANDROID_get_native_client_buffer=%d, ANDROID_image_native_buffer=%d).",
            supportsEGLImage, extensions.ANDROID_get_native_client_buffer, extensions.ANDROID_image_native_buffer);
        return false;
    }

    auto getNativeClientBufferANDROID = reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(eglGetProcAddress("eglGetNativeClientBufferANDROID"));
    if (!getNativeClientBufferANDROID) {
        RELEASE_LOG_INFO(Compositing, "Android EGL display: eglGetNativeClientBufferANDROID not found.");
        return false;
    }

    AHardwareBuffer_Desc description { };
    description.width = 1;
    description.height = 1;
    description.layers = 1;
    description.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    description.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    AHardwareBuffer* hardwareBufferPtr { nullptr };
    if (AHardwareBuffer_allocate(&description, &hardwareBufferPtr)) {
        RELEASE_LOG_INFO(Compositing, "Android EGL display: AHardwareBuffer_allocate failed.");
        return false;
    }
    auto hardwareBuffer = adoptRef(hardwareBufferPtr);

    auto clientBuffer = getNativeClientBufferANDROID(hardwareBuffer.get());
    if (!clientBuffer) {
        RELEASE_LOG_INFO(Compositing, "Android EGL display: eglGetNativeClientBufferANDROID returned null.");
        return false;
    }

    static const Vector<EGLAttrib> attributes { EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE };
    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attributes);
    if (image == EGL_NO_IMAGE) {
        RELEASE_LOG_INFO(Compositing, "Android EGL display: eglCreateImage for AHardwareBuffer failed with error %#04x.", eglGetError());
        return false;
    }

    display.destroyEGLImage(image);
    return true;
}

std::unique_ptr<PlatformDisplayAndroid> PlatformDisplayAndroid::create()
{
#if defined(EGL_PLATFORM_ANDROID_KHR)
    using GetPlatformDisplayType = PFNEGLGETPLATFORMDISPLAYEXTPROC;
    GetPlatformDisplayType getPlatformDisplay =
        [] {
            const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
            if (GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base")) {
                if (auto extension = reinterpret_cast<GetPlatformDisplayType>(eglGetProcAddress("eglGetPlatformDisplayEXT")))
                    return extension;
            }
            if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base")) {
                if (auto extension = reinterpret_cast<GetPlatformDisplayType>(eglGetProcAddress("eglGetPlatformDisplay")))
                    return extension;
            }
            return GetPlatformDisplayType(nullptr);
        }();

    if (getPlatformDisplay) {
        if (auto glDisplay = GLDisplay::create(getPlatformDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY, nullptr))) {
            auto display = std::unique_ptr<PlatformDisplayAndroid>(new PlatformDisplayAndroid(glDisplay.releaseNonNull()));
            if (supportsAndroidNativeBufferImport(*display))
                return display;
            RELEASE_LOG_INFO(Compositing, "Android platform EGL display cannot import AHardwareBuffer objects; continuing with the normal fallback path.");
        }
    }
#endif

    return nullptr;
}

PlatformDisplayAndroid::PlatformDisplayAndroid(Ref<GLDisplay>&& glDisplay)
    : PlatformDisplay(WTF::move(glDisplay))
{
#if ENABLE(WEBGL) && defined(EGL_PLATFORM_ANDROID_KHR)
    m_anglePlatform = EGL_PLATFORM_ANDROID_KHR;
    m_angleNativeDisplay = EGL_DEFAULT_DISPLAY;
#endif
}

PlatformDisplayAndroid::~PlatformDisplayAndroid() = default;

} // namespace WebCore

#endif // OS(ANDROID)
