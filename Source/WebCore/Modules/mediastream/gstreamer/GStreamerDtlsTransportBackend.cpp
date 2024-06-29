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

GST_DEBUG_CATEGORY(webkit_webrtc_dtls_transport_debug);
#define GST_CAT_DEFAULT webkit_webrtc_dtls_transport_debug

class GStreamerDtlsTransportBackendObserver final : public ThreadSafeRefCounted<GStreamerDtlsTransportBackendObserver> {
public:
    static Ref<GStreamerDtlsTransportBackendObserver> create(RTCDtlsTransportBackendClient& client, GRefPtr<GstWebRTCDTLSTransport>&& backend) { return adoptRef(*new GStreamerDtlsTransportBackendObserver(client, WTFMove(backend))); }

    void start();
    void stop();

private:
    GStreamerDtlsTransportBackendObserver(RTCDtlsTransportBackendClient&, GRefPtr<GstWebRTCDTLSTransport>&&);

    void stateChanged();

    GRefPtr<GstWebRTCDTLSTransport> m_backend;
    WeakPtr<RTCDtlsTransportBackendClient> m_client;
};

GStreamerDtlsTransportBackendObserver::GStreamerDtlsTransportBackendObserver(RTCDtlsTransportBackendClient& client, GRefPtr<GstWebRTCDTLSTransport>&& backend)
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

#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_DTLS_TRANSPORT_STATE, state));
        GST_DEBUG_OBJECT(m_backend.get(), "DTLS transport state changed to %s", desc.get());
#endif

        Vector<Ref<JSC::ArrayBuffer>> certificates;

        // Access to DTLS certificates is not memory-safe in GStreamer versions older than 1.22.3.
        // See also: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/d9c853f165288071b63af9a56b6d76e358fbdcc2
        if (webkitGstCheckVersion(1, 22, 3)) {
            GUniqueOutPtr<char> remoteCertificate;
            GUniqueOutPtr<char> certificate;
            g_object_get(m_backend.get(), "remote-certificate", &remoteCertificate.outPtr(), "certificate", &certificate.outPtr(), nullptr);

            if (remoteCertificate)
                certificates.append(JSC::ArrayBuffer::create(span8(remoteCertificate.get())));

            if (certificate)
                certificates.append(JSC::ArrayBuffer::create(span8(certificate.get())));
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
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_dtls_transport_debug, "webkitwebrtcdtls", 0, "WebKit WebRTC DTLS Transport");
    });
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

void GStreamerDtlsTransportBackend::registerClient(RTCDtlsTransportBackendClient& client)
{
    m_observer = GStreamerDtlsTransportBackendObserver::create(client, GRefPtr<GstWebRTCDTLSTransport>(m_backend));
    m_observer->start();
}

void GStreamerDtlsTransportBackend::unregisterClient()
{
    if (m_observer)
        m_observer->stop();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
