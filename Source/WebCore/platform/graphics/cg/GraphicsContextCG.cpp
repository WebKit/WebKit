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
#include "GraphicsContextPlatformPrivateCG.h"
#include "ImageBuffer.h"
#include "ImageOrientation.h"
#include "Logging.h"
#include "Path.h"
#include "Pattern.h"
#include "ShadowBlur.h"
#include "SubimageCacheWithTimer.h"
#include "Timer.h"
#include "URL.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(WIN)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

#define USE_DRAW_PATH_DIRECT (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400))

// FIXME: The following using declaration should be in <wtf/HashFunctions.h>.
using WTF::pairIntHash;

// FIXME: The following using declaration should be in <wtf/HashTraits.h>.
using WTF::GenericHashTraits;

namespace WebCore {

static void setCGFillColor(CGContextRef context, const Color& color)
{
    CGContextSetFillColorWithColor(context, cachedCGColor(color));
}

static void setCGStrokeColor(CGContextRef context, const Color& color)
{
    CGContextSetStrokeColorWithColor(context, cachedCGColor(color));
}

CGColorSpaceRef sRGBColorSpaceRef()
{
    static CGColorSpaceRef sRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(WIN)
        // Out-of-date CG installations will not honor kCGColorSpaceSRGB. This logic avoids
        // causing a crash under those conditions. Since the default color space in Windows
        // is sRGB, this all works out nicely.
        // FIXME: Is this still needed? rdar://problem/15213515 was fixed.
        sRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        if (!sRGBColorSpace)
            sRGBColorSpace = CGColorSpaceCreateDeviceRGB();
#else
        sRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
#endif // PLATFORM(WIN)
    });
    return sRGBColorSpace;
}

CGColorSpaceRef linearRGBColorSpaceRef()
{
    static CGColorSpaceRef linearRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(WIN)
        // FIXME: Windows should be able to use linear sRGB, this is tracked by http://webkit.org/b/80000.
        linearRGBColorSpace = sRGBColorSpaceRef();
#else
        linearRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
#endif
    });
    return linearRGBColorSpace;
}

CGColorSpaceRef extendedSRGBColorSpaceRef()
{
    static CGColorSpaceRef extendedSRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        CGColorSpaceRef colorSpace = NULL;
#if PLATFORM(COCOA)
        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
#endif
        // If there is no support for extended sRGB, fall back to sRGB.
        if (!colorSpace)
            colorSpace = sRGBColorSpaceRef();

        extendedSRGBColorSpace = colorSpace;
    });
    return extendedSRGBColorSpace;
}

CGColorSpaceRef displayP3ColorSpaceRef()
{
    static CGColorSpaceRef displayP3ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
        displayP3ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
#else
        displayP3ColorSpace = sRGBColorSpaceRef();
#endif
    });
    return displayP3ColorSpace;
}

static InterpolationQuality convertInterpolationQuality(CGInterpolationQuality quality)
{
    switch (quality) {
    case kCGInterpolationDefault:
        return InterpolationDefault;
    case kCGInterpolationNone:
        return InterpolationNone;
    case kCGInterpolationLow:
        return InterpolationLow;
    case kCGInterpolationMedium:
        return InterpolationMedium;
    case kCGInterpolationHigh:
        return InterpolationHigh;
    }
    return InterpolationDefault;
}

