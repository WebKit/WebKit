/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CalendarICUBridge.h"

#include "DateConstructor.h"
#include "ISOArithmetic.h"
#include "IntlObject.h"
#include <unicode/ucal.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DateMath.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {
namespace TemporalCore {

CalendarID calendarIDFromString(StringView identifier)
{
    const auto& calendars = intlAvailableCalendars();
    for (unsigned i = 0; i < calendars.size(); ++i) {
        if (calendars[i] == identifier)
            return i;
    }
    return iso8601CalendarID();
}

// buildICULocale — internal: maps BCP47 calendar ID to ICU locale string
static CString buildICULocale(StringView calendarId)
{
    String bcp47(calendarId.toString());
    auto mapped = mapBCP47ToICUCalendarKeyword(bcp47);
    auto icuKeyword = mapped ? *mapped : bcp47;
    return makeString("und@calendar="_s, icuKeyword).utf8();
}

// buildCalendarTemplate — internal: opens ICU UCalendar for the given CalendarID, set to UTC.
// NOTE: For Gregory/ISO/Japanese/Buddhist/Roc: sets Gregorian change date to -infinity for proleptic Gregorian arithmetic.
static std::unique_ptr<UCalendar, ICUDeleter<ucal_close>> buildCalendarTemplate(const AbstractLocker&, CalendarID calendarId)
{
    auto str = calendarIDToString(calendarId);
    auto locale = buildICULocale(str);
    UErrorCode status = U_ZERO_ERROR;
    auto cal = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(ucal_open(u"UTC", 3, locale.data(), UCAL_DEFAULT, &status));
    if (U_FAILURE(status)) [[unlikely]]
        return nullptr;
    // Set to ExactTime::minValue in ms — the minimum representable Temporal instant — making ICU
    // use proleptic Gregorian for all valid dates (effectively -infinity for our purposes).
    // ucal_setGregorianChange is only supported on the base Gregorian calendar; ICU returns
    // U_UNSUPPORTED_ERROR for derived calendars (Japanese, Buddhist, ROC). Those derived
    // calendars already use proleptic-Gregorian arithmetic in the years Temporal cares about,
    // so calling ucal_setGregorianChange would be a no-op and we skip it.
    if (calendarId == gregoryCalendarID() || calendarIsISO(calendarId)) {
        const double prolepticGregorianChangeMs = -8.64e15; // ExactTime::minValue / nsPerMillisecond
        ucal_setGregorianChange(cal.get(), prolepticGregorianChangeMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return nullptr;
    }
    return cal;
}

struct CalendarCacheEntry final : public ThreadSafeRefCounted<CalendarCacheEntry> {
    WTF_MAKE_TZONE_ALLOCATED(CalendarCacheEntry);
public:
    Lock useLock;
    std::unique_ptr<UCalendar, ICUDeleter<ucal_close>> cal;
};
WTF_MAKE_TZONE_ALLOCATED_IMPL(CalendarCacheEntry);

struct CalendarLRUCachePolicy {
    static bool isKeyNull(const CalendarID&) { return false; }
    static RefPtr<CalendarCacheEntry> createValueForNullKey() { return nullptr; }
    static RefPtr<CalendarCacheEntry> createValueForKey(const CalendarID&) { return adoptRef(*new CalendarCacheEntry); }
    static CalendarID createKeyForStorage(const CalendarID& id) { return id; }
};

static RefPtr<CalendarCacheEntry> calendarCacheEntry(CalendarID calendarId)
{
    static Lock cacheLock;
    static LazyNeverDestroyed<TinyLRUCache<CalendarID, RefPtr<CalendarCacheEntry>, 8, CalendarLRUCachePolicy>> cache;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        cache.construct();
    });
    Locker locker { cacheLock };
    return cache.get().get(calendarId);
}

template<typename F>
static auto withCalendar(CalendarID calendarId, F&& fn) -> decltype(fn(static_cast<UCalendar*>(nullptr)))
{
    auto entry = calendarCacheEntry(calendarId);
    ASSERT(entry);
    Locker locker { entry->useLock };
    if (!entry->cal)
        entry->cal = buildCalendarTemplate(locker, calendarId);
    if (entry->cal)
        ucal_clear(entry->cal.get());
    return fn(entry->cal.get());
}

