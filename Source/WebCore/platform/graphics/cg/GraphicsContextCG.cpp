/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "GraphicsContextCG.h"

#if USE(CG)

#include "AffineTransform.h"
#include "DisplayListRecorder.h"
#include "FloatConversion.h"
#include "Gradient.h"
#include "GraphicsContextPlatformPrivateCG.h"
#include "ImageBuffer.h"
#include "ImageOrientation.h"
#include "Logging.h"
#include "Path.h"
#include "Pattern.h"
#include "ShadowBlur.h"
#include "SubimageCacheWithTimer.h"
#include "Timer.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static void setCGFillColor(CGContextRef context, const Color& color)
{
    CGContextSetFillColorWithColor(context, cachedCGColor(color).get());
}

inline CGAffineTransform getUserToBaseCTM(CGContextRef context)
{
    return CGAffineTransformConcat(CGContextGetCTM(context), CGAffineTransformInvert(CGContextGetBaseCTM(context)));
}

static InterpolationQuality coreInterpolationQuality(CGContextRef context)
{
    switch (CGContextGetInterpolationQuality(context)) {
    case kCGInterpolationDefault:
        return InterpolationQuality::Default;
    case kCGInterpolationNone:
        return InterpolationQuality::DoNotInterpolate;
    case kCGInterpolationLow:
        return InterpolationQuality::Low;
    case kCGInterpolationMedium:
        return InterpolationQuality::Medium;
    case kCGInterpolationHigh:
        return InterpolationQuality::High;
    }
    return InterpolationQuality::Default;
}

static CGInterpolationQuality cgInterpolationQuality(InterpolationQuality quality)
{
    switch (quality) {
    case InterpolationQuality::Default:
        return kCGInterpolationDefault;
    case InterpolationQuality::DoNotInterpolate:
        return kCGInterpolationNone;
    case InterpolationQuality::Low:
        return kCGInterpolationLow;
    case InterpolationQuality::Medium:
        return kCGInterpolationMedium;
    case InterpolationQuality::High:
        return kCGInterpolationHigh;
    }
    return kCGInterpolationDefault;
}

static CGTextDrawingMode cgTextDrawingMode(TextDrawingModeFlags mode)
{
    bool fill = mode.contains(TextDrawingMode::Fill);
    bool stroke = mode.contains(TextDrawingMode::Stroke);
    if (fill && stroke)
        return kCGTextFillStroke;
    if (fill)
        return kCGTextFill;
    return kCGTextStroke;
}

static CGBlendMode selectCGBlendMode(CompositeOperator compositeOperator, BlendMode blendMode)
{
    switch (blendMode) {
    case BlendMode::Normal:
        switch (compositeOperator) {
        case CompositeOperator::Clear:
            return kCGBlendModeClear;
        case CompositeOperator::Copy:
            return kCGBlendModeCopy;
        case CompositeOperator::SourceOver:
            return kCGBlendModeNormal;
        case CompositeOperator::SourceIn:
            return kCGBlendModeSourceIn;
        case CompositeOperator::SourceOut:
            return kCGBlendModeSourceOut;
        case CompositeOperator::SourceAtop:
            return kCGBlendModeSourceAtop;
        case CompositeOperator::DestinationOver:
            return kCGBlendModeDestinationOver;
        case CompositeOperator::DestinationIn:
            return kCGBlendModeDestinationIn;
        case CompositeOperator::DestinationOut:
            return kCGBlendModeDestinationOut;
        case CompositeOperator::DestinationAtop:
            return kCGBlendModeDestinationAtop;
        case CompositeOperator::XOR:
            return kCGBlendModeXOR;
        case CompositeOperator::PlusDarker:
            return kCGBlendModePlusDarker;
        case CompositeOperator::PlusLighter:
            return kCGBlendModePlusLighter;
        case CompositeOperator::Difference:
            return kCGBlendModeDifference;
        }
        break;
    case BlendMode::Multiply:
        return kCGBlendModeMultiply;
    case BlendMode::Screen:
        return kCGBlendModeScreen;
    case BlendMode::Overlay:
        return kCGBlendModeOverlay;
    case BlendMode::Darken:
        return kCGBlendModeDarken;
    case BlendMode::Lighten:
        return kCGBlendModeLighten;
    case BlendMode::ColorDodge:
        return kCGBlendModeColorDodge;
    case BlendMode::ColorBurn:
        return kCGBlendModeColorBurn;
    case BlendMode::HardLight:
        return kCGBlendModeHardLight;
    case BlendMode::SoftLight:
        return kCGBlendModeSoftLight;
    case BlendMode::Difference:
        return kCGBlendModeDifference;
    case BlendMode::Exclusion:
        return kCGBlendModeExclusion;
    case BlendMode::Hue:
        return kCGBlendModeHue;
    case BlendMode::Saturation:
        return kCGBlendModeSaturation;
    case BlendMode::Color:
        return kCGBlendModeColor;
    case BlendMode::Luminosity:
        return kCGBlendModeLuminosity;
    case BlendMode::PlusDarker:
        return kCGBlendModePlusDarker;
    case BlendMode::PlusLighter:
        return kCGBlendModePlusLighter;
    }

    return kCGBlendModeNormal;
}

static void setCGBlendMode(CGContextRef context, CompositeOperator op, BlendMode blendMode)
{
    CGContextSetBlendMode(context, selectCGBlendMode(op, blendMode));
}

GraphicsContextCG::GraphicsContextCG(CGContextRef cgContext)
    : GraphicsContext(GraphicsContextState::basicChangeFlags, coreInterpolationQuality(cgContext))
{
    if (!cgContext)
        return;

    m_data = new GraphicsContextPlatformPrivate(cgContext);
    // Make sure the context starts in sync with our state.
    didUpdateState(m_state);
}

GraphicsContextCG::~GraphicsContextCG()
{
    delete m_data;
}

bool GraphicsContextCG::hasPlatformContext() const
{
    return true;
}

CGContextRef GraphicsContextCG::platformContext() const
{
    ASSERT(m_data->m_cgContext);
    return m_data->m_cgContext.get();
}

void GraphicsContextCG::save()
{
    GraphicsContext::save();

    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // save of the secondary context (in GraphicsContextPlatformPrivateCG.h).
    CGContextSaveGState(platformContext());
    m_data->save();
}

