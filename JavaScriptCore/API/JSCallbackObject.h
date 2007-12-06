// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#ifndef JSCallbackObject_h
#define JSCallbackObject_h

#include "JSObjectRef.h"
#include "JSValueRef.h"
#include "object.h"

namespace KJS {

template <class Base>
class JSCallbackObject : public Base
{
public:
    JSCallbackObject(ExecState*, JSClassRef, JSValue* prototype, void* data);
    JSCallbackObject(JSClassRef);
    virtual ~JSCallbackObject();

    virtual UString className() const;

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned, PropertySlot&);
    
    virtual void put(ExecState*, const Identifier&, JSValue*, int attr);
    virtual void put(ExecState*, unsigned, JSValue*, int attr);

    virtual bool deleteProperty(ExecState*, const Identifier&);
    virtual bool deleteProperty(ExecState*, unsigned);

    virtual bool implementsConstruct() const;
    virtual JSObject* construct(ExecState*, const List& args);

    virtual bool implementsHasInstance() const;
    virtual bool hasInstance(ExecState *exec, JSValue *value);

    virtual bool implementsCall() const;
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List &args);

    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual double toNumber(ExecState*) const;
    virtual UString toString(ExecState*) const;

    void setPrivate(void* data);
    void* getPrivate();
    
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;

    bool inherits(JSClassRef) const;
    
private:
    void init(ExecState*);
    
    static JSValue* cachedValueGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* staticValueGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot);
    static JSValue* staticFunctionGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot);
    static JSValue* callbackGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    
    void* m_privateData;
    JSClassRef m_class;
};

} // namespace KJS

// include the actual template class implementation
#include "JSCallbackObjectFunctions.h"

#endif // JSCallbackObject_h
