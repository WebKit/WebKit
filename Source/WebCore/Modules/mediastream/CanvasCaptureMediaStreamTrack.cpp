/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "CanvasCaptureMediaStreamTrack.h"

#if ENABLE(MEDIA_STREAM)

#include "ContextDestructionObserverInlines.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "VideoFrame.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CanvasCaptureMediaStreamTrack);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CanvasCaptureMediaStreamTrack::Source);

Ref<CanvasCaptureMediaStreamTrack> CanvasCaptureMediaStreamTrack::create(Document& document, Ref<HTMLCanvasElement>&& canvas, std::optional<double>&& frameRequestRate)
{
    auto source = CanvasCaptureMediaStreamTrack::Source::create(canvas.get(), WTF::move(frameRequestRate));
    auto track = adoptRef(*new CanvasCaptureMediaStreamTrack(document, WTF::move(canvas), WTF::move(source)));
    track->suspendIfNeeded();
    return track;
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(Document& document, Ref<HTMLCanvasElement>&& canvas, Ref<CanvasCaptureMediaStreamTrack::Source>&& source)
    : MediaStreamTrack(document, MediaStreamTrackPrivate::create(document.logger(), source.copyRef()))
    , m_canvas(WTF::move(canvas))
{
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(Document& document, Ref<HTMLCanvasElement>&& canvas, Ref<MediaStreamTrackPrivate>&& privateTrack)
    : MediaStreamTrack(document, WTF::move(privateTrack))
    , m_canvas(WTF::move(canvas))
{
}

RefPtr<VideoFrame> CanvasCaptureMediaStreamTrack::grabFrame()
{
    Ref source = static_cast<Source&>(this->source());
    return source->grabFrame();
}

Ref<CanvasCaptureMediaStreamTrack::Source> CanvasCaptureMediaStreamTrack::Source::create(HTMLCanvasElement& canvas, std::optional<double>&& frameRequestRate)
{
    auto source = adoptRef(*new Source(canvas, WTF::move(frameRequestRate)));
    source->start();

    callOnMainThread([source] {
        if (!source->m_canvas)
            return;
        source->captureCanvas();
    });
    return source;
}

// FIXME: Give source id and name
CanvasCaptureMediaStreamTrack::Source::Source(HTMLCanvasElement& canvas, std::optional<double>&& frameRequestRate)
    : RealtimeMediaSource(CaptureDevice { { }, CaptureDevice::DeviceType::Camera, "CanvasCaptureMediaStreamTrack"_s })
    , m_frameRequestRate(WTF::move(frameRequestRate))
    , m_requestFrameTimer(*this, &Source::requestFrameTimerFired)
    , m_captureCanvasTimer(*this, &Source::captureCanvas)
    , m_canvas(&canvas)
{
}

void CanvasCaptureMediaStreamTrack::Source::startProducingData()
{
    RefPtr canvas = m_canvas.get();
    if (!canvas)
        return;

    canvas->addObserver(*this);
    canvas->addDisplayBufferObserver(*this);

    if (!m_frameRequestRate)
        return;

    if (m_frameRequestRate.value())
        m_requestFrameTimer.startRepeating(1_s / m_frameRequestRate.value());
}

void CanvasCaptureMediaStreamTrack::Source::stopProducingData()
{
    m_requestFrameTimer.stop();

    RefPtr canvas = m_canvas.get();
    if (!canvas)
        return;

    canvas->removeObserver(*this);
    canvas->removeDisplayBufferObserver(*this);
}

void CanvasCaptureMediaStreamTrack::Source::requestFrameTimerFired()
{
    requestFrame();
}

void CanvasCaptureMediaStreamTrack::Source::canvasDestroyed(CanvasBase& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);

    stop();
    m_canvas = { };
}

const RealtimeMediaSourceSettings& CanvasCaptureMediaStreamTrack::Source::settings()
{
    if (m_currentSettings)
        return m_currentSettings.value();

    RealtimeMediaSourceSupportedConstraints constraints;
    RefPtr canvas = m_canvas.get();
    if (canvas) {
        constraints.setSupportsWidth(true);
        constraints.setSupportsHeight(true);
    }

    RealtimeMediaSourceSettings settings;
    if (canvas) {
        settings.setWidth(canvas->width());
        settings.setHeight(canvas->height());
    }
    settings.setSupportedConstraints(constraints);

    m_currentSettings = WTF::move(settings);
    return m_currentSettings.value();
}

void CanvasCaptureMediaStreamTrack::Source::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height }))
        m_currentSettings = std::nullopt;
}

