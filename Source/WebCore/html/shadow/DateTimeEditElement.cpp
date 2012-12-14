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
#include "FontCache.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformLocale.h"
#include "RenderStyle.h"
#include "StyleResolver.h"
#include "Text.h"
#include <wtf/DateMath.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

class DateTimeEditBuilder : private DateTimeFormat::TokenHandler {
    WTF_MAKE_NONCOPYABLE(DateTimeEditBuilder);

public:
    // The argument objects must be alive until this object dies.
    DateTimeEditBuilder(DateTimeEditElement&, const DateTimeEditElement::LayoutParameters&, const DateComponents&);

    bool build(const String&);

private:
    bool needMillisecondField() const;
    bool shouldHourFieldReadOnly() const;
    bool shouldMillisecondFieldReadOnly() const;
    bool shouldMinuteFieldReadOnly() const;
    bool shouldSecondFieldReadOnly() const;
    inline const StepRange& stepRange() const { return m_parameters.stepRange; }
    DateTimeNumericFieldElement::Parameters createNumericFieldParameters(const Decimal& msPerFieldUnit, const Decimal& msPerFieldSize) const;

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
    const int countForAbbreviatedMonth = 3;
    const int countForFullMonth = 4;
    const int countForNarrowMonth = 5;
    Document* const document = m_editElement.document();

