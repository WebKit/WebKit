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
#include "RemoteAudioTrackProxy.h"

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "TrackPrivateRemoteConfiguration.h"

namespace WebKit {

using namespace WebCore;

RemoteAudioTrackProxy::RemoteAudioTrackProxy(GPUConnectionToWebProcess& connectionToWebProcess, TrackPrivateRemoteIdentifier identifier, AudioTrackPrivate& trackPrivate, MediaPlayerIdentifier mediaPlayerIdentifier)
    : m_connectionToWebProcess(makeWeakPtr(connectionToWebProcess))
    , m_identifier(identifier)
    , m_trackPrivate(trackPrivate)
    , m_mediaPlayerIdentifier(mediaPlayerIdentifier)
{
    m_trackPrivate->setClient(this);
    m_connectionToWebProcess->connection().send(Messages::MediaPlayerPrivateRemote::AddRemoteAudioTrack(m_identifier, configuration()), m_mediaPlayerIdentifier);
}

RemoteAudioTrackProxy::~RemoteAudioTrackProxy()
{
}

TrackPrivateRemoteConfiguration& RemoteAudioTrackProxy::configuration()
{
    static NeverDestroyed<TrackPrivateRemoteConfiguration> configuration;

    configuration->trackId = m_trackPrivate->id();
    configuration->label = m_trackPrivate->label();
    configuration->language = m_trackPrivate->language();
    configuration->trackIndex = m_trackPrivate->trackIndex();
    configuration->startTimeVariance = m_trackPrivate->startTimeVariance();
    configuration->enabled = m_trackPrivate->enabled();
    configuration->audioKind = m_trackPrivate->kind();

    return configuration.get();
}

void RemoteAudioTrackProxy::configurationChanged()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::MediaPlayerPrivateRemote::RemoteAudioTrackConfigurationChanged(m_identifier, configuration()), m_mediaPlayerIdentifier);
}

void RemoteAudioTrackProxy::willRemove()
{
    ASSERT_NOT_REACHED();
}

void RemoteAudioTrackProxy::enabledChanged(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    configurationChanged();
}

void RemoteAudioTrackProxy::idChanged(const AtomString&)
{
    configurationChanged();
}

void RemoteAudioTrackProxy::labelChanged(const AtomString&)
{
    configurationChanged();
}

void RemoteAudioTrackProxy::languageChanged(const AtomString&)
{
    configurationChanged();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
