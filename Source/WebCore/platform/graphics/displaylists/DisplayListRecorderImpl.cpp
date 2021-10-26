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
#include "DisplayListRecorder.h"

#include "DisplayList.h"
#include "DisplayListDrawingContext.h"
#include "DisplayListItems.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

RecorderImpl::RecorderImpl(DisplayList& displayList, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, Delegate* delegate, DrawGlyphsRecorder::DeconstructDrawGlyphs deconstructDrawGlyphs)
    : Recorder(state, initialClip, initialCTM, deconstructDrawGlyphs)
    , m_displayList(displayList)
    , m_delegate(delegate)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nRecording with clip " << initialClip);
}

RecorderImpl::RecorderImpl(RecorderImpl& parent, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM)
    : Recorder(parent, state, initialClip, initialCTM)
    , m_displayList(parent.m_displayList)
    , m_delegate(parent.m_delegate)
    , m_isNested(true)
{
}

RecorderImpl::~RecorderImpl()
{
    ASSERT(stateStack().size() == 1); // If this fires, it indicates mismatched save/restore.
    if (!m_isNested)
        LOG(DisplayLists, "Recorded display list:\n%s", m_displayList.description().data());
}

void RecorderImpl::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& sourceRect)
{
    append<GetPixelBuffer>(outputFormat, sourceRect);
}

void RecorderImpl::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    append<PutPixelBuffer>(pixelBuffer, srcRect, destPoint, destFormat);
}

bool RecorderImpl::canAppendItemOfType(ItemType type) const
{
    return !m_delegate || m_delegate->canAppendItemOfType(type);
}

bool RecorderImpl::canDrawImageBuffer(const ImageBuffer& imageBuffer) const
{
    return !m_delegate || m_delegate->isCachedImageBuffer(imageBuffer);
}

RenderingMode RecorderImpl::renderingMode() const
{
    return m_delegate ? m_delegate->renderingMode() : RenderingMode::Unaccelerated;
}

std::unique_ptr<GraphicsContext> RecorderImpl::createNestedContext(const FloatRect& initialClip, const AffineTransform& initialCTM)
{
    return makeUnique<RecorderImpl>(*this, GraphicsContextState { }, initialClip, initialCTM);
}

void RecorderImpl::recordSave()
{
    append<Save>();
}

void RecorderImpl::recordRestore()
{
    append<Restore>();
}

void RecorderImpl::recordTranslate(float x, float y)
{
    append<Translate>(x, y);
}

void RecorderImpl::recordRotate(float angle)
{
    append<Rotate>(angle);
}

void RecorderImpl::recordScale(const FloatSize& scale)
{
    append<Scale>(scale);
}

void RecorderImpl::recordSetCTM(const AffineTransform& transform)
{
    append<SetCTM>(transform);
}

void RecorderImpl::recordConcatenateCTM(const AffineTransform& transform)
{
    append<ConcatenateCTM>(transform);
}

void RecorderImpl::recordSetInlineFillColor(SRGBA<uint8_t> inlineColor)
{
    append<SetInlineFillColor>(inlineColor);
}

void RecorderImpl::recordSetInlineStrokeColor(SRGBA<uint8_t> inlineColor)
{
    append<SetInlineStrokeColor>(inlineColor);
}

void RecorderImpl::recordSetStrokeThickness(float thickness)
{
    append<SetStrokeThickness>(thickness);
}

void RecorderImpl::recordSetState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags changeFlags)
{
    append<SetState>(state, changeFlags);
}

void RecorderImpl::recordSetLineCap(LineCap lineCap)
{
    append<SetLineCap>(lineCap);
}

void RecorderImpl::recordSetLineDash(const DashArray& array, float dashOffset)
{
    append<SetLineDash>(array, dashOffset);
}

void RecorderImpl::recordSetLineJoin(LineJoin join)
{
    append<SetLineJoin>(join);
}

