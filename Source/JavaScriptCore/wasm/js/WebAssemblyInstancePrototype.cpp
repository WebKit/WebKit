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
#include "WebAssemblyInstancePrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssemblyInstance.h"
#include "WebAssemblyModuleRecord.h"

namespace JSC {
static JSC_DECLARE_CUSTOM_GETTER(webAssemblyInstanceProtoGetterExports);
}

#include "WebAssemblyInstancePrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyInstancePrototype::s_info = { "WebAssembly.Instance", &Base::s_info, &prototypeTableWebAssemblyInstance, nullptr, CREATE_METHOD_TABLE(WebAssemblyInstancePrototype) };

/* Source for WebAssemblyInstancePrototype.lut.h
 @begin prototypeTableWebAssemblyInstance
  exports webAssemblyInstanceProtoGetterExports ReadOnly|CustomAccessor
 @end
 */

static ALWAYS_INLINE JSWebAssemblyInstance* getInstance(JSGlobalObject* globalObject, VM& vm, JSValue v)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyInstance* result = jsDynamicCast<JSWebAssemblyInstance*>(vm, v);
    if (!result) {
        throwException(globalObject, throwScope, 
            createTypeError(globalObject, "expected |this| value to be an instance of WebAssembly.Instance"_s));
        return nullptr;
    }
    return result;
}

JSC_DEFINE_CUSTOM_GETTER(webAssemblyInstanceProtoGetterExports, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyInstance* instance = getInstance(globalObject, vm, JSValue::decode(thisValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    RELEASE_AND_RETURN(throwScope, JSValue::encode(instance->moduleRecord()->exportsObject()));
}

WebAssemblyInstancePrototype* WebAssemblyInstancePrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyInstancePrototype>(vm.heap)) WebAssemblyInstancePrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyInstancePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyInstancePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyInstancePrototype::WebAssemblyInstancePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
