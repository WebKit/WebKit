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
#include "SourceBufferPrivateRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "InitializationSegmentInfo.h"
#include "Logging.h"
#include "MediaPlayerPrivateRemote.h"
#include "MediaSourcePrivateRemote.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SharedBufferReference.h"
#include "SourceBufferPrivateRemoteMessageReceiverMessages.h"
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/Locker.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(SourceBufferPrivateRemote);

WorkQueue& SourceBufferPrivateRemote::queueSingleton()
{
    return MediaSourcePrivateRemote::queueSingleton();
}

void SourceBufferPrivateRemote::ensureWeakOnDispatcher(Function<void(SourceBufferPrivateRemote&)>&& function)
{
    auto weakWrapper = [function = WTFMove(function), weakThis = ThreadSafeWeakPtr(*this)] {
        if (RefPtr protectedThis = weakThis.get()) {
            auto gpuProcessConnection = protectedThis->m_gpuProcessConnection.get();
            if (!gpuProcessConnection || !protectedThis->isGPURunning())
                return;
            function(*protectedThis);
        }
    };
    ensureOnDispatcher(WTFMove(weakWrapper));
}

Ref<SourceBufferPrivateRemote> SourceBufferPrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, MediaSourcePrivateRemote& mediaSourcePrivate)
{
    return adoptRef(*new SourceBufferPrivateRemote(gpuProcessConnection, remoteSourceBufferIdentifier, mediaSourcePrivate));
}

SourceBufferPrivateRemote::SourceBufferPrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, MediaSourcePrivateRemote& mediaSourcePrivate)
    : SourceBufferPrivate(mediaSourcePrivate, queueSingleton())
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_receiver(MessageReceiver::create(*this))
    , m_remoteSourceBufferIdentifier(remoteSourceBufferIdentifier)
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSourcePrivate.logger())
    , m_logIdentifier(mediaSourcePrivate.nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    gpuProcessConnection.connection().addWorkQueueMessageReceiver(Messages::SourceBufferPrivateRemoteMessageReceiver::messageReceiverName(), queueSingleton(), m_receiver, m_remoteSourceBufferIdentifier.toUInt64());
}

SourceBufferPrivateRemote::~SourceBufferPrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->connection().removeWorkQueueMessageReceiver(Messages::SourceBufferPrivateRemoteMessageReceiver::messageReceiverName(), m_remoteSourceBufferIdentifier.toUInt64());
}

Ref<MediaPromise> SourceBufferPrivateRemote::append(Ref<SharedBuffer>&& data)
{
    return invokeAsync(m_dispatcher, [protectedThis = Ref { *this }, this, data = WTFMove(data)]() mutable -> Ref<MediaPromise> {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);

        return sendWithPromisedReply(Messages::RemoteSourceBufferProxy::Append(IPC::SharedBufferReference { WTFMove(data) }))->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
            if (!result)
                return MediaPromise::createAndReject(PlatformMediaError::IPCError);
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setTimestampOffset(std::get<MediaTime>(*result));
            return MediaPromise::createAndSettle(std::get<MediaPromise::Result>(*result));
        });
    });
}

void SourceBufferPrivateRemote::abort()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        // When abort() is being issued; this will force the RemoteSourceBufferProxy to abort the current remote source buffer operation
        // The totalTrackBufferSizeInBytes will be recalculated after the next operation which allows for potentially having
        // m_totalTrackBufferSizeInBytes being temporarily stale as it won't be used until then.
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::Abort());
    });
}

void SourceBufferPrivateRemote::resetParserState()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::ResetParserState());
    });
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
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.m_removed = true;
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::RemovedFromMediaSource());
    });
}

void SourceBufferPrivateRemote::setActive(bool active)
{
    // Called from the SourceBuffer's dispatcher
    ensureWeakOnDispatcher([active](auto& buffer) {
        auto mediaSource = buffer.m_mediaSource.get();
        if (!mediaSource)
            return;

        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetActive(active));
    });
}

bool SourceBufferPrivateRemote::canSwitchToType(const ContentType& contentType)
{
    bool canSwitch = false;
    // FIXME: Uses a new Connection for remote playback, and not the main GPUProcessConnection's one.
    // FIXME: m_mimeTypeCache is a main-thread only object.
    callOnMainRunLoopAndWait([&, contentTypeString = contentType.raw().isolatedCopy()] {
        RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
        if (gpuProcessConnection && isGPURunning()) {
            ContentType contentType { contentTypeString };
            auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::CanSwitchToType(WTFMove(contentType)), m_remoteSourceBufferIdentifier);
            std::tie(canSwitch) = sendResult.takeReplyOr(false);
        }
    });
    return canSwitch;
}