// lunarCalendarExtendedYearFor1972 — probes UCAL_EXTENDED_YEAR for the given lunisolar calendar
// at ISO 1972-02-15. Returns 1972 on ISO-proleptic ICU; epoch-based year on older Apple ICU.
// Chinese uses epoch 2637 BCE -> returns 4609 on ICU ≥76; Dangi uses epoch 2333 BCE -> returns 4305.
// Result is cached per calendar: ICU version is fixed for the process lifetime.
int32_t lunarCalendarExtendedYearFor1972(CalendarID calendarId)
{
    static std::atomic<int32_t> chineseCached { INT32_MIN };
    static std::atomic<int32_t> dangiCached { INT32_MIN };
    auto& cached = (calendarId == dangiCalendarID()) ? dangiCached : chineseCached;
    int32_t value = cached.load(std::memory_order_relaxed);
    if (value != INT32_MIN)
        return value;
    // Use ucal_setMillis with a precomputed ISO epoch time — NOT ucal_setDateTime which
    // sets calendar-native fields (not ISO fields) on a non-Gregorian calendar.
    // ISO 1972-02-15 00:00:00 UTC = 66,960,000,000 ms from Unix epoch (1970-01-01).
    static constexpr double iso1972Feb15EpochMs = 66960000000.0;
    value = withCalendar(calendarId, [&](UCalendar* cal) -> int32_t {
        if (!cal)
            return 1972; // fallback: assume ISO-proleptic
        UErrorCode status = U_ZERO_ERROR;
        ucal_setMillis(cal, iso1972Feb15EpochMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return 1972; // fallback: assume ISO-proleptic
        int32_t extYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return 1972; // fallback: assume ISO-proleptic
        // Return the raw EXTENDED_YEAR: 1972 on ISO-proleptic ICU,
        // epoch-based year (whatever the current ICU uses) on older Apple ICU.
        return extYear;
    });
    cached.store(value, std::memory_order_relaxed);
    return value;
}

// isoDateToEpochMs — internal: converts ISO PlainDate to epoch ms at noon UTC (avoids DST boundary issues)
static double isoDateToEpochMs(const ISO8601::PlainDate& date)
{
    const double noonEpochOffsetMs = 43'200'000.0; // nsPerDay / nsPerMillisecond / 2
    double days = makeDay(date.year(), date.month() - 1, date.day());
    return makeDate(days, noonEpochOffsetMs);
}

// setCalendarToISODate — internal: sets ICU calendar to a specific ISO date via epoch milliseconds
static bool setCalendarToISODate(UCalendar* cal, const ISO8601::PlainDate& isoDate)
{
    // Use a proleptic Gregorian calendar to compute epoch ms, then set ICU calendar.
    double epochMs = isoDateToEpochMs(isoDate);
    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(cal, epochMs, &status);
    return U_SUCCESS(status);
}

// isoDateFromCalendarChecked — internal: reads back ISO date from ICU calendar's current epoch ms; returns nullopt if out of representable range
static std::optional<ISO8601::PlainDate> isoDateFromCalendarChecked(UCalendar* cal)
{
    UErrorCode status = U_ZERO_ERROR;
    double epochMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    WTF::Int64Milliseconds msWT(static_cast<int64_t>(epochMs));
    int32_t days = WTF::msToDays(msWT);
    auto [year, month, day] = WTF::yearMonthDayFromDays(days);
    if (!ISO8601::isYearWithinLimits(year)) [[unlikely]]
        return std::nullopt;
    return ISO8601::PlainDate(year, static_cast<uint8_t>(month + 1), static_cast<uint8_t>(day));
}

// Japanese era table — start years are historically fixed calendar facts.
// icu4x: components/calendar/src/cal/japanese.rs Japanese::eras()
struct JapaneseEra {
    int32_t startYear;
    ASCIILiteral name;
};
static constexpr auto japaneseEras = WTF::toArray<JapaneseEra>({
    { 2019, "reiwa"_s },
    { 1989, "heisei"_s },
    { 1926, "showa"_s },
    { 1912, "taisho"_s },
    { 1868, "meiji"_s },
});

// japaneseEraStartYear — no spec AO, no ICU4X equivalent (ICU4X keys on EraStartDate directly).
// Bridges ICU4C's UCAL_ERA integer to a Gregorian start year for lookup in japaneseEras[].
static std::optional<int32_t> japaneseEraStartYear(int32_t icuEraCode)
{
    return withCalendar(japaneseCalendarID(), [&](UCalendar* cal) -> std::optional<int32_t> {
        if (!cal)
            return std::nullopt;
        UErrorCode status = U_ZERO_ERROR;
        ucal_set(cal, UCAL_ERA, icuEraCode);
        ucal_set(cal, UCAL_YEAR, 2);
        // YEAR=2: YEAR=1/January doesn't exist for eras starting mid-year (e.g. Showa on Dec 25).
        ucal_set(cal, UCAL_MONTH, UCAL_JANUARY);
        ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
        int32_t extYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        return extYear - 1; // EXTENDED_YEAR of YEAR=2/Jan 1 is eraStartYear+1.
    });
}

// japaneseEraCode — inverse of japaneseEraStartYear: derives the ICU era integer from a known era start year.
static std::optional<int32_t> japaneseEraCode(int32_t startYear)
{
    return withCalendar(japaneseCalendarID(), [&](UCalendar* cal) -> std::optional<int32_t> {
        if (!cal)
            return std::nullopt;
        UErrorCode status = U_ZERO_ERROR;
        ucal_set(cal, UCAL_EXTENDED_YEAR, startYear + 1); // startYear+1/Jan 1 is always within the era.
        ucal_set(cal, UCAL_MONTH, UCAL_JANUARY);
        ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
        int32_t eraCode = ucal_get(cal, UCAL_ERA, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        return eraCode;
    });
}

// mapICUEraToTemporalEra — ICU4C has no locale-independent era identifier API (UDAT_ERA_NAMES is locale-dependent).
// For non-Japanese calendars, era indices are 0/1 and fixed by the calendar system — hardcoded string mapping suffices.
// For Japanese, era indices grow as new eras are added; japaneseEraStartYear derives the name via ICU4C dynamically.
// Era strings match icu4x era_year_from_extended (gregorian.rs, japanese.rs, coptic.rs, ethiopian.rs, hebrew.rs, persian.rs, indian.rs, roc.rs).
static std::optional<String> mapICUEraToTemporalEra(CalendarID calendarId, int32_t icuEra)
{
    if (!calendarHasEras(calendarId))
        return std::nullopt;

    if (calendarId == gregoryCalendarID())
        return !icuEra ? "bce"_s : "ce"_s;
    if (calendarId == buddhistCalendarID())
        return "be"_s;
    if (calendarId == japaneseCalendarID()) {
        auto startYear = japaneseEraStartYear(icuEra);
        if (!startYear) [[unlikely]]
            return std::nullopt;
        for (auto& e : japaneseEras) {
            if (*startYear == e.startYear)
                return String(e.name);
        }
        return *startYear > 0 ? "ce"_s : "bce"_s;
    }
    if (calendarId == rocCalendarID())
        return !icuEra ? "broc"_s : "roc"_s;
    if (calendarId == copticCalendarID() || calendarId == ethiopicCalendarID())
        return "am"_s;
    if (calendarId == ethioaaCalendarID())
        return "aa"_s;
    if (calendarId == hebrewCalendarID())
        return "am"_s;
    if (calendarId == indianCalendarID())
        return "shaka"_s;
    if (calendarId == persianCalendarID())
        return "ap"_s;
    if (calendarIsIslamic(calendarId))
        return "ah"_s;
    return std::nullopt;
}

// getMonthCode — internal: returns monthCode string from ICU calendar state (e.g. "M05L" for Hebrew Adar I).
// Read-only: does not modify cal.
// icu4x: components/calendar/src/cal/hebrew.rs (Hebrew special case)
// icu4x: components/calendar/src/cal/chinese_based.rs (Chinese/Dangi IS_LEAP_MONTH)
static std::optional<String> getMonthCode(UCalendar* cal, CalendarID calendarId)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t ucalMonth = ucal_get(cal, UCAL_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    if (calendarId == hebrewCalendarID()) {
        // ICU4C Hebrew never sets IS_LEAP_MONTH=1. Instead, leap years have 13 slots
        // (maxMonth=12, UCAL_MONTH 0-12) and non-leap years have 12 slots (maxMonth=11).
        // Leap year slot 5 = Adar I (M05L); non-leap slot 5 = Adar (M06).
        int32_t maxMonth = ucal_getLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        bool isLeapYear = (maxMonth == 12);
        if (isLeapYear && ucalMonth == 5)
            return String("M05L"_s);
        int32_t codeNum;
        if (isLeapYear && ucalMonth >= 6)
            codeNum = ucalMonth; // leap post-Adar-I: M06-M12 for slots 6-12
        else
            codeNum = ucalMonth + 1; // non-leap all slots, or leap slots 0-4: M01-M05
        return makeString("M"_s, codeNum < 10 ? "0"_s : ""_s, codeNum);
    }

    // Chinese/Dangi: IS_LEAP_MONTH distinguishes leap months on the same UCAL_MONTH slot.
    int32_t month = ucalMonth + 1;
    int32_t isLeap = ucal_get(cal, UCAL_IS_LEAP_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    if (isLeap)
        return makeString("M"_s, month < 10 ? "0"_s : ""_s, month, "L"_s);
    return makeString("M"_s, month < 10 ? "0"_s : ""_s, month);
}

// computeOrdinalMonth — internal: returns 1-based ordinal month position in year, counting leap months for lunisolar
// icu4x: components/calendar/src/date.rs Date::month().ordinal
// NOTE: mutates cal (walks months via ucal_add). Must be the last ICU cal operation in a withCalendar lambda;
//       withCalendar calls ucal_clear before the next use so no restore is needed.
static std::optional<uint8_t> computeOrdinalMonth(UCalendar* cal, CalendarID calendarId)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t month = ucal_get(cal, UCAL_MONTH, &status) + 1;
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    if (!calendarIsLunisolar(calendarId))
        return static_cast<uint8_t>(month); // For lunisolar: the ordinal month includes leap months.
    // UCAL_MONTH gives the underlying month index (leap months share same index).
    // Count how many months from start of year to current position.
    int32_t isLeap = ucal_get(cal, UCAL_IS_LEAP_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    ucal_set(cal, UCAL_MONTH, 0);
    ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
    ucal_set(cal, UCAL_DAY_OF_MONTH, 1);

    int32_t ordinal = 1;
    int32_t targetMonth = month - 1; // 0-indexed for comparison with UCAL_MONTH
    int32_t targetIsLeap = isLeap;
    for (int i = 0; i < 15; i++) {
        // max 13 months + safety
        int32_t curMonth = ucal_get(cal, UCAL_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        int32_t curLeap = ucal_get(cal, UCAL_IS_LEAP_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        if (curMonth == targetMonth && curLeap == targetIsLeap)
            break;
        ucal_add(cal, UCAL_MONTH, 1, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        ordinal++;
    }

    return static_cast<uint8_t>(ordinal);
}

// mapTemporalEraToICUEra — ICU4C has no set-by-era-name API; ucal_set only accepts UCAL_ERA as integer.
// For non-Japanese calendars, era indices are 0/1 and fixed by the calendar system — hardcoded mapping suffices.
// For Japanese, japaneseEraCode derives the integer via ICU4C dynamically using the era's historically-fixed start year.
// Era strings match icu4x extended_year_from_era_year_unchecked (gregorian.rs, japanese.rs, coptic.rs, ethiopian.rs, hebrew.rs, persian.rs, indian.rs, roc.rs).
static std::optional<int32_t> mapTemporalEraToICUEra(CalendarID calendarId, StringView era)
{
    if (calendarId == gregoryCalendarID()) {
        if (era == "bce"_s)
            return 0;
        if (era == "ce"_s)
            return 1;
        return std::nullopt;
    }
    if (calendarId == buddhistCalendarID())
        return era == "be"_s ? std::optional<int32_t>(0) : std::nullopt;
    if (calendarId == japaneseCalendarID()) {
        for (auto& e : japaneseEras) {
            if (era == StringView(e.name))
                return japaneseEraCode(e.startYear);
        }
        if (era == "ce"_s)
            return 0;
        return std::nullopt;
    }
    if (calendarId == rocCalendarID()) {
        if (era == "broc"_s)
            return 0;
        if (era == "roc"_s)
            return 1;
        return std::nullopt;
    }
    if (calendarId == copticCalendarID() || calendarId == ethiopicCalendarID())
        return era == "am"_s ? std::optional<int32_t>(0) : std::nullopt;
    if (calendarId == ethioaaCalendarID())
        return era == "aa"_s ? std::optional<int32_t>(0) : std::nullopt;
    if (calendarId == hebrewCalendarID())
        return era == "am"_s ? std::optional<int32_t>(0) : std::nullopt;
    if (calendarId == indianCalendarID())
        return era == "shaka"_s ? std::optional<int32_t>(0) : std::nullopt;
    if (calendarId == persianCalendarID())
        return era == "ap"_s ? std::optional<int32_t>(0) : std::nullopt;
    if (calendarIsIslamic(calendarId))
        return era == "ah"_s ? std::optional<int32_t>(0) : std::nullopt;
    return std::nullopt;
}

// isoToCalendarFields — no single temporal_rs equivalent; aggregates Calendar::year/month/month_code/day/era into one ICU pass.
TemporalResult<CalendarFields> isoToCalendarFields(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    struct RawFields {
        int32_t extendedYear { 0 };
        int32_t ucalEra { 0 };
        int32_t ucalYear { 0 };
        int32_t day { 0 };
        std::optional<uint8_t> ordinalMonth;
        std::optional<String> monthCode;
        bool hasEra { false };
    };

    auto rawOrError = withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<RawFields> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        UErrorCode status = U_ZERO_ERROR;
        RawFields raw;
        // NOTE: ROC UCAL_EXTENDED_YEAR may return Gregorian year on some ICU versions; handled below.
        raw.extendedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.ucalEra = ucal_get(cal, UCAL_ERA, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.ucalYear = ucal_get(cal, UCAL_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.monthCode = getMonthCode(cal, calendarId);
        if (!raw.monthCode) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.hasEra = calendarHasEras(calendarId);
        raw.ordinalMonth = computeOrdinalMonth(cal, calendarId);
        if (!raw.ordinalMonth) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return raw;
    });

    if (!rawOrError)
        return makeUnexpected(rawOrError.error());
    auto& raw = *rawOrError;

    CalendarFields fields;
    fields.year = raw.extendedYear;
    if (calendarId == rocCalendarID())
        fields.year = !raw.ucalEra ? -(raw.ucalYear - 1) : raw.ucalYear;
    fields.month = *raw.ordinalMonth;
    fields.day = static_cast<uint8_t>(raw.day);
    fields.monthCode = WTF::move(*raw.monthCode);

    // isLeapMonth is re-derived from the month code (avoids needing UCAL_IS_LEAP_MONTH in raw)
    fields.isLeapMonth = fields.monthCode.endsWith("L"_s);

    if (raw.hasEra) {
        // mapICUEraToTemporalEra for Japanese calls japaneseEraStartYear which uses withCalendar
        // on the same calendarId — safe because the outer lock was released above.
        fields.era = mapICUEraToTemporalEra(calendarId, raw.ucalEra);
        fields.eraYear = raw.ucalYear;
        if (calendarId == japaneseCalendarID()) {
            if (isoDate.year() < 1873) {
                fields.era = isoDate.year() > 0 ? "ce"_s : "bce"_s;
                fields.eraYear = isoDate.year() > 0 ? isoDate.year() : (1 - isoDate.year());
            } else if (fields.era && *fields.era == "ce"_s)
                fields.eraYear = isoDate.year();
        }
    }

    return fields;
}

// calendarYear — temporal_rs: Calendar::year (src/builtins/core/calendar.rs)
//                icu4x: Date::extended_year (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[Year]] field:
//   1. (iso8601) Return isoDate.[[Year]].
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<int32_t> calendarYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the extended year of isoDate in calendarId.
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        // NOTE: ROC UCAL_EXTENDED_YEAR may return Gregorian year on some ICU versions; compute from era+year.
        if (calendarId == rocCalendarID()) {
            int32_t era = ucal_get(cal, UCAL_ERA, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            int32_t eraYear = ucal_get(cal, UCAL_YEAR, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            return !era ? -(eraYear - 1) : eraYear;
        }
        auto result = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return result;
    });
}

// calendarMonth — temporal_rs: Calendar::month (src/builtins/core/calendar.rs)
//                 icu4x: Date::month().ordinal (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[Month]] field:
//   1. (iso8601) Return isoDate.[[Month]].
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<uint8_t> calendarMonth(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the ordinal month (1-based position in year, counting leap months) of isoDate in calendarId.
    // NOTE: Japanese pre-1873 "ce"/"bce" eras use ISO month directly to bypass ICU Julian conversion.
    if (calendarId == japaneseCalendarID() && isoDate.year() < 1873)
        return isoDate.month();
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<uint8_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        auto ordinal = computeOrdinalMonth(cal, calendarId);
        if (!ordinal) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return *ordinal;
    });
}

// calendarMonthCode — temporal_rs: Calendar::month_code (src/builtins/core/calendar.rs)
//                     icu4x: Date::month().to_input().code() (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[MonthCode]] field:
//   1. (iso8601) Return CreateMonthCode(isoDate.[[Month]], false).
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<String> calendarMonthCode(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the month code string (e.g. "M01", "M05L") of isoDate in calendarId.
    // NOTE: Japanese pre-1873 "ce"/"bce" eras use ISO month code directly to bypass ICU Julian conversion.
    if (calendarId == japaneseCalendarID() && isoDate.year() < 1873)
        return ISO8601::monthCode(isoDate.month());
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<String> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        auto code = getMonthCode(cal, calendarId);
        if (!code) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return WTF::move(*code);
    });
}

// calendarDay — temporal_rs: Calendar::day (src/builtins/core/calendar.rs)
//               icu4x: Date::day_of_month (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[Day]] field:
//   1. (iso8601) Return isoDate.[[Day]].
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<uint8_t> calendarDay(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the day-of-month of isoDate in calendarId.
    // NOTE: Japanese pre-1873 "ce"/"bce" eras return ISO day directly to bypass ICU Julian conversion.
    if (calendarId == japaneseCalendarID() && isoDate.year() < 1873)
        return isoDate.day();
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<uint8_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        int32_t day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return static_cast<uint8_t>(day);
    });
}

