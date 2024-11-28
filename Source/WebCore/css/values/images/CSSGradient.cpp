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
#include "StyleBuilderConverter.h"
#include "StyleGradientImage.h"
#include "StylePosition.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

// MARK: - Gradient Color Stop

template<typename T> static void colorStopSerializationForCSS(StringBuilder& builder, const GradientColorStop<T>& stop)
{
    if (stop.color && stop.position) {
        builder.append(stop.protectedColor()->cssText(), ' ');
        serializationForCSS(builder, *stop.position);
    } else if (stop.color)
        builder.append(stop.protectedColor()->cssText());
    else if (stop.position)
        serializationForCSS(builder, *stop.position);
}

void Serialize<GradientAngularColorStop>::operator()(StringBuilder& builder, const GradientAngularColorStop& stop)
{
    colorStopSerializationForCSS(builder, stop);
}

void Serialize<GradientLinearColorStop>::operator()(StringBuilder& builder, const GradientLinearColorStop& stop)
{
    colorStopSerializationForCSS(builder, stop);
}

void Serialize<GradientDeprecatedColorStop>::operator()(StringBuilder& builder, const GradientDeprecatedColorStop& stop)
{
    auto appendRaw = [&](const auto& color, NumberRaw<> raw) {
        if (!raw.value)
            builder.append("from("_s, color->cssText(), ')');
        else if (raw.value == 1)
            builder.append("to("_s, color->cssText(), ')');
        else {
            builder.append("color-stop("_s);
            serializationForCSS(builder, raw);
            builder.append(", "_s, color->cssText(), ')');
        }
    };

    auto appendCalc = [&](const auto& calc) {
        builder.append("color-stop("_s, calc.protectedCalc()->cssText(), ", "_s, stop.protectedColor()->cssText(), ')');
    };

    WTF::switchOn(stop.position,
        [&](const Number<>& number) {
            return WTF::switchOn(number,
                [&](NumberRaw<> raw) {
                    appendRaw(stop.color, raw);
                },
                [&](const UnevaluatedCalc<NumberRaw<>>& calc) {
                    appendCalc(calc);
                }
            );
        },
        [&](const Percentage<>& percentage) {
            return WTF::switchOn(percentage,
                [&](PercentageRaw<> raw) {
                    appendRaw(stop.color, { raw.value / 100.0 });
                },
                [&](const UnevaluatedCalc<PercentageRaw<>>& calc) {
                    appendCalc(calc);
                }
            );
        }
    );
}

template<typename T> static void collectComputedStyleDependenciesOnStop(ComputedStyleDependencies& dependencies, const GradientColorStop<T>& stop)
{
    if (RefPtr color = stop.color)
        color->collectComputedStyleDependencies(dependencies);
    collectComputedStyleDependencies(dependencies, stop.position);
}

void ComputedStyleDependenciesCollector<GradientAngularColorStop>::operator()(ComputedStyleDependencies& dependencies, const GradientAngularColorStop& stop)
{
    collectComputedStyleDependenciesOnStop(dependencies, stop);
}

void ComputedStyleDependenciesCollector<GradientLinearColorStop>::operator()(ComputedStyleDependencies& dependencies, const GradientLinearColorStop& stop)
{
    collectComputedStyleDependenciesOnStop(dependencies, stop);
}

void ComputedStyleDependenciesCollector<GradientDeprecatedColorStop>::operator()(ComputedStyleDependencies& dependencies, const GradientDeprecatedColorStop& stop)
{
    collectComputedStyleDependenciesOnStop(dependencies, stop);
}

