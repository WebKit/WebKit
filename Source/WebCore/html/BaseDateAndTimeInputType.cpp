/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "BaseDateAndTimeInputType.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "BaseClickableWithKeyInputType.h"
#include "Chrome.h"
#include "DateComponents.h"
#include "DateTimeChooserParameters.h"
#include "Decimal.h"
#include "FocusController.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "PlatformLocale.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StepRange.h"
#include "Text.h"
#include "UserGestureIndicator.h"
#include <limits>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {

using namespace HTMLNames;

static const int msecPerMinute = 60 * 1000;
static const int msecPerSecond = 1000;

void BaseDateAndTimeInputType::DateTimeFormatValidator::visitField(DateTimeFormat::FieldType fieldType, int)
{
    switch (fieldType) {
    case DateTimeFormat::FieldTypeYear:
        m_results.add(DateTimeFormatValidationResults::HasYear);
        break;

    case DateTimeFormat::FieldTypeMonth:
    case DateTimeFormat::FieldTypeMonthStandAlone:
        m_results.add(DateTimeFormatValidationResults::HasMonth);
        break;

    case DateTimeFormat::FieldTypeWeekOfYear:
        m_results.add(DateTimeFormatValidationResults::HasWeek);
        break;

    case DateTimeFormat::FieldTypeDayOfMonth:
        m_results.add(DateTimeFormatValidationResults::HasDay);
        break;

    case DateTimeFormat::FieldTypePeriod:
        m_results.add(DateTimeFormatValidationResults::HasMeridiem);
        break;

    case DateTimeFormat::FieldTypeHour11:
    case DateTimeFormat::FieldTypeHour12:
        m_results.add(DateTimeFormatValidationResults::HasHour);
        break;

    case DateTimeFormat::FieldTypeHour23:
    case DateTimeFormat::FieldTypeHour24:
        m_results.add(DateTimeFormatValidationResults::HasHour);
        m_results.add(DateTimeFormatValidationResults::HasMeridiem);
        break;

    case DateTimeFormat::FieldTypeMinute:
        m_results.add(DateTimeFormatValidationResults::HasMinute);
        break;

    case DateTimeFormat::FieldTypeSecond:
        m_results.add(DateTimeFormatValidationResults::HasSecond);
        break;

    default:
        break;
    }
}

bool BaseDateAndTimeInputType::DateTimeFormatValidator::validateFormat(const String& format, const BaseDateAndTimeInputType& inputType)
{
    if (!DateTimeFormat::parse(format, *this))
        return false;
    return inputType.isValidFormat(m_results);
}

BaseDateAndTimeInputType::~BaseDateAndTimeInputType()
{
    closeDateTimeChooser();
}

double BaseDateAndTimeInputType::valueAsDate() const
{
    return valueAsDouble();
}

ExceptionOr<void> BaseDateAndTimeInputType::setValueAsDate(double value) const
{
    ASSERT(element());
    element()->setValue(serializeWithMilliseconds(value));
    return { };
}

double BaseDateAndTimeInputType::valueAsDouble() const
{
    ASSERT(element());
    const Decimal value = parseToNumber(element()->value(), Decimal::nan());
    return value.isFinite() ? value.toDouble() : DateComponents::invalidMilliseconds();
}

ExceptionOr<void> BaseDateAndTimeInputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    element()->setValue(serialize(newValue), eventBehavior);
    return { };
}

bool BaseDateAndTimeInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && !parseToDateComponents(value);
}

bool BaseDateAndTimeInputType::typeMismatch() const
{
    ASSERT(element());
    return typeMismatchFor(element()->value());
}

Decimal BaseDateAndTimeInputType::defaultValueForStepUp() const
{
    double ms = WallTime::now().secondsSinceEpoch().milliseconds();
    int offset = calculateLocalTimeOffset(ms).offset / msPerMinute;
    return Decimal::fromDouble(ms + (offset * msPerMinute));
}

bool BaseDateAndTimeInputType::isSteppable() const
{
    return true;
}

Decimal BaseDateAndTimeInputType::parseToNumber(const String& source, const Decimal& defaultValue) const
{
    auto date = parseToDateComponents(source);
    if (!date)
        return defaultValue;
    double msec = date->millisecondsSinceEpoch();
    ASSERT(std::isfinite(msec));
    return Decimal::fromDouble(msec);
}

String BaseDateAndTimeInputType::serialize(const Decimal& value) const
{
    if (!value.isFinite())
        return { };
    auto date = setMillisecondToDateComponents(value.toDouble());
    if (!date)
        return { };
    return serializeWithComponents(*date);
}

String BaseDateAndTimeInputType::serializeWithComponents(const DateComponents& date) const
{
    ASSERT(element());
    Decimal step;
    if (!element()->getAllowedValueStep(&step) || step.remainder(msecPerMinute).isZero())
        return date.toString();
    if (step.remainder(msecPerSecond).isZero())
        return date.toString(SecondFormat::Second);
    return date.toString(SecondFormat::Millisecond);
}

