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
#include "TemporalPlainYearMonth.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "Rounding.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonth::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonth) };

TemporalPlainYearMonth* TemporalPlainYearMonth::create(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainYearMonth>(vm)) TemporalPlainYearMonth(vm, structure, WTF::move(plainYearMonth));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainYearMonth::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonth::TemporalPlainYearMonth(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
    : Base(vm, structure)
    , m_plainYearMonth(WTF::move(plainYearMonth))
    , m_calendarID(iso8601CalendarID())
{
}


// CreateTemporalYearMonth ( isoDate, calendar [, newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalyearmonth
TemporalPlainYearMonth* TemporalPlainYearMonth::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isYearMonthWithinLimits(plainDate.year(), plainDate.month())) [[unlikely]] {
        throwRangeError(globalObject, scope, "PlainYearMonth is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainYearMonth::create(vm, structure, ISO8601::PlainYearMonth(WTF::move(plainDate)));
}

String TemporalPlainYearMonth::toString() const
{
    return ISO8601::temporalYearMonthToString(m_plainYearMonth, "auto"_s, m_calendarID);
}

String TemporalPlainYearMonth::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options) [[likely]]
        return toString();

    String calendarName = temporalShowCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalYearMonthToString(m_plainYearMonth, calendarName, m_calendarID);
}

static TemporalPlainYearMonth* fromYearMonthString(JSGlobalObject*, StringView);

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
TemporalPlainYearMonth* TemporalPlainYearMonth::from(JSGlobalObject* globalObject, JSValue item, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: options defaults to undefined (handled by caller passing jsUndefined()).

    // Steps 4-13 (String path) are checked first so RangeError from parsing precedes
    // TypeError from options validation — see spec step ordering note.
    auto string = item.getString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!string.isNull()) {
        // Steps 4-13: ParseISODateTime → canonicalize calendar → CalendarYearMonthFromFields.
        auto* result = fromYearMonthString(globalObject, string);
        RETURN_IF_EXCEPTION(scope, { });
        // Step 8: GetOptionsObject + GetTemporalOverflowOption (validate only; overflow unused for strings).
        if (!optionsValue.isUndefined()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
        }
        RELEASE_AND_RETURN(scope, result);
    }

    // Step 2: item is an Object.
    if (item.isObject()) {
        if (item.inherits<TemporalPlainYearMonth>()) {
            // Step 2.a.i: GetOptionsObject(options).
            auto* resolvedOptions = intlGetOptionsObject(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            // Step 2.a.ii: GetTemporalOverflowOption (validate; result discarded).
            if (resolvedOptions) {
                toTemporalOverflow(globalObject, resolvedOptions);
                RETURN_IF_EXCEPTION(scope, { });
            }
            // Step 2.a.iii: Return CreateTemporalYearMonth(item.[[ISODate]], item.[[Calendar]]).
            auto* src = uncheckedDowncast<TemporalPlainYearMonth>(item);
            auto* clone = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), src->plainYearMonth());
            clone->setCalendarID(src->calendarID());
            return clone;
        }

        // Step 2.b: GetTemporalCalendarIdentifierWithISODefault(item).
        // Step 2.c: PrepareCalendarFields(calendar, item, {year, month, monthCode}, {}, {}).
        // (Steps 2.b-c are fused into readCalendarFieldsFromObject.)
        CalendarID calendarId = iso8601CalendarID();
        auto fields = readCalendarFieldsFromObject<FieldSetType::YearMonth>(globalObject, asObject(item), calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.d: GetOptionsObject(options). Step 2.e: GetTemporalOverflowOption.
        // Read options AFTER fields per spec (PrepareCalendarFields precedes GetOptionsObject).
        JSObject* opts = nullptr;
        if (!optionsValue.isUndefined()) {
            if (!optionsValue.isObject()) [[unlikely]] {
                throwTypeError(globalObject, scope, "options must be an object"_s);
                return { };
            }
            opts = asObject(optionsValue);
        }
        auto overflow = TemporalOverflow::Constrain;
        if (opts) {
            overflow = toTemporalOverflow(globalObject, opts);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 2.f: CalendarYearMonthFromFields(calendar, fields, overflow).
        auto resolved = TemporalCore::yearMonthFromFields(calendarId, fields, overflow);
        if (!resolved) [[unlikely]] {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }

        // Step 2.g: Return CreateTemporalYearMonth(isoDate, calendar).
        auto* result = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(resolved->isoDate)));
        result->setCalendarID(resolved->calendarId);
        return result;
    }

    // Step 3: item is not a String — throw TypeError.
    throwTypeError(globalObject, scope, "can only convert to PlainYearMonth from object or string values"_s);
    return { };
}

