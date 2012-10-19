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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeEditElement.h"

#include "DateComponents.h"
#include "DateTimeFieldElements.h"
#include "DateTimeFieldsState.h"
#include "DateTimeFormat.h"
#include "DateTimeSymbolicFieldElement.h"
#include "EventHandler.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "Localizer.h"
#include "MouseEvent.h"
#include "Text.h"
#include <wtf/DateMath.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

class DateTimeEditBuilder : private DateTimeFormat::TokenHandler {
    WTF_MAKE_NONCOPYABLE(DateTimeEditBuilder);

public:
    // The argument objects must be alive until this object dies.
    DateTimeEditBuilder(DateTimeEditElement&, const DateTimeEditElement::LayoutParameters&, const DateComponents&);

    bool build(const String&);

private:
    bool needMillisecondField() const;
    bool shouldMillisecondFieldReadOnly() const;
    bool shouldMinuteFieldReadOnly() const;
    bool shouldSecondFieldReadOnly() const;
    inline const StepRange& stepRange() const { return m_parameters.stepRange; }

    // DateTimeFormat::TokenHandler functions.
    virtual void visitField(DateTimeFormat::FieldType, int) OVERRIDE FINAL;
    virtual void visitLiteral(const String&) OVERRIDE FINAL;

    DateTimeEditElement& m_editElement;
    const DateComponents m_dateValue;
    const DateTimeEditElement::LayoutParameters& m_parameters;
};

DateTimeEditBuilder::DateTimeEditBuilder(DateTimeEditElement& elemnt, const DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& dateValue)
    : m_editElement(elemnt)
    , m_dateValue(dateValue)
    , m_parameters(layoutParameters)
{
}

bool DateTimeEditBuilder::build(const String& formatString)
{
    m_editElement.resetFields();
    return DateTimeFormat::parse(formatString, *this);
}

bool DateTimeEditBuilder::needMillisecondField() const
{
    return m_dateValue.millisecond()
        || !stepRange().minimum().remainder(static_cast<int>(msPerSecond)).isZero()
        || !stepRange().step().remainder(static_cast<int>(msPerSecond)).isZero();
}

