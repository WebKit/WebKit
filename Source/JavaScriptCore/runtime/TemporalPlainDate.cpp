/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include "TemporalPlainDate.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "DateConstructor.h"
#include "DurationArithmetic.h"
#include "IntlObjectInlines.h"
#include "Rounding.h"
#include "JSCInlines.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "TemporalZonedDateTime.h"
#include "VMTrapsInlines.h"

#include <wtf/text/MakeString.h>
namespace JSC {

const ClassInfo TemporalPlainDate::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainDate) };

TemporalPlainDate* TemporalPlainDate::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDate>(vm)) TemporalPlainDate(vm, structure, WTF::move(plainDate));
    object->finishCreation(vm);
    return object;
}

TemporalPlainDate* TemporalPlainDate::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, String&& calendarId)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDate>(vm)) TemporalPlainDate(vm, structure, WTF::move(plainDate), WTF::move(calendarId));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainDate::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDate::TemporalPlainDate(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate)
    : Base(vm, structure)
    , m_plainDate(WTF::move(plainDate))
    , m_calendarID(iso8601CalendarID())
{
}

TemporalPlainDate::TemporalPlainDate(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, String&& calendarId)
    : Base(vm, structure)
    , m_plainDate(WTF::move(plainDate))
    , m_calendarID(TemporalCore::calendarIDFromString(calendarId))
{
    UNUSED_PARAM(calendarId);
}

String TemporalPlainDate::toString() const
{
    auto base = ISO8601::temporalDateToString(m_plainDate);
    if (TemporalCore::calendarIsISO(m_calendarID))
        return base;
    return makeString(base, "[u-ca="_s, TemporalCore::calendarIDToString(m_calendarID), ']');
}

void TemporalPlainDate::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

ISO8601::PlainDate TemporalPlainDate::toPlainDate(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double yearDouble = duration.years();
    double monthDouble = duration.months();
    double dayDouble = duration.days();

    if (!ISO8601::isYearWithinLimits(yearDouble)) {
        throwRangeError(globalObject, scope, "year is out of range"_s);
        return { };
    }
    int32_t year = static_cast<int32_t>(yearDouble);

    if (!(monthDouble >= 1 && monthDouble <= 12)) {
        throwRangeError(globalObject, scope, "month is out of range"_s);
        return { };
    }
    unsigned month = static_cast<unsigned>(monthDouble);

    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (!(dayDouble >= 1 && dayDouble <= daysInMonth)) {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }
    unsigned day = static_cast<unsigned>(dayDouble);

    return ISO8601::PlainDate(year, month, day);
}

// CreateTemporalDate ( years, months, days )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldate
TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDate::create(vm, structure, WTF::move(plainDate));
}

TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate, String&& calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDate::create(vm, structure, WTF::move(plainDate), WTF::move(calendarId));
}

TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto plainDate = toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, structure,  WTF::move(plainDate)));
}