void SourceBufferPrivateRemote::setMediaSourceEnded(bool isEnded)
{
    ensureWeakOnDispatcher([isEnded](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetMediaSourceEnded(isEnded));
    });
}

void SourceBufferPrivateRemote::setMode(SourceBufferAppendMode mode)
{
    ensureWeakOnDispatcher([mode](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetMode(mode));
    });
}

Ref<MediaPromise> SourceBufferPrivateRemote::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime)
{
    return invokeAsync(m_dispatcher, [protectedThis = Ref { *this }, this, start, end, currentMediaTime]() mutable -> Ref<MediaPromise> {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);

        return sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteSourceBufferProxy::RemoveCodedFrames(start, end, currentMediaTime));
    });
}

bool SourceBufferPrivateRemote::evictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime)
{
    if (canAppend(newDataSize)) {
        if (!isBufferFullFor(newDataSize))
            return false;
        // The buffer is full, but we will be able to evict the content prior appending.
        ensureWeakOnDispatcher([newDataSize, currentTime](auto& buffer) {
            buffer.sendToProxy(Messages::RemoteSourceBufferProxy::AsyncEvictCodedFrames(newDataSize, currentTime));
        });
        return false;
    }

    // FIXME: Uses a new Connection for remote playback, and not the main GPUProcessConnection's one.
    callOnMainRunLoopAndWait([&] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;

        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::EvictCodedFrames(newDataSize, currentTime), m_remoteSourceBufferIdentifier);
        if (sendResult.succeeded()) {
            if (auto client = this->client()) {
                Vector<PlatformTimeRanges> trackBufferRanges;
                {
                    Locker locker { m_lock };
                    std::tie(trackBufferRanges, m_evictionData) = sendResult.takeReply();
                }
                client->sourceBufferPrivateBufferedChanged(trackBufferRanges);
            }
        }
    });
    return isBufferFullFor(newDataSize);
}

void SourceBufferPrivateRemote::addTrackBuffer(TrackID trackId, RefPtr<MediaDescription>&&)
{
    ensureWeakOnDispatcher([trackId](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::AddTrackBuffer(trackId));
    });
}

void SourceBufferPrivateRemote::resetTrackBuffers()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::ResetTrackBuffers());
    });
}

void SourceBufferPrivateRemote::clearTrackBuffers(bool)
{
    {
        Locker locker { m_lock };
        m_evictionData.clear();
    }
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::ClearTrackBuffers());
    });
}

void SourceBufferPrivateRemote::setAllTrackBuffersNeedRandomAccess()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetAllTrackBuffersNeedRandomAccess());
    });
}

void SourceBufferPrivateRemote::setGroupStartTimestamp(const MediaTime& timestamp)
{
    ensureWeakOnDispatcher([timestamp](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetGroupStartTimestamp(timestamp));
    });
}

void SourceBufferPrivateRemote::setGroupStartTimestampToEndTimestamp()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetGroupStartTimestampToEndTimestamp());
    });
}

void SourceBufferPrivateRemote::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    ensureWeakOnDispatcher([shouldGenerateTimestamps](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetShouldGenerateTimestamps(shouldGenerateTimestamps));
    });
}

void SourceBufferPrivateRemote::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    ensureWeakOnDispatcher([currentMediaTime](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::ReenqueueMediaIfNeeded(currentMediaTime));
    });
}

void SourceBufferPrivateRemote::resetTimestampOffsetInTrackBuffers()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::ResetTimestampOffsetInTrackBuffers());
    });
}

void SourceBufferPrivateRemote::startChangingType()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::StartChangingType());
    });
}

void SourceBufferPrivateRemote::setTimestampOffset(const MediaTime& timestampOffset)
{
    // Called from the SourceBuffer's dispatcher
    SourceBufferPrivate::setTimestampOffset(timestampOffset);
    ensureWeakOnDispatcher([timestampOffset](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetTimestampOffset(timestampOffset));
    });
}

void SourceBufferPrivateRemote::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    ensureWeakOnDispatcher([appendWindowStart](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetAppendWindowStart(appendWindowStart));
    });
}

void SourceBufferPrivateRemote::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    ensureWeakOnDispatcher([appendWindowEnd](auto& buffer) {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::SetAppendWindowEnd(appendWindowEnd));
    });
}

