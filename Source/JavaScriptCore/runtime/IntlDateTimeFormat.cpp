/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSDateMath.h"
#include "ObjectConstructor.h"
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <wtf/Range.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)
#include <unicode/uformattedvalue.h>
#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#endif // HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)
#include <unicode/udateintervalformat.h>
#if HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)
#define U_HIDE_DRAFT_API 1
#endif // HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)

namespace JSC {

// We do not use ICUDeleter<udtitvfmt_close> because we do not want to include udateintervalformat.h in IntlDateTimeFormat.h.
// udateintervalformat.h needs to be included with #undef U_HIDE_DRAFT_API, and we would like to minimize this effect in IntlDateTimeFormat.cpp.
void UDateIntervalFormatDeleter::operator()(UDateIntervalFormat* formatter)
{
    if (formatter)
        udtitvfmt_close(formatter);
}

const ClassInfo IntlDateTimeFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormat) };

namespace IntlDateTimeFormatInternal {
static constexpr bool verbose = false;
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
    IntlDateTimeFormat* thisObject = jsCast<IntlDateTimeFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundFormat);
}

DEFINE_VISIT_CHILDREN(IntlDateTimeFormat);

void IntlDateTimeFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

static String canonicalizeTimeZoneName(const String& timeZoneName)
{
    // 6.4.1 IsValidTimeZoneName (timeZone)
    // The abstract operation returns true if timeZone, converted to upper case as described in 6.1, is equal to one of the Zone or Link names of the IANA Time Zone Database, converted to upper case as described in 6.1. It returns false otherwise.
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* timeZones = ucal_openTimeZones(&status);
    ASSERT(U_SUCCESS(status));

    String canonical;
    do {
        status = U_ZERO_ERROR;
        int32_t ianaTimeZoneLength;
        // Time zone names are represented as UChar[] in all related ICU APIs.
        const UChar* ianaTimeZone = uenum_unext(timeZones, &ianaTimeZoneLength, &status);
        ASSERT(U_SUCCESS(status));

        // End of enumeration.
        if (!ianaTimeZone)
            break;

        StringView ianaTimeZoneView(ianaTimeZone, ianaTimeZoneLength);
        if (!equalIgnoringASCIICase(timeZoneName, ianaTimeZoneView))
            continue;

        // Found a match, now canonicalize.
        // 6.4.2 CanonicalizeTimeZoneName (timeZone) (ECMA-402 2.0)
        // 1. Let ianaTimeZone be the Zone or Link name of the IANA Time Zone Database such that timeZone, converted to upper case as described in 6.1, is equal to ianaTimeZone, converted to upper case as described in 6.1.
        // 2. If ianaTimeZone is a Link name, then let ianaTimeZone be the corresponding Zone name as specified in the “backward” file of the IANA Time Zone Database.

        Vector<UChar, 32> buffer;
        auto status = callBufferProducingFunction(ucal_getCanonicalTimeZoneID, ianaTimeZone, ianaTimeZoneLength, buffer, nullptr);
        ASSERT_UNUSED(status, U_SUCCESS(status));
        canonical = String(buffer);
    } while (canonical.isNull());
    uenum_close(timeZones);

    // 3. If ianaTimeZone is "Etc/UTC" or "Etc/GMT", then return "UTC".
    if (isUTCEquivalent(canonical))
        return "UTC"_s;

    // 4. Return ianaTimeZone.
    return canonical;
}

