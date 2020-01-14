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

#include "config.h"
#include "AudioTrackPrivateRemote.h"

#if ENABLE(GPU_PROCESS)

#include "MediaPlayerPrivateRemote.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "TrackPrivateRemoteConfiguration.h"

namespace WebKit {

AudioTrackPrivateRemote::AudioTrackPrivateRemote(MediaPlayerPrivateRemote& player, TrackPrivateRemoteIdentifier idendifier, TrackPrivateRemoteConfiguration&& configuration)
    : m_player(player)
    , m_idendifier(idendifier)
{
    updateConfiguration(WTFMove(configuration));
}

void AudioTrackPrivateRemote::setEnabled(bool enabled)
{
    if (enabled != this->enabled())
        m_player.connection().send(Messages::RemoteMediaPlayerProxy::AudioTrackSetEnabled(m_idendifier, enabled), m_player.itentifier());

    AudioTrackPrivate::setEnabled(enabled);
}

void AudioTrackPrivateRemote::updateConfiguration(TrackPrivateRemoteConfiguration&& configuration)
{
    if (configuration.id != m_id) {
        auto changed = !m_id.isEmpty();
        m_id = configuration.id;
        if (changed && client())
            client()->idChanged(m_id);
    }

    if (configuration.label != m_label) {
        auto changed = !m_label.isEmpty();
        m_label = configuration.label;
        if (changed && client())
            client()->labelChanged(m_label);
    }

    if (configuration.language != m_language) {
        auto changed = !m_language.isEmpty();
        m_language = configuration.language;
        if (changed && client())
            client()->languageChanged(m_language);
    }

    m_trackIndex = configuration.trackIndex;
    m_startTimeVariance = configuration.startTimeVariance;
    m_kind = configuration.audioKind;
    setEnabled(configuration.enabled);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
