/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedSurfaceWPE.h"

#include "WebPage.h"
#include <WebCore/PlatformDisplayLibWPE.h>
#include <wpe/wpe-egl.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<AcceleratedSurfaceWPE> AcceleratedSurfaceWPE::create(WebPage& webPage, Client& client)
{
    return std::unique_ptr<AcceleratedSurfaceWPE>(new AcceleratedSurfaceWPE(webPage, client));
}

AcceleratedSurfaceWPE::AcceleratedSurfaceWPE(WebPage& webPage, Client& client)
    : AcceleratedSurface(webPage, client)
{
}

AcceleratedSurfaceWPE::~AcceleratedSurfaceWPE()
{
    ASSERT(!m_backend);
}

void AcceleratedSurfaceWPE::initialize()
{
    m_backend = wpe_renderer_backend_egl_target_create(m_webPage.releaseHostFileDescriptor());
    static struct wpe_renderer_backend_egl_target_client s_client = {
        // frame_complete
        [](void* data)
        {
            auto& surface = *reinterpret_cast<AcceleratedSurfaceWPE*>(data);
            surface.m_client.frameComplete();
        },
        // padding
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    wpe_renderer_backend_egl_target_set_client(m_backend, &s_client, this);
    wpe_renderer_backend_egl_target_initialize(m_backend, downcast<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()).backend(),
        std::max(1, m_size.width()), std::max(1, m_size.height()));
}

void AcceleratedSurfaceWPE::finalize()
{
    wpe_renderer_backend_egl_target_destroy(m_backend);
    m_backend = nullptr;
}

uint64_t AcceleratedSurfaceWPE::window() const
{
    ASSERT(m_backend);
    // EGLNativeWindowType changes depending on the EGL implementation: reinterpret_cast works
    // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
    // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
    // a plain C cast expression in this one instance works in all cases.
    static_assert(sizeof(EGLNativeWindowType) <= sizeof(uint64_t), "EGLNativeWindowType must not be longer than 64 bits.");
    return (uint64_t) wpe_renderer_backend_egl_target_get_native_window(m_backend);
}

uint64_t AcceleratedSurfaceWPE::surfaceID() const
{
    return m_webPage.pageID();
}

void AcceleratedSurfaceWPE::clientResize(const IntSize& size)
{
    ASSERT(m_backend);
    wpe_renderer_backend_egl_target_resize(m_backend, std::max(1, m_size.width()), std::max(1, m_size.height()));
}

void AcceleratedSurfaceWPE::willRenderFrame()
{
    ASSERT(m_backend);
    wpe_renderer_backend_egl_target_frame_will_render(m_backend);
}

void AcceleratedSurfaceWPE::didRenderFrame()
{
    ASSERT(m_backend);
    wpe_renderer_backend_egl_target_frame_rendered(m_backend);
}

} // namespace WebKit

