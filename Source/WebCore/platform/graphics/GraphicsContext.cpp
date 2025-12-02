/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "GraphicsContext.h"

#include "BidiResolver.h"
#include "DisplayList.h"
#include "Filter.h"
#include "FilterImage.h"
#include "FloatRoundedRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "ImageOrientation.h"
#include "IntRect.h"
#include "LayoutRoundedRect.h"
#include "SystemImage.h"
#include "TextRunIterator.h"
#include "VideoFrame.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsContext);

GraphicsContext::GraphicsContext(IsDeferred isDeferred, const GraphicsContextState::ChangeFlags& changeFlags, InterpolationQuality imageInterpolationQuality)
    : m_state(changeFlags, imageInterpolationQuality)
    , m_isDeferred(isDeferred)
{
}

GraphicsContext::GraphicsContext(IsDeferred isDeferred, const GraphicsContextState& state)
    : m_state(state)
    , m_isDeferred(isDeferred)
{
}

GraphicsContext::~GraphicsContext()
{
    ASSERT(m_stack.isEmpty());
    ASSERT(!m_transparencyLayerCount);
}

void GraphicsContext::save(GraphicsContextState::Purpose purpose)
{
    ASSERT(purpose == GraphicsContextState::Purpose::SaveRestore || purpose == GraphicsContextState::Purpose::TransparencyLayer);
    m_stack.append(m_state);
    m_state.repurpose(purpose);
}

void GraphicsContext::restore(GraphicsContextState::Purpose purpose)
{
    if (m_stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }

    ASSERT_UNUSED(purpose, purpose == m_state.purpose());
    ASSERT_UNUSED(purpose, purpose == GraphicsContextState::Purpose::SaveRestore || purpose == GraphicsContextState::Purpose::TransparencyLayer);

    m_state = m_stack.last();
    m_stack.removeLast();

    // Make sure we deallocate the state stack buffer when it goes empty.
    // Canvas elements will immediately save() again, but that goes into inline capacity.
    if (m_stack.isEmpty())
        m_stack.clear();
}

