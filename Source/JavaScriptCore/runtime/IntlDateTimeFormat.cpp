/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "IntlDateTimeFormat.h"

#include "ISO8601.h"
#include "IntlCache.h"
#include "IntlObjectInlines.h"
#include "IntlPartObject.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSDateMath.h"
#include "ObjectConstructor.h"
#include "TemporalInstant.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include <unicode/ucal.h>
#include <unicode/udatpg.h>
#include <unicode/uenum.h>
#include <wtf/Range.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#include <unicode/uformattedvalue.h>
#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#include <unicode/udateintervalformat.h>
#define U_HIDE_DRAFT_API 1

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

// We do not use ICUDeleter<udtitvfmt_close> because we do not want to include udateintervalformat.h in IntlDateTimeFormat.h.
// udateintervalformat.h needs to be included with #undef U_HIDE_DRAFT_API, and we would like to minimize this effect in IntlDateTimeFormat.cpp.
void UDateIntervalFormatDeleter::operator()(UDateIntervalFormat* formatter)
{
    if (formatter)
        udtitvfmt_close(formatter);
}

void UFormattedDateIntervalDeleter::operator()(UFormattedDateInterval* result)
{
    if (result)
        udtitvfmt_closeResult(result);
}

const ClassInfo IntlDateTimeFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormat) };

WTF_MAKE_TZONE_ALLOCATED_IMPL(IntlDateTimeFormatImpl);
WTF_MAKE_TZONE_ALLOCATED_IMPL(IntlDateTimeFormatTemporalFormatterCache);

// Approximate sizes of ICU objects for GC memory pressure reporting, measured empirically with udat_open + udat_format.
static constexpr size_t estimatedUDateFormatSize = 30000;
static constexpr size_t estimatedUDateIntervalFormatSize = 30000;

namespace IntlDateTimeFormatInternal {
static constexpr bool verbose = false;
}

static std::unique_ptr<UDateFormat, UDateFormatDeleter> openDateFormat(const CString& dataLocale, const String& timeZone, std::span<const char16_t> pattern, UErrorCode& status)
{
    auto timeZoneView = StringView(timeZone).upconvertedCharacters();
    auto* dateFormat = udat_open(UDAT_PATTERN, UDAT_PATTERN, dataLocale.data(), timeZoneView.get(), timeZone.length(), pattern.data(), pattern.size(), &status);
    if (U_FAILURE(status))
        return nullptr;

    // Gregorian calendar should be used from the beginning of ECMAScript time.
    // Failure here means unsupported calendar, and can safely be ignored.
    UErrorCode calStatus = U_ZERO_ERROR;
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(dateFormat));
    ucal_setGregorianChange(cal, minECMAScriptTime, &calStatus);

    return std::unique_ptr<UDateFormat, UDateFormatDeleter>(dateFormat);
}