Vector<String> IntlDateTimeFormat::localeData(const String& locale, RelevantExtensionKey key)
{
    Vector<String> keyLocaleData;
    switch (key) {
    case RelevantExtensionKey::Ca: {
        UErrorCode status = U_ZERO_ERROR;
        UEnumeration* calendars = ucal_getKeywordValuesForLocale("calendar", locale.utf8().data(), false, &status);
        ASSERT(U_SUCCESS(status));

        int32_t nameLength;
        while (const char* availableName = uenum_next(calendars, &nameLength, &status)) {
            ASSERT(U_SUCCESS(status));
            String calendar = String(availableName, nameLength);
            keyLocaleData.append(calendar);
            // Adding "islamicc" candidate for backward compatibility.
            if (calendar == "islamic-civil"_s)
                keyLocaleData.append("islamicc"_s);
            if (auto mapped = mapICUCalendarKeywordToBCP47(calendar))
                keyLocaleData.append(WTFMove(mapped.value()));
        }
        uenum_close(calendars);
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
static inline unsigned skipLiteralText(const Container& container, unsigned start, unsigned length)
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

void IntlDateTimeFormat::setFormatsFromPattern(StringView pattern)
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
                m_era = Era::Short;
            else if (count == 4)
                m_era = Era::Long;
            else if (count == 5)
                m_era = Era::Narrow;
            break;
        case 'y':
            if (count == 1)
                m_year = Year::Numeric;
            else if (count == 2)
                m_year = Year::TwoDigit;
            break;
        case 'M':
        case 'L':
            if (count == 1)
                m_month = Month::Numeric;
            else if (count == 2)
                m_month = Month::TwoDigit;
            else if (count == 3)
                m_month = Month::Short;
            else if (count == 4)
                m_month = Month::Long;
            else if (count == 5)
                m_month = Month::Narrow;
            break;
        case 'E':
        case 'e':
        case 'c':
            if (count <= 3)
                m_weekday = Weekday::Short;
            else if (count == 4)
                m_weekday = Weekday::Long;
            else if (count == 5)
                m_weekday = Weekday::Narrow;
            break;
        case 'd':
            if (count == 1)
                m_day = Day::Numeric;
            else if (count == 2)
                m_day = Day::TwoDigit;
            break;
        case 'a':
        case 'b':
        case 'B':
            if (count <= 3)
                m_dayPeriod = DayPeriod::Short;
            else if (count == 4)
                m_dayPeriod = DayPeriod::Long;
            else if (count == 5)
                m_dayPeriod = DayPeriod::Narrow;
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
            m_hourCycle = hourCycleFromSymbol(currentCharacter);
            if (count == 1)
                m_hour = Hour::Numeric;
            else if (count == 2)
                m_hour = Hour::TwoDigit;
            break;
        }
        case 'm':
            if (count == 1)
                m_minute = Minute::Numeric;
            else if (count == 2)
                m_minute = Minute::TwoDigit;
            break;
        case 's':
            if (count == 1)
                m_second = Second::Numeric;
            else if (count == 2)
                m_second = Second::TwoDigit;
            break;
        case 'z':
            if (count == 1)
                m_timeZoneName = TimeZoneName::Short;
            else if (count == 4)
                m_timeZoneName = TimeZoneName::Long;
            break;
        case 'O':
            if (count == 1)
                m_timeZoneName = TimeZoneName::ShortOffset;
            else if (count == 4)
                m_timeZoneName = TimeZoneName::LongOffset;
            break;
        case 'v':
        case 'V':
            if (count == 1)
                m_timeZoneName = TimeZoneName::ShortGeneric;
            else if (count == 4)
                m_timeZoneName = TimeZoneName::LongGeneric;
            break;
        case 'S':
            m_fractionalSecondDigits = count;
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

inline IntlDateTimeFormat::HourCycle IntlDateTimeFormat::hourCycleFromSymbol(UChar symbol)
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

IntlDateTimeFormat::HourCycle IntlDateTimeFormat::hourCycleFromPattern(const Vector<UChar, 32>& pattern)
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

inline void IntlDateTimeFormat::replaceHourCycleInSkeleton(Vector<UChar, 32>& skeleton, bool isHour12)
{
    UChar skeletonCharacter = 'H';
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

inline void IntlDateTimeFormat::replaceHourCycleInPattern(Vector<UChar, 32>& pattern, HourCycle hourCycle)
{
    UChar hourFromHourCycle = 'H';
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
            break;
        }
    }
}

String IntlDateTimeFormat::buildSkeleton(Weekday weekday, Era era, Year year, Month month, Day day, TriState hour12, HourCycle hourCycle, Hour hour, DayPeriod dayPeriod, Minute minute, Second second, unsigned fractionalSecondDigits, TimeZoneName timeZoneName)
{
    StringBuilder skeletonBuilder;

    switch (weekday) {
    case Weekday::Narrow:
        skeletonBuilder.append("EEEEE");
        break;
    case Weekday::Short:
        skeletonBuilder.append("EEE");
        break;
    case Weekday::Long:
        skeletonBuilder.append("EEEE");
        break;
    case Weekday::None:
        break;
    }

    switch (era) {
    case Era::Narrow:
        skeletonBuilder.append("GGGGG");
        break;
    case Era::Short:
        skeletonBuilder.append("GGG");
        break;
    case Era::Long:
        skeletonBuilder.append("GGGG");
        break;
    case Era::None:
        break;
    }

    switch (year) {
    case Year::TwoDigit:
        skeletonBuilder.append("yy");
        break;
    case Year::Numeric:
        skeletonBuilder.append('y');
        break;
    case Year::None:
        break;
    }

    switch (month) {
    case Month::TwoDigit:
        skeletonBuilder.append("MM");
        break;
    case Month::Numeric:
        skeletonBuilder.append('M');
        break;
    case Month::Narrow:
        skeletonBuilder.append("MMMMM");
        break;
    case Month::Short:
        skeletonBuilder.append("MMM");
        break;
    case Month::Long:
        skeletonBuilder.append("MMMM");
        break;
    case Month::None:
        break;
    }

    switch (day) {
    case Day::TwoDigit:
        skeletonBuilder.append("dd");
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
        UChar skeletonCharacter = 'j';
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
        skeletonBuilder.append("BBBBB");
        break;
    case DayPeriod::Short:
        skeletonBuilder.append('B');
        break;
    case DayPeriod::Long:
        skeletonBuilder.append("BBBB");
        break;
    case DayPeriod::None:
        break;
    }

    switch (minute) {
    case Minute::TwoDigit:
        skeletonBuilder.append("mm");
        break;
    case Minute::Numeric:
        skeletonBuilder.append('m');
        break;
    case Minute::None:
        break;
    }

    switch (second) {
    case Second::TwoDigit:
        skeletonBuilder.append("ss");
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
        skeletonBuilder.append("zzzz");
        break;
    case TimeZoneName::ShortOffset:
        skeletonBuilder.append('O');
        break;
    case TimeZoneName::LongOffset:
        skeletonBuilder.append("OOOO");
        break;
    case TimeZoneName::ShortGeneric:
        skeletonBuilder.append('v');
        break;
    case TimeZoneName::LongGeneric:
        skeletonBuilder.append("vvvv");
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
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Ca)] = calendar;
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

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat due to invalid locale"_s);
        return;
    }

    {
        String calendar = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Ca)];
        if (auto mapped = mapICUCalendarKeywordToBCP47(calendar))
            m_calendar = WTFMove(mapped.value());
        else
            m_calendar = WTFMove(calendar);
    }

    hourCycle = parseHourCycle(resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Hc)]);
    m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];
    m_dataLocale = resolved.dataLocale;
    CString dataLocaleWithExtensions = makeString(m_dataLocale, "-u-ca-"_s, m_calendar, "-nu-"_s, m_numberingSystem).utf8();

    JSValue tzValue = jsUndefined();
    if (options) {
        tzValue = options->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, void());
    }
    String tz;
    String timeZoneForICU;
    if (!tzValue.isUndefined()) {
        String originalTz = tzValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (auto minutesValue = ISO8601::parseUTCOffsetInMinutes(originalTz)) {
            int64_t minutes = minutesValue.value();
            int64_t absMinutes = std::abs(minutes);
            tz = makeString(minutes < 0 ? '-' : '+', pad('0', 2, absMinutes / 60), ':', pad('0', 2, absMinutes % 60));
            timeZoneForICU = makeString("GMT", minutes < 0 ? '-' : '+', pad('0', 2, absMinutes / 60), pad('0', 2, absMinutes % 60));
        } else {
            tz = canonicalizeTimeZoneName(originalTz);
            if (tz.isNull()) {
                String message = tryMakeString("invalid time zone: "_s, originalTz);
                if (!message)
                    message = "invalid time zone"_s;
                throwRangeError(globalObject, scope, message);
                return;
            }
        }
    } else
        tz = vm.dateCache.defaultTimeZone();
    m_timeZone = tz;
    if (!timeZoneForICU.isNull())
        m_timeZoneForICU = WTFMove(timeZoneForICU);
    else
        m_timeZoneForICU = tz;

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

    m_dateStyle = intlOption<DateTimeStyle>(globalObject, options, vm.propertyNames->dateStyle, { { "full"_s, DateTimeStyle::Full }, { "long"_s, DateTimeStyle::Long }, { "medium"_s, DateTimeStyle::Medium }, { "short"_s, DateTimeStyle::Short } }, "dateStyle must be \"full\", \"long\", \"medium\", or \"short\""_s, DateTimeStyle::None);
    RETURN_IF_EXCEPTION(scope, void());

    m_timeStyle = intlOption<DateTimeStyle>(globalObject, options, vm.propertyNames->timeStyle, { { "full"_s, DateTimeStyle::Full }, { "long"_s, DateTimeStyle::Long }, { "medium"_s, DateTimeStyle::Medium }, { "short"_s, DateTimeStyle::Short } }, "timeStyle must be \"full\", \"long\", \"medium\", or \"short\""_s, DateTimeStyle::None);
    RETURN_IF_EXCEPTION(scope, void());

    Vector<UChar, 32> patternBuffer;
    if (m_dateStyle != DateTimeStyle::None || m_timeStyle != DateTimeStyle::None) {
        // 30. For each row in Table 1, except the header row, do
        //     i. Let prop be the name given in the Property column of the row.
        //     ii. Let p be opt.[[<prop>]].
        //     iii. If p is not undefined, then
        //         1. Throw a TypeError exception.
        if (weekday != Weekday::None || era != Era::None || year != Year::None || month != Month::None || day != Day::None || dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits != 0 || timeZoneName != TimeZoneName::None) {
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

        if (UNLIKELY(required == RequiredComponent::Date && m_timeStyle != DateTimeStyle::None)) {
            throwTypeError(globalObject, scope, "timeStyle is specified while formatting date is requested"_s);
            return;
        }

        if (UNLIKELY(required == RequiredComponent::Time && m_dateStyle != DateTimeStyle::None)) {
            throwTypeError(globalObject, scope, "dateStyle is specified while formatting time is requested"_s);
            return;
        }

        // We cannot use this UDateFormat directly yet because we need to enforce specified hourCycle.
        // First, we create UDateFormat via dateStyle and timeStyle. And then convert it to pattern string.
        // After updating this pattern string with hourCycle, we create a final UDateFormat with the updated pattern string.
        UErrorCode status = U_ZERO_ERROR;
        StringView timeZoneView(m_timeZoneForICU);
        auto dateFormatFromStyle = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_open(parseUDateFormatStyle(m_timeStyle), parseUDateFormatStyle(m_dateStyle), dataLocaleWithExtensions.data(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), nullptr, -1, &status));
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
        if (m_timeStyle != DateTimeStyle::None && (hourCycle != HourCycle::None || hour12 != TriState::Indeterminate)) {
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
                Vector<UChar, 32> skeleton;
                auto status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, patternBuffer.data(), patternBuffer.size(), skeleton);
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
                    return;
                }
                replaceHourCycleInSkeleton(skeleton, specifiedHour12);
                dataLogLnIf(IntlDateTimeFormatInternal::verbose, "replaced:(", StringView(skeleton.data(), skeleton.size()), ")");

                patternBuffer = vm.intlCache().getBestDateTimePattern(dataLocaleWithExtensions, skeleton.data(), skeleton.size(), status);
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
            if (dayPeriod != DayPeriod::None || hour != Hour::None || minute != Minute::None || second != Second::None || fractionalSecondDigits != 0)
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
        patternBuffer = vm.intlCache().getBestDateTimePattern(dataLocaleWithExtensions, StringView(skeleton).upconvertedCharacters().get(), skeleton.length(), status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
            return;
        }
    }

    // After generating pattern from skeleton, we need to change h11 vs. h12 and h23 vs. h24 if hourCycle is specified.
    if (hourCycle != HourCycle::None)
        replaceHourCycleInPattern(patternBuffer, hourCycle);

    StringView pattern(patternBuffer.data(), patternBuffer.size());
    setFormatsFromPattern(pattern);

    dataLogLnIf(IntlDateTimeFormatInternal::verbose, "locale:(", m_locale, "),dataLocale:(", dataLocaleWithExtensions, "),pattern:(", pattern, ")");

    UErrorCode status = U_ZERO_ERROR;
    StringView timeZoneView(m_timeZoneForICU);
    m_dateFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_open(UDAT_PATTERN, UDAT_PATTERN, dataLocaleWithExtensions.data(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), pattern.upconvertedCharacters(), pattern.length(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat"_s);
        return;
    }

    // Gregorian calendar should be used from the beginning of ECMAScript time.
    // Failure here means unsupported calendar, and can safely be ignored.
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(m_dateFormat.get()));
    ucal_setGregorianChange(cal, minECMAScriptTime, &status);
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

    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->calendar, jsNontrivialString(vm, m_calendar));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsNontrivialString(vm, m_numberingSystem));
    options->putDirect(vm, vm.propertyNames->timeZone, jsNontrivialString(vm, m_timeZone));

    if (m_hourCycle != HourCycle::None) {
        options->putDirect(vm, vm.propertyNames->hourCycle, jsNontrivialString(vm, hourCycleString(m_hourCycle)));
        options->putDirect(vm, vm.propertyNames->hour12, jsBoolean(m_hourCycle == HourCycle::H11 || m_hourCycle == HourCycle::H12));
    }

    if (m_dateStyle == DateTimeStyle::None && m_timeStyle == DateTimeStyle::None) {
        if (m_weekday != Weekday::None)
            options->putDirect(vm, vm.propertyNames->weekday, jsNontrivialString(vm, weekdayString(m_weekday)));

        if (m_era != Era::None)
            options->putDirect(vm, vm.propertyNames->era, jsNontrivialString(vm, eraString(m_era)));

        if (m_year != Year::None)
            options->putDirect(vm, vm.propertyNames->year, jsNontrivialString(vm, yearString(m_year)));

        if (m_month != Month::None)
            options->putDirect(vm, vm.propertyNames->month, jsNontrivialString(vm, monthString(m_month)));

        if (m_day != Day::None)
            options->putDirect(vm, vm.propertyNames->day, jsNontrivialString(vm, dayString(m_day)));

        if (m_dayPeriod != DayPeriod::None)
            options->putDirect(vm, vm.propertyNames->dayPeriod, jsNontrivialString(vm, dayPeriodString(m_dayPeriod)));

        if (m_hour != Hour::None)
            options->putDirect(vm, vm.propertyNames->hour, jsNontrivialString(vm, hourString(m_hour)));

        if (m_minute != Minute::None)
            options->putDirect(vm, vm.propertyNames->minute, jsNontrivialString(vm, minuteString(m_minute)));

        if (m_second != Second::None)
            options->putDirect(vm, vm.propertyNames->second, jsNontrivialString(vm, secondString(m_second)));

        if (m_fractionalSecondDigits)
            options->putDirect(vm, vm.propertyNames->fractionalSecondDigits, jsNumber(m_fractionalSecondDigits));

        if (m_timeZoneName != TimeZoneName::None)
            options->putDirect(vm, vm.propertyNames->timeZoneName, jsNontrivialString(vm, timeZoneNameString(m_timeZoneName)));
    } else {
        if (m_dateStyle != DateTimeStyle::None)
            options->putDirect(vm, vm.propertyNames->dateStyle, jsNontrivialString(vm, formatStyleString(m_dateStyle)));

        if (m_timeStyle != DateTimeStyle::None)
            options->putDirect(vm, vm.propertyNames->timeStyle, jsNontrivialString(vm, formatStyleString(m_timeStyle)));
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
static void replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(Container& vector)
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
    ASSERT(m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat format()"_s);

    Vector<UChar, 32> result;
    auto status = callBufferProducingFunction(udat_format, m_dateFormat.get(), value, result, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);

    return jsString(vm, String(WTFMove(result)));
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
    ASSERT(m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return throwRangeError(globalObject, scope, "date value is not finite in DateTimeFormat formatToParts()"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto fields = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<UChar, 32> result;
    status = callBufferProducingFunction(udat_formatForFields, m_dateFormat.get(), value, result, fields.get());
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format date value"_s);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(result);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    StringView resultStringView(result.data(), result.size());
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
            JSObject* part = constructEmptyObject(globalObject);
            part->putDirect(vm, vm.propertyNames->type, literalString);
            part->putDirect(vm, vm.propertyNames->value, value);
            if (sourceType)
                part->putDirect(vm, vm.propertyNames->source, sourceType);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;

        if (fieldType >= 0) {
            auto type = jsNontrivialString(vm, partTypeString(UDateFormatField(fieldType)));
            auto value = jsString(vm, resultStringView.substring(beginIndex, endIndex - beginIndex));
            JSObject* part = constructEmptyObject(globalObject);
            part->putDirect(vm, vm.propertyNames->type, type);
            part->putDirect(vm, vm.propertyNames->value, value);
            if (sourceType)
                part->putDirect(vm, vm.propertyNames->source, sourceType);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    return parts;
}

UDateIntervalFormat* IntlDateTimeFormat::createDateIntervalFormatIfNecessary(JSGlobalObject* globalObject)
{
    ASSERT(m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_dateIntervalFormat)
        return m_dateIntervalFormat.get();

    Vector<UChar, 32> pattern;
    {
        auto status = callBufferProducingFunction(udat_toPattern, m_dateFormat.get(), false, pattern);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
            return nullptr;
        }
    }

    Vector<UChar, 32> skeleton;
    {
        auto status = callBufferProducingFunction(udatpg_getSkeleton, nullptr, pattern.data(), pattern.size(), skeleton);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
            return nullptr;
        }
    }

    dataLogLnIf(IntlDateTimeFormatInternal::verbose, "interval format pattern:(", String(pattern), "),skeleton:(", String(skeleton), ")");

    // While the pattern is including right HourCycle patterns, UDateIntervalFormat does not follow.
    // We need to enforce HourCycle by setting "hc" extension if it is specified.
    StringBuilder localeBuilder;
    localeBuilder.append(m_dataLocale, "-u-ca-", m_calendar, "-nu-", m_numberingSystem);
    if (m_hourCycle != HourCycle::None)
        localeBuilder.append("-hc-", hourCycleString(m_hourCycle));
    CString dataLocaleWithExtensions = localeBuilder.toString().utf8();

    UErrorCode status = U_ZERO_ERROR;
    StringView timeZoneView(m_timeZoneForICU);
    m_dateIntervalFormat = std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>(udtitvfmt_open(dataLocaleWithExtensions.data(), skeleton.data(), skeleton.size(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
        return nullptr;
    }
    return m_dateIntervalFormat.get();
}

#if HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)

static std::unique_ptr<UFormattedDateInterval, ICUDeleter<udtitvfmt_closeResult>> formattedValueFromDateRange(UDateIntervalFormat& dateIntervalFormat, UDateFormat& dateFormat, double startDate, double endDate, UErrorCode& status)
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

#endif // HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)

JSValue IntlDateTimeFormat::formatRange(JSGlobalObject* globalObject, double startDate, double endDate)
{
    ASSERT(m_dateFormat);

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

#if HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)
    UErrorCode status = U_ZERO_ERROR;
    auto result = formattedValueFromDateRange(*dateIntervalFormat, *m_dateFormat, startDate, endDate, status);
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
    const UChar* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }
    Vector<UChar, 32> buffer(formattedStringPointer, formattedStringLength);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);

    return jsString(vm, String(WTFMove(buffer)));
#else
    Vector<UChar, 32> buffer;
    auto status = callBufferProducingFunction(udtitvfmt_format, dateIntervalFormat, startDate, endDate, buffer, nullptr);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);

    return jsString(vm, String(WTFMove(buffer)));
#endif
}

JSValue IntlDateTimeFormat::formatRangeToParts(JSGlobalObject* globalObject, double startDate, double endDate)
{
    ASSERT(m_dateFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

#if HAVE(ICU_U_DATE_INTERVAL_FORMAT_FORMAT_RANGE_TO_PARTS)
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
    auto result = formattedValueFromDateRange(*dateIntervalFormat, *m_dateFormat, startDate, endDate, status);
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
    const UChar* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }
    Vector<UChar, 32> buffer(formattedStringPointer, formattedStringLength);
    replaceNarrowNoBreakSpaceOrThinSpaceWithNormalSpace(buffer);

    StringView resultStringView(buffer.data(), buffer.size());

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
        JSObject* part = constructEmptyObject(globalObject);
        part->putDirect(vm, vm.propertyNames->type, type);
        part->putDirect(vm, vm.propertyNames->value, value);
        part->putDirect(vm, vm.propertyNames->source, sourceType(beginIndex));
        return part;
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
#else
    UNUSED_PARAM(startDate);
    UNUSED_PARAM(endDate);
    throwTypeError(globalObject, scope, "Failed to format date interval"_s);
    return { };
#endif
}


} // namespace JSC
