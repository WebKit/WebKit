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



NP_Identifier NP_IdentifierFromUTF8 (NP_UTF8 name)
{
    return 0;
}

void NP_GetIdentifiers (NP_UTF8 *names, int nameCount, NP_Identifier *identifiers)
{
}

NP_UTF8 NP_UTF8FromIdentifier (NP_Identifier identifier)
{
    return NULL;
}

NP_Object *NP_CreateObject (NP_Class *aClass)
{
    if (aClass->create != NULL)
        return aClass->create ();

    NP_Object *obj = (NP_Object *)malloc (sizeof(NP_Object));
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
        if (obj->_class->destroy)
            obj->_class->destroy (obj);
        else
            free (obj);
    }
}

bool NP_IsKindOfClass (NP_Object *obj, NP_Class *aClass)
{
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


NP_Number *NP_CreateNumberWithInt (int i)
{
    return NULL;
}

NP_Number *NP_CreateNumberWithFloat (float f)
{
    return NULL;
}

NP_Number *NP_CreateNumberWithDouble (double d)
{
    return NULL;
}

int NP_IntFromNumber (NP_Number *obj)
{
    return 0;
}

float NP_FloatFromNumber (NP_Number *obj)
{
    return 0.;
}

double NP_DoubleFromNumber (NP_Number *obj)
{
    return 0.;
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