void GraphicsContextCG::restore()
{
    if (!stackSize())
        return;

    GraphicsContext::restore();

    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // restore of the secondary context (in GraphicsContextPlatformPrivateCG.h).
    CGContextRestoreGState(platformContext());
    m_data->restore();
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::drawNativeImage(NativeImage& nativeImage, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    auto image = nativeImage.platformImage();
    auto imageRect = FloatRect { { }, imageSize };
    auto normalizedSrcRect = normalizeRect(srcRect);
    auto normalizedDestRect = normalizeRect(destRect);

    if (!image || !imageRect.intersects(normalizedSrcRect))
        return;

#if !LOG_DISABLED
    MonotonicTime startTime = MonotonicTime::now();
#endif

    auto shouldUseSubimage = [](CGInterpolationQuality interpolationQuality, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& transform) -> bool {
        if (interpolationQuality == kCGInterpolationNone)
            return false;
        if (transform.isRotateOrShear())
            return true;
        auto xScale = destRect.width() * transform.xScale() / srcRect.width();
        auto yScale = destRect.height() * transform.yScale() / srcRect.height();
        return !WTF::areEssentiallyEqual(xScale, yScale) || xScale > 1;
    };

    auto getSubimage = [](CGImageRef image, const FloatSize& imageSize, const FloatRect& subimageRect, const ImagePaintingOptions& options) -> RetainPtr<CGImageRef> {
        auto physicalSubimageRect = subimageRect;

        if (options.orientation() != ImageOrientation::None) {
            // subimageRect is in logical coordinates. getSubimage() deals with none-oriented
            // image. We need to convert subimageRect to physical image coordinates.
            if (auto transform = options.orientation().transformFromDefault(imageSize).inverse())
                physicalSubimageRect = transform.value().mapRect(physicalSubimageRect);
        }

#if CACHE_SUBIMAGES
        return SubimageCacheWithTimer::getSubimage(image, physicalSubimageRect);
#else
        return adoptCF(CGImageCreateWithImageInRect(image, physicalSubimageRect));
#endif
    };

    auto imageLogicalSize = [](CGImageRef image, const ImagePaintingOptions& options) -> FloatSize {
        FloatSize size = FloatSize(CGImageGetWidth(image), CGImageGetHeight(image));
        return options.orientation().usesWidthAsHeight() ? size.transposedSize() : size;
    };
    
    auto context = platformContext();
    CGContextStateSaver stateSaver(context, false);
    auto transform = CGContextGetCTM(context);

    convertToDestinationColorSpaceIfNeeded(image);

    auto subImage = image;
    auto currentImageSize = imageLogicalSize(image.get(), options);

    auto adjustedDestRect = normalizedDestRect;

    if (normalizedSrcRect != imageRect) {
        CGInterpolationQuality interpolationQuality = CGContextGetInterpolationQuality(context);
        auto scale = normalizedDestRect.size() / normalizedSrcRect.size();

        if (shouldUseSubimage(interpolationQuality, normalizedDestRect, normalizedSrcRect, transform)) {
            auto subimageRect = enclosingIntRect(normalizedSrcRect);

            // When the image is scaled using high-quality interpolation, we create a temporary CGImage
            // containing only the portion we want to display. We need to do this because high-quality
            // interpolation smoothes sharp edges, causing pixels from outside the source rect to bleed
            // into the destination rect. See <rdar://problem/6112909>.
            subImage = getSubimage(subImage.get(), imageSize, subimageRect, options);

            auto subPixelPadding = normalizedSrcRect.location() - subimageRect.location();
            adjustedDestRect = { adjustedDestRect.location() - subPixelPadding * scale, subimageRect.size() * scale };

            // If the image is only partially loaded, then shrink the destination rect that we're drawing
            // into accordingly.
            if (currentImageSize.height() < normalizedSrcRect.maxY()) {
                auto currentSubimageSize = imageLogicalSize(subImage.get(), options);
                adjustedDestRect.setHeight(currentSubimageSize.height() * scale.height());
            }
        } else {
            // If the source rect is a subportion of the image, then we compute an inflated destination rect
            // that will hold the entire image and then set a clip to the portion that we want to display.
            adjustedDestRect = { adjustedDestRect.location() - toFloatSize(normalizedSrcRect.location()) * scale, imageSize * scale };
        }

        if (!normalizedDestRect.contains(adjustedDestRect)) {
            stateSaver.save();
            CGContextClipToRect(context, normalizedDestRect);
        }
    }

    // If the image is only partially loaded, then shrink the destination rect that we're drawing into accordingly.
    if (subImage == image && currentImageSize.height() < imageSize.height())
        adjustedDestRect.setHeight(adjustedDestRect.height() * currentImageSize.height() / imageSize.height());

#if PLATFORM(IOS_FAMILY)
    bool wasAntialiased = CGContextGetShouldAntialias(context);
    // Anti-aliasing is on by default on the iPhone. Need to turn it off when drawing images.
    CGContextSetShouldAntialias(context, false);

    // Align to pixel boundaries
    adjustedDestRect = roundToDevicePixels(adjustedDestRect);
#endif

    auto oldCompositeOperator = compositeOperation();
    auto oldBlendMode = blendMode();
    setCGBlendMode(context, options.compositeOperator(), options.blendMode());

    // Make the origin be at adjustedDestRect.location()
    CGContextTranslateCTM(context, adjustedDestRect.x(), adjustedDestRect.y());
    adjustedDestRect.setLocation(FloatPoint::zero());

    if (options.orientation() != ImageOrientation::None) {
        CGContextConcatCTM(context, options.orientation().transformFromDefault(adjustedDestRect.size()));

        // The destination rect will have its width and height already reversed for the orientation of
        // the image, as it was needed for page layout, so we need to reverse it back here.
        if (options.orientation().usesWidthAsHeight())
            adjustedDestRect = adjustedDestRect.transposedRect();
    }
    
    // Flip the coords.
    CGContextTranslateCTM(context, 0, adjustedDestRect.height());
    CGContextScaleCTM(context, 1, -1);

    // Draw the image.
    CGContextDrawImage(context, adjustedDestRect, subImage.get());

    if (!stateSaver.didSave()) {
        CGContextSetCTM(context, transform);
#if PLATFORM(IOS_FAMILY)
        CGContextSetShouldAntialias(context, wasAntialiased);
#endif
        setCGBlendMode(context, oldCompositeOperator, oldBlendMode);
    }

    LOG_WITH_STREAM(Images, stream << "GraphicsContextCG::drawNativeImage " << image.get() << " size " << imageSize << " into " << destRect << " took " << (MonotonicTime::now() - startTime).milliseconds() << "ms");
}

bool GraphicsContextCG::needsCachedNativeImageInvalidationWorkaround(RenderingMode imageRenderingMode)
{
    // Only accelerated images cache CGImageRefs underneath us (when returned by
    // IOSurface::createImage), and thus need the workaround.
    if (imageRenderingMode == RenderingMode::Unaccelerated)
        return false;

    // Accelerated destinations have "live" CGImageRefs, so we only need
    // the workaround when painting into an unaccelerated context.
    return renderingMode() == RenderingMode::Unaccelerated;
}

static void drawPatternCallback(void* info, CGContextRef context)
{
    CGImageRef image = (CGImageRef)info;
    CGFloat height = CGImageGetHeight(image);
    CGContextDrawImage(context, GraphicsContextCG(context).roundToDevicePixels(FloatRect(0, 0, CGImageGetWidth(image), height)), image);
}

static void patternReleaseCallback(void* info)
{
    callOnMainThread([image = adoptCF(static_cast<CGImageRef>(info))] { });
}

void GraphicsContextCG::drawPattern(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    if (!patternTransform.isInvertible())
        return;

    auto image = nativeImage.platformImage();
    auto imageSize = nativeImage.size();

    CGContextRef context = platformContext();
    CGContextStateSaver stateSaver(context);
    CGContextClipToRect(context, destRect);

    setCGBlendMode(context, options.compositeOperator(), options.blendMode());

    CGContextTranslateCTM(context, destRect.x(), destRect.y() + destRect.height());
    CGContextScaleCTM(context, 1, -1);
    
    // Compute the scaled tile size.
    float scaledTileHeight = tileRect.height() * narrowPrecisionToFloat(patternTransform.d());
    
    // We have to adjust the phase to deal with the fact we're in Cartesian space now (with the bottom left corner of destRect being
    // the origin).
    float adjustedX = phase.x() - destRect.x() + tileRect.x() * narrowPrecisionToFloat(patternTransform.a()); // We translated the context so that destRect.x() is the origin, so subtract it out.
    float adjustedY = destRect.height() - (phase.y() - destRect.y() + tileRect.y() * narrowPrecisionToFloat(patternTransform.d()) + scaledTileHeight);

    float h = CGImageGetHeight(image.get());

    RetainPtr<CGImageRef> subImage;
    if (tileRect.size() == imageSize)
        subImage = image;
    else {
        // Copying a sub-image out of a partially-decoded image stops the decoding of the original image. It should never happen
        // because sub-images are only used for border-image, which only renders when the image is fully decoded.
        ASSERT(h == imageSize.height());
        subImage = adoptCF(CGImageCreateWithImageInRect(image.get(), tileRect));
    }

    // If we need to paint gaps between tiles because we have a partially loaded image or non-zero spacing,
    // fall back to the less efficient CGPattern-based mechanism.
    float scaledTileWidth = tileRect.width() * narrowPrecisionToFloat(patternTransform.a());
    float w = CGImageGetWidth(image.get());
    if (w == imageSize.width() && h == imageSize.height() && !spacing.width() && !spacing.height()) {
        // FIXME: CG seems to snap the images to integral sizes. When we care (e.g. with border-image-repeat: round),
        // we should tile all but the last, and stretch the last image to fit.
        CGContextDrawTiledImage(context, FloatRect(adjustedX, adjustedY, scaledTileWidth, scaledTileHeight), subImage.get());
    } else {
        static const CGPatternCallbacks patternCallbacks = { 0, drawPatternCallback, patternReleaseCallback };
        CGAffineTransform matrix = CGAffineTransformMake(narrowPrecisionToCGFloat(patternTransform.a()), 0, 0, narrowPrecisionToCGFloat(patternTransform.d()), adjustedX, adjustedY);
        matrix = CGAffineTransformConcat(matrix, CGContextGetCTM(context));
        // The top of a partially-decoded image is drawn at the bottom of the tile. Map it to the top.
        matrix = CGAffineTransformTranslate(matrix, 0, imageSize.height() - h);
        CGImageRef platformImage = CGImageRetain(subImage.get());
        RetainPtr<CGPatternRef> pattern = adoptCF(CGPatternCreate(platformImage, CGRectMake(0, 0, tileRect.width(), tileRect.height()), matrix,
            tileRect.width() + spacing.width() * (1 / narrowPrecisionToFloat(patternTransform.a())),
            tileRect.height() + spacing.height() * (1 / narrowPrecisionToFloat(patternTransform.d())),
            kCGPatternTilingConstantSpacing, true, &patternCallbacks));
        
        if (!pattern)
            return;

        RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));

        CGFloat alpha = 1;
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreateWithPattern(patternSpace.get(), pattern.get(), &alpha));
        CGContextSetFillColorSpace(context, patternSpace.get());

        CGContextSetBaseCTM(context, CGAffineTransformIdentity);
        CGContextSetPatternPhase(context, CGSizeZero);

        CGContextSetFillColorWithColor(context, color.get());
        CGContextFillRect(context, CGContextGetClipBoundingBox(context)); // FIXME: we know the clip; we set it above.
    }
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextCG::drawRect(const FloatRect& rect, float borderThickness)
{
    // FIXME: this function does not handle patterns and gradients like drawPath does, it probably should.
    ASSERT(!rect.isEmpty());

    CGContextRef context = platformContext();

    CGContextFillRect(context, rect);

    if (strokeStyle() != StrokeStyle::NoStroke) {
        // We do a fill of four rects to simulate the stroke of a border.
        Color oldFillColor = fillColor();
        if (oldFillColor != strokeColor())
            setCGFillColor(context, strokeColor());
        CGRect rects[4] = {
            FloatRect(rect.x(), rect.y(), rect.width(), borderThickness),
            FloatRect(rect.x(), rect.maxY() - borderThickness, rect.width(), borderThickness),
            FloatRect(rect.x(), rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness),
            FloatRect(rect.maxX() - borderThickness, rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness)
        };
        CGContextFillRects(context, rects, 4);
        if (oldFillColor != strokeColor())
            setCGFillColor(context, oldFillColor);
    }
}

