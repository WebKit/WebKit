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
#include "NumberInputType.h"

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

static const double numberDefaultMinimum = -FLT_MAX;
static const double numberDefaultMaximum = FLT_MAX;

static const double numberDefaultStep = 1.0;
static const double numberStepScaleFactor = 1.0;

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

void NumberInputType::setValueAsNumber(double newValue, ExceptionCode& ec) const
{
    if (newValue < numberDefaultMinimum) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue > numberDefaultMaximum) {
        ec = INVALID_STATE_ERR;
        return;
    }
    element()->setValue(serialize(newValue));
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

double NumberInputType::minimum() const
{
    return parseToDouble(element()->fastGetAttribute(minAttr), numberDefaultMinimum);
}

double NumberInputType::maximum() const
{
    return parseToDouble(element()->fastGetAttribute(maxAttr), numberDefaultMaximum);
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

bool NumberInputType::handleKeydownEvent(KeyboardEvent* event)
{
    return handleKeydownEventForSpinButton(event) || TextFieldInputType::handleKeydownEventForSpinButton(event);
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


} // namespace WebCore
