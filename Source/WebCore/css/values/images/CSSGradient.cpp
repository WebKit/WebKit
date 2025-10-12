/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSGradient.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveValueMappings.h"
#include "CalculationValue.h"
#include "ColorInterpolation.h"
#include "StyleGradientImage.h"
#include "StylePosition.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

// MARK: - Gradient Color Stop

template<typename C, typename P> static void colorStopSerializationForCSS(StringBuilder& builder, const SerializationContext& context, const GradientColorStop<C, P>& stop)
{
    if (stop.color && stop.position) {
        serializationForCSS(builder, context, *stop.color);
        builder.append(' ');
        serializationForCSS(builder, context, *stop.position);
    } else if (stop.color)
        serializationForCSS(builder, context, *stop.color);
    else if (stop.position)
        serializationForCSS(builder, context, *stop.position);
}

void Serialize<GradientAngularColorStop>::operator()(StringBuilder& builder, const SerializationContext& context, const GradientAngularColorStop& stop)
{
    colorStopSerializationForCSS(builder, context, stop);
}

void Serialize<GradientLinearColorStop>::operator()(StringBuilder& builder, const SerializationContext& context, const GradientLinearColorStop& stop)
{
    colorStopSerializationForCSS(builder, context, stop);
}

void Serialize<GradientDeprecatedColorStop>::operator()(StringBuilder& builder, const SerializationContext& context, const GradientDeprecatedColorStop& stop)
{
    auto appendRaw = [&](const auto& color, NumberRaw<> raw) {
        if (!raw.value) {
            builder.append("from("_s);
            serializationForCSS(builder, context, color);
            builder.append(')');
        } else if (raw.value == 1) {
            builder.append("to("_s);
            serializationForCSS(builder, context, color);
            builder.append(')');
        } else {
            builder.append("color-stop("_s);
            serializationForCSS(builder, context, raw);
            builder.append(", "_s);
            serializationForCSS(builder, context, color);
            builder.append(')');
        }
    };

    auto appendCalc = [&](const auto& color, const auto& calc) {
        builder.append("color-stop("_s);
        serializationForCSS(builder, context, calc);
        builder.append(", "_s);
        serializationForCSS(builder, context, color);
        builder.append(')');
    };

    WTF::switchOn(stop.position,
        [&](const Number<>& number) {
            return WTF::switchOn(number,
                [&](const Number<>::Raw& raw) {
                    appendRaw(stop.color, raw);
                },
                [&](const Number<>::Calc& calc) {
                    appendCalc(stop.color, calc);
                }
            );
        },
        [&](const Percentage<>& percentage) {
            return WTF::switchOn(percentage,
                [&](const Percentage<>::Raw& raw) {
                    appendRaw(stop.color, { raw.value / 100.0 });
                },
                [&](const Percentage<>::Calc& calc) {
                    appendCalc(stop.color, calc);
                }
            );
        }
    );
}

// MARK: - Gradient Color Interpolation

static bool appendColorInterpolationMethod(StringBuilder& builder, CSS::GradientColorInterpolationMethod colorInterpolationMethod, bool needsLeadingSpace)
{
    return WTF::switchOn(colorInterpolationMethod.method.colorSpace,
        [&](const ColorInterpolationMethod::OKLab&) {
            if (colorInterpolationMethod.defaultMethod != CSS::GradientColorInterpolationMethod::Default::OKLab) {
                builder.append(needsLeadingSpace ? " "_s : ""_s, "in oklab"_s);
                return true;
            }
            return false;
        },
        [&](const ColorInterpolationMethod::SRGB&) {
            if (colorInterpolationMethod.defaultMethod != CSS::GradientColorInterpolationMethod::Default::SRGB) {
                builder.append(needsLeadingSpace ? " "_s : ""_s, "in srgb"_s);
                return true;
            }
            return false;
        },
        [&]<typename MethodColorSpace>(const MethodColorSpace& methodColorSpace) {
            builder.append(needsLeadingSpace ? " "_s : ""_s, "in "_s, WebCore::serializationForCSS(methodColorSpace.interpolationColorSpace));
            if constexpr (hasHueInterpolationMethod<MethodColorSpace>)
                WebCore::serializationForCSS(builder, methodColorSpace.hueInterpolationMethod);
            return true;
        }
    );
}

