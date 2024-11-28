/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "ActiveDOMObject.h"
#include "AudioTrackClient.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "SourceBufferPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "TextTrackClient.h"
#include "VideoTrackClient.h"
#include <wtf/LoggerHelper.h>
#include <wtf/NativePromise.h>
#include <wtf/Observer.h>
#include <wtf/Ref.h>

namespace WebCore {

class AudioTrackList;
class BufferSource;
class SourceBufferClientImpl;
class MediaSource;
class PlatformTimeRanges;
class SourceBufferPrivate;
class TextTrackList;
class TimeRanges;
class VideoTrackList;
class WebCoreOpaqueRoot;

class SourceBuffer
    : public RefCounted<SourceBuffer>
    , public CanMakeWeakPtr<SourceBuffer>
    , public ActiveDOMObject
    , public EventTarget
    , private AudioTrackClient
    , private VideoTrackClient
    , private TextTrackClient
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SourceBuffer);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<SourceBuffer> create(Ref<SourceBufferPrivate>&&, MediaSource&);
    virtual ~SourceBuffer();

    USING_CAN_MAKE_WEAKPTR(CanMakeWeakPtr<SourceBuffer>);

    static bool enabledForContext(ScriptExecutionContext&);

    bool updating() const { return m_updating; }
    ExceptionOr<Ref<TimeRanges>> buffered();
    double timestampOffset() const;
    ExceptionOr<void> setTimestampOffset(double);

    VideoTrackList& videoTracks();
    VideoTrackList* videoTracksIfExists() const { return m_videoTracks.get(); }
    AudioTrackList& audioTracks();
    AudioTrackList* audioTracksIfExists() const { return m_audioTracks.get(); }
    TextTrackList& textTracks();
    TextTrackList* textTracksIfExists() const { return m_textTracks.get(); }

    double appendWindowStart() const;
    ExceptionOr<void> setAppendWindowStart(double);
    double appendWindowEnd() const;
    ExceptionOr<void> setAppendWindowEnd(double);

    ExceptionOr<void> appendBuffer(const BufferSource&);
    ExceptionOr<void> abort();
    ExceptionOr<void> remove(double start, double end);
    ExceptionOr<void> remove(const MediaTime&, const MediaTime&);
    ExceptionOr<void> changeType(const String&);

    const PlatformTimeRanges& bufferedInternal() const { return m_buffered->ranges(); }

    void abortIfUpdating();
    void removedFromMediaSource();
    using ComputeSeekPromise = SourceBufferPrivate::ComputeSeekPromise;
    Ref<ComputeSeekPromise> computeSeekTime(const SeekTarget&);
    void seekToTime(const MediaTime&);

    bool hasVideo() const;

    bool active() const { return m_active; }

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    enum class AppendMode { Segments, Sequence };
    AppendMode mode() const { return m_mode; }
    ExceptionOr<void> setMode(AppendMode);

    WEBCORE_EXPORT void setShouldGenerateTimestamps(bool flag);

    bool isBufferedDirty() const;
    void setBufferedDirty(bool flag);

    MediaTime highestPresentationTimestamp() const;

    size_t memoryCost() const;

    void setMediaSourceEnded(bool isEnded);
    bool receivedFirstInitializationSegment() const { return m_receivedFirstInitializationSegment; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "SourceBuffer"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    WebCoreOpaqueRoot opaqueRoot();

    virtual bool isManaged() const { return false; }
    void memoryPressure();

    // Detachable MSE methods.
    void detach();
    void attach();

protected:
    SourceBuffer(Ref<SourceBufferPrivate>&&, MediaSource&);

private:
    friend class SourceBufferClientImpl;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject.
    bool virtualHasPendingActivity() const final;

    Ref<MediaPromise> sourceBufferPrivateDidReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&&);
    Ref<MediaPromise> sourceBufferPrivateBufferedChanged(Vector<PlatformTimeRanges>&&);
    void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&);
    Ref<MediaPromise> sourceBufferPrivateDurationChanged(const MediaTime& duration);
    void sourceBufferPrivateDidDropSample();
    void sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode);
    Ref<MediaPromise> sourceBufferPrivateDidAttach(SourceBufferPrivateClient::InitializationSegment&&);

    // AudioTrackClient
    void audioTrackEnabledChanged(AudioTrack&) final;
    void audioTrackKindChanged(AudioTrack&) final;
    void audioTrackLabelChanged(AudioTrack&) final;
    void audioTrackLanguageChanged(AudioTrack&) final;

    // TextTrackClient
    void textTrackKindChanged(TextTrack&) final;
    void textTrackModeChanged(TextTrack&) final;
    void textTrackLanguageChanged(TextTrack&) final;

    // VideoTrackClient
    void videoTrackKindChanged(VideoTrack&) final;
    void videoTrackLabelChanged(VideoTrack&) final;
    void videoTrackLanguageChanged(VideoTrack&) final;
    void videoTrackSelectedChanged(VideoTrack&) final;

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::SourceBuffer; }

    bool isRemoved() const;
    void scheduleEvent(const AtomString& eventName);

    ExceptionOr<void> appendBufferInternal(std::span<const uint8_t>);
    void sourceBufferPrivateAppendComplete(MediaPromise::Result&&);
    void resetParserState();

    void setActive(bool);

    bool validateInitializationSegment(const SourceBufferPrivateClient::InitializationSegment&);

    uint64_t maximumBufferSize() const;

    void reportExtraMemoryAllocated(uint64_t extraMemory);

    void appendError(bool);

    bool hasAudio() const;

    void rangeRemoval(const MediaTime&, const MediaTime&);

    friend class Internals;
    using SamplesPromise = NativePromise<Vector<String>, PlatformMediaError>;
    WEBCORE_EXPORT Ref<SamplesPromise> bufferedSamplesForTrackId(TrackID);
    WEBCORE_EXPORT Ref<SamplesPromise> enqueuedSamplesForTrackID(TrackID);
    WEBCORE_EXPORT MediaTime minimumUpcomingPresentationTimeForTrackID(TrackID);
    WEBCORE_EXPORT void setMaximumQueueDepthForTrackID(TrackID, uint64_t);
    WEBCORE_EXPORT Ref<GenericPromise> setMaximumSourceBufferSize(uint64_t);
    WEBCORE_EXPORT size_t evictableSize() const;

    void updateBuffered();

    Ref<SourceBufferPrivate> m_private;
    Ref<SourceBufferClientImpl> m_client;

    MediaSource* m_source;
    AppendMode m_mode { AppendMode::Segments };

    WTF::Observer<WebCoreOpaqueRoot()> m_opaqueRootProvider;

    RefPtr<SharedBuffer> m_pendingAppendData;

    RefPtr<VideoTrackList> m_videoTracks;
    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;

    Vector<AtomString> m_videoCodecs;
    Vector<AtomString> m_audioCodecs;
    Vector<AtomString> m_textCodecs;

    MediaTime m_appendWindowStart;
    MediaTime m_appendWindowEnd;
    MediaTime m_highestPresentationTimestamp;

    enum AppendStateType { WaitingForSegment, ParsingInitSegment, ParsingMediaSegment };
    AppendStateType m_appendState;

    bool m_bufferedDirty { true };

    // Can only grow.
    uint64_t m_reportedExtraMemoryCost { 0 };
    // Can grow and shrink.
    uint64_t m_extraMemoryCost { 0 };

    bool m_updating { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_active { false };
    bool m_shouldGenerateTimestamps { false };
    bool m_pendingInitializationSegmentForChangeType { false };
    bool m_mediaSourceEnded { false };
    Ref<TimeRanges> m_buffered;
    Vector<PlatformTimeRanges> m_trackBuffers;
    bool m_appendBufferPending { false };
    uint32_t m_appendBufferOperationId { 0 };
    bool m_removeCodedFramesPending { false };
    std::optional<uint64_t> m_maximumBufferSize;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

#endif