// This is only used to draw borders.
void GraphicsContextCG::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == StrokeStyle::NoStroke)
        return;

    float thickness = strokeThickness();
    bool isVerticalLine = (point1.x() + thickness == point2.x());
    float strokeWidth = isVerticalLine ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!thickness || !strokeWidth)
        return;

    CGContextRef context = platformContext();

    StrokeStyle strokeStyle = this->strokeStyle();
    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == StrokeStyle::DottedStroke || strokeStyle == StrokeStyle::DashedStroke;

    CGContextStateSaver stateSaver(context, drawsDashedLine);
    if (drawsDashedLine) {
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth);
        setCGFillColor(context, strokeColor());
        if (isVerticalLine) {
            CGContextFillRect(context, FloatRect(point1.x(), point1.y(), thickness, cornerWidth));
            CGContextFillRect(context, FloatRect(point1.x(), point2.y() - cornerWidth, thickness, cornerWidth));
        } else {
            CGContextFillRect(context, FloatRect(point1.x(), point1.y(), cornerWidth, thickness));
            CGContextFillRect(context, FloatRect(point2.x() - cornerWidth, point1.y(), cornerWidth, thickness));
        }
        strokeWidth -= 2 * cornerWidth;
        float patternWidth = dashedLinePatternWidthForStrokeWidth(strokeWidth);
        // Check if corner drawing sufficiently covers the line.
        if (strokeWidth <= patternWidth + 1)
            return;

        float patternOffset = dashedLinePatternOffsetForPatternAndStrokeWidth(patternWidth, strokeWidth);
        const CGFloat dashedLine[2] = { static_cast<CGFloat>(patternWidth), static_cast<CGFloat>(patternWidth) };
        CGContextSetLineDash(context, patternOffset, dashedLine, 2);
    }

    auto centeredPoints = centerLineAndCutOffCorners(isVerticalLine, cornerWidth, point1, point2);
    auto p1 = centeredPoints[0];
    auto p2 = centeredPoints[1];

    if (shouldAntialias()) {
#if PLATFORM(IOS_FAMILY)
        // Force antialiasing on for line patterns as they don't look good with it turned off (<rdar://problem/5459772>).
        CGContextSetShouldAntialias(context, strokeStyle == StrokeStyle::DottedStroke || strokeStyle == StrokeStyle::DashedStroke);
#else
        CGContextSetShouldAntialias(context, false);
#endif
    }
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, p1.x(), p1.y());
    CGContextAddLineToPoint(context, p2.x(), p2.y());
    CGContextStrokePath(context);
    if (shouldAntialias())
        CGContextSetShouldAntialias(context, true);
}

