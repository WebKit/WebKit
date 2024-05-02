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

#include "MockRealtimeVideoSourceGStreamer.h"

namespace WebCore {

class MockDisplayCaptureSourceGStreamer : public RealtimeVideoCaptureSource, RealtimeMediaSource::VideoFrameObserver {
public:
    static CaptureSourceOrError create(const CaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*, PageIdentifier);

    void requestToEnd(RealtimeMediaSourceObserver&) final;
    bool isProducingData() const final { return m_source->isProducingData(); }
    void setMuted(bool isMuted) final;
    const IntSize size() const final { return m_source->size(); }

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final { return "MockDisplayCaptureSourceGStreamer"_s; }
#endif

protected:
    // RealtimeMediaSource::VideoFrameObserver
    void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata) final;

    void generatePresets() override { };
    const Vector<VideoPreset>& presets() final { return m_presets; }

private:
    MockDisplayCaptureSourceGStreamer(const CaptureDevice&, Ref<MockRealtimeVideoSourceGStreamer>&&, MediaDeviceHashSalts&&, PageIdentifier);
    ~MockDisplayCaptureSourceGStreamer();

    void startProducingData() final { m_source->start(); }
    void stopProducingData() final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final { m_currentSettings = { }; }
    bool isCaptureSource() const final { return true; }
    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;
    CaptureDevice::DeviceType deviceType() const final { return m_deviceType; }

    Vector<VideoPreset> m_presets;
    Ref<MockRealtimeVideoSourceGStreamer> m_source;
    CaptureDevice::DeviceType m_deviceType;
    std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    std::optional<RealtimeMediaSourceSettings> m_currentSettings;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
