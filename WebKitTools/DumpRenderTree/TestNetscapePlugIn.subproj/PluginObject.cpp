/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Holger Hans Peter Freyther
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
#include <stdarg.h>
#include <stdio.h>

#include <string.h>
#include <stdlib.h>

void pluginLog(NPP instance, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char message[2048] = "PLUGIN: ";
    vsprintf(message + strlen(message), format, args);
    va_end(args);

    NPObject* windowObject = 0;
    NPError error = browser->getvalue(instance, NPNVWindowNPObject, &windowObject);
    if (error != NPERR_NO_ERROR) {
        fprintf(stderr, "Failed to retrieve window object while logging: %s\n", message);
        return;
    }

    NPVariant consoleVariant;
    if (!browser->getproperty(instance, windowObject, browser->getstringidentifier("console"), &consoleVariant)) {
        fprintf(stderr, "Failed to retrieve console object while logging: %s\n", message);
        browser->releaseobject(windowObject);
        return;
    }

    NPObject* consoleObject = NPVARIANT_TO_OBJECT(consoleVariant);

    NPVariant messageVariant;
    STRINGZ_TO_NPVARIANT(message, messageVariant);

    NPVariant result;
    if (!browser->invoke(instance, consoleObject, browser->getstringidentifier("log"), &messageVariant, 1, &result)) {
        fprintf(stderr, "Failed to invoke console.log while logging: %s\n", message);
        browser->releaseobject(consoleObject);
        browser->releaseobject(windowObject);
        return;
    }

    browser->releasevariantvalue(&result);
    browser->releaseobject(consoleObject);
    browser->releaseobject(windowObject);
}

static void pluginInvalidate(NPObject*);
static bool pluginHasProperty(NPObject*, NPIdentifier name);
static bool pluginHasMethod(NPObject*, NPIdentifier name);
static bool pluginGetProperty(NPObject*, NPIdentifier name, NPVariant*);
static bool pluginSetProperty(NPObject*, NPIdentifier name, const NPVariant*);
static bool pluginInvoke(NPObject*, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool pluginInvokeDefault(NPObject*, const NPVariant* args, uint32_t argCount, NPVariant* result);
static NPObject* pluginAllocate(NPP npp, NPClass*);
static void pluginDeallocate(NPObject*);

NPNetscapeFuncs* browser;

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

enum {
    ID_PROPERTY_PROPERTY = 0,
    ID_PROPERTY_EVENT_LOGGING,
    ID_PROPERTY_HAS_STREAM,
    ID_PROPERTY_TEST_OBJECT,
    ID_PROPERTY_LOG_DESTROY,
    ID_PROPERTY_RETURN_ERROR_FROM_NEWSTREAM,
    ID_PROPERTY_PRIVATE_BROWSING_ENABLED,
    ID_PROPERTY_CACHED_PRIVATE_BROWSING_ENABLED,
    NUM_PROPERTY_IDENTIFIERS
};

static NPIdentifier pluginPropertyIdentifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8 *pluginPropertyIdentifierNames[NUM_PROPERTY_IDENTIFIERS] = {
    "property",
    "eventLoggingEnabled",
    "hasStream",
    "testObject",
    "logDestroy",
    "returnErrorFromNewStream",
    "privateBrowsingEnabled",
    "cachedPrivateBrowsingEnabled",
};

enum {
    ID_TEST_CALLBACK_METHOD = 0,
    ID_TEST_GETURL,
    ID_REMOVE_DEFAULT_METHOD,
    ID_TEST_DOM_ACCESS,
    ID_TEST_GET_URL_NOTIFY,
    ID_TEST_INVOKE_DEFAULT,
    ID_DESTROY_STREAM,
    ID_TEST_ENUMERATE,
    ID_TEST_GETINTIDENTIFIER,
    ID_TEST_GET_PROPERTY,
    ID_TEST_HAS_PROPERTY,
    ID_TEST_HAS_METHOD,
    ID_TEST_EVALUATE,
    ID_TEST_GET_PROPERTY_RETURN_VALUE,
    ID_TEST_IDENTIFIER_TO_STRING,
    ID_TEST_IDENTIFIER_TO_INT,
    ID_TEST_POSTURL_FILE,
    ID_TEST_CONSTRUCT,
    ID_TEST_THROW_EXCEPTION_METHOD,
    ID_TEST_FAIL_METHOD,
    ID_DESTROY_NULL_STREAM,
    NUM_METHOD_IDENTIFIERS
};

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
    "testHasProperty",
    "testHasMethod",
    "testEvaluate",
    "testGetPropertyReturnValue",
    "testIdentifierToString",
    "testIdentifierToInt",
    "testPostURLFile",
    "testConstruct",
    "testThrowException",
    "testFail",
    "destroyNullStream"
};