void GraphicsContextCG::drawEllipse(const FloatRect& rect)
{
    Path path;
    path.addEllipse(rect);
    drawPath(path);
}

void GraphicsContextCG::applyStrokePattern()
{
    if (!strokePattern())
        return;

    CGContextRef cgContext = platformContext();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    auto platformPattern = strokePattern()->createPlatformPattern(userToBaseCTM);
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(0));
    CGContextSetStrokeColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(cgContext, platformPattern.get(), &patternAlpha);
}

void GraphicsContextCG::applyFillPattern()
{
    if (!fillPattern())
        return;

    CGContextRef cgContext = platformContext();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    auto platformPattern = fillPattern()->createPlatformPattern(userToBaseCTM);
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));
    CGContextSetFillColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(cgContext, platformPattern.get(), &patternAlpha);
}

static inline bool calculateDrawingMode(const GraphicsContext& context, CGPathDrawingMode& mode)
{
    bool shouldFill = context.fillBrush().isVisible();
    bool shouldStroke = context.strokeBrush().isVisible() || (context.strokeStyle() != StrokeStyle::NoStroke);
    bool useEOFill = context.fillRule() == WindRule::EvenOdd;

    if (shouldFill) {
        if (shouldStroke) {
            if (useEOFill)
                mode = kCGPathEOFillStroke;
            else
                mode = kCGPathFillStroke;
        } else { // fill, no stroke
            if (useEOFill)
                mode = kCGPathEOFill;
            else
                mode = kCGPathFill;
        }
    } else {
        // Setting mode to kCGPathStroke even if shouldStroke is false. In that case, we return false and mode will not be used,
        // but the compiler will not complain about an uninitialized variable.
        mode = kCGPathStroke;
    }

    return shouldFill || shouldStroke;
}

void GraphicsContextCG::drawPath(const Path& path)
{
    if (path.isEmpty())
        return;

    CGContextRef context = platformContext();

    if (fillGradient() || strokeGradient()) {
        // We don't have any optimized way to fill & stroke a path using gradients
        // FIXME: Be smarter about this.
        fillPath(path);
        strokePath(path);
        return;
    }

    if (fillPattern())
        applyFillPattern();
    if (strokePattern())
        applyStrokePattern();

    CGPathDrawingMode drawingMode;
    if (calculateDrawingMode(*this, drawingMode)) {
#if HAVE(CG_CONTEXT_DRAW_PATH_DIRECT)
        CGContextDrawPathDirect(context, drawingMode, path.platformPath(), nullptr);
#else
        CGContextBeginPath(context);
        CGContextAddPath(context, path.platformPath());
        CGContextDrawPath(context, drawingMode);
#endif
    }
}

void GraphicsContextCG::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;

    CGContextRef context = platformContext();

    if (auto fillGradient = this->fillGradient()) {
        if (hasShadow()) {
            FloatRect rect = path.fastBoundingRect();
            FloatSize layerSize = getCTM().mapSize(rect.size());

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));
            CGContextRef layerContext = CGLayerGetContext(layer.get());

            CGContextScaleCTM(layerContext, layerSize.width() / rect.width(), layerSize.height() / rect.height());
            CGContextTranslateCTM(layerContext, -rect.x(), -rect.y());
            CGContextBeginPath(layerContext);
            CGContextAddPath(layerContext, path.platformPath());
            CGContextConcatCTM(layerContext, fillGradientSpaceTransform());

            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(layerContext);
            else
                CGContextClip(layerContext);

            fillGradient->paint(layerContext);
            CGContextDrawLayerInRect(context, rect, layer.get());
        } else {
            CGContextBeginPath(context);
            CGContextAddPath(context, path.platformPath());
            CGContextStateSaver stateSaver(context);
            CGContextConcatCTM(context, fillGradientSpaceTransform());

            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(context);
            else
                CGContextClip(context);

            fillGradient->paint(*this);
        }

        return;
    }

    if (fillPattern())
        applyFillPattern();
#if HAVE(CG_CONTEXT_DRAW_PATH_DIRECT)
    CGContextDrawPathDirect(context, fillRule() == WindRule::EvenOdd ? kCGPathEOFill : kCGPathFill, path.platformPath(), nullptr);
#else
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    if (fillRule() == WindRule::EvenOdd)
        CGContextEOFillPath(context);
    else
        CGContextFillPath(context);
#endif
}