IntlDateTimeFormat* IntlDateTimeFormat::create(VM& vm, Structure* structure)
{
    IntlDateTimeFormat* format = new (NotNull, allocateCell<IntlDateTimeFormat>(vm)) IntlDateTimeFormat(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlDateTimeFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormat::IntlDateTimeFormat(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename Visitor>
void IntlDateTimeFormat::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    IntlDateTimeFormat* thisObject = uncheckedDowncast<IntlDateTimeFormat>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundFormat);

    // Reported per-instance even when the UDateFormat is cache-shared
    if (thisObject->m_impl && thisObject->m_impl->m_dateFormat)
        visitor.reportExtraMemoryVisited(estimatedUDateFormatSize);
    if (thisObject->m_dateIntervalFormat)
        visitor.reportExtraMemoryVisited(estimatedUDateIntervalFormatSize);
    if (auto* cache = thisObject->m_impl ? thisObject->m_impl->m_temporalFormatterCache.get() : nullptr) {
        for (auto& formatter : cache->m_formatters) {
            if (formatter)
                visitor.reportExtraMemoryVisited(estimatedUDateFormatSize);
        }
    }
}

DEFINE_VISIT_CHILDREN(IntlDateTimeFormat);

void IntlDateTimeFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

IntlDateTimeFormat::DateTimeStyle IntlDateTimeFormat::dateStyle() const { return m_impl->m_dateStyle; }
IntlDateTimeFormat::DateTimeStyle IntlDateTimeFormat::timeStyle() const { return m_impl->m_timeStyle; }
IntlDateTimeFormat::TimeZoneName IntlDateTimeFormat::timeZoneName() const { return m_impl->m_timeZoneName; }

const String& IntlDateTimeFormat::ensureCalendar() const
{
    if (m_impl->m_calendar.isNull())
        m_impl->m_calendar = defaultCalendarForLocale(m_impl->m_dataLocale);
    return m_impl->m_calendar;
}

const String& IntlDateTimeFormat::ensureNumberingSystem() const
{
    if (m_impl->m_numberingSystem.isNull())
        m_impl->m_numberingSystem = defaultNumberingSystemForLocale(m_impl->m_dataLocale);
    return m_impl->m_numberingSystem;
}

bool IntlDateTimeFormat::calendarMatchesICU(StringView temporalId, const String& icuCalId)
{
    if (temporalId == icuCalId)
        return true;
    // Normalize both IDs to their BCP47 canonical form and compare.
    // mapICUCalendarKeywordToBCP47 maps ICU -> BCP47: "gregorian"->"gregory", "ethiopic-amete-alem"->"ethioaa".
    // "islamicc" is a legacy alias for "islamic-civil" not covered by the mapping tables.
    auto normalize = [](const String& id) -> String {
        if (auto bcp47 = mapICUCalendarKeywordToBCP47(id))
            return *bcp47;
        if (id == "islamicc"_s)
            return "islamic-civil"_s;
        return id;
    };
    return normalize(temporalId.toString()) == normalize(icuCalId);
}

Vector<String> IntlDateTimeFormat::localeData(const String& locale, RelevantExtensionKey key)
{
    Vector<String> keyLocaleData;
    switch (key) {
    case RelevantExtensionKey::Ca: {
        UErrorCode status = U_ZERO_ERROR;
        auto calendars = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucal_getKeywordValuesForLocale("calendar", locale.utf8().data(), false, &status));
        ASSERT(U_SUCCESS(status));

        int32_t nameLength;
        while (const char* availableName = uenum_next(calendars.get(), &nameLength, &status)) {
            ASSERT(U_SUCCESS(status));
            String calendar = String(unsafeMakeSpan(availableName, static_cast<size_t>(nameLength)));
            // Adding "islamicc" candidate for backward compatibility.
            if (calendar == "islamic-civil"_s)
                keyLocaleData.append("islamicc"_s);

            if (auto mapped = mapICUCalendarKeywordToBCP47(calendar)) {
                // Specially allowing non BCP-47 compliant cases here, e.g. "gregorian"
                // This is fine because this function's purpose is collecting what calendar strings are accepted by IntlDateTimeFormat.
                // When "gregorian" is specified, we convert it to "gregory" to make it aligned to BCP-47. Thus we accept non BCP-47 compliant
                // calendar IDs only when we can convert it to corresponding BCP-47 compliant ID: when mapICUCalendarKeywordToBCP47 returns a mapped value.
                keyLocaleData.append(WTF::move(calendar));
                keyLocaleData.append(WTF::move(mapped.value()));
            } else {
                // Skip if the obtained calendar code is not meeting Unicode Locale Identifier's `type` definition
                // as whole ECMAScript's i18n is relying on Unicode Local Identifiers.
                if (isUnicodeLocaleIdentifierType(calendar))
                    keyLocaleData.append(WTF::move(calendar));
            }
        }
        break;
    }
    case RelevantExtensionKey::Hc:
        // Null default so we know to use 'j' in pattern.
        keyLocaleData.append(String());
        keyLocaleData.append("h11"_s);
        keyLocaleData.append("h12"_s);
        keyLocaleData.append("h23"_s);
        keyLocaleData.append("h24"_s);
        break;
    case RelevantExtensionKey::Nu:
        keyLocaleData = numberingSystemsForLocale(locale);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

template<typename Container>
static inline unsigned NODELETE skipLiteralText(const Container& container, unsigned start, unsigned length)
{
    // Skip literal text. We do not recognize '' single quote specially.
    // `'ICU''s change'` is `ICU's change` literal text, but even if we split this text into two literal texts,
    // we can anyway skip the same thing.
    // This function returns the last character index which can be considered as a literal text.
    ASSERT(length);
    ASSERT(start < length);
    ASSERT(container[start] == '\'');
    unsigned index = start;
    ++index;
    if (!(index < length))
        return length - 1;
    for (; index < length; ++index) {
        if (container[index] == '\'')
            return index;
    }
    return length - 1;
}

void IntlDateTimeFormat::setFormatsFromPattern(IntlDateTimeFormatImpl& impl, StringView pattern)
{
    // Get all symbols from the pattern, and set format fields accordingly.
    // http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
    //
    // A date pattern is a character string consisting of two types of elements:
    // 1. Pattern fields, which repeat a specific pattern character one or more times.
    //    These fields are replaced with date and time data from a calendar when formatting,
    //    or used to generate data for a calendar when parsing. Currently, A..Z and a..z are
    //    reserved for use as pattern characters (unless they are quoted, see next item).
    //    The pattern characters currently defined, and the meaning of different fields
    //    lengths for then, are listed in the Date Field Symbol Table below.
    // 2. Literal text, which is output as-is when formatting, and must closely match when
    //    parsing. Literal text can include:
    //      1. Any characters other than A..Z and a..z, including spaces and punctuation.
    //      2. Any text between single vertical quotes ('xxxx'), which may include A..Z and
    //         a..z as literal text.
    //      3. Two adjacent single vertical quotes (''), which represent a literal single quote,
    //         either inside or outside quoted text.
    unsigned length = pattern.length();
    for (unsigned i = 0; i < length; ++i) {
        auto currentCharacter = pattern[i];

        if (currentCharacter == '\'') {
            i = skipLiteralText(pattern, i, length);
            continue;
        }

        if (!isASCIIAlpha(currentCharacter))
            continue;

        unsigned count = 1;
        while (i + 1 < length && pattern[i + 1] == currentCharacter) {
            ++count;
            ++i;
        }

        switch (currentCharacter) {
        case 'G':
            if (count <= 3)
                impl.m_era = Era::Short;
            else if (count == 4)
                impl.m_era = Era::Long;
            else if (count == 5)
                impl.m_era = Era::Narrow;
            break;
        case 'y':
            if (count == 1)
                impl.m_year = Year::Numeric;
            else if (count == 2)
                impl.m_year = Year::TwoDigit;
            break;
        case 'M':
        case 'L':
            if (count == 1)
                impl.m_month = Month::Numeric;
            else if (count == 2)
                impl.m_month = Month::TwoDigit;
            else if (count == 3)
                impl.m_month = Month::Short;
            else if (count == 4)
                impl.m_month = Month::Long;
            else if (count == 5)
                impl.m_month = Month::Narrow;
            break;
        case 'E':
        case 'e':
        case 'c':
            if (count <= 3)
                impl.m_weekday = Weekday::Short;
            else if (count == 4)
                impl.m_weekday = Weekday::Long;
            else if (count == 5)
                impl.m_weekday = Weekday::Narrow;
            break;
        case 'd':
            if (count == 1)
                impl.m_day = Day::Numeric;
            else if (count == 2)
                impl.m_day = Day::TwoDigit;
            break;
        case 'a':
        case 'b':
        case 'B':
            if (count <= 3)
                impl.m_dayPeriod = DayPeriod::Short;
            else if (count == 4)
                impl.m_dayPeriod = DayPeriod::Long;
            else if (count == 5)
                impl.m_dayPeriod = DayPeriod::Narrow;
            break;
        case 'h':
        case 'H':
        case 'k':
        case 'K': {
            // Populate hourCycle from actually generated patterns. It is possible that locale or option is specifying hourCycle explicitly,
            // but the generated pattern does not include related part since the pattern does not include hours.
            // This is tested in test262/test/intl402/DateTimeFormat/prototype/resolvedOptions/hourCycle-dateStyle.js and our stress tests.
            // Example:
            //     new Intl.DateTimeFormat(`de-u-hc-h11`, {
            //         dateStyle: "full"
            //     }).resolvedOptions().hourCycle === undefined
            impl.m_hourCycle = hourCycleFromSymbol(currentCharacter);
            if (count == 1)
                impl.m_hour = Hour::Numeric;
            else if (count == 2)
                impl.m_hour = Hour::TwoDigit;
            break;
        }
        case 'm':
            if (count == 1)
                impl.m_minute = Minute::Numeric;
            else if (count == 2)
                impl.m_minute = Minute::TwoDigit;
            break;
        case 's':
            if (count == 1)
                impl.m_second = Second::Numeric;
            else if (count == 2)
                impl.m_second = Second::TwoDigit;
            break;
        case 'z':
            if (count == 1)
                impl.m_timeZoneName = TimeZoneName::Short;
            else if (count == 4)
                impl.m_timeZoneName = TimeZoneName::Long;
            break;
        case 'O':
            if (count == 1)
                impl.m_timeZoneName = TimeZoneName::ShortOffset;
            else if (count == 4)
                impl.m_timeZoneName = TimeZoneName::LongOffset;
            break;
        case 'v':
        case 'V':
            if (count == 1)
                impl.m_timeZoneName = TimeZoneName::ShortGeneric;
            else if (count == 4)
                impl.m_timeZoneName = TimeZoneName::LongGeneric;
            break;
        case 'S':
            impl.m_fractionalSecondDigits = count;
            break;
        }
    }
}

IntlDateTimeFormat::HourCycle IntlDateTimeFormat::parseHourCycle(const String& hourCycle)
{
    if (hourCycle == "h11"_s)
        return HourCycle::H11;
    if (hourCycle == "h12"_s)
        return HourCycle::H12;
    if (hourCycle == "h23"_s)
        return HourCycle::H23;
    if (hourCycle == "h24"_s)
        return HourCycle::H24;
    return HourCycle::None;
}

inline IntlDateTimeFormat::HourCycle IntlDateTimeFormat::hourCycleFromSymbol(char16_t symbol)
{
    switch (symbol) {
    case 'K':
        return HourCycle::H11;
    case 'h':
        return HourCycle::H12;
    case 'H':
        return HourCycle::H23;
    case 'k':
        return HourCycle::H24;
    }
    return HourCycle::None;
}

IntlDateTimeFormat::HourCycle IntlDateTimeFormat::hourCycleFromPattern(const Vector<char16_t, 32>& pattern)
{
    for (unsigned i = 0, length = pattern.size(); i < length; ++i) {
        auto character = pattern[i];

        if (character == '\'') {
            i = skipLiteralText(pattern, i, length);
            continue;
        }

        switch (character) {
        case 'K':
        case 'h':
        case 'H':
        case 'k':
            return hourCycleFromSymbol(character);
        }
    }
    return HourCycle::None;
}

inline void IntlDateTimeFormat::replaceHourCycleInSkeleton(Vector<char16_t, 32>& skeleton, bool isHour12)
{
    char16_t skeletonCharacter = 'H';
    if (isHour12)
        skeletonCharacter = 'h';
    for (unsigned i = 0, length = skeleton.size(); i < length; ++i) {
        auto& character = skeleton[i];

        // ICU DateTimeFormat skeleton also has single-quoted literal text.
        // https://github.com/unicode-org/icu/blob/main/icu4c/source/i18n/dtptngen.cpp
        if (character == '\'') {
            i = skipLiteralText(skeleton, i, length);
            continue;
        }

        switch (character) {
        case 'h':
        case 'H':
        case 'j':
            character = skeletonCharacter;
            break;
        }
    }
}

// Returns the ICU pattern character for the given hour cycle, or 'j' (locale default) for None.
static char16_t hourCharForCycle(IntlDateTimeFormat::HourCycle hourCycle)
{
    switch (hourCycle) {
    case IntlDateTimeFormat::HourCycle::H11:
        return 'K';
    case IntlDateTimeFormat::HourCycle::H12:
        return 'h';
    case IntlDateTimeFormat::HourCycle::H23:
        return 'H';
    case IntlDateTimeFormat::HourCycle::H24:
        return 'k';
    default:
        return 'j';
    }
}

// UTS#35 hour skeleton characters (h=h12, H=h23, k=h24, K=h11).
// Used to detect and replace hour display characters when applying the user's hourCycle.
static constexpr bool isHourChar(char16_t ch)
{
    return ch == 'h' || ch == 'H' || ch == 'k' || ch == 'K';
}

inline void IntlDateTimeFormat::replaceHourCycleInPattern(Vector<char16_t, 32>& pattern, HourCycle hourCycle)
{
    if (hourCycle == HourCycle::None)
        return;
    char16_t hourFromHourCycle = hourCharForCycle(hourCycle);

    bool isTarget24Hour = (hourCycle == HourCycle::H23 || hourCycle == HourCycle::H24);

    for (unsigned i = 0, length = pattern.size(); i < length; ++i) {
        auto& character = pattern[i];

        if (character == '\'') {
            i = skipLiteralText(pattern, i, length);
            continue;
        }

        switch (character) {
        case 'K':
        case 'h':
        case 'H':
        case 'k': {
            char16_t originalChar = character;
            character = hourFromHourCycle;
            // Pad single->double (e.g. H->HH) only when switching from a 12-hour char
            // (h/K) to a 24-hour cycle, and only if this is a lone single-char symbol.
            // getTemporalFormatter uses 'j' in the skeleton so ICU returns the locale's
            // default h12 pattern; switching h->H without padding gives "0:00:00" at midnight.
            bool isOrigin12Hour = (originalChar == 'h' || originalChar == 'K');
            bool prevIsHour = i > 0 && isHourChar(pattern[i - 1]);
            bool nextIsHour = i + 1 < length && isHourChar(pattern[i + 1]);
            if (isTarget24Hour && isOrigin12Hour && !prevIsHour && !nextIsHour) {
                pattern.insert(i + 1, hourFromHourCycle);
                length++;
                i++;
            }
            break;
        }
        }
    }
}

String IntlDateTimeFormat::buildSkeleton(Weekday weekday, Era era, Year year, Month month, Day day, TriState hour12, HourCycle hourCycle, Hour hour, DayPeriod dayPeriod, Minute minute, Second second, unsigned fractionalSecondDigits, TimeZoneName timeZoneName)
{
    StringBuilder skeletonBuilder;

    switch (weekday) {
    case Weekday::Narrow:
        skeletonBuilder.append("EEEEE"_s);
        break;
    case Weekday::Short:
        skeletonBuilder.append("EEE"_s);
        break;
    case Weekday::Long:
        skeletonBuilder.append("EEEE"_s);
        break;
    case Weekday::None:
        break;
    }

    switch (era) {
    case Era::Narrow:
        skeletonBuilder.append("GGGGG"_s);
        break;
    case Era::Short:
        skeletonBuilder.append("GGG"_s);
        break;
    case Era::Long:
        skeletonBuilder.append("GGGG"_s);
        break;
    case Era::None:
        break;
    }

    switch (year) {
    case Year::TwoDigit:
        skeletonBuilder.append("yy"_s);
        break;
    case Year::Numeric:
        skeletonBuilder.append('y');
        break;
    case Year::None:
        break;
    }

    switch (month) {
    case Month::TwoDigit:
        skeletonBuilder.append("MM"_s);
        break;
    case Month::Numeric:
        skeletonBuilder.append('M');
        break;
    case Month::Narrow:
        skeletonBuilder.append("MMMMM"_s);
        break;
    case Month::Short:
        skeletonBuilder.append("MMM"_s);
        break;
    case Month::Long:
        skeletonBuilder.append("MMMM"_s);
        break;
    case Month::None:
        break;
    }

    switch (day) {
    case Day::TwoDigit:
        skeletonBuilder.append("dd"_s);
        break;
    case Day::Numeric:
        skeletonBuilder.append('d');
        break;
    case Day::None:
        break;
    }

    {
        // Specifically, this hour-cycle / hour12 behavior is slightly different from the spec.
        // But the spec behavior is known to cause surprising behaviors, and the spec change is ongoing.
        // We implement SpiderMonkey's behavior.
        //
        //     > No option present: "j"
        //     > hour12 = true: "h"
        //     > hour12 = false: "H"
        //     > hourCycle = h11: "h", plus modifying the resolved pattern to use the hour symbol "K".
        //     > hourCycle = h12: "h", plus modifying the resolved pattern to use the hour symbol "h".
        //     > hourCycle = h23: "H", plus modifying the resolved pattern to use the hour symbol "H".
        //     > hourCycle = h24: "H", plus modifying the resolved pattern to use the hour symbol "k".
        //
        char16_t skeletonCharacter = 'j';
        if (hour12 == TriState::Indeterminate) {
            switch (hourCycle) {
            case HourCycle::None:
                break;
            case HourCycle::H11:
            case HourCycle::H12:
                skeletonCharacter = 'h';
                break;
            case HourCycle::H23:
            case HourCycle::H24:
                skeletonCharacter = 'H';
                break;
            }
        } else {
            if (hour12 == TriState::True)
                skeletonCharacter = 'h';
            else
                skeletonCharacter = 'H';
        }

        switch (hour) {
        case Hour::TwoDigit:
            skeletonBuilder.append(skeletonCharacter);
            skeletonBuilder.append(skeletonCharacter);
            break;
        case Hour::Numeric:
            skeletonBuilder.append(skeletonCharacter);
            break;
        case Hour::None:
            break;
        }
    }

    // dayPeriod must be set after setting hour.
    // https://unicode-org.atlassian.net/browse/ICU-20731
    switch (dayPeriod) {
    case DayPeriod::Narrow:
        skeletonBuilder.append("BBBBB"_s);
        break;
    case DayPeriod::Short:
        skeletonBuilder.append('B');
        break;
    case DayPeriod::Long:
        skeletonBuilder.append("BBBB"_s);
        break;
    case DayPeriod::None:
        break;
    }

    switch (minute) {
    case Minute::TwoDigit:
        skeletonBuilder.append("mm"_s);
        break;
    case Minute::Numeric:
        skeletonBuilder.append('m');
        break;
    case Minute::None:
        break;
    }

    switch (second) {
    case Second::TwoDigit:
        skeletonBuilder.append("ss"_s);
        break;
    case Second::Numeric:
        skeletonBuilder.append('s');
        break;
    case Second::None:
        break;
    }

    for (unsigned i = 0; i < fractionalSecondDigits; ++i)
        skeletonBuilder.append('S');

    switch (timeZoneName) {
    case TimeZoneName::Short:
        skeletonBuilder.append('z');
        break;
    case TimeZoneName::Long:
        skeletonBuilder.append("zzzz"_s);
        break;
    case TimeZoneName::ShortOffset:
        skeletonBuilder.append('O');
        break;
    case TimeZoneName::LongOffset:
        skeletonBuilder.append("OOOO"_s);
        break;
    case TimeZoneName::ShortGeneric:
        skeletonBuilder.append('v');
        break;
    case TimeZoneName::LongGeneric:
        skeletonBuilder.append("vvvv"_s);
        break;
    case TimeZoneName::None:
        break;
    }

    return skeletonBuilder.toString();
}

// https://tc39.es/proposal-temporal/#sec-initializedatetimeformat
void IntlDateTimeFormat::initializeDateTimeFormat(JSGlobalObject* globalObject, JSValue locales, JSValue originalOptions, RequiredComponent required, Defaults defaults, StringView toLocaleStringTimeZone)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = intlCoerceOptionsToObject(globalObject, originalOptions);
    RETURN_IF_EXCEPTION(scope, void());

    Ref<IntlDateTimeFormatImpl> impl = IntlDateTimeFormatImpl::create();

    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    String calendar = intlStringOption(globalObject, options, vm.propertyNames->calendar, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!calendar.isNull()) {
        if (!isUnicodeLocaleIdentifierType(calendar)) [[unlikely]] {
            throwRangeError(globalObject, scope, "calendar is not a well-formed calendar value"_s);
            return;
        }
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Ca)] = calendar.convertToASCIILowercase();
    }

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!numberingSystem.isNull()) {
        if (!isUnicodeLocaleIdentifierType(numberingSystem)) [[unlikely]] {
            throwRangeError(globalObject, scope, "numberingSystem is not a well-formed numbering system value"_s);
            return;
        }
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Nu)] = numberingSystem;
    }

    TriState hour12 = intlBooleanOption(globalObject, options, vm.propertyNames->hour12);
    RETURN_IF_EXCEPTION(scope, void());

    HourCycle hourCycle = intlOption<HourCycle>(globalObject, options, vm.propertyNames->hourCycle, { { "h11"_s, HourCycle::H11 }, { "h12"_s, HourCycle::H12 }, { "h23"_s, HourCycle::H23 }, { "h24"_s, HourCycle::H24 } }, "hourCycle must be \"h11\", \"h12\", \"h23\", or \"h24\""_s, HourCycle::None);
    RETURN_IF_EXCEPTION(scope, void());
    if (hour12 == TriState::Indeterminate) {
        if (hourCycle != HourCycle::None)
            localeOptions[static_cast<unsigned>(RelevantExtensionKey::Hc)] = String(hourCycleString(hourCycle));
    } else {
        // If there is hour12, hourCycle is ignored.
        // We are setting null String explicitly here (localeOptions' entries are std::optional<String>). This leads us to use HourCycle::None later.
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Hc)] = String();
    }

    const auto& availableLocales = intlDateTimeFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Ca, RelevantExtensionKey::Hc, RelevantExtensionKey::Nu }, localeData);

    impl->m_locale = resolved.locale;
    if (impl->m_locale.isEmpty()) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat due to invalid locale"_s);
        return;
    }

    {
        String calendar = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Ca)];
        if (!calendar.isNull()) {
            if (auto mapped = mapICUCalendarKeywordToBCP47(calendar))
                calendar = WTF::move(mapped.value());
            // Handling "islamicc" candidate for backward compatibility.
            if (calendar == "islamicc"_s)
                calendar = "islamic-civil"_s;
        }
        impl->m_calendar = WTF::move(calendar);
    }

    hourCycle = parseHourCycle(resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Hc)]);
    impl->m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];
    impl->m_dataLocale = resolved.dataLocale;

    StringBuilder localeBuilder;
    localeBuilder.append(impl->m_dataLocale);
    if (!impl->m_calendar.isNull() || !impl->m_numberingSystem.isNull()) {
        localeBuilder.append("-u"_s);
        if (!impl->m_calendar.isNull())
            localeBuilder.append("-ca-"_s, impl->m_calendar);
        if (!impl->m_numberingSystem.isNull())
            localeBuilder.append("-nu-"_s, impl->m_numberingSystem);
    }
    CString dataLocaleWithExtensions = localeBuilder.toString().utf8();

    JSValue tzValue = jsUndefined();
    if (options) {
        tzValue = options->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, void());
    }
    if (!toLocaleStringTimeZone.isNull()) {
        // Per spec CreateDateTimeFormat with toLocaleStringTimeZone: if the caller also set
        // options.timeZone, throw TypeError — the ZDT's timezone is always used.
        if (!tzValue.isUndefined()) [[unlikely]] {
            throwTypeError(globalObject, scope, "ZonedDateTime.toLocaleString does not accept a timeZone option; the ZonedDateTime's time zone is used"_s);
            return;
        }
        tzValue = jsString(vm, toLocaleStringTimeZone);
    }
    TimeZone tz;
    String tzForResolvedOptions;
    if (!tzValue.isUndefined()) {
        String originalTz = tzValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (auto minutesValue = ISO8601::parseUTCOffsetInMinutes(originalTz)) {
            int64_t nanoseconds = minutesValue.value() * 60LL * 1000 * 1000 * 1000;
            tz = TimeZone::fromUTCOffset(nanoseconds);
            tzForResolvedOptions = ISO8601::formatTimeZoneOffsetString(nanoseconds);
        } else if (auto resolved = intlAvailableNamedTimeZone(originalTz)) {
            tz = TimeZone::fromID(resolved->id);
            tzForResolvedOptions = resolved->identifier;
        } else {
            String message = tryMakeString("invalid time zone: "_s, originalTz);
            if (!message)
                message = "invalid time zone"_s;
            throwRangeError(globalObject, scope, message);
            return;
        }
    } else {
        tz = vm.dateCache.defaultTimeZone();
        tzForResolvedOptions = tz.toString();
    }
    impl->m_timeZone = tz;
    impl->m_timeZoneForResolvedOptions = WTF::move(tzForResolvedOptions);

    Weekday weekday = intlOption<Weekday>(globalObject, options, vm.propertyNames->weekday, { { "narrow"_s, Weekday::Narrow }, { "short"_s, Weekday::Short }, { "long"_s, Weekday::Long } }, "weekday must be \"narrow\", \"short\", or \"long\""_s, Weekday::None);
    RETURN_IF_EXCEPTION(scope, void());

    Era era = intlOption<Era>(globalObject, options, vm.propertyNames->era, { { "narrow"_s, Era::Narrow }, { "short"_s, Era::Short }, { "long"_s, Era::Long } }, "era must be \"narrow\", \"short\", or \"long\""_s, Era::None);
    RETURN_IF_EXCEPTION(scope, void());

    Year year = intlOption<Year>(globalObject, options, vm.propertyNames->year, { { "2-digit"_s, Year::TwoDigit }, { "numeric"_s, Year::Numeric } }, "year must be \"2-digit\" or \"numeric\""_s, Year::None);
    RETURN_IF_EXCEPTION(scope, void());

    Month month = intlOption<Month>(globalObject, options, vm.propertyNames->month, { { "2-digit"_s, Month::TwoDigit }, { "numeric"_s, Month::Numeric }, { "narrow"_s, Month::Narrow }, { "short"_s, Month::Short }, { "long"_s, Month::Long } }, "month must be \"2-digit\", \"numeric\", \"narrow\", \"short\", or \"long\""_s, Month::None);
    RETURN_IF_EXCEPTION(scope, void());

    Day day = intlOption<Day>(globalObject, options, vm.propertyNames->day, { { "2-digit"_s, Day::TwoDigit }, { "numeric"_s, Day::Numeric } }, "day must be \"2-digit\" or \"numeric\""_s, Day::None);
    RETURN_IF_EXCEPTION(scope, void());

    DayPeriod dayPeriod = intlOption<DayPeriod>(globalObject, options, vm.propertyNames->dayPeriod, { { "narrow"_s, DayPeriod::Narrow }, { "short"_s, DayPeriod::Short }, { "long"_s, DayPeriod::Long } }, "dayPeriod must be \"narrow\", \"short\", or \"long\""_s, DayPeriod::None);
    RETURN_IF_EXCEPTION(scope, void());

    Hour hour = intlOption<Hour>(globalObject, options, vm.propertyNames->hour, { { "2-digit"_s, Hour::TwoDigit }, { "numeric"_s, Hour::Numeric } }, "hour must be \"2-digit\" or \"numeric\""_s, Hour::None);
    RETURN_IF_EXCEPTION(scope, void());

    Minute minute = intlOption<Minute>(globalObject, options, vm.propertyNames->minute, { { "2-digit"_s, Minute::TwoDigit }, { "numeric"_s, Minute::Numeric } }, "minute must be \"2-digit\" or \"numeric\""_s, Minute::None);
    RETURN_IF_EXCEPTION(scope, void());

    Second second = intlOption<Second>(globalObject, options, vm.propertyNames->second, { { "2-digit"_s, Second::TwoDigit }, { "numeric"_s, Second::Numeric } }, "second must be \"2-digit\" or \"numeric\""_s, Second::None);
    RETURN_IF_EXCEPTION(scope, void());

    unsigned fractionalSecondDigits = intlNumberOption(globalObject, options, vm.propertyNames->fractionalSecondDigits, 1, 3, 0);
    RETURN_IF_EXCEPTION(scope, void());

    TimeZoneName timeZoneName = intlOption<TimeZoneName>(globalObject, options, vm.propertyNames->timeZoneName, { { "short"_s, TimeZoneName::Short }, { "long"_s, TimeZoneName::Long }, { "shortOffset"_s, TimeZoneName::ShortOffset }, { "longOffset"_s, TimeZoneName::LongOffset }, { "shortGeneric"_s, TimeZoneName::ShortGeneric}, { "longGeneric"_s, TimeZoneName::LongGeneric } }, "timeZoneName must be \"short\", \"long\", \"shortOffset\", \"longOffset\", \"shortGeneric\", or \"longGeneric\""_s, TimeZoneName::None);
    RETURN_IF_EXCEPTION(scope, void());

    intlStringOption(globalObject, options, vm.propertyNames->formatMatcher, { "basic"_s, "best fit"_s }, "formatMatcher must be either \"basic\" or \"best fit\""_s, "best fit"_s);
    RETURN_IF_EXCEPTION(scope, void());

    impl->m_anyPresent = (weekday != Weekday::None || year != Year::None || month != Month::None || day != Day::None || dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits);

    impl->m_dateStyle = intlOption<DateTimeStyle>(globalObject, options, vm.propertyNames->dateStyle, { { "full"_s, DateTimeStyle::Full }, { "long"_s, DateTimeStyle::Long }, { "medium"_s, DateTimeStyle::Medium }, { "short"_s, DateTimeStyle::Short } }, "dateStyle must be \"full\", \"long\", \"medium\", or \"short\""_s, DateTimeStyle::None);
    RETURN_IF_EXCEPTION(scope, void());

    impl->m_timeStyle = intlOption<DateTimeStyle>(globalObject, options, vm.propertyNames->timeStyle, { { "full"_s, DateTimeStyle::Full }, { "long"_s, DateTimeStyle::Long }, { "medium"_s, DateTimeStyle::Medium }, { "short"_s, DateTimeStyle::Short } }, "timeStyle must be \"full\", \"long\", \"medium\", or \"short\""_s, DateTimeStyle::None);
    RETURN_IF_EXCEPTION(scope, void());

    Vector<char16_t, 32> patternBuffer;
    if (impl->m_dateStyle != DateTimeStyle::None || impl->m_timeStyle != DateTimeStyle::None) {
        // 30. For each row in Table 1, except the header row, do
        //     i. Let prop be the name given in the Property column of the row.
        //     ii. Let p be opt.[[<prop>]].
        //     iii. If p is not undefined, then
        //         1. Throw a TypeError exception.
        if (weekday != Weekday::None || era != Era::None || year != Year::None || month != Month::None || day != Day::None || dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits || timeZoneName != TimeZoneName::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "dateStyle and timeStyle may not be used with other DateTimeFormat options"_s);
            return;
        }

        auto parseUDateFormatStyle = [](DateTimeStyle style) {
            switch (style) {
            case DateTimeStyle::Full:
                return UDAT_FULL;
            case DateTimeStyle::Long:
                return UDAT_LONG;
            case DateTimeStyle::Medium:
                return UDAT_MEDIUM;
            case DateTimeStyle::Short:
                return UDAT_SHORT;
            case DateTimeStyle::None:
                return UDAT_NONE;
            }
            return UDAT_NONE;
        };

        if (required == RequiredComponent::Date && impl->m_timeStyle != DateTimeStyle::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "timeStyle is specified while formatting date is requested"_s);
            return;
        }

        if (required == RequiredComponent::Time && impl->m_dateStyle != DateTimeStyle::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "dateStyle is specified while formatting time is requested"_s);
            return;
        }

        // We cannot use this UDateFormat directly yet because we need to enforce specified hourCycle.
        // First, we create UDateFormat via dateStyle and timeStyle. And then convert it to pattern string.
        // After updating this pattern string with hourCycle, we create a final UDateFormat with the updated pattern string.
        UErrorCode status = U_ZERO_ERROR;
        String timeZoneForICU = impl->m_timeZone.toICUString();
        StringView timeZoneView(timeZoneForICU);
        auto dateFormatFromStyle = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_open(parseUDateFormatStyle(impl->m_timeStyle), parseUDateFormatStyle(impl->m_dateStyle), dataLocaleWithExtensions.data(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), nullptr, -1, &status));
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
            return;
        }
        constexpr bool localized = false; // Aligned with how ICU SimpleDateTimeFormat::format works. We do not need to translate this to localized pattern.
        status = callBufferProducingFunction(udat_toPattern, dateFormatFromStyle.get(), localized, patternBuffer);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
            return;
        }

        // It is possible that timeStyle includes dayPeriod, which is sensitive to hour-cycle.
        // If dayPeriod is included, just replacing hour based on hourCycle / hour12 produces strange results.
        // Let's consider about the example. The formatted result looks like "02:12:47 PM Coordinated Universal Time"
        // If we simply replace 02 to 14, this becomes "14:12:47 PM Coordinated Universal Time", this looks strange since "PM" is unnecessary!
        //
        // If the generated pattern's hour12 does not match against the option's one, we retrieve skeleton from the pattern, enforcing hour-cycle,
        // and re-generating the best pattern from the modified skeleton. ICU will look into the generated skeleton, and pick the best format for the request.
        // We do not care about h11 vs. h12 and h23 vs. h24 difference here since this will be later adjusted by replaceHourCycleInPattern.
        //
        // test262/test/intl402/DateTimeFormat/prototype/format/timedatestyle-en.js includes the test for this behavior.
        if (impl->m_timeStyle != DateTimeStyle::None && (hourCycle != HourCycle::None || hour12 != TriState::Indeterminate)) {
            auto isHour12 = [](HourCycle hourCycle) {
                return hourCycle == HourCycle::H11 || hourCycle == HourCycle::H12;
            };
            bool specifiedHour12 = false;
            // If hour12 is specified, we prefer it and ignore hourCycle.
            if (hour12 != TriState::Indeterminate)
                specifiedHour12 = hour12 == TriState::True;
            else
                specifiedHour12 = isHour12(hourCycle);
            HourCycle extractedHourCycle = hourCycleFromPattern(patternBuffer);
            if (extractedHourCycle != HourCycle::None && isHour12(extractedHourCycle) != specifiedHour12) {
                Vector<char16_t, 32> skeleton;
                auto status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, patternBuffer.span().data(), patternBuffer.size(), skeleton);
                if (U_FAILURE(status)) [[unlikely]] {
                    throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
                    return;
                }
                replaceHourCycleInSkeleton(skeleton, specifiedHour12);
                dataLogLnIf(IntlDateTimeFormatInternal::verbose, "replaced:(", StringView { skeleton.span() }, ")");

                patternBuffer = vm.intlCache().getBestDateTimePattern(dataLocaleWithExtensions, skeleton.span(), status);
                if (U_FAILURE(status)) [[unlikely]] {
                    throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
                    return;
                }
            }
        }
    } else {
        bool needDefaults = true;
        if (required == RequiredComponent::Date || required == RequiredComponent::Any) {
            // i. For each property name prop of « "weekday", "year", "month", "day" », do
            //     1. Let value be formatOptions.[[<prop>]].
            //     2. If value is not undefined, let needDefaults be false.
            if (weekday != Weekday::None || year != Year::None || month != Month::None || day != Day::None)
                needDefaults = false;
        }

        if (required == RequiredComponent::Time || required == RequiredComponent::Any) {
            // i. For each property name prop of « "dayPeriod", "hour", "minute", "second", "fractionalSecondDigits" », do
            //     1. Let value be formatOptions.[[<prop>]].
            //     2. If value is not undefined, let needDefaults be false.
            if (dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits != 0)
                needDefaults = false;
        }

        // Save the user's skeleton before defaults are injected; computeGetDateTimeFormat
        // uses this to know exactly which fields the user explicitly set.
        String userSkeletonStr = buildSkeleton(weekday, era, year, month, day, hour12, hourCycle, hour, dayPeriod, minute, second, fractionalSecondDigits, timeZoneName);
        impl->m_userSkeleton = Vector<char16_t, 32>(StringView(userSkeletonStr).upconvertedCharacters().span());

        if (needDefaults && (defaults == Defaults::Date || defaults == Defaults::All || defaults == Defaults::ZonedDateTime)) {
            year = Year::Numeric;
            month = Month::Numeric;
            day = Day::Numeric;
        }

        if (needDefaults && (defaults == Defaults::Time || defaults == Defaults::All || defaults == Defaults::ZonedDateTime)) {
            hour = Hour::Numeric;
            minute = Minute::Numeric;
            second = Second::Numeric;
        }

        // https://tc39.es/proposal-temporal/#sec-getdatetimeformat step 17c:
        // defaults=~zoned-date-time~ → inject timeZoneName="short" if absent and needDefaults.
        // (Spec targets [[TemporalInstantFormat]]; we inject into m_dateFormat instead since
        // ZonedDateTime.toLocaleString calls format(double) which uses m_dateFormat directly.)
        if (needDefaults && defaults == Defaults::ZonedDateTime && timeZoneName == TimeZoneName::None)
            timeZoneName = TimeZoneName::Short;

        String skeleton = buildSkeleton(weekday, era, year, month, day, hour12, hourCycle, hour, dayPeriod, minute, second, fractionalSecondDigits, timeZoneName);
        UErrorCode status = U_ZERO_ERROR;
        patternBuffer = vm.intlCache().getBestDateTimePattern(dataLocaleWithExtensions, StringView(skeleton).upconvertedCharacters(), status);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
            return;
        }
    }

    // After generating pattern from skeleton, we need to change h11 vs. h12 and h23 vs. h24 if hourCycle is specified.
    if (hourCycle != HourCycle::None)
        replaceHourCycleInPattern(patternBuffer, hourCycle);

    StringView pattern(patternBuffer.span());
    setFormatsFromPattern(impl, pattern);

    dataLogLnIf(IntlDateTimeFormatInternal::verbose, "locale:(", impl->m_locale, "),dataLocale:(", dataLocaleWithExtensions, "),pattern:(", pattern, ")");

    UErrorCode status = U_ZERO_ERROR;
    String timeZoneForICU = impl->m_timeZone.toICUString();
    impl->m_dateFormat = openDateFormat(dataLocaleWithExtensions, timeZoneForICU, patternBuffer.span(), status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
        return;
    }

    vm.heap.reportExtraMemoryAllocated(this, estimatedUDateFormatSize);
    m_impl = WTF::move(impl);
}

