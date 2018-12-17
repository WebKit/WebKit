/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if ENABLE(INTL)

#include "DateInstance.h"
#include "Error.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <unicode/ucal.h>
#include <unicode/udatpg.h>
#include <unicode/uenum.h>
#include <wtf/text/StringBuilder.h>

#if JSC_ICU_HAS_UFIELDPOSITER
#include <unicode/ufieldpositer.h>
#endif

namespace JSC {

static const double minECMAScriptTime = -8.64E15;

const ClassInfo IntlDateTimeFormat::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormat) };

namespace IntlDTFInternal {
static const char* const relevantExtensionKeys[3] = { "ca", "nu", "hc" };
}

static const size_t indexOfExtensionKeyCa = 0;
static const size_t indexOfExtensionKeyNu = 1;
static const size_t indexOfExtensionKeyHc = 2;

void IntlDateTimeFormat::UDateFormatDeleter::operator()(UDateFormat* dateFormat) const
{
    if (dateFormat)
        udat_close(dateFormat);
}

#if JSC_ICU_HAS_UFIELDPOSITER
void IntlDateTimeFormat::UFieldPositionIteratorDeleter::operator()(UFieldPositionIterator* iterator) const
{
    if (iterator)
        ufieldpositer_close(iterator);
}
#endif

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
    : JSDestructibleObject(vm, structure)
{
}

void IntlDateTimeFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlDateTimeFormat::destroy(JSCell* cell)
{
    static_cast<IntlDateTimeFormat*>(cell)->IntlDateTimeFormat::~IntlDateTimeFormat();
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

static String defaultTimeZone()
{
    // 6.4.3 DefaultTimeZone () (ECMA-402 2.0)
    // The DefaultTimeZone abstract operation returns a String value representing the valid (6.4.1) and canonicalized (6.4.2) time zone name for the host environment’s current time zone.

    UErrorCode status = U_ZERO_ERROR;
    Vector<UChar, 32> buffer(32);
    auto bufferLength = ucal_getDefaultTimeZone(buffer.data(), buffer.size(), &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        buffer.grow(bufferLength);
        ucal_getDefaultTimeZone(buffer.data(), bufferLength, &status);
    }
    if (U_SUCCESS(status)) {
        status = U_ZERO_ERROR;
        Vector<UChar, 32> canonicalBuffer(32);
        auto canonicalLength = ucal_getCanonicalTimeZoneID(buffer.data(), bufferLength, canonicalBuffer.data(), canonicalBuffer.size(), nullptr, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR) {
            status = U_ZERO_ERROR;
            canonicalBuffer.grow(canonicalLength);
            ucal_getCanonicalTimeZoneID(buffer.data(), bufferLength, canonicalBuffer.data(), canonicalLength, nullptr, &status);
        }
        if (U_SUCCESS(status))
            return String(canonicalBuffer.data(), canonicalLength);
    }

    return "UTC"_s;
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
        // Time zone names are respresented as UChar[] in all related ICU apis.
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

        Vector<UChar, 32> buffer(ianaTimeZoneLength);
        status = U_ZERO_ERROR;
        auto canonicalLength = ucal_getCanonicalTimeZoneID(ianaTimeZone, ianaTimeZoneLength, buffer.data(), ianaTimeZoneLength, nullptr, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR) {
            buffer.grow(canonicalLength);
            status = U_ZERO_ERROR;
            ucal_getCanonicalTimeZoneID(ianaTimeZone, ianaTimeZoneLength, buffer.data(), canonicalLength, nullptr, &status);
        }
        ASSERT(U_SUCCESS(status));
        canonical = String(buffer.data(), canonicalLength);
    } while (canonical.isNull());
    uenum_close(timeZones);

    // 3. If ianaTimeZone is "Etc/UTC" or "Etc/GMT", then return "UTC".
    if (canonical == "Etc/UTC" || canonical == "Etc/GMT")
        canonical = "UTC"_s;

    // 4. Return ianaTimeZone.
    return canonical;
}

