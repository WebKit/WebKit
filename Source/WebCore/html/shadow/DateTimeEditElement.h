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

#ifndef DateTimeEditElement_h
#define DateTimeEditElement_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateComponents.h"
#include "DateTimeFieldElement.h"
#include "StepRange.h"

namespace WebCore {

class DateTimeFieldsState;
class KeyboardEvent;
class Locale;
class MouseEvent;
class StepRange;

// DateTimeEditElement class contains numberic field and symbolc field for
// representing date and time, such as
//  - Year, Month, Day Of Month
//  - Hour, Minute, Second, Millisecond, AM/PM
class DateTimeEditElement : public HTMLDivElement, public DateTimeFieldElement::FieldOwner {
    WTF_MAKE_NONCOPYABLE(DateTimeEditElement);

public:
    // EditControlOwner implementer must call removeEditControlOwner when
    // it doesn't handle event, e.g. at destruction.
    class EditControlOwner {
    public:
        virtual ~EditControlOwner();
        virtual void didBlurFromControl() = 0;
        virtual void didFocusOnControl() = 0;
        virtual void editControlValueChanged() = 0;
        virtual String formatDateTimeFieldsState(const DateTimeFieldsState&) const = 0;
        virtual bool isEditControlOwnerDisabled() const = 0;
        virtual bool isEditControlOwnerReadOnly() const = 0;
        virtual AtomicString localeIdentifier() const = 0;
    };

    struct LayoutParameters {
        String dateTimeFormat;
        String fallbackDateTimeFormat;
        Locale& locale;
        const StepRange stepRange;
        DateComponents minimum;
        DateComponents maximum;
        String placeholderForDay;
        String placeholderForMonth;
        String placeholderForYear;

        LayoutParameters(Locale& locale, const StepRange& stepRange)
            : locale(locale)
            , stepRange(stepRange)
        {
        }
    };

    static PassRefPtr<DateTimeEditElement> create(Document*, EditControlOwner&);

    virtual ~DateTimeEditElement();
    void addField(PassRefPtr<DateTimeFieldElement>);
    bool anyEditableFieldsHaveValues() const;
    void blurByOwner();
    virtual void defaultEventHandler(Event*) OVERRIDE;
    void disabledStateChanged();
    void focusIfNoFocus();
    void focusByOwner();
    bool hasFocusedField();
    void readOnlyStateChanged();
    void removeEditControlOwner() { m_editControlOwner = 0; }
    void resetFields();
    void setEmptyValue(const LayoutParameters&, const DateComponents& dateForReadOnlyField);
    void setValueAsDate(const LayoutParameters&, const DateComponents&);
    void setValueAsDateTimeFieldsState(const DateTimeFieldsState&);
    void setOnlyYearMonthDay(const DateComponents&);
    void stepDown();
    void stepUp();
    String value() const;
    DateTimeFieldsState valueAsDateTimeFieldsState() const;

private:
    static const size_t invalidFieldIndex = static_cast<size_t>(-1);

    // Datetime can be represent at most five fields, such as
    //  1. year
    //  2. month
    //  3. day-of-month
    //  4. hour
    //  5. minute
    //  6. second
    //  7. millisecond
    //  8. AM/PM
    static const int maximumNumberOfFields = 8;

    DateTimeEditElement(Document*, EditControlOwner&);

    DateTimeFieldElement* fieldAt(size_t) const;
    size_t fieldIndexOf(const DateTimeFieldElement&) const;
    DateTimeFieldElement* focusedField() const;
    size_t focusedFieldIndex() const;
    bool isDisabled() const;
    bool isReadOnly() const;
    void layout(const LayoutParameters&, const DateComponents&);
    void updateUIState();

    // Element function.
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;

    // DateTimeFieldElement::FieldOwner functions.
    virtual void didBlurFromField() OVERRIDE FINAL;
    virtual void didFocusOnField() OVERRIDE FINAL;
    virtual void fieldValueChanged() OVERRIDE FINAL;
    virtual bool focusOnNextField(const DateTimeFieldElement&) OVERRIDE FINAL;
    virtual bool focusOnPreviousField(const DateTimeFieldElement&) OVERRIDE FINAL;
    virtual bool isFieldOwnerDisabledOrReadOnly() const OVERRIDE FINAL;
    virtual AtomicString localeIdentifier() const OVERRIDE FINAL;

    Vector<DateTimeFieldElement*, maximumNumberOfFields> m_fields;
    EditControlOwner* m_editControlOwner;
};

} // namespace WebCore

#endif
#endif
