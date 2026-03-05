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
#include <epoxy/egl.h>

namespace WebCore {

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
        if (auto glDisplay = GLDisplay::create(getPlatformDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY, nullptr)))
            return std::unique_ptr<PlatformDisplayAndroid>(new PlatformDisplayAndroid(glDisplay.releaseNonNull()));
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