namespace IntlDTFInternal {
static Vector<String> localeData(const String& locale, size_t keyIndex)
{
    Vector<String> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCa: {
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
    case indexOfExtensionKeyNu:
        keyLocaleData = numberingSystemsForLocale(locale);
        break;
    case indexOfExtensionKeyHc:
        // Null default so we know to use 'j' in pattern.
        keyLocaleData.append(String());
        keyLocaleData.append("h11"_s);
        keyLocaleData.append("h12"_s);
        keyLocaleData.append("h23"_s);
        keyLocaleData.append("h24"_s);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

static JSObject* toDateTimeOptionsAnyDate(ExecState& exec, JSValue originalOptions)
{
    // 12.1.1 ToDateTimeOptions abstract operation (ECMA-402 2.0)
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If options is undefined, then let options be null, else let options be ToObject(options).
    // 2. ReturnIfAbrupt(options).
    // 3. Let options be ObjectCreate(options).
    JSObject* options;
    if (originalOptions.isUndefined())
        options = constructEmptyObject(&exec, exec.lexicalGlobalObject()->nullPrototypeObjectStructure());
    else {
        JSObject* originalToObject = originalOptions.toObject(&exec);
        RETURN_IF_EXCEPTION(scope, nullptr);
        options = constructEmptyObject(&exec, originalToObject);
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
    JSValue weekday = options->get(&exec, vm.propertyNames->weekday);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!weekday.isUndefined())
        needDefaults = false;

    JSValue year = options->get(&exec, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!year.isUndefined())
        needDefaults = false;

    JSValue month = options->get(&exec, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!month.isUndefined())
        needDefaults = false;

    JSValue day = options->get(&exec, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!day.isUndefined())
        needDefaults = false;

    // 6. If required is "time" or "any",
    // Always "any".

    // a. For each of the property names "hour", "minute", "second":
    // i. Let prop be the property name.
    // ii. Let value be Get(options, prop).
    // iii. ReturnIfAbrupt(value).
    // iv. If value is not undefined, then let needDefaults be false.
    JSValue hour = options->get(&exec, vm.propertyNames->hour);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!hour.isUndefined())
        needDefaults = false;

    JSValue minute = options->get(&exec, vm.propertyNames->minute);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!minute.isUndefined())
        needDefaults = false;

    JSValue second = options->get(&exec, vm.propertyNames->second);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!second.isUndefined())
        needDefaults = false;

    // 7. If needDefaults is true and defaults is either "date" or "all", then
    // Defaults is always "date".
    if (needDefaults) {
        // a. For each of the property names "year", "month", "day":
        // i. Let status be CreateDatePropertyOrThrow(options, prop, "numeric").
        // ii. ReturnIfAbrupt(status).
        JSString* numeric = jsNontrivialString(&exec, "numeric"_s);

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

        // If hourCycle was null, this sets it to the locale default.
        if (m_hourCycle.isNull()) {
            if (currentCharacter == 'h')
                m_hourCycle = "h12"_s;
            else if (currentCharacter == 'H')
                m_hourCycle = "h23"_s;
            else if (currentCharacter == 'k')
                m_hourCycle = "h24"_s;
            else if (currentCharacter == 'K')
                m_hourCycle = "h11"_s;
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
        case 'h':
        case 'H':
        case 'k':
        case 'K':
            if (count == 1)
                m_hour = Hour::Numeric;
            else if (count == 2)
                m_hour = Hour::TwoDigit;
            break;
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
        }
    }
}

