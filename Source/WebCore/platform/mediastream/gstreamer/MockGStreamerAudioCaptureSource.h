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

#include "GStreamerAudioCaptureSource.h"

namespace WebCore {

class MockGStreamerAudioCaptureSource final : public GStreamerAudioCaptureSource, RealtimeMediaSource::Observer {
public:
    MockGStreamerAudioCaptureSource(String&& deviceID, String&& name, String&& hashSalt);
    ~MockGStreamerAudioCaptureSource();
    std::optional<std::pair<String, String>> applyConstraints(const MediaConstraints&);
    void applyConstraints(const MediaConstraints&, SuccessHandler&&, FailureHandler&&) final;

private:
    void stopProducingData() final;
    void startProducingData() final;
    const RealtimeMediaSourceSettings& settings() final;
    const RealtimeMediaSourceCapabilities& capabilities() final;

    void captureFailed();
    std::unique_ptr<RealtimeMediaSource> m_wrappedSource;

    void videoSampleAvailable(MediaSample&) override { };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
