/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
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
#include "AsyncGeneratorFunctionConstructor.h"

#include "AsyncGeneratorFunctionPrototype.h"
#include "FunctionConstructor.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(AsyncGeneratorFunctionConstructor);

const ClassInfo AsyncGeneratorFunctionConstructor::s_info = { "AsyncGeneratorFunction", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AsyncGeneratorFunctionConstructor) };

JSC_DECLARE_HOST_FUNCTION(callAsyncGeneratorFunctionConstructor);
JSC_DECLARE_HOST_FUNCTION(constructAsyncGeneratorFunctionConstructor);

JSC_DEFINE_HOST_FUNCTION(callAsyncGeneratorFunctionConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructFunction(globalObject, callFrame, args, FunctionConstructionMode::AsyncGenerator));
}

JSC_DEFINE_HOST_FUNCTION(constructAsyncGeneratorFunctionConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructFunction(globalObject, callFrame, args, FunctionConstructionMode::AsyncGenerator, callFrame->newTarget()));
}

AsyncGeneratorFunctionConstructor::AsyncGeneratorFunctionConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callAsyncGeneratorFunctionConstructor, constructAsyncGeneratorFunctionConstructor)
{
}

void AsyncGeneratorFunctionConstructor::finishCreation(VM& vm, AsyncGeneratorFunctionPrototype* prototype)
{
    Base::finishCreation(vm, 1, "AsyncGeneratorFunction"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

} // namespace JSC