static CGBlendMode selectCGBlendMode(CompositeOperator compositeOperator, BlendMode blendMode)
{
    switch (blendMode) {
    case BlendMode::Normal:
        switch (compositeOperator) {
        case CompositeClear:
            return kCGBlendModeClear;
        case CompositeCopy:
            return kCGBlendModeCopy;
        case CompositeSourceOver:
            return kCGBlendModeNormal;
        case CompositeSourceIn:
            return kCGBlendModeSourceIn;
        case CompositeSourceOut:
            return kCGBlendModeSourceOut;
        case CompositeSourceAtop:
            return kCGBlendModeSourceAtop;
        case CompositeDestinationOver:
            return kCGBlendModeDestinationOver;
        case CompositeDestinationIn:
            return kCGBlendModeDestinationIn;
        case CompositeDestinationOut:
            return kCGBlendModeDestinationOut;
        case CompositeDestinationAtop:
            return kCGBlendModeDestinationAtop;
        case CompositeXOR:
            return kCGBlendModeXOR;
        case CompositePlusDarker:
            return kCGBlendModePlusDarker;
        case CompositePlusLighter:
            return kCGBlendModePlusLighter;
        case CompositeDifference:
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

void GraphicsContext::platformInit(CGContextRef cgContext)
{
    if (!cgContext)
        return;

    m_data = new GraphicsContextPlatformPrivate(cgContext);
    // Make sure the context starts in sync with our state.
    setPlatformFillColor(fillColor());
    setPlatformStrokeColor(strokeColor());
    setPlatformStrokeThickness(strokeThickness());
    m_state.imageInterpolationQuality = convertInterpolationQuality(CGContextGetInterpolationQuality(platformContext()));
}

void GraphicsContext::platformDestroy()
{
    delete m_data;
}

CGContextRef GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    ASSERT(m_data->m_cgContext);
    return m_data->m_cgContext.get();
}

void GraphicsContext::savePlatformState()
{
    ASSERT(!paintingDisabled());
    ASSERT(hasPlatformContext());

    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // save of the secondary context (in GraphicsContextPlatformPrivateCG.h).
    CGContextSaveGState(platformContext());
    m_data->save();
}

void GraphicsContext::restorePlatformState()
{
    ASSERT(!paintingDisabled());
    ASSERT(hasPlatformContext());

    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // restore of the secondary context (in GraphicsContextPlatformPrivateCG.h).
    CGContextRestoreGState(platformContext());
    m_data->restore();
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::drawNativeImage(const RetainPtr<CGImageRef>& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode, ImageOrientation orientation)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawNativeImage(image, imageSize, destRect, srcRect, op, blendMode, orientation);
        return;
    }

#if !LOG_DISABLED
    MonotonicTime startTime = MonotonicTime::now();
#endif
    RetainPtr<CGImageRef> subImage(image);

    float currHeight = orientation.usesWidthAsHeight() ? CGImageGetWidth(subImage.get()) : CGImageGetHeight(subImage.get());
    if (currHeight <= srcRect.y())
        return;

    CGContextRef context = platformContext();
    CGAffineTransform transform = CGContextGetCTM(context);
    CGContextStateSaver stateSaver(context, false);
    
    bool shouldUseSubimage = false;

    // If the source rect is a subportion of the image, then we compute an inflated destination rect that will hold the entire image
    // and then set a clip to the portion that we want to display.
    FloatRect adjustedDestRect = destRect;

    if (srcRect.size() != imageSize) {
        CGInterpolationQuality interpolationQuality = CGContextGetInterpolationQuality(context);
        // When the image is scaled using high-quality interpolation, we create a temporary CGImage
        // containing only the portion we want to display. We need to do this because high-quality
        // interpolation smoothes sharp edges, causing pixels from outside the source rect to bleed
        // into the destination rect. See <rdar://problem/6112909>.
        shouldUseSubimage = (interpolationQuality != kCGInterpolationNone) && (srcRect.size() != destRect.size() || !getCTM().isIdentityOrTranslationOrFlipped());
        float xScale = srcRect.width() / destRect.width();
        float yScale = srcRect.height() / destRect.height();
        if (shouldUseSubimage) {
            FloatRect subimageRect = srcRect;
            float leftPadding = srcRect.x() - floorf(srcRect.x());
            float topPadding = srcRect.y() - floorf(srcRect.y());

            subimageRect.move(-leftPadding, -topPadding);
            adjustedDestRect.move(-leftPadding / xScale, -topPadding / yScale);

            subimageRect.setWidth(ceilf(subimageRect.width() + leftPadding));
            adjustedDestRect.setWidth(subimageRect.width() / xScale);

            subimageRect.setHeight(ceilf(subimageRect.height() + topPadding));
            adjustedDestRect.setHeight(subimageRect.height() / yScale);

#if CACHE_SUBIMAGES
            subImage = SubimageCacheWithTimer::getSubimage(subImage.get(), subimageRect);
#else
            subImage = adoptCF(CGImageCreateWithImageInRect(subImage.get(), subimageRect));
#endif
            if (currHeight < srcRect.maxY()) {
                ASSERT(CGImageGetHeight(subImage.get()) == currHeight - CGRectIntegral(srcRect).origin.y);
                adjustedDestRect.setHeight(CGImageGetHeight(subImage.get()) / yScale);
            }
        } else {
            adjustedDestRect.setLocation(FloatPoint(destRect.x() - srcRect.x() / xScale, destRect.y() - srcRect.y() / yScale));
            adjustedDestRect.setSize(FloatSize(imageSize.width() / xScale, imageSize.height() / yScale));
        }

        if (!destRect.contains(adjustedDestRect)) {
            stateSaver.save();
            CGContextClipToRect(context, destRect);
        }
    }

    // If the image is only partially loaded, then shrink the destination rect that we're drawing into accordingly.
    if (!shouldUseSubimage && currHeight < imageSize.height())
        adjustedDestRect.setHeight(adjustedDestRect.height() * currHeight / imageSize.height());

#if PLATFORM(IOS_FAMILY)
    bool wasAntialiased = CGContextGetShouldAntialias(context);
    // Anti-aliasing is on by default on the iPhone. Need to turn it off when drawing images.
    CGContextSetShouldAntialias(context, false);

    // Align to pixel boundaries
    adjustedDestRect = roundToDevicePixels(adjustedDestRect);
#endif

    setPlatformCompositeOperation(op, blendMode);

    // ImageOrientation expects the origin to be at (0, 0)
    CGContextTranslateCTM(context, adjustedDestRect.x(), adjustedDestRect.y());
    adjustedDestRect.setLocation(FloatPoint());

    if (orientation != DefaultImageOrientation) {
        CGContextConcatCTM(context, orientation.transformFromDefault(adjustedDestRect.size()));
        if (orientation.usesWidthAsHeight()) {
            // The destination rect will have it's width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            adjustedDestRect = FloatRect(adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.height(), adjustedDestRect.width());
        }
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
    }

    LOG_WITH_STREAM(Images, stream << "GraphicsContext::drawNativeImage " << image.get() << " size " << imageSize << " into " << destRect << " took " << (MonotonicTime::now() - startTime).milliseconds() << "ms");
}

static void drawPatternCallback(void* info, CGContextRef context)
{
    CGImageRef image = (CGImageRef)info;
    CGFloat height = CGImageGetHeight(image);
#if PLATFORM(IOS_FAMILY)
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -height);
#endif
    CGContextDrawImage(context, GraphicsContext(context).roundToDevicePixels(FloatRect(0, 0, CGImageGetWidth(image), height)), image);
}

