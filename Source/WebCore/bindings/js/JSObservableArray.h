/*
 * Copyright (C) 2022, 2023 Apple Inc.  All rights reserved.
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

#include "JSDOMBinding.h"
#include <JavaScriptCore/ArrayPrototype.h>

namespace JSC {

class ObservableArray : public RefCounted<ObservableArray> {
public:
    virtual ~ObservableArray() { }

    virtual bool setValueAt(JSGlobalObject*, unsigned index, JSValue) = 0;
    virtual void removeLast() = 0;
    virtual JSValue valueAt(JSGlobalObject*, unsigned index) const = 0;
    virtual unsigned length() const = 0;
    virtual void shrinkTo(unsigned) = 0;
};

class JSObservableArray final : public JSArray {
public:
    using Base = JSArray;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnPropertyNames | OverridesPut | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero;
    static constexpr bool needsDestruction = true;

    template<typename CellType, JSC::SubspaceAccess>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        return subspaceForImpl(vm);
    }

    static JSObservableArray* create(JSGlobalObject* lexicalGlobalObject, Ref<ObservableArray>&& array)
    {
        VM& vm = lexicalGlobalObject->vm();
        // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
        // We need to pass in the right global object for "array".
        Structure* domStructure = WebCore::deprecatedGetDOMStructure<JSObservableArray>(lexicalGlobalObject);
        JSObservableArray* observableArray = new (NotNull, allocateCell<JSObservableArray>(vm)) JSObservableArray(vm, domStructure);
        observableArray->finishCreation(vm, WTFMove(array));
        return observableArray;
    }

    ~JSObservableArray();
    static void destroy(JSCell*);

    static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool throwException);
    static void getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, DontEnumPropertiesMode);
    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned, PropertySlot&);
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool putByIndex(JSCell*, JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);

    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned propertyName);

    unsigned length() const { return m_array->length(); }

    ObservableArray& getConcreteArray() const { return *m_array; }

    DECLARE_INFO;

    static ArrayPrototype* createPrototype(VM&, JSGlobalObject& globalObject)
    {
        return globalObject.arrayPrototype();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(DerivedArrayType, StructureFlags), info(), NonArray);
    }

private:
    JSObservableArray(VM&, Structure*);
    void finishCreation(VM&, Ref<ObservableArray>&&);

    static JSC::GCClient::IsoSubspace* subspaceForImpl(JSC::VM&);

    RefPtr<ObservableArray> m_array;
};

} // namespace JSC
