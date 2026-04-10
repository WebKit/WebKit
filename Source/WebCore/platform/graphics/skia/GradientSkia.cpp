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
#include "Gradient.h"

#if USE(SKIA)

#include "AffineTransform.h"
#include "GradientColorStops.h"
#include "GraphicsContextSkia.h"
#include "NotImplemented.h"
#include <numbers>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColor.h>
#include <skia/core/SkPathBuilder.h>
#include <skia/core/SkScalar.h>
#include <skia/effects/SkGradient.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <span>

namespace WebCore {

void Gradient::stopsChanged()
{
}

static void fillSolidBandsSkia(SkCanvas& canvas, float left, float length, const SkRect& clipBounds, std::span<const Gradient::SolidBand> bands, float globalAlpha)
{
    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setStyle(SkPaint::kFill_Style);

    for (auto& band : bands) {
        float bandStart = left + band.startOffset * length;
        float bandEnd = left + band.endOffset * length;
        if (bandStart > bandEnd)
            std::swap(bandStart, bandEnd);
        bandStart = std::max(bandStart, clipBounds.fLeft);
        bandEnd = std::min(bandEnd, clipBounds.fRight);
        if (bandStart >= bandEnd)
            continue;
        SkColor4f color = static_cast<SkColor4f>(band.color);
        color.fA *= globalAlpha;
        paint.setColor4f(color);
        canvas.drawRect(SkRect::MakeLTRB(bandStart, clipBounds.fTop, bandEnd, clipBounds.fBottom), paint);
    }
}

static bool paintLinearSolidBands(SkCanvas& canvas, FloatPoint point0, FloatPoint point1, const Vector<Gradient::SolidBand>& bands, float globalAlpha)
{
    if (bands.isEmpty())
        return false;

    FloatSize gradientVector = point1 - point0;
    // Check axis-alignment in screen space by transforming the gradient vector through the CTM.
    SkMatrix ctm = canvas.getTotalMatrix();
    SkPoint screenVector = ctm.mapVector(gradientVector.width(), gradientVector.height());
    bool isAxisAligned = !screenVector.fX || !screenVector.fY;
    if (!isAxisAligned)
        return false;

    canvas.save();

    float gradientLength = gradientVector.diagonalLength();
    canvas.translate(point0.x(), point0.y());
    if (gradientLength > 0)
        canvas.rotate(SkRadiansToDegrees(FloatPoint(gradientVector).slopeAngleRadians()));

    SkRect clipBounds;
    if (!canvas.getLocalClipBounds(&clipBounds) || clipBounds.isEmpty()) {
        canvas.restore();
        return true;
    }

    float padStart = gradientLength > 0 ? clipBounds.fLeft / gradientLength : 0;
    float padEnd = gradientLength > 0 ? clipBounds.fRight / gradientLength : 1;
    padStart = std::min(padStart, bands.first().startOffset);
    padEnd = std::max(padEnd, bands.last().endOffset);

    if (padStart < bands.first().startOffset) {
        Gradient::SolidBand leadingPad { padStart, bands.first().startOffset, bands.first().color };
        fillSolidBandsSkia(canvas, 0, gradientLength, clipBounds, { &leadingPad, 1 }, globalAlpha);
    }

    fillSolidBandsSkia(canvas, 0, gradientLength, clipBounds, bands, globalAlpha);

    if (padEnd > bands.last().endOffset) {
        Gradient::SolidBand trailingPad { bands.last().endOffset, padEnd, bands.last().color };
        fillSolidBandsSkia(canvas, 0, gradientLength, clipBounds, { &trailingPad, 1 }, globalAlpha);
    }

    canvas.restore();
    return true;
}

static bool paintConicSolidBands(SkCanvas& canvas, const Gradient::ConicData& data, const Vector<Gradient::SolidBand>& bands, float globalAlpha)
{
    if (bands.isEmpty())
        return false;

    canvas.save();

    SkRect clipBounds;
    if (!canvas.getLocalClipBounds(&clipBounds) || clipBounds.isEmpty()) {
        canvas.restore();
        return true;
    }

    float dx = std::max(std::abs(clipBounds.fLeft - data.point0.x()), std::abs(clipBounds.fRight - data.point0.x()));
    float dy = std::max(std::abs(clipBounds.fTop - data.point0.y()), std::abs(clipBounds.fBottom - data.point0.y()));
    float radius = std::hypot(dx, dy);

    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setStyle(SkPaint::kFill_Style);

    float startDegrees = SkRadiansToDegrees(data.angleRadians) - 90.0f;
    SkRect oval = SkRect::MakeLTRB(
        data.point0.x() - radius, data.point0.y() - radius,
        data.point0.x() + radius, data.point0.y() + radius);

    auto drawSector = [&](float startOffset, float endOffset, const Color& bandColor) {
        float startAngle = startDegrees + startOffset * 360.0f;
        float sweepAngle = (endOffset - startOffset) * 360.0f;
        SkColor4f color = static_cast<SkColor4f>(bandColor);
        color.fA *= globalAlpha;
        paint.setColor4f(color);

        SkPathBuilder pathBuilder;
        pathBuilder.moveTo(data.point0.x(), data.point0.y());
        pathBuilder.arcTo(oval, startAngle, sweepAngle, false);
        pathBuilder.close();
        canvas.drawPath(pathBuilder.detach(), paint);
    };

    if (bands.first().startOffset > 0)
        drawSector(0, bands.first().startOffset, bands.first().color);

    for (auto& band : bands)
        drawSector(band.startOffset, band.endOffset, band.color);

    if (bands.last().endOffset < 1)
        drawSector(bands.last().endOffset, 1, bands.last().color);

    canvas.restore();
    return true;
}

inline SkScalar webCoreDoubleToSkScalar(double d)
{
    return SkDoubleToScalar(std::isfinite(d) ? d : 0);
}

static SkGradient::Interpolation toSkiaInterpolation(const ColorInterpolationMethod& method)
{
    SkGradient::Interpolation interpolation;

    WTF::switchOn(method.colorSpace,
        [&] (const ColorInterpolationMethod::HSL&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kHSL;
        },
        [&] (const ColorInterpolationMethod::HWB&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kHWB;
        },
        [&] (const ColorInterpolationMethod::LCH&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kLCH;
        },
        [&] (const ColorInterpolationMethod::Lab&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kLab;
        },
        [&] (const ColorInterpolationMethod::OKLCH&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kOKLCH;
        },
        [&] (const ColorInterpolationMethod::OKLab&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kOKLab;
        },
        [&] (const ColorInterpolationMethod::SRGB&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kSRGB;
        },
        [&] (const ColorInterpolationMethod::SRGBLinear&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kSRGBLinear;
        },
        [&] (const ColorInterpolationMethod::DisplayP3&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kDisplayP3;
        },
        [&] (const ColorInterpolationMethod::A98RGB&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kA98RGB;
        },
        [&] (const ColorInterpolationMethod::ProPhotoRGB&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kProphotoRGB;
        },
        [&] (const ColorInterpolationMethod::Rec2020&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kRec2020;
        },
        [&] (const ColorInterpolationMethod::XYZD50&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kSRGBLinear;
        },
        [&] (const ColorInterpolationMethod::XYZD65&) {
            interpolation.fColorSpace = SkGradient::Interpolation::ColorSpace::kSRGBLinear;
        },
        [&] (const auto&) {
            // FIXME: Support other color spaces once skia has support for them.
        });

    WTF::switchOn(method.colorSpace,
        [&]<typename ColorSpace> (const ColorSpace& colorSpace) {
            if constexpr (hasHueInterpolationMethod<ColorSpace>) {
                switch (colorSpace.hueInterpolationMethod) {
                case HueInterpolationMethod::Shorter:
                    interpolation.fHueMethod = SkGradient::Interpolation::HueMethod::kShorter;
                    break;
                case HueInterpolationMethod::Longer:
                    interpolation.fHueMethod = SkGradient::Interpolation::HueMethod::kLonger;
                    break;
                case HueInterpolationMethod::Increasing:
                    interpolation.fHueMethod = SkGradient::Interpolation::HueMethod::kIncreasing;
                    break;
                case HueInterpolationMethod::Decreasing:
                    interpolation.fHueMethod = SkGradient::Interpolation::HueMethod::kDecreasing;
                    break;
                }
            }
        }
    );

    switch (method.alphaPremultiplication) {
    case AlphaPremultiplication::Premultiplied:
        interpolation.fInPremul = SkGradient::Interpolation::InPremul::kYes;
        break;
    case AlphaPremultiplication::Unpremultiplied:
        interpolation.fInPremul = SkGradient::Interpolation::InPremul::kNo;
        break;
    }

    return interpolation;
}

sk_sp<SkShader> Gradient::shader(float globalAlpha, const AffineTransform& gradientSpaceTransform)
{
    auto interpolation = toSkiaInterpolation(colorInterpolationMethod());

    Vector<SkColor4f, 8> colors;
    colors.reserveInitialCapacity(stops().size());
    Vector<SkScalar, 8> positions;
    positions.reserveInitialCapacity(stops().size());
    auto fillStops = [&](const GradientColorStops::StopVector& stops) {
        if (stops.isEmpty()) {
            positions.append(webCoreDoubleToSkScalar(0));
            colors.append(SkColors::kTransparent);
        } else if (stops.begin()->offset > 0 && interpolation.fHueMethod != SkGradient::Interpolation::HueMethod::kLonger) {
            positions.append(webCoreDoubleToSkScalar(0));
            colors.append(stops.begin()->color.colorWithAlphaMultipliedBy(globalAlpha));
        }

        for (size_t i = 0; i < stops.size(); i++) {
            positions.append(webCoreDoubleToSkScalar(stops[i].offset));
            colors.append(stops[i].color.colorWithAlphaMultipliedBy(globalAlpha));
        }

        if (positions.last() < 1 && interpolation.fHueMethod != SkGradient::Interpolation::HueMethod::kLonger) {
            positions.append(webCoreDoubleToSkScalar(1));
            colors.append(colors.last());
        }
    };
    fillStops(stops().sorted().stops());

    SkTileMode tileMode = SkTileMode::kClamp;
    switch (m_spreadMethod) {
    case GradientSpreadMethod::Pad:
        tileMode = SkTileMode::kClamp;
        break;
    case GradientSpreadMethod::Reflect:
        tileMode = SkTileMode::kMirror;
        break;
    case GradientSpreadMethod::Repeat:
        tileMode = SkTileMode::kRepeat;
        break;
    }

    SkMatrix matrix = gradientSpaceTransform;

    auto shader = WTF::switchOn(
        m_data,
        [&](const LinearData& data) {
            SkPoint points[] = { SkPoint::Make(data.point0.x(), data.point0.y()), SkPoint::Make(data.point1.x(), data.point1.y()) };

            return SkShaders::LinearGradient(points, { { colors.span(), positions.span(), tileMode }, interpolation }, &matrix);
        },
        [&](const RadialData& data) {
            if (data.aspectRatio != 1)
                matrix.preScale(1, 1 / data.aspectRatio, data.point0.x(), data.point0.y());

            SkPoint start = SkPoint::Make(data.point0.x(), data.point0.y());
            SkPoint end = SkPoint::Make(data.point1.x(), data.point1.y());
            SkScalar startRadius = std::max(webCoreDoubleToSkScalar(data.startRadius), 0.0f);
            SkScalar endRadius = std::max(webCoreDoubleToSkScalar(data.endRadius), 0.0f);

            return SkShaders::TwoPointConicalGradient(start, startRadius, end, endRadius, { { colors.span(), positions.span(), tileMode }, interpolation }, &matrix);
        },
        [&](const ConicData& data) {
            // Skia's renders it tilted by 90 degrees, so offset that rotation in the matrix
            matrix.preRotate(SkRadiansToDegrees(data.angleRadians) - 90.0f, data.point0.x(), data.point0.y());

            return SkShaders::SweepGradient(SkPoint::Make(data.point0.x(), data.point0.y()), { { colors.span(), positions.span(), tileMode }, interpolation }, &matrix);
        });

    return shader;
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    SkCanvas* canvas = context.platformContext();

    const auto& bands = solidBands();
    if (!bands.isEmpty()) {
        bool handled = WTF::switchOn(m_data,
            [&] (const LinearData& data) {
                if (m_spreadMethod != GradientSpreadMethod::Pad)
                    return false;
                canvas->save();
                canvas->clipRect(rect);
                bool result = paintLinearSolidBands(*canvas, data.point0, data.point1, bands, context.alpha());
                canvas->restore();
                return result;
            },
            [&] (const ConicData& data) {
                canvas->save();
                canvas->clipRect(rect);
                bool result = paintConicSolidBands(*canvas, data, bands, context.alpha());
                canvas->restore();
                return result;
            },
            [&] (const RadialData&) { return false; }
        );
        if (handled)
            return;
    }

    auto paint = static_cast<GraphicsContextSkia*>(&context)->createFillPaint();
    paint.setShader(shader(context.alpha(), context.fillGradientSpaceTransform()));
    canvas->drawRect(rect, paint);
}

} // namespace WebCore

#endif // USE(SKIA)