static void patternReleaseCallback(void* info)
{
    callOnMainThread([image = static_cast<CGImageRef>(info)] {
        CGImageRelease(image);
    });
}

void GraphicsContext::drawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
{
    if (paintingDisabled() || !patternTransform.isInvertible())
        return;

    if (m_impl) {
        m_impl->drawPattern(image, destRect, tileRect, patternTransform, phase, spacing, op, blendMode);
        return;
    }

    CGContextRef context = platformContext();
    CGContextStateSaver stateSaver(context);
    CGContextClipToRect(context, destRect);

    setPlatformCompositeOperation(op, blendMode);

    CGContextTranslateCTM(context, destRect.x(), destRect.y() + destRect.height());
    CGContextScaleCTM(context, 1, -1);
    
    // Compute the scaled tile size.
    float scaledTileHeight = tileRect.height() * narrowPrecisionToFloat(patternTransform.d());
    
    // We have to adjust the phase to deal with the fact we're in Cartesian space now (with the bottom left corner of destRect being
    // the origin).
    float adjustedX = phase.x() - destRect.x() + tileRect.x() * narrowPrecisionToFloat(patternTransform.a()); // We translated the context so that destRect.x() is the origin, so subtract it out.
    float adjustedY = destRect.height() - (phase.y() - destRect.y() + tileRect.y() * narrowPrecisionToFloat(patternTransform.d()) + scaledTileHeight);

    auto tileImage = image.nativeImageForCurrentFrame();
    float h = CGImageGetHeight(tileImage.get());

    RetainPtr<CGImageRef> subImage;
#if PLATFORM(IOS_FAMILY)
    FloatSize imageSize = image.originalSize();
#else
    FloatSize imageSize = image.size();
#endif
    if (tileRect.size() == imageSize)
        subImage = tileImage;
    else {
        // Copying a sub-image out of a partially-decoded image stops the decoding of the original image. It should never happen
        // because sub-images are only used for border-image, which only renders when the image is fully decoded.
        ASSERT(h == image.height());
        subImage = adoptCF(CGImageCreateWithImageInRect(tileImage.get(), tileRect));
    }

    // If we need to paint gaps between tiles because we have a partially loaded image or non-zero spacing,
    // fall back to the less efficient CGPattern-based mechanism.
    float scaledTileWidth = tileRect.width() * narrowPrecisionToFloat(patternTransform.a());
    float w = CGImageGetWidth(tileImage.get());
    if (w == image.size().width() && h == image.size().height() && !spacing.width() && !spacing.height()) {
        // FIXME: CG seems to snap the images to integral sizes. When we care (e.g. with border-image-repeat: round),
        // we should tile all but the last, and stetch the last image to fit.
        CGContextDrawTiledImage(context, FloatRect(adjustedX, adjustedY, scaledTileWidth, scaledTileHeight), subImage.get());
    } else {
        static const CGPatternCallbacks patternCallbacks = { 0, drawPatternCallback, patternReleaseCallback };
        CGAffineTransform matrix = CGAffineTransformMake(narrowPrecisionToCGFloat(patternTransform.a()), 0, 0, narrowPrecisionToCGFloat(patternTransform.d()), adjustedX, adjustedY);
        matrix = CGAffineTransformConcat(matrix, CGContextGetCTM(context));
        // The top of a partially-decoded image is drawn at the bottom of the tile. Map it to the top.
        matrix = CGAffineTransformTranslate(matrix, 0, image.size().height() - h);
#if PLATFORM(IOS_FAMILY)
        matrix = CGAffineTransformScale(matrix, 1, -1);
        matrix = CGAffineTransformTranslate(matrix, 0, -h);
#endif
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

void GraphicsContext::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    if (paintingDisabled())
        return;

    FloatSize bufferDestinationSize = buffer.sizeForDestinationSize(destRect.size());
    RetainPtr<CGImageRef> image = buffer.copyNativeImage(DontCopyBackingStore);

    CGContextRef context = platformContext();
    // FIXME: This image needs to be grayscale to be used as an alpha mask here.
    CGContextTranslateCTM(context, destRect.x(), destRect.y() + bufferDestinationSize.height());
    CGContextScaleCTM(context, 1, -1);
    CGContextClipToRect(context, FloatRect(FloatPoint(0, bufferDestinationSize.height() - destRect.height()), destRect.size()));
    CGContextClipToMask(context, FloatRect(FloatPoint(), bufferDestinationSize), image.get());
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, -destRect.x(), -destRect.y() - destRect.height());
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawRect(rect, borderThickness);
        return;
    }

    // FIXME: this function does not handle patterns and gradients like drawPath does, it probably should.
    ASSERT(!rect.isEmpty());

    CGContextRef context = platformContext();

    CGContextFillRect(context, rect);

    if (strokeStyle() != NoStroke) {
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
void GraphicsContext::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke)
        return;

    if (m_impl) {
        m_impl->drawLine(point1, point2);
        return;
    }

    float thickness = strokeThickness();
    bool isVerticalLine = (point1.x() + thickness == point2.x());
    float strokeWidth = isVerticalLine ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!thickness || !strokeWidth)
        return;

    CGContextRef context = platformContext();

    StrokeStyle strokeStyle = this->strokeStyle();
    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == DottedStroke || strokeStyle == DashedStroke;

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
        CGContextSetShouldAntialias(context, strokeStyle == DottedStroke || strokeStyle == DashedStroke);
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

