/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include "IntlCache.h"
#include "IntlObjectInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

static const double minECMAScriptTime = -8.64E15;

const ClassInfo IntlDateTimeFormat::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormat) };

namespace IntlDateTimeFormatInternal {
static constexpr bool verbose = false;
}

IntlDateTimeFormat* IntlDateTimeFormat::create(VM& vm, Structure* structure)
{
    IntlDateTimeFormat* format = new (NotNull, allocateCell<IntlDateTimeFormat>(vm.heap)) IntlDateTimeFormat(vm, structure);
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

void IntlDateTimeFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlDateTimeFormat::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlDateTimeFormat* thisObject = jsCast<IntlDateTimeFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundFormat);
}

void IntlDateTimeFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

static ALWAYS_INLINE bool isUTCEquivalent(StringView timeZone)
{
    return timeZone == "Etc/UTC" || timeZone == "Etc/GMT";
}

// https://tc39.es/ecma402/#sec-defaulttimezone
static String defaultTimeZone()
{
    String canonical;

    Vector<UChar, 32> buffer;
    auto status = callBufferProducingFunction(ucal_getDefaultTimeZone, buffer);
    if (U_SUCCESS(status)) {
        Vector<UChar, 32> canonicalBuffer;
        status = callBufferProducingFunction(ucal_getCanonicalTimeZoneID, buffer.data(), buffer.size(), canonicalBuffer, nullptr);
        if (U_SUCCESS(status))
            canonical = String(canonicalBuffer);
    }

    if (canonical.isNull() || isUTCEquivalent(canonical))
        return "UTC"_s;

    return canonical;
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
            // Ensure aliases used in language tag are allowed.
            if (calendar == "gregorian")
                keyLocaleData.append("gregory"_s);
            else if (calendar == "islamic-civil")
                keyLocaleData.append("islamicc"_s);
            else if (calendar == "ethiopic-amete-alem")
                keyLocaleData.append("ethioaa"_s);
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

static JSObject* toDateTimeOptionsAnyDate(JSGlobalObject* globalObject, JSValue originalOptions)
{
    // 12.1.1 ToDateTimeOptions abstract operation (ECMA-402 2.0)
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If options is undefined, then let options be null, else let options be ToObject(options).
    // 2. ReturnIfAbrupt(options).
    // 3. Let options be ObjectCreate(options).
    JSObject* options;
    if (originalOptions.isUndefined())
        options = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    else {
        JSObject* originalToObject = originalOptions.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        options = constructEmptyObject(globalObject, originalToObject);
    }

    // 4. Let needDefaults be true.
    bool needDefaults = true;

    // 5. If required is "date" or "any",
    // Always "any".

    // a. For each of the property names "weekday", "year", "month", "day":
    // i. Let prop be the property name.
    // ii. Let value be Get(options, prop).
    // iii. ReturnIfAbrupt(value).
    // iv. If value is not undefined, then let needDefaults be false.
    JSValue weekday = options->get(globalObject, vm.propertyNames->weekday);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!weekday.isUndefined())
        needDefaults = false;

    JSValue year = options->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!year.isUndefined())
        needDefaults = false;

    JSValue month = options->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!month.isUndefined())
        needDefaults = false;

    JSValue day = options->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!day.isUndefined())
        needDefaults = false;

    // 6. If required is "time" or "any",
    // Always "any".

    // a. For each of the property names ""dayPeriod", hour", "minute", "second", "fractionalSecondDigits":
    // i. Let prop be the property name.
    // ii. Let value be Get(options, prop).
    // iii. ReturnIfAbrupt(value).
    // iv. If value is not undefined, then let needDefaults be false.
    if (Options::useIntlDateTimeFormatDayPeriod()) {
        JSValue dayPeriod = options->get(globalObject, vm.propertyNames->dayPeriod);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!dayPeriod.isUndefined())
            needDefaults = false;
    }

    JSValue hour = options->get(globalObject, vm.propertyNames->hour);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!hour.isUndefined())
        needDefaults = false;

    JSValue minute = options->get(globalObject, vm.propertyNames->minute);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!minute.isUndefined())
        needDefaults = false;

    JSValue second = options->get(globalObject, vm.propertyNames->second);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!second.isUndefined())
        needDefaults = false;

    JSValue fractionalSecondDigits = options->get(globalObject, vm.propertyNames->fractionalSecondDigits);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!fractionalSecondDigits.isUndefined())
        needDefaults = false;

    JSValue dateStyle = options->get(globalObject, vm.propertyNames->dateStyle);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSValue timeStyle = options->get(globalObject, vm.propertyNames->timeStyle);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (!dateStyle.isUndefined() || !timeStyle.isUndefined())
        needDefaults = false;

    // 7. If needDefaults is true and defaults is either "date" or "all", then
    // Defaults is always "date".
    if (needDefaults) {
        // a. For each of the property names "year", "month", "day":
        // i. Let status be CreateDatePropertyOrThrow(options, prop, "numeric").
        // ii. ReturnIfAbrupt(status).
        JSString* numeric = jsNontrivialString(vm, "numeric"_s);

        options->putDirect(vm, vm.propertyNames->year, numeric);
        RETURN_IF_EXCEPTION(scope, nullptr);

        options->putDirect(vm, vm.propertyNames->month, numeric);
        RETURN_IF_EXCEPTION(scope, nullptr);

        options->putDirect(vm, vm.propertyNames->day, numeric);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    // 8. If needDefaults is true and defaults is either "time" or "all", then
    // Defaults is always "date". Ignore this branch.

    // 9. Return options.
    return options;
}

