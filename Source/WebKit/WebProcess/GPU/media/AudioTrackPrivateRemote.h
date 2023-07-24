/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/MediaPlayerIdentifier.h>

namespace WebKit {

class GPUProcessConnection;
class MediaPlayerPrivateRemote;
struct AudioTrackPrivateRemoteConfiguration;

class AudioTrackPrivateRemote final : public WebCore::AudioTrackPrivate {
    WTF_MAKE_NONCOPYABLE(AudioTrackPrivateRemote)
public:
    static Ref<AudioTrackPrivateRemote> create(GPUProcessConnection& gpuProcessConnection, WebCore::MediaPlayerIdentifier playerIdentifier, TrackPrivateRemoteIdentifier identifier, AudioTrackPrivateRemoteConfiguration&& configuration)
    {
        return adoptRef(*new AudioTrackPrivateRemote(gpuProcessConnection, playerIdentifier, identifier, WTFMove(configuration)));
    }

    AtomString id() const final { return m_id; }
    void updateConfiguration(AudioTrackPrivateRemoteConfiguration&&);

private:
    AudioTrackPrivateRemote(GPUProcessConnection&, WebCore::MediaPlayerIdentifier, TrackPrivateRemoteIdentifier, AudioTrackPrivateRemoteConfiguration&&);

    using AudioTrackKind = WebCore::AudioTrackPrivate::Kind;
    AudioTrackKind kind() const final { return m_kind; }
    AtomString label() const final { return m_label; }
    AtomString language() const final { return m_language; }
    int trackIndex() const final { return m_trackIndex; }
    void setEnabled(bool) final;
    MediaTime startTimeVariance() const final { return m_startTimeVariance; }

    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    AudioTrackKind m_kind { None };
    AtomString m_id;
    AtomString m_label;
    AtomString m_language;
    int m_trackIndex { -1 };

    MediaTime m_startTimeVariance { MediaTime::zeroTime() };
    WebCore::MediaPlayerIdentifier m_playerIdentifier;
    TrackPrivateRemoteIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
