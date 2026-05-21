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
#include "FractionToDouble.h"
#include "IntlObjectInlines.h"
#include "Rounding.h"
#include "JSCInlines.h"
#include "DurationArithmetic.h"
#include "ISOArithmetic.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalZonedDateTime.h"
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

    if (!ISO8601::isValidDuration(duration)) {
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

        if (!isInteger(v) || !std::isfinite(v)) {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return { };
        }
        result.setField(unit, v);
    }

    if (!hasRelevantProperty) {
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
        if (!itemValue.isString()) {
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

    if (!ISO8601::isValidDuration(duration)) {
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

    if (!isValidDuration(duration)) {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    for (TemporalUnit unit : disallowedUnits) {
        if (duration[unit]) {
            throwRangeError(globalObject, scope, makeString("Adding "_s, temporalUnitPluralPropertyName(vm, unit).publicName(), " not supported by Temporal.Instant. Try Temporal.ZonedDateTime instead"_s));
            return { };
        }
    }

    return duration;
}

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
static ISO8601::PlainDate calendarAwareDateAdd(JSGlobalObject* globalObject, StringView calendarId, const ISO8601::PlainDate& date, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    if (!calendarId.isEmpty() && calendarId != "iso8601"_s)
        return calendarDateAdd(globalObject, calendarId, date, duration, overflow);
    return isoDateAdd(globalObject, date, duration, overflow);
}

// https://tc39.es/proposal-temporal/#sec-temporal-torelativetemporalobject
struct RelativeToRecord {
    TemporalZonedDateTime* zonedRelativeTo { nullptr };
    ISO8601::PlainDate plainDate;
    bool hasPlainRelativeTo { false };
    String calendarId { "iso8601"_s };
};

static RelativeToRecord toRelativeTemporalObject(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue relativeToValue = options->get(globalObject, Identifier::fromString(vm, "relativeTo"_s));
    RETURN_IF_EXCEPTION(scope, { });

    if (relativeToValue.isUndefined())
        return { };

    if (relativeToValue.isObject()) {
        JSObject* obj = asObject(relativeToValue);
        if (obj->inherits<TemporalZonedDateTime>()) {
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(obj);
            return RelativeToRecord { zdt, { }, false, String(zdt->calendarId()) };
        }
        if (obj->inherits<TemporalPlainDateTime>()) {
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(obj);
            return RelativeToRecord { nullptr, pdt->plainDate(), true, String(pdt->calendarId()) };
        }
        if (obj->inherits<TemporalPlainDate>()) {
            auto* pd = uncheckedDowncast<TemporalPlainDate>(obj);
            return RelativeToRecord { nullptr, pd->plainDate(), true, String(pd->calendarId()) };
        }
        // Property bag: read ALL temporal fields in alphabetical order per spec
        // (GetTemporalRelativeToOption / PrepareCalendarFields).

        // calendar
        String calendarId = "iso8601"_s;
        JSValue calendarProperty = obj->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, { });
        if (!calendarProperty.isUndefined()) {
            calendarId = toTemporalCalendarIdentifier(globalObject, calendarProperty);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // day (required)
        JSValue dayProperty = obj->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, { });
        if (dayProperty.isUndefined()) {
            throwTypeError(globalObject, scope, "day property must be present"_s);
            return { };
        }
        double day = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!(day > 0 && std::isfinite(day))) {
            throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
            return { };
        }

        // era, eraYear (alphabetical, only read for calendars with eras)
        std::optional<String> era;
        std::optional<double> eraYear;
        bool calHasEras = TemporalCore::calendarHasEras(TemporalCore::calendarIDFromString(calendarId));
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
                if (!std::isfinite(ey)) {
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
            if (!std::isfinite(d)) {
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
            if (!otherMonth) {
                throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
                return { };
            }
        }
        if (monthProperty.isUndefined() && !otherMonth) {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return { };
        }

        // nanosecond
        double nanosecond = readTimeField(Identifier::fromString(vm, "nanosecond"_s));
        RETURN_IF_EXCEPTION(scope, { });

        // offset: GET at alphabetical position, then eagerly process via ToOffsetString
        // (ToPrimitive(string) + isString check + parse). The spec's PrepareCalendarFields
        // processes offset at read-time regardless of whether timeZone is present.
        // For PlainDate bags where offset is undefined, the processing is skipped.
        JSValue offsetProperty = obj->get(globalObject, Identifier::fromString(vm, "offset"_s));
        RETURN_IF_EXCEPTION(scope, { });
        std::optional<int64_t> givenOffsetNs;
        if (!offsetProperty.isUndefined()) {
            JSValue offsetPrimitive = offsetProperty.toPrimitive(globalObject, PreferString);
            RETURN_IF_EXCEPTION(scope, { });
            if (!offsetPrimitive.isString()) {
                throwTypeError(globalObject, scope, "offset property must be a string"_s);
                return { };
            }
            auto offsetStr = asString(offsetPrimitive)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto offsetNs = ISO8601::parseUTCOffset(offsetStr);
            if (!offsetNs) {
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
        if (yearProperty.isUndefined()) {
            throwTypeError(globalObject, scope, "year property must be present"_s);
            return { };
        }
        double year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(year)) {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }

        // Resolve month vs monthCode
        if (monthProperty.isUndefined()) {
            ASSERT(otherMonth);
            month = otherMonth->monthNumber;
        } else {
            if (!(month > 0 && std::isfinite(month))) {
                throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
                return { };
            }
            if (otherMonth && static_cast<double>(otherMonth->monthNumber) != month) {
                throwRangeError(globalObject, scope, "month and monthCode properties must match"_s);
                return { };
            }
        }

        if (!timeZoneValue.isUndefined()) {
            // ZDT path: construct ZonedDateTime from already-read field values.

            // Resolve timezone from string value (spec: no valueOf/toString coercion).
            if (!timeZoneValue.isString()) {
                throwTypeError(globalObject, scope, "timeZone must be a string"_s);
                return { };
            }
            auto tzString = asString(timeZoneValue)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto tzOpt = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
            if (!tzOpt) {
                throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));
                return { };
            }
            TimeZone timeZone = *tzOpt;
            String timeZoneId = timeZone.toString();

            // Build PlainDate.
            auto plainDate = isoDateFromFields(globalObject, TemporalDateFormat::Date,
                static_cast<int32_t>(year), static_cast<unsigned>(month), static_cast<unsigned>(day),
                otherMonth, TemporalOverflow::Constrain);
            RETURN_IF_EXCEPTION(scope, { });

            // Build PlainTime.
            ISO8601::Duration timeDur;
            timeDur.setField(TemporalUnit::Hour, hour);
            timeDur.setField(TemporalUnit::Minute, minute);
            timeDur.setField(TemporalUnit::Second, second);
            timeDur.setField(TemporalUnit::Millisecond, millisecond);
            timeDur.setField(TemporalUnit::Microsecond, microsecond);
            timeDur.setField(TemporalUnit::Nanosecond, nanosecond);
            auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), TemporalOverflow::Constrain);
            RETURN_IF_EXCEPTION(scope, { });

            // Get epoch nanoseconds. If offset was provided, validate it matches the timezone.
            if (givenOffsetNs) {
                auto possible = TemporalCore::getPossibleEpochNanosecondsFor(timeZone, plainDate, plainTime);
                if (!possible) {
                    throwRangeError(globalObject, scope, possible.error().message);
                    return { };
                }
                bool found = false;
                ISO8601::ExactTime matchedEpoch;
                for (auto& candidate : TemporalCore::epochCandidates(*possible)) {
                    auto offsetResult = TemporalCore::getOffsetNanosecondsFor(timeZone, candidate);
                    if (offsetResult && *offsetResult == *givenOffsetNs) {
                        matchedEpoch = candidate;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    throwRangeError(globalObject, scope, "offset does not agree with timezone for the given date/time"_s);
                    return { };
                }
                auto* zdt = TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), matchedEpoch, WTF::move(timeZone), WTF::move(timeZoneId), "iso8601"_s);
                return RelativeToRecord { zdt, { }, false };
            }

            auto epochNs = TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, timeZone, plainDate, plainTime, TemporalDisambiguation::Compatible);
            if (!epochNs)
                return { };

            auto* zdt = TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), *epochNs, WTF::move(timeZone), WTF::move(timeZoneId), "iso8601"_s);
            return RelativeToRecord { zdt, { }, false };
        }

        // PlainDate path: validate date, ignore time/offset fields.
        UNUSED_PARAM(hour);
        UNUSED_PARAM(minute);
        UNUSED_PARAM(second);
        UNUSED_PARAM(millisecond);
        UNUSED_PARAM(microsecond);
        UNUSED_PARAM(nanosecond);
        UNUSED_PARAM(givenOffsetNs);

        auto plainDate = isoDateFromFields(globalObject, TemporalDateFormat::Date,
            static_cast<int32_t>(year), static_cast<unsigned>(month), static_cast<unsigned>(day),
            otherMonth, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        return RelativeToRecord { nullptr, plainDate, true, WTF::move(calendarId) };
    }

    if (!relativeToValue.isString()) {
        throwTypeError(globalObject, scope, "relativeTo must be a string or Temporal object"_s);
        return { };
    }

    String string = relativeToValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto parsed = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    if (!parsed) {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is not a valid date or ZonedDateTime string"_s));
        return { };
    }
    auto& [parsedDate, parsedTimeOpt, parsedTzOpt, parsedCalOpt] = *parsed;

    // Validate calendar annotation — only iso8601 is supported.
    if (parsedCalOpt && !WTF::equalIgnoringASCIICase(StringView(*parsedCalOpt), "iso8601"_s)) {
        throwRangeError(globalObject, scope, makeString("'"_s, StringView(*parsedCalOpt), "' is not a valid calendar identifier"_s));
        return { };
    }

    if (parsedTzOpt) {
        bool hasBracket = std::holds_alternative<int64_t>(parsedTzOpt->m_nameOrOffset)
            || !std::get<Vector<Latin1Character>>(parsedTzOpt->m_nameOrOffset).isEmpty();
        if (hasBracket) {
            // Bracketed annotation (IANA name or numeric offset) → ZDT.
            auto* zdt = TemporalZonedDateTime::from(globalObject, relativeToValue, jsUndefined());
            RETURN_IF_EXCEPTION(scope, { });
            return RelativeToRecord { zdt, { }, false };
        }
        // Z without bracket is invalid for relativeTo (spec requires bracket annotation for ZDT).
        if (parsedTzOpt->m_z) {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is not a valid relativeTo string: 'Z' designator requires a time zone annotation"_s));
            return { };
        }
        // Bare numeric offset (e.g. "-07:00") without bracket → treat as PlainDate, ignore offset.
    }

    // Validate that the plain date is within representable range.
    if (!ISO8601::isDateTimeWithinLimits(parsedDate.year(), parsedDate.month(), parsedDate.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is outside the representable range for a relativeTo parameter"_s));
        return { };
    }
    return RelativeToRecord { nullptr, parsedDate, true };
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

    // ZDT path: use addZonedDateTime when either duration has a non-zero date-category unit
    // (years, months, weeks, or days). Pure time durations don't need TZ-aware comparison.
    bool hasDateUnit1 = one->years() || one->months() || one->weeks() || one->days();
    bool hasDateUnit2 = two->years() || two->months() || two->weeks() || two->days();
    if (relativeTo.zonedRelativeTo && (hasDateUnit1 || hasDateUnit2)) {
        auto* zdt = relativeTo.zonedRelativeTo;
        auto startExact = zdt->exactTime();
        auto endTime1 = TemporalCore::addZonedDateTime(startExact, zdt->timeZone(), one->m_duration, TemporalOverflow::Constrain, TemporalCore::calendarIDFromString(zdt->calendarId()));
        if (!endTime1) {
            throwRangeError(globalObject, scope, endTime1.error().message);
            return { };
        }
        auto endTime2 = TemporalCore::addZonedDateTime(startExact, zdt->timeZone(), two->m_duration, TemporalOverflow::Constrain, TemporalCore::calendarIDFromString(zdt->calendarId()));
        if (!endTime2) {
            throwRangeError(globalObject, scope, endTime2.error().message);
            return { };
        }
        Int128 ns1 = endTime1->epochNanoseconds();
        Int128 ns2 = endTime2->epochNanoseconds();
        return jsNumber(ns1 > ns2 ? 1 : ns1 < ns2 ? -1 : 0);
    }

    // Fast path: no ZDT addZonedDateTime needed — pure 24h-day time comparison.
    bool hasCalendarUnits = one->years() || two->years() || one->months() || two->months() || one->weeks() || two->weeks();
    if (!hasCalendarUnits) {
        auto timeDuration1 = add24HourDaysToTimeDuration(globalObject, toInternalDuration(one->m_duration).time(), one->days());
        RETURN_IF_EXCEPTION(scope, { });
        auto timeDuration2 = add24HourDaysToTimeDuration(globalObject, toInternalDuration(two->m_duration).time(), two->days());
        RETURN_IF_EXCEPTION(scope, { });
        return jsNumber(timeDuration1 > timeDuration2 ? 1 : timeDuration1 < timeDuration2 ? -1 : 0);
    }

    if (!relativeTo.hasPlainRelativeTo) {
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

        if (!isInteger(v) || !std::isfinite(v)) {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return { };
        }
        result.setField(unit, v);
    }

    if (!hasRelevantProperty) {
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
    if (largestUnit <= TemporalUnit::Week) {
        throwRangeError(globalObject, scope, "Cannot add or subtract durations with calendar units (years, months, or weeks)"_s);
        return { };
    }

    auto d1 = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto d2 = toInternalDurationRecordWith24HourDays(globalObject, other);
    RETURN_IF_EXCEPTION(scope, { });
    auto timeResult = d1.time() + d2.time();
    if (absInt128(timeResult) > ISO8601::InternalDuration::maxTimeDuration) {
        throwRangeError(globalObject, scope, "Sum of durations exceeds maximum time duration"_s);
        return { };
    }

    auto result = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(),
        timeResult);
    return temporalDurationFromInternal(result, largestUnit);
}

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

