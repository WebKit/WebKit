/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "BaseChooserOnlyDateAndTimeInputType.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "Chrome.h"
#include "DateTimeChooserParameters.h"
#include "DateTimeFormat.h"
#include "FocusController.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "Page.h"
#include "PlatformLocale.h"
#include "RenderElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "UserGestureIndicator.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class DateTimeFormatValidator final : public DateTimeFormat::TokenHandler {
public:
    DateTimeFormatValidator() { }

    void visitField(DateTimeFormat::FieldType, int);
    void visitLiteral(const String&) { }

    bool validateFormat(const String& format, const BaseChooserOnlyDateAndTimeInputType&);

private:
    OptionSet<DateTimeFormatValidationResults> m_results;
};

void DateTimeFormatValidator::visitField(DateTimeFormat::FieldType fieldType, int)
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
        m_results.add(DateTimeFormatValidationResults::HasAMPM);
        break;

    case DateTimeFormat::FieldTypeHour11:
    case DateTimeFormat::FieldTypeHour12:
        m_results.add(DateTimeFormatValidationResults::HasHour);
        break;

    case DateTimeFormat::FieldTypeHour23:
    case DateTimeFormat::FieldTypeHour24:
        m_results.add(DateTimeFormatValidationResults::HasHour);
        m_results.add(DateTimeFormatValidationResults::HasAMPM);
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

bool DateTimeFormatValidator::validateFormat(const String& format, const BaseChooserOnlyDateAndTimeInputType& inputType)
{
    if (!DateTimeFormat::parse(format, *this))
        return false;
    return inputType.isValidFormat(m_results);
}

BaseChooserOnlyDateAndTimeInputType::~BaseChooserOnlyDateAndTimeInputType()
{
    closeDateTimeChooser();
}

void BaseChooserOnlyDateAndTimeInputType::handleDOMActivateEvent(Event&)
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

void BaseChooserOnlyDateAndTimeInputType::elementDidBlur()
{
    closeDateTimeChooser();
}

bool BaseChooserOnlyDateAndTimeInputType::isPresentingAttachedView() const
{
    return !!m_dateTimeChooser;
}

void BaseChooserOnlyDateAndTimeInputType::createShadowSubtree()
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

void BaseChooserOnlyDateAndTimeInputType::destroyShadowSubtree()
{
    InputType::destroyShadowSubtree();
    m_dateTimeEditElement = nullptr;
}

void BaseChooserOnlyDateAndTimeInputType::updateInnerTextValue()
{
    ASSERT(element());
    if (!m_dateTimeEditElement) {
        auto node = element()->userAgentShadowRoot()->firstChild();
        if (!is<HTMLElement>(node))
            return;
        String displayValue = visibleValue();
        if (displayValue.isEmpty()) {
            // Need to put something to keep text baseline.
            displayValue = " "_s;
        }
        downcast<HTMLElement>(*node).setInnerText(displayValue);
        return;
    }

    DateTimeEditElement::LayoutParameters layoutParameters(element()->locale());
    setupLayoutParameters(layoutParameters);

    if (!DateTimeFormatValidator().validateFormat(layoutParameters.dateTimeFormat, *this))
        layoutParameters.dateTimeFormat = layoutParameters.fallbackDateTimeFormat;

    if (auto date = parseToDateComponents(element()->value()))
        m_dateTimeEditElement->setValueAsDate(layoutParameters, *date);
    else
        m_dateTimeEditElement->setEmptyValue(layoutParameters);
}

void BaseChooserOnlyDateAndTimeInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    BaseDateAndTimeInputType::setValue(value, valueChanged, eventBehavior);
    if (valueChanged)
        updateInnerTextValue();
}

void BaseChooserOnlyDateAndTimeInputType::detach()
{
    closeDateTimeChooser();
}

void BaseChooserOnlyDateAndTimeInputType::didChooseValue(StringView value)
{
    ASSERT(element());
    element()->setValue(value.toString(), DispatchInputAndChangeEvent);
}

void BaseChooserOnlyDateAndTimeInputType::didEndChooser()
{
    m_dateTimeChooser = nullptr;
}

void BaseChooserOnlyDateAndTimeInputType::closeDateTimeChooser()
{
    if (m_dateTimeChooser)
        m_dateTimeChooser->endChooser();
}

auto BaseChooserOnlyDateAndTimeInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    ASSERT(element());
    return BaseClickableWithKeyInputType::handleKeydownEvent(*element(), event);
}

void BaseChooserOnlyDateAndTimeInputType::handleKeypressEvent(KeyboardEvent& event)
{
    ASSERT(element());
    BaseClickableWithKeyInputType::handleKeypressEvent(*element(), event);
}

void BaseChooserOnlyDateAndTimeInputType::handleKeyupEvent(KeyboardEvent& event)
{
    BaseClickableWithKeyInputType::handleKeyupEvent(*this, event);
}

void BaseChooserOnlyDateAndTimeInputType::handleFocusEvent(Node* oldFocusedNode, FocusDirection direction)
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

bool BaseChooserOnlyDateAndTimeInputType::accessKeyAction(bool sendMouseEvents)
{
    BaseDateAndTimeInputType::accessKeyAction(sendMouseEvents);
    ASSERT(element());
    return BaseClickableWithKeyInputType::accessKeyAction(*element(), sendMouseEvents);
}

bool BaseChooserOnlyDateAndTimeInputType::isMouseFocusable() const
{
    ASSERT(element());
    return element()->isTextFormControlFocusable();
}

bool BaseChooserOnlyDateAndTimeInputType::hasCustomFocusLogic() const
{
    if (m_dateTimeEditElement)
        return false;
    return InputType::hasCustomFocusLogic();
}

void BaseChooserOnlyDateAndTimeInputType::attributeChanged(const QualifiedName& name)
{
    if (name == valueAttr) {
        if (auto* element = this->element()) {
            if (!element->hasDirtyValue())
                updateInnerTextValue();
        }
    }
    BaseDateAndTimeInputType::attributeChanged(name);
}

void BaseChooserOnlyDateAndTimeInputType::didBlurFromControl()
{
    closeDateTimeChooser();
}

void BaseChooserOnlyDateAndTimeInputType::didChangeValueFromControl()
{
    String value = sanitizeValue(m_dateTimeEditElement->value());
    BaseDateAndTimeInputType::setValue(value, value != element()->value(), DispatchInputAndChangeEvent);

    DateTimeChooserParameters parameters;
    if (!element()->setupDateTimeChooserParameters(parameters))
        return;

    if (m_dateTimeChooser)
        m_dateTimeChooser->showChooser(parameters);
}

AtomString BaseChooserOnlyDateAndTimeInputType::localeIdentifier() const
{
    ASSERT(element());
    return element()->computeInheritedLanguage();
}

}

#endif
