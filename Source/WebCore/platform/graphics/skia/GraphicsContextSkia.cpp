/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "GraphicsContextSkia.h"

#if USE(SKIA)
#include "AffineTransform.h"
#include "DecomposedGlyphs.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "GLContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformDisplay.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorFilter.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkPathEffect.h>
#include <skia/core/SkPathTypes.h>
#include <skia/core/SkPictureRecorder.h>
#include <skia/core/SkPoint3.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkRegion.h>
#include <skia/core/SkSurface.h>
#include <skia/core/SkTileMode.h>
#include <skia/effects/SkImageFilters.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/MathExtras.h>

#if USE(THEME_ADWAITA)
#include "Adwaita.h"
#endif

namespace WebCore {

GraphicsContextSkia::GraphicsContextSkia(SkCanvas& canvas, RenderingMode renderingMode, RenderingPurpose renderingPurpose, CompletionHandler<void()>&& destroyNotify)
    : m_canvas(canvas)
    , m_renderingMode(renderingMode)
    , m_renderingPurpose(renderingPurpose)
    , m_destroyNotify(WTFMove(destroyNotify))
    , m_colorSpace(canvas.imageInfo().colorSpace() ? DestinationColorSpace(canvas.imageInfo().refColorSpace()) : DestinationColorSpace::SRGB())
{
}

GraphicsContextSkia::~GraphicsContextSkia()
{
    if (m_destroyNotify)
        m_destroyNotify();
}

bool GraphicsContextSkia::hasPlatformContext() const
{
    return true;
}

AffineTransform GraphicsContextSkia::getCTM(IncludeDeviceScale includeScale) const
{
    UNUSED_PARAM(includeScale);
    return m_canvas.getTotalMatrix();
}

SkCanvas* GraphicsContextSkia::platformContext() const
{
    return &m_canvas;
}

const DestinationColorSpace& GraphicsContextSkia::colorSpace() const
{
    return m_colorSpace;
}

bool GraphicsContextSkia::makeGLContextCurrentIfNeeded() const
{
    if (m_renderingMode == RenderingMode::Unaccelerated || m_renderingPurpose != RenderingPurpose::Canvas)
        return true;

    return PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();
}

void GraphicsContextSkia::save(GraphicsContextState::Purpose purpose)
{
    GraphicsContext::save(purpose);
    m_skiaStateStack.append(m_skiaState);
    m_canvas.save();
}

void GraphicsContextSkia::restore(GraphicsContextState::Purpose purpose)
{
    if (!stackSize())
        return;

    GraphicsContext::restore(purpose);

    if (!m_skiaStateStack.isEmpty()) {
        m_skiaState = m_skiaStateStack.takeLast();
        if (m_skiaStateStack.isEmpty())
            m_skiaStateStack.clear();
    }

    m_canvas.restore();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextSkia::drawRect(const FloatRect& rect, float borderThickness)
{
    ASSERT(!rect.isEmpty());
    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    setupFillSource(paint);
    m_canvas.drawRect(rect, paint);
    if (strokeStyle() == StrokeStyle::NoStroke)
        return;

    SkIRect rects[4] = {
        SkIRect::MakeXYWH(rect.x(), rect.y(), rect.width(), borderThickness),
        SkIRect::MakeXYWH(rect.x(), rect.maxY() - borderThickness, rect.width(), borderThickness),
        SkIRect::MakeXYWH(rect.x(), rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness),
        SkIRect::MakeXYWH(rect.maxX() - borderThickness, rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness)
    };

    SkRegion region;
    region.setRects(rects, 4);
    SkPaint strokePaint = createStrokePaint();
    setupStrokeSource(strokePaint);
    m_canvas.drawRegion(region, strokePaint);
}

static SkBlendMode toSkiaBlendMode(CompositeOperator operation, BlendMode blendMode)
{
    switch (blendMode) {
    case BlendMode::Normal:
        switch (operation) {
        case CompositeOperator::Clear:
            return SkBlendMode::kClear;
        case CompositeOperator::Copy:
            return SkBlendMode::kSrc;
        case CompositeOperator::SourceOver:
            return SkBlendMode::kSrcOver;
        case CompositeOperator::SourceIn:
            return SkBlendMode::kSrcIn;
        case CompositeOperator::SourceOut:
            return SkBlendMode::kSrcOut;
        case CompositeOperator::SourceAtop:
            return SkBlendMode::kSrcATop;
        case CompositeOperator::DestinationOver:
            return SkBlendMode::kDstOver;
        case CompositeOperator::DestinationIn:
            return SkBlendMode::kDstIn;
        case CompositeOperator::DestinationOut:
            return SkBlendMode::kDstOut;
        case CompositeOperator::DestinationAtop:
            return SkBlendMode::kDstATop;
        case CompositeOperator::XOR:
            return SkBlendMode::kXor;
        case CompositeOperator::PlusLighter:
            return SkBlendMode::kPlus;
        case CompositeOperator::PlusDarker:
            notImplemented();
            return SkBlendMode::kSrcOver;
        case CompositeOperator::Difference:
            return SkBlendMode::kDifference;
        }
        break;
    case BlendMode::Multiply:
        return SkBlendMode::kMultiply;
    case BlendMode::Screen:
        return SkBlendMode::kScreen;
    case BlendMode::Overlay:
        return SkBlendMode::kOverlay;
    case BlendMode::Darken:
        return SkBlendMode::kDarken;
    case BlendMode::Lighten:
        return SkBlendMode::kLighten;
    case BlendMode::ColorDodge:
        return SkBlendMode::kColorDodge;
    case BlendMode::ColorBurn:
        return SkBlendMode::kColorBurn;
    case BlendMode::HardLight:
        return SkBlendMode::kHardLight;
    case BlendMode::SoftLight:
        return SkBlendMode::kSoftLight;
    case BlendMode::Difference:
        return SkBlendMode::kDifference;
    case BlendMode::Exclusion:
        return SkBlendMode::kExclusion;
    case BlendMode::Hue:
        return SkBlendMode::kHue;
    case BlendMode::Saturation:
        return SkBlendMode::kSaturation;
    case BlendMode::Color:
        return SkBlendMode::kColor;
    case BlendMode::Luminosity:
        return SkBlendMode::kLuminosity;
    case BlendMode::PlusLighter:
        return SkBlendMode::kPlus;
    case BlendMode::PlusDarker:
        notImplemented();
        break;
    }

    return SkBlendMode::kSrcOver;
}

static SkSamplingOptions toSkSamplingOptions(InterpolationQuality quality)
{
    switch (quality) {
    case InterpolationQuality::DoNotInterpolate:
        return SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);
    case InterpolationQuality::Low:
        return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone);
    case InterpolationQuality::Medium:
    case InterpolationQuality::Default:
        return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest);
    case InterpolationQuality::High:
        return SkSamplingOptions(SkCubicResampler::CatmullRom());
    }

    return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest);
}