Ref<GenericPromise> SourceBufferPrivateRemote::setMaximumBufferSize(size_t size)
{
    if (m_maximumBufferSize.exchange(size) == size)
        return GenericPromise::createAndResolve();
    GenericPromise::AutoRejectProducer producer;
    Ref promise = producer.promise();
    ensureWeakOnDispatcher([size, producer = WTFMove(producer)](auto& buffer) mutable {
        buffer.sendWithPromisedReply(Messages::RemoteSourceBufferProxy::SetMaximumBufferSize(size))->chainTo(WTFMove(producer));
    });
    return promise;
}

Ref<SourceBufferPrivate::ComputeSeekPromise> SourceBufferPrivateRemote::computeSeekTime(const WebCore::SeekTarget& target)
{
    return invokeAsync(m_dispatcher, [protectedThis = Ref { *this }, this, target]() -> Ref<ComputeSeekPromise> {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return ComputeSeekPromise::createAndReject(PlatformMediaError::IPCError);

        return sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteSourceBufferProxy::ComputeSeekTime(target));
    });
}

void SourceBufferPrivateRemote::seekToTime(const MediaTime& time)
{
    ASSERT_NOT_REACHED();
}

void SourceBufferPrivateRemote::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIDPairs)
{
    ensureWeakOnDispatcher([trackIDPairs = WTFMove(trackIDPairs)](auto& buffer) mutable {
        buffer.sendToProxy(Messages::RemoteSourceBufferProxy::UpdateTrackIds(WTFMove(trackIDPairs)));
    });
}

Ref<SourceBufferPrivate::SamplesPromise> SourceBufferPrivateRemote::bufferedSamplesForTrackId(TrackID trackID)
{
    return invokeAsync(m_dispatcher, [protectedThis = Ref { *this }, this, trackID]() -> Ref<SamplesPromise> {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return SamplesPromise::createAndReject(PlatformMediaError::IPCError);

        return sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteSourceBufferProxy::BufferedSamplesForTrackId(trackID));
    });
}

Ref<SourceBufferPrivate::SamplesPromise> SourceBufferPrivateRemote::enqueuedSamplesForTrackID(TrackID trackID)
{
    return invokeAsync(m_dispatcher, [protectedThis = Ref { *this }, this, trackID]() -> Ref<SamplesPromise> {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return SamplesPromise::createAndReject(PlatformMediaError::IPCError);

        return sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteSourceBufferProxy::EnqueuedSamplesForTrackID(trackID));
    });
}

RefPtr<SourceBufferPrivateClient> SourceBufferPrivateRemote::MessageReceiver::client() const
{
    if (RefPtr parent = m_parent.get()) {
        if (RefPtr client = parent->client())
            return client;
    }
    return nullptr;
}

