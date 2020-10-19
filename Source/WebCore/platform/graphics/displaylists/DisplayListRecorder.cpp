/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Recorder::Recorder(GraphicsContext& context, DisplayList& displayList, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, Observer* observer)
    : GraphicsContextImpl(context, initialClip, AffineTransform())
    , m_displayList(displayList)
    , m_observer(observer)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nRecording with clip " << initialClip);
    m_stateStack.append({ state, initialCTM, initialClip });
}

Recorder::~Recorder()
{
    ASSERT(m_stateStack.size() == 1); // If this fires, it indicates mismatched save/restore.
    LOG(DisplayLists, "Recorded display list:\n%s", m_displayList.description().data());
}

void Recorder::putImageData(WebCore::AlphaPremultiplication inputFormat, const WebCore::ImageData& imageData, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat)
{
    appendItem(WebCore::DisplayList::PutImageData::create(inputFormat, imageData, srcRect, destPoint, destFormat));
}

void Recorder::willAppendItem(const Item& item)
{
    if (m_observer)
        m_observer->willAppendItem(item);

    if (item.isDrawingItem()
#if USE(CG)
        || item.type() == ItemType::ApplyStrokePattern || item.type() == ItemType::ApplyStrokePattern
#endif
    ) {
        GraphicsContextStateChange& stateChanges = currentState().stateChange;
        GraphicsContextState::StateChangeFlags changesFromLastState = stateChanges.changesFromState(currentState().lastDrawingState);
        if (changesFromLastState) {
            LOG_WITH_STREAM(DisplayLists, stream << "pre-drawing, saving state " << GraphicsContextStateChange(stateChanges.m_state, changesFromLastState));
            m_displayList.append(SetState::create(stateChanges.m_state, changesFromLastState));
            stateChanges.m_changeFlags = { };
            currentState().lastDrawingState = stateChanges.m_state;
        }
        currentState().wasUsedForDrawing = true;
    }
}

void Recorder::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    currentState().stateChange.accumulate(state, flags);
}

void Recorder::clearShadow()
{
    appendItem(ClearShadow::create());
}

void Recorder::setLineCap(LineCap lineCap)
{
    appendItem(SetLineCap::create(lineCap));
}

void Recorder::setLineDash(const DashArray& dashArray, float dashOffset)
{
    appendItem(SetLineDash::create(dashArray, dashOffset));
}

void Recorder::setLineJoin(LineJoin lineJoin)
{
    appendItem(SetLineJoin::create(lineJoin));
}

void Recorder::setMiterLimit(float miterLimit)
{
    appendItem(SetMiterLimit::create(miterLimit));
}

void Recorder::drawGlyphs(const Font& font, const GlyphBuffer& glyphBuffer, unsigned from, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    appendItemAndUpdateExtent(DrawGlyphs::create(font, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs, startPoint, smoothingMode));
}

ImageDrawResult Recorder::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
{
    appendItemAndUpdateExtent(DrawImage::create(image, destination, source, imagePaintingOptions));
    return ImageDrawResult::DidRecord;
}

ImageDrawResult Recorder::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& imagePaintingOptions)
{
    appendItemAndUpdateExtent(DrawTiledImage::create(image, destination, source, tileSize, spacing, imagePaintingOptions));
    return ImageDrawResult::DidRecord;
}

ImageDrawResult Recorder::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& imagePaintingOptions)
{
    appendItemAndUpdateExtent(DrawTiledScaledImage::create(image, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions));
    return ImageDrawResult::DidRecord;
}

void Recorder::drawNativeImage(const NativeImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    appendItemAndUpdateExtent(DrawNativeImage::create(image, imageSize, destRect, srcRect, options));
}

void Recorder::drawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    appendItemAndUpdateExtent(DrawPattern::create(image, destRect, tileRect, patternTransform, phase, spacing, options));
}

void Recorder::save()
{
    appendItem(Save::create());
    m_stateStack.append(m_stateStack.last().cloneForSave());
}

void Recorder::restore()
{
    if (!m_stateStack.size())
        return;

    bool stateUsedForDrawing = currentState().wasUsedForDrawing;

    m_stateStack.removeLast();
    // Have to avoid eliding nested Save/Restore when a descendant state contains drawing items.
    currentState().wasUsedForDrawing |= stateUsedForDrawing;

    appendItem(Restore::create());
}

void Recorder::translate(float x, float y)
{
    currentState().translate(x, y);
    appendItem(Translate::create(x, y));
}

