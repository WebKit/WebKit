/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleLinearEasingFunction.h"

#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "TimingFunction.h"
#include <wtf/IndexedRange.h>

namespace WebCore {
namespace Style {

CSS::LinearEasingFunction toCSSLinearEasingFunction(const LinearTimingFunction& function, const RenderStyle& style)
{
    return CSS::LinearEasingFunction {
        .parameters = {
            .stops = {
                function.points().map([&](const auto& point) {
                    return CSS::LinearEasingParameters::Stop {
                        .output = toCSS(Number<> { point.value }, style),
                        .input = CSS::LinearEasingParameters::Stop::Length {
                            .input = toCSS(Percentage<> { point.progress * 100 }, style),
                            .extra = std::nullopt
                        }
                    };
                })
            }
        }
    };
}

template<typename Resolver> static Ref<TimingFunction> createTimingFunctionWithResolver(const CSS::LinearEasingFunction& function, Resolver&& resolver)
{
    // https://drafts.csswg.org/css-easing-2/#create-a-linear-easing-function

    // `PendingPoint` is used for the first look of the conversion algorithm to gather the outputs, but not necessarily all the inputs.
    struct PendingPoint {
        double output;
        std::optional<double> input { std::nullopt };
    };

    Vector<PendingPoint> points;
    points.reserveInitialCapacity(function->stops.size());

    // 1. Let function be a new linear easing function.
    // NOTE: This is implicit.

    // 2. Let `largestInput` be negative infinity.
    auto largestInput = -std::numeric_limits<double>::infinity();

    // 3. If there are less than two items in stopList, then return failure.
    ASSERT(function->stops.size() >= 2);

    // 4. For each stop in stopList:
    for (auto [i, stop] : indexedRange(function->stops)) {
        // 4.1 Let `point` be a new linear easing point with its output set to stop’s <number> as a number.
        auto point = PendingPoint { .output = resolver(stop.output) };

        // 4.2 Append point to function’s points.
        points.append(point);

        if (stop.input) {
            // 4.3 If stop has a <linear-stop-length>, then:

            // 4.3.1 Set point’s input to whichever is greater: stop’s <linear-stop-length>’s first <percentage> as a number, or largestInput.
            auto firstPercentage = resolver(stop.input->input);
            auto input = std::max(firstPercentage / 100.0, largestInput);
            points.last().input = input;

            // 4.3.2 Set largestInput to point’s input.
            largestInput = input;

            if (stop.input->extra) {
                // 4.3.3 If stop’s <linear-stop-length> has a second <percentage>, then:

                // 4.3.3.1 Let extraPoint be a new linear easing point with its output set to stop’s <number> as a number.
                auto extraPoint = PendingPoint { .output = point.output };

                // 4.3.3.2 Append extraPoint to function’s points.
                points.append(extraPoint);

                // 4.3.3.3 Set extraPoint’s input to whichever is greater: stop’s <linear-stop-length>’s second <percentage> as a number, or largestInput.
                auto secondPercentage = resolver(*stop.input->extra);
                auto extraInput = std::max(secondPercentage / 100.0, largestInput);
                points.last().input = extraInput;

                // 4.3.3.4 Set largestInput to extraPoint’s input.
                largestInput = extraInput;
            }
        } else if (!i) {
            // 4.4 Otherwise, if stop is the first item in stopList, then:

            // 4.4.1 Set point’s input to 0.
            points.last().input = 0;

            // 4.4.2 Set largestInput to 0.
            largestInput = 0;
        } else if (i == (function->stops.size() - 1)) {
            // 4.5 Otherwise, if stop is the last item in stopList, then set point’s input to whichever is greater: 1 or largestInput.
            points.last().input = std::max(1.0, largestInput);
        }
    }

    // 5. For runs of items in function’s points that have a null input, assign a number to the input by linearly interpolating between the closest previous and next points that have a non-null input.
    Vector<LinearTimingFunction::Point> resolvedPoints;
    resolvedPoints.reserveInitialCapacity(points.size());

    std::optional<size_t> missingInputRunStart;
    for (size_t i = 0; i <= points.size(); ++i) {
        if (i < points.size() && !points[i].input) {
            if (!missingInputRunStart)
                missingInputRunStart = i;
            continue;
        }

        if (missingInputRunStart) {
            auto startInput = *points[*missingInputRunStart - 1].input;
            auto endInput = *points[i].input;
            auto numberOfMissingInputs = i - *missingInputRunStart + 1;
            auto increment = (endInput - startInput) / numberOfMissingInputs;
            for (auto j = *missingInputRunStart; j < i; ++j)
                resolvedPoints.append({ .value = points[j].output, .progress = startInput + increment * (j - *missingInputRunStart + 1) });
            missingInputRunStart = std::nullopt;
        }

        if (i < points.size() && points[i].input)
            resolvedPoints.append({ .value = points[i].output, .progress = *points[i].input });
    }
    ASSERT(!missingInputRunStart);
    ASSERT(resolvedPoints.size() == points.size());

    // 6. Return function.
    return LinearTimingFunction::create(WTFMove(resolvedPoints));
}

Ref<TimingFunction> createTimingFunction(const CSS::LinearEasingFunction& function, const CSSToLengthConversionData& conversionData)
{
    return createTimingFunctionWithResolver(function, [&](const auto& value) -> double {
        return toStyle(value, conversionData).value;
    });
}

Ref<TimingFunction> createTimingFunctionDeprecated(const CSS::LinearEasingFunction& function)
{
    if (!CSS::collectComputedStyleDependencies(function).canResolveDependenciesWithConversionData({ }))
        return LinearTimingFunction::create();

    return createTimingFunctionWithResolver(function, [&](const auto& value) -> double {
        return toStyleNoConversionDataRequired(value).value;
    });
}

} // namespace Style
} // namespace WebCore
