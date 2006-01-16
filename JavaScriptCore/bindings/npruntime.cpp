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
#include "config.h"
#include <CoreFoundation/CoreFoundation.h>

#include <npruntime.h>
#include <c_utility.h>

#include <JavaScriptCore/npruntime_impl.h>

using namespace KJS::Bindings;

static Boolean stringIdentifierEqual(const void *value1, const void *value2)
{
    return strcmp((const char *)value1, (const char *)value2) == 0;
}

static CFHashCode stringIdentifierHash (const void *value)
{
    const char *key = (const char *)value;
    unsigned int len = strlen(key);
    unsigned int result = len;

    if (len <= 16) {
        unsigned cnt = len;
        while (cnt--) result = result * 257 + *(unsigned char *)key++;
    } else {
        unsigned cnt;
        for (cnt = 8; cnt > 0; cnt--) result = result * 257 + *(unsigned char *)key++;
        key += (len - 16);
        for (cnt = 8; cnt > 0; cnt--) result = result * 257 + *(unsigned char *)key++;
    }
    result += (result << (len & 31));

    return result;
}

static CFDictionaryKeyCallBacks stringIdentifierCallbacks = {
    0,
    NULL,
    NULL,
    NULL,
    stringIdentifierEqual,
    stringIdentifierHash
};

static CFMutableDictionaryRef stringIdentifierDictionary = 0;

static CFMutableDictionaryRef getStringIdentifierDictionary()
{
    if (!stringIdentifierDictionary)
        stringIdentifierDictionary = CFDictionaryCreateMutable(NULL, 0, &stringIdentifierCallbacks, NULL);
    return stringIdentifierDictionary;
}

static Boolean intIdentifierEqual(const void *value1, const void *value2)
{
    return value1 == value2;
}

static CFHashCode intIdentifierHash (const void *value)
{
    return (CFHashCode)value;
}

static CFDictionaryKeyCallBacks intIdentifierCallbacks = {
    0,
    NULL,
    NULL,
    NULL,
    intIdentifierEqual,
    intIdentifierHash
};

static CFMutableDictionaryRef intIdentifierDictionary = 0;

static CFMutableDictionaryRef getIntIdentifierDictionary()
{
    if (!intIdentifierDictionary)
        intIdentifierDictionary = CFDictionaryCreateMutable(NULL, 0, &intIdentifierCallbacks, NULL);
    return intIdentifierDictionary;
}

NPIdentifier _NPN_GetStringIdentifier (const NPUTF8 *name)
{
    assert (name);
    
    if (name) {
        PrivateIdentifier *identifier = 0;
        
        identifier = (PrivateIdentifier *)CFDictionaryGetValue (getStringIdentifierDictionary(), (const void *)name);
        if (identifier == 0) {
            identifier = (PrivateIdentifier *)malloc (sizeof(PrivateIdentifier));
            // We never release identifier names, so this dictionary will grow, as will
            // the memory for the identifier name strings.
            identifier->isString = true;
            const char *identifierName = strdup (name);
            identifier->value.string = identifierName;

            CFDictionaryAddValue (getStringIdentifierDictionary(), identifierName, (const void *)identifier);
        }
        return (NPIdentifier)identifier;
    }
    
    return 0;
}

void _NPN_GetStringIdentifiers (const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
    assert (names);
    assert (identifiers);
    
    if (names && identifiers) {
        int i;
        
        for (i = 0; i < nameCount; i++) {
            identifiers[i] = _NPN_GetStringIdentifier (names[i]);
        }
    }
}

NPIdentifier _NPN_GetIntIdentifier(int32_t intid)
{
    PrivateIdentifier *identifier = 0;
    
    identifier = (PrivateIdentifier *)CFDictionaryGetValue (getIntIdentifierDictionary(), (const void *)intid);
    if (identifier == 0) {
        identifier = (PrivateIdentifier *)malloc (sizeof(PrivateIdentifier));
        // We never release identifier names, so this dictionary will grow.
        identifier->isString = false;
        identifier->value.number = intid;

        CFDictionaryAddValue (getIntIdentifierDictionary(), (const void *)intid, (const void *)identifier);
    }
    return (NPIdentifier)identifier;
}