void GraphicsContextSkia::drawNativeImageInternal(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    auto image = nativeImage.platformImage();
    if (!image)
        return;

    auto imageSize = nativeImage.size();
    if (options.orientation().usesWidthAsHeight())
        imageSize = imageSize.transposedSize();
    auto imageRect = FloatRect { { }, imageSize };
    auto normalizedSrcRect = normalizeRect(srcRect);
    if (!imageRect.intersects(normalizedSrcRect))
        return;

    if (options.orientation().usesWidthAsHeight())
        normalizedSrcRect = normalizedSrcRect.transposedRect();

    if (!makeGLContextCurrentIfNeeded())
        return;

    auto normalizedDestRect = normalizeRect(destRect);
    if (options.orientation() != ImageOrientation::Orientation::None) {
        m_canvas.save();

        // ImageOrientation expects the origin to be at (0, 0).
        m_canvas.translate(normalizedDestRect.x(), normalizedDestRect.y());
        normalizedDestRect.setLocation(FloatPoint());
        m_canvas.concat(options.orientation().transformFromDefault(normalizedDestRect.size()));

        if (options.orientation().usesWidthAsHeight()) {
            // The destination rectangle will have its width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            normalizedDestRect.setSize(normalizedDestRect.size().transposedSize());
        }
    }

    SkPaint paint = createFillPaint();
    paint.setAlphaf(alpha());
    paint.setBlendMode(toSkiaBlendMode(options.compositeOperator(), options.blendMode()));
    bool inExtraTransparencyLayer = false;
    auto clampingConstraint = options.strictImageClamping() == StrictImageClamping::Yes ? SkCanvas::kStrict_SrcRectConstraint : SkCanvas::kFast_SrcRectConstraint;
    if (hasDropShadow()) {
        if (image->isTextureBacked() && renderingMode() == RenderingMode::Unaccelerated) {
            // When drawing GPU-backed image on CPU-backed canvas with filter, we need to convert image to CPU-backed one.
            image = image->makeRasterImage();
        }
        inExtraTransparencyLayer = drawOutsetShadow(paint, [&](const SkPaint& paint) {
            m_canvas.drawImageRect(image, normalizedSrcRect, normalizedDestRect, toSkSamplingOptions(m_state.imageInterpolationQuality()), &paint, clampingConstraint);
        });
    }
    m_canvas.drawImageRect(image, normalizedSrcRect, normalizedDestRect, toSkSamplingOptions(m_state.imageInterpolationQuality()), &paint, clampingConstraint);
    if (inExtraTransparencyLayer)
        endTransparencyLayer();

    if (options.orientation() != ImageOrientation::Orientation::None)
        m_canvas.restore();
}

