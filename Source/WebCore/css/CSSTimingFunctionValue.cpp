/*
 * Copyright (C) 2007, 2013, 2016 Apple Inc. All rights reserved.
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
#include "CSSTimingFunctionValue.h"

#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

RefPtr<TimingFunction> createTimingFunction(const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        switch (value.valueID()) {
        case CSSValueLinear:
            return LinearTimingFunction::create();
        case CSSValueEase:
            return CubicBezierTimingFunction::create();
        case CSSValueEaseIn:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseIn);
        case CSSValueEaseOut:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseOut);
        case CSSValueEaseInOut:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
        case CSSValueStepStart:
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::Start);
        case CSSValueStepEnd:
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::End);
        default:
            return nullptr;
        }
    }

    if (auto* timingFunction = dynamicDowncast<CSSLinearTimingFunctionValue>(value))
        return timingFunction->createTimingFunction();
    if (auto* timingFunction = dynamicDowncast<CSSCubicBezierTimingFunctionValue>(value))
        return timingFunction->createTimingFunction();
    if (auto* timingFunction = dynamicDowncast<CSSStepsTimingFunctionValue>(value))
        return timingFunction->createTimingFunction();
    if (auto* timingFunction = dynamicDowncast<CSSSpringTimingFunctionValue>(value))
        return timingFunction->createTimingFunction();

    return nullptr;
}

// MARK: - linear()

bool CSSLinearTimingFunctionValue::LinearStop::Length::operator==(const CSSLinearTimingFunctionValue::LinearStop::Length& other) const
{
    return compareCSSValue(input, other.input)
        && compareCSSValuePtr(extra, other.extra);
}

bool CSSLinearTimingFunctionValue::LinearStop::operator==(const CSSLinearTimingFunctionValue::LinearStop& other) const
{
    return compareCSSValue(output, other.output)
        && input == other.input;
}

static void serializeLinearStop(StringBuilder& builder, const CSSLinearTimingFunctionValue::LinearStop& stop)
{
    builder.append(stop.output->customCSSText());
    if (stop.input) {
        builder.append(' ', stop.input->input->customCSSText());
        if (stop.input->extra)
            builder.append(' ', stop.input->extra->customCSSText());
    }
}

String CSSLinearTimingFunctionValue::customCSSText() const
{
    return makeString("linear("_s, interleave(m_stops, serializeLinearStop, ", "_s), ')');
}

bool CSSLinearTimingFunctionValue::equals(const CSSLinearTimingFunctionValue& other) const
{
    return m_stops == other.m_stops;
}

RefPtr<TimingFunction> CSSLinearTimingFunctionValue::createTimingFunction() const
{
    // https://drafts.csswg.org/css-easing-2/#create-a-linear-easing-function

    // `PendingPoint` is used for the first look of the conversion algorithm to gather the outputs, but not necessarily all the inputs.
    struct PendingPoint {
        double output;
        std::optional<double> input { std::nullopt };
    };

    Vector<PendingPoint> points;
    points.reserveInitialCapacity(m_stops.size());

    // 1. Let function be a new linear easing function.
    // NOTE: This is implicit.

    // 2. Let `largestInput` be negative infinity.
    auto largestInput = -std::numeric_limits<double>::infinity();

    // 3. If there are less than two items in stopList, then return failure.
    ASSERT(m_stops.size() >= 2);

    // 4. For each stop in stopList:
    for (size_t i = 0; i < m_stops.size(); ++i) {
        auto& stop = m_stops[i];

        // 4.1 Let `point` be a new linear easing point with its output set to stop’s <number> as a number.
        auto point = PendingPoint { .output = stop.output->resolveAsNumberDeprecated() };

        // 4.2 Append point to function’s points.
        points.append(point);

        if (stop.input) {
            // 4.3 If stop has a <linear-stop-length>, then:

            // 4.3.1 Set point’s input to whichever is greater: stop’s <linear-stop-length>’s first <percentage> as a number, or largestInput.
            auto firstPercentage = stop.input->input->resolveAsPercentageDeprecated();
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
                auto secondPercentage = stop.input->extra->resolveAsPercentageDeprecated();
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
        } else if (i == (m_stops.size() - 1)) {
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

// MARK: - cubic-bezier()

String CSSCubicBezierTimingFunctionValue::customCSSText() const
{
    return makeString("cubic-bezier("_s, m_x1->customCSSText(), ", "_s, m_y1->customCSSText(), ", "_s, m_x2->customCSSText(), ", "_s, m_y2->customCSSText(), ')');
}

bool CSSCubicBezierTimingFunctionValue::equals(const CSSCubicBezierTimingFunctionValue& other) const
{
    return compareCSSValue(m_x1, other.m_x1)
        && compareCSSValue(m_x2, other.m_x2)
        && compareCSSValue(m_y1, other.m_y1)
        && compareCSSValue(m_y2, other.m_y2);
}

RefPtr<TimingFunction> CSSCubicBezierTimingFunctionValue::createTimingFunction() const
{
    // Clamp `x1` and `x2` to their supported ranges.

    auto x1 = clampTo<double>(m_x1->resolveAsNumberDeprecated(), 0, 1);
    auto y1 = m_y1->resolveAsNumberDeprecated();
    auto x2 = clampTo<double>(m_x2->resolveAsNumberDeprecated(), 0, 1);
    auto y2 = m_y2->resolveAsNumberDeprecated();

    return CubicBezierTimingFunction::create(x1, y1, x2, y2);
}

// MARK: - steps()

String CSSStepsTimingFunctionValue::customCSSText() const
{
    ASCIILiteral position = ""_s;
    if (m_stepPosition) {
        switch (m_stepPosition.value()) {
        case StepsTimingFunction::StepPosition::JumpStart:
            position = ", jump-start"_s;
            break;
        case StepsTimingFunction::StepPosition::JumpNone:
            position = ", jump-none"_s;
            break;
        case StepsTimingFunction::StepPosition::JumpBoth:
            position = ", jump-both"_s;
            break;
        case StepsTimingFunction::StepPosition::Start:
            position = ", start"_s;
            break;
        case StepsTimingFunction::StepPosition::JumpEnd:
        case StepsTimingFunction::StepPosition::End:
            break;
        }
    }
    return makeString("steps("_s, m_steps->customCSSText(), position, ')');
}

bool CSSStepsTimingFunctionValue::equals(const CSSStepsTimingFunctionValue& other) const
{
    return compareCSSValue(m_steps, other.m_steps)
        && m_stepPosition == other.m_stepPosition;
}

RefPtr<TimingFunction> CSSStepsTimingFunctionValue::createTimingFunction() const
{
    auto steps = m_steps->resolveAsIntegerDeprecated();

    // Clamp `steps` to the supported range.
    if (m_stepPosition && *m_stepPosition == StepsTimingFunction::StepPosition::JumpNone)
        steps = std::max(steps, 2);
    else
        steps = std::max(steps, 1);

    return StepsTimingFunction::create(steps, m_stepPosition);
}

// MARK: - spring()

String CSSSpringTimingFunctionValue::customCSSText() const
{
    return makeString("spring("_s, m_mass->customCSSText(), ' ', m_stiffness->customCSSText(), ' ', m_damping->customCSSText(), ' ', m_initialVelocity->customCSSText(), ')');
}

bool CSSSpringTimingFunctionValue::equals(const CSSSpringTimingFunctionValue& other) const
{
    return compareCSSValue(m_mass, other.m_mass)
        && compareCSSValue(m_stiffness, other.m_stiffness)
        && compareCSSValue(m_damping, other.m_damping)
        && compareCSSValue(m_initialVelocity, other.m_initialVelocity);
}

RefPtr<TimingFunction> CSSSpringTimingFunctionValue::createTimingFunction() const
{
    // Clamp `mass`, `stiffness` and `damping` to their supported ranges.

    // FIXME: Contexts that allow calc() should not be defined using a closed interval - https://drafts.csswg.org/css-values-4/#calc-range
    // If spring() ever goes further with standardization, the allowable ranges for `mass` and `stiffness` should be reconsidered as the
    // std::nextafter() clamping is non-obvious.

    auto mass = m_mass->resolveAsNumberDeprecated();
    if (mass <= 0)
        mass = std::nextafter(0, std::numeric_limits<double>::max());

    auto stiffness = m_stiffness->resolveAsNumberDeprecated();
    if (stiffness <= 0)
        stiffness = std::nextafter(0, std::numeric_limits<double>::max());

    auto damping = m_damping->resolveAsNumberDeprecated();
    if (damping < 0)
        damping = 0;

    auto initialVelocity = m_initialVelocity->resolveAsNumberDeprecated();

    return SpringTimingFunction::create(mass, stiffness, damping, initialVelocity);
}

} // namespace WebCore
