/*
 *  Copyright (C) 2017 Collabora Ltd.
 *  Copyright (C) 2018 Igalia S.L.
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
#include "LowPowerModeNotifier.h"

#include <cstring>
#include <gio/gio.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

static const char kWarningLevel[] = "WarningLevel";

LowPowerModeNotifier::LowPowerModeNotifier(LowPowerModeChangeCallback&& callback)
    : m_cancellable(adoptGRef(g_cancellable_new()))
    , m_callback(WTFMove(callback))
{
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES),
        nullptr, "org.freedesktop.UPower", "/org/freedesktop/UPower/devices/DisplayDevice", "org.freedesktop.UPower.Device", m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto* self = static_cast<LowPowerModeNotifier*>(userData);
            if (proxy) {
                GUniquePtr<char> nameOwner(g_dbus_proxy_get_name_owner(proxy.get()));
                if (nameOwner) {
                    self->m_displayDeviceProxy = WTFMove(proxy);
                    self->updateWarningLevel();
                    g_signal_connect_swapped(self->m_displayDeviceProxy.get(), "g-properties-changed", G_CALLBACK(gPropertiesChangedCallback), self);
                    return;
                }
            }

            // Now, if there is no name owner, it would be good to try to
            // connect to a Flatpak battery status portal instead.
            // Unfortunately, no such portal currently exists.
            self->m_cancellable = nullptr;
    }, this);
}

void LowPowerModeNotifier::updateWarningLevel()
{
    GRefPtr<GVariant> variant = adoptGRef(g_dbus_proxy_get_cached_property(m_displayDeviceProxy.get(), kWarningLevel));
    if (!variant) {
        m_lowPowerModeEnabled = false;
        return;
    }

    // 0: Unknown
    // 1: None
    // 2: Discharging (only for universal power supplies)
    // 3: Low
    // 4: Critical
    // 5: Action
    m_lowPowerModeEnabled = g_variant_get_uint32(variant.get()) > 1;
}

void LowPowerModeNotifier::warningLevelChanged()
{
    updateWarningLevel();
    m_callback(m_lowPowerModeEnabled);
}

void LowPowerModeNotifier::gPropertiesChangedCallback(LowPowerModeNotifier* self, GVariant* changedProperties)
{
    GUniqueOutPtr<GVariantIter> iter;
    g_variant_get(changedProperties, "a{sv}", &iter.outPtr());

    const char* propertyName;
    while (g_variant_iter_next(iter.get(), "{&sv}", &propertyName, nullptr)) {
        if (!strcmp(propertyName, kWarningLevel)) {
            self->warningLevelChanged();
            break;
        }
    }
}

LowPowerModeNotifier::~LowPowerModeNotifier()
{
    g_cancellable_cancel(m_cancellable.get());
}

bool LowPowerModeNotifier::isLowPowerModeEnabled() const
{
    return m_lowPowerModeEnabled;
}

} // namespace WebCore