std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> TemporalDuration::combineISODateAndTimeRecord(ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime)
{
    return { isoDate, isoTime };
}

// Local wrapper: tuple-form getUTCEpochNanoseconds delegates to TemporalCore two-arg form.
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
        if (!timeDuration) {
            throwRangeError(globalObject, scope, "Rounded duration exceeds maximum time duration"_s);
            return { };
        }
        return ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), timeDuration.value());
    }
}

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
        if (!smallest) {
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

    bool hasRelativeTo = relativeTo.zonedRelativeTo || relativeTo.hasPlainRelativeTo;

    if (!hasRelativeTo) {
        if (years() || months() || weeks() || largestUnit <= TemporalUnit::Week || smallestUnit <= TemporalUnit::Week) {
            throwRangeError(globalObject, scope, "Cannot round a duration of years, months, or weeks without a relativeTo option"_s);
            return { };
        }
    }

    if (relativeTo.zonedRelativeTo) {
        auto* zdt = relativeTo.zonedRelativeTo;
        auto& tz = zdt->timeZone();
        auto startExact = zdt->exactTime();

        auto endExactResult = TemporalCore::addZonedDateTime(startExact, tz, m_duration, TemporalOverflow::Constrain, TemporalCore::calendarIDFromString(zdt->calendarId()));
        if (!endExactResult) {
            throwRangeError(globalObject, scope, endExactResult.error().message);
            return { };
        }
        auto endExact = *endExactResult;
        Int128 nsA = startExact.epochNanoseconds();
        Int128 nsB = endExact.epochNanoseconds();

        if (largestUnit > TemporalUnit::Day) {
            auto zdtInternal = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), nsB - nsA);
            RETURN_IF_EXCEPTION(scope, { });
            zdtInternal = round(globalObject, zdtInternal, roundingIncrement, smallestUnit, roundingMode);
            RETURN_IF_EXCEPTION(scope, { });
            return temporalDurationFromInternal(zdtInternal, largestUnit);
        }

        auto zdtDiffResult = TemporalCore::differenceZonedDateTimeWithRounding(startExact, endExact, tz, largestUnit, smallestUnit, roundingMode, static_cast<double>(roundingIncrement), TemporalCore::calendarIDFromString(zdt->calendarId()));
        if (!zdtDiffResult) {
            throwTemporalError(globalObject, scope, zdtDiffResult.error());
            return { };
        }
        // Spec Duration.round: if TemporalUnitCategory(largestUnit) is ~date~, set largestUnit to ~hour~.
        TemporalUnit effectiveLargestUnit = (largestUnit <= TemporalUnit::Day) ? TemporalUnit::Hour : largestUnit;
        return temporalDurationFromInternal(*zdtDiffResult, effectiveLargestUnit);
    }

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
        if (dtOutOfRange) {
            throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
            return { };
        }

        auto diff = TemporalCore::diffISODateTime(plainDate, midnight, targetDate, targetTime, largestUnit);
        auto roundResult2 = TemporalCore::roundRelativeDuration(diff, originEpochNs, destEpochNs, plainDate, midnight,
            largestUnit, roundingIncrement, smallestUnit, roundingMode, nullptr, TemporalCore::calendarIDFromString(relativeTo.calendarId));
        if (!roundResult2) {
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
    if (!unitType) {
        throwRangeError(globalObject, scope, "unit is an invalid Temporal unit"_s);
        return 0;
    }
    TemporalUnit unit = unitType.value();

    bool hasRelativeTo = relativeTo.zonedRelativeTo || relativeTo.hasPlainRelativeTo;

    if (!hasRelativeTo) {
        if (unit <= TemporalUnit::Week || years() || months() || weeks()) {
            throwRangeError(globalObject, scope, "Cannot total a duration of years, months, or weeks without a relativeTo option"_s);
            return 0;
        }
        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, 0);
        return TemporalCore::totalTimeDuration(internalDuration.time(), unit);
    }

    if (relativeTo.zonedRelativeTo) {
        auto* zdt = relativeTo.zonedRelativeTo;
        auto& tz = zdt->timeZone();
        auto startExact = zdt->exactTime();

        auto endExactResult = TemporalCore::addZonedDateTime(startExact, tz, m_duration, TemporalOverflow::Constrain, TemporalCore::calendarIDFromString(zdt->calendarId()));
        if (!endExactResult) {
            throwRangeError(globalObject, scope, endExactResult.error().message);
            return 0;
        }
        auto endExact = *endExactResult;
        Int128 nsA = startExact.epochNanoseconds();
        Int128 nsB = endExact.epochNanoseconds();

        // For sub-day time units: pure nanosecond arithmetic.
        // (Day with timezone is irregular-length — falls through to calendar path.)
        if (unit > TemporalUnit::Day)
            return TemporalCore::totalTimeDuration(nsB - nsA, unit);

        // Calendar units (Year/Month/Week/Day-with-TZ): full ZDT diff then nudge.
        auto zdtDiffResult2 = TemporalCore::differenceZonedDateTimeWithRounding(startExact, endExact, tz, unit, unit, RoundingMode::Trunc, 1.0, TemporalCore::calendarIDFromString(zdt->calendarId()));
        if (!zdtDiffResult2) {
            throwTemporalError(globalObject, scope, zdtDiffResult2.error());
            return 0;
        }
        auto zdtDiff = *zdtDiffResult2;

        ISO8601::PlainDate startDate;
        ISO8601::PlainTime startTime;
        auto offset1Result = TemporalCore::getOffsetNanosecondsFor(tz, startExact);
        if (!offset1Result) {
            throwRangeError(globalObject, scope, offset1Result.error().message);
            return 0;
        }
        TemporalCore::exactTimeToLocalDateAndTime(startExact, *offset1Result, startDate, startTime);

        int32_t sign = (nsB > nsA) ? 1 : (nsB < nsA) ? -1 : 1;
        auto nudgedResult1 = TemporalCore::nudgeToCalendarUnit(sign, zdtDiff, nsA, nsB, startDate, startTime,
            1.0, unit, RoundingMode::Trunc, &tz, TemporalCore::calendarIDFromString(relativeTo.zonedRelativeTo->calendarId()));
        if (!nudgedResult1) {
            throwTemporalError(globalObject, scope, nudgedResult1.error());
            return 0;
        }
        return nudgedResult1->total;
    }

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
        if (dtOutOfRange2) {
            throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
            return 0;
        }

        if (unit >= TemporalUnit::Day)
            return TemporalCore::totalTimeDuration(destEpochNs - originEpochNs, unit);

        // Calendar units: diff then nudge.
        auto diff = TemporalCore::diffISODateTime(plainDate, midnight, targetDate, targetTime, unit);
        int32_t sign = (destEpochNs > originEpochNs) ? 1 : (destEpochNs < originEpochNs) ? -1 : 1;
        auto nudgedResult2 = TemporalCore::nudgeToCalendarUnit(sign, diff, originEpochNs, destEpochNs,
            plainDate, midnight, 1.0, unit, RoundingMode::Trunc, nullptr, TemporalCore::calendarIDFromString(relativeTo.calendarId));
        if (!nudgedResult2) {
            throwTemporalError(globalObject, scope, nudgedResult2.error());
            return 0;
        }
        return nudgedResult2->total;
    }
}

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
    if (std::holds_alternative<TemporalAuto>(smallestUnitResult)) {
        throwRangeError(globalObject, scope, "smallestUnit \"auto\" is not valid for toString"_s);
        return { };
    }
    smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    if (smallestUnit) {
        auto disallowed = { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour, TemporalUnit::Minute };
        if (std::ranges::find(disallowed, *smallestUnit) != disallowed.end()) {
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
