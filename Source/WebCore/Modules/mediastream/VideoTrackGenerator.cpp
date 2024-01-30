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
#include "VideoTrackGenerator.h"

#if ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)

#include "Exception.h"
#include "JSWebCodecsVideoFrame.h"
#include "MediaStreamTrack.h"
#include "VideoFrame.h"
#include "WritableStream.h"
#include "WritableStreamSink.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(VideoTrackGenerator);

ExceptionOr<Ref<VideoTrackGenerator>> VideoTrackGenerator::create(ScriptExecutionContext& context)
{
    auto source = Source::create(context.identifier());
    auto sink = Sink::create(Ref { source });
    auto writableOrException = WritableStream::create(*JSC::jsCast<JSDOMGlobalObject*>(context.globalObject()), Ref { sink });

    if (writableOrException.hasException())
        return writableOrException.releaseException();

    Ref writable = writableOrException.releaseReturnValue();
    source->setWritable(writable.get());

    // FIXME: Maybe we should have the writable buffer frames until the source is actually started.
    callOnMainThread([source] {
        source->start();
    });

    auto logger = Logger::create(&context);
    if (auto sessionID = context.sessionID())
        logger->setEnabled(&context, sessionID->isAlwaysOnLoggingAllowed());

    auto privateTrack = MediaStreamTrackPrivate::create(WTFMove(logger), WTFMove(source), [identifier = context.identifier()](Function<void()>&& task) {
        ScriptExecutionContext::postTaskTo(identifier, [task = WTFMove(task)] (auto&) mutable {
            task();
        });
    });

    RealtimeMediaSourceSupportedConstraints supportedConstraints;
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);

    RealtimeMediaSourceSettings settings;
    settings.setSupportedConstraints(supportedConstraints);
    settings.setWidth(0);
    settings.setHeight(0);

    RealtimeMediaSourceCapabilities capabilities { supportedConstraints };
    capabilities.setWidth({ 0, 0 });
    capabilities.setHeight({ 0, 0 });

    privateTrack->initializeSettings(WTFMove(settings));
    privateTrack->initializeCapabilities(WTFMove(capabilities));

    return adoptRef(*new VideoTrackGenerator(WTFMove(sink), WTFMove(writable), MediaStreamTrack::create(context, WTFMove(privateTrack))));
}

VideoTrackGenerator::VideoTrackGenerator(Ref<Sink>&& sink, Ref<WritableStream>&& writable, Ref<MediaStreamTrack>&& track)
    : m_sink(WTFMove(sink))
    , m_writable(WTFMove(writable))
    , m_track(WTFMove(track))
{
}

VideoTrackGenerator::~VideoTrackGenerator()
{
}

void VideoTrackGenerator::setMuted(ScriptExecutionContext& context, bool muted)
{
    if (muted == m_muted)
        return;

    m_muted = muted;
    m_sink->setMuted(m_muted);

    if (m_hasMutedChanged)
        return;

    m_hasMutedChanged = true;
    context.postTask([this, protectedThis = Ref { *this }] (auto&) {
        m_hasMutedChanged = false;
        m_track->privateTrack().setMuted(m_muted);
    });
}

Ref<WritableStream> VideoTrackGenerator::writable()
{
    return Ref { m_writable };
}

Ref<MediaStreamTrack> VideoTrackGenerator::track()
{
    return Ref { m_track };
}

VideoTrackGenerator::Source::Source(ScriptExecutionContextIdentifier identifier)
    : RealtimeMediaSource(CaptureDevice { { }, CaptureDevice::DeviceType::Camera, emptyString() })
    , m_contextIdentifier(identifier)
{
}

void VideoTrackGenerator::Source::endProducingData()
{
    ASSERT(isMainThread());
    ScriptExecutionContext::postTaskTo(m_contextIdentifier, [weakThis = ThreadSafeWeakPtr { *this }] (auto&) {
        RefPtr protectedSource = weakThis.get();
        RefPtr writable = protectedSource ? protectedSource->m_writable.get() : nullptr;
        if (writable)
            writable->closeIfPossible();
    });
}

void VideoTrackGenerator::Source::setWritable(WritableStream& writable)
{
    ASSERT(!isMainThread());
    ASSERT(!m_writable);
    m_writable = writable;
}

void VideoTrackGenerator::Source::writeVideoFrame(VideoFrame& frame, VideoFrameTimeMetadata metadata)
{
    ASSERT(!isMainThread());

    auto frameSize = IntSize(frame.presentationSize());
    if (frame.rotation() == VideoFrame::Rotation::Left || frame.rotation() == VideoFrame::Rotation::Right)
        frameSize = frameSize.transposedSize();

    if (m_videoFrameSize != frameSize) {
        m_videoFrameSize = frameSize;
        callOnMainThread([this, protectedThis = Ref { *this }, frameSize] {
            RealtimeMediaSourceSupportedConstraints supportedConstraints;
            supportedConstraints.setSupportsWidth(true);
            supportedConstraints.setSupportsHeight(true);

            if (m_maxVideoFrameSize.width() < frameSize.width() || m_maxVideoFrameSize.height() < frameSize.height()) {
                m_maxVideoFrameSize.clampToMinimumSize(frameSize);

                m_capabilities = RealtimeMediaSourceCapabilities { supportedConstraints };
                m_capabilities.setWidth({ 0, m_maxVideoFrameSize.width() });
                m_capabilities.setHeight({ 0, m_maxVideoFrameSize.height() });
            }

            m_settings.setSupportedConstraints(supportedConstraints);
            m_settings.setWidth(frameSize.width());
            m_settings.setHeight(frameSize.height());

            setSize(frameSize);
        });
    }

    videoFrameAvailable(frame, metadata);
}

VideoTrackGenerator::Sink::Sink(Ref<Source>&& source)
    : m_source(WTFMove(source))
{
}

void VideoTrackGenerator::Sink::write(ScriptExecutionContext&, JSC::JSValue value, DOMPromiseDeferred<void>&& promise)
{
    auto* jsFrameObject = JSC::jsDynamicCast<JSWebCodecsVideoFrame*>(value);
    RefPtr frameObject = jsFrameObject ? &jsFrameObject->wrapped() : nullptr;
    if (!frameObject) {
        promise.reject(Exception { ExceptionCode::TypeError, "Expected a VideoFrame object"_s });
        return;
    }

    RefPtr videoFrame = frameObject->internalFrame();
    if (!videoFrame) {
        promise.reject(Exception { ExceptionCode::TypeError, "VideoFrame object is not valid"_s });
        return;
    }

    if (!m_muted)
        m_source->writeVideoFrame(*videoFrame, { });

    frameObject->close();
    promise.resolve();
}

void VideoTrackGenerator::Sink::close()
{
    callOnMainThread([source = m_source] {
        source->endImmediatly();
    });
}

void VideoTrackGenerator::Sink::error(String&&)
{
    close();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)