static NPUTF8* createCStringFromNPVariant(const NPVariant* variant)
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

static bool pluginGetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_PROPERTY]) {
        STRINGZ_TO_NPVARIANT("property", *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_EVENT_LOGGING]) {
        BOOLEAN_TO_NPVARIANT(plugin->eventLogging, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_LOG_DESTROY]) {
        BOOLEAN_TO_NPVARIANT(plugin->logDestroy, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_HAS_STREAM]) {
        BOOLEAN_TO_NPVARIANT(plugin->stream != 0, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_TEST_OBJECT]) {
        NPObject* testObject = plugin->testObject;
        browser->retainobject(testObject);
        OBJECT_TO_NPVARIANT(testObject, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_RETURN_ERROR_FROM_NEWSTREAM]) {
        BOOLEAN_TO_NPVARIANT(plugin->returnErrorFromNewStream, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_PRIVATE_BROWSING_ENABLED]) {
        NPBool privateBrowsingEnabled = FALSE;
        browser->getvalue(plugin->npp, NPNVprivateModeBool, &privateBrowsingEnabled);
        BOOLEAN_TO_NPVARIANT(privateBrowsingEnabled, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_CACHED_PRIVATE_BROWSING_ENABLED]) {
        BOOLEAN_TO_NPVARIANT(plugin->cachedPrivateBrowsingMode, *result);
        return true;
    }
    return false;
}

static bool pluginSetProperty(NPObject* obj, NPIdentifier name, const NPVariant* variant)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_EVENT_LOGGING]) {
        plugin->eventLogging = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_LOG_DESTROY]) {
        plugin->logDestroy = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_RETURN_ERROR_FROM_NEWSTREAM]) {
        plugin->returnErrorFromNewStream = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    }

    return false;
}

static bool testDOMAccess(PluginObject* obj, const NPVariant*, uint32_t, NPVariant* result)
{
    // Get plug-in's DOM element
    NPObject* elementObject;
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
    VOID_TO_NPVARIANT(*result);
    return true;
}

static NPIdentifier stringVariantToIdentifier(NPVariant variant)
{
    assert(NPVARIANT_IS_STRING(variant));
    NPUTF8* utf8String = createCStringFromNPVariant(&variant);
    NPIdentifier identifier = browser->getstringidentifier(utf8String);
    free(utf8String);
    return identifier;
}

static NPIdentifier int32VariantToIdentifier(NPVariant variant)
{
    assert(NPVARIANT_IS_INT32(variant));
    int32 integer = NPVARIANT_TO_INT32(variant);
    return browser->getintidentifier(integer);
}

static NPIdentifier doubleVariantToIdentifier(NPVariant variant)
{
    assert(NPVARIANT_IS_DOUBLE(variant));
    double value = NPVARIANT_TO_DOUBLE(variant);
    // Sadly there is no "getdoubleidentifier"
    int32 integer = static_cast<int32>(value);
    return browser->getintidentifier(integer);
}

static NPIdentifier variantToIdentifier(NPVariant variant)
{
    if (NPVARIANT_IS_STRING(variant))
        return stringVariantToIdentifier(variant);
    else if (NPVARIANT_IS_INT32(variant))
        return int32VariantToIdentifier(variant);
    else if (NPVARIANT_IS_DOUBLE(variant))
        return doubleVariantToIdentifier(variant);
    return 0;
}

static bool testIdentifierToString(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1)
        return true;
    NPIdentifier identifier = variantToIdentifier(args[0]);
    if (!identifier)
        return true;
    NPUTF8* utf8String = browser->utf8fromidentifier(identifier);
    if (!utf8String)
        return true;
    STRINGZ_TO_NPVARIANT(utf8String, *result);
    return true;
}

static bool testIdentifierToInt(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1)
        return false;
    NPIdentifier identifier = variantToIdentifier(args[0]);
    if (!identifier)
        return false;
    int32 integer = browser->intfromidentifier(identifier);
    INT32_TO_NPVARIANT(integer, *result);
    return true;
}

static bool testCallback(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 0 || !NPVARIANT_IS_STRING(args[0]))
        return false;

    NPObject* windowScriptObject;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

    NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
    NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
    free(callbackString);

    NPVariant browserResult;
    browser->invoke(obj->npp, windowScriptObject, callbackIdentifier, 0, 0, &browserResult);
    browser->releasevariantvalue(&browserResult);

    browser->releaseobject(windowScriptObject);
    
    VOID_TO_NPVARIANT(*result);
    return true;
}

