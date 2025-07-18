/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_RECORDER)

#include "Blob.h"
#include "BlobEvent.h"
#include "ContentType.h"
#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "EventNames.h"
#include "MediaRecorderErrorEvent.h"
#include "MediaRecorderPrivate.h"
#include "Page.h"
#include "SharedBuffer.h"
#include "WindowEventLoop.h"
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA) && USE(AVFOUNDATION)
#include "MediaRecorderPrivateAVFImpl.h"
#endif

#if USE(GSTREAMER_TRANSCODER)
#include "MediaRecorderPrivateGStreamer.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaRecorder);

MediaRecorder::CreatorFunction MediaRecorder::m_customCreator = nullptr;

bool MediaRecorder::isTypeSupported(Document& document, const String& value)
{
#if PLATFORM(COCOA) || USE(GSTREAMER_TRANSCODER)
    if (value.isEmpty())
        return true;

    ContentType mimeType(value);
#if PLATFORM(COCOA)
    return MediaRecorderPrivateAVFImpl::isTypeSupported(document, mimeType);
#elif USE(GSTREAMER_TRANSCODER)
    UNUSED_PARAM(document);
    return MediaRecorderPrivateGStreamer::isTypeSupported(mimeType);
#endif
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(value);
    return false;
#endif
}

ExceptionOr<Ref<MediaRecorder>> MediaRecorder::create(Document& document, Ref<MediaStream>&& stream, Options&& options)
{
    auto* page = document.page();
    if (!page)
        return Exception { ExceptionCode::InvalidStateError };

    if (!isTypeSupported(document, options.mimeType))
        return Exception { ExceptionCode::NotSupportedError, "mimeType is not supported"_s };

    auto recorder = adoptRef(*new MediaRecorder(document, WTFMove(stream), WTFMove(options)));
    recorder->suspendIfNeeded();
    return recorder;
}

void MediaRecorder::setCustomPrivateRecorderCreator(CreatorFunction creator)
{
    m_customCreator = creator;
}

ExceptionOr<std::unique_ptr<MediaRecorderPrivate>> MediaRecorder::createMediaRecorderPrivate(MediaStreamPrivate& stream, const Options& options)
{
    if (m_customCreator)
        return m_customCreator(stream, options);

#if PLATFORM(COCOA) && USE(AVFOUNDATION)
    std::unique_ptr<MediaRecorderPrivate> result = MediaRecorderPrivateAVFImpl::create(stream, options);
#elif USE(GSTREAMER_TRANSCODER)
    std::unique_ptr<MediaRecorderPrivate> result = MediaRecorderPrivateGStreamer::create(stream, options);
#else
    std::unique_ptr<MediaRecorderPrivate> result;
#endif
    if (!result)
        return Exception { ExceptionCode::NotSupportedError, "The MediaRecorder is unsupported on this platform"_s };
    return result;
}

MediaRecorder::MediaRecorder(Document& document, Ref<MediaStream>&& stream, Options&& options)
    : ActiveDOMObject(document)
    , m_options(WTFMove(options))
    , m_stream(WTFMove(stream))
    , m_timeSliceTimer(*this, &MediaRecorder::timeSlicerTimerFired)
{
    computeInitialBitRates();

    m_tracks = m_stream->privateStream().tracks();
    m_stream->privateStream().addObserver(*this);
}

MediaRecorder::~MediaRecorder()
{
    m_stream->privateStream().removeObserver(*this);
    stopRecordingInternal();
}

void MediaRecorder::timeSlicerTimerFired()
{
    requestDataInternal(ReturnDataIfEmpty::No);
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

    queueTaskToDispatchEvent(*this, TaskSource::Networking, MediaRecorderErrorEvent::create(eventNames().errorEvent, Exception { ExceptionCode::UnknownError, "MediaStream recording was interrupted"_s }));
}

