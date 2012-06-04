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

static const int numberDefaultStep = 1;
static const int numberDefaultStepBase = 0;
static const int numberStepScaleFactor = 1;

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
    return parseToNumber(element()->value(), numeric_limits<double>::quiet_NaN());
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
    return !value.isEmpty() && !isfinite(parseToDoubleForNumberType(value));
}

bool NumberInputType::typeMismatch() const
{
    ASSERT(!typeMismatchFor(element()->value()));
    return false;
}

StepRange NumberInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (numberDefaultStep, numberDefaultStepBase, numberStepScaleFactor));

    unsigned stepBaseDecimalPlaces;
    double stepBaseValue = parseToNumberWithDecimalPlaces(element()->fastGetAttribute(minAttr), numberDefaultStepBase, &stepBaseDecimalPlaces);
    StepRange::NumberWithDecimalPlaces stepBase(stepBaseValue, min(stepBaseDecimalPlaces, 16u));
    double minimum = parseToNumber(element()->fastGetAttribute(minAttr), -numeric_limits<float>::max());
    double maximum = parseToNumber(element()->fastGetAttribute(maxAttr), numeric_limits<float>::max());

    StepRange::NumberWithDecimalPlacesOrMissing step = StepRange::parseStep(anyStepHandling, stepDescription, element()->fastGetAttribute(stepAttr));
    return StepRange(stepBase, minimum, maximum, step, stepDescription);
}

bool NumberInputType::sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const
{
    preferredSize = defaultSize;

    unsigned minValueDecimalPlaces;
    String minValue = element()->fastGetAttribute(minAttr);
    double minValueDouble = parseToDoubleForNumberTypeWithDecimalPlaces(minValue, &minValueDecimalPlaces);
    if (!isfinite(minValueDouble))
        return false;

    unsigned maxValueDecimalPlaces;
    String maxValue = element()->fastGetAttribute(maxAttr);
    double maxValueDouble = parseToDoubleForNumberTypeWithDecimalPlaces(maxValue, &maxValueDecimalPlaces);
    if (!isfinite(maxValueDouble))
        return false;

    if (maxValueDouble < minValueDouble) {
        maxValueDouble = minValueDouble;
        maxValueDecimalPlaces = minValueDecimalPlaces;
    }

    String stepValue = element()->fastGetAttribute(stepAttr);
    if (equalIgnoringCase(stepValue, "any"))
        return false;
    unsigned stepValueDecimalPlaces;
    double stepValueDouble = parseToDoubleForNumberTypeWithDecimalPlaces(stepValue, &stepValueDecimalPlaces);
    if (!isfinite(stepValueDouble)) {
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

double NumberInputType::parseToNumber(const String& src, double defaultValue) const
{
    return parseToDoubleForNumberType(src, defaultValue);
}

double NumberInputType::parseToNumberWithDecimalPlaces(const String& src, double defaultValue, unsigned *decimalPlaces) const
{
    return parseToDoubleForNumberTypeWithDecimalPlaces(src, decimalPlaces, defaultValue);
}

String NumberInputType::serialize(double value) const
{
    if (!isfinite(value))
        return String();
    return serializeForNumberType(value);
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

String NumberInputType::localizeValue(const String& proposedValue) const
{
    if (proposedValue.isEmpty())
        return proposedValue;
    // We don't localize scientific notations.
    if (proposedValue.find(isE) != notFound)
        return proposedValue;
    // FIXME: The following three lines should be removed when we
    // remove the second argument of convertToLocalizedNumber().
    // Note: parseToDoubleForNumberTypeWithDecimalPlaces set zero to decimalPlaces
    // if currentValue isn't valid floating pointer number.
    unsigned decimalPlace;
    parseToDoubleForNumberTypeWithDecimalPlaces(proposedValue, &decimalPlace);
    return convertToLocalizedNumber(proposedValue, decimalPlace);
}

String NumberInputType::visibleValue() const
{
    return localizeValue(element()->value());
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
    return standardValue.isEmpty() || isfinite(parseToDoubleForNumberType(standardValue));
}

String NumberInputType::sanitizeValue(const String& proposedValue) const
{
    if (proposedValue.isEmpty())
        return proposedValue;
    return isfinite(parseToDoubleForNumberType(proposedValue)) ? proposedValue : emptyString();
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
