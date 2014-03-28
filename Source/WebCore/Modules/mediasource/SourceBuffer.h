/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef SourceBuffer_h
#define SourceBuffer_h

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
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioTrackList;
class MediaSource;
class SourceBufferPrivate;
class TextTrackList;
class TimeRanges;
class VideoTrackList;

class SourceBuffer final : public RefCounted<SourceBuffer>, public ActiveDOMObject, public EventTargetWithInlineData, public ScriptWrappable, public SourceBufferPrivateClient, public AudioTrackClient, public VideoTrackClient, public TextTrackClient {
public:
    static PassRef<SourceBuffer> create(PassRef<SourceBufferPrivate>, MediaSource*);

    virtual ~SourceBuffer();

    // SourceBuffer.idl methods
    bool updating() const { return m_updating; }
    PassRefPtr<TimeRanges> buffered(ExceptionCode&) const;
    const RefPtr<TimeRanges>& buffered() const;
    double timestampOffset() const;
    void setTimestampOffset(double, ExceptionCode&);
    void appendBuffer(PassRefPtr<ArrayBuffer> data, ExceptionCode&);
    void appendBuffer(PassRefPtr<ArrayBufferView> data, ExceptionCode&);
    void abort(ExceptionCode&);
    void remove(double start, double end, ExceptionCode&);

    void abortIfUpdating();
    void removedFromMediaSource();
    const MediaTime& highestPresentationEndTimestamp() const { return m_highestPresentationEndTimestamp; }

#if ENABLE(VIDEO_TRACK)
    VideoTrackList* videoTracks();
    AudioTrackList* audioTracks();
    TextTrackList* textTracks();
#endif

    bool hasCurrentTime() const;
    bool hasFutureTime() const;
    bool canPlayThrough();

    bool active() const { return m_active; }

    // ActiveDOMObject interface
    virtual bool hasPendingActivity() const override;
    virtual void stop() override;

    // EventTarget interface
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    virtual EventTargetInterface eventTargetInterface() const override { return SourceBufferEventTargetInterfaceType; }

    using RefCounted<SourceBuffer>::ref;
    using RefCounted<SourceBuffer>::deref;

protected:
    // EventTarget interface
    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

private:
    SourceBuffer(PassRef<SourceBufferPrivate>, MediaSource*);

    // SourceBufferPrivateClient
    virtual void sourceBufferPrivateDidEndStream(SourceBufferPrivate*, const WTF::AtomicString&) override;
    virtual void sourceBufferPrivateDidReceiveInitializationSegment(SourceBufferPrivate*, const InitializationSegment&) override;
    virtual void sourceBufferPrivateDidReceiveSample(SourceBufferPrivate*, PassRefPtr<MediaSample>) override;
    virtual bool sourceBufferPrivateHasAudio(const SourceBufferPrivate*) const override;
    virtual bool sourceBufferPrivateHasVideo(const SourceBufferPrivate*) const override;
    virtual void sourceBufferPrivateDidBecomeReadyForMoreSamples(SourceBufferPrivate*, AtomicString trackID) override;
    virtual void sourceBufferPrivateSeekToTime(SourceBufferPrivate*, const MediaTime&);
    virtual MediaTime sourceBufferPrivateFastSeekTimeForMediaTime(SourceBufferPrivate*, const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);


    // AudioTrackClient
    virtual void audioTrackEnabledChanged(AudioTrack*) override;

    // VideoTrackClient
    virtual void videoTrackSelectedChanged(VideoTrack*) override;

    // TextTrackClient
    virtual void textTrackKindChanged(TextTrack*) override;
    virtual void textTrackModeChanged(TextTrack*) override;
    virtual void textTrackAddCues(TextTrack*, const TextTrackCueList*) override;
    virtual void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) override;
    virtual void textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue>) override;
    virtual void textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue>) override;

    static const WTF::AtomicString& decodeError();
    static const WTF::AtomicString& networkError();

    bool isRemoved() const;
    void scheduleEvent(const AtomicString& eventName);

    void appendBufferInternal(unsigned char*, unsigned, ExceptionCode&);
    void appendBufferTimerFired(Timer<SourceBuffer>&);

    void setActive(bool);

    bool validateInitializationSegment(const InitializationSegment&);

    struct TrackBuffer;
    void provideMediaData(TrackBuffer&, AtomicString trackID);
    void didDropSample();

    void monitorBufferingRate();

    void removeTimerFired(Timer<SourceBuffer>*);
    void removeCodedFrames(const MediaTime& start, const MediaTime& end);

    RefPtr<SourceBufferPrivate> m_private;
    MediaSource* m_source;
    GenericEventQueue m_asyncEventQueue;

    bool m_updating;

    Vector<unsigned char> m_pendingAppendData;
    Timer<SourceBuffer> m_appendBufferTimer;

    RefPtr<VideoTrackList> m_videoTracks;
    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;

    Vector<AtomicString> m_videoCodecs;
    Vector<AtomicString> m_audioCodecs;
    Vector<AtomicString> m_textCodecs;

    MediaTime m_timestampOffset;
    MediaTime m_highestPresentationEndTimestamp;

    HashMap<AtomicString, TrackBuffer> m_trackBufferMap;
    bool m_receivedFirstInitializationSegment;
    RefPtr<TimeRanges> m_buffered;
    bool m_active;

    enum AppendStateType { WaitingForSegment, ParsingInitSegment, ParsingMediaSegment };
    AppendStateType m_appendState;

    double m_timeOfBufferingMonitor;
    double m_bufferedSinceLastMonitor;
    double m_averageBufferRate;

    MediaTime m_pendingRemoveStart;
    MediaTime m_pendingRemoveEnd;
    Timer<SourceBuffer> m_removeTimer;
};

} // namespace WebCore

#endif

#endif
