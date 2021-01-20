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
#include <WebCore/PlatformTimeRanges.h>
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
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::Abort(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetParserState()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::ResetParserState(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::removedFromMediaSource()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::RemovedFromMediaSource(), m_remoteSourceBufferIdentifier);
}

MediaPlayer::ReadyState SourceBufferPrivateRemote::readyState() const
{
    return m_mediaPlayerPrivate ? m_mediaPlayerPrivate->readyState() : MediaPlayer::ReadyState::HaveNothing;
}

void SourceBufferPrivateRemote::setReadyState(MediaPlayer::ReadyState state)
{
    if (!m_mediaSourcePrivate)
        return;

    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetReadyState(state), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setActive(bool active)
{
    if (!m_mediaSourcePrivate)
        return;

    m_isActive = active;
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetActive(active), m_remoteSourceBufferIdentifier);
}

bool SourceBufferPrivateRemote::canSwitchToType(const ContentType&)
{
    notImplemented();
    return false;
}

void SourceBufferPrivateRemote::setMediaSourceEnded(bool isEnded)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetMediaSourceEnded(isEnded), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setMode(SourceBufferAppendMode mode)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetMode(mode), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::updateBufferedFromTrackBuffers(bool sourceIsEnded)
{
    if (!m_mediaSourcePrivate)
        return;

    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::UpdateBufferedFromTrackBuffers(sourceIsEnded), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime, bool isEnded, CompletionHandler<void()>&& completionHandler)
{
    m_gpuProcessConnection.connection().sendWithAsyncReply(Messages::RemoteSourceBufferProxy::RemoveCodedFrames(start, end, currentMediaTime, isEnded), WTFMove(completionHandler), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::evictCodedFrames(uint64_t newDataSize, uint64_t pendingAppendDataCapacity, uint64_t maximumBufferSize, const MediaTime& currentTime, const MediaTime& duration, bool isEnded)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::EvictCodedFrames(newDataSize, pendingAppendDataCapacity, maximumBufferSize, currentTime, duration, isEnded), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::addTrackBuffer(const AtomString& trackId, RefPtr<MediaDescription>&&)
{
    ASSERT(m_trackIdentifierMap.contains(trackId));
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::AddTrackBuffer(m_trackIdentifierMap.get(trackId)), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetTrackBuffers()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::ResetTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::clearTrackBuffers()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::ClearTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAllTrackBuffersNeedRandomAccess()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetAllTrackBuffersNeedRandomAccess(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setGroupStartTimestamp(const MediaTime& timestamp)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestamp(timestamp), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setGroupStartTimestampToEndTimestamp()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestampToEndTimestamp(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetShouldGenerateTimestamps(shouldGenerateTimestamps), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime, uint64_t pendingAppendDataCapacity, uint64_t maximumBufferSize)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::ReenqueueMediaIfNeeded(currentMediaTime, pendingAppendDataCapacity, maximumBufferSize), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetTimestampOffsetInTrackBuffers()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::ResetTimestampOffsetInTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::startChangingType()
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::StartChangingType(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setTimestampOffset(const MediaTime& timestampOffset)
{
    SourceBufferPrivate::setTimestampOffset(timestampOffset);
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetTimestampOffset(timestampOffset), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowStart(appendWindowStart), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowEnd(appendWindowEnd), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::seekToTime(const MediaTime& mediaTime)
{
    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::SeekToTime(mediaTime), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::updateTrackIds(Vector<std::pair<AtomString, AtomString>>&& trackIdPairs)
{
    Vector<std::pair<TrackPrivateRemoteIdentifier, TrackPrivateRemoteIdentifier>> identifierPairs;

    for (auto& trackIdPair : trackIdPairs) {
        ASSERT(m_trackIdentifierMap.contains(trackIdPair.first));
        ASSERT(m_trackIdentifierMap.contains(trackIdPair.second));

        auto oldIdentifier = m_trackIdentifierMap.take(trackIdPair.first);
        auto newIdentifier = m_trackIdentifierMap.get(trackIdPair.second);
        identifierPairs.append(std::make_pair(oldIdentifier, newIdentifier));
    }

    m_gpuProcessConnection.connection().send(Messages::RemoteSourceBufferProxy::UpdateTrackIds(identifierPairs), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::bufferedSamplesForTrackId(const AtomString& trackId, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    m_gpuProcessConnection.connection().sendWithAsyncReply(Messages::RemoteSourceBufferProxy::BufferedSamplesForTrackId(m_trackIdentifierMap.get(trackId)), [completionHandler = WTFMove(completionHandler)](auto&& samples) mutable {
        completionHandler(WTFMove(samples));
    }, m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&& segmentInfo, CompletionHandler<void()>&& completionHandler)
{
    if (!m_client || !m_mediaPlayerPrivate) {
        completionHandler();
        return;
    }

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = segmentInfo.duration;

    for (auto& audioTrack : segmentInfo.audioTracks) {
        SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
        info.track = m_mediaPlayerPrivate->audioTrackPrivateRemote(audioTrack.identifier);
        info.description = RemoteMediaDescription::create(audioTrack.description);
        segment.audioTracks.append(info);
        m_trackIdentifierMap.add(info.track->id(), audioTrack.identifier);
    }

    for (auto& videoTrack : segmentInfo.videoTracks) {
        SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
        info.track = m_mediaPlayerPrivate->videoTrackPrivateRemote(videoTrack.identifier);
        info.description = RemoteMediaDescription::create(videoTrack.description);
        segment.videoTracks.append(info);
        m_trackIdentifierMap.add(info.track->id(), videoTrack.identifier);
    }

    for (auto& textTrack : segmentInfo.textTracks) {
        SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info;
        info.track = m_mediaPlayerPrivate->textTrackPrivateRemote(textTrack.identifier);
        info.description = RemoteMediaDescription::create(textTrack.description);
        segment.textTracks.append(info);
        m_trackIdentifierMap.add(info.track->id(), textTrack.identifier);
    }

    m_client->sourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segment), WTFMove(completionHandler));
}

void SourceBufferPrivateRemote::sourceBufferPrivateStreamEndedWithDecodeError()
{
    if (m_client)
        m_client->sourceBufferPrivateStreamEndedWithDecodeError();
}

void SourceBufferPrivateRemote::sourceBufferPrivateAppendError(bool decodeError)
{
    if (m_client)
        m_client->sourceBufferPrivateAppendError(decodeError);
}

void SourceBufferPrivateRemote::sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::AppendResult appendResult)
{
    if (m_client)
        m_client->sourceBufferPrivateAppendComplete(appendResult);
}

void SourceBufferPrivateRemote::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    if (m_client)
        m_client->sourceBufferPrivateHighestPresentationTimestampChanged(timestamp);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDurationChanged(const MediaTime& duration)
{
    if (m_client)
        m_client->sourceBufferPrivateDurationChanged(duration);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidParseSample(double sampleDuration)
{
    if (m_client)
        m_client->sourceBufferPrivateDidParseSample(sampleDuration);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidDropSample()
{
    if (m_client)
        m_client->sourceBufferPrivateDidDropSample();
}

void SourceBufferPrivateRemote::sourceBufferPrivateBufferedDirtyChanged(bool dirty)
{
    if (m_client)
        m_client->sourceBufferPrivateBufferedDirtyChanged(dirty);
}

void SourceBufferPrivateRemote::sourceBufferPrivateBufferedRangesChanged(const WebCore::PlatformTimeRanges& timeRanges)
{
    if (m_client)
        m_client->sourceBufferPrivateBufferedRangesChanged(timeRanges);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode)
{
    if (m_client)
        m_client->sourceBufferPrivateDidReceiveRenderingError(errorCode);
}

void SourceBufferPrivateRemote::sourceBufferPrivateReportExtraMemoryCost(uint64_t extraMemory)
{
    if (m_client)
        m_client->sourceBufferPrivateReportExtraMemoryCost(extraMemory);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateRemote::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
