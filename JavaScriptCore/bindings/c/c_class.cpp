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
#include "config.h"
#include <c_class.h>
#include <c_instance.h>
#include <c_runtime.h>

using namespace KJS::Bindings;

bool NPN_IsValidIdentifier (const NPUTF8 *name);

void CClass::_commonDelete() {
    CFRelease (_fields);
    CFRelease (_methods);
}
    

void CClass::_commonCopy(const CClass &other) {
    _isa = other._isa;
    _methods = CFDictionaryCreateCopy (NULL, other._methods);
    _fields = CFDictionaryCreateCopy (NULL, other._fields);
}
    

void CClass::_commonInit (NPClass *aClass)
{
    _isa = aClass;
    _methods = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _fields = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
}


static CFMutableDictionaryRef classesByIsA = 0;

static void _createClassesByIsAIfNecessary()
{
    if (classesByIsA == 0)
        classesByIsA = CFDictionaryCreateMutable (NULL, 0, NULL, NULL);
}

CClass *CClass::classForIsA (NPClass *isa)
{
    _createClassesByIsAIfNecessary();
    
    CClass *aClass = (CClass *)CFDictionaryGetValue(classesByIsA, isa);
    if (aClass == NULL) {
        aClass = new CClass (isa);
        CFDictionaryAddValue (classesByIsA, isa, aClass);
    }
    
    return aClass;
}


CClass::CClass (NPClass *isa)
{
    _commonInit (isa);
}

const char *CClass::name() const
{
    return "";
}

MethodList CClass::methodsNamed(const char *_name, Instance *instance) const
{
    MethodList methodList;

    CFStringRef methodName = CFStringCreateWithCString(NULL, _name, kCFStringEncodingASCII);
    Method *method = (Method *)CFDictionaryGetValue (_methods, methodName);
    if (method) {
        CFRelease (methodName);
        methodList.addMethod(method);
        return methodList;
    }
    
    NPIdentifier ident = _NPN_GetStringIdentifier (_name);
    const CInstance *inst = static_cast<const CInstance*>(instance);
    NPObject *obj = inst->getObject();
    if (_isa->hasMethod && _isa->hasMethod (obj, ident)){
        Method *aMethod = new CMethod (ident);
        CFDictionaryAddValue ((CFMutableDictionaryRef)_methods, methodName, aMethod);
        methodList.addMethod (aMethod);
    }

    CFRelease (methodName);
    
    return methodList;
}


Field *CClass::fieldNamed(const char *name, Instance *instance) const
{
    CFStringRef fieldName = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
    Field *aField = (Field *)CFDictionaryGetValue (_fields, fieldName);
    if (aField) {
        CFRelease (fieldName);
        return aField;
    }

    NPIdentifier ident = _NPN_GetStringIdentifier (name);
    const CInstance *inst = static_cast<const CInstance*>(instance);
    NPObject *obj = inst->getObject();
    if (_isa->hasProperty && _isa->hasProperty (obj, ident)){
        aField = new CField (ident);
        CFDictionaryAddValue ((CFMutableDictionaryRef)_fields, fieldName, aField);
    }
    
    CFRelease (fieldName);

    return aField;
};
