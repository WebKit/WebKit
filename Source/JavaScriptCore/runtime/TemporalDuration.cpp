/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "TemporalDuration.h"

#include "DateConstructor.h"
#include "DurationArithmetic.h"
#include "FractionToDouble.h"
#include "ISOArithmetic.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
// FIXME: #include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include "ZonedDateTimeCore.h"
#include <wtf/DateMath.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

const ClassInfo TemporalDuration::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalDuration) };

TemporalDuration* TemporalDuration::create(VM& vm, Structure* structure, ISO8601::Duration&& duration)
{
    auto* object = new (NotNull, allocateCell<TemporalDuration>(vm)) TemporalDuration(vm, structure, WTF::move(duration));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalDuration::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalDuration::TemporalDuration(VM& vm, Structure* structure, ISO8601::Duration&& duration)
    : Base(vm, structure)
    , m_duration(WTF::move(duration))
{
}

// CreateTemporalDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds [ , newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalduration
TemporalDuration* TemporalDuration::tryCreateIfValid(JSGlobalObject* globalObject, ISO8601::Duration&& duration, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isValidDuration(duration)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    return TemporalDuration::create(vm, structure ? structure : globalObject->durationStructure(), WTF::move(duration));
}

// ToTemporalDurationRecord ( temporalDurationLike )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldurationrecord
ISO8601::Duration TemporalDuration::fromDurationLike(JSGlobalObject* globalObject, JSObject* durationLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (durationLike->inherits<TemporalDuration>())
        return uncheckedDowncast<TemporalDuration>(durationLike)->m_duration;

    ISO8601::Duration result;
    auto hasRelevantProperty = false;
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        JSValue value = durationLike->get(globalObject, temporalUnitPluralPropertyName(vm, unit));
        RETURN_IF_EXCEPTION(scope, { });

        if (value.isUndefined())
            continue;

        hasRelevantProperty = true;
        double v = value.toNumber(globalObject) + 0.0;
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(v) || !std::isfinite(v)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return { };
        }
        result.setField(unit, v);
    }

    if (!hasRelevantProperty) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal.Duration property"_s);
        return { };
    }

    return result;
}

// ToLimitedTemporalDuration ( temporalDurationLike, disallowedFields )
// https://tc39.es/proposal-temporal/#sec-temporal-tolimitedtemporalduration
ISO8601::Duration TemporalDuration::toISO8601Duration(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration duration;
    if (itemValue.isObject()) {
        duration = fromDurationLike(globalObject, asObject(itemValue));
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        if (!itemValue.isString()) [[unlikely]] {
            throwTypeError(globalObject, scope, "can only convert to Duration from object or string values"_s);
            return { };
        }

        String string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        auto parsedDuration = ISO8601::parseDuration(string);
        if (!parsedDuration) {
            // 3090: 308 digits * 10 fields + 10 designators
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(3090, string), "' is not a valid Duration string"_s));
            return { };
        }

        duration = parsedDuration.value();
    }

    if (!ISO8601::isValidDuration(duration)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    return duration;
}

// ToTemporalDuration ( item )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
TemporalDuration* TemporalDuration::toTemporalDuration(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.inherits<TemporalDuration>())
        return uncheckedDowncast<TemporalDuration>(itemValue);

    auto result = toISO8601Duration(globalObject, itemValue);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return TemporalDuration::create(vm, globalObject->durationStructure(), WTF::move(result));
}

// ToLimitedTemporalDuration ( temporalDurationLike, disallowedFields )
// https://tc39.es/proposal-temporal/#sec-temporal-tolimitedtemporalduration
ISO8601::Duration TemporalDuration::toLimitedDuration(JSGlobalObject* globalObject, JSValue itemValue, std::initializer_list<TemporalUnit> disallowedUnits)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration duration = toISO8601Duration(globalObject, itemValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!isValidDuration(duration)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    for (TemporalUnit unit : disallowedUnits) {
        if (duration[unit]) [[unlikely]] {
            throwRangeError(globalObject, scope, makeString("Adding "_s, temporalUnitPluralPropertyName(vm, unit).publicName(), " not supported by Temporal.Instant. Try Temporal.ZonedDateTime instead"_s));
            return { };
        }
    }

    return duration;
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.from
TemporalDuration* TemporalDuration::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();

    if (itemValue.inherits<TemporalDuration>()) {
        ISO8601::Duration cloned = uncheckedDowncast<TemporalDuration>(itemValue)->m_duration;
        return TemporalDuration::create(vm, globalObject->durationStructure(), WTF::move(cloned));
    }

    return toTemporalDuration(globalObject, itemValue);
}

