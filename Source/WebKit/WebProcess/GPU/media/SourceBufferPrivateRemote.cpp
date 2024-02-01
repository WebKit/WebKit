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

Ref<SourceBufferPrivateRemote> SourceBufferPrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, MediaSourcePrivateRemote& mediaSourcePrivate, const MediaPlayerPrivateRemote& mediaPlayerPrivate)
{
    return adoptRef(*new SourceBufferPrivateRemote(gpuProcessConnection, remoteSourceBufferIdentifier, mediaSourcePrivate, mediaPlayerPrivate));
}

SourceBufferPrivateRemote::SourceBufferPrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, MediaSourcePrivateRemote& mediaSourcePrivate, const MediaPlayerPrivateRemote& mediaPlayerPrivate)
    : SourceBufferPrivate(mediaSourcePrivate)
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_remoteSourceBufferIdentifier(remoteSourceBufferIdentifier)
    , m_mediaPlayerPrivate(mediaPlayerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSourcePrivate.logger())
    , m_logIdentifier(mediaSourcePrivate.nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::SourceBufferPrivateRemote::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64(), *this);
}

SourceBufferPrivateRemote::~SourceBufferPrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection)
        return;

    gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::SourceBufferPrivateRemote::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64());
}

Ref<MediaPromise> SourceBufferPrivateRemote::append(Ref<SharedBuffer>&& data)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return MediaPromise::createAndReject(PlatformMediaError::IPCError);

    return gpuProcessConnection->connection().sendWithPromisedReply(Messages::RemoteSourceBufferProxy::Append(IPC::SharedBufferReference { WTFMove(data) }), m_remoteSourceBufferIdentifier)->whenSettled(RunLoop::current(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (!result)
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setTimestampOffset(std::get<MediaTime>(*result));
        return MediaPromise::createAndSettle(std::get<MediaPromise::Result>(*result));
    });
}

void SourceBufferPrivateRemote::takeOwnershipOfMemory(WebCore::SharedMemory::Handle&& bufferHandle)
{
    // Take ownership of shared memory and mark it as media-related memory.
    bufferHandle.takeOwnershipOfMemory(MemoryLedger::Media);
}

void SourceBufferPrivateRemote::abort()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    // When abort() is being issued; this will force the RemoteSourceBufferProxy to abort the current remote source buffer operation
    // The totalTrackBufferSizeInBytes will be recalculated after the next operation which allows for potentially having
    // m_totalTrackBufferSizeInBytes being temporarily stale as it won't be used until then.
    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::Abort(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetParserState()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ResetParserState(), m_remoteSourceBufferIdentifier);
}

Ref<MediaPromise> SourceBufferPrivateRemote::appendInternal(Ref<SharedBuffer>&&)
{
    ASSERT_NOT_REACHED();
    return MediaPromise::createAndReject(PlatformMediaError::IPCError);
}

void SourceBufferPrivateRemote::resetParserStateInternal()
{
    ASSERT_NOT_REACHED();
}

void SourceBufferPrivateRemote::removedFromMediaSource()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::RemovedFromMediaSource(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setActive(bool active)
{
    SourceBufferPrivate::setActive(active);

    auto mediaSource = m_mediaSource.get();
    if (!mediaSource)
        return;

    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetActive(active), m_remoteSourceBufferIdentifier);
}

bool SourceBufferPrivateRemote::canSwitchToType(const ContentType& contentType)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return false;

    auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::CanSwitchToType(contentType), m_remoteSourceBufferIdentifier);
    auto [canSwitch] = sendResult.takeReplyOr(false);
    return canSwitch;
}

void SourceBufferPrivateRemote::setMediaSourceEnded(bool isEnded)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetMediaSourceEnded(isEnded), m_remoteSourceBufferIdentifier);
    SourceBufferPrivate::setMediaSourceEnded(isEnded);
}

void SourceBufferPrivateRemote::setMode(SourceBufferAppendMode mode)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetMode(mode), m_remoteSourceBufferIdentifier);
}

