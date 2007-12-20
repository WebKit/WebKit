// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "JavaScriptCore.h"
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/UnusedParam.h>

static JSGlobalContextRef context = 0;

static void assertEqualsAsBoolean(JSValueRef value, bool expectedValue)
{
    if (JSValueToBoolean(context, value) != expectedValue)
        fprintf(stderr, "assertEqualsAsBoolean failed: %p, %d\n", value, expectedValue);
}

static void assertEqualsAsNumber(JSValueRef value, double expectedValue)
{
    double number = JSValueToNumber(context, value, NULL);

    // FIXME <rdar://4668451> - On i386 the isnan(double) macro tries to map to the isnan(float) function,
    // causing a build break with -Wshorten-64-to-32 enabled.  The issue is known by the appropriate team.
    // After that's resolved, we can remove these casts
    if (number != expectedValue && !(isnan((float)number) && isnan((float)expectedValue)))
        fprintf(stderr, "assertEqualsAsNumber failed: %p, %lf\n", value, expectedValue);
}

static void assertEqualsAsUTF8String(JSValueRef value, const char* expectedValue)
{
    JSStringRef valueAsString = JSValueToStringCopy(context, value, NULL);

    size_t jsSize = JSStringGetMaximumUTF8CStringSize(valueAsString);
    char jsBuffer[jsSize];
    JSStringGetUTF8CString(valueAsString, jsBuffer, jsSize);
    
    unsigned i;
    for (i = 0; jsBuffer[i]; i++)
        if (jsBuffer[i] != expectedValue[i])
            fprintf(stderr, "assertEqualsAsUTF8String failed at character %d: %c(%d) != %c(%d)\n", i, jsBuffer[i], jsBuffer[i], expectedValue[i], expectedValue[i]);
        
    if (jsSize < strlen(jsBuffer) + 1)
        fprintf(stderr, "assertEqualsAsUTF8String failed: jsSize was too small\n");

    JSStringRelease(valueAsString);
}

static void assertEqualsAsCharactersPtr(JSValueRef value, const char* expectedValue)
{
    JSStringRef valueAsString = JSValueToStringCopy(context, value, NULL);

    size_t jsLength = JSStringGetLength(valueAsString);
    const JSChar* jsBuffer = JSStringGetCharactersPtr(valueAsString);

    CFStringRef expectedValueAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                                    expectedValue,
                                                                    kCFStringEncodingUTF8);    
    CFIndex cfLength = CFStringGetLength(expectedValueAsCFString);
    UniChar cfBuffer[cfLength];
    CFStringGetCharacters(expectedValueAsCFString, CFRangeMake(0, cfLength), cfBuffer);
    CFRelease(expectedValueAsCFString);

    if (memcmp(jsBuffer, cfBuffer, cfLength * sizeof(UniChar)) != 0)
        fprintf(stderr, "assertEqualsAsCharactersPtr failed: jsBuffer != cfBuffer\n");
    
    if (jsLength != (size_t)cfLength)
        fprintf(stderr, "assertEqualsAsCharactersPtr failed: jsLength(%ld) != cfLength(%ld)\n", jsLength, cfLength);
    
    JSStringRelease(valueAsString);
}

static JSValueRef jsGlobalValue; // non-stack value for testing JSValueProtect()

/* MyObject pseudo-class */

static bool MyObject_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (JSStringIsEqualToUTF8CString(propertyName, "alwaysOne")
        || JSStringIsEqualToUTF8CString(propertyName, "cantFind")
        || JSStringIsEqualToUTF8CString(propertyName, "myPropertyName")
        || JSStringIsEqualToUTF8CString(propertyName, "hasPropertyLie")
        || JSStringIsEqualToUTF8CString(propertyName, "0")) {
        return true;
    }
    
    return false;
}

static JSValueRef MyObject_getProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    if (JSStringIsEqualToUTF8CString(propertyName, "alwaysOne")) {
        return JSValueMakeNumber(context, 1);
    }
    
    if (JSStringIsEqualToUTF8CString(propertyName, "myPropertyName")) {
        return JSValueMakeNumber(context, 1);
    }

    if (JSStringIsEqualToUTF8CString(propertyName, "cantFind")) {
        return JSValueMakeUndefined(context);
    }
    
    if (JSStringIsEqualToUTF8CString(propertyName, "0")) {
        *exception = JSValueMakeNumber(context, 1);
        return JSValueMakeNumber(context, 1);
    }
    
    return NULL;
}

