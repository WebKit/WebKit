/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WebAssemblyMemoryPrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "JSArrayBuffer.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyMemory.h"
#include "StructureInlines.h"

namespace JSC {
static JSC_DECLARE_HOST_FUNCTION(webAssemblyMemoryProtoFuncGrow);
static JSC_DECLARE_CUSTOM_GETTER(webAssemblyMemoryProtoGetterBuffer);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyMemoryProtoFuncType);
}

#include "WebAssemblyMemoryPrototype.lut.h"


namespace JSC {

const ClassInfo WebAssemblyMemoryPrototype::s_info = { "WebAssembly.Memory", &Base::s_info, &prototypeTableWebAssemblyMemory, nullptr, CREATE_METHOD_TABLE(WebAssemblyMemoryPrototype) };

/* Source for WebAssemblyMemoryPrototype.lut.h
@begin prototypeTableWebAssemblyMemory
 grow   webAssemblyMemoryProtoFuncGrow   Function 1
 buffer webAssemblyMemoryProtoGetterBuffer ReadOnly|CustomAccessor
 type   webAssemblyMemoryProtoFuncType   Function 0
@end
*/

ALWAYS_INLINE JSWebAssemblyMemory* getMemory(JSGlobalObject* globalObject, VM& vm, JSValue value)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyMemory* memory = jsDynamicCast<JSWebAssemblyMemory*>(vm, value); 
    if (!memory) {
        throwException(globalObject, throwScope, 
            createTypeError(globalObject, "WebAssembly.Memory.prototype.buffer getter called with non WebAssembly.Memory |this| value"_s));
        return nullptr;
    }
    return memory;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyMemoryProtoFuncGrow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyMemory* memory = getMemory(globalObject, vm, callFrame->thisValue()); 
    RETURN_IF_EXCEPTION(throwScope, { });
    
    uint32_t delta = toNonWrappingUint32(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(throwScope, { });

    Wasm::PageCount result = memory->grow(vm, globalObject, delta);
    RETURN_IF_EXCEPTION(throwScope, { });

    return JSValue::encode(jsNumber(result.pageCount()));
}

JSC_DEFINE_CUSTOM_GETTER(webAssemblyMemoryProtoGetterBuffer, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyMemory* memory = getMemory(globalObject, vm, JSValue::decode(thisValue)); 
    RETURN_IF_EXCEPTION(throwScope, { });
    RELEASE_AND_RETURN(throwScope, JSValue::encode(memory->buffer(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyMemoryProtoFuncType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyMemory* memory = getMemory(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, { });
    RELEASE_AND_RETURN(throwScope, JSValue::encode(memory->type(globalObject)));
}


WebAssemblyMemoryPrototype* WebAssemblyMemoryPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyMemoryPrototype>(vm.heap)) WebAssemblyMemoryPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyMemoryPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyMemoryPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyMemoryPrototype::WebAssemblyMemoryPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
