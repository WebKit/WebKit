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
#include "InternalFunction.h"
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

// IsValidISODate + CreateISODateRecord
// https://tc39.es/proposal-temporal/#sec-temporal-isvalidisodate
ISO8601::PlainDate TemporalPlainDate::validateAndCreateISODateRecord(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
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

    // IsValidISODate Step 1: If month < 1 or month > 12, return false.
    if (!(monthDouble >= 1 && monthDouble <= 12)) [[unlikely]] {
        throwRangeError(globalObject, scope, "month is out of range"_s);
        return { };
    }
    uint32_t month = static_cast<uint32_t>(monthDouble);

    // IsValidISODate Steps 2-3: reject day outside [1, ISODaysInMonth(year, month)].
    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (!(dayDouble >= 1 && dayDouble <= daysInMonth)) [[unlikely]] {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }
    uint32_t day = static_cast<uint32_t>(dayDouble);

    // CreateISODateRecord ( year, month, day ): Return ISO Date Record { [[Year]], [[Month]], [[Day]] }.
    return ISO8601::PlainDate(year, month, day);
}

static bool isValidPlainDateOrThrow(JSGlobalObject* globalObject, ThrowScope& scope, const ISO8601::PlainDate& plainDate)
{
    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return false;
    }
    return true;
}

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldate
template<TemporalConstructTarget target>
static TemporalPlainDate* createTemporalDateImpl(JSGlobalObject* globalObject, ISO8601::PlainDate&& plainDate, CalendarID calendarID, TemporalNewTarget newTarget = { })
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If ISODateWithinLimits(isoDate) is false, throw a RangeError exception.
    if (!isValidPlainDateOrThrow(globalObject, scope, plainDate))
        return { };

    // Step 2: If newTarget is not present, set newTarget to %Temporal.PlainDate%.
    // Step 3: Let object be ? OrdinaryCreateFromConstructor(newTarget, "%Temporal.PlainDate.prototype%", « ... »).
    Structure* structure;
    if constexpr (target == TemporalConstructTarget::Intrinsic)
        structure = globalObject->plainDateStructure();
    else {
        ASSERT(newTarget.newTarget && newTarget.constructor);
        structure = JSC_GET_DERIVED_STRUCTURE(vm, plainDateStructure, newTarget.newTarget, newTarget.constructor);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Steps 4-5: set [[ISODate]] and [[Calendar]]. Step 6: Return object.
    return TemporalPlainDate::create(vm, structure, WTF::move(plainDate), calendarID);
}

TemporalPlainDate* createTemporalDate(JSGlobalObject* globalObject, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isValidPlainDateOrThrow(globalObject, scope, plainDate))
        return { };
    return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(plainDate));
}

TemporalPlainDate* createTemporalDate(JSGlobalObject* globalObject, ISO8601::PlainDate&& plainDate, String&& calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isValidPlainDateOrThrow(globalObject, scope, plainDate))
        return { };
    return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(plainDate), WTF::move(calendarId));
}

TemporalPlainDate* createTemporalDate(JSGlobalObject* globalObject, ISO8601::PlainDate&& plainDate, CalendarID calendarID)
{
    return createTemporalDateImpl<TemporalConstructTarget::Intrinsic>(globalObject, WTF::move(plainDate), calendarID);
}

TemporalPlainDate* createTemporalDate(JSGlobalObject* globalObject, ISO8601::PlainDate&& plainDate, CalendarID calendarID, TemporalNewTarget newTarget)
{
    return createTemporalDateImpl<TemporalConstructTarget::NewTarget>(globalObject, WTF::move(plainDate), calendarID, newTarget);
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
            auto [date, time] = zdt->getLocalDateTime(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!TemporalCore::calendarIsISO(zdt->calendarID()))
                return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), String(zdt->calendarId()));
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date));
        }

        // Step 2.d: calendar = ? GetTemporalCalendarIdentifierWithISODefault(item).
        CalendarID calendarId = getTemporalCalendarIdentifierWithISODefault(globalObject, asObject(itemValue));
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.e: fields = ? PrepareCalendarFields(...). Fields before options (spec order).
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
        RELEASE_AND_RETURN(scope, createTemporalDate(globalObject, WTF::move(result->isoDate), result->calendarId));
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
            RELEASE_AND_RETURN(scope, createTemporalDate(globalObject, WTF::move(plainDate)));
        RELEASE_AND_RETURN(scope, createTemporalDate(globalObject, WTF::move(plainDate), WTF::move(calendarId)));
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
            auto [date, time] = zdt->getLocalDateTime(globalObject);
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
            RELEASE_AND_RETURN(scope, createTemporalDate(globalObject, WTF::move(plainDate)));
        RELEASE_AND_RETURN(scope, createTemporalDate(globalObject, WTF::move(plainDate), WTF::move(calendarId)));
    }

    // Step 4: ParseISODateTime failed → throw RangeError.
    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindate
// Step 1 (ToTemporalDate) done by the prototype host fn via TemporalPlainDate::from().
// Uses spec's flip-mode + negate-at-end pattern for Since (unlike PlainTime's operand-swap).
template<DifferenceOperation op>
ISO8601::Duration TemporalPlainDate::differenceTemporalPlainDate(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 2: If CalendarEquals is false, throw RangeError.
    if (m_calendarID != other->m_calendarID) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between dates with different calendars"_s);
        return { };
    }

    // Steps 3-4: resolvedOptions = ? GetOptionsObject(options); settings = ? GetDifferenceSettings(operation, resolvedOptions, ~date~, «», ~day~, ~day~).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day, op);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: If CompareISODate = 0, return zero duration.
    if (!TemporalCore::isoDateCompare(plainDate(), other->plainDate()))
        return ISO8601::Duration();

    // Step 6: dateDifference = CalendarDateUntil(calendar, this, other, largestUnit).
    ISO8601::Duration dateDiff;
    if (!TemporalCore::calendarIsISO(m_calendarID))
        dateDiff = calendarDateUntil(m_calendarID, plainDate(), other->plainDate(), largestUnit);
    else
        dateDiff = TemporalCore::calendarDateUntil(plainDate(), other->plainDate(), largestUnit);

    // Step 7: duration = CombineDateAndTimeDuration(dateDifference, 0).
    ISO8601::InternalDuration duration = ISO8601::InternalDuration::combineDateAndTimeDuration(dateDiff, 0);

    // Step 8: If smallestUnit ≠ ~day~ or increment ≠ 1, RoundRelativeDuration
    // (spec sub-steps 8.a-e build isoDateTime + originEpochNs + destEpochNs).
    if (smallestUnit != TemporalUnit::Day || increment != 1) {
        auto isoDate = plainDate();
        Int128 originEpochNs = TemporalCore::getUTCEpochNanoseconds(isoDate, ISO8601::PlainTime());
        auto isoDateOther = other->plainDate();
        Int128 destEpochNs = TemporalCore::getUTCEpochNanoseconds(isoDateOther, ISO8601::PlainTime());
        auto roundResult = TemporalCore::roundRelativeDuration(
            duration, originEpochNs, destEpochNs, isoDate, ISO8601::PlainTime(),
            largestUnit, increment, smallestUnit, roundingMode, nullptr, m_calendarID);
        if (!roundResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, roundResult.error());
            return { };
        }
    }

    // Step 9: result = ! TemporalDurationFromInternal(duration, ~day~).
    auto durResult = TemporalCore::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    ISO8601::Duration result = *durResult;

    // Step 10: If since, negate result. Step 11: Return result.
    if constexpr (op == DifferenceOperation::Since)
        result = -result;
    return result;
}

template ISO8601::Duration TemporalPlainDate::differenceTemporalPlainDate<DifferenceOperation::Until>(JSGlobalObject*, TemporalPlainDate*, JSValue);
template ISO8601::Duration TemporalPlainDate::differenceTemporalPlainDate<DifferenceOperation::Since>(JSGlobalObject*, TemporalPlainDate*, JSValue);

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
// Step 3: Return ? DifferenceTemporalPlainDate(~until~, this, other, options).
ISO8601::Duration TemporalPlainDate::until(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    return differenceTemporalPlainDate<DifferenceOperation::Until>(globalObject, other, optionsValue);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
// Step 3: Return ? DifferenceTemporalPlainDate(~since~, this, other, options).
ISO8601::Duration TemporalPlainDate::since(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    return differenceTemporalPlainDate<DifferenceOperation::Since>(globalObject, other, optionsValue);
}

} // namespace JSC
