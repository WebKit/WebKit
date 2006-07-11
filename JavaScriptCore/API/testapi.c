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
    JSInternalStringRef valueAsString = JSValueCopyStringValue(context, value);

    size_t jsSize = JSInternalStringGetMaxLengthUTF8(valueAsString);
    char jsBuffer[jsSize];
    JSInternalStringGetCharactersUTF8(valueAsString, jsBuffer, jsSize);
    
    if (strcmp(jsBuffer, expectedValue) != 0)
        fprintf(stderr, "assertEqualsAsUTF8String strcmp failed: %s != %s\n", jsBuffer, expectedValue);
        
    if (jsSize < strlen(jsBuffer) + 1)
        fprintf(stderr, "assertEqualsAsUTF8String failed: jsSize was too small\n");

    JSInternalStringRelease(valueAsString);
}

#if defined(__APPLE__)
static void assertEqualsAsCharactersPtr(JSValueRef value, const char* expectedValue)
{
    JSInternalStringRef valueAsString = JSValueCopyStringValue(context, value);

    size_t jsLength = JSInternalStringGetLength(valueAsString);
    const JSChar* jsBuffer = JSInternalStringGetCharactersPtr(valueAsString);

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
    
    JSInternalStringRelease(valueAsString);
}

static void assertEqualsAsCharacters(JSValueRef value, const char* expectedValue)
{
    JSInternalStringRef valueAsString = JSValueCopyStringValue(context, value);
    
    size_t jsLength = JSInternalStringGetLength(valueAsString);
    JSChar jsBuffer[jsLength];
    JSInternalStringGetCharacters(valueAsString, jsBuffer, jsLength);
    
    CFStringRef expectedValueAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                                    expectedValue,
                                                                    kCFStringEncodingUTF8);    
    CFIndex cfLength = CFStringGetLength(expectedValueAsCFString);
    UniChar cfBuffer[cfLength];
    CFStringGetCharacters(expectedValueAsCFString, CFRangeMake(0, cfLength), cfBuffer);
    CFRelease(expectedValueAsCFString);
    
    if (memcmp(jsBuffer, cfBuffer, cfLength * sizeof(UniChar)) != 0)
        fprintf(stderr, "assertEqualsAsCharacters failed: jsBuffer != cfBuffer\n");
    
    if (jsLength != (size_t)cfLength)
        fprintf(stderr, "assertEqualsAsCharacters failed: jsLength(%ld) != cfLength(%ld)\n", jsLength, cfLength);
    
    JSInternalStringRelease(valueAsString);
}
#endif // __APPLE__

static JSValueRef jsGlobalValue; // non-stack value for testing JSGCProtect()

/* MyObject pseudo-class */

static bool didInitialize = false;
static void MyObject_initialize(JSContextRef context, JSObjectRef object, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    didInitialize = true;
}

static bool MyObject_hasProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (JSInternalStringIsEqualUTF8(propertyName, "alwaysOne")
        || JSInternalStringIsEqualUTF8(propertyName, "cantFind")
        || JSInternalStringIsEqualUTF8(propertyName, "myPropertyName")) {
        return true;
    }
    
    return false;
}

static bool MyObject_getProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* returnValue, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    if (JSInternalStringIsEqualUTF8(propertyName, "alwaysOne")) {
        *returnValue = JSNumberMake(1);
        return true;
    }
    
    if (JSInternalStringIsEqualUTF8(propertyName, "myPropertyName")) {
        *returnValue = JSNumberMake(1);
        return true;
    }

    if (JSInternalStringIsEqualUTF8(propertyName, "cantFind")) {
        *returnValue = JSUndefinedMake();
        return true;
    }
    
    return false;
}

static bool MyObject_setProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(value);

    if (JSInternalStringIsEqualUTF8(propertyName, "cantSet"))
        return true; // pretend we set the property in order to swallow it
    
    return false;
}

static bool MyObject_deleteProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    if (JSInternalStringIsEqualUTF8(propertyName, "cantDelete"))
        return true;
    
    return false;
}

static void MyObject_getPropertyList(JSContextRef context, JSObjectRef object, JSPropertyListRef propertyList, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    
    JSInternalStringRef propertyName;
    
    propertyName = JSInternalStringCreateUTF8("alwaysOne");
    JSPropertyListAdd(propertyList, object, propertyName);
    JSInternalStringRelease(propertyName);
    
    propertyName = JSInternalStringCreateUTF8("myPropertyName");
    JSPropertyListAdd(propertyList, object, propertyName);
    JSInternalStringRelease(propertyName);
}