void Recorder::rotate(float angleInRadians)
{
    currentState().rotate(angleInRadians);
    appendItem(Rotate::create(angleInRadians));
}

void Recorder::scale(const FloatSize& size)
{
    currentState().scale(size);
    appendItem(Scale::create(size));
}

void Recorder::concatCTM(const AffineTransform& transform)
{
    currentState().concatCTM(transform);
    appendItem(ConcatenateCTM::create(transform));
}

void Recorder::setCTM(const AffineTransform& transform)
{
    currentState().setCTM(transform);
    appendItem(SetCTM::create(transform));
}

AffineTransform Recorder::getCTM(GraphicsContext::IncludeDeviceScale)
{
    // FIXME: Respect the given value of IncludeDeviceScale.
    return currentState().ctm;
}

void Recorder::beginTransparencyLayer(float opacity)
{
    appendItemAndUpdateExtent(BeginTransparencyLayer::create(opacity));
}

void Recorder::endTransparencyLayer()
{
    appendItem(EndTransparencyLayer::create());
}

void Recorder::drawRect(const FloatRect& rect, float borderThickness)
{
    appendItemAndUpdateExtent(DrawRect::create(rect, borderThickness));
}

void Recorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendItemAndUpdateExtent(DrawLine::create(point1, point2));
}

void Recorder::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines)
{
    appendItemAndUpdateExtent(DrawLinesForText::create(FloatPoint(), toFloatSize(point), thickness, widths, printing, doubleLines));
}

void Recorder::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendItemAndUpdateExtent(DrawDotsForDocumentMarker::create(rect, style));
}

void Recorder::drawEllipse(const FloatRect& rect)
{
    appendItemAndUpdateExtent(DrawEllipse::create(rect));
}

void Recorder::drawPath(const Path& path)
{
    appendItemAndUpdateExtent(DrawPath::create(path));
}

void Recorder::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    appendItemAndUpdateExtent(DrawFocusRingPath::create(path, width, offset, color));
}

void Recorder::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    appendItemAndUpdateExtent(DrawFocusRingRects::create(rects, width, offset, color));
}

void Recorder::fillRect(const FloatRect& rect)
{
    appendItemAndUpdateExtent(FillRect::create(rect));
}

void Recorder::fillRect(const FloatRect& rect, const Color& color)
{
    appendItemAndUpdateExtent(FillRectWithColor::create(rect, color));
}

void Recorder::fillRect(const FloatRect& rect, Gradient& gradient)
{
    appendItemAndUpdateExtent(FillRectWithGradient::create(rect, gradient));
}

void Recorder::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    appendItemAndUpdateExtent(FillCompositedRect::create(rect, color, op, blendMode));
}

void Recorder::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    appendItemAndUpdateExtent(FillRoundedRect::create(rect, color, blendMode));
}

void Recorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    appendItemAndUpdateExtent(FillRectWithRoundedHole::create(rect, roundedHoleRect, color));
}

void Recorder::fillPath(const Path& path)
{
    appendItemAndUpdateExtent(FillPath::create(path));
}

void Recorder::fillEllipse(const FloatRect& rect)
{
    appendItemAndUpdateExtent(FillEllipse::create(rect));
}

void Recorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    appendItemAndUpdateExtent(StrokeRect::create(rect, lineWidth));
}

void Recorder::strokePath(const Path& path)
{
    appendItemAndUpdateExtent(StrokePath::create(path));
}

void Recorder::strokeEllipse(const FloatRect& rect)
{
    appendItemAndUpdateExtent(StrokeEllipse::create(rect));
}

void Recorder::clearRect(const FloatRect& rect)
{
    appendItemAndUpdateExtent(ClearRect::create(rect));
}

#if USE(CG)
void Recorder::applyStrokePattern()
{
    appendItem(ApplyStrokePattern::create());
}

void Recorder::applyFillPattern()
{
    appendItem(ApplyFillPattern::create());
}
#endif

void Recorder::clip(const FloatRect& rect)
{
    currentState().clipBounds.intersect(rect);
    appendItem(Clip::create(rect));
}

void Recorder::clipOut(const FloatRect& rect)
{
    appendItem(ClipOut::create(rect));
}

void Recorder::clipOut(const Path& path)
{
    appendItem(ClipOutToPath::create(path));
}

void Recorder::clipPath(const Path& path, WindRule windRule)
{
    currentState().clipBounds.intersect(path.fastBoundingRect());
    appendItem(ClipPath::create(path, windRule));
}