void IntlDateTimeFormat::initializeDateTimeFormat(ExecState& exec, JSValue locales, JSValue originalOptions)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.1.1 InitializeDateTimeFormat (dateTimeFormat, locales, options) (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-initializedatetimeformat

    Vector<String> requestedLocales = canonicalizeLocaleList(exec, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = IntlDTFInternal::toDateTimeOptionsAnyDate(exec, originalOptions);
    RETURN_IF_EXCEPTION(scope, void());

    HashMap<String, String> opt;

    String localeMatcher = intlStringOption(exec, options, vm.propertyNames->localeMatcher, { "lookup", "best fit" }, "localeMatcher must be either \"lookup\" or \"best fit\"", "best fit");
    RETURN_IF_EXCEPTION(scope, void());
    opt.add(vm.propertyNames->localeMatcher.string(), localeMatcher);

    bool isHour12Undefined;
    bool hour12 = intlBooleanOption(exec, options, vm.propertyNames->hour12, isHour12Undefined);
    RETURN_IF_EXCEPTION(scope, void());

    String hourCycle = intlStringOption(exec, options, vm.propertyNames->hourCycle, { "h11", "h12", "h23", "h24" }, "hourCycle must be \"h11\", \"h12\", \"h23\", or \"h24\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (isHour12Undefined) {
        // Set hour12 here to simplify hour logic later.
        hour12 = (hourCycle == "h11" || hourCycle == "h12");
        if (!hourCycle.isNull())
            opt.add("hc"_s, hourCycle);
    } else
        opt.add("hc"_s, String());

    const HashSet<String> availableLocales = exec.jsCallee()->globalObject(vm)->intlDateTimeFormatAvailableLocales();
    HashMap<String, String> resolved = resolveLocale(exec, availableLocales, requestedLocales, opt, IntlDTFInternal::relevantExtensionKeys, WTF_ARRAY_LENGTH(IntlDTFInternal::relevantExtensionKeys), IntlDTFInternal::localeData);

    m_locale = resolved.get(vm.propertyNames->locale.string());
    if (m_locale.isEmpty()) {
        throwTypeError(&exec, scope, "failed to initialize DateTimeFormat due to invalid locale"_s);
        return;
    }

    m_calendar = resolved.get("ca"_s);
    if (m_calendar == "gregorian")
        m_calendar = "gregory"_s;
    else if (m_calendar == "islamicc")
        m_calendar = "islamic-civil"_s;
    else if (m_calendar == "ethioaa")
        m_calendar = "ethiopic-amete-alem"_s;

    m_hourCycle = resolved.get("hc"_s);
    m_numberingSystem = resolved.get("nu"_s);
    String dataLocale = resolved.get("dataLocale"_s);

    JSValue tzValue = options->get(&exec, vm.propertyNames->timeZone);
    RETURN_IF_EXCEPTION(scope, void());
    String tz;
    if (!tzValue.isUndefined()) {
        String originalTz = tzValue.toWTFString(&exec);
        RETURN_IF_EXCEPTION(scope, void());
        tz = canonicalizeTimeZoneName(originalTz);
        if (tz.isNull()) {
            throwRangeError(&exec, scope, String::format("invalid time zone: %s", originalTz.utf8().data()));
            return;
        }
    } else
        tz = defaultTimeZone();
    m_timeZone = tz;

    StringBuilder skeletonBuilder;
    auto narrowShortLong = { "narrow", "short", "long" };
    auto twoDigitNumeric = { "2-digit", "numeric" };
    auto twoDigitNumericNarrowShortLong = { "2-digit", "numeric", "narrow", "short", "long" };
    auto shortLong = { "short", "long" };

    String weekday = intlStringOption(exec, options, vm.propertyNames->weekday, narrowShortLong, "weekday must be \"narrow\", \"short\", or \"long\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!weekday.isNull()) {
        if (weekday == "narrow")
            skeletonBuilder.appendLiteral("EEEEE");
        else if (weekday == "short")
            skeletonBuilder.appendLiteral("EEE");
        else if (weekday == "long")
            skeletonBuilder.appendLiteral("EEEE");
    }

    String era = intlStringOption(exec, options, vm.propertyNames->era, narrowShortLong, "era must be \"narrow\", \"short\", or \"long\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!era.isNull()) {
        if (era == "narrow")
            skeletonBuilder.appendLiteral("GGGGG");
        else if (era == "short")
            skeletonBuilder.appendLiteral("GGG");
        else if (era == "long")
            skeletonBuilder.appendLiteral("GGGG");
    }

    String year = intlStringOption(exec, options, vm.propertyNames->year, twoDigitNumeric, "year must be \"2-digit\" or \"numeric\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!year.isNull()) {
        if (year == "2-digit")
            skeletonBuilder.appendLiteral("yy");
        else if (year == "numeric")
            skeletonBuilder.append('y');
    }

    String month = intlStringOption(exec, options, vm.propertyNames->month, twoDigitNumericNarrowShortLong, "month must be \"2-digit\", \"numeric\", \"narrow\", \"short\", or \"long\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!month.isNull()) {
        if (month == "2-digit")
            skeletonBuilder.appendLiteral("MM");
        else if (month == "numeric")
            skeletonBuilder.append('M');
        else if (month == "narrow")
            skeletonBuilder.appendLiteral("MMMMM");
        else if (month == "short")
            skeletonBuilder.appendLiteral("MMM");
        else if (month == "long")
            skeletonBuilder.appendLiteral("MMMM");
    }

    String day = intlStringOption(exec, options, vm.propertyNames->day, twoDigitNumeric, "day must be \"2-digit\" or \"numeric\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!day.isNull()) {
        if (day == "2-digit")
            skeletonBuilder.appendLiteral("dd");
        else if (day == "numeric")
            skeletonBuilder.append('d');
    }

    String hour = intlStringOption(exec, options, vm.propertyNames->hour, twoDigitNumeric, "hour must be \"2-digit\" or \"numeric\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (hour == "2-digit") {
        if (isHour12Undefined && m_hourCycle.isNull())
            skeletonBuilder.appendLiteral("jj");
        else if (hour12)
            skeletonBuilder.appendLiteral("hh");
        else
            skeletonBuilder.appendLiteral("HH");
    } else if (hour == "numeric") {
        if (isHour12Undefined && m_hourCycle.isNull())
            skeletonBuilder.append('j');
        else if (hour12)
            skeletonBuilder.append('h');
        else
            skeletonBuilder.append('H');
    } else
        m_hourCycle = String();

    String minute = intlStringOption(exec, options, vm.propertyNames->minute, twoDigitNumeric, "minute must be \"2-digit\" or \"numeric\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!minute.isNull()) {
        if (minute == "2-digit")
            skeletonBuilder.appendLiteral("mm");
        else if (minute == "numeric")
            skeletonBuilder.append('m');
    }

    String second = intlStringOption(exec, options, vm.propertyNames->second, twoDigitNumeric, "second must be \"2-digit\" or \"numeric\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!second.isNull()) {
        if (second == "2-digit")
            skeletonBuilder.appendLiteral("ss");
        else if (second == "numeric")
            skeletonBuilder.append('s');
    }

    String timeZoneName = intlStringOption(exec, options, vm.propertyNames->timeZoneName, shortLong, "timeZoneName must be \"short\" or \"long\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!timeZoneName.isNull()) {
        if (timeZoneName == "short")
            skeletonBuilder.append('z');
        else if (timeZoneName == "long")
            skeletonBuilder.appendLiteral("zzzz");
    }

    intlStringOption(exec, options, vm.propertyNames->formatMatcher, { "basic", "best fit" }, "formatMatcher must be either \"basic\" or \"best fit\"", "best fit");
    RETURN_IF_EXCEPTION(scope, void());

    // Always use ICU date format generator, rather than our own pattern list and matcher.
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* generator = udatpg_open(dataLocale.utf8().data(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(&exec, scope, "failed to initialize DateTimeFormat"_s);
        return;
    }

    String skeleton = skeletonBuilder.toString();
    StringView skeletonView(skeleton);
    Vector<UChar, 32> patternBuffer(32);
    status = U_ZERO_ERROR;
    auto patternLength = udatpg_getBestPattern(generator, skeletonView.upconvertedCharacters(), skeletonView.length(), patternBuffer.data(), patternBuffer.size(), &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        patternBuffer.grow(patternLength);
        udatpg_getBestPattern(generator, skeletonView.upconvertedCharacters(), skeletonView.length(), patternBuffer.data(), patternLength, &status);
    }
    udatpg_close(generator);
    if (U_FAILURE(status)) {
        throwTypeError(&exec, scope, "failed to initialize DateTimeFormat"_s);
        return;
    }

    // Enforce our hourCycle, replacing hour characters in pattern.
    if (!m_hourCycle.isNull()) {
        UChar hour = 'H';
        if (m_hourCycle == "h11")
            hour = 'K';
        else if (m_hourCycle == "h12")
            hour = 'h';
        else if (m_hourCycle == "h24")
            hour = 'k';

        bool isEscaped = false;
        bool hasHour = false;
        for (auto i = 0; i < patternLength; ++i) {
            UChar c = patternBuffer[i];
            if (c == '\'')
                isEscaped = !isEscaped;
            else if (!isEscaped && (c == 'h' || c == 'H' || c == 'k' || c == 'K')) {
                patternBuffer[i] = hour;
                hasHour = true;
            }
        }
        if (!hasHour)
            m_hourCycle = String();
    }

    StringView pattern(patternBuffer.data(), patternLength);
    setFormatsFromPattern(pattern);

    status = U_ZERO_ERROR;
    StringView timeZoneView(m_timeZone);
    m_dateFormat = std::unique_ptr<UDateFormat, UDateFormatDeleter>(udat_open(UDAT_PATTERN, UDAT_PATTERN, m_locale.utf8().data(), timeZoneView.upconvertedCharacters(), timeZoneView.length(), pattern.upconvertedCharacters(), pattern.length(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(&exec, scope, "failed to initialize DateTimeFormat"_s);
        return;
    }

    // Gregorian calendar should be used from the beginning of ECMAScript time.
    // Failure here means unsupported calendar, and can safely be ignored.
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(m_dateFormat.get()));
    ucal_setGregorianChange(cal, minECMAScriptTime, &status);

    m_initializedDateTimeFormat = true;
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

JSObject* IntlDateTimeFormat::resolvedOptions(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions() (ECMA-402 2.0)
    // The function returns a new object whose properties and attributes are set as if constructed by an object literal assigning to each of the following properties the value of the corresponding internal slot of this DateTimeFormat object (see 12.4): locale, calendar, numberingSystem, timeZone, hour12, weekday, era, year, month, day, hour, minute, second, and timeZoneName. Properties whose corresponding internal slots are not present are not assigned.
    // Note: In this version of the ECMAScript 2015 Internationalization API, the timeZone property will be the name of the default time zone if no timeZone property was provided in the options object provided to the Intl.DateTimeFormat constructor. The previous version left the timeZone property undefined in this case.
    if (!m_initializedDateTimeFormat) {
        initializeDateTimeFormat(exec, jsUndefined(), jsUndefined());
        scope.assertNoException();
    }

    JSObject* options = constructEmptyObject(&exec);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(&exec, m_locale));
    options->putDirect(vm, vm.propertyNames->calendar, jsNontrivialString(&exec, m_calendar));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsNontrivialString(&exec, m_numberingSystem));
    options->putDirect(vm, vm.propertyNames->timeZone, jsNontrivialString(&exec, m_timeZone));

    if (m_weekday != Weekday::None)
        options->putDirect(vm, vm.propertyNames->weekday, jsNontrivialString(&exec, weekdayString(m_weekday)));

    if (m_era != Era::None)
        options->putDirect(vm, vm.propertyNames->era, jsNontrivialString(&exec, eraString(m_era)));

    if (m_year != Year::None)
        options->putDirect(vm, vm.propertyNames->year, jsNontrivialString(&exec, yearString(m_year)));

    if (m_month != Month::None)
        options->putDirect(vm, vm.propertyNames->month, jsNontrivialString(&exec, monthString(m_month)));

    if (m_day != Day::None)
        options->putDirect(vm, vm.propertyNames->day, jsNontrivialString(&exec, dayString(m_day)));

    if (m_hour != Hour::None)
        options->putDirect(vm, vm.propertyNames->hour, jsNontrivialString(&exec, hourString(m_hour)));

    if (!m_hourCycle.isNull()) {
        options->putDirect(vm, vm.propertyNames->hourCycle, jsNontrivialString(&exec, m_hourCycle));
        options->putDirect(vm, vm.propertyNames->hour12, jsBoolean(m_hourCycle == "h11" || m_hourCycle == "h12"));
    }

    if (m_minute != Minute::None)
        options->putDirect(vm, vm.propertyNames->minute, jsNontrivialString(&exec, minuteString(m_minute)));

    if (m_second != Second::None)
        options->putDirect(vm, vm.propertyNames->second, jsNontrivialString(&exec, secondString(m_second)));

    if (m_timeZoneName != TimeZoneName::None)
        options->putDirect(vm, vm.propertyNames->timeZoneName, jsNontrivialString(&exec, timeZoneNameString(m_timeZoneName)));

    return options;
}

JSValue IntlDateTimeFormat::format(ExecState& exec, double value)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.4 FormatDateTime abstract operation (ECMA-402 2.0)
    if (!m_initializedDateTimeFormat) {
        initializeDateTimeFormat(exec, jsUndefined(), jsUndefined());
        scope.assertNoException();
    }

    // 1. If x is not a finite Number, then throw a RangeError exception.
    if (!std::isfinite(value))
        return throwRangeError(&exec, scope, "date value is not finite in DateTimeFormat format()"_s);

    // Delegate remaining steps to ICU.
    UErrorCode status = U_ZERO_ERROR;
    Vector<UChar, 32> result(32);
    auto resultLength = udat_format(m_dateFormat.get(), value, result.data(), result.size(), nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        result.grow(resultLength);
        udat_format(m_dateFormat.get(), value, result.data(), resultLength, nullptr, &status);
    }
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to format date value"_s);

    return jsString(&exec, String(result.data(), resultLength));
}

#if JSC_ICU_HAS_UFIELDPOSITER
ASCIILiteral IntlDateTimeFormat::partTypeString(UDateFormatField field)
{
    switch (field) {
    case UDAT_ERA_FIELD:
        return "era"_s;
    case UDAT_YEAR_FIELD:
    case UDAT_YEAR_NAME_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
        return "year"_s;
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
    case UDAT_FRACTIONAL_SECOND_FIELD:
        return "second"_s;
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
        return "weekday"_s;
    case UDAT_AM_PM_FIELD:
#if U_ICU_VERSION_MAJOR_NUM >= 57
    case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
    case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
#endif
        return "dayPeriod"_s;
    case UDAT_TIMEZONE_FIELD:
    case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD:
    case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
    case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
        return "timeZoneName"_s;
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
    case UDAT_RELATED_YEAR_FIELD:
    case UDAT_TIME_SEPARATOR_FIELD:
#if U_ICU_VERSION_MAJOR_NUM < 58 || !defined(U_HIDE_DEPRECATED_API)
    case UDAT_FIELD_COUNT:
#endif
        return "unknown"_s;
    }
    // Any newer additions to the UDateFormatField enum should just be considered an "unknown" part.
    return "unknown"_s;
}


JSValue IntlDateTimeFormat::formatToParts(ExecState& exec, double value)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.1.8 FormatDateTimeToParts (ECMA-402 4.0)
    // https://tc39.github.io/ecma402/#sec-formatdatetimetoparts

    if (!std::isfinite(value))
        return throwRangeError(&exec, scope, "date value is not finite in DateTimeFormat formatToParts()"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto fields = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to open field position iterator"_s);

    status = U_ZERO_ERROR;
    Vector<UChar, 32> result(32);
    auto resultLength = udat_formatForFields(m_dateFormat.get(), value, result.data(), result.size(), fields.get(), &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        result.grow(resultLength);
        udat_formatForFields(m_dateFormat.get(), value, result.data(), resultLength, fields.get(), &status);
    }
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to format date value"_s);

    JSGlobalObject* globalObject = exec.jsCallee()->globalObject(vm);
    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(&exec, scope);

    auto resultString = String(result.data(), resultLength);
    auto typePropertyName = Identifier::fromString(&vm, "type");
    auto literalString = jsString(&exec, "literal"_s);

    int32_t previousEndIndex = 0;
    int32_t beginIndex = 0;
    int32_t endIndex = 0;
    while (previousEndIndex < resultLength) {
        auto fieldType = ufieldpositer_next(fields.get(), &beginIndex, &endIndex);
        if (fieldType < 0)
            beginIndex = endIndex = resultLength;

        if (previousEndIndex < beginIndex) {
            auto value = jsString(&exec, resultString.substring(previousEndIndex, beginIndex - previousEndIndex));
            JSObject* part = constructEmptyObject(&exec);
            part->putDirect(vm, typePropertyName, literalString);
            part->putDirect(vm, vm.propertyNames->value, value);
            parts->push(&exec, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;

        if (fieldType >= 0) {
            auto type = jsString(&exec, partTypeString(UDateFormatField(fieldType)));
            auto value = jsString(&exec, resultString.substring(beginIndex, endIndex - beginIndex));
            JSObject* part = constructEmptyObject(&exec);
            part->putDirect(vm, typePropertyName, type);
            part->putDirect(vm, vm.propertyNames->value, value);
            parts->push(&exec, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }


    return parts;
}
#endif

} // namespace JSC

#endif // ENABLE(INTL)
