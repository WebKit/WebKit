/*
 * Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef KJS_RUNTIME_OBJECT_H
#define KJS_RUNTIME_OBJECT_H

#include "BridgeJSC.h"
#include <JavaScriptCore/JSGlobalObject.h>

namespace JSC {
namespace Bindings {

class WEBCORE_EXPORT RuntimeObject : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetPropertyNames | OverridesGetCallData;

    static RuntimeObject* create(VM& vm, Structure* structure, RefPtr<Instance>&& instance)
    {
        RuntimeObject* object = new (NotNull, allocateCell<RuntimeObject>(vm.heap)) RuntimeObject(vm, structure, WTFMove(instance));
        object->finishCreation(vm);
        return object;
    }

    static void destroy(JSCell*);

    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static bool put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    static JSValue defaultValue(const JSObject*, ExecState*, PreferredPrimitiveType);
    static CallType getCallData(JSCell*, CallData&);
    static ConstructType getConstructData(JSCell*, ConstructData&);

    static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    void invalidate();

    Instance* getInternalInstance() const { return m_instance.get(); }

    static Exception* throwInvalidAccessError(ExecState*, ThrowScope&);

    DECLARE_INFO;

    static ObjectPrototype* createPrototype(VM&, JSGlobalObject& globalObject)
    {
        return globalObject.objectPrototype();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

protected:
    RuntimeObject(VM&, Structure*, RefPtr<Instance>&&);
    void finishCreation(VM&);

private:
    static EncodedJSValue fallbackObjectGetter(ExecState*, EncodedJSValue, PropertyName);
    static EncodedJSValue fieldGetter(ExecState*, EncodedJSValue, PropertyName);
    static EncodedJSValue methodGetter(ExecState*, EncodedJSValue, PropertyName);

    RefPtr<Instance> m_instance;
};
    
}
}

#endif
