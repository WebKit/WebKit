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
#include "RealtimeMediaSource.h"

namespace WebCore {

class GStreamerVideoCaptureSource : public RealtimeMediaSource {
public:
    static CaptureSourceOrError create(const String& deviceID, const MediaConstraints*);
    WEBCORE_EXPORT static VideoCaptureFactory& factory();

    const RealtimeMediaSourceCapabilities& capabilities() const override;
    const RealtimeMediaSourceSettings& settings() const override;
    GstElement* pipeline() { return m_capturer->pipeline(); }
    GStreamerCapturer* capturer() { return m_capturer.get(); }


protected:
    GStreamerVideoCaptureSource(const String& deviceID, const String& name, const gchar * source_factory);
    GStreamerVideoCaptureSource(GStreamerCaptureDevice);
    virtual ~GStreamerVideoCaptureSource();
    void startProducingData() override;
    void stopProducingData() override;

    mutable std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

private:
    static GstFlowReturn newSampleCallback(GstElement*, GStreamerVideoCaptureSource*);

    bool isCaptureSource() const final { return true; }
    bool applySize(const IntSize&) final;
    bool applyFrameRate(double) final;
    bool applyAspectRatio(double) final { return true; }

    std::unique_ptr<GStreamerVideoCapturer> m_capturer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
