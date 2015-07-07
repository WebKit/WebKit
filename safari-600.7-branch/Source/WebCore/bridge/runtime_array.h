/*
 * Copyright (C) 2003, 2008 Apple Inc.  All rights reserved.
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

#ifndef RUNTIME_ARRAY_H_
#define RUNTIME_ARRAY_H_

#include "BridgeJSC.h"
#include "JSDOMBinding.h"
#include <runtime/ArrayPrototype.h>

namespace JSC {
    
class RuntimeArray : public JSArray {
public:
    typedef JSArray Base;

    static RuntimeArray* create(ExecState* exec, Bindings::Array* array)
    {
        // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
        // We need to pass in the right global object for "array".
        Structure* domStructure = WebCore::deprecatedGetDOMStructure<RuntimeArray>(exec);
        RuntimeArray* runtimeArray = new (NotNull, allocateCell<RuntimeArray>(*exec->heap())) RuntimeArray(exec, domStructure);
        runtimeArray->finishCreation(exec->vm(), array);
        exec->vm().heap.addFinalizer(runtimeArray, destroy);
        return runtimeArray;
    }

    typedef Bindings::Array BindingsArray;
    ~RuntimeArray();
    static void destroy(JSCell*);
    static const bool needsDestruction = false;

    static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSObject*, ExecState*, unsigned, PropertySlot&);
    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
    
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);
    
    unsigned getLength() const { return m_array->getLength(); }
    
    Bindings::Array* getConcreteArray() const { return m_array; }

    DECLARE_INFO;

    static ArrayPrototype* createPrototype(VM&, JSGlobalObject* globalObject)
    {
        return globalObject->arrayPrototype();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info(), ArrayClass);
    }

protected:
    void finishCreation(VM&, Bindings::Array*);

    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | OverridesGetPropertyNames | JSArray::StructureFlags;

private:
    RuntimeArray(ExecState*, Structure*);
    static EncodedJSValue lengthGetter(ExecState*, JSObject*, EncodedJSValue, PropertyName);

    BindingsArray* m_array;
};

} // namespace JSC

#endif // RUNTIME_ARRAY_H_