// This is only used to draw borders, so we should not draw shadows.
void GraphicsContextSkia::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == StrokeStyle::NoStroke)
        return;

    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createStrokePaint();
    paint.setColor(SkColor(strokeColor().colorWithAlphaMultipliedBy(alpha())));

    const bool isVertical = (point1.x() + strokeThickness() == point2.x());
    float strokeWidth = isVertical ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!strokeThickness() || !strokeWidth)
        return;

    float cornerWidth = 0;

    if (strokeStyle() == StrokeStyle::DottedStroke || strokeStyle() == StrokeStyle::DashedStroke) {
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth);
        if (isVertical) {
            fillRect(FloatRect(point1.x(), point1.y(), strokeThickness(), cornerWidth), strokeColor());
            fillRect(FloatRect(point1.x(), point2.y() - cornerWidth, strokeThickness(), cornerWidth), strokeColor());
        } else {
            fillRect(FloatRect(point1.x(), point1.y(), cornerWidth, strokeThickness()), strokeColor());
            fillRect(FloatRect(point2.x() - cornerWidth, point1.y(), cornerWidth, strokeThickness()), strokeColor());
        }
        strokeWidth -= 2 * cornerWidth;
        const float patternWidth = dashedLinePatternWidthForStrokeWidth(strokeWidth);
        // Check if corner drawing sufficiently covers the line.
        if (strokeWidth <= patternWidth + 1)
            return;

        const SkScalar dashIntervals[] = { SkFloatToScalar(patternWidth), SkFloatToScalar(patternWidth) };
        const float patternOffset = dashedLinePatternOffsetForPatternAndStrokeWidth(patternWidth, strokeWidth);
        paint.setPathEffect(SkDashPathEffect::Make(dashIntervals, 2, patternOffset));
    }

    const auto centeredPoints = centerLineAndCutOffCorners(isVertical, cornerWidth, point1, point2);
    const auto& centeredPoint1 = centeredPoints[0];
    const auto& centeredPoint2 = centeredPoints[1];

    m_canvas.drawLine(SkFloatToScalar(centeredPoint1.x()), SkFloatToScalar(centeredPoint1.y()), SkFloatToScalar(centeredPoint2.x()), SkFloatToScalar(centeredPoint2.y()), paint);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContextSkia::drawEllipse(const FloatRect& boundaries)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    setupFillSource(paint);
    m_canvas.drawOval(boundaries, paint);
}

static inline SkPathFillType toSkiaFillType(const WindRule& windRule)
{
    switch (windRule) {
    case WindRule::EvenOdd:
        return SkPathFillType::kEvenOdd;
    case WindRule::NonZero:
        return SkPathFillType::kWinding;
    }

    return SkPathFillType::kWinding;
}

void GraphicsContextSkia::drawSkiaPath(const SkPath& path, SkPaint& paint)
{
    bool inExtraTransparencyLayer = false;
    if (hasDropShadow()) {
        inExtraTransparencyLayer = drawOutsetShadow(paint, [&](const SkPaint& paint) {
            m_canvas.drawPath(path, paint);
        });
    }
    m_canvas.drawPath(path, paint);
    if (inExtraTransparencyLayer)
        endTransparencyLayer();
}

void GraphicsContextSkia::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;

    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    setupFillSource(paint);

    auto fillRule = toSkiaFillType(state().fillRule());
    auto& skiaPath= *path.platformPath();
    if (skiaPath.getFillType() == fillRule) {
        drawSkiaPath(skiaPath, paint);
        return;
    }

    auto skiaPathCopy = skiaPath;
    skiaPathCopy.setFillType(fillRule);
    drawSkiaPath(skiaPathCopy, paint);
}

void GraphicsContextSkia::strokePath(const Path& path)
{
    if (path.isEmpty())
        return;

    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint strokePaint = createStrokePaint();
    setupStrokeSource(strokePaint);
    drawSkiaPath(*path.platformPath(), strokePaint);
}

