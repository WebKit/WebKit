/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
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
#include "RTCRtpReceiverBackend.h"
#include "RealtimeMediaSource.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GStreamerRtpReceiverBackend final : public RTCRtpReceiverBackend {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerRtpReceiverBackend);
public:
    explicit GStreamerRtpReceiverBackend(GRefPtr<GstWebRTCRTPTransceiver>&&);

    GstWebRTCRTPReceiver* rtcReceiver() { return m_rtcReceiver.get(); }
    Ref<RealtimeMediaSource> createSource(const String& trackKind, const String& trackId);

private:
    RTCRtpParameters getParameters() final;
    Vector<RTCRtpContributingSource> getContributingSources() const final;
    Vector<RTCRtpSynchronizationSource> getSynchronizationSources() const final;
    Ref<RTCRtpTransformBackend> rtcRtpTransformBackend() final;
    std::unique_ptr<RTCDtlsTransportBackend> dtlsTransportBackend() final;

    GRefPtr<GstWebRTCRTPReceiver> m_rtcReceiver;
    GRefPtr<GstWebRTCRTPTransceiver> m_rtcTransceiver;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
