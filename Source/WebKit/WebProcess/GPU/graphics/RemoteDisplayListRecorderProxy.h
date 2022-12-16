/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS)

#include "DisplayListRecorderFlushIdentifier.h"
#include <WebCore/DisplayListRecorder.h>
#include <WebCore/DrawGlyphsRecorder.h>
#include <WebCore/GraphicsContext.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class StreamClientConnection;
}

namespace WebKit {

class RemoteRenderingBackendProxy;
class RemoteImageBufferProxy;

class RemoteDisplayListRecorderProxy : public WebCore::DisplayList::Recorder {
public:
    RemoteDisplayListRecorderProxy(RemoteImageBufferProxy&, RemoteRenderingBackendProxy&, const WebCore::FloatRect& initialClip, const WebCore::AffineTransform&);
    ~RemoteDisplayListRecorderProxy() = default;

    void convertToLuminanceMask() final;
    void transformToColorSpace(const WebCore::DestinationColorSpace&) final;
    void flushContext(DisplayListRecorderFlushIdentifier);
    void disconnect();

private:
    template<typename T> void send(T&& message);

    friend class WebCore::DrawGlyphsRecorder;

    WebCore::RenderingMode renderingMode() const final;

    void recordSave() final;
    void recordRestore() final;
    void recordTranslate(float x, float y) final;
    void recordRotate(float angle) final;
    void recordScale(const WebCore::FloatSize&) final;
    void recordSetCTM(const WebCore::AffineTransform&) final;
    void recordConcatenateCTM(const WebCore::AffineTransform&) final;
    void recordSetInlineFillColor(WebCore::SRGBA<uint8_t>) final;
    void recordSetInlineStrokeColor(WebCore::SRGBA<uint8_t>) final;
    void recordSetStrokeThickness(float) final;
    void recordSetState(const WebCore::GraphicsContextState&) final;
    void recordSetLineCap(WebCore::LineCap) final;
    void recordSetLineDash(const WebCore::DashArray&, float dashOffset) final;
    void recordSetLineJoin(WebCore::LineJoin) final;
    void recordSetMiterLimit(float) final;
    void recordClearShadow() final;
    void recordClip(const WebCore::FloatRect&) final;
    void recordClipOut(const WebCore::FloatRect&) final;
    void recordClipToImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destinationRect) final;
    void recordClipOutToPath(const WebCore::Path&) final;
    void recordClipPath(const WebCore::Path&, WebCore::WindRule) final;
    void recordDrawFilteredImageBuffer(WebCore::ImageBuffer*, const WebCore::FloatRect& sourceImageRect, WebCore::Filter&) final;
    void recordDrawGlyphs(const WebCore::Font&, const WebCore::GlyphBufferGlyph*, const WebCore::GlyphBufferAdvance*, unsigned count, const WebCore::FloatPoint& localAnchor, WebCore::FontSmoothingMode) final;
    void recordDrawDecomposedGlyphs(const WebCore::Font&, const WebCore::DecomposedGlyphs&) final;
    void recordDrawImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&) final;
    void recordDrawNativeImage(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatSize& imageSize, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&) final;
    void recordDrawSystemImage(WebCore::SystemImage&, const WebCore::FloatRect&);
    void recordDrawPattern(WebCore::RenderingResourceIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, const WebCore::ImagePaintingOptions& = { }) final;
    void recordBeginTransparencyLayer(float) final;
    void recordEndTransparencyLayer() final;
    void recordDrawRect(const WebCore::FloatRect&, float) final;
    void recordDrawLine(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2) final;
    void recordDrawLinesForText(const WebCore::FloatPoint& blockLocation, const WebCore::FloatSize& localAnchor, float thickness, const WebCore::DashArray& widths, bool printing, bool doubleLines, WebCore::StrokeStyle) final;
    void recordDrawDotsForDocumentMarker(const WebCore::FloatRect&, const WebCore::DocumentMarkerLineStyle&) final;
    void recordDrawEllipse(const WebCore::FloatRect&) final;
    void recordDrawPath(const WebCore::Path&) final;
    void recordDrawFocusRingPath(const WebCore::Path&, float width, float offset, const WebCore::Color&) final;
    void recordDrawFocusRingRects(const Vector<WebCore::FloatRect>&, float width, float offset, const WebCore::Color&) final;
    void recordFillRect(const WebCore::FloatRect&) final;
    void recordFillRectWithColor(const WebCore::FloatRect&, const WebCore::Color&) final;
    void recordFillRectWithGradient(const WebCore::FloatRect&, WebCore::Gradient&) final;
    void recordFillCompositedRect(const WebCore::FloatRect&, const WebCore::Color&, WebCore::CompositeOperator, WebCore::BlendMode) final;
    void recordFillRoundedRect(const WebCore::FloatRoundedRect&, const WebCore::Color&, WebCore::BlendMode) final;
    void recordFillRectWithRoundedHole(const WebCore::FloatRect&, const WebCore::FloatRoundedRect&, const WebCore::Color&) final;
#if ENABLE(INLINE_PATH_DATA)
    void recordFillLine(const WebCore::LineData&) final;
    void recordFillArc(const WebCore::ArcData&) final;
    void recordFillQuadCurve(const WebCore::QuadCurveData&) final;
    void recordFillBezierCurve(const WebCore::BezierCurveData&) final;
#endif
    void recordFillPath(const WebCore::Path&) final;
    void recordFillEllipse(const WebCore::FloatRect&) final;
    void recordPaintFrameForMedia(WebCore::MediaPlayer&, const WebCore::FloatRect& destination) final;
    void recordStrokeRect(const WebCore::FloatRect&, float) final;
#if ENABLE(INLINE_PATH_DATA)
    void recordStrokeLine(const WebCore::LineData&) final;
    void recordStrokeLineWithColorAndThickness(WebCore::SRGBA<uint8_t>, float, const WebCore::LineData&) final;
    void recordStrokeArc(const WebCore::ArcData&) final;
    void recordStrokeQuadCurve(const WebCore::QuadCurveData&) final;
    void recordStrokeBezierCurve(const WebCore::BezierCurveData&) final;
#endif
    void recordStrokePath(const WebCore::Path&) final;
    void recordStrokeEllipse(const WebCore::FloatRect&) final;
    void recordClearRect(const WebCore::FloatRect&) final;
#if USE(CG)
    void recordApplyStrokePattern() final;
    void recordApplyFillPattern() final;
#endif
    void recordApplyDeviceScaleFactor(float) final;

    bool recordResourceUse(WebCore::NativeImage&) final;
    bool recordResourceUse(WebCore::ImageBuffer&) final;
    bool recordResourceUse(const WebCore::SourceImage&) final;
    bool recordResourceUse(WebCore::Font&) final;
    bool recordResourceUse(WebCore::DecomposedGlyphs&) final;

    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, float resolutionScale, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMode>, std::optional<WebCore::RenderingMethod>) const final;
    RefPtr<WebCore::ImageBuffer> createAlignedImageBuffer(const WebCore::FloatSize&, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMethod>) const final;
    RefPtr<WebCore::ImageBuffer> createAlignedImageBuffer(const WebCore::FloatRect&, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMethod>) const final;

    static inline constexpr Seconds defaultSendTimeout = 3_s;
    WebCore::RenderingResourceIdentifier m_destinationBufferIdentifier;
    WeakPtr<RemoteImageBufferProxy> m_imageBuffer;
    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
