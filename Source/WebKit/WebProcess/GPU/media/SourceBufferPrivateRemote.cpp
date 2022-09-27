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
#include "SharedBufferReference.h"
#include "SourceBufferPrivateRemoteMessages.h"
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
    , m_mediaSourcePrivate(mediaSourcePrivate)
    , m_mediaPlayerPrivate(mediaPlayerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_mediaSourcePrivate->logger())
    , m_logIdentifier(m_mediaSourcePrivate->nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::SourceBufferPrivateRemote::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64(), *this);
}

SourceBufferPrivateRemote::~SourceBufferPrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(!m_client);

    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::SourceBufferPrivateRemote::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64());
}

void SourceBufferPrivateRemote::append(Ref<SharedBuffer>&& data)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().sendWithAsyncReply(Messages::RemoteSourceBufferProxy::Append(IPC::SharedBufferReference { WTFMove(data) }), [] (auto&& bufferHandle) {
        // Take ownership of shared memory and mark it as media-related memory.
        if (bufferHandle)
            bufferHandle->takeOwnershipOfMemory(MemoryLedger::Media);
    }, m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::abort()
{
    if (!m_gpuProcessConnection)
        return;

    // When abort() is being issued; this will force the RemoteSourceBufferProxy to abort the current remote source buffer operation
    // The totalTrackBufferSizeInBytes will be recalculated after the next operation which allows for potentially having
    // m_totalTrackBufferSizeInBytes being temporarily stale as it won't be used until then.
    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::Abort(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetParserState()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ResetParserState(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::removedFromMediaSource()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::RemovedFromMediaSource(), m_remoteSourceBufferIdentifier);
}

MediaPlayer::ReadyState SourceBufferPrivateRemote::readyState() const
{
    return m_mediaPlayerPrivate ? m_mediaPlayerPrivate->readyState() : MediaPlayer::ReadyState::HaveNothing;
}

void SourceBufferPrivateRemote::setReadyState(MediaPlayer::ReadyState state)
{
    if (!m_mediaSourcePrivate)
        return;

    if (m_mediaPlayerPrivate)
        m_mediaPlayerPrivate->setReadyState(state);

    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetReadyState(state), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setActive(bool active)
{
    if (!m_mediaSourcePrivate)
        return;

    if (!m_gpuProcessConnection)
        return;

    m_isActive = active;
    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetActive(active), m_remoteSourceBufferIdentifier);
}

bool SourceBufferPrivateRemote::canSwitchToType(const ContentType& contentType)
{
    if (!m_gpuProcessConnection)
        return false;

    auto sendResult = m_gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::CanSwitchToType(contentType), m_remoteSourceBufferIdentifier);
    auto [canSwitch] = sendResult.takeReplyOr(false);
    return canSwitch;
}

void SourceBufferPrivateRemote::setMediaSourceEnded(bool isEnded)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetMediaSourceEnded(isEnded), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setMode(SourceBufferAppendMode mode)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetMode(mode), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::updateBufferedFromTrackBuffers(bool sourceIsEnded)
{
    if (!m_mediaSourcePrivate)
        return;

    if (!m_gpuProcessConnection)
        return;

    auto sendResult = m_gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::UpdateBufferedFromTrackBuffers(sourceIsEnded), m_remoteSourceBufferIdentifier);
    if (!sendResult)
        return;

    auto [buffered] = sendResult.takeReply();
    setBufferedRanges(buffered);
}

void SourceBufferPrivateRemote::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime, bool isEnded, CompletionHandler<void()>&& completionHandler)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().sendWithAsyncReply(
        Messages::RemoteSourceBufferProxy::RemoveCodedFrames(start, end, currentMediaTime, isEnded), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto&& buffered, uint64_t totalTrackBufferSizeInBytes) mutable {
            setBufferedRanges(buffered);
            m_totalTrackBufferSizeInBytes = totalTrackBufferSizeInBytes;
            completionHandler();
        },
        m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime, const MediaTime& duration, bool isEnded)
{
    if (!m_gpuProcessConnection)
        return;

    auto sendResult = m_gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::EvictCodedFrames(newDataSize, maximumBufferSize, currentTime, duration, isEnded), m_remoteSourceBufferIdentifier);
    if (sendResult)
        std::tie(m_totalTrackBufferSizeInBytes) = sendResult.takeReply();
}

void SourceBufferPrivateRemote::addTrackBuffer(const AtomString& trackId, RefPtr<MediaDescription>&&)
{
    ASSERT(m_trackIdentifierMap.contains(trackId));
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::AddTrackBuffer(m_trackIdentifierMap.get(trackId)), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetTrackBuffers()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ResetTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::clearTrackBuffers()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ClearTrackBuffers(), m_remoteSourceBufferIdentifier);
    m_totalTrackBufferSizeInBytes = 0;
}

