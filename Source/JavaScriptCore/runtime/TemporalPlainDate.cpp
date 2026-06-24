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

TemporalPlainDate* TemporalPlainDate::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, CalendarID calendarID)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDate>(vm)) TemporalPlainDate(vm, structure, WTF::move(plainDate));
    object->m_calendarID = calendarID;
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
}

String TemporalPlainDate::toString() const
{
    auto base = ISO8601::temporalDateToString(m_plainDate);
    if (TemporalCore::calendarIsISO(m_calendarID))
        return base;
    return makeString(base, "[u-ca="_s, TemporalCore::calendarIDToString(m_calendarID), ']');
}


ISO8601::PlainDate TemporalPlainDate::toPlainDate(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double yearDouble = duration.years();
    double monthDouble = duration.months();
    double dayDouble = duration.days();

    if (!ISO8601::isYearWithinLimits(yearDouble)) [[unlikely]] {
        throwRangeError(globalObject, scope, "year is out of range"_s);
        return { };
    }
    int32_t year = static_cast<int32_t>(yearDouble);

    if (!(monthDouble >= 1 && monthDouble <= 12)) [[unlikely]] {
        throwRangeError(globalObject, scope, "month is out of range"_s);
        return { };
    }
    uint32_t month = static_cast<uint32_t>(monthDouble);

    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (!(dayDouble >= 1 && dayDouble <= daysInMonth)) [[unlikely]] {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }
    uint32_t day = static_cast<uint32_t>(dayDouble);

    return ISO8601::PlainDate(year, month, day);
}

// CreateTemporalDate ( years, months, days )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldate
TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDate::create(vm, structure, WTF::move(plainDate));
}

TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate, String&& calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDate::create(vm, structure, WTF::move(plainDate), WTF::move(calendarId));
}

TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate, CalendarID calendarID)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDate::create(vm, structure, WTF::move(plainDate), calendarID);
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

    String calOpt = temporalShowCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    auto base = ISO8601::temporalDateToString(m_plainDate);
    auto calId = calendarIDAsString();
    if (calOpt == "never"_s)
        return base;
    if (calOpt == "always"_s)
        return makeString(base, "[u-ca="_s, calId, ']');
    if (calOpt == "critical"_s)
        return makeString(base, "[!u-ca="_s, calId, ']');
    // "auto": show calendar annotation only for non-ISO calendars.
    if (calendarID() != iso8601CalendarID())
        return makeString(base, "[u-ca="_s, calId, ']');
    return base;
}

static TemporalPlainDate* fromImpl(JSGlobalObject*, JSValue, Variant<JSObject*, TemporalOverflow>);