sk_sp<SkImageFilter> GraphicsContextSkia::createDropShadowFilterIfNeeded(ShadowStyle shadowStyle) const
{
    if (!hasDropShadow())
        return nullptr;

    const auto& shadow = dropShadow();
    ASSERT(shadow);

    auto offset = shadow->offset;
    const auto& shadowColor = shadow->color;

    if (!shadowColor.isVisible() || (!offset.width() && !offset.height() && !shadow->radius))
        return nullptr;

    const auto& state = this->state();
    auto sigma = shadow->radius / 2.0;

    if (shadowStyle == ShadowStyle::Inset) {
        auto dropShadow = SkImageFilters::DropShadowOnly(offset.width(), offset.height(), sigma, sigma, SK_ColorBLACK, nullptr);
        return SkImageFilters::ColorFilter(SkColorFilters::Blend(shadowColor, SkBlendMode::kSrcIn), dropShadow);
    }

    RELEASE_ASSERT(shadowStyle == ShadowStyle::Outset);

    if (!state.shadowsIgnoreTransforms())
        return SkImageFilters::DropShadowOnly(offset.width(), offset.height(), sigma, sigma, shadowColor, nullptr);

    // Fast path: identity CTM doesn't need the transform compensation
    AffineTransform ctm = getCTM(GraphicsContext::IncludeDeviceScale::PossiblyIncludeDeviceScale);
    if (ctm.isIdentity())
        return SkImageFilters::DropShadowOnly(offset.width(), offset.height(), sigma, sigma, shadowColor, nullptr);

    // Ignoring the CTM is practically equal as applying the inverse of
    // the CTM when post-processing the drop shadow.
    if (const std::optional<SkMatrix>& inverse = ctm.inverse()) {
        SkPoint3 p = SkPoint3::Make(offset.width(), offset.height(), 0);
        inverse->mapHomogeneousPoints(&p, &p, 1);
        sigma = inverse->mapRadius(sigma);
        return SkImageFilters::DropShadowOnly(p.x(), p.y(), sigma, sigma, shadowColor, nullptr);
    }

    return nullptr;
}

bool GraphicsContextSkia::drawOutsetShadow(SkPaint& paint, Function<void(const SkPaint&)>&& drawFunction)
{
    auto shadow = createDropShadowFilterIfNeeded(ShadowStyle::Outset);
    if (!shadow)
        return false;

    paint.setImageFilter(shadow);
    drawFunction(paint);
    paint.setImageFilter(nullptr);
    if (!m_layerStateStack.isEmpty()) {
        if (auto compositeMode = m_layerStateStack.last().compositeMode) {
            beginTransparencyLayer(compositeMode->operation, compositeMode->blendMode);
            return true;
        }
    }
    return false;
}

SkPaint GraphicsContextSkia::createFillPaint() const
{
    SkPaint paint;
    paint.setAntiAlias(shouldAntialias());
    paint.setStyle(SkPaint::kFill_Style);
    paint.setBlendMode(toSkiaBlendMode(compositeMode().operation, blendMode()));

    return paint;
}

void GraphicsContextSkia::setupFillSource(SkPaint& paint) const
{
    if (auto fillPattern = fillBrush().pattern()) {
        paint.setShader(fillPattern->createPlatformPattern({ }, toSkSamplingOptions(imageInterpolationQuality())));
        paint.setAlphaf(alpha());
    } else if (auto fillGradient = fillBrush().gradient())
        paint.setShader(fillGradient->shader(alpha(), fillBrush().gradientSpaceTransform()));
    else
        paint.setColor(SkColor(fillColor().colorWithAlphaMultipliedBy(alpha())));
}

SkPaint GraphicsContextSkia::createStrokePaint() const
{
    SkPaint paint;
    paint.setAntiAlias(shouldAntialias());
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setBlendMode(toSkiaBlendMode(compositeMode().operation, blendMode()));
    paint.setStrokeCap(m_skiaState.m_stroke.cap);
    paint.setStrokeJoin(m_skiaState.m_stroke.join);
    paint.setStrokeMiter(m_skiaState.m_stroke.miter);
    paint.setStrokeWidth(SkFloatToScalar(strokeThickness()));
    paint.setPathEffect(m_skiaState.m_stroke.dash);
    return paint;
}

