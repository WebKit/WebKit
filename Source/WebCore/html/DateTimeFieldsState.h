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

#ifndef DateTimeFieldsState_h
#define DateTimeFieldsState_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormControlState;

// DateTimeFieldsState represents fields in date/time for form state
// save/restore for input type "date", "datetime", "datetime-local", "month",
// "time", and "week" with multiple fields input UI.
//
// Each field can contain invalid value for date, e.g. day of month field can
// be 30 even if month field is February.
class DateTimeFieldsState {
public:
    enum AMPMValue {
        AMPMValueEmpty = -1,
        AMPMValueAM,
        AMPMValuePM,
    };

    static const unsigned emptyValue;

    DateTimeFieldsState();

    static DateTimeFieldsState restoreFormControlState(const FormControlState&);
    FormControlState saveFormControlState() const;

    AMPMValue ampm() const { return m_ampm; }
    unsigned dayOfMonth() const { return m_dayOfMonth; }
    unsigned hour() const { return m_hour; }
    unsigned hour23() const;
    unsigned millisecond() const { return m_millisecond; }
    unsigned minute() const { return m_minute; }
    unsigned month() const { return m_month; }
    unsigned second() const { return m_second; }
    unsigned weekOfYear() const { return m_weekOfYear; }
    unsigned year() const { return m_year; }

    bool hasAMPM() const { return m_ampm != AMPMValueEmpty; }
    bool hasDayOfMonth() const { return m_dayOfMonth != emptyValue; }
    bool hasHour() const { return m_hour != emptyValue; }
    bool hasMillisecond() const { return m_millisecond != emptyValue; }
    bool hasMinute() const { return m_minute != emptyValue; }
    bool hasMonth() const { return m_month != emptyValue; }
    bool hasSecond() const { return m_second != emptyValue; }
    bool hasWeekOfYear() const { return m_weekOfYear != emptyValue; }
    bool hasYear() const { return m_year != emptyValue; }

    void setAMPM(AMPMValue ampm) { m_ampm = ampm; }
    void setDayOfMonth(unsigned dayOfMonth) { m_dayOfMonth = dayOfMonth; }
    void setHour(unsigned hour12) { m_hour = hour12; }
    void setMillisecond(unsigned millisecond) { m_millisecond = millisecond; }
    void setMinute(unsigned minute) { m_minute = minute; }
    void setMonth(unsigned month) { m_month = month; }
    void setSecond(unsigned second) { m_second = second; }
    void setWeekOfYear(unsigned weekOfYear) { m_weekOfYear = weekOfYear; }
    void setYear(unsigned year) { m_year = year; }

private:
    unsigned m_year;
    unsigned m_month;
    unsigned m_dayOfMonth;
    unsigned m_hour; // 1 to 12.
    unsigned m_minute;
    unsigned m_second;
    unsigned m_millisecond;
    unsigned m_weekOfYear;
    AMPMValue m_ampm;
};

} // namespace WebCore

#endif
#endif