// calendarEra — temporal_rs: Calendar::era (src/builtins/core/calendar.rs)
//               icu4x: Date::year() returning YearInfo / era_year (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[Era]] field:
//   1. (iso8601) Return undefined.
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<std::optional<String>> calendarEra(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. If calendarId has no eras, return undefined.
    if (!calendarHasEras(calendarId))
        return std::optional<String>(std::nullopt);
    // 2. Return the era string for isoDate in calendarId (e.g. "ce", "bce", "reiwa").
    // NOTE: Japanese dates before 1873 use "ce"/"bce" per spec, not ICU era names.
    if (calendarId == japaneseCalendarID() && isoDate.year() < 1873)
        return std::optional<String>(isoDate.year() > 0 ? String("ce"_s) : String("bce"_s));
    // Read UCAL_ERA inside the lock, then map to era string outside (mapICUEraToTemporalEra
    // for Japanese calls japaneseEraStartYear which needs its own withCalendar call).
    auto icuEraOrError = withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        int32_t icuEra = ucal_get(cal, UCAL_ERA, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return icuEra;
    });
    if (!icuEraOrError)
        return makeUnexpected(icuEraOrError.error());
    return mapICUEraToTemporalEra(calendarId, *icuEraOrError);
}

// calendarEraYear — temporal_rs: Calendar::era_year (src/builtins/core/calendar.rs)
//                   icu4x: Date::era_year (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[EraYear]] field:
//   1. (iso8601) Return undefined.
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<std::optional<int32_t>> calendarEraYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. If calendarId has no eras, return undefined.
    if (!calendarHasEras(calendarId))
        return std::optional<int32_t>(std::nullopt);
    // 2. Return the era year (year within the current era) of isoDate in calendarId.
    // NOTE: Japanese "ce"/"bce" fallback: eraYear is the Gregorian year.
    if (calendarId == japaneseCalendarID() && isoDate.year() < 1873)
        return std::optional<int32_t>(isoDate.year() > 0 ? isoDate.year() : (1 - isoDate.year()));

    struct RawEraYear {
        int32_t eraYear { 0 };
        int32_t icuEra { 0 };
    };
    auto rawOrError = withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<RawEraYear> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        RawEraYear raw;
        raw.eraYear = ucal_get(cal, UCAL_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.icuEra = ucal_get(cal, UCAL_ERA, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return raw;
    });
    if (!rawOrError)
        return makeUnexpected(rawOrError.error());

    int32_t eraYear = rawOrError->eraYear;
    if (calendarId == japaneseCalendarID()) {
        // isoDate.year() >= 1873 here (earlier case handled above).
        // mapICUEraToTemporalEra calls japaneseEraStartYear which uses its own withCalendar.
        auto era = mapICUEraToTemporalEra(calendarId, rawOrError->icuEra);
        if (era && *era == "ce"_s)
            eraYear = isoDate.year();
    }
    return std::optional<int32_t>(eraYear);
}

// calendarDaysInMonth — temporal_rs: Calendar::days_in_month (src/builtins/core/calendar.rs)
//                       icu4x: Date::days_in_month (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[DaysInMonth]] field:
//   1. (iso8601) Return ISODaysInMonth(isoDate.[[Year]], isoDate.[[Month]]).
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<int32_t> calendarDaysInMonth(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the number of days in the month containing isoDate in calendarId.
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        auto result = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return result;
    });
}

// calendarDaysInYear — temporal_rs: Calendar::days_in_year (src/builtins/core/calendar.rs)
//                      icu4x: Date::days_in_year (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[DaysInYear]] field:
//   1. (iso8601) Return MathematicalDaysInYear(isoDate.[[Year]]).
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<int32_t> calendarDaysInYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the number of days in the year containing isoDate in calendarId.
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        int32_t result = ucal_getLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return result;
    });
}

// calendarMonthsInYear — temporal_rs: Calendar::months_in_year (src/builtins/core/calendar.rs)
//                        icu4x: Date::months_in_year (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[MonthsInYear]] field:
//   1. (iso8601) Return 12.
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<int32_t> calendarMonthsInYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return the number of months in the year containing isoDate in calendarId.
    // NOTE: Lunisolar calendars may have 12 or 13 months; requires walking all months.
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;

        if (calendarIsLunisolar(calendarId)) {
            // For lunisolar calendars, count months by walking from month 1 to end of year.
            // cal is local; mutating it has no observable effect outside this function.
            int32_t savedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            ucal_set(cal, UCAL_MONTH, 0);
            ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
            ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
            ucal_getMillis(cal, &status); // resolve
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            int32_t count = 1;
            for (int i = 0; i < 14; i++) {
                ucal_add(cal, UCAL_MONTH, 1, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                if (curYear != savedYear)
                    break;
                count++;
            }
            return count;
        }

        int32_t result = ucal_getLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM, &status) + 1;
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return result;
    });
}