// https://tc39.es/proposal-temporal/#sec-temporal-add24hourdaystonormalizedtimeduration
static Int128 add24HourDaysToTimeDuration(JSGlobalObject* globalObject, Int128 d, double days)
{
    auto result = TemporalCore::add24HourDaysToTimeDuration(d, days);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// Helper: calendar-aware date addition. Uses calendarDateAdd for non-ISO, isoDateAdd for ISO.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
static ISO8601::PlainDate calendarAwareDateAdd(JSGlobalObject* globalObject, CalendarID calendarId, const ISO8601::PlainDate& date, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    if (calendarId == iso8601CalendarID())
        return isoDateAdd(globalObject, date, duration, overflow);
    return calendarDateAdd(globalObject, calendarId, date, duration, overflow);
}

struct RelativeToRecord {
    // FIXME: TemporalZonedDateTime* zonedRelativeTo
    // zonedRelativeTo is always nullptr until TemporalZonedDateTime is implemented.
    ISO8601::PlainDate plainDate;
    bool hasPlainRelativeTo { false };
    CalendarID calendarId { iso8601CalendarID() };
};

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalrelativetooption
static RelativeToRecord toRelativeTemporalObject(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let value be ?Get(options, "relativeTo").
    JSValue relativeToValue = options->get(globalObject, Identifier::fromString(vm, "relativeTo"_s));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: If value is undefined, return {[[PlainRelativeTo]]: undefined, [[ZonedRelativeTo]]: undefined}.
    if (relativeToValue.isUndefined())
        return { };

    // Step 3-4: offsetBehaviour = ~option~, matchBehaviour = ~match-exactly~ (tracked in RelativeToRecord).

    // Step 5: If value is an Object.
    if (relativeToValue.isObject()) {
        JSObject* obj = asObject(relativeToValue);
        // Step 5a: [[InitializedTemporalZonedDateTime]] → return {ZonedRelativeTo: value}.
        // FIXME: TemporalZonedDateTime
        // if (obj->inherits<TemporalZonedDateTime>()) { ... }

        // Step 5b: [[InitializedTemporalDate]] → return {PlainRelativeTo: value}.
        if (obj->inherits<TemporalPlainDate>()) {
            auto* pd = uncheckedDowncast<TemporalPlainDate>(obj);
            return RelativeToRecord { pd->plainDate(), true, pd->calendarID() };
        }
        // Step 5c: [[InitializedTemporalDateTime]] → CreateTemporalDate(value.[[ISODateTime]].[[ISODate]]).
        if (obj->inherits<TemporalPlainDateTime>()) {
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(obj);
            return RelativeToRecord { pdt->plainDate(), true, pdt->calendarID() };
        }
        // Step 5d: GetTemporalCalendarIdentifierWithISODefault.
        // Step 5e: PrepareCalendarFields(calendar, value, «year,month,monthCode,day»,
        //          «hour,minute,second,millisecond,microsecond,nanosecond,offset,timeZone», «»).
        // Property bag: read ALL temporal fields in alphabetical order per spec.

        // calendar
        CalendarID calendarId = iso8601CalendarID();
        JSValue calendarProperty = obj->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, { });
        if (!calendarProperty.isUndefined()) {
            auto calStr = toTemporalCalendarIdentifier(globalObject, calendarProperty);
            RETURN_IF_EXCEPTION(scope, { });
            calendarId = TemporalCore::calendarIDFromString(calStr);
        }

        // day (required)
        JSValue dayProperty = obj->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, { });
        if (dayProperty.isUndefined()) [[unlikely]] {
            throwTypeError(globalObject, scope, "day property must be present"_s);
            return { };
        }
        double day = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!(day > 0 && std::isfinite(day))) [[unlikely]] {
            throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
            return { };
        }

        // era, eraYear (alphabetical, only read for calendars with eras)
        std::optional<String> era;
        std::optional<double> eraYear;
        bool calHasEras = TemporalCore::calendarHasEras(calendarId);
        if (calHasEras) {
            JSValue eraProperty = obj->get(globalObject, Identifier::fromString(vm, "era"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!eraProperty.isUndefined()) {
                era = eraProperty.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
            }
            JSValue eraYearProperty = obj->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!eraYearProperty.isUndefined()) {
                double ey = eraYearProperty.toIntegerOrInfinity(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                if (!std::isfinite(ey)) [[unlikely]] {
                    throwRangeError(globalObject, scope, "eraYear property must be finite"_s);
                    return { };
                }
                eraYear = ey;
            }
        }

        // hour, microsecond, millisecond, minute — read and validate (Infinity check)
        auto readTimeField = [&](const Identifier& name) -> double {
            JSValue val = obj->get(globalObject, name);
            if (scope.exception())
                return 0;
            if (val.isUndefined())
                return 0;
            double d = val.toIntegerOrInfinity(globalObject);
            if (scope.exception())
                return 0;
            if (!std::isfinite(d)) [[unlikely]] {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return 0;
            }
            return d;
        };

        double hour = readTimeField(vm.propertyNames->hour);
        RETURN_IF_EXCEPTION(scope, { });
        double microsecond = readTimeField(Identifier::fromString(vm, "microsecond"_s));
        RETURN_IF_EXCEPTION(scope, { });
        double millisecond = readTimeField(Identifier::fromString(vm, "millisecond"_s));
        RETURN_IF_EXCEPTION(scope, { });
        double minute = readTimeField(Identifier::fromString(vm, "minute"_s));
        RETURN_IF_EXCEPTION(scope, { });

        // month
        JSValue monthProperty = obj->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, { });
        double month = 0;
        if (!monthProperty.isUndefined()) {
            month = monthProperty.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // monthCode
        JSValue monthCodeProperty = obj->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, { });
        std::optional<ParsedMonthCode> otherMonth;
        if (!monthCodeProperty.isUndefined()) {
            auto monthCodeString = monthCodeProperty.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            otherMonth = ISO8601::parseMonthCode(monthCodeString);
            if (!otherMonth) [[unlikely]] {
                throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
                return { };
            }
        }
        if (monthProperty.isUndefined() && !otherMonth) [[unlikely]] {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return { };
        }

        // nanosecond
        double nanosecond = readTimeField(Identifier::fromString(vm, "nanosecond"_s));
        RETURN_IF_EXCEPTION(scope, { });

        // offset: read at alphabetical position and eagerly process via ToOffsetString.
        // Skipped for PlainDate bags where offset is undefined.
        JSValue offsetProperty = obj->get(globalObject, Identifier::fromString(vm, "offset"_s));
        RETURN_IF_EXCEPTION(scope, { });
        std::optional<int64_t> givenOffsetNs;
        if (!offsetProperty.isUndefined()) {
            JSValue offsetPrimitive = offsetProperty.toPrimitive(globalObject, PreferString);
            RETURN_IF_EXCEPTION(scope, { });
            if (!offsetPrimitive.isString()) [[unlikely]] {
                throwTypeError(globalObject, scope, "offset property must be a string"_s);
                return { };
            }
            auto offsetStr = asString(offsetPrimitive)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto offsetNs = ISO8601::parseUTCOffset(offsetStr);
            if (!offsetNs) [[unlikely]] {
                throwRangeError(globalObject, scope, "offset property is not a valid UTC offset string"_s);
                return { };
            }
            givenOffsetNs = *offsetNs;
        }

        // second
        double second = readTimeField(Identifier::fromString(vm, "second"_s));
        RETURN_IF_EXCEPTION(scope, { });

        // timeZone
        JSValue timeZoneValue = obj->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, { });

        // year (required)
        JSValue yearProperty = obj->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, { });
        if (yearProperty.isUndefined()) [[unlikely]] {
            throwTypeError(globalObject, scope, "year property must be present"_s);
            return { };
        }
        double year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(year)) [[unlikely]] {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }

        // Resolve month vs monthCode
        if (monthProperty.isUndefined()) {
            ASSERT(otherMonth);
            month = otherMonth->monthNumber;
        } else {
            if (!(month > 0 && std::isfinite(month))) [[unlikely]] {
                throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
                return { };
            }
            if (otherMonth && static_cast<double>(otherMonth->monthNumber) != month) [[unlikely]] {
                throwRangeError(globalObject, scope, "month and monthCode properties must match"_s);
                return { };
            }
        }

        if (!timeZoneValue.isUndefined()) {
            // FIXME: ZDT path: construct ZonedDateTime from field values once implemented.
            throwRangeError(globalObject, scope, "relativeTo with timeZone (ZonedDateTime) is not yet implemented"_s);
            return { };
        }

        // PlainDate path: validate date, ignore time/offset fields.
        // Step 5f: InterpretTemporalDateTimeFields → isoDate, time.
        // Step 5g-i: timeZone, offsetString, offsetBehaviour.
        // (Time fields and offset are read above but not yet used — pending ZDT support.)
        UNUSED_PARAM(hour);
        UNUSED_PARAM(minute);
        UNUSED_PARAM(second);
        UNUSED_PARAM(millisecond);
        UNUSED_PARAM(microsecond);
        UNUSED_PARAM(nanosecond);
        UNUSED_PARAM(givenOffsetNs);

        // Step 7 (no timeZone): return {PlainRelativeTo: CreateTemporalDate(isoDate, calendar)}.
        auto plainDate = isoDateFromFields(globalObject, TemporalDateFormat::Date, static_cast<int32_t>(year), static_cast<unsigned>(month), static_cast<unsigned>(day), otherMonth, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        return RelativeToRecord { plainDate, true, calendarId };
    }

    // Step 6a: If value is not a String, throw TypeError.
    if (!relativeToValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "relativeTo must be a string or Temporal object"_s);
        return { };
    }

    // Step 6b: ParseISODateTime(value, «TemporalDateTimeString[+Zoned], TemporalDateTimeString[~Zoned]»).
    String string = relativeToValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto parsed = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    if (!parsed) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is not a valid date or ZonedDateTime string"_s));
        return { };
    }
    auto& [parsedDate, parsedTimeOpt, parsedTzOpt, parsedCalOpt] = *parsed;

    // Step 6g: calendar from result; canonicalize.
    if (parsedCalOpt && !WTF::equalIgnoringASCIICase(StringView(*parsedCalOpt), "iso8601"_s)) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, StringView(*parsedCalOpt), "' is not a valid calendar identifier"_s));
        return { };
    }

    // Steps 6c-f: timeZone annotation, offsetString, offsetBehaviour, matchBehaviour.
    if (parsedTzOpt) {
        bool hasBracket = std::holds_alternative<int64_t>(parsedTzOpt->m_nameOrOffset)
            || !std::get<Vector<Latin1Character>>(parsedTzOpt->m_nameOrOffset).isEmpty();
        if (hasBracket) {
            // Steps 8-12: ZonedDateTime creation — FIXME: not yet implemented.
            throwRangeError(globalObject, scope, "relativeTo as ZonedDateTime string is not yet implemented"_s);
            return { };
        }
        // Z without bracket is invalid for relativeTo (spec requires bracket annotation for ZDT).
        if (parsedTzOpt->m_z) [[unlikely]] {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is not a valid relativeTo string: 'Z' designator requires a time zone annotation"_s));
            return { };
        }
        // Bare numeric offset without bracket → no timeZone, treated as PlainDate (step 7).
    }

    // Step 7: timeZone is ~unset~ → return {PlainRelativeTo: CreateTemporalDate(isoDate, calendar)}.
    if (!ISO8601::isDateTimeWithinLimits(parsedDate.year(), parsedDate.month(), parsedDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is outside the representable range for a relativeTo parameter"_s));
        return { };
    }
    return RelativeToRecord { parsedDate, true };
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.compare
JSValue TemporalDuration::compare(JSGlobalObject* globalObject, JSValue valueOne, JSValue valueTwo, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* one = toTemporalDuration(globalObject, valueOne);
    RETURN_IF_EXCEPTION(scope, { });

    auto* two = toTemporalDuration(globalObject, valueTwo);
    RETURN_IF_EXCEPTION(scope, { });

    // Always validate options type (even if we don't need relativeTo).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Parse relativeTo — must happen before identity check per spec ordering.
    RelativeToRecord relativeTo;
    if (options) {
        relativeTo = toRelativeTemporalObject(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // After parsing relativeTo, identical durations always compare equal.
    if (one->years() == two->years()
        && one->months() == two->months()
        && one->weeks() == two->weeks() && one->days() == two->days()
        && one->hours() == two->hours() && one->minutes() == two->minutes()
        && one->seconds() == two->seconds() && one->milliseconds() == two->milliseconds()
        && one->microseconds() == two->microseconds() && one->nanoseconds() == two->nanoseconds()) {
        return jsNumber(0);
    }

    // FIXME: TemporalZonedDateTime
    // ZDT path (relativeTo.zonedRelativeTo) is skipped until TemporalZonedDateTime is implemented.

    // Fast path: no ZDT addZonedDateTime needed — pure 24h-day time comparison.
    bool hasCalendarUnits = one->years() || two->years() || one->months() || two->months() || one->weeks() || two->weeks();
    if (!hasCalendarUnits) {
        auto timeDuration1 = add24HourDaysToTimeDuration(globalObject, toInternalDuration(one->m_duration).time(), one->days());
        RETURN_IF_EXCEPTION(scope, { });
        auto timeDuration2 = add24HourDaysToTimeDuration(globalObject, toInternalDuration(two->m_duration).time(), two->days());
        RETURN_IF_EXCEPTION(scope, { });
        return jsNumber(timeDuration1 > timeDuration2 ? 1 : timeDuration1 < timeDuration2 ? -1 : 0);
    }

    if (!relativeTo.hasPlainRelativeTo) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot compare a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    // PlainDate relativeTo: DateDurationDays(dateDuration, plainDate).
    auto& plainDate = relativeTo.plainDate;

    ISO8601::Duration dateDuration1(one->years(), one->months(), one->weeks(), one->days(), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
    auto endDate1 = calendarAwareDateAdd(globalObject, relativeTo.calendarId, plainDate, dateDuration1, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });
    auto daysDiff1 = TemporalCore::diffISODate(plainDate, endDate1, TemporalUnit::Day);
    auto timeDuration1 = add24HourDaysToTimeDuration(globalObject, toInternalDuration(one->m_duration).time(), daysDiff1.days());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration dateDuration2(two->years(), two->months(), two->weeks(), two->days(), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
    auto endDate2 = calendarAwareDateAdd(globalObject, relativeTo.calendarId, plainDate, dateDuration2, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });
    auto daysDiff2 = TemporalCore::diffISODate(plainDate, endDate2, TemporalUnit::Day);
    auto timeDuration2 = add24HourDaysToTimeDuration(globalObject, toInternalDuration(two->m_duration).time(), daysDiff2.days());
    RETURN_IF_EXCEPTION(scope, { });

    return jsNumber(timeDuration1 > timeDuration2 ? 1 : timeDuration1 < timeDuration2 ? -1 : 0);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.with
ISO8601::Duration TemporalDuration::with(JSGlobalObject* globalObject, JSObject* durationLike) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration result;
    auto hasRelevantProperty = false;
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        JSValue value = durationLike->get(globalObject, temporalUnitPluralPropertyName(vm, unit));
        RETURN_IF_EXCEPTION(scope, { });

        if (value.isUndefined()) {
            result.setField(unit, m_duration[unit]);
            continue;
        }

        hasRelevantProperty = true;
        double v = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(v) || !std::isfinite(v)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return { };
        }
        result.setField(unit, v);
    }

    if (!hasRelevantProperty) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal.Duration property"_s);
        return { };
    }

    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecordwith24hourdays
ISO8601::InternalDuration TemporalDuration::toInternalDurationRecordWith24HourDays(JSGlobalObject* globalObject,
    ISO8601::Duration d)
{
    auto result = TemporalCore::toInternalDurationRecordWith24HourDays(d);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// Thin shim: delegates to TemporalCore::regulateISODate and converts TemporalResult → optional.
std::optional<ISO8601::PlainDate> TemporalDuration::regulateISODate(int32_t year, int32_t month, int64_t day, TemporalOverflow overflow)
{
    auto result = TemporalCore::regulateISODate(year, month, day, overflow);
    return result ? std::optional(*result) : std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-todatedurationrecordwithouttime
ISO8601::Duration TemporalDuration::toDateDurationRecordWithoutTime(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    auto result = TemporalCore::toDateDurationRecordWithoutTime(duration);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.add
// Spec: Duration.prototype.add(other) — no relativeTo; throws RangeError if calendar units present.
ISO8601::Duration TemporalDuration::add(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto other = toISO8601Duration(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto largestUnit = std::min(TemporalCore::largestSubduration(m_duration), TemporalCore::largestSubduration(other));
    RELEASE_AND_RETURN(scope, addDurations(globalObject, AddOrSubtract::Add, other, largestUnit));
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurations
/* static */ ISO8601::Duration TemporalDuration::addDurations(JSGlobalObject* globalObject,
    AddOrSubtract op, ISO8601::Duration other, TemporalUnit largestUnit) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (op == AddOrSubtract::Subtract)
        other = -other;

    // Spec step 6: if either duration has calendar units, throw RangeError.
    if (isCalendarUnit(largestUnit)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot add or subtract durations with calendar units (years, months, or weeks)"_s);
        return { };
    }

    auto d1 = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto d2 = toInternalDurationRecordWith24HourDays(globalObject, other);
    RETURN_IF_EXCEPTION(scope, { });
    auto timeResult = d1.time() + d2.time();
    if (absInt128(timeResult) > ISO8601::InternalDuration::maxTimeDuration) [[unlikely]] {
        throwRangeError(globalObject, scope, "Sum of durations exceeds maximum time duration"_s);
        return { };
    }

    auto result = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(),
        timeResult);
    return temporalDurationFromInternal(result, largestUnit);
}

// https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecord
ISO8601::InternalDuration TemporalDuration::toInternalDuration(ISO8601::Duration d)
{
    return TemporalCore::toInternalDuration(d);
}

// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationfrominternal
ISO8601::Duration TemporalDuration::temporalDurationFromInternal(ISO8601::InternalDuration internalDuration, TemporalUnit largestUnit)
{
    return TemporalCore::temporalDurationFromInternal(internalDuration, largestUnit);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.subtract
// Spec: Duration.prototype.subtract(other) — no relativeTo; throws RangeError if calendar units present.
ISO8601::Duration TemporalDuration::subtract(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto other = toISO8601Duration(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto largestUnit = std::min(TemporalCore::largestSubduration(m_duration), TemporalCore::largestSubduration(other));
    RELEASE_AND_RETURN(scope, addDurations(globalObject, AddOrSubtract::Subtract, other, largestUnit));
}

// https://tc39.es/proposal-temporal/#sec-temporal-combinedateandtimerecord
std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> TemporalDuration::combineISODateAndTimeRecord(ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime)
{
    return { isoDate, isoTime };
}

// Local wrapper: tuple-form getUTCEpochNanoseconds delegates to TemporalCore two-arg form.
// https://tc39.es/proposal-temporal/#sec-temporal-getutcepochnanoseconds
Int128 getUTCEpochNanoseconds(std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime)
{
    return TemporalCore::getUTCEpochNanoseconds(std::get<0>(isoDateTime), std::get<1>(isoDateTime));
}

// RoundDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-roundduration
ISO8601::InternalDuration TemporalDuration::round(JSGlobalObject* globalObject, ISO8601::InternalDuration internalDuration, double increment, TemporalUnit unit, RoundingMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(unit >= TemporalUnit::Day);

    if (unit == TemporalUnit::Day) {
        double fractionalDays = TemporalCore::totalTimeDuration(internalDuration.time(), TemporalUnit::Day);
        double days = TemporalCore::roundNumberToIncrementDouble(fractionalDays, increment, mode);
        return ISO8601::InternalDuration::combineDateAndTimeDuration(
            ISO8601::Duration { 0LL, 0LL, 0LL, static_cast<int64_t>(days), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) },
            0);
    } else  {
        std::optional<Int128> timeDuration =
            ISO8601::roundTimeDuration(globalObject, internalDuration.time(), increment, unit, mode);
        RETURN_IF_EXCEPTION(scope, { });
        if (!timeDuration) [[unlikely]] {
            throwRangeError(globalObject, scope, "Rounded duration exceeds maximum time duration"_s);
            return { };
        }
        return ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), timeDuration.value());
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round
ISO8601::Duration TemporalDuration::round(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = nullptr;
    std::optional<TemporalUnit> smallest;
    TemporalUnit defaultLargestUnit = TemporalCore::largestSubduration(m_duration);
    if (optionsValue.isString()) {
        auto string = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalUnitType(string);
        if (!smallest) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
    }

    bool smallestUnitPresent = true;
    bool largestUnitPresent = true;

    auto largestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->largestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Parse relativeTo before roundingIncrement/roundingMode/smallestUnit (spec step ordering).
    RelativeToRecord relativeTo;
    if (options) {
        relativeTo = toRelativeTemporalObject(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });
    }

    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    if (!smallest) {
        auto smallestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(std::holds_alternative<std::optional<TemporalUnit>>(smallestUnitMaybeAuto));
        auto smallestUnitOptional = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
        if (smallestUnitOptional)
            smallest = smallestUnitOptional.value();
    }

    validateTemporalUnitValue(globalObject, smallest, UnitGroup::DateTime, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallestUnit = TemporalUnit::Nanosecond;
    if (!smallest)
        smallestUnitPresent = false;
    else
        smallestUnit = smallest.value();

    auto existingLargestUnit = TemporalCore::largestSubduration(m_duration);
    defaultLargestUnit = std::min(existingLargestUnit, smallestUnit);

    auto largestUnit = defaultLargestUnit;
    if (isAbsentUnit(largestUnitMaybeAuto))
        largestUnitPresent = false;
    else if (std::holds_alternative<std::optional<TemporalUnit>>(largestUnitMaybeAuto))
        largestUnit = std::get<std::optional<TemporalUnit>>(largestUnitMaybeAuto).value();

    if (!smallestUnitPresent && !largestUnitPresent) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot round without a smallestUnit or largestUnit option"_s);
        return { };
    }

    if (smallestUnit < largestUnit) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }
    auto maximum = TemporalCore::maximumRoundingIncrement(smallestUnit);
    validateTemporalRoundingIncrement(globalObject, roundingIncrement, maximum, Inclusivity::Exclusive);
    RETURN_IF_EXCEPTION(scope, { });

    if (roundingIncrement > 1 && largestUnit != smallestUnit && smallestUnit <= TemporalUnit::Day) [[unlikely]] {
        throwRangeError(globalObject, scope, "Incompatible rounding increment and largest/smallest units"_s);
        return { };
    }

    bool hasRelativeTo = relativeTo.hasPlainRelativeTo;
    // FIXME: TemporalZonedDateTime
    // bool hasRelativeTo = relativeTo.zonedRelativeTo || relativeTo.hasPlainRelativeTo;

    if (!hasRelativeTo) {
        if (years() || months() || weeks() || isCalendarUnit(largestUnit) || isCalendarUnit(smallestUnit)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Cannot round a duration of years, months, or weeks without a relativeTo option"_s);
            return { };
        }
    }

    // FIXME: TemporalZonedDateTime
    // if (relativeTo.zonedRelativeTo) { ... ZDT rounding path skipped ... }

    if (relativeTo.hasPlainRelativeTo) {
        auto& plainDate = relativeTo.plainDate;
        ISO8601::PlainTime midnight;

        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, { });

        // Add Y/M/W to base date; days are already folded into the time ns.
        auto intermediateDate = calendarAwareDateAdd(globalObject, relativeTo.calendarId, plainDate, internalDuration.dateDuration(), TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });

        auto [overflowDays, subdayNs] = TemporalCore::splitTimeDuration(internalDuration.time());
        ISO8601::Duration dayDuration(0LL, 0LL, 0LL, static_cast<int64_t>(overflowDays), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
        auto targetDate = calendarAwareDateAdd(globalObject, relativeTo.calendarId, intermediateDate, dayDuration, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        ISO8601::PlainTime targetTime = TemporalCore::plainTimeFromSubdayNs(subdayNs);

        Int128 originEpochNs = getUTCEpochNanoseconds(combineISODateAndTimeRecord(plainDate, midnight));
        Int128 destEpochNs = getUTCEpochNanoseconds(combineISODateAndTimeRecord(targetDate, targetTime));

        // Spec: DifferencePlainDateTimeWithRounding early-return for zero duration,
        // then ISODateTimeWithinLimits check on both endpoints.
        if (originEpochNs == destEpochNs)
            return temporalDurationFromInternal(ISO8601::InternalDuration(), largestUnit);
        bool dtOutOfRange = !ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 0, 0, 0, 0, 0, 0)
            || !ISO8601::isDateTimeWithinLimits(targetDate.year(), targetDate.month(), targetDate.day(),
                targetTime.hour(), targetTime.minute(), targetTime.second(),
                targetTime.millisecond(), targetTime.microsecond(), targetTime.nanosecond());
        if (dtOutOfRange) [[unlikely]] {
            throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
            return { };
        }

        auto diff = TemporalCore::diffISODateTime(plainDate, midnight, targetDate, targetTime, largestUnit);
        auto roundResult2 = TemporalCore::roundRelativeDuration(diff, originEpochNs, destEpochNs, plainDate, midnight,
            largestUnit, roundingIncrement, smallestUnit, roundingMode, nullptr, relativeTo.calendarId);
        if (!roundResult2) [[unlikely]] {
            throwTemporalError(globalObject, scope, roundResult2.error());
            return { };
        }
        return temporalDurationFromInternal(diff, largestUnit);
    }

    // No relativeTo — pure sub-day time path.
    ISO8601::InternalDuration internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = round(globalObject, internalDuration, roundingIncrement, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    return temporalDurationFromInternal(result, largestUnit);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.total
double TemporalDuration::total(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = nullptr;
    String unitString;
    if (optionsValue.isString()) {
        unitString = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, 0);
    }

    // Parse relativeTo before unit — spec requires this ordering.
    RelativeToRecord relativeTo;
    if (options) {
        relativeTo = toRelativeTemporalObject(globalObject, options);
        RETURN_IF_EXCEPTION(scope, 0);
        unitString = intlStringOption(globalObject, options, vm.propertyNames->unit, { }, { }, { });
        RETURN_IF_EXCEPTION(scope, 0);
    }

    auto unitType = temporalUnitType(unitString);
    if (!unitType) [[unlikely]] {
        throwRangeError(globalObject, scope, "unit is an invalid Temporal unit"_s);
        return 0;
    }
    TemporalUnit unit = unitType.value();

    bool hasRelativeTo = relativeTo.hasPlainRelativeTo;
    // FIXME: TemporalZonedDateTime
    // bool hasRelativeTo = relativeTo.zonedRelativeTo || relativeTo.hasPlainRelativeTo;

    if (!hasRelativeTo) {
        if (isCalendarUnit(unit) || years() || months() || weeks()) [[unlikely]] {
            throwRangeError(globalObject, scope, "Cannot total a duration of years, months, or weeks without a relativeTo option"_s);
            return 0;
        }
        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, 0);
        return TemporalCore::totalTimeDuration(internalDuration.time(), unit);
    }

    // FIXME: TemporalZonedDateTime
    // if (relativeTo.zonedRelativeTo) { ... ZDT total path skipped ... }

    // PlainDate relativeTo path.
    {
        auto& plainDate = relativeTo.plainDate;
        ISO8601::PlainTime midnight;

        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, 0);

        auto intermediateDate = calendarAwareDateAdd(globalObject, relativeTo.calendarId, plainDate, internalDuration.dateDuration(), TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, 0);

        auto [overflowDays, subdayNs] = TemporalCore::splitTimeDuration(internalDuration.time());
        ISO8601::Duration dayDuration(0LL, 0LL, 0LL, static_cast<int64_t>(overflowDays), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
        auto targetDate = calendarAwareDateAdd(globalObject, relativeTo.calendarId, intermediateDate, dayDuration, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, 0);
        ISO8601::PlainTime targetTime = TemporalCore::plainTimeFromSubdayNs(subdayNs);

        Int128 originEpochNs = getUTCEpochNanoseconds(combineISODateAndTimeRecord(plainDate, midnight));
        Int128 destEpochNs = getUTCEpochNanoseconds(combineISODateAndTimeRecord(targetDate, targetTime));

        // Spec: DifferencePlainDateTimeWithTotal early-return for zero duration,
        // then ISODateTimeWithinLimits check on both endpoints.
        if (originEpochNs == destEpochNs)
            return 0;
        bool dtOutOfRange2 = !ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 0, 0, 0, 0, 0, 0)
            || !ISO8601::isDateTimeWithinLimits(targetDate.year(), targetDate.month(), targetDate.day(),
                targetTime.hour(), targetTime.minute(), targetTime.second(),
                targetTime.millisecond(), targetTime.microsecond(), targetTime.nanosecond());
        if (dtOutOfRange2) [[unlikely]] {
            throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
            return 0;
        }

        if (unit >= TemporalUnit::Day)
            return TemporalCore::totalTimeDuration(destEpochNs - originEpochNs, unit);

        // Calendar units: diff then nudge.
        auto diff = TemporalCore::diffISODateTime(plainDate, midnight, targetDate, targetTime, unit);
        int32_t sign = (destEpochNs > originEpochNs) ? 1 : (destEpochNs < originEpochNs) ? -1 : 1;
        auto nudgedResult2 = TemporalCore::nudgeToCalendarUnit(sign, diff, originEpochNs, destEpochNs,
            plainDate, midnight, 1.0, unit, RoundingMode::Trunc, nullptr, relativeTo.calendarId);
        if (!nudgedResult2) [[unlikely]] {
            throwTemporalError(globalObject, scope, nudgedResult2.error());
            return 0;
        }
        return nudgedResult2->total;
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tostring
String TemporalDuration::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    // Read options in spec alphabetical order: fractionalSecondDigits, roundingMode, smallestUnit.
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallestUnitResult = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Validate smallestUnit.
    std::optional<TemporalUnit> smallestUnit;
    if (std::holds_alternative<TemporalAuto>(smallestUnitResult)) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit \"auto\" is not valid for toString"_s);
        return { };
    }
    smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    if (smallestUnit) {
        auto disallowed = { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour, TemporalUnit::Minute };
        if (std::ranges::find(disallowed, *smallestUnit) != disallowed.end()) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit must not be \"minute\" or larger"_s);
            return { };
        }
    }

    // Compute precision from (smallestUnit, digits).
    PrecisionData data;
    if (smallestUnit) {
        switch (*smallestUnit) {
        case TemporalUnit::Second: data = { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 }; break;
        case TemporalUnit::Millisecond: data = { { Precision::Fixed, 3 }, TemporalUnit::Millisecond, 1 }; break;
        case TemporalUnit::Microsecond: data = { { Precision::Fixed, 6 }, TemporalUnit::Microsecond, 1 }; break;
        case TemporalUnit::Nanosecond: data = { { Precision::Fixed, 9 }, TemporalUnit::Nanosecond, 1 }; break;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
    } else if (!digits)
        data = { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };
    else {
        auto pow10 = [](unsigned n) -> unsigned {
            unsigned r = 1;
            for (unsigned i = 0; i < n; i++)
                r *= 10;
            return r;
        };
        unsigned d = digits.value();
        if (!d)
            data = { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
        else if (d <= 3)
            data = { { Precision::Fixed, d }, TemporalUnit::Millisecond, pow10(3 - d) };
        else if (d <= 6)
            data = { { Precision::Fixed, d }, TemporalUnit::Microsecond, pow10(6 - d) };
        else
            data = { { Precision::Fixed, d }, TemporalUnit::Nanosecond, pow10(9 - d) };
    }

    // No need to make a new object if we were given explicit defaults.
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto timeDuration = ISO8601::roundTimeDuration(globalObject, internalDuration.time(),
        data.increment, data.unit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(internalDuration.dateDuration(),
        timeDuration);
    auto roundedLargestUnit = std::min(TemporalCore::largestSubduration(m_duration), TemporalUnit::Second);
    auto roundedDuration = temporalDurationFromInternal(internalDuration, roundedLargestUnit);
    RELEASE_AND_RETURN(scope, toString(globalObject, roundedDuration, data.precision));
}

static TemporalUnit NODELETE defaultTemporalLargestUnit(const ISO8601::Duration& duration)
{
    if (duration.years())
        return TemporalUnit::Year;
    if (duration.months())
        return TemporalUnit::Month;
    if (duration.weeks())
        return TemporalUnit::Week;
    if (duration.days())
        return TemporalUnit::Day;
    if (duration.hours())
        return TemporalUnit::Hour;
    if (duration.minutes())
        return TemporalUnit::Minute;
    if (duration.seconds())
        return TemporalUnit::Second;
    if (duration.milliseconds())
        return TemporalUnit::Millisecond;
    if (duration.microseconds())
        return TemporalUnit::Microsecond;
    return TemporalUnit::Nanosecond;
}

static void appendInteger(JSGlobalObject* globalObject, StringBuilder& builder, double value)
{
    ASSERT(std::isfinite(value));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double absValue = std::abs(value);
    if (absValue <= maxSafeInteger()) [[likely]] {
        builder.append(absValue);
        return;
    }

    auto* bigint = JSBigInt::createFrom(globalObject, absValue);
    RETURN_IF_EXCEPTION(scope, void());

    String string = bigint->toString(globalObject, 10);
    RETURN_IF_EXCEPTION(scope, void());

    builder.append(string);
}

// TemporalDurationToString ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, precision )
// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationtostring
String TemporalDuration::toString(JSGlobalObject* globalObject, const ISO8601::Duration& duration, std::tuple<Precision, unsigned> precision)
{
    ASSERT(std::get<0>(precision) == Precision::Auto || std::get<1>(precision) < 10);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringBuilder builder;
    auto sign = TemporalCore::durationSign(duration);
    if (sign < 0)
        builder.append('-');

    builder.append('P');
    if (duration.years()) {
        appendInteger(globalObject, builder, duration.years());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('Y');
    }
    if (duration.months()) {
        appendInteger(globalObject, builder, duration.months());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('M');
    }
    if (duration.weeks()) {
        appendInteger(globalObject, builder, duration.weeks());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('W');
    }
    if (duration.days()) {
        appendInteger(globalObject, builder, duration.days());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('D');
    }

    auto secondsDuration = TemporalCore::timeDurationFromComponents(0, 0, duration.seconds(), duration.milliseconds(), static_cast<double>(duration.microseconds()), static_cast<double>(duration.nanoseconds()));

    if (!duration.hours() && !duration.minutes() && !secondsDuration && sign && std::get<0>(precision) == Precision::Auto)
        return builder.toString();

    builder.append('T');
    if (duration.hours()) {
        appendInteger(globalObject, builder, duration.hours());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('H');
    }
    if (duration.minutes()) {
        appendInteger(globalObject, builder, duration.minutes());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('M');
    }

    bool zeroMinutesAndHigher = defaultTemporalLargestUnit(duration) >= TemporalUnit::Second;

    if (secondsDuration || (zeroMinutesAndHigher || std::get<0>(precision) != Precision::Auto)) {
        double secondsPart = std::abs(static_cast<double>(static_cast<int64_t>(secondsDuration / 1000000000)));
        double subSecondsPart = std::abs(static_cast<double>(static_cast<int64_t>(secondsDuration % 1000000000)));
        appendInteger(globalObject, builder, secondsPart);
        RETURN_IF_EXCEPTION(scope, { });
        formatSecondsStringFraction(builder, subSecondsPart, precision);
        builder.append('S');
    }

    return builder.toString();
}

} // namespace JSC