bool _NPN_IdentifierIsString(NPIdentifier identifier)
{
    PrivateIdentifier *i = (PrivateIdentifier *)identifier;
    return i->isString;
}

NPUTF8 *_NPN_UTF8FromIdentifier (NPIdentifier identifier)
{
    PrivateIdentifier *i = (PrivateIdentifier *)identifier;
    if (!i->isString || !i->value.string)
        return NULL;
        
    return (NPUTF8 *)strdup(i->value.string);
}

int32_t _NPN_IntFromIdentifier(NPIdentifier identifier)
{
    PrivateIdentifier *i = (PrivateIdentifier *)identifier;
    if (!i->isString)
        return 0;
    return i->value.number;
}

NPBool NPN_VariantIsVoid (const NPVariant *variant)
{
    return variant->type == NPVariantType_Void;
}

NPBool NPN_VariantIsNull (const NPVariant *variant)
{
    return variant->type == NPVariantType_Null;
}

NPBool NPN_VariantIsUndefined (const NPVariant *variant)
{
    return variant->type == NPVariantType_Void;
}

NPBool NPN_VariantIsBool (const NPVariant *variant)
{
    return variant->type == NPVariantType_Bool;
}

NPBool NPN_VariantIsInt32 (const NPVariant *variant)
{
    return variant->type == NPVariantType_Int32;
}

NPBool NPN_VariantIsDouble (const NPVariant *variant)
{
    return variant->type == NPVariantType_Double;
}

NPBool NPN_VariantIsString (const NPVariant *variant)
{
    return variant->type == NPVariantType_String;
}

NPBool NPN_VariantIsObject (const NPVariant *variant)
{
    return variant->type == NPVariantType_Object;
}

NPBool NPN_VariantToBool (const NPVariant *variant, NPBool *result)
{
    if (variant->type != NPVariantType_Bool)
        return false;
        
    *result = variant->value.boolValue;
    
    return true;
}

NPString NPN_VariantToString (const NPVariant *variant)
{
    if (variant->type != NPVariantType_String) {
        NPString emptyString = { 0, 0 };
        return emptyString;
    }
            
    return variant->value.stringValue;
}

NPBool NPN_VariantToInt32 (const NPVariant *variant, int32_t *result)
{
    if (variant->type == NPVariantType_Int32)
        *result = variant->value.intValue;
    else if (variant->type == NPVariantType_Double)
        *result = (int32_t)variant->value.doubleValue;
    else
        return false;
    
    return true;
}

NPBool NPN_VariantToDouble (const NPVariant *variant, double *result)
{
    if (variant->type == NPVariantType_Int32)
        *result = (double)variant->value.intValue;
    else if (variant->type == NPVariantType_Double)
        *result = variant->value.doubleValue;
    else
        return false;
    
    return true;
}

NPBool NPN_VariantToObject (const NPVariant *variant, NPObject **result)
{
    if (variant->type != NPVariantType_Object)
        return false;
            
    *result = variant->value.objectValue;
    
    return true;
}

void NPN_InitializeVariantAsVoid (NPVariant *variant)
{
    variant->type = NPVariantType_Void;
}

void NPN_InitializeVariantAsNull (NPVariant *variant)
{
    variant->type = NPVariantType_Null;
}

void NPN_InitializeVariantAsUndefined (NPVariant *variant)
{
    variant->type = NPVariantType_Void;
}

void NPN_InitializeVariantWithBool (NPVariant *variant, NPBool value)
{
    variant->type = NPVariantType_Bool;
    variant->value.boolValue = value;
}

void NPN_InitializeVariantWithInt32 (NPVariant *variant, int32_t value)
{
    variant->type = NPVariantType_Int32;
    variant->value.intValue = value;
}