// ToTemporalDate property-bag and string paths (spec steps 2.d-2.i and 3-11).
static TemporalPlainDate* fromImpl(JSGlobalObject* globalObject, JSValue itemValue, Variant<JSObject*, TemporalOverflow> optionsOrOverflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.isObject()) {
        // Fast paths for typed Temporal objects (spec steps 2.a/2.b/2.c).
        // Options are handled by the caller (from()); these paths skip field reading.
        if (itemValue.inherits<TemporalPlainDate>()) {
            auto* existing = uncheckedDowncast<TemporalPlainDate>(itemValue);
            if (existing->calendarID() != iso8601CalendarID())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate(), existing->calendarID());
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate());
        }

        if (itemValue.inherits<TemporalPlainDateTime>()) {
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            if (pdt->calendarID() != iso8601CalendarID())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate(), pdt->calendarID());
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate());
        }

        if (itemValue.inherits<TemporalZonedDateTime>()) {
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
            ISO8601::PlainDate date;
            ISO8601::PlainTime time;
            zdt->getLocalDateAndTime(globalObject, date, time);
            RETURN_IF_EXCEPTION(scope, { });
            if (!TemporalCore::calendarIsISO(zdt->calendarID()))
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), String(zdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date));
        }

        // Step 2.d: calendar = ? GetTemporalCalendarIdentifierWithISODefault(item).
        // Step 2.e: fields = ? PrepareCalendarFields(...). Fields before options (spec order).
        CalendarID calendarId = iso8601CalendarID();
        auto fields = readCalendarFieldsFromObject(globalObject, asObject(itemValue), calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 2.f-2.g: resolvedOptions = ? GetOptionsObject(options);
        //                overflow = ? GetTemporalOverflowOption(resolvedOptions).
        auto overflow = TemporalOverflow::Constrain;
        if (std::holds_alternative<TemporalOverflow>(optionsOrOverflow))
            overflow = std::get<TemporalOverflow>(optionsOrOverflow);
        else if (auto* opts = std::get<JSObject*>(optionsOrOverflow)) {
            overflow = toTemporalOverflow(globalObject, opts);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 2.h: isoDate = ? CalendarDateFromFields(calendar, fields, overflow).
        auto result = TemporalCore::dateFromFields(calendarId, fields, overflow);
        if (!result) [[unlikely]] {
            if (result.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(result.error().message));
            else
                throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }

        // Step 2.i: Return ! CreateTemporalDate(isoDate, calendar).
        RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(),
            WTF::move(result->isoDate), result->calendarId));
    }

    // String path (spec steps 3-11).
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to PlainDate from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: result = ? ParseISODateTime(item, « TemporalDateTimeString[~Zoned] »).
    auto dateTime = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::DateTimeUnzoned);
    if (dateTime) [[likely]] {
        auto [plainDateOpt, plainTimeOptional, timeZoneOptional, calendarOptional, matched, isShortForm] = WTF::move(*dateTime);
        ASSERT(plainDateOpt);
        auto plainDate = WTF::move(*plainDateOpt);

        // Steps 5-7: calendar = result.[[Calendar]]; if ~empty~ → "iso8601"; CanonicalizeCalendar.
        CalendarID calendarId = iso8601CalendarID();
        if (calendarOptional) {
            auto rawCal = StringView(*calendarOptional).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return { };
            }
            calendarId = *canonicalized;
        }
        // Steps 8-9: GetOptionsObject + GetTemporalOverflowOption.
        //   Options aren't reachable on the compare path (compare takes no options arg); the
        //   spec calls are no-ops on undefined and the overflow value is unused for strings.
        // Steps 10-11: isoDate = CreateISODateRecord(...); Return ? CreateTemporalDate(isoDate, calendar).
        if (calendarId == iso8601CalendarID())
            RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate)));
        RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate), WTF::move(calendarId)));
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
// Entry point from Temporal.PlainDate.from() — handles step 1 (default options).
TemporalPlainDate* TemporalPlainDate::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If options is not present, set options to undefined. (Caller passes jsUndefined().)

    // Step 2: If item is an Object:
    if (itemValue.isObject()) {
        // Step 2.a: [[InitializedTemporalDate]] → GetOptionsObject + overflow + CreateTemporalDate.
        if (itemValue.inherits<TemporalPlainDate>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* existing = uncheckedDowncast<TemporalPlainDate>(itemValue);
            if (existing->calendarID() != iso8601CalendarID())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate(), existing->calendarID());
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), existing->plainDate());
        }

        // Step 2.b: [[InitializedTemporalZonedDateTime]] →
        //   GetISODateTimeFor (before options, per spec order) + overflow + CreateTemporalDate.
        if (itemValue.inherits<TemporalZonedDateTime>()) {
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
            ISO8601::PlainDate date;
            ISO8601::PlainTime time;
            zdt->getLocalDateAndTime(globalObject, date, time);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            if (!TemporalCore::calendarIsISO(zdt->calendarID()))
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), String(zdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date));
        }

        // Step 2.c: [[InitializedTemporalDateTime]] → GetOptionsObject + overflow + CreateTemporalDate.
        if (itemValue.inherits<TemporalPlainDateTime>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            if (pdt->calendarID() != iso8601CalendarID())
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate(), pdt->calendarID());
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), pdt->plainDate());
        }

        // Steps 2.d-2.i: property bag → PrepareCalendarFields (before options) + overflow + CalendarDateFromFields.
        // fromImpl() reads fields first then options; split here so TypeError for bad options type
        // is thrown only after field reads (spec observability).
        JSObject* opts = nullptr;
        if (!optionsValue.isUndefined()) {
            if (!optionsValue.isObject()) {
                fromImpl(globalObject, itemValue, Variant<JSObject*, TemporalOverflow>(TemporalOverflow::Constrain));
                RETURN_IF_EXCEPTION(scope, { });
                throwTypeError(globalObject, scope, "options must be an object"_s);
                return { };
            }
            opts = asObject(optionsValue);
        }
        RELEASE_AND_RETURN(scope, fromImpl(globalObject, itemValue, Variant<JSObject*, TemporalOverflow>(opts)));
    }

    // Step 3: If item is not a String, throw TypeError.
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to PlainDate from object or string values"_s);
        return { };
    }

    // Step 4: result = ? ParseISODateTime(item, « TemporalDateTimeString[~Zoned] »).
    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto dateTime = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::DateTimeUnzoned);
    if (dateTime) [[likely]] {
        auto [plainDateOpt, plainTimeOptional, timeZoneOptional, calendarOptional, matched, isShortForm] = WTF::move(*dateTime);
        ASSERT(plainDateOpt);
        auto plainDate = WTF::move(*plainDateOpt);

        // Steps 5-7: calendar = result.[[Calendar]]; if ~empty~ → "iso8601"; CanonicalizeCalendar.
        CalendarID calendarId = iso8601CalendarID();
        if (calendarOptional) {
            auto rawCal = StringView(*calendarOptional).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return { };
            }
            calendarId = *canonicalized;
        }

        // Steps 8-9: GetOptionsObject + overflow. (result unused for strings)
        toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 10-11: isoDate = CreateISODateRecord(...); Return ? CreateTemporalDate(isoDate, calendar).
        if (calendarId == iso8601CalendarID())
            RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate)));
        RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(plainDate), WTF::move(calendarId)));
    }

    // Step 4: ParseISODateTime failed → throw RangeError.
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
    int32_t defaultYear, uint32_t defaultMonth, uint32_t defaultDay)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemporalAnyProperties any = TemporalAnyProperties::None;

    std::optional<double> day;
    JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    if (!dayProperty.isUndefined()) {
        day = dayProperty.toIntegerWithTruncation(globalObject);
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
        month = monthProperty.toIntegerWithTruncation(globalObject);
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
        otherMonth = parseMonthCode(globalObject, monthCodeProperty);
        RETURN_IF_EXCEPTION(scope, { });
        monthCodePresent = true;
        any = TemporalAnyProperties::Some;
    }

    std::optional<double> year;
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (!yearProperty.isUndefined()) {
        year = yearProperty.toIntegerWithTruncation(globalObject);
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
    uint32_t monthToUse = defaultMonth;
    if (month) {
        if (overflow == TemporalOverflow::Constrain)
            monthToUse = clampTo<uint32_t>(*month, 1, 12);
        else {
            if (!(*month >= 1 && *month <= 12)) [[unlikely]] {
                throwRangeError(globalObject, scope, "month is out of range"_s);
                return { };
            }
            monthToUse = static_cast<uint32_t>(*month);
        }
    }
    uint8_t daysInMonth = ISO8601::daysInMonth(yearToUse, monthToUse);
    double rawDay = day.has_value() ? *day : static_cast<double>(defaultDay);

    uint32_t dayToUse;
    if (overflow == TemporalOverflow::Constrain)
        dayToUse = clampTo<uint32_t>(rawDay, 1, static_cast<uint32_t>(daysInMonth));
    else {
        if (!(rawDay >= 1 && rawDay <= daysInMonth)) [[unlikely]] {
            throwRangeError(globalObject, scope, "day is out of range"_s);
            return { };
        }
        dayToUse = static_cast<uint32_t>(rawDay);
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
        double doubleDay = dayProperty.toIntegerWithTruncation(globalObject);
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
        double doubleYear = yearProperty.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(doubleYear)) [[unlikely]] {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }

        if (!ISO8601::isYearWithinLimits(doubleYear)) [[unlikely]]
            year = ISO8601::outOfRangeYear;
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
        double doubleMonth = monthProperty.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(doubleMonth)) [[unlikely]] {
            throwRangeError(globalObject, scope, "month property must be finite"_s);
            return { };
        }

        // See step 9(c)(iv) of PrepareCalendarFields
        // https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
        if (doubleMonth <= 0) [[unlikely]] {
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
        monthCode = parseMonthCode(globalObject, monthCodeProperty);
        RETURN_IF_EXCEPTION(scope, { });
    }

    scope.release();
    auto year = toYear(globalObject, temporalDateLike);
    RETURN_IF_EXCEPTION(scope, { });

    return { month, monthCode, year };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