void GraphicsContext::unwindStateStack(unsigned count)
{
    ASSERT(count <= stackSize());
    while (count-- > 0) {
        switch (m_state.purpose()) {
        case GraphicsContextState::Purpose::SaveRestore:
            restore();
            break;
        case GraphicsContextState::Purpose::TransparencyLayer:
            endTransparencyLayer();
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void GraphicsContext::unwindStateStack()
{
    unwindStateStack(stackSize());
}

FloatSize GraphicsContext::platformShadowOffset(const FloatSize& shadowOffset) const
{
#if USE(CG)
    if (shadowsIgnoreTransforms())
        return { shadowOffset.width(), -shadowOffset.height() };
#endif
    return shadowOffset;
}

void GraphicsContext::mergeLastChanges(const GraphicsContextState& state, const std::optional<GraphicsContextState>& lastDrawingState)
{
    m_state.mergeLastChanges(state, lastDrawingState);
    didUpdateState(m_state);
}

void GraphicsContext::mergeAllChanges(const GraphicsContextState& state)
{
    m_state.mergeAllChanges(state);
    didUpdateState(m_state);
}

void GraphicsContext::drawRaisedEllipse(const FloatRect& rect, const Color& ellipseColor, const Color& shadowColor)
{
    save();

    setStrokeColor(shadowColor);
    setFillColor(shadowColor);

    drawEllipse(FloatRect(rect.x(), rect.y() + 1, rect.width(), rect.height()));

    setStrokeColor(ellipseColor);
    setFillColor(ellipseColor);

    drawEllipse(rect);

    restore();
}

void GraphicsContext::beginTransparencyLayer(float)
{
    ++m_transparencyLayerCount;
}

void GraphicsContext::beginTransparencyLayer(CompositeOperator, BlendMode)
{
    ++m_transparencyLayerCount;
}

void GraphicsContext::endTransparencyLayer()
{
    ASSERT(m_transparencyLayerCount > 0);
    --m_transparencyLayerCount;
}

FloatSize GraphicsContext::drawText(const FontCascade& font, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    // Display list recording for text content is done at glyphs level. See GraphicsContext::drawGlyphs.
    return font.drawText(*this, run, point, from, to);
}

void GraphicsContext::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    FontCascade::drawGlyphs(*this, font, glyphs, advances, point, fontSmoothingMode);
}

void GraphicsContext::drawGlyphsImmediate(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    // Called by implementations that transform drawGlyphs into drawGlyphsImmediate, drawImageBuffer, etc calls.
    drawGlyphs(font, glyphs, advances, point, fontSmoothingMode);
}

void GraphicsContext::drawEmphasisMarks(const FontCascade& font, const TextRun& run, const AtomString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    font.drawEmphasisMarks(*this, run, mark, point, from, to);
}

void GraphicsContext::drawBidiText(const FontCascade& font, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
    bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

    // FIXME: This ownership should be reversed. We should pass BidiRunList
    // to BidiResolver in createBidiRunsForLine.
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));

    if (!bidiRuns.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
        bool isRTL = bidiRun->level() % 2;
        subrun.setDirection(isRTL ? TextDirection::RTL : TextDirection::LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        auto advance = font.drawText(*this, subrun, currPoint, 0, std::nullopt, customFontNotReadyAction);
        currPoint.move(advance);

        bidiRun = bidiRun->next();
    }

    bidiRuns.clear();
}

static IntSize scaledImageBufferSize(const FloatSize& size, const FloatSize& scale)
{
    // Enlarge the buffer size if the context's transform is scaling it so we need a higher
    // resolution than one pixel per unit.
    return expandedIntSize(size * scale);
}

static IntRect scaledImageBufferRect(const FloatRect& rect, const FloatSize& scale)
{
    auto scaledRect = rect;
    scaledRect.scale(scale);
    return enclosingIntRect(scaledRect);
}

static FloatSize clampingScaleForImageBufferSize(const FloatSize& size)
{
    FloatSize clampingScale(1, 1);
    ImageBuffer::sizeNeedsClamping(size, clampingScale);
    return clampingScale;
}

IntSize GraphicsContext::compatibleImageBufferSize(const FloatSize& size) const
{
    return scaledImageBufferSize(size, scaleFactor());
}

RenderingMode GraphicsContext::renderingModeForCompatibleBuffer() const
{
    switch (renderingMode()) {
    case RenderingMode::Accelerated:
    case RenderingMode::Unaccelerated:
    case RenderingMode::DisplayList:
        return renderingMode();
    case RenderingMode::PDFDocument:
        return RenderingMode::Unaccelerated;
    }

    return RenderingMode::Unaccelerated;
}

RefPtr<ImageBuffer> GraphicsContext::createImageBuffer(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod>, ImageBufferFormat pixelFormat) const
{
    return ImageBuffer::create(size, renderingMode.value_or(this->renderingModeForCompatibleBuffer()), RenderingPurpose::Unspecified, resolutionScale, colorSpace, pixelFormat);
}

RefPtr<ImageBuffer> GraphicsContext::createScaledImageBuffer(const FloatSize& size, const FloatSize& scale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod> renderingMethod) const
{
    auto expandedScaledSize = scaledImageBufferSize(size, scale);
    if (expandedScaledSize.isEmpty())
        return nullptr;

    auto clampingScale = clampingScaleForImageBufferSize(expandedScaledSize);

    auto imageBuffer = createImageBuffer(expandedScaledSize * clampingScale, 1, colorSpace, renderingMode, renderingMethod);
    if (!imageBuffer)
        return nullptr;

    imageBuffer->context().scale(clampingScale);

    // 'expandedScaledSize' is mapped to 'size'. So use 'expandedScaledSize / size'
    // not 'scale' because they are not necessarily equal.
    imageBuffer->context().scale(expandedScaledSize / size);
    return imageBuffer;
}

RefPtr<ImageBuffer> GraphicsContext::createScaledImageBuffer(const FloatRect& rect, const FloatSize& scale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod> renderingMethod) const
{
    auto expandedScaledRect = scaledImageBufferRect(rect, scale);
    if (expandedScaledRect.isEmpty())
        return nullptr;

    auto clampingScale = clampingScaleForImageBufferSize(expandedScaledRect.size());

    auto imageBuffer = createImageBuffer(expandedScaledRect.size() * clampingScale, 1, colorSpace, renderingMode, renderingMethod);
    if (!imageBuffer)
        return nullptr;

    imageBuffer->context().scale(clampingScale);
    
    // 'rect' is mapped to a rectangle inside expandedScaledRect.
    imageBuffer->context().translate(-expandedScaledRect.location());
    
    // The size of this rectangle is not necessarily equal to expandedScaledRect.size().
    // So use 'scale' not 'expandedScaledRect.size() / rect.size()'.
    imageBuffer->context().scale(scale);
    return imageBuffer;
}

RefPtr<ImageBuffer> GraphicsContext::createAlignedImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    return createScaledImageBuffer(size, scaleFactor(), colorSpace, renderingModeForCompatibleBuffer(), renderingMethod);
}

