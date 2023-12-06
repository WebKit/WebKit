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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "VideoTrackPrivateRemoteConfiguration.h"

namespace WebKit {

using namespace WebCore;

RemoteVideoTrackProxy::RemoteVideoTrackProxy(GPUConnectionToWebProcess& connectionToWebProcess, VideoTrackPrivate& trackPrivate, MediaPlayerIdentifier mediaPlayerIdentifier)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_trackPrivate(trackPrivate)
    , m_id(trackPrivate.id())
    , m_mediaPlayerIdentifier(mediaPlayerIdentifier)
{
    m_trackPrivate->setClient(*this);
    m_connectionToWebProcess->connection().send(Messages::MediaPlayerPrivateRemote::AddRemoteVideoTrack(configuration()), m_mediaPlayerIdentifier);
}

RemoteVideoTrackProxy::~RemoteVideoTrackProxy()
{
    m_trackPrivate->clearClient();
}

VideoTrackPrivateRemoteConfiguration RemoteVideoTrackProxy::configuration()
{
    return {
        {
            m_trackPrivate->id(),
            m_trackPrivate->label(),
            m_trackPrivate->language(),
            m_trackPrivate->startTimeVariance(),
            m_trackPrivate->trackIndex(),
        },
        m_trackPrivate->selected(),
        m_trackPrivate->kind(),
        m_trackPrivate->configuration(),
    };
}

void RemoteVideoTrackProxy::updateConfiguration()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::MediaPlayerPrivateRemote::RemoteVideoTrackConfigurationChanged(std::exchange(m_id, m_trackPrivate->id()), configuration()), m_mediaPlayerIdentifier);
}

void RemoteVideoTrackProxy::willRemove()
{
    ASSERT_NOT_REACHED();
}

void RemoteVideoTrackProxy::selectedChanged(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    updateConfiguration();
}

void RemoteVideoTrackProxy::idChanged(TrackID)
{
    updateConfiguration();
}

void RemoteVideoTrackProxy::labelChanged(const AtomString&)
{
    updateConfiguration();
}

void RemoteVideoTrackProxy::languageChanged(const AtomString&)
{
    updateConfiguration();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
