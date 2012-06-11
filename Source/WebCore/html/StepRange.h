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

#ifndef StepRange_h
#define StepRange_h

#include "Decimal.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class HTMLInputElement;

enum AnyStepHandling { RejectAny, AnyIsDefaultStep };

// FIXME: We should rename InputNumber to Decimal in all places.
typedef Decimal InputNumber;

inline InputNumber convertDoubleToInputNumber(double doubleValue)
{
    return Decimal::fromDouble(doubleValue);
}

inline double convertInputNumberToDouble(const InputNumber& numericValue)
{
    return numericValue.toDouble();
}

class StepRange {
public:
    enum StepValueShouldBe {
        StepValueShouldBeReal,
        ParsedStepValueShouldBeInteger,
        ScaledStepValueShouldBeInteger,
    };

    struct StepDescription {
        int defaultStep;
        int defaultStepBase;
        int stepScaleFactor;
        StepValueShouldBe stepValueShouldBe;

        StepDescription(int defaultStep, int defaultStepBase, int stepScaleFactor, StepValueShouldBe stepValueShouldBe = StepValueShouldBeReal)
            : defaultStep(defaultStep)
            , defaultStepBase(defaultStepBase)
            , stepScaleFactor(stepScaleFactor)
            , stepValueShouldBe(stepValueShouldBe)
        {
        }

        StepDescription()
            : defaultStep(1)
            , defaultStepBase(0)
            , stepScaleFactor(1)
            , stepValueShouldBe(StepValueShouldBeReal)
        {
        }

        InputNumber defaultValue() const
        {
            return defaultStep * stepScaleFactor;
        }
    };

    StepRange();
    StepRange(const StepRange&);
    StepRange(const InputNumber& stepBase, const InputNumber& minimum, const InputNumber& maximum, const InputNumber& step, const StepDescription&);
    InputNumber acceptableError() const;
    InputNumber alignValueForStep(const InputNumber& currentValue, const InputNumber& newValue) const;
    InputNumber clampValue(const InputNumber& value) const;
    bool hasStep() const { return m_hasStep; }
    InputNumber maximum() const { return m_maximum; }
    InputNumber minimum() const { return m_minimum; }
    static InputNumber parseStep(AnyStepHandling, const StepDescription&, const String&);
    InputNumber step() const { return m_step; }
    InputNumber stepBase() const { return m_stepBase; }
    int stepScaleFactor() const { return m_stepDescription.stepScaleFactor; }
    bool stepMismatch(const InputNumber&) const;

    // Clamp the middle value according to the step
    InputNumber defaultValue() const
    {
        return clampValue((m_minimum + m_maximum) / 2);
    }

    // Map value into 0-1 range
    InputNumber proportionFromValue(const InputNumber& value) const
    {
        if (m_minimum == m_maximum)
            return 0;

        return (value - m_minimum) / (m_maximum - m_minimum);
    }

    // Map from 0-1 range to value
    InputNumber valueFromProportion(const InputNumber& proportion) const
    {
        return m_minimum + proportion * (m_maximum - m_minimum);
    }

private:
    StepRange& operator =(const StepRange&);
    InputNumber roundByStep(const InputNumber& value, const InputNumber& base) const;

    const InputNumber m_maximum; // maximum must be >= minimum.
    const InputNumber m_minimum;
    const InputNumber m_step;
    const InputNumber m_stepBase;
    const StepDescription m_stepDescription;
    const bool m_hasStep;
};

}

#endif // StepRange_h
