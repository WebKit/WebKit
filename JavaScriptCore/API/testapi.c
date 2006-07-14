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

static JSGlobalContextRef context = 0;

static void assertEqualsAsBoolean(JSValueRef value, bool expectedValue)
{
    if (JSValueToBoolean(context, value, NULL) != expectedValue)
        fprintf(stderr, "assertEqualsAsBoolean failed: %p, %d\n", value, expectedValue);
}

static void assertEqualsAsNumber(JSValueRef value, double expectedValue)
{
    double number = JSValueToNumber(context, value, NULL);
    if (number != expectedValue && !(isnan(number) && isnan(expectedValue)))
        fprintf(stderr, "assertEqualsAsNumber failed: %p, %lf\n", value, expectedValue);
}

static void assertEqualsAsUTF8String(JSValueRef value, const char* expectedValue)
{
    JSStringRef valueAsString = JSValueToStringCopy(context, value, NULL);

    size_t jsSize = JSStringGetMaximumUTF8CStringSize(valueAsString);
    char jsBuffer[jsSize];
    JSStringGetUTF8CString(valueAsString, jsBuffer, jsSize);
    
    if (strcmp(jsBuffer, expectedValue) != 0)
        fprintf(stderr, "assertEqualsAsUTF8String strcmp failed: %s != %s\n", jsBuffer, expectedValue);
        
    if (jsSize < strlen(jsBuffer) + 1)
        fprintf(stderr, "assertEqualsAsUTF8String failed: jsSize was too small\n");

    JSStringRelease(valueAsString);
}

#if defined(__APPLE__)
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

#endif // __APPLE__

static JSValueRef jsGlobalValue; // non-stack value for testing JSValueProtect()

/* MyObject pseudo-class */

static bool didInitialize = false;
static void MyObject_initialize(JSContextRef context, JSObjectRef object, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    didInitialize = true;
}

static bool MyObject_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (JSStringIsEqualToUTF8CString(propertyName, "alwaysOne")
        || JSStringIsEqualToUTF8CString(propertyName, "cantFind")
        || JSStringIsEqualToUTF8CString(propertyName, "myPropertyName")) {
        return true;
    }
    
    return false;
}

static JSValueRef MyObject_getProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    if (JSStringIsEqualToUTF8CString(propertyName, "alwaysOne")) {
        return JSValueMakeNumber(1);
    }
    
    if (JSStringIsEqualToUTF8CString(propertyName, "myPropertyName")) {
        return JSValueMakeNumber(1);
    }

    if (JSStringIsEqualToUTF8CString(propertyName, "cantFind")) {
        return JSValueMakeUndefined();
    }
    
    return NULL;
}

static bool MyObject_setProperty(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(value);

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
    
    return false;
}

static void MyObject_addPropertiesToList(JSObjectRef object, JSPropertyListRef propertyList)
{
    UNUSED_PARAM(context);
    
    JSStringRef propertyName;
    
    propertyName = JSStringCreateWithUTF8CString("alwaysOne");
    JSPropertyListAdd(propertyList, object, propertyName);
    JSStringRelease(propertyName);
    
    propertyName = JSStringCreateWithUTF8CString("myPropertyName");
    JSPropertyListAdd(propertyList, object, propertyName);
    JSStringRelease(propertyName);
}

static JSValueRef MyObject_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    UNUSED_PARAM(thisObject);

    if (argc > 0 && JSValueIsStrictEqual(context, argv[0], JSValueMakeNumber(0)))
        return JSValueMakeNumber(1);
    
    return JSValueMakeUndefined();
}

static JSObjectRef MyObject_callAsConstructor(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);

    if (argc > 0 && JSValueIsStrictEqual(context, argv[0], JSValueMakeNumber(0)))
        return JSValueToObject(context, JSValueMakeNumber(1), NULL);
    
    return JSValueToObject(context, JSValueMakeNumber(0), NULL);
}

static bool MyObject_hasInstance(JSContextRef context, JSObjectRef constructor, JSValueRef possibleValue, JSValueRef* exception)
{
    UNUSED_PARAM(context);

    JSStringRef numberString = JSStringCreateWithUTF8CString("Number");
    JSObjectRef numberConstructor = JSValueToObject(context, JSObjectGetProperty(context, JSContextGetGlobalObject(context), numberString), NULL);
    JSStringRelease(numberString);

    return JSValueIsInstanceOfConstructor(context, possibleValue, numberConstructor);
}

