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
#include "RemoteVideoTrackProxy.h"

#if ENABLE(GPU_PROCESS)

#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "TrackPrivateRemoteConfiguration.h"

namespace WebKit {

RemoteVideoTrackProxy::RemoteVideoTrackProxy(RemoteMediaPlayerProxy& player, TrackPrivateRemoteIdentifier id, Ref<IPC::Connection>&& connection, VideoTrackPrivate& trackPrivate)
    : m_player(player)
    , m_identifier(id)
    , m_webProcessConnection(WTFMove(connection))
    , m_trackPrivate(trackPrivate)
{
    m_trackPrivate->setClient(this);
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::AddRemoteVideoTrack(m_identifier, configuration()), m_player.idendifier());
}

TrackPrivateRemoteConfiguration& RemoteVideoTrackProxy::configuration()
{
    static NeverDestroyed<TrackPrivateRemoteConfiguration> configuration;

    configuration->id = m_trackPrivate->id();
    configuration->label = m_trackPrivate->label();
    configuration->language = m_trackPrivate->language();
    configuration->trackIndex = m_trackPrivate->trackIndex();
    configuration->startTimeVariance = m_trackPrivate->startTimeVariance();
    configuration->selected = m_trackPrivate->selected();
    configuration->videoKind = m_trackPrivate->kind();

    return configuration.get();
}

void RemoteVideoTrackProxy::configurationChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoteVideoTrackConfigurationChanged(m_identifier, configuration()), m_player.idendifier());
}

void RemoteVideoTrackProxy::willRemove()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteVideoTrack(m_identifier), m_player.idendifier());
}

void RemoteVideoTrackProxy::selectedChanged(bool)
{
    configurationChanged();
}

void RemoteVideoTrackProxy::idChanged(const AtomString&)
{
    configurationChanged();
}

void RemoteVideoTrackProxy::labelChanged(const AtomString&)
{
    configurationChanged();
}

void RemoteVideoTrackProxy::languageChanged(const AtomString&)
{
    configurationChanged();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
