/*
 * Copyright (C) 2017 Igalia, S.L.
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

#if USE(EGL) && USE(WPE_RENDERER)
#include "PlatformDisplayLibWPE.h"

#if USE(LIBEPOXY)
// FIXME: For now default to the GBM EGL platform, but this should really be
// somehow deducible from the build configuration.
#define __GBM__ 1
#include "EpoxyEGL.h"
#else
#if PLATFORM(WAYLAND)
// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and eglplatform.h, included by egl.h, checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#endif
#include <EGL/egl.h>
#endif

#include <wpe/wpe-egl.h>

namespace WebCore {

GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, struct wpe_renderer_backend_egl_offscreen_target* target)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(WindowSurface)
    , m_wpeTarget(target)
{
}

EGLSurface GLContextEGL::createWindowSurfaceWPE(EGLDisplay display, EGLConfig config, GLNativeWindowType window)
{
    return eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(window), nullptr);
}

std::unique_ptr<GLContextEGL> GLContextEGL::createWPEContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, WindowSurface)) {
        WTFLogAlways("Cannot obtain EGL WPE context configuration: %s\n", lastErrorString());
        return nullptr;
    }

    auto* target = wpe_renderer_backend_egl_offscreen_target_create();
    if (!target)
        return nullptr;

    wpe_renderer_backend_egl_offscreen_target_initialize(target, downcast<PlatformDisplayLibWPE>(platformDisplay).backend());
    EGLNativeWindowType window = wpe_renderer_backend_egl_offscreen_target_get_native_window(target);
    if (!window) {
        wpe_renderer_backend_egl_offscreen_target_destroy(target);
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(platformDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        WTFLogAlways("Cannot create EGL WPE context: %s\n", lastErrorString());
        wpe_renderer_backend_egl_offscreen_target_destroy(target);
        return nullptr;
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(window), nullptr);
    if (surface == EGL_NO_SURFACE) {
        WTFLogAlways("Cannot create EGL WPE window surface: %s\n", lastErrorString());
        eglDestroyContext(display, context);
        wpe_renderer_backend_egl_offscreen_target_destroy(target);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, target));
}

void GLContextEGL::destroyWPETarget()
{
    if (m_wpeTarget)
        wpe_renderer_backend_egl_offscreen_target_destroy(m_wpeTarget);
}

} // namespace WebCore

#endif // USE(EGL) && USE(WPE_RENDERER)
