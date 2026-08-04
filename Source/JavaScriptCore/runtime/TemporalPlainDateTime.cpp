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
#include "TemporalZonedDateTime.h"
#include "VMTrapsInlines.h"
namespace JSC {

const ClassInfo TemporalPlainDateTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateTime) };

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

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldatetime
TemporalPlainDateTime* TemporalPlainDateTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime, CalendarID calendarID)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If ISODateTimeWithinLimits(isoDateTime) is false, throw a RangeError exception.
    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond())) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    // Steps 2-6: OrdinaryCreateFromConstructor + set [[ISODateTime]] and [[Calendar]] internal slots.
    return TemporalPlainDateTime::create(vm, structure, WTF::move(plainDate), WTF::move(plainTime), calendarID);
}

static TemporalPlainDateTime* fromImpl(JSGlobalObject*, JSValue, JSValue);

// Property-bag + string tail of ToTemporalDateTime (steps 2.d-2.h + 3+); TPDT/PD/ZDT cases handled by the caller.
static TemporalPlainDateTime* fromImpl(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.isObject()) {
        JSObject* item = asObject(itemValue);

        // Step 2.d: GetTemporalCalendarIdentifierWithISODefault(item).
        CalendarID extractedCalendarId = getTemporalCalendarIdentifierWithISODefault(globalObject, item);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.e: PrepareCalendarFields(calendar, item, {year,month,monthCode,day},
        //           {hour,minute,second,millisecond,microsecond,nanosecond}, {}) — alphabetical.
        TemporalCore::TimeFieldsIn timeFields;
        TemporalCore::CalendarFieldsIn dateFields = readCalendarFieldsFromObject<FieldSetType::DateTime>(globalObject, item, extractedCalendarId, &timeFields);
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 2.f-g: GetOptionsObject + GetTemporalOverflowOption (after all fields per spec).
        TemporalOverflow overflow = toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.h: dateTimeResult = ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
        auto pdt = interpretTemporalDateTimeFields(globalObject, extractedCalendarId, dateFields, timeFields, overflow);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.h cont.: CreateTemporalDateTime(result, calendar).
        auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), ISO8601::PlainDate(pdt.date), ISO8601::PlainTime(pdt.time), extractedCalendarId);
        RETURN_IF_EXCEPTION(scope, { });
        return result;
    }

    // Step 3: item is not a String — TypeError.
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to PlainDateTime from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: ? ParseISODateTime(item, « TemporalDateTimeString[~Zoned] »).
    auto dateTime = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::DateTimeUnzoned);
    if (dateTime) [[likely]] {
        auto [plainDateOpt, plainTimeOptional, timeZoneOptional, calendarOptional, matched, isShortForm] = WTF::move(*dateTime);
        ASSERT(plainDateOpt);
        auto plainDate = WTF::move(*plainDateOpt);
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
        // Step 8: GetOptionsObject + GetTemporalOverflowOption (after parse, per spec).
        toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
        // Steps 9-13: CreateISODateRecord + CombineISODateAndTimeRecord + CreateTemporalDateTime.
        auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), plainTimeOptional.value_or(ISO8601::PlainTime()), calendarId);
        RETURN_IF_EXCEPTION(scope, { });
        return result;
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

    // Step 1: If options is not present, set options to undefined. (Caller passes jsUndefined().)

    // Step 2: If item is an Object:
    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDateTime>()) {
            // Step 2.a.i-ii: validate options, Step 2.a.iii: return new CreateTemporalDateTime.
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* src = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), src->plainDate(), src->plainTime(), src->calendarID());
        }
        if (itemValue.inherits<TemporalPlainDate>()) {
            // Step 2.c.i-ii: validate options, Step 2.c.iii: return CreateTemporalDateTime(date, midnight).
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            auto* pd = uncheckedDowncast<TemporalPlainDate>(itemValue);
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), pd->plainDate(), { }, pd->calendarID());
        }
        if (itemValue.inherits<TemporalZonedDateTime>()) {
            // Step 2.b.i: GetISODateTimeFor FIRST (before options — spec step order).
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
            auto [date, time] = zdt->getLocalDateTime(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            // Step 2.b.ii: GetOptionsObject + overflow.
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            // Step 2.b.iii: CreateTemporalDateTime.
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), WTF::move(date), WTF::move(time), zdt->calendarID());
        }
        // Steps 2.d-2.h: property bag — read all fields before options (ToTemporalDateTime step order).
        RELEASE_AND_RETURN(scope, fromImpl(globalObject, itemValue, optionsValue));
    }

    // String: fromImpl parses first, then validates options internally (Step 8).
    if (itemValue.isString())
        RELEASE_AND_RETURN(scope, fromImpl(globalObject, itemValue, optionsValue));

    throwTypeError(globalObject, scope, "can only convert to PlainDateTime from object or string values"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
String TemporalPlainDateTime::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding check done by the caller (temporalPlainDateTimePrototypeFuncToString).

    // Step 3: Let resolvedOptions be ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options) {
        // Fast path: no options ⇒ Precision::Auto ⇒ RoundISODateTime is a no-op; Steps 12-14 collapse to ISODateTimeToString.
        auto base = toString();
        if (calendarID() != iso8601CalendarID())
            return makeString(base, "[u-ca="_s, calendarIDAsString(), ']');
        return base;
    }

    // Step 4: NOTE: The following steps read options in alphabetical order.
    // Step 5: Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    String calOpt = temporalShowCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: Let roundingMode be ? GetRoundingModeOption(resolvedOptions, ~trunc~).
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 8: Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", ~unset~).
    auto smallestUnitResult = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 9: Perform ? ValidateTemporalUnitValue(smallestUnit, ~time~).
    validateTemporalUnitValue(globalObject, smallestUnitResult, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<TemporalUnit> smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    // Step 10: If smallestUnit is ~hour~, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit cannot be \"hour\" for PlainDateTime.toString"_s);
        return { };
    }

    // Step 11: Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto data = toSecondsStringPrecisionRecord(smallestUnit, digits);

    // Steps 12-14: RoundISODateTime + ISODateTimeWithinLimits + ISODateTimeToString.
    // Fast path: Precision::Auto ⇒ increment=1ns ⇒ rounding is a no-op.
    if (std::get<0>(data.precision) == Precision::Auto) {
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

    // Step 12: Let result be RoundISODateTime(plainDateTime.[[ISODateTime]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    Int128 incrementNs = static_cast<Int128>(lengthInNanoseconds(data.unit)) * static_cast<Int128>(data.increment);
    auto [roundedDate, roundedTime] = TemporalCore::roundISODateTime(plainDate(), m_plainTime, incrementNs, data.unit, roundingMode);

    // Step 13: If ISODateTimeWithinLimits(result) is false, throw a RangeError exception.
    bool roundOutOfRange = !ISO8601::isDateTimeWithinLimits(roundedDate.year(), roundedDate.month(), roundedDate.day(),
        roundedTime.hour(), roundedTime.minute(), roundedTime.second(),
        roundedTime.millisecond(), roundedTime.microsecond(), roundedTime.nanosecond());
    if (roundOutOfRange) [[unlikely]] {
        throwRangeError(globalObject, scope, "Rounding result is outside the representable range"_s);
        return { };
    }

    // Step 14: Return ISODateTimeToString(result, plainDateTime.[[Calendar]], precision.[[Precision]], showCalendar).
    auto base = ISO8601::temporalDateTimeToString(roundedDate, roundedTime, data.precision);
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

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindatetime
// Step 1 (ToTemporalDateTime on `other`) is done by the caller (host fn) before invoking this helper.
template<DifferenceOperation op>
ISO8601::Duration TemporalPlainDateTime::differenceTemporalPlainDateTime(JSGlobalObject* globalObject, TemporalPlainDateTime* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 2: If CalendarEquals(dateTime.[[Calendar]], other.[[Calendar]]) is false, throw a RangeError exception.
    if (m_calendarID != other->m_calendarID) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between date-times with different calendars"_s);
        return { };
    }

    // Step 3: settings = ? GetDifferenceSettings(operation, options, ~datetime~, «», ~nanosecond~, ~day~).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Day, op);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 4-8: If CompareISODateTime = 0 return zero-duration; else DifferenceISODateTime + optional round
    //            + TemporalDurationFromInternal + negate-if-since (all fused in TemporalCore::differenceTemporalPlainDateTime).
    auto coreResult = TemporalCore::differenceTemporalPlainDateTime(op,
        plainDate(), plainTime(), other->plainDate(), other->plainTime(),
        m_calendarID, smallestUnit, largestUnit, roundingMode, increment);
    if (!coreResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, coreResult.error());
        return { };
    }
    // Step 9: Return result.
    return *coreResult;
}

template ISO8601::Duration TemporalPlainDateTime::differenceTemporalPlainDateTime<DifferenceOperation::Until>(JSGlobalObject*, TemporalPlainDateTime*, JSValue);
template ISO8601::Duration TemporalPlainDateTime::differenceTemporalPlainDateTime<DifferenceOperation::Since>(JSGlobalObject*, TemporalPlainDateTime*, JSValue);

} // namespace JSC
