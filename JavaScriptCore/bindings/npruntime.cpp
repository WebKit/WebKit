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
#include <CoreFoundation/CoreFoundation.h>

#include <npruntime.h>
#include <c_utility.h>

static Boolean identifierEqual(const void *value1, const void *value2)
{
    return strcmp((const char *)value1, (const char *)value2) == 0;
}

static CFHashCode identifierHash (const void *value)
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

CFDictionaryKeyCallBacks identifierCallbacks = {
    0,
    NULL,
    NULL,
    NULL,
    identifierEqual,
    identifierHash
};

static CFMutableDictionaryRef identifierDictionary = 0;

static CFMutableDictionaryRef getIdentifierDictionary()
{
    if (!identifierDictionary)
        identifierDictionary = CFDictionaryCreateMutable(NULL, 0, &identifierCallbacks, NULL);
    return identifierDictionary;
}

#define INITIAL_IDENTIFIER_NAME_COUNT   128

static const char **identifierNames = 0;
static unsigned int maxIdentifierNames;
static uint32_t identifierCount = 1;

NPIdentifier NPN_GetIdentifier (const NPUTF8 *name)
{
    assert (name);
    
    if (name) {
        NPIdentifier identifier = 0;
        
        identifier = (NPIdentifier)CFDictionaryGetValue (getIdentifierDictionary(), (const void *)name);
        if (identifier == 0) {
            identifier = (NPIdentifier)identifierCount++;
            // We never release identifier names, so this dictionary will grow, as will
            // the memory for the identifier name strings.
            const char *identifierName = strdup (name);
            
            if (!identifierNames) {
                identifierNames = (const char **)malloc (sizeof(const char *)*INITIAL_IDENTIFIER_NAME_COUNT);
                maxIdentifierNames = INITIAL_IDENTIFIER_NAME_COUNT;
            }
            
            if (identifierCount >= maxIdentifierNames) {
                maxIdentifierNames *= 2;
                identifierNames = (const char **)realloc ((void *)identifierNames, sizeof(const char *)*maxIdentifierNames);
            }
            
            identifierNames[(uint32_t)identifier] = identifierName;

            CFDictionaryAddValue (getIdentifierDictionary(), identifierName, (const void *)identifier);
        }
        return identifier;
    }
    
    return 0;
}

bool NPN_IsValidIdentifier (const NPUTF8 *name)
{
    assert (name);
    
    if (name)
        return CFDictionaryGetValue (getIdentifierDictionary(), (const void *)name) == 0 ? false : true;

    return false;
}

void NPN_GetIdentifiers (const NPUTF8 **names, int nameCount, NPIdentifier *identifiers)
{
    assert (names);
    assert (identifiers);
    
    if (names && identifiers) {
        int i;
        
        for (i = 0; i < nameCount; i++) {
            identifiers[i] = NPN_GetIdentifier (names[i]);
        }
    }
}

NPUTF8 *NPN_UTF8FromIdentifier (NPIdentifier identifier)
{
    if (identifier == 0 || (uint32_t)identifier >= identifierCount)
        return NULL;
        
    return (NPUTF8 *)identifierNames[(uint32_t)identifier];
}

NPBool NPN_VariantIsVoid (const NPVariant *variant)
{
    return variant->type == NPVariantVoidType;
}

NPBool NPN_VariantIsNull (const NPVariant *variant)
{
    return variant->type == NPVariantNullType;
}

NPBool NPN_VariantIsUndefined (const NPVariant *variant)
{
    return variant->type == NPVariantUndefinedType;
}

NPBool NPN_VariantIsBool (const NPVariant *variant)
{
    return variant->type == NPVariantBoolType;
}

NPBool NPN_VariantIsInt32 (const NPVariant *variant)
{
    return variant->type == NPVariantInt32Type;
}

NPBool NPN_VariantIsDouble (const NPVariant *variant)
{
    return variant->type == NPVariantDoubleType;
}

NPBool NPN_VariantIsString (const NPVariant *variant)
{
    return variant->type == NPVariantStringType;
}

NPBool NPN_VariantIsObject (const NPVariant *variant)
{
    return variant->type == NPVariantObjectType;
}

