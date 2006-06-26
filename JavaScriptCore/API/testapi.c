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
#include <wtf/UnusedParam.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <assert.h>
#include <math.h>

static JSContextRef context = 0;

static void assertEqualsAsBoolean(JSValueRef value, bool expectedValue)
{
    if (JSValueToBoolean(context, value) != expectedValue)
        fprintf(stderr, "assertEqualsAsBoolean failed: %p, %d\n", value, expectedValue);
}

static void assertEqualsAsNumber(JSValueRef value, double expectedValue)
{
    double number = JSValueToNumber(context, value);
    if (number != expectedValue && !(isnan(number) && isnan(expectedValue)))
        fprintf(stderr, "assertEqualsAsNumber failed: %p, %lf\n", value, expectedValue);
}

static void assertEqualsAsUTF8String(JSValueRef value, const char* expectedValue)
{
    JSCharBufferRef valueAsString = JSValueCopyStringValue(context, value);

    size_t jsSize = JSCharBufferGetMaxLengthUTF8(valueAsString);
    char jsBuffer[jsSize];
    JSCharBufferGetCharactersUTF8(valueAsString, jsBuffer, jsSize);
    
    if (strcmp(jsBuffer, expectedValue) != 0)
        fprintf(stderr, "assertEqualsAsUTF8String strcmp failed: %s != %s\n", jsBuffer, expectedValue);
        
    if (jsSize < strlen(jsBuffer) + 1)
        fprintf(stderr, "assertEqualsAsUTF8String failed: jsSize was too small\n");

    JSCharBufferRelease(valueAsString);
}

#if defined(__APPLE__)
static void assertEqualsAsCharactersPtr(JSValueRef value, const char* expectedValue)
{
    JSCharBufferRef valueAsString = JSValueCopyStringValue(context, value);

    size_t jsLength = JSCharBufferGetLength(valueAsString);
    const JSChar* jsBuffer = JSCharBufferGetCharactersPtr(valueAsString);

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
    
    JSCharBufferRelease(valueAsString);
}

static void assertEqualsAsCharacters(JSValueRef value, const char* expectedValue)
{
    JSCharBufferRef valueAsString = JSValueCopyStringValue(context, value);
    
    size_t jsLength = JSCharBufferGetLength(valueAsString);
    JSChar jsBuffer[jsLength];
    JSCharBufferGetCharacters(valueAsString, jsBuffer, jsLength);
    
    CFStringRef expectedValueAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                                    expectedValue,
                                                                    kCFStringEncodingUTF8);    
    CFIndex cfLength = CFStringGetLength(expectedValueAsCFString);
    UniChar cfBuffer[cfLength];
    CFStringGetCharacters(expectedValueAsCFString, CFRangeMake(0, cfLength), cfBuffer);
    CFRelease(expectedValueAsCFString);
    
    assert(memcmp(jsBuffer, cfBuffer, cfLength * sizeof(UniChar)) == 0);
    assert(jsLength == (size_t)cfLength);
    
    JSCharBufferRelease(valueAsString);
}
#endif // __APPLE__

static JSValueRef jsGlobalValue; // non-stack value for testing JSGCProtect()

/* MyObject pseudo-class */

static bool didInitialize = false;
static void MyObject_initialize(JSObjectRef object)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    didInitialize = true;
}

static JSCharBufferRef MyObject_copyDescription(JSObjectRef object)
{
    UNUSED_PARAM(object);
    return JSCharBufferCreateUTF8("MyObject");
}

static bool MyObject_hasProperty(JSObjectRef object, JSCharBufferRef propertyName)
{
    UNUSED_PARAM(object);

    if (JSCharBufferIsEqualUTF8(propertyName, "alwaysOne")
        || JSCharBufferIsEqualUTF8(propertyName, "cantFind")
        || JSCharBufferIsEqualUTF8(propertyName, "myPropertyName")) {
        return true;
    }
    
    return false;
}

static bool MyObject_getProperty(JSObjectRef object, JSCharBufferRef propertyName, JSValueRef* returnValue)
{
    UNUSED_PARAM(object);
    
    if (JSCharBufferIsEqualUTF8(propertyName, "alwaysOne")) {
        *returnValue = JSNumberMake(1);
        return true;
    }
    
    if (JSCharBufferIsEqualUTF8(propertyName, "myPropertyName")) {
        *returnValue = JSNumberMake(1);
        return true;
    }

    if (JSCharBufferIsEqualUTF8(propertyName, "cantFind")) {
        *returnValue = JSUndefinedMake();
        return true;
    }
    
    return false;
}