ASCIILiteral IntlDateTimeFormat::hourCycleString(HourCycle hourCycle)
{
    switch (hourCycle) {
    case HourCycle::H11:
        return "h11"_s;
    case HourCycle::H12:
        return "h12"_s;
    case HourCycle::H23:
        return "h23"_s;
    case HourCycle::H24:
        return "h24"_s;
    case HourCycle::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::weekdayString(Weekday weekday)
{
    switch (weekday) {
    case Weekday::Narrow:
        return "narrow"_s;
    case Weekday::Short:
        return "short"_s;
    case Weekday::Long:
        return "long"_s;
    case Weekday::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::eraString(Era era)
{
    switch (era) {
    case Era::Narrow:
        return "narrow"_s;
    case Era::Short:
        return "short"_s;
    case Era::Long:
        return "long"_s;
    case Era::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::yearString(Year year)
{
    switch (year) {
    case Year::TwoDigit:
        return "2-digit"_s;
    case Year::Numeric:
        return "numeric"_s;
    case Year::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::monthString(Month month)
{
    switch (month) {
    case Month::TwoDigit:
        return "2-digit"_s;
    case Month::Numeric:
        return "numeric"_s;
    case Month::Narrow:
        return "narrow"_s;
    case Month::Short:
        return "short"_s;
    case Month::Long:
        return "long"_s;
    case Month::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::dayString(Day day)
{
    switch (day) {
    case Day::TwoDigit:
        return "2-digit"_s;
    case Day::Numeric:
        return "numeric"_s;
    case Day::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::dayPeriodString(DayPeriod dayPeriod)
{
    switch (dayPeriod) {
    case DayPeriod::Narrow:
        return "narrow"_s;
    case DayPeriod::Short:
        return "short"_s;
    case DayPeriod::Long:
        return "long"_s;
    case DayPeriod::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::hourString(Hour hour)
{
    switch (hour) {
    case Hour::TwoDigit:
        return "2-digit"_s;
    case Hour::Numeric:
        return "numeric"_s;
    case Hour::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::minuteString(Minute minute)
{
    switch (minute) {
    case Minute::TwoDigit:
        return "2-digit"_s;
    case Minute::Numeric:
        return "numeric"_s;
    case Minute::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::secondString(Second second)
{
    switch (second) {
    case Second::TwoDigit:
        return "2-digit"_s;
    case Second::Numeric:
        return "numeric"_s;
    case Second::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::timeZoneNameString(TimeZoneName timeZoneName)
{
    switch (timeZoneName) {
    case TimeZoneName::Short:
        return "short"_s;
    case TimeZoneName::Long:
        return "long"_s;
    case TimeZoneName::ShortOffset:
        return "shortOffset"_s;
    case TimeZoneName::LongOffset:
        return "longOffset"_s;
    case TimeZoneName::ShortGeneric:
        return "shortGeneric"_s;
    case TimeZoneName::LongGeneric:
        return "longGeneric"_s;
    case TimeZoneName::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDateTimeFormat::formatStyleString(DateTimeStyle style)
{
    switch (style) {
    case DateTimeStyle::Full:
        return "full"_s;
    case DateTimeStyle::Long:
        return "long"_s;
    case DateTimeStyle::Medium:
        return "medium"_s;
    case DateTimeStyle::Short:
        return "short"_s;
    case DateTimeStyle::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

// https://tc39.es/ecma402/#sec-intl.datetimeformat.prototype.resolvedoptions
JSObject* IntlDateTimeFormat::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();

    if (m_impl->m_calendar.isNull())
        m_impl->m_calendar = defaultCalendarForLocale(m_impl->m_dataLocale);
    if (m_impl->m_numberingSystem.isNull())
        m_impl->m_numberingSystem = defaultNumberingSystemForLocale(m_impl->m_dataLocale);

    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(vm, m_impl->m_locale));
    options->putDirect(vm, vm.propertyNames->calendar, jsNontrivialString(vm, m_impl->m_calendar));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsNontrivialString(vm, m_impl->m_numberingSystem));
    options->putDirect(vm, vm.propertyNames->timeZone, jsNontrivialString(vm, m_impl->m_timeZoneForResolvedOptions));

    if (m_impl->m_hourCycle != HourCycle::None) {
        options->putDirect(vm, vm.propertyNames->hourCycle, jsNontrivialString(vm, hourCycleString(m_impl->m_hourCycle)));
        options->putDirect(vm, vm.propertyNames->hour12, jsBoolean(m_impl->m_hourCycle == HourCycle::H11 || m_impl->m_hourCycle == HourCycle::H12));
    }

    if (m_impl->m_dateStyle == DateTimeStyle::None && m_impl->m_timeStyle == DateTimeStyle::None) {
        if (m_impl->m_weekday != Weekday::None)
            options->putDirect(vm, vm.propertyNames->weekday, jsNontrivialString(vm, weekdayString(m_impl->m_weekday)));

        if (m_impl->m_era != Era::None)
            options->putDirect(vm, vm.propertyNames->era, jsNontrivialString(vm, eraString(m_impl->m_era)));

        if (m_impl->m_year != Year::None)
            options->putDirect(vm, vm.propertyNames->year, jsNontrivialString(vm, yearString(m_impl->m_year)));

        if (m_impl->m_month != Month::None)
            options->putDirect(vm, vm.propertyNames->month, jsNontrivialString(vm, monthString(m_impl->m_month)));

        if (m_impl->m_day != Day::None)
            options->putDirect(vm, vm.propertyNames->day, jsNontrivialString(vm, dayString(m_impl->m_day)));

        if (m_impl->m_dayPeriod != DayPeriod::None)
            options->putDirect(vm, vm.propertyNames->dayPeriod, jsNontrivialString(vm, dayPeriodString(m_impl->m_dayPeriod)));

        if (m_impl->m_hour != Hour::None)
            options->putDirect(vm, vm.propertyNames->hour, jsNontrivialString(vm, hourString(m_impl->m_hour)));

        if (m_impl->m_minute != Minute::None)
            options->putDirect(vm, vm.propertyNames->minute, jsNontrivialString(vm, minuteString(m_impl->m_minute)));

        if (m_impl->m_second != Second::None)
            options->putDirect(vm, vm.propertyNames->second, jsNontrivialString(vm, secondString(m_impl->m_second)));

        if (m_impl->m_fractionalSecondDigits)
            options->putDirect(vm, vm.propertyNames->fractionalSecondDigits, jsNumber(m_impl->m_fractionalSecondDigits));

        if (m_impl->m_timeZoneName != TimeZoneName::None)
            options->putDirect(vm, vm.propertyNames->timeZoneName, jsNontrivialString(vm, timeZoneNameString(m_impl->m_timeZoneName)));
    } else {
        if (m_impl->m_dateStyle != DateTimeStyle::None)
            options->putDirect(vm, vm.propertyNames->dateStyle, jsNontrivialString(vm, formatStyleString(m_impl->m_dateStyle)));

        if (m_impl->m_timeStyle != DateTimeStyle::None)
            options->putDirect(vm, vm.propertyNames->timeStyle, jsNontrivialString(vm, formatStyleString(m_impl->m_timeStyle)));
    }

    return options;
}

// ICU 72 uses narrowNoBreakSpace (u202F) and thinSpace (u2009) for the output of Intl.DateTimeFormat.
// However, a lot of real world code (websites[1], Node.js modules[2] etc.) strongly assumes that this output
// only contains normal spaces and these code stops working because of parsing failures. As a workaround
// for this issue, this function replaces narrowNoBreakSpace and thinSpace with normal space.
// This behavior is aligned to SpiderMonkey[3] and V8[4].
// [1]: https://bugzilla.mozilla.org/show_bug.cgi?id=1806042
// [2]: https://github.com/nodejs/node/issues/46123
// [3]: https://hg.mozilla.org/mozilla-central/rev/40e2c54d5618
// [4]: https://chromium.googlesource.com/v8/v8/+/bab790f9165f65a44845b4383c8df7c6c32cf4b3
template<typename Container>
static void NODELETE replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(Container& vector)
{
    // The key of this replacement is that we are not changing size of string.
    // This allows us not to adjust offsets reported from formatToParts / formatRangeToParts
    for (auto& character : vector) {
        if (character == narrowNoBreakSpace || character == thinSpace)
            character = space;
    }
}

// https://tc39.es/proposal-temporal/#sec-formatdatetime
JSValue IntlDateTimeFormat::format(JSGlobalObject* globalObject, double value) const
{
    ASSERT(m_impl->m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat format()"_s);

    Vector<char16_t, 32> result;
    auto status = callBufferProducingFunction(udat_format, m_impl->m_dateFormat.get(), value, result, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);

    return jsString(vm, String(WTF::move(result)));
}

static ASCIILiteral partTypeString(UDateFormatField field)
{
    switch (field) {
    case UDAT_ERA_FIELD:
        return "era"_s;
    case UDAT_YEAR_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
        return "year"_s;
    case UDAT_YEAR_NAME_FIELD:
        return "yearName"_s;
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
        return "month"_s;
    case UDAT_DATE_FIELD:
        return "day"_s;
    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
        return "hour"_s;
    case UDAT_MINUTE_FIELD:
        return "minute"_s;
    case UDAT_SECOND_FIELD:
        return "second"_s;
    case UDAT_FRACTIONAL_SECOND_FIELD:
        return "fractionalSecond"_s;
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
        return "weekday"_s;
    case UDAT_AM_PM_FIELD:
    case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
    case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
        return "dayPeriod"_s;
    case UDAT_TIMEZONE_FIELD:
    case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD:
    case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
    case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
        return "timeZoneName"_s;
    case UDAT_RELATED_YEAR_FIELD:
        return "relatedYear"_s;
    // These should not show up because there is no way to specify them in DateTimeFormat options.
    // If they do, they don't fit well into any of known part types, so consider it an "unknown".
    case UDAT_DAY_OF_YEAR_FIELD:
    case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
    case UDAT_WEEK_OF_YEAR_FIELD:
    case UDAT_WEEK_OF_MONTH_FIELD:
    case UDAT_YEAR_WOY_FIELD:
    case UDAT_JULIAN_DAY_FIELD:
    case UDAT_MILLISECONDS_IN_DAY_FIELD:
    case UDAT_QUARTER_FIELD:
    case UDAT_STANDALONE_QUARTER_FIELD:
    case UDAT_TIME_SEPARATOR_FIELD:
    // Any newer additions to the UDateFormatField enum should just be considered an "unknown" part.
    default:
        return "unknown"_s;
    }
    return "unknown"_s;
}

static JSValue buildFormattedDateTimeParts(JSGlobalObject* globalObject, UDateFormat* format, double value, JSString* sourceType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value)) [[unlikely]]
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat formatToParts()"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto fields = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status)) [[unlikely]]
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<char16_t, 32> result;
    status = callBufferProducingFunction(udat_formatForFields, format, value, result, fields.get());
    if (U_FAILURE(status)) [[unlikely]]
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts) [[unlikely]]
        return throwOutOfMemoryError(globalObject, scope);

    StringView resultStringView(result.span());
    auto literalString = jsNontrivialString(vm, "literal"_s);

    int32_t resultLength = result.size();
    int32_t previousEndIndex = 0;
    int32_t beginIndex = 0;
    int32_t endIndex = 0;
    while (previousEndIndex < resultLength) {
        auto fieldType = ufieldpositer_next(fields.get(), &beginIndex, &endIndex);
        if (fieldType < 0)
            beginIndex = endIndex = resultLength;

        if (previousEndIndex < beginIndex) {
            auto value = jsString(vm, resultStringView.substring(previousEndIndex, beginIndex - previousEndIndex));
            JSObject* part = sourceType
                ? createIntlPartObjectWithSource(globalObject, literalString, value, sourceType)
                : createIntlPartObject(globalObject, literalString, value);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;

        if (fieldType >= 0) {
            auto type = jsNontrivialString(vm, partTypeString(UDateFormatField(fieldType)));
            auto value = jsString(vm, resultStringView.substring(beginIndex, endIndex - beginIndex));
            JSObject* part = sourceType
                ? createIntlPartObjectWithSource(globalObject, type, value, sourceType)
                : createIntlPartObject(globalObject, type, value);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    return parts;
}

UDateIntervalFormat* IntlDateTimeFormat::createDateIntervalFormatIfNecessary(JSGlobalObject* globalObject)
{
    ASSERT(m_impl->m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_dateIntervalFormat)
        return m_dateIntervalFormat.get();

    Vector<char16_t, 32> pattern;
    {
        auto status = callBufferProducingFunction(udat_toPattern, m_impl->m_dateFormat.get(), false, pattern);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
            return nullptr;
        }
    }

    Vector<char16_t, 32> skeleton;
    {
        auto status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, pattern.span().data(), pattern.size(), skeleton);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
            return nullptr;
        }
    }

    dataLogLnIf(IntlDateTimeFormatInternal::verbose, "interval format pattern:(", String(pattern), "),skeleton:(", String(skeleton), ")");

    // While the pattern is including right HourCycle patterns, UDateIntervalFormat does not follow.
    // We need to enforce HourCycle by setting "hc" extension if it is specified.
    StringBuilder localeBuilder;
    localeBuilder.append(m_impl->m_dataLocale);
    if (!m_impl->m_calendar.isNull() || !m_impl->m_numberingSystem.isNull() || m_impl->m_hourCycle != HourCycle::None) {
        localeBuilder.append("-u"_s);
        if (!m_impl->m_calendar.isNull())
            localeBuilder.append("-ca-"_s, m_impl->m_calendar);
        if (!m_impl->m_numberingSystem.isNull())
            localeBuilder.append("-nu-"_s, m_impl->m_numberingSystem);
        if (m_impl->m_hourCycle != HourCycle::None)
            localeBuilder.append("-hc-"_s, hourCycleString(m_impl->m_hourCycle));
    }
    CString dataLocaleWithExtensions = localeBuilder.toString().utf8();

    UErrorCode status = U_ZERO_ERROR;
    String timeZoneForICU = m_impl->m_timeZone.toICUString();
    StringView timeZoneView(timeZoneForICU);
    m_dateIntervalFormat = std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>(udtitvfmt_open(dataLocaleWithExtensions.data(), skeleton.span().data(), skeleton.size(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), &status));
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
        return nullptr;
    }

    vm.heap.reportExtraMemoryAllocated(this, estimatedUDateIntervalFormatSize);

    return m_dateIntervalFormat.get();
}

static std::unique_ptr<UFormattedDateInterval, UFormattedDateIntervalDeleter> formattedValueFromDateRange(UDateIntervalFormat& dateIntervalFormat, const UDateFormat& dateFormat, double startDate, double endDate, UErrorCode& status)
{
    auto result = std::unique_ptr<UFormattedDateInterval, UFormattedDateIntervalDeleter>(udtitvfmt_openResult(&status));
    if (U_FAILURE(status))
        return nullptr;

    // After ICU 67, udtitvfmt_formatToResult's signature is changed.
#if U_ICU_VERSION_MAJOR_NUM >= 67
    // If a date is after Oct 15, 1582, the configuration of gregorian calendar change date in UCalendar does not affect
    // on the formatted string. To ensure that it is after Oct 15 in all timezones, we add one day to gregorian calendar
    // change date in UTC, so that this check can conservatively answer whether the date is definitely after gregorian
    // calendar change date.
    auto definitelyAfterGregorianCalendarChangeDate = [](double millisecondsFromEpoch) {
        constexpr double gregorianCalendarReformDateInUTC = -12219292800000.0;
        return millisecondsFromEpoch >= (gregorianCalendarReformDateInUTC + msPerDay);
    };

    // UFormattedDateInterval does not have a way to configure gregorian calendar change date while ECMAScript requires that
    // gregorian calendar change should not have effect (we are setting ucal_setGregorianChange(cal, minECMAScriptTime, &status) explicitly).
    // As a result, if the input date is older than gregorian calendar change date (Oct 15, 1582), the formatted string becomes
    // julian calendar date.
    // udtitvfmt_formatCalendarToResult API offers the way to set calendar to each date of the input, so that we can use UDateFormat's
    // calendar which is already configured to meet ECMAScript's requirement (effectively clearing gregorian calendar change date).
    //
    // If we can ensure that startDate is after gregorian calendar change date, we can just use udtitvfmt_formatToResult since gregorian
    // calendar change date does not affect on the formatted string.
    //
    // https://unicode-org.atlassian.net/browse/ICU-20705
    if (definitelyAfterGregorianCalendarChangeDate(startDate))
        udtitvfmt_formatToResult(&dateIntervalFormat, startDate, endDate, result.get(), &status);
    else {
        auto createCalendarForDate = [](const UCalendar* calendar, double date, UErrorCode& status) -> std::unique_ptr<UCalendar, ICUDeleter<ucal_close>> {
            auto result = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(ucal_clone(calendar, &status));
            if (U_FAILURE(status))
                return nullptr;
            ucal_setMillis(result.get(), date, &status);
            if (U_FAILURE(status))
                return nullptr;
            return result;
        };

        auto calendar = udat_getCalendar(&dateFormat);

        auto startCalendar = createCalendarForDate(calendar, startDate, status);
        if (U_FAILURE(status))
            return nullptr;

        auto endCalendar = createCalendarForDate(calendar, endDate, status);
        if (U_FAILURE(status))
            return nullptr;

        udtitvfmt_formatCalendarToResult(&dateIntervalFormat, startCalendar.get(), endCalendar.get(), result.get(), &status);
    }
#else
    UNUSED_PARAM(dateFormat);
    udtitvfmt_formatToResult(&dateIntervalFormat, result.get(), startDate, endDate, &status);
#endif
    return result;
}

static bool dateFieldsPracticallyEqual(const UFormattedValue* formattedValue, UErrorCode& status)
{
    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status))
        return false;

    // We only care about UFIELD_CATEGORY_DATE_INTERVAL_SPAN category.
    ucfpos_constrainCategory(iterator.get(), UFIELD_CATEGORY_DATE_INTERVAL_SPAN, &status);
    if (U_FAILURE(status))
        return false;

    bool hasSpan = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
    if (U_FAILURE(status))
        return false;

    return !hasSpan;
}

auto IntlDateTimeFormat::prepareDateRange(JSGlobalObject* globalObject, double& startDate, double& endDate) -> std::optional<DateRangePreamble>
{
    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-partitiondatetimerangepattern
    startDate = timeClip(startDate);
    endDate = timeClip(endDate);
    if (std::isnan(startDate) || std::isnan(endDate)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Passed date is out of range"_s);
        return std::nullopt;
    }

    auto* dateIntervalFormat = createDateIntervalFormatIfNecessary(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    UErrorCode status = U_ZERO_ERROR;
    auto result = formattedValueFromDateRange(*dateIntervalFormat, *m_impl->m_dateFormat, startDate, endDate, status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    // UFormattedValue is owned by UFormattedDateInterval. We do not need to close it.
    auto* formattedValue = udtitvfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    // If the formatted parts of startDate and endDate are the same, it is possible that the resulted string does not look like range.
    // For example, if the requested format only includes "year" and startDate and endDate are the same year, the result just contains one year.
    // In that case, startDate and endDate are *practically-equal* (spec term), and we generate parts as we call `formatToParts(startDate)` with
    // `source: "shared"` additional fields.
    bool equal = dateFieldsPracticallyEqual(formattedValue, status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    return DateRangePreamble { std::unique_ptr<UFormattedDateInterval, UFormattedDateIntervalDeleter>(result.release()), formattedValue, equal };
}

// https://tc39.es/proposal-temporal/#sec-partitiondatetimerangepattern
//
// NOTE: Diverges from the spec in two ways for efficiency:
// 1. Returns a TemporalDateRangePreamble (raw ICU UFormattedDateInterval) instead of the spec's
//    List of {[[Type]],[[Value]],[[Source]]} records, avoiding an intermediate parts vector.
// 2. Defers the practically-equal case (spec step: FormatDateTimePattern + tag "shared") to the
//    callers — formatRange needs a string, formatRangeToParts needs parts; no single output fits both.
auto IntlDateTimeFormat::partitionDateTimeRangePattern(JSGlobalObject* globalObject, JSValue xValue, JSValue yValue) -> std::optional<TemporalDateRangePreamble>
{
    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If IsTemporalObject(x) or IsTemporalObject(y), SameTemporalType(x,y) must be true.
    if (isTemporalObject(xValue) || isTemporalObject(yValue)) [[unlikely]] {
        if (!sameTemporalType(xValue, yValue)) [[unlikely]] {
            throwTypeError(globalObject, scope, "formatRange requires both arguments to be the same Temporal type"_s);
            return std::nullopt;
        }
    }

    // Steps 2-3: HandleDateTimeValue for x and y — converts Temporal objects / Numbers to
    // epoch milliseconds and validates calendar compatibility and format availability.
    auto startRecord = handleDateTimeValue(globalObject, this, xValue);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    auto endRecord = handleDateTimeValue(globalObject, this, yValue);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Steps 4-5: xEpochNanoseconds / yEpochNanoseconds (we use milliseconds for ICU).
    double startMs = startRecord.value;
    double endMs = endRecord.value;
    auto kind = startRecord.kind;

    // Steps 9-10: Assert format and [[IsPlain]] match — guaranteed because SameTemporalType
    // ensures both records have the same kind, which uniquely determines the format slot.
    ASSERT(startRecord.kind == endRecord.kind);

    // Non-Temporal path (kind == None): steps 6-21 are handled by the legacy prepareDateRange
    // path in the caller; return a bare preamble with no ICU interval formatter.
    if (kind == TemporalFieldKind::None)
        return TemporalDateRangePreamble { nullptr, nullptr, nullptr, false, startMs, endMs, kind };

    // Step 8: Obtain [[Format]] = the temporal UDateFormat for this kind.
    // (null means the DTF has no fields applicable to this Temporal type — already checked in
    // handleDateTimeValue, but guarded here too for the range path.)
    auto tempFormat = getTemporalFormatter(vm, kind);
    if (!tempFormat) [[unlikely]] {
        throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);
        return std::nullopt;
    }

    // Steps 6-7, 11-18: ToLocalTime for both endpoints + field-by-field comparison to select
    // the appropriate range pattern are all handled internally by ICU's UDateIntervalFormat.
    // createTemporalIntervalFormat builds the interval formatter from the temporal skeleton;
    // formattedValueFromDateRange calls udtitvfmt_formatToResult (or formatCalendarToResult
    // for pre-1582 dates) which executes the ToLocalTime + field-comparison loop.
    UErrorCode status = U_ZERO_ERROR;
    auto intervalFormat = createTemporalIntervalFormat(tempFormat, kind, status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    auto result = formattedValueFromDateRange(*intervalFormat, *tempFormat, startMs, endMs, status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    // UFormattedValue is owned by UFormattedDateInterval. We do not need to close it.
    auto* formattedValue = udtitvfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    // Step 19: relevantFieldsEqual — ICU marks fields as "shared" when both endpoints are equal.

    // If the formatted parts of startDate and endDate are the same, it is possible that the resulted string does not look like range.
    // For example, if the requested format only includes "year" and startDate and endDate are the same year, the result just contains one year.
    // In that case, startDate and endDate are *practically-equal* (spec term), and we generate parts as we call `formatToParts(startDate)` with
    // `source: "shared"` additional fields.
    bool equal = dateFieldsPracticallyEqual(formattedValue, status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return std::nullopt;
    }

    // Steps 19-21 (completion): building the parts List with [[Source]] = "shared" / "startRange"
    // / "endRange" is done by the callers (formatRange / formatRangeToParts) using this preamble.
    return TemporalDateRangePreamble {
        tempFormat,
        std::unique_ptr<UFormattedDateInterval, UFormattedDateIntervalDeleter>(result.release()),
        formattedValue,
        equal,
        startMs,
        endMs,
        kind
    };
}

static JSValue buildFormattedDateIntervalParts(JSGlobalObject* globalObject, const UFormattedValue* formattedValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // ICU produces ranges for the formatted string, and we construct parts array from that.
    // For example, startDate = Jan 3, 2019, endDate = Jan 5, 2019 with en-US locale is,
    //
    // Formatted string: "1/3/2019 – 1/5/2019"
    //                    | | |  |   | | |  |
    //                    B C |  |   F G |  |
    //                    |   +-D+   |   +-H+
    //                    |      |   |      |
    //                    +--A---+   +--E---+
    //
    // Ranges ICU generates:
    //     A:    (0, 8)   UFIELD_CATEGORY_DATE_INTERVAL_SPAN startRange
    //     B:    (0, 1)   UFIELD_CATEGORY_DATE month
    //     C:    (2, 3)   UFIELD_CATEGORY_DATE day
    //     D:    (4, 8)   UFIELD_CATEGORY_DATE year
    //     E:    (11, 19) UFIELD_CATEGORY_DATE_INTERVAL_SPAN endRange
    //     F:    (11, 12) UFIELD_CATEGORY_DATE month
    //     G:    (13, 14) UFIELD_CATEGORY_DATE day
    //     H:    (15, 19) UFIELD_CATEGORY_DATE year
    //
    //  We use UFIELD_CATEGORY_DATE_INTERVAL_SPAN range to determine each part is either "startRange", "endRange", or "shared".
    //  It is guaranteed that UFIELD_CATEGORY_DATE_INTERVAL_SPAN comes first before any other parts including that range.
    //  For example, in the above formatted string, " – " is "shared" part. For UFIELD_CATEGORY_DATE ranges, we generate corresponding
    //  part object with types such as "month". And non populated parts (e.g. "/") become "literal" parts.
    //  In the above case, expected parts are,
    //
    //     { type: "month", value: "1", source: "startRange" },
    //     { type: "literal", value: "/", source: "startRange" },
    //     { type: "day", value: "3", source: "startRange" },
    //     { type: "literal", value: "/", source: "startRange" },
    //     { type: "year", value: "2019", source: "startRange" },
    //     { type: "literal", value: " - ", source: "shared" },
    //     { type: "month", value: "1", source: "endRange" },
    //     { type: "literal", value: "/", source: "endRange" },
    //     { type: "day", value: "5", source: "endRange" },
    //     { type: "literal", value: "/", source: "endRange" },
    //     { type: "year", value: "2019", source: "endRange" },
    //

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t formattedStringLength = 0;
    const char16_t* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }
    Vector<char16_t, 32> buffer(std::span<const char16_t> { formattedStringPointer, static_cast<size_t>(formattedStringLength) });
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);

    StringView resultStringView(buffer.span());

    // We care multiple categories (UFIELD_CATEGORY_DATE and UFIELD_CATEGORY_DATE_INTERVAL_SPAN).
    // So we do not constraint iterator.
    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto startRangeString = jsNontrivialString(vm, "startRange"_s);
    auto endRangeString = jsNontrivialString(vm, "endRange"_s);
    auto sharedString = jsNontrivialString(vm, "shared"_s);
    auto literalString = jsNontrivialString(vm, "literal"_s);

    WTF::Range<int32_t> startRange { -1, -1 };
    WTF::Range<int32_t> endRange { -1, -1 };

    auto createPart = [&] (JSString* type, int32_t beginIndex, int32_t length) {
        auto sourceType = [&](int32_t index) -> JSString* {
            if (startRange.contains(index))
                return startRangeString;
            if (endRange.contains(index))
                return endRangeString;
            return sharedString;
        };

        auto value = jsString(vm, resultStringView.substring(beginIndex, length));
        return createIntlPartObjectWithSource(globalObject, type, value, sourceType(beginIndex));
    };

    int32_t resultLength = resultStringView.length();
    int32_t previousEndIndex = 0;
    while (true) {
        bool next = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }
        if (!next)
            break;

        int32_t category = ucfpos_getCategory(iterator.get(), &status);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }

        int32_t fieldType = ucfpos_getField(iterator.get(), &status);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }

        int32_t beginIndex = 0;
        int32_t endIndex = 0;
        ucfpos_getIndexes(iterator.get(), &beginIndex, &endIndex, &status);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }

        dataLogLnIf(IntlDateTimeFormatInternal::verbose, category, " ", fieldType, " (", beginIndex, ", ", endIndex, ")");

        if (category != UFIELD_CATEGORY_DATE && category != UFIELD_CATEGORY_DATE_INTERVAL_SPAN)
            continue;
        if (category == UFIELD_CATEGORY_DATE && fieldType < 0)
            continue;

        if (previousEndIndex < beginIndex) {
            JSObject* part = createPart(literalString, previousEndIndex, beginIndex - previousEndIndex);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
            previousEndIndex = beginIndex;
        }

        if (category == UFIELD_CATEGORY_DATE_INTERVAL_SPAN) {
            // > The special field category UFIELD_CATEGORY_DATE_INTERVAL_SPAN is used to indicate which datetime
            // > primitives came from which arguments: 0 means fromCalendar, and 1 means toCalendar. The span category
            // > will always occur before the corresponding fields in UFIELD_CATEGORY_DATE in the nextPosition() iterator.
            // from ICU comment. So, field 0 is startRange, field 1 is endRange.
            if (!fieldType)
                startRange = WTF::Range<int32_t>(beginIndex, endIndex);
            else {
                ASSERT(fieldType == 1);
                endRange = WTF::Range<int32_t>(beginIndex, endIndex);
            }
            continue;
        }

        ASSERT(category == UFIELD_CATEGORY_DATE);

        auto type = jsNontrivialString(vm, partTypeString(UDateFormatField(fieldType)));
        JSObject* part = createPart(type, beginIndex, endIndex - beginIndex);
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, { });
        previousEndIndex = endIndex;
    }

    if (previousEndIndex < resultLength) {
        JSObject* part = createPart(literalString, previousEndIndex, resultLength - previousEndIndex);
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}

// https://tc39.es/proposal-temporal/#sec-formatdatetimerangetoparts
JSValue IntlDateTimeFormat::formatRangeToParts(JSGlobalObject* globalObject, JSValue xValue, JSValue yValue)
{
    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto preamble = partitionDateTimeRangePattern(globalObject, xValue, yValue);
    RETURN_IF_EXCEPTION(scope, { });
    if (!preamble)
        return { };

    if (preamble->kind == TemporalFieldKind::None) {
        auto nonTemporalPreamble = prepareDateRange(globalObject, preamble->startMs, preamble->endMs);
        RETURN_IF_EXCEPTION(scope, { });
        if (!nonTemporalPreamble)
            return { };

        if (nonTemporalPreamble->equal)
            RELEASE_AND_RETURN(scope, buildFormattedDateTimeParts(globalObject, m_impl->m_dateFormat.get(), preamble->startMs, jsNontrivialString(vm, "shared"_s)));
        RELEASE_AND_RETURN(scope, buildFormattedDateIntervalParts(globalObject, nonTemporalPreamble->formattedValue));
    }

    if (preamble->equal)
        RELEASE_AND_RETURN(scope, buildFormattedDateTimeParts(globalObject, preamble->tempFormat, preamble->startMs, jsNontrivialString(vm, "shared"_s)));
    RELEASE_AND_RETURN(scope, buildFormattedDateIntervalParts(globalObject, preamble->formattedValue));
}

// BitSet<128> of ASCII pattern letters for each Temporal field kind.
// constexpr: fully evaluated at compile time, zero runtime cost.
using TemporalFieldSet = WTF::BitSet<128>;

static constexpr TemporalFieldSet makeTemporalFieldSet(std::string_view chars)
{
    TemporalFieldSet s;
    for (unsigned char c : chars)
        s.set(c);
    return s;
}

// Plain Temporal types have no timezone; they use GMT for epoch math and exclude timezone
// skeleton chars. Only Instant uses the formatter's configured timezone.
static constexpr bool isPlain(IntlDateTimeFormat::TemporalFieldKind kind)
{
    // HandleDateTimeOthers (Kind::None) returns [[IsPlain]]: false.
    // Instant and ZonedDateTime are timezone-aware, also [[IsPlain]]: false.
    // All plain Temporal types return [[IsPlain]]: true.
    return kind != IntlDateTimeFormat::TemporalFieldKind::None
        && kind != IntlDateTimeFormat::TemporalFieldKind::Instant
        && kind != IntlDateTimeFormat::TemporalFieldKind::ZonedDateTime;
}

// UTS#35 skeleton chars (https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table)
// mapped to ECMA-402 DateTimeFormat properties (https://tc39.es/ecma402/#table-datetimeformat-components):
//   E/c -> weekday
//   G -> era
//   y -> year
//   M/L -> month
//   d -> day
//   h/H/k/K/j -> hour
//   m -> minute
//   s -> second
//   S -> fractionalSecondDigits
//   B/b/a -> dayPeriod
static constexpr TemporalFieldSet dateFields = makeTemporalFieldSet("EcGyMLd"); // weekday, era, year, month, day
static constexpr TemporalFieldSet timeFields = makeTemporalFieldSet("hHkKjmsBbaS"); // hour, minute, second, fractionalSecondDigits, dayPeriod
static constexpr TemporalFieldSet yearMonthFields = makeTemporalFieldSet("GyML"); // era, year, month
static constexpr TemporalFieldSet monthDayFields = makeTemporalFieldSet("MLd"); // month, day
static constexpr TemporalFieldSet dateTimeFields = [] {
    auto s = dateFields;
    s |= timeFields;
    return s;
}();

// requiredOptions per GetDateTimeFormat steps 1-5 — excludes era (not in spec's requiredOptions).
static constexpr TemporalFieldSet dateRequiredFields = makeTemporalFieldSet("EcyMLd"); // weekday, year, month, day
static constexpr TemporalFieldSet yearMonthRequiredFields = makeTemporalFieldSet("yML"); // year, month
static constexpr TemporalFieldSet dateTimeRequiredFields = [] {
    auto s = dateRequiredFields;
    s |= timeFields;
    return s;
}();

// allowedOptions for AdjustDateTimeStyleFormat — broader than requiredOptions, includes era.
static const TemporalFieldSet& allowedFieldsForKind(IntlDateTimeFormat::TemporalFieldKind kind)
{
    static constexpr TemporalFieldSet emptyFields;
    switch (kind) {
    case IntlDateTimeFormat::TemporalFieldKind::PlainDate:
        return dateFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainDateTime:
        return dateTimeFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainTime:
        return timeFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainYearMonth:
        return yearMonthFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainMonthDay:
        return monthDayFields;
    default:
        return emptyFields;
    }
}

// requiredOptions for GetDateTimeFormat — which user-set fields satisfy the type's requirement.
static const TemporalFieldSet& requiredFieldsForKind(IntlDateTimeFormat::TemporalFieldKind kind)
{
    static constexpr TemporalFieldSet emptyFields;
    switch (kind) {
    case IntlDateTimeFormat::TemporalFieldKind::PlainDate:
        return dateRequiredFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainDateTime:
    case IntlDateTimeFormat::TemporalFieldKind::Instant:
        return dateTimeRequiredFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainTime:
        return timeFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainYearMonth:
        return yearMonthRequiredFields;
    case IntlDateTimeFormat::TemporalFieldKind::PlainMonthDay:
        return monthDayFields;
    default:
        return emptyFields;
    }
}

// Lazily computes and caches the [[TemporalXxxFormat]] slot for the given Temporal type.
// Returns a non-owning pointer — no clone needed since JS is single-threaded (no re-entrance).
// The spec computes these eagerly in CreateDateTimeFormat; we do it on demand and cache.
UDateFormat* IntlDateTimeFormat::getTemporalFormatter(VM& vm, TemporalFieldKind kind) const
{
    ASSERT(kind != TemporalFieldKind::None && kind != TemporalFieldKind::ZonedDateTime);
    // Instant with dateStyle/timeStyle: the base formatter already has the correct
    // style and timezone — reuse m_dateFormat directly, no clone or pattern change needed.
    if (kind == TemporalFieldKind::Instant
        && (m_impl->m_dateStyle != DateTimeStyle::None || m_impl->m_timeStyle != DateTimeStyle::None))
        return m_impl->m_dateFormat.get();
    if (!m_impl->m_temporalFormatterCache) {
        auto cache = makeUnique<IntlDateTimeFormatTemporalFormatterCache>();
        WTF::storeStoreFence(); // Publish cache contents before the pointer.
        m_impl->m_temporalFormatterCache = WTF::move(cache);
    }
    auto& cached = m_impl->m_temporalFormatterCache->m_formatters[static_cast<size_t>(kind)];
    if (!cached) {
        cached = computeTemporalFormatter(kind);
        if (cached)
            vm.heap.reportExtraMemoryAllocated(this, estimatedUDateFormatSize);
    }
    return cached.get();
}


// https://tc39.es/proposal-temporal/#sec-createdatetimeformat (dispatch)
// CreateDateTimeFormat calls AdjustDateTimeStyleFormat/GetDateTimeFormat eagerly at construction.
// We implement them lazily here on first use, dispatching based on the same conditions.
std::unique_ptr<UDateFormat, IntlDateTimeFormat::UDateFormatDeleter> IntlDateTimeFormat::computeTemporalFormatter(TemporalFieldKind kind) const
{
    ASSERT(kind != TemporalFieldKind::ZonedDateTime);
    Vector<char16_t, 32> patternBuf;
    UErrorCode status = U_ZERO_ERROR;
    status = callBufferProducingFunction(udat_toPattern, m_impl->m_dateFormat.get(), false, patternBuf);
    if (U_FAILURE(status))
        return nullptr;
    Vector<char16_t, 32> skeleton;
    status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, patternBuf.span().data(), patternBuf.size(), skeleton);
    if (U_FAILURE(status))
        return nullptr;

    if (m_impl->m_dateStyle != DateTimeStyle::None || m_impl->m_timeStyle != DateTimeStyle::None) {
        // CreateDateTimeFormat step k: Instant -> bestFormat directly (no field filtering).
        // Steps f/g: PlainDate/YearMonth/MonthDay -> null if dateStyle is undefined (step g).
        // Steps h/i: PlainTime -> null if timeStyle is undefined (step i).
        // Step j: PlainDateTime -> AdjustDateTimeStyleFormat always.
        if (isPlain(kind) && kind != TemporalFieldKind::PlainDateTime) {
            bool isDateType = (kind == TemporalFieldKind::PlainDate || kind == TemporalFieldKind::PlainYearMonth || kind == TemporalFieldKind::PlainMonthDay);
            if (isDateType && m_impl->m_dateStyle == DateTimeStyle::None)
                return nullptr; // Step g: dateStyle undefined -> null.
            if (kind == TemporalFieldKind::PlainTime && m_impl->m_timeStyle == DateTimeStyle::None)
                return nullptr; // Step i: timeStyle undefined -> null.
        }
        return computeAdjustDateTimeStyleFormat(kind, skeleton);
    }
    // CreateDateTimeFormat -> GetDateTimeFormat.
    return computeGetDateTimeFormat(kind);
}


// https://tc39.es/proposal-temporal/#sec-adjustdatetimestyleformat
std::unique_ptr<UDateFormat, IntlDateTimeFormat::UDateFormatDeleter> IntlDateTimeFormat::computeAdjustDateTimeStyleFormat(TemporalFieldKind kind, const Vector<char16_t, 32>& skeleton) const
{
    ASSERT(kind != TemporalFieldKind::ZonedDateTime);

    UErrorCode status = U_ZERO_ERROR;
    auto allowed = allowedFieldsForKind(kind);
    bool plain = isPlain(kind);

    // Steps 1-2: AdjustDateTimeStyleFormat conflict check. Timezone chars (z/O/v) correspond
    // to [[timeZoneName]] in the format record — plain types never include timeZoneName in
    // allowedOptions, so they correctly conflict and step 3 is not taken.
    Vector<char16_t, 32> filteredSkeleton;
    bool anyConflictingFields = false;
    if (plain) {
        for (auto ch : skeleton) {
            if (ch >= 128)
                continue;
            if (allowed.get(ch))
                filteredSkeleton.append(ch);
            else
                anyConflictingFields = true; // includes timezone chars: not in plain types' allowedOptions
        }
    }

    // Step 3: no conflicting fields — return baseFormat with GMT for plain types.
    if (!anyConflictingFields) {
        auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_clone(m_impl->m_dateFormat.get(), &status));
        if (U_FAILURE(status))
            return nullptr;
        if (plain) {
            auto* tempCal = const_cast<UCalendar*>(udat_getCalendar(tempFormat.get()));
            ucal_setTimeZone(tempCal, u"GMT", 3, &status);
            if (U_FAILURE(status))
                return nullptr;
        }
        return tempFormat;
    }

    // Step 4 (NOTE): steps 1-3 avoid an altered format when baseFormat is already sufficient,
    // because ECMA-402 does not guarantee DateTimeStyleFormat output can round-trip through
    // BestFitFormatMatcher.

    // Steps 5-6: formatOptions = allowedOptions fields present in baseFormat (filteredSkeleton).
    if (filteredSkeleton.isEmpty())
        return nullptr;

    // Steps 7-8: BestFitFormatMatcher(formatOptions, formats).
    String generatorLocale = m_impl->m_dataLocale;
    if (!m_impl->m_calendar.isEmpty())
        generatorLocale = makeString(generatorLocale, "-u-ca-"_s, m_impl->m_calendar);
    auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(udatpg_open(generatorLocale.utf8().data(), &status));
    if (U_FAILURE(status))
        return nullptr;
    Vector<char16_t, 32> bestPattern;
    status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, generator.get(),
        filteredSkeleton.span().data(), filteredSkeleton.size(),
        UDATPG_MATCH_HOUR_FIELD_LENGTH, bestPattern);
    if (U_FAILURE(status) || bestPattern.isEmpty())
        return nullptr;
    if (m_impl->m_hourCycle != HourCycle::None)
        replaceHourCycleInPattern(bestPattern, m_impl->m_hourCycle);

    // Step 9: return bestFormat; plain types use GMT for timezone-unaware epoch math.
    auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_clone(m_impl->m_dateFormat.get(), &status));
    if (U_FAILURE(status))
        return nullptr;
    udat_applyPattern(tempFormat.get(), false, bestPattern.span().data(), bestPattern.size());
    if (plain) {
        auto* tempCal = const_cast<UCalendar*>(udat_getCalendar(tempFormat.get()));
        ucal_setTimeZone(tempCal, u"GMT", 3, &status);
        if (U_FAILURE(status))
            return nullptr;
    }
    return tempFormat;
}

// https://tc39.es/proposal-temporal/#sec-getdatetimeformat
std::unique_ptr<UDateFormat, IntlDateTimeFormat::UDateFormatDeleter> IntlDateTimeFormat::computeGetDateTimeFormat(TemporalFieldKind kind) const
{
    ASSERT(kind != TemporalFieldKind::ZonedDateTime);
    UErrorCode status = U_ZERO_ERROR;

    // Steps 1-5: requiredOptions — the set of option property names that satisfy this type's
    //            format requirement. Era is excluded per spec (not listed in requiredOptions).
    auto requiredOptions = requiredFieldsForKind(kind);
    // Steps 6-10: defaultOptions — injected in step 17b below, one switch case per type.

    // Steps 11-12: Determine inherit mode and initialise formatOptions.
    // The spec uses inherit=~all~ for Instant (format includes all user options as-is)
    // and inherit=~relevant~ for all plain types (only type-relevant fields are inherited).
    bool inheritAll = kind == TemporalFieldKind::Instant;

    // Steps 11-19 shortcut for Instant when the user set at least one date/time option:
    // inherit=~all~ → formatOptions = copy of options → step 13 sets anyPresent=true →
    // step 15 needDefaults=true but step 16 immediately sets it false (a required field is
    // present) → steps 18-19 BestFitFormatMatcher on those same options → result == m_dateFormat.
    // m_dateFormat was already built from the full user options in initializeDateTimeFormat,
    // so we can return a clone directly without rebuilding the pattern.
    if (m_impl->m_anyPresent && inheritAll) {
        auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_clone(m_impl->m_dateFormat.get(), &status));
        if (U_FAILURE(status))
            return nullptr;
        return tempFormat;
    }

    // Step 12 (inherit=~relevant~): start from an empty record and copy only type-relevant fields.
    //   • era ('G'): copy if required ∈ {~date~, ~year-month~, ~any~}, i.e., if 'G' is in
    //     allowedFieldsForKind(kind). Era is copied from m_userSkeleton when the user set it.
    //   • hourCycle: spec says copy to formatOptions here, but we apply it as a post-processing
    //     step via replaceHourCycleInPattern after BestFitFormatMatcher (steps 18-19), which
    //     is observably equivalent because ICU's pattern generator respects hour-field length.
    Vector<char16_t, 32> formatOptions;
    if (!inheritAll && allowedFieldsForKind(kind).get('G')) {
        for (auto ch : m_impl->m_userSkeleton) {
            if (ch == u'G')
                formatOptions.append(ch);
        }
    }

    // Steps 13-14: anyPresent — whether the user set any date/time option.
    //              Pre-computed as m_anyPresent in initializeDateTimeFormat.

    // Step 15: needDefaults = true.
    bool needDefaults = true;

    // Step 16: For each prop of requiredOptions: if options.[[prop]] is not undefined →
    //          add it to formatOptions and set needDefaults = false.
    //          m_userSkeleton represents "options.[[prop]] is not undefined" for each char.
    for (auto ch : m_impl->m_userSkeleton) {
        if (ch < 128 && requiredOptions.get(ch)) {
            formatOptions.append(ch);
            needDefaults = false;
        }
    }

    // Step 17: If needDefaults:
    if (needDefaults) {
        // Step 17a: If anyPresent && inherit=~relevant~ → return null.
        //           (The user set some option but none matching this type's required fields —
        //            the base formatter is not appropriate for this Temporal type.)
        if (m_impl->m_anyPresent && !inheritAll)
            return nullptr;

        // Step 17b: Inject defaultOptions — the canonical fields for this type.
        //           These correspond to steps 6-10 (defaultOptions per 'defaults' parameter).
        char16_t hourChar = hourCharForCycle(m_impl->m_hourCycle);
        switch (kind) {
        case TemporalFieldKind::Instant: {
            // defaults=~all~ → defaultOptions = {year,month,day,hour,minute,second}.
            // Start from m_userSkeleton to preserve any user-set timezone field,
            // then append any missing default fields.
            formatOptions = m_impl->m_userSkeleton;
            TemporalFieldSet existingChars;
            for (auto ch : formatOptions) {
                if (ch < 128)
                    existingChars.set(ch);
            }
            for (auto c : { u'y', u'M', u'd', u'j', u'm', u's' }) {
                if (!existingChars.get(c))
                    formatOptions.append(c);
            }
            break;
        }
        case TemporalFieldKind::PlainTime:
            // defaults=~time~ → defaultOptions = {hour,minute,second}.
            formatOptions.appendList({ hourChar, u'm', u's' });
            break;
        case TemporalFieldKind::PlainDate:
            // defaults=~date~ → defaultOptions = {year,month,day}.
            formatOptions.appendList({ u'y', u'M', u'd' });
            break;
        case TemporalFieldKind::PlainDateTime:
            // defaults=~all~ → defaultOptions = {year,month,day,hour,minute,second}.
            formatOptions.appendList({ u'y', u'M', u'd', hourChar, u'm', u's' });
            break;
        case TemporalFieldKind::PlainYearMonth:
            // defaults=~year-month~ → defaultOptions = {year,month}.
            formatOptions.appendList({ u'y', u'M' });
            break;
        case TemporalFieldKind::PlainMonthDay:
            // defaults=~month-day~ → defaultOptions = {month,day}.
            formatOptions.appendList({ u'M', u'd' });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED(); // ZonedDateTime is guarded by the ASSERT at function entry.
        }

        // Step 17c: defaults=~zoned-date-time~ → inject timeZoneName="short" if absent.
        // Not implemented here — handled in initializeDateTimeFormat via Defaults::ZonedDateTime,
        // which injects timeZoneName=Short directly into m_dateFormat before this path runs.
    }

    // Steps 18-19: BestFitFormatMatcher(formatOptions, formats).
    // We always use best-fit (spec also allows basicFormatMatcher but best-fit is the default).
    // The hourCycle field from step 12 is applied post-pattern via replaceHourCycleInPattern.
    String generatorLocale = m_impl->m_dataLocale;
    if (!m_impl->m_calendar.isEmpty())
        generatorLocale = makeString(generatorLocale, "-u-ca-"_s, m_impl->m_calendar);
    auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(udatpg_open(generatorLocale.utf8().data(), &status));
    if (U_FAILURE(status))
        return nullptr;
    Vector<char16_t, 32> bestPattern;
    status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, generator.get(),
        formatOptions.span().data(), formatOptions.size(),
        UDATPG_MATCH_HOUR_FIELD_LENGTH, bestPattern);
    if (U_FAILURE(status) || bestPattern.isEmpty())
        return nullptr;
    if (m_impl->m_hourCycle != HourCycle::None)
        replaceHourCycleInPattern(bestPattern, m_impl->m_hourCycle);

    // Step 20: Return bestFormat.
    // Clone the base formatter (inheriting locale, calendar, number system, timezone) and
    // apply the best-fit pattern. Plain types (no timezone concept) override the calendar's
    // timezone to GMT so that epoch-millisecond inputs format at the correct wall-clock time.
    auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_clone(m_impl->m_dateFormat.get(), &status));
    if (U_FAILURE(status))
        return nullptr;
    udat_applyPattern(tempFormat.get(), false, bestPattern.span().data(), bestPattern.size());
    if (isPlain(kind)) {
        auto* tempCal = const_cast<UCalendar*>(udat_getCalendar(tempFormat.get()));
        ucal_setTimeZone(tempCal, u"GMT", 3, &status);
        if (U_FAILURE(status))
            return nullptr;
    }
    return tempFormat;
}

// https://tc39.es/proposal-temporal/#sec-formatdatetime
// FormatDateTime(dateTimeFormat, x) — dispatches through HandleDateTimeValue for Temporal objects.
JSValue IntlDateTimeFormat::format(JSGlobalObject* globalObject, JSValue x) const
{
    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto record = handleDateTimeValue(globalObject, this, x);
    RETURN_IF_EXCEPTION(scope, { });

    if (record.kind == TemporalFieldKind::None)
        RELEASE_AND_RETURN(scope, format(globalObject, record.value));

    ASSERT(record.kind != TemporalFieldKind::ZonedDateTime);

    if (!std::isfinite(record.value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat format()"_s);

    auto tempFormat = getTemporalFormatter(vm, record.kind);
    if (!tempFormat) [[unlikely]]
        return throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);

    Vector<char16_t, 32> result;
    auto fmtStatus = callBufferProducingFunction(udat_format, tempFormat, record.value, result, nullptr);
    if (U_FAILURE(fmtStatus))
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);
    return jsString(vm, String(WTF::move(result)));
}

// https://tc39.es/proposal-temporal/#sec-formatdatetimetoparts
JSValue IntlDateTimeFormat::formatToParts(JSGlobalObject* globalObject, JSValue x) const
{
    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto record = handleDateTimeValue(globalObject, this, x);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(record.kind != TemporalFieldKind::ZonedDateTime);

    UDateFormat* fmt;
    if (record.kind == TemporalFieldKind::None)
        fmt = m_impl->m_dateFormat.get();
    else {
        fmt = getTemporalFormatter(vm, record.kind);
        if (!fmt) [[unlikely]]
            return throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);
    }

    RELEASE_AND_RETURN(scope, buildFormattedDateTimeParts(globalObject, fmt, record.value, nullptr));
}

std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>
IntlDateTimeFormat::createTemporalIntervalFormat(UDateFormat* tempFormat, TemporalFieldKind kind, UErrorCode& status) const
{
    ASSERT(kind != TemporalFieldKind::ZonedDateTime);
    Vector<char16_t, 32> tempPattern;
    status = callBufferProducingFunction(udat_toPattern, tempFormat, false, tempPattern);
    if (U_FAILURE(status))
        return nullptr;
    Vector<char16_t, 32> tempSkeleton;
    status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, tempPattern.span().data(), tempPattern.size(), tempSkeleton);
    if (U_FAILURE(status))
        return nullptr;

    bool plain = isPlain(kind);
    String tzForInterval = plain ? "GMT"_s : m_impl->m_timeZone.toICUString();
    StringView tzView(tzForInterval);

    StringBuilder localeBuilder;
    localeBuilder.append(m_impl->m_dataLocale, "-u-ca-"_s, ensureCalendar(), "-nu-"_s, ensureNumberingSystem());
    if (m_impl->m_hourCycle != HourCycle::None)
        localeBuilder.append("-hc-"_s, hourCycleString(m_impl->m_hourCycle));
    CString localeWithExt = localeBuilder.toString().utf8();

    return std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>(
        udtitvfmt_open(localeWithExt.data(), tempSkeleton.span().data(), tempSkeleton.size(),
        tzView.upconvertedCharacters(), tzView.length(), &status));
}


// https://tc39.es/proposal-temporal/#sec-formatdatetimerange
JSValue IntlDateTimeFormat::formatRange(JSGlobalObject* globalObject, JSValue xValue, JSValue yValue)
{
    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto preamble = partitionDateTimeRangePattern(globalObject, xValue, yValue);
    RETURN_IF_EXCEPTION(scope, { });
    if (!preamble)
        return { };

    if (preamble->kind == TemporalFieldKind::None) {
        auto nonTemporalPreamble = prepareDateRange(globalObject, preamble->startMs, preamble->endMs);
        RETURN_IF_EXCEPTION(scope, { });
        if (!nonTemporalPreamble)
            return { };

        if (nonTemporalPreamble->equal)
            RELEASE_AND_RETURN(scope, format(globalObject, preamble->startMs));

        UErrorCode status = U_ZERO_ERROR;
        int32_t formattedStringLength = 0;
        const char16_t* formattedStringPointer = ufmtval_getString(nonTemporalPreamble->formattedValue, &formattedStringLength, &status);
        if (U_FAILURE(status)) [[unlikely]] {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }
        Vector<char16_t, 32> buffer(std::span<const char16_t> { formattedStringPointer, static_cast<size_t>(formattedStringLength) });
        replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);
        return jsString(vm, String(WTF::move(buffer)));
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t length;
    const char16_t* chars = ufmtval_getString(preamble->formattedValue, &length, &status);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    if (preamble->equal) {
        Vector<char16_t, 32> singleResult;
        auto singleStatus = callBufferProducingFunction(udat_format, preamble->tempFormat, preamble->startMs, singleResult, nullptr);
        if (U_FAILURE(singleStatus)) [[unlikely]]
            return throwTypeError(globalObject, scope, "failed to format date value"_s);
        replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(singleResult);
        return jsString(vm, String(WTF::move(singleResult)));
    }

    Vector<char16_t, 32> resultChars(std::span(chars, length));
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(resultChars);
    return jsString(vm, String(WTF::move(resultChars)));
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