static bool getURL(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 2 && NPVARIANT_IS_STRING(args[0]) && NPVARIANT_IS_STRING(args[1])) {
        NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
        NPUTF8* targetString = createCStringFromNPVariant(&args[1]);
        NPError npErr = browser->geturl(obj->npp, urlString, targetString);
        free(urlString);
        free(targetString);

        INT32_TO_NPVARIANT(npErr, *result);
        return true;
    } else if (argCount == 1 && NPVARIANT_IS_STRING(args[0])) {
        NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
        NPError npErr = browser->geturl(obj->npp, urlString, 0);
        free(urlString);

        INT32_TO_NPVARIANT(npErr, *result);
        return true;
    }
    return false;
}

static bool removeDefaultMethod(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    pluginClass.invokeDefault = 0;
    VOID_TO_NPVARIANT(*result);
    return true;
}

static bool getURLNotify(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 3 || !NPVARIANT_IS_STRING(args[0])
        || (!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1]))
        || !NPVARIANT_IS_STRING(args[2]))
        return false;

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

static bool testInvokeDefault(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (!NPVARIANT_IS_OBJECT(args[0]))
        return false;

    NPObject *callback = NPVARIANT_TO_OBJECT(args[0]);

    NPVariant invokeArgs[1];
    NPVariant browserResult;

    STRINGZ_TO_NPVARIANT("test", invokeArgs[0]);
    bool retval = browser->invokeDefault(obj->npp, callback, invokeArgs, 1, &browserResult);

    if (retval)
        browser->releasevariantvalue(&browserResult);

    BOOLEAN_TO_NPVARIANT(retval, *result);
    return true;
}

static bool destroyStream(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPError npError = browser->destroystream(obj->npp, obj->stream, NPRES_USER_BREAK);
    INT32_TO_NPVARIANT(npError, *result);
    return true;
}

static bool destroyNullStream(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPError npError = browser->destroystream(obj->npp, 0, NPRES_USER_BREAK);
    INT32_TO_NPVARIANT(npError, *result);
    return true;
}

static bool testEnumerate(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_OBJECT(args[1]))
        return false;

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

static bool testGetIntIdentifier(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1 || !NPVARIANT_IS_DOUBLE(args[0]))
        return false;

    NPIdentifier identifier = browser->getintidentifier((int)NPVARIANT_TO_DOUBLE(args[0]));
    INT32_TO_NPVARIANT((int32)(long long)identifier, *result);
    return true;
}

static bool testGetProperty(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 0)
        return false;

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
}

static bool testHasProperty(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_STRING(args[1]))
        return false;

    NPUTF8* propertyString = createCStringFromNPVariant(&args[1]);
    NPIdentifier propertyIdentifier = browser->getstringidentifier(propertyString);
    free(propertyString);

    bool retval = browser->hasproperty(obj->npp, NPVARIANT_TO_OBJECT(args[0]), propertyIdentifier);

    BOOLEAN_TO_NPVARIANT(retval, *result);
    return true;
}

static bool testHasMethod(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_STRING(args[1]))
        return false;

    NPUTF8* propertyString = createCStringFromNPVariant(&args[1]);
    NPIdentifier propertyIdentifier = browser->getstringidentifier(propertyString);
    free(propertyString);

    bool retval = browser->hasmethod(obj->npp, NPVARIANT_TO_OBJECT(args[0]), propertyIdentifier);

    BOOLEAN_TO_NPVARIANT(retval, *result);
    return true;
}

static bool testEvaluate(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1 || !NPVARIANT_IS_STRING(args[0]))
        return false;
    NPObject* windowScriptObject;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

    NPString s = NPVARIANT_TO_STRING(args[0]);

    bool retval = browser->evaluate(obj->npp, windowScriptObject, &s, result);
    browser->releaseobject(windowScriptObject);
    return retval;
}

static bool testGetPropertyReturnValue(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_STRING(args[1]))
        return false;

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

static char* toCString(const NPString& string)
{
    char* result = static_cast<char*>(malloc(string.UTF8Length + 1));
    memcpy(result, string.UTF8Characters, string.UTF8Length);
    result[string.UTF8Length] = '\0';

    return result;
}

static bool testPostURLFile(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 4 || !NPVARIANT_IS_STRING(args[0]) || !NPVARIANT_IS_STRING(args[1]) || !NPVARIANT_IS_STRING(args[2]) || !NPVARIANT_IS_STRING(args[3]))
        return false;

    NPString urlString = NPVARIANT_TO_STRING(args[0]);
    char* url = toCString(urlString);

    NPString targetString = NPVARIANT_TO_STRING(args[1]);
    char* target = toCString(targetString);

    NPString pathString = NPVARIANT_TO_STRING(args[2]);
    char* path = toCString(pathString);

    NPString contentsString = NPVARIANT_TO_STRING(args[3]);

    FILE* tempFile = fopen(path, "w");
    if (!tempFile)
        return false;

    fwrite(contentsString.UTF8Characters, contentsString.UTF8Length, 1, tempFile);
    fclose(tempFile);

    NPError error = browser->posturl(obj->npp, url, target, pathString.UTF8Length, path, TRUE);

    free(path);
    free(target);
    free(url);

    BOOLEAN_TO_NPVARIANT(error == NPERR_NO_ERROR, *result);
    return true;
}