Ref<MediaPromise> SourceBufferPrivateRemote::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return MediaPromise::createAndReject(PlatformMediaError::IPCError);

    return gpuProcessConnection->connection().sendWithPromisedReply(
        Messages::RemoteSourceBufferProxy::RemoveCodedFrames(start, end, currentMediaTime), m_remoteSourceBufferIdentifier)->whenSettled(RunLoop::current(), [] (auto&& result) mutable {
            if (!result)
                return MediaPromise::createAndReject(PlatformMediaError::IPCError);
            return MediaPromise::createAndResolve();
        });
}

void SourceBufferPrivateRemote::evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::EvictCodedFrames(newDataSize, maximumBufferSize, currentTime), m_remoteSourceBufferIdentifier);
    if (sendResult.succeeded()) {
        if (RefPtr client = this->client()) {
            Vector<PlatformTimeRanges> trackBufferRanges;
            std::tie(trackBufferRanges, m_totalTrackBufferSizeInBytes) = sendResult.takeReply();
            client->sourceBufferPrivateBufferedChanged(trackBufferRanges, m_totalTrackBufferSizeInBytes);
        }
    }
}

void SourceBufferPrivateRemote::addTrackBuffer(TrackID trackId, RefPtr<MediaDescription>&&)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::AddTrackBuffer(trackId), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetTrackBuffers()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ResetTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::clearTrackBuffers(bool)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ClearTrackBuffers(), m_remoteSourceBufferIdentifier);
    m_totalTrackBufferSizeInBytes = 0;
}

void SourceBufferPrivateRemote::setAllTrackBuffersNeedRandomAccess()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetAllTrackBuffersNeedRandomAccess(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setGroupStartTimestamp(const MediaTime& timestamp)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestamp(timestamp), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setGroupStartTimestampToEndTimestamp()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestampToEndTimestamp(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetShouldGenerateTimestamps(shouldGenerateTimestamps), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ReenqueueMediaIfNeeded(currentMediaTime), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::resetTimestampOffsetInTrackBuffers()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::ResetTimestampOffsetInTrackBuffers(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::startChangingType()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::StartChangingType(), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setTimestampOffset(const MediaTime& timestampOffset)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    SourceBufferPrivate::setTimestampOffset(timestampOffset);
    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetTimestampOffset(timestampOffset), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowStart(appendWindowStart), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowEnd(appendWindowEnd), m_remoteSourceBufferIdentifier);
}

Ref<SourceBufferPrivate::ComputeSeekPromise> SourceBufferPrivateRemote::computeSeekTime(const WebCore::SeekTarget& target)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return ComputeSeekPromise::createAndReject(PlatformMediaError::IPCError);

    return gpuProcessConnection->connection().sendWithPromisedReply(Messages::RemoteSourceBufferProxy::ComputeSeekTime(target), m_remoteSourceBufferIdentifier)->whenSettled(RunLoop::current(), [](auto&& result) {
        return result ? ComputeSeekPromise::createAndSettle(*result) : ComputeSeekPromise::createAndReject(PlatformMediaError::IPCError);
    });
}

void SourceBufferPrivateRemote::seekToTime(const MediaTime& time)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SeekToTime(time), m_remoteSourceBufferIdentifier);
}

void SourceBufferPrivateRemote::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIdPairs)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::UpdateTrackIds(WTFMove(trackIdPairs)), m_remoteSourceBufferIdentifier);
}

Ref<SourceBufferPrivate::SamplesPromise> SourceBufferPrivateRemote::bufferedSamplesForTrackId(TrackID trackId)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return SamplesPromise::createAndResolve(Vector<String> { });

    return gpuProcessConnection->connection().sendWithPromisedReply(Messages::RemoteSourceBufferProxy::BufferedSamplesForTrackId(trackId), m_remoteSourceBufferIdentifier)->whenSettled(RunLoop::current(), [](auto&& result) {
        if (!result)
            return SamplesPromise::createAndResolve(Vector<String> { });
        return SamplesPromise::createAndSettle(WTFMove(*result));
    });
}

