/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "PluginObject.h"

#include "TestObject.h"
#include <assert.h>
#include <stdio.h>

static void pluginInvalidate(NPObject *obj);
static bool pluginHasProperty(NPObject *obj, NPIdentifier name);
static bool pluginHasMethod(NPObject *obj, NPIdentifier name);
static bool pluginGetProperty(NPObject *obj, NPIdentifier name, NPVariant *variant);
static bool pluginSetProperty(NPObject *obj, NPIdentifier name, const NPVariant *variant);
static bool pluginInvoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
static bool pluginInvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
static NPObject *pluginAllocate(NPP npp, NPClass *theClass);
static void pluginDeallocate(NPObject *obj);

NPNetscapeFuncs *browser;

static NPClass pluginClass = { 
    NP_CLASS_STRUCT_VERSION,
    pluginAllocate, 
    pluginDeallocate, 
    pluginInvalidate,
    pluginHasMethod,
    pluginInvoke,
    pluginInvokeDefault,
    pluginHasProperty,
    pluginGetProperty,
    pluginSetProperty,
};
 
NPClass *getPluginClass(void)
{
    return &pluginClass;
}

static bool identifiersInitialized = false;

#define ID_PROPERTY_PROPERTY        0
#define ID_PROPERTY_EVENT_LOGGING   1
#define ID_PROPERTY_HAS_STREAM      2
#define ID_PROPERTY_TEST_OBJECT     3
#define ID_PROPERTY_LOG_DESTROY     4
#define NUM_PROPERTY_IDENTIFIERS    5

static NPIdentifier pluginPropertyIdentifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8 *pluginPropertyIdentifierNames[NUM_PROPERTY_IDENTIFIERS] = {
    "property",
    "eventLoggingEnabled",
    "hasStream",
    "testObject",
    "logDestroy",
};

#define ID_TEST_CALLBACK_METHOD     0
#define ID_TEST_GETURL              1
#define ID_REMOVE_DEFAULT_METHOD    2
#define ID_TEST_DOM_ACCESS          3
#define ID_TEST_GET_URL_NOTIFY      4
#define ID_TEST_INVOKE_DEFAULT      5
#define ID_DESTROY_STREAM           6
#define ID_TEST_ENUMERATE           7
#define ID_TEST_GETINTIDENTIFIER    8
#define ID_TEST_GET_PROPERTY        9
#define ID_TEST_EVALUATE            10
#define ID_TEST_GET_PROPERTY_RETURN_VALUE 11
#define NUM_METHOD_IDENTIFIERS      12

static NPIdentifier pluginMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *pluginMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
    "testCallback",
    "getURL",
    "removeDefaultMethod",
    "testDOMAccess",
    "getURLNotify",
    "testInvokeDefault",
    "destroyStream",
    "testEnumerate",
    "testGetIntIdentifier",
    "testGetProperty",
    "testEvaluate",
    "testGetPropertyReturnValue",
};

static NPUTF8* createCStringFromNPVariant(const NPVariant *variant)
{
    size_t length = NPVARIANT_TO_STRING(*variant).UTF8Length;
    NPUTF8* result = (NPUTF8*)malloc(length + 1);
    memcpy(result, NPVARIANT_TO_STRING(*variant).UTF8Characters, length);
    result[length] = '\0';
    return result;
}

static void initializeIdentifiers(void)
{
    browser->getstringidentifiers(pluginPropertyIdentifierNames, NUM_PROPERTY_IDENTIFIERS, pluginPropertyIdentifiers);
    browser->getstringidentifiers(pluginMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, pluginMethodIdentifiers);
}

static bool pluginHasProperty(NPObject *obj, NPIdentifier name)
{
    for (int i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++)
        if (name == pluginPropertyIdentifiers[i])
            return true;
    return false;
}

static bool pluginHasMethod(NPObject *obj, NPIdentifier name)
{
    for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++)
        if (name == pluginMethodIdentifiers[i])
            return true;
    return false;
}

