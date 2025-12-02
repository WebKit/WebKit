/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SharedBufferReference.h"
#include "SourceBufferPrivateRemoteMessageReceiverMessages.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/ContentType.h>
#include <WebCore/MediaDescription.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <WebCore/VideoTrackPrivate.h>
#include <wtf/RefPtr.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection())

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSourceBufferProxy);

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
    connectionToWebProcess.messageReceiverMap().addMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
    protectedSourceBufferPrivate()->setClient(*this);
}

RemoteSourceBufferProxy::~RemoteSourceBufferProxy()
{
    disconnect();
}

void RemoteSourceBufferProxy::setMediaPlayer(RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
{
    m_remoteMediaPlayerProxy = remoteMediaPlayerProxy;
}

RefPtr<IPC::Connection> RemoteSourceBufferProxy::connection() const
{
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return nullptr;
    return &connection->connection();
}

void RemoteSourceBufferProxy::disconnect()
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64());
    m_connectionToWebProcess = nullptr;
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&& segment)
{
    ASSERT(isMainRunLoop());

    RefPtr remoteMediaPlayerProxy { m_remoteMediaPlayerProxy.get() };

    auto segmentInfo = createInitializationSegmentInfo(WTFMove(segment));
    if (!segmentInfo)
        return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);

    ASSERT(remoteMediaPlayerProxy);
    // We need to wait for the CP's MediaPlayerRemote to have created all the tracks
    return remoteMediaPlayerProxy->commitAllTransactions()->whenSettled(RunLoop::currentSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, segmentInfo = WTFMove(*segmentInfo)](auto&& result) mutable -> Ref<MediaPromise> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);
        RefPtr connection = protectedThis->m_connectionToWebProcess.get();
        if (!result || !connection)
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);

        return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segmentInfo)), protectedThis->m_identifier);
    });
}

