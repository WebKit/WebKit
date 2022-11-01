/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "LibWebRTCAudioModule.h"
#include "Logging.h"

namespace WebCore {

RealtimeIncomingAudioSource::RealtimeIncomingAudioSource(rtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
    : RealtimeMediaSource(CaptureDevice { WTFMove(audioTrackId), CaptureDevice::DeviceType::Microphone, "remote audio"_s })
    , m_audioTrack(WTFMove(audioTrack))
{
    ASSERT(m_audioTrack);
    m_audioTrack->RegisterObserver(this);
}

RealtimeIncomingAudioSource::~RealtimeIncomingAudioSource()
{
    stop();
    m_audioTrack->UnregisterObserver(this);
}

void RealtimeIncomingAudioSource::startProducingData()
{
    m_audioTrack->AddSink(this);
}

void RealtimeIncomingAudioSource::stopProducingData()
{
    m_audioTrack->RemoveSink(this);
}

void RealtimeIncomingAudioSource::OnChanged()
{
    callOnMainThread([protectedThis = Ref { *this }] {
        if (protectedThis->m_audioTrack->state() == webrtc::MediaStreamTrackInterface::kEnded)
            protectedThis->end();
    });
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingAudioSource::capabilities()
{
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

const RealtimeMediaSourceSettings& RealtimeIncomingAudioSource::settings()
{
    return m_currentSettings;
}

void RealtimeIncomingAudioSource::setAudioModule(RefPtr<LibWebRTCAudioModule>&& audioModule)
{
    ASSERT(!m_audioModule);
    m_audioModule = WTFMove(audioModule);
}

}

#endif // USE(LIBWEBRTC)