static bool MyObject_setProperty(JSObjectRef object, JSCharBufferRef propertyName, JSValueRef value)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(value);

    if (JSCharBufferIsEqualUTF8(propertyName, "cantSet"))
        return true; // pretend we set the property in order to swallow it
    
    return false;
}

static bool MyObject_deleteProperty(JSObjectRef object, JSCharBufferRef propertyName)
{
    UNUSED_PARAM(object);
    
    if (JSCharBufferIsEqualUTF8(propertyName, "cantDelete"))
        return true;
    
    return false;
}

static void MyObject_getPropertyList(JSObjectRef object, JSPropertyListRef propertyList)
{
    JSCharBufferRef propertyNameBuf;
    
    propertyNameBuf = JSCharBufferCreateUTF8("alwaysOne");
    JSPropertyListAdd(propertyList, object, propertyNameBuf);
    JSCharBufferRelease(propertyNameBuf);
    
    propertyNameBuf = JSCharBufferCreateUTF8("myPropertyName");
    JSPropertyListAdd(propertyList, object, propertyNameBuf);
    JSCharBufferRelease(propertyNameBuf);
}

static JSValueRef MyObject_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[])
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(thisObject);

    if (argc > 0 && JSValueIsStrictEqual(context, argv[0], JSNumberMake(0)))
        return JSNumberMake(1);
    
    return JSUndefinedMake();
}

static JSObjectRef MyObject_callAsConstructor(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[])
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (argc > 0 && JSValueIsStrictEqual(context, argv[0], JSNumberMake(0)))
        return JSValueToObject(context, JSNumberMake(1));
    
    return JSValueToObject(context, JSNumberMake(0));
}

static bool MyObject_convertToType(JSObjectRef object, JSTypeCode typeCode, JSValueRef* returnValue)
{
    UNUSED_PARAM(object);
    
    switch (typeCode) {
    case kJSTypeBoolean:
        *returnValue = JSBooleanMake(false); // default object conversion is 'true'
        return true;
    case kJSTypeNumber:
        *returnValue = JSNumberMake(1);
        return true;
    default:
        break;
    }

    // string
    return false;
}

static bool didFinalize = false;
static void MyObject_finalize(JSObjectRef object)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    didFinalize = true;
}

JSObjectCallbacks MyObjectCallbacks = {
    0,
    0,
    &MyObject_initialize,
    &MyObject_finalize,
    &MyObject_copyDescription,
    &MyObject_hasProperty,
    &MyObject_getProperty,
    &MyObject_setProperty,
    &MyObject_deleteProperty,
    &MyObject_getPropertyList,
    &MyObject_callAsFunction,
    &MyObject_callAsConstructor,
    &MyObject_convertToType,
};

static JSValueRef print_callAsFunction(JSContextRef context, JSObjectRef functionObject, JSObjectRef thisObject, size_t argc, JSValueRef argv[])
{
    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);
    
    if (argc > 0) {
        JSCharBufferRef string = JSValueCopyStringValue(context, argv[0]);
        size_t sizeUTF8 = JSCharBufferGetMaxLengthUTF8(string);
        char stringUTF8[sizeUTF8];
        JSCharBufferGetCharactersUTF8(string, stringUTF8, sizeUTF8);
        printf("%s\n", stringUTF8);
        JSCharBufferRelease(string);
    }
    
    return JSUndefinedMake();
}

static JSObjectRef myConstructor_callAsConstructor(JSContextRef context, JSObjectRef constructorObject, size_t argc, JSValueRef argv[])
{
    UNUSED_PARAM(constructorObject);
    
    JSObjectRef result = JSObjectMake(context, &kJSObjectCallbacksNone, 0);
    if (argc > 0) {
        JSCharBufferRef valueBuffer = JSCharBufferCreateUTF8("value");
        JSObjectSetProperty(context, result, valueBuffer, argv[0], kJSPropertyAttributeNone);
        JSCharBufferRelease(valueBuffer);
    }
    
    return result;
}

static char* createStringWithContentsOfFile(const char* fileName);

