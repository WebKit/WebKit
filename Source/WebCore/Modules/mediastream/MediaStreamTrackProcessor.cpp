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

#include "ContextDestructionObserverInlines.h"
#include "JSWebCodecsVideoFrame.h"
#include "Logging.h"
#include "ReadableStream.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Seconds.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamTrackProcessor);

ExceptionOr<Ref<MediaStreamTrackProcessor>> MediaStreamTrackProcessor::create(ScriptExecutionContext& context, Init&& init)
{
    if (!init.track->isVideo())
        return Exception { ExceptionCode::TypeError, "Track is not video"_s };

    if (init.track->ended())
        return Exception { ExceptionCode::TypeError, "Track is ended"_s };

    return adoptRef(*new MediaStreamTrackProcessor(context, *init.track, init.maxBufferSize));
}

MediaStreamTrackProcessor::MediaStreamTrackProcessor(ScriptExecutionContext& context, Ref<MediaStreamTrack>&& track, unsigned short maxVideoFramesCount)
    : ContextDestructionObserver(&context)
    , m_videoFrameObserverWrapper(VideoFrameObserverWrapper::create(context.identifier(), *this, Ref { track->sourceForProcessor() }, maxVideoFramesCount))
    , m_track(WTFMove(track))
{
}

MediaStreamTrackProcessor::~MediaStreamTrackProcessor()
{
    stopVideoFrameObserver();
}

ExceptionOr<Ref<ReadableStream>> MediaStreamTrackProcessor::readable(JSC::JSGlobalObject& globalObject)
{
    if (!m_readable) {
        m_readableStreamSource = makeUniqueWithoutRefCountedCheck<Source>(m_track.get(), *this);
        auto readableOrException = ReadableStream::create(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject), *m_readableStreamSource);
        if (readableOrException.hasException()) {
            m_readableStreamSource = nullptr;
            return readableOrException.releaseException();
        }
        m_readable = readableOrException.releaseReturnValue();
        m_videoFrameObserverWrapper->start();
    }

    return Ref { *m_readable };
}

void MediaStreamTrackProcessor::contextDestroyed()
{
    m_readableStreamSource = nullptr;
    stopVideoFrameObserver();
}

void MediaStreamTrackProcessor::stopVideoFrameObserver()
{
    m_videoFrameObserverWrapper = nullptr;
}

void MediaStreamTrackProcessor::tryEnqueueingVideoFrame()
{
    RefPtr context = scriptExecutionContext();
    if (!context || !m_videoFrameObserverWrapper || !m_readableStreamSource)
        return;

    // FIXME: If the stream is waiting, we might want to buffer based on
    // https://w3c.github.io/mediacapture-transform/#dom-mediastreamtrackprocessorinit-maxbuffersize.
    if (!m_readableStreamSource->isWaiting())
        return;

    if (auto videoFrame = m_videoFrameObserverWrapper->takeVideoFrame(*context))
        m_readableStreamSource->enqueue(*videoFrame, *context);
}

Ref<MediaStreamTrackProcessor::VideoFrameObserverWrapper> MediaStreamTrackProcessor::VideoFrameObserverWrapper::create(ScriptExecutionContextIdentifier identifier, MediaStreamTrackProcessor& processor, Ref<RealtimeMediaSource>&& source, unsigned short maxVideoFramesCount)
{
#if PLATFORM(COCOA)
    if (source->deviceType() == CaptureDevice::DeviceType::Camera)
        maxVideoFramesCount = 1;
#endif
    return adoptRef(*new VideoFrameObserverWrapper(identifier, processor, WTFMove(source), maxVideoFramesCount));
}

MediaStreamTrackProcessor::VideoFrameObserverWrapper::VideoFrameObserverWrapper(ScriptExecutionContextIdentifier identifier, MediaStreamTrackProcessor& processor, Ref<RealtimeMediaSource>&& source, unsigned short maxVideoFramesCount)
    : m_observer(makeUniqueRef<VideoFrameObserver>(identifier, processor, WTFMove(source), maxVideoFramesCount))
{
}

