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
 * 3. Neither the name of Ericsson nor the names of its contributors
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

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RealtimeMediaSource.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/mediastreaminterface.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/RetainPtr.h>

namespace WebCore {

class RealtimeIncomingAudioSource : public RealtimeMediaSource, private webrtc::AudioTrackSinkInterface {
public:
    static Ref<RealtimeIncomingAudioSource> create(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&, String&&);

    void setSourceTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&);

protected:
    RealtimeIncomingAudioSource(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&, String&&);
    ~RealtimeIncomingAudioSource();

private:
    // webrtc::AudioTrackSinkInterface API
    virtual void OnData(const void* /* audioData */, int /* bitsPerSample */, int /* sampleRate */, size_t /* numberOfChannels */, size_t /* numberOfFrames */) { };

    // RealtimeMediaSource API
    void startProducingData() final;
    void stopProducingData()  final;

    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;

    bool isIncomingAudioSource() const final { return true; }

    RealtimeMediaSourceSettings m_currentSettings;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> m_audioTrack;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RealtimeIncomingAudioSource)
    static bool isType(const WebCore::RealtimeMediaSource& source) { return source.isIncomingAudioSource(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(LIBWEBRTC)
