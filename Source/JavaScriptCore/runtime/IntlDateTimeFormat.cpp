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

const ClassInfo IntlDateTimeFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormat) };

WTF_MAKE_TZONE_ALLOCATED_IMPL(IntlDateTimeFormatImpl);

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
}

DEFINE_VISIT_CHILDREN(IntlDateTimeFormat);

IntlDateTimeFormat::DateTimeStyle IntlDateTimeFormat::dateStyle() const { return m_impl->m_dateStyle; }
IntlDateTimeFormat::DateTimeStyle IntlDateTimeFormat::timeStyle() const { return m_impl->m_timeStyle; }
IntlDateTimeFormat::TimeZoneName IntlDateTimeFormat::timeZoneName() const { return m_impl->m_timeZoneName; }

void IntlDateTimeFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
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
        break;
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

inline void IntlDateTimeFormat::replaceHourCycleInPattern(Vector<char16_t, 32>& pattern, HourCycle hourCycle, bool allowSingleDigit)
{
    char16_t hourFromHourCycle = 'H';
    switch (hourCycle) {
    case HourCycle::H11:
        hourFromHourCycle = 'K';
        break;
    case HourCycle::H12:
        hourFromHourCycle = 'h';
        break;
    case HourCycle::H23:
        hourFromHourCycle = 'H';
        break;
    case HourCycle::H24:
        hourFromHourCycle = 'k';
        break;
    case HourCycle::None:
        return;
    }

    bool is24Hour = (hourCycle == HourCycle::H23 || hourCycle == HourCycle::H24);

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
        case 'k':
            character = hourFromHourCycle;
            // For h23/h24, pad single-digit hour to two digits unless the caller
            // explicitly requested hour: 'numeric' (allowSingleDigit = true).
            if (is24Hour && !allowSingleDigit
                && (!i || (pattern[i - 1] != 'H' && pattern[i - 1] != 'K' && pattern[i - 1] != 'h' && pattern[i - 1] != 'k'))) {
                unsigned hourCount = 1;
                while (i + hourCount < length && (pattern[i + hourCount] == 'K' || pattern[i + hourCount] == 'h' || pattern[i + hourCount] == 'H' || pattern[i + hourCount] == 'k'))
                    hourCount++;
                if (hourCount == 1) {
                    pattern.insert(i + 1, hourFromHourCycle);
                    length++;
                    i++;
                }
            }
            break;
        case 'a':
            // Remove AM/PM indicator for 24-hour cycles (H/k).
            if (is24Hour)
                character = 0; // mark for removal
            break;
        }
    }
    // Remove marked characters (AM/PM when using 24-hour).
    if (is24Hour) {
        pattern.removeAllMatching([](char16_t ch) {
            return !ch;
        });
        // Trim trailing spaces.
        while (!pattern.isEmpty() && pattern.last() == ' ')
            pattern.removeLast();
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

// https://tc39.github.io/ecma402/#sec-initializedatetimeformat
void IntlDateTimeFormat::initializeDateTimeFormat(JSGlobalObject* globalObject, JSValue locales, JSValue originalOptions, RequiredComponent required, Defaults defaults)
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
        if (!isUnicodeLocaleIdentifierType(calendar)) {
            throwRangeError(globalObject, scope, "calendar is not a well-formed calendar value"_s);
            return;
        }
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Ca)] = calendar.convertToASCIILowercase();
    }

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!numberingSystem.isNull()) {
        if (!isUnicodeLocaleIdentifierType(numberingSystem)) {
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
    if (impl->m_locale.isEmpty()) {
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

    impl->m_hasExplicitComponents = (weekday != Weekday::None || year != Year::None || month != Month::None || day != Day::None || dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits);

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
        if (weekday != Weekday::None || era != Era::None || year != Year::None || month != Month::None || day != Day::None || dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits || timeZoneName != TimeZoneName::None) {
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
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
            return;
        }
        constexpr bool localized = false; // Aligned with how ICU SimpleDateTimeFormat::format works. We do not need to translate this to localized pattern.
        status = callBufferProducingFunction(udat_toPattern, dateFormatFromStyle.get(), localized, patternBuffer);
        if (U_FAILURE(status)) {
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
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
                    return;
                }
                replaceHourCycleInSkeleton(skeleton, specifiedHour12);
                dataLogLnIf(IntlDateTimeFormatInternal::verbose, "replaced:(", StringView { skeleton.span() }, ")");

                patternBuffer = vm.intlCache().getBestDateTimePattern(dataLocaleWithExtensions, skeleton.span(), status);
                if (U_FAILURE(status)) {
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
            if (dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits)
                needDefaults = false;
        }

        if (needDefaults && (defaults == Defaults::Date || defaults == Defaults::All)) {
            year = Year::Numeric;
            month = Month::Numeric;
            day = Day::Numeric;
        }

        if (needDefaults && (defaults == Defaults::Time || defaults == Defaults::All)) {
            hour = Hour::Numeric;
            minute = Minute::Numeric;
            second = Second::Numeric;
        }

        String skeleton = buildSkeleton(weekday, era, year, month, day, hour12, hourCycle, hour, dayPeriod, minute, second, fractionalSecondDigits, timeZoneName);
        UErrorCode status = U_ZERO_ERROR;
        patternBuffer = vm.intlCache().getBestDateTimePattern(dataLocaleWithExtensions, StringView(skeleton).upconvertedCharacters(), status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
            return;
        }
    }

    // After generating pattern from skeleton, we need to change h11 vs. h12 and h23 vs. h24 if hourCycle is specified.
    // Pass allowSingleDigit=true only when hour: 'numeric' was explicitly requested — the skeleton then has a single H,
    // and the user wants single-digit output (e.g. "0:00" not "00:00").
    if (hourCycle != HourCycle::None)
        replaceHourCycleInPattern(patternBuffer, hourCycle, hour == Hour::Numeric);

    StringView pattern(patternBuffer.span());
    setFormatsFromPattern(impl, pattern);

    dataLogLnIf(IntlDateTimeFormatInternal::verbose, "locale:(", impl->m_locale, "),dataLocale:(", dataLocaleWithExtensions, "),pattern:(", pattern, ")");

    UErrorCode status = U_ZERO_ERROR;
    String timeZoneForICU = impl->m_timeZone.toICUString();
    impl->m_dateFormat = openDateFormat(dataLocaleWithExtensions, timeZoneForICU, patternBuffer.span(), status);
    if (U_FAILURE(status)) {
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

// Lazy getter: resolves the locale's default calendar on first call.
// m_calendar stays null until needed, preserving the construction-time perf
// optimization from https://bugs.webkit.org/show_bug.cgi?id=313197.
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
    if (temporalId == "gregory"_s && icuCalId == "gregorian"_s)
        return true;
    if (temporalId == "gregorian"_s && icuCalId == "gregory"_s)
        return true;
    if (temporalId == "ethioaa"_s && (icuCalId == "ethiopic-amete-alem"_s || icuCalId == "ethioaa"_s))
        return true;
    if (temporalId == "ethiopic-amete-alem"_s && (icuCalId == "ethioaa"_s || icuCalId == "ethiopic-amete-alem"_s))
        return true;
    if (temporalId == "islamicc"_s && (icuCalId == "islamic-civil"_s || icuCalId == "islamicc"_s))
        return true;
    if (temporalId == "islamic-civil"_s && (icuCalId == "islamicc"_s || icuCalId == "islamic-civil"_s))
        return true;
    return false;
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

// https://tc39.es/ecma402/#sec-formatdatetime
JSValue IntlDateTimeFormat::format(JSGlobalObject* globalObject, double value) const
{
    return format(globalObject, value, TemporalFieldKind::None);
}

static HashSet<char16_t> allowedFieldsForKind(IntlDateTimeFormat::TemporalFieldKind kind)
{
    HashSet<char16_t> allowed;
    auto addDate = [&]() {
        for (auto c : { 'E', 'c', 'G', 'y', 'M', 'L', 'd' })
            allowed.add(c);
    };
    auto addTime = [&]() {
        for (auto c : { 'h', 'H', 'k', 'K', 'j', 'm', 's', 'B', 'b', 'a', 'S' })
            allowed.add(c);
    };

    switch (kind) {
    case IntlDateTimeFormat::TemporalFieldKind::PlainDate:
        addDate();
        break;
    case IntlDateTimeFormat::TemporalFieldKind::PlainDateTime:
        addDate();
        addTime();
        break;
    case IntlDateTimeFormat::TemporalFieldKind::PlainTime:
        addTime();
        break;
    case IntlDateTimeFormat::TemporalFieldKind::PlainYearMonth:
        for (auto c : { 'G', 'y', 'M', 'L' })
            allowed.add(c);
        break;
    case IntlDateTimeFormat::TemporalFieldKind::PlainMonthDay:
        for (auto c : { 'M', 'L', 'd' })
            allowed.add(c);
        break;
    default:
        break;
    }
    return allowed;
}

// Create a cloned UDateFormat with pattern filtered for the given Temporal kind,
// and timezone set to GMT. Returns nullptr if no relevant fields remain (caller should throw).
std::unique_ptr<UDateFormat, IntlDateTimeFormat::UDateFormatDeleter> IntlDateTimeFormat::createTemporalFormatter(TemporalFieldKind kind) const
{
    UErrorCode status = U_ZERO_ERROR;

    // Get current pattern and extract skeleton.
    Vector<char16_t, 32> patternBuf;
    callBufferProducingFunction(udat_toPattern, m_impl->m_dateFormat.get(), false, patternBuf);

    // Extract skeleton from pattern, then filter the skeleton.
    Vector<char16_t, 32> skeleton;
    callBufferProducingFunction(udatpg_getSkeleton, nullptr,
        reinterpret_cast<const UChar*>(patternBuf.span().data()), patternBuf.size(), skeleton);

    auto allowed = allowedFieldsForKind(kind);

    // For Instant/ZonedDateTime, keep the existing pattern. Only add time defaults
    // when the user didn't provide any explicit component options and no style is set.
    // For dateStyle/timeStyle, use the pattern as-is (V8 keeps the full style pattern).
    if (kind == TemporalFieldKind::Instant || kind == TemporalFieldKind::ZonedDateTime) {
        // If dateStyle or timeStyle is set, use the main formatter's pattern directly.
        // For Instant, this means dateStyle="short" → date-only output (matching V8).
        if (m_impl->m_dateStyle != DateTimeStyle::None || m_impl->m_timeStyle != DateTimeStyle::None) {
            auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(
                udat_clone(m_impl->m_dateFormat.get(), &status));
            if (U_FAILURE(status))
                return nullptr;
            if (kind == TemporalFieldKind::ZonedDateTime && m_impl->m_timeStyle != DateTimeStyle::None) {
                // For ZDT with time displayed, ensure timezone name is present.
                // When only dateStyle is set (no timeStyle), the date-only pattern
                // should match Date.toLocaleString output exactly (no TZ suffix).
                Vector<char16_t, 32> curPattern;
                callBufferProducingFunction(udat_toPattern, tempFormat.get(), false, curPattern);
                bool hasTz = false;
                for (auto ch : curPattern) {
                    if (ch == 'z' || ch == 'O' || ch == 'v') {
                        hasTz = true;
                        break;
                    }
                }
                if (!hasTz) {
                    // Append short timezone to pattern
                    curPattern.append(' ');
                    curPattern.append('z');
                    udat_applyPattern(tempFormat.get(), false,
                        reinterpret_cast<const UChar*>(curPattern.span().data()), curPattern.size());
                }
            }
            return tempFormat;
        }

        // For component-based formatters, if user provided explicit components,
        // use the main formatter's pattern directly (V8's Inherit::kAll copies all options).
        if (m_impl->m_hasExplicitComponents) {
            auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(
                udat_clone(m_impl->m_dateFormat.get(), &status));
            if (U_FAILURE(status))
                return nullptr;
            if (kind == TemporalFieldKind::ZonedDateTime) {
                // ZDT with explicit components: keep timezone if user requested it,
                // but DON'T add default timezone. V8 only adds 'z' in the defaults path.
            }
            // Instant: keep pattern as-is (all user options including timezone name).
            return tempFormat;
        }

        // No explicit components, no dateStyle/timeStyle — add date+time defaults.
        Vector<char16_t, 32> instantSkeleton;
        HashSet<char16_t> tzChars;
        for (auto c : { 'z', 'O', 'v' })
            tzChars.add(c);
        bool hasTzField = false;
        for (auto ch : skeleton) {
            instantSkeleton.append(ch);
            if (tzChars.contains(ch))
                hasTzField = true;
        }

        // Add default date+time fields.
        HashSet<char16_t> existingChars;
        for (auto ch : instantSkeleton)
            existingChars.add(ch);
        for (auto c : { 'y', 'M', 'd', 'j', 'm', 's' }) {
            if (!existingChars.contains(c))
                instantSkeleton.append(c);
        }

        Vector<char16_t, 32> finalSkeleton;
        if (kind == TemporalFieldKind::Instant) {
            // Instant: keep all fields including timezone if user requested it.
            finalSkeleton = WTF::move(instantSkeleton);
        } else {
            finalSkeleton = WTF::move(instantSkeleton);
            if (!hasTzField)
                finalSkeleton.append('z');
        }

        auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(
            udatpg_open(m_impl->m_dataLocale.utf8().data(), &status));
        if (U_FAILURE(status))
            return nullptr;
        Vector<char16_t, 32> bestPattern;
        status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, generator.get(),
            reinterpret_cast<const UChar*>(finalSkeleton.span().data()), finalSkeleton.size(),
            UDATPG_MATCH_HOUR_FIELD_LENGTH, bestPattern);
        if (U_FAILURE(status) || bestPattern.isEmpty())
            return nullptr;

        // Apply the user's hour cycle (from hour12/hourCycle option) to the generated pattern.
        // udatpg_getBestPatternWithOptions uses locale defaults, not the user's hour cycle.
        if (m_impl->m_hourCycle != HourCycle::None)
            replaceHourCycleInPattern(bestPattern, m_impl->m_hourCycle);

        auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(
            udat_clone(m_impl->m_dateFormat.get(), &status));
        if (U_FAILURE(status))
            return nullptr;
        udat_applyPattern(tempFormat.get(), false,
            reinterpret_cast<const UChar*>(bestPattern.span().data()), bestPattern.size());
        return tempFormat;
    }

    Vector<char16_t, 32> filteredSkeleton;
    for (auto ch : skeleton) {
        if (allowed.contains(ch))
            filteredSkeleton.append(ch);
    }

    // Strip timezone chars (z, O, v) from plain type skeletons.
    {
        Vector<char16_t, 32> noTzSkeleton;
        for (auto ch : filteredSkeleton) {
            if (ch != 'z' && ch != 'O' && ch != 'v')
                noTzSkeleton.append(ch);
        }
        filteredSkeleton = WTF::move(noTzSkeleton);
    }

    // For PlainDate/PlainDateTime with ONLY dateStyle (no timeStyle):
    // The main formatter already has a date-only pattern — clone it directly.
    // This ensures toLocaleString matches Date.prototype.toLocaleString output exactly.
    if ((kind == TemporalFieldKind::PlainDate || kind == TemporalFieldKind::PlainDateTime)
        && m_impl->m_dateStyle != DateTimeStyle::None && m_impl->m_timeStyle == DateTimeStyle::None) {
        if (filteredSkeleton.isEmpty())
            return nullptr;
        auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(
            udat_clone(m_impl->m_dateFormat.get(), &status));
        if (U_FAILURE(status))
            return nullptr;
        static const UChar gmtTz[] = { 'G', 'M', 'T', 0 };
        auto* tempCal = const_cast<UCalendar*>(udat_getCalendar(tempFormat.get()));
        ucal_setTimeZone(tempCal, gmtTz, 3, &status);
        return tempFormat;
    }

    // For dateStyle/timeStyle, regenerate from filtered skeleton using the
    // formatter's calendar to preserve calendar-specific patterns (e.g., iso8601).
    if (m_impl->m_dateStyle != DateTimeStyle::None || m_impl->m_timeStyle != DateTimeStyle::None) {
        if (filteredSkeleton.isEmpty())
            return nullptr;

        // Use the formatter's locale with calendar for the generator.
        auto baseUtf8 = m_impl->m_dataLocale.utf8();
        String dataLocaleWithCal = String::fromUTF8(baseUtf8.data());
        if (!dataLocaleWithCal.contains("calendar"_s) && !m_impl->m_calendar.isEmpty()) {
            if (dataLocaleWithCal.contains('@'))
                dataLocaleWithCal = makeString(dataLocaleWithCal, ";calendar="_s, m_impl->m_calendar);
            else
                dataLocaleWithCal = makeString(dataLocaleWithCal, "@calendar="_s, m_impl->m_calendar);
        }

        auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(
            udatpg_open(dataLocaleWithCal.utf8().data(), &status));
        if (U_FAILURE(status))
            return nullptr;
        Vector<char16_t, 32> bestPattern;
        status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, generator.get(),
            reinterpret_cast<const UChar*>(filteredSkeleton.span().data()), filteredSkeleton.size(),
            UDATPG_MATCH_HOUR_FIELD_LENGTH, bestPattern);
        if (U_FAILURE(status) || bestPattern.isEmpty())
            return nullptr;

        auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(
            udat_clone(m_impl->m_dateFormat.get(), &status));
        if (U_FAILURE(status))
            return nullptr;
        udat_applyPattern(tempFormat.get(), false,
            reinterpret_cast<const UChar*>(bestPattern.span().data()), bestPattern.size());
        auto* tempCal = const_cast<UCalendar*>(udat_getCalendar(tempFormat.get()));
        static const UChar gmtTz[] = { 'G', 'M', 'T', 0 };
        ucal_setTimeZone(tempCal, gmtTz, 3, &status);
        return tempFormat;
    }

    if (filteredSkeleton.isEmpty()) {
        if (m_impl->m_hasExplicitComponents)
            return nullptr;

        // Choose hour character based on resolved hourCycle (not locale default 'j').
        char16_t hourChar = 'j'; // default: locale-dependent
        switch (m_impl->m_hourCycle) {
        case HourCycle::H11: hourChar = 'K'; break;
        case HourCycle::H12: hourChar = 'h'; break;
        case HourCycle::H23: hourChar = 'H'; break;
        case HourCycle::H24: hourChar = 'k'; break;
        default: break;
        }

        switch (kind) {
        case TemporalFieldKind::PlainTime:
            filteredSkeleton.append(hourChar);
            for (auto c : { 'm', 's' })
                filteredSkeleton.append(c);
            break;
        case TemporalFieldKind::PlainDate:
        case TemporalFieldKind::PlainDateTime:
            for (auto c : { 'y', 'M', 'd' })
                filteredSkeleton.append(c);
            if (kind == TemporalFieldKind::PlainDateTime) {
                filteredSkeleton.append(hourChar);
                for (auto c : { 'm', 's' })
                    filteredSkeleton.append(c);
            }
            break;
        case TemporalFieldKind::PlainYearMonth:
            for (auto c : { 'y', 'M' })
                filteredSkeleton.append(c);
            break;
        case TemporalFieldKind::PlainMonthDay:
            for (auto c : { 'M', 'd' })
                filteredSkeleton.append(c);
            break;
        default:
            return nullptr;
        }
    }

    // For PlainDateTime: ensure both date and time fields are present.
    // If the DTF only has date fields, add default time fields (and vice versa).
    if (kind == TemporalFieldKind::PlainDateTime && !filteredSkeleton.isEmpty()) {
        HashSet<char16_t> timeChars;
        for (auto c : { 'h', 'H', 'k', 'K', 'j', 'm', 's' })
            timeChars.add(c);
        HashSet<char16_t> dateChars;
        for (auto c : { 'y', 'M', 'L', 'd', 'E', 'c' })
            dateChars.add(c);
        bool hasTime = false, hasDate = false;
        for (auto ch : filteredSkeleton) {
            if (timeChars.contains(ch))
                hasTime = true;
            if (dateChars.contains(ch))
                hasDate = true;
        }
        if (!hasTime && !m_impl->m_hasExplicitComponents) {
            char16_t hourChar = 'j';
            switch (m_impl->m_hourCycle) {
            case HourCycle::H11: hourChar = 'K'; break;
            case HourCycle::H12: hourChar = 'h'; break;
            case HourCycle::H23: hourChar = 'H'; break;
            case HourCycle::H24: hourChar = 'k'; break;
            default: break;
            }
            filteredSkeleton.append(hourChar);
            for (auto c : { 'm', 's' })
                filteredSkeleton.append(c);
        }
        if (!hasDate && !m_impl->m_hasExplicitComponents) {
            for (auto c : { 'y', 'M', 'd' })
                filteredSkeleton.append(c);
        }
    }

    // Generate a locale-appropriate pattern from the filtered skeleton.
    auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(
        udatpg_open(m_impl->m_dataLocale.utf8().data(), &status));
    if (U_FAILURE(status))
        return nullptr;

    Vector<char16_t, 32> bestPattern;
    status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, generator.get(),
        reinterpret_cast<const UChar*>(filteredSkeleton.span().data()), filteredSkeleton.size(),
        UDATPG_MATCH_HOUR_FIELD_LENGTH, bestPattern);
    if (U_FAILURE(status) || bestPattern.isEmpty())
        return nullptr;

    // Fix up hour character in the pattern to match the resolved hourCycle.
    // udatpg_getBestPattern may normalize h24('k') to h23('H') or h11('K') to h12('h').
    if (m_impl->m_hourCycle != HourCycle::None) {
        char16_t wantHour = 0;
        switch (m_impl->m_hourCycle) {
        case HourCycle::H11: wantHour = 'K'; break;
        case HourCycle::H12: wantHour = 'h'; break;
        case HourCycle::H23: wantHour = 'H'; break;
        case HourCycle::H24: wantHour = 'k'; break;
        default: break;
        }
        if (wantHour) {
            for (auto& ch : bestPattern) {
                if (ch == 'h' || ch == 'H' || ch == 'k' || ch == 'K')
                    ch = wantHour;
            }
        }
    }

    // Clone the formatter and apply the generated pattern.
    auto tempFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(
        udat_clone(m_impl->m_dateFormat.get(), &status));
    if (U_FAILURE(status))
        return nullptr;

    udat_applyPattern(tempFormat.get(), false,
        reinterpret_cast<const UChar*>(bestPattern.span().data()), bestPattern.size());

    // Set timezone to GMT for plain types.
    auto* tempCal = const_cast<UCalendar*>(udat_getCalendar(tempFormat.get()));
    static const UChar gmtTz[] = { 'G', 'M', 'T', 0 };
    ucal_setTimeZone(tempCal, gmtTz, 3, &status);

    return tempFormat;
}