void GraphicsContext::drawEllipse(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawEllipse(rect);
        return;
    }

    Path path;
    path.addEllipse(rect);
    drawPath(path);
}

void GraphicsContext::applyStrokePattern()
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->applyStrokePattern();
        return;
    }

    CGContextRef cgContext = platformContext();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    RetainPtr<CGPatternRef> platformPattern = adoptCF(m_state.strokePattern->createPlatformPattern(userToBaseCTM));
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(0));
    CGContextSetStrokeColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(cgContext, platformPattern.get(), &patternAlpha);
}

void GraphicsContext::applyFillPattern()
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->applyFillPattern();
        return;
    }

    CGContextRef cgContext = platformContext();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    RetainPtr<CGPatternRef> platformPattern = adoptCF(m_state.fillPattern->createPlatformPattern(userToBaseCTM));
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));
    CGContextSetFillColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(cgContext, platformPattern.get(), &patternAlpha);
}

static inline bool calculateDrawingMode(const GraphicsContextState& state, CGPathDrawingMode& mode)
{
    bool shouldFill = state.fillPattern || state.fillColor.isVisible();
    bool shouldStroke = state.strokePattern || (state.strokeStyle != NoStroke && state.strokeColor.isVisible());
    bool useEOFill = state.fillRule == WindRule::EvenOdd;

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

void GraphicsContext::drawPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->drawPath(path);
        return;
    }

    CGContextRef context = platformContext();
    const GraphicsContextState& state = m_state;

    if (state.fillGradient || state.strokeGradient) {
        // We don't have any optimized way to fill & stroke a path using gradients
        // FIXME: Be smarter about this.
        fillPath(path);
        strokePath(path);
        return;
    }

    if (state.fillPattern)
        applyFillPattern();
    if (state.strokePattern)
        applyStrokePattern();

    CGPathDrawingMode drawingMode;
    if (calculateDrawingMode(state, drawingMode)) {
#if USE_DRAW_PATH_DIRECT
        CGContextDrawPathDirect(context, drawingMode, path.platformPath(), nullptr);
#else
        CGContextBeginPath(context);
        CGContextAddPath(context, path.platformPath());
        CGContextDrawPath(context, drawingMode);
#endif
    }
}

void GraphicsContext::fillPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->fillPath(path);
        return;
    }

    CGContextRef context = platformContext();

    if (m_state.fillGradient) {
        if (hasShadow()) {
            FloatRect rect = path.fastBoundingRect();
            FloatSize layerSize = getCTM().mapSize(rect.size());

            CGLayerRef layer = CGLayerCreateWithContext(context, layerSize, 0);
            CGContextRef layerContext = CGLayerGetContext(layer);

            CGContextScaleCTM(layerContext, layerSize.width() / rect.width(), layerSize.height() / rect.height());
            CGContextTranslateCTM(layerContext, -rect.x(), -rect.y());
            CGContextBeginPath(layerContext);
            CGContextAddPath(layerContext, path.platformPath());
            CGContextConcatCTM(layerContext, m_state.fillGradient->gradientSpaceTransform());

            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(layerContext);
            else
                CGContextClip(layerContext);

            m_state.fillGradient->paint(layerContext);
            CGContextDrawLayerInRect(context, rect, layer);
            CGLayerRelease(layer);
        } else {
            CGContextBeginPath(context);
            CGContextAddPath(context, path.platformPath());
            CGContextStateSaver stateSaver(context);
            CGContextConcatCTM(context, m_state.fillGradient->gradientSpaceTransform());

            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(context);
            else
                CGContextClip(context);

            m_state.fillGradient->paint(*this);
        }

        return;
    }

    if (m_state.fillPattern)
        applyFillPattern();
#if USE_DRAW_PATH_DIRECT
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

