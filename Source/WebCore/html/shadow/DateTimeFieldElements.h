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
    WTF_MAKE_ISO_ALLOCATED(DateTimeDayFieldElement);

public:
    static Ref<DateTimeDayFieldElement> create(Document&, FieldOwner&);

private:
    DateTimeDayFieldElement(Document&, FieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeHourFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeHourFieldElement);

public:
    static Ref<DateTimeHourFieldElement> create(Document&, FieldOwner&, int minimum, int maximum);

private:
    DateTimeHourFieldElement(Document&, FieldOwner&, int minimum, int maximum);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeMeridiemFieldElement final : public DateTimeSymbolicFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeMeridiemFieldElement);

public:
    static Ref<DateTimeMeridiemFieldElement> create(Document&, FieldOwner&, const Vector<String>&);

private:
    DateTimeMeridiemFieldElement(Document&, FieldOwner&, const Vector<String>&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeMillisecondFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeMillisecondFieldElement);

public:
    static Ref<DateTimeMillisecondFieldElement> create(Document&, FieldOwner&);

private:
    DateTimeMillisecondFieldElement(Document&, FieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeMinuteFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeMinuteFieldElement);

public:
    static Ref<DateTimeMinuteFieldElement> create(Document&, FieldOwner&);

private:
    DateTimeMinuteFieldElement(Document&, FieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeMonthFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeMonthFieldElement);

public:
    static Ref<DateTimeMonthFieldElement> create(Document&, FieldOwner&);

private:
    DateTimeMonthFieldElement(Document&, FieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeSecondFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeSecondFieldElement);

public:
    static Ref<DateTimeSecondFieldElement> create(Document&, FieldOwner&);

private:
    DateTimeSecondFieldElement(Document&, FieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeSymbolicMonthFieldElement final : public DateTimeSymbolicFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeSymbolicMonthFieldElement);

public:
    static Ref<DateTimeSymbolicMonthFieldElement> create(Document&, FieldOwner&, const Vector<String>&);

private:
    DateTimeSymbolicMonthFieldElement(Document&, FieldOwner&, const Vector<String>&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

class DateTimeYearFieldElement final : public DateTimeNumericFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeYearFieldElement);

public:
    static Ref<DateTimeYearFieldElement> create(Document&, FieldOwner&);

private:
    DateTimeYearFieldElement(Document&, FieldOwner&);

    // DateTimeFieldElement functions:
    void setValueAsDate(const DateComponents&) final;
    void populateDateTimeFieldsState(DateTimeFieldsState&) final;
};

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