// Implements ToTemporalYearMonth steps 4-15 (step 8 done by caller after return):
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
static TemporalPlainYearMonth* fromYearMonthString(JSGlobalObject* globalObject, StringView string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 4: ParseISODateTime(item, « TemporalYearMonthString »).
    auto dateTime = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::YearMonth);
    if (!dateTime) {
        String message = tryMakeString("Temporal.PlainYearMonth.from: invalid year-month string "_s, string);
        throwRangeError(globalObject, scope, message.isNull() ? "Temporal.PlainYearMonth.from: invalid year-month string"_s : message);
        return { };
    }
    auto [plainDateOpt, plainTimeOptional, timeZoneOptional, calendarOptional, matched, isShortForm] = WTF::move(*dateTime);
    ASSERT(plainDateOpt);
    auto plainDate = WTF::move(*plainDateOpt);

    // Steps 5-7: extract and canonicalize [[Calendar]].
    // (parseISODateTime already enforced Step 4.a.ii.(3): short-form non-iso8601 → nullopt.)
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

    // Step 10: isoDate = CreateISODateRecord(year, month, day).
    //   For short form (YYYY-MM), parser stores day=1; for full date, parser extracts the actual day.
    // Step 11: ISOYearMonthWithinLimits — enforced inside tryCreateIfValid.
    // Step 12: ISODateToFields(calendar, isoDate, ~year-month~).
    // Steps 14-15: CalendarYearMonthFromFields(~constrain~) + CreateTemporalYearMonth — inside tryCreateIfValid.
    if (calendarId == iso8601CalendarID())
        RELEASE_AND_RETURN(scope, TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), ISO8601::PlainDate(plainDate.year(), plainDate.month(), 1)));

    // Non-ISO steps 12+14+15: ISODateToFields → CalendarYearMonthFromFields → CreateTemporalYearMonth
    // (fused into plainYearMonthFromISODate + tryCreateIfValid).
    // For short form (YYYY-MM), parser stores day=1; otherwise day was extracted from the input.
    auto resolved = TemporalCore::plainYearMonthFromISODate(calendarId, plainDate);
    if (!resolved) [[unlikely]] {
        throwRangeError(globalObject, scope, String(resolved.error().message));
        return { };
    }
    auto* result = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(),
        WTF::move(resolved->isoDate));
    RETURN_IF_EXCEPTION(scope, { });
    if (result)
        result->setCalendarID(resolved->calendarId);
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
ISO8601::PlainDate TemporalPlainYearMonth::with(JSGlobalObject* globalObject, JSObject* temporalYearMonthLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot + type check done by caller.

    // Step 3: IsPartialTemporalObject.
    rejectObjectWithCalendarOrTimeZone(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: PrepareCalendarFields(calendar, temporalYearMonthLike, {year, month, monthCode}, {}, ~partial~).
    // skipCalendarRead=true: calendar known from m_calendarID; step 3 ensures no calendar property.
    CalendarID unusedCalId = m_calendarID;
    auto partialFields = readCalendarFieldsFromObject<FieldSetType::YearMonth, CalendarRead::Skip>(globalObject, temporalYearMonthLike, unusedCalId);
    RETURN_IF_EXCEPTION(scope, { });
    if (!partialFields.year && !partialFields.month && !partialFields.monthCode
        && !partialFields.era && !partialFields.eraYear) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    // Steps 8-9: GetOptionsObject + GetTemporalOverflowOption.
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5, 7, 10, 11: ISODateToFields + CalendarMergeFields + CalendarYearMonthFromFields
    // + CreateTemporalYearMonth — fused into plainYearMonthWith.
    auto result = TemporalCore::plainYearMonthWith(m_calendarID, m_plainYearMonth.isoPlainDate(), partialFields, overflow);
    if (!result) [[unlikely]] {
        if (result.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(result.error().message));
        else
            throwRangeError(globalObject, scope, String(result.error().message));
        return { };
    }
    return result->isoDate;
}

template<DifferenceOperation op>
ISO8601::Duration TemporalPlainYearMonth::sinceOrUntil(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Month, TemporalUnit::Year, op);
    RETURN_IF_EXCEPTION(scope, { });

    if (m_calendarID != other->m_calendarID) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between year-months with different calendars"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSC::differenceTemporalPlainYearMonth<op>(globalObject, plainYearMonth(), other->plainYearMonth(), increment, smallestUnit, largestUnit, roundingMode, m_calendarID));
}

ISO8601::Duration TemporalPlainYearMonth::until(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    return sinceOrUntil<DifferenceOperation::Until>(globalObject, other, optionsValue);
}

ISO8601::Duration TemporalPlainYearMonth::since(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    return sinceOrUntil<DifferenceOperation::Since>(globalObject, other, optionsValue);
}

} // namespace JSC
