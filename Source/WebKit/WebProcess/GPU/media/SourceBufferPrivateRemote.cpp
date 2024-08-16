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

WorkQueue& SourceBufferPrivateRemote::queue()
{
    return MediaSourcePrivateRemote::queue();
}

void SourceBufferPrivateRemote::ensureOnDispatcherSync(Function<void()>&& function)
{
    if (queue().isCurrent())
        function();
    else
        queue().dispatchSync(WTFMove(function));
}

void SourceBufferPrivateRemote::ensureWeakOnDispatcher(Function<void()>&& function)
{
    auto weakWrapper = [function = WTFMove(function), weakThis = ThreadSafeWeakPtr(*this), this] {
        if (RefPtr protectedThis = weakThis.get()) {
            auto gpuProcessConnection = m_gpuProcessConnection.get();
            if (!gpuProcessConnection || !isGPURunning())
                return;
            function();
        }
    };
    ensureOnDispatcher(WTFMove(weakWrapper));
}

Ref<SourceBufferPrivateRemote> SourceBufferPrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, MediaSourcePrivateRemote& mediaSourcePrivate, const MediaPlayerPrivateRemote& mediaPlayerPrivate)
{
    return adoptRef(*new SourceBufferPrivateRemote(gpuProcessConnection, remoteSourceBufferIdentifier, mediaSourcePrivate, mediaPlayerPrivate));
}

SourceBufferPrivateRemote::SourceBufferPrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteSourceBufferIdentifier remoteSourceBufferIdentifier, MediaSourcePrivateRemote& mediaSourcePrivate, const MediaPlayerPrivateRemote& mediaPlayerPrivate)
    : SourceBufferPrivate(mediaSourcePrivate, queue())
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_receiver(MessageReceiver::create(*this))
    , m_remoteSourceBufferIdentifier(remoteSourceBufferIdentifier)
    , m_mediaPlayerPrivate(mediaPlayerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSourcePrivate.logger())
    , m_logIdentifier(mediaSourcePrivate.nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    gpuProcessConnection.connection().addWorkQueueMessageReceiver(Messages::SourceBufferPrivateRemoteMessageReceiver::messageReceiverName(), queue(), m_receiver, m_remoteSourceBufferIdentifier.toUInt64());
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
            if (RefPtr protectedThis = weakThis.get()) {
                Locker locker { protectedThis->m_lock };
                protectedThis->m_timestampOffset = std::get<MediaTime>(*result);
            }
            return MediaPromise::createAndSettle(std::get<MediaPromise::Result>(*result));
        });
    });
}