ExceptionOr<void> MediaRecorder::startRecording(std::optional<unsigned> timeSlice)
{
    if (!m_isActive)
        return Exception { ExceptionCode::InvalidStateError, "The MediaRecorder is not active"_s };

    if (state() != RecordingState::Inactive)
        return Exception { ExceptionCode::InvalidStateError, "The MediaRecorder's state must be inactive in order to start recording"_s };

    updateBitRates();

    Options options { m_options };
    options.audioBitsPerSecond = m_audioBitsPerSecond;
    options.videoBitsPerSecond = m_videoBitsPerSecond;

    ASSERT(!m_private);
    auto result = createMediaRecorderPrivate(m_stream->privateStream(), options);

    if (result.hasException())
        return result.releaseException();

    m_private = result.releaseReturnValue();
    m_private->startRecording([pendingActivity = makePendingActivity(*this)](auto&& mimeTypeOrException, unsigned audioBitsPerSecond, unsigned videoBitsPerSecond) mutable {
        if (!pendingActivity->object().m_isActive)
            return;

        if (mimeTypeOrException.hasException()) {
            pendingActivity->object().stopRecordingInternal();
            queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::Networking, [exception = mimeTypeOrException.releaseException()](auto& recorder) mutable {
                if (!recorder.m_isActive)
                    return;
                recorder.dispatchError(WTFMove(exception));
            });
            return;
        }

        queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::Networking, [mimeType = mimeTypeOrException.releaseReturnValue(), audioBitsPerSecond, videoBitsPerSecond](auto& recorder) mutable {
            if (!recorder.m_isActive)
                return;
            recorder.m_options.mimeType = WTFMove(mimeType);
            recorder.m_options.audioBitsPerSecond = audioBitsPerSecond;
            recorder.m_options.videoBitsPerSecond = videoBitsPerSecond;

            recorder.dispatchEvent(Event::create(eventNames().startEvent, Event::CanBubble::No, Event::IsCancelable::No));
        });
    });

    for (auto& track : m_tracks)
        track->addObserver(*this);

    m_state = RecordingState::Recording;
    m_timeSlice = timeSlice ? std::make_optional(std::max(m_mimimumTimeSlice, *timeSlice)) : std::nullopt;
    if (m_timeSlice)
        m_timeSliceTimer.startOneShot(Seconds::fromMilliseconds(*m_timeSlice));
    return { };
}

static inline Ref<BlobEvent> createDataAvailableEvent(ScriptExecutionContext* context, RefPtr<FragmentedSharedBuffer>&& buffer, const String& mimeType, double timeCode)
{
    auto blob = buffer ? Blob::create(context, buffer->extractData(), mimeType) : Blob::create(context);
    return BlobEvent::create(eventNames().dataavailableEvent, BlobEvent::Init { { false, false, false }, WTFMove(blob), timeCode }, BlobEvent::IsTrusted::Yes);
}

