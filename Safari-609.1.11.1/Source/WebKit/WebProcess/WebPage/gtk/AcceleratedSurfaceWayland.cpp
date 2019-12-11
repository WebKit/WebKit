/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "AcceleratedSurfaceWayland.h"

#if PLATFORM(WAYLAND)

#include "WaylandCompositorDisplay.h"
#include "WebProcess.h"
#include <wayland-egl.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<AcceleratedSurfaceWayland> AcceleratedSurfaceWayland::create(WebPage& webPage, Client& client)
{
    return WebProcess::singleton().waylandCompositorDisplay() ? std::unique_ptr<AcceleratedSurfaceWayland>(new AcceleratedSurfaceWayland(webPage, client)) : nullptr;
}

AcceleratedSurfaceWayland::AcceleratedSurfaceWayland(WebPage& webPage, Client& client)
    : AcceleratedSurface(webPage, client)
{
}

void AcceleratedSurfaceWayland::initialize()
{
    m_surface = WebProcess::singleton().waylandCompositorDisplay()->createSurface();
    m_window = wl_egl_window_create(m_surface.get(), std::max(1, m_size.width()), std::max(1, m_size.height()));
    WebProcess::singleton().waylandCompositorDisplay()->bindSurfaceToPage(m_surface.get(), m_webPage);
}

void AcceleratedSurfaceWayland::finalize()
{
    wl_egl_window_destroy(m_window);
}

void AcceleratedSurfaceWayland::clientResize(const IntSize& size)
{
    wl_egl_window_resize(m_window, m_size.width(), m_size.height(), 0, 0);
}

void AcceleratedSurfaceWayland::didRenderFrame()
{
    // FIXME: frameComplete() should be called when the frame was actually rendered in the screen.
    m_client.frameComplete();
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