void SourceBufferPrivateRemote::MessageReceiver::takeOwnershipOfMemory(WebKit::SharedMemory::Handle&& bufferHandle)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    // Take ownership of shared memory and mark it as media-related memory.
    bufferHandle.takeOwnershipOfMemory(MemoryLedger::Media);
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&& segmentInfo, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    RefPtr parent = m_parent.get();
    auto client = this->client();
    if (!client)
        return completionHandler(makeUnexpected(WebCore::PlatformMediaError::SourceRemoved));

    ASSERT(parent);

    RefPtr mediaPlayer = parent->player();
    if (!mediaPlayer)
        return completionHandler(makeUnexpected(WebCore::PlatformMediaError::SourceRemoved));

    client->sourceBufferPrivateDidReceiveInitializationSegment(createInitializationSegment(*mediaPlayer, WTFMove(segmentInfo)))->whenSettled(parent->queueSingleton(), WTFMove(completionHandler));
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateEvictionDataChanged(SourceBufferEvictionData&& evictionData)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    if (RefPtr parent = m_parent.get()) {
        Locker locker { parent->m_lock };
        parent->m_evictionData = WTFMove(evictionData);
    }
    if (auto client = this->client())
        client->sourceBufferPrivateEvictionDataChanged(WTFMove(evictionData));
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    if (auto client = this->client())
        client->sourceBufferPrivateHighestPresentationTimestampChanged(timestamp);
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDurationChanged(const MediaTime& duration, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    if (auto client = this->client()) {
        client->sourceBufferPrivateDurationChanged(duration)->whenSettled(SourceBufferPrivateRemote::queueSingleton(), WTFMove(completionHandler));
        return;
    }
    completionHandler();
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateBufferedChanged(Vector<WebCore::PlatformTimeRanges>&& trackBuffersRanges, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    if (auto client = this->client()) {
        client->sourceBufferPrivateBufferedChanged(WTFMove(trackBuffersRanges))->whenSettled(SourceBufferPrivateRemote::queueSingleton(), WTFMove(completionHandler));
        return;
    }
    completionHandler();
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDidDropSample()
{
    if (auto client = this->client())
        client->sourceBufferPrivateDidDropSample();
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDidAttach(InitializationSegmentInfo&& segmentInfo, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queueSingleton());

    RefPtr parent = m_parent.get();
    auto client = this->client();
    if (!client)
        return completionHandler(makeUnexpected(WebCore::PlatformMediaError::SourceRemoved));

    ASSERT(parent);

    RefPtr mediaPlayer = parent->player();
    if (!mediaPlayer)
        return completionHandler(makeUnexpected(WebCore::PlatformMediaError::SourceRemoved));

    client->sourceBufferPrivateDidAttach(createInitializationSegment(*mediaPlayer, WTFMove(segmentInfo)))->whenSettled(parent->queueSingleton(), WTFMove(completionHandler));
}

SourceBufferPrivateClient::InitializationSegment SourceBufferPrivateRemote::MessageReceiver::createInitializationSegment(MediaPlayerPrivateRemote& mediaPlayer, InitializationSegmentInfo&& segmentInfo) const
{
    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = segmentInfo.duration;

    segment.audioTracks = WTF::map(segmentInfo.audioTracks, [&](auto& audioTrack) {
        SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info {
            RemoteMediaDescription::create(audioTrack.description),
            mediaPlayer.audioTrackPrivateRemote(audioTrack.id)
        };
        return info;
    });

    segment.videoTracks = WTF::map(segmentInfo.videoTracks, [&](auto& videoTrack) {
        SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info {
            RemoteMediaDescription::create(videoTrack.description),
            mediaPlayer.videoTrackPrivateRemote(videoTrack.id)
        };
        return info;
    });

    segment.textTracks = WTF::map(segmentInfo.textTracks, [&](auto& textTrack) {
        SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info {
            RemoteMediaDescription::create(textTrack.description),
            mediaPlayer.textTrackPrivateRemote(textTrack.id)
        };
        return info;
    });

    return segment;
}

void SourceBufferPrivateRemote::memoryPressure(const MediaTime& currentTime)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, this, currentTime]() mutable {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;

        sendToProxy(Messages::RemoteSourceBufferProxy::MemoryPressure(currentTime));
    });
}

MediaTime SourceBufferPrivateRemote::minimumUpcomingPresentationTimeForTrackID(TrackID trackID)
{
    MediaTime result = MediaTime::invalidTime();
    // FIXME: Uses a new Connection for remote playback, and not the main GPUProcessConnection's one.
    callOnMainRunLoopAndWait([&] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;
        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteSourceBufferProxy::MinimumUpcomingPresentationTimeForTrackID(trackID), m_remoteSourceBufferIdentifier);

        result = std::get<0>(sendResult.takeReplyOr(MediaTime::invalidTime()));
    });
    return result;
}

void SourceBufferPrivateRemote::setMaximumQueueDepthForTrackID(TrackID trackID, uint64_t depth)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, this, trackID, depth]() {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;

        sendToProxy(Messages::RemoteSourceBufferProxy::SetMaximumQueueDepthForTrackID(trackID, depth));
    });
}

RefPtr<MediaPlayerPrivateRemote> SourceBufferPrivateRemote::player() const
{
    if (RefPtr mediaSource = m_mediaSource.get())
        return downcast<MediaPlayerPrivateRemote>(mediaSource->player());
    return nullptr;
}

void SourceBufferPrivateRemote::detach()
{
    ensureOnDispatcher([protectedThis = Ref { *this }, this]() {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;

        sendToProxy(Messages::RemoteSourceBufferProxy::Detach());
    });
}

void SourceBufferPrivateRemote::attach()
{
    ensureOnDispatcher([protectedThis = Ref { *this }, this]() {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;

        sendToProxy(Messages::RemoteSourceBufferProxy::Attach());
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

SourceBufferPrivateRemote::MessageReceiver::MessageReceiver(SourceBufferPrivateRemote& parent)
    : m_parent(parent)
{
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
