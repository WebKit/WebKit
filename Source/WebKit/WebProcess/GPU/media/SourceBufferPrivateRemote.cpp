/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceBufferPrivateRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "InitializationSegmentInfo.h"
#include "Logging.h"
#include "MediaPlayerPrivateRemote.h"
#include "MediaSourcePrivateRemote.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SourceBufferPrivateRemoteMessages.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/Ref.h>

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {

using namespace WebCore;

Ref<SourceBufferPrivateRemote> SourceBufferPrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, const MediaSourcePrivateRemote& mediaSourcePrivate, const MediaPlayerPrivateRemote& mediaPlayerPrivate)
{
    return adoptRef(*new SourceBufferPrivateRemote(gpuProcessConnection, remoteSourceBufferIdentifier, mediaSourcePrivate, mediaPlayerPrivate));
}

SourceBufferPrivateRemote::SourceBufferPrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, const MediaSourcePrivateRemote& mediaSourcePrivate, const MediaPlayerPrivateRemote& mediaPlayerPrivate)
    : m_gpuProcessConnection(gpuProcessConnection)
    , m_remoteSourceBufferIdentifier(remoteSourceBufferIdentifier)
    , m_mediaSourcePrivate(makeWeakPtr(mediaSourcePrivate))
    , m_mediaPlayerPrivate(makeWeakPtr(mediaPlayerPrivate))
#if !RELEASE_LOG_DISABLED
    , m_logger(m_mediaSourcePrivate->logger())
    , m_logIdentifier(m_mediaSourcePrivate->nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_gpuProcessConnection.messageReceiverMap().addMessageReceiver(Messages::SourceBufferPrivateRemote::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64(), *this);
}

SourceBufferPrivateRemote::~SourceBufferPrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(!m_client);

    m_gpuProcessConnection.messageReceiverMap().removeMessageReceiver(Messages::SourceBufferPrivateRemote::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64());
}

void SourceBufferPrivateRemote::append(Vector<unsigned char>&& data)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::Append(IPC::DataReference(data)), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::abort()
{
    notImplemented();
}

void SourceBufferPrivateRemote::resetParserState()
{
    notImplemented();
}

void SourceBufferPrivateRemote::removedFromMediaSource()
{
    notImplemented();
}

MediaPlayer::ReadyState SourceBufferPrivateRemote::readyState() const
{
    return m_mediaPlayerPrivate ? m_mediaPlayerPrivate->readyState() : MediaPlayer::ReadyState::HaveNothing;
}

void SourceBufferPrivateRemote::setReadyState(MediaPlayer::ReadyState)
{
    notImplemented();
}

void SourceBufferPrivateRemote::flush(const AtomString& trackID)
{
    notImplemented();
}

void SourceBufferPrivateRemote::enqueueSample(Ref<MediaSample>&&, const AtomString& trackID)
{
    notImplemented();
}

bool SourceBufferPrivateRemote::isReadyForMoreSamples(const AtomString& trackID)
{
    return false;
}

void SourceBufferPrivateRemote::setActive(bool)
{
    notImplemented();
}

void SourceBufferPrivateRemote::notifyClientWhenReadyForMoreSamples(const AtomString& trackID)
{
    notImplemented();
}

bool SourceBufferPrivateRemote::canSetMinimumUpcomingPresentationTime(const AtomString&) const
{
    return false;
}

void SourceBufferPrivateRemote::setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime&)
{
    notImplemented();
}

void SourceBufferPrivateRemote::clearMinimumUpcomingPresentationTime(const AtomString&)
{
    notImplemented();
}

bool SourceBufferPrivateRemote::canSwitchToType(const ContentType&)
{
    notImplemented();
    return false;
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&& segmentInfo)
{
    if (!m_client || !m_mediaPlayerPrivate)
        return;

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = segmentInfo.duration;

    for (auto& audioTrack : segmentInfo.audioTracks) {
        SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
        info.track = m_mediaPlayerPrivate->audioTrackPrivateRemote(audioTrack.identifier);
        info.description = RemoteMediaDescription::create(audioTrack.description);
        segment.audioTracks.append(info);
    }

    for (auto& videoTrack : segmentInfo.videoTracks) {
        SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
        info.track = m_mediaPlayerPrivate->videoTrackPrivateRemote(videoTrack.identifier);
        info.description = RemoteMediaDescription::create(videoTrack.description);
        segment.videoTracks.append(info);
    }

    for (auto& textTrack : segmentInfo.textTracks) {
        SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info;
        info.track = m_mediaPlayerPrivate->textTrackPrivateRemote(textTrack.identifier);
        info.description = RemoteMediaDescription::create(textTrack.description);
        segment.textTracks.append(info);
    }

    m_client->sourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segment));
}

void SourceBufferPrivateRemote::sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::AppendResult appendResult)
{
    if (m_client)
        m_client->sourceBufferPrivateAppendComplete(appendResult);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateRemote::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
