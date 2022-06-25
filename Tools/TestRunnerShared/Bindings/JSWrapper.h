/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSBasics.h"
#include "JSWrappable.h"

namespace WTR {

// FIXME: If necessary, we can do wrapper caching here.
class JSWrapper {
public:
    static JSValueRef wrap(JSContextRef, JSWrappable*);
    static JSWrappable* unwrap(JSContextRef, JSValueRef);

    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);
};

inline JSValueRef toJS(JSContextRef context, JSWrappable* impl)
{
    return JSWrapper::wrap(context, impl);
}

inline void setProperty(JSContextRef context, JSObjectRef object, const char* propertyName, JSWrappable* value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    JSObjectSetProperty(context, object, createJSString(propertyName).get(), JSWrapper::wrap(context, value), attributes, exception);
}

inline void setGlobalObjectProperty(JSContextRef context, const char* propertyName, JSWrappable* value)
{
    setGlobalObjectProperty(context, propertyName, JSWrapper::wrap(context, value));
}

} // namespace WTR
