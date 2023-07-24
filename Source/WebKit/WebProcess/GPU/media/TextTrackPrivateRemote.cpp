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
#include "TextTrackPrivateRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "DataReference.h"
#include "GPUProcessConnection.h"
#include "MediaPlayerPrivateRemote.h"
#include "RemoteMediaPlayerProxyMessages.h"

namespace WebKit {
using namespace WebCore;

TextTrackPrivateRemote::TextTrackPrivateRemote(GPUProcessConnection& gpuProcessConnection, MediaPlayerIdentifier playerIdentifier, TrackPrivateRemoteIdentifier identifier, TextTrackPrivateRemoteConfiguration&& configuration)
    : WebCore::InbandTextTrackPrivate(configuration.cueFormat)
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_playerIdentifier(playerIdentifier)
    , m_identifier(identifier)
{
    updateConfiguration(WTFMove(configuration));
}

void TextTrackPrivateRemote::setMode(TextTrackMode mode)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection)
        return;

    if (mode == InbandTextTrackPrivate::mode())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteMediaPlayerProxy::TextTrackSetMode(m_identifier, mode), m_playerIdentifier);
    InbandTextTrackPrivate::setMode(mode);
}

void TextTrackPrivateRemote::updateConfiguration(TextTrackPrivateRemoteConfiguration&& configuration)
{
    if (configuration.trackId != m_id) {
        auto changed = !m_id.isEmpty();
        m_id = configuration.trackId;
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
    m_inBandMetadataTrackDispatchType = configuration.inBandMetadataTrackDispatchType;
    m_startTimeVariance = configuration.startTimeVariance;

    m_kind = configuration.kind;
    m_isClosedCaptions = configuration.isClosedCaptions;
    m_isSDH = configuration.isSDH;
    m_containsOnlyForcedSubtitles = configuration.containsOnlyForcedSubtitles;
    m_isMainProgramContent = configuration.isMainProgramContent;
    m_isEasyToRead = configuration.isEasyToRead;
    m_isDefault = configuration.isDefault;
}

void TextTrackPrivateRemote::addGenericCue(Ref<InbandGenericCue> cue)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->addGenericCue(cue);
}

void TextTrackPrivateRemote::updateGenericCue(Ref<InbandGenericCue> cue)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->updateGenericCue(cue);
}

void TextTrackPrivateRemote::removeGenericCue(Ref<InbandGenericCue> cue)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->removeGenericCue(cue);
}

void TextTrackPrivateRemote::parseWebVTTFileHeader(String&& header)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->parseWebVTTFileHeader(WTFMove(header));
}

void TextTrackPrivateRemote::parseWebVTTCueData(const IPC::DataReference& data)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->parseWebVTTCueData(data.data(), data.size());
}

void TextTrackPrivateRemote::parseWebVTTCueDataStruct(ISOWebVTTCue&& cueData)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->parseWebVTTCueData(WTFMove(cueData));
}

void TextTrackPrivateRemote::addDataCue(MediaTime&& start, MediaTime&& end, IPC::DataReference&& data)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->addDataCue(WTFMove(start), WTFMove(end), data.data(), data.size());
}

#if ENABLE(DATACUE_VALUE)
void TextTrackPrivateRemote::addDataCueWithType(MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& dataValue, String&& type)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->addDataCue(WTFMove(start), WTFMove(end), WebCore::SerializedPlatformDataCue::create(WTFMove(dataValue)), type);
}

void TextTrackPrivateRemote::updateDataCue(MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& dataValue)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->updateDataCue(WTFMove(start), WTFMove(end), WebCore::SerializedPlatformDataCue::create(WTFMove(dataValue)));
}

void TextTrackPrivateRemote::removeDataCue(MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& dataValue)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->removeDataCue(WTFMove(start), WTFMove(end), WebCore::SerializedPlatformDataCue::create(WTFMove(dataValue)));
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