static bool MyObject_setProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(value);
    UNUSED_PARAM(exception);

    if (JSStringIsEqualToUTF8CString(propertyName, "cantSet"))
        return true; // pretend we set the property in order to swallow it
    
    return false;
}

static bool MyObject_deleteProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    if (JSStringIsEqualToUTF8CString(propertyName, "cantDelete"))
        return true;
    
    if (JSStringIsEqualToUTF8CString(propertyName, "throwOnDelete")) {
        *exception = JSValueMakeNumber(context, 2);
        return false;
    }

    return false;
}

static void MyObject_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    JSStringRef propertyName;
    
    propertyName = JSStringCreateWithUTF8CString("alwaysOne");
    JSPropertyNameAccumulatorAddName(propertyNames, propertyName);
    JSStringRelease(propertyName);
    
    propertyName = JSStringCreateWithUTF8CString("myPropertyName");
    JSPropertyNameAccumulatorAddName(propertyNames, propertyName);
    JSStringRelease(propertyName);
}

static JSValueRef MyObject_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(exception);

    if (argumentCount > 0 && JSValueIsStrictEqual(context, arguments[0], JSValueMakeNumber(context, 0)))
        return JSValueMakeNumber(context, 1);
    
    return JSValueMakeUndefined(context);
}

static JSObjectRef MyObject_callAsConstructor(JSContextRef context, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (argumentCount > 0 && JSValueIsStrictEqual(context, arguments[0], JSValueMakeNumber(context, 0)))
        return JSValueToObject(context, JSValueMakeNumber(context, 1), exception);
    
    return JSValueToObject(context, JSValueMakeNumber(context, 0), exception);
}

static bool MyObject_hasInstance(JSContextRef context, JSObjectRef constructor, JSValueRef possibleValue, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(constructor);

    JSStringRef numberString = JSStringCreateWithUTF8CString("Number");
    JSObjectRef numberConstructor = JSValueToObject(context, JSObjectGetProperty(context, JSContextGetGlobalObject(context), numberString, exception), exception);
    JSStringRelease(numberString);

    return JSValueIsInstanceOfConstructor(context, possibleValue, numberConstructor, exception);
}

static JSValueRef MyObject_convertToType(JSContextRef context, JSObjectRef object, JSType type, JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(exception);
    
    switch (type) {
    case kJSTypeNumber:
        return JSValueMakeNumber(context, 1);
    default:
        break;
    }

    // string conversion -- forward to default object class
    return NULL;
}

