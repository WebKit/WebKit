/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TemporalCalendarPrototype.h"

#include "IntlObjectInlines.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalCalendar.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncDateFromFields);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncDateAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncDateUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncFields);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncMergeFields);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarPrototypeFuncToJSON);
static JSC_DECLARE_CUSTOM_GETTER(temporalCalendarPrototypeGetterId);

}

#include "TemporalCalendarPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalCalendarPrototype::s_info = { "Temporal.Calendar"_s, &Base::s_info, &temporalCalendarPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalCalendarPrototype) };

/* Source for TemporalCalendarPrototype.lut.h
@begin temporalCalendarPrototypeTable
    dateFromFields      temporalCalendarPrototypeFuncDateFromFields      DontEnum|Function 1
    dateAdd             temporalCalendarPrototypeFuncDateAdd             DontEnum|Function 2
    dateUntil           temporalCalendarPrototypeFuncDateUntil           DontEnum|Function 2
    fields              temporalCalendarPrototypeFuncFields              DontEnum|Function 1
    mergeFields         temporalCalendarPrototypeFuncMergeFields         DontEnum|Function 2
    toString            temporalCalendarPrototypeFuncToString            DontEnum|Function 0
    toJSON              temporalCalendarPrototypeFuncToJSON              DontEnum|Function 0
    id                  temporalCalendarPrototypeGetterId                ReadOnly|DontEnum|CustomAccessor
@end
*/

TemporalCalendarPrototype* TemporalCalendarPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    TemporalCalendarPrototype* object = new (NotNull, allocateCell<TemporalCalendarPrototype>(vm)) TemporalCalendarPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* TemporalCalendarPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalCalendarPrototype::TemporalCalendarPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalCalendarPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.calendar.prototype.id
JSC_DEFINE_CUSTOM_GETTER(temporalCalendarPrototypeGetterId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    return JSValue::encode(JSValue::decode(thisValue).toString(globalObject));
}

// https://tc39.es/proposal-temporal/#sup-temporal.calendar.prototype.datefromfields
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncDateFromFields, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.dateFromFields called on value that's not a Calendar"_s);

    // FIXME: Implement after fleshing out the rest of Temporal.Calendar.
    if (!calendar->isISO8601())
        return throwVMRangeError(globalObject, scope, "unimplemented: non-ISO8601 calendar"_s);

    JSValue value = callFrame->argument(0);
    if (!value.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.Calendar.prototype.dateFromFields must be an object"_s);

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainDate plainDate = calendar->isoDateFromFields(globalObject, asObject(value), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(plainDate))));
}

// https://tc39.es/proposal-temporal/#sup-temporal.calendar.prototype.dateadd
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncDateAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.dateAdd called on value that's not a Calendar"_s);

    // FIXME: Implement after fleshing out the rest of Temporal.Calendar.
    if (!calendar->isISO8601())
        return throwVMRangeError(globalObject, scope, "unimplemented: non-ISO8601 calendar"_s);

    auto* date = TemporalPlainDate::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(2));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainDate plainDate = calendar->isoDateAdd(globalObject, date->plainDate(), duration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(plainDate))));
}