void DateTimeEditBuilder::visitField(DateTimeFormat::FieldType fieldType, int count)
{
    // In LDML, M and MM are numeric, and MMM, MMMM, and MMMMM are symbolic.
    const int symbolicMonthThreshold = 3;
    Document* const document = m_editElement.document();

    switch (fieldType) {
    case DateTimeFormat::FieldTypeDayOfMonth:
        m_editElement.addField(DateTimeDayFieldElement::create(document, m_editElement, m_parameters.placeholderForDay));
        return;

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

    case DateTimeFormat::FieldTypeMonth:
        if (count >= symbolicMonthThreshold) {
            // We always use abbreviations.
            m_editElement.addField(DateTimeSymbolicMonthFieldElement::create(document, m_editElement, m_parameters.localizer.shortMonthLabels()));
        } else
            m_editElement.addField(DateTimeMonthFieldElement::create(document, m_editElement, m_parameters.placeholderForMonth));
        return;

    case DateTimeFormat::FieldTypeMonthStandAlone:
        if (count >= symbolicMonthThreshold) {
            // We always use abbreviations.
            m_editElement.addField(DateTimeSymbolicMonthFieldElement::create(document, m_editElement, m_parameters.localizer.shortStandAloneMonthLabels()));
        } else
            m_editElement.addField(DateTimeMonthFieldElement::create(document, m_editElement, m_parameters.placeholderForMonth));
        return;

    case DateTimeFormat::FieldTypePeriod:
        m_editElement.addField(DateTimeAMPMFieldElement::create(document, m_editElement, m_parameters.localizer.timeAMPMLabels()));
        return;

    case DateTimeFormat::FieldTypeSecond: {
        RefPtr<DateTimeSecondFieldElement> field = DateTimeSecondFieldElement::create(document, m_editElement);
        m_editElement.addField(field);
        if (shouldSecondFieldReadOnly())
            field->setReadOnly();

        if (needMillisecondField()) {
            visitLiteral(m_parameters.localizer.localizedDecimalSeparator());
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

    case DateTimeFormat::FieldTypeWeekOfYear:
        m_editElement.addField(DateTimeWeekFieldElement::create(document, m_editElement));
        return;

    case DateTimeFormat::FieldTypeYear: {
        DateTimeYearFieldElement::Parameters yearParams;
        if (m_parameters.minimumYear == m_parameters.undefinedYear()) {
            yearParams.minimumYear = DateComponents::minimumYear();
            yearParams.minIsSpecified = false;
        } else {
            yearParams.minimumYear = m_parameters.minimumYear;
            yearParams.minIsSpecified = true;
        }
        if (m_parameters.maximumYear == m_parameters.undefinedYear()) {
            yearParams.maximumYear = DateComponents::maximumYear();
            yearParams.maxIsSpecified = false;
        } else {
            yearParams.maximumYear = m_parameters.maximumYear;
            yearParams.maxIsSpecified = true;
        }
        if (yearParams.minimumYear > yearParams.maximumYear) {
            std::swap(yearParams.minimumYear, yearParams.maximumYear);
            std::swap(yearParams.minIsSpecified, yearParams.maxIsSpecified);
        }
        yearParams.placeholder = m_parameters.placeholderForYear;
        m_editElement.addField(DateTimeYearFieldElement::create(document, m_editElement, yearParams));
        return;
    }

    default:
        return;
    }
}

bool DateTimeEditBuilder::shouldMillisecondFieldReadOnly() const
{
    return !m_dateValue.millisecond() && stepRange().step().remainder(static_cast<int>(msPerSecond)).isZero();
}

bool DateTimeEditBuilder::shouldMinuteFieldReadOnly() const
{
    return !m_dateValue.minute() && stepRange().step().remainder(static_cast<int>(msPerHour)).isZero();
}

bool DateTimeEditBuilder::shouldSecondFieldReadOnly() const
{
    return !m_dateValue.second() && stepRange().step().remainder(static_cast<int>(msPerMinute)).isZero();
}

void DateTimeEditBuilder::visitLiteral(const String& text)
{
    DEFINE_STATIC_LOCAL(AtomicString, textPseudoId, ("-webkit-datetime-edit-text", AtomicString::ConstructFromLiteral));
    ASSERT(text.length());
    RefPtr<HTMLDivElement> element = HTMLDivElement::create(m_editElement.document());
    element->setShadowPseudoId(textPseudoId);
    element->appendChild(Text::create(m_editElement.document(), text));
    m_editElement.appendChild(element);
}

// ----------------------------

DateTimeEditElement::EditControlOwner::~EditControlOwner()
{
}

DateTimeEditElement::DateTimeEditElement(Document* document, EditControlOwner& editControlOwner)
    : HTMLDivElement(divTag, document)
    , m_editControlOwner(&editControlOwner)
    , m_spinButton(0)
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

void DateTimeEditElement::blurByOwner()
{
    if (DateTimeFieldElement* field = focusedField())
        field->blur();
}

PassRefPtr<DateTimeEditElement> DateTimeEditElement::create(Document* document, EditControlOwner& editControlOwner)
{
    RefPtr<DateTimeEditElement> container = adoptRef(new DateTimeEditElement(document, editControlOwner));
    return container.release();
}

void DateTimeEditElement::didBlurFromField()
{
    if (m_editControlOwner)
        m_editControlOwner->didBlurFromControl();
}

void DateTimeEditElement::didFocusOnField()
{
    if (m_editControlOwner)
        m_editControlOwner->didFocusOnControl();
}

void DateTimeEditElement::disabledStateChanged()
{
    updateUIState();
}

DateTimeFieldElement* DateTimeEditElement::fieldAt(size_t fieldIndex) const
{
    return fieldIndex < m_fields.size() ? m_fields[fieldIndex] : 0;
}

size_t DateTimeEditElement::fieldIndexOf(const DateTimeFieldElement& field) const
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
        if (m_fields[fieldIndex] == &field)
            return fieldIndex;
    }
    return invalidFieldIndex;
}

void DateTimeEditElement::focusAndSelectSpinButtonOwner()
{
    if (focusedFieldIndex() != invalidFieldIndex)
        return;

    if (DateTimeFieldElement* field = fieldAt(0))
        field->focus();
}

void DateTimeEditElement::focusByOwner()
{
    if (DateTimeFieldElement* field = fieldAt(0))
        field->focus();
}

DateTimeFieldElement* DateTimeEditElement::focusedField() const
{
    return fieldAt(focusedFieldIndex());
}

size_t DateTimeEditElement::focusedFieldIndex() const
{
    Node* const focusedFieldNode = document()->focusedNode();
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
        if (m_fields[fieldIndex] == focusedFieldNode)
            return fieldIndex;
    }
    return invalidFieldIndex;
}

void DateTimeEditElement::fieldValueChanged()
{
    if (m_editControlOwner)
        m_editControlOwner->editControlValueChanged();
}

bool DateTimeEditElement::focusOnNextField(const DateTimeFieldElement& field)
{
    const size_t startFieldIndex = fieldIndexOf(field);
    if (startFieldIndex == invalidFieldIndex)
        return false;
    for (size_t fieldIndex = startFieldIndex + 1; fieldIndex < m_fields.size(); ++fieldIndex) {
        if (!m_fields[fieldIndex]->isReadOnly()) {
            m_fields[fieldIndex]->focus();
            return true;
        }
    }
    return false;
}

bool DateTimeEditElement::focusOnPreviousField(const DateTimeFieldElement& field)
{
    const size_t startFieldIndex = fieldIndexOf(field);
    if (startFieldIndex == invalidFieldIndex)
        return false;
    size_t fieldIndex = startFieldIndex;
    while (fieldIndex > 0) {
        --fieldIndex;
        if (!m_fields[fieldIndex]->isReadOnly()) {
            m_fields[fieldIndex]->focus();
            return true;
        }
    }
    return false;
}

bool DateTimeEditElement::isDisabled() const
{
    return m_editControlOwner && m_editControlOwner->isEditControlOwnerDisabled();
}

