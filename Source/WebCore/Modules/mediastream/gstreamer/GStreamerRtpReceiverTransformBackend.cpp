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
#include "GStreamerRtpReceiverTransformBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "NotImplemented.h"

namespace WebCore {

static inline GStreamerRtpReceiverTransformBackend::MediaType mediaTypeFromReceiver(GstWebRTCRTPReceiver*)
{
    notImplemented();
    return RTCRtpTransformBackend::MediaType::Video;
}

GStreamerRtpReceiverTransformBackend::GStreamerRtpReceiverTransformBackend(const GRefPtr<GstWebRTCRTPReceiver>& rtcReceiver)
    : GStreamerRtpTransformBackend(mediaTypeFromReceiver(rtcReceiver.get()), Side::Receiver)
    , m_rtcReceiver(rtcReceiver)
{
}

GStreamerRtpReceiverTransformBackend::~GStreamerRtpReceiverTransformBackend()
{
}

void GStreamerRtpReceiverTransformBackend::setTransformableFrameCallback(Callback&& callback)
{
    setInputCallback(WTFMove(callback));
    notImplemented();
}

bool GStreamerRtpReceiverTransformBackend::requestKeyFrame(const String&)
{
    ASSERT(mediaType() == MediaType::Video);
    notImplemented();
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
