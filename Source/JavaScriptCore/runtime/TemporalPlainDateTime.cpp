/*
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
#include "TemporalPlainDateTime.h"

#include "CalendarICUBridge.h"
#include "DurationArithmetic.h"
#include "ISOArithmetic.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "PlainDateTimeCore.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainTime.h"
// FIXME: #include "TemporalZonedDateTime.h"
#include "VMTrapsInlines.h"
namespace JSC {

const ClassInfo TemporalPlainDateTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateTime) };

TemporalPlainDateTime* TemporalPlainDateTime::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDateTime>(vm)) TemporalPlainDateTime(vm, structure, WTF::move(plainDate), WTF::move(plainTime));
    object->finishCreation(vm);
    return object;
}

TemporalPlainDateTime* TemporalPlainDateTime::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime, CalendarID calendarID)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDateTime>(vm)) TemporalPlainDateTime(vm, structure, WTF::move(plainDate), WTF::move(plainTime));
    object->m_calendarID = calendarID;
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainDateTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDateTime::TemporalPlainDateTime(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime)
    : Base(vm, structure)
    , m_plainDate(WTF::move(plainDate))
    , m_plainTime(WTF::move(plainTime))
    , m_calendarID(iso8601CalendarID())
{
}

void TemporalPlainDateTime::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldatetime
TemporalPlainDateTime* TemporalPlainDateTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond())) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDateTime::create(vm, structure, WTF::move(plainDate), WTF::move(plainTime));
}

TemporalPlainDateTime* TemporalPlainDateTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, structure, WTF::move(plainDate), WTF::move(plainTime)));
}

static TemporalPlainDateTime* fromImpl(JSGlobalObject*, JSValue, JSObject*);

// Implements ToTemporalDateTime core (steps 2-3); step 1 options handled by caller.
static TemporalPlainDateTime* fromImpl(JSGlobalObject* globalObject, JSValue itemValue, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDateTime>()) {
            // Step 2.a.i: GetOptionsObject + GetTemporalOverflowOption (validate only).
            if (options) {
                toTemporalOverflow(globalObject, options);
                RETURN_IF_EXCEPTION(scope, { });
            }
            // Step 2.a.iii: Return CreateTemporalDateTime(item.[[ISODateTime]], item.[[Calendar]]).
            auto* src = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            auto* clone = TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), src->plainDate(), src->plainTime());
            if (src->calendarID() != iso8601CalendarID())
                clone->setCalendarID(src->calendarID());
            return clone;
        }

        if (itemValue.inherits<TemporalPlainDate>()) {
            // Step 2.a.ii: GetOptionsObject + GetTemporalOverflowOption, return CreateTemporalDateTime.
            if (options) {
                toTemporalOverflow(globalObject, options);
                RETURN_IF_EXCEPTION(scope, { });
            }
            auto* pd = uncheckedDowncast<TemporalPlainDate>(itemValue);
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), pd->plainDate(), { }, pd->calendarID());
        }

        // FIXME: TemporalZonedDateTime conversion path not yet available
        // if (itemValue.inherits<TemporalZonedDateTime>()) { ... }

        JSObject* item = asObject(itemValue);

        // Step 2.b: GetTemporalCalendarIdentifierWithISODefault(item).
        // Step 2.c: PrepareCalendarFields(calendar, item, {year,month,monthCode,day},
        //           {hour,minute,second,millisecond,microsecond,nanosecond}, {}) — alphabetical.
        // calendar
        CalendarID extractedCalendarId = iso8601CalendarID();
        JSValue calendarProperty = item->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, { });
        if (!calendarProperty.isUndefined()) {
            auto calStr = toTemporalCalendarIdentifier(globalObject, calendarProperty);
            RETURN_IF_EXCEPTION(scope, { });
            auto canonicalized = isBuiltinCalendar(calStr);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, calStr, "' is not a valid calendar identifier"_s));
                return { };
            }
            extractedCalendarId = *canonicalized;
        }

        // day
        JSValue dayProperty = item->get(globalObject, vm.propertyNames->day);
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

        // era, eraYear (between day and hour, alphabetical)
        bool calIsNonISO = extractedCalendarId != iso8601CalendarID();
        bool calUsesEras = calIsNonISO && extractedCalendarId != chineseCalendarID() && extractedCalendarId != dangiCalendarID();
        std::optional<String> extractedEra;
        std::optional<int32_t> extractedEraYear;
        if (calUsesEras) {
            JSValue eraProperty = item->get(globalObject, Identifier::fromString(vm, "era"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!eraProperty.isUndefined()) {
                auto eraStr = eraProperty.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                extractedEra = WTF::move(eraStr);
            }
            JSValue eraYearProperty = item->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!eraYearProperty.isUndefined()) {
                double ey = eraYearProperty.toIntegerOrInfinity(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                if (!std::isfinite(ey)) [[unlikely]] {
                    throwRangeError(globalObject, scope, "eraYear property must be finite"_s);
                    return { };
                }
                extractedEraYear = static_cast<int32_t>(ey);
            }
        }

        // hour, microsecond, millisecond, minute (time fields interleaved before month)
        ISO8601::Duration timeDuration { };
        auto readTimeField = [&](JSValue val, TemporalUnit unit) -> bool {
            if (val.isUndefined())
                return true;
            double d = val.toIntegerOrInfinity(globalObject);
            if (scope.exception())
                return false;
            if (!std::isfinite(d)) [[unlikely]] {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return false;
            }
            timeDuration.setField(unit, d);
            return true;
        };

        JSValue hourProperty = item->get(globalObject, vm.propertyNames->hour);
        RETURN_IF_EXCEPTION(scope, { });
        if (!readTimeField(hourProperty, TemporalUnit::Hour))
            return { };

        JSValue microsecondProperty = item->get(globalObject, vm.propertyNames->microsecond);
        RETURN_IF_EXCEPTION(scope, { });
        if (!readTimeField(microsecondProperty, TemporalUnit::Microsecond))
            return { };

        JSValue millisecondProperty = item->get(globalObject, vm.propertyNames->millisecond);
        RETURN_IF_EXCEPTION(scope, { });
        if (!readTimeField(millisecondProperty, TemporalUnit::Millisecond))
            return { };

        JSValue minuteProperty = item->get(globalObject, vm.propertyNames->minute);
        RETURN_IF_EXCEPTION(scope, { });
        if (!readTimeField(minuteProperty, TemporalUnit::Minute))
            return { };

        // month
        JSValue monthProperty = item->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, { });
        double month = 0;
        if (!monthProperty.isUndefined()) {
            month = monthProperty.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // monthCode
        JSValue monthCodeProperty = item->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, { });
        std::optional<ParsedMonthCode> otherMonth;
        if (!monthCodeProperty.isUndefined()) {
            auto monthCodePrimitive = monthCodeProperty.toPrimitive(globalObject, PreferString);
            RETURN_IF_EXCEPTION(scope, { });
            if (!monthCodePrimitive.isString()) [[unlikely]] {
                throwTypeError(globalObject, scope, "monthCode must be a string"_s);
                return { };
            }
            auto monthCodeString = asString(monthCodePrimitive)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            otherMonth = ISO8601::parseMonthCode(monthCodeString);
            if (!otherMonth) [[unlikely]] {
                throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
                return { };
            }
            // Note: suitability (monthNumber 1-12, no leap month) is checked after year.
        }
        if (monthProperty.isUndefined() && !otherMonth) [[unlikely]] {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return { };
        }

        // nanosecond
        JSValue nanosecondProperty = item->get(globalObject, vm.propertyNames->nanosecond);
        RETURN_IF_EXCEPTION(scope, { });
        if (!readTimeField(nanosecondProperty, TemporalUnit::Nanosecond))
            return { };

        // second
        JSValue secondProperty = item->get(globalObject, vm.propertyNames->second);
        RETURN_IF_EXCEPTION(scope, { });
        if (!readTimeField(secondProperty, TemporalUnit::Second))
            return { };

        // year
        JSValue yearProperty = item->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, { });
        if (yearProperty.isUndefined() && !(extractedEra && extractedEraYear)) [[unlikely]] {
            throwTypeError(globalObject, scope, "year property must be present"_s);
            return { };
        }
        double year = yearProperty.isUndefined() ? 0 : yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(year)) [[unlikely]] {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }

        // Steps 2.d-e: GetOptionsObject + GetTemporalOverflowOption (after all fields per spec).
        TemporalOverflow overflow = TemporalOverflow::Constrain;
        if (options) {
            overflow = toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 2.f: InterpretTemporalDateTimeFields(calendar, fields, overflow).
        // Resolve month vs monthCode — done after all fields (including year) are read.
        bool isNonISO = extractedCalendarId != iso8601CalendarID();
        if (otherMonth) {
            if (!isNonISO && (otherMonth->monthNumber < 1 || otherMonth->monthNumber > 12 || otherMonth->isLeapMonth)) [[unlikely]] {
                throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
                return { };
            }
        }
        if (monthProperty.isUndefined()) {
            ASSERT(otherMonth);
            month = otherMonth->monthNumber;
        } else {
            if (!(month > 0 && std::isfinite(month))) [[unlikely]] {
                throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
                return { };
            }
            if (otherMonth && static_cast<double>(otherMonth->monthNumber) != month) [[unlikely]] {
                throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
                return { };
            }
        }


        // Validate/clamp month and day at double level before double→unsigned cast.
        // static_cast<unsigned> of values >= 2^32 is UB and wraps to 0 on x86.
        if (!isNonISO) {
            if (overflow == TemporalOverflow::Constrain) {
                month = std::clamp(month, 1.0, 12.0);
                day = std::clamp(day, 1.0, 31.0);
            } else {
                if (!(month >= 1 && month <= 12)) [[unlikely]] {
                    throwRangeError(globalObject, scope, "month is out of range"_s);
                    return { };
                }
                if (!(day >= 1 && day <= 31)) [[unlikely]] {
                    throwRangeError(globalObject, scope, "day is out of range"_s);
                    return { };
                }
            }
        }

        ISO8601::PlainDate plainDate;
        if (calUsesEras && (extractedEra || extractedEraYear)) {
            std::optional<StringView> era;
            if (extractedEra)
                era = StringView(*extractedEra);
            auto result = TemporalCore::calendarDateFromFields(
                extractedCalendarId, static_cast<int32_t>(year), static_cast<uint8_t>(month),
                static_cast<uint8_t>(day), era, extractedEraYear,
                otherMonth, overflow);
            if (!result) [[unlikely]] {
                throwRangeError(globalObject, scope, String(result.error().message));
                return { };
            }
            plainDate = *result;
        } else {
            plainDate = isoDateFromFields(globalObject, TemporalDateFormat::Date, static_cast<int32_t>(year), static_cast<unsigned>(month), static_cast<unsigned>(day), otherMonth, overflow, extractedCalendarId);
            RETURN_IF_EXCEPTION(scope, { });
        }

        auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDuration), overflow);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.g: CreateTemporalDateTime(result, calendar).
        auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime));
        RETURN_IF_EXCEPTION(scope, { });
        if (result && isNonISO)
            result->setCalendarID(extractedCalendarId);
        return result;
    }

    // Step 3: item is not a String — TypeError.
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to PlainDateTime from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (options) {
        toTemporalOverflow(globalObject, options); // Validate overflow
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 4: ParseISODateTime(item, {TemporalDateTimeString}).
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTF::move(dateTime.value());
        // Steps 5-7: extract and canonicalize [[Calendar]].
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
        // Steps 9-13: CreateISODateRecord + CombineISODateAndTimeRecord + CreateTemporalDateTime.
        if (!(timeZoneOptional && timeZoneOptional->m_z)) {
            auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), plainTimeOptional.value_or(ISO8601::PlainTime()));
            RETURN_IF_EXCEPTION(scope, { });
            if (result && calendarId != iso8601CalendarID())
                result->setCalendarID(calendarId);
            return result;
        }
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldatetime
// Entry point from Temporal.PlainDateTime.from() — handles step 1 (default options).
TemporalPlainDateTime* TemporalPlainDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 2: item is an Object.
    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDateTime>()) {
            // Step 2.a.i-iii: validate options, return new CreateTemporalDateTime.
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* src = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            auto* clone = TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), src->plainDate(), src->plainTime());
            if (src->calendarID() != iso8601CalendarID())
                clone->setCalendarID(src->calendarID());
            return clone;
        }
        if (itemValue.inherits<TemporalPlainDate>()) {
            // Step 2.a.ii: validate options, return CreateTemporalDateTime(date, midnight).
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* pd = uncheckedDowncast<TemporalPlainDate>(itemValue);
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), pd->plainDate(), { }, pd->calendarID());
        }
        // FIXME: TemporalZonedDateTime conversion path not yet available
        // if (itemValue.inherits<TemporalZonedDateTime>()) { ... }

        // Property bag: read all fields before options (ToTemporalDateTime step order).
        // Do NOT call intlGetOptionsObject here — it throws for null before fields are read.
        if (optionsValue.isUndefined())
            RELEASE_AND_RETURN(scope, fromImpl(globalObject, itemValue, nullptr));
        if (!optionsValue.isObject()) {
            // Read all fields first (spec order: PrepareCalendarFields before GetTemporalOverflowOption).
            fromImpl(globalObject, itemValue, nullptr);
            RETURN_IF_EXCEPTION(scope, { });
            throwTypeError(globalObject, scope, "options must be an object"_s);
            return { };
        }
        RELEASE_AND_RETURN(scope, fromImpl(globalObject, itemValue, asObject(optionsValue)));
    }

    // String: parse first, then validate options.
    if (itemValue.isString()) {
        auto* result = fromImpl(globalObject, itemValue, nullptr);
        RETURN_IF_EXCEPTION(scope, { });
        toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
        return result;
    }

    throwTypeError(globalObject, scope, "can only convert to PlainDateTime from object or string values"_s);
    return { };
}

String TemporalPlainDateTime::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options) {
        auto base = toString();
        auto calId = calendarIDAsString();
        if (calendarID() != iso8601CalendarID())
            return makeString(base, "[u-ca="_s, calId, ']');
        return base;
    }

    // Read options in spec order: calendarName, fractionalSecondDigits, roundingMode, smallestUnit.

    // calendarName (read for observable side-effect ordering, result used for output)
    String calOpt = "auto"_s;
    if (options) {
        calOpt = intlStringOption(globalObject, options, Identifier::fromString(vm, "calendarName"_s),
            { "auto"_s, "always"_s, "never"_s, "critical"_s }, "calendarName must be \"auto\", \"always\", \"never\", or \"critical\""_s, "auto"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // fractionalSecondDigits
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // roundingMode
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // smallestUnit
    auto smallestUnitResult = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Validate + compute precision.
    std::optional<TemporalUnit> smallestUnit;
    if (std::holds_alternative<TemporalAuto>(smallestUnitResult)) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit \"auto\" is not valid for toString"_s);
        return { };
    }
    smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    if (smallestUnit) {
        auto disallowed = { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour };
        if (std::ranges::find(disallowed, *smallestUnit) != disallowed.end()) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    }

    PrecisionData data;
    if (smallestUnit) {
        switch (*smallestUnit) {
        case TemporalUnit::Minute: data = { { Precision::Minute, 0 }, TemporalUnit::Minute, 1 }; break;
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
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc) {
        auto base = toString();
        auto calId = calendarIDAsString();
        if (calOpt == "never"_s)
            return base;
        if (calOpt == "always"_s)
            return makeString(base, "[u-ca="_s, calId, ']');
        if (calOpt == "critical"_s)
            return makeString(base, "[!u-ca="_s, calId, ']');
        if (calendarID() != iso8601CalendarID())
            return makeString(base, "[u-ca="_s, calId, ']');
        return base;
    }

    auto duration = TemporalPlainTime::roundTime(m_plainTime, data.increment, data.unit, roundingMode, std::nullopt);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    double extraDays = duration.days();
    ASSERT(!extraDays || extraDays == 1);
    auto plainDate = TemporalCore::balanceISODate(year(), month(), day() + static_cast<int64_t>(extraDays));

    bool roundOutOfRange = !ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(),
        plainTime.hour(), plainTime.minute(), plainTime.second(),
        plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond());
    if (roundOutOfRange) [[unlikely]] {
        throwRangeError(globalObject, scope, "Rounding result is outside the representable range"_s);
        return { };
    }

    auto base = ISO8601::temporalDateTimeToString(plainDate, plainTime, data.precision);
    auto calId = calendarIDAsString();
    if (calOpt == "never"_s)
        return base;
    if (calOpt == "always"_s)
        return makeString(base, "[u-ca="_s, calId, ']');
    if (calOpt == "critical"_s)
        return makeString(base, "[!u-ca="_s, calId, ']');
    if (calendarID() != iso8601CalendarID())
        return makeString(base, "[u-ca="_s, calId, ']');
    return base;
}

String TemporalPlainDateTime::monthCode() const
{
    return ISO8601::monthCode(m_plainDate.month());
}

uint8_t TemporalPlainDateTime::dayOfWeek() const
{
    return ISO8601::dayOfWeek(m_plainDate);
}

uint16_t TemporalPlainDateTime::dayOfYear() const
{
    return ISO8601::dayOfYear(m_plainDate);
}

uint8_t TemporalPlainDateTime::weekOfYear() const
{
    return ISO8601::weekOfYear(m_plainDate);
}

int32_t TemporalPlainDateTime::yearOfWeek() const
{
    return ISO8601::yearOfWeek(m_plainDate);
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindatetime
ISO8601::Duration TemporalPlainDateTime::differenceTemporalPlainDateTime(
    JSGlobalObject* globalObject, DifferenceOperation op,
    TemporalPlainDateTime* other,
    TemporalUnit smallestUnit, TemporalUnit largestUnit,
    RoundingMode roundingMode, double increment)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_calendarID != other->m_calendarID) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between date-times with different calendars"_s);
        return { };
    }

    auto coreResult = TemporalCore::differenceTemporalPlainDateTime(op,
        plainDate(), plainTime(), other->plainDate(), other->plainTime(),
        m_calendarID, smallestUnit, largestUnit, roundingMode, increment);
    if (!coreResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, coreResult.error());
        return { };
    }
    return *coreResult;
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
TemporalPlainDateTime* TemporalPlainDateTime::with(JSGlobalObject* globalObject, JSObject* temporalDateTimeLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalDateTimeLike);
    RETURN_IF_EXCEPTION(scope, { });

    int32_t baseYear = year();
    uint8_t baseMonth = month();
    uint8_t baseDay = day();
    if (!TemporalCore::calendarIsISO(m_calendarID)) {
        auto calFields = TemporalCore::isoToCalendarFields(m_calendarID, m_plainDate);
        if (calFields) {
            baseYear = calFields->year;
            baseMonth = calFields->month;
            baseDay = calFields->day;
        }
    }

    auto [y, m, d, optionalMonthCode, overflow, any] = TemporalPlainDate::mergeDateFields(globalObject, temporalDateTimeLike, optionsValue, baseYear, baseMonth, baseDay);
    RETURN_IF_EXCEPTION(scope, { });

    bool requiresTimeProperty = any == TemporalAnyProperties::None;
    auto [optionalHour, optionalMinute, optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond] = TemporalPlainTime::toPartialTime(globalObject, temporalDateTimeLike, !requiresTimeProperty);
    RETURN_IF_EXCEPTION(scope, { });

    auto plainDate = isoDateFromFields(globalObject, TemporalDateFormat::Date, y, m, d, optionalMonthCode, overflow, m_calendarID);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration duration { };
    duration.setField(TemporalUnit::Hour, optionalHour.value_or(hour()));
    duration.setField(TemporalUnit::Minute, optionalMinute.value_or(minute()));
    duration.setField(TemporalUnit::Second, optionalSecond.value_or(second()));
    duration.setField(TemporalUnit::Millisecond, optionalMillisecond.value_or(millisecond()));
    duration.setField(TemporalUnit::Microsecond, optionalMicrosecond.value_or(microsecond()));
    duration.setField(TemporalUnit::Nanosecond, optionalNanosecond.value_or(nanosecond()));
    auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(duration), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.round
TemporalPlainDateTime* TemporalPlainDateTime::round(JSGlobalObject* globalObject, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = nullptr;
    std::optional<TemporalUnit> smallest;
    if (optionsValue.isString()) {
        auto string = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalUnitType(string);
        if (!smallest) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }

        if (isCalendarUnit(smallest.value())) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
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
        smallest = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
        if (!smallest) [[unlikely]] {
            throwRangeError(globalObject, scope, "Cannot round without a smallestUnit option"_s);
            return { };
        }
    }

    auto smallestUnit = smallest.value();

    validateTemporalUnitValue(globalObject, smallestUnit, UnitGroup::Time, AllowedUnit::Day, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned maximum = 1;
    Inclusivity isInclusive = Inclusivity::Inclusive;
    if (smallestUnit != TemporalUnit::Day) {
        auto maximumOptional = TemporalCore::maximumRoundingIncrement(smallestUnit);
        ASSERT(maximumOptional);
        maximum = maximumOptional.value();
        isInclusive = Inclusivity::Exclusive;
    }
    validateTemporalRoundingIncrement(globalObject, roundingIncrement, maximum, isInclusive);
    RETURN_IF_EXCEPTION(scope, { });

    auto duration = TemporalPlainTime::roundTime(m_plainTime, roundingIncrement, smallestUnit, roundingMode, std::nullopt);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    double extraDays = duration.days();
    ASSERT(!extraDays || extraDays == 1);
    auto plainDate = TemporalCore::balanceISODate(year(), month(), day() + static_cast<int64_t>(extraDays));

    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime)));
}

} // namespace JSC