void GraphicsContext::strokePath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->strokePath(path);
        return;
    }

    CGContextRef context = platformContext();

    if (m_state.strokeGradient) {
        if (hasShadow()) {
            FloatRect rect = path.fastBoundingRect();
            float lineWidth = strokeThickness();
            float doubleLineWidth = lineWidth * 2;
            float adjustedWidth = ceilf(rect.width() + doubleLineWidth);
            float adjustedHeight = ceilf(rect.height() + doubleLineWidth);

            FloatSize layerSize = getCTM().mapSize(FloatSize(adjustedWidth, adjustedHeight));

            CGLayerRef layer = CGLayerCreateWithContext(context, layerSize, 0);
            CGContextRef layerContext = CGLayerGetContext(layer);
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
            CGContextConcatCTM(layerContext, m_state.strokeGradient->gradientSpaceTransform());
            m_state.strokeGradient->paint(layerContext);

            float destinationX = roundf(rect.x() - lineWidth);
            float destinationY = roundf(rect.y() - lineWidth);
            CGContextDrawLayerInRect(context, CGRectMake(destinationX, destinationY, adjustedWidth, adjustedHeight), layer);
            CGLayerRelease(layer);
        } else {
            CGContextStateSaver stateSaver(context);
            CGContextBeginPath(context);
            CGContextAddPath(context, path.platformPath());
            CGContextReplacePathWithStrokedPath(context);
            CGContextClip(context);
            CGContextConcatCTM(context, m_state.strokeGradient->gradientSpaceTransform());
            m_state.strokeGradient->paint(*this);
        }
        return;
    }

    if (m_state.strokePattern)
        applyStrokePattern();
#if USE_DRAW_PATH_DIRECT
    CGContextDrawPathDirect(context, kCGPathStroke, path.platformPath(), nullptr);
#else
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    CGContextStrokePath(context);
#endif
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect);
        return;
    }

    CGContextRef context = platformContext();

    if (m_state.fillGradient) {
        CGContextStateSaver stateSaver(context);
        if (hasShadow()) {
            FloatSize layerSize = getCTM().mapSize(rect.size());

            CGLayerRef layer = CGLayerCreateWithContext(context, layerSize, 0);
            CGContextRef layerContext = CGLayerGetContext(layer);

            CGContextScaleCTM(layerContext, layerSize.width() / rect.width(), layerSize.height() / rect.height());
            CGContextTranslateCTM(layerContext, -rect.x(), -rect.y());
            CGContextAddRect(layerContext, rect);
            CGContextClip(layerContext);

            CGContextConcatCTM(layerContext, m_state.fillGradient->gradientSpaceTransform());
            m_state.fillGradient->paint(layerContext);
            CGContextDrawLayerInRect(context, rect, layer);
            CGLayerRelease(layer);
        } else {
            CGContextClipToRect(context, rect);
            CGContextConcatCTM(context, m_state.fillGradient->gradientSpaceTransform());
            m_state.fillGradient->paint(*this);
        }
        return;
    }

    if (m_state.fillPattern)
        applyFillPattern();

    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms; // Don't use ShadowBlur for canvas yet.
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        // Turn off CG shadows.
        CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);

        ShadowBlur contextShadow(m_state);
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect, color);
        return;
    }

    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color);

    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms; // Don't use ShadowBlur for canvas yet.
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        // Turn off CG shadows.
        CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);

        ShadowBlur contextShadow(m_state);
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
    
    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor);
}

void GraphicsContext::platformFillRoundedRect(const FloatRoundedRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    
    ASSERT(hasPlatformContext());

    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color);

    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms; // Don't use ShadowBlur for canvas yet.
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        // Turn off CG shadows.
        CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);

        ShadowBlur contextShadow(m_state);
        contextShadow.drawRectShadow(*this, rect);
    }

    const FloatRect& r = rect.rect();
    const FloatRoundedRect::Radii& radii = rect.radii();
    bool equalWidths = (radii.topLeft().width() == radii.topRight().width() && radii.topRight().width() == radii.bottomLeft().width() && radii.bottomLeft().width() == radii.bottomRight().width());
    bool equalHeights = (radii.topLeft().height() == radii.bottomLeft().height() && radii.bottomLeft().height() == radii.topRight().height() && radii.topRight().height() == radii.bottomRight().height());
    bool hasCustomFill = m_state.fillGradient || m_state.fillPattern;
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

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRectWithRoundedHole(rect, roundedHoleRect, color);
        return;
    }

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
    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms;
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        // Turn off CG shadows.
        CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);

        ShadowBlur contextShadow(m_state);
        contextShadow.drawInsetShadow(*this, rect, roundedHoleRect);
    }

    fillPath(path);

    if (drawOwnShadow)
        stateSaver.restore();
    
    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clip(rect);
        return;
    }

    CGContextClipToRect(platformContext(), rect);
    m_data->clip(rect);
}