int main(int argc, char* argv[])
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);
    
    context = JSContextCreate(&kJSObjectCallbacksNone, 0);

    JSValueRef jsUndefined = JSUndefinedMake();
    JSValueRef jsNull = JSNullMake();
    JSValueRef jsTrue = JSBooleanMake(true);
    JSValueRef jsFalse = JSBooleanMake(false);
    JSValueRef jsZero = JSNumberMake(0);
    JSValueRef jsOne = JSNumberMake(1);
    JSValueRef jsOneThird = JSNumberMake(1.0 / 3.0);
    // FIXME: test funny utf8 characters
    JSCharBufferRef jsEmptyStringBuf = JSCharBufferCreateUTF8("");
    JSValueRef jsEmptyString = JSStringMake(jsEmptyStringBuf);
    
    JSCharBufferRef jsOneStringBuf = JSCharBufferCreateUTF8("1");
    JSValueRef jsOneString = JSStringMake(jsOneStringBuf);

#if defined(__APPLE__)
    UniChar singleUniChar = 65; // Capital A
    CFMutableStringRef cfString = 
        CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault,
                                                          &singleUniChar,
                                                          1,
                                                          1,
                                                          kCFAllocatorNull);

    JSCharBufferRef jsCFStringBuf = JSCharBufferCreateWithCFString(cfString);
    JSValueRef jsCFString = JSStringMake(jsCFStringBuf);
    
    CFStringRef cfEmptyString = CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
    
    JSCharBufferRef jsCFEmptyStringBuf = JSCharBufferCreateWithCFString(cfEmptyString);
    JSValueRef jsCFEmptyString = JSStringMake(jsCFEmptyStringBuf);

    CFIndex cfStringLength = CFStringGetLength(cfString);
    UniChar buffer[cfStringLength];
    CFStringGetCharacters(cfString, 
                          CFRangeMake(0, cfStringLength), 
                          buffer);
    JSCharBufferRef jsCFStringWithCharactersBuf = JSCharBufferCreate(buffer, cfStringLength);
    JSValueRef jsCFStringWithCharacters = JSStringMake(jsCFStringWithCharactersBuf);
    
    JSCharBufferRef jsCFEmptyStringWithCharactersBuf = JSCharBufferCreate(buffer, CFStringGetLength(cfEmptyString));
    JSValueRef jsCFEmptyStringWithCharacters = JSStringMake(jsCFEmptyStringWithCharactersBuf);
#endif // __APPLE__

    assert(JSValueGetType(jsUndefined) == kJSTypeUndefined);
    assert(JSValueGetType(jsNull) == kJSTypeNull);
    assert(JSValueGetType(jsTrue) == kJSTypeBoolean);
    assert(JSValueGetType(jsFalse) == kJSTypeBoolean);
    assert(JSValueGetType(jsZero) == kJSTypeNumber);
    assert(JSValueGetType(jsOne) == kJSTypeNumber);
    assert(JSValueGetType(jsOneThird) == kJSTypeNumber);
    assert(JSValueGetType(jsEmptyString) == kJSTypeString);
    assert(JSValueGetType(jsOneString) == kJSTypeString);
#if defined(__APPLE__)
    assert(JSValueGetType(jsCFString) == kJSTypeString);
    assert(JSValueGetType(jsCFStringWithCharacters) == kJSTypeString);
    assert(JSValueGetType(jsCFEmptyString) == kJSTypeString);
    assert(JSValueGetType(jsCFEmptyStringWithCharacters) == kJSTypeString);
#endif // __APPLE__

    assertEqualsAsBoolean(jsUndefined, false);
    assertEqualsAsBoolean(jsNull, false);
    assertEqualsAsBoolean(jsTrue, true);
    assertEqualsAsBoolean(jsFalse, false);
    assertEqualsAsBoolean(jsZero, false);
    assertEqualsAsBoolean(jsOne, true);
    assertEqualsAsBoolean(jsOneThird, true);
    assertEqualsAsBoolean(jsEmptyString, false);
    assertEqualsAsBoolean(jsOneString, true);
#if defined(__APPLE__)
    assertEqualsAsBoolean(jsCFString, true);
    assertEqualsAsBoolean(jsCFStringWithCharacters, true);
    assertEqualsAsBoolean(jsCFEmptyString, false);
    assertEqualsAsBoolean(jsCFEmptyStringWithCharacters, false);
#endif // __APPLE__
    
    assertEqualsAsNumber(jsUndefined, nan(""));
    assertEqualsAsNumber(jsNull, 0);
    assertEqualsAsNumber(jsTrue, 1);
    assertEqualsAsNumber(jsFalse, 0);
    assertEqualsAsNumber(jsZero, 0);
    assertEqualsAsNumber(jsOne, 1);
    assertEqualsAsNumber(jsOneThird, 1.0 / 3.0);
    assertEqualsAsNumber(jsEmptyString, 0);
    assertEqualsAsNumber(jsOneString, 1);
