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
#include "HTMLCanvasElement.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/IsoMallocInlines.h>

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CanvasCaptureMediaStreamTrack);

Ref<CanvasCaptureMediaStreamTrack> CanvasCaptureMediaStreamTrack::create(Document& document, Ref<HTMLCanvasElement>&& canvas, Optional<double>&& frameRequestRate)
{
    auto source = CanvasCaptureMediaStreamTrack::Source::create(canvas.get(), WTFMove(frameRequestRate));
    return adoptRef(*new CanvasCaptureMediaStreamTrack(document, WTFMove(canvas), WTFMove(source)));
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

Ref<CanvasCaptureMediaStreamTrack::Source> CanvasCaptureMediaStreamTrack::Source::create(HTMLCanvasElement& canvas, Optional<double>&& frameRequestRate)
{
    auto source = adoptRef(*new Source(canvas, WTFMove(frameRequestRate)));
    source->start();

    callOnMainThread([source = source.copyRef()] {
        if (!source->m_canvas)
            return;
        source->captureCanvas();
    });
    return source;
}

// FIXME: Give source id and name
CanvasCaptureMediaStreamTrack::Source::Source(HTMLCanvasElement& canvas, Optional<double>&& frameRequestRate)
    : RealtimeMediaSource(Type::Video, "CanvasCaptureMediaStreamTrack"_s)
    , m_frameRequestRate(WTFMove(frameRequestRate))
    , m_requestFrameTimer(*this, &Source::requestFrameTimerFired)
    , m_canvasChangedTimer(*this, &Source::captureCanvas)
    , m_canvas(&canvas)
{
}

void CanvasCaptureMediaStreamTrack::Source::startProducingData()
{
    if (!m_canvas)
        return;
    m_canvas->addObserver(*this);

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
        m_currentSettings = WTF::nullopt;
}

void CanvasCaptureMediaStreamTrack::Source::canvasResized(CanvasBase& canvas)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);
    setSize(IntSize(m_canvas->width(), m_canvas->height()));
}

void CanvasCaptureMediaStreamTrack::Source::canvasChanged(CanvasBase& canvas, const FloatRect&)
{
    ASSERT_UNUSED(canvas, m_canvas == &canvas);

#if ENABLE(WEBGL)
    // FIXME: We need to preserve drawing buffer as we are currently grabbing frames asynchronously.
    // We should instead add an anchor point for both 2d and 3d contexts where canvas will actually paint.
    // And call canvas observers from that point.
    if (is<WebGLRenderingContextBase>(canvas.renderingContext())) {
        auto& context = downcast<WebGLRenderingContextBase>(*canvas.renderingContext());
        if (!context.isPreservingDrawingBuffer()) {
            canvas.scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Turning drawing buffer preservation for the WebGL canvas being captured"_s);
            context.setPreserveDrawingBuffer(true);
        }
    }
#endif

    // FIXME: We should try to generate the frame at the time the screen is being updated.
    if (m_canvasChangedTimer.isActive())
        return;
    m_canvasChangedTimer.startOneShot(0_s);
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

    auto sample = m_canvas->toMediaSample();
    if (!sample)
        return;

    videoSampleAvailable(*sample);
}

RefPtr<MediaStreamTrack> CanvasCaptureMediaStreamTrack::clone()
{
    if (!scriptExecutionContext())
        return nullptr;
    
    return adoptRef(*new CanvasCaptureMediaStreamTrack(downcast<Document>(*scriptExecutionContext()), m_canvas.copyRef(), m_private->clone()));
}

}

#endif // ENABLE(MEDIA_STREAM)
