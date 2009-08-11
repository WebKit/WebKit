/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "TestObject.h"
#include "PluginObject.h"

#include <string.h>
#include <stdlib.h>

static bool testEnumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);
static bool testHasMethod(NPObject*, NPIdentifier name);
static bool testInvoke(NPObject*, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool testHasProperty(NPObject*, NPIdentifier name);
static bool testGetProperty(NPObject*, NPIdentifier name, NPVariant*);
static NPObject *testAllocate(NPP npp, NPClass *theClass);
static void testDeallocate(NPObject *obj);
static bool testConstruct(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result);

static NPClass testClass = { 
    NP_CLASS_STRUCT_VERSION,
    testAllocate, 
    testDeallocate, 
    0,
    testHasMethod,
    testInvoke,
    0,
    testHasProperty,
    testGetProperty,
    0,
    0,
    testEnumerate,
    testConstruct
};

NPClass *getTestClass(void)
{
    return &testClass;
}

static bool identifiersInitialized = false;

#define ID_OBJECT_POINTER 2

#define NUM_ENUMERATABLE_TEST_IDENTIFIERS 2
#define NUM_TEST_IDENTIFIERS 3

static NPIdentifier testIdentifiers[NUM_TEST_IDENTIFIERS];
static const NPUTF8 *testIdentifierNames[NUM_TEST_IDENTIFIERS] = {
    "foo",
    "bar",
    "objectPointer",
};

#define ID_THROW_EXCEPTION_METHOD   0
#define NUM_METHOD_IDENTIFIERS      1

static NPIdentifier testMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *testMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
    "throwException",
};

static void initializeIdentifiers(void)
{
    browser->getstringidentifiers(testIdentifierNames, NUM_TEST_IDENTIFIERS, testIdentifiers);
    browser->getstringidentifiers(testMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, testMethodIdentifiers);
}

static NPObject *testAllocate(NPP /*npp*/, NPClass* /*theClass*/)
{
    NPObject *newInstance = static_cast<NPObject*>(malloc(sizeof(NPObject)));
    
    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }
    
    return newInstance;
}

static void testDeallocate(NPObject *obj) 
{
    free(obj);
}

static bool testHasMethod(NPObject*, NPIdentifier name)
{
    for (unsigned i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
        if (testMethodIdentifiers[i] == name)
            return true;
    }
    return false;
}

static bool testInvoke(NPObject* header, NPIdentifier name, const NPVariant* /*args*/, uint32_t /*argCount*/, NPVariant* /*result*/)
{
    if (name == testMethodIdentifiers[ID_THROW_EXCEPTION_METHOD]) {
        browser->setexception(header, "test object throwException SUCCESS");
        return true;
     }
     return false;
}

static bool testHasProperty(NPObject*, NPIdentifier name)
{
    for (unsigned i = 0; i < NUM_TEST_IDENTIFIERS; i++) {
        if (testIdentifiers[i] == name)
            return true;
    }
    
    return false;
}

static bool testGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
    if (name == testIdentifiers[ID_OBJECT_POINTER]) {
        int32_t objectPointer = static_cast<int32_t>(reinterpret_cast<long long>(npobj));

        INT32_TO_NPVARIANT(objectPointer, *result);
        return true;
    }
    
    return false;
}


static bool testEnumerate(NPObject* /*npobj*/, NPIdentifier **value, uint32_t *count)
{
    *count = NUM_ENUMERATABLE_TEST_IDENTIFIERS;
    
    *value = (NPIdentifier*)browser->memalloc(NUM_ENUMERATABLE_TEST_IDENTIFIERS * sizeof(NPIdentifier));
    memcpy(*value, testIdentifiers, sizeof(NPIdentifier) * NUM_ENUMERATABLE_TEST_IDENTIFIERS);
    
    return true;
}

static bool testConstruct(NPObject* npobj, const NPVariant* /*args*/, uint32_t /*argCount*/, NPVariant* result)
{
    browser->retainobject(npobj);
    
    // Just return the same object.
    OBJECT_TO_NPVARIANT(npobj, *result);
    return true;
}


