/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "ISO8601.h"
#include "JSObject.h"
#include <unicode/udat.h>
#include <unicode/uformattedvalue.h>
#include <wtf/unicode/icu/ICUHelpers.h>

struct UDateIntervalFormat;
struct UFormattedDateInterval;

namespace JSC {

enum class RelevantExtensionKey : uint8_t;

class JSBoundFunction;

struct UDateIntervalFormatDeleter {
    JS_EXPORT_PRIVATE void operator()(UDateIntervalFormat*);
};

struct UFormattedDateIntervalDeleter {
    JS_EXPORT_PRIVATE void operator()(UFormattedDateInterval*);
};

using UDateFormatDeleter = ICUDeleter<udat_close>;

class IntlDateTimeFormatImpl;

class IntlDateTimeFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlDateTimeFormat*>(cell)->IntlDateTimeFormat::~IntlDateTimeFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlDateTimeFormatSpace<mode>();
    }

    static IntlDateTimeFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    DECLARE_VISIT_CHILDREN;

    enum class RequiredComponent : uint8_t { Date, Time, Any };
    enum class Defaults : uint8_t { Date, Time, All, ZonedDateTime };
    enum class HourCycle : uint8_t { None, H11, H12, H23, H24 };

    void initializeDateTimeFormat(JSGlobalObject*, JSValue locales, JSValue options, RequiredComponent, Defaults, StringView toLocaleStringTimeZone = { });
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimevalue dispatches to:
    //   None          -> HandleDateTimeOthers          -> [[DateTimeFormat]]
    //   Instant       -> HandleDateTimeTemporalInstant -> [[TemporalInstantFormat]]
    //   PlainDate     -> HandleDateTimeTemporalDate    -> [[TemporalPlainDateFormat]]
    //   PlainDateTime -> HandleDateTimeTemporalDateTime-> [[TemporalPlainDateTimeFormat]]
    //   PlainTime     -> HandleDateTimeTemporalTime    -> [[TemporalPlainTimeFormat]]
    //   PlainYearMonth-> HandleDateTimeTemporalYearMonth->[[TemporalPlainYearMonthFormat]]
    //   PlainMonthDay -> HandleDateTimeTemporalMonthDay-> [[TemporalPlainMonthDayFormat]]
    //   ZonedDateTime -> spec throws TypeError (use toLocaleString() or convert to PlainDateTime first)
    enum class TemporalFieldKind : uint8_t {
        None = 0, // Non-Temporal (legacy Date)
        Instant, // Temporal.Instant — all fields, formatter's timezone
        ZonedDateTime, // Temporal.ZonedDateTime — all fields + timezone name
        PlainDate, // Date fields only, GMT
        PlainDateTime, // Date + time fields, GMT
        PlainTime, // Time fields only, GMT
        PlainYearMonth, // Year + month fields, GMT
        PlainMonthDay, // Month + day fields, GMT
        Count // sentinel — must remain last
    };

    struct DateTimeValueRecord {
        double value;
        TemporalFieldKind kind;
        // [[IsPlain]] from the spec Value Format Record is not stored here: our ICU-based
        // implementation derives plain/non-plain from `kind` directly — getTemporalFormatter
        // uses GMT for plain types and createTemporalIntervalFormat recomputes it as
        // (kind != Instant), so no separate field is needed.
    };
    static DateTimeValueRecord handleDateTimeValue(JSGlobalObject*, const IntlDateTimeFormat*, JSValue);

    JSValue format(JSGlobalObject*, double value) const;
    // https://tc39.es/proposal-temporal/#sec-formatdatetime
    JSValue format(JSGlobalObject*, JSValue) const;
    JSValue formatToParts(JSGlobalObject*, JSValue) const;
    JSValue formatRange(JSGlobalObject*, JSValue startDate, JSValue endDate);
    JSValue formatRangeToParts(JSGlobalObject*, JSValue startDate, JSValue endDate);
    JSObject* resolvedOptions(JSGlobalObject*) const;

    static bool isTemporalObject(JSValue);
    static bool sameTemporalType(JSValue, JSValue);

    JSBoundFunction* boundFormat() const LIFETIME_BOUND { return m_boundFormat.get(); }
    void setBoundFormat(VM&, JSBoundFunction*);

    static IntlDateTimeFormat* unwrapForOldFunctions(JSGlobalObject*, JSValue);

    static HourCycle NODELETE hourCycleFromPattern(const Vector<char16_t, 32>&);

    const IntlDateTimeFormatImpl& impl() const LIFETIME_BOUND { return *m_impl; }
    void setImpl(Ref<const IntlDateTimeFormatImpl>&& impl) { m_impl = WTF::move(impl); }

    enum class TimeZoneName : uint8_t { None, Short, Long, ShortOffset, LongOffset, ShortGeneric, LongGeneric };
    enum class DateTimeStyle : uint8_t { None, Full, Long, Medium, Short };

    DateTimeStyle dateStyle() const;
    DateTimeStyle timeStyle() const;
    TimeZoneName timeZoneName() const;
    const String& ensureCalendar() const;
    const String& ensureNumberingSystem() const;

    static bool calendarMatchesICU(StringView temporalId, const String& icuCalId);

    using UDateFormatDeleter = ICUDeleter<udat_close>;
    UDateFormat* getTemporalFormatter(VM&, TemporalFieldKind) const; // Non-owning; no clone needed (single-threaded JS).
    std::unique_ptr<UDateFormat, UDateFormatDeleter> computeTemporalFormatter(TemporalFieldKind) const;
    std::unique_ptr<UDateFormat, UDateFormatDeleter> computeAdjustDateTimeStyleFormat(TemporalFieldKind, const Vector<char16_t, 32>& skeleton) const;
    std::unique_ptr<UDateFormat, UDateFormatDeleter> computeGetDateTimeFormat(TemporalFieldKind) const;
    std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter> createTemporalIntervalFormat(UDateFormat*, TemporalFieldKind, UErrorCode&) const;

    struct DateRangePreamble {
        std::unique_ptr<UFormattedDateInterval, UFormattedDateIntervalDeleter> result;
        const UFormattedValue* formattedValue { nullptr };
        bool equal { false };
    };
    std::optional<DateRangePreamble> prepareDateRange(JSGlobalObject*, double& startDate, double& endDate);

    // https://tc39.es/proposal-temporal/#sec-partitiondatetimerangepattern
    struct TemporalDateRangePreamble {
        UDateFormat* tempFormat { nullptr }; // Non-owning; lifetime guaranteed by IntlDateTimeFormatImpl.
        std::unique_ptr<UFormattedDateInterval, UFormattedDateIntervalDeleter> result;
        const UFormattedValue* formattedValue { nullptr };
        bool equal { false };
        double startMs { 0 };
        double endMs { 0 };
        TemporalFieldKind kind { TemporalFieldKind::None };
    };
    std::optional<TemporalDateRangePreamble> partitionDateTimeRangePattern(JSGlobalObject*, JSValue x, JSValue y);