#if defined(__APPLE__)
    assertEqualsAsNumber(jsCFString, nan(""));
    assertEqualsAsNumber(jsCFStringWithCharacters, nan(""));
    assertEqualsAsNumber(jsCFEmptyString, 0);
    assertEqualsAsNumber(jsCFEmptyStringWithCharacters, 0);
    assert(sizeof(JSChar) == sizeof(UniChar));
#endif // __APPLE__
    
    assertEqualsAsCharactersPtr(jsUndefined, "undefined");
    assertEqualsAsCharactersPtr(jsNull, "null");
    assertEqualsAsCharactersPtr(jsTrue, "true");
    assertEqualsAsCharactersPtr(jsFalse, "false");
    assertEqualsAsCharactersPtr(jsZero, "0");
    assertEqualsAsCharactersPtr(jsOne, "1");
    assertEqualsAsCharactersPtr(jsOneThird, "0.3333333333333333");
    assertEqualsAsCharactersPtr(jsEmptyString, "");
    assertEqualsAsCharactersPtr(jsOneString, "1");
#if defined(__APPLE__)
    assertEqualsAsCharactersPtr(jsCFString, "A");
    assertEqualsAsCharactersPtr(jsCFStringWithCharacters, "A");
    assertEqualsAsCharactersPtr(jsCFEmptyString, "");
    assertEqualsAsCharactersPtr(jsCFEmptyStringWithCharacters, "");
#endif // __APPLE__
    
    assertEqualsAsCharacters(jsUndefined, "undefined");
    assertEqualsAsCharacters(jsNull, "null");
    assertEqualsAsCharacters(jsTrue, "true");
    assertEqualsAsCharacters(jsFalse, "false");
    assertEqualsAsCharacters(jsZero, "0");
    assertEqualsAsCharacters(jsOne, "1");
    assertEqualsAsCharacters(jsOneThird, "0.3333333333333333");
    assertEqualsAsCharacters(jsEmptyString, "");
    assertEqualsAsCharacters(jsOneString, "1");
#if defined(__APPLE__)
    assertEqualsAsCharacters(jsCFString, "A");
    assertEqualsAsCharacters(jsCFStringWithCharacters, "A");
    assertEqualsAsCharacters(jsCFEmptyString, "");
    assertEqualsAsCharacters(jsCFEmptyStringWithCharacters, "");
#endif // __APPLE__
    
    assertEqualsAsUTF8String(jsUndefined, "undefined");
    assertEqualsAsUTF8String(jsNull, "null");
    assertEqualsAsUTF8String(jsTrue, "true");
    assertEqualsAsUTF8String(jsFalse, "false");
    assertEqualsAsUTF8String(jsZero, "0");
    assertEqualsAsUTF8String(jsOne, "1");
    assertEqualsAsUTF8String(jsOneThird, "0.3333333333333333");
    assertEqualsAsUTF8String(jsEmptyString, "");
    assertEqualsAsUTF8String(jsOneString, "1");
#if defined(__APPLE__)
    assertEqualsAsUTF8String(jsCFString, "A");
    assertEqualsAsUTF8String(jsCFStringWithCharacters, "A");
    assertEqualsAsUTF8String(jsCFEmptyString, "");
    assertEqualsAsUTF8String(jsCFEmptyStringWithCharacters, "");
#endif // __APPLE__
    
    assert(JSValueIsStrictEqual(context, jsTrue, jsTrue));
    assert(!JSValueIsStrictEqual(context, jsOne, jsOneString));

    assert(JSValueIsEqual(context, jsOne, jsOneString));
    assert(!JSValueIsEqual(context, jsTrue, jsFalse));
    
#if defined(__APPLE__)
    CFStringRef cfJSString = CFStringCreateWithJSCharBuffer(kCFAllocatorDefault, jsCFStringBuf);
    CFStringRef cfJSEmptyString = CFStringCreateWithJSCharBuffer(kCFAllocatorDefault, jsCFEmptyStringBuf);
    assert(CFEqual(cfJSString, cfString));
    assert(CFEqual(cfJSEmptyString, cfEmptyString));
    CFRelease(cfJSString);
    CFRelease(cfJSEmptyString);

    CFRelease(cfString);
    CFRelease(cfEmptyString);
