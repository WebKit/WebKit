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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
        RuntimeArray* runtimeArray = new (allocateCell<RuntimeArray>(*exec->heap())) RuntimeArray(exec, domStructure);
        runtimeArray->finishCreation(exec->globalData(), array);
        return runtimeArray;
    }

    typedef Bindings::Array BindingsArray;
    ~RuntimeArray();
    static void destroy(JSCell*);

    static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static bool getOwnPropertySlot(JSCell*, ExecState*, const Identifier&, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSCell*, ExecState*, unsigned, PropertySlot&);
    static bool getOwnPropertyDescriptor(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&);
    static void put(JSCell*, ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
    static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue);
    
    static bool deleteProperty(JSCell*, ExecState*, const Identifier &propertyName);
    static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);
    
    unsigned getLength() const { return getConcreteArray()->getLength(); }
    
    Bindings::Array* getConcreteArray() const { return static_cast<BindingsArray*>(subclassData()); }

    static const ClassInfo s_info;

    static ArrayPrototype* createPrototype(ExecState*, JSGlobalObject* globalObject)
    {
        return globalObject->arrayPrototype();
    }

    static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(globalData, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), &s_info);
    }

protected:
    void finishCreation(JSGlobalData&, Bindings::Array*);

    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesGetPropertyNames | JSArray::StructureFlags;

private:
    RuntimeArray(ExecState*, Structure*);
    static JSValue lengthGetter(ExecState*, JSValue, const Identifier&);
    static JSValue indexGetter(ExecState*, JSValue, unsigned);
};
    
} // namespace JSC

#endif // RUNTIME_ARRAY_H_
