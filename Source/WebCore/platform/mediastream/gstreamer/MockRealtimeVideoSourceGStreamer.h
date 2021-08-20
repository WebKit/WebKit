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

#include "MockRealtimeVideoSource.h"

namespace WebCore {

class MockRealtimeVideoSourceGStreamer final : public MockRealtimeVideoSource {
public:
    MockRealtimeVideoSourceGStreamer(String&& deviceID, String&& name, String&& hashSalt);
    ~MockRealtimeVideoSourceGStreamer() = default;

private:
    friend class MockRealtimeVideoSource;

    void updateSampleBuffer() final;
    bool canResizeVideoFrames() const final { return true; }
};

class MockDisplayCaptureSourceGStreamer final : public RealtimeMediaSource {
public:
    static CaptureSourceOrError create(const CaptureDevice&, const MediaConstraints*);

private:
    MockDisplayCaptureSourceGStreamer(Ref<MockRealtimeVideoSourceGStreamer>&& source, CaptureDevice::DeviceType type)
        : RealtimeMediaSource(Type::Video, source->name().isolatedCopy())
        , m_source(WTFMove(source))
        , m_type(type) { }

    friend class MockRealtimeVideoSourceGStreamer;

    void startProducingData() final { m_source->start(); }
    void stopProducingData() final { m_source->stop(); }
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final { m_currentSettings = { }; }
    bool isCaptureSource() const final { return true; }
    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;
    CaptureDevice::DeviceType deviceType() const { return m_type; }

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "MockDisplayCaptureSourceGStreamer"; }
#endif

    Ref<MockRealtimeVideoSourceGStreamer> m_source;
    CaptureDevice::DeviceType m_type;
    std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    std::optional<RealtimeMediaSourceSettings> m_currentSettings;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
