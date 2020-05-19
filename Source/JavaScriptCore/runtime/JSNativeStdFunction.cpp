/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "JSNativeStdFunction.h"

#include "JSCJSValueInlines.h"
#include "VM.h"

namespace JSC {

const ClassInfo JSNativeStdFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSNativeStdFunction) };

JSNativeStdFunction::JSNativeStdFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, NativeStdFunction&& function)
    : Base(vm, executable, globalObject, structure)
    , m_function(WTFMove(function))
{
}

void JSNativeStdFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSNativeStdFunction* thisObject = jsCast<JSNativeStdFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

void JSNativeStdFunction::finishCreation(VM& vm, NativeExecutable* executable, int length, const String& name)
{
    Base::finishCreation(vm, executable, length, name);
    ASSERT(inherits(vm, info()));
}

static EncodedJSValue JSC_HOST_CALL runStdFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSNativeStdFunction* function = jsCast<JSNativeStdFunction*>(callFrame->jsCallee());
    ASSERT(function);
    return function->function()(globalObject, callFrame);
}

JSNativeStdFunction* JSNativeStdFunction::create(VM& vm, JSGlobalObject* globalObject, int length, const String& name, NativeStdFunction&& nativeStdFunction, Intrinsic intrinsic, NativeFunction nativeConstructor)
{
    NativeExecutable* executable = vm.getHostFunction(runStdFunction, intrinsic, nativeConstructor, nullptr, name);
    Structure* structure = globalObject->nativeStdFunctionStructure();
    JSNativeStdFunction* function = new (NotNull, allocateCell<JSNativeStdFunction>(vm.heap)) JSNativeStdFunction(vm, executable, globalObject, structure, WTFMove(nativeStdFunction));
    function->finishCreation(vm, executable, length, name);
    return function;
}

} // namespace JSC