bool DateTimeEditElement::isFieldOwnerDisabledOrReadOnly() const
{
    return isDisabled() || isReadOnly();
}

bool DateTimeEditElement::isReadOnly() const
{
    return m_editControlOwner && m_editControlOwner->isEditControlOwnerReadOnly();
}

void DateTimeEditElement::layout(const LayoutParameters& layoutParameters, const DateComponents& dateValue)
{
    size_t focusedFieldIndex = this->focusedFieldIndex();
    DateTimeFieldElement* const focusedField = fieldAt(focusedFieldIndex);
    const AtomicString focusedFieldId = focusedField ? focusedField->shadowPseudoId() : nullAtom;

    DateTimeEditBuilder builder(*this, layoutParameters, dateValue);
    Node* lastChildToBeRemoved = lastChild();
    if (!builder.build(layoutParameters.dateTimeFormat) || m_fields.isEmpty()) {
        lastChildToBeRemoved = lastChild();
        builder.build(layoutParameters.fallbackDateTimeFormat);
    }

    if (focusedFieldIndex != invalidFieldIndex) {
        for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
            if (m_fields[fieldIndex]->shadowPseudoId() == focusedFieldId) {
                focusedFieldIndex = fieldIndex;
                break;
            }
        }
        if (DateTimeFieldElement* field = fieldAt(std::min(focusedFieldIndex, m_fields.size() - 1)))
            field->focus();
    }

    if (lastChildToBeRemoved) {
        for (Node* childNode = firstChild(); childNode; childNode = firstChild()) {
            removeChild(childNode);
            if (childNode == lastChildToBeRemoved)
                break;
        }
    }

    DEFINE_STATIC_LOCAL(AtomicString, gapPseudoId, ("-webkit-datetime-edit-gap", AtomicString::ConstructFromLiteral));
    RefPtr<HTMLDivElement> gapElement = HTMLDivElement::create(document());
    gapElement->setShadowPseudoId(gapPseudoId);
    appendChild(gapElement);
    RefPtr<SpinButtonElement> spinButton = SpinButtonElement::create(document(), *this);
    m_spinButton = spinButton.get();
    appendChild(spinButton);
}

AtomicString DateTimeEditElement::localeIdentifier() const
{
    return m_editControlOwner ? m_editControlOwner->localeIdentifier() : nullAtom;
}

void DateTimeEditElement::readOnlyStateChanged()
{
    updateUIState();
}

void DateTimeEditElement::resetFields()
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->removeEventHandler();
    m_fields.shrink(0);
}

void DateTimeEditElement::defaultEventHandler(Event* event)
{
    // In case of control owner forward event to control, e.g. DOM
    // dispatchEvent method.
    if (DateTimeFieldElement* field = focusedField()) {
        field->defaultEventHandler(event);
        if (event->defaultHandled())
            return;
    }

    if (m_spinButton) {
        m_spinButton->forwardEvent(event);
        if (event->defaultHandled())
            return;
    }

    HTMLDivElement::defaultEventHandler(event);
}

void DateTimeEditElement::setValueAsDate(const LayoutParameters& layoutParameters, const DateComponents& date)
{
    layout(layoutParameters, date);
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setValueAsDate(date);
}

void DateTimeEditElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState, const DateComponents& dateForReadOnlyField)
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setValueAsDateTimeFieldsState(dateTimeFieldsState, dateForReadOnlyField);
}

void DateTimeEditElement::setEmptyValue(const LayoutParameters& layoutParameters, const DateComponents& dateForReadOnlyField)
{
    layout(layoutParameters, dateForReadOnlyField);
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setEmptyValue(dateForReadOnlyField, DateTimeFieldElement::DispatchNoEvent);
}

bool DateTimeEditElement::shouldSpinButtonRespondToMouseEvents()
{
    return !isDisabled() && !isReadOnly();
}

bool DateTimeEditElement::shouldSpinButtonRespondToWheelEvents()
{
    if (!shouldSpinButtonRespondToMouseEvents())
        return false;

    return focusedFieldIndex() != invalidFieldIndex;
}

void DateTimeEditElement::spinButtonStepDown()
{
    if (DateTimeFieldElement* const field = focusedField())
        field->stepDown();
}

void DateTimeEditElement::spinButtonStepUp()
{
    if (DateTimeFieldElement* const field = focusedField())
        field->stepUp();
}

void DateTimeEditElement::updateUIState()
{
    if (isDisabled() || isReadOnly()) {
        m_spinButton->releaseCapture();
        if (DateTimeFieldElement* field = focusedField())
            field->blur();
    }
}

String DateTimeEditElement::value() const
{
    if (!m_editControlOwner)
        return emptyString();
    return m_editControlOwner->formatDateTimeFieldsState(valueAsDateTimeFieldsState());
}

DateTimeFieldsState DateTimeEditElement::valueAsDateTimeFieldsState() const
{
    DateTimeFieldsState dateTimeFieldsState;
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->populateDateTimeFieldsState(dateTimeFieldsState);
    return dateTimeFieldsState;
}

} // namespace WebCore

#endif