void MediaRecorder::stopRecording()
{
    if (state() == RecordingState::Inactive)
        return;

    updateBitRates();

    stopRecordingInternal();
    fetchData([](auto& recorder, auto&& buffer, auto& mimeType, auto timeCode) {
        if (!recorder.m_isActive)
            return;

        RefPtr scriptExecutionContext = recorder.scriptExecutionContext();
        recorder.dispatchEvent(createDataAvailableEvent(scriptExecutionContext.get(), WTFMove(buffer), mimeType, timeCode));

        if (!recorder.m_isActive)
            return;
        recorder.dispatchEvent(Event::create(eventNames().stopEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }, TakePrivateRecorder::Yes);
    return;
}

ExceptionOr<void> MediaRecorder::requestData()
{
    return requestDataInternal(ReturnDataIfEmpty::Yes);
}

ExceptionOr<void> MediaRecorder::requestDataInternal(ReturnDataIfEmpty returnDataIfEmpty)
{
    if (state() == RecordingState::Inactive)
        return Exception { ExceptionCode::InvalidStateError, "The MediaRecorder's state cannot be inactive"_s };

    if (m_timeSliceTimer.isActive())
        m_timeSliceTimer.stop();

    fetchData([returnDataIfEmpty](auto& recorder, auto&& buffer, auto& mimeType, auto timeCode) {
        if (returnDataIfEmpty == ReturnDataIfEmpty::Yes || !buffer->isEmpty()) {
            RefPtr scriptExecutionContext = recorder.scriptExecutionContext();
            recorder.dispatchEvent(createDataAvailableEvent(scriptExecutionContext.get(), WTFMove(buffer), mimeType, timeCode));
        }

        switch (recorder.state()) {
        case RecordingState::Inactive:
            break;
        case RecordingState::Recording:
            ASSERT(recorder.m_isActive);
            if (recorder.m_timeSlice)
                recorder.m_timeSliceTimer.startOneShot(Seconds::fromMilliseconds(*recorder.m_timeSlice));
            break;
        case RecordingState::Paused:
            if (recorder.m_timeSlice)
                recorder.m_nextFireInterval = Seconds::fromMilliseconds(*recorder.m_timeSlice);
            break;
        }
    }, TakePrivateRecorder::No);
    return { };
}

ExceptionOr<void> MediaRecorder::pauseRecording()
{
    if (state() == RecordingState::Inactive)
        return Exception { ExceptionCode::InvalidStateError, "The MediaRecorder's state cannot be inactive"_s };

    if (state() == RecordingState::Paused)
        return { };

    m_state = RecordingState::Paused;

    if (m_timeSliceTimer.isActive()) {
        m_nextFireInterval = m_timeSliceTimer.nextFireInterval();
        m_timeSliceTimer.stop();
    }

    m_private->pause([pendingActivity = makePendingActivity(*this)]() {
        if (!pendingActivity->object().m_isActive)
            return;
        queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::Networking, [](auto& recorder) mutable {
            if (!recorder.m_isActive)
                return;
            recorder.dispatchEvent(Event::create(eventNames().pauseEvent, Event::CanBubble::No, Event::IsCancelable::No));
        });
    });
    return { };
}

ExceptionOr<void> MediaRecorder::resumeRecording()
{
    if (state() == RecordingState::Inactive)
        return Exception { ExceptionCode::InvalidStateError, "The MediaRecorder's state cannot be inactive"_s };

    if (state() == RecordingState::Recording)
        return { };

    m_state = RecordingState::Recording;

    if (m_nextFireInterval) {
        m_timeSliceTimer.startOneShot(*m_nextFireInterval);
        m_nextFireInterval = { };
    }

    m_private->resume([pendingActivity = makePendingActivity(*this)]() {
        if (!pendingActivity->object().m_isActive)
            return;
        queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::Networking, [](auto& recorder) mutable {
            if (!recorder.m_isActive)
                return;
            recorder.dispatchEvent(Event::create(eventNames().resumeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        });
    });
    return { };
}

void MediaRecorder::fetchData(FetchDataCallback&& callback, TakePrivateRecorder takeRecorder)
{
    auto& privateRecorder = *m_private;

    std::unique_ptr<MediaRecorderPrivate> takenPrivateRecorder;
    if (takeRecorder == TakePrivateRecorder::Yes)
        takenPrivateRecorder = WTFMove(m_private);

    FetchDataCallback fetchDataCallback = [privateRecorder = WTFMove(takenPrivateRecorder), callback = WTFMove(callback)](auto& recorder, auto&& buffer, auto& mimeType, auto timeCode) mutable {
        queueTaskKeepingObjectAlive(recorder, TaskSource::Networking, [buffer = WTFMove(buffer), mimeType, timeCode, callback = WTFMove(callback)](auto& recorder) mutable {
            callback(recorder, WTFMove(buffer), mimeType, timeCode);
        });
    };

    if (m_isFetchingData) {
        m_pendingFetchDataTasks.append(WTFMove(fetchDataCallback));
        return;
    }

    m_isFetchingData = true;
    privateRecorder.fetchData([pendingActivity = makePendingActivity(*this), callback = WTFMove(fetchDataCallback)](auto&& buffer, auto& mimeType, auto timeCode) mutable {
        pendingActivity->object().m_isFetchingData = false;
        callback(pendingActivity->object(), WTFMove(buffer), mimeType, timeCode);
        for (auto& task : std::exchange(pendingActivity->object().m_pendingFetchDataTasks, { }))
            task(pendingActivity->object(), FragmentedSharedBuffer::create(), mimeType, timeCode);
    });
}

void MediaRecorder::stopRecordingInternal(CompletionHandler<void()>&& completionHandler)
{
    if (state() == RecordingState::Inactive) {
        completionHandler();
        return;
    }

    for (auto& track : m_tracks)
        track->removeObserver(*this);

    m_state = RecordingState::Inactive;
    m_private->stop(WTFMove(completionHandler));
}

void MediaRecorder::handleTrackChange()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [](auto& recorder) {
        recorder.stopRecordingInternal([pendingActivity = recorder.makePendingActivity(recorder)] {
            Ref protectedRecorder = pendingActivity->object();
            queueTaskKeepingObjectAlive(protectedRecorder.get(), TaskSource::Networking, [](auto& recorder) {
                if (!recorder.m_isActive)
                    return;
                recorder.dispatchError(Exception { ExceptionCode::InvalidModificationError, "Track cannot be added to or removed from the MediaStream while recording"_s });

                if (!recorder.m_isActive)
                    return;
                recorder.dispatchEvent(createDataAvailableEvent(recorder.scriptExecutionContext(), { }, { }, 0));

                if (!recorder.m_isActive)
                    return;
                recorder.dispatchEvent(Event::create(eventNames().stopEvent, Event::CanBubble::No, Event::IsCancelable::No));
            });
        });
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
    auto position = m_tracks.findIf([](auto& track) {
        return !track->ended();
    });
    if (position != notFound)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [](auto& recorder) {
        recorder.stopRecordingInternal([pendingActivity = recorder.makePendingActivity(recorder)] {
            queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::Networking, [](auto& recorder) {
                if (!recorder.m_isActive)
                    return;
                recorder.dispatchEvent(createDataAvailableEvent(recorder.scriptExecutionContext(), { }, { }, 0));

                if (!recorder.m_isActive)
                    return;
                recorder.dispatchEvent(Event::create(eventNames().stopEvent, Event::CanBubble::No, Event::IsCancelable::No));
            });
        });
    });
}

void MediaRecorder::trackMutedChanged(MediaStreamTrackPrivate& track)
{
    if (m_private)
        m_private->trackMutedChanged(track);
}

void MediaRecorder::trackEnabledChanged(MediaStreamTrackPrivate& track)
{
    if (m_private)
        m_private->trackEnabledChanged(track);
}

bool MediaRecorder::virtualHasPendingActivity() const
{
    return m_state != RecordingState::Inactive;
}

void MediaRecorder::computeBitRates(const MediaStreamPrivate* stream)
{
    auto bitRates = MediaRecorderPrivate::computeBitRates(m_options, stream);
    m_audioBitsPerSecond = bitRates.audio;
    m_videoBitsPerSecond = bitRates.video;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
