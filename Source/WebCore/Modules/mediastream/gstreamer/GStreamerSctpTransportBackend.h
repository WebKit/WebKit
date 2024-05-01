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
#include "RTCSctpTransportBackend.h"
#include <wtf/WeakPtr.h>

typedef struct _GstWebRTCSCTPTransport GstWebRTCSCTPTransport;

namespace WebCore {

class GStreamerSctpTransportBackend final : public RTCSctpTransportBackend, public CanMakeWeakPtr<GStreamerSctpTransportBackend> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GStreamerSctpTransportBackend(GRefPtr<GstWebRTCSCTPTransport>&&);
    ~GStreamerSctpTransportBackend();

protected:
    void stateChanged();

private :
    // RTCSctpTransportBackend
    const void* backend() const final { return m_backend.get(); }
    UniqueRef<RTCDtlsTransportBackend> dtlsTransportBackend() final;
    void registerClient(RTCSctpTransportBackendClient&) final;
    void unregisterClient() final;

    GRefPtr<GstWebRTCSCTPTransport> m_backend;
    WeakPtr<RTCSctpTransportBackendClient> m_client;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
