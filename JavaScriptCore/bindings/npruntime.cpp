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
        if (obj->_class->deallocate)
            obj->_class->deallocate (obj);
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

// ---------------------------------- Types ----------------------------------

// ---------------------------------- NP_Number ----------------------------------

typedef struct
{
    NP_Object object;
    double number;
} NumberObject;

static NP_Object *numberAllocate()
{
    return (NP_Object *)malloc(sizeof(NumberObject));
}

static NP_Class _numberClass = { 
    1,
    numberAllocate, 
    (NP_DeallocateInterface)free, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *numberClass = &_numberClass;
NP_Class *NP_NumberClass = numberClass;

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


// ---------------------------------- NP_String ----------------------------------

typedef struct
{
    NP_Object object;
    NP_UTF16 string;
    int32_t length;
} StringObject;

static NP_Object *stringAllocate()
{
    return (NP_Object *)malloc(sizeof(StringObject));
}

void stringDeallocate (StringObject *string)
{
    free (string->string);
    free (string);
}

static NP_Class _stringClass = { 
    1,
    stringAllocate, 
    (NP_DeallocateInterface)stringDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *stringClass = &_stringClass;
NP_Class *NP_StringClass = stringClass;

#define LOCAL_CONVERSION_BUFFER_SIZE    4096

NP_String *NP_CreateStringWithUTF8 (NP_UTF8 utf8String)
{
    StringObject *string = (StringObject *)NP_CreateObject (stringClass);

    CFStringRef stringRef = CFStringCreateWithCString (NULL, utf8String, kCFStringEncodingUTF8);

    string->length = CFStringGetLength (stringRef);
    string->string = (NP_UTF16)malloc(sizeof(int16_t)*string->length);

    // Convert the string to UTF16.
    CFRange range = { 0, string->length };
    CFStringGetCharacters (stringRef, range, (UniChar *)string->string);
    CFRelease (stringRef);

    return (NP_String *)string;
}


NP_String *NP_CreateStringWithUTF16 (NP_UTF16 utf16String, int32_t len)
{
    StringObject *string = (StringObject *)NP_CreateObject (stringClass);

    string->length = len;
    string->string = (NP_UTF16)malloc(sizeof(int16_t)*string->length);
    memcpy (string->string, utf16String, sizeof(int16_t)*string->length);
    
    return (NP_String *)string;
}

NP_UTF8 NP_UTF8FromString (NP_String *obj)
{
    assert (NP_IsKindOfClass (obj, stringClass));

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
    
    NP_UTF8 resultString = (NP_UTF8)malloc (usedBufferLength+1);
    strncpy (resultString, (const char *)buffer, usedBufferLength);
    resultString[usedBufferLength] = 0;
    
    CFRelease (stringRef);
    if (buffer != _localBuffer)
        free ((void *)buffer);
        
    return resultString;
}

NP_UTF16 NP_UTF16FromString (NP_String *obj)
{
    assert (NP_IsKindOfClass (obj, stringClass));

    StringObject *string = (StringObject *)obj;
    
    NP_UTF16 resultString = (NP_UTF16)malloc(sizeof(int16_t)*string->length);
    memcpy (resultString, string->string, sizeof(int16_t)*string->length);

    return resultString;
}

int32_t NP_StringLength (NP_String *obj)
{
    assert (NP_IsKindOfClass (obj, stringClass));

    StringObject *string = (StringObject *)obj;
    return string->length;
}

// ---------------------------------- NP_Boolean ----------------------------------

typedef struct
{
    NP_Object object;
} BooleanObject;

static NP_Object *booleanAllocate()
{
    return (NP_Object *)malloc(sizeof(BooleanObject));
}

static void booleanDeallocate (BooleanObject *string)
{
    // Do nothing, single true and false instances.
}

static NP_Class _booleanClass = { 
    1,
    booleanAllocate, 
    (NP_DeallocateInterface)booleanDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static BooleanObject *theTrueObject = 0;
static BooleanObject *theFalseObject = 0;

static NP_Class *booleanClass = &_booleanClass;
NP_Class *NP_BooleanClass = booleanClass;

NP_Boolean *NP_CreateBoolean (bool f)
{
    if (f) {
        if (!theTrueObject) {
            theTrueObject = (BooleanObject *)NP_CreateObject (booleanClass);
        }
        return (NP_Boolean *)theTrueObject;
    }

    // False
    if (!theFalseObject) {
        theFalseObject = (BooleanObject *)NP_CreateObject (booleanClass);
    }
    return (NP_Boolean *)theFalseObject;
}

bool NP_BoolFromBoolean (NP_Boolean *obj)
{
    assert (NP_IsKindOfClass (obj, booleanClass) 
            && ((BooleanObject *)obj == theTrueObject || (BooleanObject *)obj == theFalseObject));

    BooleanObject *booleanObj = (BooleanObject *)obj;
    if (booleanObj == theTrueObject)
        return true;
    return false;
}

// ---------------------------------- NP_Null ----------------------------------

typedef struct
{
    NP_Object object;
} NullObject;

static NP_Object *nullAllocate()
{
    return (NP_Object *)malloc(sizeof(NullObject));
}

static void nullDeallocate (StringObject *string)
{
    // Do nothing, the null object is a singleton.
}


static NullObject *theNullObject = 0;

static NP_Class _nullClass = { 
    1,
    nullAllocate, 
    (NP_DeallocateInterface)nullDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *nullClass = &_nullClass;
NP_Class *NP_NullClass = nullClass;

NP_Null *NP_GetNull()
{
    if (!theNullObject)
        theNullObject = (NullObject *)NP_CreateObject(nullClass);
    return (NP_Null *)theNullObject;
}


// ---------------------------------- NP_Undefined ----------------------------------

typedef struct
{
    NP_Object object;
} UndefinedObject;

static NP_Object *undefinedAllocate()
{
    return (NP_Object *)malloc(sizeof(UndefinedObject));
}

static void undefinedDeallocate (StringObject *string)
{
    // Do nothing, the null object is a singleton.
}


static NullObject *theUndefinedObject = 0;

static NP_Class _undefinedClass = { 
    1,
    undefinedAllocate, 
    (NP_DeallocateInterface)undefinedDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *undefinedClass = &_undefinedClass;
NP_Class *NP_UndefinedClass = undefinedClass;

NP_Undefined *NP_GetUndefined()
{
    if (!theUndefinedObject)
        theUndefinedObject = (NullObject *)NP_CreateObject(undefinedClass);
    return (NP_Undefined *)theUndefinedObject;
}

// ---------------------------------- NP_Array ----------------------------------

typedef struct
{
    NP_Object object;
    NP_Object **objects;
    int32_t count;
} ArrayObject;

static NP_Object *arrayAllocate()
{
    return (NP_Object *)malloc(sizeof(ArrayObject));
}

static void arrayDeallocate (ArrayObject *array)
{
    int32_t i;
    
    for (i = 0; i < array->count; i++) {
        NP_ReleaseObject(array->objects[i]);
    }
    free (array->objects);
    free (array);
}


static NP_Class _arrayClass = { 
    1,
    arrayAllocate, 
    (NP_DeallocateInterface)arrayDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *arrayClass = &_arrayClass;
NP_Class *NP_ArrayClass = arrayClass;

NP_Array *NP_CreateArray (NP_Object **objects, int32_t count)
{
    int32_t i;

    assert (count >= 0);
    
    ArrayObject *array = (ArrayObject *)NP_CreateObject(arrayClass);
    array->objects = (NP_Object **)malloc (sizeof(NP_Object *)*count);
    for (i = 0; i < count; i++) {
        array->objects[i] = NP_RetainObject (objects[i]);
    }
    
    return (NP_Array *)array;
}

NP_Array *NP_CreateArrayV (int32_t count, ...)
{
    va_list args;

    assert (count >= 0);

    ArrayObject *array = (ArrayObject *)NP_CreateObject(arrayClass);
    array->objects = (NP_Object **)malloc (sizeof(NP_Object *)*count);

    va_start (args, count);
    
    int32_t i;
    for (i = 0; i < count; i++) {
        NP_Object *obj = va_arg (args, NP_Object *);
        array->objects[i] = NP_RetainObject (obj);
    }
        
    va_end (args);

    return (NP_Array *)array;
}

NP_Object *NP_ObjectAtIndex (NP_Array *obj, int32_t index)
{
    ArrayObject *array = (ArrayObject *)obj;

    assert (index < array->count && array > 0);

    return NP_RetainObject (array->objects[index]);
}

