/*
 * Copyright (C) 2018 Metrological Group B.V.
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerVideoCapturer.h"
#include "RealtimeVideoSource.h"

namespace WebCore {

class GStreamerVideoCaptureSource : public RealtimeVideoSource {
public:
    static CaptureSourceOrError create(String&& deviceID, String&& hashSalt, const MediaConstraints*);
    WEBCORE_EXPORT static VideoCaptureFactory& factory();

    // FIXME: Implement this.
    WEBCORE_EXPORT static DisplayCaptureFactory& displayFactory(); 

    const RealtimeMediaSourceCapabilities& capabilities() override;
    const RealtimeMediaSourceSettings& settings() override;
    GstElement* pipeline() { return m_capturer->pipeline(); }
    GStreamerCapturer* capturer() { return m_capturer.get(); }

protected:
    GStreamerVideoCaptureSource(String&& deviceID, String&& name, String&& hashSalt, const gchar * source_factory);
    GStreamerVideoCaptureSource(GStreamerCaptureDevice, String&& hashSalt);
    virtual ~GStreamerVideoCaptureSource();
    void startProducingData() override;
    void stopProducingData() override;
    bool canResizeVideoFrames() const final { return true; }
    void generatePresets() final;


    mutable std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

private:
    static GstFlowReturn newSampleCallback(GstElement*, GStreamerVideoCaptureSource*);

    bool isCaptureSource() const final { return true; }
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;

    std::unique_ptr<GStreamerVideoCapturer> m_capturer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
