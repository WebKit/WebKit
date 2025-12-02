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

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonth::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonth) };

TemporalPlainYearMonth* TemporalPlainYearMonth::create(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainYearMonth>(vm)) TemporalPlainYearMonth(vm, structure, WTFMove(plainYearMonth));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainYearMonth::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonth::TemporalPlainYearMonth(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
    : Base(vm, structure)
    , m_plainYearMonth(WTFMove(plainYearMonth))
{
}

void TemporalPlainYearMonth::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainYearMonth = jsCast<TemporalPlainYearMonth*>(init.owner);
            auto* globalObject = plainYearMonth->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainYearMonth::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainYearMonth*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainYearMonth);

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

    return TemporalPlainYearMonth::create(vm, structure, ISO8601::PlainYearMonth(WTFMove(plainDate)));
}

String TemporalPlainYearMonth::monthCode() const
{
    return ISO8601::monthCode(m_plainYearMonth.month());
}

} // namespace JSC