ISO8601::PlainDate TemporalPlainDate::with(JSGlobalObject* globalObject, JSObject* temporalDateLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-4: RequireInternalSlot, IsPartialTemporalObject, calendar — done by caller.
    // Step 3 continued: rejectObjectWithCalendarOrTimeZone enforces the "no calendar/timeZone" constraint.
    rejectObjectWithCalendarOrTimeZone(globalObject, temporalDateLike);
    RETURN_IF_EXCEPTION(scope, { });

    bool isNonISO = !TemporalCore::calendarIsISO(m_calendarID);

    if (isNonISO) {
        // Step 6: PrepareCalendarFields(calendar, temporalDateLike, «year,month,month-code,day», «», ~partial~).
        // CalendarRead::Skip — calendar already known from m_calendarID; step 3 ensures no calendar property.
        CalendarID unusedCalId = m_calendarID;
        auto partialFields = readCalendarFieldsFromObject<FieldSetType::Date, CalendarRead::Skip>(globalObject, temporalDateLike, unusedCalId);
        RETURN_IF_EXCEPTION(scope, { });
        if (!partialFields.day && !partialFields.era && !partialFields.eraYear && !partialFields.month && !partialFields.monthCode && !partialFields.year) [[unlikely]] {
            throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
            return { };
        }

        // Steps 8-9: GetOptionsObject + GetTemporalOverflowOption.
        TemporalOverflow overflow = toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 5, 7, 10, 11: ISODateToFields + CalendarMergeFields + CalendarDateFromFields
        // + CreateTemporalDate — fused into plainDateWith.
        auto result = TemporalCore::plainDateWith(m_calendarID, m_plainDate, partialFields, overflow);
        if (!result) [[unlikely]] {
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

    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, TemporalDateFormat::Date, y, m, d, optionalMonthCode, overflow, m_calendarID));
}

