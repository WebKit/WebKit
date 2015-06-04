/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WaylandSurface.h"

#if PLATFORM(WAYLAND)

#include "GLContextEGL.h"
#include "IntSize.h"
#include "PlatformDisplayWayland.h"
#include <EGL/egl.h>

namespace WebCore {

void frameCallback(void*, struct wl_callback* callback, uint32_t)
{
    if (callback)
        wl_callback_destroy(callback);
}

static const struct wl_callback_listener frameListener = {
    frameCallback
};

WaylandSurface::WaylandSurface(struct wl_surface* wlSurface, EGLNativeWindowType nativeWindow)
    : m_wlSurface(wlSurface)
    , m_nativeWindow(nativeWindow)
{
}

WaylandSurface::~WaylandSurface()
{
    // The surface couldn't have been created in the first place if WaylandDisplay wasn't properly initialized.
    const PlatformDisplayWayland& waylandDisplay = downcast<PlatformDisplayWayland>(PlatformDisplay::sharedDisplay());
    ASSERT(waylandDisplay.native());
    eglMakeCurrent(waylandDisplay.native(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    wl_egl_window_destroy(m_nativeWindow);
    wl_surface_destroy(m_wlSurface);
}

void WaylandSurface::resize(const IntSize& size)
{
    wl_egl_window_resize(m_nativeWindow, size.width(), size.height(), 0, 0);
}

std::unique_ptr<GLContextEGL> WaylandSurface::createGLContext()
{
    return GLContextEGL::createWindowContext(m_nativeWindow, GLContext::sharingContext());
}

void WaylandSurface::requestFrame()
{
    struct wl_callback* frameCallback = wl_surface_frame(m_wlSurface);
    wl_callback_add_listener(frameCallback, &frameListener, this);
}

} // namespace WebCore

#endif // PLATFORM(WAYLAND)
