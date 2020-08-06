/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaStream.h"
#include "MediaStreamTrackPrivate.h"
#include "Timer.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class Blob;
class Document;
class MediaRecorderPrivate;

class MediaRecorder final
    : public ActiveDOMObject
    , public RefCounted<MediaRecorder>
    , public EventTargetWithInlineData
    , private MediaStreamPrivate::Observer
    , private MediaStreamTrackPrivate::Observer {
    WTF_MAKE_ISO_ALLOCATED(MediaRecorder);
public:
    enum class RecordingState { Inactive, Recording, Paused };
    
    struct Options {
        String mimeType;
        unsigned audioBitsPerSecond;
        unsigned videoBitsPerSecond;
        unsigned bitsPerSecond;
    };
    
    ~MediaRecorder();
    
    static ExceptionOr<Ref<MediaRecorder>> create(Document&, Ref<MediaStream>&&, Options&& = { });
    
    using CreatorFunction = std::unique_ptr<MediaRecorderPrivate>(*)(MediaStreamPrivate&);

    WEBCORE_EXPORT static void setCustomPrivateRecorderCreator(CreatorFunction);
    
    RecordingState state() const { return m_state; }
    
    using RefCounted::ref;
    using RefCounted::deref;
    
    ExceptionOr<void> startRecording(Optional<unsigned>);
    ExceptionOr<void> stopRecording();
    ExceptionOr<void> requestData();

    MediaStream& stream() { return m_stream.get(); }

private:
    MediaRecorder(Document&, Ref<MediaStream>&&, Options&& = { });

    static std::unique_ptr<MediaRecorderPrivate> createMediaRecorderPrivate(Document&, MediaStreamPrivate&);
    
    Document* document() const;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return MediaRecorderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject API.
    void suspend(ReasonForSuspension) final;
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;
    
    void stopRecordingInternal();

    void dispatchError(Exception&&);

    // MediaStream::Observer
    void didAddTrack(MediaStreamTrackPrivate&) final { handleTrackChange(); }
    void didRemoveTrack(MediaStreamTrackPrivate&) final { handleTrackChange(); }

    void handleTrackChange();

    // MediaStreamTrackPrivate::Observer
    void trackEnded(MediaStreamTrackPrivate&) final;
    void trackMutedChanged(MediaStreamTrackPrivate&) final { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };
    void trackEnabledChanged(MediaStreamTrackPrivate&) final { };

    static CreatorFunction m_customCreator;
    
    Options m_options;
    Ref<MediaStream> m_stream;
    std::unique_ptr<MediaRecorderPrivate> m_private;
    RecordingState m_state { RecordingState::Inactive };
    Vector<Ref<MediaStreamTrackPrivate>> m_tracks;
    Optional<unsigned> m_timeSlice;
    Timer m_timeSliceTimer;
    
    bool m_isActive { true };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
