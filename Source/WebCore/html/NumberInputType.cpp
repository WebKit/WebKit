/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "NumberInputType.h"

#include "BeforeTextInsertedEvent.h"
#include "ExceptionCode.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "LocalizedNumber.h"
#include "RenderTextControl.h"
#include <limits>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

static const double numberDefaultStep = 1.0;
static const double numberStepScaleFactor = 1.0;

static unsigned lengthBeforeDecimalPoint(double value)
{
    // If value is negative, '-' should be counted.

    double absoluteValue = fabs(value);
    if (absoluteValue < 1)
        return value < 0 ? 2 : 1;

    unsigned length = static_cast<unsigned>(log10(floor(absoluteValue))) + 1;
    if (value < 0)
        length += 1;

    return length;
}

PassOwnPtr<InputType> NumberInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new NumberInputType(element));
}

const AtomicString& NumberInputType::formControlType() const
{
    return InputTypeNames::number();
}

double NumberInputType::valueAsNumber() const
{
    return parseToDouble(element()->value(), numeric_limits<double>::quiet_NaN());
}

void NumberInputType::setValueAsNumber(double newValue, TextFieldEventBehavior eventBehavior, ExceptionCode& ec) const
{
    if (newValue < -numeric_limits<float>::max()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue > numeric_limits<float>::max()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    element()->setValue(serialize(newValue), eventBehavior);
}

bool NumberInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && !parseToDoubleForNumberType(value, 0);
}

bool NumberInputType::typeMismatch() const
{
    ASSERT(!typeMismatchFor(element()->value()));
    return false;
}

bool NumberInputType::rangeUnderflow(const String& value) const
{
    const double nan = numeric_limits<double>::quiet_NaN();
    double doubleValue = parseToDouble(value, nan);
    return isfinite(doubleValue) && doubleValue < minimum();
}

bool NumberInputType::rangeOverflow(const String& value) const
{
    const double nan = numeric_limits<double>::quiet_NaN();
    double doubleValue = parseToDouble(value, nan);
    return isfinite(doubleValue) && doubleValue > maximum();
}

bool NumberInputType::supportsRangeLimitation() const
{
    return true;
}

double NumberInputType::minimum() const
{
    return parseToDouble(element()->fastGetAttribute(minAttr), -numeric_limits<float>::max());
}

double NumberInputType::maximum() const
{
    return parseToDouble(element()->fastGetAttribute(maxAttr), numeric_limits<float>::max());
}

bool NumberInputType::sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const
{
    preferredSize = defaultSize;

    unsigned minValueDecimalPlaces;
    double minValueDouble;
    String minValue = element()->fastGetAttribute(minAttr);
    if (!parseToDoubleForNumberTypeWithDecimalPlaces(minValue, &minValueDouble, &minValueDecimalPlaces))
        return false;

    unsigned maxValueDecimalPlaces;
    double maxValueDouble;
    String maxValue = element()->fastGetAttribute(maxAttr);
    if (!parseToDoubleForNumberTypeWithDecimalPlaces(maxValue, &maxValueDouble, &maxValueDecimalPlaces))
        return false;

    if (maxValueDouble < minValueDouble) {
        maxValueDouble = minValueDouble;
        maxValueDecimalPlaces = minValueDecimalPlaces;
    }

    unsigned stepValueDecimalPlaces;
    double stepValueDouble;
    String stepValue = element()->fastGetAttribute(stepAttr);
    if (equalIgnoringCase(stepValue, "any"))
        return false;
    if (!parseToDoubleForNumberTypeWithDecimalPlaces(stepValue, &stepValueDouble, &stepValueDecimalPlaces)) {
        stepValueDouble = 1;
        stepValueDecimalPlaces = 0;
    }

    unsigned length = lengthBeforeDecimalPoint(minValueDouble);
    length = max(length, lengthBeforeDecimalPoint(maxValueDouble));
    length = max(length, lengthBeforeDecimalPoint(stepValueDouble));

    unsigned lengthAfterDecimalPoint = minValueDecimalPlaces;
    lengthAfterDecimalPoint = max(lengthAfterDecimalPoint, maxValueDecimalPlaces);
    lengthAfterDecimalPoint = max(lengthAfterDecimalPoint, stepValueDecimalPlaces);

    // '.' should be counted if the value has decimal places.
    if (lengthAfterDecimalPoint > 0)
        length += lengthAfterDecimalPoint + 1;

    preferredSize = length;
    return true;
}

bool NumberInputType::isSteppable() const
{
    return true;
}

