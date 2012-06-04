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

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class HTMLInputElement;

enum AnyStepHandling { RejectAny, AnyIsDefaultStep };

class StepRange {
public:
    struct NumberWithDecimalPlaces {
        unsigned decimalPlaces;
        double value;

        NumberWithDecimalPlaces(double value = 0, unsigned decimalPlaces = 0)
            : decimalPlaces(decimalPlaces)
            , value(value)
        {
        }
    };

    struct NumberWithDecimalPlacesOrMissing {
        bool hasValue;
        NumberWithDecimalPlaces value;

        NumberWithDecimalPlacesOrMissing(NumberWithDecimalPlaces value, bool hasValue = true)
            : hasValue(hasValue)
            , value(value)
        {
        }
    };

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

        double defaultValue() const
        {
            return defaultStep * stepScaleFactor;
        }
    };

    StepRange();
    StepRange(const StepRange&);
    StepRange(const NumberWithDecimalPlaces& stepBase, double minimum, double maximum, const NumberWithDecimalPlacesOrMissing& step, const StepDescription&);
    double acceptableError() const;
    double alignValueForStep(double currentValue, unsigned currentDecimalPlaces, double newValue) const;
    double clampValue(double value) const;
    bool hasStep() const { return m_hasStep; }
    double maximum() const { return m_maximum; }
    double minimum() const { return m_minimum; }
    static NumberWithDecimalPlacesOrMissing parseStep(AnyStepHandling, const StepDescription&, const String&);
    double step() const { return m_step; }
    double stepBase() const { return m_stepBase; }
    unsigned stepBaseDecimalPlaces() const { return m_stepBaseDecimalPlaces; }
    unsigned stepDecimalPlaces() const { return m_stepDecimalPlaces; }
    int stepScaleFactor() const { return m_stepDescription.stepScaleFactor; }
    bool stepMismatch(double) const;

    // Clamp the middle value according to the step
    double defaultValue() const
    {
        return clampValue((m_minimum + m_maximum) / 2);
    }

    // Map value into 0-1 range
    double proportionFromValue(double value) const
    {
        if (m_minimum == m_maximum)
            return 0;

        return (value - m_minimum) / (m_maximum - m_minimum);
    }

    // Map from 0-1 range to value
    double valueFromProportion(double proportion) const
    {
        return m_minimum + proportion * (m_maximum - m_minimum);
    }

private:
    StepRange& operator =(const StepRange&);

    const double m_maximum; // maximum must be >= minimum.
    const double m_minimum;
    const double m_step;
    const double m_stepBase;
    const StepDescription m_stepDescription;
    const unsigned m_stepBaseDecimalPlaces;
    const unsigned m_stepDecimalPlaces;
    const bool m_hasStep;
};

}

#endif // StepRange_h
