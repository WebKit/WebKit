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
#include "RemoteSourceBufferProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "Connection.h"
#include "InitializationSegmentInfo.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SharedBufferReference.h"
#include "SourceBufferPrivateRemoteMessages.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/ContentType.h>
#include <WebCore/MediaDescription.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/VideoTrackPrivate.h>
#include <wtf/Scope.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteSourceBufferProxy> RemoteSourceBufferProxy::create(GPUConnectionToWebProcess& connectionToWebProcess, RemoteSourceBufferIdentifier identifier, Ref<SourceBufferPrivate>&& sourceBufferPrivate, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
{
    auto remoteSourceBufferProxy = adoptRef(*new RemoteSourceBufferProxy(connectionToWebProcess, identifier, WTFMove(sourceBufferPrivate), remoteMediaPlayerProxy));
    return remoteSourceBufferProxy;
}

RemoteSourceBufferProxy::RemoteSourceBufferProxy(GPUConnectionToWebProcess& connectionToWebProcess, RemoteSourceBufferIdentifier identifier, Ref<SourceBufferPrivate>&& sourceBufferPrivate, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_identifier(identifier)
    , m_sourceBufferPrivate(WTFMove(sourceBufferPrivate))
    , m_remoteMediaPlayerProxy(remoteMediaPlayerProxy)
{
    m_connectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
    m_sourceBufferPrivate->setClient(*this);
}

RemoteSourceBufferProxy::~RemoteSourceBufferProxy()
{
    m_sourceBufferPrivate->detach();
    disconnect();
}

void RemoteSourceBufferProxy::disconnect()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64());
    m_connectionToWebProcess = nullptr;
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&& segment)
{
    if (!m_remoteMediaPlayerProxy)
        return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);

    InitializationSegmentInfo segmentInfo;
    segmentInfo.duration = segment.duration;

    segmentInfo.audioTracks = segment.audioTracks.map([&](auto& audioTrackInfo) {
        auto id = audioTrackInfo.track->id();
        m_remoteMediaPlayerProxy->addRemoteAudioTrackProxy(*audioTrackInfo.track);
        m_mediaDescriptions.try_emplace(id, *audioTrackInfo.description);
        return InitializationSegmentInfo::TrackInformation { MediaDescriptionInfo(*audioTrackInfo.description), id };
    });

    segmentInfo.videoTracks = segment.videoTracks.map([&](auto& videoTrackInfo) {
        auto id = videoTrackInfo.track->id();
        m_remoteMediaPlayerProxy->addRemoteVideoTrackProxy(*videoTrackInfo.track);
        m_mediaDescriptions.try_emplace(id, *videoTrackInfo.description);
        return InitializationSegmentInfo::TrackInformation { MediaDescriptionInfo(*videoTrackInfo.description), id };
    });

    segmentInfo.textTracks = segment.textTracks.map([&](auto& textTrackInfo) {
        auto id = textTrackInfo.track->id();
        m_remoteMediaPlayerProxy->addRemoteTextTrackProxy(*textTrackInfo.track);
        m_mediaDescriptions.try_emplace(id, *textTrackInfo.description);
        return InitializationSegmentInfo::TrackInformation { MediaDescriptionInfo(*textTrackInfo.description), id };
    });

    if (!m_connectionToWebProcess)
        return MediaPromise::createAndReject(PlatformMediaError::IPCError);

    return m_connectionToWebProcess->connection().sendWithPromisedReply(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidReceiveInitializationSegment(segmentInfo), m_identifier)->whenSettled(RunLoop::current(), [](auto&& result) {
        return MediaPromise::createAndSettle(!result ? makeUnexpected(PlatformMediaError::IPCError) : WTFMove(*result));
    });
}

void RemoteSourceBufferProxy::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateHighestPresentationTimestampChanged(timestamp), m_identifier);
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateDurationChanged(const MediaTime& duration)
{
    if (!m_connectionToWebProcess)
        return MediaPromise::createAndReject(PlatformMediaError::IPCError);

    return m_connectionToWebProcess->connection().sendWithPromisedReply(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDurationChanged(duration), m_identifier)->whenSettled(RunLoop::current(), [](auto&& result) {
        return result ? MediaPromise::createAndResolve() : MediaPromise::createAndReject(PlatformMediaError::IPCError);
    });
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateBufferedChanged(const Vector<WebCore::PlatformTimeRanges>& trackRanges, uint64_t totalMemorySize)
{
    if (!m_connectionToWebProcess)
        return MediaPromise::createAndResolve();

    return m_connectionToWebProcess->connection().sendWithPromisedReply(Messages::SourceBufferPrivateRemote::SourceBufferPrivateBufferedChanged(trackRanges, totalMemorySize), m_identifier)->whenSettled(RunLoop::current(), [](auto&& result) {
        return result ? MediaPromise::createAndResolve() : MediaPromise::createAndReject(PlatformMediaError::IPCError);
    });
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidDropSample()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidDropSample(), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidReceiveRenderingError(errorCode), m_identifier);
}

void RemoteSourceBufferProxy::append(IPC::SharedBufferReference&& buffer, CompletionHandler<void(MediaPromise::Result, const MediaTime&)>&& completionHandler)
{
    auto sharedMemory = buffer.sharedCopy();
    if (!sharedMemory)
        return completionHandler(makeUnexpected(PlatformMediaError::MemoryError), m_sourceBufferPrivate->timestampOffset());

    auto handle = sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    if (handle && m_connectionToWebProcess)
        m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::TakeOwnershipOfMemory(WTFMove(*handle)), m_identifier);

    m_sourceBufferPrivate->append(sharedMemory->createSharedBuffer(buffer.size()))->whenSettled(RunLoop::current(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        completionHandler(WTFMove(result), m_sourceBufferPrivate->timestampOffset());
    });
}

