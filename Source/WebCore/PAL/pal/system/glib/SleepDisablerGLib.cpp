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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SleepDisablerGLib.h"

#include <gio/gio.h>
#include <wtf/glib/GUniquePtr.h>

namespace PAL {

std::unique_ptr<SleepDisabler> SleepDisabler::create(const char* reason, Type type)
{
    return std::unique_ptr<SleepDisabler>(new SleepDisablerGLib(reason, type));
}

SleepDisablerGLib::SleepDisablerGLib(const char* reason, Type type)
    : SleepDisabler(reason, type)
    , m_cancellable(adoptGRef(g_cancellable_new()))
    , m_reason(reason)
{
    // We don't support suspend ("System") inhibitors, only idle inhibitors.
    // To get suspend inhibitors, we'd need to use the fancy GNOME
    // SessionManager API, which requires registering as a client application,
    // which is not practical from the web process. Secondly, because the only
    // current use of a suspend inhibitor in WebKit,
    // HTMLMediaElement::shouldDisableSleep, is suspicious. There's really no
    // valid reason for WebKit to ever block suspend, only idle.
    if (type != SleepDisabler::Type::Display)
        return;

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
        nullptr, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto* self = static_cast<SleepDisablerGLib*>(userData);
            if (proxy) {
                GUniquePtr<char> nameOwner(g_dbus_proxy_get_name_owner(proxy.get()));
                if (nameOwner) {
                    self->m_screenSaverProxy = WTFMove(proxy);
                    self->acquireInhibitor();
                    return;
                }
            }

            // Give up. Don't warn the user: this is expected.
            self->m_cancellable = nullptr;
        }, this);
}

SleepDisablerGLib::~SleepDisablerGLib()
{
    if (m_cancellable)
        g_cancellable_cancel(m_cancellable.get());
    else if (m_screenSaverCookie)
        releaseInhibitor();
}

void SleepDisablerGLib::acquireInhibitor()
{
    ASSERT(m_screenSaverProxy);
    g_dbus_proxy_call(m_screenSaverProxy.get(), "Inhibit", g_variant_new("(ss)", g_get_prgname(), m_reason.data()),
        G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(), [](GObject* proxy, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto* self = static_cast<SleepDisablerGLib*>(userData);
            if (error)
                g_warning("Calling %s.Inhibit failed: %s", g_dbus_proxy_get_interface_name(self->m_screenSaverProxy.get()), error->message);
            else {
                ASSERT(returnValue);
                g_variant_get(returnValue.get(), "(u)", &self->m_screenSaverCookie);
            }
            self->m_cancellable = nullptr;
        }, this);
}

void SleepDisablerGLib::releaseInhibitor()
{
    ASSERT(m_screenSaverCookie);
    ASSERT(m_screenSaverProxy);

    g_dbus_proxy_call(m_screenSaverProxy.get(), "UnInhibit", g_variant_new("(u)", m_screenSaverCookie),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, [](GObject* proxy, GAsyncResult* result, gpointer) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
            if (error)
                g_warning("Calling %s.UnInhibit failed: %s", g_dbus_proxy_get_interface_name(G_DBUS_PROXY(proxy)), error->message);
    }, nullptr);
}

} // namespace PAL