void GraphicsContextCG::strokePath(const Path& path)
{
    if (path.isEmpty())
        return;

    CGContextRef context = platformContext();

    if (auto strokeGradient = this->strokeGradient()) {
        if (hasShadow()) {
            FloatRect rect = path.fastBoundingRect();
            float lineWidth = strokeThickness();
            float doubleLineWidth = lineWidth * 2;
            float adjustedWidth = ceilf(rect.width() + doubleLineWidth);
            float adjustedHeight = ceilf(rect.height() + doubleLineWidth);

            FloatSize layerSize = getCTM().mapSize(FloatSize(adjustedWidth, adjustedHeight));

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));
            CGContextRef layerContext = CGLayerGetContext(layer.get());
            CGContextSetLineWidth(layerContext, lineWidth);

            // Compensate for the line width, otherwise the layer's top-left corner would be
            // aligned with the rect's top-left corner. This would result in leaving pixels out of
            // the layer on the left and top sides.
            float translationX = lineWidth - rect.x();
            float translationY = lineWidth - rect.y();
            CGContextScaleCTM(layerContext, layerSize.width() / adjustedWidth, layerSize.height() / adjustedHeight);
            CGContextTranslateCTM(layerContext, translationX, translationY);

            CGContextAddPath(layerContext, path.platformPath());
            CGContextReplacePathWithStrokedPath(layerContext);
            CGContextClip(layerContext);
            CGContextConcatCTM(layerContext, strokeGradientSpaceTransform());
            strokeGradient->paint(layerContext);

            float destinationX = roundf(rect.x() - lineWidth);
            float destinationY = roundf(rect.y() - lineWidth);
            CGContextDrawLayerInRect(context, CGRectMake(destinationX, destinationY, adjustedWidth, adjustedHeight), layer.get());
        } else {
            CGContextStateSaver stateSaver(context);
            CGContextBeginPath(context);
            CGContextAddPath(context, path.platformPath());
            CGContextReplacePathWithStrokedPath(context);
            CGContextClip(context);
            CGContextConcatCTM(context, strokeGradientSpaceTransform());
            strokeGradient->paint(*this);
        }
        return;
    }

    if (strokePattern())
        applyStrokePattern();

#if USE(CG_CONTEXT_STROKE_LINE_SEGMENTS_WHEN_STROKING_PATH)
    if (path.hasInlineData<LineData>()) {
        auto& lineData = path.inlineData<LineData>();
        CGPoint points[2] { lineData.start, lineData.end };
        CGContextStrokeLineSegments(context, points, 2);
        return;
    }
#endif

#if HAVE(CG_CONTEXT_DRAW_PATH_DIRECT)
    CGContextDrawPathDirect(context, kCGPathStroke, path.platformPath(), nullptr);
#else
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    CGContextStrokePath(context);
#endif
}

void GraphicsContextCG::fillRect(const FloatRect& rect)
{
    CGContextRef context = platformContext();

    if (auto fillGradient = this->fillGradient()) {
        CGContextStateSaver stateSaver(context);
        if (hasShadow()) {
            FloatSize layerSize = getCTM().mapSize(rect.size());

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));
            CGContextRef layerContext = CGLayerGetContext(layer.get());

            CGContextScaleCTM(layerContext, layerSize.width() / rect.width(), layerSize.height() / rect.height());
            CGContextTranslateCTM(layerContext, -rect.x(), -rect.y());
            CGContextAddRect(layerContext, rect);
            CGContextClip(layerContext);

            CGContextConcatCTM(layerContext, fillGradientSpaceTransform());
            fillGradient->paint(layerContext);
            CGContextDrawLayerInRect(context, rect, layer.get());
        } else {
            CGContextClipToRect(context, rect);
            CGContextConcatCTM(context, fillGradientSpaceTransform());
            fillGradient->paint(*this);
        }
        return;
    }

    if (fillPattern())
        applyFillPattern();

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        ShadowBlur contextShadow(dropShadow(), shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
}

void GraphicsContextCG::fillRect(const FloatRect& rect, const Color& color)
{
    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color);

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        ShadowBlur contextShadow(dropShadow(), shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
    
    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor);
}

void GraphicsContextCG::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color);

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        ShadowBlur contextShadow(dropShadow(), shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, rect);
    }

    const FloatRect& r = rect.rect();
    const FloatRoundedRect::Radii& radii = rect.radii();
    bool equalWidths = (radii.topLeft().width() == radii.topRight().width() && radii.topRight().width() == radii.bottomLeft().width() && radii.bottomLeft().width() == radii.bottomRight().width());
    bool equalHeights = (radii.topLeft().height() == radii.bottomLeft().height() && radii.bottomLeft().height() == radii.topRight().height() && radii.topRight().height() == radii.bottomRight().height());
    bool hasCustomFill = fillGradient() || fillPattern();
    if (!hasCustomFill && equalWidths && equalHeights && radii.topLeft().width() * 2 == r.width() && radii.topLeft().height() * 2 == r.height())
        CGContextFillEllipseInRect(context, r);
    else {
        Path path;
        path.addRoundedRect(rect);
        fillPath(path);
    }

    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor);
}

void GraphicsContextCG::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    CGContextRef context = platformContext();

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

    // fillRectWithRoundedHole() assumes that the edges of rect are clipped out, so we only care about shadows cast around inside the hole.
    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        ShadowBlur contextShadow(dropShadow(), shadowsIgnoreTransforms());
        contextShadow.drawInsetShadow(*this, rect, roundedHoleRect);
    }

    fillPath(path);

    if (drawOwnShadow)
        stateSaver.restore();
    
    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContextCG::clip(const FloatRect& rect)
{
    CGContextClipToRect(platformContext(), rect);
    m_data->clip(rect);
}

void GraphicsContextCG::clipOut(const FloatRect& rect)
{
    // FIXME: Using CGRectInfinite is much faster than getting the clip bounding box. However, due
    // to <rdar://problem/12584492>, CGRectInfinite can't be used with an accelerated context that
    // has certain transforms that aren't just a translation or a scale. And due to <rdar://problem/14634453>
    // we cannot use it in for a printing context either.
    const AffineTransform& ctm = getCTM();
    bool canUseCGRectInfinite = CGContextGetType(platformContext()) != kCGContextTypePDF && (renderingMode() == RenderingMode::Unaccelerated || (!ctm.b() && !ctm.c()));
    CGRect rects[2] = { canUseCGRectInfinite ? CGRectInfinite : CGContextGetClipBoundingBox(platformContext()), rect };
    CGContextBeginPath(platformContext());
    CGContextAddRects(platformContext(), rects, 2);
    CGContextEOClip(platformContext());
}

void GraphicsContextCG::clipOut(const Path& path)
{
    CGContextBeginPath(platformContext());
    CGContextAddRect(platformContext(), CGContextGetClipBoundingBox(platformContext()));
    if (!path.isEmpty())
        CGContextAddPath(platformContext(), path.platformPath());
    CGContextEOClip(platformContext());
}

void GraphicsContextCG::clipPath(const Path& path, WindRule clipRule)
{
    CGContextRef context = platformContext();
    if (path.isEmpty())
        CGContextClipToRect(context, CGRectZero);
    else {
        CGContextBeginPath(platformContext());
        CGContextAddPath(platformContext(), path.platformPath());

        if (clipRule == WindRule::EvenOdd)
            CGContextEOClip(context);
        else
            CGContextClip(context);
    }
    
    m_data->clip(path);
}

IntRect GraphicsContextCG::clipBounds() const
{
    return enclosingIntRect(CGContextGetClipBoundingBox(platformContext()));
}

void GraphicsContextCG::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);

    save();

    CGContextRef context = platformContext();
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
    
    m_state.didBeginTransparencyLayer();
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();

    CGContextRef context = platformContext();
    CGContextEndTransparencyLayer(context);

    restore();
}

