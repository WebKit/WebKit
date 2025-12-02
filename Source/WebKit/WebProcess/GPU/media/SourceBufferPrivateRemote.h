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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "GPUProcessConnection.h"
#include "RemoteSourceBufferIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ContentType.h>
#include <WebCore/MediaSample.h>
#include <WebCore/SourceBufferPrivate.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <atomic>
#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class PlatformTimeRanges;
}

namespace WebKit {

struct InitializationSegmentInfo;
class MediaPlayerPrivateRemote;
class MediaSourcePrivateRemote;

class SourceBufferPrivateRemote final
    : public WebCore::SourceBufferPrivate
{
    WTF_MAKE_TZONE_ALLOCATED(SourceBufferPrivateRemote);
public:
    static Ref<SourceBufferPrivateRemote> create(GPUProcessConnection&, RemoteSourceBufferIdentifier, MediaSourcePrivateRemote&);
    virtual ~SourceBufferPrivateRemote();

    constexpr WebCore::MediaPlatformType platformType() const final { return WebCore::MediaPlatformType::Remote; }

    static WorkQueue& queueSingleton();

    class MessageReceiver : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
    public:
        static Ref<MessageReceiver> create(SourceBufferPrivateRemote& parent)
        {
            return adoptRef(*new MessageReceiver(parent));
        }

    private:
        MessageReceiver(SourceBufferPrivateRemote&);
        void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
        void sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&&, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&&);
        void takeOwnershipOfMemory(WebCore::SharedMemory::Handle&&);
        void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&);
        void sourceBufferPrivateBufferedChanged(Vector<WebCore::PlatformTimeRanges>&&, CompletionHandler<void()>&&);
        void sourceBufferPrivateDurationChanged(const MediaTime&, CompletionHandler<void()>&&);
        void sourceBufferPrivateDidDropSample();
        void sourceBufferPrivateEvictionDataChanged(WebCore::SourceBufferEvictionData&&);
        void sourceBufferPrivateDidAttach(InitializationSegmentInfo&&, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&&);
        RefPtr<WebCore::SourceBufferPrivateClient> client() const;
        WebCore::SourceBufferPrivateClient::InitializationSegment createInitializationSegment(MediaPlayerPrivateRemote&, InitializationSegmentInfo&&) const;
        ThreadSafeWeakPtr<SourceBufferPrivateRemote> m_parent;
    };

private:
    SourceBufferPrivateRemote(GPUProcessConnection&, RemoteSourceBufferIdentifier, MediaSourcePrivateRemote&);

    // SourceBufferPrivate overrides
    void setActive(bool) final;
    Ref<WebCore::MediaPromise> append(Ref<WebCore::SharedBuffer>&&) final;
    Ref<WebCore::MediaPromise> appendInternal(Ref<WebCore::SharedBuffer>&&) final;
    void resetParserStateInternal() final;
    void abort() final;
    void resetParserState() final;
    void removedFromMediaSource() final;
    bool canSwitchToType(const WebCore::ContentType&) final;
    void setMediaSourceEnded(bool) final;
    void setMode(WebCore::SourceBufferAppendMode) final;
    void reenqueueMediaIfNeeded(const MediaTime& currentMediaTime) final;
    void addTrackBuffer(TrackID, RefPtr<WebCore::MediaDescription>&&) final;
    void resetTrackBuffers() final;
    void clearTrackBuffers(bool) final;
    void setAllTrackBuffersNeedRandomAccess() final;
    void setGroupStartTimestamp(const MediaTime&) final;
    void setGroupStartTimestampToEndTimestamp() final;
    void setShouldGenerateTimestamps(bool) final;
    Ref<WebCore::MediaPromise> removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime) final;
    bool evictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime) final;
    void resetTimestampOffsetInTrackBuffers() final;
    void startChangingType() final;
    void setTimestampOffset(const MediaTime&) final;
    void setAppendWindowStart(const MediaTime&) final;
    void setAppendWindowEnd(const MediaTime&) final;
    Ref<GenericPromise> setMaximumBufferSize(size_t) final;

    Ref<ComputeSeekPromise> computeSeekTime(const WebCore::SeekTarget&) final;
    void seekToTime(const MediaTime&) final;

    void updateTrackIds(Vector<std::pair<TrackID, TrackID>>&&) final;

    void memoryPressure(const MediaTime& currentTime) final;

    void detach() final;
    void attach() final;

    // Internals Utility methods
    Ref<SamplesPromise> bufferedSamplesForTrackId(TrackID) final;
    Ref<SamplesPromise> enqueuedSamplesForTrackID(TrackID) final;
    MediaTime minimumUpcomingPresentationTimeForTrackID(TrackID) final;
    void setMaximumQueueDepthForTrackID(TrackID, uint64_t) final;

    void ensureWeakOnDispatcher(Function<void(SourceBufferPrivateRemote&)>&&);

    template<typename T> void sendToProxy(T&& message);

    RefPtr<MediaPlayerPrivateRemote> player() const;

    template<typename PC = IPC::Connection::NoOpPromiseConverter, typename T>
    auto sendWithPromisedReply(T&& message)
    {
        return m_gpuProcessConnection.get()->connection().sendWithPromisedReply<PC, T>(std::forward<T>(message), m_remoteSourceBufferIdentifier);
    }

    friend class MessageReceiver;
    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    const Ref<MessageReceiver> m_receiver;
    const RemoteSourceBufferIdentifier m_remoteSourceBufferIdentifier;

    bool isGPURunning() const { return !m_removed; }
    std::atomic<bool> m_removed { false };

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const override { return "SourceBufferPrivateRemote"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    uint64_t sourceBufferLogIdentifier() final { return logIdentifier(); }

    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

template<typename T>
void SourceBufferPrivateRemote::sendToProxy(T&& message)
{
    m_gpuProcessConnection.get()->connection().send(WTFMove(message), m_remoteSourceBufferIdentifier);
}

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::SourceBufferPrivateRemote)
static bool isType(const WebCore::SourceBufferPrivate& sourceBuffer) { return sourceBuffer.platformType() == WebCore::MediaPlatformType::Remote; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
