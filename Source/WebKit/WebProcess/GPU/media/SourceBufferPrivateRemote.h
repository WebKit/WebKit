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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "RemoteSourceBufferIdentifier.h"
#include <WebCore/ContentType.h>
#include <WebCore/MediaSample.h>
#include <WebCore/SourceBufferPrivate.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/Ref.h>
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
    , public IPC::MessageReceiver
{
public:
    static Ref<SourceBufferPrivateRemote> create(GPUProcessConnection&, RemoteSourceBufferIdentifier, MediaSourcePrivateRemote&, const MediaPlayerPrivateRemote&);
    virtual ~SourceBufferPrivateRemote();

    constexpr WebCore::MediaPlatformType platformType() const final { return WebCore::MediaPlatformType::Remote; }

    void disconnect() { m_disconnected = true; }

private:
    SourceBufferPrivateRemote(GPUProcessConnection&, RemoteSourceBufferIdentifier, MediaSourcePrivateRemote&, const MediaPlayerPrivateRemote&);

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
    void evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime) final;
    void resetTimestampOffsetInTrackBuffers() final;
    void startChangingType() final;
    void setTimestampOffset(const MediaTime&) final;
    void setAppendWindowStart(const MediaTime&) final;
    void setAppendWindowEnd(const MediaTime&) final;

    Ref<ComputeSeekPromise> computeSeekTime(const WebCore::SeekTarget&) final;
    void seekToTime(const MediaTime&) final;

    void updateTrackIds(Vector<std::pair<TrackID, TrackID>>&&) final;
    uint64_t totalTrackBufferSizeInBytes() const final;

    void memoryPressure(uint64_t maximumBufferSize, const MediaTime& currentTime) final;

    // Internals Utility methods
    Ref<SamplesPromise> bufferedSamplesForTrackId(TrackID) final;
    Ref<SamplesPromise> enqueuedSamplesForTrackID(TrackID) final;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegmentInfo&&, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&&);
    void takeOwnershipOfMemory(WebCore::SharedMemory::Handle&&);
    void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&);
    void sourceBufferPrivateBufferedChanged(Vector<WebCore::PlatformTimeRanges>&&, uint64_t, CompletionHandler<void()>&&);
    void sourceBufferPrivateDurationChanged(const MediaTime&, CompletionHandler<void()>&&);
    void sourceBufferPrivateDidDropSample();
    void sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode);
    MediaTime minimumUpcomingPresentationTimeForTrackID(TrackID) override;
    void setMaximumQueueDepthForTrackID(TrackID, uint64_t) override;

    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    RemoteSourceBufferIdentifier m_remoteSourceBufferIdentifier;
    WeakPtr<MediaPlayerPrivateRemote> m_mediaPlayerPrivate;

    uint64_t m_totalTrackBufferSizeInBytes = { 0 };

    bool isGPURunning() const { return !m_disconnected && m_gpuProcessConnection.get(); }
    bool m_disconnected { false };

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "SourceBufferPrivateRemote"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    const void* sourceBufferLogIdentifier() final { return logIdentifier(); }

    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::SourceBufferPrivateRemote)
static bool isType(const WebCore::SourceBufferPrivate& sourceBuffer) { return sourceBuffer.platformType() == WebCore::MediaPlatformType::Remote; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
