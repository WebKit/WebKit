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

#include "DataReference.h"
#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "RemoteSourceBufferIdentifier.h"
#include "SharedMemory.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/MediaDescription.h>
#include <WebCore/SourceBufferPrivate.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/Ref.h>
#include <wtf/text/AtomString.h>

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
    : public RefCounted<RemoteSourceBufferProxy>
    , public WebCore::SourceBufferPrivateClient
    , private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteSourceBufferProxy> create(GPUConnectionToWebProcess&, RemoteSourceBufferIdentifier, Ref<WebCore::SourceBufferPrivate>&&, RemoteMediaPlayerProxy&);
    virtual ~RemoteSourceBufferProxy();

private:
    RemoteSourceBufferProxy(GPUConnectionToWebProcess&, RemoteSourceBufferIdentifier, Ref<WebCore::SourceBufferPrivate>&&, RemoteMediaPlayerProxy&);

    // SourceBufferPrivateClient
    void sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&&, CompletionHandler<void(ReceiveResult)>&&) final;
    void sourceBufferPrivateStreamEndedWithDecodeError() final;
    void sourceBufferPrivateAppendError(bool decodeError) final;
    void sourceBufferPrivateAppendComplete(WebCore::SourceBufferPrivateClient::AppendResult) final;
    void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&) final;
    void sourceBufferPrivateDurationChanged(const MediaTime&) final;
    void sourceBufferPrivateDidParseSample(double sampleDuration) final;
    void sourceBufferPrivateDidDropSample() final;
    void sourceBufferPrivateBufferedDirtyChanged(bool) final;
    void sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode) final;
    void sourceBufferPrivateReportExtraMemoryCost(uint64_t extraMemory) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void setActive(bool);
    void canSwitchToType(const WebCore::ContentType&, CompletionHandler<void(bool)>&&);
    void setMode(WebCore::SourceBufferAppendMode);
    void append(IPC::SharedBufferReference&&, CompletionHandler<void(std::optional<WebKit::SharedMemory::Handle>&&)>&&);
    void abort();
    void resetParserState();
    void removedFromMediaSource();
    void setMediaSourceEnded(bool);
    void setReadyState(WebCore::MediaPlayer::ReadyState);
    void startChangingType();
    void updateBufferedFromTrackBuffers(bool sourceIsEnded, CompletionHandler<void(WebCore::PlatformTimeRanges&&)>&&);
    void removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime, bool isEnded, CompletionHandler<void(WebCore::PlatformTimeRanges&&, uint64_t)>&&);
    void evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime, bool isEnded, CompletionHandler<void(uint64_t)>&&);
    void addTrackBuffer(TrackPrivateRemoteIdentifier);
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
    void seekToTime(const MediaTime&);
    void updateTrackIds(Vector<std::pair<TrackPrivateRemoteIdentifier, TrackPrivateRemoteIdentifier>>&&);
    void bufferedSamplesForTrackId(TrackPrivateRemoteIdentifier, CompletionHandler<void(Vector<String>&&)>&&);
    void enqueuedSamplesForTrackID(TrackPrivateRemoteIdentifier, CompletionHandler<void(Vector<String>&&)>&&);

    WeakPtr<GPUConnectionToWebProcess> m_connectionToWebProcess;
    RemoteSourceBufferIdentifier m_identifier;
    Ref<WebCore::SourceBufferPrivate> m_sourceBufferPrivate;
    WeakPtr<RemoteMediaPlayerProxy> m_remoteMediaPlayerProxy;

    HashMap<TrackPrivateRemoteIdentifier, AtomString> m_trackIds;
    HashMap<TrackPrivateRemoteIdentifier, Ref<WebCore::MediaDescription>> m_mediaDescriptions;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
