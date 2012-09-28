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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "BaseMultipleFieldsDateAndTimeInputType.h"

#include "DateComponents.h"
#include "DateTimeFieldsState.h"
#include "ElementShadow.h"
#include "FormController.h"
#include "HTMLInputElement.h"
#include "KeyboardEvent.h"
#include "Localizer.h"
#include "ShadowRoot.h"

namespace WebCore {

void BaseMultipleFieldsDateAndTimeInputType::didBlurFromControl()
{
    // We don't need to call blur(). This function is called when control
    // lost focus.

    // Remove focus ring by CSS "focus" pseudo class.
    element()->setFocus(false);
}

void BaseMultipleFieldsDateAndTimeInputType::didFocusOnControl()
{
    // We don't need to call focus(). This function is called when control
    // got focus.

    // Add focus ring by CSS "focus" pseudo class.
    element()->setFocus(true);
}

void BaseMultipleFieldsDateAndTimeInputType::editControlValueChanged()
{
    RefPtr<HTMLInputElement> input(element());
    input->setValueInternal(m_dateTimeEditElement->value(), DispatchNoEvent);
    input->setNeedsStyleRecalc();
    input->dispatchFormControlInputEvent();
    input->dispatchFormControlChangeEvent();
    input->notifyFormStateChanged();
}

bool BaseMultipleFieldsDateAndTimeInputType::hasCustomFocusLogic() const
{
    return false;
}

bool BaseMultipleFieldsDateAndTimeInputType::isEditControlOwnerDisabled() const
{
    return element()->readOnly();
}

bool BaseMultipleFieldsDateAndTimeInputType::isEditControlOwnerReadOnly() const
{
    return element()->disabled();
}

BaseMultipleFieldsDateAndTimeInputType::BaseMultipleFieldsDateAndTimeInputType(HTMLInputElement* element)
    : BaseDateAndTimeInputType(element)
    , m_dateTimeEditElement(0)
{
}

BaseMultipleFieldsDateAndTimeInputType::~BaseMultipleFieldsDateAndTimeInputType()
{
    if (m_dateTimeEditElement)
        m_dateTimeEditElement->removeEditControlOwner();
}

void BaseMultipleFieldsDateAndTimeInputType::blur()
{
    if (m_dateTimeEditElement)
        m_dateTimeEditElement->blurByOwner();
}

RenderObject* BaseMultipleFieldsDateAndTimeInputType::createRenderer(RenderArena* arena, RenderStyle* style) const
{
    return InputType::createRenderer(arena, style);
}

void BaseMultipleFieldsDateAndTimeInputType::createShadowSubtree()
{
    ASSERT(element()->shadow());

    RefPtr<DateTimeEditElement> dateTimeEditElement(DateTimeEditElement::create(element()->document(), *this));
    m_dateTimeEditElement = dateTimeEditElement.get();
    element()->userAgentShadowRoot()->appendChild(m_dateTimeEditElement);
    updateInnerTextValue();
}

void BaseMultipleFieldsDateAndTimeInputType::destroyShadowSubtree()
{
    if (m_dateTimeEditElement) {
        m_dateTimeEditElement->removeEditControlOwner();
        m_dateTimeEditElement = 0;
    }
    BaseDateAndTimeInputType::destroyShadowSubtree();
}

void BaseMultipleFieldsDateAndTimeInputType::focus(bool)
{
    if (m_dateTimeEditElement)
        m_dateTimeEditElement->focusByOwner();
}

void BaseMultipleFieldsDateAndTimeInputType::forwardEvent(Event* event)
{
    if (m_dateTimeEditElement)
        m_dateTimeEditElement->defaultEventHandler(event);
}

void BaseMultipleFieldsDateAndTimeInputType::disabledAttributeChanged()
{
    if (m_dateTimeEditElement)
        m_dateTimeEditElement->disabledStateChanged();
}

void BaseMultipleFieldsDateAndTimeInputType::handleKeydownEvent(KeyboardEvent* event)
{
    forwardEvent(event);
}

bool BaseMultipleFieldsDateAndTimeInputType::isKeyboardFocusable(KeyboardEvent*) const
{
    return false;
}

bool BaseMultipleFieldsDateAndTimeInputType::isMouseFocusable() const
{
    return false;
}

AtomicString BaseMultipleFieldsDateAndTimeInputType::localeIdentifier() const
{
    return element()->computeInheritedLanguage();
}

void BaseMultipleFieldsDateAndTimeInputType::minOrMaxAttributeChanged()
{
    updateInnerTextValue();
}

void BaseMultipleFieldsDateAndTimeInputType::readonlyAttributeChanged()
{
    if (m_dateTimeEditElement)
        m_dateTimeEditElement->readOnlyStateChanged();
}

bool BaseMultipleFieldsDateAndTimeInputType::isTextField() const
{
    return false;
}

void BaseMultipleFieldsDateAndTimeInputType::restoreFormControlState(const FormControlState& state)
{
    if (!m_dateTimeEditElement)
        return;
    DateComponents date;
    setMillisecondToDateComponents(createStepRange(AnyIsDefaultStep).minimum().toDouble(), &date);
    DateTimeFieldsState dateTimeFieldsState = DateTimeFieldsState::restoreFormControlState(state);
    m_dateTimeEditElement->setValueAsDateTimeFieldsState(dateTimeFieldsState, date);
    element()->setValueInternal(m_dateTimeEditElement->value(), DispatchNoEvent);
}

FormControlState BaseMultipleFieldsDateAndTimeInputType::saveFormControlState() const
{
    if (!m_dateTimeEditElement)
        return FormControlState();

    return m_dateTimeEditElement->valueAsDateTimeFieldsState().saveFormControlState();
}

void BaseMultipleFieldsDateAndTimeInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    InputType::setValue(sanitizedValue, valueChanged, eventBehavior);
    if (valueChanged)
        updateInnerTextValue();
}

bool BaseMultipleFieldsDateAndTimeInputType::shouldUseInputMethod() const
{
    return false;
}

void BaseMultipleFieldsDateAndTimeInputType::stepAttributeChanged()
{
    updateInnerTextValue();
}

void BaseMultipleFieldsDateAndTimeInputType::updateInnerTextValue()
{
    if (!m_dateTimeEditElement)
        return;

    Localizer& localizer = element()->document()->getLocalizer(element()->computeInheritedLanguage());
    DateTimeEditElement::LayoutParameters layoutParameters(localizer, createStepRange(AnyIsDefaultStep));

    DateComponents date;
    const bool hasValue = parseToDateComponents(element()->value(), &date);
    if (!hasValue)
        setMillisecondToDateComponents(layoutParameters.stepRange.minimum().toDouble(), &date);

    setupLayoutParameters(layoutParameters, date);

    if (hasValue)
        m_dateTimeEditElement->setValueAsDate(layoutParameters, date);
    else
        m_dateTimeEditElement->setEmptyValue(layoutParameters, date);
}

} // namespace WebCore

#endif