static bool pluginGetProperty(NPObject *obj, NPIdentifier name, NPVariant *variant)
{
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_PROPERTY]) {
        STRINGZ_TO_NPVARIANT("property", *variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_EVENT_LOGGING]) {
        BOOLEAN_TO_NPVARIANT(((PluginObject *)obj)->eventLogging, *variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_LOG_DESTROY]) {
        BOOLEAN_TO_NPVARIANT(((PluginObject *)obj)->logDestroy, *variant);
        return true;            
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_HAS_STREAM]) {
        BOOLEAN_TO_NPVARIANT(((PluginObject *)obj)->stream != 0, *variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_TEST_OBJECT]) {
        NPObject *testObject = ((PluginObject *)obj)->testObject;
        browser->retainobject(testObject);
        OBJECT_TO_NPVARIANT(testObject, *variant);
        return true;
    }
    return false;
}

static bool pluginSetProperty(NPObject *obj, NPIdentifier name, const NPVariant *variant)
{
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_EVENT_LOGGING]) {
        ((PluginObject *)obj)->eventLogging = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_LOG_DESTROY]) {
        ((PluginObject *)obj)->logDestroy = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    }
    
    return false;
}

static void testDOMAccess(PluginObject *obj)
{
    // Get plug-in's DOM element
    NPObject *elementObject;
    if (browser->getvalue(obj->npp, NPNVPluginElementNPObject, &elementObject) == NPERR_NO_ERROR) {
        // Get style
        NPVariant styleVariant;
        NPIdentifier styleIdentifier = browser->getstringidentifier("style");
        if (browser->getproperty(obj->npp, elementObject, styleIdentifier, &styleVariant) && NPVARIANT_IS_OBJECT(styleVariant)) {
            // Set style.border
            NPIdentifier borderIdentifier = browser->getstringidentifier("border");
            NPVariant borderVariant;
            STRINGZ_TO_NPVARIANT("3px solid red", borderVariant);
            browser->setproperty(obj->npp, NPVARIANT_TO_OBJECT(styleVariant), borderIdentifier, &borderVariant);
            browser->releasevariantvalue(&styleVariant);
        }
        
        browser->releaseobject(elementObject);
    }
}

