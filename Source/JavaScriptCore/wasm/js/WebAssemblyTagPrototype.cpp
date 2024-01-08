/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebAssemblyTagPrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCellInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyTag.h"
#include "ObjectConstructor.h"
#include "StructureInlines.h"
#include "WasmFormat.h"

namespace JSC {
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTagProtoFuncType);
}

#include "WebAssemblyTagPrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyTagPrototype::s_info = { "WebAssembly.Tag"_s, &Base::s_info, &prototypeTableWebAssemblyTag, nullptr, CREATE_METHOD_TABLE(WebAssemblyTagPrototype) };

/* Source for WebAssemblyTagPrototype.lut.h
 @begin prototypeTableWebAssemblyTag
 type   webAssemblyTagProtoFuncType   Function 0
 @end
 */


WebAssemblyTagPrototype* WebAssemblyTagPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyTagPrototype>(vm)) WebAssemblyTagPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyTagPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyTagPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyTagPrototype::WebAssemblyTagPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

ALWAYS_INLINE static JSWebAssemblyTag* getTag(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!thisValue.isCell())) {
        throwVMError(globalObject, scope, createNotAnObjectError(globalObject, thisValue));
        return nullptr;
    }
    auto* tag = jsDynamicCast<JSWebAssemblyTag*>(thisValue.asCell());
    if (LIKELY(tag))
        return tag;
    throwTypeError(globalObject, scope, "WebAssembly.Tag operation called on non-Tag object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTagProtoFuncType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTag* jsTag = getTag(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    const Wasm::Tag& tag = jsTag->tag();

    MarkedArgumentBuffer argList;
    argList.ensureCapacity(tag.parameterCount());
    for (size_t i = 0; i < tag.parameterCount(); ++i)
        argList.append(Wasm::typeToString(vm, tag.parameter(i).kind));

    if (UNLIKELY(argList.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, throwScope);
        return encodedJSValue();
    }

    JSArray* parameters = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), argList);
    JSObject* type = constructEmptyObject(globalObject, globalObject->objectPrototype(), 1);
    type->putDirect(vm, Identifier::fromString(vm, "parameters"_s), parameters);

    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    RELEASE_AND_RETURN(throwScope, JSValue::encode(type));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
