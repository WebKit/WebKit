/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
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
#include "GStreamerIceBackend.h"

#if USE(GSTREAMER_WEBRTC)

#include "NetworkConnectionToWebProcess.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerIceBackend);

void GStreamerIceBackend::initialize(NetworkConnectionToWebProcess& connectionToWebProcess, WebKit::WebPageProxyIdentifier&&, CompletionHandler<void(RefPtr<GStreamerIceBackend>&&)>&& completionHandler)
{
    Ref backend = GStreamerIceBackend::create(connectionToWebProcess);
    completionHandler(WTFMove(backend));
}

GStreamerIceBackend::GStreamerIceBackend(NetworkConnectionToWebProcess& connection)
#if USE(LIBNICE)
    : GStreamerIceBackendNice()
#endif
    , m_connection(connection)
{
}

GStreamerIceBackend::~GStreamerIceBackend() = default;

IPC::Connection* GStreamerIceBackend::messageSenderConnection() const
{
    return m_connection ? &m_connection->connection() : nullptr;
}

uint64_t GStreamerIceBackend::messageSenderDestinationID() const
{
    return identifier().toUInt64();
}

std::optional<SharedPreferencesForWebProcess> GStreamerIceBackend::sharedPreferencesForWebProcess() const
{
    if (auto connectionToWebProcess = m_connection.get())
        return connectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

} // namespace WebKit

#endif // USE(GSTREAMER_WEBRTC)