static JSValueRef MyObject_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(thisObject);

    if (argc > 0 && JSValueIsStrictEqual(context, argv[0], JSNumberMake(0)))
        return JSNumberMake(1);
    
    return JSUndefinedMake();
}

static JSObjectRef MyObject_callAsConstructor(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (argc > 0 && JSValueIsStrictEqual(context, argv[0], JSNumberMake(0)))
        return JSValueToObject(context, JSNumberMake(1));
    
    return JSValueToObject(context, JSNumberMake(0));
}

static bool MyObject_convertToType(JSContextRef context, JSObjectRef object, JSTypeCode typeCode, JSValueRef* returnValue, JSValueRef* exception)
{
    UNUSED_PARAM(context);
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

JSObjectCallbacks MyObject_callbacks = {
    0,
    &MyObject_initialize,
    &MyObject_finalize,
    &MyObject_hasProperty,
    &MyObject_getProperty,
    &MyObject_setProperty,
    &MyObject_deleteProperty,
    &MyObject_getPropertyList,
    &MyObject_callAsFunction,
    &MyObject_callAsConstructor,
    &MyObject_convertToType,
};

static JSClassRef MyObject_class(JSContextRef context)
{
    static JSClassRef jsClass;
    if (!jsClass) {
        jsClass = JSClassCreate(NULL, NULL, &MyObject_callbacks, NULL);
    }
    
    return jsClass;
}

static JSValueRef print_callAsFunction(JSContextRef context, JSObjectRef functionObject, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);
    
    if (argc > 0) {
        JSInternalStringRef string = JSValueCopyStringValue(context, argv[0]);
        size_t sizeUTF8 = JSInternalStringGetMaxLengthUTF8(string);
        char stringUTF8[sizeUTF8];
        JSInternalStringGetCharactersUTF8(string, stringUTF8, sizeUTF8);
        printf("%s\n", stringUTF8);
        JSInternalStringRelease(string);
    }
    
    return JSUndefinedMake();
}

static JSObjectRef myConstructor_callAsConstructor(JSContextRef context, JSObjectRef constructorObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(constructorObject);
    
    JSObjectRef result = JSObjectMake(context, NULL, 0);
    if (argc > 0) {
        JSInternalStringRef value = JSInternalStringCreateUTF8("value");
        JSObjectSetProperty(context, result, value, argv[0], kJSPropertyAttributeNone);
        JSInternalStringRelease(value);
    }
    
    return result;
}

static char* createStringWithContentsOfFile(const char* fileName);

int main(int argc, char* argv[])
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);
    
    context = JSContextCreate(NULL);

    JSValueRef jsUndefined = JSUndefinedMake();
    JSValueRef jsNull = JSNullMake();
    JSValueRef jsTrue = JSBooleanMake(true);
    JSValueRef jsFalse = JSBooleanMake(false);
    JSValueRef jsZero = JSNumberMake(0);
    JSValueRef jsOne = JSNumberMake(1);
    JSValueRef jsOneThird = JSNumberMake(1.0 / 3.0);
    JSObjectRef jsObjectNoProto = JSObjectMake(context, NULL, JSNullMake());

    // FIXME: test funny utf8 characters
    JSInternalStringRef jsEmptyIString = JSInternalStringCreateUTF8("");
    JSValueRef jsEmptyString = JSStringMake(jsEmptyIString);
    
    JSInternalStringRef jsOneIString = JSInternalStringCreateUTF8("1");
    JSValueRef jsOneString = JSStringMake(jsOneIString);

