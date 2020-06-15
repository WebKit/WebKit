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

#include "Blob.h"
#include "BlobEvent.h"
#include "Document.h"
#include "EventNames.h"
#include "MediaRecorderErrorEvent.h"
#include "MediaRecorderPrivate.h"
#include "MediaRecorderProvider.h"
#include "Page.h"
#include "SharedBuffer.h"
#include "WindowEventLoop.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaRecorder);

MediaRecorder::CreatorFunction MediaRecorder::m_customCreator = nullptr;

ExceptionOr<Ref<MediaRecorder>> MediaRecorder::create(Document& document, Ref<MediaStream>&& stream, Options&& options)
{
    auto privateInstance = MediaRecorder::createMediaRecorderPrivate(document, stream->privateStream());
    if (!privateInstance)
        return Exception { NotSupportedError, "The MediaRecorder is unsupported on this platform"_s };
    auto recorder = adoptRef(*new MediaRecorder(document, WTFMove(stream), WTFMove(privateInstance), WTFMove(options)));
    recorder->suspendIfNeeded();
    recorder->m_private->setErrorCallback([recorder = recorder.copyRef()](auto&& exception) mutable {
        recorder->dispatchError(WTFMove(*exception));
    });
    return recorder;
}

void MediaRecorder::setCustomPrivateRecorderCreator(CreatorFunction creator)
{
    m_customCreator = creator;
}

std::unique_ptr<MediaRecorderPrivate> MediaRecorder::createMediaRecorderPrivate(Document& document, MediaStreamPrivate& stream)
{
    if (m_customCreator)
        return m_customCreator(stream);

#if PLATFORM(COCOA)
    auto* page = document.page();
    if (!page)
        return nullptr;

    return page->mediaRecorderProvider().createMediaRecorderPrivate(stream);
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(stream);
    return nullptr;
#endif
}

MediaRecorder::MediaRecorder(Document& document, Ref<MediaStream>&& stream, std::unique_ptr<MediaRecorderPrivate>&& privateImpl, Options&& option)
    : ActiveDOMObject(document)
    , m_options(WTFMove(option))
    , m_stream(WTFMove(stream))
    , m_private(WTFMove(privateImpl))
{
    m_tracks = WTF::map(m_stream->getTracks(), [] (auto&& track) -> Ref<MediaStreamTrackPrivate> {
        return track->privateTrack();
    });
    m_stream->privateStream().addObserver(*this);
}

MediaRecorder::~MediaRecorder()
{
    m_stream->privateStream().removeObserver(*this);
    stopRecordingInternal();
}

Document* MediaRecorder::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

void MediaRecorder::stop()
{
    m_isActive = false;
    stopRecordingInternal();
}

void MediaRecorder::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    if (!m_isActive || state() == RecordingState::Inactive)
        return;

    stopRecordingInternal();

    queueTaskToDispatchEvent(*this, TaskSource::Networking, MediaRecorderErrorEvent::create(eventNames().errorEvent, Exception { UnknownError, "MediaStream recording was interrupted"_s }));
}

const char* MediaRecorder::activeDOMObjectName() const
{
    return "MediaRecorder";
}

ExceptionOr<void> MediaRecorder::startRecording(Optional<int> timeslice)
{
    UNUSED_PARAM(timeslice);
    if (state() != RecordingState::Inactive)
        return Exception { InvalidStateError, "The MediaRecorder's state must be inactive in order to start recording"_s };
    
    for (auto& track : m_tracks)
        track->addObserver(*this);

    m_state = RecordingState::Recording;
    return { };
}

ExceptionOr<void> MediaRecorder::stopRecording()
{
    if (state() == RecordingState::Inactive)
        return Exception { InvalidStateError, "The MediaRecorder's state cannot be inactive"_s };
    
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this] {
        if (!m_isActive || state() == RecordingState::Inactive)
            return;

        stopRecordingInternal();
        ASSERT(m_state == RecordingState::Inactive);
        m_private->fetchData([this, pendingActivity = makePendingActivity(*this)](auto&& buffer, auto& mimeType) {
            if (!m_isActive)
                return;
    
            dispatchEvent(BlobEvent::create(eventNames().dataavailableEvent, Event::CanBubble::No, Event::IsCancelable::No, buffer ? Blob::create(buffer.releaseNonNull(), mimeType) : Blob::create()));

            if (!m_isActive)
                return;

            dispatchEvent(Event::create(eventNames().stopEvent, Event::CanBubble::No, Event::IsCancelable::No));
        });
    });
    return { };
}

ExceptionOr<void> MediaRecorder::requestData()
{
    if (state() == RecordingState::Inactive)
        return Exception { InvalidStateError, "The MediaRecorder's state cannot be inactive"_s };

    m_private->fetchData([this, pendingActivity = makePendingActivity(*this)](auto&& buffer, auto& mimeType) {
        if (!m_isActive)
            return;

        dispatchEvent(BlobEvent::create(eventNames().dataavailableEvent, Event::CanBubble::No, Event::IsCancelable::No, buffer ? Blob::create(buffer.releaseNonNull(), mimeType) : Blob::create()));
    });
    return { };
}

void MediaRecorder::stopRecordingInternal()
{
    if (state() != RecordingState::Recording)
        return;

    for (auto& track : m_tracks)
        track->removeObserver(*this);

    m_state = RecordingState::Inactive;
    m_private->stopRecording();
}

void MediaRecorder::handleTrackChange()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this] {
        if (!m_isActive || state() == RecordingState::Inactive)
            return;
        stopRecordingInternal();
        dispatchError(Exception { UnknownError, "Track cannot be added to or removed from the MediaStream while recording is happening"_s });
    });
}

void MediaRecorder::dispatchError(Exception&& exception)
{
    if (!m_isActive)
        return;
    dispatchEvent(MediaRecorderErrorEvent::create(eventNames().errorEvent, WTFMove(exception)));
}

void MediaRecorder::trackEnded(MediaStreamTrackPrivate&)
{
    auto position = m_tracks.findMatching([](auto& track) {
        return !track->ended();
    });
    if (position != notFound)
        return;

    stopRecording();
}

bool MediaRecorder::virtualHasPendingActivity() const
{
    return m_state != RecordingState::Inactive;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