static JSValueRef MyObject_convertToType(JSContextRef context, JSObjectRef object, JSType type, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
    
    switch (type) {
    case kJSTypeBoolean:
        *exception = JSValueMakeNumber(2);
        return NULL;
    case kJSTypeNumber:
        return JSValueMakeNumber(1);
    default:
        break;
    }

    // string conversion -- forward to default object class
    return NULL;
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
    MyObject_initialize,
    MyObject_finalize,
    MyObject_hasProperty,
    MyObject_getProperty,
    MyObject_setProperty,
    MyObject_deleteProperty,
    MyObject_addPropertiesToList,
    MyObject_callAsFunction,
    MyObject_callAsConstructor,
    MyObject_hasInstance,
    MyObject_convertToType,
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
        JSStringRef string = JSValueToStringCopy(context, argv[0], NULL);
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
        char stringUTF8[sizeUTF8];
        JSStringGetUTF8CString(string, stringUTF8, sizeUTF8);
        printf("%s\n", stringUTF8);
        JSStringRelease(string);
    }
    
    return JSValueMakeUndefined();
}

static JSObjectRef myConstructor_callAsConstructor(JSContextRef context, JSObjectRef constructorObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(constructorObject);
    
    JSObjectRef result = JSObjectMake(context, NULL, 0);
    if (argc > 0) {
        JSStringRef value = JSStringCreateWithUTF8CString("value");
        JSObjectSetProperty(context, result, value, argv[0], kJSPropertyAttributeNone);
        JSStringRelease(value);
    }
    
    return result;
}

static char* createStringWithContentsOfFile(const char* fileName);

int main(int argc, char* argv[])
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);
    
    context = JSGlobalContextCreate(NULL);
    
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    assert(JSValueIsObject(globalObject));
    
    JSValueRef jsUndefined = JSValueMakeUndefined();
    JSValueRef jsNull = JSValueMakeNull();
    JSValueRef jsTrue = JSValueMakeBoolean(true);
    JSValueRef jsFalse = JSValueMakeBoolean(false);
    JSValueRef jsZero = JSValueMakeNumber(0);
    JSValueRef jsOne = JSValueMakeNumber(1);
    JSValueRef jsOneThird = JSValueMakeNumber(1.0 / 3.0);
    JSObjectRef jsObjectNoProto = JSObjectMake(context, NULL, JSValueMakeNull());

    // FIXME: test funny utf8 characters
    JSStringRef jsEmptyIString = JSStringCreateWithUTF8CString("");
    JSValueRef jsEmptyString = JSValueMakeString(jsEmptyIString);
    
    JSStringRef jsOneIString = JSStringCreateWithUTF8CString("1");
    JSValueRef jsOneString = JSValueMakeString(jsOneIString);

#if defined(__APPLE__)
    UniChar singleUniChar = 65; // Capital A
    CFMutableStringRef cfString = 
        CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault,
                                                          &singleUniChar,
                                                          1,
                                                          1,
                                                          kCFAllocatorNull);

    JSStringRef jsCFIString = JSStringCreateWithCFString(cfString);
    JSValueRef jsCFString = JSValueMakeString(jsCFIString);
    
    CFStringRef cfEmptyString = CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
    
    JSStringRef jsCFEmptyIString = JSStringCreateWithCFString(cfEmptyString);
    JSValueRef jsCFEmptyString = JSValueMakeString(jsCFEmptyIString);

    CFIndex cfStringLength = CFStringGetLength(cfString);
    UniChar buffer[cfStringLength];
    CFStringGetCharacters(cfString, 
                          CFRangeMake(0, cfStringLength), 
                          buffer);
    JSStringRef jsCFIStringWithCharacters = JSStringCreateWithCharacters(buffer, cfStringLength);
    JSValueRef jsCFStringWithCharacters = JSValueMakeString(jsCFIStringWithCharacters);
    
    JSStringRef jsCFEmptyIStringWithCharacters = JSStringCreateWithCharacters(buffer, CFStringGetLength(cfEmptyString));
    JSValueRef jsCFEmptyStringWithCharacters = JSValueMakeString(jsCFEmptyIStringWithCharacters);
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

    JSObjectRef myObject = JSObjectMake(context, MyObject_class(context), NULL);
    assert(didInitialize);
    JSStringRef myObjectIString = JSStringCreateWithUTF8CString("MyObject");
    JSObjectSetProperty(context, globalObject, myObjectIString, myObject, kJSPropertyAttributeNone);
    JSStringRelease(myObjectIString);
    
    JSValueRef exception;

    // Conversions that throw exceptions
    exception = NULL;
    assert(NULL == JSValueToObject(context, jsNull, &exception));
    assert(exception);
    
    exception = NULL;
    assert(isnan(JSValueToNumber(context, jsObjectNoProto, &exception)));
    assert(exception);

    exception = NULL;
    assert(!JSValueToStringCopy(context, jsObjectNoProto, &exception));
    assert(exception);
    
    exception = NULL;
    assert(!JSValueToBoolean(context, myObject, &exception));
    assert(exception);
    
    exception = NULL;
    assert(!JSValueIsEqual(context, jsObjectNoProto, JSValueMakeNumber(1), &exception));
    assert(exception);

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

    assert(JSValueIsEqual(context, jsOne, jsOneString, NULL));
    assert(!JSValueIsEqual(context, jsTrue, jsFalse, NULL));
    