void GraphicsContext::clipOut(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(rect);
        return;
    }

    // FIXME: Using CGRectInfinite is much faster than getting the clip bounding box. However, due
    // to <rdar://problem/12584492>, CGRectInfinite can't be used with an accelerated context that
    // has certain transforms that aren't just a translation or a scale. And due to <rdar://problem/14634453>
    // we cannot use it in for a printing context either.
    const AffineTransform& ctm = getCTM();
    bool canUseCGRectInfinite = CGContextGetType(platformContext()) != kCGContextTypePDF && (!isAcceleratedContext() || (!ctm.b() && !ctm.c()));
    CGRect rects[2] = { canUseCGRectInfinite ? CGRectInfinite : CGContextGetClipBoundingBox(platformContext()), rect };
    CGContextBeginPath(platformContext());
    CGContextAddRects(platformContext(), rects, 2);
    CGContextEOClip(platformContext());
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(path);
        return;
    }

    CGContextBeginPath(platformContext());
    CGContextAddRect(platformContext(), CGContextGetClipBoundingBox(platformContext()));
    if (!path.isEmpty())
        CGContextAddPath(platformContext(), path.platformPath());
    CGContextEOClip(platformContext());
}

void GraphicsContext::clipPath(const Path& path, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipPath(path, clipRule);
        return;
    }

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

IntRect GraphicsContext::clipBounds() const
{
    if (paintingDisabled())
        return IntRect();

    if (m_impl)
        return m_impl->clipBounds();

    return enclosingIntRect(CGContextGetClipBoundingBox(platformContext()));
}

void GraphicsContext::beginPlatformTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    save();

    CGContextRef context = platformContext();
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::endPlatformTransparencyLayer()
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextRef context = platformContext();
    CGContextEndTransparencyLayer(context);

    restore();
}

bool GraphicsContext::supportsTransparencyLayers()
{
    return true;
}

static void applyShadowOffsetWorkaroundIfNeeded(const GraphicsContext& context, CGFloat& xOffset, CGFloat& yOffset)
{
#if PLATFORM(IOS_FAMILY) || PLATFORM(WIN)
    UNUSED_PARAM(context);
    UNUSED_PARAM(xOffset);
    UNUSED_PARAM(yOffset);
#else
    if (context.isAcceleratedContext())
        return;

    if (CGContextDrawsWithCorrectShadowOffsets(context.platformContext()))
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

void GraphicsContext::setPlatformShadow(const FloatSize& offset, float blur, const Color& color)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    
    // FIXME: we could avoid the shadow setup cost when we know we'll render the shadow ourselves.

    CGFloat xOffset = offset.width();
    CGFloat yOffset = offset.height();
    CGFloat blurRadius = blur;
    CGContextRef context = platformContext();

    if (!m_state.shadowsIgnoreTransforms) {
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

    applyShadowOffsetWorkaroundIfNeeded(*this, xOffset, yOffset);

    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    if (!color.isValid())
        CGContextSetShadow(context, CGSizeMake(xOffset, yOffset), blurRadius);
    else
        CGContextSetShadowWithColor(context, CGSizeMake(xOffset, yOffset), blurRadius, cachedCGColor(color));
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;
    CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        // Maybe this should be part of the state.
        m_impl->setMiterLimit(limit);
        return;
    }

    CGContextSetMiterLimit(platformContext(), limit);
}

void GraphicsContext::clearRect(const FloatRect& r)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clearRect(r);
        return;
    }

    CGContextClearRect(platformContext(), r);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->strokeRect(rect, lineWidth);
        return;
    }

    CGContextRef context = platformContext();

    if (m_state.strokeGradient) {
        if (hasShadow()) {
            const float doubleLineWidth = lineWidth * 2;
            float adjustedWidth = ceilf(rect.width() + doubleLineWidth);
            float adjustedHeight = ceilf(rect.height() + doubleLineWidth);
            FloatSize layerSize = getCTM().mapSize(FloatSize(adjustedWidth, adjustedHeight));

            CGLayerRef layer = CGLayerCreateWithContext(context, layerSize, 0);

            CGContextRef layerContext = CGLayerGetContext(layer);
            m_state.strokeThickness = lineWidth;
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
            CGContextConcatCTM(layerContext, m_state.strokeGradient->gradientSpaceTransform());
            m_state.strokeGradient->paint(layerContext);

            const float destinationX = roundf(rect.x() - lineWidth);
            const float destinationY = roundf(rect.y() - lineWidth);
            CGContextDrawLayerInRect(context, CGRectMake(destinationX, destinationY, adjustedWidth, adjustedHeight), layer);
            CGLayerRelease(layer);
        } else {
            CGContextStateSaver stateSaver(context);
            setStrokeThickness(lineWidth);
            CGContextAddRect(context, rect);
            CGContextReplacePathWithStrokedPath(context);
            CGContextClip(context);
            CGContextConcatCTM(context, m_state.strokeGradient->gradientSpaceTransform());
            m_state.strokeGradient->paint(*this);
        }
        return;
    }

    if (m_state.strokePattern)
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

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineCap(cap);
        return;
    }

    switch (cap) {
    case ButtCap:
        CGContextSetLineCap(platformContext(), kCGLineCapButt);
        break;
    case RoundCap:
        CGContextSetLineCap(platformContext(), kCGLineCapRound);
        break;
    case SquareCap:
        CGContextSetLineCap(platformContext(), kCGLineCapSquare);
        break;
    }
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineDash(dashes, dashOffset);
        return;
    }

    if (dashOffset < 0) {
        float length = 0;
        for (size_t i = 0; i < dashes.size(); ++i)
            length += static_cast<float>(dashes[i]);
        if (length)
            dashOffset = fmod(dashOffset, length) + length;
    }
    CGContextSetLineDash(platformContext(), dashOffset, dashes.data(), dashes.size());
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineJoin(join);
        return;
    }

    switch (join) {
    case MiterJoin:
        CGContextSetLineJoin(platformContext(), kCGLineJoinMiter);
        break;
    case RoundJoin:
        CGContextSetLineJoin(platformContext(), kCGLineJoinRound);
        break;
    case BevelJoin:
        CGContextSetLineJoin(platformContext(), kCGLineJoinBevel);
        break;
    }
}