// https://tc39.es/proposal-temporal/#sup-temporal.calendar.prototype.dateuntil
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncDateUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.dateUntil called on value that's not a Calendar"_s);

    // FIXME: Implement after fleshing out the rest of Temporal.Calendar.
    if (!calendar->isISO8601())
        return throwVMRangeError(globalObject, scope, "unimplemented: non-ISO8601 calendar"_s);

    auto* date1 = TemporalPlainDate::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto* date2 = TemporalPlainDate::from(globalObject, callFrame->argument(1), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(2));
    RETURN_IF_EXCEPTION(scope, { });

    auto largest = temporalLargestUnit(globalObject, options, { TemporalUnit::Hour, TemporalUnit::Minute, TemporalUnit::Second, TemporalUnit::Millisecond, TemporalUnit::Microsecond, TemporalUnit::Nanosecond }, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalUnit largestUnit = largest.value_or(TemporalUnit::Day);

    auto result = TemporalCalendar::isoDateDifference(globalObject, date1->plainDate(), date2->plainDate(), largestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sup-temporal.calendar.prototype.fields
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncFields, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.fields called on value that's not a Calendar"_s);

    bool isISO8601 = calendar->isISO8601();
    bool shouldAddEraAndEraYear = false;
    MarkedArgumentBuffer fieldNames;
    forEachInIterable(globalObject, callFrame->argument(0), [isISO8601, &shouldAddEraAndEraYear, &fieldNames](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (!value.isString()) {
            throwTypeError(globalObject, scope, "fields include non string value"_s);
            return;
        }
        if (!isISO8601 && !shouldAddEraAndEraYear) {
            auto string = jsCast<JSString*>(value)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
            if (string == "year"_s)
                shouldAddEraAndEraYear = true;
        }
        fieldNames.append(value);
        if (UNLIKELY(fieldNames.hasOverflowed()))
            throwStackOverflowError(globalObject, scope);
    });
    RETURN_IF_EXCEPTION(scope, { });

    if (shouldAddEraAndEraYear) {
        fieldNames.append(jsNontrivialString(vm, vm.propertyNames->era.impl()));
        fieldNames.append(jsNontrivialString(vm, vm.propertyNames->eraYear.impl()));
        if (UNLIKELY(fieldNames.hasOverflowed()))
            throwStackOverflowError(globalObject, scope);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), fieldNames)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-defaultmergefields
static JSObject* defaultMergeFields(JSGlobalObject* globalObject, JSObject* fields, JSObject* additionalFields)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* merged = constructEmptyObject(globalObject);

    {
        PropertyNameArray originalKeys(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        fields->methodTable()->getOwnPropertyNames(fields, globalObject, originalKeys, DontEnumPropertiesMode::Include);
        RETURN_IF_EXCEPTION(scope, { });

        for (const auto& nextKey : originalKeys) {
            if (nextKey != vm.propertyNames->month && nextKey != vm.propertyNames->monthCode) {
                JSValue propValue = fields->get(globalObject, nextKey);
                RETURN_IF_EXCEPTION(scope, { });
                if (!propValue.isUndefined()) {
                    PutPropertySlot slot(merged, true);
                    merged->putOwnDataProperty(vm, nextKey, propValue, slot);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
        }
    }

    bool includesMonthOrMonthCode = false;
    {
        PropertyNameArray newKeys(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        additionalFields->methodTable()->getOwnPropertyNames(additionalFields, globalObject, newKeys, DontEnumPropertiesMode::Include);
        RETURN_IF_EXCEPTION(scope, { });

        for (const auto& nextKey : newKeys) {
            if (!includesMonthOrMonthCode) {
                if (nextKey == vm.propertyNames->month || nextKey == vm.propertyNames->monthCode)
                    includesMonthOrMonthCode = true;
            }

            JSValue propValue = additionalFields->get(globalObject, nextKey);
            RETURN_IF_EXCEPTION(scope, { });
            if (!propValue.isUndefined()) {
                PutPropertySlot slot(merged, true);
                merged->putOwnDataProperty(vm, nextKey, propValue, slot);
                RETURN_IF_EXCEPTION(scope, { });
            }
        }
    }

    if (!includesMonthOrMonthCode) {
        JSValue month = fields->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, { });
        if (!month.isUndefined()) {
            PutPropertySlot slot(merged, true);
            merged->putOwnDataProperty(vm, vm.propertyNames->month, month, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }

        JSValue monthCode = fields->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, { });
        if (!monthCode.isUndefined()) {
            PutPropertySlot slot(merged, true);
            merged->putOwnDataProperty(vm, vm.propertyNames->monthCode, monthCode, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    return merged;
}

// https://tc39.es/proposal-temporal/#sup-temporal.calendar.prototype.mergefields
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncMergeFields, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.mergeFields called on value that's not a Calendar"_s);

    auto* fields = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto* additionalFields = callFrame->argument(1).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (calendar->isISO8601())
        RELEASE_AND_RETURN(scope, JSValue::encode(defaultMergeFields(globalObject, fields, additionalFields)));

    auto copyObject = [](JSGlobalObject* globalObject, JSObject* object) -> JSObject* {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto* copied = constructEmptyObject(globalObject);

        PropertyNameArray keys(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        object->methodTable()->getOwnPropertyNames(object, globalObject, keys, DontEnumPropertiesMode::Include);
        RETURN_IF_EXCEPTION(scope, { });

        for (const auto& key : keys) {
            JSValue propValue = object->get(globalObject, key);
            RETURN_IF_EXCEPTION(scope, { });
            if (!propValue.isUndefined()) {
                PutPropertySlot slot(copied, true);
                copied->putOwnDataProperty(vm, key, propValue, slot);
                RETURN_IF_EXCEPTION(scope, { });
            }
        }

        return copied;
    };

    auto* fieldsCopied = copyObject(globalObject, fields);
    RETURN_IF_EXCEPTION(scope, { });

    auto* additionalFieldsCopied = copyObject(globalObject, additionalFields);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue newMonth = jsUndefined();
    JSValue newMonthCode = jsUndefined();
    JSValue newYear = jsUndefined();
    JSValue newEra = jsUndefined();
    JSValue newEraYear = jsUndefined();
    {
        PropertyNameArray keys(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        additionalFieldsCopied->methodTable()->getOwnPropertyNames(additionalFieldsCopied, globalObject, keys, DontEnumPropertiesMode::Include);
        RETURN_IF_EXCEPTION(scope, { });

        for (const auto& key : keys) {
            JSValue propValue = additionalFieldsCopied->get(globalObject, key);
            RETURN_IF_EXCEPTION(scope, { });
            if (!propValue.isUndefined()) {
                if (key == vm.propertyNames->month)
                    newMonth = propValue;
                else if (key == vm.propertyNames->monthCode)
                    newMonthCode = propValue;
                else if (key == vm.propertyNames->year)
                    newYear = propValue;
                else if (key == vm.propertyNames->era)
                    newEra = propValue;
                else if (key == vm.propertyNames->eraYear)
                    newEraYear = propValue;
                else {
                    PutPropertySlot slot(fieldsCopied, true);
                    fieldsCopied->putOwnDataProperty(vm, key, propValue, slot);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
        }
    }

    if (!newMonth.isUndefined() || !newMonthCode.isUndefined()) {
        {
            PutPropertySlot slot(fieldsCopied, true);
            fieldsCopied->putOwnDataProperty(vm, vm.propertyNames->month, newMonth, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
        {
            PutPropertySlot slot(fieldsCopied, true);
            fieldsCopied->putOwnDataProperty(vm, vm.propertyNames->monthCode, newMonthCode, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    if (!newYear.isUndefined() || !newEra.isUndefined() || !newEraYear.isUndefined()) {
        {
            PutPropertySlot slot(fieldsCopied, true);
            fieldsCopied->putOwnDataProperty(vm, vm.propertyNames->year, newYear, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
        {
            PutPropertySlot slot(fieldsCopied, true);
            fieldsCopied->putOwnDataProperty(vm, vm.propertyNames->era, newEra, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
        {
            PutPropertySlot slot(fieldsCopied, true);
            fieldsCopied->putOwnDataProperty(vm, vm.propertyNames->eraYear, newEraYear, slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    return JSValue::encode(fieldsCopied);
}

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.toString called on value that's not a Calendar"_s);

    return JSValue::encode(jsString(vm, intlAvailableCalendars()[calendar->identifier()]));
}

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalCalendarPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* calendar = jsDynamicCast<TemporalCalendar*>(callFrame->thisValue());
    if (!calendar)
        return throwVMTypeError(globalObject, scope, "Temporal.Calendar.prototype.toJSON called on value that's not a Calendar"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(calendar->toString(globalObject)));
}

} // namespace JSC
