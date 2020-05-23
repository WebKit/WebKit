/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "WebAssemblyGlobalPrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "GetterSetter.h"
#include "IntegrityInlines.h"
#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyGlobal.h"
#include "StructureInlines.h"

namespace JSC {
static EncodedJSValue JSC_HOST_CALL webAssemblyGlobalProtoFuncValueOf(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL webAssemblyGlobalProtoGetterFuncValue(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL webAssemblyGlobalProtoSetterFuncValue(JSGlobalObject*, CallFrame*);
}

#include "WebAssemblyGlobalPrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyGlobalPrototype::s_info = { "WebAssembly.Global", &Base::s_info, &prototypeGlobalWebAssemblyGlobal, nullptr, CREATE_METHOD_TABLE(WebAssemblyGlobalPrototype) };

/* Source for WebAssemblyGlobalPrototype.lut.h
 @begin prototypeGlobalWebAssemblyGlobal
 valueOf webAssemblyGlobalProtoFuncValueOf Function 0
 @end
 */

static ALWAYS_INLINE JSWebAssemblyGlobal* getGlobal(JSGlobalObject* globalObject, VM& vm, JSValue v)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyGlobal* result = jsDynamicCast<JSWebAssemblyGlobal*>(vm, v);
    if (!result) {
        throwException(globalObject, throwScope,
            createTypeError(globalObject, "expected |this| value to be an instance of WebAssembly.Global"_s));
        return nullptr;
    }
    Integrity::auditStructureID(vm, result->structureID());
    return result;
}

static EncodedJSValue JSC_HOST_CALL webAssemblyGlobalProtoFuncValueOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    switch (global->global()->type()) {
    case Wasm::Type::I64:
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global.prototype.valueOf does not work with i64 type"_s)));
    default:
        return JSValue::encode(global->global()->get());
    }
}

static EncodedJSValue JSC_HOST_CALL webAssemblyGlobalProtoGetterFuncValue(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    switch (global->global()->type()) {
    case Wasm::Type::I64:
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global.prototype.value does not work with i64 type"_s)));
    default:
        return JSValue::encode(global->global()->get());
    }
}

static EncodedJSValue JSC_HOST_CALL webAssemblyGlobalProtoSetterFuncValue(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame->argumentCount() < 1))
        return JSValue::encode(throwException(globalObject, throwScope, createNotEnoughArgumentsError(globalObject)));

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    if (global->global()->mutability() == Wasm::GlobalInformation::Mutability::Immutable)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global.prototype.value attempts to modify immutable global value"_s)));

    switch (global->global()->type()) {
    case Wasm::Type::I64:
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global.prototype.value does not work with i64 type"_s)));
    default:
        global->global()->set(globalObject, callFrame->argument(0));
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        return JSValue::encode(jsUndefined());
    }
}

WebAssemblyGlobalPrototype* WebAssemblyGlobalPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyGlobalPrototype>(vm.heap)) WebAssemblyGlobalPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* WebAssemblyGlobalPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyGlobalPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    JSFunction* valueGetterFunction = JSFunction::create(vm, globalObject, 0, "get value"_s, webAssemblyGlobalProtoGetterFuncValue, NoIntrinsic);
    JSFunction* valueSetterFunction = JSFunction::create(vm, globalObject, 1, "set value"_s, webAssemblyGlobalProtoSetterFuncValue, NoIntrinsic);
    GetterSetter* valueAccessor = GetterSetter::create(vm, globalObject, valueGetterFunction, valueSetterFunction);
    putDirectNonIndexAccessorWithoutTransition(vm, Identifier::fromString(vm, "value"_s), valueAccessor, static_cast<unsigned>(PropertyAttribute::Accessor));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyGlobalPrototype::WebAssemblyGlobalPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
