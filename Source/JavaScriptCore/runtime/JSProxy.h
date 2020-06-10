/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "JSObject.h"

namespace JSC {

class JSProxy : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetPropertyNames | OverridesAnyFormOfGetPropertyNames | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        static_assert(sizeof(CellType) == sizeof(JSProxy));
        return &vm.jsProxySpace;
    }

    static JSProxy* create(VM& vm, Structure* structure, JSObject* target)
    {
        JSProxy* proxy = new (NotNull, allocateCell<JSProxy>(vm.heap)) JSProxy(vm, structure);
        proxy->finishCreation(vm, target);
        return proxy;
    }

    static JSProxy* create(VM& vm, Structure* structure)
    {
        JSProxy* proxy = new (NotNull, allocateCell<JSProxy>(vm.heap)) JSProxy(vm, structure);
        proxy->finishCreation(vm);
        return proxy;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, JSType proxyType)
    {
        ASSERT(proxyType == ImpureProxyType || proxyType == PureForwardingProxyType);
        return Structure::create(vm, globalObject, prototype, TypeInfo(proxyType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;

    JSObject* target() const { return m_target.get(); }
    static ptrdiff_t targetOffset() { return OBJECT_OFFSETOF(JSProxy, m_target); }

    JS_EXPORT_PRIVATE void setTarget(VM&, JSGlobalObject*);

protected:
    JSProxy(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
    }

    void finishCreation(VM& vm, JSObject* target)
    {
        Base::finishCreation(vm);
        m_target.set(vm, this, target);
    }

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    JS_EXPORT_PRIVATE static String className(const JSObject*, VM&);
    JS_EXPORT_PRIVATE static String toStringName(const JSObject*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned, PropertySlot&);
    JS_EXPORT_PRIVATE static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    JS_EXPORT_PRIVATE static bool putByIndex(JSCell*, JSGlobalObject*, unsigned, JSValue, bool shouldThrow);
    JS_EXPORT_PRIVATE static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    JS_EXPORT_PRIVATE static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned);
    JS_EXPORT_PRIVATE static void getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static uint32_t getEnumerableLength(JSGlobalObject*, JSObject*);
    JS_EXPORT_PRIVATE static void getStructurePropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getGenericPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    JS_EXPORT_PRIVATE static bool setPrototype(JSObject*, JSGlobalObject*, JSValue, bool shouldThrowIfCantSet);
    JS_EXPORT_PRIVATE static JSValue getPrototype(JSObject*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static bool isExtensible(JSObject*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static bool preventExtensions(JSObject*, JSGlobalObject*);

private:
    WriteBarrier<JSObject> m_target;
};

} // namespace JSC