bool NumberInputType::stepMismatch(const String& value, double step) const
{
    double doubleValue;
    if (!parseToDoubleForNumberType(value, &doubleValue))
        return false;
    doubleValue = fabs(doubleValue - stepBase());
    if (isinf(doubleValue))
        return false;
    // double's fractional part size is DBL_MAN_DIG-bit. If the current value
    // is greater than step*2^DBL_MANT_DIG, the following computation for
    // remainder makes no sense.
    if (doubleValue / pow(2.0, DBL_MANT_DIG) > step)
        return false;
    // The computation follows HTML5 4.10.7.2.10 `The step attribute' :
    // ... that number subtracted from the step base is not an integral multiple
    // of the allowed value step, the element is suffering from a step mismatch.
    double remainder = fabs(doubleValue - step * round(doubleValue / step));
    // Accepts erros in lower fractional part which IEEE 754 single-precision
    // can't represent.
    double computedAcceptableError = acceptableError(step);
    return computedAcceptableError < remainder && remainder < (step - computedAcceptableError);
}

double NumberInputType::stepBase() const
{
    return parseToDouble(element()->fastGetAttribute(minAttr), defaultStepBase());
}

double NumberInputType::stepBaseWithDecimalPlaces(unsigned* decimalPlaces) const
{
    return parseToDoubleWithDecimalPlaces(element()->fastGetAttribute(minAttr), defaultStepBase(), decimalPlaces);
}

double NumberInputType::defaultStep() const
{
    return numberDefaultStep;
}

double NumberInputType::stepScaleFactor() const
{
    return numberStepScaleFactor;
}

void NumberInputType::handleKeydownEvent(KeyboardEvent* event)
{
    handleKeydownEventForSpinButton(event);
    if (!event->defaultHandled())
        TextFieldInputType::handleKeydownEvent(event);
}

void NumberInputType::handleWheelEvent(WheelEvent* event)
{
    handleWheelEventForSpinButton(event);
}

double NumberInputType::parseToDouble(const String& src, double defaultValue) const
{
    double numberValue;
    if (!parseToDoubleForNumberType(src, &numberValue))
        return defaultValue;
    ASSERT(isfinite(numberValue));
    return numberValue;
}

double NumberInputType::parseToDoubleWithDecimalPlaces(const String& src, double defaultValue, unsigned *decimalPlaces) const
{
    double numberValue;
    if (!parseToDoubleForNumberTypeWithDecimalPlaces(src, &numberValue, decimalPlaces))
        return defaultValue;
    ASSERT(isfinite(numberValue));
    return numberValue;
}

String NumberInputType::serialize(double value) const
{
    if (!isfinite(value))
        return String();
    return serializeForNumberType(value);
}

double NumberInputType::acceptableError(double step) const
{
    return step / pow(2.0, FLT_MANT_DIG);
}

void NumberInputType::handleBlurEvent()
{
    // Reset the renderer value, which might be unmatched with the element value.
    element()->setFormControlValueMatchesRenderer(false);

    // We need to reset the renderer value explicitly because an unacceptable
    // renderer value should be purged before style calculation.
    element()->updateInnerTextValue();
}

static bool isE(UChar ch)
{
    return ch == 'e' || ch == 'E';
}

String NumberInputType::visibleValue() const
{
    String currentValue = element()->value();
    if (currentValue.isEmpty())
        return currentValue;
    // We don't localize scientific notations.
    if (currentValue.find(isE) != notFound)
        return currentValue;
    // FIXME: The following three lines should be removed when we
    // remove the second argument of convertToLocalizedNumber().
    double doubleValue = numeric_limits<double>::quiet_NaN();
    unsigned decimalPlace;
    parseToDoubleForNumberTypeWithDecimalPlaces(currentValue, &doubleValue, &decimalPlace);
    return convertToLocalizedNumber(currentValue, decimalPlace);
}

String NumberInputType::convertFromVisibleValue(const String& visibleValue) const
{
    if (visibleValue.isEmpty())
        return visibleValue;
    // We don't localize scientific notations.
    if (visibleValue.find(isE) != notFound)
        return visibleValue;
    return convertFromLocalizedNumber(visibleValue);
}

bool NumberInputType::isAcceptableValue(const String& proposedValue)
{
    String standardValue = convertFromVisibleValue(proposedValue);
    return standardValue.isEmpty() || parseToDoubleForNumberType(standardValue, 0);
}

String NumberInputType::sanitizeValue(const String& proposedValue) const
{
    if (proposedValue.isEmpty())
        return proposedValue;
    return parseToDoubleForNumberType(proposedValue, 0) ? proposedValue : emptyAtom.string();
}

bool NumberInputType::hasUnacceptableValue()
{
    return element()->renderer() && !isAcceptableValue(element()->innerTextValue());
}

bool NumberInputType::shouldRespectSpeechAttribute()
{
    return true;
}

bool NumberInputType::supportsPlaceholder() const
{
    return true;
}

bool NumberInputType::isNumberField() const
{
    return true;
}

void NumberInputType::minOrMaxAttributeChanged()
{
    InputType::minOrMaxAttributeChanged();

    if (element()->renderer())
        element()->renderer()->setNeedsLayoutAndPrefWidthsRecalc();
}

void NumberInputType::stepAttributeChanged()
{
    InputType::stepAttributeChanged();

    if (element()->renderer())
        element()->renderer()->setNeedsLayoutAndPrefWidthsRecalc();
}

} // namespace WebCore
