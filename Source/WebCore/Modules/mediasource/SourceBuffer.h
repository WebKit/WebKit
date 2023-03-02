/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "Timer.h"
#include "VideoTrackClient.h"
#include <wtf/LoggerHelper.h>
#include <wtf/Observer.h>

namespace WebCore {

class AudioTrackList;
class BufferSource;
class MediaSource;
class PlatformTimeRanges;
class SourceBufferPrivate;
class TextTrackList;
class TimeRanges;
class VideoTrackList;
class WebCoreOpaqueRoot;

class SourceBuffer final
    : public RefCounted<SourceBuffer>
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , private SourceBufferPrivateClient
    , private AudioTrackClient
    , private VideoTrackClient
    , private TextTrackClient
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(SourceBuffer);
public:
    using WeakValueType = EventTarget::WeakValueType;
    using EventTarget::weakPtrFactory;

    static Ref<SourceBuffer> create(Ref<SourceBufferPrivate>&&, MediaSource*);
    virtual ~SourceBuffer();

    bool updating() const { return m_updating; }
    ExceptionOr<Ref<TimeRanges>> buffered() const;
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

    const TimeRanges& bufferedInternal() const { ASSERT(m_private->buffered()); return *m_private->buffered(); }

    void abortIfUpdating();
    void removedFromMediaSource();
    void seekToTime(const MediaTime&);

    bool canPlayThroughRange(const PlatformTimeRanges&);

    bool hasVideo() const;

    bool active() const { return m_active; }

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

    Document& document() const;
    enum class AppendMode { Segments, Sequence };
    AppendMode mode() const { return m_mode; }
    ExceptionOr<void> setMode(AppendMode);

    WEBCORE_EXPORT void setShouldGenerateTimestamps(bool flag);

    bool isBufferedDirty() const;
    void setBufferedDirty(bool flag);

    MediaTime highestPresentationTimestamp() const;
    void readyStateChanged();

    size_t memoryCost() const;

    void setMediaSourceEnded(bool isEnded);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "SourceBuffer"; }
    WTFLogChannel& logChannel() const final;
#endif

    WebCoreOpaqueRoot opaqueRoot();

private:
    SourceBuffer(Ref<SourceBufferPrivate>&&, MediaSource*);

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject.
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    // SourceBufferPrivateClient
    void sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&&, CompletionHandler<void(ReceiveResult)>&&) final;
    void sourceBufferPrivateStreamEndedWithDecodeError() final;
    void sourceBufferPrivateAppendError(bool decodeError) final;
    void sourceBufferPrivateAppendComplete(AppendResult) final;
    void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&) final;
    void sourceBufferPrivateDurationChanged(const MediaTime& duration, CompletionHandler<void()>&&) final;
    void sourceBufferPrivateDidParseSample(double sampleDuration) final;
    void sourceBufferPrivateDidDropSample() final;
    void sourceBufferPrivateBufferedDirtyChanged(bool) final;
    void sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode) final;
    void sourceBufferPrivateReportExtraMemoryCost(uint64_t) final;

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
    EventTargetInterface eventTargetInterface() const final { return SourceBufferEventTargetInterfaceType; }

    bool isRemoved() const;
    void scheduleEvent(const AtomString& eventName);

    ExceptionOr<void> appendBufferInternal(const unsigned char*, unsigned);
    void appendBufferTimerFired();
    void resetParserState();

    void setActive(bool);

    bool validateInitializationSegment(const InitializationSegment&);

    uint64_t maximumBufferSize() const;

    void monitorBufferingRate();

    void removeTimerFired();

    void reportExtraMemoryAllocated(uint64_t extraMemory);

    void appendError(bool);

    bool hasAudio() const;

    void rangeRemoval(const MediaTime&, const MediaTime&);

    friend class Internals;
    WEBCORE_EXPORT void bufferedSamplesForTrackId(const AtomString&, CompletionHandler<void(Vector<String>&&)>&&);
    WEBCORE_EXPORT void enqueuedSamplesForTrackID(const AtomString&, CompletionHandler<void(Vector<String>&&)>&&);
    WEBCORE_EXPORT MediaTime minimumUpcomingPresentationTimeForTrackID(const AtomString&);
    WEBCORE_EXPORT void setMaximumQueueDepthForTrackID(const AtomString&, uint64_t);

    Ref<SourceBufferPrivate> m_private;
    MediaSource* m_source;
    AppendMode m_mode { AppendMode::Segments };

    WTF::Observer<WebCoreOpaqueRoot()> m_opaqueRootProvider;

    RefPtr<SharedBuffer> m_pendingAppendData;
    Timer m_appendBufferTimer;

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

    MonotonicTime m_timeOfBufferingMonitor;
    double m_bufferedSinceLastMonitor { 0 };
    double m_averageBufferRate { 0 };
    bool m_bufferedDirty { true };

    // Can only grow.
    uint64_t m_reportedExtraMemoryCost { 0 };
    // Can grow and shrink.
    uint64_t m_extraMemoryCost { 0 };

    MediaTime m_pendingRemoveStart;
    MediaTime m_pendingRemoveEnd;
    Timer m_removeTimer;

    bool m_updating { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_active { false };
    bool m_shouldGenerateTimestamps { false };
    bool m_pendingInitializationSegmentForChangeType { false };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebCore

#endif