void SourceBufferPrivateRemote::setAllTrackBuffersNeedRandomAccess()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetAllTrackBuffersNeedRandomAccess(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setGroupStartTimestamp(const MediaTime& timestamp)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestamp(timestamp), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setGroupStartTimestampToEndTimestamp()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestampToEndTimestamp(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetShouldGenerateTimestamps(shouldGenerateTimestamps), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ReenqueueMediaIfNeeded(currentMediaTime), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetTimestampOffsetInTrackBuffers()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ResetTimestampOffsetInTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::startChangingType()
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::StartChangingType(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setTimestampOffset(const MediaTime& timestampOffset)
{
    if (!m_gpuProcessConnection)
        return;

    SourceBufferPrivate::setTimestampOffset(timestampOffset);
    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetTimestampOffset(timestampOffset), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowStart(appendWindowStart), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowEnd(appendWindowEnd), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::seekToTime(const MediaTime& mediaTime)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SeekToTime(mediaTime), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::updateTrackIds(Vector<std::pair<AtomString, AtomString>>&& trackIdPairs)
{
    if (!m_gpuProcessConnection)
        return;

    auto identifierPairs = trackIdPairs.map([this](auto& trackIdPair) {
        ASSERT(m_prevTrackIdentifierMap.contains(trackIdPair.first));
        ASSERT(m_trackIdentifierMap.contains(trackIdPair.second));
        return std::pair { m_prevTrackIdentifierMap.take(trackIdPair.first), m_trackIdentifierMap.get(trackIdPair.second) };
    });

    m_gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::UpdateTrackIds(identifierPairs), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::bufferedSamplesForTrackId(const AtomString& trackId, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().sendWithAsyncReply(Messages::RemoteSourceBufferProxy::BufferedSamplesForTrackId(m_trackIdentifierMap.get(trackId)), [completionHandler = WTFMove(completionHandler)](auto&& samples) mutable {
        completionHandler(WTFMove(samples));
    }, m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::enqueuedSamplesForTrackID(const AtomString& trackId, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    if (!m_gpuProcessConnection)
        return;

    m_gpuProcessConnection->connection().sendWithAsyncReply(Messages::RemoteSourceBufferProxy::EnqueuedSamplesForTrackID(m_trackIdentifierMap.get(trackId)), [completionHandler = WTFMove(completionHandler)](auto&& samples) mutable {
        completionHandler(WTFMove(samples));
    }, m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&& segmentInfo, CompletionHandler<void(WebCore::SourceBufferPrivateClient::ReceiveResult)>&& completionHandler)
{
    if (!m_client || !m_mediaPlayerPrivate) {
        completionHandler(WebCore::SourceBufferPrivateClient::ReceiveResult::ClientDisconnected);
        return;
    }

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = segmentInfo.duration;

    m_prevTrackIdentifierMap.swap(m_trackIdentifierMap);
    segment.audioTracks.reserveInitialCapacity(segmentInfo.audioTracks.size());
    for (auto& audioTrack : segmentInfo.audioTracks) {
        SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
        info.track = m_mediaPlayerPrivate->audioTrackPrivateRemote(audioTrack.identifier);
        info.description = RemoteMediaDescription::create(audioTrack.description);
        segment.audioTracks.uncheckedAppend(info);
        m_trackIdentifierMap.add(info.track->id(), audioTrack.identifier);
    }

    segment.videoTracks.reserveInitialCapacity(segmentInfo.videoTracks.size());
    for (auto& videoTrack : segmentInfo.videoTracks) {
        SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
        info.track = m_mediaPlayerPrivate->videoTrackPrivateRemote(videoTrack.identifier);
        info.description = RemoteMediaDescription::create(videoTrack.description);
        segment.videoTracks.uncheckedAppend(info);
        m_trackIdentifierMap.add(info.track->id(), videoTrack.identifier);
    }

    segment.textTracks.reserveInitialCapacity(segmentInfo.textTracks.size());
    for (auto& textTrack : segmentInfo.textTracks) {
        SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info;
        info.track = m_mediaPlayerPrivate->textTrackPrivateRemote(textTrack.identifier);
        info.description = RemoteMediaDescription::create(textTrack.description);
        segment.textTracks.uncheckedAppend(info);
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

void SourceBufferPrivateRemote::sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::AppendResult appendResult, const PlatformTimeRanges& buffered, uint64_t totalTrackBufferSizeInBytes, const MediaTime& timestampOffset)
{
    setBufferedRanges(buffered);
    m_totalTrackBufferSizeInBytes = totalTrackBufferSizeInBytes;
    if (m_client) {
        setTimestampOffset(timestampOffset);
        m_client->sourceBufferPrivateAppendComplete(appendResult);
    }
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

uint64_t SourceBufferPrivateRemote::totalTrackBufferSizeInBytes() const
{
    return m_totalTrackBufferSizeInBytes;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
