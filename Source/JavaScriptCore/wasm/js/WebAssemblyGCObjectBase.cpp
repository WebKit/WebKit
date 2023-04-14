/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebAssemblyGCObjectBase.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "TypeError.h"

namespace JSC {

const ClassInfo WebAssemblyGCObjectBase::s_info = { "WebAssemblyGCObjectBase"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyGCObjectBase) };

WebAssemblyGCObjectBase::WebAssemblyGCObjectBase(VM& vm, Structure* structure, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure)
    , m_rtt(rtt)
{
}

template<typename Visitor>
void WebAssemblyGCObjectBase::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    WebAssemblyGCObjectBase* thisObject = jsCast<WebAssemblyGCObjectBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(WebAssemblyGCObjectBase);

bool WebAssemblyGCObjectBase::getOwnPropertySlot(JSObject* object, JSGlobalObject*, PropertyName, PropertySlot& slot)
{
    slot.setValue(object, static_cast<unsigned>(JSC::PropertyAttribute::None), jsUndefined());
    return false;
}

bool WebAssemblyGCObjectBase::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject*, unsigned, PropertySlot& slot)
{
    slot.setValue(object, static_cast<unsigned>(JSC::PropertyAttribute::None), jsUndefined());
    return false;
}

bool WebAssemblyGCObjectBase::put(JSCell*, JSGlobalObject* globalObject, PropertyName, JSValue, PutPropertySlot&)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, true, "Cannot set property for WebAssembly GC object"_s);
}

bool WebAssemblyGCObjectBase::putByIndex(JSCell*, JSGlobalObject* globalObject, unsigned, JSValue, bool)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, true, "Cannot set property for WebAssembly GC object"_s);
}

bool WebAssemblyGCObjectBase::deleteProperty(JSCell*, JSGlobalObject* globalObject, PropertyName, DeletePropertySlot&)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, true, "Cannot delete property for WebAssembly GC object"_s);
}

bool WebAssemblyGCObjectBase::deletePropertyByIndex(JSCell*, JSGlobalObject* globalObject, unsigned)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, true, "Cannot delete property for WebAssembly GC object"_s);
}

void WebAssemblyGCObjectBase::getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray& propertyNameArray, DontEnumPropertiesMode)
{
#if ASSERT_ENABLED
    ASSERT(!propertyNameArray.size());
#else
    UNUSED_PARAM(propertyNameArray);
#endif
    return;
}

bool WebAssemblyGCObjectBase::defineOwnProperty(JSObject*, JSGlobalObject* globalObject, PropertyName, const PropertyDescriptor&, bool shouldThrow)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, shouldThrow, "Cannot define property for WebAssembly GC object"_s);
}

JSValue WebAssemblyGCObjectBase::getPrototype(JSObject*, JSGlobalObject*)
{
    return jsNull();
}

bool WebAssemblyGCObjectBase::setPrototype(JSObject*, JSGlobalObject* globalObject, JSValue, bool shouldThrowIfCantSet)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, shouldThrowIfCantSet, "Cannot set prototype of WebAssembly GC object"_s);
}

bool WebAssemblyGCObjectBase::isExtensible(JSObject*, JSGlobalObject*)
{
    return false;
}

bool WebAssemblyGCObjectBase::preventExtensions(JSObject*, JSGlobalObject* globalObject)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return typeError(globalObject, scope, true, "Cannot run preventExtensions operation on WebAssembly GC object"_s);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
