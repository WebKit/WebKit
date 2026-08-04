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
// NOTE: For Gregory/ISO: sets Gregorian change date to -infinity for proleptic Gregorian arithmetic.
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
    // calendars cannot be configured this way, so Gregorian-arithmetic accessors route through
    // the base Gregorian calendar below.
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

static bool calendarIsIslamic(CalendarID id)
{
    return id == islamicCivilCalendarID() || id == islamicTblaCalendarID() || id == islamicUmalquraCalendarID();
}

static bool calendarIsGregorianStructured(CalendarID id)
{
    return id == buddhistCalendarID() || id == rocCalendarID() || id == japaneseCalendarID();
}

static bool calendarUsesISODateArithmetic(CalendarID id)
{
    return calendarIsISO(id) || id == gregoryCalendarID() || calendarIsGregorianStructured(id);
}

// Japanese/ROC/Buddhist derive from Gregorian and reject ucal_setGregorianChange
// (U_UNSUPPORTED_ERROR), so Gregorian-arithmetic accessors route through gregory
// to get proleptic Gregorian. Calendar-native year and era fields are handled
// separately.
static CalendarID gregorianArithmeticCalendarFor(CalendarID calendarId)
{
    if (calendarIsGregorianStructured(calendarId))
        return gregoryCalendarID();
    return calendarId;
}

// https://tc39.es/proposal-intl-era-monthcode/#table-epoch-years
static constexpr int32_t rocCalendarYearOffset = 1911;
static constexpr int32_t buddhistCalendarYearOffset = -543;
static constexpr int32_t indianCalendarYearOffset = 78;

struct GregorianArithmeticYearFields {
    int32_t year;
    ASCIILiteral era;
    int32_t eraYear;
};

static GregorianArithmeticYearFields gregorianArithmeticYearFieldsFor(CalendarID calendarId, int32_t isoYear)
{
    ASSERT(calendarId == rocCalendarID() || calendarId == buddhistCalendarID());
    if (calendarId == buddhistCalendarID()) {
        int32_t year = isoYear - buddhistCalendarYearOffset;
        return { year, "be"_s, year };
    }

    int32_t year = isoYear - rocCalendarYearOffset;
    if (year > 0)
        return { year, "roc"_s, year };
    return { year, "broc"_s, 1 - year };
}

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
        if (!cal) [[unlikely]]
            return 1972; // fallback: assume ISO-proleptic
        UErrorCode status = U_ZERO_ERROR;
        ucal_setMillis(cal, iso1972Feb15EpochMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return 1972; // fallback: assume ISO-proleptic
        int32_t extYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return 1972; // fallback: assume ISO-proleptic
        return extYear;
    });
    cached.store(value, std::memory_order_relaxed);
    return value;
}

// isoDateToEpochMs — internal: converts ISO PlainDate to epoch ms at noon UTC
// (avoids DST boundary issues).
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

// isoDateFromCalendarChecked — internal: reads back ISO date from ICU calendar's current epoch ms; returns nullopt if outside the supported ISO year range
static std::optional<ISO8601::PlainDate> isoDateFromCalendarChecked(UCalendar* cal)
{
    UErrorCode status = U_ZERO_ERROR;
    double epochMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    WTF::Int64Milliseconds msWT(ISO8601::Duration::doubleToInt64Saturating(epochMs));
    int32_t days = WTF::msToDays(msWT);
    auto [year, month, day] = WTF::yearMonthDayFromDays(days);
    if (!ISO8601::isYearWithinLimits(year)) [[unlikely]]
        return std::nullopt;
    return ISO8601::PlainDate(year, static_cast<uint8_t>(month + 1), static_cast<uint8_t>(day));
}

template<typename F>
static auto withCalendarSetToDate(CalendarID calendarId, const ISO8601::PlainDate& isoDate, F&& body) -> decltype(body(static_cast<UCalendar*>(nullptr)))
{
    return withCalendar(calendarId, [&](UCalendar* cal) -> decltype(body(cal)) {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        return body(cal);
    });
}

static TemporalResult<int32_t> readICUField(UCalendar* cal, UCalendarDateFields field)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t result = ucal_get(cal, field, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    return result;
}

static TemporalResult<int32_t> readICUFieldLimit(UCalendar* cal, UCalendarDateFields field, UCalendarLimitType limitType)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t result = ucal_getLimit(cal, field, limitType, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    return result;
}

static bool resetCalendarToMonthStart(UCalendar* cal)
{
    UErrorCode status = U_ZERO_ERROR;
    ucal_set(cal, UCAL_MONTH, 0);
    ucal_set(cal, UCAL_IS_LEAP_MONTH, 0);
    ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
    ucal_getMillis(cal, &status);
    return U_SUCCESS(status);
}

// ICU4C-WORKAROUND: rdar://182952830 - after ucal_add(MONTH) with no other field read
// in between, ucal_getLimit(DAY_OF_MONTH, ACTUAL_MAXIMUM) can return the previous month's
// value. The getMillis+setMillis roundtrip (same value, re-written) forces re-resolution.
static bool forceICUFieldReresolution(UCalendar* cal)
{
    UErrorCode status = U_ZERO_ERROR;
    double epochMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return false;
    ucal_setMillis(cal, epochMs, &status);
    return U_SUCCESS(status);
}

static TemporalResult<void> validateRejectMode(UCalendar*, CalendarID, std::optional<ParsedMonthCode>, uint8_t day);

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendarintegerstoiso
static TemporalResult<ISO8601::PlainDate> calendarIntegersToISO(UCalendar* cal, CalendarID calendarId, std::optional<ParsedMonthCode> monthCode, uint8_t day, TemporalOverflow overflow)
{
    // Step 1: on Reject, verify cursor-resolved fields match the user-requested triple.
    if (overflow == TemporalOverflow::Reject) {
        if (auto r = validateRejectMode(cal, calendarId, monthCode, day); !r) [[unlikely]]
            return makeUnexpected(r.error());
    }
    // Step 2: read UCAL_MILLIS + decompose to ISO Y/M/D.
    UErrorCode status = U_ZERO_ERROR;
    double epochMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    WTF::Int64Milliseconds msWT(ISO8601::Duration::doubleToInt64Saturating(epochMs));
    int32_t days = WTF::msToDays(msWT);
    auto [year, month, dom] = WTF::yearMonthDayFromDays(days);
    if (!ISO8601::isYearWithinLimits(year)) [[unlikely]] // not in spec; defensive PlainDate-range guard.
        return makeUnexpected(rangeError("Resolved calendar date is outside representable range"_s));
    // Step 3 (NOTE): no known calendars have ambiguous mappings.
    // Step 4: return isoDate.
    return ISO8601::PlainDate(year, static_cast<uint8_t>(month + 1), static_cast<uint8_t>(dom));
}

// Arithmetic year → ISO year for buddhist/roc/japanese. Bypasses ICU (Julian shift + range wrap).
static int32_t gregorianStructuredCalendarISOYear(CalendarID calendarId, int32_t year)
{
    if (calendarId == buddhistCalendarID())
        return year + buddhistCalendarYearOffset;
    if (calendarId == rocCalendarID())
        return year + rocCalendarYearOffset;
    ASSERT(calendarId == japaneseCalendarID());
    return year;
}

// Japanese era table. https://tc39.es/proposal-intl-era-monthcode/#table-eras
// icu4x: components/calendar/src/cal/japanese.rs Japanese::eras()
// https://github.com/tc39/proposal-intl-era-monthcode/issues/86
// ICU4C exposes UCAL_ERA as an integer index only; we keep the mapping to canonical codes locally.
// startYear/Month/Day = earliest ISO date this era is emitted on; epochYear = era-year origin (differs only for meiji).
struct JapaneseEra {
    int32_t startYear;
    uint8_t startMonth;
    uint8_t startDay;
    int32_t epochYear;
    ASCIILiteral name;
};
static constexpr auto japaneseEras = WTF::toArray<JapaneseEra>({
    { 2019, 5, 1, 2019, "reiwa"_s },
    { 1989, 1, 8, 1989, "heisei"_s },
    { 1926, 12, 25, 1926, "showa"_s },
    { 1912, 7, 30, 1912, "taisho"_s },
    // Meiji: historical epoch 1868-10-23; Japan adopted Gregorian on 1873-01-01.
    // Emit only for ISO ≥ 1873; era-year counting still uses 1868 (Meiji 6 = ISO 1873).
    // https://github.com/tc39/proposal-intl-era-monthcode/issues/86
    { 1873, 1, 1, 1868, "meiji"_s },
});

// https://tc39.es/proposal-intl-era-monthcode/#table-eras
enum class EraKind : uint8_t { Epoch, Negative, Offset };
struct EraRow {
    CalendarID calendar;
    ASCIILiteral era;
    ASCIILiteral alias;
    EraKind kind;
    int32_t offset;
};
static const Vector<EraRow>& eraTable()
{
    static LazyNeverDestroyed<Vector<EraRow>> table;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [] {
        table.construct();
        table->append({ buddhistCalendarID(), "be"_s, { }, EraKind::Epoch, 0 });
        table->append({ copticCalendarID(), "am"_s, { }, EraKind::Epoch, 0 });
        table->append({ ethioaaCalendarID(), "aa"_s, { }, EraKind::Epoch, 0 });
        table->append({ ethiopicCalendarID(), "am"_s, { }, EraKind::Epoch, 0 });
        table->append({ ethiopicCalendarID(), "aa"_s, { }, EraKind::Offset, -5499 });
        table->append({ gregoryCalendarID(), "ce"_s, "ad"_s, EraKind::Epoch, 0 });
        table->append({ gregoryCalendarID(), "bce"_s, "bc"_s, EraKind::Negative, 0 });
        table->append({ hebrewCalendarID(), "am"_s, { }, EraKind::Epoch, 0 });
        table->append({ indianCalendarID(), "shaka"_s, { }, EraKind::Epoch, 0 });
        for (auto cal : { islamicCivilCalendarID(), islamicTblaCalendarID(), islamicUmalquraCalendarID() }) {
            table->append({ cal, "ah"_s, { }, EraKind::Epoch, 0 });
            table->append({ cal, "bh"_s, { }, EraKind::Negative, 0 });
        }
        // japanese: modern era rows + legacy ce/bce (for ISO ≤ 1872, per spec).
        table->append({ japaneseCalendarID(), "ce"_s, "ad"_s, EraKind::Epoch, 0 });
        table->append({ japaneseCalendarID(), "bce"_s, "bc"_s, EraKind::Negative, 0 });
        for (auto& e : japaneseEras)
            table->append({ japaneseCalendarID(), e.name, { }, EraKind::Offset, e.epochYear });
        table->append({ persianCalendarID(), "ap"_s, { }, EraKind::Epoch, 0 });
        table->append({ rocCalendarID(), "roc"_s, { }, EraKind::Epoch, 0 });
        table->append({ rocCalendarID(), "broc"_s, { }, EraKind::Negative, 0 });
    });
    return table.get();
}

static const JapaneseEra* japaneseEraForDate(const ISO8601::PlainDate& isoDate)
{
    int32_t y = isoDate.year();
    uint8_t m = isoDate.month();
    uint8_t d = isoDate.day();
    for (auto& e : japaneseEras) {
        if (y > e.startYear
            || (y == e.startYear && m > e.startMonth)
            || (y == e.startYear && m == e.startMonth && d >= e.startDay))
            return &e;
    }
    return nullptr;
}

struct EraAndYear {
    std::optional<String> era;
    std::optional<int32_t> eraYear;
};

static std::optional<EraAndYear> emitEraProleptic(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    if (calendarId == japaneseCalendarID()) {
        if (auto* era = japaneseEraForDate(isoDate))
            return EraAndYear { String(era->name), isoDate.year() - era->epochYear + 1 };
        return EraAndYear { isoDate.year() > 0 ? String("ce"_s) : String("bce"_s),
            isoDate.year() > 0 ? isoDate.year() : (1 - isoDate.year()) };
    }
    if (calendarId == buddhistCalendarID() || calendarId == rocCalendarID()) {
        auto fields = gregorianArithmeticYearFieldsFor(calendarId, isoDate.year());
        return EraAndYear { String(fields.era), fields.eraYear };
    }
    return std::nullopt;
}

