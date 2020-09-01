/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DateTimeEditElement.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateComponents.h"
#include "DateTimeFieldElements.h"
#include "DateTimeFieldsState.h"
#include "DateTimeFormat.h"
#include "DateTimeSymbolicFieldElement.h"
#include "HTMLNames.h"
#include "PlatformLocale.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeEditElement);

class DateTimeEditBuilder final : private DateTimeFormat::TokenHandler {
    WTF_MAKE_NONCOPYABLE(DateTimeEditBuilder);

public:
    DateTimeEditBuilder(DateTimeEditElement&, const DateTimeEditElement::LayoutParameters&);

    bool build(const String&);

private:
    // DateTimeFormat::TokenHandler functions:
    void visitField(DateTimeFormat::FieldType, int);
    void visitLiteral(const String&);

    DateTimeEditElement& m_editElement;
    const DateTimeEditElement::LayoutParameters& m_parameters;
};

DateTimeEditBuilder::DateTimeEditBuilder(DateTimeEditElement& element, const DateTimeEditElement::LayoutParameters& layoutParameters)
    : m_editElement(element)
    , m_parameters(layoutParameters)
{
}

bool DateTimeEditBuilder::build(const String& formatString)
{
    m_editElement.resetFields();
    return DateTimeFormat::parse(formatString, *this);
}

void DateTimeEditBuilder::visitField(DateTimeFormat::FieldType fieldType, int count)
{
    Document& document = m_editElement.document();

    switch (fieldType) {
    case DateTimeFormat::FieldTypeDayOfMonth: {
        m_editElement.addField(DateTimeDayFieldElement::create(document, m_editElement));
        return;
    }

    case DateTimeFormat::FieldTypeMonth:
    case DateTimeFormat::FieldTypeMonthStandAlone: {
        constexpr int countForAbbreviatedMonth = 3;
        constexpr int countForFullMonth = 4;
        constexpr int countForNarrowMonth = 5;

        switch (count) {
        case countForNarrowMonth:
        case countForAbbreviatedMonth: {
            auto field = DateTimeSymbolicMonthFieldElement::create(document, m_editElement, fieldType == DateTimeFormat::FieldTypeMonth ? m_parameters.locale.shortMonthLabels() : m_parameters.locale.shortStandAloneMonthLabels());
            m_editElement.addField(field);
            return;
        }
        case countForFullMonth: {
            auto field = DateTimeSymbolicMonthFieldElement::create(document, m_editElement, fieldType == DateTimeFormat::FieldTypeMonth ? m_parameters.locale.monthLabels() : m_parameters.locale.standAloneMonthLabels());
            m_editElement.addField(field);
            return;
        }
        default:
            m_editElement.addField(DateTimeMonthFieldElement::create(document, m_editElement));
            return;
        }
    }

    case DateTimeFormat::FieldTypeYear: {
        m_editElement.addField(DateTimeYearFieldElement::create(document, m_editElement));
        return;
    }

    default:
        return;
    }
}

void DateTimeEditBuilder::visitLiteral(const String& text)
{
    ASSERT(text.length());
    static MainThreadNeverDestroyed<const AtomString> textPseudoId("-webkit-datetime-edit-text", AtomString::ConstructFromLiteral);
    auto element = HTMLDivElement::create(m_editElement.document());
    element->setPseudo(textPseudoId);
    element->appendChild(Text::create(m_editElement.document(), text));
    m_editElement.fieldsWrapperElement().appendChild(element);
}

DateTimeEditElement::EditControlOwner::~EditControlOwner() = default;

DateTimeEditElement::DateTimeEditElement(Document& document, EditControlOwner& editControlOwner)
    : HTMLDivElement(divTag, document)
    , m_editControlOwner(makeWeakPtr(editControlOwner))
{
    static MainThreadNeverDestroyed<const AtomString> dateTimeEditPseudoId("-webkit-datetime-edit", AtomString::ConstructFromLiteral);
    setPseudo(dateTimeEditPseudoId);
}

DateTimeEditElement::~DateTimeEditElement() = default;

inline Element& DateTimeEditElement::fieldsWrapperElement() const
{
    ASSERT(firstChild());
    return downcast<Element>(*firstChild());
}

void DateTimeEditElement::addField(Ref<DateTimeFieldElement> field)
{
    if (m_fields.size() == m_fields.capacity())
        return;
    m_fields.append(field);
    fieldsWrapperElement().appendChild(field);
}