Ref<SourceBufferPrivate::SamplesPromise> SourceBufferPrivateRemote::enqueuedSamplesForTrackID(TrackID trackId)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return SamplesPromise::createAndResolve(Vector<String> { });

    return gpuProcessConnection->connection().sendWithPromisedReply(Messages::RemoteSourceBufferProxy::EnqueuedSamplesForTrackID(trackId), m_remoteSourceBufferIdentifier)->whenSettled(RunLoop::current(), [](auto&& result) {
        if (!result)
            return SamplesPromise::createAndResolve(Vector<String> { });
        return SamplesPromise::createAndSettle(WTFMove(*result));
    });
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&& segmentInfo, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&& completionHandler)
{
    RefPtr client = this->client();
    if (!client || !m_mediaPlayerPrivate) {
        completionHandler(makeUnexpected(WebCore::PlatformMediaError::ClientDisconnected));
        return;
    }

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = segmentInfo.duration;

    segment.audioTracks = WTF::map(segmentInfo.audioTracks, [&](auto& audioTrack) {
        SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info {
            RemoteMediaDescription::create(audioTrack.description),
            m_mediaPlayerPrivate->audioTrackPrivateRemote(audioTrack.id)
        };
        return info;
    });

    segment.videoTracks = WTF::map(segmentInfo.videoTracks, [&](auto& videoTrack) {
        SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info {
            RemoteMediaDescription::create(videoTrack.description),
            m_mediaPlayerPrivate->videoTrackPrivateRemote(videoTrack.id)
        };
        return info;
    });

    segment.textTracks = WTF::map(segmentInfo.textTracks, [&](auto& textTrack) {
        SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info {
            RemoteMediaDescription::create(textTrack.description),
            m_mediaPlayerPrivate->textTrackPrivateRemote(textTrack.id)
        };
        return info;
    });

    client->sourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segment))->whenSettled(RunLoop::current(), WTFMove(completionHandler));
}

void SourceBufferPrivateRemote::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    if (RefPtr client = this->client())
        client->sourceBufferPrivateHighestPresentationTimestampChanged(timestamp);
}

void SourceBufferPrivateRemote::sourceBufferPrivateDurationChanged(const MediaTime& duration, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr client = this->client())
        client->sourceBufferPrivateDurationChanged(duration)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
    else
        completionHandler();
}

void SourceBufferPrivateRemote::sourceBufferPrivateBufferedChanged(Vector<WebCore::PlatformTimeRanges>&& trackBuffersRanges, uint64_t extraMemory, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr client = this->client())
        client->sourceBufferPrivateBufferedChanged(WTFMove(trackBuffersRanges), extraMemory)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
    else
        completionHandler();
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidDropSample()
{
    if (RefPtr client = this->client())
        client->sourceBufferPrivateDidDropSample();
}

void SourceBufferPrivateRemote::sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode)
{
    if (RefPtr client = this->client())
        client->sourceBufferPrivateDidReceiveRenderingError(errorCode);
}

uint64_t SourceBufferPrivateRemote::totalTrackBufferSizeInBytes() const
{
    return m_totalTrackBufferSizeInBytes;
}

void SourceBufferPrivateRemote::memoryPressure(uint64_t maximumBufferSize, const MediaTime& currentTime)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::MemoryPressure(maximumBufferSize, currentTime), m_remoteSourceBufferIdentifier);
}

MediaTime SourceBufferPrivateRemote::minimumUpcomingPresentationTimeForTrackID(TrackID trackID)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return MediaTime::invalidTime();
    auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::MinimumUpcomingPresentationTimeForTrackID(trackID), m_remoteSourceBufferIdentifier);

    return std::get<0>(sendResult.takeReplyOr(MediaTime::invalidTime()));
}

void SourceBufferPrivateRemote::setMaximumQueueDepthForTrackID(TrackID trackID, uint64_t depth)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(
        Messages::RemoteSourceBufferProxy::SetMaximumQueueDepthForTrackID(trackID, depth), m_remoteSourceBufferIdentifier);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
