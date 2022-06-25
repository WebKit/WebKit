/*
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <string.h>
#include <time.h>
#include <wtf/DateMath.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

class GregorianDateTime final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GregorianDateTime() = default;
    WTF_EXPORT_PRIVATE explicit GregorianDateTime(double ms, LocalTimeOffset);

    inline int year() const { return m_year; }
    inline int month() const { return m_month; }
    inline int yearDay() const { return m_yearDay; }
    inline int monthDay() const { return m_monthDay; }
    inline int weekDay() const { return m_weekDay; }
    inline int hour() const { return m_hour; }
    inline int minute() const { return m_minute; }
    inline int second() const { return m_second; }
    inline int utcOffsetInMinute() const { return m_utcOffsetInMinute; }
    inline int isDST() const { return m_isDST; }

    inline void setYear(int year) { m_year = year; }
    inline void setMonth(int month) { m_month = month; }
    inline void setYearDay(int yearDay) { m_yearDay = yearDay; }
    inline void setMonthDay(int monthDay) { m_monthDay = monthDay; }
    inline void setWeekDay(int weekDay) { m_weekDay = weekDay; }
    inline void setHour(int hour) { m_hour = hour; }
    inline void setMinute(int minute) { m_minute = minute; }
    inline void setSecond(int second) { m_second = second; }
    inline void setUTCOffsetInMinute(int utcOffsetInMinute) { m_utcOffsetInMinute = utcOffsetInMinute; }
    inline void setIsDST(int isDST) { m_isDST = isDST; }

    static ptrdiff_t offsetOfYear() { return OBJECT_OFFSETOF(GregorianDateTime, m_year); }
    static ptrdiff_t offsetOfMonth() { return OBJECT_OFFSETOF(GregorianDateTime, m_month); }
    static ptrdiff_t offsetOfYearDay() { return OBJECT_OFFSETOF(GregorianDateTime, m_yearDay); }
    static ptrdiff_t offsetOfMonthDay() { return OBJECT_OFFSETOF(GregorianDateTime, m_monthDay); }
    static ptrdiff_t offsetOfWeekDay() { return OBJECT_OFFSETOF(GregorianDateTime, m_weekDay); }
    static ptrdiff_t offsetOfHour() { return OBJECT_OFFSETOF(GregorianDateTime, m_hour); }
    static ptrdiff_t offsetOfMinute() { return OBJECT_OFFSETOF(GregorianDateTime, m_minute); }
    static ptrdiff_t offsetOfSecond() { return OBJECT_OFFSETOF(GregorianDateTime, m_second); }
    static ptrdiff_t offsetOfUTCOffsetInMinute() { return OBJECT_OFFSETOF(GregorianDateTime, m_utcOffsetInMinute); }
    static ptrdiff_t offsetOfIsDST() { return OBJECT_OFFSETOF(GregorianDateTime, m_isDST); }

    WTF_EXPORT_PRIVATE void setToCurrentLocalTime();

    operator tm() const
    {
        tm ret;
        memset(&ret, 0, sizeof(ret));

        ret.tm_year = m_year - 1900;
        ret.tm_mon = m_month;
        ret.tm_yday = m_yearDay;
        ret.tm_mday = m_monthDay;
        ret.tm_wday = m_weekDay;
        ret.tm_hour = m_hour;
        ret.tm_min = m_minute;
        ret.tm_sec = m_second;
        ret.tm_isdst = m_isDST;

#if HAVE(TM_GMTOFF)
        ret.tm_gmtoff = static_cast<long>(m_utcOffsetInMinute) * static_cast<long>(secondsPerMinute);
#endif

        return ret;
    }

private:
    int m_year { 0 };
    int m_month { 0 };
    int m_yearDay { 0 };
    int m_monthDay { 0 };
    int m_weekDay { 0 };
    int m_hour { 0 };
    int m_minute { 0 };
    int m_second { 0 };
    int m_utcOffsetInMinute { 0 };
    int m_isDST { 0 };
};

} // namespace WTF

using WTF::GregorianDateTime;
