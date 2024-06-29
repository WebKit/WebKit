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
#include "RemoteTextTrackProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "TextTrackPrivateRemoteConfiguration.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/ISOVTTCue.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {

using namespace WebCore;

RemoteTextTrackProxy::RemoteTextTrackProxy(GPUConnectionToWebProcess& connectionToWebProcess, InbandTextTrackPrivate& trackPrivate, MediaPlayerIdentifier mediaPlayerIdentifier)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_trackPrivate(trackPrivate)
    , m_id(trackPrivate.id())
    , m_mediaPlayerIdentifier(mediaPlayerIdentifier)
{
    m_clientId = trackPrivate.addClient([](auto&& task) {
        ensureOnMainThread(WTFMove(task));
    }, *this);
    connectionToWebProcess.connection().send(Messages::MediaPlayerPrivateRemote::AddRemoteTextTrack(configuration()), m_mediaPlayerIdentifier);
}

RemoteTextTrackProxy::~RemoteTextTrackProxy()
{
    m_trackPrivate->removeClient(m_clientId);
}

TextTrackPrivateRemoteConfiguration& RemoteTextTrackProxy::configuration()
{
    static NeverDestroyed<TextTrackPrivateRemoteConfiguration> configuration;

    configuration->trackId = m_trackPrivate->id();
    configuration->label = m_trackPrivate->label();
    configuration->language = m_trackPrivate->language();
    configuration->trackIndex = m_trackPrivate->trackIndex();
    configuration->inBandMetadataTrackDispatchType = m_trackPrivate->inBandMetadataTrackDispatchType();
    configuration->startTimeVariance = m_trackPrivate->startTimeVariance();

    configuration->cueFormat = m_trackPrivate->cueFormat();
    configuration->isClosedCaptions = m_trackPrivate->isClosedCaptions();
    configuration->isSDH = m_trackPrivate->isSDH();
    configuration->containsOnlyForcedSubtitles = m_trackPrivate->containsOnlyForcedSubtitles();
    configuration->isMainProgramContent = m_trackPrivate->isMainProgramContent();
    configuration->isEasyToRead = m_trackPrivate->isEasyToRead();
    configuration->isDefault = m_trackPrivate->isDefault();
    configuration->kind = m_trackPrivate->kind();

    return configuration.get();
}

void RemoteTextTrackProxy::configurationChanged()
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;

    connection->connection().send(Messages::MediaPlayerPrivateRemote::RemoteTextTrackConfigurationChanged(std::exchange(m_id, m_trackPrivate->id()), configuration()), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::willRemove()
{
    ASSERT_NOT_REACHED();
}

void RemoteTextTrackProxy::idChanged(TrackID)
{
    configurationChanged();
}

void RemoteTextTrackProxy::labelChanged(const AtomString&)
{
    configurationChanged();
}

void RemoteTextTrackProxy::languageChanged(const AtomString&)
{
    configurationChanged();
}

void RemoteTextTrackProxy::addDataCue(const MediaTime& start, const MediaTime& end, std::span<const uint8_t> data)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::AddDataCue(m_trackPrivate->id(), start, end, data), m_mediaPlayerIdentifier);
}

#if ENABLE(DATACUE_VALUE)
void RemoteTextTrackProxy::addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformDataCue>&& cueData, const String& type)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::AddDataCueWithType(m_trackPrivate->id(), start, end, cueData->encodableValue(), type), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::updateDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformDataCue& cueData)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::UpdateDataCue(m_trackPrivate->id(), start, end, cueData.encodableValue()), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::removeDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformDataCue& cueData)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::RemoveDataCue(m_trackPrivate->id(), start, end, cueData.encodableValue()), m_mediaPlayerIdentifier);
}
#endif

void RemoteTextTrackProxy::addGenericCue(InbandGenericCue& cue)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::AddGenericCue(m_trackPrivate->id(), cue.cueData()), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::updateGenericCue(InbandGenericCue& cue)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::UpdateGenericCue(m_trackPrivate->id(), cue.cueData()), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::removeGenericCue(InbandGenericCue& cue)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::RemoveGenericCue(m_trackPrivate->id(), cue.cueData()), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::parseWebVTTFileHeader(String&& header)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::ParseWebVTTFileHeader(m_trackPrivate->id(), header), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::parseWebVTTCueData(std::span<const uint8_t> data)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::ParseWebVTTCueData(m_trackPrivate->id(), data), m_mediaPlayerIdentifier);
}

void RemoteTextTrackProxy::parseWebVTTCueData(ISOWebVTTCue&& cueData)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->connection().send(Messages::MediaPlayerPrivateRemote::ParseWebVTTCueDataStruct(m_trackPrivate->id(), cueData), m_mediaPlayerIdentifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
