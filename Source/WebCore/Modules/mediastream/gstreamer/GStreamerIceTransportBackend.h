/*
 *  Copyright (C) 2021-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
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

#pragma once

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"
#include "RTCIceTransportBackend.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GStreamerIceTransportBackendObserver;

class GStreamerIceTransportBackend final : public RTCIceTransportBackend {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerIceTransportBackend);

public:
    explicit GStreamerIceTransportBackend(GRefPtr<GstWebRTCDTLSTransport>&&);
    ~GStreamerIceTransportBackend();

private:
    // RTCIceTransportBackend
    const void* backend() const final { return m_dtlsTransport.get(); }
    void registerClient(RTCIceTransportBackendClient&) final;
    void unregisterClient() final;

    GRefPtr<GstWebRTCDTLSTransport> m_dtlsTransport;
    const RefPtr<GStreamerIceTransportBackendObserver> m_observer;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
