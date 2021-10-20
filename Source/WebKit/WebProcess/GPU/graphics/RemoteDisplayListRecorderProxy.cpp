/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteDisplayListRecorderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteDisplayListRecorderMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListDrawingContext.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

RemoteDisplayListRecorderProxy::RemoteDisplayListRecorderProxy(ImageBuffer& imageBuffer, RemoteRenderingBackendProxy& renderingBackend, const FloatRect& initialClip, const AffineTransform& initialCTM, DrawGlyphsRecorder::DeconstructDrawGlyphs deconstructDrawGlyphs)
    : DisplayList::Recorder({ }, initialClip, initialCTM, deconstructDrawGlyphs)
    , m_destinationBufferIdentifier(imageBuffer.renderingResourceIdentifier())
    , m_imageBuffer(imageBuffer)
    , m_renderingBackend(renderingBackend)
{
}

RemoteDisplayListRecorderProxy::RemoteDisplayListRecorderProxy(RemoteDisplayListRecorderProxy& parent, const FloatRect& initialClip, const AffineTransform& initialCTM)
    : DisplayList::Recorder(parent, { }, initialClip, initialCTM)
    , m_destinationBufferIdentifier(parent.m_destinationBufferIdentifier)
    , m_imageBuffer(parent.m_imageBuffer)
    , m_renderingBackend(parent.m_renderingBackend)
{
}

void RemoteDisplayListRecorderProxy::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& sourceRect)
{
    send(Messages::RemoteDisplayListRecorder::GetPixelBuffer(sourceRect, outputFormat));
}

void RemoteDisplayListRecorderProxy::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    send(Messages::RemoteDisplayListRecorder::PutPixelBuffer(srcRect, destPoint, pixelBuffer, destFormat));
}

bool RemoteDisplayListRecorderProxy::canDrawImageBuffer(const ImageBuffer& imageBuffer) const
{
    return m_renderingBackend && m_renderingBackend->isCached(imageBuffer);
}

RenderingMode RemoteDisplayListRecorderProxy::renderingMode() const
{
    return m_imageBuffer ? m_imageBuffer->renderingMode() : RenderingMode::Unaccelerated;
}

void RemoteDisplayListRecorderProxy::recordSave()
{
    send(Messages::RemoteDisplayListRecorder::Save());
}

void RemoteDisplayListRecorderProxy::recordRestore()
{
    send(Messages::RemoteDisplayListRecorder::Restore());
}

void RemoteDisplayListRecorderProxy::recordTranslate(float x, float y)
{
    send(Messages::RemoteDisplayListRecorder::Translate(x, y));
}

void RemoteDisplayListRecorderProxy::recordRotate(float angle)
{
    send(Messages::RemoteDisplayListRecorder::Rotate(angle));
}

void RemoteDisplayListRecorderProxy::recordScale(const FloatSize& scale)
{
    send(Messages::RemoteDisplayListRecorder::Scale(scale));
}

void RemoteDisplayListRecorderProxy::recordSetCTM(const AffineTransform& transform)
{
    send(Messages::RemoteDisplayListRecorder::SetCTM(transform));
}

void RemoteDisplayListRecorderProxy::recordConcatenateCTM(const AffineTransform& transform)
{
    send(Messages::RemoteDisplayListRecorder::ConcatenateCTM(transform));
}

void RemoteDisplayListRecorderProxy::recordSetInlineFillColor(SRGBA<uint8_t> color)
{
    send(Messages::RemoteDisplayListRecorder::SetInlineFillColor(DisplayList::SetInlineFillColor { color }));
}

void RemoteDisplayListRecorderProxy::recordSetInlineStrokeColor(SRGBA<uint8_t> color)
{
    send(Messages::RemoteDisplayListRecorder::SetInlineStrokeColor(DisplayList::SetInlineStrokeColor { color }));
}

void RemoteDisplayListRecorderProxy::recordSetStrokeThickness(float thickness)
{
    send(Messages::RemoteDisplayListRecorder::SetStrokeThickness(thickness));
}

void RemoteDisplayListRecorderProxy::recordSetState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    send(Messages::RemoteDisplayListRecorder::SetState(DisplayList::SetState { state, flags }));
}

void RemoteDisplayListRecorderProxy::recordSetLineCap(LineCap lineCap)
{
    send(Messages::RemoteDisplayListRecorder::SetLineCap(lineCap));
}

void RemoteDisplayListRecorderProxy::recordSetLineDash(const DashArray& array, float dashOffset)
{
    send(Messages::RemoteDisplayListRecorder::SetLineDash(DisplayList::SetLineDash { array, dashOffset }));
}

void RemoteDisplayListRecorderProxy::recordSetLineJoin(LineJoin lineJoin)
{
    send(Messages::RemoteDisplayListRecorder::SetLineJoin(lineJoin));
}