private:
    friend class IntlDateTimeFormatImpl;

    IntlDateTimeFormat(VM&, Structure*);
    DECLARE_DEFAULT_FINISH_CREATION;

    static Vector<String> localeData(const String&, RelevantExtensionKey);

    UDateIntervalFormat* createDateIntervalFormatIfNecessary(JSGlobalObject*);

    enum class Weekday : uint8_t { None, Narrow, Short, Long };
    enum class Era : uint8_t { None, Narrow, Short, Long };
    enum class Year : uint8_t { None, TwoDigit, Numeric };
    enum class Month : uint8_t { None, TwoDigit, Numeric, Narrow, Short, Long };
    enum class Day : uint8_t { None, TwoDigit, Numeric };
    enum class DayPeriod : uint8_t { None, Narrow, Short, Long };
    enum class Hour : uint8_t { None, TwoDigit, Numeric };
    enum class Minute : uint8_t { None, TwoDigit, Numeric };
    enum class Second : uint8_t { None, TwoDigit, Numeric };

    static void NODELETE setFormatsFromPattern(IntlDateTimeFormatImpl&, StringView);
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

    static HourCycle NODELETE hourCycleFromSymbol(char16_t);
    static HourCycle parseHourCycle(const String&);
    static void NODELETE replaceHourCycleInSkeleton(Vector<char16_t, 32>&, bool hour12);
    static void replaceHourCycleInPattern(Vector<char16_t, 32>&, HourCycle);
    static String buildSkeleton(Weekday, Era, Year, Month, Day, TriState, HourCycle, Hour, DayPeriod, Minute, Second, unsigned, TimeZoneName);

    WriteBarrier<JSBoundFunction> m_boundFormat;
    std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter> m_dateIntervalFormat;
    RefPtr<const IntlDateTimeFormatImpl> m_impl;
};

