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
#include "DateInputType.h"

#include "CalendarPickerElement.h"
#include "DateComponents.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedDate.h"
#include <wtf/PassOwnPtr.h>

#if ENABLE(INPUT_TYPE_DATE)

namespace WebCore {

using namespace HTMLNames;

static const int dateDefaultStep = 1;
static const int dateDefaultStepBase = 0;
static const int dateStepScaleFactor = 86400000;

inline DateInputType::DateInputType(HTMLInputElement* element)
    : BaseDateAndTimeInputType(element)
#if ENABLE(CALENDAR_PICKER)
    , m_pickerElement(0)
#endif
{
}

PassOwnPtr<InputType> DateInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new DateInputType(element));
}

const AtomicString& DateInputType::formControlType() const
{
    return InputTypeNames::date();
}

DateComponents::Type DateInputType::dateType() const
{
    return DateComponents::Date;
}

StepRange DateInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (dateDefaultStep, dateDefaultStepBase, dateStepScaleFactor, StepRange::ParsedStepValueShouldBeInteger));

    const Decimal stepBase = parseToNumber(element()->fastGetAttribute(minAttr), 0);
    const Decimal minimum = parseToNumber(element()->fastGetAttribute(minAttr), Decimal::fromDouble(DateComponents::minimumDate()));
    const Decimal maximum = parseToNumber(element()->fastGetAttribute(maxAttr), Decimal::fromDouble(DateComponents::maximumDate()));
    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element()->fastGetAttribute(stepAttr));
    return StepRange(stepBase, minimum, maximum, step, stepDescription);
}

bool DateInputType::parseToDateComponentsInternal(const UChar* characters, unsigned length, DateComponents* out) const
{
    ASSERT(out);
    unsigned end;
    return out->parseDate(characters, length, 0, end) && end == length;
}

bool DateInputType::setMillisecondToDateComponents(double value, DateComponents* date) const
{
    ASSERT(date);
    return date->setMillisecondsSinceEpochForDate(value);
}

bool DateInputType::isDateField() const
{
    return true;
}

#if ENABLE(CALENDAR_PICKER)
void DateInputType::createShadowSubtree()
{
    BaseDateAndTimeInputType::createShadowSubtree();
    RefPtr<CalendarPickerElement> pickerElement = CalendarPickerElement::create(element()->document());
    m_pickerElement = pickerElement.get();
    containerElement()->insertBefore(m_pickerElement, innerBlockElement()->nextSibling(), ASSERT_NO_EXCEPTION);
}

void DateInputType::destroyShadowSubtree()
{
    TextFieldInputType::destroyShadowSubtree();
    m_pickerElement = 0;
}

bool DateInputType::needsContainer() const
{
    return true;
}

bool DateInputType::shouldHaveSpinButton() const
{
    return false;
}

void DateInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (element()->disabled() || element()->readOnly())
        return;
    if (event->keyIdentifier() == "Down") {
        if (m_pickerElement)
            m_pickerElement->openPopup();
        event->setDefaultHandled();
        return;
    }
    BaseDateAndTimeInputType::handleKeydownEvent(event);
}

void DateInputType::handleBlurEvent()
{
    if (m_pickerElement)
        m_pickerElement->closePopup();

    // Reset the renderer value, which might be unmatched with the element value.
    element()->setFormControlValueMatchesRenderer(false);
    // We need to reset the renderer value explicitly because an unacceptable
    // renderer value should be purged before style calculation.
    updateInnerTextValue();
}

bool DateInputType::supportsPlaceholder() const
{
    return true;
}

bool DateInputType::usesFixedPlaceholder() const
{
    return true;
}

String DateInputType::fixedPlaceholder()
{
    return localizedDateFormatText();
}
#endif // ENABLE(CALENDAR_PICKER)

} // namespace WebCore
#endif
