/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeIncomingVideoSource.h"

#if USE(LIBWEBRTC)

#include "Logging.h"

namespace WebCore {

RealtimeIncomingVideoSource::RealtimeIncomingVideoSource(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId)
    : RealtimeMediaSource(Type::Video, "remote video"_s, WTFMove(videoTrackId))
    , m_videoTrack(WTFMove(videoTrack))
{
    notifyMutedChange(!m_videoTrack);

    RealtimeMediaSourceSupportedConstraints constraints;
    constraints.setSupportsWidth(true);
    constraints.setSupportsHeight(true);
    m_currentSettings = RealtimeMediaSourceSettings { };
    m_currentSettings->setSupportedConstraints(WTFMove(constraints));
}

void RealtimeIncomingVideoSource::startProducingData()
{
    if (m_videoTrack)
        m_videoTrack->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void RealtimeIncomingVideoSource::setSourceTrack(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& track)
{
    ASSERT(track);

    if (m_videoTrack && isProducingData())
        m_videoTrack->RemoveSink(this);
    m_videoTrack = WTFMove(track);
    notifyMutedChange(!m_videoTrack);
    if (isProducingData())
        m_videoTrack->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void RealtimeIncomingVideoSource::stopProducingData()
{
    if (m_videoTrack)
        m_videoTrack->RemoveSink(this);
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingVideoSource::capabilities()
{
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

const RealtimeMediaSourceSettings& RealtimeIncomingVideoSource::settings()
{
    if (m_currentSettings)
        return m_currentSettings.value();

    RealtimeMediaSourceSupportedConstraints constraints;
    constraints.setSupportsWidth(true);
    constraints.setSupportsHeight(true);

    RealtimeMediaSourceSettings settings;
    auto& size = this->size();
    settings.setWidth(size.width());
    settings.setHeight(size.height());
    settings.setSupportedConstraints(constraints);

    m_currentSettings = WTFMove(settings);
    return m_currentSettings.value();
}

void RealtimeIncomingVideoSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height }))
        m_currentSettings = WTF::nullopt;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