NPBool NPN_VariantToBool (const NPVariant *variant, NPBool *result)
{
    if (variant->type != NPVariantBoolType)
        return false;
        
    *result = variant->value.boolValue;
    
    return true;
}

NPString NPN_VariantToString (const NPVariant *variant)
{
    if (variant->type != NPVariantStringType) {
        NPString emptyString = { 0, 0 };
        return emptyString;
    }
            
    return variant->value.stringValue;
}

NPBool NPN_VariantToInt32 (const NPVariant *variant, int32_t *result)
{
    if (variant->type == NPVariantInt32Type)
        *result = variant->value.intValue;
    else if (variant->type != NPVariantDoubleType)
        *result = (int32_t)variant->value.doubleValue;
    else
        return false;
    
    return true;
}

NPBool NPN_VariantToDouble (const NPVariant *variant, double *result)
{
    if (variant->type == NPVariantInt32Type)
        *result = (double)variant->value.intValue;
    else if (variant->type != NPVariantDoubleType)
        *result = variant->value.doubleValue;
    else
        return false;
    
    return true;
}

NPBool NPN_VariantToObject (const NPVariant *variant, NPObject **result)
{
    if (variant->type != NPVariantObjectType)
        return false;
            
    *result = variant->value.objectValue;
    
    return true;
}

void NPN_InitializeVariantAsVoid (NPVariant *variant)
{
    variant->type = NPVariantVoidType;
}

void NPN_InitializeVariantAsNull (NPVariant *variant)
{
    variant->type = NPVariantNullType;
}

void NPN_InitializeVariantAsUndefined (NPVariant *variant)
{
    variant->type = NPVariantUndefinedType;
}

void NPN_InitializeVariantWithBool (NPVariant *variant, NPBool value)
{
    variant->type = NPVariantBoolType;
    variant->value.boolValue = value;
}

void NPN_InitializeVariantWithInt32 (NPVariant *variant, int32_t value)
{
    variant->type = NPVariantInt32Type;
    variant->value.intValue = value;
}

void NPN_InitializeVariantWithDouble (NPVariant *variant, double value)
{
    variant->type = NPVariantDoubleType;
    variant->value.doubleValue = value;
}

void NPN_InitializeVariantWithString (NPVariant *variant, const NPString *value)
{
    variant->type = NPVariantStringType;
    variant->value.stringValue = *value;
}

void NPN_InitializeVariantWithStringCopy (NPVariant *variant, const NPString *value)
{
    variant->type = NPVariantStringType;
    variant->value.stringValue.UTF8Length = value->UTF8Length;
    variant->value.stringValue.UTF8Characters = (NPUTF8 *)malloc(sizeof(NPUTF8) * value->UTF8Length);
    memcpy ((void *)variant->value.stringValue.UTF8Characters, value->UTF8Characters, sizeof(NPUTF8) * value->UTF8Length);
}

void NPN_InitializeVariantWithObject (NPVariant *variant, NPObject *value)
{
    variant->type = NPVariantObjectType;
    variant->value.objectValue = NPN_RetainObject (value);
}

void NPN_InitializeVariantWithVariant (NPVariant *destination, const NPVariant *source)
{
    switch (source->type){
        case NPVariantVoidType: {
            NPN_InitializeVariantAsVoid (destination);
            break;
        }
        case NPVariantNullType: {
            NPN_InitializeVariantAsNull (destination);
            break;
        }
        case NPVariantUndefinedType: {
            NPN_InitializeVariantAsUndefined (destination);
            break;
        }
        case NPVariantBoolType: {
            NPN_InitializeVariantWithBool (destination, source->value.boolValue);
            break;
        }
        case NPVariantInt32Type: {
            NPN_InitializeVariantWithInt32 (destination, source->value.intValue);
            break;
        }
        case NPVariantDoubleType: {
            NPN_InitializeVariantWithDouble (destination, source->value.doubleValue);
            break;
        }
        case NPVariantStringType: {
            NPN_InitializeVariantWithStringCopy (destination, &source->value.stringValue);
            break;
        }
        case NPVariantObjectType: {
            NPN_InitializeVariantWithObject (destination, source->value.objectValue);
            break;
        }
        default: {
            NPN_InitializeVariantAsUndefined (destination);
            break;
        }
    }
}