void GraphicsContextSkia::setupStrokeSource(SkPaint& paint) const
{
    if (auto strokePattern = strokeBrush().pattern())
        paint.setShader(strokePattern->createPlatformPattern({ }, toSkSamplingOptions(imageInterpolationQuality())));
    else if (auto strokeGradient = strokeBrush().gradient())
        paint.setShader(strokeGradient->shader(alpha(), strokeBrush().gradientSpaceTransform()));
    else
        paint.setColor(SkColor(strokeBrush().color().colorWithAlphaMultipliedBy(alpha())));
}

void GraphicsContextSkia::drawSkiaRect(const SkRect& boundaries, SkPaint& paint)
{
    bool inExtraTransparencyLayer = false;
    if (hasDropShadow()) {
        inExtraTransparencyLayer = drawOutsetShadow(paint, [&](const SkPaint& paint) {
            m_canvas.drawRect(boundaries, paint);
        });
    }
    m_canvas.drawRect(boundaries, paint);
    if (inExtraTransparencyLayer)
        endTransparencyLayer();
}

void GraphicsContextSkia::fillRect(const FloatRect& boundaries, RequiresClipToRect)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    setupFillSource(paint);
    drawSkiaRect(boundaries, paint);
}

void GraphicsContextSkia::fillRect(const FloatRect& boundaries, const Color& fillColor)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    paint.setColor(SkColor(fillColor));
    drawSkiaRect(boundaries, paint);
}

void GraphicsContextSkia::fillRect(const FloatRect& boundaries, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    paint.setShader(gradient.shader(alpha(), gradientSpaceTransform));
    drawSkiaRect(boundaries, paint);
}

void GraphicsContextSkia::resetClip()
{
    notImplemented();
}

void GraphicsContextSkia::clip(const FloatRect& rect)
{
    m_canvas.clipRect(rect, SkClipOp::kIntersect, false);
}

void GraphicsContextSkia::clipPath(const Path& path, WindRule clipRule)
{
    auto fillRule = toSkiaFillType(clipRule);
    auto& skiaPath = *path.platformPath();
    if (skiaPath.getFillType() == fillRule) {
        m_canvas.clipPath(skiaPath, true);
        return;
    }

    auto skiaPathCopy = skiaPath;
    skiaPathCopy.setFillType(fillRule);
    m_canvas.clipPath(skiaPathCopy, true);
}

IntRect GraphicsContextSkia::clipBounds() const
{
    return enclosingIntRect(m_canvas.getLocalClipBounds());
}

void GraphicsContextSkia::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    if (auto nativeImage = nativeImageForDrawing(buffer))
        m_canvas.clipShader(nativeImage->platformImage()->makeShader(SkTileMode::kDecal, SkTileMode::kDecal, { }, SkMatrix::Translate(SkFloatToScalar(destRect.x()), SkFloatToScalar(destRect.y()))));
}

void GraphicsContextSkia::drawFocusRing(const Path& path, float, const Color& color)
{
#if USE(THEME_ADWAITA)
    Adwaita::paintFocus(*this, path, color);
#else
    notImplemented();
    UNUSED_PARAM(path);
    UNUSED_PARAM(color);
#endif
}

void GraphicsContextSkia::drawFocusRing(const Vector<FloatRect>& rects, float, float, const Color& color)
{
#if USE(THEME_ADWAITA)
    Adwaita::paintFocus(*this, rects, color);
#else
    notImplemented();
    UNUSED_PARAM(rects);
    UNUSED_PARAM(color);
#endif
}

void GraphicsContextSkia::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle strokeStyle)
{
    if (widths.isEmpty())
        return;

    Color localStrokeColor(strokeColor());
    FloatRect bounds = computeLineBoundsAndAntialiasingModeForText(FloatRect(point, FloatSize(widths.last(), thickness)), printing, localStrokeColor);
    if (bounds.isEmpty())
        return;

    Vector<FloatRect, 4> dashBounds;
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
            dashBounds.append({ bounds.x() + left, bounds.y(), width, bounds.height() });
        else {
            auto startParticle = static_cast<int>(std::ceil(left / (2 * dashWidth)));
            auto endParticle = static_cast<int>((left + width) / (2 * dashWidth));
            for (auto j = startParticle; j < endParticle; ++j)
                dashBounds.append({ bounds.x() + j * 2 * dashWidth, bounds.y(), dashWidth, bounds.height() });
        }
    }

    if (doubleUnderlines) {
        // The space between double underlines is equal to the height of the underline
        for (size_t i = 0; i < widths.size(); i += 2)
            dashBounds.append({ bounds.x() + widths[i], bounds.y() + 2 * bounds.height(), widths[i+1] - widths[i], bounds.height() });
    }

    for (auto& dash : dashBounds)
        fillRect(dash, localStrokeColor);
}