String TemporalPlainDate::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    String calOpt = intlStringOption(globalObject, options, Identifier::fromString(vm, "calendarName"_s),
        { "auto"_s, "always"_s, "never"_s, "critical"_s },
        "calendarName must be \"auto\", \"always\", \"never\", or \"critical\""_s, "auto"_s);
    RETURN_IF_EXCEPTION(scope, { });

    auto base = ISO8601::temporalDateToString(m_plainDate);
    auto calId = TemporalCore::calendarIDToString(m_calendarID).toString();
    if (calOpt == "never"_s)
        return base;
    if (calOpt == "always"_s)
        return makeString(base, "[u-ca="_s, calId, ']');
    if (calOpt == "critical"_s)
        return makeString(base, "[!u-ca="_s, calId, ']');
    // "auto": show calendar annotation only for non-ISO calendars.
    if (calId != "iso8601"_s)
        return makeString(base, "[u-ca="_s, calId, ']');
    return base;
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
TemporalPlainDate* TemporalPlainDate::from(JSGlobalObject* globalObject, JSValue itemValue, Variant<JSObject*, TemporalOverflow> optionsOrOverflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDate>()) {
            auto* existing = uncheckedDowncast<TemporalPlainDate>(itemValue);
            if (existing->calendarId() != "iso8601"_s && !existing->calendarId().isEmpty())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate(), String(existing->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate());
        }

        if (itemValue.inherits<TemporalPlainDateTime>()) {
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            if (pdt->calendarId() != "iso8601"_s && !pdt->calendarId().isEmpty())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate(), String(pdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate());
        }

        if (itemValue.inherits<TemporalZonedDateTime>()) {
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
            ISO8601::PlainDate date;
            ISO8601::PlainTime time;
            if (!zdt->getLocalDateAndTime(globalObject, date, time))
                return { };
            if (zdt->calendarId() != "iso8601"_s && !zdt->calendarId().isEmpty())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), String(zdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date));
        }

        auto overflow = TemporalOverflow::Constrain;
        if (std::holds_alternative<TemporalOverflow>(optionsOrOverflow))
            overflow = std::get<TemporalOverflow>(optionsOrOverflow);
        else if (auto* opts = std::get<JSObject*>(optionsOrOverflow)) {
            overflow = toTemporalOverflow(globalObject, opts);
            RETURN_IF_EXCEPTION(scope, { });
        }

        String calendarId;
        auto fields = readCalendarFieldsFromObject(globalObject, asObject(itemValue), calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        auto result = TemporalCore::dateFromFields(TemporalCore::calendarIDFromString(calendarId), fields, overflow);
        if (!result) {
            if (result.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(result.error().message));
            else
                throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }

        RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(),
            WTF::move(result->isoDate), WTF::move(result->calendarId)));
    }

    if (!itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to PlainDate from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTF::move(dateTime.value());
        String calendarId = "iso8601"_s;
        if (calendarOptional) {
            auto rawCal = StringView(*calendarOptional).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return { };
            }
            calendarId = intlAvailableCalendars().at(*canonicalized);
        }
        if (!(timeZoneOptional && timeZoneOptional->m_z)) {
            if (calendarId == "iso8601"_s)
                RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate)));
            RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate), WTF::move(calendarId)));
        }
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// Overload for constructor path: processes item first, then validates options.
// This ensures field reading order matches the spec (PrepareCalendarFields before GetOptionsObject).
TemporalPlainDate* TemporalPlainDate::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDate>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* existing = uncheckedDowncast<TemporalPlainDate>(itemValue);
            if (existing->calendarId() != "iso8601"_s && !existing->calendarId().isEmpty())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate(), String(existing->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate());
        }
        if (itemValue.inherits<TemporalPlainDateTime>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            if (pdt->calendarId() != "iso8601"_s && !pdt->calendarId().isEmpty())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate(), String(pdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate());
        }
        if (itemValue.inherits<TemporalZonedDateTime>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
            ISO8601::PlainDate date;
            ISO8601::PlainTime time;
            if (!zdt->getLocalDateAndTime(globalObject, date, time))
                return { };
            if (zdt->calendarId() != "iso8601"_s && !zdt->calendarId().isEmpty())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), String(zdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date));
        }

        // Property bag: read fields first (per spec ordering), then validate options.
        String calendarId;
        auto fields = readCalendarFieldsFromObject(globalObject, asObject(itemValue), calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        auto overflow = TemporalOverflow::Constrain;
        {
            JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            if (options) {
                overflow = toTemporalOverflow(globalObject, options);
                RETURN_IF_EXCEPTION(scope, { });
            }
        }

        auto result = TemporalCore::dateFromFields(TemporalCore::calendarIDFromString(calendarId), fields, overflow);
        if (!result) {
            if (result.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(result.error().message));
            else
                throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(),
            WTF::move(result->isoDate), WTF::move(result->calendarId)));
    }

    if (!itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to PlainDate from object or string values"_s);
        return { };
    }

    // String path: parse first, then validate options.
    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTF::move(dateTime.value());
        String calendarId = "iso8601"_s;
        if (calendarOptional) {
            auto rawCal = StringView(*calendarOptional).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return { };
            }
            calendarId = intlAvailableCalendars().at(*canonicalized);
        }
        // Validate options AFTER string parsing succeeds.
        toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
        if (!(timeZoneOptional && timeZoneOptional->m_z)) {
            if (calendarId == "iso8601"_s)
                RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate)));
            RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate), WTF::move(calendarId)));
        }
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// This operation is not in the spec, but does the same work as a combination of
// PrepareCalendarFields and CalendarMergeFields:
// https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
// https://tc39.es/proposal-temporal/#sec-temporal-calendarmergefields
// Needs to take a default year, month and day so that validity can be checked.
std::tuple<int32_t, unsigned, unsigned, std::optional<ParsedMonthCode>, TemporalOverflow, TemporalAnyProperties>
TemporalPlainDate::mergeDateFields(JSGlobalObject* globalObject, JSObject* temporalDateLike, JSValue optionsValue,
    int32_t defaultYear, unsigned defaultMonth, unsigned defaultDay)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemporalAnyProperties any = TemporalAnyProperties::None;

    std::optional<double> day;
    JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    if (!dayProperty.isUndefined()) {
        day = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (day.value() <= 0 || !std::isfinite(day.value())) [[unlikely]] {
            throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
            return { };
        }

        any = TemporalAnyProperties::Some;
    }

    std::optional<double> month;
    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthProperty.isUndefined()) {
        month = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (month.value() <= 0 || !std::isfinite(month.value())) [[unlikely]] {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return { };
        }
        any = TemporalAnyProperties::Some;
    }

    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<ParsedMonthCode> otherMonth;
    bool monthCodePresent = false;
    if (!monthCodeProperty.isUndefined()) {
        auto monthCodePrimitive = monthCodeProperty.toPrimitive(globalObject, PreferString);
        RETURN_IF_EXCEPTION(scope, { });
        if (!monthCodePrimitive.isString()) {
            throwTypeError(globalObject, scope, "monthCode must be a string"_s);
            return { };
        }
        auto monthCodeString = asString(monthCodePrimitive)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        otherMonth = ISO8601::parseMonthCode(monthCodeString);
        if (!otherMonth) {
            throwRangeError(globalObject, scope, "Invalid monthCode"_s);
            return { };
        }
        monthCodePresent = true;
        any = TemporalAnyProperties::Some;
    }

    std::optional<double> year;
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (!yearProperty.isUndefined()) {
        year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(year.value())) [[unlikely]] {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }
        any = TemporalAnyProperties::Some;
    }

    if (monthCodePresent) {
        if (!otherMonth) [[unlikely]] {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return { };
        }
        if (!month)
            month = otherMonth->monthNumber;
        else if (month.value() != otherMonth->monthNumber) [[unlikely]] {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return { };
        }
    }

    TemporalOverflow overflow = toTemporalOverflow(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Duplicate code from TemporalPlainDate::toPlainDate so we can convert from
    // double to int32_t / unsigned here
    if (year && !ISO8601::isYearWithinLimits(*year)) [[unlikely]] {
        throwRangeError(globalObject, scope, "year is out of range"_s);
        return { };
    }

    int32_t yearToUse = defaultYear;
    if (year)
        yearToUse = static_cast<int32_t>(*year);
    unsigned monthToUse = defaultMonth;
    if (month) {
        if (overflow == TemporalOverflow::Constrain)
            monthToUse = static_cast<unsigned>(std::clamp<double>(*month, 1, 12)); // clamp at double level
        else {
            if (!(*month >= 1 && *month <= 12)) [[unlikely]] {
                throwRangeError(globalObject, scope, "month is out of range"_s);
                return { };
            }
            monthToUse = static_cast<unsigned>(*month);
        }
    }
    uint8_t daysInMonth = ISO8601::daysInMonth(yearToUse, monthToUse);
    double rawDay = day.has_value() ? *day : static_cast<double>(defaultDay);

    unsigned dayToUse;
    if (overflow == TemporalOverflow::Constrain)
        dayToUse = static_cast<unsigned>(std::clamp<double>(rawDay, 1, daysInMonth));
    else {
        if (!(rawDay >= 1 && rawDay <= daysInMonth)) [[unlikely]] {
            throwRangeError(globalObject, scope, "day is out of range"_s);
            return { };
        }
        dayToUse = static_cast<unsigned>(rawDay);
    }

    return { yearToUse, monthToUse, dayToUse, otherMonth, overflow, any };
}

