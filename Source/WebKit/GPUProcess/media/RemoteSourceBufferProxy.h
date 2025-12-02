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

#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "RemoteSourceBufferIdentifier.h"
#include <WebCore/MediaDescription.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/SourceBufferPrivate.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
class SharedBufferReference;
}

namespace WebCore {
class ContentType;
class MediaSample;
class PlatformTimeRanges;
}

namespace WebKit {

struct MediaDescriptionInfo;
class RemoteMediaPlayerProxy;

class RemoteSourceBufferProxy final
    : public WebCore::SourceBufferPrivateClient
    , private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSourceBufferProxy);
public:
    static Ref<RemoteSourceBufferProxy> create(GPUConnectionToWebProcess&, RemoteSourceBufferIdentifier, Ref<WebCore::SourceBufferPrivate>&&, RemoteMediaPlayerProxy&);
    virtual ~RemoteSourceBufferProxy();

    void ref() const final { WebCore::SourceBufferPrivateClient::ref(); }
    void deref() const final { WebCore::SourceBufferPrivateClient::deref(); }

    void setMediaPlayer(RemoteMediaPlayerProxy&);

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    void connectionToWebProcessClosed();

private:
    RemoteSourceBufferProxy(GPUConnectionToWebProcess&, RemoteSourceBufferIdentifier, Ref<WebCore::SourceBufferPrivate>&&, RemoteMediaPlayerProxy&);

    RefPtr<IPC::Connection> connection() const;
    Ref<WebCore::SourceBufferPrivate> protectedSourceBufferPrivate() const { return m_sourceBufferPrivate; }

    // SourceBufferPrivateClient
    Ref<WebCore::MediaPromise> sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&&) final;
    Ref<WebCore::MediaPromise> sourceBufferPrivateBufferedChanged(const Vector<WebCore::PlatformTimeRanges>&) final;
    void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&) final;
    Ref<WebCore::MediaPromise> sourceBufferPrivateDurationChanged(const MediaTime&) final;
    void sourceBufferPrivateDidDropSample() final;
    void sourceBufferPrivateEvictionDataChanged(const WebCore::SourceBufferEvictionData&) final;
    Ref<WebCore::MediaPromise> sourceBufferPrivateDidAttach(InitializationSegment&&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void setActive(bool);
    void canSwitchToType(const WebCore::ContentType&, CompletionHandler<void(bool)>&&);
    void setMode(WebCore::SourceBufferAppendMode);
    void append(IPC::SharedBufferReference&&, CompletionHandler<void(WebCore::MediaPromise::Result, const MediaTime&)>&&);
    void abort();
    void resetParserState();
    void removedFromMediaSource();
    void setMediaSourceEnded(bool);
    void startChangingType();
    void removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime, CompletionHandler<void()>&&);
    void evictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime, CompletionHandler<void(Vector<WebCore::PlatformTimeRanges>&&, WebCore::SourceBufferEvictionData&&)>&&);
    void asyncEvictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime);
    void addTrackBuffer(TrackID);
    void resetTrackBuffers();
    void clearTrackBuffers();
    void setAllTrackBuffersNeedRandomAccess();
    void reenqueueMediaIfNeeded(const MediaTime& currentMediaTime);
    void setGroupStartTimestamp(const MediaTime&);
    void setGroupStartTimestampToEndTimestamp();
    void setShouldGenerateTimestamps(bool);
    void resetTimestampOffsetInTrackBuffers();
    void setTimestampOffset(const MediaTime&);
    void setAppendWindowStart(const MediaTime&);
    void setAppendWindowEnd(const MediaTime&);
    void setMaximumBufferSize(uint64_t, CompletionHandler<void()>&&);
    void computeSeekTime(const WebCore::SeekTarget&, CompletionHandler<void(WebCore::SourceBufferPrivate::ComputeSeekPromise::Result&&)>&&);
    void updateTrackIds(Vector<std::pair<TrackID, TrackID>>&&);
    void bufferedSamplesForTrackId(TrackID, CompletionHandler<void(WebCore::SourceBufferPrivate::SamplesPromise::Result&&)>&&);
    void enqueuedSamplesForTrackID(TrackID, CompletionHandler<void(WebCore::SourceBufferPrivate::SamplesPromise::Result&&)>&&);
    void memoryPressure(const MediaTime& currentTime);
    void minimumUpcomingPresentationTimeForTrackID(TrackID, CompletionHandler<void(MediaTime)>&&);
    void setMaximumQueueDepthForTrackID(TrackID, uint64_t);
    void detach();
    void attach();
    void disconnect();
    std::optional<InitializationSegmentInfo> createInitializationSegmentInfo(InitializationSegment&&);

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_connectionToWebProcess;
    RemoteSourceBufferIdentifier m_identifier;
    Ref<WebCore::SourceBufferPrivate> m_sourceBufferPrivate;
    WeakPtr<RemoteMediaPlayerProxy> m_remoteMediaPlayerProxy;

    StdUnorderedMap<TrackID, Ref<WebCore::MediaDescription>> m_mediaDescriptions;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
