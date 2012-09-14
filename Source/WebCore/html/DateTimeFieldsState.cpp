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
#include "DateTimeFieldsState.h"

#include "FormController.h"

namespace WebCore {

static unsigned getNumberFromFormControlState(const FormControlState& state, size_t index)
{
    if (index >= state.valueSize())
        return DateTimeFieldsState::emptyValue;
    bool parsed;
    unsigned const value = state[index].toUInt(&parsed);
    return parsed ? value : DateTimeFieldsState::emptyValue;
}

static DateTimeFieldsState::AMPMValue getAMPMFromFormControlState(const FormControlState& state, size_t index)
{
    if (index >= state.valueSize())
        return DateTimeFieldsState::AMPMValueEmpty;
    const String value = state[index];
    if (value == "A")
        return DateTimeFieldsState::AMPMValueAM;
    if (value == "P")
        return DateTimeFieldsState::AMPMValuePM;
    return DateTimeFieldsState::AMPMValueEmpty;
}

DateTimeFieldsState::DateTimeFieldsState()
    : m_year(emptyValue)
    , m_month(emptyValue)
    , m_dayOfMonth(emptyValue)
    , m_hour(emptyValue)
    , m_minute(emptyValue)
    , m_second(emptyValue)
    , m_millisecond(emptyValue)
    , m_weekOfYear(emptyValue)
    , m_ampm(AMPMValueEmpty)
{
}

DateTimeFieldsState DateTimeFieldsState::restoreFormControlState(const FormControlState& state)
{
    DateTimeFieldsState dateTimeFieldsState;
    dateTimeFieldsState.setYear(getNumberFromFormControlState(state, 0));
    dateTimeFieldsState.setMonth(getNumberFromFormControlState(state, 1));
    dateTimeFieldsState.setDayOfMonth(getNumberFromFormControlState(state, 2));
    dateTimeFieldsState.setHour(getNumberFromFormControlState(state, 3));
    dateTimeFieldsState.setMinute(getNumberFromFormControlState(state, 4));
    dateTimeFieldsState.setSecond(getNumberFromFormControlState(state, 5));
    dateTimeFieldsState.setMillisecond(getNumberFromFormControlState(state, 6));
    dateTimeFieldsState.setWeekOfYear(getNumberFromFormControlState(state, 7));
    dateTimeFieldsState.setAMPM(getAMPMFromFormControlState(state, 8));
    return dateTimeFieldsState;
}

FormControlState DateTimeFieldsState::saveFormControlState() const
{
    FormControlState state;
    state.append(hasYear() ? String::number(m_year) : emptyString());
    state.append(hasMonth() ? String::number(m_month) : emptyString());
    state.append(hasDayOfMonth() ? String::number(m_dayOfMonth) : emptyString());
    state.append(hasHour() ? String::number(m_hour) : emptyString());
    state.append(hasMinute() ? String::number(m_minute) : emptyString());
    state.append(hasSecond() ? String::number(m_second) : emptyString());
    state.append(hasMillisecond() ? String::number(m_millisecond) : emptyString());
    state.append(hasWeekOfYear() ? String::number(m_weekOfYear) : emptyString());
    if (hasAMPM())
        state.append(m_ampm == AMPMValueAM ? "A" : "P");
    else
        state.append(emptyString());
    return state;
}

} // namespace WebCore

#endif