// Creates a path comprising of two triangle waves separated by some empty space in Y axis.
// The empty space can be filled using SkPaint::kFill_Style thus forming an elegant triangle wave.
// Such triangle wave can be used e.g. as an error underline for text.
static SkPath createErrorUnderlinePath(const FloatRect& boundaries)
{
    const double y = boundaries.y();
    double width = boundaries.width();
    const double height = boundaries.height();
    static const double heightSquares = 2.5;

    const double square = height / heightSquares;
    const double halfSquare = 0.5 * square;

    const double unitWidth = (heightSquares - 1.0) * square;
    const int widthUnits = static_cast<int>((width + 0.5 * unitWidth) / unitWidth);

    double x = boundaries.x() + 0.5 * (width - widthUnits * unitWidth);
    width = widthUnits * unitWidth;

    const double bottom = y + height;
    const double top = y;

    SkPath path;

    // Bottom triangle wave, left to right.
    path.moveTo(SkDoubleToScalar(x - halfSquare), SkDoubleToScalar(top + halfSquare));

    int i = 0;
    for (i = 0; i < widthUnits; i += 2) {
        const double middle = x + (i + 1) * unitWidth;
        const double right = x + (i + 2) * unitWidth;

        path.lineTo(SkDoubleToScalar(middle), SkDoubleToScalar(bottom));

        if (i + 2 == widthUnits)
            path.lineTo(SkDoubleToScalar(right + halfSquare), SkDoubleToScalar(top + halfSquare));
        else if (i + 1 != widthUnits)
            path.lineTo(SkDoubleToScalar(right), SkDoubleToScalar(top + square));
    }

    // Top triangle wave, right to left.
    for (i -= 2; i >= 0; i -= 2) {
        const double left = x + i * unitWidth;
        const double middle = x + (i + 1) * unitWidth;
        const double right = x + (i + 2) * unitWidth;

        if (i + 1 == widthUnits)
            path.lineTo(SkDoubleToScalar(middle + halfSquare), SkDoubleToScalar(bottom - halfSquare));
        else {
            if (i + 2 == widthUnits)
                path.lineTo(SkDoubleToScalar(right), SkDoubleToScalar(top));

            path.lineTo(SkDoubleToScalar(middle), SkDoubleToScalar(bottom - halfSquare));
        }

        path.lineTo(SkDoubleToScalar(left), SkDoubleToScalar(top));
    }

    return path;
}

void GraphicsContextSkia::drawDotsForDocumentMarker(const FloatRect& boundaries, DocumentMarkerLineStyle style)
{
    if (style.mode != DocumentMarkerLineStyleMode::Spelling
        && style.mode != DocumentMarkerLineStyleMode::Grammar)
        return;

    SkPaint paint = createFillPaint();
    paint.setColor(SkColor(style.color));
    m_canvas.drawPath(createErrorUnderlinePath(boundaries), paint);
}

void GraphicsContextSkia::translate(float x, float y)
{
    m_canvas.translate(SkFloatToScalar(x), SkFloatToScalar(y));
}

void GraphicsContextSkia::didUpdateState(GraphicsContextState& state)
{
    // FIXME: Handle stroke changes.
    state.didApplyChanges();
}

void GraphicsContextSkia::concatCTM(const AffineTransform& ctm)
{
    m_canvas.concat(ctm);
}

void GraphicsContextSkia::setCTM(const AffineTransform& ctm)
{
    m_canvas.setMatrix(ctm);
}

void GraphicsContextSkia::beginTransparencyLayer(float opacity)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    GraphicsContext::beginTransparencyLayer(opacity);
    m_layerStateStack.append({ });

    SkPaint paint;
    paint.setAlphaf(opacity);
    paint.setBlendMode(toSkiaBlendMode(m_state.compositeMode().operation, m_state.compositeMode().blendMode));
    m_canvas.saveLayer(nullptr, &paint);
}

void GraphicsContextSkia::beginTransparencyLayer(CompositeOperator operation, BlendMode blendMode)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    GraphicsContext::beginTransparencyLayer(operation, blendMode);
    m_layerStateStack.append({ CompositeMode(operation, blendMode) });

    SkPaint paint;
    paint.setBlendMode(toSkiaBlendMode(operation, blendMode));
    m_canvas.saveLayer(nullptr, &paint);
    // When on transparency layer, we don't want to blend operations as when layer ends, we blend it as a whole.
    setCompositeMode({ CompositeOperator::SourceOver, BlendMode::Normal });
}

