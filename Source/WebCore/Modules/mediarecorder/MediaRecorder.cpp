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
#include "SharedBuffer.h"

#if PLATFORM(COCOA)
#include "MediaRecorderPrivateAVFImpl.h"
#endif

namespace WebCore {

creatorFunction MediaRecorder::m_customCreator = nullptr;

ExceptionOr<Ref<MediaRecorder>> MediaRecorder::create(Document& document, Ref<MediaStream>&& stream, Options&& options)
{
    auto privateInstance = MediaRecorder::getPrivateImpl(stream->privateStream());
    if (!privateInstance)
        return Exception { NotSupportedError, "The MediaRecorder is unsupported on this platform"_s };
    auto recorder = adoptRef(*new MediaRecorder(document, WTFMove(stream), WTFMove(privateInstance), WTFMove(options)));
    recorder->suspendIfNeeded();
    return WTFMove(recorder);
}

void MediaRecorder::setCustomPrivateRecorderCreator(creatorFunction creator)
{
    m_customCreator = creator;
}

std::unique_ptr<MediaRecorderPrivate> MediaRecorder::getPrivateImpl(const MediaStreamPrivate& stream)
{
    if (m_customCreator)
        return m_customCreator();
    
#if PLATFORM(COCOA)
    return MediaRecorderPrivateAVFImpl::create(stream);
#endif
    return nullptr;
}

MediaRecorder::MediaRecorder(Document& document, Ref<MediaStream>&& stream, std::unique_ptr<MediaRecorderPrivate>&& privateImpl, Options&& option)
    : ActiveDOMObject(&document)
    , m_options(WTFMove(option))
    , m_stream(WTFMove(stream))
    , m_private(WTFMove(privateImpl))
{
    m_tracks = WTF::map(m_stream->getTracks(), [] (auto&& track) -> Ref<MediaStreamTrackPrivate> {
        return track->privateTrack();
    });
    m_stream->addObserver(this);
}

MediaRecorder::~MediaRecorder()
{
    m_stream->removeObserver(this);
    stopRecordingInternal();
}

void MediaRecorder::stop()
{
    m_isActive = false;
    stopRecordingInternal();
}

const char* MediaRecorder::activeDOMObjectName() const
{
    return "MediaRecorder";
}

bool MediaRecorder::canSuspendForDocumentSuspension() const
{
    return false; // FIXME: We should do better here as this prevents entering PageCache.
}

ExceptionOr<void> MediaRecorder::startRecording(std::optional<int> timeslice)
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
    
    scheduleDeferredTask([this] {
        if (!m_isActive || state() == RecordingState::Inactive)
            return;

        stopRecordingInternal();
        ASSERT(m_state == RecordingState::Inactive);
        dispatchEvent(BlobEvent::create(eventNames().dataavailableEvent, Event::CanBubble::No, Event::IsCancelable::No, createRecordingDataBlob()));
        if (!m_isActive)
            return;
        dispatchEvent(Event::create(eventNames().stopEvent, Event::CanBubble::No, Event::IsCancelable::No));
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

Ref<Blob> MediaRecorder::createRecordingDataBlob()
{
    auto data = m_private->fetchData();
    if (!data)
        return Blob::create();
    return Blob::create(*data, m_private->mimeType());
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

    stopRecording();
}

void MediaRecorder::sampleBufferUpdated(MediaStreamTrackPrivate& track, MediaSample& mediaSample)
{
    m_private->sampleBufferUpdated(track, mediaSample);
}

void MediaRecorder::audioSamplesAvailable(MediaStreamTrackPrivate& track, const MediaTime& mediaTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)
{
    m_private->audioSamplesAvailable(track, mediaTime, audioData, description, sampleCount);
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

    scriptExecutionContext->postTask([protectedThis = makeRef(*this), function = WTFMove(function)] (auto&) {
        function();
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