void RecorderImpl::recordSetMiterLimit(float limit)
{
    append<SetMiterLimit>(limit);
}

void RecorderImpl::recordClearShadow()
{
    append<ClearShadow>();
}

void RecorderImpl::recordClip(const FloatRect& clipRect)
{
    append<Clip>(clipRect);
}

void RecorderImpl::recordClipOut(const FloatRect& clipRect)
{
    append<ClipOut>(clipRect);
}

void RecorderImpl::recordClipToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect)
{
    append<ClipToImageBuffer>(imageBufferIdentifier, destinationRect);
}

void RecorderImpl::recordClipOutToPath(const Path& path)
{
    append<ClipOutToPath>(path);
}

void RecorderImpl::recordClipPath(const Path& path, WindRule rule)
{
    append<ClipPath>(path, rule);
}

void RecorderImpl::recordBeginClipToDrawingCommands(const FloatRect& destination, DestinationColorSpace colorSpace)
{
    append<BeginClipToDrawingCommands>(destination, colorSpace);
}

void RecorderImpl::recordEndClipToDrawingCommands(const FloatRect& destination)
{
    append<EndClipToDrawingCommands>(destination);
}

void RecorderImpl::recordDrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode)
{
    append<DrawGlyphs>(font, glyphs, advances, count, localAnchor, mode);
}

void RecorderImpl::recordDrawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    append<DrawImageBuffer>(imageBufferIdentifier, destRect, srcRect, options);
}

void RecorderImpl::recordDrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    append<DrawNativeImage>(imageIdentifier, imageSize, destRect, srcRect, options);
}

void RecorderImpl::recordDrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    append<DrawPattern>(imageIdentifier, imageSize, destRect, tileRect, transform, phase, spacing, options);
}

void RecorderImpl::recordBeginTransparencyLayer(float opacity)
{
    append<BeginTransparencyLayer>(opacity);
}

void RecorderImpl::recordEndTransparencyLayer()
{
    append<EndTransparencyLayer>();
}

void RecorderImpl::recordDrawRect(const FloatRect& rect, float lineWidth)
{
    append<DrawRect>(rect, lineWidth);
}

void RecorderImpl::recordDrawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    append<DrawLine>(point1, point2);
}

void RecorderImpl::recordDrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines)
{
    append<DrawLinesForText>(blockLocation, localAnchor, thickness, widths, printing, doubleLines);
}

void RecorderImpl::recordDrawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
{
    append<DrawDotsForDocumentMarker>(rect, style);
}

void RecorderImpl::recordDrawEllipse(const FloatRect& rect)
{
    append<DrawEllipse>(rect);
}

void RecorderImpl::recordDrawPath(const Path& path)
{
    append<DrawPath>(path);
}

void RecorderImpl::recordDrawFocusRingPath(const Path& path, float width, float offset, const Color& color)
{
    append<DrawFocusRingPath>(path, width, offset, color);
}

void RecorderImpl::recordDrawFocusRingRects(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    append<DrawFocusRingRects>(rects, width, offset, color);
}

void RecorderImpl::recordFillRect(const FloatRect& rect)
{
    append<FillRect>(rect);
}

void RecorderImpl::recordFillRectWithColor(const FloatRect& rect, const Color& color)
{
    append<FillRectWithColor>(rect, color);
}

void RecorderImpl::recordFillRectWithGradient(const FloatRect& rect, Gradient& gradient)
{
    append<FillRectWithGradient>(rect, gradient);
}

void RecorderImpl::recordFillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    append<FillCompositedRect>(rect, color, op, mode);
}

void RecorderImpl::recordFillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode mode)
{
    append<FillRoundedRect>(rect, color, mode);
}

void RecorderImpl::recordFillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    append<FillRectWithRoundedHole>(rect, roundedRect, color);
}

#if ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordFillLine(const LineData& line)
{
    append<FillLine>(line);
}

