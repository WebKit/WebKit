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
static NPIdentifier identifierCount = 1;

NPIdentifier NPN_IdentifierFromUTF8 (const NPUTF8 *name)
{
    assert (name);
    
    if (name) {
        NPIdentifier identifier = 0;
        
        identifier = (NPIdentifier)CFDictionaryGetValue (getIdentifierDictionary(), (const void *)name);
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
            identifiers[i] = NPN_IdentifierFromUTF8 (names[i]);
        }
    }
}

const NPUTF8 *NPN_UTF8FromIdentifier (NPIdentifier identifier)
{
    if (identifier == 0 || identifier >= identifierCount)
        return NULL;
        
    return (const NPUTF8 *)identifierNames[identifier];
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
        NPString *m = NPN_CreateStringWithUTF8(message, length);
        NPN_SetException (obj, m);
        NPN_ReleaseObject (m);
    }
}


void NPN_SetException (NPObject *obj, NPString *message)
{
    // FIX ME.  Need to implement.
}

// ---------------------------------- Types ----------------------------------

// ---------------------------------- NPNumber ----------------------------------

typedef struct
{
    NPObject object;
    double number;
} NumberObject;

static NPObject *numberAllocate()
{
    return (NPObject *)malloc(sizeof(NumberObject));
}

static NPClass _numberClass = { 
    1,
    numberAllocate, 
    (NPDeallocateFunctionPtr)free, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NPClass *numberClass = &_numberClass;
NPClass *NPNumberClass = numberClass;

NPNumber *NPN_CreateNumberWithInt (int i)
{
    NumberObject *number = (NumberObject *)NPN_CreateObject (numberClass);
    number->number = i;
    return (NPNumber *)number;
}

NPNumber *NPN_CreateNumberWithFloat (float f)
{
    NumberObject *number = (NumberObject *)NPN_CreateObject (numberClass);
    number->number = f;
    return (NPNumber *)number;
}

NPNumber *NPN_CreateNumberWithDouble (double d)
{
    NumberObject *number = (NumberObject *)NPN_CreateObject (numberClass);
    number->number = d;
    return (NPNumber *)number;
}

int NPN_IntFromNumber (NPNumber *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, numberClass));

    if (obj && NPN_IsKindOfClass (obj, numberClass)) {
        NumberObject *number = (NumberObject *)obj;
        return (int)number->number;
    }
    
    return 0;
}

float NPN_FloatFromNumber (NPNumber *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, numberClass));

    if (obj && NPN_IsKindOfClass (obj, numberClass)) {
        NumberObject *number = (NumberObject *)obj;
        return (float)number->number;
    }
    
    return 0.;
}

double NPN_DoubleFromNumber (NPNumber *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, numberClass));
    
    if (obj && NPN_IsKindOfClass (obj, numberClass)) {
        NumberObject *number = (NumberObject *)obj;
        return number->number;
    }
    
    return 0.;
}


// ---------------------------------- NPString ----------------------------------

typedef struct
{
    NPObject object;
    NPUTF16 *string;
    int32_t length;
} StringObject;

static NPObject *stringAllocate()
{
    return (NPObject *)malloc(sizeof(StringObject));
}

void stringDeallocate (StringObject *string)
{
    free (string->string);
    free (string);
}

static NPClass _stringClass = { 
    1,
    stringAllocate, 
    (NPDeallocateFunctionPtr)stringDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NPClass *stringClass = &_stringClass;
NPClass *NPStringClass = stringClass;

#define LOCAL_CONVERSION_BUFFER_SIZE    4096

NPString *NPN_CreateStringWithUTF8 (const NPUTF8 *utf8String, int32_t length)
{
    assert (utf8String);
    
    if (utf8String) {
        if (length == -1)
            length = strlen(utf8String);
            
        StringObject *string = (StringObject *)NPN_CreateObject (stringClass);

        CFStringRef stringRef = CFStringCreateWithBytes (NULL, (const UInt8*)utf8String, (CFIndex)length, kCFStringEncodingUTF8, false);

        string->length = CFStringGetLength (stringRef);
        string->string = (NPUTF16 *)malloc(sizeof(NPUTF16)*string->length);

        // Convert the string to UTF16.
        CFRange range = { 0, string->length };
        CFStringGetCharacters (stringRef, range, (UniChar *)string->string);
        CFRelease (stringRef);

        return (NPString *)string;
    }
    
    return 0;
}


NPString *NPN_CreateStringWithUTF16 (const NPUTF16 *utf16String, int32_t len)
{
    assert (utf16String);
    
    if (utf16String) {
        StringObject *string = (StringObject *)NPN_CreateObject (stringClass);

        string->length = len;
        string->string = (NPUTF16 *)malloc(sizeof(NPUTF16)*string->length);
        memcpy ((void *)string->string, utf16String, sizeof(NPUTF16)*string->length);
        
        return (NPString *)string;
    }

    return 0;
}

void NPN_DeallocateUTF8 (NPUTF8 *UTF8Buffer)
{
    free (UTF8Buffer);
}

NPUTF8 *NPN_UTF8FromString (NPString *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, stringClass));

    if (obj && NPN_IsKindOfClass (obj, stringClass)) {
        StringObject *string = (StringObject *)obj;

        // Allow for max conversion factor.
        UInt8 *buffer;
        UInt8 _localBuffer[LOCAL_CONVERSION_BUFFER_SIZE];
        CFIndex maxBufferLength;
        
        if (string->length*sizeof(UInt8)*8 > LOCAL_CONVERSION_BUFFER_SIZE) {
            maxBufferLength = string->length*sizeof(UInt8)*8;
            buffer = (UInt8 *)malloc(maxBufferLength);
        }
        else {
            maxBufferLength = LOCAL_CONVERSION_BUFFER_SIZE;
            buffer = _localBuffer;
        }
        
        // Convert the string to UTF8.
        CFIndex usedBufferLength;
        CFStringRef stringRef = CFStringCreateWithCharacters (NULL, (UniChar *)string->string, string->length);
        CFRange range = { 0, string->length };
        CFStringGetBytes (stringRef, range, kCFStringEncodingUTF8, 0, false, buffer, maxBufferLength, &usedBufferLength);
        
        NPUTF8 *resultString = (NPUTF8 *)malloc (usedBufferLength+1);
        strncpy ((char *)resultString, (const char *)buffer, usedBufferLength);
        char *cp = (char *)resultString;
        cp[usedBufferLength] = 0;
        
        CFRelease (stringRef);
        if (buffer != _localBuffer)
            free ((void *)buffer);
            
        return resultString;
    }
    
    return 0;
}

