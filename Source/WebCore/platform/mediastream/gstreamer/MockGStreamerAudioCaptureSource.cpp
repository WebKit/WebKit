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

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "MockGStreamerAudioCaptureSource.h"

#include "MockRealtimeAudioSource.h"

namespace WebCore {

class WrappedMockRealtimeAudioSource : public MockRealtimeAudioSource {
public:
    WrappedMockRealtimeAudioSource(String&& deviceID, String&& name, String&& hashSalt)
        : MockRealtimeAudioSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
    {
    }
};

CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID,
    String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockGStreamerAudioCaptureSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));

    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

std::optional<std::pair<String, String>> MockGStreamerAudioCaptureSource::applyConstraints(const MediaConstraints& constraints)
{
    m_wrappedSource->applyConstraints(constraints);
    return GStreamerAudioCaptureSource::applyConstraints(constraints);
}

void MockGStreamerAudioCaptureSource::applyConstraints(const MediaConstraints& constraints, SuccessHandler&& successHandler, FailureHandler&& failureHandler)
{
    m_wrappedSource->applyConstraints(constraints, WTFMove(successHandler), WTFMove(failureHandler));
}

MockGStreamerAudioCaptureSource::MockGStreamerAudioCaptureSource(String&& deviceID, String&& name, String&& hashSalt)
    : GStreamerAudioCaptureSource(String { deviceID }, String { name }, String { hashSalt })
    , m_wrappedSource(std::make_unique<WrappedMockRealtimeAudioSource>(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)))
{
    m_wrappedSource->addObserver(*this);
}

MockGStreamerAudioCaptureSource::~MockGStreamerAudioCaptureSource()
{
    m_wrappedSource->removeObserver(*this);
}

void MockGStreamerAudioCaptureSource::stopProducingData()
{
    m_wrappedSource->stop();

    GStreamerAudioCaptureSource::stopProducingData();
}

void MockGStreamerAudioCaptureSource::startProducingData()
{
    GStreamerAudioCaptureSource::startProducingData();
    m_wrappedSource->start();
}

const RealtimeMediaSourceSettings& MockGStreamerAudioCaptureSource::settings()
{
    return m_wrappedSource->settings();
}

const RealtimeMediaSourceCapabilities& MockGStreamerAudioCaptureSource::capabilities()
{
    m_capabilities = m_wrappedSource->capabilities();
    m_currentSettings = m_wrappedSource->settings();
    return m_wrappedSource->capabilities();
}

void MockGStreamerAudioCaptureSource::captureFailed()
{
    stop();
    RealtimeMediaSource::captureFailed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
