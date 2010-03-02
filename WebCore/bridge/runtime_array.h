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

#include "Bridge.h"
#include <runtime/ArrayPrototype.h>

namespace JSC {
    
class RuntimeArray : public JSArray {
public:
    RuntimeArray(ExecState*, Bindings::Array*);
    virtual ~RuntimeArray();

    virtual void getOwnPropertyNames(ExecState*, PropertyNameArray&, EnumerationMode mode = ExcludeDontEnumProperties);
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned, PropertySlot&);
    virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
    virtual void put(ExecState*, unsigned propertyName, JSValue);
    
    virtual bool deleteProperty(ExecState* exec, const Identifier &propertyName);
    virtual bool deleteProperty(ExecState* exec, unsigned propertyName);
    
    virtual const ClassInfo* classInfo() const { return &s_info; }
    
    unsigned getLength() const { return getConcreteArray()->getLength(); }
    
    Bindings::Array* getConcreteArray() const { return static_cast<Bindings::Array*>(subclassData()); }

    static const ClassInfo s_info;

    static ArrayPrototype* createPrototype(ExecState*, JSGlobalObject* globalObject)
    {
        return globalObject->arrayPrototype();
    }

private:
    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesGetPropertyNames | JSObject::StructureFlags;
    static JSValue lengthGetter(ExecState*, JSValue, const Identifier&);
    static JSValue indexGetter(ExecState*, JSValue, unsigned);
};
    
} // namespace JSC

#endif // RUNTIME_ARRAY_H_
