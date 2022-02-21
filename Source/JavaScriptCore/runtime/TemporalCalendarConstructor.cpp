/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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
#include "TemporalCalendarConstructor.h"

#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalCalendarPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalCalendarConstructor);
static JSC_DECLARE_HOST_FUNCTION(temporalCalendarConstructorFuncFrom);

}

#include "TemporalCalendarConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalCalendarConstructor::s_info = { "Function", &InternalFunction::s_info, &temporalCalendarConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalCalendarConstructor) };

/* Source for TemporalCalendarConstructor.lut.h
@begin temporalCalendarConstructorTable
    from             temporalCalendarConstructorFuncFrom             DontEnum|Function 1
@end
*/

TemporalCalendarConstructor* TemporalCalendarConstructor::create(VM& vm, Structure* structure, TemporalCalendarPrototype* temporalCalendarPrototype)
{
    TemporalCalendarConstructor* constructor = new (NotNull, allocateCell<TemporalCalendarConstructor>(vm)) TemporalCalendarConstructor(vm, structure);
    constructor->finishCreation(vm, temporalCalendarPrototype);
    return constructor;
}

Structure* TemporalCalendarConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalCalendar);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalCalendar);

TemporalCalendarConstructor::TemporalCalendarConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callTemporalCalendar, constructTemporalCalendar)
{
}

void TemporalCalendarConstructor::finishCreation(VM& vm, TemporalCalendarPrototype* temporalCalendarPrototype)
{
    Base::finishCreation(vm, 0, "Calendar"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, temporalCalendarPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    temporalCalendarPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(constructTemporalCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, calendarStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    auto calendarString = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<CalendarID> identifier = TemporalCalendar::isBuiltinCalendar(calendarString);
    if (!identifier) {
        throwRangeError(globalObject, scope, "invalid calendar ID"_s);
        return { };
    }

    return JSValue::encode(TemporalCalendar::create(vm, structure, identifier.value()));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalCalendar, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Calendar"));
}

JSC_DEFINE_HOST_FUNCTION(temporalCalendarConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalCalendar::from(globalObject, callFrame->argument(0)));
}

} // namespace JSC