    switch (fieldType) {
    case DateTimeFormat::FieldTypeDayOfMonth:
        m_editElement.addField(DateTimeDayFieldElement::create(document, m_editElement, m_parameters.placeholderForDay));
        return;

    case DateTimeFormat::FieldTypeHour11: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(static_cast<int>(msPerHour), static_cast<int>(msPerHour * 12));
        RefPtr<DateTimeFieldElement> field = DateTimeHourFieldElement::create(document, m_editElement, 0, 11, parameters);
        m_editElement.addField(field);
        if (shouldHourFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
        return;
    }

    case DateTimeFormat::FieldTypeHour12: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(static_cast<int>(msPerHour), static_cast<int>(msPerHour * 12));
        RefPtr<DateTimeFieldElement> field = DateTimeHourFieldElement::create(document, m_editElement, 1, 12, parameters);
        m_editElement.addField(field);
        if (shouldHourFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
        return;
    }

    case DateTimeFormat::FieldTypeHour23: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(static_cast<int>(msPerHour), static_cast<int>(msPerDay));
        RefPtr<DateTimeFieldElement> field = DateTimeHourFieldElement::create(document, m_editElement, 0, 23, parameters);
        m_editElement.addField(field);
        if (shouldHourFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
        return;
    }

    case DateTimeFormat::FieldTypeHour24: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(static_cast<int>(msPerHour), static_cast<int>(msPerDay));
        RefPtr<DateTimeFieldElement> field = DateTimeHourFieldElement::create(document, m_editElement, 1, 24, parameters);
        m_editElement.addField(field);
        if (shouldHourFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
        return;
    }

    case DateTimeFormat::FieldTypeMinute: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(static_cast<int>(msPerMinute), static_cast<int>(msPerHour));
        RefPtr<DateTimeNumericFieldElement> field = DateTimeMinuteFieldElement::create(document, m_editElement, parameters);
        m_editElement.addField(field);
        if (shouldMinuteFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
        return;
    }

    case DateTimeFormat::FieldTypeMonth:
        switch (count) {
        case countForNarrowMonth: // Fallthrough.
        case countForAbbreviatedMonth:
            m_editElement.addField(DateTimeSymbolicMonthFieldElement::create(document, m_editElement, m_parameters.locale.shortMonthLabels()));
            break;
        case countForFullMonth:
            m_editElement.addField(DateTimeSymbolicMonthFieldElement::create(document, m_editElement, m_parameters.locale.monthLabels()));
            break;
        default:
            m_editElement.addField(DateTimeMonthFieldElement::create(document, m_editElement, m_parameters.placeholderForMonth));
            break;
        }
        return;

    case DateTimeFormat::FieldTypeMonthStandAlone:
        switch (count) {
        case countForNarrowMonth: // Fallthrough.
        case countForAbbreviatedMonth:
            m_editElement.addField(DateTimeSymbolicMonthFieldElement::create(document, m_editElement, m_parameters.locale.shortStandAloneMonthLabels()));
            break;
        case countForFullMonth:
            m_editElement.addField(DateTimeSymbolicMonthFieldElement::create(document, m_editElement, m_parameters.locale.standAloneMonthLabels()));
            break;
        default:
            m_editElement.addField(DateTimeMonthFieldElement::create(document, m_editElement, m_parameters.placeholderForMonth));
            break;
        }
        return;

    case DateTimeFormat::FieldTypePeriod: {
        RefPtr<DateTimeFieldElement> field = DateTimeAMPMFieldElement::create(document, m_editElement, m_parameters.locale.timeAMPMLabels());
        m_editElement.addField(field);
        if (shouldHourFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
        return;
    }

    case DateTimeFormat::FieldTypeSecond: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(static_cast<int>(msPerSecond), static_cast<int>(msPerMinute));
        RefPtr<DateTimeNumericFieldElement> field = DateTimeSecondFieldElement::create(document, m_editElement, parameters);
        m_editElement.addField(field);
        if (shouldSecondFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }

        if (needMillisecondField()) {
            visitLiteral(m_parameters.locale.localizedDecimalSeparator());
            visitField(DateTimeFormat::FieldTypeFractionalSecond, 3);
        }
        return;
    }

    case DateTimeFormat::FieldTypeFractionalSecond: {
        DateTimeNumericFieldElement::Parameters parameters = createNumericFieldParameters(1, static_cast<int>(msPerSecond));
        RefPtr<DateTimeNumericFieldElement> field = DateTimeMillisecondFieldElement::create(document, m_editElement, parameters);
        m_editElement.addField(field);
        if (shouldMillisecondFieldReadOnly()) {
            field->setValueAsDate(m_dateValue);
            field->setReadOnly();
        }
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

bool DateTimeEditBuilder::shouldHourFieldReadOnly() const
{
    const Decimal decimalMsPerDay(static_cast<int>(msPerDay));
    Decimal hourPartOfMinimum = (stepRange().minimum().abs().remainder(decimalMsPerDay) / static_cast<int>(msPerHour)).floor();
    return hourPartOfMinimum == m_dateValue.hour() && stepRange().step().remainder(decimalMsPerDay).isZero();
}

bool DateTimeEditBuilder::shouldMillisecondFieldReadOnly() const
{
    const Decimal decimalMsPerSecond(static_cast<int>(msPerSecond));
    return stepRange().minimum().abs().remainder(decimalMsPerSecond) == m_dateValue.millisecond() && stepRange().step().remainder(decimalMsPerSecond).isZero();
}

bool DateTimeEditBuilder::shouldMinuteFieldReadOnly() const
{
    const Decimal decimalMsPerHour(static_cast<int>(msPerHour));
    Decimal minutePartOfMinimum = (stepRange().minimum().abs().remainder(decimalMsPerHour) / static_cast<int>(msPerMinute)).floor();
    return minutePartOfMinimum == m_dateValue.minute() && stepRange().step().remainder(decimalMsPerHour).isZero();
}

bool DateTimeEditBuilder::shouldSecondFieldReadOnly() const
{
    const Decimal decimalMsPerMinute(static_cast<int>(msPerMinute));
    Decimal secondPartOfMinimum = (stepRange().minimum().abs().remainder(decimalMsPerMinute) / static_cast<int>(msPerSecond)).floor();
    return secondPartOfMinimum == m_dateValue.second() && stepRange().step().remainder(decimalMsPerMinute).isZero();
}

void DateTimeEditBuilder::visitLiteral(const String& text)
{
    DEFINE_STATIC_LOCAL(AtomicString, textPseudoId, ("-webkit-datetime-edit-text", AtomicString::ConstructFromLiteral));
    ASSERT(text.length());
    RefPtr<HTMLDivElement> element = HTMLDivElement::create(m_editElement.document());
    element->setPseudo(textPseudoId);
    if (m_parameters.locale.isRTL() && text.length()) {
        Direction dir = direction(text[0]);
        if (dir == SegmentSeparator || dir == WhiteSpaceNeutral || dir == OtherNeutral)
            element->appendChild(Text::create(m_editElement.document(), String(&rightToLeftMark, 1)));
    }
    element->appendChild(Text::create(m_editElement.document(), text));
    m_editElement.appendChild(element);
}

DateTimeNumericFieldElement::Parameters DateTimeEditBuilder::createNumericFieldParameters(const Decimal& msPerFieldUnit, const Decimal& msPerFieldSize) const
{
    ASSERT(!msPerFieldUnit.isZero());
    ASSERT(!msPerFieldSize.isZero());
    Decimal stepMilliseconds = stepRange().step();
    ASSERT(!stepMilliseconds.isZero());

    DateTimeNumericFieldElement::Parameters parameters(1, 0);

    if (stepMilliseconds.remainder(msPerFieldSize).isZero())
        stepMilliseconds = msPerFieldSize;

    if (msPerFieldSize.remainder(stepMilliseconds).isZero() && stepMilliseconds.remainder(msPerFieldUnit).isZero()) {
        parameters.step = static_cast<int>((stepMilliseconds / msPerFieldUnit).toDouble());
        parameters.stepBase = static_cast<int>((stepRange().stepBase() / msPerFieldUnit).floor().remainder(msPerFieldSize / msPerFieldUnit).toDouble());
    }
    return parameters;
}

// ----------------------------

DateTimeEditElement::EditControlOwner::~EditControlOwner()
{
}

DateTimeEditElement::DateTimeEditElement(Document* document, EditControlOwner& editControlOwner)
    : HTMLDivElement(divTag, document)
    , m_editControlOwner(&editControlOwner)
{
    DEFINE_STATIC_LOCAL(AtomicString, dateTimeEditPseudoId, ("-webkit-datetime-edit", AtomicString::ConstructFromLiteral));
    setPseudo(dateTimeEditPseudoId);
    setHasCustomCallbacks();
}

DateTimeEditElement::~DateTimeEditElement()
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->removeEventHandler();
}

void DateTimeEditElement::addField(PassRefPtr<DateTimeFieldElement> field)
{
    if (m_fields.size() == m_fields.capacity())
        return;
    m_fields.append(field.get());
    appendChild(field);
}

bool DateTimeEditElement::anyEditableFieldsHaveValues() const
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
        if (!m_fields[fieldIndex]->isReadOnly() && m_fields[fieldIndex]->hasValue())
            return true;
    }
    return false;
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

PassRefPtr<RenderStyle> DateTimeEditElement::customStyleForRenderer()
{
    // FIXME: This is a kind of layout. We might want to introduce new renderer.
    FontCachePurgePreventer fontCachePurgePreventer;
    RefPtr<RenderStyle> originalStyle = document()->styleResolver()->styleForElement(this);
    RefPtr<RenderStyle> style = RenderStyle::clone(originalStyle.get());
    float width = 0;
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* childElement = toElement(child);
        if (childElement->isDateTimeFieldElement()) {
            // We need to pass the Font of this element because child elements
            // can't resolve inherited style at this timing.
            width += static_cast<DateTimeFieldElement*>(childElement)->maximumWidth(style->font());
        } else {
            // ::-webkit-datetime-edit-text case. It has no
            // border/padding/margin in html.css.
            width += style->font().width(childElement->textContent());
        }
    }
    style->setMinWidth(Length(ceilf(width), Fixed));
    return style.release();
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

void DateTimeEditElement::focusIfNoFocus()
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

