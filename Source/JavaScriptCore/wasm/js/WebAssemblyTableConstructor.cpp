/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebAssemblyTableConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "FunctionPrototype.h"
#include "JSCInlines.h"
#include "WebAssemblyTablePrototype.h"

#include "WebAssemblyTableConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyTableConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyTable, CREATE_METHOD_TABLE(WebAssemblyTableConstructor) };

/* Source for WebAssemblyTableConstructor.lut.h
 @begin constructorTableWebAssemblyTable
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyTable(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=164135
    return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the Table constructor property"))));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyTable(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(state, scope, "WebAssembly.Table"));
}

WebAssemblyTableConstructor* WebAssemblyTableConstructor::create(VM& vm, Structure* structure, WebAssemblyTablePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyTableConstructor>(vm.heap)) WebAssemblyTableConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyTableConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyTableConstructor::finishCreation(VM& vm, WebAssemblyTablePrototype* prototype)
{
    Base::finishCreation(vm, ASCIILiteral("Table"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

WebAssemblyTableConstructor::WebAssemblyTableConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

ConstructType WebAssemblyTableConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructJSWebAssemblyTable;
    return ConstructType::Host;
}

CallType WebAssemblyTableConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callJSWebAssemblyTable;
    return CallType::Host;
}

void WebAssemblyTableConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<WebAssemblyTableConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