void SourceBufferPrivateRemote::abort()
{
    ensureWeakOnDispatcher([this] {
        // When abort() is being issued; this will force the RemoteSourceBufferProxy to abort the current remote source buffer operation
        // The totalTrackBufferSizeInBytes will be recalculated after the next operation which allows for potentially having
        // m_totalTrackBufferSizeInBytes being temporarily stale as it won't be used until then.
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::Abort(), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::resetParserState()
{
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::ResetParserState(), m_remoteSourceBufferIdentifier);
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
    ensureWeakOnDispatcher([this] {
        m_removed = true;
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::RemovedFromMediaSource(), m_remoteSourceBufferIdentifier);
    });
}

bool SourceBufferPrivateRemote::isActive() const
{
    return m_isActive;
}

void SourceBufferPrivateRemote::setActive(bool active)
{
    // Called from the SourceBuffer's dispatcher
    m_isActive = true;
    ensureWeakOnDispatcher([this, active] {
        auto mediaSource = m_mediaSource.get();
        if (!mediaSource)
            return;

        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetActive(active), m_remoteSourceBufferIdentifier);
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
    ensureWeakOnDispatcher([this, isEnded] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetMediaSourceEnded(isEnded), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setMode(SourceBufferAppendMode mode)
{
    ensureWeakOnDispatcher([this, mode] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetMode(mode), m_remoteSourceBufferIdentifier);
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
        ensureWeakOnDispatcher([this, newDataSize, currentTime] {
            m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::AsyncEvictCodedFrames(newDataSize, currentTime), m_remoteSourceBufferIdentifier);
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
    ensureWeakOnDispatcher([this, trackId] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::AddTrackBuffer(trackId), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::resetTrackBuffers()
{
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::ResetTrackBuffers(), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::clearTrackBuffers(bool)
{
    {
        Locker locker { m_lock };
        m_evictionData.clear();
    }
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::ClearTrackBuffers(), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setAllTrackBuffersNeedRandomAccess()
{
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetAllTrackBuffersNeedRandomAccess(), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setGroupStartTimestamp(const MediaTime& timestamp)
{
    ensureWeakOnDispatcher([this, timestamp] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestamp(timestamp), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setGroupStartTimestampToEndTimestamp()
{
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetGroupStartTimestampToEndTimestamp(), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    ensureWeakOnDispatcher([this, shouldGenerateTimestamps] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetShouldGenerateTimestamps(shouldGenerateTimestamps), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    ensureWeakOnDispatcher([this, currentMediaTime] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::ReenqueueMediaIfNeeded(currentMediaTime), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::resetTimestampOffsetInTrackBuffers()
{
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::ResetTimestampOffsetInTrackBuffers(), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::startChangingType()
{
    ensureWeakOnDispatcher([this] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::StartChangingType(), m_remoteSourceBufferIdentifier);
    });
}

MediaTime SourceBufferPrivateRemote::timestampOffset() const
{
    Locker locker { m_lock };
    return m_timestampOffset;
}

void SourceBufferPrivateRemote::setTimestampOffset(const MediaTime& timestampOffset)
{
    // Called from the SourceBuffer's dispatcher
    {
        Locker locker { m_lock };
        m_timestampOffset = timestampOffset;
    }
    ensureWeakOnDispatcher([this, timestampOffset] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetTimestampOffset(timestampOffset), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    ensureWeakOnDispatcher([this, appendWindowStart] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowStart(appendWindowStart), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    ensureWeakOnDispatcher([this, appendWindowEnd] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SetAppendWindowEnd(appendWindowEnd), m_remoteSourceBufferIdentifier);
    });
}

Ref<GenericPromise> SourceBufferPrivateRemote::setMaximumBufferSize(size_t size)
{
    {
        Locker locker { m_lock };
        m_evictionData.maximumBufferSize = size;
    }
    GenericPromise::AutoRejectProducer producer;
    Ref promise = producer.promise();
    ensureWeakOnDispatcher([this, size, producer = WTFMove(producer)]() mutable {
        sendWithPromisedReply(Messages::RemoteSourceBufferProxy::SetMaximumBufferSize(size))->chainTo(WTFMove(producer));
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
    ensureWeakOnDispatcher([this, time] {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::SeekToTime(time), m_remoteSourceBufferIdentifier);
    });
}

void SourceBufferPrivateRemote::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIDPairs)
{
    ensureWeakOnDispatcher([this, trackIDPairs = WTFMove(trackIDPairs)]() mutable {
        m_gpuProcessConnection.get()->connection().send(Messages::RemoteSourceBufferProxy::UpdateTrackIds(WTFMove(trackIDPairs)), m_remoteSourceBufferIdentifier);
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
    assertIsCurrent(SourceBufferPrivateRemote::queue());

    // Take ownership of shared memory and mark it as media-related memory.
    bufferHandle.takeOwnershipOfMemory(MemoryLedger::Media);
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&& segmentInfo, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queue());

    RefPtr parent = m_parent.get();
    auto client = this->client();
    if (!client)
        return completionHandler(makeUnexpected(WebCore::PlatformMediaError::SourceRemoved));

    ASSERT(parent);

    RefPtr mediaPlayer = parent->m_mediaPlayerPrivate.get();
    if (!mediaPlayer)
        return completionHandler(makeUnexpected(WebCore::PlatformMediaError::SourceRemoved));

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = segmentInfo.duration;

    segment.audioTracks = WTF::map(segmentInfo.audioTracks, [&](auto& audioTrack) {
        SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info {
            RemoteMediaDescription::create(audioTrack.description),
            mediaPlayer->audioTrackPrivateRemote(audioTrack.id)
        };
        return info;
    });

    segment.videoTracks = WTF::map(segmentInfo.videoTracks, [&](auto& videoTrack) {
        SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info {
            RemoteMediaDescription::create(videoTrack.description),
            mediaPlayer->videoTrackPrivateRemote(videoTrack.id)
        };
        return info;
    });

    segment.textTracks = WTF::map(segmentInfo.textTracks, [&](auto& textTrack) {
        SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info {
            RemoteMediaDescription::create(textTrack.description),
            mediaPlayer->textTrackPrivateRemote(textTrack.id)
        };
        return info;
    });

    client->sourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segment))->whenSettled(parent->queue(), WTFMove(completionHandler));
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateEvictionDataChanged(SourceBufferEvictionData&& evictionData)
{
    assertIsCurrent(SourceBufferPrivateRemote::queue());

    if (RefPtr parent = m_parent.get()) {
        Locker locker { parent->m_lock };
        parent->m_evictionData = WTFMove(evictionData);
    }
    if (auto client = this->client())
        client->sourceBufferPrivateEvictionDataChanged(WTFMove(evictionData));
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    assertIsCurrent(SourceBufferPrivateRemote::queue());

    if (auto client = this->client())
        client->sourceBufferPrivateHighestPresentationTimestampChanged(timestamp);
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDurationChanged(const MediaTime& duration, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queue());

    if (auto client = this->client()) {
        client->sourceBufferPrivateDurationChanged(duration)->whenSettled(SourceBufferPrivateRemote::queue(), WTFMove(completionHandler));
        return;
    }
    completionHandler();
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateBufferedChanged(Vector<WebCore::PlatformTimeRanges>&& trackBuffersRanges, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(SourceBufferPrivateRemote::queue());

    if (auto client = this->client()) {
        client->sourceBufferPrivateBufferedChanged(WTFMove(trackBuffersRanges))->whenSettled(SourceBufferPrivateRemote::queue(), WTFMove(completionHandler));
        return;
    }
    completionHandler();
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDidDropSample()
{
    if (auto client = this->client())
        client->sourceBufferPrivateDidDropSample();
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode)
{
    if (auto client = this->client())
        client->sourceBufferPrivateDidReceiveRenderingError(errorCode);
}

void SourceBufferPrivateRemote::MessageReceiver::sourceBufferPrivateShuttingDown(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (RefPtr parent = m_parent.get())
        parent->m_shutdown = true;
    completionHandler();
}

uint64_t SourceBufferPrivateRemote::totalTrackBufferSizeInBytes() const
{
//    assertIsHeld(m_lock);
    return m_evictionData.contentSize;
}

void SourceBufferPrivateRemote::memoryPressure(const MediaTime& currentTime)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, this, currentTime]() mutable {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!gpuProcessConnection || !isGPURunning())
            return;

        gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::MemoryPressure(currentTime), m_remoteSourceBufferIdentifier);
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

        gpuProcessConnection->connection().send(Messages::RemoteSourceBufferProxy::SetMaximumQueueDepthForTrackID(trackID, depth), m_remoteSourceBufferIdentifier);
    });
}

bool SourceBufferPrivateRemote::isBufferFullFor(uint64_t requiredSize) const
{
    Locker locker { m_lock };

    ALWAYS_LOG(LOGIDENTIFIER, "requiredSize:", requiredSize, " evictionData:", m_evictionData);

    return SourceBufferPrivate::isBufferFullFor(requiredSize);
}

bool SourceBufferPrivateRemote::canAppend(uint64_t requiredSize) const
{
    Locker locker { m_lock };

    return SourceBufferPrivate::canAppend(requiredSize);
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