// MARK: - LinearGradient

void Serialize<LinearGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const LinearGradient& gradient)
{
    bool wroteSomething = false;

    WTF::switchOn(gradient.gradientLine,
        [&](const Angle<>& angle) {
            WTF::switchOn(angle,
                [&](const Angle<>::Raw& angleRaw) {
                    if (convertToValueInUnitsOf<AngleUnit::Deg>(angleRaw) == 180)
                        return;

                    serializationForCSS(builder, context, angleRaw);
                    wroteSomething = true;
                },
                [&](const Angle<>::Calc& angleCalc) {
                    serializationForCSS(builder, context, angleCalc);
                    wroteSomething = true;
                }
            );
        },
        [&](const Horizontal& horizontal) {
            builder.append("to "_s);
            serializationForCSS(builder, context, horizontal);
            wroteSomething = true;
        },
        [&](const Vertical& vertical) {
            if (std::holds_alternative<Keyword::Bottom>(vertical))
                return;

            builder.append("to "_s);
            serializationForCSS(builder, context, vertical);
            wroteSomething = true;
        },
        [&](const SpaceSeparatedTuple<Horizontal, Vertical>& pair) {
            builder.append("to "_s);
            serializationForCSS(builder, context, pair);
            wroteSomething = true;
        }
    );

    if (appendColorInterpolationMethod(builder, gradient.colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, context, gradient.stops);
}

// MARK: - PrefixedLinearGradient

void Serialize<PrefixedLinearGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const PrefixedLinearGradient& gradient)
{
    serializationForCSS(builder, context, gradient.gradientLine);
    builder.append(", "_s);
    serializationForCSS(builder, context, gradient.stops);
}

// MARK: - DeprecatedLinearGradient

void Serialize<DeprecatedLinearGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const DeprecatedLinearGradient& gradient)
{
    builder.append("linear, "_s);

    serializationForCSS(builder, context, gradient.gradientLine);

    if (!gradient.stops.isEmpty()) {
        builder.append(", "_s);
        serializationForCSS(builder, context, gradient.stops);
    }
}

// MARK: - RadialGradient

void Serialize<RadialGradient::Ellipse>::operator()(StringBuilder& builder, const SerializationContext& context, const RadialGradient::Ellipse& ellipse)
{
    auto lengthBefore = builder.length();

    WTF::switchOn(ellipse.size,
        [&](const RadialGradient::Ellipse::Size& size) {
            serializationForCSS(builder, context, size);
        },
        [&](const RadialGradient::Extent& extent) {
            if (!std::holds_alternative<Keyword::FarthestCorner>(extent))
                serializationForCSS(builder, context, extent);
        }
    );

    if (ellipse.position) {
        if (!isCenterPosition(*ellipse.position)) {
            bool wroteSomething = builder.length() != lengthBefore;
            if (wroteSomething)
                builder.append(' ');

            builder.append("at "_s);
            serializationForCSS(builder, context, *ellipse.position);
        }
    }
}

void Serialize<RadialGradient::Circle>::operator()(StringBuilder& builder, const SerializationContext& context, const RadialGradient::Circle& circle)
{
    WTF::switchOn(circle.size,
        [&](const RadialGradient::Circle::Length& length) {
            serializationForCSS(builder, context, length);
        },
        [&](const RadialGradient::Extent& extent) {
            if (!std::holds_alternative<Keyword::FarthestCorner>(extent)) {
                builder.append("circle "_s);
                serializationForCSS(builder, context, extent);
            } else
                builder.append("circle"_s);
        }
    );

    if (circle.position) {
        if (!isCenterPosition(*circle.position)) {
            builder.append(" at "_s);
            serializationForCSS(builder, context, *circle.position);
        }
    }
}

