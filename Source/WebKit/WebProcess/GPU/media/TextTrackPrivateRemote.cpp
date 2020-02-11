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

#if ENABLE(GPU_PROCESS)

#include "MediaPlayerPrivateRemote.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

TextTrackPrivateRemote::TextTrackPrivateRemote(MediaPlayerPrivateRemote& player, TrackPrivateRemoteIdentifier idendifier, TextTrackPrivateRemoteConfiguration&& configuration)
    : WebCore::InbandTextTrackPrivate(configuration.cueFormat)
    , m_player(player)
    , m_idendifier(idendifier)
{
    updateConfiguration(WTFMove(configuration));
}

void TextTrackPrivateRemote::setMode(TextTrackMode mode)
{
    if (mode != m_mode)
        m_player.connection().send(Messages::RemoteMediaPlayerProxy::TextTrackSetMode(m_idendifier, mode), m_player.itentifier());

    InbandTextTrackPrivate::setMode(mode);
}

void TextTrackPrivateRemote::updateConfiguration(TextTrackPrivateRemoteConfiguration&& configuration)
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

    m_format = configuration.cueFormat;
    m_kind = configuration.kind;
    m_mode = configuration.mode;
    m_isClosedCaptions = configuration.isClosedCaptions;
    m_isSDH = configuration.isSDH;
    m_containsOnlyForcedSubtitles = configuration.containsOnlyForcedSubtitles;
    m_isMainProgramContent = configuration.isMainProgramContent;
    m_isEasyToRead = configuration.isEasyToRead;
    m_isDefault = configuration.isDefault;
}

void TextTrackPrivateRemote::addDataCue(const MediaTime& start, const MediaTime& end, const void*, unsigned)
{
    notImplemented();
}

#if ENABLE(DATACUE_VALUE)
void TextTrackPrivateRemote::addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformRepresentation>&&, const String&)
{
    notImplemented();
}

void TextTrackPrivateRemote::updateDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformRepresentation&)
{
    notImplemented();
}

void TextTrackPrivateRemote::removeDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformRepresentation&)
{
    notImplemented();
}

#endif

void TextTrackPrivateRemote::addGenericCue(GenericCueData&)
{
    notImplemented();
}

void TextTrackPrivateRemote::updateGenericCue(GenericCueData&)
{
    notImplemented();
}

void TextTrackPrivateRemote::removeGenericCue(GenericCueData&)
{
    notImplemented();
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
        client->parseWebVTTCueData(reinterpret_cast<const char*>(data.data()), data.size());
}

void TextTrackPrivateRemote::parseWebVTTCueDataStruct(ISOWebVTTCue&& cueData)
{
    ASSERT(client());
    if (auto* client = this->client())
        client->parseWebVTTCueData(WTFMove(cueData));
}


} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