void RemoteDisplayListRecorderProxy::recordSetMiterLimit(float limit)
{
    send(Messages::RemoteDisplayListRecorder::SetMiterLimit(limit));
}

void RemoteDisplayListRecorderProxy::recordClearShadow()
{
    send(Messages::RemoteDisplayListRecorder::ClearShadow());
}

void RemoteDisplayListRecorderProxy::recordClip(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::Clip(rect));
}

void RemoteDisplayListRecorderProxy::recordClipOut(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::ClipOut(rect));
}

void RemoteDisplayListRecorderProxy::recordClipToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect)
{
    send(Messages::RemoteDisplayListRecorder::ClipToImageBuffer(imageBufferIdentifier, destinationRect));
}

void RemoteDisplayListRecorderProxy::recordClipOutToPath(const Path& path)
{
    send(Messages::RemoteDisplayListRecorder::ClipOutToPath(path));
}

void RemoteDisplayListRecorderProxy::recordClipPath(const Path& path, WindRule rule)
{
    send(Messages::RemoteDisplayListRecorder::ClipPath(path, rule));
}

void RemoteDisplayListRecorderProxy::recordBeginClipToDrawingCommands(const FloatRect& destination, DestinationColorSpace colorSpace)
{
    send(Messages::RemoteDisplayListRecorder::BeginClipToDrawingCommands(destination, colorSpace));
}

void RemoteDisplayListRecorderProxy::recordEndClipToDrawingCommands(const FloatRect& destination)
{
    send(Messages::RemoteDisplayListRecorder::EndClipToDrawingCommands(destination));
}

void RemoteDisplayListRecorderProxy::recordDrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode)
{
    send(Messages::RemoteDisplayListRecorder::DrawGlyphs(DisplayList::DrawGlyphs { font, glyphs, advances, count, localAnchor, mode }));
}

void RemoteDisplayListRecorderProxy::recordDrawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    send(Messages::RemoteDisplayListRecorder::DrawImageBuffer(imageBufferIdentifier, destRect, srcRect, options));
}

void RemoteDisplayListRecorderProxy::recordDrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    send(Messages::RemoteDisplayListRecorder::DrawNativeImage(imageIdentifier, imageSize, destRect, srcRect, options));
}

void RemoteDisplayListRecorderProxy::recordDrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    send(Messages::RemoteDisplayListRecorder::DrawPattern(imageIdentifier, imageSize, destRect, tileRect, transform, phase, spacing, options));
}

void RemoteDisplayListRecorderProxy::recordBeginTransparencyLayer(float opacity)
{
    send(Messages::RemoteDisplayListRecorder::BeginTransparencyLayer(opacity));
}

void RemoteDisplayListRecorderProxy::recordEndTransparencyLayer()
{
    send(Messages::RemoteDisplayListRecorder::EndTransparencyLayer());
}

void RemoteDisplayListRecorderProxy::recordDrawRect(const FloatRect& rect, float width)
{
    send(Messages::RemoteDisplayListRecorder::DrawRect(rect, width));
}

void RemoteDisplayListRecorderProxy::recordDrawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    send(Messages::RemoteDisplayListRecorder::DrawLine(point1, point2));
}

void RemoteDisplayListRecorderProxy::recordDrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines)
{
    send(Messages::RemoteDisplayListRecorder::DrawLinesForText(DisplayList::DrawLinesForText { blockLocation, localAnchor, thickness, widths, printing, doubleLines }));
}

void RemoteDisplayListRecorderProxy::recordDrawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
{
    send(Messages::RemoteDisplayListRecorder::DrawDotsForDocumentMarker(rect, style));
}

void RemoteDisplayListRecorderProxy::recordDrawEllipse(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::DrawEllipse(rect));
}

void RemoteDisplayListRecorderProxy::recordDrawPath(const Path& path)
{
    send(Messages::RemoteDisplayListRecorder::DrawPath(path));
}

void RemoteDisplayListRecorderProxy::recordDrawFocusRingPath(const Path& path, float width, float offset, const Color& color)
{
    send(Messages::RemoteDisplayListRecorder::DrawFocusRingPath(path, width, offset, color));
}

void RemoteDisplayListRecorderProxy::recordDrawFocusRingRects(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    send(Messages::RemoteDisplayListRecorder::DrawFocusRingRects(rects, width, offset, color));
}

void RemoteDisplayListRecorderProxy::recordFillRect(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::FillRect(rect));
}

void RemoteDisplayListRecorderProxy::recordFillRectWithColor(const FloatRect& rect, const Color& color)
{
    send(Messages::RemoteDisplayListRecorder::FillRectWithColor(rect, color));
}

