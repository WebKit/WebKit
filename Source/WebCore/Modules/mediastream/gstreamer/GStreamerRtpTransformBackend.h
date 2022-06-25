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

#include "RTCRtpTransformBackend.h"
#include <wtf/Lock.h>

namespace WebCore {

class GStreamerRtpTransformBackend : public RTCRtpTransformBackend {
protected:
    GStreamerRtpTransformBackend(MediaType, Side);
    void setInputCallback(Callback&&);

protected:
    MediaType mediaType() const final { return m_mediaType; }

private:
    // RTCRtpTransformBackend
    void processTransformedFrame(RTCRtpTransformableFrame&) final;
    void clearTransformableFrameCallback() final;
    Side side() const final { return m_side; }

    MediaType m_mediaType;
    Side m_side;

    Lock m_inputCallbackLock;
    Callback m_inputCallback;

    Lock m_outputCallbackLock;
};

inline GStreamerRtpTransformBackend::GStreamerRtpTransformBackend(MediaType mediaType, Side side)
    : m_mediaType(mediaType)
    , m_side(side)
{
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