size_t DateTimeEditElement::fieldIndexOf(const DateTimeFieldElement& fieldToFind) const
{
    return m_fields.findMatching([&] (auto& field) {
        return field.ptr() == &fieldToFind;
    });
}

Ref<DateTimeEditElement> DateTimeEditElement::create(Document& document, EditControlOwner& editControlOwner)
{
    return adoptRef(*new DateTimeEditElement(document, editControlOwner));
}

void DateTimeEditElement::layout(const LayoutParameters& layoutParameters)
{
    static MainThreadNeverDestroyed<const AtomString> fieldsWrapperPseudoId("-webkit-datetime-edit-fields-wrapper", AtomString::ConstructFromLiteral);

    if (!firstChild()) {
        auto element = HTMLDivElement::create(document());
        element->setPseudo(fieldsWrapperPseudoId);
        appendChild(element);
    }

    Element& fieldsWrapper = fieldsWrapperElement();

    DateTimeEditBuilder builder(*this, layoutParameters);
    Node* lastChildToBeRemoved = fieldsWrapper.lastChild();
    if (!builder.build(layoutParameters.dateTimeFormat) || m_fields.isEmpty()) {
        lastChildToBeRemoved = fieldsWrapper.lastChild();
        builder.build(layoutParameters.fallbackDateTimeFormat);
    }

    if (lastChildToBeRemoved) {
        while (auto* childNode = fieldsWrapper.firstChild()) {
            fieldsWrapper.removeChild(*childNode);
            if (childNode == lastChildToBeRemoved)
                break;
        }
    }
}

void DateTimeEditElement::blurFromField(RefPtr<Element>&& newFocusedElement)
{
    bool notifyOwner = notFound == m_fields.findMatching([&] (auto& field) {
        return field.ptr() == newFocusedElement.get();
    });

    HTMLElement::dispatchBlurEvent(WTFMove(newFocusedElement));

    if (m_editControlOwner && notifyOwner)
        m_editControlOwner->didBlurFromControl();
}

void DateTimeEditElement::fieldValueChanged()
{
    if (m_editControlOwner)
        m_editControlOwner->didChangeValueFromControl();
}

bool DateTimeEditElement::focusOnNextFocusableField(size_t startIndex)
{
    for (size_t i = startIndex; i < m_fields.size(); ++i) {
        if (m_fields[i]->isFocusable()) {
            m_fields[i]->focus();
            return true;
        }
    }
    return false;
}

void DateTimeEditElement::focusByOwner()
{
    focusOnNextFocusableField(0);
}

bool DateTimeEditElement::focusOnNextField(const DateTimeFieldElement& field)
{
    auto startFieldIndex = fieldIndexOf(field);
    if (startFieldIndex == notFound)
        return false;

    return focusOnNextFocusableField(startFieldIndex + 1);
}

bool DateTimeEditElement::focusOnPreviousField(const DateTimeFieldElement& field)
{
    auto startFieldIndex = fieldIndexOf(field);
    if (startFieldIndex == notFound)
        return false;

    auto fieldIndex = startFieldIndex;
    while (fieldIndex > 0) {
        --fieldIndex;
        if (m_fields[fieldIndex]->isFocusable()) {
            m_fields[fieldIndex]->focus();
            return true;
        }
    }

    return false;
}

AtomString DateTimeEditElement::localeIdentifier() const
{
    return m_editControlOwner ? m_editControlOwner->localeIdentifier() : nullAtom();
}

void DateTimeEditElement::resetFields()
{
    m_fields.shrink(0);
}

void DateTimeEditElement::setValueAsDate(const LayoutParameters& layoutParameters, const DateComponents& date)
{
    layout(layoutParameters);
    for (auto& field : m_fields)
        field->setValueAsDate(date);
}

void DateTimeEditElement::setEmptyValue(const LayoutParameters& layoutParameters)
{
    layout(layoutParameters);
    for (auto& field : m_fields)
        field->setEmptyValue();
}

String DateTimeEditElement::value() const
{
    return m_editControlOwner ? m_editControlOwner->formatDateTimeFieldsState(valueAsDateTimeFieldsState()) : emptyString();
}

DateTimeFieldsState DateTimeEditElement::valueAsDateTimeFieldsState() const
{
    DateTimeFieldsState dateTimeFieldsState;
    for (auto& field : m_fields)
        field->populateDateTimeFieldsState(dateTimeFieldsState);
    return dateTimeFieldsState;
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