#if defined(__APPLE__)
    UniChar singleUniChar = 65; // Capital A
    CFMutableStringRef cfString = 
        CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault,
                                                          &singleUniChar,
                                                          1,
                                                          1,
                                                          kCFAllocatorNull);

    JSInternalStringRef jsCFIString = JSInternalStringCreateCF(cfString);
    JSValueRef jsCFString = JSStringMake(jsCFIString);
    
    CFStringRef cfEmptyString = CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
    
    JSInternalStringRef jsCFEmptyIString = JSInternalStringCreateCF(cfEmptyString);
    JSValueRef jsCFEmptyString = JSStringMake(jsCFEmptyIString);

    CFIndex cfStringLength = CFStringGetLength(cfString);
    UniChar buffer[cfStringLength];
    CFStringGetCharacters(cfString, 
                          CFRangeMake(0, cfStringLength), 
                          buffer);
    JSInternalStringRef jsCFIStringWithCharacters = JSInternalStringCreate(buffer, cfStringLength);
    JSValueRef jsCFStringWithCharacters = JSStringMake(jsCFIStringWithCharacters);
    
    JSInternalStringRef jsCFEmptyIStringWithCharacters = JSInternalStringCreate(buffer, CFStringGetLength(cfEmptyString));
    JSValueRef jsCFEmptyStringWithCharacters = JSStringMake(jsCFEmptyIStringWithCharacters);
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

    // Conversions that throw exceptions
    assert(NULL == JSValueToObject(context, jsNull));
    assert(isnan(JSValueToNumber(context, jsObjectNoProto)));
    assertEqualsAsCharactersPtr(jsObjectNoProto, "");

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
    CFStringRef cfJSString = CFStringCreateWithJSInternalString(kCFAllocatorDefault, jsCFIString);
    CFStringRef cfJSEmptyString = CFStringCreateWithJSInternalString(kCFAllocatorDefault, jsCFEmptyIString);
    assert(CFEqual(cfJSString, cfString));
    assert(CFEqual(cfJSEmptyString, cfEmptyString));
    CFRelease(cfJSString);
    CFRelease(cfJSEmptyString);

    CFRelease(cfString);
    CFRelease(cfEmptyString);