RefPtr<ImageBuffer> GraphicsContext::createAlignedImageBuffer(const FloatRect& rect, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    return createScaledImageBuffer(rect, scaleFactor(), colorSpace, renderingModeForCompatibleBuffer(), renderingMethod);
}

void GraphicsContext::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    systemImage.draw(*this, destinationRect);
}

ImageDrawResult GraphicsContext::drawImage(Image& image, const FloatPoint& destination, ImagePaintingOptions imagePaintingOptions)
{
    return drawImage(image, FloatRect(destination, image.size()), FloatRect(FloatPoint(), image.size()), imagePaintingOptions);
}

ImageDrawResult GraphicsContext::drawImage(Image& image, const FloatRect& destination, ImagePaintingOptions imagePaintingOptions)
{
    FloatRect srcRect(FloatPoint(), image.size(imagePaintingOptions.orientation()));
    return drawImage(image, destination, srcRect, imagePaintingOptions);
}

ImageDrawResult GraphicsContext::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    return image.draw(*this, destination, source, options);
}

ImageDrawResult GraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    return image.drawTiled(*this, destination, source, tileSize, spacing, options);
}

ImageDrawResult GraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor,
    Image::TileRule hRule, Image::TileRule vRule, ImagePaintingOptions options)
{
    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        return drawImage(image, destination, source, options);
    }

    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    return image.drawTiled(*this, destination, source, tileScaleFactor, hRule, vRule, { options.compositeOperator() });
}

RefPtr<NativeImage> GraphicsContext::nativeImageForDrawing(ImageBuffer& imageBuffer)
{
    if (m_isDeferred == IsDeferred::Yes || &imageBuffer.context() == this)
        return imageBuffer.copyNativeImage();
    return imageBuffer.createNativeImageReference();
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatPoint& destination, ImagePaintingOptions imagePaintingOptions)
{
    drawImageBuffer(image, FloatRect(destination, image.logicalSize()), FloatRect({ }, image.logicalSize()), imagePaintingOptions);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatRect& destination, ImagePaintingOptions imagePaintingOptions)
{
    drawImageBuffer(image, destination, FloatRect({ }, image.logicalSize()), imagePaintingOptions);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    FloatRect sourceScaled = source;
    sourceScaled.scale(image.resolutionScale());
    if (auto nativeImage = nativeImageForDrawing(image))
        drawNativeImage(*nativeImage, destination, sourceScaled, options);
}

void GraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer> image, const FloatPoint& destination, ImagePaintingOptions imagePaintingOptions)
{
    if (!image)
        return;
    auto imageLogicalSize = image->logicalSize();
    drawConsumingImageBuffer(WTFMove(image), FloatRect(destination, imageLogicalSize), FloatRect({ }, imageLogicalSize), imagePaintingOptions);
}

void GraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer> image, const FloatRect& destination, ImagePaintingOptions imagePaintingOptions)
{
    if (!image)
        return;
    auto imageLogicalSize = image->logicalSize();
    drawConsumingImageBuffer(WTFMove(image), destination, FloatRect({ }, imageLogicalSize), imagePaintingOptions);
}

void GraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer> image, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options)
{
    if (!image)
        return;
    ASSERT(this != &image->context());
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    FloatRect scaledSource = source;
    scaledSource.scale(image->resolutionScale());
    if (auto nativeImage = ImageBuffer::sinkIntoNativeImage(WTFMove(image)))
        drawNativeImage(*nativeImage, destination, scaledSource, options);
}

void GraphicsContext::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    auto result = filter.apply(sourceImage, sourceImageRect, results);
    if (!result)
        return;
    
    RefPtr imageBuffer = result->imageBuffer();
    if (!imageBuffer)
        return;

    scale({ 1 / filter.filterScale().width(), 1 / filter.filterScale().height() });
    drawImageBuffer(*imageBuffer, result->absoluteImageRect());
    scale(filter.filterScale());
}

void GraphicsContext::drawPattern(ImageBuffer& image, const FloatRect& destRect, const FloatRect& source, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    FloatRect scaledSource = source;
    scaledSource.scale(image.resolutionScale());
    if (auto nativeImage = nativeImageForDrawing(image))
        drawPattern(*nativeImage, destRect, source, patternTransform, phase, spacing, options);
}

void GraphicsContext::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    part.draw(*this, borderRect, deviceScaleFactor, style);
}

#if ENABLE(VIDEO)
void GraphicsContext::drawVideoFrame(const VideoFrame& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    RefPtr image = frame.copyNativeImage();
    if (!image)
        return;
    IntSize size = image->size();
    if (orientation.usesWidthAsHeight())
        size = size.transposedSize();
    auto compositeOperator = !shouldDiscardAlpha && image->hasAlpha() ? CompositeOperator::SourceOver : CompositeOperator::Copy;
    drawNativeImage(*image, destination, { { }, size }, { compositeOperator, orientation });
}
#endif

void GraphicsContext::clipRoundedRect(const FloatRoundedRect& rect)
{
    Path path;
    path.addRoundedRect(rect);
    clipPath(path);
}

void GraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    if (!rect.isRounded()) {
        clipOut(rect.rect());
        return;
    }

    Path path;
    path.addRoundedRect(rect);
    clipOut(path);
}

IntRect GraphicsContext::clipBounds() const
{
    ASSERT_NOT_REACHED();
    return IntRect();
}

void GraphicsContext::fillRect(const FloatRect& rect, Gradient& gradient)
{
    gradient.fill(*this, rect);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    CompositeOperator previousOperator = compositeOperation();
    setCompositeOperation(op, blendMode);
    fillRect(rect, color);
    setCompositeOperation(previousOperator);
}

void GraphicsContext::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    if (rect.isRounded()) {
        setCompositeOperation(compositeOperation(), blendMode);
        fillRoundedRectImpl(rect, color);
        setCompositeOperation(compositeOperation());
    } else
        fillRect(rect.rect(), color, compositeOperation(), blendMode);
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();
    
    setFillRule(WindRule::EvenOdd);
    setFillColor(color);

    fillPath(path);
    
    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

FloatSize GraphicsContext::scaleFactor() const
{
    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    return FloatSize(transform.xScale(), transform.yScale());
}

FloatSize GraphicsContext::scaleFactorForDrawing(const FloatRect& destRect, const FloatRect& srcRect) const
{
    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    auto transformedDestRect = transform.mapRect(destRect);
    return transformedDestRect.size() / srcRect.size();
}

void GraphicsContext::drawPath(const Path& path)
{
    fillPath(path);
    strokePath(path);
}

void GraphicsContext::fillEllipseAsPath(const FloatRect& ellipse)
{
    Path path;
    path.addEllipseInRect(ellipse);
    fillPath(path);
}

void GraphicsContext::strokeEllipseAsPath(const FloatRect& ellipse)
{
    Path path;
    path.addEllipseInRect(ellipse);
    strokePath(path);
}