// calendarInLeapYear — temporal_rs: Calendar::in_leap_year (src/builtins/core/calendar.rs)
//   temporal_rs delegates to icu4x: AnyCalendar::is_in_leap_year (components/calendar/src/date.rs)
//   ICU4C has no equivalent: lunisolar mirrors icu4x (months > 12); non-lunisolar uses ucal_getLimit(UCAL_DAY_OF_YEAR).
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[InLeapYear]] field:
//   1. (iso8601) MathematicalInLeapYear(EpochTimeForYear(year)).
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<bool> calendarInLeapYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    // 1. Return true if isoDate falls in a leap year in calendarId.
    // NOTE: Lunisolar calendars: leap = 13 months in year. Others: leap = extra days in year.
    if (calendarIsLunisolar(calendarId)) {
        auto months = calendarMonthsInYear(calendarId, isoDate);
        if (!months) [[unlikely]]
            return makeUnexpected(months.error());
        return *months > 12;
    }
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<bool> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        int32_t actualMax = ucal_getLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        int32_t leastMax = ucal_getLimit(cal, UCAL_DAY_OF_YEAR, UCAL_LEAST_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return actualMax > leastMax;
    });
}

// setCalendarToMonthCode — internal: sets ICU calendar to the first day of the month matching monthCode in the current year.
// icu4x: ArithmeticDate::from_input_year_month_code_day (components/calendar/src/calendar_arithmetic.rs)
// Returns: 1 = found exact, 0 = constrained (month code doesn't exist in year), -1 = error
static std::optional<int> setCalendarToMonthCode(UCalendar* cal, CalendarID calendarId, const String& monthCode)
{
    auto parsed = ISO8601::parseMonthCode(monthCode);
    if (!parsed)
        return std::nullopt;

    UErrorCode status = U_ZERO_ERROR;
    if (calendarId == hebrewCalendarID()) {
        // Hebrew: walk months in the target year to find the matching month code.
        // NOTE: UCAL_ACTUAL_MAXIMUM is unreliable for Hebrew; walk is correct and safe.
        int32_t savedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        ucal_set(cal, UCAL_MONTH, 0);
        ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
        ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
        ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;

        for (int i = 0; i < 15; i++) {
            auto curCode = getMonthCode(cal, calendarId);
            if (!curCode) [[unlikely]]
                return std::nullopt;
            if (*curCode == monthCode)
                return 1; // exact match
            ucal_add(cal, UCAL_MONTH, 1, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return std::nullopt;
            int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return std::nullopt;
            if (curYear != savedYear) {
                // Month code doesn't exist in this year.
                // For M05L (Adar I), constrain to M06 (Adar, slot 5 in non-leap year).
                ucal_set(cal, UCAL_EXTENDED_YEAR, savedYear);
                ucal_set(cal, UCAL_MONTH, 5);
                ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
                ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
                return 0; // constrained
            }
        }
        return std::nullopt;
    }

    // Chinese/Dangi: walk months from start of year to find matching month code.
    int32_t savedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    ucal_set(cal, UCAL_MONTH, 0);
    ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
    ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
    ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    for (int i = 0; i < 14; i++) {
        auto curCode = getMonthCode(cal, calendarId);
        if (!curCode) [[unlikely]]
            return std::nullopt;
        if (*curCode == monthCode)
            return 1; // exact match
        ucal_add(cal, UCAL_MONTH, 1, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        if (curYear != savedYear) {
            // Month code doesn't exist in this year — constrain to last month.
            ucal_add(cal, UCAL_MONTH, -1, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return std::nullopt;
            return 0; // constrained
        }
    }
    return std::nullopt;
}

// compareSurpassesLexicographic — icu4x: compare_surpasses_lexicographic (components/calendar/src/calendar_arithmetic.rs)
static bool compareSurpassesLexicographic(
    int32_t sign, int32_t year, const String& monthCode, int32_t day,
    int32_t targetYear, const String& targetMonthCode, int32_t targetDay)
{
    if (year != targetYear)
        return sign * (static_cast<int64_t>(year) - targetYear) > 0;
    if (monthCode != targetMonthCode) {
        auto ordering = codePointCompare(monthCode, targetMonthCode);
        return sign > 0 ? ordering > 0 : ordering < 0;
    }
    if (day != targetDay)
        return sign * (static_cast<int64_t>(day) - targetDay) > 0;
    return false;
}

// compareSurpassesOrdinally — icu4x: compare_surpasses_ordinal (components/calendar/src/calendar_arithmetic.rs)
static bool compareSurpassesOrdinally(
    int32_t sign, int32_t year, int32_t ordinalMonth, int32_t day,
    int32_t targetYear, int32_t targetOrdinalMonth, int32_t targetDay)
{
    if (year != targetYear)
        return sign * (static_cast<int64_t>(year) - targetYear) > 0;
    if (ordinalMonth != targetOrdinalMonth)
        return sign * (static_cast<int64_t>(ordinalMonth) - targetOrdinalMonth) > 0;
    if (day != targetDay)
        return sign * (static_cast<int64_t>(day) - targetDay) > 0;
    return false;
}

// resolveMonthCodeToOrdinal — internal: resolves a monthCode to its 1-based ordinal position in the given year
static int32_t resolveMonthCodeToOrdinal(CalendarID calendarId, const String& monthCode, int32_t year)
{
    return withCalendar(calendarId, [&](UCalendar* cal) -> int32_t {
        if (!cal)
            return 1;
        UErrorCode status = U_ZERO_ERROR;
        ucal_set(cal, UCAL_EXTENDED_YEAR, year);
        ucal_set(cal, UCAL_MONTH, 0);
        ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
        ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
        ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return 1;

        int32_t savedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return 1;
        int32_t lastOrdinal = 1;
        for (int i = 0; i < 14; i++) {
            auto curCode = getMonthCode(cal, calendarId);
            if (!curCode) [[unlikely]]
                return lastOrdinal;
            if (*curCode == monthCode)
                return i + 1;
            if (codePointCompare(*curCode, monthCode) > 0) {
                // Hebrew M05L (Adar I) constrains FORWARD to M06 (Adar) in non-leap years.
                // icu4x components/calendar/src/cal/hebrew.rs ordinal_from_month: M05L -> ordinal 6 with Overflow::Constrain.
                // All other calendars constrain backward to the previous existing month.
                if (calendarId == hebrewCalendarID() && monthCode == "M05L"_s)
                    return i + 1;
                return lastOrdinal;
            }
            lastOrdinal = i + 1;
            ucal_add(cal, UCAL_MONTH, 1, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return lastOrdinal;
            int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return lastOrdinal;
            if (curYear != savedYear)
                return lastOrdinal;
        }
        return lastOrdinal;
    });
}

// nonISODateSurpasses — icu4x: surpasses() (components/calendar/src/calendar_arithmetic.rs) — two-phase lexicographic + ordinal check
static bool nonISODateSurpasses(
    CalendarID calendarId,
    int32_t sign,
    int32_t sourceYear,
    const String& sourceMonthCode,
    int32_t sourceDay,
    int32_t candidateYears,
    int32_t targetYear,
    const String& targetMonthCode,
    int32_t targetOrdinalMonth,
    int32_t targetDay)
{
    int32_t y0 = sourceYear + candidateYears; // Phase 1: lexicographic check (year, monthCode, day).
    if (compareSurpassesLexicographic(sign, y0, sourceMonthCode, sourceDay, targetYear, targetMonthCode, targetDay))
        return true; // Phase 2: constrain source monthCode to year y0, compare ordinally.
    int32_t m0 = resolveMonthCodeToOrdinal(calendarId, sourceMonthCode, y0);
    return compareSurpassesOrdinally(sign, y0, m0, sourceDay, targetYear, targetOrdinalMonth, targetDay);
}

// calendarDateAdd — temporal_rs: Calendar::date_add (src/builtins/core/calendar.rs)
//   temporal_rs delegates to icu4x: AnyCalendar::add -> ArithmeticDate::added (components/calendar/src/calendar_arithmetic.rs)
//   ICU4C has no equivalent: we use ucal_add(UCAL_EXTENDED_YEAR/UCAL_MONTH) with month-code re-resolution for lunisolar.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
// CalendarDateAdd steps:
//   1. If iso8601 -> isoDateAdd (BalanceISOYearMonth + RegulateISODate + AddDaysToISODate). (our steps 1–2)
//   2. (else) NonISODateAdd — implementation-defined. (our steps 3–9)
//   3. If ISODateWithinLimits(result) is false, throw RangeError. (checked via isoDateFromCalendarChecked)
//   4. Return result.
TemporalResult<ISO8601::PlainDate> calendarDateAdd(CalendarID calendarId, const ISO8601::PlainDate& isoDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    // 1. If calendarId is not a lunisolar calendar, use ISO proleptic-Gregorian arithmetic.
    // NOTE: Non-lunisolar calendars share proleptic Gregorian arithmetic; bypass ICU (gregory uses Julian pre-1582).
    if (!calendarIsLunisolar(calendarId))
        return isoDateAdd(isoDate, duration, overflow);
    // 2. If there are no year or month components, day/week addition is calendar-independent; use isoDateAdd.
    // NOTE: ICU's Chinese/Dangi approximation gives wrong results for far-future dates (year > ~2100).
    if (!duration.years() && !duration.months())
        return isoDateAdd(isoDate, duration, overflow);
    // 3. Let totalDays be duration.[[Days]] + 7 × duration.[[Weeks]].
    // NOTE: ucal_add takes int32_t; any component outside int32_t exceeds Temporal's representable range.
    auto fitsInt32 = [](int64_t v) -> bool {
        return v >= INT32_MIN && v <= INT32_MAX;
    };
    int64_t totalDays64 = duration.days() + 7LL * duration.weeks();
    if (!fitsInt32(duration.years()) || !fitsInt32(duration.months()) || !fitsInt32(totalDays64)) [[unlikely]]
        return makeUnexpected(rangeError("duration is out of the representable range"_s));

    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::PlainDate> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));

        // 4. Open ICU calendar for calendarId. (done by withCalendar above)
        // 5. Set ICU calendar to isoDate.
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        // 6. Let origMonthCode and origDay be the source month code and day (preserved across year addition per icu4x).
        UErrorCode status = U_ZERO_ERROR;
        auto origMonthCodeOpt = getMonthCode(cal, calendarId);
        if (!origMonthCodeOpt) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        String origMonthCode = WTF::move(*origMonthCodeOpt);
        int32_t originalDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        // 7. If duration.[[Years]] ≠ 0, add years; for lunisolar, re-resolve the original month code in the new year.
        if (duration.years()) {
            ucal_add(cal, UCAL_EXTENDED_YEAR, clampTo<int32_t>(duration.years()), &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            if (calendarIsLunisolar(calendarId)) {
                auto foundState = setCalendarToMonthCode(cal, calendarId, origMonthCode);
                if (!foundState) [[unlikely]]
                    return makeUnexpected(rangeError("Failed to resolve month code after year addition"_s));
                //    a. If overflow is ~reject~ and month code doesn't exist in new year, throw RangeError.
                if (!foundState.value() && overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("month code does not exist in the target year (overflow: reject)"_s));
            }
        }

        // 8. If duration.[[Months]] ≠ 0, add months.
        if (duration.months()) {
            ucal_add(cal, UCAL_MONTH, clampTo<int32_t>(duration.months()), &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        }
        // 9. Clamp or reject the day to the new month's maximum.
        // NOTE: ucal_add already clamps for constrain. For reject, check if day was reduced.
        // Do NOT use ucal_set(DAY_OF_MONTH) after ucal_add — causes ICU state corruption on lunisolar calendars.
        int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        if (overflow == TemporalOverflow::Reject && originalDay > maxDay) [[unlikely]]
            return makeUnexpected(rangeError("day is out of range for the resulting month (overflow: reject)"_s));
        int32_t curDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));

        if (curDay != originalDay && originalDay <= maxDay) {
            ucal_add(cal, UCAL_DAY_OF_MONTH, originalDay - curDay, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        } else if (originalDay > maxDay) {
            int32_t adj = maxDay - curDay;
            if (adj) {
                ucal_add(cal, UCAL_DAY_OF_MONTH, adj, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            }
        }

        // 10. Add totalDays.
        // 11. If result is outside representable range, throw RangeError.
        int32_t totalDays = static_cast<int32_t>(totalDays64);
        if (totalDays) {
            ucal_add(cal, UCAL_DAY_OF_MONTH, totalDays, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        }

        // 12. Return result.
        auto result = isoDateFromCalendarChecked(cal);
        if (!result) [[unlikely]]
            return makeUnexpected(rangeError("Result of calendar date addition is outside representable range"_s));
        return *result;
    });
}

// surpassesMonths — icu4x: SurpassesChecker::surpasses_months (components/calendar/src/calendar_arithmetic.rs)
// Returns nullopt on ICU failure
static std::optional<bool> surpassesMonths(
    UCalendar* trialCal,
    CalendarID calendarId,
    int32_t sign,
    int32_t sourceDay,
    int32_t targetYear,
    const String& targetMonthCode,
    int32_t targetOrdinalMonth,
    int32_t targetDay)
{
    UErrorCode status = U_ZERO_ERROR;
    ucal_add(trialCal, UCAL_MONTH, sign, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t trialYear = ucal_get(trialCal, UCAL_EXTENDED_YEAR, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    auto trialMonthCode = getMonthCode(trialCal, calendarId);
    if (!trialMonthCode) [[unlikely]]
        return std::nullopt;

    // Phase 1: lexicographic check (year, monthCode, day).
    // Equivalent to nonISODateSurpasses with candidateYears=0, but inlined so we don't
    // need resolveMonthCodeToOrdinal — that would re-enter withCalendar and deadlock with
    // the cache lock the caller holds across the month iteration.
    if (compareSurpassesLexicographic(sign, trialYear, *trialMonthCode, sourceDay, targetYear, targetMonthCode, targetDay))
        return true;

    // Phase 2: constrain source monthCode to year y0, compare ordinally.
    // trialCal is at the trial position, so its ordinal month equals resolveMonthCodeToOrdinal(calendarId, trialMonthCode, trialYear).
    // Read it directly. computeOrdinalMonth mutates trialCal (walks from year-start for lunisolar). save+restore epoch ms preserves
    // the trial position for the next loop iteration.
    double savedMs = ucal_getMillis(trialCal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    auto trialOrdinal = computeOrdinalMonth(trialCal, calendarId);
    UErrorCode restoreStatus = U_ZERO_ERROR;
    ucal_setMillis(trialCal, savedMs, &restoreStatus);
    if (!trialOrdinal || U_FAILURE(restoreStatus)) [[unlikely]]
        return std::nullopt;
    return compareSurpassesOrdinally(sign, trialYear, *trialOrdinal, sourceDay, targetYear, targetOrdinalMonth, targetDay);
}

// setMonths — icu4x: SurpassesChecker::set_months (components/calendar/src/calendar_arithmetic.rs)
static std::optional<bool> setMonths(UCalendar* cal, int32_t sourceDay)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t regulatedDay = std::min(sourceDay, maxDay);
    int32_t currentDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    if (currentDay != regulatedDay) {
        ucal_add(cal, UCAL_DAY_OF_MONTH, regulatedDay - currentDay, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
    }
    return true;
}

// calendarDateUntil — temporal_rs: Calendar::date_until (src/builtins/core/calendar.rs)
//   temporal_rs delegates to icu4x: AnyCalendar::until -> ArithmeticDate::until + SurpassesChecker (components/calendar/src/calendar_arithmetic.rs)
//   ICU4C has no equivalent: we use epoch-ms comparison + iterative ucal_add(UCAL_MONTH) walking.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil
// CalendarDateUntil steps:
//   1. Let sign be CompareISODate(one, two). (our step 4)
//   2. If sign = 0, return ZeroDateDuration(). (our step 4)
//   3. If iso8601 -> diffISODate (ISODateSurpasses algorithm). (our steps 1–2)
//   4. Return NonISODateUntil — implementation-defined. (our steps 3–8)
TemporalResult<ISO8601::Duration> calendarDateUntil(CalendarID calendarId, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    // CalendarDateUntil takes "largestUnit: a date unit" — spec guarantees Year/Month/Week/Day only.
    ASSERT(largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month || largestUnit == TemporalUnit::Week || largestUnit == TemporalUnit::Day);

    // 1. If calendarId is not lunisolar, use ISO proleptic-Gregorian arithmetic (route through diffISODate).
    // NOTE: ICU's 'gregory' uses Julian before 1582, causing field mismatches; always route through ISO.
    if (!calendarIsLunisolar(calendarId))
        return diffISODate(one, two, largestUnit);
    // 2. If largestUnit is ~day~ or ~week~, use pure ISO day count (calendar-independent).
    // NOTE: ICU Chinese/Dangi epoch ms is approximate for dates beyond ~year 2100; pure ISO is exact.
    if (largestUnit == TemporalUnit::Day || largestUnit == TemporalUnit::Week)
        return diffISODate(one, two, largestUnit);

    // 3. Read target (two) fields — separate withCalendar call for minimal lock scope.
    struct TargetFields {
        double epochMs { 0 };
        int32_t year { 0 };
        String monthCode;
        int32_t day { 0 };
        int32_t ordinalMonth { 0 };
    };
    auto targetOrError = withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<TargetFields> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, two)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        TargetFields t;
        t.epochMs = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        t.year = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        auto monthCodeOpt = getMonthCode(cal, calendarId);
        if (!monthCodeOpt) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        t.monthCode = WTF::move(*monthCodeOpt);
        t.day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        auto ordinalMonthOpt = computeOrdinalMonth(cal, calendarId);
        if (!ordinalMonthOpt) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        t.ordinalMonth = *ordinalMonthOpt;
        return t;
    });
    if (!targetOrError)
        return makeUnexpected(targetOrError.error());
    auto& target = *targetOrError;

    // 4. Read source (one) fields — separate withCalendar call for minimal lock scope.
    struct SourceFields {
        double epochMs { 0 };
        int32_t year { 0 };
        String monthCode;
        int32_t day { 0 };
    };
    auto sourceOrError = withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<SourceFields> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, one)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        SourceFields s;
        s.epochMs = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        s.year = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        auto monthCodeOpt = getMonthCode(cal, calendarId);
        if (!monthCodeOpt) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        s.monthCode = WTF::move(*monthCodeOpt);
        s.day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        return s;
    });
    if (!sourceOrError)
        return makeUnexpected(sourceOrError.error());
    auto& source = *sourceOrError;

    // 5. Let sign be +1 if one < two, -1 if one > two, 0 if equal. Return zero duration if sign = 0.
    //    (Source/target calendar fields already read above.)
    int32_t sign;
    if (source.epochMs < target.epochMs)
        sign = 1;
    else if (source.epochMs > target.epochMs)
        sign = -1;
    else
        return ISO8601::Duration { };

    // NOTE: min_years fast-forward: pre-guess year delta that doesn't surpass (icu4x optimization).
    int32_t yearDiff = target.year - source.year;
    int32_t minYears = !yearDiff ? 0 : yearDiff - sign;

    // 6. largestUnit is ~year~ or ~month~: count years (if any) using nonISODateSurpasses,
    //    then run the month loop on the shared cached calendar.
    ASSERT(largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month);

    int32_t years = 0;
    int32_t months = 0;

    //    a. If largestUnit is ~year~: count full years using nonISODateSurpasses fast-forward.
    if (largestUnit == TemporalUnit::Year) {
        int64_t candidateYears = minYears ? minYears : sign;
        auto yearDoesNotSurpass = [&] {
            return !nonISODateSurpasses(calendarId, sign, source.year, source.monthCode, source.day, static_cast<int32_t>(candidateYears), target.year, target.monthCode, target.ordinalMonth, target.day);
        };
        while (yearDoesNotSurpass()) {
            years = static_cast<int32_t>(candidateYears);
            candidateYears += sign;
        }
    }

    //    b. Compute the month-loop start: (one + years) via calendarDateAdd (preserves month code).
    ISO8601::PlainDate monthLoopStart = one;
    if (years) {
        ISO8601::Duration yearDur;
        yearDur.setYears(static_cast<int64_t>(years));
        auto advanced = calendarDateAdd(calendarId, one, yearDur, TemporalOverflow::Constrain);
        if (!advanced) [[unlikely]]
            return makeUnexpected(advanced.error());
        monthLoopStart = *advanced;
    }

    //    c. Month iteration on the cached calendar.
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::Duration> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, monthLoopStart)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        UErrorCode status = U_ZERO_ERROR;
        // NOTE: lunisolar months per year vary; iterate one at a time (no min_months fast-forward).
        int32_t candidateMonths = sign;
        //    d. Set cal to (one + years) with day=1 for clamping-free month advancement.
        double startMs = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
        ucal_getMillis(cal, &status); // force ICU state resolution
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));

        for (;;) {
            auto surpasses = surpassesMonths(cal, calendarId, sign, source.day, target.year, target.monthCode, target.ordinalMonth, target.day);
            if (!surpasses) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            if (*surpasses)
                break;
            months = candidateMonths;
            candidateMonths += sign;
            // cal already advanced by surpassesMonths — no reset needed here.
            // ucal_setMillis(cal, startMs) below resets it before applying the final months count.
        }

        // Restore cal to (one + years), apply total months in one ucal_add (avoids undo asymmetry).
        ucal_setMillis(cal, startMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        if (months) {
            ucal_add(cal, UCAL_MONTH, months, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        }
        //    e. Apply regulated day = min(sourceDay, end_of_month) via setMonths.
        if (!setMonths(cal, source.day)) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));

        // 7. Compute remaining days from the epoch ms difference between cal and two.
        const double msPerDay = 86'400'000.0; // nsPerDay / nsPerMillisecond
        // 8. Return Duration { years, months, weeks, days }.
        double finalMs1 = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        double daysDiff = std::trunc((target.epochMs - finalMs1) / msPerDay);

        //    f. If largestUnit is ~month~, clear years (months were counted from one + years=0).
        int32_t resultYears = (largestUnit == TemporalUnit::Month) ? 0 : years;

        return ISO8601::Duration {
            resultYears,
            months,
            0,
            static_cast<int64_t>(daysDiff),
            0, 0, 0, 0, Int128(0), Int128(0)
        };
    });
}


