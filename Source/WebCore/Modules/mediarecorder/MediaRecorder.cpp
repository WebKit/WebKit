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


#include "config.h"
#include "MediaRecorder.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "EventNames.h"
#include "MediaRecorderErrorEvent.h"

namespace WebCore {

Ref<MediaRecorder> MediaRecorder::create(Document& document, Ref<MediaStream>&& stream, Options&& options)
{
    auto recorder = adoptRef(*new MediaRecorder(document, WTFMove(stream), WTFMove(options)));
    recorder->suspendIfNeeded();
    return recorder;
}

MediaRecorder::MediaRecorder(Document& document, Ref<MediaStream>&& stream, Options&& option)
    : ActiveDOMObject(&document)
    , m_options(WTFMove(option))
    , m_stream(WTFMove(stream))
{
    m_tracks = WTF::map(m_stream->getTracks(), [this] (const auto& track) -> Ref<MediaStreamTrackPrivate> {
        auto& privateTrack = track->privateTrack();
        privateTrack.addObserver(*this);
        return privateTrack;
    });
    m_stream->addObserver(this);
}

MediaRecorder::~MediaRecorder()
{
    m_stream->removeObserver(this);
    for (auto& track : m_tracks)
        track->removeObserver(*this);
}

void MediaRecorder::stop()
{
    m_isActive = false;
}

const char* MediaRecorder::activeDOMObjectName() const
{
    return "MediaRecorder";
}

bool MediaRecorder::canSuspendForDocumentSuspension() const
{
    return false; // FIXME: We should do better here as this prevents entering PageCache.
}

ExceptionOr<void> MediaRecorder::start(std::optional<int> timeslice)
{
    UNUSED_PARAM(timeslice);
    if (state() != RecordingState::Inactive)
        return Exception { InvalidStateError, "The MediaRecorder's state must be inactive in order to start recording"_s };
    
    m_state = RecordingState::Recording;
    return { };
}

void MediaRecorder::didAddOrRemoveTrack()
{
    scheduleDeferredTask([this] {
        if (!m_isActive || state() == RecordingState::Inactive)
            return;
        auto event = MediaRecorderErrorEvent::create(eventNames().errorEvent, Exception { UnknownError, "Track cannot be added to or removed from the MediaStream while recording is happening"_s });
        setNewRecordingState(RecordingState::Inactive, WTFMove(event));
    });
}

void MediaRecorder::trackEnded(MediaStreamTrackPrivate&)
{
    auto position = m_tracks.findMatching([](auto& track) {
        return !track->ended();
    });
    if (position != notFound)
        return;
    scheduleDeferredTask([this] {
        if (!m_isActive || state() == RecordingState::Inactive)
            return;
        // FIXME: Add a dataavailable event
        auto event = Event::create(eventNames().stopEvent, Event::CanBubble::No, Event::IsCancelable::No);
        setNewRecordingState(RecordingState::Inactive, WTFMove(event));
    });
}

void MediaRecorder::setNewRecordingState(RecordingState newState, Ref<Event>&& event)
{
    m_state = newState;
    dispatchEvent(WTFMove(event));
}

void MediaRecorder::scheduleDeferredTask(Function<void()>&& function)
{
    ASSERT(function);
    auto* scriptExecutionContext = this->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;
    scriptExecutionContext->postTask([weakThis = makeWeakPtr(*this), function = WTFMove(function)] (auto&) {
        if (!weakThis)
            return;

        function();
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
