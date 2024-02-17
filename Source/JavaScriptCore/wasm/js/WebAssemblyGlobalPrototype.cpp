/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"
#include "JSWebAssemblyGlobal.h"

namespace JSC {
static JSC_DECLARE_HOST_FUNCTION(webAssemblyGlobalProtoFuncValueOf);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyGlobalProtoGetterFuncValue);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyGlobalProtoSetterFuncValue);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyGlobalProtoFuncType);
}

#include "WebAssemblyGlobalPrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyGlobalPrototype::s_info = { "WebAssembly.Global"_s, &Base::s_info, &prototypeGlobalWebAssemblyGlobal, nullptr, CREATE_METHOD_TABLE(WebAssemblyGlobalPrototype) };

/* Source for WebAssemblyGlobalPrototype.lut.h
 @begin prototypeGlobalWebAssemblyGlobal
 valueOf webAssemblyGlobalProtoFuncValueOf Function 0
 type    webAssemblyGlobalProtoFuncType    Function 0

 @end
 */

static ALWAYS_INLINE JSWebAssemblyGlobal* getGlobal(JSGlobalObject* globalObject, VM& vm, JSValue v)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyGlobal* result = jsDynamicCast<JSWebAssemblyGlobal*>(v);
    if (!result) {
        throwException(globalObject, throwScope,
            createTypeError(globalObject, "expected |this| value to be an instance of WebAssembly.Global"_s));
        return nullptr;
    }
    Integrity::auditStructureID(result->structureID());
    return result;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyGlobalProtoFuncValueOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    RELEASE_AND_RETURN(throwScope, JSValue::encode(global->global()->get(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyGlobalProtoFuncType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    JSObject* typeDescriptor = global->type(globalObject);
    if (!typeDescriptor)
        return throwVMTypeError(globalObject, throwScope, "WebAssembly.Global.prototype.type unable to produce type descriptor for the given global"_s);

    RELEASE_AND_RETURN(throwScope, JSValue::encode(typeDescriptor));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyGlobalProtoGetterFuncValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    RELEASE_AND_RETURN(throwScope, JSValue::encode(global->global()->get(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyGlobalProtoSetterFuncValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame->argumentCount() < 1))
        return JSValue::encode(throwException(globalObject, throwScope, createNotEnoughArgumentsError(globalObject)));

    JSWebAssemblyGlobal* global = getGlobal(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    if (global->global()->mutability() == Wasm::Mutability::Immutable)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global.prototype.value attempts to modify immutable global value"_s)));

    global->global()->set(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    return JSValue::encode(jsUndefined());
}

WebAssemblyGlobalPrototype* WebAssemblyGlobalPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyGlobalPrototype>(vm)) WebAssemblyGlobalPrototype(vm, structure);
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
    ASSERT(inherits(info()));

    JSFunction* valueGetterFunction = JSFunction::create(vm, globalObject, 0, "get value"_s, webAssemblyGlobalProtoGetterFuncValue, ImplementationVisibility::Public);
    JSFunction* valueSetterFunction = JSFunction::create(vm, globalObject, 1, "set value"_s, webAssemblyGlobalProtoSetterFuncValue, ImplementationVisibility::Public);
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