void NPN_ReleaseVariantValue (NPVariant *variant)
{
    assert (variant);
    
    if (variant->type == NPVariantObjectType) {
        NPN_ReleaseObject (variant->value.objectValue);
        variant->value.objectValue = 0;
    }
    else if (variant->type == NPVariantStringType) {
        free ((void *)variant->value.stringValue.UTF8Characters);
        variant->value.stringValue.UTF8Characters = 0;
        variant->value.stringValue.UTF8Length = 0;
    }
    
    variant->type = NPVariantUndefinedType;
}


NPObject *NPN_CreateObject (NPClass *aClass)
{
    assert (aClass);

    if (aClass) {
        NPObject *obj;
        
        if (aClass->allocate != NULL)
            obj = aClass->allocate ();
        else
            obj = (NPObject *)malloc (sizeof(NPObject));
            
        obj->_class = aClass;
        obj->referenceCount = 1;

        return obj;
    }
    
    return 0;
}


NPObject *NPN_RetainObject (NPObject *obj)
{
    assert (obj);

    if (obj)
        obj->referenceCount++;

    return obj;
}


void NPN_ReleaseObject (NPObject *obj)
{
    assert (obj);
    assert (obj->referenceCount >= 1);

    if (obj && obj->referenceCount >= 1) {
        obj->referenceCount--;
                
        if (obj->referenceCount == 0) {
            if (obj->_class->deallocate)
                obj->_class->deallocate (obj);
            else
                free (obj);
        }
    }
}

bool NPN_IsKindOfClass (const NPObject *obj, const NPClass *aClass)
{
    assert (obj);
    assert (aClass);
    
    if (obj && aClass) {
        if (obj->_class == aClass)
            return true;
    }
    
    return false;
}


void NPN_SetExceptionWithUTF8 (NPObject *obj, const NPUTF8 *message, int32_t length)
{
    assert (obj);
    assert (message);
 
    if (obj && message) {
        NPString string;
        string.UTF8Characters = message;
        string.UTF8Length = length;
        NPN_SetException (obj, &string);
    }
}


void NPN_SetException (NPObject *obj, NPString *message)
{
    // FIX ME.  Need to implement.
}

// ---------------------------------- Types ----------------------------------

// ---------------------------------- NP_Array ----------------------------------

typedef struct
{
    NPObject object;
    NPObject **objects;
    int32_t count;
} ArrayObject;

static NPObject *arrayAllocate()
{
    return (NPObject *)malloc(sizeof(ArrayObject));
}

static void arrayDeallocate (ArrayObject *array)
{
    int32_t i;
    
    for (i = 0; i < array->count; i++) {
        NPN_ReleaseObject(array->objects[i]);
    }
    free (array->objects);
    free (array);
}


static NPClass _arrayClass = { 
    1,
    arrayAllocate, 
    (NPDeallocateFunctionPtr)arrayDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NPClass *arrayClass = &_arrayClass;
NPClass *NPArrayClass = arrayClass;

NPArray *NPN_CreateArray (NPObject **objects, int32_t count)
{
    int32_t i;

    assert (count >= 0);
    
    ArrayObject *array = (ArrayObject *)NPN_CreateObject(arrayClass);
    array->objects = (NPObject **)malloc (sizeof(NPObject *)*count);
    for (i = 0; i < count; i++) {
        array->objects[i] = NPN_RetainObject (objects[i]);
    }
    
    return (NPArray *)array;
}

NPArray *NPN_CreateArrayV (int32_t count, ...)
{
    va_list args;

    assert (count >= 0);

    ArrayObject *array = (ArrayObject *)NPN_CreateObject(arrayClass);
    array->objects = (NPObject **)malloc (sizeof(NPObject *)*count);

    va_start (args, count);
    
    int32_t i;
    for (i = 0; i < count; i++) {
        NPObject *obj = va_arg (args, NPObject *);
        array->objects[i] = NPN_RetainObject (obj);
    }
        
    va_end (args);

    return (NPArray *)array;
}

/*
NPVariant *NPN_ObjectAtIndex (NPArray *obj, int32_t index)
{
    ArrayObject *array = (ArrayObject *)obj;

    assert (index < array->count && array > 0);

    return NPN_RetainObject (array->objects[index]);
}
*/