static void applyShadowOffsetWorkaroundIfNeeded(CGContextRef context, CGFloat& xOffset, CGFloat& yOffset)
{
#if PLATFORM(IOS_FAMILY) || PLATFORM(WIN)
    UNUSED_PARAM(context);
    UNUSED_PARAM(xOffset);
    UNUSED_PARAM(yOffset);
#else
    if (CGContextDrawsWithCorrectShadowOffsets(context))
        return;

    // Work around <rdar://problem/5539388> by ensuring that the offsets will get truncated
    // to the desired integer. Also see: <rdar://problem/10056277>
    static const CGFloat extraShadowOffset = narrowPrecisionToCGFloat(1.0 / 128);
    if (xOffset > 0)
        xOffset += extraShadowOffset;
    else if (xOffset < 0)
        xOffset -= extraShadowOffset;

    if (yOffset > 0)
        yOffset += extraShadowOffset;
    else if (yOffset < 0)
        yOffset -= extraShadowOffset;
#endif
}

void GraphicsContextCG::setCGShadow(RenderingMode renderingMode, const FloatSize& offset, float blur, const Color& color, bool shadowsIgnoreTransforms)
{
    if (offset.isZero() && !blur) {
        clearCGShadow();
        return;
    }

    // FIXME: we could avoid the shadow setup cost when we know we'll render the shadow ourselves.

    CGFloat xOffset = offset.width();
    CGFloat yOffset = offset.height();
    CGFloat blurRadius = blur;
    CGContextRef context = platformContext();

    if (!shadowsIgnoreTransforms) {
        CGAffineTransform userToBaseCTM = getUserToBaseCTM(context);

        CGFloat A = userToBaseCTM.a * userToBaseCTM.a + userToBaseCTM.b * userToBaseCTM.b;
        CGFloat B = userToBaseCTM.a * userToBaseCTM.c + userToBaseCTM.b * userToBaseCTM.d;
        CGFloat C = B;
        CGFloat D = userToBaseCTM.c * userToBaseCTM.c + userToBaseCTM.d * userToBaseCTM.d;

        CGFloat smallEigenvalue = narrowPrecisionToCGFloat(sqrt(0.5 * ((A + D) - sqrt(4 * B * C + (A - D) * (A - D)))));

        blurRadius = blur * smallEigenvalue;

        CGSize offsetInBaseSpace = CGSizeApplyAffineTransform(offset, userToBaseCTM);

        xOffset = offsetInBaseSpace.width;
        yOffset = offsetInBaseSpace.height;
    }

    // Extreme "blur" values can make text drawing crash or take crazy long times, so clamp
    blurRadius = std::min(blurRadius, narrowPrecisionToCGFloat(1000.0));

    if (renderingMode != RenderingMode::Accelerated)
        applyShadowOffsetWorkaroundIfNeeded(context, xOffset, yOffset);

    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    if (!color.isValid())
        CGContextSetShadow(context, CGSizeMake(xOffset, yOffset), blurRadius);
    else
        CGContextSetShadowWithColor(context, CGSizeMake(xOffset, yOffset), blurRadius, cachedCGColor(color).get());
}

void GraphicsContextCG::clearCGShadow()
{
    CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);
}

#if PLATFORM(COCOA)
static void setCGStyle(CGContextRef context, const std::optional<GraphicsStyle>& style)
{
    if (!style) {
        CGContextSetStyle(context, nullptr);
        return;
    }

    auto cgStyle = WTF::switchOn(*style,
        [&] (const GraphicsDropShadow& dropShadow) -> RetainPtr<CGStyleRef> {
#if HAVE(CGSTYLE_CREATE_SHADOW2)
            return adoptCF(CGStyleCreateShadow2(dropShadow.offset, dropShadow.radius.width(), cachedCGColor(dropShadow.color).get()));
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(dropShadow);
            return nullptr;
#endif
        },
        [&] (const GraphicsGaussianBlur& gaussianBlur) -> RetainPtr<CGStyleRef> {
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
            ASSERT(gaussianBlur.radius.width() == gaussianBlur.radius.height());
            CGGaussianBlurStyle gaussianBlurStyle = { 1, gaussianBlur.radius.width() };
            return adoptCF(CGStyleCreateGaussianBlur(&gaussianBlurStyle));
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(gaussianBlur);
            return nullptr;
#endif
        },
        [&] (const GraphicsColorMatrix& colorMatrix) -> RetainPtr<CGStyleRef> {
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
            CGColorMatrixStyle colorMatrixStyle = { 1, { 0 } };
            for (size_t i = 0; i < colorMatrix.values.size(); ++i)
                colorMatrixStyle.matrix[i] = colorMatrix.values[i];
            return adoptCF(CGStyleCreateColorMatrix(&colorMatrixStyle));
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(colorMatrix);
            return nullptr;
#endif
        }
    );

    if (cgStyle)
        CGContextSetStyle(context, cgStyle.get());
}
#endif

void GraphicsContextCG::didUpdateState(GraphicsContextState& state)
{
    if (!state.changes())
        return;

    auto context = platformContext();

    for (auto change : state.changes()) {
        switch (change) {
        case GraphicsContextState::Change::FillBrush:
            setCGFillColor(context, state.fillBrush().color());
            break;

        case GraphicsContextState::Change::StrokeThickness:
            CGContextSetLineWidth(context, std::max(state.strokeThickness(), 0.f));
            break;

        case GraphicsContextState::Change::StrokeBrush:
            CGContextSetStrokeColorWithColor(context, cachedCGColor(state.strokeBrush().color()).get());
            break;

        case GraphicsContextState::Change::CompositeMode:
            setCGBlendMode(context, state.compositeMode().operation, state.compositeMode().blendMode);
            break;

        case GraphicsContextState::Change::DropShadow:
            setCGShadow(renderingMode(), state.dropShadow().offset, state.dropShadow().blurRadius, state.dropShadow().color, state.shadowsIgnoreTransforms());
            break;

        case GraphicsContextState::Change::Style:
#if PLATFORM(COCOA)
            setCGStyle(context, state.style());
#endif
            break;

        case GraphicsContextState::Change::Alpha:
            CGContextSetAlpha(context, state.alpha());
            break;

        case GraphicsContextState::Change::ImageInterpolationQuality:
            CGContextSetInterpolationQuality(context, cgInterpolationQuality(state.imageInterpolationQuality()));
            break;

        case GraphicsContextState::Change::TextDrawingMode:
            CGContextSetTextDrawingMode(context, cgTextDrawingMode(state.textDrawingMode()));
            break;

        case GraphicsContextState::Change::ShouldAntialias:
            CGContextSetShouldAntialias(context, state.shouldAntialias());
            break;

        case GraphicsContextState::Change::ShouldSmoothFonts:
            CGContextSetShouldSmoothFonts(context, state.shouldSmoothFonts());
            break;

        default:
            break;
        }
    }

    state.didApplyChanges();
}