static bool testConstruct(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (!argCount || !NPVARIANT_IS_OBJECT(args[0]))
        return false;
    
    return browser->construct(obj->npp, NPVARIANT_TO_OBJECT(args[0]), args + 1, argCount - 1, result);
}

static bool pluginInvoke(NPObject* header, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
    if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_METHOD])
        return testCallback(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GETURL])
        return getURL(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_REMOVE_DEFAULT_METHOD])
        return removeDefaultMethod(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_DOM_ACCESS])
        return testDOMAccess(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GET_URL_NOTIFY])
        return getURLNotify(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_INVOKE_DEFAULT])
        return testInvokeDefault(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_ENUMERATE])
        return testEnumerate(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_DESTROY_STREAM])
        return destroyStream(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GETINTIDENTIFIER])
        return testGetIntIdentifier(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_EVALUATE])
        return testEvaluate(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GET_PROPERTY])
        return testGetProperty(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GET_PROPERTY_RETURN_VALUE])
        return testGetPropertyReturnValue(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_HAS_PROPERTY])
        return testHasProperty(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_HAS_METHOD])
        return testHasMethod(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_IDENTIFIER_TO_STRING])
        return testIdentifierToString(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_IDENTIFIER_TO_INT])
        return testIdentifierToInt(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_POSTURL_FILE])
        return testPostURLFile(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_CONSTRUCT])
        return testConstruct(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_THROW_EXCEPTION_METHOD]) {
        browser->setexception(header, "plugin object testThrowException SUCCESS");
        return true;
    } else if (name == pluginMethodIdentifiers[ID_TEST_FAIL_METHOD]) {
        NPObject* windowScriptObject;
        browser->getvalue(plugin->npp, NPNVWindowNPObject, &windowScriptObject);
        browser->invoke(plugin->npp, windowScriptObject, name, args, argCount, result);
    } else if (name == pluginMethodIdentifiers[ID_DESTROY_NULL_STREAM]) 
        return destroyNullStream(plugin, args, argCount, result);
    
    return false;
}

static bool pluginInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    INT32_TO_NPVARIANT(1, *result);
    return true;
}

static void pluginInvalidate(NPObject* header)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
    plugin->testObject = 0;
}

static NPObject *pluginAllocate(NPP npp, NPClass *theClass)
{
    PluginObject* newInstance = (PluginObject*)malloc(sizeof(PluginObject));

    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    newInstance->npp = npp;
    newInstance->testObject = browser->createobject(npp, getTestClass());
    newInstance->eventLogging = FALSE;
    newInstance->onStreamLoad = 0;
    newInstance->onStreamDestroy = 0;
    newInstance->onURLNotify = 0;
    newInstance->logDestroy = FALSE;
    newInstance->logSetWindow = FALSE;
    newInstance->returnErrorFromNewStream = FALSE;
    newInstance->stream = 0;

    newInstance->firstUrl = NULL;
    newInstance->firstHeaders = NULL;
    newInstance->lastUrl = NULL;
    newInstance->lastHeaders = NULL;

    return (NPObject*)newInstance;
}

static void pluginDeallocate(NPObject* header)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
    if (plugin->testObject)
        browser->releaseobject(plugin->testObject);

    free(plugin->firstUrl);
    free(plugin->firstHeaders);
    free(plugin->lastUrl);
    free(plugin->lastHeaders);
    free(plugin);
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

void testNPRuntime(NPP npp)
{
    NPObject* windowScriptObject;
    browser->getvalue(npp, NPNVWindowNPObject, &windowScriptObject);

    // Invoke
    NPIdentifier testNPInvoke = browser->getstringidentifier("testNPInvoke");
    NPVariant args[7];
    
    VOID_TO_NPVARIANT(args[0]);
    NULL_TO_NPVARIANT(args[1]);
    BOOLEAN_TO_NPVARIANT(true, args[2]);
    INT32_TO_NPVARIANT(242, args[3]);
    DOUBLE_TO_NPVARIANT(242.242, args[4]);
    STRINGZ_TO_NPVARIANT("Hello, World", args[5]);
    OBJECT_TO_NPVARIANT(windowScriptObject, args[6]);
    
    NPVariant result;
    if (browser->invoke(npp, windowScriptObject, testNPInvoke, args, 7, &result))
        browser->releasevariantvalue(&result);
    
    browser->releaseobject(windowScriptObject);
}
