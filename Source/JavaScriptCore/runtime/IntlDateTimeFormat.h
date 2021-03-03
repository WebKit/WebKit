/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "JSObject.h"
#include <unicode/udat.h>
#include <wtf/unicode/icu/ICUHelpers.h>

struct UDateIntervalFormat;

#if !defined(HAVE_ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)
// ICU header is up-to-date if the build is non-Darwin or using Apple Internal SDK.
#if (USE(APPLE_INTERNAL_SDK) || !OS(DARWIN)) && U_ICU_VERSION_MAJOR_NUM >= 64
#define HAVE_ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS 1
#endif
#endif

namespace JSC {

enum class RelevantExtensionKey : uint8_t;

class JSBoundFunction;

struct UDateIntervalFormatDeleter {
    JS_EXPORT_PRIVATE void operator()(UDateIntervalFormat*);
};

class IntlDateTimeFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlDateTimeFormat*>(cell)->IntlDateTimeFormat::~IntlDateTimeFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlDateTimeFormatSpace<mode>();
    }

    static IntlDateTimeFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeDateTimeFormat(JSGlobalObject*, JSValue locales, JSValue options);
    JSValue format(JSGlobalObject*, double value) const;
    JSValue formatToParts(JSGlobalObject*, double value, JSString* sourceType = nullptr) const;
    JSValue formatRange(JSGlobalObject*, double startDate, double endDate);
    JSValue formatRangeToParts(JSGlobalObject*, double startDate, double endDate);
    JSObject* resolvedOptions(JSGlobalObject*) const;

    JSBoundFunction* boundFormat() const { return m_boundFormat.get(); }
    void setBoundFormat(VM&, JSBoundFunction*);

    static IntlDateTimeFormat* unwrapForOldFunctions(JSGlobalObject*, JSValue);

private:
    IntlDateTimeFormat(VM&, Structure*);
    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

    static Vector<String> localeData(const String&, RelevantExtensionKey);

    UDateIntervalFormat* createDateIntervalFormatIfNecessary(JSGlobalObject*);

    enum class HourCycle : uint8_t { None, H11, H12, H23, H24 };
    enum class Weekday : uint8_t { None, Narrow, Short, Long };
    enum class Era : uint8_t { None, Narrow, Short, Long };
    enum class Year : uint8_t { None, TwoDigit, Numeric };
    enum class Month : uint8_t { None, TwoDigit, Numeric, Narrow, Short, Long };
    enum class Day : uint8_t { None, TwoDigit, Numeric };
    enum class DayPeriod : uint8_t { None, Narrow, Short, Long };
    enum class Hour : uint8_t { None, TwoDigit, Numeric };
    enum class Minute : uint8_t { None, TwoDigit, Numeric };
    enum class Second : uint8_t { None, TwoDigit, Numeric };
    enum class TimeZoneName : uint8_t { None, Short, Long };
    enum class DateTimeStyle : uint8_t { None, Full, Long, Medium, Short };

    void setFormatsFromPattern(const StringView&);
    static ASCIILiteral hourCycleString(HourCycle);
    static ASCIILiteral weekdayString(Weekday);
    static ASCIILiteral eraString(Era);
    static ASCIILiteral yearString(Year);
    static ASCIILiteral monthString(Month);
    static ASCIILiteral dayString(Day);
    static ASCIILiteral dayPeriodString(DayPeriod);
    static ASCIILiteral hourString(Hour);
    static ASCIILiteral minuteString(Minute);
    static ASCIILiteral secondString(Second);
    static ASCIILiteral timeZoneNameString(TimeZoneName);
    static ASCIILiteral formatStyleString(DateTimeStyle);

    static HourCycle hourCycleFromSymbol(UChar);
    static HourCycle parseHourCycle(const String&);
    static HourCycle hourCycleFromPattern(const Vector<UChar, 32>&);
    static void replaceHourCycleInSkeleton(Vector<UChar, 32>&, bool hour12);
    static void replaceHourCycleInPattern(Vector<UChar, 32>&, HourCycle);

    using UDateFormatDeleter = ICUDeleter<udat_close>;

    WriteBarrier<JSBoundFunction> m_boundFormat;
    std::unique_ptr<UDateFormat, UDateFormatDeleter> m_dateFormat;
    std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter> m_dateIntervalFormat;

    String m_locale;
    String m_dataLocale;
    String m_calendar;
    String m_numberingSystem;
    String m_timeZone;
    HourCycle m_hourCycle { HourCycle::None };
    Weekday m_weekday { Weekday::None };
    Era m_era { Era::None };
    Year m_year { Year::None };
    Month m_month { Month::None };
    Day m_day { Day::None };
    DayPeriod m_dayPeriod { DayPeriod::None };
    Hour m_hour { Hour::None };
    Minute m_minute { Minute::None };
    Second m_second { Second::None };
    uint8_t m_fractionalSecondDigits { 0 };
    TimeZoneName m_timeZoneName { TimeZoneName::None };
    DateTimeStyle m_dateStyle { DateTimeStyle::None };
    DateTimeStyle m_timeStyle { DateTimeStyle::None };
};

} // namespace JSC