void CanvasCaptureMediaStreamTrack::Source::canvasResized(CanvasBase& canvas)
{
    ASSERT(m_canvas == &canvas);
    setSize(IntSize(canvas.width(), canvas.height()));
}

void CanvasCaptureMediaStreamTrack::Source::canvasChanged(CanvasBase&, const FloatRect&)
{
    // If canvas needs preparation, the capture will be scheduled once document prepares the canvas.
    RefPtr canvas = m_canvas.get();
    if (!canvas || canvas->needsPreparationForDisplay())
        return;

    scheduleCaptureCanvas();
}

void CanvasCaptureMediaStreamTrack::Source::scheduleCaptureCanvas()
{
    // FIXME: We should try to generate the frame at the time the screen is being updated.
    if (m_captureCanvasTimer.isActive())
        return;
    m_captureCanvasTimer.startOneShot(0_s);
}

void CanvasCaptureMediaStreamTrack::Source::canvasDisplayBufferPrepared(CanvasBase& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);
    // FIXME: Here we should capture the image instead.
    // However, submitting the sample to the receiver might cause layout,
    // and currently the display preparation is done after layout.
    scheduleCaptureCanvas();
}

RefPtr<VideoFrame> CanvasCaptureMediaStreamTrack::Source::grabFrame()
{
    RefPtr canvas = m_canvas.get();
    if (!canvas)
        return nullptr;

#if ENABLE(WEBGL)
    if (RefPtr gl = dynamicDowncast<WebGLRenderingContextBase>(canvas->renderingContext()))
        return gl->surfaceBufferToVideoFrame(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer);
#endif
    return canvas->toVideoFrame();
}

void CanvasCaptureMediaStreamTrack::Source::captureCanvas()
{
    ASSERT(m_canvas);
    RefPtr canvas = m_canvas.get();
    if (!canvas || !isProducingData())
        return;

    if (m_frameRequestRate) {
        if (!m_shouldEmitFrame)
            return;
        m_shouldEmitFrame = false;
    }

    if (!canvas->originClean())
        return;

    RefPtr videoFrame = [&]() -> RefPtr<VideoFrame> {
#if ENABLE(WEBGL)
        if (RefPtr gl = dynamicDowncast<WebGLRenderingContextBase>(canvas->renderingContext()))
            return gl->surfaceBufferToVideoFrame(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer);
#endif
        return canvas->toVideoFrame();
    }();
    if (!videoFrame)
        return;

    VideoFrameTimeMetadata metadata;
    metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();

#if USE(GSTREAMER)
    auto& gstVideoFrame = downcast<VideoFrameGStreamer>(*videoFrame);
    static const double s_fixedFrameRate = 60.0;

    if (!m_clock)
        m_clock = adoptGRef(gst_system_clock_obtain());
    RELEASE_ASSERT(m_clock);

    if (!m_frameRequestRate)
        gstVideoFrame.setMaxFrameRate(s_fixedFrameRate);

    auto frameRate = s_fixedFrameRate;
    if (m_frameRequestRate && *m_frameRequestRate)
        frameRate = *m_frameRequestRate;

    gstVideoFrame.setFrameRate(frameRate);
    gstVideoFrame.setPresentationTime(fromGstClockTime(gst_clock_get_time(m_clock.get())));
    gstVideoFrame.setMetadataAndContentHint({ metadata }, VideoFrameContentHint::Canvas);
#endif

    videoFrameAvailable(*videoFrame, metadata);
}

RefPtr<MediaStreamTrack> CanvasCaptureMediaStreamTrack::clone()
{
    if (!scriptExecutionContext())
        return nullptr;
    
    auto track = adoptRef(*new CanvasCaptureMediaStreamTrack(downcast<Document>(*scriptExecutionContext()), m_canvas.copyRef(), privateTrack().clone()));
    track->suspendIfNeeded();
    return track;
}

}

#endif // ENABLE(MEDIA_STREAM)
