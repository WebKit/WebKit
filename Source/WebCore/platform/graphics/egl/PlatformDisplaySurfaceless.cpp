/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "PlatformDisplaySurfaceless.h"

#include "GLContext.h"
#include <epoxy/egl.h>

namespace WebCore {

std::unique_ptr<PlatformDisplaySurfaceless> PlatformDisplaySurfaceless::create()
{
    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    if (!GLContext::isExtensionSupported(extensions, "EGL_MESA_platform_surfaceless"))
        return nullptr;

    std::unique_ptr<GLDisplay> glDisplay;
    if (GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
        glDisplay = GLDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr));
    else if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
        glDisplay = GLDisplay::create(eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr));

    if (!glDisplay) {
        WTFLogAlways("Could not create surfaceless EGL display: %s. Aborting...", GLContext::lastErrorString());
        CRASH();
    }

    return std::unique_ptr<PlatformDisplaySurfaceless>(new PlatformDisplaySurfaceless(WTFMove(glDisplay)));
}

PlatformDisplaySurfaceless::PlatformDisplaySurfaceless(std::unique_ptr<GLDisplay>&& glDisplay)
    : PlatformDisplay(WTFMove(glDisplay))
{
#if ENABLE(WEBGL)
    m_anglePlatform = EGL_PLATFORM_SURFACELESS_MESA;
    m_angleNativeDisplay = EGL_DEFAULT_DISPLAY;
#endif
}

PlatformDisplaySurfaceless::~PlatformDisplaySurfaceless()
{
}

} // namespace WebCore
