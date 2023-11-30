/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "PlatformDisplayLibWPE.h"

#if USE(WPE_RENDERER)

#include "GLContext.h"

#if USE(LIBEPOXY)
// FIXME: For now default to the GBM EGL platform, but this should really be
// somehow deducible from the build configuration.
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#if PLATFORM(WAYLAND)
// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and eglplatform.h, included by egl.h, checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#endif
#include <EGL/egl.h>
#endif

#include <wpe/wpe-egl.h>

#ifndef EGL_EXT_platform_base
#define EGL_EXT_platform_base 1
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplayLibWPE> PlatformDisplayLibWPE::create()
{
    return std::unique_ptr<PlatformDisplayLibWPE>(new PlatformDisplayLibWPE());
}

PlatformDisplayLibWPE::PlatformDisplayLibWPE()
{
#if PLATFORM(GTK)
    PlatformDisplay::setSharedDisplayForCompositing(*this);
#endif
}

PlatformDisplayLibWPE::~PlatformDisplayLibWPE()
{
    if (m_backend)
        wpe_renderer_backend_egl_destroy(m_backend);
}

bool PlatformDisplayLibWPE::initialize(int hostFd)
{
    m_backend = wpe_renderer_backend_egl_create(hostFd);

    EGLNativeDisplayType eglNativeDisplay = wpe_renderer_backend_egl_get_native_display(m_backend);

#if WPE_CHECK_VERSION(1, 1, 0)
    uint32_t eglPlatform = wpe_renderer_backend_egl_get_platform(m_backend);
    if (eglPlatform) {
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

        if (getPlatformDisplay)
            m_eglDisplay = getPlatformDisplay(eglPlatform, eglNativeDisplay, nullptr);
    }
#endif

    if (m_eglDisplay == EGL_NO_DISPLAY)
        m_eglDisplay = eglGetDisplay(eglNativeDisplay);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        WTFLogAlways("PlatformDisplayLibWPE: could not create the EGL display: %s.", GLContext::lastErrorString());
        return false;
    }

    PlatformDisplay::initializeEGLDisplay();

#if ENABLE(WEBGL)
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        m_anglePlatform = eglPlatform;
        m_angleNativeDisplay = eglNativeDisplay;
    }
#endif
    return m_eglDisplay != EGL_NO_DISPLAY;
}

} // namespace WebCore

#endif // USE(WPE_RENDERER)