NPUTF16 *NPN_UTF16FromString (NPString *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, stringClass));

    if (obj && NPN_IsKindOfClass (obj, stringClass)) {
        StringObject *string = (StringObject *)obj;
        
        NPUTF16 *resultString = (NPUTF16*)malloc(sizeof(int16_t)*string->length);
        memcpy ((void *)resultString, string->string, sizeof(int16_t)*string->length);

        return resultString;
    }
    
    return 0;
}

int32_t NPN_StringLength (NPString *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, stringClass));

    if (obj && NPN_IsKindOfClass (obj, stringClass)) {
        StringObject *string = (StringObject *)obj;
        return string->length;
    }
    
    return 0;
}

// ---------------------------------- NP_Boolean ----------------------------------

typedef struct
{
    NPObject object;
} BooleanObject;

static NPObject *booleanAllocate()
{
    return (NPObject *)malloc(sizeof(BooleanObject));
}

static void booleanDeallocate (BooleanObject *string)
{
    // Do nothing, single true and false instances.
}

static NPClass _booleanClass = { 
    1,
    booleanAllocate, 
    (NPDeallocateFunctionPtr)booleanDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static BooleanObject *theTrueObject = 0;
static BooleanObject *theFalseObject = 0;

static NPClass *booleanClass = &_booleanClass;
NPClass *NPBooleanClass = booleanClass;

NPBoolean *NPN_CreateBoolean (bool f)
{
    if (f) {
        if (!theTrueObject) {
            theTrueObject = (BooleanObject *)NPN_CreateObject (booleanClass);
        }
        return (NPBoolean *)theTrueObject;
    }

    // False
    if (!theFalseObject) {
        theFalseObject = (BooleanObject *)NPN_CreateObject (booleanClass);
    }
    return (NPBoolean *)theFalseObject;
}

bool NPN_BoolFromBoolean (NPBoolean *obj)
{
    assert (obj && NPN_IsKindOfClass (obj, booleanClass) 
            && ((BooleanObject *)obj == theTrueObject || (BooleanObject *)obj == theFalseObject));

    if (obj && NPN_IsKindOfClass (obj, booleanClass) 
            && ((BooleanObject *)obj == theTrueObject || (BooleanObject *)obj == theFalseObject)) {
        BooleanObject *booleanObj = (BooleanObject *)obj;
        if (booleanObj == theTrueObject)
            return true;
    }
    
    return false;
}

// ---------------------------------- NP_Null ----------------------------------

typedef struct
{
    NPObject object;
} NullObject;

static NPObject *nullAllocate()
{
    return (NPObject *)malloc(sizeof(NullObject));
}

static void nullDeallocate (StringObject *string)
{
    // Do nothing, the null object is a singleton.
}


static NullObject *theNullObject = 0;

static NPClass _nullClass = { 
    1,
    nullAllocate, 
    (NPDeallocateFunctionPtr)nullDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NPClass *nullClass = &_nullClass;
NPClass *NPNullClass = nullClass;

NPNull *NPN_GetNull()
{
    if (!theNullObject)
        theNullObject = (NullObject *)NPN_CreateObject(nullClass);
    return (NPNull *)theNullObject;
}


// ---------------------------------- NP_Undefined ----------------------------------

typedef struct
{
    NPObject object;
} UndefinedObject;

static NPObject *undefinedAllocate()
{
    return (NPObject *)malloc(sizeof(UndefinedObject));
}

static void undefinedDeallocate (StringObject *string)
{
    // Do nothing, the null object is a singleton.
}


static NullObject *theUndefinedObject = 0;

static NPClass _undefinedClass = { 
    1,
    undefinedAllocate, 
    (NPDeallocateFunctionPtr)undefinedDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NPClass *undefinedClass = &_undefinedClass;
NPClass *NPUndefinedClass = undefinedClass;

NPUndefined *NPN_GetUndefined()
{
    if (!theUndefinedObject)
        theUndefinedObject = (NullObject *)NPN_CreateObject(undefinedClass);
    return (NPUndefined *)theUndefinedObject;
}

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

NPObject *NPN_ObjectAtIndex (NPArray *obj, int32_t index)
{
    ArrayObject *array = (ArrayObject *)obj;

    assert (index < array->count && array > 0);

    return NPN_RetainObject (array->objects[index]);
}

