/*
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
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
#include "CGSubimageCacheWithTimer.h"
#include "CGUtilities.h"
#include "DisplayListRecorder.h"
#include "FloatConversion.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "ImageOrientation.h"
#include "Logging.h"
#include "Path.h"
#include "PathCG.h"
#include "Pattern.h"
#include "ShadowBlur.h"
#include "Timer.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cg/ImageIOSPI.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/ZippedRange.h>
#include <wtf/text/TextStream.h>

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

#include <pal/cg/CoreGraphicsSoftLink.h>

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/IOSurfaceAdditionsBefore.mm>)
#include <WebKitAdditions/IOSurfaceAdditionsBefore.mm>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsContextCG);

static void setCGFillColor(CGContextRef context, const Color& color, const ColorSpace& colorSpace)
{
    CGContextSetFillColorWithColor(context, cachedCGColorInDestinationStandardRange(color, colorSpace).get());
}

CGAffineTransform getUserToBaseCTM(CGContextRef context)
{
    return CGAffineTransformConcat(CGContextGetCTM(context), CGAffineTransformInvert(CGContextGetBaseCTM(context)));
}

CGFloat singularValue(const CGAffineTransform& userToBaseCTM, SingularValueSelection selection)
{
    CGFloat A = userToBaseCTM.a * userToBaseCTM.a + userToBaseCTM.b * userToBaseCTM.b;
    CGFloat B = userToBaseCTM.a * userToBaseCTM.c + userToBaseCTM.b * userToBaseCTM.d;
    CGFloat D = userToBaseCTM.c * userToBaseCTM.c + userToBaseCTM.d * userToBaseCTM.d;
    CGFloat discriminant = sqrt(4 * B * B + (A - D) * (A - D));
    if (selection == SingularValueSelection::Smallest)
        discriminant = -discriminant;
    return narrowPrecisionToCGFloat(sqrt(0.5 * ((A + D) + discriminant)));
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

static CGTextDrawingMode LEGACY_NODELETE cgTextDrawingMode(TextDrawingModeFlags mode)
{
    bool fill = mode.contains(TextDrawingMode::Fill);
    bool stroke = mode.contains(TextDrawingMode::Stroke);
    if (fill && stroke)
        return kCGTextFillStroke;
    if (fill)
        return kCGTextFill;
    return kCGTextStroke;
}

static CGBlendMode LEGACY_NODELETE selectCGBlendMode(CompositeOperator compositeOperator, BlendMode blendMode)
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

static void setCGContextPath(CGContextRef context, const Path& path)
{
    CGContextBeginPath(context);
    addToCGContextPath(context, path);
}

static FloatRect strokedPathBoundingBox(CGContextRef context, NOESCAPE const Invocable<void()> auto& addPathAndStrokeStyle)
{
    CGContextStateSaver stateSaver(context);
    addPathAndStrokeStyle();
    CGContextReplacePathWithStrokedPath(context);
    FloatRect bounds = CGContextGetPathBoundingBox(context);
    CGContextBeginPath(context);
    return bounds;
}

#if !HAVE(CGSTYLE_FIXES_DROP_SHADOW_BLUR)
// Older transparency layers are bounded by the clip, so content outside of it would lose its shadow.
void GraphicsContextCG::paintGradientInCGLayer(Gradient& gradient, const AffineTransform& gradientSpaceTransform, const FloatRect& contentBounds, NOESCAPE const Invocable<void(CGContextRef)> auto& clip)
{
    FloatRect layerBounds = enclosingIntRect(contentBounds);
    if (layerBounds.isEmpty())
        return;

    const RetainPtr<CGContextRef>& context = contextForDraw();
    FloatSize layerSize = getCTM().mapSize(layerBounds.size());
    RetainPtr layer = adoptCF(CGLayerCreateWithContext(context.get(), layerSize, 0));
    RetainPtr layerContext = CGLayerGetContext(layer.get());

    CGContextScaleCTM(layerContext.get(), layerSize.width() / layerBounds.width(), layerSize.height() / layerBounds.height());
    CGContextTranslateCTM(layerContext.get(), -layerBounds.x(), -layerBounds.y());
    clip(layerContext.get());
    CGContextConcatCTM(layerContext.get(), gradientSpaceTransform);
    gradient.paint(layerContext.get());
    CGContextDrawLayerInRect(context.get(), layerBounds, layer.get());
}
#endif

void GraphicsContextCG::paintGradientInClip(Gradient& gradient, const AffineTransform& gradientSpaceTransform, NOESCAPE const Invocable<FloatRect()> auto& computeContentBounds, NOESCAPE const Invocable<void(CGContextRef)> auto& clip)
{
    bool hasDropShadow = this->hasDropShadow();
#if !HAVE(CGSTYLE_FIXES_DROP_SHADOW_BLUR)
    if (hasDropShadow) {
        paintGradientInCGLayer(gradient, gradientSpaceTransform, computeContentBounds(), clip);
        return;
    }
#endif

    const RetainPtr<CGContextRef>& context = contextForDraw();

    // The clip has to be applied inside the transparency layer, otherwise it would also clip away the shadow.
    // Passing the content bounds to CGContextBeginTransparencyLayerWithRect() would lose the shadow of content
    // outside the clip (rdar://190563103), so bound the layer with a clip covering the content and its shadow.
    if (hasDropShadow) {
        auto layerBounds = boundsIncludingDropShadow(computeContentBounds());
        CGContextSaveGState(context.get());
        CGContextClipToRect(context.get(), layerBounds);
        CGContextBeginTransparencyLayer(context.get(), nullptr);
#if !HAVE(FIX_FOR_RADAR_190563103)
        // FIXME: Remove this workaround once the system framework rdar://190563103 is fixed.
        CGContextClearRect(context.get(), layerBounds);
#endif
    }
    {
        CGContextStateSaver stateSaver(context.get());
        clip(context.get());
        CGContextConcatCTM(context.get(), gradientSpaceTransform);
        gradient.paint(*this);
    }
    if (hasDropShadow) {
        CGContextEndTransparencyLayer(context.get());
        CGContextRestoreGState(context.get());
    }
}

static void drawPathWithCGContext(CGContextRef context, CGPathDrawingMode drawingMode, const Path& path)
{
    CGContextDrawPathDirect(context, drawingMode, protect(path.platformPath()), nullptr);
}

static RenderingMode renderingModeForCGContext(CGContextRef cgContext, GraphicsContextCG::CGContextSource source)
{
    if (!cgContext)
        return RenderingMode::Unaccelerated;
    auto type = CGContextGetType(cgContext);
    if (type == kCGContextTypeIOSurface || (source == GraphicsContextCG::CGContextFromCALayer && type == kCGContextTypeUnknown))
        return RenderingMode::Accelerated;
    if (type == kCGContextTypePDF)
        return RenderingMode::PDFDocument;
    return RenderingMode::Unaccelerated;
}

static GraphicsContext::IsDeferred isDeferredForCGContext(CGContextRef cgContext)
{
    if (!cgContext || CGContextGetType(cgContext) == kCGContextTypeBitmap)
        return GraphicsContext::IsDeferred::No;
    // Other CGContexts are deferred (iosurface, display list) or potentially deferred.
    return GraphicsContext::IsDeferred::Yes;
}

GraphicsContextCG::GraphicsContextCG(CGContextRef cgContext, CGContextSource source, std::optional<RenderingMode> knownRenderingMode)
    : GraphicsContext(isDeferredForCGContext(cgContext), GraphicsContextState::basicChangeFlags, coreInterpolationQuality(cgContext))
    , m_cgContext(cgContext)
    , m_renderingMode(knownRenderingMode.value_or(renderingModeForCGContext(cgContext, source)))
    , m_isLayerCGContext(source == GraphicsContextCG::CGContextFromCALayer)
{
    if (!cgContext)
        return;
    // Make sure the context starts in sync with our state.
    didUpdateState(m_state);
}

GraphicsContextCG::~GraphicsContextCG() = default;

bool GraphicsContextCG::hasPlatformContext() const
{
    return true;
}

const RetainPtr<CGContextRef>& GraphicsContextCG::contextForState() const
{
    ASSERT(m_cgContext);
    return m_cgContext;
}

const ColorSpace& GraphicsContextCG::colorSpace() const
{
    if (m_colorSpace)
        return *m_colorSpace;

    const RetainPtr<CGContextRef>& context = contextForDraw();
    RetainPtr<CGColorSpaceRef> colorSpace;

    // FIXME: Need to handle kCGContextTypePDF.
    auto contextType = CGContextGetType(context);
    if (contextType == kCGContextTypeIOSurface)
        colorSpace = CGIOSurfaceContextGetColorSpace(context);
    else if (contextType == kCGContextTypeBitmap)
        colorSpace = CGBitmapContextGetColorSpace(context);
    else
        colorSpace = CGContextGetColorSpace(context);

    // FIXME: Need to ASSERT(colorSpace). For now fall back to sRGB if colorSpace is nil.
    m_colorSpace = colorSpace ? ColorSpace(colorSpace) : ColorSpace::SRGB();
    return *m_colorSpace;
}

void GraphicsContextCG::save(GraphicsContextState::Purpose purpose)
{
    GraphicsContext::save(purpose);
    CGContextSaveGState(contextForState());
}

void GraphicsContextCG::restore(GraphicsContextState::Purpose purpose)
{
    if (!stackSize())
        return;

    GraphicsContext::restore(purpose);
    CGContextRestoreGState(contextForState());
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::drawNativeImage(const NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& srcRect, ImageDrawingOptions options)
{
    auto image = nativeImage.platformImage();
    if (!image)
        return;

#if ENABLE(AX_CUSTOM_COLOR_MODE) && HAVE(CGSTYLE_COLORMATRIX_BLUR)
    if (options.invertContent() == InvertContent::Yes) {
        if (auto colorMatrix = AXCustomColorModeController::colorMatrixForImageInversion(nativeImage.paintTimeTransformation())) {
            GraphicsContextStateSaver stateSaver(*this);
            clip(normalizeRect(destRect));
            setStyle(*colorMatrix);
            setCompositeOperation(options.compositeOperator(), options.blendMode());
            beginTransparencyLayer(options.compositeOperator(), options.blendMode());
            drawNativeImage(nativeImage, destRect, srcRect, { options, InvertContent::No, CompositeOperator::SourceOver, BlendMode::Normal });
            endTransparencyLayer();
            return;
        }
    }
#endif
    auto imageSize = nativeImage.size();
    if (options.orientation().usesWidthAsHeight())
        imageSize = imageSize.transposedSize();
    auto imageRect = FloatRect { { }, imageSize };
    auto normalizedSrcRect = normalizeRect(srcRect);
    auto normalizedDestRect = normalizeRect(destRect);
    if (!imageRect.intersects(normalizedSrcRect))
        return;

#if !LOG_DISABLED
    MonotonicTime startTime = MonotonicTime::now();
#endif

    auto shouldUseSubimage = [](InterpolationQuality interpolationQuality, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& transform) -> bool {
        if (interpolationQuality == InterpolationQuality::DoNotInterpolate)
            return false;
        if (transform.isRotateOrShear())
            return true;
        auto xScale = destRect.width() * transform.xScale() / srcRect.width();
        auto yScale = destRect.height() * transform.yScale() / srcRect.height();
        return !WTF::areEssentiallyEqual(xScale, yScale) || xScale > 1;
    };

    auto getSubimage = [](CGImageRef image, const FloatSize& imageSize, const FloatRect& subimageRect, ImageDrawingOptions options) -> RetainPtr<CGImageRef> {
        auto physicalSubimageRect = subimageRect;

        if (options.orientation() != ImageOrientation::Orientation::None) {
            // subimageRect is in logical coordinates. getSubimage() deals with none-oriented
            // image. We need to convert subimageRect to physical image coordinates.
            if (auto transform = options.orientation().transformFromDefault(imageSize).inverse())
                physicalSubimageRect = transform.value().mapRect(physicalSubimageRect);
        }

#if CACHE_SUBIMAGES
        if (!(CGImageGetCachingFlags(image) & kCGImageCachingTransient))
            return CGSubimageCacheWithTimer::getSubimage(image, physicalSubimageRect);
#endif
        return adoptCF(CGImageCreateWithImageInRect(image, physicalSubimageRect));
    };

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    auto setCGDynamicRangeLimitForImage = [](CGContextRef context, CGImageRef image, float dynamicRangeLimit) {
        float edrStrength = dynamicRangeLimit == 1.0 ? 1 : 0;
        float cdrStrength = dynamicRangeLimit == 0.5 ? 1 : 0;
        unsigned averageLightLevel = CGImageGetContentAverageLightLevelNits(image);

        RetainPtr edrStrengthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &edrStrength));
        RetainPtr cdrStrengthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &cdrStrength));
        RetainPtr averageLightLevelNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &averageLightLevel));

        CFTypeRef toneMappingKeys[] = { kCGContentEDRStrength, kCGContentAverageLightLevel, kCGConstrainedDynamicRange };
        CFTypeRef toneMappingValues[] = { edrStrengthNumber.get(), averageLightLevelNumber.get(), cdrStrengthNumber.get() };

        RetainPtr toneMappingOptions = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, toneMappingKeys, toneMappingValues, sizeof(toneMappingKeys) / sizeof(toneMappingKeys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        CGContentToneMappingInfo toneMappingInfo = { kCGToneMappingReferenceWhiteBased, toneMappingOptions.get() };
        CGContextSetContentToneMappingInfo(context, toneMappingInfo);
    };
#endif

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextStateSaver stateSaver(context, false);
    auto transform = CGContextGetCTM(context);

    auto oldInterpolationQuality = imageInterpolationQuality();
    auto interpolationQuality = imageInterpolationQualityForOptions(options);

    auto subImage = image;

    auto adjustedDestRect = normalizedDestRect;

    if (normalizedSrcRect != imageRect) {
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

    if (interpolationQuality != oldInterpolationQuality)
        CGContextSetInterpolationQuality(context, toCGInterpolationQuality(interpolationQuality));

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    auto oldHeadroom = CGContextGetEDRTargetHeadroom(context);
    auto oldToneMappingInfo = CGContextGetContentToneMappingInfo(context);

    auto headroom = options.headroom();
    if (headroom == Headroom::FromImage)
        headroom = nativeImage.headroom();
    if (m_maxEDRHeadroom)
        headroom = Headroom(std::min<float>(headroom, *m_maxEDRHeadroom));

    if (nativeImage.headroom() > headroom) {
        LOG_WITH_STREAM(HDR, stream << "GraphicsContextCG::drawNativeImage setEDRTargetHeadroom " << headroom << " max(" << m_maxEDRHeadroom << ")");
        CGContextSetEDRTargetHeadroom(context, headroom);
    }

    if (options.dynamicRangeLimit() == PlatformDynamicRangeLimit::standard() && options.drawsHDRContent() == DrawsHDRContent::Yes)
        setCGDynamicRangeLimitForImage(context, subImage.get(), options.dynamicRangeLimit().value());
#endif

    // Make the origin be at adjustedDestRect.location()
    CGContextTranslateCTM(context, adjustedDestRect.x(), adjustedDestRect.y());
    adjustedDestRect.setLocation(FloatPoint::zero());

    if (options.orientation() != ImageOrientation::Orientation::None) {
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
        if (interpolationQuality != oldInterpolationQuality)
            CGContextSetInterpolationQuality(context, toCGInterpolationQuality(oldInterpolationQuality));
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
        CGContextSetContentToneMappingInfo(context, oldToneMappingInfo);
        CGContextSetEDRTargetHeadroom(context, oldHeadroom);
#endif
    }

    LOG_WITH_STREAM(Images, stream << "GraphicsContextCG::drawNativeImage " << image.get() << " size " << imageSize << " into " << destRect << " took " << (MonotonicTime::now() - startTime).milliseconds() << "ms");
}

static void drawPatternCallback(void* info, CGContextRef context)
{
    CGImageRef image = (CGImageRef)info;
    auto rect = cgRoundToDevicePixels(CGContextGetUserSpaceToDeviceSpaceTransform(context), cgImageRect(image));
    CGContextDrawImage(context, rect, image);
}

static void drawPatternReleaseCallback(void* info)
{
    // info is the image reference that drawPattern() transferred to the pattern.
    SUPPRESS_RETAINPTR_CTOR_ADOPT callOnMainThread([image = adoptCF(static_cast<CGImageRef>(info))] { });
}

void GraphicsContextCG::drawPattern(const NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options)
{
    if (!patternTransform.isInvertible())
        return;

    auto image = nativeImage.platformImage();
    auto imageSize = nativeImage.size();

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextStateSaver stateSaver(context);
    CGContextClipToRect(context, destRect);

    setCGBlendMode(context, options.compositeOperator(), options.blendMode());

    auto interpolationQuality = imageInterpolationQualityForOptions(options);
    if (interpolationQuality != imageInterpolationQuality())
        CGContextSetInterpolationQuality(context, toCGInterpolationQuality(interpolationQuality));

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
        static const CGPatternCallbacks patternCallbacks = { 0, drawPatternCallback, drawPatternReleaseCallback };
        CGAffineTransform matrix = CGAffineTransformMake(narrowPrecisionToCGFloat(patternTransform.a()), 0, 0, narrowPrecisionToCGFloat(patternTransform.d()), adjustedX, adjustedY);
        matrix = CGAffineTransformConcat(matrix, CGContextGetCTM(context));
        // The top of a partially-decoded image is drawn at the bottom of the tile. Map it to the top.
        matrix = CGAffineTransformTranslate(matrix, 0, imageSize.height() - h);
        RetainPtr<CGPatternRef> pattern;
#if HAVE(CGPATTERN_CREATE_WITH_IMAGE_TRANSFORM_STEP)
        if (PAL::canLoad_CoreGraphics_CGPatternCreateWithImageTransformStep()) {
            pattern = adoptCF(PAL::softLink_CoreGraphics_CGPatternCreateWithImageTransformStep(subImage.get(), matrix,
                tileRect.width() + spacing.width() * (1 / narrowPrecisionToFloat(patternTransform.a())),
                tileRect.height() + spacing.height() * (1 / narrowPrecisionToFloat(patternTransform.d())),
                kCGPatternTilingConstantSpacing));
        } else
#endif
        {
            // The pattern takes ownership of the image reference, which is released in drawPatternReleaseCallback().
            // FIXME: If CGPatternCreate() returns NULL, drawPatternReleaseCallback() never runs and the image reference leaks.
            SUPPRESS_RETAINPTR_CTOR_ADOPT pattern = adoptCF(CGPatternCreate(subImage.leakRef(), CGRectMake(0, 0, tileRect.width(), tileRect.height()), matrix,
                tileRect.width() + spacing.width() * (1 / narrowPrecisionToFloat(patternTransform.a())),
                tileRect.height() + spacing.height() * (1 / narrowPrecisionToFloat(patternTransform.d())),
                kCGPatternTilingConstantSpacing, true, &patternCallbacks));
        }

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

    const RetainPtr<CGContextRef>& context = contextForDraw();

    CGContextFillRect(context, rect);

    if (strokeStyle() != StrokeStyle::NoStroke) {
        // We do a fill of four rects to simulate the stroke of a border.
        Color oldFillColor = fillColor();
        if (oldFillColor != strokeColor())
            setCGFillColor(context, strokeColor(), colorSpace());
        CGRect rects[4] = {
            FloatRect(rect.x(), rect.y(), rect.width(), borderThickness),
            FloatRect(rect.x(), rect.maxY() - borderThickness, rect.width(), borderThickness),
            FloatRect(rect.x(), rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness),
            FloatRect(rect.maxX() - borderThickness, rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness)
        };
        CGContextFillRects(context, rects, 4);
        if (oldFillColor != strokeColor())
            setCGFillColor(context, oldFillColor, colorSpace());
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

    const RetainPtr<CGContextRef>& context = contextForDraw();

    StrokeStyle strokeStyle = this->strokeStyle();
    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == StrokeStyle::DottedStroke || strokeStyle == StrokeStyle::DashedStroke;

    CGContextStateSaver stateSaver(context, drawsDashedLine);
    if (drawsDashedLine) {
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth);
        setCGFillColor(context, strokeColor(), colorSpace());
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
    path.addEllipseInRect(rect);
    drawPath(path);
}

void GraphicsContextCG::applyStrokePattern()
{
    RefPtr strokePattern = this->strokePattern();
    if (!strokePattern)
        return;

    const RetainPtr<CGContextRef>& cgContext = contextForDraw();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    auto platformPattern = strokePattern->createPlatformPattern(userToBaseCTM);
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(0));
    CGContextSetStrokeColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(cgContext, platformPattern.get(), &patternAlpha);
}

void GraphicsContextCG::applyFillPattern()
{
    RefPtr fillPattern = this->fillPattern();
    if (!fillPattern)
        return;

    const RetainPtr<CGContextRef>& cgContext = contextForDraw();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    auto platformPattern = fillPattern->createPlatformPattern(userToBaseCTM);
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));
    CGContextSetFillColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(cgContext, platformPattern.get(), &patternAlpha);
}

static inline bool LEGACY_NODELETE calculateDrawingMode(const GraphicsContext& context, CGPathDrawingMode& mode)
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

    const RetainPtr<CGContextRef>& context = contextForDraw();

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
    if (calculateDrawingMode(*this, drawingMode))
        drawPathWithCGContext(context, drawingMode, path);
}

void GraphicsContextCG::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;

    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (RefPtr fillGradient = this->fillGradient()) {
        paintGradientInClip(*fillGradient, fillGradientSpaceTransform(), [&] {
            return path.fastBoundingRect();
        }, [&](CGContextRef clipContext) {
            setCGContextPath(clipContext, path);
            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(clipContext);
            else
                CGContextClip(clipContext);
        });

        return;
    }

    if (fillPattern())
        applyFillPattern();

    drawPathWithCGContext(context, fillRule() == WindRule::EvenOdd ? kCGPathEOFill : kCGPathFill, path);
}

void GraphicsContextCG::strokePath(const Path& path)
{
    if (path.isEmpty())
        return;

    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (RefPtr strokeGradient = this->strokeGradient()) {
        paintGradientInClip(*strokeGradient, strokeGradientSpaceTransform(), [&] {
            return strokedPathBoundingBox(context.get(), [&] {
                setCGContextPath(context.get(), path);
            });
        }, [&](CGContextRef clipContext) {
            CGContextSetLineWidth(clipContext, strokeThickness());
            setCGContextPath(clipContext, path);
            CGContextReplacePathWithStrokedPath(clipContext);
            CGContextClip(clipContext);
        });
        return;
    }

    if (strokePattern())
        applyStrokePattern();

    if (auto line = path.singleDataLine()) {
        CGPoint cgPoints[2] { line->start(), line->end() };
        CGContextStrokeLineSegments(context, cgPoints, 2);
        return;
    }

    drawPathWithCGContext(context, kCGPathStroke, path);
}

void GraphicsContextCG::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (RefPtr fillGradient = this->fillGradient()) {
        fillRect(rect, *fillGradient, fillGradientSpaceTransform(), requiresClipToRect);
        return;
    }

    if (fillPattern())
        applyFillPattern();

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGDropShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
}

void GraphicsContextCG::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    paintGradientInClip(gradient, gradientSpaceTransform, [&] {
        return rect;
    }, [&](CGContextRef clipContext) {
        if (requiresClipToRect == RequiresClipToRect::Yes || hasDropShadow())
            CGContextClipToRect(clipContext, rect);
    });
}

void GraphicsContextCG::fillRect(const FloatRect& rect, const Color& color)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color, colorSpace());

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGDropShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);

    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor, colorSpace());
}

void GraphicsContextCG::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color, colorSpace());

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGDropShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, rect);
    }

    const FloatRect& r = rect.rect();
    const CornerRadii& radii = rect.radii();
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
        setCGFillColor(context, oldFillColor, colorSpace());
}

void GraphicsContextCG::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color, colorSpace());

    // fillRectWithRoundedHole() assumes that the edges of rect are clipped out, so we only care about shadows cast around inside the hole.
    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGDropShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawInsetShadow(*this, rect, roundedHoleRect);
    }

    drawPathWithCGContext(context, kCGPathEOFill, path);

    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor, colorSpace());
}

void GraphicsContextCG::resetClip()
{
    CGContextResetClip(contextForDraw());
}

void GraphicsContextCG::clip(const FloatRect& rect)
{
    CGContextClipToRect(contextForDraw(), rect);
}

void GraphicsContextCG::clipOut(const FloatRect& rect)
{
    // FIXME: Using CGRectInfinite is much faster than getting the clip bounding box. However, due
    // to <rdar://problem/12584492>, CGRectInfinite can't be used with an accelerated context that
    // has certain transforms that aren't just a translation or a scale. And due to <rdar://problem/14634453>
    // we cannot use it in for a printing context either.
    const RetainPtr<CGContextRef>& context = contextForDraw();
    const AffineTransform& ctm = getCTM();
    bool canUseCGRectInfinite = CGContextGetType(context) != kCGContextTypePDF && (renderingMode() == RenderingMode::Unaccelerated || (!ctm.b() && !ctm.c()));
    CGRect rects[2] = { canUseCGRectInfinite ? CGRectInfinite : CGContextGetClipBoundingBox(context), rect };
    CGContextBeginPath(context);
    CGContextAddRects(context, rects, 2);
    CGContextEOClip(context);
}

void GraphicsContextCG::clipOut(const Path& path)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextBeginPath(context);
    CGContextAddRect(context, CGContextGetClipBoundingBox(context));
    if (!path.isEmpty())
        addToCGContextPath(context, path);
    CGContextEOClip(context);
}

void GraphicsContextCG::clipPath(const Path& path, WindRule clipRule)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();
    if (path.isEmpty())
        CGContextClipToRect(context, CGRectZero);
    else {
        setCGContextPath(context, path);
        if (clipRule == WindRule::EvenOdd)
            CGContextEOClip(context);
        else
            CGContextClip(context);
    }
}

void GraphicsContextCG::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect)
{
    auto nativeImage = imageBuffer.createNativeImageReference();
    if (!nativeImage)
        return;

    // FIXME: This image needs to be grayscale to be used as an alpha mask here.
    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextTranslateCTM(context, destRect.x(), destRect.maxY());
    CGContextScaleCTM(context, 1, -1);
    CGContextClipToRect(context, { { }, destRect.size() });
    CGContextClipToMask(context, { { }, destRect.size() }, nativeImage->platformImage().get());
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, -destRect.x(), -destRect.maxY());
}

IntRect GraphicsContextCG::clipBounds() const
{
    return enclosingIntRect(CGContextGetClipBoundingBox(contextForDraw()));
}

void GraphicsContextCG::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);

    save(GraphicsContextState::Purpose::TransparencyLayer);

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);

    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::beginTransparencyLayer(CompositeOperator, BlendMode)
{
    // Passing state().alpha() to beginTransparencyLayer(opacity) will
    // preserve the current global alpha.
    beginTransparencyLayer(state().alpha());
}

void GraphicsContextCG::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextEndTransparencyLayer(context);

    restore(GraphicsContextState::Purpose::TransparencyLayer);
}

static CGFloat scaledBlurRadius(CGFloat blurRadius, const CGAffineTransform& userToBaseCTM, bool shadowsIgnoreTransforms)
{
    if (!shadowsIgnoreTransforms)
        blurRadius *= singularValue(userToBaseCTM, SingularValueSelection::Smallest);

    // Extreme "blur" values can make text drawing crash or take crazy long times, so clamp
    return std::min(blurRadius, narrowPrecisionToCGFloat(1000.0));
}

struct BaseSpaceDropShadow {
    CGSize offset;
    CGFloat blurRadius;
};

static BaseSpaceDropShadow baseSpaceDropShadow(const GraphicsDropShadow& shadow, const CGAffineTransform& userToBaseCTM, bool shadowsIgnoreTransforms)
{
    CGSize offset = shadow.offset;
    if (!shadowsIgnoreTransforms)
        offset = CGSizeApplyAffineTransform(offset, userToBaseCTM);
    return { offset, scaledBlurRadius(shadow.radius, userToBaseCTM, shadowsIgnoreTransforms) };
}

void GraphicsContextCG::setCGDropShadow(const std::optional<GraphicsDropShadow>& shadow, bool shadowsIgnoreTransforms)
{
    if (!shadow || !shadow->color.isValid() || (shadow->offset.isZero() && !shadow->radius)) {
        clearCGDropShadow();
        return;
    }

    const RetainPtr<CGContextRef>& context = contextForDraw();
    auto [offset, blurRadius] = baseSpaceDropShadow(*shadow, getUserToBaseCTM(context), shadowsIgnoreTransforms);

    CGContextSetAlpha(context, shadow->opacity);

    auto style = adoptCF(CGStyleCreateShadow2(offset, blurRadius, cachedCGColorInDestinationStandardRange(shadow->color, colorSpace()).get()));
    CGContextSetStyle(context, style.get());
}

FloatRect GraphicsContextCG::boundsIncludingDropShadow(const FloatRect& bounds)
{
    auto shadow = dropShadow();
    ASSERT(shadow);

    AffineTransform userToBaseCTM = getUserToBaseCTM(contextForDraw());
    auto baseToUserCTM = userToBaseCTM.inverse();
    if (!baseToUserCTM)
        return bounds;

    auto [offset, blurRadius] = baseSpaceDropShadow(*shadow, userToBaseCTM, shadowsIgnoreTransforms());

    // CG blurs shadows with a standard deviation of half the blur radius, and bounds the
    // result at 2.8 standard deviations plus a pixel.
    FloatRect baseSpaceBounds = userToBaseCTM.mapRect(bounds);
    FloatRect shadowBounds = baseSpaceBounds;
    shadowBounds.move(FloatSize(offset));
    shadowBounds.inflate(1.4 * blurRadius + 1);
    return baseToUserCTM->mapRect(unionRect(baseSpaceBounds, shadowBounds));
}

void GraphicsContextCG::clearCGDropShadow()
{
    CGContextSetStyle(contextForDraw(), nullptr);
}

#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
void GraphicsContextCG::setCGGaussianBlur(const GraphicsGaussianBlur& gaussianBlur, bool shadowsIgnoreTransforms)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    ASSERT(gaussianBlur.radius.width() == gaussianBlur.radius.height());

    CGAffineTransform userToBaseCTM = getUserToBaseCTM(context);
    CGFloat blurRadius = scaledBlurRadius(gaussianBlur.radius.width(), userToBaseCTM, shadowsIgnoreTransforms);

    CGGaussianBlurStyle gaussianBlurStyle = { 1, blurRadius };
    auto style = adoptCF(CGStyleCreateGaussianBlur(&gaussianBlurStyle));
    CGContextSetStyle(context, style.get());
}

void GraphicsContextCG::setCGColorMatrix(const GraphicsColorMatrix& colorMatrix)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    CGColorMatrixStyle cgColorMatrix = { 1, { 0 } };
    for (auto [dst, src] : zippedRange(cgColorMatrix.matrix, colorMatrix.values))
        dst = src;
    auto style = adoptCF(CGStyleCreateColorMatrix(&cgColorMatrix));
    CGContextSetStyle(context, style.get());
}
#endif

void GraphicsContextCG::setCGStyle(const std::optional<GraphicsStyle>& style, bool shadowsIgnoreTransforms)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (!style) {
        CGContextSetStyle(context, nullptr);
        return;
    }

    WTF::switchOn(*style,
        [&] (const GraphicsDropShadow& dropShadow) {
            setCGDropShadow(dropShadow, shadowsIgnoreTransforms);
        },
        [&] (const GraphicsGaussianBlur& gaussianBlur) {
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
            setCGGaussianBlur(gaussianBlur, shadowsIgnoreTransforms);
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(gaussianBlur);
#endif
        },
        [&] (const GraphicsColorMatrix& colorMatrix) {
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
            setCGColorMatrix(colorMatrix);
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(colorMatrix);
#endif
        }
    );
}

void GraphicsContextCG::didUpdateState(GraphicsContextState& state)
{
    auto changes = state.changes();
    if (!changes)
        return;

    if (changes.contains(GraphicsContextState::Change::ShadowsIgnoreTransforms)) {
        if (state.dropShadow())
            changes.add(GraphicsContextState::Change::DropShadow);
        if (state.style())
            changes.add(GraphicsContextState::Change::Style);
    }

    const RetainPtr<CGContextRef>& context = contextForDraw();

    for (auto change : changes) {
        switch (change) {
        case GraphicsContextState::Change::FillBrush:
            if (!state.fillBrush().hasPatternOrGradient())
                setCGFillColor(context, state.fillBrush().color(), colorSpace());
            break;

        case GraphicsContextState::Change::StrokeThickness:
            CGContextSetLineWidth(context, std::max(state.strokeThickness(), 0.f));
            break;

        case GraphicsContextState::Change::StrokeBrush:
            if (!state.strokeBrush().hasPatternOrGradient())
                CGContextSetStrokeColorWithColor(context, cachedCGColorInDestinationStandardRange(state.strokeBrush().color(), colorSpace()).get());
            break;

        case GraphicsContextState::Change::CompositeMode:
            setCGBlendMode(context, state.compositeMode().operation, state.compositeMode().blendMode);
            break;

        case GraphicsContextState::Change::DropShadow:
            setCGDropShadow(state.dropShadow(), state.shadowsIgnoreTransforms());
            break;

        case GraphicsContextState::Change::Style:
            setCGStyle(state.style(), state.shadowsIgnoreTransforms());
            break;

        case GraphicsContextState::Change::Alpha:
            CGContextSetAlpha(context, state.alpha());
            break;

        case GraphicsContextState::Change::ImageInterpolationQuality:
            CGContextSetInterpolationQuality(context, toCGInterpolationQuality(state.imageInterpolationQuality()));
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
    CGContextSetMiterLimit(contextForDraw(), limit);
}

void GraphicsContextCG::clearRect(const FloatRect& r)
{
    CGContextClearRect(contextForDraw(), r);
}

void GraphicsContextCG::strokeRect(const FloatRect& rect, float lineWidth)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (RefPtr strokeGradient = this->strokeGradient()) {
        paintGradientInClip(*strokeGradient, strokeGradientSpaceTransform(), [&] {
            return strokedPathBoundingBox(context.get(), [&] {
                CGContextSetLineWidth(context.get(), std::max(lineWidth, 0.f));
                CGContextAddRect(context.get(), rect);
            });
        }, [&](CGContextRef clipContext) {
            CGContextSetLineWidth(clipContext, std::max(lineWidth, 0.f));
            CGContextAddRect(clipContext, rect);
            CGContextReplacePathWithStrokedPath(clipContext);
            CGContextClip(clipContext);
        });
        return;
    }

    if (strokePattern())
        applyStrokePattern();

    // Using CGContextAddRect and CGContextStrokePath to stroke rect rather than
    // convenience functions (CGContextStrokeRect/CGContextStrokeRectWithWidth).
    // The convenience functions currently (in at least OSX 10.9.4) fail to
    // apply some attributes of the graphics state in certain cases
    // (as identified in https://bugs.webkit.org/show_bug.cgi?id=132948)
    CGContextSetLineWidth(context, std::max(lineWidth, 0.f));
    CGContextAddRect(context, rect);
    CGContextStrokePath(context);
    CGContextSetLineWidth(context, std::max(strokeThickness(), 0.f));
}

void GraphicsContextCG::strokeArc(const PathArc& arc)
{
#if HAVE(CGCONTEXT_STROKE_ARC)
    if (!strokeGradient()) {
        if (strokePattern())
            applyStrokePattern();

        const RetainPtr<CGContextRef>& context = contextForDraw();
        CGContextStrokeArc(context, arc.center.x(), arc.center.y(), arc.radius, arc.startAngle, arc.endAngle, arc.direction == RotationDirection::Counterclockwise);
        return;
    }
#endif
    GraphicsContext::strokeArc(arc);
}

void GraphicsContextCG::strokeLine(const PathDataLine& line)
{
    // Gradient stroking requires building a stroked path and clipping to it,
    // which CGContextStrokeLineSegments cannot do. Defer to strokePath() via
    // the base class for that case. Patterns can be inlined as long as we set
    // them up first, matching the fast path inside strokePath().
    if (strokeGradient()) {
        GraphicsContext::strokeLine(line);
        return;
    }
    if (strokePattern())
        applyStrokePattern();

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGPoint pts[2] = { line.start(), line.end() };
    CGContextStrokeLineSegments(context, pts, 2);
}

void GraphicsContextCG::setLineCap(LineCap cap)
{
    switch (cap) {
    case LineCap::Butt:
        CGContextSetLineCap(contextForDraw(), kCGLineCapButt);
        break;
    case LineCap::Round:
        CGContextSetLineCap(contextForDraw(), kCGLineCapRound);
        break;
    case LineCap::Square:
        CGContextSetLineCap(contextForDraw(), kCGLineCapSquare);
        break;
    }
}

void GraphicsContextCG::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (dashOffset < 0) {
        float length = 0;
        for (size_t i = 0; i < dashes.size(); ++i)
            length += static_cast<float>(dashes[i]);
        // CGContextSetLineDash repeats odd-length arrays, so the effective cycle is twice the sum.
        if (dashes.size() % 2)
            length *= 2;
        if (length)
            dashOffset = fmod(dashOffset, length) + length;
    }
    auto dashesSpan = dashes.span();
    CGContextSetLineDash(contextForDraw(), dashOffset, dashesSpan.data(), dashesSpan.size());
}

void GraphicsContextCG::setLineJoin(LineJoin join)
{
    switch (join) {
    case LineJoin::Miter:
        CGContextSetLineJoin(contextForDraw(), kCGLineJoinMiter);
        break;
    case LineJoin::Round:
        CGContextSetLineJoin(contextForDraw(), kCGLineJoinRound);
        break;
    case LineJoin::Bevel:
        CGContextSetLineJoin(contextForDraw(), kCGLineJoinBevel);
        break;
    }
}

void GraphicsContextCG::scale(const FloatSize& size)
{
    CGContextScaleCTM(contextForDraw(), size.width(), size.height());
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::rotate(float angle)
{
    CGContextRotateCTM(contextForDraw(), angle);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::translate(float x, float y)
{
    CGContextTranslateCTM(contextForDraw(), x, y);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::concatCTM(const AffineTransform& transform)
{
    CGContextConcatCTM(contextForDraw(), transform);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::setCTM(const AffineTransform& transform)
{
    CGContextSetCTM(contextForDraw(), transform);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContextCG::getCTM(IncludeDeviceScale includeScale) const
{
    // The CTM usually includes the deviceScaleFactor except in WebKit 1 when the
    // content is non-composited, since the scale factor is integrated at a lower
    // level. To guarantee the deviceScale is included, we can use this CG API.
    if (includeScale == DefinitelyIncludeDeviceScale)
        return CGContextGetUserSpaceToDeviceSpaceTransform(contextForDraw());

    return CGContextGetCTM(contextForDraw());
}

FloatRect GraphicsContextCG::roundToDevicePixels(const FloatRect& rect) const
{
    CGAffineTransform deviceMatrix;
    if (!m_userToDeviceTransformKnownToBeIdentity) {
        deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(contextForState());
        if (CGAffineTransformIsIdentity(deviceMatrix))
            m_userToDeviceTransformKnownToBeIdentity = true;
    }
    if (m_userToDeviceTransformKnownToBeIdentity)
        return roundedIntRect(rect);
    return cgRoundToDevicePixelsNonIdentity(deviceMatrix, rect);
}

void GraphicsContextCG::drawLinesForText(const FloatPoint& origin, float thickness, std::span<const FloatSegment> lineSegments, bool isPrinting, bool doubleLines, StrokeStyle strokeStyle)
{
    auto [rects, color] = computeRectsAndStrokeColorForLinesForText(origin, thickness, lineSegments, isPrinting, doubleLines, strokeStyle);
    if (rects.isEmpty())
        return;
    bool changeFillColor = fillColor() != color;
    if (changeFillColor)
        setCGFillColor(contextForDraw(), color, colorSpace());
    CGContextFillRects(contextForDraw(), rects.span().data(), rects.size());
    if (changeFillColor)
        setCGFillColor(contextForDraw(), fillColor(), colorSpace());
}

void GraphicsContextCG::setURLForRect(const URL& link, const FloatRect& destRect)
{
    RetainPtr<CFURLRef> urlRef = link.createCFURL();
    if (!urlRef)
        return;

    const RetainPtr<CGContextRef>& context = contextForDraw();

    FloatRect rect = destRect;
    // Get the bounding box to handle clipping.
    rect.intersect(CGContextGetClipBoundingBox(context));

    CGPDFContextSetURLForRect(context, urlRef.get(), CGRectApplyAffineTransform(rect, CGContextGetCTM(context)));
}

bool GraphicsContextCG::isCALayerContext() const
{
    return m_isLayerCGContext;
}

bool GraphicsContextCG::knownToHaveFloatBasedBacking() const
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (CGContextGetType(context) == kCGContextTypeIOSurface)
        return CGIOSurfaceContextGetBitmapInfo(context) & kCGBitmapFloatComponents;
    if (CGContextGetType(context) == kCGContextTypeBitmap)
        return CGBitmapContextGetBitmapInfo(context) & kCGBitmapFloatComponents;
    return false;
}

RenderingMode GraphicsContextCG::renderingMode() const
{
    return m_renderingMode;
}

void GraphicsContextCG::applyDeviceScaleFactor(float deviceScaleFactor)
{
    GraphicsContext::applyDeviceScaleFactor(deviceScaleFactor);

    // CoreGraphics expects the base CTM of a HiDPI context to have the scale factor applied to it.
    // Failing to change the base level CTM will cause certain CG features, such as focus rings,
    // to draw with a scale factor of 1 rather than the actual scale factor.
    CGContextSetBaseCTM(contextForDraw(), CGAffineTransformScale(CGContextGetBaseCTM(contextForDraw()), deviceScaleFactor, deviceScaleFactor));
}

void GraphicsContextCG::fillEllipse(const FloatRect& ellipse)
{
    // CGContextFillEllipseInRect only supports solid colors.
    if (fillGradient() || fillPattern()) {
        fillEllipseAsPath(ellipse);
        return;
    }

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextFillEllipseInRect(context, ellipse);
}

void GraphicsContextCG::strokeEllipse(const FloatRect& ellipse)
{
    // CGContextStrokeEllipseInRect only supports solid colors.
    if (strokeGradient() || strokePattern()) {
        strokeEllipseAsPath(ellipse);
        return;
    }

    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGContextStrokeEllipseInRect(context, ellipse);
}

void GraphicsContextCG::beginPage(const FloatRect& pageRect)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (CGContextGetType(context) != kCGContextTypePDF) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto mediaBox = CGRectMake(pageRect.x(), pageRect.y(), pageRect.width(), pageRect.height());
    auto mediaBoxData = adoptCF(CFDataCreate(nullptr, (const UInt8 *)&mediaBox, sizeof(CGRect)));

    const void* key = kCGPDFContextMediaBox;
    const void* value = mediaBoxData.get();
    auto pageInfo = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CGPDFContextBeginPage(context, pageInfo.get());

    ASSERT(!stackSize());
    m_state = GraphicsContextState { GraphicsContextState::basicChangeFlags, coreInterpolationQuality(context.get()) };
    didUpdateState(m_state);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextCG::endPage()
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    if (CGContextGetType(context) != kCGContextTypePDF) {
        ASSERT_NOT_REACHED();
        return;
    }

    CGPDFContextEndPage(context);
}

bool GraphicsContextCG::supportsInternalLinks() const
{
    return true;
}

void GraphicsContextCG::setDestinationForRect(const String& name, const FloatRect& destRect)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();

    FloatRect rect = destRect;
    rect.intersect(CGContextGetClipBoundingBox(context));

    CGRect transformedRect = CGRectApplyAffineTransform(rect, CGContextGetCTM(context));
    CGPDFContextSetDestinationForRect(context, name.createCFString().get(), transformedRect);
}

void GraphicsContextCG::addDestinationAtPoint(const String& name, const FloatPoint& position)
{
    const RetainPtr<CGContextRef>& context = contextForDraw();
    CGPoint transformedPoint = CGPointApplyAffineTransform(position, CGContextGetCTM(context));
    CGPDFContextAddDestinationAtPoint(context, name.createCFString().get(), transformedPoint);
}

bool GraphicsContextCG::canUseShadowBlur() const
{
    return (renderingMode() == RenderingMode::Unaccelerated) && hasBlurredDropShadow() && !m_state.shadowsIgnoreTransforms();
}

bool GraphicsContextCG::consumeHasDrawn()
{
    bool hasDrawn = m_hasDrawn;
    m_hasDrawn = false;
    return hasDrawn;
}

#if HAVE(SUPPORT_HDR_DISPLAY)
void GraphicsContextCG::setMaxEDRHeadroom(std::optional<float> headroom)
{
    m_maxEDRHeadroom = headroom;
}
#endif


}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/IOSurfaceAdditionsAfter.mm>)
#include <WebKitAdditions/IOSurfaceAdditionsAfter.mm>
#endif

#endif