void RemoteSourceBufferProxy::abort()
{
    m_sourceBufferPrivate->abort();
}

void RemoteSourceBufferProxy::resetParserState()
{
    m_sourceBufferPrivate->resetParserState();
}

void RemoteSourceBufferProxy::removedFromMediaSource()
{
    m_sourceBufferPrivate->removedFromMediaSource();
}

void RemoteSourceBufferProxy::setMediaSourceEnded(bool isEnded)
{
    m_sourceBufferPrivate->setMediaSourceEnded(isEnded);
}

void RemoteSourceBufferProxy::setActive(bool active)
{
    m_sourceBufferPrivate->setActive(active);
}

void RemoteSourceBufferProxy::canSwitchToType(const ContentType& contentType, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_sourceBufferPrivate->canSwitchToType(contentType));
}

void RemoteSourceBufferProxy::setMode(WebCore::SourceBufferAppendMode appendMode)
{
    m_sourceBufferPrivate->setMode(appendMode);
}

void RemoteSourceBufferProxy::startChangingType()
{
    m_sourceBufferPrivate->startChangingType();
}

void RemoteSourceBufferProxy::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime, CompletionHandler<void()>&& completionHandler)
{
    m_sourceBufferPrivate->removeCodedFrames(start, end, currentTime)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime, CompletionHandler<void(Vector<WebCore::PlatformTimeRanges>&&, uint64_t)>&& completionHandler)
{
    m_sourceBufferPrivate->evictCodedFrames(newDataSize, maximumBufferSize, currentTime);
    completionHandler(m_sourceBufferPrivate->trackBuffersRanges(), m_sourceBufferPrivate->totalTrackBufferSizeInBytes());
}

void RemoteSourceBufferProxy::addTrackBuffer(TrackID trackId)
{
    ASSERT(m_mediaDescriptions.contains(trackId));
    m_sourceBufferPrivate->addTrackBuffer(trackId, m_mediaDescriptions.find(trackId)->second.ptr());
}

void RemoteSourceBufferProxy::resetTrackBuffers()
{
    m_sourceBufferPrivate->resetTrackBuffers();
}

void RemoteSourceBufferProxy::clearTrackBuffers()
{
    m_sourceBufferPrivate->clearTrackBuffers();
}

void RemoteSourceBufferProxy::setAllTrackBuffersNeedRandomAccess()
{
    m_sourceBufferPrivate->setAllTrackBuffersNeedRandomAccess();
}

void RemoteSourceBufferProxy::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    m_sourceBufferPrivate->reenqueueMediaIfNeeded(currentMediaTime);
}

void RemoteSourceBufferProxy::setGroupStartTimestamp(const MediaTime& timestamp)
{
    m_sourceBufferPrivate->setGroupStartTimestamp(timestamp);
}

void RemoteSourceBufferProxy::setGroupStartTimestampToEndTimestamp()
{
    m_sourceBufferPrivate->setGroupStartTimestampToEndTimestamp();
}

void RemoteSourceBufferProxy::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    m_sourceBufferPrivate->setShouldGenerateTimestamps(shouldGenerateTimestamps);
}

void RemoteSourceBufferProxy::resetTimestampOffsetInTrackBuffers()
{
    m_sourceBufferPrivate->resetTimestampOffsetInTrackBuffers();
}

void RemoteSourceBufferProxy::setTimestampOffset(const MediaTime& timestampOffset)
{
    m_sourceBufferPrivate->setTimestampOffset(timestampOffset);
}

void RemoteSourceBufferProxy::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    m_sourceBufferPrivate->setAppendWindowStart(appendWindowStart);
}

void RemoteSourceBufferProxy::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    m_sourceBufferPrivate->setAppendWindowEnd(appendWindowEnd);
}

void RemoteSourceBufferProxy::computeSeekTime(const SeekTarget& target, CompletionHandler<void(SourceBufferPrivate::ComputeSeekPromise::Result&&)>&& completionHandler)
{
    m_sourceBufferPrivate->computeSeekTime(target)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::seekToTime(const MediaTime& time)
{
    m_sourceBufferPrivate->seekToTime(time);
}

void RemoteSourceBufferProxy::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIdPairs)
{
    if (!trackIdPairs.isEmpty())
        m_sourceBufferPrivate->updateTrackIds(WTFMove(trackIdPairs));
}

void RemoteSourceBufferProxy::bufferedSamplesForTrackId(TrackID trackId, CompletionHandler<void(WebCore::SourceBufferPrivate::SamplesPromise::Result&&)>&& completionHandler)
{
    m_sourceBufferPrivate->bufferedSamplesForTrackId(trackId)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::enqueuedSamplesForTrackID(TrackID trackId, CompletionHandler<void(WebCore::SourceBufferPrivate::SamplesPromise::Result&&)>&& completionHandler)
{
    m_sourceBufferPrivate->enqueuedSamplesForTrackID(trackId)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::memoryPressure(uint64_t maximumBufferSize, const MediaTime& currentTime)
{
    m_sourceBufferPrivate->memoryPressure(maximumBufferSize, currentTime);
}

void RemoteSourceBufferProxy::minimumUpcomingPresentationTimeForTrackID(TrackID trackID, CompletionHandler<void(MediaTime)>&& completionHandler)
{
    completionHandler(m_sourceBufferPrivate->minimumUpcomingPresentationTimeForTrackID(trackID));
}

void RemoteSourceBufferProxy::setMaximumQueueDepthForTrackID(TrackID trackID, uint64_t depth)
{
    m_sourceBufferPrivate->setMaximumQueueDepthForTrackID(trackID, depth);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
