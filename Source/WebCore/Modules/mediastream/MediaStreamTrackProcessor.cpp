/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "MediaStreamTrackProcessor.h"

#if ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)

#include "CSSStyleImageValue.h"
#include "ContextDestructionObserverInlines.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "JSWebCodecsVideoFrame.h"
#include "Logging.h"
#include "OffscreenCanvas.h"
#include "ReadableStream.h"
#include "SVGImageElement.h"
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStreamTrackProcessor);

ExceptionOr<Ref<MediaStreamTrackProcessor>> MediaStreamTrackProcessor::create(ScriptExecutionContext& context, Init&& init)
{
    RefPtr<MediaStreamTrackHandle> handle;
    if (auto* trackHandle = std::get_if<Ref<MediaStreamTrackHandle>>(&init.track)) {
        handle = WTF::move(*trackHandle);
        if (handle->isDetached())
            return Exception { ExceptionCode::TypeError, "Track handle is detached"_s };
        if (!protect(protect(handle->trackSourceObserver())->source())->isVideo())
            return Exception { ExceptionCode::TypeError, "Track is not video"_s };
    } else {
        Ref track = std::get<Ref<MediaStreamTrack>>(init.track);
        if (!track->isVideo())
            return Exception { ExceptionCode::TypeError, "Track is not video"_s };
        if (track->ended())
            return Exception { ExceptionCode::TypeError, "Track is ended"_s };
        auto handleOrException = MediaStreamTrackHandle::create(track.get());
        if (handleOrException.hasException())
            return handleOrException.releaseException();
        handle = handleOrException.releaseReturnValue();
    }

    return adoptRef(*new MediaStreamTrackProcessor(context, handle.releaseNonNull(), init.maxBufferSize.value_or(1)));
}

MediaStreamTrackProcessor::MediaStreamTrackProcessor(ScriptExecutionContext& context, Ref<MediaStreamTrackHandle>&& trackHandle, unsigned short maxVideoFramesCount)
    : ContextDestructionObserver(&context)
    , m_trackKeeper(trackHandle->trackKeeper())
    , m_videoFrameObserverWrapper(VideoFrameObserverWrapper::create(context.identifier(), *this, protect(protect(trackHandle->trackSourceObserver())->source()), maxVideoFramesCount))
    , m_trackObserver(TrackObserverWrapper::create(context, *this, WTF::move(trackHandle)))
{
    m_trackObserver->start();
}

MediaStreamTrackProcessor::~MediaStreamTrackProcessor()
{
    stopObserving();
}

ExceptionOr<Ref<ReadableStream>> MediaStreamTrackProcessor::readable(JSC::JSGlobalObject& globalObject)
{
    if (!m_readable) {
        if (!m_readableStreamSource)
            lazyInitialize(m_readableStreamSource, makeUniqueWithoutRefCountedCheck<Source>(*this));
        auto readableOrException = ReadableStream::create(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject), *m_readableStreamSource);
        if (readableOrException.hasException()) {
            m_readableStreamSource->setAsCancelled();
            return readableOrException.releaseException();
        }
        m_readable = readableOrException.releaseReturnValue();
        if (!m_isTrackEnded)
            Ref { *m_videoFrameObserverWrapper }->start();
    }

    return Ref { *m_readable };
}

void MediaStreamTrackProcessor::contextDestroyed()
{
    m_trackKeeper = nullptr;
    if (m_readableStreamSource)
        m_readableStreamSource->setAsCancelled();
    stopObserving();
}

void MediaStreamTrackProcessor::stopObserving()
{
    m_videoFrameObserverWrapper = nullptr;
    m_trackObserver->stop();
}

void MediaStreamTrackProcessor::tryEnqueueingVideoFrame()
{
    RefPtr context = scriptExecutionContext();
    RefPtr videoFrameObserverWrapper = m_videoFrameObserverWrapper;
    if (!context || !videoFrameObserverWrapper || !m_readable)
        return;

    if (m_readableStreamSource->isCancelled() || !m_readableStreamSource->isEnabled())
        return;

    // FIXME: If the stream is waiting, we might want to buffer based on
    // https://w3c.github.io/mediacapture-transform/#dom-mediastreamtrackprocessorinit-maxbuffersize.
    if (!m_readableStreamSource->isWaiting())
        return;

    if (auto videoFrame = videoFrameObserverWrapper->takeVideoFrame(*context))
        m_readableStreamSource->enqueue(*videoFrame, *context);
}

