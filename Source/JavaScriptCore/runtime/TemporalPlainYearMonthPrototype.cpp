/*
 * Copyright (C) 2022 Apple Inc.
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
#include "TemporalPlainYearMonthPrototype.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"

namespace JSC {

static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear);

}

#include "TemporalPlainYearMonthPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonthPrototype::s_info = { "Temporal.PlainYearMonth"_s, &Base::s_info, &plainYearMonthPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonthPrototype) };

/* Source for TemporalPlainYearMonthPrototype.lut.h
@begin plainYearMonthPrototypeTable
  calendarId       temporalPlainYearMonthPrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainYearMonthPrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainYearMonthPrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainYearMonthPrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainYearMonthPrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainYearMonthPrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainYearMonthPrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainYearMonthPrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainYearMonthPrototype* TemporalPlainYearMonthPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainYearMonthPrototype>(vm)) TemporalPlainYearMonthPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainYearMonthPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonthPrototype::TemporalPlainYearMonthPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainYearMonthPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.calendar called on value that's not a PlainYearMonth"_s);

    // FIXME: when calendars are supported, get the string ID of the calendar
    return JSValue::encode(jsString(vm, String::fromLatin1("iso8601")));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.year called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(yearMonth->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.month called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(yearMonth->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthCode called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNontrivialString(vm, yearMonth->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInMonth called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(ISO8601::daysInMonth(yearMonth->year(), yearMonth->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInYear called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(isLeapYear(yearMonth->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthsInYear called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.inLeapYear called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsBoolean(isLeapYear(yearMonth->year())));
}

} // namespace JSC
