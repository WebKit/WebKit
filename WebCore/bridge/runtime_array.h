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

#include "runtime.h"
#include <runtime/JSGlobalObject.h>

namespace JSC {
    
class RuntimeArray : public JSObject {
public:
    RuntimeArray(ExecState*, Bindings::Array*);
    
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState *, unsigned, PropertySlot&);
    virtual bool getOwnPropertyDescriptor(ExecState *, const Identifier&, PropertyDescriptor&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
    virtual void put(ExecState*, unsigned propertyName, JSValue);
    
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual bool deleteProperty(ExecState *exec, unsigned propertyName);
    
    virtual const ClassInfo *classInfo() const { return &s_info; }
    
    unsigned getLength() const { return getConcreteArray()->getLength(); }
    
    Bindings::Array *getConcreteArray() const { return _array.get(); }

    static const ClassInfo s_info;

    static ArrayPrototype* createPrototype(ExecState*, JSGlobalObject* globalObject)
    {
        return globalObject->arrayPrototype();
    }

    static PassRefPtr<Structure> createStructure(JSValue prototype)
    {
        return Structure::create(prototype, TypeInfo(ObjectType, OverridesGetOwnPropertySlot | OverridesGetPropertyNames));
    }

private:
    static JSValue lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue indexGetter(ExecState*, const Identifier&, const PropertySlot&);

    OwnPtr<Bindings::Array> _array;
};
    
} // namespace JSC

#endif // RUNTIME_ARRAY_H_
