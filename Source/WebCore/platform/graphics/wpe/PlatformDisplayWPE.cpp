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
#include "PlatformDisplayWPE.h"

#if PLATFORM(WPE)

#include "GLContextEGL.h"
#include "IntSize.h"
#include <EGL/egl.h>
#include <wpe/renderer-backend-egl.h>

namespace WebCore {

PlatformDisplayWPE::PlatformDisplayWPE() = default;

PlatformDisplayWPE::~PlatformDisplayWPE()
{
    wpe_renderer_backend_egl_destroy(m_backend);
}

void PlatformDisplayWPE::initialize(int hostFd)
{
    m_backend = wpe_renderer_backend_egl_create(hostFd);

    m_eglDisplay = eglGetDisplay(wpe_renderer_backend_egl_get_native_display(m_backend));
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        WTFLogAlways("PlatformDisplayWPE: could not create the EGL display.");
        return;
    }

    PlatformDisplay::initializeEGLDisplay();
}

std::unique_ptr<PlatformDisplayWPE::EGLTarget> PlatformDisplayWPE::createEGLTarget(EGLTarget::Client& client, int hostFd)
{
    return std::make_unique<EGLTarget>(*this, client, hostFd);
}

std::unique_ptr<PlatformDisplayWPE::EGLOffscreenTarget> PlatformDisplayWPE::createEGLOffscreenTarget()
{
    return std::make_unique<EGLOffscreenTarget>(*this);
}

PlatformDisplayWPE::EGLTarget::EGLTarget(const PlatformDisplayWPE& display, PlatformDisplayWPE::EGLTarget::Client& client, int hostFd)
    : m_display(display)
    , m_client(client)
{
    m_backend = wpe_renderer_backend_egl_target_create(hostFd);

    static struct wpe_renderer_backend_egl_target_client s_client = {
        // frame_complete
        [](void* data)
        {
            auto& surface = *reinterpret_cast<EGLTarget*>(data);
            surface.m_client.frameComplete();
        },
    };
    wpe_renderer_backend_egl_target_set_client(m_backend, &s_client, this);
}

PlatformDisplayWPE::EGLTarget::~EGLTarget()
{
    wpe_renderer_backend_egl_target_destroy(m_backend);
}

void PlatformDisplayWPE::EGLTarget::initialize(const IntSize& size)
{
    wpe_renderer_backend_egl_target_initialize(m_backend, m_display.m_backend,
        std::max(0, size.width()), std::max(0, size.height()));
}

EGLNativeWindowType PlatformDisplayWPE::EGLTarget::nativeWindow() const
{
    return wpe_renderer_backend_egl_target_get_native_window(m_backend);
}

void PlatformDisplayWPE::EGLTarget::resize(const IntSize& size)
{
    wpe_renderer_backend_egl_target_resize(m_backend, std::max(0, size.width()), std::max(0, size.height()));
}

void PlatformDisplayWPE::EGLTarget::frameWillRender()
{
    wpe_renderer_backend_egl_target_frame_will_render(m_backend);
}

void PlatformDisplayWPE::EGLTarget::frameRendered()
{
    wpe_renderer_backend_egl_target_frame_rendered(m_backend);
}

PlatformDisplayWPE::EGLOffscreenTarget::EGLOffscreenTarget(const PlatformDisplayWPE& display)
{
    m_target = wpe_renderer_backend_egl_offscreen_target_create();
    wpe_renderer_backend_egl_offscreen_target_initialize(m_target, display.m_backend);
}

PlatformDisplayWPE::EGLOffscreenTarget::~EGLOffscreenTarget()
{
    wpe_renderer_backend_egl_offscreen_target_destroy(m_target);
}

EGLNativeWindowType PlatformDisplayWPE::EGLOffscreenTarget::nativeWindow() const
{
    return wpe_renderer_backend_egl_offscreen_target_get_native_window(m_target);
}

} // namespace WebCore

#endif // PLATFORM(WPE)