// ecmaReferenceYear — No spec AO and no ICU4C equivalent.
// Ported from icu4x: ecma_reference_year (components/calendar/src/cal/east_asian_traditional.rs, hijri.rs, hebrew.rs, coptic.rs).
// Returns the extended calendar year whose ISO date falls nearest 1972 for (monthNumber, isLeapMonth, day).
int32_t ecmaReferenceYear(CalendarID calendarId, uint8_t monthNumber, bool isLeapMonth, uint8_t day)
{
    bool bigDay = day > 29;

    if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID()) {
        // Ported from icu4x components/calendar/src/cal/east_asian_traditional.rs (Chinese/Dangi tables).
        // Generated by icu4x's generate_reference_years tool.
        if (!isLeapMonth) {
            switch (monthNumber) {
            case 1:
                return bigDay ? 1970 : 1972;
            case 2:
                return 1972;
            case 3:
                return bigDay ? (calendarId == dangiCalendarID() ? 1968 : 1966) : 1972;
            case 4:
                return bigDay ? 1970 : 1972;
            case 5:
                return 1972;
            case 6:
                return bigDay ? 1971 : 1972;
            case 7:
                return 1972;
            case 8:
                return bigDay ? 1971 : 1972;
            case 9:
                return 1972;
            case 10:
                return 1972;
            case 11:
                // icu4x: (11,false,true)=>1969, (11,false,false) if day>26=>1971, else=>1972
                // bigDay (day>29) must be checked BEFORE day>26 since both can be true.
                if (bigDay)
                    return 1969;
                return (day > 26) ? 1971 : 1972;
            case 12:
                return 1971;
            default:
                return 1972;
            }
        }
        // Leap months — icu4x components/calendar/src/cal/east_asian_traditional.rs:ecma_reference_year_common
        // Entries matching UseRegularIfConstrain return ecmaRefYearUseRegular:
        // caller uses non-leap month reference year for Constrain, throws for Reject.
        switch (monthNumber) {
        case 1:
            return ecmaRefYearUseRegular; // icu4x: (1, true, _) => UseRegularIfConstrain
        case 2:
            return bigDay ? ecmaRefYearUseRegular : 1947;
        case 3:
            return bigDay ? 1955 : 1966;
        case 4:
            return bigDay ? 1944 : 1963;
        case 5:
            return bigDay ? 1952 : 1971;
        case 6:
            return bigDay ? 1941 : 1960;
        case 7:
            return bigDay ? 1938 : 1968;
        case 8:
            return bigDay ? ecmaRefYearUseRegular : 1957;
        case 9:
            return bigDay ? ecmaRefYearUseRegular : 2014;
        case 10:
            return bigDay ? ecmaRefYearUseRegular : 1984;
        case 11:
            return bigDay ? ecmaRefYearUseRegular : 2033;
        case 12:
            return ecmaRefYearUseRegular; // icu4x: (12, true, _) => UseRegularIfConstrain
        default:
            return 1972;
        }
    }

    if (calendarIsIslamic(calendarId)) {
        // icu4x components/calendar/src/cal/hijri.rs: Islamic calendars have no leap months.
        if (isLeapMonth)
            return ecmaRefYearNotInCalendar;
        // icu4x components/calendar/src/cal/hijri.rs: Islamic-civil and Islamic-tbla (tabular) use a simpler table
        // than UmmAlQura. All months 1-10 use year 1392 for tabular variants.
        // Month 11 threshold differs: civil (Friday epoch) = day < 26, tbla (Thu) = day < 27.
        bool isCivil = (calendarId == islamicCivilCalendarID());
        bool isTbla = (calendarId == islamicTblaCalendarID());
        if (isCivil || isTbla) {
            // icu4x: TabularAlgorithm::ecma_reference_year (components/calendar/src/cal/hijri.rs)
            if (monthNumber <= 10)
                return 1392;
            if (monthNumber == 11)
                return (day < (isCivil ? 26 : 27)) ? 1392 : 1391;
            if (monthNumber == 12)
                return bigDay ? 1390 : 1391;
            return 1392;
        }

        // UmmAlQura table — icu4x: UmmAlQura::ecma_reference_year (components/calendar/src/cal/hijri.rs)
        switch (monthNumber) {
        case 1:
            return 1392;
        case 2:
            return bigDay ? 1390 : 1392;
        case 3:
            return bigDay ? 1391 : 1392;
        case 4:
            return 1392;
        case 5:
            return bigDay ? 1391 : 1392;
        case 6:
            return 1392;
        case 7:
            return bigDay ? 1389 : 1392;
        case 8:
            return 1392;
        case 9:
            return 1392;
        case 10:
            return bigDay ? 1390 : 1392;
        case 11:
            return (day > 25) ? 1391 : 1392;
        case 12:
            return bigDay ? 1390 : 1391;
        default:
            return 1392;
        }
    }

    if (calendarId == hebrewCalendarID()) {
        // Hebrew: ported from icu4x components/calendar/src/cal/hebrew.rs reference_year_from_month_day.
        // Dec 31, 1972 = Hebrew 4th month (Tevet), day 26, year 5733 AM.
        // Returns Hebrew UCAL_EXTENDED_YEAR (anno mundi year).
        if (isLeapMonth) {
            // icu4x components/calendar/src/cal/hebrew.rs: only M05L (Adar I) is valid; all other leap months don't exist.
            if (monthNumber == 5)
                return 5730;
            return ecmaRefYearNotInCalendar; // M01L, M02L, etc. are invalid in Hebrew
        }
        switch (monthNumber) {
        case 1:
            return 5733; // Tishri: all days fit
        case 2:
            return day <= 29 ? 5733 : 5732; // Cheshvan: 5733 has 29 days
        case 3:
            return day <= 29 ? 5733 : 5732; // Kislev: 5733 has 29 days
        case 4:
            return day <= 26 ? 5733 : 5732; // Tevet: Dec 31 = 4/26/5733
        default:
            return 5732; // M05-M12
        }
    }

    if (calendarId == copticCalendarID() || calendarId == ethiopicCalendarID()) {
        // Coptic/Ethiopian: ported from icu4x components/calendar/src/cal/coptic.rs reference_year_from_month_day.
        // Dec 31, 1972 = Coptic 4th month (Koiak), day 22, year 1689 AM.
        // Returns Coptic/Ethiopian UCAL_EXTENDED_YEAR.
        // icu4x components/calendar/src/cal/coptic.rs: Coptic AM reference years (Dec 31, 1972 = Coptic 1689 M04 day22).
        // Ethiopic (Amete Mihret): Ethiopic extended year = Coptic AM year + 276.
        // (Coptic epoch 284 CE, Ethiopic epoch 8 CE: difference = 276 years.)
        int32_t copticYear;
        if (monthNumber < 4 || (monthNumber == 4 && day <= 22))
            copticYear = 1689;
        else if (monthNumber == 13 && day >= 6)
            copticYear = 1687; // leap year
        else
            copticYear = 1688;
        return (calendarId == ethiopicCalendarID()) ? copticYear + 276 : copticYear;
    }

    if (calendarId == ethioaaCalendarID()) {
        // Ethioaa (Amete Alem): same month structure as Ethiopic/Coptic, but different year offset.
        // ISO 1972-12-31 = Ethioaa 7465, M04, day 22. Offset from Coptic: Ethioaa = Coptic + 5776.
        // M13 leap year nearest to Dec 31, 1972: year 7463 (ISO 1971).
        if (isLeapMonth)
            return 7464;
        if (monthNumber < 4 || (monthNumber == 4 && day <= 22))
            return 7465;
        if (monthNumber == 13 && day >= 6)
            return 7463; // leap year
        return 7464;
    }

    if (calendarId == persianCalendarID()) {
        // Persian (Jalali): ported from icu4x components/calendar/src/cal/persian.rs reference_year_from_month_day.
        // Dec 31, 1972 = 10th month (Dey), day 10, year 1351 AP.
        if (monthNumber < 10 || (monthNumber == 10 && day <= 10))
            return 1351;
        return 1350; // 1350 AP is a leap year
    }

    if (calendarId == indianCalendarID()) {
        // Indian (Saka): ported from icu4x components/calendar/src/cal/indian.rs reference_year_from_month_day.
        // Dec 31, 1972 = 10th month, day 10, year 1894 Shaka.
        if (monthNumber < 10 || (monthNumber == 10 && day <= 10))
            return 1894;
        return 1893;
    }

    if (calendarId == buddhistCalendarID()) {
        // Buddhist: ICU4C UCAL_EXTENDED_YEAR for Buddhist = ISO/Gregorian year (not Buddhist era year).
        // So ISO year 1972 is the reference — same as gregory/japanese.
        return 1972;
    }

    if (calendarId == rocCalendarID()) {
        // ROC: handled via era+eraYear in calendarDateFromFields (year 61 = ROC era 1, year 61).
        // ROC year 61 = ISO year 1972.
        return 61;
    }

    // Japanese/Gregory/ISO and indic numeral-only calendars:
    // UCAL_EXTENDED_YEAR for these = Gregorian/ISO year.
    return 1972;
}

