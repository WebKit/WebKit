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

#include "SystemSettingsManagerProxy.h"
#include <WebCore/GLContext.h>
#include <WebCore/GLDisplay.h>
#include <WebCore/GtkVersioning.h>
#include <epoxy/egl.h>
#include <gtk/gtk.h>
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {
using namespace WebCore;

Display& Display::singleton()
{
    static LazyNeverDestroyed<Display> display;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        display.construct();
    });
    return display;
}

Display::Display()
{
    if (!gtk_init_check(nullptr, nullptr))
        return;

    // As soon as gtk is initialized we listen to GtkSettings.
    SystemSettingsManagerProxy::initialize();

    m_gdkDisplay = gdk_display_manager_get_default_display(gdk_display_manager_get());
    if (!m_gdkDisplay)
        return;

    g_signal_connect(m_gdkDisplay.get(), "closed", G_CALLBACK(+[](GdkDisplay*, gboolean, gpointer userData) {
        auto& display = *static_cast<Display*>(userData);
        if (display.m_glDisplay && display.m_glDisplayOwned)
            display.m_glDisplay->terminate();
        display.m_glDisplay = nullptr;
    }), this);
}

Display::~Display()
{
    if (m_gdkDisplay)
        g_signal_handlers_disconnect_by_data(m_gdkDisplay.get(), this);
}

GLDisplay* Display::glDisplay() const
{
    if (m_glInitialized)
        return m_glDisplay.get();

    m_glInitialized = true;
    if (!m_gdkDisplay)
        return nullptr;

#if PLATFORM(X11)
    if (initializeGLDisplayX11())
        return m_glDisplay.get();
#endif
#if PLATFORM(WAYLAND)
    if (initializeGLDisplayWayland())
        return m_glDisplay.get();
#endif

    return nullptr;
}

String Display::accessibilityBusAddress() const
{
    if (!m_gdkDisplay)
        return { };

#if USE(GTK4)
    if (const char* atspiBusAddress = static_cast<const char*>(g_object_get_data(G_OBJECT(m_gdkDisplay.get()), "-gtk-atspi-bus-address")))
        return String::fromUTF8(atspiBusAddress);
#endif

#if PLATFORM(X11)
    if (isX11())
        return accessibilityBusAddressX11();
#endif

    return { };
}

#if !PLATFORM(X11)
bool Display::isX11() const
{
    return false;
}
#endif

#if !PLATFORM(WAYLAND)
bool Display::isWayland() const
{
    return false;
}
#endif

} // namespace WebKit
