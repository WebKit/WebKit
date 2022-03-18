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

#include "config.h"
#include "GStreamerDtlsTransportBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GStreamerIceTransportBackend.h"
#include "GStreamerWebRTCUtils.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

GStreamerDtlsTransportBackend::GStreamerDtlsTransportBackend(const GRefPtr<GstWebRTCDTLSTransport>& transport)
    : m_backend(transport)
{
    ASSERT(m_backend);
    ASSERT(isMainThread());
}

GStreamerDtlsTransportBackend::~GStreamerDtlsTransportBackend()
{
    unregisterClient();
}

UniqueRef<RTCIceTransportBackend> GStreamerDtlsTransportBackend::iceTransportBackend()
{
    return makeUniqueRef<GStreamerIceTransportBackend>(m_backend);
}

void GStreamerDtlsTransportBackend::registerClient(Client& client)
{
    ASSERT(!m_client);
    m_client = client;
    g_signal_connect_swapped(m_backend.get(), "notify::state", G_CALLBACK(+[](GStreamerDtlsTransportBackend* backend) {
        backend->stateChanged();
    }), this);
}

void GStreamerDtlsTransportBackend::unregisterClient()
{
    g_signal_handlers_disconnect_by_data(m_backend.get(), this);
    m_client.clear();
}

void GStreamerDtlsTransportBackend::stateChanged() const
{
    callOnMainThreadAndWait([weakThis = WeakPtr { this }] {
        if (!weakThis)
            return;
        if (!weakThis->m_client)
            return;

        GUniqueOutPtr<char> remoteCertificate;
        GUniqueOutPtr<char> certificate;
        GstWebRTCDTLSTransportState state;
        g_object_get(weakThis->m_backend.get(), "state", &state, "remote-certificate", &remoteCertificate.outPtr(), "certificate", &certificate.outPtr(), nullptr);

        Vector<Ref<JSC::ArrayBuffer>> certificates;
        certificates.reserveInitialCapacity(2);
        if (remoteCertificate) {
            auto remoteCertificateString = makeString(remoteCertificate.get());
            auto jsRemoteCertificate = JSC::ArrayBuffer::create(remoteCertificateString.characters8(), remoteCertificateString.sizeInBytes());
            certificates.uncheckedAppend(WTFMove(jsRemoteCertificate));

            auto certificateString = makeString(certificate.get());
            auto jsCertificate = JSC::ArrayBuffer::create(certificateString.characters8(), certificateString.sizeInBytes());
            certificates.uncheckedAppend(WTFMove(jsCertificate));
        }
        weakThis->m_client->onStateChanged(toRTCDtlsTransportState(state), WTFMove(certificates));
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