void RemoteSourceBufferProxy::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    if (RefPtr connection = m_connectionToWebProcess.get())
        connection->connection().send(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateHighestPresentationTimestampChanged(timestamp), m_identifier);
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateDurationChanged(const MediaTime& duration)
{
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return MediaPromise::createAndReject(PlatformMediaError::IPCError);

    return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateDurationChanged(duration), m_identifier);
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateBufferedChanged(const Vector<WebCore::PlatformTimeRanges>& trackRanges)
{
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return MediaPromise::createAndResolve();

    return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateBufferedChanged(trackRanges), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidDropSample()
{
    if (RefPtr connection = m_connectionToWebProcess.get())
        connection->connection().send(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateDidDropSample(), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateEvictionDataChanged(const WebCore::SourceBufferEvictionData& evictionData)
{
    if (RefPtr connection = m_connectionToWebProcess.get())
        connection->connection().send(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateEvictionDataChanged(evictionData), m_identifier);
}

void RemoteSourceBufferProxy::append(IPC::SharedBufferReference&& buffer, CompletionHandler<void(MediaPromise::Result, const MediaTime&)>&& completionHandler)
{
    auto sharedMemory = buffer.sharedCopy();
    Ref sourceBufferPrivate = m_sourceBufferPrivate;

    if (!sharedMemory)
        return completionHandler(makeUnexpected(PlatformMediaError::MemoryError), sourceBufferPrivate->timestampOffset());

    auto handle = sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    RefPtr connection = m_connectionToWebProcess.get();
    if (handle && connection)
        connection->connection().send(Messages::SourceBufferPrivateRemoteMessageReceiver::TakeOwnershipOfMemory(WTFMove(*handle)), m_identifier);

    sourceBufferPrivate->append(sharedMemory->createSharedBuffer(buffer.size()))->whenSettled(RunLoop::currentSingleton(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        completionHandler(WTFMove(result), protectedSourceBufferPrivate()->timestampOffset());
    });
}

void RemoteSourceBufferProxy::abort()
{
    protectedSourceBufferPrivate()->abort();
}

void RemoteSourceBufferProxy::resetParserState()
{
    protectedSourceBufferPrivate()->resetParserState();
}

void RemoteSourceBufferProxy::removedFromMediaSource()
{
    protectedSourceBufferPrivate()->removedFromMediaSource();
}

void RemoteSourceBufferProxy::setMediaSourceEnded(bool isEnded)
{
    protectedSourceBufferPrivate()->setMediaSourceEnded(isEnded);
}

void RemoteSourceBufferProxy::setActive(bool active)
{
    protectedSourceBufferPrivate()->setActive(active);
}

void RemoteSourceBufferProxy::canSwitchToType(const ContentType& contentType, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(protectedSourceBufferPrivate()->canSwitchToType(contentType));
}

void RemoteSourceBufferProxy::setMode(WebCore::SourceBufferAppendMode appendMode)
{
    protectedSourceBufferPrivate()->setMode(appendMode);
}

void RemoteSourceBufferProxy::startChangingType()
{
    protectedSourceBufferPrivate()->startChangingType();
}

void RemoteSourceBufferProxy::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime, CompletionHandler<void()>&& completionHandler)
{
    protectedSourceBufferPrivate()->removeCodedFrames(start, end, currentTime)->whenSettled(RunLoop::currentSingleton(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::evictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime, CompletionHandler<void(Vector<WebCore::PlatformTimeRanges>&&, WebCore::SourceBufferEvictionData&&)>&& completionHandler)
{
    Ref sourceBufferPrivate { m_sourceBufferPrivate };
    sourceBufferPrivate->evictCodedFrames(newDataSize, currentTime);
    completionHandler(sourceBufferPrivate->trackBuffersRanges(), sourceBufferPrivate->evictionData());
}

void RemoteSourceBufferProxy::asyncEvictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime)
{
    protectedSourceBufferPrivate()->asyncEvictCodedFrames(newDataSize, currentTime);
}

void RemoteSourceBufferProxy::addTrackBuffer(TrackID trackId)
{
    MESSAGE_CHECK(m_mediaDescriptions.contains(trackId));
    protectedSourceBufferPrivate()->addTrackBuffer(trackId, m_mediaDescriptions.find(trackId)->second.ptr());
}

void RemoteSourceBufferProxy::resetTrackBuffers()
{
    protectedSourceBufferPrivate()->resetTrackBuffers();
}

void RemoteSourceBufferProxy::clearTrackBuffers()
{
    protectedSourceBufferPrivate()->clearTrackBuffers();
}

void RemoteSourceBufferProxy::setAllTrackBuffersNeedRandomAccess()
{
    protectedSourceBufferPrivate()->setAllTrackBuffersNeedRandomAccess();
}

void RemoteSourceBufferProxy::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    protectedSourceBufferPrivate()->reenqueueMediaIfNeeded(currentMediaTime);
}

void RemoteSourceBufferProxy::setGroupStartTimestamp(const MediaTime& timestamp)
{
    protectedSourceBufferPrivate()->setGroupStartTimestamp(timestamp);
}

void RemoteSourceBufferProxy::setGroupStartTimestampToEndTimestamp()
{
    protectedSourceBufferPrivate()->setGroupStartTimestampToEndTimestamp();
}

void RemoteSourceBufferProxy::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    protectedSourceBufferPrivate()->setShouldGenerateTimestamps(shouldGenerateTimestamps);
}

void RemoteSourceBufferProxy::resetTimestampOffsetInTrackBuffers()
{
    protectedSourceBufferPrivate()->resetTimestampOffsetInTrackBuffers();
}

void RemoteSourceBufferProxy::setTimestampOffset(const MediaTime& timestampOffset)
{
    protectedSourceBufferPrivate()->setTimestampOffset(timestampOffset);
}

void RemoteSourceBufferProxy::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    protectedSourceBufferPrivate()->setAppendWindowStart(appendWindowStart);
}

void RemoteSourceBufferProxy::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    protectedSourceBufferPrivate()->setAppendWindowEnd(appendWindowEnd);
}

