/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
static JSC_DECLARE_HOST_FUNCTION(webAssemblyInstanceProtoGetterExports);
}

#include "WebAssemblyInstancePrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyInstancePrototype::s_info = { "WebAssembly.Instance"_s, &Base::s_info, &prototypeTableWebAssemblyInstance, nullptr, CREATE_METHOD_TABLE(WebAssemblyInstancePrototype) };

/* Source for WebAssemblyInstancePrototype.lut.h
 @begin prototypeTableWebAssemblyInstance
 @end
 */

static ALWAYS_INLINE JSWebAssemblyInstance* getInstance(JSGlobalObject* globalObject, VM& vm, JSValue v)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyInstance* result = jsDynamicCast<JSWebAssemblyInstance*>(v);
    if (!result) {
        throwException(globalObject, throwScope, 
            createTypeError(globalObject, "expected |this| value to be an instance of WebAssembly.Instance"_s));
        return nullptr;
    }
    return result;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyInstanceProtoGetterExports, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyInstance* instance = getInstance(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, JSValue::encode(instance->moduleRecord()->exportsObject()));
}

WebAssemblyInstancePrototype* WebAssemblyInstancePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyInstancePrototype>(vm)) WebAssemblyInstancePrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* WebAssemblyInstancePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyInstancePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->exports, webAssemblyInstanceProtoGetterExports, PropertyAttribute::ReadOnly, WebAssemblyInstanceExportsIntrinsic);
}

WebAssemblyInstancePrototype::WebAssemblyInstancePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
