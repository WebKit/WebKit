/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include "DateTimeEditElement.h"

#include "DateComponents.h"
#include "DateTimeFieldElements.h"
#include "DateTimeFormat.h"
#include "DateTimeSymbolicFieldElement.h"
#include "EventHandler.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedDate.h"
#include "LocalizedNumber.h"
#include "MouseEvent.h"
#include "StepRange.h"
#include "Text.h"
#include <wtf/DateMath.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

class DateTimeEditBuilder : private DateTimeFormat::TokenHandler {
    WTF_MAKE_NONCOPYABLE(DateTimeEditBuilder);

public:
    DateTimeEditBuilder(DateTimeEditElement&, const StepRange&);

    bool build(const String&);
    bool needSecondField() const;

private:
    bool needMillisecondField() const;
    bool needMinuteField() const;
    bool shouldMillisecondFieldReadOnly() const;
    bool shouldMinuteFieldReadOnly() const;
    bool shouldSecondFieldReadOnly() const;

    // DateTimeFormat::TokenHandler functions.
    virtual void visitField(DateTimeFormat::FieldType, int) OVERRIDE FINAL;
    virtual void visitLiteral(const String&) OVERRIDE FINAL;

    DateTimeEditElement& m_editElement;
    const StepRange& m_stepRange;
};

DateTimeEditBuilder::DateTimeEditBuilder(DateTimeEditElement& elemnt, const StepRange& stepRange)
    : m_editElement(elemnt)
    , m_stepRange(stepRange)
{
}

bool DateTimeEditBuilder::build(const String& formatString)
{
    m_editElement.resetLayout();
    return DateTimeFormat::parse(formatString, *this);
}

bool DateTimeEditBuilder::needMillisecondField() const
{
    return !m_stepRange.minimum().remainder(static_cast<int>(msPerSecond)).isZero()
        || !m_stepRange.step().remainder(static_cast<int>(msPerSecond)).isZero();
}

bool DateTimeEditBuilder::needMinuteField() const
{
    return !m_stepRange.minimum().remainder(static_cast<int>(msPerHour)).isZero()
        || !m_stepRange.step().remainder(static_cast<int>(msPerHour)).isZero();
}

bool DateTimeEditBuilder::needSecondField() const
{
    return !m_stepRange.minimum().remainder(static_cast<int>(msPerMinute)).isZero()
        || !m_stepRange.step().remainder(static_cast<int>(msPerMinute)).isZero();
}

void DateTimeEditBuilder::visitField(DateTimeFormat::FieldType fieldType, int)
{
    Document* const document = m_editElement.document();

    switch (fieldType) {
    case DateTimeFormat::FieldTypeHour11:
        m_editElement.addField(DateTimeHourFieldElement::create(document, m_editElement, 0, 11));
        return;

    case DateTimeFormat::FieldTypeHour12:
        m_editElement.addField(DateTimeHourFieldElement::create(document, m_editElement, 1, 12));
        return;

    case DateTimeFormat::FieldTypeHour23:
        m_editElement.addField(DateTimeHourFieldElement::create(document, m_editElement, 0, 23));
        return;

    case DateTimeFormat::FieldTypeHour24:
        m_editElement.addField(DateTimeHourFieldElement::create(document, m_editElement, 1, 24));
        return;

    case DateTimeFormat::FieldTypeMinute: {
        RefPtr<DateTimeNumericFieldElement> field = DateTimeMinuteFieldElement::create(document, m_editElement);
        m_editElement.addField(field);
        if (shouldMinuteFieldReadOnly())
            field->setReadOnly();
        return;
    }

    case DateTimeFormat::FieldTypePeriod:
        m_editElement.addField(DateTimeAMPMFieldElement::create(document, m_editElement, timeAMPMLabels()));
        return;

    case DateTimeFormat::FieldTypeSecond: {
        RefPtr<DateTimeSecondFieldElement> field = DateTimeSecondFieldElement::create(document, m_editElement);
        m_editElement.addField(field);
        if (shouldSecondFieldReadOnly())
            field->setReadOnly();

        if (needMillisecondField()) {
            visitLiteral(localizedDecimalSeparator());
            visitField(DateTimeFormat::FieldTypeFractionalSecond, 3);
        }
        return;
    }

    case DateTimeFormat::FieldTypeFractionalSecond: {
        RefPtr<DateTimeMillisecondFieldElement> field = DateTimeMillisecondFieldElement::create(document, m_editElement);
        m_editElement.addField(field);
        if (shouldMillisecondFieldReadOnly())
            field->setReadOnly();
        return;
    }

    default:
        return;
    }
}

bool DateTimeEditBuilder::shouldMillisecondFieldReadOnly() const
{
    return m_stepRange.step().remainder(static_cast<int>(msPerSecond)).isZero();
}

