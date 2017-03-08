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

#include "GraphicsContext.h"

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

Ref<CanvasCaptureMediaStreamTrack> CanvasCaptureMediaStreamTrack::create(ScriptExecutionContext& context, Ref<HTMLCanvasElement>&& canvas, std::optional<double>&& frameRequestRate)
{
    auto source = CanvasCaptureMediaStreamTrack::Source::create(canvas.get(), WTFMove(frameRequestRate));
    return adoptRef(*new CanvasCaptureMediaStreamTrack(context, WTFMove(canvas), WTFMove(source)));
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(ScriptExecutionContext& context, Ref<HTMLCanvasElement>&& canvas, Ref<CanvasCaptureMediaStreamTrack::Source>&& source)
    : MediaStreamTrack(context, MediaStreamTrackPrivate::create(source.copyRef()))
    , m_canvas(WTFMove(canvas))
    , m_source(WTFMove(source))
{
}

Ref<CanvasCaptureMediaStreamTrack::Source> CanvasCaptureMediaStreamTrack::Source::create(HTMLCanvasElement& canvas, std::optional<double>&& frameRequestRate)
{
    auto source = adoptRef(*new Source(canvas, WTFMove(frameRequestRate)));
    source->startProducingData();

    callOnMainThread([source = source.copyRef()] {
        if (!source->m_canvas)
            return;
        source->captureCanvas();
    });
    return source;
}

// FIXME: Give source id and name
CanvasCaptureMediaStreamTrack::Source::Source(HTMLCanvasElement& canvas, std::optional<double>&& frameRequestRate)
    : RealtimeMediaSource(String(), RealtimeMediaSource::Video, String())
    , m_frameRequestRate(WTFMove(frameRequestRate))
    , m_requestFrameTimer(*this, &Source::requestFrameTimerFired)
    , m_canvas(&canvas)
{
    m_settings.setWidth(canvas.width());
    m_settings.setHeight(canvas.height());
}

void CanvasCaptureMediaStreamTrack::Source::startProducingData()
{
    if (m_isProducingData)
        return;
    m_isProducingData = true;

    if (!m_canvas)
        return;
    m_canvas->addObserver(*this);

    if (!m_frameRequestRate)
        return;

    if (m_frameRequestRate.value())
        m_requestFrameTimer.startRepeating(1. / m_frameRequestRate.value());
}

void CanvasCaptureMediaStreamTrack::Source::stopProducingData()
{
    if (!m_isProducingData)
        return;
    m_isProducingData = false;

    m_requestFrameTimer.stop();

    if (!m_canvas)
        return;
    m_canvas->removeObserver(*this);
}

void CanvasCaptureMediaStreamTrack::Source::requestFrameTimerFired()
{
    requestFrame();
}

void CanvasCaptureMediaStreamTrack::Source::canvasDestroyed(HTMLCanvasElement& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);

    stopProducingData();
    m_canvas = nullptr;
}

void CanvasCaptureMediaStreamTrack::Source::canvasResized(HTMLCanvasElement& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);

    m_settings.setWidth(m_canvas->width());
    m_settings.setHeight(m_canvas->height());
}

void CanvasCaptureMediaStreamTrack::Source::canvasChanged(HTMLCanvasElement& canvas, const FloatRect&)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);

    captureCanvas();
}

void CanvasCaptureMediaStreamTrack::Source::captureCanvas()
{
    ASSERT(m_canvas);

    if (!m_isProducingData)
        return;

    if (m_frameRequestRate) {
        if (!m_shouldEmitFrame)
            return;
        m_shouldEmitFrame = false;
    }

    if (!m_canvas->originClean())
        return;

    // FIXME: This is probably not efficient.
    m_currentImage = m_canvas->copiedImage();

    auto sample = m_canvas->toMediaSample();
    if (!sample)
        return;

    videoSampleAvailable(*sample);
}

void CanvasCaptureMediaStreamTrack::Source::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (!m_canvas)
        return;

    if (context.paintingDisabled())
        return;

    auto image = currentFrameImage();
    if (!image)
        return;

    FloatRect fullRect(0, 0, m_canvas->width(), m_canvas->height());

    GraphicsContextStateSaver stateSaver(context);
    context.setImageInterpolationQuality(InterpolationLow);
    IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
    context.drawImage(*image, rect);
}

RefPtr<Image> CanvasCaptureMediaStreamTrack::Source::currentFrameImage()
{
    return m_currentImage;
}

}

#endif // ENABLE(MEDIA_STREAM)