void NPN_InitializeVariantWithDouble (NPVariant *variant, double value)
{
    variant->type = NPVariantType_Double;
    variant->value.doubleValue = value;
}

void NPN_InitializeVariantWithString (NPVariant *variant, const NPString *value)
{
    variant->type = NPVariantType_String;
    variant->value.stringValue = *value;
}

void NPN_InitializeVariantWithStringCopy (NPVariant *variant, const NPString *value)
{
    variant->type = NPVariantType_String;
    variant->value.stringValue.UTF8Length = value->UTF8Length;
    variant->value.stringValue.UTF8Characters = (NPUTF8 *)malloc(sizeof(NPUTF8) * value->UTF8Length);
    memcpy ((void *)variant->value.stringValue.UTF8Characters, value->UTF8Characters, sizeof(NPUTF8) * value->UTF8Length);
}

void NPN_InitializeVariantWithObject (NPVariant *variant, NPObject *value)
{
    variant->type = NPVariantType_Object;
    variant->value.objectValue = _NPN_RetainObject (value);
}

void NPN_InitializeVariantWithVariant (NPVariant *destination, const NPVariant *source)
{
    switch (source->type){
        case NPVariantType_Void: {
            NPN_InitializeVariantAsVoid (destination);
            break;
        }
        case NPVariantType_Null: {
            NPN_InitializeVariantAsNull (destination);
            break;
        }
        case NPVariantType_Bool: {
            NPN_InitializeVariantWithBool (destination, source->value.boolValue);
            break;
        }
        case NPVariantType_Int32: {
            NPN_InitializeVariantWithInt32 (destination, source->value.intValue);
            break;
        }
        case NPVariantType_Double: {
            NPN_InitializeVariantWithDouble (destination, source->value.doubleValue);
            break;
        }
        case NPVariantType_String: {
            NPN_InitializeVariantWithStringCopy (destination, &source->value.stringValue);
            break;
        }
        case NPVariantType_Object: {
            NPN_InitializeVariantWithObject (destination, source->value.objectValue);
            break;
        }
        default: {
            NPN_InitializeVariantAsUndefined (destination);
            break;
        }
    }
}

void _NPN_ReleaseVariantValue (NPVariant *variant)
{
    assert (variant);
    
    if (variant->type == NPVariantType_Object) {
        _NPN_ReleaseObject (variant->value.objectValue);
        variant->value.objectValue = 0;
    }
    else if (variant->type == NPVariantType_String) {
        free ((void *)variant->value.stringValue.UTF8Characters);
        variant->value.stringValue.UTF8Characters = 0;
        variant->value.stringValue.UTF8Length = 0;
    }
    
    variant->type = NPVariantType_Void;
}


NPObject *_NPN_CreateObject (NPP npp, NPClass *aClass)
{
    assert (aClass);

    if (aClass) {
        NPObject *obj;
        
        if (aClass->allocate != NULL)
            obj = aClass->allocate (npp, aClass);
        else
            obj = (NPObject *)malloc (sizeof(NPObject));
            
        obj->_class = aClass;
        obj->referenceCount = 1;

        return obj;
    }
    
    return 0;
}


NPObject *_NPN_RetainObject (NPObject *obj)
{
    assert (obj);

    if (obj)
        obj->referenceCount++;

    return obj;
}


void _NPN_ReleaseObject (NPObject *obj)
{
    assert (obj);
    assert (obj->referenceCount >= 1);

    if (obj && obj->referenceCount >= 1) {
        obj->referenceCount--;
        
        if (obj->referenceCount == 0)
            _NPN_DeallocateObject(obj);
    }
}

void _NPN_DeallocateObject(NPObject *obj)
{
    assert(obj);

    if (obj) {
        if (obj->_class->deallocate)
            obj->_class->deallocate(obj);
        else
            free(obj);
    }
}
