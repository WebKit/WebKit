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

#include <NP_runtime.h>


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
static NP_Identifier identifierCount = 1;

NP_Identifier NP_IdentifierFromUTF8 (NP_UTF8 name)
{
    NP_Identifier identifier = 0;
    
    identifier = (NP_Identifier)CFDictionaryGetValue (getIdentifierDictionary(), (const void *)name);
    if (identifier == 0) {
        identifier = identifierCount++;
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
        
        identifierNames[identifier] = identifierName;

        CFDictionaryAddValue (getIdentifierDictionary(), identifierName, (const void *)identifier);
    }

    return identifier;
}

void NP_GetIdentifiers (NP_UTF8 *names, int nameCount, NP_Identifier *identifiers)
{
}

NP_UTF8 NP_UTF8FromIdentifier (NP_Identifier identifier)
{
    if (identifier == 0 || identifier >= identifierCount)
        return NULL;
        
    return (NP_UTF8)identifierNames[identifier];
}

NP_Object *NP_CreateObject (NP_Class *aClass)
{
    NP_Object *obj;
    
    if (aClass->allocate != NULL)
        obj = aClass->allocate ();
    else
        obj = (NP_Object *)malloc (sizeof(NP_Object));
        
    obj->_class = aClass;
    obj->referenceCount = 1;

    return obj;
}


NP_Object *NP_RetainObject (NP_Object *obj)
{
    obj->referenceCount++;

    return obj;
}


void NP_ReleaseObject (NP_Object *obj)
{
    assert (obj->referenceCount >= 1);

    obj->referenceCount--;
            
    if (obj->referenceCount == 0) {
        if (obj->_class->free)
            obj->_class->free (obj);
        else
            free (obj);
    }
}

bool NP_IsKindOfClass (NP_Object *obj, NP_Class *aClass)
{
    if (obj->_class == aClass)
        return true;

    return false;
}

void NP_SetException (NP_Object *obj,  NP_String *message)
{
}

// ---------------------------------- NP_JavaScriptObject ----------------------------------
NP_Object *NP_Call (NP_JavaScriptObject *obj, NP_Identifier methodName, NP_Object **args, unsigned argCount)
{
    return NULL;
}

NP_Object *NP_Evaluate (NP_JavaScriptObject *obj, NP_String *script)
{
    return NULL;
}

NP_Object *NP_GetProperty (NP_JavaScriptObject *obj, NP_Identifier  propertyName)
{
    return NULL;
}

void NP_SetProperty (NP_JavaScriptObject *obj, NP_Identifier  propertyName, NP_Object value)
{
}


void NP_RemoveProperty (NP_JavaScriptObject *obj, NP_Identifier propertyName)
{
}

NP_UTF8 NP_ToString (NP_JavaScriptObject *obj)
{
    return NULL;
}

NP_Object *NP_GetPropertyAtIndex (NP_JavaScriptObject *obj, unsigned int index)
{
    return NULL;
}

void NP_SetPropertyAtIndex (NP_JavaScriptObject *obj, unsigned index, NP_Object value)
{
}


// ---------------------------------- Types ----------------------------------

// ---------------------------------- NP_Number ----------------------------------

typedef struct
{
    NP_Object object;
    double number;
} NumberObject;

NP_Object *numberCreate()
{
    return (NP_Object *)malloc(sizeof(NumberObject));
}

static NP_Class _numberClass = { 
    1,
    numberCreate, 
    (NP_FreeInterface)free, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *numberClass = &_numberClass;

NP_Number *NP_CreateNumberWithInt (int i)
{
    NumberObject *number = (NumberObject *)NP_CreateObject (numberClass);
    number->number = i;
    return (NP_Number *)number;
}

NP_Number *NP_CreateNumberWithFloat (float f)
{
    NumberObject *number = (NumberObject *)NP_CreateObject (numberClass);
    number->number = f;
    return (NP_Number *)number;
}

NP_Number *NP_CreateNumberWithDouble (double d)
{
    NumberObject *number = (NumberObject *)NP_CreateObject (numberClass);
    number->number = d;
    return (NP_Number *)number;
}

int NP_IntFromNumber (NP_Number *obj)
{
    assert (NP_IsKindOfClass (obj, numberClass));
    NumberObject *number = (NumberObject *)obj;
    return (int)number->number;
}

float NP_FloatFromNumber (NP_Number *obj)
{
    assert (NP_IsKindOfClass (obj, numberClass));
    NumberObject *number = (NumberObject *)obj;
    return (float)number->number;
}

double NP_DoubleFromNumber (NP_Number *obj)
{
    assert (NP_IsKindOfClass (obj, numberClass));
    NumberObject *number = (NumberObject *)obj;
    return number->number;
}


NP_String *NP_CreateStringWithUTF8 (NP_UTF8 utf8String)
{
    return NULL;
}

NP_String *NP_CreateStringWithUTF16 (NP_UTF16 utf16String)
{
    return NULL;
}


NP_UTF8 NP_UTF8FromString (NP_String *obj)
{
    return NULL;
}

NP_UTF16 NP_UTF16FromString (NP_String *obj)
{
    return NULL;
}

NP_Boolean *NP_CreateBoolean (bool f)
{
    return NULL;
}

bool NP_BoolFromBoolean (NP_Boolean *aBool)
{
    return true;
}

NP_Null *NP_GetNull()
{
    return NULL;
}

NP_Undefined *NP_GetUndefined()
{
    return NULL;
}

NP_Array *NP_CreateArray (const NP_Object **, unsigned count)
{
    return NULL;
}

NP_Array *NP_CreateArrayV (unsigned count, ...)
{
    return NULL;
}

NP_Object *NP_ObjectAtIndex (NP_Array *array, unsigned index)
{
    return NULL;
}

