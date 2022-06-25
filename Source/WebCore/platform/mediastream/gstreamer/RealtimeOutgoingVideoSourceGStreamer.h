/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
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

#if USE(GSTREAMER_WEBRTC)

#include "RealtimeOutgoingMediaSourceGStreamer.h"

namespace WebCore {

class RealtimeOutgoingVideoSourceGStreamer final : public RealtimeOutgoingMediaSourceGStreamer {
public:
    static Ref<RealtimeOutgoingVideoSourceGStreamer> create(Ref<MediaStreamTrackPrivate>&& source) { return adoptRef(*new RealtimeOutgoingVideoSourceGStreamer(WTFMove(source))); }

    void setApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = shouldApplyRotation; }

    bool setPayloadType(const GRefPtr<GstCaps>&) final;

protected:
    explicit RealtimeOutgoingVideoSourceGStreamer(Ref<MediaStreamTrackPrivate>&&);

    bool m_shouldApplyRotation { false };

private:
    GRefPtr<GstElement> m_videoConvert;
    GRefPtr<GstElement> m_videoFlip;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