#if defined(__APPLE__)
    CFStringRef cfJSString = JSStringCopyCFString(kCFAllocatorDefault, jsCFIString);
    CFStringRef cfJSEmptyString = JSStringCopyCFString(kCFAllocatorDefault, jsCFEmptyIString);
    assert(CFEqual(cfJSString, cfString));
    assert(CFEqual(cfJSEmptyString, cfEmptyString));
    CFRelease(cfJSString);
    CFRelease(cfJSEmptyString);

    CFRelease(cfString);
    CFRelease(cfEmptyString);
#endif // __APPLE__
    
    jsGlobalValue = JSObjectMake(context, NULL, NULL);
    JSValueProtect(jsGlobalValue);
    JSGarbageCollect();
    assert(JSValueIsObject(jsGlobalValue));
    JSValueUnprotect(jsGlobalValue);

    JSStringRef goodSyntax = JSStringCreateWithUTF8CString("x = 1;");
    JSStringRef badSyntax = JSStringCreateWithUTF8CString("x := 1;");
    assert(JSCheckScriptSyntax(context, goodSyntax, NULL, 0, NULL));
    assert(!JSCheckScriptSyntax(context, badSyntax, NULL, 0, NULL));

    JSValueRef result;
    JSValueRef v;
    JSObjectRef o;

    result = JSEvaluateScript(context, goodSyntax, NULL, NULL, 1, NULL);
    assert(result);
    assert(JSValueIsEqual(context, result, jsOne, NULL));

    exception = NULL;
    result = JSEvaluateScript(context, badSyntax, NULL, NULL, 1, &exception);
    assert(!result);
    assert(JSValueIsObject(exception));
    
    JSStringRef array = JSStringCreateWithUTF8CString("Array");
    v = JSObjectGetProperty(context, globalObject, array);
    assert(v);
    JSObjectRef arrayConstructor = JSValueToObject(context, v, NULL);
    JSStringRelease(array);
    result = JSObjectCallAsConstructor(context, arrayConstructor, 0, NULL, NULL);
    assert(result);
    assert(JSValueIsInstanceOfConstructor(context, result, arrayConstructor));
    assert(!JSValueIsInstanceOfConstructor(context, JSValueMakeNull(), arrayConstructor));
    
    JSStringRef functionBody;
    
    exception = NULL;
    functionBody = JSStringCreateWithUTF8CString("rreturn Array;");
    JSStringRef line = JSStringCreateWithUTF8CString("line");
    assert(!JSObjectMakeFunctionWithBody(context, functionBody, NULL, 1, &exception));
    assert(JSValueIsObject(exception));
    v = JSObjectGetProperty(context, JSValueToObject(context, exception, NULL), line);
    assert(v);
    assertEqualsAsNumber(v, 2); // FIXME: Lexer::setCode bumps startingLineNumber by 1 -- we need to change internal callers so that it doesn't have to (saying '0' to mean '1' in the API would be really confusing -- it's really confusing internally, in fact)
    JSStringRelease(functionBody);
    JSStringRelease(line);

    functionBody = JSStringCreateWithUTF8CString("return Array;");
    JSObjectRef function = JSObjectMakeFunctionWithBody(context, functionBody, NULL, 1, NULL);
    JSStringRelease(functionBody);

    assert(JSObjectIsFunction(function));
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, NULL);
    assert(JSValueIsEqual(context, v, arrayConstructor, NULL));
                                                  
    JSStringRef print = JSStringCreateWithUTF8CString("print");
    JSObjectRef printFunction = JSObjectMakeFunction(context, print_callAsFunction);
    JSObjectSetProperty(context, globalObject, print, printFunction, kJSPropertyAttributeNone); 
    JSStringRelease(print);
    
    assert(JSObjectSetPrivate(printFunction, (void*)1));
    assert(JSObjectGetPrivate(printFunction) == (void*)1);

    JSStringRef myConstructorIString = JSStringCreateWithUTF8CString("MyConstructor");
    JSObjectRef myConstructor = JSObjectMakeConstructor(context, myConstructor_callAsConstructor);
    JSObjectSetProperty(context, globalObject, myConstructorIString, myConstructor, kJSPropertyAttributeNone);
    JSStringRelease(myConstructorIString);
    
    assert(JSObjectSetPrivate(myConstructor, (void*)1));
    assert(JSObjectGetPrivate(myConstructor) == (void*)1);
    
    o = JSObjectMake(context, NULL, NULL);
    JSObjectSetProperty(context, o, jsOneIString, JSValueMakeNumber(1), kJSPropertyAttributeNone);
    JSObjectSetProperty(context, o, jsCFIString,  JSValueMakeNumber(1), kJSPropertyAttributeDontEnum);
    JSPropertyEnumeratorRef enumerator = JSObjectCreatePropertyEnumerator(o);
    int count = 0;
    while (JSPropertyEnumeratorGetNextName(enumerator))
        ++count;
    JSPropertyEnumeratorRelease(enumerator);
    assert(count == 1); // jsCFString should not be enumerated

    JSClassRef nullCallbacksClass = JSClassCreate(NULL, NULL, NULL, NULL);
    JSClassRelease(nullCallbacksClass);
    
    functionBody = JSStringCreateWithUTF8CString("return this;");
    function = JSObjectMakeFunctionWithBody(context, functionBody, NULL, 1, NULL);
    JSStringRelease(functionBody);
    v = JSObjectCallAsFunction(context, function, NULL, 0, NULL, NULL);
    assert(JSValueIsEqual(context, v, globalObject, NULL));
    v = JSObjectCallAsFunction(context, function, o, 0, NULL, NULL);
    assert(JSValueIsEqual(context, v, o, NULL));
    
    char* scriptUTF8 = createStringWithContentsOfFile("testapi.js");
    JSStringRef script = JSStringCreateWithUTF8CString(scriptUTF8);
    result = JSEvaluateScript(context, script, NULL, NULL, 1, &exception);
    if (JSValueIsUndefined(result))
        printf("PASS: Test script executed successfully.\n");
    else {
        printf("FAIL: Test script returned unexcpected value:\n");
        JSStringRef exceptionIString = JSValueToStringCopy(context, exception, NULL);
        CFStringRef exceptionCF = JSStringCopyCFString(kCFAllocatorDefault, exceptionIString);
        CFShow(exceptionCF);
        CFRelease(exceptionCF);
        JSStringRelease(exceptionIString);
    }
    JSStringRelease(script);
    free(scriptUTF8);

    // Allocate a few dummies so that at least one will be collected
    JSObjectMake(context, MyObject_class(context), 0);
    JSObjectMake(context, MyObject_class(context), 0);
    JSGarbageCollect();
    assert(didFinalize);

    JSStringRelease(jsEmptyIString);
    JSStringRelease(jsOneIString);
#if defined(__APPLE__)
    JSStringRelease(jsCFIString);
    JSStringRelease(jsCFEmptyIString);
    JSStringRelease(jsCFIStringWithCharacters);
    JSStringRelease(jsCFEmptyIStringWithCharacters);
#endif // __APPLE__
    JSStringRelease(goodSyntax);
    JSStringRelease(badSyntax);
    
    JSGlobalContextRelease(context);
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