void GraphicsContext::canvasClip(const Path& path, WindRule fillRule)
{
    clipPath(path, fillRule);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->scale(size);
        return;
    }

    CGContextScaleCTM(platformContext(), size.width(), size.height());
    m_data->scale(size);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::rotate(float angle)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->rotate(angle);
        return;
    }

    CGContextRotateCTM(platformContext(), angle);
    m_data->rotate(angle);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->translate(x, y);
        return;
    }

    CGContextTranslateCTM(platformContext(), x, y);
    m_data->translate(x, y);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->concatCTM(transform);
        return;
    }

    CGContextConcatCTM(platformContext(), transform);
    m_data->concatCTM(transform);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setCTM(transform);
        return;
    }

    CGContextSetCTM(platformContext(), transform);
    m_data->setCTM(transform);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale includeScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    if (m_impl)
        return m_impl->getCTM(includeScale);

    // The CTM usually includes the deviceScaleFactor except in WebKit 1 when the
    // content is non-composited, since the scale factor is integrated at a lower
    // level. To guarantee the deviceScale is included, we can use this CG API.
    if (includeScale == DefinitelyIncludeDeviceScale)
        return CGContextGetUserSpaceToDeviceSpaceTransform(platformContext());

    return CGContextGetCTM(platformContext());
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    if (paintingDisabled())
        return rect;

    if (m_impl)
        return m_impl->roundToDevicePixels(rect, roundingMode);

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

    float deviceScaleX = sqrtf(deviceMatrix.a * deviceMatrix.a + deviceMatrix.b * deviceMatrix.b);
    float deviceScaleY = sqrtf(deviceMatrix.c * deviceMatrix.c + deviceMatrix.d * deviceMatrix.d);

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

void GraphicsContext::drawLineForText(const FloatPoint& point, float width, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    DashArray widths;
    widths.append(width);
    widths.append(0);
    drawLinesForText(point, widths, printing, doubleLines, strokeStyle);
}