static EraAndYear emitEraFromICU(CalendarID calendarId, int32_t icuEra, int32_t ucalYear, int32_t extendedYear)
{
    if (!calendarHasEras(calendarId))
        return { std::nullopt, std::nullopt };
    // https://tc39.es/proposal-intl-era-monthcode/#table-eras: pre-epoch eras derived from extended year.
    if (extendedYear <= 0) {
        if (calendarIsIslamic(calendarId))
            return { String("bh"_s), 1 - extendedYear };
    }
    if (calendarId == ethiopicCalendarID())
        return { !icuEra ? String("aa"_s) : String("am"_s), ucalYear };
    // Coptic is proposal single-era "am"; ICU wraps UCAL_YEAR to positive for pre-AM, so use extendedYear.
    if (calendarId == copticCalendarID())
        return { String("am"_s), extendedYear };
    // Gregorian is the only remaining calendar where UCAL_ERA (0=bce, 1=ce) picks between two rows.
    // Single-era calendars (hebrew/indian/persian/ethioaa, plus islamic when extendedYear > 0) return their Epoch row.
    for (const auto& r : eraTable()) {
        if (r.calendar != calendarId)
            continue;
        if (calendarId == gregoryCalendarID()) {
            if (!icuEra && r.kind == EraKind::Negative)
                return { String(r.era), ucalYear };
            if (icuEra && r.kind == EraKind::Epoch)
                return { String(r.era), ucalYear };
        } else if (r.kind == EraKind::Epoch)
            return { String(r.era), ucalYear };
    }
    return { std::nullopt, std::nullopt };
}