void MediaStreamTrackProcessor::trackEnded()
{
    ASSERT(!m_isTrackEnded);
    m_isTrackEnded = true;
    m_trackKeeper = nullptr;
    if (m_readableStreamSource)
        m_readableStreamSource->trackEnded();
}

Ref<MediaStreamTrackProcessor::VideoFrameObserverWrapper> MediaStreamTrackProcessor::VideoFrameObserverWrapper::create(ScriptExecutionContextIdentifier identifier, MediaStreamTrackProcessor& processor, Ref<RealtimeMediaSource>&& source, unsigned short maxVideoFramesCount)
{
#if PLATFORM(COCOA)
    if (source->deviceType() == CaptureDevice::DeviceType::Camera)
        maxVideoFramesCount = 1;
#endif
    return adoptRef(*new VideoFrameObserverWrapper(identifier, processor, WTF::move(source), maxVideoFramesCount));
}

MediaStreamTrackProcessor::VideoFrameObserverWrapper::VideoFrameObserverWrapper(ScriptExecutionContextIdentifier identifier, MediaStreamTrackProcessor& processor, Ref<RealtimeMediaSource>&& source, unsigned short maxVideoFramesCount)
    : m_observer(makeUniqueRef<VideoFrameObserver>(identifier, processor, WTF::move(source), maxVideoFramesCount))
{
}

void MediaStreamTrackProcessor::VideoFrameObserverWrapper::start()
{
    ASSERT(m_observer->isContextThread());
    callOnMainThreadAndWait([protectedThis = Ref { *this }] {
        protectedThis->m_observer->start();
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStreamTrackProcessor::VideoFrameObserver);

MediaStreamTrackProcessor::VideoFrameObserver::VideoFrameObserver(ScriptExecutionContextIdentifier identifier, WeakPtr<MediaStreamTrackProcessor>&& processor, Ref<RealtimeMediaSource>&& source, unsigned short maxVideoFramesCount)
    : m_realtimeVideoSource(WTF::move(source))
    , m_contextIdentifier(identifier)
    , m_processor(WTF::move(processor))
    , m_maxVideoFramesCount(maxVideoFramesCount)
{
    ASSERT(isContextThread());
}

void MediaStreamTrackProcessor::VideoFrameObserver::start()
{
    assertIsMainThread();
    m_isStarted = true;
    m_realtimeVideoSource->addVideoFrameObserver(*this);
}

MediaStreamTrackProcessor::VideoFrameObserver::~VideoFrameObserver()
{
    assertIsMainThread();
    if (m_isStarted)
        m_realtimeVideoSource->removeVideoFrameObserver(*this);
}

RefPtr<WebCodecsVideoFrame> MediaStreamTrackProcessor::VideoFrameObserver::takeVideoFrame(ScriptExecutionContext& context)
{
    ASSERT(isContextThread());

    RefPtr<VideoFrame> videoFrame;
    {
        Locker lock(m_videoFramesLock);
        if (m_videoFrames.isEmpty())
            return nullptr;

        videoFrame = m_videoFrames.takeFirst();
    }

    WebCodecsVideoFrame::BufferInit init {
        .format = convertVideoFramePixelFormat(videoFrame->pixelFormat()),
        .codedWidth = static_cast<size_t>(videoFrame->presentationSize().width()),
        .codedHeight = static_cast<size_t>(videoFrame->presentationSize().height()),
        .timestamp = Seconds(videoFrame->presentationTime().toDouble()).microsecondsAs<int64_t>(),
        .colorSpace = videoFrame->colorSpace()
    };

    return WebCodecsVideoFrame::create(context, videoFrame.releaseNonNull(), WTF::move(init));
}

void MediaStreamTrackProcessor::VideoFrameObserver::videoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata)
{
    // Can be called on any thread.
    {
        Locker lock(m_videoFramesLock);
        m_videoFrames.append(frame);
        if (m_videoFrames.size() > m_maxVideoFramesCount) {
            RELEASE_LOG_DEBUG(WebRTC, "MediaStreamTrackProcessor::VideoFrameObserver::videoFrameAvailable buffer is full");
            m_videoFrames.takeFirst();
        }
    }
    ScriptExecutionContext::postTaskTo(m_contextIdentifier, [processor = m_processor] (auto&) mutable {
        if (RefPtr protectedProcessor = processor.get())
            protectedProcessor->tryEnqueueingVideoFrame();
    });
}

using MediaStreamTrackProcessorSource = MediaStreamTrackProcessor::Source;
WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStreamTrackProcessorSource);