    HTMLDivElement::defaultEventHandler(event);
}

void DateTimeEditElement::setValueAsDate(const LayoutParameters& layoutParameters, const DateComponents& date)
{
    layout(layoutParameters, date);
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setValueAsDate(date);
}

void DateTimeEditElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setValueAsDateTimeFieldsState(dateTimeFieldsState);
}

void DateTimeEditElement::setEmptyValue(const LayoutParameters& layoutParameters, const DateComponents& dateForReadOnlyField)
{
    layout(layoutParameters, dateForReadOnlyField);
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex)
        m_fields[fieldIndex]->setEmptyValue(DateTimeFieldElement::DispatchNoEvent);
}

bool DateTimeEditElement::hasFocusedField()
{
    return focusedFieldIndex() != invalidFieldIndex;
}

void DateTimeEditElement::setOnlyYearMonthDay(const DateComponents& date)
{
    ASSERT(date.type() == DateComponents::Date);

    if (!m_editControlOwner)
        return;

    DateTimeFieldsState dateTimeFieldsState = valueAsDateTimeFieldsState();
    dateTimeFieldsState.setYear(date.fullYear());
    dateTimeFieldsState.setMonth(date.month() + 1);
    dateTimeFieldsState.setDayOfMonth(date.monthDay());
    setValueAsDateTimeFieldsState(dateTimeFieldsState);
    m_editControlOwner->editControlValueChanged();
}

void DateTimeEditElement::stepDown()
{
    if (DateTimeFieldElement* const field = focusedField())
        field->stepDown();
}

void DateTimeEditElement::stepUp()
{
    if (DateTimeFieldElement* const field = focusedField())
        field->stepUp();
}

void DateTimeEditElement::updateUIState()
{
    if (isDisabled() || isReadOnly()) {
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