class IntlDateTimeFormatTemporalFormatterCache {
    WTF_MAKE_TZONE_ALLOCATED(IntlDateTimeFormatTemporalFormatterCache);
    WTF_MAKE_NONCOPYABLE(IntlDateTimeFormatTemporalFormatterCache);
public:
    IntlDateTimeFormatTemporalFormatterCache() = default;
    std::array<std::unique_ptr<UDateFormat, IntlDateTimeFormat::UDateFormatDeleter>, static_cast<size_t>(IntlDateTimeFormat::TemporalFieldKind::Count)> m_formatters { };
};

class IntlDateTimeFormatImpl : public RefCounted<IntlDateTimeFormatImpl> {
    WTF_MAKE_TZONE_ALLOCATED(IntlDateTimeFormatImpl);
    WTF_MAKE_NONCOPYABLE(IntlDateTimeFormatImpl);
public:
    static Ref<IntlDateTimeFormatImpl> create() { return adoptRef(*new IntlDateTimeFormatImpl); }

    String m_locale;
    String m_dataLocale;
    mutable String m_calendar;
    mutable String m_numberingSystem;
    TimeZone m_timeZone;
    // Time zone string returned by resolvedOptions().timeZone. Per spec this is
    // [[Identifier]] (the case-normalized accepted form, e.g. "Asia/Calcutta"),
    // not [[PrimaryIdentifier]] (e.g. "Asia/Kolkata"). For UTC offset inputs
    // this is the canonical "+HH:MM" form. m_timeZone holds the canonicalized
    // primary used for ICU formatting.
    String m_timeZoneForResolvedOptions;
    IntlDateTimeFormat::HourCycle m_hourCycle { IntlDateTimeFormat::HourCycle::None };
    IntlDateTimeFormat::Weekday m_weekday { IntlDateTimeFormat::Weekday::None };
    IntlDateTimeFormat::Era m_era { IntlDateTimeFormat::Era::None };
    IntlDateTimeFormat::Year m_year { IntlDateTimeFormat::Year::None };
    IntlDateTimeFormat::Month m_month { IntlDateTimeFormat::Month::None };
    IntlDateTimeFormat::Day m_day { IntlDateTimeFormat::Day::None };
    IntlDateTimeFormat::DayPeriod m_dayPeriod { IntlDateTimeFormat::DayPeriod::None };
    IntlDateTimeFormat::Hour m_hour { IntlDateTimeFormat::Hour::None };
    IntlDateTimeFormat::Minute m_minute { IntlDateTimeFormat::Minute::None };
    IntlDateTimeFormat::Second m_second { IntlDateTimeFormat::Second::None };
    uint8_t m_fractionalSecondDigits { 0 };
    IntlDateTimeFormat::TimeZoneName m_timeZoneName { IntlDateTimeFormat::TimeZoneName::None };
    IntlDateTimeFormat::DateTimeStyle m_dateStyle { IntlDateTimeFormat::DateTimeStyle::None };
    IntlDateTimeFormat::DateTimeStyle m_timeStyle { IntlDateTimeFormat::DateTimeStyle::None };
    bool m_anyPresent { false };
    Vector<char16_t, 32> m_userSkeleton; // user's explicit options as skeleton, before defaults injection; used by computeGetDateTimeFormat
    std::unique_ptr<UDateFormat, UDateFormatDeleter> m_dateFormat;
    mutable std::unique_ptr<IntlDateTimeFormatTemporalFormatterCache> m_temporalFormatterCache;

private:
    IntlDateTimeFormatImpl() = default;
};

} // namespace JSC