void GraphicsContextSkia::endTransparencyLayer()
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    GraphicsContext::endTransparencyLayer();
    m_canvas.restore();
    if (!m_layerStateStack.isEmpty()) {
        auto layerState = m_layerStateStack.takeLast();
        if (layerState.compositeMode)
            setCompositeMode(*layerState.compositeMode);
    }
}

void GraphicsContextSkia::clearRect(const FloatRect& rect)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    auto paint = createFillPaint();
    paint.setBlendMode(SkBlendMode::kClear);
    m_canvas.drawRect(rect, paint);
}

void GraphicsContextSkia::strokeRect(const FloatRect& boundaries, float lineWidth)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    auto strokePaint = createStrokePaint();
    strokePaint.setStrokeWidth(SkFloatToScalar(lineWidth));
    setupStrokeSource(strokePaint);
    drawSkiaRect(boundaries, strokePaint);
}

void GraphicsContextSkia::setLineCap(LineCap lineCap)
{
    auto toSkiaCap = [](const LineCap& lineCap) -> SkPaint::Cap {
        switch (lineCap) {
        case LineCap::Butt:
            return SkPaint::Cap::kButt_Cap;
        case LineCap::Round:
            return SkPaint::Cap::kRound_Cap;
        case LineCap::Square:
            return SkPaint::Cap::kSquare_Cap;
        }

        return SkPaint::Cap::kDefault_Cap;
    };

    m_skiaState.m_stroke.cap = toSkiaCap(lineCap);
}

void GraphicsContextSkia::setLineDash(const DashArray& dashArray, float dashOffset)
{
    if (dashArray.isEmpty()) {
        m_skiaState.m_stroke.dash = nullptr;
        return;
    }

    if (dashArray.size() % 2 == 1) {
        // Repeat the array to ensure even number of dash array elements, see e.g. 'stroke-dasharray' spec.
        DashArray repeatedDashArray(dashArray);
        repeatedDashArray.appendVector(dashArray);
        m_skiaState.m_stroke.dash = SkDashPathEffect::Make(repeatedDashArray.data(), repeatedDashArray.size(), dashOffset);
    } else
        m_skiaState.m_stroke.dash = SkDashPathEffect::Make(dashArray.data(), dashArray.size(), dashOffset);
}

void GraphicsContextSkia::setLineJoin(LineJoin lineJoin)
{
    auto toSkiaJoin = [](const LineJoin& lineJoin) -> SkPaint::Join {
        switch (lineJoin) {
        case LineJoin::Miter:
            return SkPaint::Join::kMiter_Join;
        case LineJoin::Round:
            return SkPaint::Join::kRound_Join;
        case LineJoin::Bevel:
            return SkPaint::Join::kBevel_Join;
        }

        return SkPaint::Join::kDefault_Join;
    };

    m_skiaState.m_stroke.join = toSkiaJoin(lineJoin);
}

void GraphicsContextSkia::setMiterLimit(float miter)
{
    m_skiaState.m_stroke.miter = SkFloatToScalar(miter);
}

void GraphicsContextSkia::clipOut(const Path& path)
{
    auto& skiaPath = *path.platformPath();
    skiaPath.toggleInverseFillType();
    m_canvas.clipPath(skiaPath, true);
    skiaPath.toggleInverseFillType();
}

void GraphicsContextSkia::rotate(float radians)
{
    m_canvas.rotate(SkFloatToScalar(rad2deg(radians)));
}

void GraphicsContextSkia::scale(const FloatSize& scale)
{
    m_canvas.scale(SkFloatToScalar(scale.width()), SkFloatToScalar(scale.height()));
}

void GraphicsContextSkia::clipOut(const FloatRect& rect)
{
    m_canvas.clipRect(rect, SkClipOp::kDifference, false);
}

void GraphicsContextSkia::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    paint.setColor(SkColor(color));
    bool inExtraTransparencyLayer = false;
    if (hasDropShadow()) {
        inExtraTransparencyLayer = drawOutsetShadow(paint, [&](const SkPaint& paint) {
            m_canvas.drawRRect(rect, paint);
        });
    }
    m_canvas.drawRRect(rect, paint);
    if (inExtraTransparencyLayer)
        endTransparencyLayer();
}

void GraphicsContextSkia::fillRectWithRoundedHole(const FloatRect& outerRect, const FloatRoundedRect& innerRRect, const Color& color)
{
    if (!color.isValid())
        return;

    if (!makeGLContextCurrentIfNeeded())
        return;

    SkPaint paint = createFillPaint();
    paint.setColor(SkColor(color));
    paint.setImageFilter(createDropShadowFilterIfNeeded(ShadowStyle::Inset));
    m_canvas.drawDRRect(SkRRect::MakeRect(outerRect), innerRRect, paint);
}

