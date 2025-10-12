/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "TemporalPlainMonthDay.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainMonthDay::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDay) };

TemporalPlainMonthDay* TemporalPlainMonthDay::create(VM& vm, Structure* structure, ISO8601::PlainMonthDay&& plainMonthDay)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainMonthDay>(vm)) TemporalPlainMonthDay(vm, structure, WTFMove(plainMonthDay));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainMonthDay::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainMonthDay::TemporalPlainMonthDay(VM& vm, Structure* structure, ISO8601::PlainMonthDay&& plainMonthDay)
    : Base(vm, structure)
    , m_plainMonthDay(WTFMove(plainMonthDay))
{
}

void TemporalPlainMonthDay::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainMonthDay = jsCast<TemporalPlainMonthDay*>(init.owner);
            auto* globalObject = plainMonthDay->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainMonthDay::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainMonthDay*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainMonthDay);

// CreateTemporalMonthDay ( isoDate, calendar [, newTarget ]
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isValidISODate(plainDate.year(), plainDate.month(), plainDate.day())) {
        throwRangeError(globalObject, scope, "PlainMonthDay: invalid date"_s);
        return { };
    }

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "PlainMonthDay: date out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainMonthDay::create(vm, structure, ISO8601::PlainMonthDay(WTFMove(plainDate)));
}

String TemporalPlainMonthDay::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    String calendarName = toTemporalCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalMonthDayToString(m_plainMonthDay, calendarName);
}

String TemporalPlainMonthDay::monthCode() const
{
    return ISO8601::monthCode(m_plainMonthDay.month());
}

} // namespace JSC
