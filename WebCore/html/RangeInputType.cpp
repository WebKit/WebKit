/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RangeInputType.h"

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "RenderSlider.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

static const double rangeDefaultMinimum = 0.0;
static const double rangeDefaultMaximum = 100.0;
static const double rangeDefaultStep = 1.0;
static const double rangeStepScaleFactor = 1.0;

PassOwnPtr<InputType> RangeInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new RangeInputType(element));
}

const AtomicString& RangeInputType::formControlType() const
{
    return InputTypeNames::range();
}

double RangeInputType::valueAsNumber() const
{
    return parseToDouble(element()->value(), numeric_limits<double>::quiet_NaN());
}

void RangeInputType::setValueAsNumber(double newValue, ExceptionCode&) const
{
    element()->setValue(serialize(newValue));
}

bool RangeInputType::supportsRequired() const
{
    return false;
}

bool RangeInputType::rangeUnderflow(const String& value) const
{
    // Guaranteed by sanitization.
    ASSERT_UNUSED(value, parseToDouble(value, numeric_limits<double>::quiet_NaN()) >= minimum());
    return false;
}

bool RangeInputType::rangeOverflow(const String& value) const
{
    // Guaranteed by sanitization.
    ASSERT_UNUSED(value, parseToDouble(value, numeric_limits<double>::quiet_NaN()) <= maximum());
    return false;
}

double RangeInputType::minimum() const
{
    return parseToDouble(element()->fastGetAttribute(minAttr), rangeDefaultMinimum);
}

double RangeInputType::maximum() const
{
    double max = parseToDouble(element()->fastGetAttribute(maxAttr), rangeDefaultMaximum);
    // A remedy for the inconsistent min/max values.
    // Sets the maximum to the default or the minimum value.
    double min = minimum();
    if (max < min)
        max = std::max(min, rangeDefaultMaximum);
    return max;
}

bool RangeInputType::stepMismatch(const String&, double) const
{
    // stepMismatch doesn't occur for type=range. RenderSlider guarantees the
    // value matches to step on user input, and sanitization takes care
    // of the general case.
    return false;
}

double RangeInputType::stepBase() const
{
    return minimum();
}

double RangeInputType::defaultStep() const
{
    return rangeDefaultStep;
}

double RangeInputType::stepScaleFactor() const
{
    return rangeStepScaleFactor;
}

bool RangeInputType::handleKeydownEvent(KeyboardEvent* event)
{
    const String& key = event->keyIdentifier();
    if (key != "Up" && key != "Right" && key != "Down" && key != "Left")
        return false;

    ExceptionCode ec;
    if (equalIgnoringCase(element()->fastGetAttribute(stepAttr), "any")) {
        double min = minimum();
        double max = maximum();
        // FIXME: We can't use stepUp() for the step value "any". So, we increase
        // or decrease the value by 1/100 of the value range. Is it reasonable?
        double step = (max - min) / 100;
        double current = parseToDouble(element()->value(), numeric_limits<double>::quiet_NaN());
        ASSERT(isfinite(current));
        double newValue;
        if (key == "Up" || key == "Right") {
            newValue = current + step;
            if (newValue > max)
                newValue = max;
        } else {
            newValue = current - step;
            if (newValue < min)
                newValue = min;
        }
        if (newValue != current) {
            setValueAsNumber(newValue, ec);
            element()->dispatchFormControlChangeEvent();
        }
    } else {
        int stepMagnification = (key == "Up" || key == "Right") ? 1 : -1;
        String lastStringValue = element()->value();
        element()->stepUp(stepMagnification, ec);
        if (lastStringValue != element()->value())
            element()->dispatchFormControlChangeEvent();
    }
    event->setDefaultHandled();
    return true;
}

RenderObject* RangeInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    return new (arena) RenderSlider(element());
}

double RangeInputType::parseToDouble(const String& src, double defaultValue) const
{
    double numberValue;
    if (!parseToDoubleForNumberType(src, &numberValue))
        return defaultValue;
    ASSERT(isfinite(numberValue));
    return numberValue;
}

String RangeInputType::serialize(double value) const
{
    if (!isfinite(value))
        return String();
    return serializeForNumberType(value);
}

} // namespace WebCore