static JSStaticValue evilStaticValues[] = {
    { "nullGetSet", 0, 0, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static JSStaticFunction evilStaticFunctions[] = {
    { "nullCall", 0, kJSPropertyAttributeNone },
    { 0, 0, 0 }
};

JSClassDefinition MyObject_definition = {
    0,
    kJSClassAttributeNone,
    
    "MyObject",
    NULL,
    
    evilStaticValues,
    evilStaticFunctions,
    
    NULL,
    NULL,
    MyObject_hasProperty,
    MyObject_getProperty,
    MyObject_setProperty,
    MyObject_deleteProperty,
    MyObject_getPropertyNames,
    MyObject_callAsFunction,
    MyObject_callAsConstructor,
    MyObject_hasInstance,
    MyObject_convertToType,
};

static JSClassRef MyObject_class(JSContextRef context)
{
    UNUSED_PARAM(context);

    static JSClassRef jsClass;
    if (!jsClass)
        jsClass = JSClassCreate(&MyObject_definition);
    
    return jsClass;
}

static JSValueRef Base_get(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(exception);

    return JSValueMakeNumber(ctx, 1); // distinguish base get form derived get
}

static bool Base_set(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(value);

    *exception = JSValueMakeNumber(ctx, 1); // distinguish base set from derived set
    return true;
}

static JSValueRef Base_callAsFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(function);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(exception);
    
    return JSValueMakeNumber(ctx, 1); // distinguish base call from derived call
}

static JSStaticFunction Base_staticFunctions[] = {
    { "baseProtoDup", NULL, kJSPropertyAttributeNone },
    { "baseProto", Base_callAsFunction, kJSPropertyAttributeNone },
    { 0, 0, 0 }
};

static JSStaticValue Base_staticValues[] = {
    { "baseDup", Base_get, Base_set, kJSPropertyAttributeNone },
    { "baseOnly", Base_get, Base_set, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static bool TestInitializeFinalize;
static void Base_initialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(context);

    if (TestInitializeFinalize) {
        ASSERT((void*)1 == JSObjectGetPrivate(object));
        JSObjectSetPrivate(object, (void*)2);
    }
}

static unsigned Base_didFinalize;
static void Base_finalize(JSObjectRef object)
{
    UNUSED_PARAM(object);
    if (TestInitializeFinalize) {
        ASSERT((void*)4 == JSObjectGetPrivate(object));
        Base_didFinalize = true;
    }
}

static JSClassRef Base_class(JSContextRef context)
{
    UNUSED_PARAM(context);

    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.staticValues = Base_staticValues;
        definition.staticFunctions = Base_staticFunctions;
        definition.initialize = Base_initialize;
        definition.finalize = Base_finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

static JSValueRef Derived_get(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(exception);

    return JSValueMakeNumber(ctx, 2); // distinguish base get form derived get
}

static bool Derived_set(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UNUSED_PARAM(ctx);
    UNUSED_PARAM(object);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(value);

    *exception = JSValueMakeNumber(ctx, 2); // distinguish base set from derived set
    return true;
}

static JSValueRef Derived_callAsFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(function);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(exception);
    
    return JSValueMakeNumber(ctx, 2); // distinguish base call from derived call
}

static JSStaticFunction Derived_staticFunctions[] = {
    { "protoOnly", Derived_callAsFunction, kJSPropertyAttributeNone },
    { "protoDup", NULL, kJSPropertyAttributeNone },
    { "baseProtoDup", Derived_callAsFunction, kJSPropertyAttributeNone },
    { 0, 0, 0 }
};

static JSStaticValue Derived_staticValues[] = {
    { "derivedOnly", Derived_get, Derived_set, kJSPropertyAttributeNone },
    { "protoDup", Derived_get, Derived_set, kJSPropertyAttributeNone },
    { "baseDup", Derived_get, Derived_set, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static void Derived_initialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(context);

    if (TestInitializeFinalize) {
        ASSERT((void*)2 == JSObjectGetPrivate(object));
        JSObjectSetPrivate(object, (void*)3);
    }
}

static void Derived_finalize(JSObjectRef object)
{
    if (TestInitializeFinalize) {
        ASSERT((void*)3 == JSObjectGetPrivate(object));
        JSObjectSetPrivate(object, (void*)4);
    }
}

static JSClassRef Derived_class(JSContextRef context)
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.parentClass = Base_class(context);
        definition.staticValues = Derived_staticValues;
        definition.staticFunctions = Derived_staticFunctions;
        definition.initialize = Derived_initialize;
        definition.finalize = Derived_finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

static JSValueRef print_callAsFunction(JSContextRef context, JSObjectRef functionObject, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(exception);
    
    if (argumentCount > 0) {
        JSStringRef string = JSValueToStringCopy(context, arguments[0], NULL);
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
        char stringUTF8[sizeUTF8];
        JSStringGetUTF8CString(string, stringUTF8, sizeUTF8);
        printf("%s\n", stringUTF8);
        JSStringRelease(string);
    }
    
    return JSValueMakeUndefined(context);
}

static JSObjectRef myConstructor_callAsConstructor(JSContextRef context, JSObjectRef constructorObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(constructorObject);
    UNUSED_PARAM(exception);
    
    JSObjectRef result = JSObjectMake(context, NULL, NULL);
    if (argumentCount > 0) {
        JSStringRef value = JSStringCreateWithUTF8CString("value");
        JSObjectSetProperty(context, result, value, arguments[0], kJSPropertyAttributeNone, NULL);
        JSStringRelease(value);
    }
    
    return result;
}


static void globalObject_initialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(object);
    // Ensure that an execution context is passed in
    ASSERT(context);

    // Ensure that the global object is set to the object that we were passed
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    ASSERT(globalObject);
    ASSERT(object == globalObject);

    // Ensure that the standard global properties have been set on the global object
    JSStringRef array = JSStringCreateWithUTF8CString("Array");
    JSObjectRef arrayConstructor = JSValueToObject(context, JSObjectGetProperty(context, globalObject, array, NULL), NULL);
    JSStringRelease(array);

    UNUSED_PARAM(arrayConstructor);
    ASSERT(arrayConstructor);
}

static JSValueRef globalObject_get(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(exception);

    return JSValueMakeNumber(ctx, 3);
}

static bool globalObject_set(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(value);

    *exception = JSValueMakeNumber(ctx, 3);
    return true;
}

static JSValueRef globalObject_call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(function);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(exception);

    return JSValueMakeNumber(ctx, 3);
}

static JSStaticValue globalObject_staticValues[] = {
    { "globalStaticValue", globalObject_get, globalObject_set, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static JSStaticFunction globalObject_staticFunctions[] = {
    { "globalStaticFunction", globalObject_call, kJSPropertyAttributeNone },
    { 0, 0, 0 }
};

static char* createStringWithContentsOfFile(const char* fileName);

static void testInitializeFinalize()
{
    JSObjectRef o = JSObjectMake(context, Derived_class(context), (void*)1);
    UNUSED_PARAM(o);
    ASSERT(JSObjectGetPrivate(o) == (void*)3);
}

int main(int argc, char* argv[])
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);
    
    // Test garbage collection with a fresh context
    context = JSGlobalContextCreate(NULL);
    TestInitializeFinalize = true;
    testInitializeFinalize();
    JSGlobalContextRelease(context);
    JSGarbageCollect(context);
    TestInitializeFinalize = false;

    ASSERT(Base_didFinalize);

    JSClassDefinition globalObjectClassDefinition = kJSClassDefinitionEmpty;
    globalObjectClassDefinition.initialize = globalObject_initialize;
    globalObjectClassDefinition.staticValues = globalObject_staticValues;
    globalObjectClassDefinition.staticFunctions = globalObject_staticFunctions;
    globalObjectClassDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
    JSClassRef globalObjectClass = JSClassCreate(&globalObjectClassDefinition);
    context = JSGlobalContextCreate(globalObjectClass);
    
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    ASSERT(JSValueIsObject(context, globalObject));
    
    JSValueRef jsUndefined = JSValueMakeUndefined(context);
    JSValueRef jsNull = JSValueMakeNull(context);
    JSValueRef jsTrue = JSValueMakeBoolean(context, true);
    JSValueRef jsFalse = JSValueMakeBoolean(context, false);
    JSValueRef jsZero = JSValueMakeNumber(context, 0);
    JSValueRef jsOne = JSValueMakeNumber(context, 1);
    JSValueRef jsOneThird = JSValueMakeNumber(context, 1.0 / 3.0);
    JSObjectRef jsObjectNoProto = JSObjectMake(context, NULL, NULL);
    JSObjectSetPrototype(context, jsObjectNoProto, JSValueMakeNull(context));

    // FIXME: test funny utf8 characters
    JSStringRef jsEmptyIString = JSStringCreateWithUTF8CString("");
    JSValueRef jsEmptyString = JSValueMakeString(context, jsEmptyIString);
    
    JSStringRef jsOneIString = JSStringCreateWithUTF8CString("1");
    JSValueRef jsOneString = JSValueMakeString(context, jsOneIString);

    UniChar singleUniChar = 65; // Capital A
    CFMutableStringRef cfString = 
        CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault,
                                                          &singleUniChar,
                                                          1,
                                                          1,
                                                          kCFAllocatorNull);

    JSStringRef jsCFIString = JSStringCreateWithCFString(cfString);
    JSValueRef jsCFString = JSValueMakeString(context, jsCFIString);
    
    CFStringRef cfEmptyString = CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
    
    JSStringRef jsCFEmptyIString = JSStringCreateWithCFString(cfEmptyString);
    JSValueRef jsCFEmptyString = JSValueMakeString(context, jsCFEmptyIString);

    CFIndex cfStringLength = CFStringGetLength(cfString);
    UniChar buffer[cfStringLength];
    CFStringGetCharacters(cfString, 
                          CFRangeMake(0, cfStringLength), 
                          buffer);
    JSStringRef jsCFIStringWithCharacters = JSStringCreateWithCharacters(buffer, cfStringLength);
    JSValueRef jsCFStringWithCharacters = JSValueMakeString(context, jsCFIStringWithCharacters);
    
    JSStringRef jsCFEmptyIStringWithCharacters = JSStringCreateWithCharacters(buffer, CFStringGetLength(cfEmptyString));
    JSValueRef jsCFEmptyStringWithCharacters = JSValueMakeString(context, jsCFEmptyIStringWithCharacters);

    ASSERT(JSValueGetType(context, jsUndefined) == kJSTypeUndefined);
    ASSERT(JSValueGetType(context, jsNull) == kJSTypeNull);
    ASSERT(JSValueGetType(context, jsTrue) == kJSTypeBoolean);
    ASSERT(JSValueGetType(context, jsFalse) == kJSTypeBoolean);
    ASSERT(JSValueGetType(context, jsZero) == kJSTypeNumber);
    ASSERT(JSValueGetType(context, jsOne) == kJSTypeNumber);
    ASSERT(JSValueGetType(context, jsOneThird) == kJSTypeNumber);
    ASSERT(JSValueGetType(context, jsEmptyString) == kJSTypeString);
    ASSERT(JSValueGetType(context, jsOneString) == kJSTypeString);
    ASSERT(JSValueGetType(context, jsCFString) == kJSTypeString);
    ASSERT(JSValueGetType(context, jsCFStringWithCharacters) == kJSTypeString);
    ASSERT(JSValueGetType(context, jsCFEmptyString) == kJSTypeString);
    ASSERT(JSValueGetType(context, jsCFEmptyStringWithCharacters) == kJSTypeString);

    JSObjectRef myObject = JSObjectMake(context, MyObject_class(context), NULL);
    JSStringRef myObjectIString = JSStringCreateWithUTF8CString("MyObject");
    JSObjectSetProperty(context, globalObject, myObjectIString, myObject, kJSPropertyAttributeNone, NULL);
    JSStringRelease(myObjectIString);
    
    JSValueRef exception;

    // Conversions that throw exceptions
    exception = NULL;
    ASSERT(NULL == JSValueToObject(context, jsNull, &exception));
    ASSERT(exception);
    
    exception = NULL;
    // FIXME <rdar://4668451> - On i386 the isnan(double) macro tries to map to the isnan(float) function,
    // causing a build break with -Wshorten-64-to-32 enabled.  The issue is known by the appropriate team.
    // After that's resolved, we can remove these casts
    ASSERT(isnan((float)JSValueToNumber(context, jsObjectNoProto, &exception)));
    ASSERT(exception);

    exception = NULL;
    ASSERT(!JSValueToStringCopy(context, jsObjectNoProto, &exception));
    ASSERT(exception);
    
    ASSERT(JSValueToBoolean(context, myObject));
    
    exception = NULL;
    ASSERT(!JSValueIsEqual(context, jsObjectNoProto, JSValueMakeNumber(context, 1), &exception));
    ASSERT(exception);
    
    exception = NULL;
    JSObjectGetPropertyAtIndex(context, myObject, 0, &exception);
    ASSERT(1 == JSValueToNumber(context, exception, NULL));

    assertEqualsAsBoolean(jsUndefined, false);
    assertEqualsAsBoolean(jsNull, false);
    assertEqualsAsBoolean(jsTrue, true);
    assertEqualsAsBoolean(jsFalse, false);
    assertEqualsAsBoolean(jsZero, false);
    assertEqualsAsBoolean(jsOne, true);
    assertEqualsAsBoolean(jsOneThird, true);
    assertEqualsAsBoolean(jsEmptyString, false);
    assertEqualsAsBoolean(jsOneString, true);
    assertEqualsAsBoolean(jsCFString, true);
    assertEqualsAsBoolean(jsCFStringWithCharacters, true);
    assertEqualsAsBoolean(jsCFEmptyString, false);
    assertEqualsAsBoolean(jsCFEmptyStringWithCharacters, false);
    
    assertEqualsAsNumber(jsUndefined, nan(""));
    assertEqualsAsNumber(jsNull, 0);
    assertEqualsAsNumber(jsTrue, 1);
    assertEqualsAsNumber(jsFalse, 0);
    assertEqualsAsNumber(jsZero, 0);
    assertEqualsAsNumber(jsOne, 1);
    assertEqualsAsNumber(jsOneThird, 1.0 / 3.0);
    assertEqualsAsNumber(jsEmptyString, 0);
    assertEqualsAsNumber(jsOneString, 1);
    assertEqualsAsNumber(jsCFString, nan(""));
    assertEqualsAsNumber(jsCFStringWithCharacters, nan(""));
    assertEqualsAsNumber(jsCFEmptyString, 0);
    assertEqualsAsNumber(jsCFEmptyStringWithCharacters, 0);
    ASSERT(sizeof(JSChar) == sizeof(UniChar));
    
    assertEqualsAsCharactersPtr(jsUndefined, "undefined");
    assertEqualsAsCharactersPtr(jsNull, "null");
    assertEqualsAsCharactersPtr(jsTrue, "true");
    assertEqualsAsCharactersPtr(jsFalse, "false");
    assertEqualsAsCharactersPtr(jsZero, "0");
    assertEqualsAsCharactersPtr(jsOne, "1");
    assertEqualsAsCharactersPtr(jsOneThird, "0.3333333333333333");
    assertEqualsAsCharactersPtr(jsEmptyString, "");
    assertEqualsAsCharactersPtr(jsOneString, "1");
    assertEqualsAsCharactersPtr(jsCFString, "A");
    assertEqualsAsCharactersPtr(jsCFStringWithCharacters, "A");
    assertEqualsAsCharactersPtr(jsCFEmptyString, "");
    assertEqualsAsCharactersPtr(jsCFEmptyStringWithCharacters, "");
    
    assertEqualsAsUTF8String(jsUndefined, "undefined");
    assertEqualsAsUTF8String(jsNull, "null");
    assertEqualsAsUTF8String(jsTrue, "true");
    assertEqualsAsUTF8String(jsFalse, "false");
    assertEqualsAsUTF8String(jsZero, "0");
    assertEqualsAsUTF8String(jsOne, "1");
    assertEqualsAsUTF8String(jsOneThird, "0.3333333333333333");
    assertEqualsAsUTF8String(jsEmptyString, "");
    assertEqualsAsUTF8String(jsOneString, "1");
    assertEqualsAsUTF8String(jsCFString, "A");
    assertEqualsAsUTF8String(jsCFStringWithCharacters, "A");
    assertEqualsAsUTF8String(jsCFEmptyString, "");
    assertEqualsAsUTF8String(jsCFEmptyStringWithCharacters, "");
    
    ASSERT(JSValueIsStrictEqual(context, jsTrue, jsTrue));
    ASSERT(!JSValueIsStrictEqual(context, jsOne, jsOneString));

    ASSERT(JSValueIsEqual(context, jsOne, jsOneString, NULL));
    ASSERT(!JSValueIsEqual(context, jsTrue, jsFalse, NULL));
    
    CFStringRef cfJSString = JSStringCopyCFString(kCFAllocatorDefault, jsCFIString);
    CFStringRef cfJSEmptyString = JSStringCopyCFString(kCFAllocatorDefault, jsCFEmptyIString);
    ASSERT(CFEqual(cfJSString, cfString));
    ASSERT(CFEqual(cfJSEmptyString, cfEmptyString));
    CFRelease(cfJSString);
    CFRelease(cfJSEmptyString);

    CFRelease(cfString);
    CFRelease(cfEmptyString);
    
    jsGlobalValue = JSObjectMake(context, NULL, NULL);
    JSValueProtect(context, jsGlobalValue);
    JSGarbageCollect(context);
    ASSERT(JSValueIsObject(context, jsGlobalValue));
    JSValueUnprotect(context, jsGlobalValue);

    JSStringRef goodSyntax = JSStringCreateWithUTF8CString("x = 1;");
    JSStringRef badSyntax = JSStringCreateWithUTF8CString("x := 1;");
    ASSERT(JSCheckScriptSyntax(context, goodSyntax, NULL, 0, NULL));
    ASSERT(!JSCheckScriptSyntax(context, badSyntax, NULL, 0, NULL));

    JSValueRef result;
    JSValueRef v;
    JSObjectRef o;
    JSStringRef string;

    result = JSEvaluateScript(context, goodSyntax, NULL, NULL, 1, NULL);
    ASSERT(result);
    ASSERT(JSValueIsEqual(context, result, jsOne, NULL));

    exception = NULL;
    result = JSEvaluateScript(context, badSyntax, NULL, NULL, 1, &exception);
    ASSERT(!result);
    ASSERT(JSValueIsObject(context, exception));
    
    JSStringRef array = JSStringCreateWithUTF8CString("Array");
    JSObjectRef arrayConstructor = JSValueToObject(context, JSObjectGetProperty(context, globalObject, array, NULL), NULL);
    JSStringRelease(array);
    result = JSObjectCallAsConstructor(context, arrayConstructor, 0, NULL, NULL);
    ASSERT(result);
    ASSERT(JSValueIsObject(context, result));
    ASSERT(JSValueIsInstanceOfConstructor(context, result, arrayConstructor, NULL));
    ASSERT(!JSValueIsInstanceOfConstructor(context, JSValueMakeNull(context), arrayConstructor, NULL));

    o = JSValueToObject(context, result, NULL);
    exception = NULL;
    ASSERT(JSValueIsUndefined(context, JSObjectGetPropertyAtIndex(context, o, 0, &exception)));
    ASSERT(!exception);
    
    JSObjectSetPropertyAtIndex(context, o, 0, JSValueMakeNumber(context, 1), &exception);
    ASSERT(!exception);
    
    exception = NULL;
    ASSERT(1 == JSValueToNumber(context, JSObjectGetPropertyAtIndex(context, o, 0, &exception), &exception));
    ASSERT(!exception);

    JSStringRef functionBody;
    JSObjectRef function;
    
    exception = NULL;
    functionBody = JSStringCreateWithUTF8CString("rreturn Array;");
    JSStringRef line = JSStringCreateWithUTF8CString("line");
    ASSERT(!JSObjectMakeFunction(context, NULL, 0, NULL, functionBody, NULL, 1, &exception));
    ASSERT(JSValueIsObject(context, exception));
    v = JSObjectGetProperty(context, JSValueToObject(context, exception, NULL), line, NULL);
    assertEqualsAsNumber(v, 2); // FIXME: Lexer::setCode bumps startingLineNumber by 1 -- we need to change internal callers so that it doesn't have to (saying '0' to mean '1' in the API would be really confusing -- it's really confusing internally, in fact)
    JSStringRelease(functionBody);
    JSStringRelease(line);

    exception = NULL;
    functionBody = JSStringCreateWithUTF8CString("return Array;");
    function = JSObjectMakeFunction(context, NULL, 0, NULL, functionBody, NULL, 1, &exception);
    JSStringRelease(functionBody);
    ASSERT(!exception);
    ASSERT(JSObjectIsFunction(context, function));
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, NULL);
    ASSERT(v);
    ASSERT(JSValueIsEqual(context, v, arrayConstructor, NULL));
    
    exception = NULL;
    function = JSObjectMakeFunction(context, NULL, 0, NULL, jsEmptyIString, NULL, 0, &exception);
    ASSERT(!exception);
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, &exception);
    ASSERT(v && !exception);
    ASSERT(JSValueIsUndefined(context, v));
    
    exception = NULL;
    v = NULL;
    JSStringRef foo = JSStringCreateWithUTF8CString("foo");
    JSStringRef argumentNames[] = { foo };
    functionBody = JSStringCreateWithUTF8CString("return foo;");
    function = JSObjectMakeFunction(context, foo, 1, argumentNames, functionBody, NULL, 1, &exception);
    ASSERT(function && !exception);
    JSValueRef arguments[] = { JSValueMakeNumber(context, 2) };
    v = JSObjectCallAsFunction(context, function, NULL, 1, arguments, &exception);
    JSStringRelease(foo);
    JSStringRelease(functionBody);
    
    string = JSValueToStringCopy(context, function, NULL);
    assertEqualsAsUTF8String(JSValueMakeString(context, string), "function foo(foo) \n{\n  return foo;\n}");
    JSStringRelease(string);

    JSStringRef print = JSStringCreateWithUTF8CString("print");
    JSObjectRef printFunction = JSObjectMakeFunctionWithCallback(context, print, print_callAsFunction);
    JSObjectSetProperty(context, globalObject, print, printFunction, kJSPropertyAttributeNone, NULL); 
    JSStringRelease(print);
    
    ASSERT(!JSObjectSetPrivate(printFunction, (void*)1));
    ASSERT(!JSObjectGetPrivate(printFunction));

    JSStringRef myConstructorIString = JSStringCreateWithUTF8CString("MyConstructor");
    JSObjectRef myConstructor = JSObjectMakeConstructor(context, NULL, myConstructor_callAsConstructor);
    JSObjectSetProperty(context, globalObject, myConstructorIString, myConstructor, kJSPropertyAttributeNone, NULL);
    JSStringRelease(myConstructorIString);
    
    ASSERT(!JSObjectSetPrivate(myConstructor, (void*)1));
    ASSERT(!JSObjectGetPrivate(myConstructor));
    
    string = JSStringCreateWithUTF8CString("Derived");
    JSObjectRef derivedConstructor = JSObjectMakeConstructor(context, Derived_class(context), NULL);
    JSObjectSetProperty(context, globalObject, string, derivedConstructor, kJSPropertyAttributeNone, NULL);
    JSStringRelease(string);
    
    o = JSObjectMake(context, NULL, NULL);
    JSObjectSetProperty(context, o, jsOneIString, JSValueMakeNumber(context, 1), kJSPropertyAttributeNone, NULL);
    JSObjectSetProperty(context, o, jsCFIString,  JSValueMakeNumber(context, 1), kJSPropertyAttributeDontEnum, NULL);
    JSPropertyNameArrayRef nameArray = JSObjectCopyPropertyNames(context, o);
    size_t expectedCount = JSPropertyNameArrayGetCount(nameArray);
    size_t count;
    for (count = 0; count < expectedCount; ++count)
        JSPropertyNameArrayGetNameAtIndex(nameArray, count);
    JSPropertyNameArrayRelease(nameArray);
    ASSERT(count == 1); // jsCFString should not be enumerated

    JSClassDefinition nullDefinition = kJSClassDefinitionEmpty;
    nullDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
    JSClassRef nullClass = JSClassCreate(&nullDefinition);
    JSClassRelease(nullClass);
    
    nullDefinition = kJSClassDefinitionEmpty;
    nullClass = JSClassCreate(&nullDefinition);
    JSClassRelease(nullClass);

    functionBody = JSStringCreateWithUTF8CString("return this;");
    function = JSObjectMakeFunction(context, NULL, 0, NULL, functionBody, NULL, 1, NULL);
    JSStringRelease(functionBody);
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, NULL);
    ASSERT(JSValueIsEqual(context, v, globalObject, NULL));
    v = JSObjectCallAsFunction(context, function, o, 0, NULL, NULL);
    ASSERT(JSValueIsEqual(context, v, o, NULL));
    
    char* scriptUTF8 = createStringWithContentsOfFile("testapi.js");
    if (!scriptUTF8)
        printf("FAIL: Test script could not be loaded.\n");
    else {
        JSStringRef script = JSStringCreateWithUTF8CString(scriptUTF8);
        result = JSEvaluateScript(context, script, NULL, NULL, 1, &exception);
        if (JSValueIsUndefined(context, result))
            printf("PASS: Test script executed successfully.\n");
        else {
            printf("FAIL: Test script returned unexpected value:\n");
            JSStringRef exceptionIString = JSValueToStringCopy(context, exception, NULL);
            CFStringRef exceptionCF = JSStringCopyCFString(kCFAllocatorDefault, exceptionIString);
            CFShow(exceptionCF);
            CFRelease(exceptionCF);
            JSStringRelease(exceptionIString);
        }
        JSStringRelease(script);
        free(scriptUTF8);
    }

    // Clear out local variables pointing at JSObjectRefs to allow their values to be collected
    function = NULL;
    v = NULL;
    o = NULL;
    globalObject = NULL;

    JSStringRelease(jsEmptyIString);
    JSStringRelease(jsOneIString);
    JSStringRelease(jsCFIString);
    JSStringRelease(jsCFEmptyIString);
    JSStringRelease(jsCFIStringWithCharacters);
    JSStringRelease(jsCFEmptyIStringWithCharacters);
    JSStringRelease(goodSyntax);
    JSStringRelease(badSyntax);

    JSGlobalContextRelease(context);
    JSGarbageCollect(context);
    JSClassRelease(globalObjectClass);

    printf("PASS: Program exited normally.\n");
    return 0;
}

static char* createStringWithContentsOfFile(const char* fileName)
{
    char* buffer;
    
    size_t buffer_size = 0;
    size_t buffer_capacity = 1024;
    buffer = (char*)malloc(buffer_capacity);
    
    FILE* f = fopen(fileName, "r");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName);
        return 0;
    }
    
    while (!feof(f) && !ferror(f)) {
        buffer_size += fread(buffer + buffer_size, 1, buffer_capacity - buffer_size, f);
        if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
            buffer_capacity *= 2;
            buffer = (char*)realloc(buffer, buffer_capacity);
            ASSERT(buffer);
        }
        
        ASSERT(buffer_size < buffer_capacity);
    }
    fclose(f);
    buffer[buffer_size] = '\0';
    
    return buffer;
}
