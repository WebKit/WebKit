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


LowPowerModeNotifier::LowPowerModeNotifier(LowPowerModeChangeCallback&& callback)
#if GLIB_CHECK_VERSION(2, 69, 1)
    : m_callback(WTFMove(callback))
    , m_powerProfileMonitor(adoptGRef(g_power_profile_monitor_dup_default()))
#endif
{
#if GLIB_CHECK_VERSION(2, 69, 1)
    g_signal_connect_swapped(m_powerProfileMonitor.get(), "notify::power-saver-enabled", G_CALLBACK(+[] (LowPowerModeNotifier* self, GParamSpec*, GPowerProfileMonitor*) {
        self->m_callback(self->isLowPowerModeEnabled());
    }), this);
#endif
}

LowPowerModeNotifier::~LowPowerModeNotifier()
{
#if GLIB_CHECK_VERSION(2, 69, 1)
    g_signal_handlers_disconnect_by_data(m_powerProfileMonitor.get(), this);
#endif
}

bool LowPowerModeNotifier::isLowPowerModeEnabled() const
{
#if GLIB_CHECK_VERSION(2, 69, 1)
    return g_power_profile_monitor_get_power_saver_enabled(m_powerProfileMonitor.get());
#else
    return false;
#endif
}

} // namespace WebCore