std::optional<int32_t> TemporalPlainDate::toDay(JSGlobalObject* globalObject, JSObject* temporalDateLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<int32_t> day;
    JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    if (!dayProperty.isUndefined()) {
        double doubleDay = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(doubleDay)) [[unlikely]] {
            throwRangeError(globalObject, scope, "day property must be finite"_s);
            return { };
        }

        if (!isInBounds<int32_t>(doubleDay)) [[unlikely]] {
            // Later checks will report error
            day = ISO8601::outOfRangeYear;
        } else
            day = static_cast<int32_t>(doubleDay);
    }
    return day;
}

std::optional<int32_t> TemporalPlainDate::toYear(JSGlobalObject* globalObject, JSObject* temporalDateLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<int32_t> year;
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (!yearProperty.isUndefined()) {
        double doubleYear = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(doubleYear)) [[unlikely]] {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }

        if (!ISO8601::isYearWithinLimits(doubleYear))
            [[unlikely]] year = ISO8601::outOfRangeYear;
        else
            year = static_cast<int32_t>(doubleYear);
    }
    return year;
}

std::tuple<std::optional<int32_t>, std::optional<ParsedMonthCode>, std::optional<int32_t>>
TemporalPlainDate::toYearMonth(JSGlobalObject* globalObject, JSObject* temporalDateLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<int32_t> month;
    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthProperty.isUndefined()) {
        double doubleMonth = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(doubleMonth)) [[unlikely]] {
            throwRangeError(globalObject, scope, "month property must be finite"_s);
            return { };
        }

        // See step 9(c)(iv) of PrepareCalendarFields
        // https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
        if (doubleMonth <= 0) {
            throwRangeError(globalObject, scope, "month property must be a positive integer"_s);
            return { };
        }

        if (!isInBounds<int32_t>(doubleMonth)) [[unlikely]] {
            // Later checks will report error
            month = ISO8601::outOfRangeYear;
        } else
            month = static_cast<int32_t>(doubleMonth);
    }

    std::optional<ParsedMonthCode> monthCode;
    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthCodeProperty.isUndefined()) {
        auto monthCodePrimitive = monthCodeProperty.toPrimitive(globalObject, PreferString);
        RETURN_IF_EXCEPTION(scope, { });
        if (!monthCodePrimitive.isString()) {
            throwTypeError(globalObject, scope, "monthCode must be a string"_s);
            return { };
        }
        auto monthCodeString = asString(monthCodePrimitive)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        monthCode = ISO8601::parseMonthCode(monthCodeString);
    }

    scope.release();
    auto year = toYear(globalObject, temporalDateLike);
    RETURN_IF_EXCEPTION(scope, { });

    return { month, monthCode, year };
}

