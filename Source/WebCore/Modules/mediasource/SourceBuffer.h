/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#include "AudioTrack.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "GenericEventQueue.h"
#include "ScriptWrappable.h"
#include "SourceBufferPrivateClient.h"
#include "TextTrack.h"
#include "Timer.h"
#include "VideoTrack.h"
#include <runtime/ArrayBufferView.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioTrackList;
class MediaSource;
class PlatformTimeRanges;
class SourceBufferPrivate;
class TextTrackList;
class TimeRanges;
class VideoTrackList;

class SourceBuffer final : public RefCounted<SourceBuffer>, public ActiveDOMObject, public EventTargetWithInlineData, public SourceBufferPrivateClient, public AudioTrackClient, public VideoTrackClient, public TextTrackClient {
public:
    static Ref<SourceBuffer> create(Ref<SourceBufferPrivate>&&, MediaSource*);

    virtual ~SourceBuffer();

    // SourceBuffer.idl methods
    bool updating() const { return m_updating; }
    RefPtr<TimeRanges> buffered(ExceptionCode&) const;
    const RefPtr<TimeRanges>& buffered() const;
    double timestampOffset() const;
    void setTimestampOffset(double, ExceptionCode&);

#if ENABLE(VIDEO_TRACK)
    VideoTrackList* videoTracks();
    AudioTrackList* audioTracks();
    TextTrackList* textTracks();
#endif

    double appendWindowStart() const;
    void setAppendWindowStart(double, ExceptionCode&);
    double appendWindowEnd() const;
    void setAppendWindowEnd(double, ExceptionCode&);

    void appendBuffer(ArrayBuffer&, ExceptionCode&);
    void appendBuffer(ArrayBufferView&, ExceptionCode&);
    void abort(ExceptionCode&);
    void remove(double start, double end, ExceptionCode&);
    void remove(const MediaTime&, const MediaTime&, ExceptionCode&);

    void appendError(bool);
    void abortIfUpdating();
    void removedFromMediaSource();
    void seekToTime(const MediaTime&);

    bool hasCurrentTime() const;
    bool hasFutureTime() const;
    bool canPlayThrough();

    bool hasVideo() const;
    bool hasAudio() const;

    bool active() const { return m_active; }

    // EventTarget interface
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    EventTargetInterface eventTargetInterface() const override { return SourceBufferEventTargetInterfaceType; }

    using RefCounted<SourceBuffer>::ref;
    using RefCounted<SourceBuffer>::deref;

    struct TrackBuffer;

    Document& document() const;

    enum class AppendMode { Segments, Sequence };
    AppendMode mode() const { return m_mode; }
    void setMode(AppendMode, ExceptionCode&);

    bool shouldGenerateTimestamps() const { return m_shouldGenerateTimestamps; }
    void setShouldGenerateTimestamps(bool flag) { m_shouldGenerateTimestamps = flag; }

    void rangeRemoval(const MediaTime&, const MediaTime&);

    bool isBufferedDirty() const { return m_bufferedDirty; }
    void setBufferedDirty(bool flag) { m_bufferedDirty = flag; }

    // ActiveDOMObject API.
    bool hasPendingActivity() const override;

protected:
    // EventTarget interface
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

private:
    SourceBuffer(Ref<SourceBufferPrivate>&&, MediaSource*);

    // ActiveDOMObject API.
    void stop() override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;

    // SourceBufferPrivateClient
    void sourceBufferPrivateDidReceiveInitializationSegment(SourceBufferPrivate*, const InitializationSegment&) override;
    void sourceBufferPrivateDidReceiveSample(SourceBufferPrivate*, MediaSample&) override;
    bool sourceBufferPrivateHasAudio(const SourceBufferPrivate*) const override;
    bool sourceBufferPrivateHasVideo(const SourceBufferPrivate*) const override;
    void sourceBufferPrivateDidBecomeReadyForMoreSamples(SourceBufferPrivate*, AtomicString trackID) override;
    MediaTime sourceBufferPrivateFastSeekTimeForMediaTime(SourceBufferPrivate*, const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold) override;
    void sourceBufferPrivateAppendComplete(SourceBufferPrivate*, AppendResult) override;
    void sourceBufferPrivateDidReceiveRenderingError(SourceBufferPrivate*, int errorCode) override;

    // AudioTrackClient
    void audioTrackEnabledChanged(AudioTrack*) override;

    // VideoTrackClient
    void videoTrackSelectedChanged(VideoTrack*) override;

    // TextTrackClient
    void textTrackKindChanged(TextTrack*) override;
    void textTrackModeChanged(TextTrack*) override;
    void textTrackAddCues(TextTrack*, const TextTrackCueList*) override;
    void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) override;
    void textTrackAddCue(TextTrack*, TextTrackCue&) override;
    void textTrackRemoveCue(TextTrack*, TextTrackCue&) override;

    bool isRemoved() const;
    void scheduleEvent(const AtomicString& eventName);

    void appendBufferInternal(unsigned char*, unsigned, ExceptionCode&);
    void appendBufferTimerFired();
    void resetParserState();

    void setActive(bool);

    bool validateInitializationSegment(const InitializationSegment&);

    void reenqueueMediaForTime(TrackBuffer&, AtomicString trackID, const MediaTime&);
    void provideMediaData(TrackBuffer&, AtomicString trackID);
    void didDropSample();
    void evictCodedFrames(size_t newDataSize);
    size_t maximumBufferSize() const;

    void monitorBufferingRate();

    void removeTimerFired();
    void removeCodedFrames(const MediaTime& start, const MediaTime& end);

    size_t extraMemoryCost() const;
    void reportExtraMemoryAllocated();

    std::unique_ptr<PlatformTimeRanges> bufferedAccountingForEndOfStream() const;

    // Internals
    friend class Internals;
    WEBCORE_EXPORT Vector<String> bufferedSamplesForTrackID(const AtomicString&);

    Ref<SourceBufferPrivate> m_private;
    MediaSource* m_source;
    GenericEventQueue m_asyncEventQueue;
    AppendMode m_mode { AppendMode::Segments };

    Vector<unsigned char> m_pendingAppendData;
    Timer m_appendBufferTimer;

    RefPtr<VideoTrackList> m_videoTracks;
    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;

    Vector<AtomicString> m_videoCodecs;
    Vector<AtomicString> m_audioCodecs;
    Vector<AtomicString> m_textCodecs;

    MediaTime m_timestampOffset;
    MediaTime m_appendWindowStart;
    MediaTime m_appendWindowEnd;

    MediaTime m_groupStartTimestamp;
    MediaTime m_groupEndTimestamp;

    HashMap<AtomicString, TrackBuffer> m_trackBufferMap;
    RefPtr<TimeRanges> m_buffered;
    bool m_bufferedDirty { true };

    enum AppendStateType { WaitingForSegment, ParsingInitSegment, ParsingMediaSegment };
    AppendStateType m_appendState;

    double m_timeOfBufferingMonitor;
    double m_bufferedSinceLastMonitor { 0 };
    double m_averageBufferRate { 0 };

    size_t m_reportedExtraMemoryCost { 0 };

    MediaTime m_pendingRemoveStart;
    MediaTime m_pendingRemoveEnd;
    Timer m_removeTimer;

    bool m_updating { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_active { false };
    bool m_bufferFull { false };
    bool m_shouldGenerateTimestamps { false };
};

} // namespace WebCore

#endif