MediaStreamTrackProcessor::Source::Source(MediaStreamTrackProcessor& processor)
    : m_processor(processor)
{
}

MediaStreamTrackProcessor::Source::~Source() = default;

void MediaStreamTrackProcessor::Source::trackEnded()
{
    if (!m_isWaiting)
        return;

    m_isWaiting = false;
    controller().close();
}

void MediaStreamTrackProcessor::Source::enqueue(WebCodecsVideoFrame& frame, ScriptExecutionContext& context)
{
    ASSERT(!m_isCancelled);

    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    if (!globalObject)
        return;

    Ref vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    m_isWaiting = false;

    if (controller().enqueue(toJS(globalObject, globalObject, frame)))
        pullFinished();
}

void MediaStreamTrackProcessor::Source::doStart()
{
    startFinished();
}

void MediaStreamTrackProcessor::Source::doPull()
{
    if (m_processor->m_isTrackEnded) {
        controller().close();
        return;
    }

    m_isWaiting = true;
    Ref { m_processor.get() }->tryEnqueueingVideoFrame();
}

void MediaStreamTrackProcessor::Source::doCancel(JSC::JSValue)
{
    auto scope = makeScopeExit([&] {
        cancelFinished();
    });

    m_isCancelled = true;
    Ref { m_processor.get() }->stopObserving();
}


Ref<MediaStreamTrackProcessor::TrackObserverWrapper> MediaStreamTrackProcessor::TrackObserverWrapper::create(ScriptExecutionContext& context, MediaStreamTrackProcessor& processor, MediaStreamTrackHandle& handle)
{
    return adoptRef(*new TrackObserverWrapper(context, processor, handle));
}

MediaStreamTrackProcessor::TrackObserverWrapper::TrackObserverWrapper(ScriptExecutionContext& context, MediaStreamTrackProcessor& processor, MediaStreamTrackHandle& handle)
    : m_trackContextIdentifier(handle.trackContextIdentifier())
    , m_processorContextIdentifier(context.identifier())
    , m_processor(processor)
    , m_track(handle.track())
{
}

void MediaStreamTrackProcessor::TrackObserverWrapper::start()
{
    if (m_trackContextIdentifier == m_processorContextIdentifier) {
        RefPtr track = m_track.get();
        if (!track || track->ended()) {
            trackEnded();
            return;
        }

        Ref observer = TrackObserver::create(*this);
        track->privateTrack().addObserver(observer.get());
        m_observer = WTF::move(observer);
        return;
    }

    ScriptExecutionContext::postTaskTo(m_trackContextIdentifier, [weakThis = ThreadSafeWeakPtr { *this }](auto&) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr track = protectedThis->m_track.get();
        if (!track || track->ended()) {
            ScriptExecutionContext::postTaskTo(protectedThis->m_processorContextIdentifier, [processor = protectedThis->m_processor](auto&) {
                if (RefPtr protectedProcessor = processor.get())
                    protectedProcessor->trackEnded();
            });
            return;
        }

        Ref observer = TrackObserver::create(*protectedThis);
        track->privateTrack().addObserver(observer.get());
        protectedThis->m_observer = WTF::move(observer);
    });
}

void MediaStreamTrackProcessor::TrackObserverWrapper::stop()
{
#if ASSERT_ENABLED
    m_isStopped = true;
#endif

    if (m_trackContextIdentifier == m_processorContextIdentifier) {
        removeObserver();
        return;
    }

    ScriptExecutionContext::postTaskTo(m_trackContextIdentifier, [protectedThis = Ref { *this }](auto&) {
        protectedThis->removeObserver();
    });
}

void MediaStreamTrackProcessor::TrackObserverWrapper::trackEnded()
{
    if (m_trackContextIdentifier == m_processorContextIdentifier) {
        if (RefPtr processor = m_processor.get())
            processor->trackEnded();
        return;
    }

    ScriptExecutionContext::postTaskTo(m_processorContextIdentifier, [processor = m_processor](auto&) {
        if (RefPtr protectedProcessor = processor.get())
            protectedProcessor->trackEnded();
    });
}

void MediaStreamTrackProcessor::TrackObserverWrapper::removeObserver()
{
    RefPtr observer = std::exchange(m_observer, { });
    if (!observer)
        return;

    if (RefPtr track = m_track.get())
        track->privateTrack().removeObserver(*observer);
    ScriptExecutionContext::postTaskTo(m_trackContextIdentifier, [observer = WTF::move(observer)](auto&) { });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)