// calendarDateFromFields — temporal_rs: Calendar::date_from_fields (src/builtins/core/calendar.rs)
//   temporal_rs delegates to icu4x: ArithmeticDate::from_fields (components/calendar/src/calendar_arithmetic.rs)
//   ICU4C has no equivalent: we implement the same field-resolution logic using ucal_set + ucal_get.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardatefromfields
// CalendarDateFromFields steps 1-4 (CalendarResolveFields is done by the JS-layer caller):
//   1. (Resolved by caller.)
//   2. Let result be ? CalendarDateToISO(calendar, fields, overflow).
//      -> iso8601: handled by the JS layer before reaching here.
//      -> non-ISO: implementation-defined (NonISOCalendarDateToISO); no concrete spec steps.
//   3. If ISODateWithinLimits(result) is false, throw a RangeError.  (checked via isoDateFromCalendarChecked)
//   4. Return result.
TemporalResult<ISO8601::PlainDate> calendarDateFromFields(CalendarID calendarId, std::optional<int32_t> year, uint8_t month, uint8_t day, std::optional<StringView> era, std::optional<int32_t> eraYear, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow)
{
    bool tookEraPath = false;
    if (era && eraYear) {
        tookEraPath = true;
        // Japanese "ce"/"bce": eraYear IS the Gregorian year. Use ISO date path.
        if (calendarId == japaneseCalendarID() && (*era == "ce"_s || *era == "bce"_s)) {
            // For Japanese "ce"/"bce" eras, the year IS the ISO year — bypass ICU to avoid
            // Julian/Gregorian calendar switch issues for pre-1582 dates. Apply overflow.
            CheckedInt32 checkedISOYear = *eraYear;
            if (*era == "bce"_s)
                checkedISOYear = 1 - checkedISOYear;
            if (checkedISOYear.hasOverflowed() || !ISO8601::isYearWithinLimits(checkedISOYear.value())) [[unlikely]]
                return makeUnexpected(rangeError("Resolved calendar date is outside representable range"_s));
            int32_t isoYear = checkedISOYear.value();
            // year.has_value() means user-provided; check for consistency (NonISOResolveFields step).
            if (year && *year != isoYear) [[unlikely]]
                return makeUnexpected(rangeError("year is inconsistent with era and eraYear"_s));
            uint8_t resolvedMonth = month;
            if (month > 12) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("month is out of range for this calendar"_s));
                resolvedMonth = 12;
            }
            uint8_t resolvedDay = day;
            uint8_t daysInMo = ISO8601::daysInMonth(isoYear, resolvedMonth);
            if (day > daysInMo) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("Day is out of range for the given month"_s));
                resolvedDay = daysInMo;
            }
            return ISO8601::PlainDate(isoYear, resolvedMonth, resolvedDay);
        }
    }

    std::optional<int32_t> icuEraCode;
    if (tookEraPath && era) {
        icuEraCode = mapTemporalEraToICUEra(calendarId, *era);
        if (!icuEraCode) [[unlikely]]
            return makeUnexpected(rangeError("era is not valid for this calendar"_s));
    }

    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::PlainDate> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));

        UErrorCode status = U_ZERO_ERROR;

        if (tookEraPath) {
            // era && eraYear, not Japanese "ce"/"bce".
            if (!icuEraCode) [[unlikely]]
                return makeUnexpected(rangeError("era is not valid for this calendar"_s));
            ucal_set(cal, UCAL_ERA, *icuEraCode);
            ucal_set(cal, UCAL_YEAR, *eraYear);
        } else if (calendarId == rocCalendarID()) {
            // ROC: convert temporal year to era+eraYear for ICU.
            int32_t y = year.value_or(0);
            if (y <= 0) {
                ucal_set(cal, UCAL_ERA, 0); // broc
                ucal_set(cal, UCAL_YEAR, 1 - y);
            } else {
                ucal_set(cal, UCAL_ERA, 1); // roc
                ucal_set(cal, UCAL_YEAR, y);
            }
        } else
            ucal_set(cal, UCAL_EXTENDED_YEAR, year.value_or(0));

        if (monthCode) {
            // Month codes > M13 are always invalid. M13 is only valid for Coptic/Ethiopian.
            if (monthCode->monthNumber > 13) [[unlikely]]
                return makeUnexpected(rangeError("month is out of range"_s));
            if (monthCode->monthNumber == 13 && calendarId != copticCalendarID() && calendarId != ethiopicCalendarID() && calendarId != ethioaaCalendarID()) [[unlikely]]
                return makeUnexpected(rangeError("month is out of range"_s));

            if (calendarIsLunisolar(calendarId)) {
                // Lunisolar: walk months from start of year to find target monthCode.
                // ICU4X uses precomputed year.packed.leap_month() for O(1) lookup; ICU4C
                // doesn't expose this data, so we walk. The walk is correct and safe.
                ucal_set(cal, UCAL_MONTH, 0);
                ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
                ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
                ucal_getMillis(cal, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));

                String targetCode = makeString("M"_s, monthCode->monthNumber < 10 ? "0"_s : ""_s,
                    monthCode->monthNumber, monthCode->isLeapMonth ? "L"_s : ""_s);
                int32_t savedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                bool found = false;
                for (int i = 0; i < 14; i++) {
                    auto curCode = getMonthCode(cal, calendarId);
                    if (!curCode) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    if (*curCode == targetCode) {
                        found = true;
                        break;
                    }
                    // For constrain: if we've passed the target code, stop at previous.
                    if (codePointCompare(*curCode, targetCode) > 0) {
                        // Leap month doesn't exist — constrain to previous month.
                        if (overflow == TemporalOverflow::Constrain && monthCode->isLeapMonth) {
                            if (calendarId == hebrewCalendarID()) {
                                // M05L -> M06 (Adar), which is this current month.
                                found = true;
                            } else {
                                // Chinese/Dangi: M01L->M01, revert one step.
                                ucal_add(cal, UCAL_MONTH, -1, &status);
                                if (U_FAILURE(status)) [[unlikely]]
                                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                                found = true;
                            }
                        }
                        break;
                    }
                    ucal_add(cal, UCAL_MONTH, 1, &status);
                    if (U_FAILURE(status)) [[unlikely]]
                        return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                    int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
                    if (U_FAILURE(status)) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    if (curYear != savedYear) {
                        ucal_add(cal, UCAL_MONTH, -1, &status);
                        if (U_FAILURE(status)) [[unlikely]]
                            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                        break;
                    }
                }
                if (!found && overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("monthCode does not exist in this calendar year"_s)); // Clamp day via ucal_add.
                int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                uint8_t clampedDay = day;
                if (day > maxDay) {
                    if (overflow == TemporalOverflow::Reject) [[unlikely]]
                        return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
                    clampedDay = static_cast<uint8_t>(maxDay);
                }
                if (clampedDay > 1) {
                    ucal_add(cal, UCAL_DAY_OF_MONTH, clampedDay - 1, &status);
                    if (U_FAILURE(status)) [[unlikely]]
                        return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                }
            } else if (calendarId == hebrewCalendarID()) [[unlikely]] {
                // Hebrew non-lunisolar path — shouldn't reach here since Hebrew IS lunisolar.
                ASSERT_NOT_REACHED();
                return makeUnexpected(rangeError("unexpected hebrew non-lunisolar path"_s));
            } else {
                // Non-lunisolar with monthCode (Gregorian-based calendars).
                // Set month with day=1 first to avoid ICU normalizing out-of-range day.
                ucal_set(cal, UCAL_MONTH, monthCode->monthNumber - 1);
                if (monthCode->isLeapMonth)
                    ucal_set(cal, UCAL_IS_LEAP_MONTH, 1);
                ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
                ucal_getMillis(cal, &status); // force resolution of year+month
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));

                // Now check maxDay for the resolved month.
                int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                if (day > maxDay) {
                    if (overflow == TemporalOverflow::Reject) [[unlikely]]
                        return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
                    ucal_set(cal, UCAL_DAY_OF_MONTH, maxDay);
                } else if (day > 1)
                    ucal_set(cal, UCAL_DAY_OF_MONTH, day);
            }
        } else if (calendarIsLunisolar(calendarId)) {
            // For lunisolar calendars, 'month' is the ordinal month (1-indexed).
            // Count monthsInYear using a separate calendar to avoid state corruption.
            ucal_set(cal, UCAL_MONTH, 0);
            ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
            ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
            ucal_getMillis(cal, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError("Failed to resolve lunisolar calendar"_s));
            // Count months in year by walking cal forward, then reset to year start.
            double calMs = ucal_getMillis(cal, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            int32_t savedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            int32_t monthsInYear = 1;
            for (int i = 0; i < 14; i++) {
                ucal_add(cal, UCAL_MONTH, 1, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                if (curYear != savedYear)
                    break;
                monthsInYear++;
            }
            // Reset to year start before advancing to target month.
            ucal_setMillis(cal, calMs, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuSetCalendarFailed));

            // Clamp or reject month against monthsInYear.
            uint8_t resolvedMonth = month;
            if (month > monthsInYear) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("month is out of range for this calendar year"_s));
                resolvedMonth = static_cast<uint8_t>(monthsInYear);
            }

            // Advance the original calendar to the target month.
            if (resolvedMonth > 1) {
                ucal_add(cal, UCAL_MONTH, resolvedMonth - 1, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError("Failed to resolve lunisolar month"_s));
            }

            // Clamp day to daysInMonth if constrain.
            int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            uint8_t resolvedDay = day;
            if (day > maxDay) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
                resolvedDay = static_cast<uint8_t>(maxDay);
            }
            // Use ucal_add (not ucal_set) to advance within the month — avoids
            // ICU's lazy-field resolution resetting the month position.
            if (resolvedDay > 1) {
                ucal_add(cal, UCAL_DAY_OF_MONTH, resolvedDay - 1, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            }
        } else {
            // Non-lunisolar: clamp month to calendar max.
            int32_t maxMonth = ucal_getLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM, &status) + 1;
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            uint8_t resolvedMonth = month;
            if (month > maxMonth) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("month is out of range for this calendar"_s));
                resolvedMonth = static_cast<uint8_t>(maxMonth);
            }
            ucal_set(cal, UCAL_MONTH, resolvedMonth - 1); // Resolve to get actual max day for this month.
            ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
            int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            uint8_t resolvedDay = day;
            if (day > maxDay) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
                resolvedDay = static_cast<uint8_t>(maxDay);
            }
            ucal_set(cal, UCAL_DAY_OF_MONTH, resolvedDay);
        }

        if (overflow == TemporalOverflow::Reject) {
            int32_t resolvedDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError("Failed to resolve calendar date"_s));
            if (monthCode) {
                if (calendarIsLunisolar(calendarId)) {
                    // For lunisolar calendars, ICU slot numbers != monthCode numbers (e.g. Hebrew
                    // M05L at slot 5 gives UCAL_MONTH+1=6, and ICU4C never sets IS_LEAP_MONTH).
                    // Use getMonthCode to verify the actual month — the walk already positioned
                    // the calendar at the target month, so we only need to verify the day.
                    if (resolvedDay != static_cast<int32_t>(day)) [[unlikely]]
                        return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
                } else {
                    int32_t resolvedMonth = ucal_get(cal, UCAL_MONTH, &status) + 1;
                    if (U_FAILURE(status)) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    int32_t resolvedLeap = ucal_get(cal, UCAL_IS_LEAP_MONTH, &status);
                    if (U_FAILURE(status)) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    bool leapMismatch = monthCode->isLeapMonth && !resolvedLeap;
                    if (resolvedDay != static_cast<int32_t>(day) || resolvedMonth != static_cast<int32_t>(monthCode->monthNumber) || leapMismatch) [[unlikely]]
                        return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
                }
            }
        }

        // For constrain with a leap monthCode that doesn't exist: ICU silently drops the leap
        // flag and lands on the non-leap version of the month, which is the correct constrain
        // behavior. No explicit fallback is needed.

        auto resolved = isoDateFromCalendarChecked(cal);
        if (!resolved) [[unlikely]]
            return makeUnexpected(rangeError("Resolved calendar date is outside representable range"_s));

        // NonISOResolveFields: when era+eraYear and year are all non-unset, they must identify
        // the same year. year == nullopt means the caller did not have a user-provided year.
        if (tookEraPath && year) {
            // Re-use cal (we hold the lock): set it to resolved and read back calendar year.
            if (!setCalendarToISODate(cal, *resolved)) [[unlikely]]
                return makeUnexpected(rangeError(icuSetCalendarFailed));
            UErrorCode yearStatus = U_ZERO_ERROR;
            int32_t resolvedYear;
            if (calendarId == rocCalendarID()) {
                int32_t era = ucal_get(cal, UCAL_ERA, &yearStatus);
                int32_t ey = ucal_get(cal, UCAL_YEAR, &yearStatus);
                resolvedYear = !era ? -(ey - 1) : ey;
            } else
                resolvedYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &yearStatus);
            if (!U_FAILURE(yearStatus) && resolvedYear != *year) [[unlikely]]
                return makeUnexpected(rangeError("year is inconsistent with era and eraYear"_s));
        }

        return *resolved;
    });
}

} // namespace TemporalCore
} // namespace JSC
