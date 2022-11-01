/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "VideoFrame.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CanvasCaptureMediaStreamTrack);

Ref<CanvasCaptureMediaStreamTrack> CanvasCaptureMediaStreamTrack::create(Document& document, Ref<HTMLCanvasElement>&& canvas, std::optional<double>&& frameRequestRate)
{
    auto source = CanvasCaptureMediaStreamTrack::Source::create(canvas.get(), WTFMove(frameRequestRate));
    auto track = adoptRef(*new CanvasCaptureMediaStreamTrack(document, WTFMove(canvas), WTFMove(source)));
    track->suspendIfNeeded();
    return track;
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(Document& document, Ref<HTMLCanvasElement>&& canvas, Ref<CanvasCaptureMediaStreamTrack::Source>&& source)
    : MediaStreamTrack(document, MediaStreamTrackPrivate::create(document.logger(), source.copyRef()))
    , m_canvas(WTFMove(canvas))
{
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(Document& document, Ref<HTMLCanvasElement>&& canvas, Ref<MediaStreamTrackPrivate>&& privateTrack)
    : MediaStreamTrack(document, WTFMove(privateTrack))
    , m_canvas(WTFMove(canvas))
{
}

Ref<CanvasCaptureMediaStreamTrack::Source> CanvasCaptureMediaStreamTrack::Source::create(HTMLCanvasElement& canvas, std::optional<double>&& frameRequestRate)
{
    auto source = adoptRef(*new Source(canvas, WTFMove(frameRequestRate)));
    source->start();

    callOnMainThread([source] {
        if (!source->m_canvas)
            return;
        source->captureCanvas();
    });
    return source;
}

const char* CanvasCaptureMediaStreamTrack::activeDOMObjectName() const
{
    return "CanvasCaptureMediaStreamTrack";
}

// FIXME: Give source id and name
CanvasCaptureMediaStreamTrack::Source::Source(HTMLCanvasElement& canvas, std::optional<double>&& frameRequestRate)
    : RealtimeMediaSource(CaptureDevice { { }, CaptureDevice::DeviceType::Camera, "CanvasCaptureMediaStreamTrack"_s })
    , m_frameRequestRate(WTFMove(frameRequestRate))
    , m_requestFrameTimer(*this, &Source::requestFrameTimerFired)
    , m_captureCanvasTimer(*this, &Source::captureCanvas)
    , m_canvas(&canvas)
{
}

void CanvasCaptureMediaStreamTrack::Source::startProducingData()
{
    if (!m_canvas)
        return;
    m_canvas->addObserver(*this);
    m_canvas->addDisplayBufferObserver(*this);

    if (!m_frameRequestRate)
        return;

    if (m_frameRequestRate.value())
        m_requestFrameTimer.startRepeating(1_s / m_frameRequestRate.value());
}

void CanvasCaptureMediaStreamTrack::Source::stopProducingData()
{
    m_requestFrameTimer.stop();

    if (!m_canvas)
        return;
    m_canvas->removeObserver(*this);
    m_canvas->removeDisplayBufferObserver(*this);
}

void CanvasCaptureMediaStreamTrack::Source::requestFrameTimerFired()
{
    requestFrame();
}

void CanvasCaptureMediaStreamTrack::Source::canvasDestroyed(CanvasBase& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);

    stop();
    m_canvas = nullptr;
}

const RealtimeMediaSourceSettings& CanvasCaptureMediaStreamTrack::Source::settings()
{
    if (m_currentSettings)
        return m_currentSettings.value();

    RealtimeMediaSourceSupportedConstraints constraints;
    constraints.setSupportsWidth(true);
    constraints.setSupportsHeight(true);

    RealtimeMediaSourceSettings settings;
    settings.setWidth(m_canvas->width());
    settings.setHeight(m_canvas->height());
    settings.setSupportedConstraints(constraints);

    m_currentSettings = WTFMove(settings);
    return m_currentSettings.value();
}

void CanvasCaptureMediaStreamTrack::Source::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height }))
        m_currentSettings = std::nullopt;
}

void CanvasCaptureMediaStreamTrack::Source::canvasResized(CanvasBase& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);
    setSize(IntSize(m_canvas->width(), m_canvas->height()));
}

void CanvasCaptureMediaStreamTrack::Source::canvasChanged(CanvasBase& canvas, const std::optional<FloatRect>&)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);
    if (m_canvas->renderingContext() && m_canvas->renderingContext()->needsPreparationForDisplay())
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

void CanvasCaptureMediaStreamTrack::Source::captureCanvas()
{
    ASSERT(m_canvas);

    if (!isProducingData())
        return;

    if (m_frameRequestRate) {
        if (!m_shouldEmitFrame)
            return;
        m_shouldEmitFrame = false;
    }

    if (!m_canvas->originClean())
        return;

    auto videoFrame = m_canvas->toVideoFrame();
    if (!videoFrame)
        return;

    VideoFrameTimeMetadata metadata;
    metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
    videoFrameAvailable(*videoFrame, metadata);
}

RefPtr<MediaStreamTrack> CanvasCaptureMediaStreamTrack::clone()
{
    if (!scriptExecutionContext())
        return nullptr;
    
    auto track = adoptRef(*new CanvasCaptureMediaStreamTrack(downcast<Document>(*scriptExecutionContext()), m_canvas.copyRef(), m_private->clone()));
    track->suspendIfNeeded();
    return track;
}

}

#endif // ENABLE(MEDIA_STREAM)
