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

#if ENABLE(GPU_PROCESS)

#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/TrackBase.h>

namespace WebKit {

struct TrackPrivateRemoteConfiguration;

class RemoteAudioTrackProxy final
    : public ThreadSafeRefCounted<TrackPrivateBase, WTF::DestructionThread::Main>
    , private WebCore::AudioTrackPrivateClient {
public:
    static Ref<RemoteAudioTrackProxy> create(RemoteMediaPlayerProxy& player, TrackPrivateRemoteIdentifier id, Ref<IPC::Connection>&& connection, AudioTrackPrivate& trackPrivate)
    {
        return adoptRef(*new RemoteAudioTrackProxy(player, id, WTFMove(connection), trackPrivate));
    }

    TrackPrivateRemoteIdentifier identifier() const { return m_identifier; };

    void setEnabled(bool enabled) { m_trackPrivate->setEnabled(enabled); }

private:
    RemoteAudioTrackProxy(RemoteMediaPlayerProxy&, TrackPrivateRemoteIdentifier, Ref<IPC::Connection>&&, AudioTrackPrivate&);
    ~RemoteAudioTrackProxy() = default;

    // AudioTrackPrivateClient
    void enabledChanged(bool) final;

    // TrackPrivateBaseClient
    void idChanged(const AtomString&) final;
    void labelChanged(const AtomString&) final;
    void languageChanged(const AtomString&) final;
    void willRemove() final;

    TrackPrivateRemoteConfiguration& configuration();
    void configurationChanged();

    RemoteMediaPlayerProxy& m_player;
    TrackPrivateRemoteIdentifier m_identifier;
    Ref<IPC::Connection> m_webProcessConnection;
    Ref<AudioTrackPrivate> m_trackPrivate;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
