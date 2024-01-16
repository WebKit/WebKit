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
#include "SuppressedErrorConstructor.h"

#include "SuppressedError.h"
#include "SuppressedErrorPrototype.h"
#include "ClassInfo.h"
#include "ExceptionScope.h"
#include "GCAssertions.h"
#include "JSCInlines.h"
#include "RuntimeType.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(SuppressedErrorConstructor);

const ClassInfo SuppressedErrorConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SuppressedErrorConstructor) };

static JSC_DECLARE_HOST_FUNCTION(callSuppressedErrorConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructSuppressedErrorConstructor);

SuppressedErrorConstructor::SuppressedErrorConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callSuppressedErrorConstructor, constructSuppressedErrorConstructor)
{
}

void SuppressedErrorConstructor::finishCreation(VM& vm, SuppressedErrorPrototype* prototype)
{
    Base::finishCreation(vm, 2, errorTypeName(ErrorType::SuppressedError), PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

JSC_DEFINE_HOST_FUNCTION(callSuppressedErrorConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    JSValue error = callFrame->argument(0);
    JSValue suppressed = callFrame->argument(1);
    JSValue message = callFrame->argument(2);
    JSValue options = callFrame->argument(3);
    Structure* errorStructure = globalObject->errorStructure(ErrorType::SuppressedError);
    return JSValue::encode(createSuppressedError(globalObject, vm, errorStructure, error, suppressed, message, options, nullptr, TypeNothing, false));
}

JSC_DEFINE_HOST_FUNCTION(constructSuppressedErrorConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue error = callFrame->argument(0);
    JSValue suppressed = callFrame->argument(1);
    JSValue message = callFrame->argument(2);
    JSValue options = callFrame->argument(3);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* errorStructure = JSC_GET_DERIVED_STRUCTURE(vm, errorStructureWithErrorType<ErrorType::SuppressedError>, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(errorStructure);

    RELEASE_AND_RETURN(scope, JSValue::encode(createSuppressedError(globalObject, vm, errorStructure, error, suppressed, message, options, nullptr, TypeNothing, false)));
}

} // namespace JSC
