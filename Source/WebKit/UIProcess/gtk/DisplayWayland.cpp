/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "Display.h"

#if PLATFORM(WAYLAND)

#include <WebCore/GLContext.h>
#include <WebCore/GLDisplay.h>
#include <gtk/gtk.h>

// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#include <epoxy/egl.h>

#if USE(GTK4)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>
#endif

namespace WebKit {
using namespace WebCore;

bool Display::isWayland() const
{
    return GDK_IS_WAYLAND_DISPLAY(m_gdkDisplay.get());
}

bool Display::initializeGLDisplayWayland() const
{
    if (!isWayland())
        return false;

#if USE(GTK4)
    m_glDisplay = GLDisplay::create(gdk_wayland_display_get_egl_display(m_gdkDisplay.get()));
#else
    auto* window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_realize(window);
    if (auto context = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(window), nullptr))) {
        gdk_gl_context_make_current(context.get());
        m_glDisplay = GLDisplay::create(eglGetCurrentDisplay());
    }
    gtk_widget_destroy(window);
#endif
    if (m_glDisplay)
        return true;

    auto* wlDisplay = gdk_wayland_display_get_wl_display(m_gdkDisplay.get());
    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
        m_glDisplay = GLDisplay::create(eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, wlDisplay, nullptr));
    if (!m_glDisplay && GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
        m_glDisplay = GLDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, wlDisplay, nullptr));
    if (!m_glDisplay)
        m_glDisplay = GLDisplay::create(eglGetDisplay(wlDisplay));

    if (m_glDisplay) {
        m_glDisplayOwned = true;
        return true;
    }

    return false;
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
