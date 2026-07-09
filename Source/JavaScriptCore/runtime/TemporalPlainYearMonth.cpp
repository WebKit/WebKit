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


// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
TemporalPlainYearMonth* TemporalPlainYearMonth::from(JSGlobalObject* globalObject, JSValue item, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If options is not present, set options to undefined. (Caller passes jsUndefined().)

    // Steps 2-3 reordered: String check first, Object check second. A value can't be both a String and
    // an Object, so the reorder is unobservable.
    auto string = item.getString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!string.isNull()) {
        // Step 4: result = ? ParseISODateTime(item, « TemporalYearMonthString »).
        auto dateTime = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::YearMonth);
        if (!dateTime) [[unlikely]] {
            String message = tryMakeString("Temporal.PlainYearMonth.from: invalid year-month string "_s, string);
            throwRangeError(globalObject, scope, message.isNull() ? "Temporal.PlainYearMonth.from: invalid year-month string"_s : message);
            return { };
        }
        auto [plainDateOpt, plainTimeOptional, timeZoneOptional, calendarOptional, matched, isShortForm] = WTF::move(*dateTime);
        ASSERT(plainDateOpt);
        auto plainDate = WTF::move(*plainDateOpt);

        // Steps 5-7: calendar = result.[[Calendar]] (default "iso8601"); calendar = ? CanonicalizeCalendar(calendar).
        //            parseISODateTime already enforced Step 4.a.ii.(3) — short-form non-iso8601 → nullopt.
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

        // Step 16: resolvedOptions = ? GetOptionsObject(options). Step 17: ? GetTemporalOverflowOption
        //          (validate; overflow unused for strings — a String provides an unambiguous ISO date).
        //          Spec runs these AFTER parsing; user's options object is validated but its value is unused.
        if (!optionsValue.isUndefined()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 10: isoDate = CreateISODateRecord(year, month, day) — short form gives day=1 from the parser.
        // Step 11: If ISOYearMonthWithinLimits(isoDate) is false, throw. (Enforced inside tryCreateIfValid.)
        // Steps 12+14+15: fields = ISODateToFields; isoDate = ? CalendarYearMonthFromFields; ! CreateTemporalYearMonth.
        //                 Fused into plainYearMonthFromISODate + tryCreateIfValid for non-ISO; direct for ISO.
        if (calendarId == iso8601CalendarID())
            RELEASE_AND_RETURN(scope, TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), ISO8601::PlainDate(plainDate.year(), plainDate.month(), 1)));

        auto resolved = TemporalCore::plainYearMonthFromISODate(calendarId, plainDate);
        if (!resolved) [[unlikely]] {
            throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto* result = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(resolved->isoDate));
        RETURN_IF_EXCEPTION(scope, { });
        if (result)
            result->setCalendarID(resolved->calendarId);
        RELEASE_AND_RETURN(scope, result);
    }

    // Step 2: If item is an Object, then …
    if (item.isObject()) {
        if (item.inherits<TemporalPlainYearMonth>()) {
            // Step 2.a.i: Let resolvedOptions be ? GetOptionsObject(options).
            auto* resolvedOptions = intlGetOptionsObject(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            // Step 2.a.ii: Perform ? GetTemporalOverflowOption(resolvedOptions). (Validate; result discarded.)
            if (resolvedOptions) {
                toTemporalOverflow(globalObject, resolvedOptions);
                RETURN_IF_EXCEPTION(scope, { });
            }
            // Step 2.a.iii: Return ! CreateTemporalYearMonth(item.[[ISODate]], item.[[Calendar]]).
            auto* src = uncheckedDowncast<TemporalPlainYearMonth>(item);
            auto* clone = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), src->plainYearMonth());
            clone->setCalendarID(src->calendarID());
            return clone;
        }

        // Step 2.b: Let calendar be ? GetTemporalCalendarIdentifierWithISODefault(item).
        // Step 2.c: Let fields be ? PrepareCalendarFields(calendar, item, «year, month, monthCode», «», «»).
        //          (Steps 2.b-c fused into readCalendarFieldsFromObject.)
        CalendarID calendarId = iso8601CalendarID();
        auto fields = readCalendarFieldsFromObject<FieldSetType::YearMonth>(globalObject, asObject(item), calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 2.d: Let resolvedOptions be ? GetOptionsObject(options).
        // Step 2.e: Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        //          Options read AFTER fields per spec (PrepareCalendarFields precedes GetOptionsObject).
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

        // Step 2.f: Let isoDate be ? CalendarYearMonthFromFields(calendar, fields, overflow).
        auto resolved = TemporalCore::yearMonthFromFields(calendarId, fields, overflow);
        if (!resolved) [[unlikely]] {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }

        // Step 2.g: Return ! CreateTemporalYearMonth(isoDate, calendar).
        auto* result = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(resolved->isoDate)));
        result->setCalendarID(resolved->calendarId);
        return result;
    }

    // Step 3: If item is not a String, throw a TypeError exception.
    throwTypeError(globalObject, scope, "can only convert to PlainYearMonth from object or string values"_s);
    return { };
}

} // namespace JSC
