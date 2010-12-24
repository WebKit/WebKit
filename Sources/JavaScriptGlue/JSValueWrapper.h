/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSValueWrapper_h
#define JSValueWrapper_h

#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"

class JSValueWrapper {
public:
    JSValueWrapper(JSValue);
    virtual ~JSValueWrapper();

    static void GetJSObectCallBacks(JSObjectCallBacks& callBacks);

    JSValue GetValue();

private:
    ProtectedJSValue fValue;
    
    static void JSObjectDispose(void *data);
    static CFArrayRef JSObjectCopyPropertyNames(void *data);
    static JSObjectRef JSObjectCopyProperty(void *data, CFStringRef propertyName);
    static void JSObjectSetProperty(void *data, CFStringRef propertyName, JSObjectRef jsValue);
    static JSObjectRef JSObjectCallFunction(void *data, JSObjectRef thisObj, CFArrayRef args);
    static CFTypeRef JSObjectCopyCFValue(void *data);
    static void JSObjectMark(void *data);
};

#endif