String BaseDateAndTimeInputType::serializeWithMilliseconds(double value) const
{
    return serialize(Decimal::fromDouble(value));
}

String BaseDateAndTimeInputType::localizeValue(const String& proposedValue) const
{
    auto date = parseToDateComponents(proposedValue);
    if (!date)
        return proposedValue;

    ASSERT(element());
    String localized = element()->locale().formatDateTime(*date);
    return localized.isEmpty() ? proposedValue : localized;
}

String BaseDateAndTimeInputType::visibleValue() const
{
    ASSERT(element());
    return localizeValue(element()->value());
}

String BaseDateAndTimeInputType::sanitizeValue(const String& proposedValue) const
{
    return typeMismatchFor(proposedValue) ? String() : proposedValue;
}

bool BaseDateAndTimeInputType::supportsReadOnly() const
{
    return true;
}

bool BaseDateAndTimeInputType::shouldRespectListAttribute()
{
    return InputType::themeSupportsDataListUI(this);
}

bool BaseDateAndTimeInputType::valueMissing(const String& value) const
{
    ASSERT(element());
    return element()->isRequired() && value.isEmpty();
}

bool BaseDateAndTimeInputType::isKeyboardFocusable(KeyboardEvent*) const
{
    ASSERT(element());
    return !element()->isReadOnly() && element()->isTextFormControlFocusable();
}

bool BaseDateAndTimeInputType::isMouseFocusable() const
{
    ASSERT(element());
    return element()->isTextFormControlFocusable();
}

bool BaseDateAndTimeInputType::shouldHaveSecondField(const DateComponents& date) const
{
    if (date.second())
        return true;

    auto stepRange = createStepRange(AnyStepHandling::Default);
    return !stepRange.minimum().remainder(msecPerMinute).isZero()
        || !stepRange.step().remainder(msecPerMinute).isZero();
}

bool BaseDateAndTimeInputType::shouldHaveMillisecondField(const DateComponents& date) const
{
    if (date.millisecond())
        return true;

    auto stepRange = createStepRange(AnyStepHandling::Default);
    return !stepRange.minimum().remainder(msecPerSecond).isZero()
        || !stepRange.step().remainder(msecPerSecond).isZero();
}

void BaseDateAndTimeInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    InputType::setValue(value, valueChanged, eventBehavior);
    if (valueChanged)
        updateInnerTextValue();
}

void BaseDateAndTimeInputType::handleDOMActivateEvent(Event&)
{
    ASSERT(element());
    if (element()->isDisabledOrReadOnly() || !element()->renderer() || !UserGestureIndicator::processingUserGesture())
        return;

    if (m_dateTimeChooser)
        return;
    if (!element()->document().page())
        return;

    DateTimeChooserParameters parameters;
    if (!element()->setupDateTimeChooserParameters(parameters))
        return;

    if (auto chrome = this->chrome()) {
        m_dateTimeChooser = chrome->createDateTimeChooser(*this);
        if (m_dateTimeChooser)
            m_dateTimeChooser->showChooser(parameters);
    }
}

void BaseDateAndTimeInputType::createShadowSubtree()
{
    ASSERT(element());

    auto& element = *this->element();
    auto& document = element.document();

    if (document.settings().dateTimeInputsEditableComponentsEnabled()) {
        m_dateTimeEditElement = DateTimeEditElement::create(document, *this);
        element.userAgentShadowRoot()->appendChild(*m_dateTimeEditElement);
    } else {
        static MainThreadNeverDestroyed<const AtomString> valueContainerPseudo("-webkit-date-and-time-value", AtomString::ConstructFromLiteral);
        auto valueContainer = HTMLDivElement::create(document);
        valueContainer->setPseudo(valueContainerPseudo);
        element.userAgentShadowRoot()->appendChild(valueContainer);
    }
    updateInnerTextValue();
}

void BaseDateAndTimeInputType::destroyShadowSubtree()
{
    InputType::destroyShadowSubtree();
    m_dateTimeEditElement = nullptr;
}

void BaseDateAndTimeInputType::updateInnerTextValue()
{
    ASSERT(element());
    if (!m_dateTimeEditElement) {
        auto node = element()->userAgentShadowRoot()->firstChild();
        if (!is<HTMLElement>(node))
            return;
        auto displayValue = visibleValue();
        if (displayValue.isEmpty()) {
            // Need to put something to keep text baseline.
            displayValue = " "_s;
        }
        downcast<HTMLElement>(*node).setInnerText(displayValue);
        return;
    }

    DateTimeEditElement::LayoutParameters layoutParameters(element()->locale());

    auto date = parseToDateComponents(element()->value());
    if (date)
        setupLayoutParameters(layoutParameters, *date);
    else {
        if (auto dateForLayout = setMillisecondToDateComponents(createStepRange(AnyStepHandling::Default).minimum().toDouble()))
            setupLayoutParameters(layoutParameters, *dateForLayout);
        else
            setupLayoutParameters(layoutParameters, DateComponents());
    }

    if (!DateTimeFormatValidator().validateFormat(layoutParameters.dateTimeFormat, *this))
        layoutParameters.dateTimeFormat = layoutParameters.fallbackDateTimeFormat;

    if (date)
        m_dateTimeEditElement->setValueAsDate(layoutParameters, *date);
    else
        m_dateTimeEditElement->setEmptyValue(layoutParameters);
}

