/*
 *  Copyright (C) 2017 Collabora Ltd.
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

#if USE(UPOWER)
namespace WebCore {

LowPowerModeNotifier::LowPowerModeNotifier(LowPowerModeChangeCallback&& callback)
    : m_upClient(adoptGRef(up_client_new()))
    , m_device(adoptGRef(up_client_get_display_device(m_upClient.get())))
    , m_callback(WTFMove(callback))
{
    updateState();

    g_signal_connect_swapped(m_device.get(), "notify::warning-level", G_CALLBACK(warningLevelCallback), this);
}

void LowPowerModeNotifier::updateState()
{
    UpDeviceLevel warningLevel;
    g_object_get(G_OBJECT(m_device.get()), "warning-level", &warningLevel, nullptr);
    m_lowPowerModeEnabled = warningLevel > UP_DEVICE_LEVEL_NONE && warningLevel <= UP_DEVICE_LEVEL_ACTION;
}

void LowPowerModeNotifier::warningLevelCallback(LowPowerModeNotifier* notifier)
{
    notifier->updateState();
    notifier->m_callback(notifier->m_lowPowerModeEnabled);
}

LowPowerModeNotifier::~LowPowerModeNotifier()
{
    g_signal_handlers_disconnect_by_data(m_device.get(), this);
}

bool LowPowerModeNotifier::isLowPowerModeEnabled() const
{
    return m_lowPowerModeEnabled;
}

}
#endif