void GraphicsContextSkia::drawPattern(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    if (!patternTransform.isInvertible())
        return;

    auto image = nativeImage.platformImage();
    if (!image)
        return;

    if (!makeGLContextCurrentIfNeeded())
        return;

    FloatPoint phaseOffset(tileRect.location());
    phaseOffset.scale(patternTransform.a(), patternTransform.d());
    phaseOffset.moveBy(phase);
    SkMatrix phaseMatrix;
    phaseMatrix.setTranslate(phaseOffset.x(), phaseOffset.y());
    SkMatrix shaderMatrix = SkMatrix::Concat(phaseMatrix, patternTransform);
    auto samplingOptions = toSkSamplingOptions(m_state.imageInterpolationQuality());

    SkPaint paint = createFillPaint();
    paint.setBlendMode(toSkiaBlendMode(options.compositeOperator(), options.blendMode()));

    if (spacing.isZero() && tileRect.size() == nativeImage.size()) {
        // Check whether we're sampling the pattern beyond the image size. If this is the case, we need to set the repeat
        // flag when sampling. Otherwise we use the clamp flag. This is done to avoid a situation where the pattern is scaled
        // to fit perfectly the destinationRect, but if we use the repeat flag in that case the edges are wrong because the
        // scaling interpolation is using pixels from the other end of the image.
        bool repeatX = true;
        bool repeatY = true;
        SkMatrix inverse;
        if (shaderMatrix.invert(&inverse)) {
            SkRect imageSampledRect;
            inverse.mapRect(&imageSampledRect, SkRect::MakeXYWH(destRect.x(), destRect.y(), destRect.width(), destRect.height()));
            repeatX = imageSampledRect.x() < 0 || std::trunc(imageSampledRect.right()) > nativeImage.size().width();
            repeatY = imageSampledRect.y() < 0 || std::trunc(imageSampledRect.bottom()) > nativeImage.size().height();
        }
        paint.setShader(image->makeShader(repeatX ? SkTileMode::kRepeat : SkTileMode::kClamp, repeatY ? SkTileMode::kRepeat : SkTileMode::kClamp, samplingOptions, &shaderMatrix));
    } else {
        SkPictureRecorder recorder;
        auto* recordCanvas = recorder.beginRecording(SkRect::MakeWH(tileRect.width() + spacing.width() / patternTransform.a(), tileRect.height() + spacing.height() / patternTransform.d()));
        // The below call effectively extracts a tile from the image thus performing a clipping.
        recordCanvas->drawImageRect(image, tileRect, SkRect::MakeWH(tileRect.width(), tileRect.height()), samplingOptions, nullptr, SkCanvas::kStrict_SrcRectConstraint);
        auto picture = recorder.finishRecordingAsPicture();
        paint.setShader(picture->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat, samplingOptions.filter, &shaderMatrix, nullptr));
    }

    m_canvas.drawRect(destRect, paint);
}

void GraphicsContextSkia::drawSkiaText(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, bool enableAntialias, bool isVertical)
{
    if (isVertical) {
        m_canvas.save();

        SkMatrix matrix;
        matrix.setSinCos(-1, 0, x, y);
        m_canvas.concat(matrix);
    }

    if (textDrawingMode().contains(TextDrawingMode::Fill)) {
        SkPaint paint = createFillPaint();
        setupFillSource(paint);
        paint.setAntiAlias(enableAntialias);
        bool inExtraTransparencyLayer = false;
        if (hasDropShadow()) {
            inExtraTransparencyLayer = drawOutsetShadow(paint, [&](const SkPaint& paint) {
                m_canvas.drawTextBlob(blob, x, y, paint);
            });
        }
        m_canvas.drawTextBlob(blob, x, y, paint);
        if (inExtraTransparencyLayer)
            endTransparencyLayer();
    }

    if (textDrawingMode().contains(TextDrawingMode::Stroke)) {
        SkPaint paint = createStrokePaint();
        setupStrokeSource(paint);
        paint.setAntiAlias(enableAntialias);
        m_canvas.drawTextBlob(blob, x, y, paint);
    }

    if (isVertical)
        m_canvas.restore();
}

RenderingMode GraphicsContextSkia::renderingMode() const
{
    return m_renderingMode;
}

} // namespace WebCore

#endif // USE(SKIA)