bool DateTimeEditBuilder::shouldMinuteFieldReadOnly() const
{
    return m_stepRange.step().remainder(static_cast<int>(msPerHour)).isZero();
}

bool DateTimeEditBuilder::shouldSecondFieldReadOnly() const
{
    return m_stepRange.step().remainder(static_cast<int>(msPerMinute)).isZero();
}

void DateTimeEditBuilder::visitLiteral(const String& text)
{
    ASSERT(text.length());
    m_editElement.appendChild(Text::create(m_editElement.document(), text));
}

// ----------------------------

DateTimeEditElement::EditControlOwner::~EditControlOwner()
{
}

DateTimeEditElement::DateTimeEditElement(Document* document, EditControlOwner& editControlOwner)
    : HTMLDivElement(divTag, document)
    , m_editControlOwner(&editControlOwner)
    , m_spinButton(0)
    , m_focusFieldIndex(invalidFieldIndex)
{
    DEFINE_STATIC_LOCAL(AtomicString, dateTimeEditPseudoId, ("-webkit-datetime-edit"));
    setShadowPseudoId(dateTimeEditPseudoId);
}

DateTimeEditElement::~DateTimeEditElement()
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->removeEventHandler();

    if (m_spinButton)
        m_spinButton->removeSpinButtonOwner();
}

void DateTimeEditElement::addField(PassRefPtr<DateTimeFieldElement> field)
{
    if (m_fields.size() == m_fields.capacity())
        return;
    m_fields.append(field.get());
    appendChild(field);
}

PassRefPtr<DateTimeEditElement> DateTimeEditElement::create(Document* document, EditControlOwner& editControlOwner, const StepRange& stepRange)
{
    RefPtr<DateTimeEditElement> container = adoptRef(new DateTimeEditElement(document, editControlOwner));
    container->layout(stepRange);
    return container.release();
}

void DateTimeEditElement::disabledStateChanged()
{
    updateUIState();
}

DateTimeFieldElement* DateTimeEditElement::fieldAt(size_t fieldIndex) const
{
    return fieldIndex < m_fields.size() ? m_fields[fieldIndex] : 0;
}

void DateTimeEditElement::focusFieldAt(size_t newFocusFieldIndex)
{
    if (m_focusFieldIndex == newFocusFieldIndex)
        return;

    DateTimeFieldElement* const newFocusField = fieldAt(newFocusFieldIndex);
    if (newFocusField && newFocusField->isReadOnly())
        return;

    DateTimeFieldElement* const currentFocusField = fieldAt(m_focusFieldIndex);

    if (currentFocusField)
        currentFocusField->setFocus(false);

    if (!newFocusField) {
        m_focusFieldIndex = invalidFieldIndex;
        return;
    }

    m_focusFieldIndex = newFocusFieldIndex;
    newFocusField->setFocus(true);
}

void DateTimeEditElement::handleKeyboardEvent(KeyboardEvent* keyboardEvent)
{
    if (isDisabled() || isReadOnly())
        return;

    if (!fieldAt(m_focusFieldIndex))
        return;

    if (keyboardEvent->type() != eventNames().keydownEvent)
        return;

    const String& keyIdentifier = keyboardEvent->keyIdentifier();

    if (keyIdentifier == "Left") {
        keyboardEvent->setDefaultHandled();
        const size_t fieldIndex = previousFieldIndex();
        if (fieldAt(fieldIndex))
            focusFieldAt(fieldIndex);
        return;
    }

    if (keyIdentifier == "Right") {
        keyboardEvent->setDefaultHandled();
        const size_t fieldIndex = nextFieldIndex();
        if (fieldAt(fieldIndex))
            focusFieldAt(fieldIndex);
        return;
    }

    if (keyIdentifier == "U+0009") {
        const size_t fieldIndex = keyboardEvent->getModifierState("Shift") ? previousFieldIndex() : nextFieldIndex();
        if (fieldAt(fieldIndex)) {
            keyboardEvent->setDefaultHandled();
            focusFieldAt(fieldIndex);
            return;
        }
    }
}

void DateTimeEditElement::fieldValueChanged()
{
    if (m_editControlOwner)
        m_editControlOwner->editControlValueChanged();
}

void DateTimeEditElement::focusOnNextField()
{
    if (m_focusFieldIndex != invalidFieldIndex)
        focusFieldAt(nextFieldIndex());
}