void GraphicsContext::drawLineForText(const FloatRect& rect, bool isPrinting, bool doubleUnderlines, StrokeStyle style)
{
    FloatSegment line[1] { { 0, rect.width() } };
    drawLinesForText(rect.location(), rect.height(), line, isPrinting, doubleUnderlines, style);
}

void GraphicsContext::drawDisplayList(const DisplayList::DisplayList& displayList)
{
    Ref controlFactory = ControlFactory::singleton();
    drawDisplayList(displayList, controlFactory);
}

void GraphicsContext::drawDisplayList(const DisplayList::DisplayList& displayList, ControlFactory& controlFactory)
{
    // FIXME: ControlFactory should be property of the context and not passed this way here.
    // Currently this mutates each ControlPart which is unsuitable for display lists.
    for (auto& item : displayList.items())
        applyItem(*this, controlFactory, item);
}

FloatRect GraphicsContext::computeUnderlineBoundsForText(const FloatRect& rect, bool printing)
{
    Color dummyColor;
    return computeLineBoundsAndAntialiasingModeForText(rect, printing, dummyColor);
}

FloatRect GraphicsContext::computeLineBoundsAndAntialiasingModeForText(const FloatRect& rect, bool printing, Color& color)
{
    FloatPoint origin = rect.location();
    float thickness = std::max(rect.height(), 0.5f);
    if (printing)
        return FloatRect(origin, FloatSize(rect.width(), thickness));

    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    // Just compute scale in x dimension, assuming x and y scales are equal.
    float scale = transform.b() ? std::hypot(transform.a(), transform.b()) : transform.a();
    if (scale < 1.0) {
        // This code always draws a line that is at least one-pixel line high,
        // which tends to visually overwhelm text at small scales. To counter this
        // effect, an alpha is applied to the underline color when text is at small scales.
        static const float minimumUnderlineAlpha = 0.4f;
        float shade = scale > minimumUnderlineAlpha ? scale : minimumUnderlineAlpha;
        color = color.colorWithAlphaMultipliedBy(shade);
    }

    FloatPoint devicePoint = transform.mapPoint(rect.location());
    // Visual overflow might occur here due to integral roundf/ceilf. visualOverflowForDecorations adjusts the overflow value for underline decoration.
    FloatPoint deviceOrigin = FloatPoint(roundf(devicePoint.x()), ceilf(devicePoint.y()));
    if (auto inverse = transform.inverse())
        origin = inverse.value().mapPoint(deviceOrigin);
    return FloatRect(origin, FloatSize(rect.width(), thickness));
}

float GraphicsContext::dashedLineCornerWidthForStrokeWidth(float strokeWidth) const
{
    float thickness = strokeThickness();
    return strokeStyle() == StrokeStyle::DottedStroke ? thickness : std::min(2.0f * thickness, std::max(thickness, strokeWidth / 3.0f));
}

float GraphicsContext::dashedLinePatternWidthForStrokeWidth(float strokeWidth) const
{
    float thickness = strokeThickness();
    return strokeStyle() == StrokeStyle::DottedStroke ? thickness : std::min(3.0f * thickness, std::max(thickness, strokeWidth / 3.0f));
}

float GraphicsContext::dashedLinePatternOffsetForPatternAndStrokeWidth(float patternWidth, float strokeWidth) const
{
    // Pattern starts with full fill and ends with the empty fill.
    // 1. Let's start with the empty phase after the corner.
    // 2. Check if we've got odd or even number of patterns and whether they fully cover the line.
    // 3. In case of even number of patterns and/or remainder, move the pattern start position
    // so that the pattern is balanced between the corners.
    float patternOffset = patternWidth;
    int numberOfSegments = std::floor(strokeWidth / patternWidth);
    bool oddNumberOfSegments = numberOfSegments % 2;
    float remainder = strokeWidth - (numberOfSegments * patternWidth);
    if (oddNumberOfSegments && remainder)
        patternOffset -= remainder / 2.0f;
    else if (!oddNumberOfSegments) {
        if (remainder)
            patternOffset += patternOffset - (patternWidth + remainder) / 2.0f;
        else
            patternOffset += patternWidth / 2.0f;
    }

    return patternOffset;
}