#endif // __APPLE__
    
    jsGlobalValue = JSObjectMake(context, NULL, NULL);
    JSGCProtect(jsGlobalValue);
    JSGCCollect();
    assert(JSValueIsObject(jsGlobalValue));
    JSGCUnprotect(jsGlobalValue);

    /* JSInterpreter.h */
    
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    assert(JSValueIsObject(globalObject));

    JSInternalStringRef goodSyntax = JSInternalStringCreateUTF8("x = 1;");
    JSInternalStringRef badSyntax = JSInternalStringCreateUTF8("x := 1;");
    assert(JSCheckSyntax(context, goodSyntax, NULL, 0, NULL));
    assert(!JSCheckSyntax(context, badSyntax, NULL, 0, NULL));

    JSValueRef result;
    JSValueRef exception;
    JSValueRef v;
    JSObjectRef o;

    result = JSEvaluate(context, goodSyntax, NULL, NULL, 1, NULL);
    assert(result);
    assert(JSValueIsEqual(context, result, jsOne));

    exception = NULL;
    result = JSEvaluate(context, badSyntax, NULL, NULL, 1, &exception);
    assert(!result);
    assert(JSValueIsObject(exception));
    
    JSInternalStringRef array = JSInternalStringCreateUTF8("Array");
    v = JSObjectGetProperty(context, globalObject, array);
    assert(v);
    JSObjectRef arrayConstructor = JSValueToObject(context, v);
    JSInternalStringRelease(array);
    result = JSObjectCallAsConstructor(context, arrayConstructor, 0, NULL, NULL);
    assert(result);
    assert(JSValueIsInstanceOf(context, result, arrayConstructor));
    assert(!JSValueIsInstanceOf(context, JSNullMake(), arrayConstructor));
    
    JSInternalStringRef functionBody;
    
    exception = NULL;
    functionBody = JSInternalStringCreateUTF8("rreturn Array;");
    JSInternalStringRef line = JSInternalStringCreateUTF8("line");
    assert(!JSFunctionMakeWithBody(context, functionBody, NULL, 1, &exception));
    assert(JSValueIsObject(exception));
    v = JSObjectGetProperty(context, JSValueToObject(context, exception), line);
    assert(v);
    assertEqualsAsNumber(v, 2); // FIXME: Lexer::setCode bumps startingLineNumber by 1 -- we need to change internal callers so that it doesn't have to (saying '0' to mean '1' in the API would be really confusing -- it's really confusing internally, in fact)
    JSInternalStringRelease(functionBody);
    JSInternalStringRelease(line);

    functionBody = JSInternalStringCreateUTF8("return Array;");
    JSObjectRef function = JSFunctionMakeWithBody(context, functionBody, NULL, 1, NULL);
    JSInternalStringRelease(functionBody);

    assert(JSObjectIsFunction(function));
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, NULL);
    assert(JSValueIsEqual(context, v, arrayConstructor));
                                                  
    JSObjectRef myObject = JSObjectMake(context, MyObject_class(context), NULL);
    assert(didInitialize);
    JSInternalStringRef myObjectIString = JSInternalStringCreateUTF8("MyObject");
    JSObjectSetProperty(context, globalObject, myObjectIString, myObject, kJSPropertyAttributeNone);
    JSInternalStringRelease(myObjectIString);

    JSInternalStringRef print = JSInternalStringCreateUTF8("print");
    JSObjectRef printFunction = JSFunctionMake(context, print_callAsFunction);
    JSObjectSetProperty(context, globalObject, print, printFunction, kJSPropertyAttributeNone); 
    JSInternalStringRelease(print);
    
    assert(JSObjectSetPrivate(printFunction, (void*)1));
    assert(JSObjectGetPrivate(printFunction) == (void*)1);

    JSInternalStringRef myConstructorIString = JSInternalStringCreateUTF8("MyConstructor");
    JSObjectRef myConstructor = JSConstructorMake(context, myConstructor_callAsConstructor);
    JSObjectSetProperty(context, globalObject, myConstructorIString, myConstructor, kJSPropertyAttributeNone);
    JSInternalStringRelease(myConstructorIString);
    
    assert(JSObjectSetPrivate(myConstructor, (void*)1));
    assert(JSObjectGetPrivate(myConstructor) == (void*)1);
    
    o = JSObjectMake(context, NULL, NULL);
    JSObjectSetProperty(context, o, jsOneIString, JSNumberMake(1), kJSPropertyAttributeNone);
    JSObjectSetProperty(context, o, jsCFIString,  JSNumberMake(1), kJSPropertyAttributeDontEnum);
    JSPropertyEnumeratorRef enumerator = JSObjectCreatePropertyEnumerator(context, o);
    int count = 0;
    while (JSPropertyEnumeratorGetNext(enumerator))
        ++count;
    JSPropertyEnumeratorRelease(enumerator);
    assert(count == 1); // jsCFString should not be enumerated

    JSClassRef nullCallbacksClass = JSClassCreate(NULL, NULL, NULL, NULL);
    JSClassRelease(nullCallbacksClass);
    
    functionBody = JSInternalStringCreateUTF8("return this;");
    function = JSFunctionMakeWithBody(context, functionBody, NULL, 1, NULL);
    JSInternalStringRelease(functionBody);
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, NULL);
    assert(JSValueIsEqual(context, v, globalObject));
    v = JSObjectCallAsFunction(context, function, o, 0, NULL, NULL);
    assert(JSValueIsEqual(context, v, o));
    
    char* scriptUTF8 = createStringWithContentsOfFile("testapi.js");
    JSInternalStringRef script = JSInternalStringCreateUTF8(scriptUTF8);
    result = JSEvaluate(context, script, NULL, NULL, 1, &exception);
    if (JSValueIsUndefined(result))
        printf("PASS: Test script executed successfully.\n");
    else {
        printf("FAIL: Test script returned unexcpected value:\n");
        JSInternalStringRef exceptionIString = JSValueCopyStringValue(context, exception);
        CFStringRef exceptionCF = CFStringCreateWithJSInternalString(kCFAllocatorDefault, exceptionIString);
        CFShow(exceptionCF);
        CFRelease(exceptionCF);
        JSInternalStringRelease(exceptionIString);
    }
    JSInternalStringRelease(script);
    free(scriptUTF8);

    // Allocate a few dummies so that at least one will be collected
    JSObjectMake(context, MyObject_class(context), 0);
    JSObjectMake(context, MyObject_class(context), 0);
    JSGCCollect();
    assert(didFinalize);

    JSInternalStringRelease(jsEmptyIString);
    JSInternalStringRelease(jsOneIString);
#if defined(__APPLE__)
    JSInternalStringRelease(jsCFIString);
    JSInternalStringRelease(jsCFEmptyIString);
    JSInternalStringRelease(jsCFIStringWithCharacters);
    JSInternalStringRelease(jsCFEmptyIStringWithCharacters);
#endif // __APPLE__
    JSInternalStringRelease(goodSyntax);
    JSInternalStringRelease(badSyntax);
    
    JSContextDestroy(context);
    printf("PASS: Program exited normally.\n");
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