void DateTimeEditElement::handleMouseEvent(MouseEvent* mouseEvent)
{
    if (isDisabled() || isReadOnly())
        return;

    if (mouseEvent->type() != eventNames().mousedownEvent || mouseEvent->button() != LeftButton)
        return;

    Node* const relatedTarget = mouseEvent->target()->toNode();
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
        if (m_fields[fieldIndex] == relatedTarget) {
            mouseEvent->setDefaultHandled();
            focusFieldAt(fieldIndex);
            if (m_editControlOwner)
                m_editControlOwner->editControlMouseFocus();
            break;
        }
    }
}

bool DateTimeEditElement::isDisabled() const
{
    return m_editControlOwner && m_editControlOwner->isEditControlOwnerDisabled();
}

bool DateTimeEditElement::isReadOnly() const
{
    return m_editControlOwner && m_editControlOwner->isEditControlOwnerReadOnly();
}

void DateTimeEditElement::layout(const StepRange& stepRange)
{
    DateTimeFieldElement* const focusField = fieldAt(m_focusFieldIndex);
    focusFieldAt(invalidFieldIndex);

    DateTimeEditBuilder builder(*this, stepRange);
    const String dateTimeFormat = builder.needSecondField() ? localizedTimeFormatText() : localizedShortTimeFormatText();
    if (!builder.build(dateTimeFormat) || m_fields.isEmpty())
        builder.build(builder.needSecondField() ? "HH:mm:ss" : "HH:mm");

    RefPtr<SpinButtonElement> spinButton = SpinButtonElement::create(document(), *this);
    m_spinButton = spinButton.get();
    appendChild(spinButton);

    if (focusField) {
        for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
            if (focusField == m_fields[fieldIndex]) {
                focusFieldAt(fieldIndex);
                break;
            }
        }
    }
}

size_t DateTimeEditElement::nextFieldIndex() const
{
    ASSERT(m_focusFieldIndex != invalidFieldIndex);
    for (size_t fieldIndex = m_focusFieldIndex + 1; fieldIndex < m_fields.size(); ++fieldIndex) {
        if (!m_fields[fieldIndex]->isReadOnly())
            return fieldIndex;
    }
    return m_fields.size();
}

size_t DateTimeEditElement::previousFieldIndex() const
{
    ASSERT(m_focusFieldIndex != invalidFieldIndex);
    size_t fieldIndex = m_focusFieldIndex;
    while (fieldIndex > 0) {
        --fieldIndex;
        if (!m_fields[fieldIndex]->isReadOnly())
            return fieldIndex;
    }
    return invalidFieldIndex;
}

void DateTimeEditElement::readOnlyStateChanged()
{
    updateUIState();
}

void DateTimeEditElement::resetLayout()
{
    m_fields.shrink(0);
    m_spinButton = 0;
    m_focusFieldIndex = invalidFieldIndex;
    removeChildren();
}

void DateTimeEditElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().focusEvent) {
        if (!isDisabled() && !isReadOnly() && m_focusFieldIndex == invalidFieldIndex)
            focusFieldAt(0);
        return;
    }

    if (event->type() == eventNames().blurEvent) {
        focusFieldAt(invalidFieldIndex);
        return;
    }

    if (event->isMouseEvent()) {
        handleMouseEvent(static_cast<MouseEvent*>(event));
    } else if (event->isKeyboardEvent())
        handleKeyboardEvent(static_cast<KeyboardEvent*>(event));

    if (event->defaultHandled())
        return;

    DateTimeFieldElement* const focusField = fieldAt(m_focusFieldIndex);
    if (!focusField)
        return;

    if (m_spinButton) {
        m_spinButton->forwardEvent(event);
        if (event->defaultHandled())
            return;
    }

    focusField->defaultEventHandler(event);
}

void DateTimeEditElement::setValueAsDate(const DateComponents& date)
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setValueAsDate(date);
}

void DateTimeEditElement::setEmptyValue(const DateComponents& dateForReadOnlyField)
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setEmptyValue(dateForReadOnlyField, DateTimeFieldElement::DispatchNoEvent);
}

void DateTimeEditElement::spinButtonStepDown()
{
    if (DateTimeFieldElement* const focusField = fieldAt(m_focusFieldIndex))
        focusField->stepDown();
}

void DateTimeEditElement::spinButtonStepUp()
{
    if (DateTimeFieldElement* const focusField = fieldAt(m_focusFieldIndex))
        focusField->stepUp();
}

void DateTimeEditElement::updateUIState()
{
    if (isDisabled() || isReadOnly()) {
        m_spinButton->releaseCapture();
        focusFieldAt(invalidFieldIndex);
    }
}

double DateTimeEditElement::valueAsDouble() const
{
    double value = 0;

    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
        const DateTimeFieldElement* const field = m_fields[fieldIndex];
        if (!field->hasValue())
            return std::numeric_limits<double>::quiet_NaN();
        value += field->valueAsDouble();
    }

    return value;
}

} // namespace WebCore

#endif