void MediaStreamTrackProcessor::VideoFrameObserverWrapper::start()
{
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->m_observer->start();
    });
}

MediaStreamTrackProcessor::VideoFrameObserver::VideoFrameObserver(ScriptExecutionContextIdentifier identifier, WeakPtr<MediaStreamTrackProcessor>&& processor, Ref<RealtimeMediaSource>&& source, unsigned short maxVideoFramesCount)
    : m_contextIdentifier(identifier)
    , m_processor(WTFMove(processor))
    , m_realtimeVideoSource(WTFMove(source))
    , m_maxVideoFramesCount(maxVideoFramesCount)
{
    ASSERT(!isMainThread());
}

void MediaStreamTrackProcessor::VideoFrameObserver::start()
{
    ASSERT(isMainThread());
    m_isStarted = true;
    m_realtimeVideoSource->addVideoFrameObserver(*this);
}

MediaStreamTrackProcessor::VideoFrameObserver::~VideoFrameObserver()
{
    ASSERT(isMainThread());
    if (m_isStarted)
        m_realtimeVideoSource->removeVideoFrameObserver(*this);
}

RefPtr<WebCodecsVideoFrame> MediaStreamTrackProcessor::VideoFrameObserver::takeVideoFrame(ScriptExecutionContext& context)
{
    RefPtr<VideoFrame> videoFrame;
    {
        Locker lock(m_videoFramesLock);
        if (m_videoFrames.isEmpty())
            return nullptr;

        videoFrame = m_videoFrames.takeFirst();
    }

    WebCodecsVideoFrame::BufferInit init;
    init.codedWidth = videoFrame->presentationSize().width();
    init.codedHeight = videoFrame->presentationSize().height();
    init.colorSpace = videoFrame->colorSpace();
    init.timestamp = Seconds(videoFrame->presentationTime().toDouble()).microseconds();

    return WebCodecsVideoFrame::create(context, videoFrame.releaseNonNull(), WTFMove(init));
}

void MediaStreamTrackProcessor::VideoFrameObserver::videoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata)
{
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
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamTrackProcessorSource);

MediaStreamTrackProcessor::Source::Source(Ref<MediaStreamTrack>&& track, MediaStreamTrackProcessor& processor)
    : m_track(WTFMove(track))
    , m_processor(processor)
{
    m_track->privateTrack().addObserver(*this);
}

MediaStreamTrackProcessor::Source::~Source()
{
    m_track->privateTrack().removeObserver(*this);
}

void MediaStreamTrackProcessor::Source::trackEnded(MediaStreamTrackPrivate&)
{
    close();
}

bool MediaStreamTrackProcessor::Source::isWaiting() const
{
    return m_isWaiting;
}

void MediaStreamTrackProcessor::Source::close()
{
    if (!m_isCancelled)
        controller().close();
}

void MediaStreamTrackProcessor::Source::enqueue(WebCodecsVideoFrame& frame, ScriptExecutionContext& context)
{
    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    if (!globalObject)
        return;

    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    m_isWaiting = false;

    Ref protectedThis { *this };

    if (!m_isCancelled && controller().enqueue(toJS(globalObject, globalObject, frame)))
        pullFinished();
}

void MediaStreamTrackProcessor::Source::doStart()
{
    startFinished();
}

void MediaStreamTrackProcessor::Source::doPull()
{
    m_isWaiting = true;
    if (RefPtr protectedProcessor = m_processor.get())
        protectedProcessor->tryEnqueueingVideoFrame();
}

void MediaStreamTrackProcessor::Source::doCancel()
{
    m_isCancelled = true;
    if (RefPtr protectedProcessor = m_processor.get())
        protectedProcessor->stopVideoFrameObserver();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)