// getMonthCode — internal: returns monthCode string from ICU calendar state (e.g. "M05L" for Hebrew Adar I).
// Read-only: does not modify cal.
// https://tc39.es/proposal-intl-era-monthcode/#table-additional-month-codes
// icu4x: components/calendar/src/cal/hebrew.rs (Hebrew special case)
// icu4x: components/calendar/src/cal/chinese_based.rs (Chinese/Dangi IS_LEAP_MONTH)
static std::optional<String> getMonthCode(UCalendar* cal, CalendarID calendarId, UErrorCode& status)
{
    int32_t ucalMonth = ucal_get(cal, UCAL_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    if (calendarId == hebrewCalendarID()) {
        // Hebrew never sets UCAL_IS_LEAP_MONTH (by design -- Adar I/II are distinct slots).
        // Slot 5 only occurs in leap years, so reaching it here already implies leap.
        if (ucalMonth >= 6) {
            int32_t codeNum = ucalMonth;
            return makeString("M"_s, codeNum < 10 ? "0"_s : ""_s, codeNum);
        }
        if (ucalMonth < 5)
            return makeString("M0"_s, ucalMonth + 1);
        return String("M05L"_s);
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

static std::optional<String> getMonthCode(UCalendar* cal, CalendarID calendarId)
{
    UErrorCode status = U_ZERO_ERROR;
    return getMonthCode(cal, calendarId, status);
}

static bool addUTCCalendarDays(UCalendar* cal, int32_t days, UErrorCode& status)
{
    static constexpr double millisecondsPerDay = 86'400'000.0;
    double epochMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return false;
    return ucal_setMillis(cal, epochMs + days * millisecondsPerDay, &status), U_SUCCESS(status);
}

static bool setCalendarToLunisolarYearStart(UCalendar* cal, CalendarID calendarId, std::optional<int32_t> year, UErrorCode& status)
{
    if (calendarId != chineseCalendarID() && calendarId != dangiCalendarID())
        return resetCalendarToMonthStart(cal);
    if (year && !ISO8601::isYearWithinLimits(*year)) [[unlikely]]
        return false;
    if (year && !setCalendarToISODate(cal, ISO8601::PlainDate(*year, 7, 1))) [[unlikely]]
        return false;
    int32_t day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
    if (U_FAILURE(status) || !addUTCCalendarDays(cal, 1 - day, status)) [[unlikely]]
        return false;
    for (int i = 0; i < 14; ++i) {
        auto monthCode = getMonthCode(cal, calendarId);
        if (!monthCode) [[unlikely]]
            return false;
        if (*monthCode == "M01"_s)
            return true;
        if (!addUTCCalendarDays(cal, -1, status)) [[unlikely]]
            return false;
        day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status) || !addUTCCalendarDays(cal, 1 - day, status)) [[unlikely]]
            return false;
    }
    return false;
}

static std::optional<std::pair<double, double>> lunisolarYearAnchor(UCalendar* cal, CalendarID calendarId, UErrorCode& status)
{
    double targetMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    auto targetISODate = isoDateFromCalendarChecked(cal);
    if (!targetISODate) [[unlikely]]
        return std::nullopt;
    CheckedInt32 anchorYear = targetISODate->year();
    if (!setCalendarToLunisolarYearStart(cal, calendarId, anchorYear.value(), status)) [[unlikely]]
        return std::nullopt;
    double yearStartMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    return std::pair { targetMs, yearStartMs };
}

static std::optional<bool> isChineseLeapYear(UCalendar* cal, const std::pair<double, double>& anchor, UErrorCode& status)
{
    ucal_setMillis(cal, anchor.first, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t daysInYear = ucal_getLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    return daysInYear > 360;
}

enum class LunisolarMonthAdvanceResult : uint8_t {
    Advanced,
    Error,
};

static LunisolarMonthAdvanceResult advanceToNextLunisolarMonth(UCalendar* cal, CalendarID calendarId, UErrorCode& status)
{
    if (calendarId != chineseCalendarID() && calendarId != dangiCalendarID()) {
        ucal_add(cal, UCAL_MONTH, 1, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return LunisolarMonthAdvanceResult::Error;
        return forceICUFieldReresolution(cal) ? LunisolarMonthAdvanceResult::Advanced : LunisolarMonthAdvanceResult::Error;
    }

    auto currentMonthCode = getMonthCode(cal, calendarId, status);
    if (!currentMonthCode) [[unlikely]]
        return LunisolarMonthAdvanceResult::Error;
    for (int i = 0; i < 32; ++i) {
        if (!addUTCCalendarDays(cal, 1, status)) [[unlikely]]
            return LunisolarMonthAdvanceResult::Error;
        auto adjacentMonthCode = getMonthCode(cal, calendarId, status);
        if (!adjacentMonthCode) [[unlikely]]
            return LunisolarMonthAdvanceResult::Error;
        if (*adjacentMonthCode != *currentMonthCode)
            return LunisolarMonthAdvanceResult::Advanced;
    }
    return LunisolarMonthAdvanceResult::Error;
}

static std::optional<int32_t> stableLunisolarYear(UCalendar* cal, CalendarID calendarId, UErrorCode& status)
{
    auto anchor = lunisolarYearAnchor(cal, calendarId, status);
    if (!anchor) [[unlikely]]
        return std::nullopt;
    ucal_setMillis(cal, anchor->first, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    WTF::Int64Milliseconds msWT(ISO8601::Duration::doubleToInt64Saturating(anchor->second));
    auto [year, month, day] = WTF::yearMonthDayFromDays(WTF::msToDays(msWT));
    CheckedInt32 relatedYear = year;
    relatedYear += static_cast<int32_t>(month + 1 > 7 || (month + 1 == 7 && day > 1)) - static_cast<int32_t>(anchor->second > anchor->first);
    return relatedYear.hasOverflowed() ? std::nullopt : std::optional<int32_t> { relatedYear.value() };
}

static std::optional<int32_t> actualLunisolarMonthLength(UCalendar* cal, CalendarID calendarId)
{
    UErrorCode status = U_ZERO_ERROR;
    if (calendarId != chineseCalendarID() && calendarId != dangiCalendarID()) {
        int32_t limit = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        return limit;
    }
    double savedMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    auto restoreCalendar = [&] {
        UErrorCode operationStatus = status;
        UErrorCode restoreStatus = U_ZERO_ERROR;
        ucal_setMillis(cal, savedMs, &restoreStatus);
        if (U_FAILURE(restoreStatus)) [[unlikely]] {
            if (U_SUCCESS(operationStatus))
                status = restoreStatus;
            else
                status = operationStatus;
            return false;
        }
        status = operationStatus;
        return true;
    };

    int32_t day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]] {
        restoreCalendar();
        return std::nullopt;
    }

    auto advanceResult = advanceToNextLunisolarMonth(cal, calendarId, status);
    if (advanceResult == LunisolarMonthAdvanceResult::Error) [[unlikely]] {
        restoreCalendar();
        return std::nullopt;
    }
    double nextMonthStartMs = ucal_getMillis(cal, &status);
    bool operationSucceeded = U_SUCCESS(status);
    if (!restoreCalendar()) [[unlikely]]
        return std::nullopt;
    if (!operationSucceeded) [[unlikely]]
        return std::nullopt;
    int32_t length = day - 1 + static_cast<int32_t>((nextMonthStartMs - savedMs) / 86'400'000.0);
    return length >= 29 && length <= 30 ? std::optional<int32_t> { length } : std::nullopt;
}

static bool addLunisolarCalendarDays(UCalendar* cal, CalendarID calendarId, int32_t days, UErrorCode& status)
{
    if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID())
        return addUTCCalendarDays(cal, days, status);
    return ucal_add(cal, UCAL_DAY_OF_MONTH, days, &status), U_SUCCESS(status);
}

// ICU4C-WORKAROUND: rdar://182958553 - Hebrew y0: ICU says Kislev=29 days (Deficient leap);
// icu4x says 30 (Regular). Forces maxDay=30 on input; relabels ICU's M04 D1 as M03 D30 on output.
// FIXME: only relabels the single Tevet D1 slot; doesn't shift the rest of the year, so
// dayOfYear under-counts from there on (see temporal-hebrew-year0-kislev.js skipped block).
// Wait for ICU to fix the classification rather than patching further (icu-issues/01).
static bool isHebrewYear0(UCalendar* cal, CalendarID calendarId)
{
    if (calendarId != hebrewCalendarID())
        return false;
    UErrorCode status = U_ZERO_ERROR;
    int32_t year = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
    return !U_FAILURE(status) && !year;
}
static bool isHebrewYear0ExtraKislevDay(UCalendar* cal, CalendarID calendarId)
{
    if (!isHebrewYear0(cal, calendarId))
        return false;
    UErrorCode status = U_ZERO_ERROR;
    int32_t day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
    if (U_FAILURE(status) || day != 1) [[unlikely]]
        return false;
    int32_t ucalMonth = ucal_get(cal, UCAL_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return false;
    return ucalMonth == 3; // Tevet slot in Hebrew (0-indexed): Tishri=0, Cheshvan=1, Kislev=2, Tevet=3.
}
static bool isHebrewYear0Kislev(UCalendar* cal, CalendarID calendarId)
{
    if (!isHebrewYear0(cal, calendarId))
        return false;
    UErrorCode status = U_ZERO_ERROR;
    int32_t ucalMonth = ucal_get(cal, UCAL_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return false;
    return ucalMonth == 2; // Kislev slot.
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
        if (!forceICUFieldReresolution(cal)) [[unlikely]]
            return std::nullopt;
        ordinal++;
    }

    return static_cast<uint8_t>(ordinal);
}

static std::optional<uint8_t> computeFieldResolutionOrdinalMonth(UCalendar* cal, CalendarID calendarId)
{
    if (calendarId != chineseCalendarID() && calendarId != dangiCalendarID())
        return computeOrdinalMonth(cal, calendarId);

    UErrorCode status = U_ZERO_ERROR;
    auto targetCode = getMonthCode(cal, calendarId);
    auto parsedTargetCode = targetCode ? ISO8601::parseMonthCode(*targetCode) : std::nullopt;
    if (!parsedTargetCode) [[unlikely]]
        return std::nullopt;
    auto anchor = lunisolarYearAnchor(cal, calendarId, status);
    if (!anchor) [[unlikely]]
        return std::nullopt;

    if (anchor->second > anchor->first) {
        auto isLeapYear = isChineseLeapYear(cal, *anchor, status);
        if (!isLeapYear) [[unlikely]]
            return std::nullopt;
        uint8_t ordinal = parsedTargetCode->monthNumber + parsedTargetCode->isLeapMonth;
        if (*isLeapYear && !parsedTargetCode->isLeapMonth) {
            bool leapFollows = false;
            for (uint8_t i = 0; i < 13; ++i) {
                if (advanceToNextLunisolarMonth(cal, calendarId, status) != LunisolarMonthAdvanceResult::Advanced) [[unlikely]]
                    return std::nullopt;
                auto code = getMonthCode(cal, calendarId);
                if (!code) [[unlikely]]
                    return std::nullopt;
                if (*code == "M01"_s)
                    return ordinal + !leapFollows;
                leapFollows |= code->endsWith("L"_s);
            }
            return std::nullopt;
        }
        return ordinal <= 13 ? std::optional<uint8_t> { ordinal } : std::nullopt;
    }

    for (uint8_t ordinal = 1; ordinal <= 13; ++ordinal) {
        auto code = getMonthCode(cal, calendarId);
        if (!code) [[unlikely]]
            return std::nullopt;
        if (*code == *targetCode)
            return ordinal;
        if (advanceToNextLunisolarMonth(cal, calendarId, status) != LunisolarMonthAdvanceResult::Advanced) [[unlikely]]
            return std::nullopt;
    }
    return std::nullopt;
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-canonicalizeeraincalendar
std::optional<ASCIILiteral> canonicalizeEraInCalendar(CalendarID calendarId, StringView era)
{
    // Step 1: walk table-eras; return canonical name for input matching canonical or alias.
    for (const auto& r : eraTable()) {
        if (r.calendar != calendarId)
            continue;
        if (equalIgnoringASCIICase(era, StringView(r.era)))
            return r.era;
        if (!r.alias.isNull() && equalIgnoringASCIICase(era, StringView(r.alias)))
            return r.era;
    }
    // Steps 2-3: unknown era in a known calendar → undefined; unknown calendar → implementation-defined (both nullopt).
    return std::nullopt;
}

// Swap negative eraYear to the calendar's complement era: Epoch↔Negative gives 1-eraYear;
// Epoch→Offset (ethiopic am→aa) gives eraYear + 1 - offset. Returns nullopt for positive
// eraYear or single-era calendars.
std::optional<std::pair<ASCIILiteral, int32_t>> remapNonPositiveEraYear(CalendarID calendarId, StringView era, int32_t eraYear)
{
    if (eraYear > 0)
        return std::nullopt;
    const EraRow* inputRow = nullptr;
    for (const auto& r : eraTable()) {
        if (r.calendar == calendarId && StringView(r.era) == era) {
            inputRow = &r;
            break;
        }
    }
    if (!inputRow)
        return std::nullopt;
    // Epoch ↔ Negative complement: swap era, year → 1 - eraYear (gregory ce↔bce, roc↔broc, islamic ah↔bh, japanese ce↔bce).
    if (inputRow->kind == EraKind::Epoch || inputRow->kind == EraKind::Negative) {
        EraKind wantedKind = inputRow->kind == EraKind::Epoch ? EraKind::Negative : EraKind::Epoch;
        for (const auto& r : eraTable()) {
            if (r.calendar == calendarId && r.kind == wantedKind)
                return std::make_pair(r.era, 1 - eraYear);
        }
    }
    // Epoch → Offset (ethiopic am → aa): shift eraYear into the Offset era's counting.
    if (inputRow->kind == EraKind::Epoch && calendarId == ethiopicCalendarID()) {
        for (const auto& r : eraTable()) {
            if (r.calendar == calendarId && r.kind == EraKind::Offset)
                return std::make_pair(r.era, eraYear + 1 - r.offset);
        }
    }
    return std::nullopt;
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendardatearithmeticyearforerayear
std::optional<int32_t> calendarDateArithmeticYearForEraYear(CalendarID calendarId, StringView era, int32_t eraYear)
{
    // Step 1: canonicalize (idempotent — caller may have canonicalized already).
    auto canonicalOpt = canonicalizeEraInCalendar(calendarId, era);
    StringView eraToMatch = canonicalOpt ? StringView(*canonicalOpt) : era;
    // Steps 4-6: find matching row.
    for (const auto& r : eraTable()) {
        if (r.calendar != calendarId || StringView(r.era) != eraToMatch)
            continue;
        // Steps 7-11: eraKind-driven arithmetic.
        switch (r.kind) {
        case EraKind::Epoch:
            return eraYear; // step 7
        case EraKind::Negative:
            return 1 - eraYear; // step 8
        case EraKind::Offset:
            return r.offset + eraYear - 1; // step 11
        }
    }
    // Step 3: unknown (calendar, era) pair → implementation-defined (nullopt).
    return std::nullopt;
}

// ICU4C-WORKAROUND: rdar://182960658 - IndianCalendar's ucal_setMillis/ucal_get round-trip
// produces an off-by-one day at extreme Saka years (mechanism unconfirmed -- not the JS
// Date range boundary). Local arithmetic ported from icu4x components/calendar/src/cal/indian.rs.
static uint8_t indianSakaDaysInMonth(int32_t sakaYear, uint8_t month)
{
    if (month == 1)
        return WTF::isLeapYear(sakaYear + indianCalendarYearOffset) ? 31 : 30;
    return month <= 6 ? 31 : 30;
}
static std::optional<ISO8601::PlainDate> indianSakaToISO(int32_t sakaYear, uint8_t month, uint8_t day)
{
    if (month < 1 || month > 12 || day < 1)
        return std::nullopt;
    uint8_t monthLen = indianSakaDaysInMonth(sakaYear, month);
    if (day > monthLen)
        return std::nullopt;
    bool isLeap = WTF::isLeapYear(sakaYear + indianCalendarYearOffset);
    uint32_t sakaDayOfYear = day;
    for (uint8_t m = 1; m < month; m++)
        sakaDayOfYear += indianSakaDaysInMonth(sakaYear, m);
    // ISO day-of-year = sakaDayOfYear + 80 (Chaitra 1 = ISO day 81: 31+28+22 non-leap or 31+29+21 leap).
    int32_t isoYear = sakaYear + indianCalendarYearOffset;
    uint32_t isoDayOfYear = sakaDayOfYear + 80;
    uint32_t daysInISOYear = isLeap ? 366 : 365;
    if (isoDayOfYear > daysInISOYear) {
        isoYear++;
        isoDayOfYear -= daysInISOYear;
    }
    // Convert (isoYear, isoDayOfYear) -> (isoYear, isoMonth, isoDay).
    static constexpr std::array<uint16_t, 13> offsets { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    static constexpr std::array<uint16_t, 13> offsetsLeap { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 };
    bool isoIsLeap = WTF::isLeapYear(isoYear);
    const auto& off = isoIsLeap ? offsetsLeap : offsets;
    uint8_t isoMonth = 12;
    for (uint8_t m = 1; m <= 12; m++) {
        if (isoDayOfYear <= off[m]) {
            isoMonth = m;
            break;
        }
    }
    uint8_t isoDay = static_cast<uint8_t>(isoDayOfYear - off[isoMonth - 1]);
    return ISO8601::PlainDate(isoYear, isoMonth, isoDay);
}

// Design choice, not an ICU4C workaround: beyond icu4x's WELL_BEHAVED_ASTRONOMICAL_RANGE
// (±10000 years), chinese/dangi astronomical output isn't trustworthy. Every getter below
// falls back to ISO fields here, matching nonISOCalendarDateToISO's construction-side fallback.
static bool calendarUsesISOFallbackForExtremeYear(CalendarID calendarId, int32_t isoYear)
{
    return (calendarId == chineseCalendarID() || calendarId == dangiCalendarID()) && std::abs(isoYear) > 10000;
}

// isoToCalendarFields — no single temporal_rs equivalent; aggregates Calendar::year/month/month_code/day/era.
TemporalResult<CalendarFields> isoToCalendarFields(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    if (calendarId == rocCalendarID() || calendarId == buddhistCalendarID()) {
        auto yearFields = gregorianArithmeticYearFieldsFor(calendarId, isoDate.year());
        CalendarFields fields;
        fields.year = yearFields.year;
        fields.era = String(yearFields.era);
        fields.eraYear = yearFields.eraYear;
        fields.month = isoDate.month();
        fields.day = isoDate.day();
        fields.monthCode = ISO8601::monthCode(isoDate.month());
        return fields;
    }

    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year())) {
        CalendarFields fields;
        fields.year = isoDate.year();
        fields.month = isoDate.month();
        fields.day = isoDate.day();
        fields.monthCode = ISO8601::monthCode(isoDate.month());
        return fields;
    }

    struct RawFields {
        int32_t extendedYear { 0 };
        int32_t ucalEra { 0 };
        int32_t ucalYear { 0 };
        int32_t day { 0 };
        std::optional<uint8_t> ordinalMonth;
        std::optional<String> monthCode;
        bool hasEra { false };
    };

    auto rawOrError = withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<RawFields> {
        RawFields raw;
        bool isChineseBased = calendarId == chineseCalendarID() || calendarId == dangiCalendarID();
        if (isChineseBased) {
            UErrorCode status = U_ZERO_ERROR;
            auto stableYear = stableLunisolarYear(cal, calendarId, status);
            if (U_FAILURE(status) || !stableYear) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            raw.extendedYear = *stableYear;
        } else {
            auto extendedYear = readICUField(cal, UCAL_EXTENDED_YEAR);
            if (!extendedYear) [[unlikely]]
                return makeUnexpected(extendedYear.error());
            raw.extendedYear = *extendedYear;
        }
        auto ucalEra = readICUField(cal, UCAL_ERA);
        if (!ucalEra) [[unlikely]]
            return makeUnexpected(ucalEra.error());
        raw.ucalEra = *ucalEra;
        auto ucalYear = readICUField(cal, UCAL_YEAR);
        if (!ucalYear) [[unlikely]]
            return makeUnexpected(ucalYear.error());
        raw.ucalYear = *ucalYear;
        auto day = readICUField(cal, UCAL_DAY_OF_MONTH);
        if (!day) [[unlikely]]
            return makeUnexpected(day.error());
        raw.day = *day;
        raw.monthCode = getMonthCode(cal, calendarId);
        if (!raw.monthCode) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        raw.hasEra = calendarHasEras(calendarId);
        raw.ordinalMonth = computeFieldResolutionOrdinalMonth(cal, calendarId);
        if (!raw.ordinalMonth) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        {
            // ICU4C-WORKAROUND: rdar://182958553 - re-label y0 M04 D1 as y0 M03 D30.
            if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
                return makeUnexpected(rangeError(icuSetCalendarFailed));
            if (isHebrewYear0ExtraKislevDay(cal, calendarId)) {
                raw.day = 30;
                raw.ordinalMonth = 3;
                raw.monthCode = String("M03"_s);
            }
        }
        return raw;
    });

    if (!rawOrError) [[unlikely]]
        return makeUnexpected(rawOrError.error());
    auto& raw = *rawOrError;

    CalendarFields fields;
    fields.year = raw.extendedYear;
    if (calendarId == ethioaaCalendarID())
        fields.year = raw.ucalYear;
    fields.month = *raw.ordinalMonth;
    fields.day = static_cast<uint8_t>(raw.day);
    fields.monthCode = WTF::move(*raw.monthCode);

    if (calendarId == japaneseCalendarID()) {
        // Japanese uses proleptic Gregorian for its date fields, so year/month/day/monthCode are
        // always the ISO values regardless of era. Only era/eraYear differ.
        fields.year = isoDate.year();
        fields.month = isoDate.month();
        fields.day = isoDate.day();
        fields.monthCode = ISO8601::monthCode(isoDate.month());
    }

    // isLeapMonth is re-derived from the month code (avoids needing UCAL_IS_LEAP_MONTH in raw)
    fields.isLeapMonth = fields.monthCode.endsWith("L"_s);

    if (raw.hasEra) {
        auto emitted = emitEraProleptic(calendarId, isoDate).value_or(emitEraFromICU(calendarId, raw.ucalEra, raw.ucalYear, raw.extendedYear));
        fields.era = emitted.era;
        fields.eraYear = emitted.eraYear;
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
    // NOTE: The Japanese extended year is always the ISO year.
    if (calendarId == japaneseCalendarID())
        return isoDate.year();
    if (calendarId == rocCalendarID() || calendarId == buddhistCalendarID())
        return gregorianArithmeticYearFieldsFor(calendarId, isoDate.year()).year;
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return isoDate.year();
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;
        if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID()) {
            auto year = stableLunisolarYear(cal, calendarId, status);
            if (!year) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            return *year;
        }
        // Older ICU versions expose an Amete Mihret-relative UCAL_EXTENDED_YEAR for Ethioaa;
        // newer versions may make UCAL_YEAR and UCAL_EXTENDED_YEAR equal.
        if (calendarId == ethioaaCalendarID()) {
            int32_t eraYear = ucal_get(cal, UCAL_YEAR, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            return eraYear;
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
    if (calendarIsGregorianStructured(calendarId))
        return isoDate.month();
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return isoDate.month();
    return withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<uint8_t> {
        {
            // ICU4C-WORKAROUND: rdar://182958553 - re-label y0 M04 as ordinal 3.
            if (isHebrewYear0ExtraKislevDay(cal, calendarId))
                return static_cast<uint8_t>(3);
        }
        auto ordinal = computeFieldResolutionOrdinalMonth(cal, calendarId);
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
    if (calendarIsGregorianStructured(calendarId))
        return ISO8601::monthCode(isoDate.month());
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return ISO8601::monthCode(isoDate.month());
    return withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<String> {
        {
            // ICU4C-WORKAROUND: rdar://182958553 - re-label y0 M04 as monthCode M03.
            if (isHebrewYear0ExtraKislevDay(cal, calendarId))
                return String("M03"_s);
        }
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
    if (calendarIsGregorianStructured(calendarId))
        return isoDate.day();
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return isoDate.day();
    return withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<uint8_t> {
        {
            // ICU4C-WORKAROUND: rdar://182958553 - Kislev has 30 days in y0 per icu4x.
            if (isHebrewYear0ExtraKislevDay(cal, calendarId))
                return static_cast<uint8_t>(30);
        }
        auto day = readICUField(cal, UCAL_DAY_OF_MONTH);
        if (!day) [[unlikely]]
            return makeUnexpected(day.error());
        return static_cast<uint8_t>(*day);
    });
}

// calendarDayOfYear — 1-based day within the calendar's year (ISO's if calendarUsesISODateArithmetic).
TemporalResult<int32_t> calendarDayOfYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    if (calendarUsesISODateArithmetic(calendarId))
        return static_cast<int32_t>(ISO8601::dayOfYear(isoDate));
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return static_cast<int32_t>(ISO8601::dayOfYear(isoDate));
    return withCalendarSetToDate(calendarId, isoDate, [](UCalendar* cal) {
        return readICUField(cal, UCAL_DAY_OF_YEAR);
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
    if (!calendarHasEras(calendarId))
        return std::optional<String>(std::nullopt);
    if (auto proleptic = emitEraProleptic(calendarId, isoDate))
        return proleptic->era;
    return withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<std::optional<String>> {
        auto icuEra = readICUField(cal, UCAL_ERA);
        if (!icuEra) [[unlikely]]
            return makeUnexpected(icuEra.error());
        auto ucalYear = readICUField(cal, UCAL_YEAR);
        if (!ucalYear) [[unlikely]]
            return makeUnexpected(ucalYear.error());
        auto extendedYear = readICUField(cal, UCAL_EXTENDED_YEAR);
        if (!extendedYear) [[unlikely]]
            return makeUnexpected(extendedYear.error());
        return emitEraFromICU(calendarId, *icuEra, *ucalYear, *extendedYear).era;
    });
}

// calendarEraYear — temporal_rs: Calendar::era_year (src/builtins/core/calendar.rs)
//                   icu4x: Date::era_year (components/calendar/src/date.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarisotodate
// CalendarISOToDate [[EraYear]] field:
//   1. (iso8601) Return undefined.
//   2. (non-ISO) NonISOCalendarISOToDate — implementation-defined.
TemporalResult<std::optional<int32_t>> calendarEraYear(CalendarID calendarId, const ISO8601::PlainDate& isoDate)
{
    if (!calendarHasEras(calendarId))
        return std::optional<int32_t>(std::nullopt);
    if (auto proleptic = emitEraProleptic(calendarId, isoDate))
        return proleptic->eraYear;
    return withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<std::optional<int32_t>> {
        auto icuEra = readICUField(cal, UCAL_ERA);
        if (!icuEra) [[unlikely]]
            return makeUnexpected(icuEra.error());
        auto ucalYear = readICUField(cal, UCAL_YEAR);
        if (!ucalYear) [[unlikely]]
            return makeUnexpected(ucalYear.error());
        auto extendedYear = readICUField(cal, UCAL_EXTENDED_YEAR);
        if (!extendedYear) [[unlikely]]
            return makeUnexpected(extendedYear.error());
        return emitEraFromICU(calendarId, *icuEra, *ucalYear, *extendedYear).eraYear;
    });
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
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return ISO8601::daysInMonth(isoDate.year(), isoDate.month());
    return withCalendarSetToDate(gregorianArithmeticCalendarFor(calendarId), isoDate, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        // ICU4C-WORKAROUND: rdar://182958553 - ICU says 29 (Deficient leap); icu4x says 30.
        if (isHebrewYear0Kislev(cal, calendarId) || isHebrewYear0ExtraKislevDay(cal, calendarId))
            return 30;
        if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID()) {
            auto result = actualLunisolarMonthLength(cal, calendarId);
            if (!result) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            return *result;
        }
        return readICUFieldLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM);
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
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return WTF::daysInYear(isoDate.year());
    return withCalendarSetToDate(gregorianArithmeticCalendarFor(calendarId), isoDate, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        // ICU4C-WORKAROUND: rdar://182958553 - Hebrew y0 is Regular leap per icu4x (384), not Deficient leap (383).
        if (calendarId == hebrewCalendarID()) {
            UErrorCode s = U_ZERO_ERROR;
            int32_t y = ucal_get(cal, UCAL_EXTENDED_YEAR, &s);
            if (!U_FAILURE(s) && !y)
                return 384;
        }
        return readICUFieldLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM);
    });
}

static TemporalResult<int32_t> walkLunisolarMonthsFromYearStart(UCalendar* cal, CalendarID calendarId, ASCIILiteral failureMessage)
{
    UErrorCode status = U_ZERO_ERROR;
    bool isChineseBased = calendarId == chineseCalendarID() || calendarId == dangiCalendarID();
    int32_t savedYear = isChineseBased ? 0 : ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    int32_t count = 1;
    bool reachedNextYear = false;
    for (int i = 0; i < 14; i++) {
        if (advanceToNextLunisolarMonth(cal, calendarId, status) != LunisolarMonthAdvanceResult::Advanced) [[unlikely]]
            return makeUnexpected(rangeError(failureMessage));
        int32_t curYear = isChineseBased ? savedYear : ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        auto monthCode = isChineseBased ? getMonthCode(cal, calendarId) : std::optional<String> { };
        if (U_FAILURE(status) || (isChineseBased && !monthCode)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        if ((isChineseBased && *monthCode == "M01"_s) || (!isChineseBased && curYear != savedYear)) {
            reachedNextYear = true;
            break;
        }
        count++;
    }
    if (!reachedNextYear || count > 13) [[unlikely]]
        return makeUnexpected(rangeError(failureMessage));
    return count;
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
    if (calendarUsesISOFallbackForExtremeYear(calendarId, isoDate.year()))
        return 12;
    return withCalendarSetToDate(calendarId, isoDate, [&](UCalendar* cal) -> TemporalResult<int32_t> {
        if (calendarIsLunisolar(calendarId)) {
            // For lunisolar calendars, count months by walking from month 1 to end of year.
            // cal is local; mutating it has no observable effect outside this function.
            UErrorCode status = U_ZERO_ERROR;
            bool isChineseBased = calendarId == chineseCalendarID() || calendarId == dangiCalendarID();
            if (isChineseBased) {
                auto anchor = lunisolarYearAnchor(cal, calendarId, status);
                if (!anchor) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                if (anchor->second > anchor->first) {
                    auto isLeapYear = isChineseLeapYear(cal, *anchor, status);
                    if (!isLeapYear) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    return *isLeapYear ? 13 : 12;
                }
            } else if (!setCalendarToLunisolarYearStart(cal, calendarId, std::nullopt, status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            return walkLunisolarMonthsFromYearStart(cal, calendarId, icuCalendarArithmeticFailed);
        }
        auto result = readICUFieldLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM);
        if (!result) [[unlikely]]
            return makeUnexpected(result.error());
        return *result + 1;
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
    return withCalendarSetToDate(gregorianArithmeticCalendarFor(calendarId), isoDate, [](UCalendar* cal) -> TemporalResult<bool> {
        auto actualMax = readICUFieldLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM);
        if (!actualMax) [[unlikely]]
            return makeUnexpected(actualMax.error());
        auto leastMax = readICUFieldLimit(cal, UCAL_DAY_OF_YEAR, UCAL_LEAST_MAXIMUM);
        if (!leastMax) [[unlikely]]
            return makeUnexpected(leastMax.error());
        return *actualMax > *leastMax;
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
        if (!resetCalendarToMonthStart(cal)) [[unlikely]]
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
            if (!forceICUFieldReresolution(cal)) [[unlikely]]
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
    if (!resetCalendarToMonthStart(cal)) [[unlikely]]
        return std::nullopt;
    double previousMonthStartMs = ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    for (int i = 0; i < 14; i++) {
        auto curCode = getMonthCode(cal, calendarId);
        if (!curCode) [[unlikely]]
            return std::nullopt;
        if (*curCode == monthCode)
            return 1; // exact match
        // If we've walked past the target monthCode lexicographically, the requested code
        // (a leap month) doesn't exist in this year — constrain to the base month by
        // reverting to the previous position (Chinese/Dangi convention, matches temporal_rs).
        // Without this early-stop, walking to year-end would incorrectly land on M12.
        if (codePointCompare(*curCode, monthCode) > 0) {
            ucal_setMillis(cal, previousMonthStartMs, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return std::nullopt;
            return 0; // constrained to base month
        }
        previousMonthStartMs = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        if (advanceToNextLunisolarMonth(cal, calendarId, status) != LunisolarMonthAdvanceResult::Advanced) [[unlikely]]
            return std::nullopt;
        int32_t curYear = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        if (curYear != savedYear) {
            // Month code sorts after every month in the year (e.g., "M99") — constrain to last month.
            ucal_setMillis(cal, previousMonthStartMs, &status);
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

static UCalendarDateFields calendarArithmeticYearField(CalendarID calendarId)
{
    return calendarId == ethioaaCalendarID() ? UCAL_YEAR : UCAL_EXTENDED_YEAR;
}

// Chinese/Dangi: UCAL_EXTENDED_YEAR's epoch differs across ICU versions, so position via ISO date instead.
static bool positionCalendarToYearStart(UCalendar* cal, CalendarID calendarId, int32_t year, UErrorCode& status)
{
    if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID())
        return setCalendarToLunisolarYearStart(cal, calendarId, year, status);
    auto yearField = calendarArithmeticYearField(calendarId);
    if (yearField == UCAL_YEAR)
        ucal_set(cal, UCAL_ERA, 0);
    ucal_set(cal, yearField, year);
    return resetCalendarToMonthStart(cal);
}

// resolveMonthCodeToOrdinal — internal: resolves a monthCode to its 1-based ordinal position in the given year
static int32_t resolveMonthCodeToOrdinal(CalendarID calendarId, const String& monthCode, int32_t year)
{
    return withCalendar(calendarId, [&](UCalendar* cal) -> int32_t {
        if (!cal) [[unlikely]]
            return 1;
        UErrorCode status = U_ZERO_ERROR;
        auto yearField = calendarArithmeticYearField(calendarId);
        if (!positionCalendarToYearStart(cal, calendarId, year, status)) [[unlikely]]
            return 1;

        int32_t savedYear = ucal_get(cal, yearField, &status);
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
            if (!forceICUFieldReresolution(cal)) [[unlikely]]
                return lastOrdinal;
            int32_t curYear = ucal_get(cal, yearField, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return lastOrdinal;
            if (curYear != savedYear)
                return lastOrdinal;
        }
        return lastOrdinal;
    });
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-isvalidmonthcodeforcalendar
bool isValidMonthCodeForCalendar(CalendarID calendarId, ParsedMonthCode monthCode)
{
    uint8_t number = monthCode.monthNumber;
    bool isLeap = monthCode.isLeapMonth;
    // Steps 1-2: commonMonthCodes = «M01..M12». If contains monthCode → return true.
    if (!isLeap && number >= 1 && number <= 12)
        return true;
    // Step 3: if calendar is in table-additional-month-codes, check its "Additional Month Codes" list.
    if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID())
        return isLeap && number >= 1 && number <= 12; // «M01L..M12L»
    if (calendarId == copticCalendarID() || calendarId == ethiopicCalendarID() || calendarId == ethioaaCalendarID())
        return !isLeap && number == 13; // «M13»
    if (calendarId == hebrewCalendarID())
        return isLeap && number == 5; // «M05L»
    // Step 4: calendar in table-calendar-types but not in table-additional-month-codes.
    // (buddhist / gregory / indian / islamic-* / japanese / persian / roc / iso8601)
    // Step 5 (implementation-defined for unknown calendars): the same `return false` also
    // satisfies step 5 if JSC ever adds a calendar not in the spec's table-calendar-types.
    return false;
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-yearcontainsmonthcode
static bool yearContainsMonthCodeInternal(UCalendar* cal, CalendarID calendarId, int32_t year, ParsedMonthCode monthCode)
{
    // Step 1: Assert IsValidMonthCodeForCalendar.
    ASSERT(isValidMonthCodeForCalendar(calendarId, monthCode));
    // Step 2: non-leap monthCodes always exist in every year.
    if (!monthCode.isLeapMonth)
        return true;
    // Step 3 (calendar-dependent): leap month exists iff ICU can position on the exact monthCode.
    UErrorCode status = U_ZERO_ERROR;
    if (!positionCalendarToYearStart(cal, calendarId, year, status)) [[unlikely]]
        return false;
    String mcStr = makeString("M"_s, monthCode.monthNumber < 10 ? "0"_s : ""_s, monthCode.monthNumber, "L"_s);
    auto probe = setCalendarToMonthCode(cal, calendarId, mcStr);
    return probe && *probe == 1;
}

// Public wrapper: opens ICU via withCalendar for one-shot callers.
bool yearContainsMonthCode(CalendarID calendarId, int32_t year, ParsedMonthCode monthCode)
{
    return withCalendar(calendarId, [&](UCalendar* cal) -> bool {
        return cal && yearContainsMonthCodeInternal(cal, calendarId, year, monthCode);
    });
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-constrainmonthcode
static TemporalResult<ParsedMonthCode> constrainMonthCodeGivenContainment(CalendarID calendarId, ParsedMonthCode monthCode, bool contained, TemporalOverflow overflow)
{
    // Step 2: If YearContainsMonthCode, return monthCode.
    if (contained)
        return monthCode;
    // Step 3: If overflow is ~reject~, throw RangeError.
    if (overflow == TemporalOverflow::Reject)
        return makeUnexpected(rangeError("monthCode does not exist in this calendar year"_s));
    // Step 4: Assert calendar is in table-additional-month-codes with a leap-transformation column.
    ASSERT(calendarId == chineseCalendarID() || calendarId == dangiCalendarID() || calendarId == hebrewCalendarID());
    // Steps 5-7: chinese/dangi row → ~skip-backward~ → CreateMonthCode(monthNumber, false).
    if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID())
        return ParsedMonthCode { monthCode.monthNumber, false };
    // Step 8: hebrew row → ~skip-forward~ → assert "M05L" (8.a), return "M06" (8.b).
    ASSERT(monthCode.monthNumber == 5 && monthCode.isLeapMonth);
    return ParsedMonthCode { 6, false };
}
TemporalResult<ParsedMonthCode> constrainMonthCode(CalendarID calendarId, int32_t year, ParsedMonthCode monthCode, TemporalOverflow overflow)
{
    // Step 1: Assert IsValidMonthCodeForCalendar.
    ASSERT(isValidMonthCodeForCalendar(calendarId, monthCode));
    // Steps 2-8: delegate to algorithm extract with cursor-computed containment.
    return constrainMonthCodeGivenContainment(calendarId, monthCode, yearContainsMonthCode(calendarId, year, monthCode), overflow);
}
static TemporalResult<ParsedMonthCode> constrainMonthCodeInternal(UCalendar* cal, CalendarID calendarId, ParsedMonthCode monthCode, TemporalOverflow overflow)
{
    // Step 1: Assert IsValidMonthCodeForCalendar.
    ASSERT(isValidMonthCodeForCalendar(calendarId, monthCode));
    UErrorCode status = U_ZERO_ERROR;
    int32_t year = ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    // Steps 2-8: delegate to algorithm extract with cursor-computed containment.
    return constrainMonthCodeGivenContainment(calendarId, monthCode, yearContainsMonthCodeInternal(cal, calendarId, year, monthCode), overflow);
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-monthcodetoordinal
int32_t monthCodeOrdinalInYear(CalendarID calendarId, ParsedMonthCode monthCode, int32_t year)
{
    ASSERT(isValidMonthCodeForCalendar(calendarId, monthCode));
    ASSERT(yearContainsMonthCode(calendarId, year, monthCode));

    // Step 6 (extended to non-table calendars): no leap months -> ordinal == monthNumber.
    if (!calendarIsLunisolar(calendarId))
        return monthCode.monthNumber;

    // Steps 2-4 init + steps 8-9 loop: M01, M01L, M02, M02L, ..., M12, M12L.
    return withCalendar(calendarId, [&](UCalendar* cal) -> int32_t {
        if (!cal) [[unlikely]]
            return monthCode.monthNumber; // fallback: ignore leap-month index shift
        int32_t monthsBefore = 0;
        for (uint8_t number = 1; number <= 12; number++) {
            for (bool isLeap : { false, true }) {
                ParsedMonthCode candidate { number, isLeap };
                if (isValidMonthCodeForCalendar(calendarId, candidate)
                    && yearContainsMonthCodeInternal(cal, calendarId, year, candidate))
                    monthsBefore++; // Step 9.b
                if (number == monthCode.monthNumber && isLeap == monthCode.isLeapMonth) // Step 9.c
                    return monthsBefore;
            }
        }
        RELEASE_ASSERT_NOT_REACHED(); // Precondition ensures target is visited.
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

static bool calendarIsNonISOSolar(CalendarID calendarId)
{
    return calendarId == copticCalendarID() || calendarId == ethiopicCalendarID() || calendarId == ethioaaCalendarID()
        || calendarId == indianCalendarID() || calendarId == persianCalendarID();
}

static int32_t fixedSolarMonthsInYear(CalendarID calendarId)
{
    ASSERT(calendarIsNonISOSolar(calendarId));
    return calendarId == copticCalendarID() || calendarId == ethiopicCalendarID() || calendarId == ethioaaCalendarID() ? 13 : 12;
}

struct FixedSolarYearMonth {
    int32_t year;
    int32_t month;
};

static std::optional<FixedSolarYearMonth> balanceFixedSolarYearMonth(int32_t sourceYear, int32_t sourceMonth, int32_t monthsInYear, int64_t years, int64_t months)
{
    CheckedInt64 checkedBalancedMonth = CheckedInt64(sourceMonth) + months;
    if (checkedBalancedMonth.hasOverflowed()) [[unlikely]]
        return std::nullopt;
    int64_t balancedMonth = checkedBalancedMonth;
    int64_t yearDelta = balancedMonth / monthsInYear;
    int32_t expectedMonth = balancedMonth % monthsInYear;
    if (expectedMonth < 0) {
        expectedMonth += monthsInYear;
        --yearDelta;
    }

    CheckedInt64 checkedExpectedYear = CheckedInt64(sourceYear) + years + yearDelta;
    if (checkedExpectedYear.hasOverflowed()) [[unlikely]]
        return std::nullopt;
    CheckedInt32 expectedYear = checkedExpectedYear;
    if (expectedYear.hasOverflowed()) [[unlikely]]
        return std::nullopt;
    return FixedSolarYearMonth { expectedYear, expectedMonth };
}

static std::optional<bool> verifyAndCorrectFixedSolarDay(UCalendar* cal, UCalendarDateFields yearField, int32_t expectedYear, int32_t expectedMonth, int32_t expectedDay)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t actualYear = ucal_get(cal, yearField, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t actualMonth = ucal_get(cal, UCAL_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t actualDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    if (actualYear == expectedYear && actualMonth == expectedMonth && std::abs(actualDay - expectedDay) == 1) {
        ucal_add(cal, UCAL_DAY_OF_MONTH, expectedDay - actualDay, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        actualYear = ucal_get(cal, yearField, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        actualMonth = ucal_get(cal, UCAL_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        actualDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
    }
    return actualYear == expectedYear && actualMonth == expectedMonth && actualDay == expectedDay;
}

// Clear and construct an exact native year/month at day 1. Noon preserves Temporal's partial-day
// endpoints. At ICU's extreme millisecond boundary, directly set fields can resolve one day off;
// correct only that rounding after the expected year/month resolve, then verify every field.
static std::optional<bool> setFixedSolarCalendarToYearMonth(UCalendar* cal, CalendarID calendarId, const FixedSolarYearMonth& expected)
{
    ucal_clear(cal);
    auto yearField = calendarArithmeticYearField(calendarId);
    if (yearField == UCAL_YEAR)
        ucal_set(cal, UCAL_ERA, 0);
    ucal_set(cal, yearField, expected.year);
    ucal_set(cal, UCAL_MONTH, expected.month);
    ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
    ucal_set(cal, UCAL_HOUR_OF_DAY, 12);

    UErrorCode status = U_ZERO_ERROR;
    ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    return verifyAndCorrectFixedSolarDay(cal, yearField, expected.year, expected.month, 1);
}

static TemporalResult<ISO8601::PlainDate> fixedSolarDateAdd(CalendarID calendarId, const ISO8601::PlainDate& isoDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    auto baselineOrError = withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::PlainDate> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        UErrorCode status = U_ZERO_ERROR;
        auto yearField = calendarArithmeticYearField(calendarId);
        int32_t sourceYear = ucal_get(cal, yearField, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        int32_t sourceMonth = ucal_get(cal, UCAL_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        int32_t sourceDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));

        auto expected = balanceFixedSolarYearMonth(sourceYear, sourceMonth, fixedSolarMonthsInYear(calendarId), duration.years(), duration.months());
        if (!expected) [[unlikely]]
            return makeUnexpected(rangeError("Result of calendar date addition is outside representable range"_s));
        auto constructedExactly = setFixedSolarCalendarToYearMonth(cal, calendarId, *expected);
        if (!constructedExactly) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        if (!*constructedExactly) [[unlikely]]
            return makeUnexpected(rangeError("Result of calendar date addition is outside representable range"_s));

        int32_t maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        if (overflow == TemporalOverflow::Reject && sourceDay > maxDay) [[unlikely]]
            return makeUnexpected(rangeError("day is out of range for the resulting month (overflow: reject)"_s));
        int32_t regulatedDay = std::min(sourceDay, maxDay);
        ucal_set(cal, UCAL_DAY_OF_MONTH, regulatedDay);
        ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        // At ICU's extreme millisecond boundary, resolving a directly set day can round to
        // the adjacent day even though that native day is representable. Correct only the day
        // after the expected year/month have resolved exactly, then verify all fields again.
        auto constructedRegulatedDay = verifyAndCorrectFixedSolarDay(cal, yearField, expected->year, expected->month, regulatedDay);
        if (!constructedRegulatedDay) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        if (!*constructedRegulatedDay) [[unlikely]]
            return makeUnexpected(rangeError("Result of calendar date addition is outside representable range"_s));

        auto baseline = isoDateFromCalendarChecked(cal);
        if (!baseline || !ISO8601::isDateTimeWithinLimits(baseline->year(), baseline->month(), baseline->day(), 12, 0, 0, 0, 0, 0)) [[unlikely]]
            return makeUnexpected(rangeError("Result of calendar date addition is outside representable range"_s));
        return *baseline;
    });
    if (!baselineOrError) [[unlikely]]
        return makeUnexpected(baselineOrError.error());

    ISO8601::Duration remaining;
    remaining.setWeeks(duration.weeks());
    remaining.setDays(duration.days());
    return isoDateAdd(*baselineOrError, remaining, overflow);
}

// calendarDateAdd — temporal_rs: Calendar::date_add (src/builtins/core/calendar.rs)
//   temporal_rs delegates to icu4x: AnyCalendar::add -> ArithmeticDate::added (components/calendar/src/calendar_arithmetic.rs)
//   ICU4C has no equivalent: we use ucal_add(UCAL_EXTENDED_YEAR/UCAL_MONTH) with month-code re-resolution for lunisolar.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
TemporalResult<ISO8601::PlainDate> calendarDateAdd(CalendarID calendarId, const ISO8601::PlainDate& isoDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    // Step 1: iso8601 → BalanceISOYearMonth + RegulateISODate + AddDaysToISODate.
    if (calendarUsesISODateArithmetic(calendarId))
        return isoDateAdd(isoDate, duration, overflow);
    // Fast path: day/week-only durations are calendar-independent.
    if (!duration.years() && !duration.months())
        return isoDateAdd(isoDate, duration, overflow);
    // Fixed solar calendars construct the exact expected native fields directly.
    if (calendarIsNonISOSolar(calendarId))
        return fixedSolarDateAdd(calendarId, isoDate, duration, overflow);

    // ucal_add takes int32_t; larger components exceed Temporal's representable range.
    auto fitsInt32 = [](int64_t v) -> bool {
        return v >= INT32_MIN && v <= INT32_MAX;
    };
    int64_t totalDays64 = duration.days() + 7LL * duration.weeks();
    if (!fitsInt32(duration.years()) || !fitsInt32(duration.months()) || !fitsInt32(totalDays64)) [[unlikely]]
        return makeUnexpected(rangeError("duration is out of the representable range"_s));

    // Step 2: non-ISO → NonISODateAdd. https://tc39.es/proposal-intl-era-monthcode/#sup-temporal-nonisodateadd
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::PlainDate> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, isoDate)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        // ICU4C-WORKAROUND: rdar://182958553 - y0 Kislev D30 sits at ICU's Tevet D1; step back
        // one day so ucal_add(MONTH) advances from Kislev, and treat the snapshot as M03 D30.
        bool hebrewY0Kislev = isHebrewYear0ExtraKislevDay(cal, calendarId);
        if (hebrewY0Kislev) {
            UErrorCode s = U_ZERO_ERROR;
            ucal_add(cal, UCAL_DAY_OF_MONTH, -1, &s);
            if (U_FAILURE(s)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        }

        // Snapshot source monthCode + day; icu4x preserves both across year-add.
        UErrorCode status = U_ZERO_ERROR;
        auto origMonthCodeOpt = getMonthCode(cal, calendarId);
        if (!origMonthCodeOpt) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        String origMonthCode = hebrewY0Kislev ? String("M03"_s) : WTF::move(*origMonthCodeOpt);
        int32_t originalDay = hebrewY0Kislev ? 30 : ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
        if (!hebrewY0Kislev && U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuReadCalendarFailed));

        // Add years. Lunisolar: re-resolve original monthCode in the new year (= ConstrainMonthCode).
        if (duration.years()) {
            ucal_add(cal, UCAL_EXTENDED_YEAR, clampTo<int32_t>(duration.years()), &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            if (calendarIsLunisolar(calendarId)) {
                auto foundState = setCalendarToMonthCode(cal, calendarId, origMonthCode);
                if (!foundState) [[unlikely]]
                    return makeUnexpected(rangeError("Failed to resolve month code after year addition"_s));
                if (!foundState.value() && overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("month code does not exist in the target year (overflow: reject)"_s));
            }
        }

        // Add months. ICU balances year rollover implicitly.
        if (duration.months()) {
            ucal_add(cal, UCAL_MONTH, clampTo<int32_t>(duration.months()), &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            if (!forceICUFieldReresolution(cal)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        }

        // Regulate day to the new month's max. ucal_add already clamps on constrain; for reject,
        // detect and throw. Do NOT ucal_set(DAY_OF_MONTH) after ucal_add — corrupts lunisolar cursor.
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

        // Add totalDays (= days + 7 * weeks).
        int32_t totalDays = static_cast<int32_t>(totalDays64);
        if (totalDays) {
            ucal_add(cal, UCAL_DAY_OF_MONTH, totalDays, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        }

        // Read result + range check (skip Reject re-validation: cursor is valid by construction).
        auto result = calendarIntegersToISO(cal, calendarId, std::nullopt, 0, TemporalOverflow::Constrain);
        if (!result) [[unlikely]]
            return makeUnexpected(result.error());
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
    if (calendarIsNonISOSolar(calendarId)) {
        auto yearField = calendarArithmeticYearField(calendarId);
        int32_t sourceYear = ucal_get(trialCal, yearField, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        int32_t sourceMonth = ucal_get(trialCal, UCAL_MONTH, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        auto expected = balanceFixedSolarYearMonth(sourceYear, sourceMonth, fixedSolarMonthsInYear(calendarId), 0, sign);
        if (!expected)
            return true;
        auto advancedExactly = setFixedSolarCalendarToYearMonth(trialCal, calendarId, *expected);
        if (!advancedExactly) [[unlikely]]
            return std::nullopt;
        if (!*advancedExactly)
            return true;
    } else {
        ucal_add(trialCal, UCAL_MONTH, sign, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
    }
    auto yearField = calendarArithmeticYearField(calendarId);
    int32_t trialYear = ucal_get(trialCal, yearField, &status);
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
    // Read it directly. computeFieldResolutionOrdinalMonth mutates trialCal (walks from year-start
    // for lunisolar). save+restore epoch ms preserves the trial position for the next loop iteration.
    double savedMs = ucal_getMillis(trialCal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    auto trialOrdinal = computeFieldResolutionOrdinalMonth(trialCal, calendarId);
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
//   ICU4C has no equivalent: fixed-solar calendars balance native fields directly; lunisolar calendars walk months with ucal_add.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil
TemporalResult<ISO8601::Duration> calendarDateUntil(CalendarID calendarId, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    ASSERT(largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month || largestUnit == TemporalUnit::Week || largestUnit == TemporalUnit::Day);

    // Step 3 (iso8601 inline algorithm): ISODateSurpasses-based diff.
    if (calendarUsesISODateArithmetic(calendarId))
        return diffISODate(one, two, largestUnit);
    // Fast path: day/week diff is calendar-independent regardless of largestUnit.
    if (largestUnit == TemporalUnit::Day || largestUnit == TemporalUnit::Week)
        return diffISODate(one, two, largestUnit);

    // Step 4 (non-ISO tail return): `Return NonISODateUntil(calendar, one, two, largestUnit)`.
    // https://tc39.es/proposal-intl-era-monthcode/#sup-temporal-nonisodateuntil

    // Snapshot source (one) and target (two) fields — separate withCalendar calls for minimal lock scope.
    struct DateSnapshot {
        double epochMs { 0 };
        int32_t year { 0 };
        String monthCode;
        int32_t day { 0 };
        int32_t ordinalMonth { 0 };
    };
    auto snapshotFields = [&](const ISO8601::PlainDate& date) -> TemporalResult<DateSnapshot> {
        return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<DateSnapshot> {
            if (!cal) [[unlikely]]
                return makeUnexpected(rangeError(icuOpenCalendarFailed));
            if (!setCalendarToISODate(cal, date)) [[unlikely]]
                return makeUnexpected(rangeError(icuSetCalendarFailed));
            UErrorCode status = U_ZERO_ERROR;
            DateSnapshot snapshot;
            snapshot.epochMs = ucal_getMillis(cal, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            // ICU4C-WORKAROUND: rdar://182958553 - relabel ICU's Tevet D1 as Kislev D30 for diff snapshots.
            bool hebrewY0Kislev = isHebrewYear0ExtraKislevDay(cal, calendarId);
            snapshot.year = ucal_get(cal, calendarArithmeticYearField(calendarId), &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            if (hebrewY0Kislev) {
                snapshot.monthCode = String("M03"_s);
                snapshot.day = 30;
                snapshot.ordinalMonth = 3;
                return snapshot;
            }
            auto monthCodeOpt = getMonthCode(cal, calendarId);
            if (!monthCodeOpt) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            snapshot.monthCode = WTF::move(*monthCodeOpt);
            snapshot.day = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            auto ordinalMonthOpt = computeFieldResolutionOrdinalMonth(cal, calendarId);
            if (!ordinalMonthOpt) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            snapshot.ordinalMonth = *ordinalMonthOpt;
            return snapshot;
        });
    };
    auto targetOrError = snapshotFields(two);
    if (!targetOrError) [[unlikely]]
        return makeUnexpected(targetOrError.error());
    auto& target = *targetOrError;

    auto sourceOrError = snapshotFields(one);
    if (!sourceOrError) [[unlikely]]
        return makeUnexpected(sourceOrError.error());
    auto& source = *sourceOrError;

    // CalendarDateUntil Steps 1-2 (deferred from the top): sign compute + zero-return.
    // +1 (one < two), -1 (one > two), 0 (equal → zero duration).
    int32_t sign;
    if (source.epochMs < target.epochMs)
        sign = 1;
    else if (source.epochMs > target.epochMs)
        sign = -1;
    else
        return ISO8601::Duration { };

    // Fixed-solar calendars have a constant month count, so jump directly to the native
    // total-month difference. At most one candidate can surpass because it is already in
    // the target year/month; preserve the existing unregulated source-day comparison.
    if (calendarIsNonISOSolar(calendarId) && largestUnit == TemporalUnit::Month) {
        CheckedInt64 checkedMonths = (CheckedInt64(target.year) - source.year) * fixedSolarMonthsInYear(calendarId);
        checkedMonths += target.ordinalMonth - source.ordinalMonth;
        if (checkedMonths.hasOverflowed()) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        int64_t months = checkedMonths;
        if (compareSurpassesOrdinally(sign, target.year, target.ordinalMonth, source.day, target.year, target.ordinalMonth, target.day))
            months -= sign;

        ISO8601::Duration monthDuration;
        monthDuration.setMonths(months);
        auto intermediate = calendarDateAdd(calendarId, one, monthDuration, TemporalOverflow::Constrain);
        if (!intermediate) [[unlikely]]
            return makeUnexpected(intermediate.error());
        auto remainder = diffISODate(*intermediate, two, TemporalUnit::Day);
        return ISO8601::Duration { 0, months, 0, remainder.days(), 0, 0, 0, 0, Int128(0), Int128(0) };
    }

    // icu4x optimization: pre-guess year delta that doesn't surpass, to skip ahead in the loop.
    int32_t yearDiff = target.year - source.year;
    int32_t minYears = !yearDiff ? 0 : yearDiff - sign;

    // largestUnit is Year or Month (Day/Week short-circuited above).
    ASSERT(largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month);

    int32_t years = 0;
    int32_t months = 0;

    // Count full years by fast-forward + surpass probe.
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

    // Month-loop start: (one + years) via calendarDateAdd (preserves monthCode).
    ISO8601::PlainDate monthLoopStart = one;
    if (years) {
        ISO8601::Duration yearDur;
        yearDur.setYears(static_cast<int64_t>(years));
        auto advanced = calendarDateAdd(calendarId, one, yearDur, TemporalOverflow::Constrain);
        if (!advanced) [[unlikely]]
            return makeUnexpected(advanced.error());
        monthLoopStart = *advanced;
    }

    // Month iteration on the cached calendar.
    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::Duration> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        if (!setCalendarToISODate(cal, monthLoopStart)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        UErrorCode status = U_ZERO_ERROR;
        // Lunisolar month counts vary. Fixed-solar calendars reach this loop only for
        // largestUnit year, so direct construction is bounded to at most one native year.
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

        // Restore cal to (one + years), then apply total months without undoing trial steps.
        ucal_setMillis(cal, startMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));
        if (months) {
            if (calendarIsNonISOSolar(calendarId)) {
                int32_t sourceYear = ucal_get(cal, calendarArithmeticYearField(calendarId), &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                int32_t sourceMonth = ucal_get(cal, UCAL_MONTH, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                auto expected = balanceFixedSolarYearMonth(sourceYear, sourceMonth, fixedSolarMonthsInYear(calendarId), 0, months);
                if (!expected) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                auto advancedExactly = setFixedSolarCalendarToYearMonth(cal, calendarId, *expected);
                if (!advancedExactly || !*advancedExactly) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            } else {
                ucal_add(cal, UCAL_MONTH, months, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
                if (!forceICUFieldReresolution(cal)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            }
        }
        // Regulated day = min(sourceDay, end_of_month).
        if (!setMonths(cal, source.day)) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));

        // Days: derived from epoch-ms delta between cal (source + years + months) and target.
        const double msPerDay = 86'400'000.0;
        double finalMs1 = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        double daysDiff = std::trunc((target.epochMs - finalMs1) / msPerDay);

        // If largestUnit is Month, clear years (months were counted from one + years=0).
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


// ecmaReferenceYear — no spec AO, no ICU4C equivalent.
// Ported line-for-line from icu4x: ecma_reference_year (components/calendar/src/cal/{east_asian_traditional,hijri,hebrew,coptic,persian,indian}.rs).
// Returns the extended calendar year for (monthNumber, isLeapMonth, day), or an EcmaReferenceYearError.
Expected<int32_t, EcmaReferenceYearError> ecmaReferenceYear(CalendarID calendarId, uint8_t monthNumber, bool isLeapMonth, uint8_t day)
{
    bool bigDay = day > 29;

    if (calendarId == chineseCalendarID() || calendarId == dangiCalendarID()) {
        // icu4x east_asian_traditional.rs:ecma_reference_year_common.
        if (monthNumber < 1 || monthNumber > 12)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
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
                // Check bigDay before day>26 — both can be true.
                if (bigDay)
                    return 1969;
                return (day > 26) ? 1971 : 1972;
            case 12:
                return 1971;
            }
            ASSERT_NOT_REACHED();
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        }
        // Leap months. UseRegularIfConstrain: caller retries non-leap variant on Constrain, throws on Reject.
        switch (monthNumber) {
        case 1:
            return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
        case 2:
            if (bigDay)
                return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
            return 1947;
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
            if (bigDay)
                return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
            return 1957;
        case 9:
            if (bigDay)
                return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
            return 2014;
        case 10:
            if (bigDay)
                return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
            return 1984;
        case 11:
            if (bigDay)
                return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
            return 2033;
        case 12:
            return makeUnexpected(EcmaReferenceYearError::UseRegularIfConstrain);
        }
        ASSERT_NOT_REACHED();
        return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
    }

    if (calendarIsIslamic(calendarId)) {
        // icu4x hijri.rs: no leap months; out-of-range rejects.
        if (isLeapMonth || monthNumber < 1 || monthNumber > 12)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        // TabularAlgorithm (civil = Friday epoch, day<26; tbla = Thursday epoch, day<27).
        bool isCivil = (calendarId == islamicCivilCalendarID());
        bool isTbla = (calendarId == islamicTblaCalendarID());
        if (isCivil || isTbla) {
            if (monthNumber <= 10)
                return 1392;
            if (monthNumber == 11)
                return (day < (isCivil ? 26 : 27)) ? 1392 : 1391;
            return bigDay ? 1390 : 1391; // monthNumber == 12
        }
        // UmmAlQura table.
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
        }
        ASSERT_NOT_REACHED();
        return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
    }

    if (calendarId == hebrewCalendarID()) {
        // icu4x hebrew.rs. Dec 31, 1972 = Tevet 26, 5733 AM.
        if (isLeapMonth) {
            // Only M05L (Adar I) is valid.
            if (monthNumber == 5)
                return 5730;
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        }
        if (monthNumber < 1 || monthNumber > 12)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        switch (monthNumber) {
        case 1:
            return 5733; // Tishri
        case 2:
            return day <= 29 ? 5733 : 5732; // Cheshvan (5733 has 29 days)
        case 3:
            return day <= 29 ? 5733 : 5732; // Kislev (5733 has 29 days)
        case 4:
            return day <= 26 ? 5733 : 5732; // Tevet (Dec 31 = 4/26/5733)
        }
        return 5732; // M05-M12
    }

    if (calendarId == copticCalendarID() || calendarId == ethiopicCalendarID()) {
        // icu4x coptic.rs (Ethiopian delegates to Coptic).
        // Dec 31, 1972 = Koiak 22, 1689 AM. Ethiopic year = Coptic + 276.
        if (isLeapMonth)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        int32_t copticYear;
        if (monthNumber < 4 || (monthNumber == 4 && day <= 22))
            copticYear = 1689;
        else if (monthNumber == 13 && day >= 6)
            copticYear = 1687; // Coptic leap year
        else
            copticYear = 1688;
        return (calendarId == ethiopicCalendarID()) ? copticYear + 276 : copticYear;
    }

    if (calendarId == ethioaaCalendarID()) {
        // Amete Alem: same structure as Ethiopic, offset from Coptic = 5776.
        if (isLeapMonth)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        if (monthNumber < 4 || (monthNumber == 4 && day <= 22))
            return 7465;
        if (monthNumber == 13 && day >= 6)
            return 7463; // leap year
        return 7464;
    }

    if (calendarId == persianCalendarID()) {
        // icu4x persian.rs. Dec 31, 1972 = Dey 10, 1351 AP.
        if (isLeapMonth)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        if (monthNumber < 10 || (monthNumber == 10 && day <= 10))
            return 1351;
        return 1350; // leap year
    }

    if (calendarId == indianCalendarID()) {
        // icu4x indian.rs. Dec 31, 1972 = 10th month day 10, 1894 Shaka.
        if (isLeapMonth)
            return makeUnexpected(EcmaReferenceYearError::MonthNotInCalendar);
        if (monthNumber < 10 || (monthNumber == 10 && day <= 10))
            return 1894;
        return 1893;
    }

    if (calendarId == buddhistCalendarID())
        return 2515; // BE 2515 = Gregorian 1972 (leap)

    if (calendarId == rocCalendarID())
        return 61; // ROC year 61 = ISO 1972

    // Japanese/Gregory/ISO: UCAL_EXTENDED_YEAR == Gregorian year.
    return 1972;
}

// Sets ICU4C's UCAL_ERA/UCAL_YEAR/UCAL_EXTENDED_YEAR depending on which field the calendar
// treats as authoritative for arithmetic year. ROC/Buddhist/Ethioaa need UCAL_ERA+UCAL_YEAR;
// everything else takes the extended year directly.
static void setICUCalendarYear(UCalendar* cal, CalendarID calendarId, std::optional<int32_t> year)
{
    if (calendarId == rocCalendarID()) {
        // ROC: year > 0 → roc era (1); year <= 0 → broc era (0) with eraYear = 1 - year.
        int32_t y = year.value_or(0);
        if (y <= 0) {
            ucal_set(cal, UCAL_ERA, 0); // broc
            ucal_set(cal, UCAL_YEAR, 1 - y);
        } else {
            ucal_set(cal, UCAL_ERA, 1); // roc
            ucal_set(cal, UCAL_YEAR, y);
        }
        return;
    }
    if (calendarId == buddhistCalendarID() || calendarId == ethioaaCalendarID()) {
        // These one-era calendars use calendar-native UCAL_YEAR for arithmetic.
        ucal_set(cal, UCAL_ERA, 0);
        ucal_set(cal, UCAL_YEAR, year.value_or(0));
        return;
    }
    ucal_set(cal, UCAL_EXTENDED_YEAR, year.value_or(0));
}

static TemporalResult<void> positionCursorAtConstrainedMonthCode(UCalendar* cal, CalendarID calendarId, const ParsedMonthCode& monthCode, TemporalOverflow overflow)
{
    if (!isValidMonthCodeForCalendar(calendarId, monthCode)) [[unlikely]]
        return makeUnexpected(rangeError("monthCode is not valid for this calendar"_s));

    auto constrained = constrainMonthCodeInternal(cal, calendarId, monthCode, overflow);
    if (!constrained) [[unlikely]]
        return makeUnexpected(constrained.error());

    // Position the cursor at the constrained monthCode.
    UErrorCode status = U_ZERO_ERROR;
    if (calendarIsLunisolar(calendarId)) {
        String targetCode = makeString("M"_s, constrained->monthNumber < 10 ? "0"_s : ""_s,
            constrained->monthNumber, constrained->isLeapMonth ? "L"_s : ""_s);
        auto probe = setCalendarToMonthCode(cal, calendarId, targetCode);
        if (!probe) [[unlikely]]
            return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
        return { };
    }
    // Non-lunisolar: direct set (M13 for coptic/ethiopic/ethioaa; M01..M12 otherwise).
    ucal_set(cal, UCAL_MONTH, constrained->monthNumber - 1);
    if (constrained->isLeapMonth)
        ucal_set(cal, UCAL_IS_LEAP_MONTH, 1);
    ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
    ucal_getMillis(cal, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
    return { };
}

// Detects implicit clamps when overflow=Reject (ICU helpers above always constrain).
static TemporalResult<void> validateRejectMode(UCalendar* cal, CalendarID calendarId, std::optional<ParsedMonthCode> monthCode, uint8_t day)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t resolvedDay = ucal_get(cal, UCAL_DAY_OF_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError("Failed to resolve calendar date"_s));
    if (!monthCode)
        return { };

    if (calendarIsLunisolar(calendarId)) {
        // Lunisolar: ICU slot numbers != monthCode numbers (e.g. Hebrew M05L at slot 5
        // gives UCAL_MONTH+1=6, and ICU4C never sets IS_LEAP_MONTH). constrainMonthCode
        // already positioned the calendar at the target month; verify only the day.
        // ICU4C-WORKAROUND: rdar://182958553 - y0 Kislev D30 rolls to ICU's M04 D1 (see maxDay override).
        bool hebrewYear0KislevD30 = calendarId == hebrewCalendarID()
            && monthCode->monthNumber == 3 && !monthCode->isLeapMonth && day == 30 && isHebrewYear0ExtraKislevDay(cal, calendarId);
        if (!hebrewYear0KislevD30 && resolvedDay != static_cast<int32_t>(day)) [[unlikely]]
            return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
        return { };
    }

    int32_t resolvedMonth = ucal_get(cal, UCAL_MONTH, &status) + 1;
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    int32_t resolvedLeap = ucal_get(cal, UCAL_IS_LEAP_MONTH, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return makeUnexpected(rangeError(icuReadCalendarFailed));
    bool leapMismatch = monthCode->isLeapMonth && !resolvedLeap;
    if (resolvedDay != static_cast<int32_t>(day) || resolvedMonth != static_cast<int32_t>(monthCode->monthNumber) || leapMismatch) [[unlikely]]
        return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
    return { };
}

// https://tc39.es/proposal-intl-era-monthcode/#sup-temporal-nonisocalendardatetoiso
TemporalResult<ISO8601::PlainDate> nonISOCalendarDateToISO(CalendarID calendarId, std::optional<int32_t> year, uint8_t month, uint8_t day, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow)
{
    if (year && calendarUsesISOFallbackForExtremeYear(calendarId, *year)) {
        int32_t isoYear = *year;
        if (month > 12) {
            if (overflow == TemporalOverflow::Reject) [[unlikely]]
                return makeUnexpected(rangeError("month is out of range"_s));
        }
        uint8_t isoMonth = std::clamp<uint8_t>(month, 1, 12);
        uint8_t maxDay = ISO8601::daysInMonth(isoYear, isoMonth);
        if (day > maxDay) {
            if (overflow == TemporalOverflow::Reject) [[unlikely]]
                return makeUnexpected(rangeError("Day is out of range for the given month"_s));
        }
        uint8_t isoDay = std::clamp<uint8_t>(day, 1, maxDay);
        return ISO8601::PlainDate(isoYear, isoMonth, isoDay);
    }

    {
        // ICU4C-WORKAROUND: rdar://182960658 - IndianCalendar clamped arithmetic (see helper).
        if (calendarId == indianCalendarID() && year) {
            uint8_t sakaMonth;
            if (monthCode) {
                if (monthCode->isLeapMonth) [[unlikely]]
                    return makeUnexpected(rangeError("Leap month codes are not valid for this calendar"_s));
                if (monthCode->monthNumber > 12) [[unlikely]]
                    return makeUnexpected(rangeError("month is out of range"_s));
                sakaMonth = static_cast<uint8_t>(monthCode->monthNumber);
            } else {
                if (month > 12) {
                    if (overflow == TemporalOverflow::Reject) [[unlikely]]
                        return makeUnexpected(rangeError("month is out of range"_s));
                }
                sakaMonth = std::clamp<uint8_t>(month, 1, 12);
            }
            uint8_t monthLen = indianSakaDaysInMonth(*year, sakaMonth);
            uint8_t sakaDay = day;
            if (day > monthLen) {
                if (overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("Day is out of range for the given month"_s));
                sakaDay = monthLen;
            }
            auto isoDate = indianSakaToISO(*year, sakaMonth, sakaDay);
            if (!isoDate) [[unlikely]]
                return makeUnexpected(rangeError("Resolved calendar date is outside representable range"_s));
            if (!ISO8601::isYearWithinLimits(isoDate->year())) [[unlikely]]
                return makeUnexpected(rangeError("Resolved calendar date is outside representable range"_s));
            return *isoDate;
        }
    }

    {
        // ICU4C-WORKAROUND: rdar://182963532 - ucal_setGregorianChange returns U_UNSUPPORTED_ERROR
        // for buddhist/roc/japanese derived calendars; can't set proleptic mode via API, so bypass ICU for pre-1582.
        if (calendarIsGregorianStructured(calendarId)) {
            if (monthCode && monthCode->monthNumber > 12) [[unlikely]]
                return makeUnexpected(rangeError("month is out of range"_s));
            ASSERT(year);
            int32_t isoYear = gregorianStructuredCalendarISOYear(calendarId, *year);
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

    // Step 1: Year/Month/Day asserted by caller (month=0 permitted only when monthCode supplied).
    ASSERT(day >= 1);
    ASSERT(monthCode || month >= 1);

    return withCalendar(calendarId, [&](UCalendar* cal) -> TemporalResult<ISO8601::PlainDate> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenCalendarFailed));
        UErrorCode status = U_ZERO_ERROR;

        setICUCalendarYear(cal, calendarId, year);

        bool isChineseBased = calendarId == chineseCalendarID() || calendarId == dangiCalendarID();
        if (monthCode) {
            // Step 2: ConstrainMonthCode + position ICU cursor for downstream day/millis reads.
            if (calendarIsLunisolar(calendarId)) {
                // Lunisolar: walk months from start of year to find target monthCode.
                // ICU4X uses precomputed year.packed.leap_month() for O(1) lookup; ICU4C
                // doesn't expose this data, so we walk. The walk is correct and safe.
                if (!isValidMonthCodeForCalendar(calendarId, *monthCode)) [[unlikely]]
                    return makeUnexpected(rangeError("monthCode is not valid for this calendar"_s));
                if (!setCalendarToLunisolarYearStart(cal, calendarId, year, status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));

                String targetCode = makeString("M"_s, monthCode->monthNumber < 10 ? "0"_s : ""_s,
                    monthCode->monthNumber, monthCode->isLeapMonth ? "L"_s : ""_s);
                int32_t savedYear = isChineseBased ? 0 : ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                double previousMonthMs = ucal_getMillis(cal, &status);
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
                                ucal_setMillis(cal, previousMonthMs, &status);
                                if (U_FAILURE(status)) [[unlikely]]
                                    return makeUnexpected(rangeError(icuSetCalendarFailed));
                                found = true;
                            }
                        }
                        break;
                    }
                    previousMonthMs = ucal_getMillis(cal, &status);
                    if (U_FAILURE(status)) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    auto advanceResult = advanceToNextLunisolarMonth(cal, calendarId, status);
                    if (advanceResult == LunisolarMonthAdvanceResult::Error) [[unlikely]]
                        return makeUnexpected(rangeError(U_FAILURE(status) ? icuReadCalendarFailed : icuCalendarArithmeticFailed));
                    int32_t curYear = isChineseBased ? savedYear : ucal_get(cal, UCAL_EXTENDED_YEAR, &status);
                    auto nextCode = isChineseBased ? getMonthCode(cal, calendarId) : std::optional<String> { };
                    if (U_FAILURE(status) || (isChineseBased && !nextCode)) [[unlikely]]
                        return makeUnexpected(rangeError(icuReadCalendarFailed));
                    if ((isChineseBased && *nextCode == "M01"_s) || (!isChineseBased && curYear != savedYear)) {
                        ucal_setMillis(cal, previousMonthMs, &status);
                        if (U_FAILURE(status)) [[unlikely]]
                            return makeUnexpected(rangeError(icuSetCalendarFailed));
                        break;
                    }
                }
                if (!found && overflow == TemporalOverflow::Reject) [[unlikely]]
                    return makeUnexpected(rangeError("monthCode does not exist in this calendar year"_s));
            } else {
                // Non-lunisolar with monthCode (Gregorian-based calendars).
                if (auto r = positionCursorAtConstrainedMonthCode(cal, calendarId, *monthCode, overflow); !r)
                    return makeUnexpected(r.error());
            }
        } else {
            // Step 3: CalendarMonthsInYear.
            uint8_t resolvedMonth;
            if (calendarIsLunisolar(calendarId)) {
                // For lunisolar calendars, 'month' is the ordinal month (1-indexed).
                // Count monthsInYear using a separate calendar to avoid state corruption.
                if (!setCalendarToLunisolarYearStart(cal, calendarId, year, status)) [[unlikely]]
                    return makeUnexpected(rangeError("Failed to resolve lunisolar calendar"_s));
                // Count months in year by walking cal forward, then reset to year start.
                double calMs = ucal_getMillis(cal, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));
                auto monthsInYearOrError = walkLunisolarMonthsFromYearStart(cal, calendarId, "Failed to resolve lunisolar calendar"_s);
                if (!monthsInYearOrError) [[unlikely]]
                    return makeUnexpected(monthsInYearOrError.error());
                int32_t monthsInYear = *monthsInYearOrError;
                // Reset to year start before advancing to target month.
                ucal_setMillis(cal, calMs, &status);
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuSetCalendarFailed));

                // Clamp or reject month against monthsInYear.
                resolvedMonth = month;
                if (month > monthsInYear) {
                    if (overflow == TemporalOverflow::Reject) [[unlikely]]
                        return makeUnexpected(rangeError("month is out of range for this calendar year"_s));
                    resolvedMonth = static_cast<uint8_t>(monthsInYear);
                }

                // Advance the original calendar to the target month.
                for (uint8_t ordinal = 1; ordinal < resolvedMonth; ++ordinal) {
                    auto advanceResult = advanceToNextLunisolarMonth(cal, calendarId, status);
                    if (advanceResult == LunisolarMonthAdvanceResult::Error) [[unlikely]]
                        return makeUnexpected(rangeError(U_FAILURE(status) ? icuReadCalendarFailed : icuCalendarArithmeticFailed));
                }
            } else {
                // CalendarMonthsInYear: cursor is already at year start, so read [[MonthsInYear]]
                // straight off ICU instead of round-tripping through an ISO date.
                int32_t monthsInYear = ucal_getLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM, &status) + 1;
                if (U_FAILURE(status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuReadCalendarFailed));

                // Steps 4-5: clamp month to monthsInYear (reject on out-of-range).
                if (month > monthsInYear) {
                    if (overflow == TemporalOverflow::Reject) [[unlikely]]
                        return makeUnexpected(rangeError("month is out of range for this calendar year"_s));
                    resolvedMonth = static_cast<uint8_t>(monthsInYear);
                } else
                    resolvedMonth = month;

                ucal_set(cal, UCAL_MONTH, resolvedMonth - 1);
                ucal_set(cal, UCAL_DAY_OF_MONTH, 1);
            }
        }

        // Step 6: CalendarDaysInMonth.
        int32_t maxDay;
        if (calendarIsLunisolar(calendarId)) {
            auto maxDayOrError = actualLunisolarMonthLength(cal, calendarId);
            if (!maxDayOrError) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
            maxDay = *maxDayOrError;
        } else {
            // CalendarDaysInMonth: cursor is already at the target month, so read [[DaysInMonth]]
            // straight off ICU instead of round-tripping through an ISO date.
            maxDay = ucal_getLimit(cal, UCAL_DAY_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuReadCalendarFailed));
        }

        {
            // ICU4C-WORKAROUND: rdar://182958553 - force maxDay = 30 for Hebrew y0 Kislev.
            if (isHebrewYear0Kislev(cal, calendarId))
                maxDay = 30;
        }

        // Steps 7-8: clamp day to daysInMonth (reject on out-of-range).
        uint8_t resolvedDay;
        if (day > maxDay) {
            if (overflow == TemporalOverflow::Reject) [[unlikely]]
                return makeUnexpected(rangeError("Day is out of range for the given month in this calendar"_s));
            resolvedDay = static_cast<uint8_t>(maxDay);
        } else
            resolvedDay = day;

        // Advance cursor to resolvedDay.
        if (calendarIsLunisolar(calendarId)) {
            if (resolvedDay > 1) {
                if (!addLunisolarCalendarDays(cal, calendarId, resolvedDay - 1, status)) [[unlikely]]
                    return makeUnexpected(rangeError(icuCalendarArithmeticFailed));
            }
        } else
            ucal_set(cal, UCAL_DAY_OF_MONTH, resolvedDay);

        // Step 9: CalendarIntegersToISO (runs step-1 Reject-mode validity check internally).
        auto resolved = calendarIntegersToISO(cal, calendarId, monthCode, day, overflow);
        if (!resolved) [[unlikely]]
            return makeUnexpected(resolved.error());

        return *resolved;
    });
}

} // namespace TemporalCore
} // namespace JSC