IntRect Recorder::clipBounds()
{
    WTFLogAlways("Getting the clip bounds not yet supported with DisplayList::Recorder.");
    return IntRect(-2048, -2048, 4096, 4096);
}

void Recorder::clipToImageBuffer(ImageBuffer&, const FloatRect&)
{
    WTFLogAlways("GraphicsContext::clipToImageBuffer is not compatible with DisplayList::Recorder.");
}

void Recorder::clipToDrawingCommands(const FloatRect& destination, ColorSpace colorSpace, Function<void(GraphicsContext&)>&& drawingFunction)
{
    auto recordingContext = makeUnique<DrawingContext>(destination.size());
    drawingFunction(recordingContext->context());
    appendItem(ClipToDrawingCommands::create(destination, colorSpace, recordingContext->takeDisplayList()));
}

void Recorder::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    appendItem(PaintFrameForMedia::create(player, destination));
}

void Recorder::applyDeviceScaleFactor(float deviceScaleFactor)
{
    // FIXME: this changes the baseCTM, which will invalidate all of our cached extents.
    // Assert that it's only called early on?
    appendItem(ApplyDeviceScaleFactor::create(deviceScaleFactor));
}

FloatRect Recorder::roundToDevicePixels(const FloatRect& rect, GraphicsContext::RoundingMode)
{
    WTFLogAlways("GraphicsContext::roundToDevicePixels() is not yet compatible with DisplayList::Recorder.");
    return rect;
}

void Recorder::appendItemAndUpdateExtent(Ref<DrawingItem>&& item)
{
    auto& newItem = appendItem(WTFMove(item));
    updateItemExtent(newItem);
}

template<typename ItemType>
ItemType& Recorder::appendItem(Ref<ItemType>&& item)
{
    willAppendItem(item.get());
    return downcast<ItemType>(m_displayList.append(WTFMove(item)));
}

void Recorder::updateItemExtent(DrawingItem& item) const
{
    if (Optional<FloatRect> rect = item.localBounds(graphicsContext()))
        item.setExtent(extentFromLocalBounds(rect.value()));
    else if (Optional<FloatRect> rect = item.globalBounds())
        item.setExtent(rect.value());
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

FloatRect Recorder::extentFromLocalBounds(const FloatRect& rect) const
{
    FloatRect bounds = rect;
    const ContextState& state = currentState();

    FloatSize shadowOffset;
    float shadowRadius;
    Color shadowColor;
    if (graphicsContext().getShadow(shadowOffset, shadowRadius, shadowColor)) {
        FloatRect shadowExtent= bounds;
        shadowExtent.move(shadowOffset);
        shadowExtent.inflate(shadowPaintingExtent(shadowRadius));
        bounds.unite(shadowExtent);
    }
    
    FloatRect clippedExtent = intersection(state.clipBounds, bounds);
    return state.ctm.mapRect(clippedExtent);
}

const Recorder::ContextState& Recorder::currentState() const
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

Recorder::ContextState& Recorder::currentState()
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

const AffineTransform& Recorder::ctm() const
{
    return currentState().ctm;
}

const FloatRect& Recorder::clipBounds() const
{
    return currentState().clipBounds;
}

void Recorder::ContextState::translate(float x, float y)
{
    ctm.translate(x, y);
    clipBounds.move(-x, -y);
}

void Recorder::ContextState::rotate(float angleInRadians)
{
    double angleInDegrees = rad2deg(static_cast<double>(angleInRadians));
    ctm.rotate(angleInDegrees);
    
    AffineTransform rotation;
    rotation.rotate(angleInDegrees);

    if (Optional<AffineTransform> inverse = rotation.inverse())
        clipBounds = inverse.value().mapRect(clipBounds);
}

void Recorder::ContextState::scale(const FloatSize& size)
{
    ctm.scale(size);
    clipBounds.scale(1 / size.width(), 1 / size.height());
}

void Recorder::ContextState::setCTM(const AffineTransform& matrix)
{
    Optional<AffineTransform> inverseTransformForClipBounds;
    if (auto originalCTMInverse = ctm.inverse())
        inverseTransformForClipBounds = originalCTMInverse->multiply(matrix).inverse();

    ctm = matrix;

    if (inverseTransformForClipBounds)
        clipBounds = inverseTransformForClipBounds->mapRect(clipBounds);
}

void Recorder::ContextState::concatCTM(const AffineTransform& matrix)
{
    ctm *= matrix;

    if (Optional<AffineTransform> inverse = matrix.inverse())
        clipBounds = inverse.value().mapRect(clipBounds);
}

} // namespace DisplayList
} // namespace WebCore
