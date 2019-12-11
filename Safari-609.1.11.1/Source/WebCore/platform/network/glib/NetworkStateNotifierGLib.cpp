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
#include "NetworkStateNotifier.h"

#include <gio/gio.h>

namespace WebCore {

void NetworkStateNotifier::updateStateWithoutNotifying()
{
    m_isOnLine = g_network_monitor_get_network_available(g_network_monitor_get_default());
}

void NetworkStateNotifier::networkChangedCallback(NetworkStateNotifier* networkStateNotifier)
{
    networkStateNotifier->singleton().updateStateSoon();
}

void NetworkStateNotifier::startObserving()
{
    g_signal_connect_swapped(g_network_monitor_get_default(), "network-changed", G_CALLBACK(networkChangedCallback), this);
}

}
