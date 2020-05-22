/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSInternalPromiseConstructor.h"

#include "JSCBuiltins.h"
#include "JSInternalPromisePrototype.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"

#include "JSInternalPromiseConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSInternalPromiseConstructor);

const ClassInfo JSInternalPromiseConstructor::s_info = { "Function", &Base::s_info, &internalPromiseConstructorTable, nullptr, CREATE_METHOD_TABLE(JSInternalPromiseConstructor) };

/* Source for JSInternalPromiseConstructor.lut.h
@begin internalPromiseConstructorTable
  internalAll  JSBuiltin DontEnum|Function 1
@end
*/

JSInternalPromiseConstructor* JSInternalPromiseConstructor::create(VM& vm, Structure* structure, JSInternalPromisePrototype* promisePrototype, GetterSetter* speciesSymbol)
{
    JSGlobalObject* globalObject = structure->globalObject();
    FunctionExecutable* executable = promiseConstructorInternalPromiseConstructorCodeGenerator(vm);
    JSInternalPromiseConstructor* constructor = new (NotNull, allocateCell<JSInternalPromiseConstructor>(vm.heap)) JSInternalPromiseConstructor(vm, executable, globalObject, structure);
    constructor->finishCreation(vm, promisePrototype, speciesSymbol);
    return constructor;
}

Structure* JSInternalPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

JSInternalPromiseConstructor::JSInternalPromiseConstructor(VM& vm, FunctionExecutable* executable, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, executable, globalObject, structure)
{
}

} // namespace JSC