void Serialize<RadialGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const RadialGradient& gradient)
{
    auto lengthBefore = builder.length();
    serializationForCSS(builder, context, gradient.gradientBox);
    bool wroteSomething = builder.length() != lengthBefore;

    if (appendColorInterpolationMethod(builder, gradient.colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, context, gradient.stops);
}

// MARK: - PrefixedRadialGradient

void Serialize<PrefixedRadialGradient::Ellipse>::operator()(StringBuilder& builder, const SerializationContext& context, const PrefixedRadialGradient::Ellipse& ellipse)
{
    if (ellipse.position)
        serializationForCSS(builder, context, *ellipse.position);
    else
        builder.append("center"_s);

    if (ellipse.size) {
        WTF::switchOn(*ellipse.size,
            [&](const PrefixedRadialGradient::Ellipse::Size& size) {
                builder.append(", "_s);
                serializationForCSS(builder, context, size);
            },
            [&](const PrefixedRadialGradient::Extent& extent) {
                builder.append(", ellipse "_s);
                serializationForCSS(builder, context, extent);
            }
        );
    }
}

void Serialize<PrefixedRadialGradient::Circle>::operator()(StringBuilder& builder, const SerializationContext& context, const PrefixedRadialGradient::Circle& circle)
{
    if (circle.position)
        serializationForCSS(builder, context, *circle.position);
    else
        builder.append("center"_s);

    builder.append(", circle "_s);
    serializationForCSS(builder, context, circle.size.value_or(PrefixedRadialGradient::Extent { CSS::Keyword::Cover { } }));
}

void Serialize<PrefixedRadialGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const PrefixedRadialGradient& gradient)
{
    auto lengthBefore = builder.length();
    serializationForCSS(builder, context, gradient.gradientBox);
    bool wroteSomething = builder.length() != lengthBefore;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, context, gradient.stops);
}

// MARK: - DeprecatedRadialGradient

void Serialize<DeprecatedRadialGradient::GradientBox>::operator()(StringBuilder& builder, const SerializationContext& context, const DeprecatedRadialGradient::GradientBox& gradientBox)
{
    serializationForCSS(builder, context, gradientBox.first);
    builder.append(", "_s);
    serializationForCSS(builder, context, gradientBox.firstRadius);
    builder.append(", "_s);
    serializationForCSS(builder, context, gradientBox.second);
    builder.append(", "_s);
    serializationForCSS(builder, context, gradientBox.secondRadius);
}

void Serialize<DeprecatedRadialGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const DeprecatedRadialGradient& gradient)
{
    builder.append("radial, "_s);

    serializationForCSS(builder, context, gradient.gradientBox);

    if (!gradient.stops.isEmpty()) {
        builder.append(", "_s);
        serializationForCSS(builder, context, gradient.stops);
    }
}

// MARK: - ConicGradient

void Serialize<ConicGradient::GradientBox>::operator()(StringBuilder& builder, const SerializationContext& context, const ConicGradient::GradientBox& gradientBox)
{
    bool wroteSomething = false;

    if (gradientBox.angle) {
        WTF::switchOn(*gradientBox.angle,
            [&](const Angle<>::Raw& angleRaw) {
                if (angleRaw.value) {
                    builder.append("from "_s);
                    serializationForCSS(builder, context, angleRaw);
                    wroteSomething = true;
                }
            },
            [&](const Angle<>::Calc& angleCalc) {
                builder.append("from "_s);
                serializationForCSS(builder, context, angleCalc);
                wroteSomething = true;
            }
        );
    }

    if (gradientBox.position && !isCenterPosition(*gradientBox.position)) {
        if (wroteSomething)
            builder.append(' ');
        builder.append("at "_s);
        serializationForCSS(builder, context, *gradientBox.position);
    }
}

void Serialize<ConicGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const ConicGradient& gradient)
{
    auto lengthBefore = builder.length();
    serializationForCSS(builder, context, gradient.gradientBox);
    bool wroteSomething = builder.length() != lengthBefore;

    if (appendColorInterpolationMethod(builder, gradient.colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, context, gradient.stops);
}

} // namespace CSS
} // namespace WebCore
