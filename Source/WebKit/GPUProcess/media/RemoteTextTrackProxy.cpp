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

#if ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "TextTrackPrivateRemoteConfiguration.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/ISOVTTCue.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {

using namespace WebCore;

RemoteTextTrackProxy::RemoteTextTrackProxy(RemoteMediaPlayerProxy& player, TrackPrivateRemoteIdentifier id, Ref<IPC::Connection>&& connection, InbandTextTrackPrivate& trackPrivate)
    : m_player(player)
    , m_identifier(id)
    , m_webProcessConnection(WTFMove(connection))
    , m_trackPrivate(trackPrivate)
{
    m_trackPrivate->setClient(this);
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::AddRemoteTextTrack(m_identifier, configuration()), m_player.idendifier());
}

TextTrackPrivateRemoteConfiguration& RemoteTextTrackProxy::configuration()
{
    static NeverDestroyed<TextTrackPrivateRemoteConfiguration> configuration;

    configuration->id = m_trackPrivate->id();
    configuration->label = m_trackPrivate->label();
    configuration->language = m_trackPrivate->language();
    configuration->trackIndex = m_trackPrivate->trackIndex();
    configuration->startTimeVariance = m_trackPrivate->startTimeVariance();

    configuration->cueFormat = m_trackPrivate->cueFormat();
    configuration->mode = m_trackPrivate->mode();
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
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoteTextTrackConfigurationChanged(m_identifier, configuration()), m_player.idendifier());
}

void RemoteTextTrackProxy::willRemove()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteTextTrack(m_identifier), m_player.idendifier());
}

void RemoteTextTrackProxy::idChanged(const AtomString&)
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

void RemoteTextTrackProxy::addDataCue(const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::AddDataCue(m_identifier, start, end, IPC::DataReference(reinterpret_cast<const uint8_t*>(data), length)), m_player.idendifier());
}

#if ENABLE(DATACUE_VALUE)
void RemoteTextTrackProxy::addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformDataCue>&& cueData, const String& type)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::AddDataCueWithType(m_identifier, start, end, cueData->encodableValue(), type), m_player.idendifier());
}

void RemoteTextTrackProxy::updateDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformDataCue& cueData)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::UpdateDataCue(m_identifier, start, end, cueData.encodableValue()), m_player.idendifier());
}

void RemoteTextTrackProxy::removeDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformDataCue& cueData)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveDataCue(m_identifier, start, end, cueData.encodableValue()), m_player.idendifier());
}
#endif

void RemoteTextTrackProxy::addGenericCue(InbandGenericCue& cue)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::AddGenericCue(m_identifier, cue.cueData()), m_player.idendifier());
}

void RemoteTextTrackProxy::updateGenericCue(InbandGenericCue& cue)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::UpdateGenericCue(m_identifier, cue.cueData()), m_player.idendifier());
}

void RemoteTextTrackProxy::removeGenericCue(InbandGenericCue& cue)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveGenericCue(m_identifier, cue.cueData()), m_player.idendifier());
}

void RemoteTextTrackProxy::parseWebVTTFileHeader(String&& header)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ParseWebVTTFileHeader(m_identifier, header), m_player.idendifier());
}

void RemoteTextTrackProxy::parseWebVTTCueData(const char* data, unsigned length)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ParseWebVTTCueData(m_identifier, IPC::DataReference(reinterpret_cast<const uint8_t*>(data), length)), m_player.idendifier());
}

void RemoteTextTrackProxy::parseWebVTTCueData(ISOWebVTTCue&& cueData)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ParseWebVTTCueDataStruct(m_identifier, cueData), m_player.idendifier());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