void GraphicsContext::drawLinesForText(const FloatPoint& point, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    if (paintingDisabled())
        return;

    if (!widths.size())
        return;

    if (m_impl) {
        m_impl->drawLinesForText(point, widths, printing, doubleLines, strokeThickness());
        return;
    }

    Color localStrokeColor(strokeColor());

    FloatRect bounds = computeLineBoundsAndAntialiasingModeForText(point, widths.last(), printing, localStrokeColor);
    bool fillColorIsNotEqualToStrokeColor = fillColor() != localStrokeColor;
    
    Vector<CGRect, 4> dashBounds;
    ASSERT(!(widths.size() % 2));
    dashBounds.reserveInitialCapacity(dashBounds.size() / 2);

    float dashWidth = 0;
    switch (strokeStyle) {
    case DottedStroke:
        dashWidth = bounds.height();
        break;
    case DashedStroke:
        dashWidth = 2 * bounds.height();
        break;
    case SolidStroke:
    default:
        break;
    }

    for (size_t i = 0; i < widths.size(); i += 2) {
        auto left = widths[i];
        auto width = widths[i+1] - widths[i];
        if (!dashWidth)
            dashBounds.append(CGRectMake(bounds.x() + left, bounds.y(), width, bounds.height()));
        else {
            auto startParticle = static_cast<unsigned>(std::ceil(left / (2 * dashWidth)));
            auto endParticle = static_cast<unsigned>((left + width) / (2 * dashWidth));
            for (unsigned j = startParticle; j < endParticle; ++j)
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

void GraphicsContext::setURLForRect(const URL& link, const FloatRect& destRect)
{
#if !PLATFORM(IOS_FAMILY)
    if (paintingDisabled())
        return;

    if (m_impl) {
        WTFLogAlways("GraphicsContext::setURLForRect() is not yet compatible with recording contexts.");
        return; // FIXME for display lists.
    }

    RetainPtr<CFURLRef> urlRef = link.createCFURL();
    if (!urlRef)
        return;

    CGContextRef context = platformContext();

    FloatRect rect = destRect;
    // Get the bounding box to handle clipping.
    rect.intersect(CGContextGetClipBoundingBox(context));

    CGPDFContextSetURLForRect(context, urlRef.get(), CGRectApplyAffineTransform(rect, CGContextGetCTM(context)));
#else
    UNUSED_PARAM(link);
    UNUSED_PARAM(destRect);
#endif
}

void GraphicsContext::setPlatformImageInterpolationQuality(InterpolationQuality mode)
{
    ASSERT(!paintingDisabled());

    CGInterpolationQuality quality = kCGInterpolationDefault;
    switch (mode) {
    case InterpolationDefault:
        quality = kCGInterpolationDefault;
        break;
    case InterpolationNone:
        quality = kCGInterpolationNone;
        break;
    case InterpolationLow:
        quality = kCGInterpolationLow;
        break;
    case InterpolationMedium:
        quality = kCGInterpolationMedium;
        break;
    case InterpolationHigh:
        quality = kCGInterpolationHigh;
        break;
    }
    CGContextSetInterpolationQuality(platformContext(), quality);
}

void GraphicsContext::setIsCALayerContext(bool isLayerContext)
{
    if (paintingDisabled())
        return;

    // FIXME
    if (m_impl)
        return;

    if (isLayerContext)
        m_data->m_contextFlags |= IsLayerCGContext;
    else
        m_data->m_contextFlags &= ~IsLayerCGContext;
}

bool GraphicsContext::isCALayerContext() const
{
    if (paintingDisabled())
        return false;

    // FIXME
    if (m_impl)
        return false;

    return m_data->m_contextFlags & IsLayerCGContext;
}

void GraphicsContext::setIsAcceleratedContext(bool isAccelerated)
{
    if (paintingDisabled())
        return;

    // FIXME
    if (m_impl)
        return;

    if (isAccelerated)
        m_data->m_contextFlags |= IsAcceleratedCGContext;
    else
        m_data->m_contextFlags &= ~IsAcceleratedCGContext;
}

bool GraphicsContext::isAcceleratedContext() const
{
    if (paintingDisabled())
        return false;

    // FIXME
    if (m_impl)
        return false;

    return m_data->m_contextFlags & IsAcceleratedCGContext;
}

void GraphicsContext::setPlatformTextDrawingMode(TextDrawingModeFlags mode)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextRef context = platformContext();
    switch (mode) {
    case TextModeFill:
        CGContextSetTextDrawingMode(context, kCGTextFill);
        break;
    case TextModeStroke:
        CGContextSetTextDrawingMode(context, kCGTextStroke);
        break;
    case TextModeFill | TextModeStroke:
        CGContextSetTextDrawingMode(context, kCGTextFillStroke);
        break;
    default:
        break;
    }
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    setCGStrokeColor(platformContext(), color);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextSetLineWidth(platformContext(), std::max(thickness, 0.f));
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    setCGFillColor(platformContext(), color);
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextSetShouldAntialias(platformContext(), enable);
}

void GraphicsContext::setPlatformShouldSmoothFonts(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextSetShouldSmoothFonts(platformContext(), enable);
}

void GraphicsContext::setPlatformAlpha(float alpha)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextSetAlpha(platformContext(), alpha);
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator compositeOperator, BlendMode blendMode)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    CGContextSetBlendMode(platformContext(), selectCGBlendMode(compositeOperator, blendMode));
}

void GraphicsContext::platformApplyDeviceScaleFactor(float deviceScaleFactor)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    // CoreGraphics expects the base CTM of a HiDPI context to have the scale factor applied to it.
    // Failing to change the base level CTM will cause certain CG features, such as focus rings,
    // to draw with a scale factor of 1 rather than the actual scale factor.
    CGContextSetBaseCTM(platformContext(), CGAffineTransformScale(CGContextGetBaseCTM(platformContext()), deviceScaleFactor, deviceScaleFactor));
}

void GraphicsContext::platformFillEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    // CGContextFillEllipseInRect only supports solid colors.
    if (m_state.fillGradient || m_state.fillPattern) {
        fillEllipseAsPath(ellipse);
        return;
    }

    CGContextRef context = platformContext();
    CGContextFillEllipseInRect(context, ellipse);
}

void GraphicsContext::platformStrokeEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    // CGContextStrokeEllipseInRect only supports solid colors.
    if (m_state.strokeGradient || m_state.strokePattern) {
        strokeEllipseAsPath(ellipse);
        return;
    }

    CGContextRef context = platformContext();
    CGContextStrokeEllipseInRect(context, ellipse);
}

bool GraphicsContext::supportsInternalLinks() const
{
    return true;
}

void GraphicsContext::setDestinationForRect(const String& name, const FloatRect& destRect)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextRef context = platformContext();

    FloatRect rect = destRect;
    rect.intersect(CGContextGetClipBoundingBox(context));

    CGRect transformedRect = CGRectApplyAffineTransform(rect, CGContextGetCTM(context));
    CGPDFContextSetDestinationForRect(context, name.createCFString().get(), transformedRect);
}

void GraphicsContext::addDestinationAtPoint(const String& name, const FloatPoint& position)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());

    CGContextRef context = platformContext();

    CGPoint transformedPoint = CGPointApplyAffineTransform(position, CGContextGetCTM(context));
    CGPDFContextAddDestinationAtPoint(context, name.createCFString().get(), transformedPoint);
}

}

#endif