ISO8601::PlainDate TemporalPlainDate::with(JSGlobalObject* globalObject, JSObject* temporalDateLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalDateLike);
    RETURN_IF_EXCEPTION(scope, { });

    bool isNonISO = !TemporalCore::calendarIsISO(m_calendarID);

    if (isNonISO) {
        // temporal_rs approach: merge fields with fallback from current date,
        // then call dateFromFields which handles all constrain centrally.
        auto calFields = TemporalCore::isoToCalendarFields(m_calendarID, m_plainDate);
        if (!calFields) {
            throwRangeError(globalObject, scope, "Failed to get calendar fields"_s);
            return { };
        }

        // Read user fields in alphabetical order (PrepareTemporalFields).
        std::optional<uint8_t> userDay;
        JSValue dayProp = temporalDateLike->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, { });
        if (!dayProp.isUndefined()) {
            double d = dayProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!(d > 0 && std::isfinite(d))) {

                throwRangeError(globalObject, scope, "day must be positive"_s);

                return { };

            }
            userDay = static_cast<uint8_t>(d);
        }

        std::optional<uint32_t> userMonth;
        JSValue monthProp = temporalDateLike->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, { });
        if (!monthProp.isUndefined()) {
            double m = monthProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!(m > 0 && std::isfinite(m))) {

                throwRangeError(globalObject, scope, "month must be positive"_s);

                return { };

            }
            userMonth = static_cast<uint32_t>(m);
        }

        std::optional<ParsedMonthCode> userMonthCode;
        JSValue mcProp = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, { });
        if (!mcProp.isUndefined()) {
            auto mcStr = mcProp.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            userMonthCode = ISO8601::parseMonthCode(mcStr);
            if (!userMonthCode) {

                throwRangeError(globalObject, scope, "Invalid monthCode"_s);

                return { };

            }
        }

        std::optional<int32_t> userYear;
        JSValue yearProp = temporalDateLike->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, { });
        if (!yearProp.isUndefined()) {
            double y = yearProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(y)) {

                throwRangeError(globalObject, scope, "year must be finite"_s);

                return { };

            }
            userYear = static_cast<int32_t>(y);
        }

        if (!userDay && !userMonth && !userMonthCode && !userYear) {
            throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
            return { };
        }

        // Read overflow option.
        TemporalOverflow overflow = toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        // Build merged CalendarFieldsIn: user values override fallback.
        TemporalCore::CalendarFieldsIn merged;
        merged.year = userYear.value_or(calFields->year);
        merged.month = userMonth ? *userMonth : static_cast<uint32_t>(calFields->month);
        merged.day = userDay ? *userDay : calFields->day;
        // MonthCode: user's overrides fallback's.
        if (userMonthCode)
            merged.monthCode = userMonthCode;
        else if (!userMonth && !calFields->monthCode.isEmpty())
            merged.monthCode = ISO8601::parseMonthCode(calFields->monthCode);

        // Call centralized dateFromFields — handles all constrain/reject.
        auto result = TemporalCore::dateFromFields(m_calendarID, merged, overflow);
        if (!result) {
            if (result.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(result.error().message));
            else
                throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        return result->isoDate;
    }

    // ISO path: use existing mergeDateFields.
    auto [y, m, d, optionalMonthCode, overflow, any] = mergeDateFields(globalObject, temporalDateLike, optionsValue, year(), month(), day());
    RETURN_IF_EXCEPTION(scope, { });
    if (any == TemporalAnyProperties::None) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, TemporalDateFormat::Date,
        y, m, d, optionalMonthCode, overflow, TemporalCore::calendarIDToString(m_calendarID)));
}

