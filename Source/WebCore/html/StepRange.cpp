/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StepRange.h"

#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

StepRange::StepRange()
    : m_maximum(100)
    , m_minimum(0)
    , m_step(1)
    , m_stepBase(0)
    , m_stepBaseDecimalPlaces(0)
    , m_stepDecimalPlaces(0)
    , m_hasStep(false)
{
}

StepRange::StepRange(const StepRange& stepRange)
    : m_maximum(stepRange.m_maximum)
    , m_minimum(stepRange.m_minimum)
    , m_step(stepRange.m_step)
    , m_stepBase(stepRange.m_stepBase)
    , m_stepDescription(stepRange.m_stepDescription)
    , m_stepBaseDecimalPlaces(stepRange.m_stepBaseDecimalPlaces)
    , m_stepDecimalPlaces(stepRange.m_stepDecimalPlaces)
    , m_hasStep(stepRange.m_hasStep)
{
}

StepRange::StepRange(const NumberWithDecimalPlaces& stepBase, const InputNumber& minimum, const InputNumber& maximum, const NumberWithDecimalPlacesOrMissing& step, const StepDescription& stepDescription)
    : m_maximum(maximum)
    , m_minimum(minimum)
    , m_step(step.value.value)
    , m_stepBase(stepBase.value)
    , m_stepDescription(stepDescription)
    , m_stepBaseDecimalPlaces(stepBase.decimalPlaces)
    , m_stepDecimalPlaces(step.value.decimalPlaces)
    , m_hasStep(step.hasValue)
{
    ASSERT(isfinite(m_maximum));
    ASSERT(isfinite(m_minimum));
    ASSERT(isfinite(m_step));
    ASSERT(isfinite(m_stepBase));
}

InputNumber StepRange::acceptableError() const
{
    return m_step / pow(2.0, FLT_MANT_DIG);
}

InputNumber StepRange::alignValueForStep(const InputNumber& currentValue, unsigned currentDecimalPlaces, const InputNumber& proposedValue) const
{
    if (proposedValue >= pow(10.0, 21.0))
        return proposedValue;

    InputNumber newValue = proposedValue;
    if (stepMismatch(currentValue)) {
        const InputNumber scale = pow(10.0, static_cast<InputNumber>(max(m_stepDecimalPlaces, currentDecimalPlaces)));
        newValue = round(newValue * scale) / scale;
    } else {
        const InputNumber scale = pow(10.0, static_cast<InputNumber>(max(m_stepDecimalPlaces, m_stepBaseDecimalPlaces)));
        newValue = round(roundByStep(newValue, m_stepBase) * scale) / scale;
    }

    return newValue;
}

InputNumber StepRange::clampValue(const InputNumber& value) const
{
    const InputNumber inRangeValue = max(m_minimum, min(value, m_maximum));
    if (!m_hasStep)
        return inRangeValue;
    // Rounds inRangeValue to minimum + N * step.
    const InputNumber roundedValue = roundByStep(inRangeValue, m_minimum);
    const InputNumber clampedValue = roundedValue > m_maximum ? roundedValue - m_step : roundedValue;
    ASSERT(clampedValue >= m_minimum);
    ASSERT(clampedValue <= m_maximum);
    return clampedValue;
}

StepRange::NumberWithDecimalPlacesOrMissing StepRange::parseStep(AnyStepHandling anyStepHandling, const StepDescription& stepDescription, const String& stepString)
{
    if (stepString.isEmpty())
        return NumberWithDecimalPlacesOrMissing(stepDescription.defaultValue());

    if (equalIgnoringCase(stepString, "any")) {
        switch (anyStepHandling) {
        case RejectAny:
            return NumberWithDecimalPlacesOrMissing(NumberWithDecimalPlaces(1), false);
        case AnyIsDefaultStep:
            return NumberWithDecimalPlacesOrMissing(stepDescription.defaultValue());
        default:
            ASSERT_NOT_REACHED();
        }
    }

    NumberWithDecimalPlacesOrMissing step(0);
    step.value.value = parseToDoubleForNumberTypeWithDecimalPlaces(stepString, &step.value.decimalPlaces);
    if (!isfinite(step.value.value) || step.value.value <= 0.0)
        return NumberWithDecimalPlacesOrMissing(stepDescription.defaultValue());

    switch (stepDescription.stepValueShouldBe) {
    case StepValueShouldBeReal:
        step.value.value *= stepDescription.stepScaleFactor;
        break;
    case ParsedStepValueShouldBeInteger:
        // For date, month, and week, the parsed value should be an integer for some types.
        step.value.value = max(round(step.value.value), 1.0);
        step.value.value *= stepDescription.stepScaleFactor;
        break;
    case ScaledStepValueShouldBeInteger:
        // For datetime, datetime-local, time, the result should be an integer.
        step.value.value *= stepDescription.stepScaleFactor;
        step.value.value = max(round(step.value.value), 1.0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    ASSERT(step.value.value > 0);
    return step;
}

InputNumber StepRange::roundByStep(const InputNumber& value, const InputNumber& base) const
{
    return base + round((value - base) / m_step) * m_step;
}

bool StepRange::stepMismatch(const InputNumber& valueForCheck) const
{
    if (!m_hasStep)
        return false;
    if (!isfinite(valueForCheck))
        return false;
    const InputNumber value = fabs(valueForCheck - m_stepBase);
    if (isinf(value))
        return false;
    // InputNumber's fractional part size is DBL_MAN_DIG-bit. If the current value
    // is greater than step*2^DBL_MANT_DIG, the following computation for
    // remainder makes no sense.
    if (value / pow(2.0, DBL_MANT_DIG) > m_step)
        return false;
    // The computation follows HTML5 4.10.7.2.10 `The step attribute' :
    // ... that number subtracted from the step base is not an integral multiple
    // of the allowed value step, the element is suffering from a step mismatch.
    const InputNumber remainder = fabs(value - m_step * round(value / m_step));
    // Accepts erros in lower fractional part which IEEE 754 single-precision
    // can't represent.
    const InputNumber computedAcceptableError = acceptableError();
    return computedAcceptableError < remainder && remainder < (m_step - computedAcceptableError);
}

} // namespace WebCore
