/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include <WebCore/DisplayListRecorder.h>
#include <WebCore/DrawGlyphsRecorder.h>
#include <WebCore/GraphicsContext.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Signal;
class Semaphore;
class StreamClientConnection;
}

namespace WebKit {

class RemoteRenderingBackendProxy;
class RemoteImageBufferProxy;
class SharedVideoFrameWriter;

class RemoteDisplayListRecorderProxy : public WebCore::DisplayList::Recorder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteDisplayListRecorderProxy);
public:
    RemoteDisplayListRecorderProxy(RemoteImageBufferProxy&, RemoteRenderingBackendProxy&, const WebCore::FloatRect& initialClip, const WebCore::AffineTransform&);
    RemoteDisplayListRecorderProxy(RemoteRenderingBackendProxy& , WebCore::RenderingResourceIdentifier, const WebCore::DestinationColorSpace&, WebCore::RenderingMode, const WebCore::FloatRect&, const WebCore::AffineTransform&);
    virtual ~RemoteDisplayListRecorderProxy();

    void disconnect();

    WebCore::RenderingResourceIdentifier identifier() const { return m_destinationBufferIdentifier; }

private:
    template<typename T> void send(T&& message);
    RefPtr<IPC::StreamClientConnection> connection() const;
    void didBecomeUnresponsive() const;
    friend class WebCore::DrawGlyphsRecorder;

    WebCore::RenderingMode renderingMode() const final;

    void save(WebCore::GraphicsContextState::Purpose) final;
    void restore(WebCore::GraphicsContextState::Purpose) final;
    void translate(float x, float y) final;
    void rotate(float angle) final;
    void scale(const WebCore::FloatSize&) final;
    void setCTM(const WebCore::AffineTransform&) final;
    void concatCTM(const WebCore::AffineTransform&) final;
    void setLineCap(WebCore::LineCap) final;
    void setLineDash(const WebCore::DashArray&, float dashOffset) final;
    void setLineJoin(WebCore::LineJoin) final;
    void setMiterLimit(float) final;
    void clip(const WebCore::FloatRect&) final;
    void clipRoundedRect(const WebCore::FloatRoundedRect&) final;
    void clipOut(const WebCore::FloatRect&) final;
    void clipOut(const WebCore::Path&) final;
    void clipOutRoundedRect(const WebCore::FloatRoundedRect&) final;
    void clipPath(const WebCore::Path&, WebCore::WindRule) final;
    void resetClip() final;
    void beginTransparencyLayer(float) final;
    void beginTransparencyLayer(WebCore::CompositeOperator, WebCore::BlendMode) final;
    void endTransparencyLayer() final;
    void drawRect(const WebCore::FloatRect&, float) final;
    void drawLine(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2) final;
    void drawLinesForText(const WebCore::FloatPoint&, float thickness, const WebCore::DashArray& widths, bool printing, bool doubleLines, WebCore::StrokeStyle) final;
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, WebCore::DocumentMarkerLineStyle) final;
    void drawEllipse(const WebCore::FloatRect&) final;
    void drawPath(const WebCore::Path&) final;
    void drawFocusRing(const WebCore::Path&, float outlineWidth, const WebCore::Color&) final;
    void drawFocusRing(const Vector<WebCore::FloatRect>&, float outlineOffset, float outlineWidth, const WebCore::Color&) final;
    void fillRect(const WebCore::FloatRect&, RequiresClipToRect) final;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&) final;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&) final;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&, const WebCore::AffineTransform&, RequiresClipToRect) final;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&, WebCore::CompositeOperator, WebCore::BlendMode) final;
    void fillRoundedRect(const WebCore::FloatRoundedRect&, const WebCore::Color&, WebCore::BlendMode) final;
    void fillRectWithRoundedHole(const WebCore::FloatRect&, const WebCore::FloatRoundedRect&, const WebCore::Color&) final;
    void fillEllipse(const WebCore::FloatRect&) final;
#if ENABLE(VIDEO)
    void drawVideoFrame(WebCore::VideoFrame&, const WebCore::FloatRect& distination, WebCore::ImageOrientation, bool shouldDiscardAlpha) final;