void RecorderImpl::recordFillArc(const ArcData& arc)
{
    append<FillArc>(arc);
}

void RecorderImpl::recordFillQuadCurve(const QuadCurveData& curve)
{
    append<FillQuadCurve>(curve);
}

void RecorderImpl::recordFillBezierCurve(const BezierCurveData& curve)
{
    append<FillBezierCurve>(curve);
}

#endif // ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordFillPath(const Path& path)
{
    append<FillPath>(path);
}

void RecorderImpl::recordFillEllipse(const FloatRect& rect)
{
    append<FillEllipse>(rect);
}

#if ENABLE(VIDEO)
void RecorderImpl::recordPaintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    append<PaintFrameForMedia>(player, destination);
}
#endif // ENABLE(VIDEO)

void RecorderImpl::recordStrokeRect(const FloatRect& rect, float width)
{
    append<StrokeRect>(rect, width);
}

#if ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordStrokeLine(const LineData& line)
{
    append<StrokeLine>(line);
}

void RecorderImpl::recordStrokeArc(const ArcData& arc)
{
    append<StrokeArc>(arc);
}

void RecorderImpl::recordStrokeQuadCurve(const QuadCurveData& curve)
{
    append<StrokeQuadCurve>(curve);
}

void RecorderImpl::recordStrokeBezierCurve(const BezierCurveData& curve)
{
    append<StrokeBezierCurve>(curve);
}

#endif // ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordStrokePath(const Path& path)
{
    append<StrokePath>(path);
}

void RecorderImpl::recordStrokeEllipse(const FloatRect& rect)
{
    append<StrokeEllipse>(rect);
}

void RecorderImpl::recordClearRect(const FloatRect& rect)
{
    append<ClearRect>(rect);
}

#if USE(CG)

void RecorderImpl::recordApplyStrokePattern()
{
    append<ApplyStrokePattern>();
}

void RecorderImpl::recordApplyFillPattern()
{
    append<ApplyFillPattern>();
}

#endif // USE(CG)

void RecorderImpl::recordApplyDeviceScaleFactor(float scaleFactor)
{
    append<ApplyDeviceScaleFactor>(scaleFactor);
}

void RecorderImpl::recordResourceUse(NativeImage& image)
{
    if (m_delegate)
        m_delegate->recordNativeImageUse(image);
    m_displayList.cacheNativeImage(image);
}

void RecorderImpl::recordResourceUse(Font& font)
{
    if (m_delegate)
        m_delegate->recordFontUse(font);
    m_displayList.cacheFont(font);
}

void RecorderImpl::recordResourceUse(ImageBuffer& imageBuffer)
{
    if (m_delegate)
        m_delegate->recordImageBufferUse(imageBuffer);
    m_displayList.cacheImageBuffer(imageBuffer);
}

// FIXME: share with ShadowData
static inline float shadowPaintingExtent(float blurRadius)
{
    // Blurring uses a Gaussian function whose std. deviation is m_radius/2, and which in theory
    // extends to infinity. In 8-bit contexts, however, rounding causes the effect to become
    // undetectable at around 1.4x the radius.
    const float radiusExtentMultiplier = 1.4;
    return ceilf(blurRadius * radiusExtentMultiplier);
}

FloatRect RecorderImpl::extentFromLocalBounds(const FloatRect& rect) const
{
    FloatRect bounds = rect;
    auto& state = currentState();

    FloatSize shadowOffset;
    float shadowRadius;
    Color shadowColor;
    if (getShadow(shadowOffset, shadowRadius, shadowColor)) {
        FloatRect shadowExtent = bounds;
        shadowExtent.move(shadowOffset);
        shadowExtent.inflate(shadowPaintingExtent(shadowRadius));
        bounds.unite(shadowExtent);
    }

    return intersection(state.clipBounds, state.ctm.mapRect(bounds));
}

} // namespace DisplayList
} // namespace WebCore
