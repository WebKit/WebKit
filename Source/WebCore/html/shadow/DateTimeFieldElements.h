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

#ifndef DateTimeFieldElements_h
#define DateTimeFieldElements_h

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include "DateTimeNumericFieldElement.h"
#include "DateTimeSymbolicFieldElement.h"

namespace WebCore {

class DateTimeAMPMFieldElement : public DateTimeSymbolicFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeAMPMFieldElement);

public:
    static PassRefPtr<DateTimeAMPMFieldElement> create(Document*, FieldEventHandler&, const Vector<String>&);

private:
    DateTimeAMPMFieldElement(Document*, FieldEventHandler&, const Vector<String>&);

    // DateTimeFieldElement functions.
    virtual void setValueAsDate(const DateComponents&) OVERRIDE FINAL;
    virtual double unitInMillisecond() const OVERRIDE FINAL;
};


// DateTimeHourFieldElement is used for hour field of date time format
// supporting following patterns:
//  - 0 to 11
//  - 1 to 12
//  - 0 to 23
//  - 1 to 24
class DateTimeHourFieldElement : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeHourFieldElement);

public:
    static PassRefPtr<DateTimeHourFieldElement> create(Document*, FieldEventHandler&, int minimum, int maximum);

private:
    DateTimeHourFieldElement(Document*, FieldEventHandler&, int minimum, int maximum);

    // DateTimeFieldElement functions.
    virtual void setValueAsDate(const DateComponents&) OVERRIDE FINAL;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE FINAL;
    virtual double unitInMillisecond() const OVERRIDE FINAL;
    virtual int valueAsInteger() const OVERRIDE FINAL;

    const int m_alignment;
};

class DateTimeMillisecondFieldElement : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeMillisecondFieldElement);

public:
    static PassRefPtr<DateTimeMillisecondFieldElement> create(Document*, FieldEventHandler&);

private:
    DateTimeMillisecondFieldElement(Document*, FieldEventHandler&);

    // DateTimeFieldElement functions.
    virtual void setValueAsDate(const DateComponents&) OVERRIDE FINAL;
    virtual double unitInMillisecond() const OVERRIDE FINAL;
};

class DateTimeMinuteFieldElement : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeMinuteFieldElement);

public:
    static PassRefPtr<DateTimeMinuteFieldElement> create(Document*, FieldEventHandler&);

private:
    DateTimeMinuteFieldElement(Document*, FieldEventHandler&);

    // DateTimeFieldElement functions.
    virtual void setValueAsDate(const DateComponents&) OVERRIDE FINAL;
    virtual double unitInMillisecond() const OVERRIDE FINAL;
};

class DateTimeSecondFieldElement : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeSecondFieldElement);

public:
    static PassRefPtr<DateTimeSecondFieldElement> create(Document*, FieldEventHandler&);

private:
    DateTimeSecondFieldElement(Document*, FieldEventHandler&);

    // DateTimeFieldElement functions.
    virtual void setValueAsDate(const DateComponents&) OVERRIDE FINAL;
    virtual double unitInMillisecond() const OVERRIDE FINAL;
};

} // namespace WebCore

#endif
#endif