void GraphicsContextCG::setMiterLimit(float limit)
{
    CGContextSetMiterLimit(platformContext(), limit);
}

void GraphicsContextCG::clearRect(const FloatRect& r)
{
    CGContextClearRect(platformContext(), r);
}

void GraphicsContextCG::strokeRect(const FloatRect& rect, float lineWidth)
{
    CGContextRef context = platformContext();

    if (auto strokeGradient = this->strokeGradient()) {
        if (hasShadow()) {
            const float doubleLineWidth = lineWidth * 2;
            float adjustedWidth = ceilf(rect.width() + doubleLineWidth);
            float adjustedHeight = ceilf(rect.height() + doubleLineWidth);
            FloatSize layerSize = getCTM().mapSize(FloatSize(adjustedWidth, adjustedHeight));

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));

            CGContextRef layerContext = CGLayerGetContext(layer.get());
            CGContextSetLineWidth(layerContext, lineWidth);

            // Compensate for the line width, otherwise the layer's top-left corner would be
            // aligned with the rect's top-left corner. This would result in leaving pixels out of
            // the layer on the left and top sides.
            const float translationX = lineWidth - rect.x();
            const float translationY = lineWidth - rect.y();
            CGContextScaleCTM(layerContext, layerSize.width() / adjustedWidth, layerSize.height() / adjustedHeight);
            CGContextTranslateCTM(layerContext, translationX, translationY);

            CGContextAddRect(layerContext, rect);
            CGContextReplacePathWithStrokedPath(layerContext);
            CGContextClip(layerContext);
            CGContextConcatCTM(layerContext, strokeGradientSpaceTransform());
            strokeGradient->paint(layerContext);

            const float destinationX = roundf(rect.x() - lineWidth);
            const float destinationY = roundf(rect.y() - lineWidth);
            CGContextDrawLayerInRect(context, CGRectMake(destinationX, destinationY, adjustedWidth, adjustedHeight), layer.get());
        } else {
            CGContextStateSaver stateSaver(context);
            setStrokeThickness(lineWidth);
            CGContextAddRect(context, rect);
            CGContextReplacePathWithStrokedPath(context);
            CGContextClip(context);
            CGContextConcatCTM(context, strokeGradientSpaceTransform());
            strokeGradient->paint(*this);
        }
        return;
    }

    if (strokePattern())
        applyStrokePattern();

    // Using CGContextAddRect and CGContextStrokePath to stroke rect rather than
    // convenience functions (CGContextStrokeRect/CGContextStrokeRectWithWidth).
    // The convenience functions currently (in at least OSX 10.9.4) fail to
    // apply some attributes of the graphics state in certain cases
    // (as identified in https://bugs.webkit.org/show_bug.cgi?id=132948)
    CGContextStateSaver stateSaver(context);
    setStrokeThickness(lineWidth);

    CGContextAddRect(context, rect);
    CGContextStrokePath(context);
}

void GraphicsContextCG::setLineCap(LineCap cap)
{
    switch (cap) {
    case LineCap::Butt:
        CGContextSetLineCap(platformContext(), kCGLineCapButt);
        break;
    case LineCap::Round:
        CGContextSetLineCap(platformContext(), kCGLineCapRound);
        break;
    case LineCap::Square:
        CGContextSetLineCap(platformContext(), kCGLineCapSquare);
        break;
    }
}

void GraphicsContextCG::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (dashOffset < 0) {
        float length = 0;
        for (size_t i = 0; i < dashes.size(); ++i)
            length += static_cast<float>(dashes[i]);
        if (length)
            dashOffset = fmod(dashOffset, length) + length;
    }
    CGContextSetLineDash(platformContext(), dashOffset, dashes.data(), dashes.size());
}

void GraphicsContextCG::setLineJoin(LineJoin join)
{
    switch (join) {
    case LineJoin::Miter:
        CGContextSetLineJoin(platformContext(), kCGLineJoinMiter);
        break;
    case LineJoin::Round:
        CGContextSetLineJoin(platformContext(), kCGLineJoinRound);
        break;
    case LineJoin::Bevel:
        CGContextSetLineJoin(platformContext(), kCGLineJoinBevel);
        break;
    }
}