// https://tc39.es/proposal-temporal/#sec-getutcepochnanoseconds
static Int128 getUTCEpochNanoseconds(ISO8601::PlainDate isoDate)
{
    return TemporalCore::getUTCEpochNanoseconds(isoDate, ISO8601::PlainTime());
}

ISO8601::Duration TemporalPlainDate::differenceTemporalPlainDate(JSGlobalObject* globalObject, DifferenceOperation op, TemporalPlainDate* other, TemporalUnit smallestUnit, TemporalUnit largestUnit, RoundingMode roundingMode, double increment)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: CalendarEquals — calendars must match.
    if (m_calendarID != other->m_calendarID) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between dates with different calendars"_s);
        return { };
    }

    // Step 2: if dates equal, return zero duration.
    if (!TemporalCore::isoDateCompare(plainDate(), other->plainDate()))
        return ISO8601::Duration();
    ISO8601::Duration dateDiff;
    if (!TemporalCore::calendarIsISO(m_calendarID))
        dateDiff = calendarDateUntil(m_calendarID, plainDate(), other->plainDate(), largestUnit);
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
        if (!roundResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, roundResult.error());
            return { };
        }
    }
    auto durResult = TemporalCore::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    ISO8601::Duration result = *durResult;
    if (op == DifferenceOperation::Since)
        result = -result;
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
ISO8601::Duration TemporalPlainDate::until(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 3-4: GetDifferenceSettings.
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5-9: DifferenceTemporalPlainDate.
    RELEASE_AND_RETURN(scope, differenceTemporalPlainDate(globalObject,
        DifferenceOperation::Until, other, smallestUnit, largestUnit, roundingMode, increment));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
ISO8601::Duration TemporalPlainDate::since(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 3-4: GetDifferenceSettings(~since~, ...).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day, DifferenceOperation::Since);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5-9: DifferenceTemporalPlainDate.
    RELEASE_AND_RETURN(scope, differenceTemporalPlainDate(globalObject,
        DifferenceOperation::Since, other, smallestUnit, largestUnit, roundingMode, increment));
}

} // namespace JSC