template<typename T> static IterationStatus visitCSSValueChildrenOnColorStop(const Function<IterationStatus(CSSValue&)>& func, const GradientColorStop<T>& stop)
{
    if (RefPtr color = stop.color) {
        if (func(*color) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return visitCSSValueChildren(func, stop.position);
}

IterationStatus CSSValueChildrenVisitor<GradientAngularColorStop>::operator()(const Function<IterationStatus(CSSValue&)>& func, const GradientAngularColorStop& stop)
{
    return visitCSSValueChildrenOnColorStop(func, stop);
}

IterationStatus CSSValueChildrenVisitor<GradientLinearColorStop>::operator()(const Function<IterationStatus(CSSValue&)>& func, const GradientLinearColorStop& stop)
{
    return visitCSSValueChildrenOnColorStop(func, stop);
}

IterationStatus CSSValueChildrenVisitor<GradientDeprecatedColorStop>::operator()(const Function<IterationStatus(CSSValue&)>& func, const GradientDeprecatedColorStop& stop)
{
    return visitCSSValueChildrenOnColorStop(func, stop);
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
        [&]<typename MethodColorSpace> (const MethodColorSpace& methodColorSpace) {
            builder.append(needsLeadingSpace ? " "_s : ""_s, "in "_s, serializationForCSS(methodColorSpace.interpolationColorSpace));
            if constexpr (hasHueInterpolationMethod<MethodColorSpace>)
                serializationForCSS(builder, methodColorSpace.hueInterpolationMethod);
            return true;
        }
    );
}

// MARK: - LinearGradient

void Serialize<LinearGradient>::operator()(StringBuilder& builder, const LinearGradient& gradient)
{
    bool wroteSomething = false;

    WTF::switchOn(gradient.gradientLine,
        [&](const Angle<>& angle) {
            WTF::switchOn(angle,
                [&](const AngleRaw<>& angleRaw) {
                    if (CSSPrimitiveValue::computeDegrees(angleRaw.type, angleRaw.value) == 180)
                        return;

                    serializationForCSS(builder, angleRaw);
                    wroteSomething = true;
                },
                [&](const UnevaluatedCalc<AngleRaw<>>& angleCalc) {
                    serializationForCSS(builder, angleCalc);
                    wroteSomething = true;
                }
            );
        },
        [&](const Horizontal& horizontal) {
            builder.append("to "_s);
            serializationForCSS(builder, horizontal);
            wroteSomething = true;
        },
        [&](const Vertical& vertical) {
            if (std::holds_alternative<Bottom>(vertical))
                return;

            builder.append("to "_s);
            serializationForCSS(builder, vertical);
            wroteSomething = true;
        },
        [&](const SpaceSeparatedTuple<Horizontal, Vertical>& pair) {
            builder.append("to "_s);
            serializationForCSS(builder, pair);
            wroteSomething = true;
        }
    );

    if (appendColorInterpolationMethod(builder, gradient.colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, gradient.stops);
}

// MARK: - PrefixedLinearGradient

void Serialize<PrefixedLinearGradient>::operator()(StringBuilder& builder, const PrefixedLinearGradient& gradient)
{
    serializationForCSS(builder, gradient.gradientLine);
    builder.append(", "_s);
    serializationForCSS(builder, gradient.stops);
}

// MARK: - DeprecatedLinearGradient

void Serialize<DeprecatedLinearGradient>::operator()(StringBuilder& builder, const DeprecatedLinearGradient& gradient)
{
    builder.append("linear, "_s);

    serializationForCSS(builder, gradient.gradientLine);

    if (!gradient.stops.isEmpty()) {
        builder.append(", "_s);
        serializationForCSS(builder, gradient.stops);
    }
}

// MARK: - RadialGradient

void Serialize<RadialGradient::Ellipse>::operator()(StringBuilder& builder, const RadialGradient::Ellipse& ellipse)
{
    auto lengthBefore = builder.length();

    WTF::switchOn(ellipse.size,
        [&](const RadialGradient::Ellipse::Size& size) {
            serializationForCSS(builder, size);
        },
        [&](const RadialGradient::Extent& extent) {
            if (!std::holds_alternative<FarthestCorner>(extent))
                serializationForCSS(builder, extent);
        }
    );

    if (ellipse.position) {
        if (!isCenterPosition(*ellipse.position)) {
            bool wroteSomething = builder.length() != lengthBefore;
            if (wroteSomething)
                builder.append(' ');

            builder.append("at "_s);
            serializationForCSS(builder, *ellipse.position);
        }
    }
}

void Serialize<RadialGradient::Circle>::operator()(StringBuilder& builder, const RadialGradient::Circle& circle)
{
    WTF::switchOn(circle.size,
        [&](const RadialGradient::Circle::Length& length) {
            serializationForCSS(builder, length);
        },
        [&](const RadialGradient::Extent& extent) {
            if (!std::holds_alternative<FarthestCorner>(extent)) {
                builder.append("circle "_s);
                serializationForCSS(builder, extent);
            } else
                builder.append("circle"_s);
        }
    );

    if (circle.position) {
        if (!isCenterPosition(*circle.position)) {
            builder.append(" at "_s);
            serializationForCSS(builder, *circle.position);
        }
    }
}

void Serialize<RadialGradient>::operator()(StringBuilder& builder, const RadialGradient& gradient)
{
    auto lengthBefore = builder.length();
    serializationForCSS(builder, gradient.gradientBox);
    bool wroteSomething = builder.length() != lengthBefore;

    if (appendColorInterpolationMethod(builder, gradient.colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, gradient.stops);
}

// MARK: - PrefixedRadialGradient

void Serialize<PrefixedRadialGradient::Ellipse>::operator()(StringBuilder& builder, const PrefixedRadialGradient::Ellipse& ellipse)
{
    if (ellipse.position)
        serializationForCSS(builder, *ellipse.position);
    else
        builder.append("center"_s);

    if (ellipse.size) {
        WTF::switchOn(*ellipse.size,
            [&](const PrefixedRadialGradient::Ellipse::Size& size) {
                builder.append(", "_s);
                serializationForCSS(builder, size);
            },
            [&](const PrefixedRadialGradient::Extent& extent) {
                builder.append(", ellipse "_s);
                serializationForCSS(builder, extent);
            }
        );
    }
}

void Serialize<PrefixedRadialGradient::Circle>::operator()(StringBuilder& builder, const PrefixedRadialGradient::Circle& circle)
{
    if (circle.position)
        serializationForCSS(builder, *circle.position);
    else
        builder.append("center"_s);

    builder.append(", circle "_s);
    serializationForCSS(builder, circle.size.value_or(PrefixedRadialGradient::Extent { CSS::Cover { } }));
}

void Serialize<PrefixedRadialGradient>::operator()(StringBuilder& builder, const PrefixedRadialGradient& gradient)
{
    auto lengthBefore = builder.length();
    serializationForCSS(builder, gradient.gradientBox);
    bool wroteSomething = builder.length() != lengthBefore;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, gradient.stops);
}

// MARK: - DeprecatedRadialGradient

void Serialize<DeprecatedRadialGradient::GradientBox>::operator()(StringBuilder& builder, const DeprecatedRadialGradient::GradientBox& gradientBox)
{
    serializationForCSS(builder, gradientBox.first);
    builder.append(", "_s);
    serializationForCSS(builder, gradientBox.firstRadius);
    builder.append(", "_s);
    serializationForCSS(builder, gradientBox.second);
    builder.append(", "_s);
    serializationForCSS(builder, gradientBox.secondRadius);
}

void Serialize<DeprecatedRadialGradient>::operator()(StringBuilder& builder, const DeprecatedRadialGradient& gradient)
{
    builder.append("radial, "_s);

    serializationForCSS(builder, gradient.gradientBox);

    if (!gradient.stops.isEmpty()) {
        builder.append(", "_s);
        serializationForCSS(builder, gradient.stops);
    }
}

// MARK: - ConicGradient

void Serialize<ConicGradient::GradientBox>::operator()(StringBuilder& builder, const ConicGradient::GradientBox& gradientBox)
{
    bool wroteSomething = false;

    if (gradientBox.angle) {
        WTF::switchOn(*gradientBox.angle,
            [&](const AngleRaw<>& angleRaw) {
                if (angleRaw.value) {
                    builder.append("from "_s);
                    serializationForCSS(builder, angleRaw);
                    wroteSomething = true;
                }
            },
            [&](const UnevaluatedCalc<AngleRaw<>>& angleCalc) {
                builder.append("from "_s);
                serializationForCSS(builder, angleCalc);
                wroteSomething = true;
            }
        );
    }

    if (gradientBox.position && !isCenterPosition(*gradientBox.position)) {
        if (wroteSomething)
            builder.append(' ');
        builder.append("at "_s);
        serializationForCSS(builder, *gradientBox.position);
    }
}

void Serialize<ConicGradient>::operator()(StringBuilder& builder, const ConicGradient& gradient)
{
    auto lengthBefore = builder.length();
    serializationForCSS(builder, gradient.gradientBox);
    bool wroteSomething = builder.length() != lengthBefore;

    if (appendColorInterpolationMethod(builder, gradient.colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        builder.append(", "_s);

    serializationForCSS(builder, gradient.stops);
}

} // namespace CSS
} // namespace WebCore