void IntlDateTimeFormat::setFormatsFromPattern(const StringView& pattern)
{
    // Get all symbols from the pattern, and set format fields accordingly.
    // http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
    unsigned length = pattern.length();
    for (unsigned i = 0; i < length; ++i) {
        UChar currentCharacter = pattern[i];
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
        case 'v':
        case 'V':
            if (count == 1)
                m_timeZoneName = TimeZoneName::Short;
            else if (count == 4)
                m_timeZoneName = TimeZoneName::Long;
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

inline IntlDateTimeFormat::HourCycle IntlDateTimeFormat::hourCycleFromPattern(const Vector<UChar, 32>& pattern)
{
    for (auto character : pattern) {
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
    for (auto& character : skeleton) {
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

    for (auto& character : pattern) {
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

// https://tc39.github.io/ecma402/#sec-initializedatetimeformat
void IntlDateTimeFormat::initializeDateTimeFormat(JSGlobalObject* globalObject, JSValue locales, JSValue originalOptions)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = toDateTimeOptionsAnyDate(globalObject, originalOptions);
    RETURN_IF_EXCEPTION(scope, void());

    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    String calendar = intlStringOption(globalObject, options, vm.propertyNames->calendar, { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!calendar.isNull()) {
        if (!isUnicodeLocaleIdentifierType(calendar)) {
            throwRangeError(globalObject, scope, "calendar is not a well-formed calendar value"_s);
            return;
        }
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Ca)] = calendar;
    }

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, nullptr, nullptr);
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
        // We are setting null String explicitly here (localeOptions' entries are Optional<String>). This leads us to use HourCycle::None later.
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Hc)] = String();
    }

    const HashSet<String>& availableLocales = intlDateTimeFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Ca, RelevantExtensionKey::Hc, RelevantExtensionKey::Nu }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize DateTimeFormat due to invalid locale"_s);
        return;
    }

    m_calendar = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Ca)];
    if (m_calendar == "gregorian")
        m_calendar = "gregory"_s;
    else if (m_calendar == "islamicc")
        m_calendar = "islamic-civil"_s;
    else if (m_calendar == "ethioaa")
        m_calendar = "ethiopic-amete-alem"_s;

    hourCycle = parseHourCycle(resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Hc)]);
    m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];
    m_dataLocale = resolved.dataLocale;
    CString dataLocaleWithExtensions = makeString(m_dataLocale, "-u-ca-", m_calendar, "-nu-", m_numberingSystem).utf8();

    JSValue tzValue = options->get(globalObject, vm.propertyNames->timeZone);
    RETURN_IF_EXCEPTION(scope, void());
    String tz;
    if (!tzValue.isUndefined()) {
        String originalTz = tzValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        tz = canonicalizeTimeZoneName(originalTz);
        if (tz.isNull()) {
            throwRangeError(globalObject, scope, "invalid time zone: " + originalTz);
            return;
        }
    } else
        tz = defaultTimeZone();
    m_timeZone = tz;

    StringBuilder skeletonBuilder;

    Weekday weekday = intlOption<Weekday>(globalObject, options, vm.propertyNames->weekday, { { "narrow"_s, Weekday::Narrow }, { "short"_s, Weekday::Short }, { "long"_s, Weekday::Long } }, "weekday must be \"narrow\", \"short\", or \"long\""_s, Weekday::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (weekday) {
    case Weekday::Narrow:
        skeletonBuilder.appendLiteral("EEEEE");
        break;
    case Weekday::Short:
        skeletonBuilder.appendLiteral("EEE");
        break;
    case Weekday::Long:
        skeletonBuilder.appendLiteral("EEEE");
        break;
    case Weekday::None:
        break;
    }

    Era era = intlOption<Era>(globalObject, options, vm.propertyNames->era, { { "narrow"_s, Era::Narrow }, { "short"_s, Era::Short }, { "long"_s, Era::Long } }, "era must be \"narrow\", \"short\", or \"long\""_s, Era::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (era) {
    case Era::Narrow:
        skeletonBuilder.appendLiteral("GGGGG");
        break;
    case Era::Short:
        skeletonBuilder.appendLiteral("GGG");
        break;
    case Era::Long:
        skeletonBuilder.appendLiteral("GGGG");
        break;
    case Era::None:
        break;
    }

    Year year = intlOption<Year>(globalObject, options, vm.propertyNames->year, { { "2-digit"_s, Year::TwoDigit }, { "numeric"_s, Year::Numeric } }, "year must be \"2-digit\" or \"numeric\""_s, Year::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (year) {
    case Year::TwoDigit:
        skeletonBuilder.appendLiteral("yy");
        break;
    case Year::Numeric:
        skeletonBuilder.append('y');
        break;
    case Year::None:
        break;
    }

    Month month = intlOption<Month>(globalObject, options, vm.propertyNames->month, { { "2-digit"_s, Month::TwoDigit }, { "numeric"_s, Month::Numeric }, { "narrow"_s, Month::Narrow }, { "short"_s, Month::Short }, { "long"_s, Month::Long } }, "month must be \"2-digit\", \"numeric\", \"narrow\", \"short\", or \"long\""_s, Month::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (month) {
    case Month::TwoDigit:
        skeletonBuilder.appendLiteral("MM");
        break;
    case Month::Numeric:
        skeletonBuilder.append('M');
        break;
    case Month::Narrow:
        skeletonBuilder.appendLiteral("MMMMM");
        break;
    case Month::Short:
        skeletonBuilder.appendLiteral("MMM");
        break;
    case Month::Long:
        skeletonBuilder.appendLiteral("MMMM");
        break;
    case Month::None:
        break;
    }

    Day day = intlOption<Day>(globalObject, options, vm.propertyNames->day, { { "2-digit"_s, Day::TwoDigit }, { "numeric"_s, Day::Numeric } }, "day must be \"2-digit\" or \"numeric\""_s, Day::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (day) {
    case Day::TwoDigit:
        skeletonBuilder.appendLiteral("dd");
        break;
    case Day::Numeric:
        skeletonBuilder.append('d');
        break;
    case Day::None:
        break;
    }

    DayPeriod dayPeriod = DayPeriod::None;
    if (Options::useIntlDateTimeFormatDayPeriod()) {
        dayPeriod = intlOption<DayPeriod>(globalObject, options, vm.propertyNames->dayPeriod, { { "narrow"_s, DayPeriod::Narrow }, { "short"_s, DayPeriod::Short }, { "long"_s, DayPeriod::Long } }, "dayPeriod must be \"narrow\", \"short\", or \"long\""_s, DayPeriod::None);
        RETURN_IF_EXCEPTION(scope, void());
    }

    Hour hour = intlOption<Hour>(globalObject, options, vm.propertyNames->hour, { { "2-digit"_s, Hour::TwoDigit }, { "numeric"_s, Hour::Numeric } }, "hour must be \"2-digit\" or \"numeric\""_s, Hour::None);
    RETURN_IF_EXCEPTION(scope, void());
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

    if (Options::useIntlDateTimeFormatDayPeriod()) {
        // dayPeriod must be set after setting hour.
        // https://unicode-org.atlassian.net/browse/ICU-20731
        switch (dayPeriod) {
        case DayPeriod::Narrow:
            skeletonBuilder.appendLiteral("BBBBB");
            break;
        case DayPeriod::Short:
            skeletonBuilder.append('B');
            break;
        case DayPeriod::Long:
            skeletonBuilder.appendLiteral("BBBB");
            break;
        case DayPeriod::None:
            break;
        }
    }

    Minute minute = intlOption<Minute>(globalObject, options, vm.propertyNames->minute, { { "2-digit"_s, Minute::TwoDigit }, { "numeric"_s, Minute::Numeric } }, "minute must be \"2-digit\" or \"numeric\""_s, Minute::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (minute) {
    case Minute::TwoDigit:
        skeletonBuilder.appendLiteral("mm");
        break;
    case Minute::Numeric:
        skeletonBuilder.append('m');
        break;
    case Minute::None:
        break;
    }

    Second second = intlOption<Second>(globalObject, options, vm.propertyNames->second, { { "2-digit"_s, Second::TwoDigit }, { "numeric"_s, Second::Numeric } }, "second must be \"2-digit\" or \"numeric\""_s, Second::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (second) {
    case Second::TwoDigit:
        skeletonBuilder.appendLiteral("ss");
        break;
    case Second::Numeric:
        skeletonBuilder.append('s');
        break;
    case Second::None:
        break;
    }

    unsigned fractionalSecondDigits = intlNumberOption(globalObject, options, vm.propertyNames->fractionalSecondDigits, 1, 3, 0);
    RETURN_IF_EXCEPTION(scope, void());
    for (unsigned i = 0; i < fractionalSecondDigits; ++i)
        skeletonBuilder.append('S');

    TimeZoneName timeZoneName = intlOption<TimeZoneName>(globalObject, options, vm.propertyNames->timeZoneName, { { "short"_s, TimeZoneName::Short }, { "long"_s, TimeZoneName::Long } }, "timeZoneName must be \"short\" or \"long\""_s, TimeZoneName::None);
    RETURN_IF_EXCEPTION(scope, void());
    switch (timeZoneName) {
    case TimeZoneName::Short:
        skeletonBuilder.append('z');
        break;
    case TimeZoneName::Long:
        skeletonBuilder.appendLiteral("zzzz");
        break;
    case TimeZoneName::None:
        break;
    }

    intlStringOption(globalObject, options, vm.propertyNames->formatMatcher, { "basic", "best fit" }, "formatMatcher must be either \"basic\" or \"best fit\"", "best fit");
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

        // We cannot use this UDateFormat directly yet because we need to enforce specified hourCycle.
        // First, we create UDateFormat via dateStyle and timeStyle. And then convert it to pattern string.
        // After updating this pattern string with hourCycle, we create a final UDateFormat with the updated pattern string.
        UErrorCode status = U_ZERO_ERROR;
        StringView timeZoneView(m_timeZone);
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
        UErrorCode status = U_ZERO_ERROR;
        String skeleton = skeletonBuilder.toString();
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
    StringView timeZoneView(m_timeZone);
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

ASCIILiteral IntlDateTimeFormat::timeZoneNameString(TimeZoneName timeZoneName)
{
    switch (timeZoneName) {
    case TimeZoneName::Short:
        return "short"_s;
    case TimeZoneName::Long:
        return "long"_s;
    case TimeZoneName::None:
        ASSERT_NOT_REACHED();
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        return ASCIILiteral::null();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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

    if (Options::useIntlDateTimeFormatDayPeriod()) {
        if (m_dayPeriod != DayPeriod::None)
            options->putDirect(vm, vm.propertyNames->dayPeriod, jsNontrivialString(vm, dayPeriodString(m_dayPeriod)));
    }

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

    if (m_dateStyle != DateTimeStyle::None)
        options->putDirect(vm, vm.propertyNames->dateStyle, jsNontrivialString(vm, formatStyleString(m_dateStyle)));

    if (m_timeStyle != DateTimeStyle::None)
        options->putDirect(vm, vm.propertyNames->timeStyle, jsNontrivialString(vm, formatStyleString(m_timeStyle)));

    return options;
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

    return jsString(vm, String(result));
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
JSValue IntlDateTimeFormat::formatToParts(JSGlobalObject* globalObject, double value) const
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

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    auto resultString = String(result);
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
            auto value = jsString(vm, resultString.substring(previousEndIndex, beginIndex - previousEndIndex));
            JSObject* part = constructEmptyObject(globalObject);
            part->putDirect(vm, vm.propertyNames->type, literalString);
            part->putDirect(vm, vm.propertyNames->value, value);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;

        if (fieldType >= 0) {
            auto type = jsString(vm, partTypeString(UDateFormatField(fieldType)));
            auto value = jsString(vm, resultString.substring(beginIndex, endIndex - beginIndex));
            JSObject* part = constructEmptyObject(globalObject);
            part->putDirect(vm, vm.propertyNames->type, type);
            part->putDirect(vm, vm.propertyNames->value, value);
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
    StringView timeZoneView(m_timeZone);
    m_dateIntervalFormat = std::unique_ptr<UDateIntervalFormat, UDateIntervalFormatDeleter>(udtitvfmt_open(dataLocaleWithExtensions.data(), skeleton.data(), skeleton.size(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize DateIntervalFormat"_s);
        return nullptr;
    }
    return m_dateIntervalFormat.get();
}

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

    Vector<UChar, 32> buffer;
    auto status = callBufferProducingFunction(udtitvfmt_format, dateIntervalFormat, startDate, endDate, buffer, nullptr);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format date interval"_s);
        return { };
    }

    return jsString(vm, String(buffer));
}

} // namespace JSC
