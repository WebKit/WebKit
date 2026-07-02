/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro  <alex@igalia.com>
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
#include "CaptureDevice.h"
#include "GStreamerVideoCapturer.h"
#include "PipeWireCaptureDevice.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "RealtimeVideoCaptureSource.h"
#include "VideoFrameGStreamer.h"

namespace WebCore {

class GStreamerVideoCaptureSource final : public RealtimeVideoCaptureSource, GStreamerCapturerObserver {
public:
    static CaptureSourceOrError create(String&& deviceID, MediaDeviceHashSalts&&, const MediaConstraints*);
    static CaptureSourceOrError createPipewireSource(const PipeWireCaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*);

    WEBCORE_EXPORT static VideoCaptureFactory& factory();

    WEBCORE_EXPORT static DisplayCaptureFactory& displayFactory();

    using RealtimeVideoCaptureSource::ref;
    using RealtimeVideoCaptureSource::deref;
    void virtualRef() const final { RealtimeVideoCaptureSource::ref(); }
    void virtualDeref() const final { RealtimeVideoCaptureSource::deref(); }

    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;
    void configurationChanged() final;

    GstElement* pipeline() { return m_capturer->pipeline(); }
    GStreamerCapturer* capturer() { return m_capturer.get(); }

    // GStreamerCapturerObserver
    void sourceCapsChanged(const GstCaps*) final;
    void captureEnded() final;
    void captureDeviceUpdated(const GStreamerCaptureDevice&) final;

    std::pair<GstClockTime, GstClockTime> queryCaptureLatency() const final;

protected:
    GStreamerVideoCaptureSource(GStreamerCaptureDevice&&, MediaDeviceHashSalts&&);
    GStreamerVideoCaptureSource(const PipeWireCaptureDevice&, MediaDeviceHashSalts&&);
    virtual ~GStreamerVideoCaptureSource();
    void startProducingData() final;
    void stopProducingData() final;
    bool canResizeVideoFrames() const final { return true; }
    void generatePresets() final;
    void setSizeFrameRateAndZoom(const VideoPresetConstraints&) final;
    void applyFrameRateAndZoomWithPreset(double, double, std::optional<VideoPreset>&&) final;

    mutable std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;
    CaptureDevice::DeviceType deviceType() const final { return m_deviceType; }

private:
    bool isCaptureSource() const final { return true; }
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;

    RefPtr<GStreamerVideoCapturer> m_capturer;
    CaptureDevice::DeviceType m_deviceType;

    std::optional<VideoPreset> m_currentPreset;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