Vector<FloatPoint> GraphicsContext::centerLineAndCutOffCorners(bool isVerticalLine, float cornerWidth, FloatPoint point1, FloatPoint point2) const
{
    // Center line and cut off corners for pattern painting.
    if (isVerticalLine) {
        float centerOffset = (point2.x() - point1.x()) / 2.0f;
        point1.move(centerOffset, cornerWidth);
        point2.move(-centerOffset, -cornerWidth);
    } else {
        float centerOffset = (point2.y() - point1.y()) / 2.0f;
        point1.move(cornerWidth, centerOffset);
        point2.move(-cornerWidth, -centerOffset);
    }

    return { point1, point2 };
}

auto GraphicsContext::computeRectsAndStrokeColorForLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool isPrinting, bool doubleLines, StrokeStyle strokeStyle) -> RectsAndStrokeColor
{
#if USE(CG)
    auto makeRect = [](float x, float y, float width, float height) -> CGRect {
        return { { x, y }, { width, height } };
    };
#else
    auto makeRect = [](float x, float y, float width, float height) -> FloatRect {
        return { x, y, width, height };
    };
#endif

    RectsAndStrokeColor result;
    auto& rects = result.rects;
    auto& strokeColor = result.strokeColor;
    if (lineSegments.empty())
        return result;
    strokeColor = this->strokeColor();
    FloatRect bounds = computeLineBoundsAndAntialiasingModeForText(FloatRect { point, FloatSize { lineSegments.back().end, thickness } }, isPrinting, strokeColor);
    if (bounds.isEmpty())
        return result;

    rects.reserveInitialCapacity((doubleLines ? 2 : 1) * lineSegments.size());

    float dashWidth = 0;
    switch (strokeStyle) {
    case StrokeStyle::DottedStroke:
        dashWidth = bounds.height();
        break;
    case StrokeStyle::DashedStroke:
        dashWidth = 2 * bounds.height();
        break;
    case StrokeStyle::SolidStroke:
    default:
        break;
    }

    if (dashWidth) {
        for (const auto& lineSegment : lineSegments) {
            auto left = lineSegment.begin;
            auto width = lineSegment.length();
            auto doubleWidth = 2 * dashWidth;
            auto quotient = static_cast<int>(left / doubleWidth);
            auto startOffset = left - quotient * doubleWidth;
            auto effectiveLeft = left + startOffset;
            auto startParticle = static_cast<int>(std::floor(effectiveLeft / doubleWidth));
            auto endParticle = static_cast<int>(std::ceil((left + width) / doubleWidth));

            for (auto j = startParticle; j < endParticle; ++j) {
                auto actualDashWidth = dashWidth;
                auto dashStart = bounds.x() + j * doubleWidth;

                if (j == startParticle && startOffset > 0 && startOffset < dashWidth) {
                    actualDashWidth -= startOffset;
                    dashStart += startOffset;
                }

                if (j == endParticle - 1) {
                    auto remainingWidth = left + width - (j * doubleWidth);
                    if (remainingWidth < dashWidth)
                        actualDashWidth = remainingWidth;
                }

                rects.append(makeRect(dashStart, bounds.y(), actualDashWidth, bounds.height()));
            }
        }
    } else {
        for (const auto& lineSegment : lineSegments)
            rects.append(makeRect(bounds.x() + lineSegment.begin, bounds.y(), lineSegment.length(), bounds.height()));
    }
    if (doubleLines) {
        // The space between double underlines is equal to the height of the underline.
        float y = bounds.y() + 2 * bounds.height();
        for (const auto& lineSegment : lineSegments)
            rects.append(makeRect(bounds.x() + lineSegment.begin, y, lineSegment.length(), bounds.height()));
    }
    return result;
}

} // namespace WebCore