static bool pluginInvoke(NPObject *header, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    PluginObject *obj = (PluginObject *)header;
    if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_METHOD]) {
        // call whatever method name we're given
        if (argCount > 0 && NPVARIANT_IS_STRING(args[0])) {
            NPObject *windowScriptObject;
            browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

            NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
            NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
            free(callbackString);

            NPVariant browserResult;
            browser->invoke(obj->npp, windowScriptObject, callbackIdentifier, 0, 0, &browserResult);
            browser->releasevariantvalue(&browserResult);

            VOID_TO_NPVARIANT(*result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_GETURL]) {
        if (argCount == 2 && NPVARIANT_IS_STRING(args[0]) && NPVARIANT_IS_STRING(args[1])) {
            NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
            NPUTF8* targetString = createCStringFromNPVariant(&args[1]);
            browser->geturl(obj->npp, urlString, targetString);
            free(urlString);
            free(targetString);

            VOID_TO_NPVARIANT(*result);
            return true;
        } else if (argCount == 1 && NPVARIANT_IS_STRING(args[0])) {
            NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
            browser->geturl(obj->npp, urlString, 0);
            free(urlString);

            VOID_TO_NPVARIANT(*result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_REMOVE_DEFAULT_METHOD]) {
        pluginClass.invokeDefault = 0;
        VOID_TO_NPVARIANT(*result);
        return true;
    } else if (name == pluginMethodIdentifiers[ID_TEST_DOM_ACCESS]) {
        testDOMAccess(obj);
        VOID_TO_NPVARIANT(*result);
        return true;
    } else if (name == pluginMethodIdentifiers[ID_TEST_GET_URL_NOTIFY]) {
        if (argCount == 3
          && NPVARIANT_IS_STRING(args[0])
          && (NPVARIANT_IS_STRING(args[1]) || NPVARIANT_IS_NULL(args[1]))
          && NPVARIANT_IS_STRING(args[2])) {
            NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
            NPUTF8* targetString = (NPVARIANT_IS_STRING(args[1]) ? createCStringFromNPVariant(&args[1]) : NULL);
            NPUTF8* callbackString = createCStringFromNPVariant(&args[2]);
            
            NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
            browser->geturlnotify(obj->npp, urlString, targetString, callbackIdentifier);

            free(urlString);
            free(targetString);
            free(callbackString);
            
            VOID_TO_NPVARIANT(*result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_INVOKE_DEFAULT] && NPVARIANT_IS_OBJECT(args[0])) {
        NPObject *callback = NPVARIANT_TO_OBJECT(args[0]);
        
        NPVariant args[1];
        NPVariant browserResult;
        
        STRINGZ_TO_NPVARIANT("test", args[0]);
        bool retval = browser->invokeDefault(obj->npp, callback, args, 1, &browserResult);
        
        if (retval)
            browser->releasevariantvalue(&browserResult);
        
        BOOLEAN_TO_NPVARIANT(retval, *result);
        return true;
    } else if (name == pluginMethodIdentifiers[ID_TEST_ENUMERATE]) {
        if (argCount == 2 && NPVARIANT_IS_OBJECT(args[0]) && NPVARIANT_IS_OBJECT(args[1])) {
            uint32_t count;            
            NPIdentifier* identifiers;

            if (browser->enumerate(obj->npp, NPVARIANT_TO_OBJECT(args[0]), &identifiers, &count)) {
                NPObject* outArray = NPVARIANT_TO_OBJECT(args[1]);
                NPIdentifier pushIdentifier = browser->getstringidentifier("push");
                
                for (uint32_t i = 0; i < count; i++) {
                    NPUTF8* string = browser->utf8fromidentifier(identifiers[i]);
                    
                    if (!string)
                        continue;
                                        
                    NPVariant args[1];
                    STRINGZ_TO_NPVARIANT(string, args[0]);
                    NPVariant browserResult;
                    browser->invoke(obj->npp, outArray, pushIdentifier, args, 1, &browserResult);
                    browser->releasevariantvalue(&browserResult);
                    browser->memfree(string);
                }
                
                browser->memfree(identifiers);
            }
            
            VOID_TO_NPVARIANT(*result);
            return true;            
        }
    } else if (name == pluginMethodIdentifiers[ID_DESTROY_STREAM]) {
        NPError npError = browser->destroystream(obj->npp, obj->stream, NPRES_USER_BREAK);
        INT32_TO_NPVARIANT(npError, *result);
        return true;        
    } else if (name == pluginMethodIdentifiers[ID_TEST_GETINTIDENTIFIER]) {
        if (argCount == 1 && NPVARIANT_IS_DOUBLE(args[0])) {
            NPIdentifier identifier = browser->getintidentifier((int)NPVARIANT_TO_DOUBLE(args[0]));
            INT32_TO_NPVARIANT((int32)identifier, *result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_EVALUATE] && 
               argCount == 1 && NPVARIANT_IS_STRING(args[0])) {
        NPObject *windowScriptObject;
        browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

        NPString s = NPVARIANT_TO_STRING(args[0]);
        
        bool retval = browser->evaluate(obj->npp, windowScriptObject, &s, result);
        browser->releaseobject(windowScriptObject);
        return retval;
    } else if (name == pluginMethodIdentifiers[ID_TEST_GET_PROPERTY] &&
               argCount > 0) {
        NPObject *object;
        browser->getvalue(obj->npp, NPNVWindowNPObject, &object);

        for (uint32_t i = 0; i < argCount; i++) {
            assert(NPVARIANT_IS_STRING(args[i]));
            NPUTF8* propertyString = createCStringFromNPVariant(&args[i]);
            NPIdentifier propertyIdentifier = browser->getstringidentifier(propertyString);
            free(propertyString);
            
            NPVariant variant;
            bool retval = browser->getproperty(obj->npp, object, propertyIdentifier, &variant);
            browser->releaseobject(object);
            
            if (!retval)
                break;
            
            if (i + 1 < argCount) {
                assert(NPVARIANT_IS_OBJECT(variant));
                object = NPVARIANT_TO_OBJECT(variant);
            } else {                
                *result = variant;
                return true;
            }
        }
        
        VOID_TO_NPVARIANT(*result);
        return false;
    } else if (name == pluginMethodIdentifiers[ID_TEST_GET_PROPERTY_RETURN_VALUE] &&
        argCount == 2 && NPVARIANT_IS_OBJECT(args[0]) && NPVARIANT_IS_STRING(args[1])) {
        NPUTF8* propertyString = createCStringFromNPVariant(&args[1]);
        NPIdentifier propertyIdentifier = browser->getstringidentifier(propertyString);
        free(propertyString);

        NPVariant variant;
        bool retval = browser->getproperty(obj->npp, NPVARIANT_TO_OBJECT(args[0]), propertyIdentifier, &variant);
        if (retval)
            browser->releasevariantvalue(&variant);

        BOOLEAN_TO_NPVARIANT(retval, *result);
        return true;
    }
    return false;
}

static bool pluginInvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    INT32_TO_NPVARIANT(1, *result);
    return true;
}

static void pluginInvalidate(NPObject *obj)
{
}

static NPObject *pluginAllocate(NPP npp, NPClass *theClass)
{
    PluginObject *newInstance = (PluginObject*)malloc(sizeof(PluginObject));
    
    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    newInstance->npp = npp;
    newInstance->testObject = browser->createobject(npp, getTestClass());
    newInstance->eventLogging = FALSE;
    newInstance->logDestroy = FALSE;
    newInstance->logSetWindow = FALSE;
    newInstance->returnErrorFromNewStream = FALSE;
    newInstance->stream = 0;
    
    newInstance->firstUrl = NULL;
    newInstance->firstHeaders = NULL;
    newInstance->lastUrl = NULL;
    newInstance->lastHeaders = NULL;
    
    return (NPObject *)newInstance;
}

static void pluginDeallocate(NPObject *header) 
{
    PluginObject* obj = (PluginObject*)header;
    
    browser->releaseobject(obj->testObject);

    free(obj->firstUrl);
    free(obj->firstHeaders);
    free(obj->lastUrl);
    free(obj->lastHeaders);

    free(obj);
}

void handleCallback(PluginObject* object, const char *url, NPReason reason, void *notifyData)
{
    assert(object);
    
    NPVariant args[2];
    
    NPObject *windowScriptObject;
    browser->getvalue(object->npp, NPNVWindowNPObject, &windowScriptObject);
    
    NPIdentifier callbackIdentifier = notifyData;

    INT32_TO_NPVARIANT(reason, args[0]);

    char *strHdr = NULL;
    if (object->firstUrl && object->firstHeaders && object->lastUrl && object->lastHeaders) {
        // Format expected by JavaScript validator: four fields separated by \n\n:
        // First URL; first header block; last URL; last header block.
        // Note that header blocks already end with \n due to how NPStream::headers works.
        int len = strlen(object->firstUrl) + 2
            + strlen(object->firstHeaders) + 1
            + strlen(object->lastUrl) + 2
            + strlen(object->lastHeaders) + 1;
        strHdr = (char*)malloc(len + 1);
        snprintf(strHdr, len + 1, "%s\n\n%s\n%s\n\n%s\n",
                 object->firstUrl, object->firstHeaders, object->lastUrl, object->lastHeaders);
        STRINGN_TO_NPVARIANT(strHdr, len, args[1]);
    } else
        NULL_TO_NPVARIANT(args[1]);

    NPVariant browserResult;
    browser->invoke(object->npp, windowScriptObject, callbackIdentifier, args, 2, &browserResult);
    browser->releasevariantvalue(&browserResult);

    free(strHdr);
}

void notifyStream(PluginObject* object, const char *url, const char *headers)
{
    if (object->firstUrl == NULL) {
        if (url)
            object->firstUrl = strdup(url);
        if (headers)
            object->firstHeaders = strdup(headers);
    } else {
        free(object->lastUrl);
        free(object->lastHeaders);
        object->lastUrl = (url ? strdup(url) : NULL);
        object->lastHeaders = (headers ? strdup(headers) : NULL);
    }
}
