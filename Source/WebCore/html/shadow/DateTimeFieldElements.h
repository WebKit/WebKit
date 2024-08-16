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

#include "DateTimeNumericFieldElement.h"
#include "DateTimeSymbolicFieldElement.h"

namespace WebCore {

class DateTimeDayFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeDayFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeDayFieldElement);

public:
    static Ref<DateTimeDayFieldElement> create(Document&, DateTimeFieldElementFieldOwner&);

private:
    DateTimeDayFieldElement(Document&, DateTimeFieldElementFieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeHourFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeHourFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeHourFieldElement);

public:
    static Ref<DateTimeHourFieldElement> create(Document&, DateTimeFieldElementFieldOwner&, int minimum, int maximum);

private:
    DateTimeHourFieldElement(Document&, DateTimeFieldElementFieldOwner&, int minimum, int maximum);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeMeridiemFieldElement final : public DateTimeSymbolicFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeMeridiemFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeMeridiemFieldElement);

public:
    static Ref<DateTimeMeridiemFieldElement> create(Document&, DateTimeFieldElementFieldOwner&, const Vector<String>&);

private:
    DateTimeMeridiemFieldElement(Document&, DateTimeFieldElementFieldOwner&, const Vector<String>&);

    void updateAriaValueAttributes();
    // DateTimeFieldElement functions:
    void setEmptyValue(EventBehavior = DispatchNoEvent) final;
    void setValueAsDate(const DateComponents&) final;
    void setValueAsInteger(int, EventBehavior = DispatchNoEvent) final;

    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeMillisecondFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeMillisecondFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeMillisecondFieldElement);

public:
    static Ref<DateTimeMillisecondFieldElement> create(Document&, DateTimeFieldElementFieldOwner&);

private:
    DateTimeMillisecondFieldElement(Document&, DateTimeFieldElementFieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeMinuteFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeMinuteFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeMinuteFieldElement);

public:
    static Ref<DateTimeMinuteFieldElement> create(Document&, DateTimeFieldElementFieldOwner&);

private:
    DateTimeMinuteFieldElement(Document&, DateTimeFieldElementFieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeMonthFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeMonthFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeMonthFieldElement);

public:
    static Ref<DateTimeMonthFieldElement> create(Document&, DateTimeFieldElementFieldOwner&);

private:
    DateTimeMonthFieldElement(Document&, DateTimeFieldElementFieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeSecondFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeSecondFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeSecondFieldElement);

public:
    static Ref<DateTimeSecondFieldElement> create(Document&, DateTimeFieldElementFieldOwner&);

private:
    DateTimeSecondFieldElement(Document&, DateTimeFieldElementFieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeSymbolicMonthFieldElement final : public DateTimeSymbolicFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeSymbolicMonthFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeSymbolicMonthFieldElement);

public:
    static Ref<DateTimeSymbolicMonthFieldElement> create(Document&, DateTimeFieldElementFieldOwner&, const Vector<String>&);

private:
    DateTimeSymbolicMonthFieldElement(Document&, DateTimeFieldElementFieldOwner&, const Vector<String>&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

class DateTimeYearFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DateTimeYearFieldElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DateTimeYearFieldElement);

public:
    static Ref<DateTimeYearFieldElement> create(Document&, DateTimeFieldElementFieldOwner&);

private:
    DateTimeYearFieldElement(Document&, DateTimeFieldElementFieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&, DateTimePlaceholderIfNoValue) final;
};

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