void RemoteDisplayListRecorderProxy::recordFillRectWithGradient(const FloatRect& rect, Gradient& gradient)
{
    send(Messages::RemoteDisplayListRecorder::FillRectWithGradient(DisplayList::FillRectWithGradient { rect, gradient }));
}

void RemoteDisplayListRecorderProxy::recordFillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    send(Messages::RemoteDisplayListRecorder::FillCompositedRect(rect, color, op, mode));
}

void RemoteDisplayListRecorderProxy::recordFillRoundedRect(const FloatRoundedRect& roundedRect, const Color& color, BlendMode mode)
{
    send(Messages::RemoteDisplayListRecorder::FillRoundedRect(roundedRect, color, mode));
}

void RemoteDisplayListRecorderProxy::recordFillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    send(Messages::RemoteDisplayListRecorder::FillRectWithRoundedHole(rect, roundedRect, color));
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordFillLine(const LineData& data)
{
    send(Messages::RemoteDisplayListRecorder::FillLine(data));
}

void RemoteDisplayListRecorderProxy::recordFillArc(const ArcData& data)
{
    send(Messages::RemoteDisplayListRecorder::FillArc(data));
}

void RemoteDisplayListRecorderProxy::recordFillQuadCurve(const QuadCurveData& data)
{
    send(Messages::RemoteDisplayListRecorder::FillQuadCurve(data));
}

void RemoteDisplayListRecorderProxy::recordFillBezierCurve(const BezierCurveData& data)
{
    send(Messages::RemoteDisplayListRecorder::FillBezierCurve(data));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordFillPath(const Path& path)
{
    send(Messages::RemoteDisplayListRecorder::FillPath(path));
}

void RemoteDisplayListRecorderProxy::recordFillEllipse(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::FillEllipse(rect));
}

void RemoteDisplayListRecorderProxy::recordPaintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    send(Messages::RemoteDisplayListRecorder::PaintFrameForMedia(player.identifier(), destination));
}

void RemoteDisplayListRecorderProxy::recordStrokeRect(const FloatRect& rect, float width)
{
    send(Messages::RemoteDisplayListRecorder::StrokeRect(rect, width));
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordStrokeLine(const LineData& data)
{
    send(Messages::RemoteDisplayListRecorder::StrokeLine(data));
}

void RemoteDisplayListRecorderProxy::recordStrokeArc(const ArcData& data)
{
    send(Messages::RemoteDisplayListRecorder::StrokeArc(data));
}

void RemoteDisplayListRecorderProxy::recordStrokeQuadCurve(const QuadCurveData& data)
{
    send(Messages::RemoteDisplayListRecorder::StrokeQuadCurve(data));
}

void RemoteDisplayListRecorderProxy::recordStrokeBezierCurve(const BezierCurveData& data)
{
    send(Messages::RemoteDisplayListRecorder::StrokeBezierCurve(data));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordStrokePath(const Path& path)
{
    send(Messages::RemoteDisplayListRecorder::StrokePath(path));
}

void RemoteDisplayListRecorderProxy::recordStrokeEllipse(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::StrokeEllipse(rect));
}

void RemoteDisplayListRecorderProxy::recordClearRect(const FloatRect& rect)
{
    send(Messages::RemoteDisplayListRecorder::ClearRect(rect));
}

#if USE(CG)

void RemoteDisplayListRecorderProxy::recordApplyStrokePattern()
{
    send(Messages::RemoteDisplayListRecorder::ApplyStrokePattern());
}

void RemoteDisplayListRecorderProxy::recordApplyFillPattern()
{
    send(Messages::RemoteDisplayListRecorder::ApplyFillPattern());
}

#endif // USE(CG)

void RemoteDisplayListRecorderProxy::recordApplyDeviceScaleFactor(float scaleFactor)
{
    send(Messages::RemoteDisplayListRecorder::ApplyDeviceScaleFactor(scaleFactor));
}

void RemoteDisplayListRecorderProxy::recordResourceUse(NativeImage& image)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_renderingBackend->recordNativeImageUse(image);
}

void RemoteDisplayListRecorderProxy::recordResourceUse(Font& font)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_renderingBackend->recordFontUse(font);
}

void RemoteDisplayListRecorderProxy::recordResourceUse(ImageBuffer& imageBuffer)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_renderingBackend->recordImageBufferUse(imageBuffer);
}

void RemoteDisplayListRecorderProxy::flushContext(GraphicsContextFlushIdentifier identifier)
{
    send(Messages::RemoteDisplayListRecorder::FlushContext(identifier));
}

std::unique_ptr<GraphicsContext> RemoteDisplayListRecorderProxy::createNestedContext(const FloatRect& initialClip, const AffineTransform& initialCTM)
{
    return makeUnique<RemoteDisplayListRecorderProxy>(*this, initialClip, initialCTM);
}

} // namespace WebCore

#endif // ENABLE(GPU_PROCESS)
