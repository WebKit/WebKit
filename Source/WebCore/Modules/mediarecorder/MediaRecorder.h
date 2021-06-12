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
#include "ExceptionOr.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaStream.h"
#include "MediaStreamTrackPrivate.h"
#include "Timer.h"
#include <wtf/Deque.h>
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
    
    ~MediaRecorder();
    
    static bool isTypeSupported(Document&, const String&);

    using Options = MediaRecorderPrivateOptions;
    static ExceptionOr<Ref<MediaRecorder>> create(Document&, Ref<MediaStream>&&, Options&& = { });

    using CreatorFunction = ExceptionOr<std::unique_ptr<MediaRecorderPrivate>> (*)(MediaStreamPrivate&, const Options&);

    WEBCORE_EXPORT static void setCustomPrivateRecorderCreator(CreatorFunction);

    RecordingState state() const { return m_state; }
    const String& mimeType() const { return m_options.mimeType; }

    using RefCounted::ref;
    using RefCounted::deref;
    
    ExceptionOr<void> startRecording(std::optional<unsigned>);
    void stopRecording();
    ExceptionOr<void> requestData();
    ExceptionOr<void> pauseRecording();
    ExceptionOr<void> resumeRecording();

    unsigned videoBitsPerSecond() const { return m_options.videoBitsPerSecond.value_or(0); }
    unsigned audioBitsPerSecond() const { return m_options.audioBitsPerSecond.value_or(0); }

    MediaStream& stream() { return m_stream.get(); }

private:
    MediaRecorder(Document&, Ref<MediaStream>&&, Options&&);

    static ExceptionOr<std::unique_ptr<MediaRecorderPrivate>> createMediaRecorderPrivate(Document&, MediaStreamPrivate&, const Options&);
    
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
    
        void stopRecordingInternal(CompletionHandler<void()>&& = [] { });
    void dispatchError(Exception&&);

    enum class TakePrivateRecorder { No, Yes };
    using FetchDataCallback = Function<void(RefPtr<SharedBuffer>&&, const String& mimeType, double)>;
    void fetchData(FetchDataCallback&&, TakePrivateRecorder);

    // MediaStream::Observer
    void didAddTrack(MediaStreamTrackPrivate&) final { handleTrackChange(); }
    void didRemoveTrack(MediaStreamTrackPrivate&) final { handleTrackChange(); }

    void handleTrackChange();

    // MediaStreamTrackPrivate::Observer
    void trackEnded(MediaStreamTrackPrivate&) final;
    void trackMutedChanged(MediaStreamTrackPrivate&) final;
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };

    static CreatorFunction m_customCreator;

    Options m_options;
    Ref<MediaStream> m_stream;
    std::unique_ptr<MediaRecorderPrivate> m_private;
    RecordingState m_state { RecordingState::Inactive };
    Vector<Ref<MediaStreamTrackPrivate>> m_tracks;
    std::optional<unsigned> m_timeSlice;
    Timer m_timeSliceTimer;
    
    bool m_isActive { true };
    bool m_isFetchingData { false };
    Deque<FetchDataCallback> m_pendingFetchDataTasks;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