void RemoteSourceBufferProxy::setMaximumBufferSize(uint64_t size, CompletionHandler<void()>&& completionHandler)
{
    protectedSourceBufferPrivate()->setMaximumBufferSize(size)->whenSettled(RunLoop::currentSingleton(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::computeSeekTime(const SeekTarget& target, CompletionHandler<void(SourceBufferPrivate::ComputeSeekPromise::Result&&)>&& completionHandler)
{
    protectedSourceBufferPrivate()->computeSeekTime(target)->whenSettled(RunLoop::currentSingleton(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIdPairs)
{
    if (!trackIdPairs.isEmpty())
        protectedSourceBufferPrivate()->updateTrackIds(WTFMove(trackIdPairs));
}

void RemoteSourceBufferProxy::bufferedSamplesForTrackId(TrackID trackId, CompletionHandler<void(WebCore::SourceBufferPrivate::SamplesPromise::Result&&)>&& completionHandler)
{
    protectedSourceBufferPrivate()->bufferedSamplesForTrackId(trackId)->whenSettled(RunLoop::currentSingleton(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::enqueuedSamplesForTrackID(TrackID trackId, CompletionHandler<void(WebCore::SourceBufferPrivate::SamplesPromise::Result&&)>&& completionHandler)
{
    protectedSourceBufferPrivate()->enqueuedSamplesForTrackID(trackId)->whenSettled(RunLoop::currentSingleton(), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::memoryPressure(const MediaTime& currentTime)
{
    protectedSourceBufferPrivate()->memoryPressure(currentTime);
}

void RemoteSourceBufferProxy::minimumUpcomingPresentationTimeForTrackID(TrackID trackID, CompletionHandler<void(MediaTime)>&& completionHandler)
{
    completionHandler(protectedSourceBufferPrivate()->minimumUpcomingPresentationTimeForTrackID(trackID));
}

void RemoteSourceBufferProxy::setMaximumQueueDepthForTrackID(TrackID trackID, uint64_t depth)
{
    protectedSourceBufferPrivate()->setMaximumQueueDepthForTrackID(trackID, depth);
}

void RemoteSourceBufferProxy::detach()
{
    protectedSourceBufferPrivate()->detach();
}

void RemoteSourceBufferProxy::attach()
{
    protectedSourceBufferPrivate()->attach();
}

Ref<MediaPromise> RemoteSourceBufferProxy::sourceBufferPrivateDidAttach(InitializationSegment&& segment)
{
    ASSERT(isMainRunLoop());

    RefPtr remoteMediaPlayerProxy { m_remoteMediaPlayerProxy.get() };

    auto segmentInfo = createInitializationSegmentInfo(WTFMove(segment));
    if (!segmentInfo)
        return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);

    ASSERT(remoteMediaPlayerProxy);
    // We need to wait for the CP's MediaPlayerRemote to have created all the tracks
    return remoteMediaPlayerProxy->commitAllTransactions()->whenSettled(RunLoop::currentSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, segmentInfo = WTFMove(*segmentInfo)](auto&& result) mutable -> Ref<MediaPromise> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);
        RefPtr connection = protectedThis->m_connectionToWebProcess.get();
        if (!result || !connection)
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);

        return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::SourceBufferPrivateRemoteMessageReceiver::SourceBufferPrivateDidAttach(WTFMove(segmentInfo)), protectedThis->m_identifier);
    });
}

std::optional<InitializationSegmentInfo> RemoteSourceBufferProxy::createInitializationSegmentInfo(InitializationSegment&& segment)
{
    RefPtr remoteMediaPlayerProxy { m_remoteMediaPlayerProxy.get() };
    if (!remoteMediaPlayerProxy)
        return { };

    InitializationSegmentInfo segmentInfo;
    segmentInfo.duration = segment.duration;

    segmentInfo.audioTracks = segment.audioTracks.map([&](const InitializationSegment::AudioTrackInformation& audioTrackInfo) {
        RefPtr track = audioTrackInfo.track;
        auto id = track->id();
        remoteMediaPlayerProxy->addRemoteAudioTrackProxy(*track);
        m_mediaDescriptions.try_emplace(id, *audioTrackInfo.protectedDescription());
        return InitializationSegmentInfo::TrackInformation { MediaDescriptionInfo(*audioTrackInfo.description), id };
    });

    segmentInfo.videoTracks = segment.videoTracks.map([&](const InitializationSegment::VideoTrackInformation& videoTrackInfo) {
        RefPtr track = videoTrackInfo.track;
        auto id = track->id();
        remoteMediaPlayerProxy->addRemoteVideoTrackProxy(*track);
        m_mediaDescriptions.try_emplace(id, *videoTrackInfo.protectedDescription());
        return InitializationSegmentInfo::TrackInformation { MediaDescriptionInfo(*videoTrackInfo.description), id };
    });

    segmentInfo.textTracks = segment.textTracks.map([&](const InitializationSegment::TextTrackInformation& textTrackInfo) {
        RefPtr track = textTrackInfo.track;
        auto id = track->id();
        remoteMediaPlayerProxy->addRemoteTextTrackProxy(*track);
        m_mediaDescriptions.try_emplace(id, *textTrackInfo.protectedDescription());
        return InitializationSegmentInfo::TrackInformation { MediaDescriptionInfo(*textTrackInfo.description), id };
    });

    return segmentInfo;
}

std::optional<SharedPreferencesForWebProcess> RemoteSourceBufferProxy::sharedPreferencesForWebProcess() const
{
    if (auto connectionToWebProcess = m_connectionToWebProcess.get())
        return connectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

void RemoteSourceBufferProxy::connectionToWebProcessClosed()
{
    ASSERT(isMainRunLoop());
    disconnect();
}

#undef MESSAGE_CHECK

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
