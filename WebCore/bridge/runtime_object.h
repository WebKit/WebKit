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

#ifndef KJS_RUNTIME_OBJECT_H
#define KJS_RUNTIME_OBJECT_H

#include "runtime.h"
#include <runtime/JSGlobalObject.h>

namespace JSC {

class RuntimeObjectImp : public JSObject {
public:
    RuntimeObjectImp(ExecState*, PassRefPtr<Bindings::Instance>);
    virtual ~RuntimeObjectImp();

    virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier& propertyName, PropertyDescriptor&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
    virtual JSValue defaultValue(ExecState*, PreferredPrimitiveType) const;
    virtual CallType getCallData(CallData&);
    virtual ConstructType getConstructData(ConstructData&);

    virtual void getPropertyNames(ExecState*, PropertyNameArray&);
    virtual void getOwnPropertyNames(ExecState*, PropertyNameArray&);

    void invalidate();

    Bindings::Instance* getInternalInstance() const { return m_instance.get(); }

    static JSObject* throwInvalidAccessError(ExecState*);

    static const ClassInfo s_info;

    static ObjectPrototype* createPrototype(ExecState*, JSGlobalObject* globalObject)
    {
        return globalObject->objectPrototype();
    }

    static PassRefPtr<Structure> createStructure(JSValue prototype)
    {
        return Structure::create(prototype, TypeInfo(ObjectType, OverridesGetOwnPropertySlot));
    }

protected:
    RuntimeObjectImp(ExecState*, NonNullPassRefPtr<Structure>, PassRefPtr<Bindings::Instance>);

private:
    virtual const ClassInfo* classInfo() const { return &s_info; }
    
    static JSValue fallbackObjectGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue fieldGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue methodGetter(ExecState*, const Identifier&, const PropertySlot&);

    RefPtr<Bindings::Instance> m_instance;
};
    
} // namespace

#endif
