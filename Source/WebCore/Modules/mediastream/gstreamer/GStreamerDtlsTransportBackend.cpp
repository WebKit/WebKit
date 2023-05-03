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

class GStreamerDtlsTransportBackendObserver final : public ThreadSafeRefCounted<GStreamerDtlsTransportBackendObserver> {
public:
    static Ref<GStreamerDtlsTransportBackendObserver> create(RTCDtlsTransportBackend::Client& client, GRefPtr<GstWebRTCDTLSTransport>&& backend) { return adoptRef(*new GStreamerDtlsTransportBackendObserver(client, WTFMove(backend))); }

    void start();
    void stop();

private:
    GStreamerDtlsTransportBackendObserver(RTCDtlsTransportBackend::Client&, GRefPtr<GstWebRTCDTLSTransport>&&);

    void stateChanged();

    GRefPtr<GstWebRTCDTLSTransport> m_backend;
    WeakPtr<RTCDtlsTransportBackend::Client> m_client;
};

GStreamerDtlsTransportBackendObserver::GStreamerDtlsTransportBackendObserver(RTCDtlsTransportBackend::Client& client, GRefPtr<GstWebRTCDTLSTransport>&& backend)
    : m_backend(WTFMove(backend))
    , m_client(client)
{
    ASSERT(m_backend);
}

void GStreamerDtlsTransportBackendObserver::stateChanged()
{
    if (!m_client)
        return;

    callOnMainThread([this, protectedThis = Ref { *this }]() mutable {
        if (!m_client || !m_backend)
            return;

        GstWebRTCDTLSTransportState state;
        g_object_get(m_backend.get(), "state", &state, nullptr);

        Vector<Ref<JSC::ArrayBuffer>> certificates;

        // Access to DTLS certificates is not memory-safe in GStreamer versions older than 1.22.3.
        // See also: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/d9c853f165288071b63af9a56b6d76e358fbdcc2
        if (webkitGstCheckVersion(1, 22, 3)) {
            GUniqueOutPtr<char> remoteCertificate;
            GUniqueOutPtr<char> certificate;
            g_object_get(m_backend.get(), "remote-certificate", &remoteCertificate.outPtr(), "certificate", &certificate.outPtr(), nullptr);

            if (remoteCertificate) {
                auto remoteCertificateString = makeString(remoteCertificate.get());
                auto jsRemoteCertificate = JSC::ArrayBuffer::create(remoteCertificateString.characters8(), remoteCertificateString.sizeInBytes());
                certificates.append(WTFMove(jsRemoteCertificate));
            }

            if (certificate) {
                auto certificateString = makeString(certificate.get());
                auto jsCertificate = JSC::ArrayBuffer::create(certificateString.characters8(), certificateString.sizeInBytes());
                certificates.append(WTFMove(jsCertificate));
            }
        }
        m_client->onStateChanged(toRTCDtlsTransportState(state), WTFMove(certificates));
    });
}

void GStreamerDtlsTransportBackendObserver::start()
{
    g_signal_connect_swapped(m_backend.get(), "notify::state", G_CALLBACK(+[](GStreamerDtlsTransportBackendObserver* observer) {
        observer->stateChanged();
    }), this);
}

void GStreamerDtlsTransportBackendObserver::stop()
{
    m_client = nullptr;
    g_signal_handlers_disconnect_by_data(m_backend.get(), this);
}

GStreamerDtlsTransportBackend::GStreamerDtlsTransportBackend(GRefPtr<GstWebRTCDTLSTransport>&& transport)
    : m_backend(WTFMove(transport))
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
    return makeUniqueRef<GStreamerIceTransportBackend>(GRefPtr<GstWebRTCDTLSTransport>(m_backend));
}

void GStreamerDtlsTransportBackend::registerClient(Client& client)
{
    m_observer = GStreamerDtlsTransportBackendObserver::create(client, GRefPtr<GstWebRTCDTLSTransport>(m_backend));
    m_observer->start();
}

void GStreamerDtlsTransportBackend::unregisterClient()
{
    if (m_observer)
        m_observer->stop();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
