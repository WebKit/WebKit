/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "AggregateErrorConstructor.h"

#include "AggregateError.h"
#include "ClassInfo.h"
#include "ExceptionScope.h"
#include "GCAssertions.h"
#include "JSCInlines.h"
#include "RuntimeType.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(AggregateErrorConstructor);

const ClassInfo AggregateErrorConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AggregateErrorConstructor) };

static JSC_DECLARE_HOST_FUNCTION(callAggregateErrorConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructAggregateErrorConstructor);

AggregateErrorConstructor::AggregateErrorConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callAggregateErrorConstructor, constructAggregateErrorConstructor)
{
}

void AggregateErrorConstructor::finishCreation(VM& vm, AggregateErrorPrototype* prototype)
{
    Base::finishCreation(vm, 2, errorTypeName(ErrorType::AggregateError), PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

JSC_DEFINE_HOST_FUNCTION(callAggregateErrorConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    JSValue errors = callFrame->argument(0);
    JSValue message = callFrame->argument(1);
    Structure* errorStructure = globalObject->errorStructure(ErrorType::AggregateError);
    return JSValue::encode(AggregateError::create(globalObject, vm, errorStructure, errors, message, nullptr, TypeNothing, false));
}

JSC_DEFINE_HOST_FUNCTION(constructAggregateErrorConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue errors = callFrame->argument(0);
    JSValue message = callFrame->argument(1);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* errorStructure = newTarget == callFrame->jsCallee()
        ? globalObject->errorStructure(ErrorType::AggregateError)
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->errorStructure(ErrorType::AggregateError));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(errorStructure);

    RELEASE_AND_RETURN(scope, JSValue::encode(AggregateError::create(globalObject, vm, errorStructure, errors, message, nullptr, TypeNothing, false)));
}

} // namespace JSC