// https://tc39.es/proposal-temporal/#sec-getutcepochnanoseconds
static Int128 getUTCEpochNanoseconds(ISO8601::PlainDate isoDate)
{
    return getUTCEpochNanoseconds(
        std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>(
            isoDate, ISO8601::PlainTime()));
}

ISO8601::Duration TemporalPlainDate::differenceTemporalPlainDate(JSGlobalObject* globalObject, DifferenceOperation op, TemporalPlainDate* other, TemporalUnit smallestUnit, TemporalUnit largestUnit, RoundingMode roundingMode, double increment)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // CalendarEquals: calendars must match (both already canonical from constructors).
    if (m_calendarID != other->m_calendarID) {
        throwRangeError(globalObject, scope, "cannot compute difference between dates with different calendars"_s);
        return { };
    }

    if (!TemporalCore::isoDateCompare(plainDate(), other->plainDate()))
        return ISO8601::Duration();
    ISO8601::Duration dateDiff;
    if (!TemporalCore::calendarIsISO(m_calendarID))
        dateDiff = calendarDateUntil(TemporalCore::calendarIDToString(m_calendarID), plainDate(), other->plainDate(), largestUnit);
    else
        dateDiff = TemporalCore::calendarDateUntil(plainDate(), other->plainDate(), largestUnit);
    ISO8601::InternalDuration duration = ISO8601::InternalDuration::combineDateAndTimeDuration(dateDiff, 0);
    if (smallestUnit != TemporalUnit::Day || increment != 1) {
        auto isoDate = plainDate();
        Int128 originEpochNs = getUTCEpochNanoseconds(isoDate);
        auto isoDateOther = other->plainDate();
        Int128 destEpochNs = getUTCEpochNanoseconds(isoDateOther);
        auto roundResult = TemporalCore::roundRelativeDuration(
            duration, originEpochNs, destEpochNs, isoDate, ISO8601::PlainTime(),
            largestUnit, increment, smallestUnit, roundingMode, nullptr, m_calendarID);
        if (!roundResult) {
            throwTemporalError(globalObject, scope, roundResult.error());
            return { };
        }
    }
    auto result = TemporalDuration::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (op == DifferenceOperation::Since)
        result = -result;
    return result;
}

ISO8601::Duration TemporalPlainDate::until(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, differenceTemporalPlainDate(globalObject,
        DifferenceOperation::Until, other, smallestUnit, largestUnit, roundingMode, increment));
}

ISO8601::Duration TemporalPlainDate::since(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });
    roundingMode = TemporalCore::negateTemporalRoundingMode(roundingMode);

    RELEASE_AND_RETURN(scope, differenceTemporalPlainDate(globalObject,
        DifferenceOperation::Since, other, smallestUnit, largestUnit, roundingMode, increment));
}

String TemporalPlainDate::monthCode() const
{
    return ISO8601::monthCode(m_plainDate.month());
}

uint8_t TemporalPlainDate::dayOfWeek() const
{
    return ISO8601::dayOfWeek(m_plainDate);
}

uint16_t TemporalPlainDate::dayOfYear() const
{
    return ISO8601::dayOfYear(m_plainDate);
}

uint8_t TemporalPlainDate::weekOfYear() const
{
    return ISO8601::weekOfYear(m_plainDate);
}

int32_t TemporalPlainDate::yearOfWeek() const
{
    return ISO8601::yearOfWeek(m_plainDate);
}

} // namespace JSC
