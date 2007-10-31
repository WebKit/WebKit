// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef APICast_h
#define APICast_h

#include "ustring.h"
#include "ExecState.h"

namespace KJS {
    class ExecState;
    class JSValue;
    class JSObject;
    class PropertyNameArray;
}

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSContext* JSGlobalContextRef;
typedef struct OpaqueJSString* JSStringRef;
typedef struct OpaqueJSPropertyNameAccumulator* JSPropertyNameAccumulatorRef;
typedef const struct OpaqueJSValue* JSValueRef;
typedef struct OpaqueJSValue* JSObjectRef;

/* Opaque typing convenience methods */

inline KJS::ExecState* toJS(JSContextRef c)
{
    return reinterpret_cast<KJS::ExecState*>(const_cast<OpaqueJSContext*>(c));
}

inline KJS::ExecState* toJS(JSGlobalContextRef c)
{
    return reinterpret_cast<KJS::ExecState*>(c);
}

inline KJS::JSValue* toJS(JSValueRef v)
{
    return reinterpret_cast<KJS::JSValue*>(const_cast<OpaqueJSValue*>(v));
}

inline KJS::UString::Rep* toJS(JSStringRef b)
{
    return reinterpret_cast<KJS::UString::Rep*>(b);
}

inline KJS::JSObject* toJS(JSObjectRef o)
{
    return reinterpret_cast<KJS::JSObject*>(o);
}

inline KJS::PropertyNameArray* toJS(JSPropertyNameAccumulatorRef a)
{
    return reinterpret_cast<KJS::PropertyNameArray*>(a);
}

inline JSValueRef toRef(KJS::JSValue* v)
{
    return reinterpret_cast<JSValueRef>(v);
}

inline JSValueRef* toRef(KJS::JSValue** v)
{
    return reinterpret_cast<JSValueRef*>(const_cast<const KJS::JSValue**>(v));
}

inline JSStringRef toRef(KJS::UString::Rep* s)
{
    return reinterpret_cast<JSStringRef>(s);
}

inline JSObjectRef toRef(KJS::JSObject* o)
{
    return reinterpret_cast<JSObjectRef>(o);
}

inline JSObjectRef toRef(const KJS::JSObject* o)
{
    return reinterpret_cast<JSObjectRef>(const_cast<KJS::JSObject*>(o));
}

inline JSContextRef toRef(KJS::ExecState* e)
{
    return reinterpret_cast<JSContextRef>(e);
}

inline JSGlobalContextRef toGlobalRef(KJS::ExecState* e)
{
    ASSERT(!e->callingExecState());
    return reinterpret_cast<JSGlobalContextRef>(e);
}

inline JSPropertyNameAccumulatorRef toRef(KJS::PropertyNameArray* l)
{
    return reinterpret_cast<JSPropertyNameAccumulatorRef>(l);
}

#endif // APICast_h
