/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerVideoCapturer.h"
#include "MockRealtimeVideoSource.h"

namespace WebCore {

class MockRealtimeVideoSourceGStreamer final : public MockRealtimeVideoSource, GStreamerCapturer::Observer {
public:
    MockRealtimeVideoSourceGStreamer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&);
    ~MockRealtimeVideoSourceGStreamer();

    // GStreamerCapturer::Observer
    void captureEnded() final;

private:
    friend class MockRealtimeVideoSource;

    void startProducingData() final;
    void stopProducingData() final;
    void updateSampleBuffer() final;
    bool canResizeVideoFrames() const final { return true; }
    RefPtr<GStreamerVideoCapturer> m_capturer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
