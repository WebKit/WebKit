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
#include "RealtimeIncomingAudioSource.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCAudioFormat.h"
#include "Logging.h"
#include <wtf/CryptographicallyRandomNumber.h>

namespace WebCore {

RealtimeIncomingAudioSource::RealtimeIncomingAudioSource(rtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, "remote audio"_s, WTFMove(audioTrackId))
    , m_audioTrack(WTFMove(audioTrack))
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(reinterpret_cast<const void*>(cryptographicallyRandomNumber()))
#endif
{
    notifyMutedChange(!m_audioTrack);
}

RealtimeIncomingAudioSource::~RealtimeIncomingAudioSource()
{
    stop();
}

void RealtimeIncomingAudioSource::startProducingData()
{
    if (m_audioTrack)
        m_audioTrack->AddSink(this);
}

void RealtimeIncomingAudioSource::stopProducingData()
{
    if (m_audioTrack)
        m_audioTrack->RemoveSink(this);
}

void RealtimeIncomingAudioSource::setSourceTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>&& track)
{
    ASSERT(track);

    if (m_audioTrack && isProducingData())
        m_audioTrack->RemoveSink(this);

    m_audioTrack = WTFMove(track);
    notifyMutedChange(!m_audioTrack);
    if (isProducingData())
        m_audioTrack->AddSink(this);
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingAudioSource::capabilities()
{
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

const RealtimeMediaSourceSettings& RealtimeIncomingAudioSource::settings()
{
    return m_currentSettings;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RealtimeIncomingAudioSource::logChannel() const
{
    return LogWebRTC;
}

const Logger& RealtimeIncomingAudioSource::logger() const
{
    if (!m_logger)
        m_logger = Logger::create(this);
    return *m_logger;
}
#endif

}

#endif // USE(LIBWEBRTC)