#endif
    void strokeRect(const WebCore::FloatRect&, float) final;
    void strokeEllipse(const WebCore::FloatRect&) final;
    void clearRect(const WebCore::FloatRect&) final;
    void drawControlPart(WebCore::ControlPart&, const WebCore::FloatRoundedRect& borderRect, float deviceScaleFactor, const WebCore::ControlStyle&) final;
#if USE(CG)
    void applyStrokePattern() final;
    void applyFillPattern() final;
#endif
    void applyDeviceScaleFactor(float) final;

private:
    void recordSetInlineFillColor(WebCore::PackedColor::RGBA) final;
    void recordSetInlineStroke(WebCore::DisplayList::SetInlineStroke&&) final;
    void recordSetState(const WebCore::GraphicsContextState&) final;
    void recordClearDropShadow() final;
    void recordClipToImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destinationRect) final;
    void recordDrawFilteredImageBuffer(WebCore::ImageBuffer*, const WebCore::FloatRect& sourceImageRect, WebCore::Filter&) final;
    void recordDrawGlyphs(const WebCore::Font&, const WebCore::GlyphBufferGlyph*, const WebCore::GlyphBufferAdvance*, unsigned count, const WebCore::FloatPoint& localAnchor, WebCore::FontSmoothingMode) final;
    void recordDrawDecomposedGlyphs(const WebCore::Font&, const WebCore::DecomposedGlyphs&) final;
    void recordDrawDisplayListItems(const Vector<WebCore::DisplayList::Item>&, const WebCore::FloatPoint& destination);
    void recordDrawImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions) final;
    void recordDrawNativeImage(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions) final;
    void recordDrawSystemImage(WebCore::SystemImage&, const WebCore::FloatRect&);
    void recordDrawPattern(WebCore::RenderingResourceIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions = { }) final;
#if ENABLE(INLINE_PATH_DATA)
    void recordFillLine(const WebCore::PathDataLine&) final;
    void recordFillArc(const WebCore::PathArc&) final;
    void recordFillClosedArc(const WebCore::PathClosedArc&) final;
    void recordFillQuadCurve(const WebCore::PathDataQuadCurve&) final;
    void recordFillBezierCurve(const WebCore::PathDataBezierCurve&) final;
#endif
    void recordFillPathSegment(const WebCore::PathSegment&) final;
    void recordFillPath(const WebCore::Path&) final;
#if ENABLE(INLINE_PATH_DATA)
    void recordStrokeLine(const WebCore::PathDataLine&) final;
    void recordStrokeLineWithColorAndThickness(const WebCore::PathDataLine&, WebCore::DisplayList::SetInlineStroke&&) final;
    void recordStrokeArc(const WebCore::PathArc&) final;
    void recordStrokeClosedArc(const WebCore::PathClosedArc&) final;
    void recordStrokeQuadCurve(const WebCore::PathDataQuadCurve&) final;
    void recordStrokeBezierCurve(const WebCore::PathDataBezierCurve&) final;
#endif
    void recordStrokePathSegment(const WebCore::PathSegment&) final;
    void recordStrokePath(const WebCore::Path&) final;

    bool recordResourceUse(WebCore::NativeImage&) final;
    bool recordResourceUse(WebCore::ImageBuffer&) final;
    bool recordResourceUse(const WebCore::SourceImage&) final;
    bool recordResourceUse(WebCore::Font&) final;
    bool recordResourceUse(WebCore::DecomposedGlyphs&) final;
    bool recordResourceUse(WebCore::Gradient&) final;
    bool recordResourceUse(WebCore::Filter&) final;

    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, float resolutionScale, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMode>, std::optional<WebCore::RenderingMethod>) const final;
    RefPtr<WebCore::ImageBuffer> createAlignedImageBuffer(const WebCore::FloatSize&, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMethod>) const final;
    RefPtr<WebCore::ImageBuffer> createAlignedImageBuffer(const WebCore::FloatRect&, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMethod>) const final;

    WebCore::RenderingResourceIdentifier m_destinationBufferIdentifier;
    ThreadSafeWeakPtr<RemoteImageBufferProxy> m_imageBuffer;
    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
    WebCore::RenderingMode m_renderingMode;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    Lock m_sharedVideoFrameWriterLock;
    std::unique_ptr<SharedVideoFrameWriter> m_sharedVideoFrameWriter WTF_GUARDED_BY_LOCK(m_sharedVideoFrameWriterLock);
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
