/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include "object.h"

#include <wtf/Noncopyable.h>

namespace KJS {

class RuntimeObjectImp : public JSObject {
public:
    virtual ~RuntimeObjectImp();
    
    const ClassInfo *classInfo() const { return &info; }

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual bool canPut(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual JSValue *defaultValue(ExecState *exec, JSType hint) const;
    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual void invalidate();
    Bindings::Instance *getInternalInstance() const { return instance.get(); }
    
    static JSObject* throwInvalidAccessError(ExecState*);
    
    static const ClassInfo info;

protected:
    friend class Bindings::Instance;
    RuntimeObjectImp(Bindings::Instance*); // Only allow Instances and derived classes to create us

private:
    RuntimeObjectImp(); // prevent default construction
    
    static JSValue *fallbackObjectGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *fieldGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *methodGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);

    RefPtr<Bindings::Instance> instance;
};
    
} // namespace

#endif
