/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
#include <kjs/JSGlobalObject.h>

namespace JSC {

class RuntimeObjectImp : public JSObject {
public:
    virtual ~RuntimeObjectImp();

    virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValuePtr, PutPropertySlot&);
    virtual bool deleteProperty(ExecState* , const Identifier& propertyName);
    virtual JSValuePtr defaultValue(ExecState*, PreferredPrimitiveType) const;
    virtual CallType getCallData(CallData&);
    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual void invalidate();
    Bindings::Instance* getInternalInstance() const { return instance.get(); }

    static JSObject* throwInvalidAccessError(ExecState*);

    static const ClassInfo s_info;

    static ObjectPrototype* createPrototype(ExecState* exec)
    {
        return exec->lexicalGlobalObject()->objectPrototype();
    }

    static PassRefPtr<StructureID> createStructureID(JSValuePtr prototype)
    {
        return StructureID::create(prototype, TypeInfo(ObjectType));
    }

protected:
    RuntimeObjectImp(ExecState*, PassRefPtr<StructureID>, PassRefPtr<Bindings::Instance>);

private:
    friend class Bindings::Instance;
    RuntimeObjectImp(ExecState*, PassRefPtr<Bindings::Instance>);

    virtual const ClassInfo* classInfo() const { return &s_info; }
    
    static JSValuePtr fallbackObjectGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValuePtr fieldGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValuePtr methodGetter(ExecState*, const Identifier&, const PropertySlot&);

    RefPtr<Bindings::Instance> instance;
};
    
} // namespace

#endif
