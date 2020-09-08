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

#pragma once

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateComponents.h"
#include "DateTimeFieldElement.h"

#include <wtf/WeakPtr.h>

namespace WebCore {

class Locale;

class DateTimeEditElement final : public HTMLDivElement, public DateTimeFieldElement::FieldOwner {
    WTF_MAKE_ISO_ALLOCATED(DateTimeEditElement);
public:
    class EditControlOwner : public CanMakeWeakPtr<EditControlOwner> {
    public:
        virtual ~EditControlOwner();
        virtual void didBlurFromControl() = 0;
        virtual void didChangeValueFromControl() = 0;
        virtual String formatDateTimeFieldsState(const DateTimeFieldsState&) const = 0;
        virtual bool isEditControlOwnerDisabled() const = 0;
        virtual bool isEditControlOwnerReadOnly() const = 0;
        virtual AtomString localeIdentifier() const = 0;
    };

    struct LayoutParameters {
        String dateTimeFormat;
        String fallbackDateTimeFormat;
        Locale& locale;

        LayoutParameters(Locale& locale)
            : locale(locale)
        {
        }
    };

    static Ref<DateTimeEditElement> create(Document&, EditControlOwner&);

    virtual ~DateTimeEditElement();
    void addField(Ref<DateTimeFieldElement>);
    Element& fieldsWrapperElement() const;
    void focusByOwner();
    void resetFields();
    void setEmptyValue(const LayoutParameters&);
    void setValueAsDate(const LayoutParameters&, const DateComponents&);
    String value() const;

private:
    // Datetime can be represented by at most 8 fields:
    // 1. year
    // 2. month
    // 3. day-of-month
    // 4. hour
    // 5. minute
    // 6. second
    // 7. millisecond
    // 8. AM/PM
    static constexpr int maximumNumberOfFields = 8;

    DateTimeEditElement(Document&, EditControlOwner&);

    size_t fieldIndexOf(const DateTimeFieldElement&) const;
    DateTimeFieldElement* focusedFieldElement() const;
    void layout(const LayoutParameters&);
    DateTimeFieldsState valueAsDateTimeFieldsState() const;

    bool focusOnNextFocusableField(size_t startIndex);

    // DateTimeFieldElement::FieldOwner functions:
    void blurFromField(RefPtr<Element>&& newFocusedElement) final;
    void fieldValueChanged() final;
    bool focusOnNextField(const DateTimeFieldElement&) final;
    bool focusOnPreviousField(const DateTimeFieldElement&) final;
    bool isFieldOwnerDisabled() const final;
    bool isFieldOwnerReadOnly() const final;
    AtomString localeIdentifier() const final;

    Vector<Ref<DateTimeFieldElement>, maximumNumberOfFields> m_fields;
    WeakPtr<EditControlOwner> m_editControlOwner;
};

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
