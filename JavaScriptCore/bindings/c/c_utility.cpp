/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include <c_instance.h> 
#include <c_utility.h> 
#include <internal.h>
#include <runtime.h>
#include <runtime_object.h>
#include <runtime_root.h>
#include <value.h>
#include <NP_jsobject.h>

using namespace KJS;
using namespace KJS::Bindings;

NPObject *coerceValueToNPString (KJS::ExecState *exec, const KJS::Value &value)
{
    UString ustring = value.toString(exec);
    CString cstring = ustring.UTF8String();
    return NPN_CreateStringWithUTF8 (cstring.c_str(), cstring.size());
}

NPObject *convertValueToNPValueType (KJS::ExecState *exec, const KJS::Value &value)
{
    Type type = value.type();
    
    if (type == StringType) {
        UString ustring = value.toString(exec);
        CString cstring = ustring.UTF8String();
        return NPN_CreateStringWithUTF8 (cstring.c_str(), cstring.size());
    }
    else if (type == NumberType) {
        return NPN_CreateNumberWithDouble (value.toNumber(exec));
    }
    else if (type == BooleanType) {
        return NPN_CreateBoolean (value.toBoolean(exec));
    }
    else if (type == UnspecifiedType) {
        return NPN_GetUndefined();
    }
    else if (type == NullType) {
        return NPN_GetNull();
    }
    else if (type == ObjectType) {
        KJS::ObjectImp *objectImp = static_cast<KJS::ObjectImp*>(value.imp());
        if (strcmp(objectImp->classInfo()->className, "RuntimeObject") == 0) {
            KJS::RuntimeObjectImp *imp = static_cast<KJS::RuntimeObjectImp *>(value.imp());
            CInstance *instance = static_cast<CInstance*>(imp->getInternalInstance());
            return instance->getObject();
        }
    }
    
    return 0;
}

Value convertNPValueTypeToValue (KJS::ExecState *exec, const NPObject *obj)
{
    if (NPN_IsKindOfClass (obj, NPBooleanClass)) {
        if (NPN_BoolFromBoolean ((NPBoolean *)obj))
            return KJS::Boolean (true);
        return KJS::Boolean (false);
    }
    else if (NPN_IsKindOfClass (obj, NPNullClass)) {
        return Null();
    }
    else if (NPN_IsKindOfClass (obj, NPUndefinedClass)) {
        return Undefined();
    }
    else if (NPN_IsKindOfClass (obj, NPArrayClass)) {
        // FIXME:  Need to implement
    }
    else if (NPN_IsKindOfClass (obj, NPNumberClass)) {
        return Number (NPN_DoubleFromNumber((NPNumber *)obj));
    }
    else if (NPN_IsKindOfClass (obj, NPStringClass)) {

        NPUTF8 *utf8String = NPN_UTF8FromString((NPString *)obj);
        CFStringRef stringRef = CFStringCreateWithCString (NULL, utf8String, kCFStringEncodingUTF8);
        NPN_DeallocateUTF8 (utf8String);

        int length = CFStringGetLength (stringRef);
        NPUTF16 *buffer = (NPUTF16 *)malloc(sizeof(NPUTF16)*length);

        // Convert the string to UTF16.
        CFRange range = { 0, length };
        CFStringGetCharacters (stringRef, range, (UniChar *)buffer);
        CFRelease (stringRef);

        String resultString(UString((const UChar *)buffer,length));
        free (buffer);
        
        return resultString;
    }
    else if (NPN_IsKindOfClass (obj, NPScriptObjectClass)) {
        // Get ObjectImp from NP_JavaScriptObject.
        JavaScriptObject *o = (JavaScriptObject *)obj;
        return Object(const_cast<ObjectImp*>(o->imp));
    }
    else {
        //  Wrap NPObject in a CInstance.
        return Instance::createRuntimeObject(Instance::CLanguage, (void *)obj);
    }
    
    return Undefined();
}

