/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSWebAssemblyPromisingFunction.h"

#if ENABLE(WEBASSEMBLY)

#include "ArgList.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "PinballCompletion.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(runWebAssemblyPromisingFunction);

JSC_DEFINE_HOST_FUNCTION(runWebAssemblyPromisingFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* callee = callFrame->jsCallee();
    JSWebAssemblyPromisingFunction* thisFunction = jsCast<JSWebAssemblyPromisingFunction*>(callee);
    ASSERT(thisFunction);
    JSFunction* wrappedFunction = thisFunction->wrappedFunction();

    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    if (args.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto callData = JSC::getCallData(wrappedFunction);
    if (callData.type == CallData::Type::None) {
        throwTypeError(globalObject, scope, "Object is not callable"_s);
        return { };
    }

    JSPIContext context(JSPIContext::Purpose::Promising, vm, callFrame);
    JSValue result = call(globalObject, wrappedFunction, callData, jsUndefined(), args);
    context.deactivate(vm);

    JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
    if (scope.exception()) [[unlikely]] {
        // exception was thrown in wasm code
        JSValue exceptionValue = scope.exception()->value();
        scope.clearException();
        resultPromise->reject(vm, globalObject, exceptionValue);
    } else if (context.completion) {
        // wasm called out to js and was suspended
        context.completion->setResultPromise(vm, resultPromise);
    } else {
        // the call returned normally, result is meaningful
        resultPromise->resolve(globalObject, result);
    }
    scope.release();
    return JSValue::encode(resultPromise);
}

const ClassInfo JSWebAssemblyPromisingFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyPromisingFunction) };

JSWebAssemblyPromisingFunction* JSWebAssemblyPromisingFunction::create(VM& vm, JSGlobalObject*globalObject, JSFunction* wrappedWasmFunction)
{
    const String name = "<promising wrapper>"_s;
    NativeExecutable* executable = vm.getHostFunction(runWebAssemblyPromisingFunction, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    Structure* structure = createStructure(vm, globalObject);
    JSWebAssemblyPromisingFunction* function = new (NotNull, allocateCell<JSWebAssemblyPromisingFunction>(vm)) JSWebAssemblyPromisingFunction(vm, executable, globalObject, structure, wrappedWasmFunction);
    function->finishCreation(vm, executable, name);
    return function;
}

Structure* JSWebAssemblyPromisingFunction::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    JSValue prototype = globalObject->functionPrototype();
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

void JSWebAssemblyPromisingFunction::finishCreation(VM& vm, NativeExecutable* executable, const String& name)
{
    Base::finishCreation(vm, executable, 1, name);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void JSWebAssemblyPromisingFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyPromisingFunction* thisObject = jsCast<JSWebAssemblyPromisingFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_wrappedFunction);
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyPromisingFunction);

} // namespace JSC

#endif
