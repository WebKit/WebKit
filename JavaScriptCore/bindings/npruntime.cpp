/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "npruntime.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"

#include "c_utility.h"
#include <CoreFoundation/CoreFoundation.h>

// FIXME: Use HashMap instead of CFDictionary for better performance and portability.

using namespace KJS::Bindings;

static Boolean stringIdentifierEqual(const void* value1, const void* value2)
{
    return strcmp((const char*)value1, (const char*)value2) == 0;
}

static CFHashCode stringIdentifierHash(const void* value)
{
    const unsigned char* key = (const unsigned char*)value;
    unsigned len = strlen((const char*)key);
    unsigned result = len;

    if (len <= 16) {
        unsigned cnt = len;
        while (cnt--)
            result = result * 257 + *key++;
    } else {
        unsigned cnt;
        for (cnt = 8; cnt > 0; cnt--)
            result = result * 257 + *key++;
        key += (len - 16);
        for (cnt = 8; cnt > 0; cnt--)
            result = result * 257 + *key++;
    }
    result += (result << (len & 31));

    return result;
}

static CFMutableDictionaryRef getStringIdentifierDictionary()
{
    static CFMutableDictionaryRef stringIdentifierDictionary = 0;
    if (!stringIdentifierDictionary) {
        CFDictionaryKeyCallBacks stringIdentifierCallbacks = { 0, NULL, NULL, NULL, stringIdentifierEqual, stringIdentifierHash };
        stringIdentifierDictionary = CFDictionaryCreateMutable(NULL, 0, &stringIdentifierCallbacks, NULL);
    }
    return stringIdentifierDictionary;
}

static CFMutableDictionaryRef getIntIdentifierDictionary()
{
    static CFMutableDictionaryRef intIdentifierDictionary = 0;
    if (!intIdentifierDictionary)
        intIdentifierDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    return intIdentifierDictionary;
}

NPIdentifier _NPN_GetStringIdentifier(const NPUTF8* name)
{
    assert(name);
    
    if (name) {
        PrivateIdentifier* identifier = 0;
        
        identifier = (PrivateIdentifier*)CFDictionaryGetValue(getStringIdentifierDictionary(), name);
        if (identifier == 0) {
            identifier = (PrivateIdentifier*)malloc(sizeof(PrivateIdentifier));
            // We never release identifier names, so this dictionary will grow, as will
            // the memory for the identifier name strings.
            identifier->isString = true;
            const char* identifierName = strdup(name);
            identifier->value.string = identifierName;

            CFDictionaryAddValue(getStringIdentifierDictionary(), identifierName, identifier);
        }
        return (NPIdentifier)identifier;
    }
    
    return 0;
}

void _NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers)
{
    assert(names);
    assert(identifiers);
    
    if (names && identifiers)
        for (int i = 0; i < nameCount; i++)
            identifiers[i] = _NPN_GetStringIdentifier(names[i]);
}

NPIdentifier _NPN_GetIntIdentifier(int32_t intid)
{
    PrivateIdentifier* identifier = 0;
    
    identifier = (PrivateIdentifier*)CFDictionaryGetValue(getIntIdentifierDictionary(), (const void*)intid);
    if (identifier == 0) {
        identifier = (PrivateIdentifier*)malloc(sizeof(PrivateIdentifier));
        // We never release identifier names, so this dictionary will grow.
        identifier->isString = false;
        identifier->value.number = intid;

        CFDictionaryAddValue(getIntIdentifierDictionary(), (const void*)intid, identifier);
    }
    return (NPIdentifier)identifier;
}

bool _NPN_IdentifierIsString(NPIdentifier identifier)
{
    PrivateIdentifier* i = (PrivateIdentifier*)identifier;
    return i->isString;
}

NPUTF8 *_NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    PrivateIdentifier* i = (PrivateIdentifier*)identifier;
    if (!i->isString || !i->value.string)
        return NULL;
        
    return (NPUTF8 *)strdup(i->value.string);
}

int32_t _NPN_IntFromIdentifier(NPIdentifier identifier)
{
    PrivateIdentifier* i = (PrivateIdentifier*)identifier;
    if (!i->isString)
        return 0;
    return i->value.number;
}

void NPN_InitializeVariantWithStringCopy(NPVariant* variant, const NPString* value)
{
    variant->type = NPVariantType_String;
    variant->value.stringValue.UTF8Length = value->UTF8Length;
    variant->value.stringValue.UTF8Characters = (NPUTF8 *)malloc(sizeof(NPUTF8) * value->UTF8Length);
    memcpy((void*)variant->value.stringValue.UTF8Characters, value->UTF8Characters, sizeof(NPUTF8) * value->UTF8Length);
}

void _NPN_ReleaseVariantValue(NPVariant* variant)
{
    assert(variant);

    if (variant->type == NPVariantType_Object) {
        _NPN_ReleaseObject(variant->value.objectValue);
        variant->value.objectValue = 0;
    } else if (variant->type == NPVariantType_String) {
        free((void*)variant->value.stringValue.UTF8Characters);
        variant->value.stringValue.UTF8Characters = 0;
        variant->value.stringValue.UTF8Length = 0;
    }

    variant->type = NPVariantType_Void;
}

NPObject *_NPN_CreateObject(NPP npp, NPClass* aClass)
{
    assert(aClass);

    if (aClass) {
        NPObject* obj;
        if (aClass->allocate != NULL)
            obj = aClass->allocate(npp, aClass);
        else
            obj = (NPObject*)malloc(sizeof(NPObject));

        obj->_class = aClass;
        obj->referenceCount = 1;

        return obj;
    }

    return 0;
}

NPObject* _NPN_RetainObject(NPObject* obj)
{
    assert(obj);

    if (obj)
        obj->referenceCount++;

    return obj;
}

void _NPN_ReleaseObject(NPObject* obj)
{
    assert(obj);
    assert(obj->referenceCount >= 1);

    if (obj && obj->referenceCount >= 1) {
        if (--obj->referenceCount == 0)
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