#endif // __APPLE__
    
    // GDB says jsGlobalValue actually ends up being marked by the stack crawl, so this
    // exercise is a bit academic. Not sure why that happens, or how to avoid it.
    jsGlobalValue = JSObjectMake(context, &kJSObjectCallbacksNone, 0);
    JSGCCollect();
    assert(JSValueIsObject(jsGlobalValue));
    JSGCUnprotect(jsGlobalValue);

    /* JSInterpreter.h */
    
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    assert(JSValueIsObject(globalObject));

    JSCharBufferRef goodSyntaxBuf = JSCharBufferCreateUTF8("x = 1;");
    JSCharBufferRef badSyntaxBuf = JSCharBufferCreateUTF8("x := 1;");
    assert(JSCheckSyntax(context, goodSyntaxBuf));
    assert(!JSCheckSyntax(context, badSyntaxBuf));

    JSValueRef result;
    assert(JSEvaluate(context, globalObject, goodSyntaxBuf, jsEmptyString, 0, &result));
    assert(JSValueIsEqual(context, result, jsOne));
    
    assert(!JSEvaluate(context, globalObject, badSyntaxBuf, jsEmptyString, 0, &result));
    assert(JSValueIsObject(result));
    
    JSContextSetException(context, JSNumberMake(1));
    assert(JSContextHasException(context));
    assert(JSValueIsEqual(context, jsOne, JSContextGetException(context)));
    JSContextClearException(context);
    assert(!JSContextHasException(context));

    JSCharBufferRelease(jsEmptyStringBuf);
    JSCharBufferRelease(jsOneStringBuf);
#if defined(__APPLE__)
    JSCharBufferRelease(jsCFStringBuf);
    JSCharBufferRelease(jsCFEmptyStringBuf);
    JSCharBufferRelease(jsCFStringWithCharactersBuf);
    JSCharBufferRelease(jsCFEmptyStringWithCharactersBuf);
#endif // __APPLE__
    JSCharBufferRelease(goodSyntaxBuf);
    JSCharBufferRelease(badSyntaxBuf);

    JSObjectRef myObject = JSObjectMake(context, &MyObjectCallbacks, 0);
    assert(didInitialize);
    JSCharBufferRef myObjectBuf = JSCharBufferCreateUTF8("MyObject");
    JSObjectSetProperty(context, globalObject, myObjectBuf, myObject, kJSPropertyAttributeNone);
    JSCharBufferRelease(myObjectBuf);

    JSCharBufferRef printBuf = JSCharBufferCreateUTF8("print");
    JSObjectSetProperty(context, globalObject, printBuf, JSFunctionMake(context, print_callAsFunction), kJSPropertyAttributeNone); 
    JSCharBufferRelease(printBuf);

    JSCharBufferRef myConstructorBuf = JSCharBufferCreateUTF8("MyConstructor");
    JSObjectSetProperty(context, globalObject, myConstructorBuf, JSConstructorMake(context, myConstructor_callAsConstructor), kJSPropertyAttributeNone); 
    JSCharBufferRelease(myConstructorBuf);

    char* script = createStringWithContentsOfFile("testapi.js");
    JSCharBufferRef scriptBuf = JSCharBufferCreateUTF8(script);
    JSEvaluate(context, globalObject, scriptBuf, jsEmptyString, 0, &result);
    if (JSValueIsUndefined(result))
        printf("PASS: Test script executed successfully.\n");
    else {
        printf("FAIL: Test script returned unexcpected value:\n");
        JSCharBufferRef resultBuf = JSValueCopyStringValue(context, result);
        CFStringRef resultCF = CFStringCreateWithJSCharBuffer(kCFAllocatorDefault, resultBuf);
        CFShow(resultCF);
        CFRelease(resultCF);
        JSCharBufferRelease(resultBuf);
    }
    JSCharBufferRelease(scriptBuf);
    free(script);

    // Allocate a few dummies so that at least one will be collected
    JSObjectMake(context, &MyObjectCallbacks, 0);
    JSObjectMake(context, &MyObjectCallbacks, 0);
    JSGCCollect();
    assert(didFinalize);

    JSContextDestroy(context);
    printf("PASS: All assertions passed.\n");
    return 0;
}

static char* createStringWithContentsOfFile(const char* fileName)
{
    char* buffer;
    
    int buffer_size = 0;
    int buffer_capacity = 1024;
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
            assert(buffer);
        }
        
        assert(buffer_size < buffer_capacity);
    }
    fclose(f);
    buffer[buffer_size] = '\0';
    
    return buffer;
}
