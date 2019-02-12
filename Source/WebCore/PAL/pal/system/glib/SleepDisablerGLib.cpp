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
    // We ignore Type because we always want to inhibit both screen lock and
    // suspend, but only when idle. There is no reason for WebKit to ever block
    // a user from manually suspending the computer, so inhibiting idle
    // suffices. There's also probably no good reason for code taking a sleep
    // disabler to differentiate between lock and suspend on our platform. If we
    // ever need this distinction, which seems unlikely, then we'll need to
    // audit all use of SleepDisabler.

    // First, try to use the ScreenSaver API.
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
        nullptr, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto* self = static_cast<SleepDisablerGLib*>(userData);
            if (proxy) {
                // Under Flatpak, we'll get a useless proxy with no name owner.
                GUniquePtr<char> nameOwner(g_dbus_proxy_get_name_owner(proxy.get()));
                if (nameOwner) {
                    self->m_screenSaverProxy = WTFMove(proxy);
                    self->acquireInhibitor();
                    return;
                }
            }

            // It failed. Try to use the Flatpak portal instead.
            g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
                nullptr, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Inhibit", self->m_cancellable.get(),
                [](GObject*, GAsyncResult* result, gpointer userData) {
                    GUniqueOutPtr<GError> error;
                    GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
                    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        return;

                    auto* self = static_cast<SleepDisablerGLib*>(userData);
                    if (proxy) {
                        GUniquePtr<char> nameOwner(g_dbus_proxy_get_name_owner(proxy.get()));
                        if (nameOwner) {
                            self->m_inhibitPortalProxy = WTFMove(proxy);
                            self->acquireInhibitor();
                            return;
                        }
                    }

                    // Give up. Don't warn the user: this is expected.
                    self->m_cancellable = nullptr;
            }, self);
        }, this);
}

SleepDisablerGLib::~SleepDisablerGLib()
{
    if (m_cancellable)
        g_cancellable_cancel(m_cancellable.get());
    else if (m_screenSaverCookie || m_inhibitPortalRequestObjectPath)
        releaseInhibitor();
}

void SleepDisablerGLib::acquireInhibitor()
{
    if (m_screenSaverProxy) {
        ASSERT(!m_inhibitPortalProxy);
        acquireInhibitorViaScreenSaverProxy();
    } else {
        ASSERT(m_inhibitPortalProxy);
        acquireInhibitorViaInhibitPortalProxy();
    }
}

void SleepDisablerGLib::acquireInhibitorViaScreenSaverProxy()
{
    g_dbus_proxy_call(m_screenSaverProxy.get(), "Inhibit", g_variant_new("(ss)", g_get_prgname(), m_reason.data()),
        G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(), [](GObject* proxy, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto* self = static_cast<SleepDisablerGLib*>(userData);
            if (error)
                g_warning("Calling %s.Inhibit failed: %s", g_dbus_proxy_get_interface_name(G_DBUS_PROXY(proxy)), error->message);
            else {
                ASSERT(returnValue);
                g_variant_get(returnValue.get(), "(u)", &self->m_screenSaverCookie);
            }
            self->m_cancellable = nullptr;
        }, this);
}

void SleepDisablerGLib::acquireInhibitorViaInhibitPortalProxy()
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "reason", g_variant_new_string(m_reason.data()));
    g_dbus_proxy_call(m_inhibitPortalProxy.get(), "Inhibit", g_variant_new("(su@a{sv})", "" /* no window */, 8 /* idle */, g_variant_builder_end(&builder)),
        G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(), [](GObject* proxy, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto* self = static_cast<SleepDisablerGLib*>(userData);
            if (error)
                g_warning("Calling %s.Inhibit failed: %s", g_dbus_proxy_get_interface_name(G_DBUS_PROXY(proxy)), error->message);
            else {
                ASSERT(returnValue);
                g_variant_get(returnValue.get(), "(o)", &self->m_inhibitPortalRequestObjectPath.outPtr());
            }
            self->m_cancellable = nullptr;
        }, this);
}

void SleepDisablerGLib::releaseInhibitor()
{
    if (m_screenSaverProxy) {
        ASSERT(!m_inhibitPortalProxy);
        releaseInhibitorViaScreenSaverProxy();
    } else {
        ASSERT(m_inhibitPortalProxy);
        releaseInhibitorViaInhibitPortalProxy();
    }
}

void SleepDisablerGLib::releaseInhibitorViaScreenSaverProxy()
{
    ASSERT(m_screenSaverCookie);

    g_dbus_proxy_call(m_screenSaverProxy.get(), "UnInhibit", g_variant_new("(u)", m_screenSaverCookie),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, [](GObject* proxy, GAsyncResult* result, gpointer) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
            if (error)
                g_warning("Calling %s.UnInhibit failed: %s", g_dbus_proxy_get_interface_name(G_DBUS_PROXY(proxy)), error->message);
    }, nullptr);
}

void SleepDisablerGLib::releaseInhibitorViaInhibitPortalProxy()
{
    ASSERT(m_inhibitPortalRequestObjectPath);

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
        nullptr, "org.freedesktop.portal.Desktop", m_inhibitPortalRequestObjectPath.get(), "org.freedesktop.portal.Request", nullptr,
        [](GObject*, GAsyncResult* result, gpointer) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> requestProxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (error) {
                g_warning("Failed to create org.freedesktop.portal.Request proxy: %s", error->message);
                return;
            }

            ASSERT(requestProxy);
            g_dbus_proxy_call(requestProxy.get(), "Close", g_variant_new("()"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
                [](GObject* proxy, GAsyncResult* result, gpointer) {
                    GUniqueOutPtr<GError> error;
                    GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
                    if (error)
                        g_warning("Calling %s.Close failed: %s", g_dbus_proxy_get_interface_name(G_DBUS_PROXY(proxy)), error->message);
                }, nullptr);
        }, nullptr);
}

} // namespace PAL