JSValue IntlDateTimeFormat::format(JSGlobalObject* globalObject, double value, TemporalFieldKind kind) const
{
    ASSERT(m_impl->m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat format()"_s);

    bool needsTemporalFormatter = (kind != TemporalFieldKind::None);

    if (needsTemporalFormatter) {
        auto tempFormat = createTemporalFormatter(kind);
        if (!tempFormat)
            return throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);

        Vector<char16_t, 32> result;
        auto fmtStatus = callBufferProducingFunction(udat_format, tempFormat.get(), value, result, nullptr);
        if (U_FAILURE(fmtStatus))
            return throwTypeError(globalObject, scope, "failed to format date value"_s);
        replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);
        return jsString(vm, String(WTF::move(result)));
    }

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

// https://tc39.es/ecma402/#sec-formatdatetimetoparts
JSValue IntlDateTimeFormat::formatToParts(JSGlobalObject* globalObject, double value, JSString* sourceType) const
{
    ASSERT(m_impl->m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat formatToParts()"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto fields = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<char16_t, 32> result;
    status = callBufferProducingFunction(udat_formatForFields, m_impl->m_dateFormat.get(), value, result, fields.get());
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
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

JSValue IntlDateTimeFormat::formatToParts(JSGlobalObject* globalObject, double value, TemporalFieldKind kind, JSString* sourceType) const
{
    if (kind == TemporalFieldKind::None)
        return formatToParts(globalObject, value, sourceType);

    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat formatToParts()"_s);

    auto tempFormat = createTemporalFormatter(kind);
    if (!tempFormat)
        return throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto fields = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<char16_t, 32> result;
    status = callBufferProducingFunction(udat_formatForFields, tempFormat.get(), value, result, fields.get());
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
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
            auto val = jsString(vm, resultStringView.substring(previousEndIndex, beginIndex - previousEndIndex));
            JSObject* part = sourceType
                ? createIntlPartObjectWithSource(globalObject, literalString, val, sourceType)
                : createIntlPartObject(globalObject, literalString, val);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;
        if (fieldType >= 0) {
            auto type = jsNontrivialString(vm, partTypeString(UDateFormatField(fieldType)));
            auto val = jsString(vm, resultStringView.substring(beginIndex, endIndex - beginIndex));
            JSObject* part = sourceType
                ? createIntlPartObjectWithSource(globalObject, type, val, sourceType)
                : createIntlPartObject(globalObject, type, val);
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
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
            return nullptr;
        }
    }

    Vector<char16_t, 32> skeleton;
    {
        auto status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, pattern.span().data(), pattern.size(), skeleton);
        if (U_FAILURE(status)) {
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
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
        return nullptr;
    }

    vm.heap.reportExtraMemoryAllocated(this, estimatedUDateIntervalFormatSize);

    return m_dateIntervalFormat.get();
}

static std::unique_ptr<UFormattedDateInterval, ICUDeleter<udtitvfmt_closeResult>> formattedValueFromDateRange(UDateIntervalFormat& dateIntervalFormat, const UDateFormat& dateFormat, double startDate, double endDate, UErrorCode& status)
{
    auto result = std::unique_ptr<UFormattedDateInterval, ICUDeleter<udtitvfmt_closeResult>>(udtitvfmt_openResult(&status));
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

JSValue IntlDateTimeFormat::formatRange(JSGlobalObject* globalObject, double startDate, double endDate, TemporalFieldKind kind)
{
    if (kind == TemporalFieldKind::None)
        return formatRange(globalObject, startDate, endDate);

    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(startDate) || !std::isfinite(endDate)) {
        throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat formatRange()"_s);
        return { };
    }

    auto tempFormat = createTemporalFormatter(kind);
    if (!tempFormat) {
        throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);
        return { };
    }

    // Extract skeleton from temporal formatter's pattern for interval format.
    Vector<char16_t, 32> tempPattern;
    callBufferProducingFunction(udat_toPattern, tempFormat.get(), false, tempPattern);
    Vector<char16_t, 32> tempSkeleton;
    callBufferProducingFunction(udatpg_getSkeleton, nullptr,
        reinterpret_cast<const UChar*>(tempPattern.span().data()), tempPattern.size(), tempSkeleton);

    bool isPlain = (kind != TemporalFieldKind::Instant && kind != TemporalFieldKind::ZonedDateTime);
    String tzForInterval = isPlain ? "GMT"_s : m_impl->m_timeZone.toICUString();
    StringView tzView(tzForInterval);

    StringBuilder localeBuilder;
    localeBuilder.append(m_impl->m_dataLocale, "-u-ca-"_s, ensureCalendar(), "-nu-"_s, ensureNumberingSystem());
    if (m_impl->m_hourCycle != HourCycle::None)
        localeBuilder.append("-hc-"_s, hourCycleString(m_impl->m_hourCycle));
    CString localeWithExt = localeBuilder.toString().utf8();

    UErrorCode status = U_ZERO_ERROR;
    auto intervalFormat = std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>(
        udtitvfmt_open(localeWithExt.data(), tempSkeleton.span().data(), tempSkeleton.size(),
            tzView.upconvertedCharacters(), tzView.length(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto result = formattedValueFromDateRange(*intervalFormat, *tempFormat.get(), startDate, endDate, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto formattedValue = udtitvfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    int32_t length;
    const UChar* chars = ufmtval_getString(formattedValue, &length, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    Vector<char16_t, 32> resultChars(std::span(reinterpret_cast<const char16_t*>(chars), length));
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(resultChars);

    if (dateFieldsPracticallyEqual(formattedValue, status)) {
        Vector<char16_t, 32> singleResult;
        auto singleStatus = callBufferProducingFunction(udat_format, tempFormat.get(), startDate, singleResult, nullptr);
        if (U_SUCCESS(singleStatus)) {
            replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(singleResult);
            return jsString(vm, String(WTF::move(singleResult)));
        }
    }

    return jsString(vm, String(WTF::move(resultChars)));
}

JSValue IntlDateTimeFormat::formatRangeToParts(JSGlobalObject* globalObject, double startDate, double endDate, TemporalFieldKind kind)
{
    if (kind == TemporalFieldKind::None)
        return formatRangeToParts(globalObject, startDate, endDate);

    ASSERT(m_impl->m_dateFormat);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(startDate) || !std::isfinite(endDate)) {
        throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat formatRangeToParts()"_s);
        return { };
    }

    auto tempFormat = createTemporalFormatter(kind);
    if (!tempFormat) {
        throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);
        return { };
    }

    Vector<char16_t, 32> tempPattern;
    callBufferProducingFunction(udat_toPattern, tempFormat.get(), false, tempPattern);
    Vector<char16_t, 32> tempSkeleton;
    callBufferProducingFunction(udatpg_getSkeleton, nullptr,
        reinterpret_cast<const UChar*>(tempPattern.span().data()), tempPattern.size(), tempSkeleton);

    bool isPlain = (kind != TemporalFieldKind::Instant && kind != TemporalFieldKind::ZonedDateTime);
    String tzForInterval = isPlain ? "GMT"_s : m_impl->m_timeZone.toICUString();
    StringView tzView(tzForInterval);

    StringBuilder localeBuilder;
    localeBuilder.append(m_impl->m_dataLocale, "-u-ca-"_s, ensureCalendar(), "-nu-"_s, ensureNumberingSystem());
    if (m_impl->m_hourCycle != HourCycle::None)
        localeBuilder.append("-hc-"_s, hourCycleString(m_impl->m_hourCycle));
    CString localeWithExt = localeBuilder.toString().utf8();

    UErrorCode status = U_ZERO_ERROR;
    auto intervalFormat = std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>(
        udtitvfmt_open(localeWithExt.data(), tempSkeleton.span().data(), tempSkeleton.size(),
            tzView.upconvertedCharacters(), tzView.length(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto result = formattedValueFromDateRange(*intervalFormat, *tempFormat.get(), startDate, endDate, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto formattedValue = udtitvfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    if (dateFieldsPracticallyEqual(formattedValue, status)) {
        auto sourceType = jsNontrivialString(vm, "shared"_s);
        RELEASE_AND_RETURN(scope, formatToParts(globalObject, startDate, kind, sourceType));
    }

    int32_t length;
    const UChar* chars = ufmtval_getString(formattedValue, &length, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    // Normalize narrow no-break space (U+202F) and thin space (U+2009) to regular space (U+0020),
    // matching V8/SpiderMonkey behavior (see replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace comment).
    Vector<char16_t> normalizedChars(std::span(chars, length));
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(normalizedChars);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format date interval"_s);

    auto startRangeString = jsNontrivialString(vm, "startRange"_s);
    auto endRangeString = jsNontrivialString(vm, "endRange"_s);
    auto sharedString = jsNontrivialString(vm, "shared"_s);
    auto literalString = jsNontrivialString(vm, "literal"_s);

    WTF::Range<int32_t> startRange { -1, -1 };
    WTF::Range<int32_t> endRange { -1, -1 };
    Vector<std::tuple<int32_t, int32_t, UDateFormatField, JSString*>> fields;

    while (true) {
        bool hasNext = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
        if (U_FAILURE(status))
            return throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        if (!hasNext)
            break;

        int32_t category = ucfpos_getCategory(iterator.get(), &status);
        if (U_FAILURE(status))
            return throwTypeError(globalObject, scope, "Failed to format date interval"_s);

        int32_t beginIndex, endIndex;
        ucfpos_getIndexes(iterator.get(), &beginIndex, &endIndex, &status);
        if (U_FAILURE(status))
            return throwTypeError(globalObject, scope, "Failed to format date interval"_s);

        if (category == UFIELD_CATEGORY_DATE_INTERVAL_SPAN) {
            int32_t field = ucfpos_getField(iterator.get(), &status);
            if (!field)
                startRange = { beginIndex, endIndex };
            else
                endRange = { beginIndex, endIndex };
        } else if (category == UFIELD_CATEGORY_DATE) {
            int32_t field = ucfpos_getField(iterator.get(), &status);
            fields.append({ beginIndex, endIndex, static_cast<UDateFormatField>(field), nullptr });
        }
    }

    StringView resultStringView(normalizedChars.span());
    int32_t previousEndIndex = 0;

    auto sourceForIndex = [&](int32_t index) -> JSString* {
        if (startRange.begin() >= 0 && index >= startRange.begin() && index < startRange.end())
            return startRangeString;
        if (endRange.begin() >= 0 && index >= endRange.begin() && index < endRange.end())
            return endRangeString;
        return sharedString;
    };

    for (auto& [beginIndex, endIndex, fieldType, _] : fields) {
        if (previousEndIndex < beginIndex) {
            auto val = jsString(vm, resultStringView.substring(previousEndIndex, beginIndex - previousEndIndex));
            auto* source = sourceForIndex(previousEndIndex);
            parts->push(globalObject, createIntlPartObjectWithSource(globalObject, literalString, val, source));
            RETURN_IF_EXCEPTION(scope, { });
        }
        auto type = jsNontrivialString(vm, partTypeString(fieldType));
        auto val = jsString(vm, resultStringView.substring(beginIndex, endIndex - beginIndex));
        auto* source = sourceForIndex(beginIndex);
        parts->push(globalObject, createIntlPartObjectWithSource(globalObject, type, val, source));
        RETURN_IF_EXCEPTION(scope, { });
        previousEndIndex = endIndex;
    }
    if (previousEndIndex < length) {
        auto val = jsString(vm, resultStringView.substring(previousEndIndex, length - previousEndIndex));
        auto* source = sourceForIndex(previousEndIndex);
        parts->push(globalObject, createIntlPartObjectWithSource(globalObject, literalString, val, source));
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}

JSValue IntlDateTimeFormat::formatRange(JSGlobalObject* globalObject, double startDate, double endDate)
{
    ASSERT(m_impl->m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-partitiondatetimerangepattern
    startDate = timeClip(startDate);
    endDate = timeClip(endDate);
    if (std::isnan(startDate) || std::isnan(endDate)) {
        throwRangeError(globalObject, scope, "Passed date is out of range"_s);
        return { };
    }

    auto* dateIntervalFormat = createDateIntervalFormatIfNecessary(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    UErrorCode status = U_ZERO_ERROR;
    auto result = formattedValueFromDateRange(*dateIntervalFormat, *m_impl->m_dateFormat, startDate, endDate, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    // UFormattedValue is owned by UFormattedDateInterval. We do not need to close it.
    auto formattedValue = udtitvfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    // If the formatted parts of startDate and endDate are the same, it is possible that the resulted string does not look like range.
    // For example, if the requested format only includes "year" and startDate and endDate are the same year, the result just contains one year.
    // In that case, startDate and endDate are *practically-equal* (spec term), and we generate parts as we call `formatToParts(startDate)` with
    // `source: "shared"` additional fields.
    bool equal = dateFieldsPracticallyEqual(formattedValue, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    if (equal)
        RELEASE_AND_RETURN(scope, format(globalObject, startDate));

    int32_t formattedStringLength = 0;
    const char16_t* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }
    Vector<char16_t, 32> buffer(std::span<const char16_t> { formattedStringPointer, static_cast<size_t>(formattedStringLength) });
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);

    return jsString(vm, String(WTF::move(buffer)));
}

JSValue IntlDateTimeFormat::formatRangeToParts(JSGlobalObject* globalObject, double startDate, double endDate)
{
    ASSERT(m_impl->m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-partitiondatetimerangepattern
    startDate = timeClip(startDate);
    endDate = timeClip(endDate);
    if (std::isnan(startDate) || std::isnan(endDate)) {
        throwRangeError(globalObject, scope, "Passed date is out of range"_s);
        return { };
    }

    auto* dateIntervalFormat = createDateIntervalFormatIfNecessary(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    UErrorCode status = U_ZERO_ERROR;
    auto result = formattedValueFromDateRange(*dateIntervalFormat, *m_impl->m_dateFormat, startDate, endDate, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    // UFormattedValue is owned by UFormattedDateInterval. We do not need to close it.
    auto formattedValue = udtitvfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto sharedString = jsNontrivialString(vm, "shared"_s);

    // If the formatted parts of startDate and endDate are the same, it is possible that the resulted string does not look like range.
    // For example, if the requested format only includes "year" and startDate and endDate are the same year, the result just contains one year.
    // In that case, startDate and endDate are *practically-equal* (spec term), and we generate parts as we call `formatToParts(startDate)` with
    // `source: "shared"` additional fields.
    bool equal = dateFieldsPracticallyEqual(formattedValue, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    if (equal)
        RELEASE_AND_RETURN(scope, formatToParts(globalObject, startDate, sharedString));

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

    int32_t formattedStringLength = 0;
    const char16_t* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }
    Vector<char16_t, 32> buffer(std::span<const char16_t> { formattedStringPointer, static_cast<size_t>(formattedStringLength) });
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);

    StringView resultStringView(buffer.span());

    // We care multiple categories (UFIELD_CATEGORY_DATE and UFIELD_CATEGORY_DATE_INTERVAL_SPAN).
    // So we do not constraint iterator.
    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    auto startRangeString = jsNontrivialString(vm, "startRange"_s);
    auto endRangeString = jsNontrivialString(vm, "endRange"_s);
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
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }
        if (!next)
            break;

        int32_t category = ucfpos_getCategory(iterator.get(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }

        int32_t fieldType = ucfpos_getField(iterator.get(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format date interval"_s);
            return { };
        }

        int32_t beginIndex = 0;
        int32_t endIndex = 0;
        ucfpos_getIndexes(iterator.get(), &beginIndex, &endIndex, &status);
        if (U_FAILURE(status)) {
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


} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
