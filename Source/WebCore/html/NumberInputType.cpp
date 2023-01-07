/*
 * Copyright (C) 2010-2014 Google Inc. All rights reserved.
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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

#include "Decimal.h"
#include "ElementInlines.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "PlatformLocale.h"
#include "RenderTextControl.h"
#include "StepRange.h"
#include <limits>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace HTMLNames;

static const int numberDefaultStep = 1;
static const int numberDefaultStepBase = 0;
static const int numberStepScaleFactor = 1;

struct RealNumberRenderSize {
    unsigned sizeBeforeDecimalPoint;
    unsigned sizeAfteDecimalPoint;

    RealNumberRenderSize max(const RealNumberRenderSize& other) const
    {
        return {
            std::max(sizeBeforeDecimalPoint, other.sizeBeforeDecimalPoint),
            std::max(sizeAfteDecimalPoint, other.sizeAfteDecimalPoint)
        };
    }
};

static RealNumberRenderSize calculateRenderSize(const Decimal& value)
{
    ASSERT(value.isFinite());
    const unsigned sizeOfDigits = String::number(value.value().coefficient()).length();
    const unsigned sizeOfSign = value.isNegative() ? 1 : 0;
    const int exponent = value.exponent();
    if (exponent >= 0)
        return { sizeOfSign + sizeOfDigits, 0 };

    const int sizeBeforeDecimalPoint = exponent + sizeOfDigits;
    if (sizeBeforeDecimalPoint > 0) {
        // In case of "123.456"
        return { sizeOfSign + sizeBeforeDecimalPoint, sizeOfDigits - sizeBeforeDecimalPoint };
    }

    // In case of "0.00012345"
    const unsigned sizeOfZero = 1;
    const unsigned numberOfZeroAfterDecimalPoint = -sizeBeforeDecimalPoint;
    return { sizeOfSign + sizeOfZero , numberOfZeroAfterDecimalPoint + sizeOfDigits };
}

const AtomString& NumberInputType::formControlType() const
{
    return InputTypeNames::number();
}

void NumberInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    ASSERT(element());
    if (!valueChanged && sanitizedValue.isEmpty() && !element()->innerTextValue().isEmpty())
        updateInnerTextValue();
    TextFieldInputType::setValue(sanitizedValue, valueChanged, eventBehavior, selection);
}

double NumberInputType::valueAsDouble() const
{
    ASSERT(element());
    return parseToDoubleForNumberType(element()->value());
}

ExceptionOr<void> NumberInputType::setValueAsDouble(double newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    element()->setValue(serializeForNumberType(newValue), eventBehavior);
    return { };
}

ExceptionOr<void> NumberInputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    element()->setValue(serializeForNumberType(newValue), eventBehavior);
    return { };
}

bool NumberInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && !std::isfinite(parseToDoubleForNumberType(value));
}

bool NumberInputType::typeMismatch() const
{
    ASSERT(element());
    ASSERT(!typeMismatchFor(element()->value()));
    return false;
}

StepRange NumberInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    static NeverDestroyed<const StepRange::StepDescription> stepDescription(numberDefaultStep, numberDefaultStepBase, numberStepScaleFactor);

    ASSERT(element());
    Decimal stepBase = parseToDecimalForNumberType(element()->attributeWithoutSynchronization(minAttr), Decimal::nan());
    if (stepBase.isNaN())
        stepBase = parseToDecimalForNumberType(element()->attributeWithoutSynchronization(valueAttr), numberDefaultStepBase);

    const Decimal doubleMax = Decimal::fromDouble(std::numeric_limits<double>::max());
    const Element& element = *this->element();

    RangeLimitations rangeLimitations = RangeLimitations::Invalid;
    auto extractBound = [&] (const QualifiedName& attributeName, const Decimal& defaultValue) -> Decimal {
        const AtomString& attributeValue = element.attributeWithoutSynchronization(attributeName);
        Decimal valueFromAttribute = parseToNumberOrNaN(attributeValue);
        if (valueFromAttribute.isFinite()) {
            rangeLimitations = RangeLimitations::Valid;
            return valueFromAttribute;
        }
        return defaultValue;
    };
    Decimal minimum = extractBound(minAttr, -doubleMax);
    Decimal maximum = extractBound(maxAttr, doubleMax);

    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element.attributeWithoutSynchronization(stepAttr));
    return StepRange(stepBase, rangeLimitations, minimum, maximum, step, stepDescription);
}

bool NumberInputType::sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const
{
    preferredSize = defaultSize;

    ASSERT(element());
    auto& stepString = element()->attributeWithoutSynchronization(stepAttr);
    if (equalLettersIgnoringASCIICase(stepString, "any"_s))
        return false;

    const Decimal minimum = parseToDecimalForNumberType(element()->attributeWithoutSynchronization(minAttr));
    if (!minimum.isFinite())
        return false;

    const Decimal maximum = parseToDecimalForNumberType(element()->attributeWithoutSynchronization(maxAttr));
    if (!maximum.isFinite())
        return false;

    const Decimal step = parseToDecimalForNumberType(stepString, 1);
    ASSERT(step.isFinite());

    RealNumberRenderSize size = calculateRenderSize(minimum).max(calculateRenderSize(maximum).max(calculateRenderSize(step)));

    preferredSize = size.sizeBeforeDecimalPoint + size.sizeAfteDecimalPoint + (size.sizeAfteDecimalPoint ? 1 : 0);

    return true;
}

float NumberInputType::decorationWidth() const
{
    ASSERT(element());

    float width = 0;
    RefPtr<HTMLElement> spinButton = element()->innerSpinButtonElement();
    if (RenderBox* spinRenderer = spinButton ? spinButton->renderBox() : 0) {
        width += spinRenderer->borderAndPaddingLogicalWidth();
        // Since the width of spinRenderer is not calculated yet, spinRenderer->logicalWidth() returns 0.
        // So computedStyle()->logicalWidth() is used instead.
        width += spinButton->computedStyle()->logicalWidth().value();
    }
    return width;
}

auto NumberInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    handleKeydownEventForSpinButton(event);
    if (!event.defaultHandled())
        return TextFieldInputType::handleKeydownEvent(event);
    return ShouldCallBaseEventHandler::Yes;
}

Decimal NumberInputType::parseToNumber(const String& src, const Decimal& defaultValue) const
{
    return parseToDecimalForNumberType(src, defaultValue);
}

String NumberInputType::serialize(const Decimal& value) const
{
    if (!value.isFinite())
        return String();
    return serializeForNumberType(value);
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
    ASSERT(element());
    return element()->locale().convertToLocalizedNumber(proposedValue);
}

String NumberInputType::visibleValue() const
{
    ASSERT(element());
    return localizeValue(element()->value());
}

String NumberInputType::convertFromVisibleValue(const String& visibleValue) const
{
    if (visibleValue.isEmpty())
        return visibleValue;
    // We don't localize scientific notations.
    if (visibleValue.find(isE) != notFound)
        return visibleValue;
    ASSERT(element());
    return element()->locale().convertFromLocalizedNumber(visibleValue);
}

String NumberInputType::sanitizeValue(const String& proposedValue) const
{
    if (proposedValue.isEmpty())
        return proposedValue;
    return std::isfinite(parseToDoubleForNumberType(proposedValue)) ? proposedValue : emptyString();
}

bool NumberInputType::hasBadInput() const
{
    ASSERT(element());
    String standardValue = convertFromVisibleValue(element()->innerTextValue());
    return !standardValue.isEmpty() && !std::isfinite(parseToDoubleForNumberType(standardValue));
}

String NumberInputType::badInputText() const
{
    return validationMessageBadInputForNumberText();
}

bool NumberInputType::supportsPlaceholder() const
{
    return true;
}

void NumberInputType::attributeChanged(const QualifiedName& name)
{
    ASSERT(element());
    if (name == maxAttr || name == minAttr) {
        if (auto* element = this->element()) {
            element->invalidateStyleForSubtree();
            if (auto* renderer = element->renderer())
                renderer->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (name == stepAttr) {
        if (auto* element = this->element()) {
            if (auto* renderer = element->renderer())
                renderer->setNeedsLayoutAndPrefWidthsRecalc();
        }
    }
    TextFieldInputType::attributeChanged(name);
}

} // namespace WebCore