bool BaseDateAndTimeInputType::hasCustomFocusLogic() const
{
    if (m_dateTimeEditElement)
        return false;
    return InputType::hasCustomFocusLogic();
}

void BaseDateAndTimeInputType::attributeChanged(const QualifiedName& name)
{
    if (name == maxAttr || name == minAttr) {
        if (auto* element = this->element())
            element->invalidateStyleForSubtree();
    } else if (name == valueAttr) {
        if (auto* element = this->element()) {
            if (!element->hasDirtyValue())
                updateInnerTextValue();
        }
    } else if (name == stepAttr && m_dateTimeEditElement)
        updateInnerTextValue();

    InputType::attributeChanged(name);
}

void BaseDateAndTimeInputType::elementDidBlur()
{
    if (!m_dateTimeEditElement)
        closeDateTimeChooser();
}

void BaseDateAndTimeInputType::detach()
{
    closeDateTimeChooser();
}

bool BaseDateAndTimeInputType::isPresentingAttachedView() const
{
    return !!m_dateTimeChooser;
}

auto BaseDateAndTimeInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    ASSERT(element());
    return BaseClickableWithKeyInputType::handleKeydownEvent(*element(), event);
}

void BaseDateAndTimeInputType::handleKeypressEvent(KeyboardEvent& event)
{
    ASSERT(element());
    BaseClickableWithKeyInputType::handleKeypressEvent(*element(), event);
}

void BaseDateAndTimeInputType::handleKeyupEvent(KeyboardEvent& event)
{
    BaseClickableWithKeyInputType::handleKeyupEvent(*this, event);
}

void BaseDateAndTimeInputType::handleFocusEvent(Node* oldFocusedNode, FocusDirection direction)
{
    if (!m_dateTimeEditElement) {
        InputType::handleFocusEvent(oldFocusedNode, direction);
        return;
    }

    // If the element contains editable components, the element itself should not
    // be focused. Instead, one of it's children should receive focus.

    if (direction == FocusDirectionBackward) {
        // If the element received focus when going backwards, advance the focus one more time
        // so that this element no longer has focus. In this case, one of the children should
        // not be focused as the element is losing focus entirely.
        if (auto* page = element()->document().page())
            page->focusController().advanceFocus(direction, 0);
    } else {
        // If the element received focus in any other direction, transfer focus to the first focusable child.
        m_dateTimeEditElement->focusByOwner();
    }
}

bool BaseDateAndTimeInputType::accessKeyAction(bool sendMouseEvents)
{
    InputType::accessKeyAction(sendMouseEvents);
    ASSERT(element());
    return BaseClickableWithKeyInputType::accessKeyAction(*element(), sendMouseEvents);
}

void BaseDateAndTimeInputType::didBlurFromControl()
{
    closeDateTimeChooser();
}

void BaseDateAndTimeInputType::didChangeValueFromControl()
{
    String value = sanitizeValue(m_dateTimeEditElement->value());
    InputType::setValue(value, value != element()->value(), DispatchInputAndChangeEvent);

    DateTimeChooserParameters parameters;
    if (!element()->setupDateTimeChooserParameters(parameters))
        return;

    if (m_dateTimeChooser)
        m_dateTimeChooser->showChooser(parameters);
}

bool BaseDateAndTimeInputType::isEditControlOwnerDisabled() const
{
    ASSERT(element());
    return element()->isDisabledFormControl();
}

bool BaseDateAndTimeInputType::isEditControlOwnerReadOnly() const
{
    ASSERT(element());
    return element()->isReadOnly();
}

AtomString BaseDateAndTimeInputType::localeIdentifier() const
{
    ASSERT(element());
    return element()->computeInheritedLanguage();
}

void BaseDateAndTimeInputType::didChooseValue(StringView value)
{
    ASSERT(element());
    element()->setValue(value.toString(), DispatchInputAndChangeEvent);
}

void BaseDateAndTimeInputType::didEndChooser()
{
    m_dateTimeChooser = nullptr;
}

void BaseDateAndTimeInputType::closeDateTimeChooser()
{
    if (m_dateTimeChooser)
        m_dateTimeChooser->endChooser();
}

} // namespace WebCore
#endif
