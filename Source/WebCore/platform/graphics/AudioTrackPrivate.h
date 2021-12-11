/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioTrackPrivateClient.h"
#include "TrackPrivateBase.h"
#include <wtf/Function.h>

#if ENABLE(VIDEO)

namespace WebCore {

class AudioTrackPrivate : public TrackPrivateBase {
public:
    static Ref<AudioTrackPrivate> create()
    {
        return adoptRef(*new AudioTrackPrivate);
    }

    void setClient(AudioTrackPrivateClient& client) { m_client = client; }
    void clearClient() { m_client = nullptr; }
    AudioTrackPrivateClient* client() const override { return m_client.get(); }

    virtual void setEnabled(bool enabled)
    {
        if (m_enabled == enabled)
            return;
        m_enabled = enabled;
        if (m_client)
            m_client->enabledChanged(enabled);
        if (m_enabledChangedCallback)
            m_enabledChangedCallback(*this, m_enabled);
    }

    bool enabled() const { return m_enabled; }

    enum Kind { Alternative, Description, Main, MainDesc, Translation, Commentary, None };
    virtual Kind kind() const { return None; }

    virtual bool isBackedByMediaStreamTrack() const { return false; }

    using EnabledChangedCallback = Function<void(AudioTrackPrivate&, bool enabled)>;
    void setEnabledChangedCallback(EnabledChangedCallback&& callback) { m_enabledChangedCallback = WTFMove(callback); }
    
    String codec() const { return m_codec; }
    void setCodec(String&& codec) { m_codec = WTFMove(codec); }
    
    uint32_t sampleRate() const { return m_sampleRate; }
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }
    
    uint32_t numberOfChannels() const { return m_numberOfChannels; }
    void setNumberOfChannels(uint32_t numberOfChannels) { m_numberOfChannels = numberOfChannels; }
    
    uint64_t bitrate() const { return m_bitrate; }
    void setBitrate(uint64_t bitrate) { m_bitrate = bitrate; }

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "AudioTrackPrivate"; }
#endif

protected:
    AudioTrackPrivate() = default;

private:
    WeakPtr<AudioTrackPrivateClient> m_client;
    bool m_enabled { false };
    String m_codec;
    uint32_t m_sampleRate { 0 };
    uint32_t m_numberOfChannels { 0 };
    uint64_t m_bitrate { 0 };
    EnabledChangedCallback m_enabledChangedCallback;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::AudioTrackPrivate::Kind> {
    using values = EnumValues<
        WebCore::AudioTrackPrivate::Kind,
        WebCore::AudioTrackPrivate::Kind::Alternative,
        WebCore::AudioTrackPrivate::Kind::Description,
        WebCore::AudioTrackPrivate::Kind::Main,
        WebCore::AudioTrackPrivate::Kind::MainDesc,
        WebCore::AudioTrackPrivate::Kind::Translation,
        WebCore::AudioTrackPrivate::Kind::Commentary,
        WebCore::AudioTrackPrivate::Kind::None
    >;
};

} // namespace WTF

#endif
