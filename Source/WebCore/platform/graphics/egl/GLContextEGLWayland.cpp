/*
 * Copyright (C) 2016 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GLContextEGL.h"

#if USE(EGL) && PLATFORM(WAYLAND)

#include "PlatformDisplayWayland.h"
// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#include <EGL/egl.h>

namespace WebCore {

GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, WlUniquePtr<struct wl_surface>&& wlSurface, struct wl_egl_window* wlWindow)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(WindowSurface)
    , m_wlSurface(WTFMove(wlSurface))
    , m_wlWindow(wlWindow)
{
}

EGLSurface GLContextEGL::createWindowSurfaceWayland(EGLDisplay display, EGLConfig config, GLNativeWindowType window)
{
    return eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(window), nullptr);
}

std::unique_ptr<GLContextEGL> GLContextEGL::createWaylandContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, WindowSurface))
        return nullptr;

    EGLContext context = createContextForEGLVersion(platformDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    WlUniquePtr<struct wl_surface> wlSurface(downcast<PlatformDisplayWayland>(platformDisplay).createSurface());
    if (!wlSurface) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    EGLNativeWindowType window = wl_egl_window_create(wlSurface.get(), 1, 1);
    EGLSurface surface = eglCreateWindowSurface(display, config, window, 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        wl_egl_window_destroy(window);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, WTFMove(wlSurface), window));
}

void GLContextEGL::destroyWaylandWindow()
{
    if (m_wlWindow)
        wl_egl_window_destroy(m_wlWindow);
}

} // namespace WebCore

#endif //  USE(EGL) && PLATFORM(WAYLAND)