void GraphicsContextCG::scale(const FloatSize& size)
{
    CGContextScaleCTM(platformContext(), size.width(), size.height());
    m_data->scale(size);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::rotate(float angle)
{
    CGContextRotateCTM(platformContext(), angle);
    m_data->rotate(angle);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::translate(float x, float y)
{
    CGContextTranslateCTM(platformContext(), x, y);
    m_data->translate(x, y);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::concatCTM(const AffineTransform& transform)
{
    CGContextConcatCTM(platformContext(), transform);
    m_data->concatCTM(transform);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::setCTM(const AffineTransform& transform)
{
    CGContextSetCTM(platformContext(), transform);
    m_data->setCTM(transform);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContextCG::getCTM(IncludeDeviceScale includeScale) const
{
    // The CTM usually includes the deviceScaleFactor except in WebKit 1 when the
    // content is non-composited, since the scale factor is integrated at a lower
    // level. To guarantee the deviceScale is included, we can use this CG API.
    if (includeScale == DefinitelyIncludeDeviceScale)
        return CGContextGetUserSpaceToDeviceSpaceTransform(platformContext());

    return CGContextGetCTM(platformContext());
}

FloatRect GraphicsContextCG::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode) const
{
    // It is not enough just to round to pixels in device space. The rotation part of the
    // affine transform matrix to device space can mess with this conversion if we have a
    // rotating image like the hands of the world clock widget. We just need the scale, so
    // we get the affine transform matrix and extract the scale.

    if (m_data->m_userToDeviceTransformKnownToBeIdentity)
        return roundedIntRect(rect);

    CGAffineTransform deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(platformContext());
    if (CGAffineTransformIsIdentity(deviceMatrix)) {
        m_data->m_userToDeviceTransformKnownToBeIdentity = true;
        return roundedIntRect(rect);
    }

    float deviceScaleX = std::hypot(deviceMatrix.a, deviceMatrix.b);
    float deviceScaleY = std::hypot(deviceMatrix.c, deviceMatrix.d);

    CGPoint deviceOrigin = CGPointMake(rect.x() * deviceScaleX, rect.y() * deviceScaleY);
    CGPoint deviceLowerRight = CGPointMake((rect.x() + rect.width()) * deviceScaleX,
        (rect.y() + rect.height()) * deviceScaleY);

    deviceOrigin.x = roundf(deviceOrigin.x);
    deviceOrigin.y = roundf(deviceOrigin.y);
    if (roundingMode == RoundAllSides) {
        deviceLowerRight.x = roundf(deviceLowerRight.x);
        deviceLowerRight.y = roundf(deviceLowerRight.y);
    } else {
        deviceLowerRight.x = deviceOrigin.x + roundf(rect.width() * deviceScaleX);
        deviceLowerRight.y = deviceOrigin.y + roundf(rect.height() * deviceScaleY);
    }

    // Don't let the height or width round to 0 unless either was originally 0
    if (deviceOrigin.y == deviceLowerRight.y && rect.height())
        deviceLowerRight.y += 1;
    if (deviceOrigin.x == deviceLowerRight.x && rect.width())
        deviceLowerRight.x += 1;

    FloatPoint roundedOrigin = FloatPoint(deviceOrigin.x / deviceScaleX, deviceOrigin.y / deviceScaleY);
    FloatPoint roundedLowerRight = FloatPoint(deviceLowerRight.x / deviceScaleX, deviceLowerRight.y / deviceScaleY);
    return FloatRect(roundedOrigin, roundedLowerRight - roundedOrigin);
}

void GraphicsContextCG::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    if (!widths.size())
        return;

    Color localStrokeColor(strokeColor());

    FloatRect bounds = computeLineBoundsAndAntialiasingModeForText(FloatRect(point, FloatSize(widths.last(), thickness)), printing, localStrokeColor);
    if (bounds.isEmpty())
        return;

    bool fillColorIsNotEqualToStrokeColor = fillColor() != localStrokeColor;
    
    Vector<CGRect, 4> dashBounds;
    ASSERT(!(widths.size() % 2));
    dashBounds.reserveInitialCapacity(dashBounds.size() / 2);

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

    for (size_t i = 0; i < widths.size(); i += 2) {
        auto left = widths[i];
        auto width = widths[i+1] - widths[i];
        if (!dashWidth)
            dashBounds.append(CGRectMake(bounds.x() + left, bounds.y(), width, bounds.height()));
        else {
            auto startParticle = static_cast<int>(std::ceil(left / (2 * dashWidth)));
            auto endParticle = static_cast<int>((left + width) / (2 * dashWidth));
            for (auto j = startParticle; j < endParticle; ++j)
                dashBounds.append(CGRectMake(bounds.x() + j * 2 * dashWidth, bounds.y(), dashWidth, bounds.height()));
        }
    }

    if (doubleLines) {
        // The space between double underlines is equal to the height of the underline
        for (size_t i = 0; i < widths.size(); i += 2)
            dashBounds.append(CGRectMake(bounds.x() + widths[i], bounds.y() + 2 * bounds.height(), widths[i+1] - widths[i], bounds.height()));
    }

    if (fillColorIsNotEqualToStrokeColor)
        setCGFillColor(platformContext(), localStrokeColor);

    CGContextFillRects(platformContext(), dashBounds.data(), dashBounds.size());

    if (fillColorIsNotEqualToStrokeColor)
        setCGFillColor(platformContext(), fillColor());
}

void GraphicsContextCG::setURLForRect(const URL& link, const FloatRect& destRect)
{
    RetainPtr<CFURLRef> urlRef = link.createCFURL();
    if (!urlRef)
        return;

    CGContextRef context = platformContext();

    FloatRect rect = destRect;
    // Get the bounding box to handle clipping.
    rect.intersect(CGContextGetClipBoundingBox(context));

    CGPDFContextSetURLForRect(context, urlRef.get(), CGRectApplyAffineTransform(rect, CGContextGetCTM(context)));
}

void GraphicsContextCG::setIsCALayerContext(bool isLayerContext)
{
    // Should be called for CA Context.
    m_data->m_contextFlags.set(GraphicsContextCGFlag::IsLayerCGContext, isLayerContext);
}

bool GraphicsContextCG::isCALayerContext() const
{
    return m_data && m_data->m_contextFlags.contains(GraphicsContextCGFlag::IsLayerCGContext);
}

void GraphicsContextCG::setIsAcceleratedContext(bool isAccelerated)
{
    // Should be called for CA Context.
    m_data->m_contextFlags.set(GraphicsContextCGFlag::IsAcceleratedCGContext, isAccelerated);
}

RenderingMode GraphicsContextCG::renderingMode() const
{
    return m_data->m_contextFlags.contains(GraphicsContextCGFlag::IsAcceleratedCGContext) ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
}

void GraphicsContextCG::applyDeviceScaleFactor(float deviceScaleFactor)
{
    GraphicsContext::applyDeviceScaleFactor(deviceScaleFactor);

    // CoreGraphics expects the base CTM of a HiDPI context to have the scale factor applied to it.
    // Failing to change the base level CTM will cause certain CG features, such as focus rings,
    // to draw with a scale factor of 1 rather than the actual scale factor.
    CGContextSetBaseCTM(platformContext(), CGAffineTransformScale(CGContextGetBaseCTM(platformContext()), deviceScaleFactor, deviceScaleFactor));
}

void GraphicsContextCG::fillEllipse(const FloatRect& ellipse)
{
    // CGContextFillEllipseInRect only supports solid colors.
    if (fillGradient() || fillPattern()) {
        fillEllipseAsPath(ellipse);
        return;
    }

    CGContextRef context = platformContext();
    CGContextFillEllipseInRect(context, ellipse);
}

void GraphicsContextCG::strokeEllipse(const FloatRect& ellipse)
{
    // CGContextStrokeEllipseInRect only supports solid colors.
    if (strokeGradient() || strokePattern()) {
        strokeEllipseAsPath(ellipse);
        return;
    }

    CGContextRef context = platformContext();
    CGContextStrokeEllipseInRect(context, ellipse);
}

bool GraphicsContextCG::supportsInternalLinks() const
{
    return true;
}

void GraphicsContextCG::setDestinationForRect(const String& name, const FloatRect& destRect)
{
    CGContextRef context = platformContext();

    FloatRect rect = destRect;
    rect.intersect(CGContextGetClipBoundingBox(context));

    CGRect transformedRect = CGRectApplyAffineTransform(rect, CGContextGetCTM(context));
    CGPDFContextSetDestinationForRect(context, name.createCFString().get(), transformedRect);
}

void GraphicsContextCG::addDestinationAtPoint(const String& name, const FloatPoint& position)
{
    CGContextRef context = platformContext();
    CGPoint transformedPoint = CGPointApplyAffineTransform(position, CGContextGetCTM(context));
    CGPDFContextAddDestinationAtPoint(context, name.createCFString().get(), transformedPoint);
}

bool GraphicsContextCG::canUseShadowBlur() const
{
    return (renderingMode() == RenderingMode::Unaccelerated) && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms();
}

}

#endif
