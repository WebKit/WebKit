/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "APICast.h"
#include "JSGlobalObjectInlines.h"
#include "MarkedJSValueRefArray.h"
#include <JavaScriptCore/JSContextRefPrivate.h>
#include <JavaScriptCore/JSObjectRefPrivate.h>
#include <JavaScriptCore/JavaScript.h>
#include <wtf/DataLog.h>
#include <wtf/Expected.h>
#include <wtf/Noncopyable.h>
#include <wtf/NumberOfCores.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

extern "C" void configureJSCForTesting();
extern "C" int testCAPIViaCpp(const char* filter);
extern "C" void JSSynchronousGarbageCollectForDebugging(JSContextRef);

class APIString {
    WTF_MAKE_NONCOPYABLE(APIString);
public:

    APIString(const char* string)
        : m_string(JSStringCreateWithUTF8CString(string))
    {
    }

    APIString(const String& string)
        : APIString(string.utf8().data())
    {
    }

    ~APIString()
    {
        JSStringRelease(m_string);
    }

    operator JSStringRef() { return m_string; }

private:
    JSStringRef m_string;
};

class APIContext {
    WTF_MAKE_NONCOPYABLE(APIContext);
public:

    APIContext()
        : m_context(JSGlobalContextCreate(nullptr))
    {
        APIString print("print");
        JSObjectRef printFunction = JSObjectMakeFunctionWithCallback(m_context, print, [] (JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*) {

            JSC::JSGlobalObject* globalObject = toJS(ctx);
            for (unsigned i = 0; i < argumentCount; i++)
                dataLog(toJS(globalObject, arguments[i]));
            dataLogLn();
            return JSValueMakeUndefined(ctx);
        });

        JSObjectSetProperty(m_context, JSContextGetGlobalObject(m_context), print, printFunction, kJSPropertyAttributeNone, nullptr);
    }

    ~APIContext()
    {
        JSGlobalContextRelease(m_context);
    }

    operator JSGlobalContextRef() { return m_context; }
    operator JSC::JSGlobalObject*() { return toJS(m_context); }

private:
    JSGlobalContextRef m_context;
};

template<typename T>
class APIVector : protected Vector<T> {
    using Base = Vector<T>;
public:
    APIVector(APIContext& context)
        : Base()
        , m_context(context)
    {
    }

    ~APIVector()
    {
        for (auto& value : *this)
            JSValueUnprotect(m_context, value);
    }

    using Vector<T>::operator[];
    using Vector<T>::size;
    using Vector<T>::begin;
    using Vector<T>::end;
    using typename Vector<T>::iterator;

    void append(T value)
    {
        JSValueProtect(m_context, value);
        Base::append(WTFMove(value));
    }

private:
    APIContext& m_context;
};

class TestAPI {
public:
    int run(const char* filter);

    void basicSymbol();
    void symbolsTypeof();
    void symbolsDescription();
    void symbolsGetPropertyForKey();
    void symbolsSetPropertyForKey();
    void symbolsHasPropertyForKey();
    void symbolsDeletePropertyForKey();
    void promiseResolveTrue();
    void promiseRejectTrue();
    void promiseUnhandledRejection();
    void promiseUnhandledRejectionFromUnhandledRejectionCallback();
    void promiseEarlyHandledRejections();
    void promiseDrainDoesNotEatExceptions();
    void topCallFrameAccess();
    void markedJSValueArrayAndGC();
    void classDefinitionWithJSSubclass();
    void proxyReturnedWithJSSubclassing();
    void testJSObjectSetOnGlobalObjectSubclassDefinition();
    void testBigInt();

    int failed() const { return m_failed; }

private:

    template<typename... Strings>
    bool check(bool condition, Strings... message);

    template<typename JSFunctor, typename APIFunctor>
    void checkJSAndAPIMatch(const JSFunctor&, const APIFunctor&, const char* description);

    void checkIsBigIntType(JSValueRef);

    void checkThrownException(JSValueRef* exception, const ASCIILiteral& expectedMessage, const char* description);

    // Helper methods.
    using ScriptResult = Expected<JSValueRef, JSValueRef>;
    ScriptResult evaluateScript(const char* script, JSObjectRef thisObject = nullptr);
    template<typename... ArgumentTypes>
    ScriptResult callFunction(const char* functionSource, ArgumentTypes... arguments);
    template<typename... ArgumentTypes>
    bool functionReturnsTrue(const char* functionSource, ArgumentTypes... arguments);

    bool scriptResultIs(ScriptResult, JSValueRef);

    // Ways to make sets of interesting things.
    APIVector<JSObjectRef> interestingObjects();
    APIVector<JSValueRef> interestingKeys();

    int m_failed { 0 };
    APIContext context;
};

TestAPI::ScriptResult TestAPI::evaluateScript(const char* script, JSObjectRef thisObject)
{
    APIString scriptAPIString(script);
    JSValueRef exception = nullptr;

    JSValueRef result = JSEvaluateScript(context, scriptAPIString, thisObject, nullptr, 0, &exception);
    if (exception)
        return Unexpected<JSValueRef>(exception);
    return ScriptResult(result);
}

template<typename... ArgumentTypes>
TestAPI::ScriptResult TestAPI::callFunction(const char* functionSource, ArgumentTypes... arguments)
{
    JSValueRef function;
    {
        ScriptResult functionResult = evaluateScript(functionSource);
        if (!functionResult)
            return functionResult;
        function = functionResult.value();
    }

    JSValueRef exception = nullptr;
    if (JSObjectRef functionObject = JSValueToObject(context, function, &exception)) {
        JSValueRef args[sizeof...(arguments)] { arguments... };
        JSValueRef result = JSObjectCallAsFunction(context, functionObject, functionObject, sizeof...(arguments), args, &exception);
        if (!exception)
            return ScriptResult(result);
    }

    RELEASE_ASSERT(exception);
    return Unexpected<JSValueRef>(exception);
}

template<typename... ArgumentTypes>
bool TestAPI::functionReturnsTrue(const char* functionSource, ArgumentTypes... arguments)
{
    JSValueRef trueValue = JSValueMakeBoolean(context, true);
    ScriptResult result = callFunction(functionSource, arguments...);
    if (!result)
        return false;
    return JSValueIsStrictEqual(context, trueValue, result.value());
}

bool TestAPI::scriptResultIs(ScriptResult result, JSValueRef value)
{
    if (!result)
        return false;
    return JSValueIsStrictEqual(context, result.value(), value);
}

template<typename... Strings>
bool TestAPI::check(bool condition, Strings... messages)
{
    if (!condition) {
        dataLogLn(messages..., ": FAILED");
        m_failed++;
    } else
        dataLogLn(messages..., ": PASSED");

    return condition;
}

template<typename JSFunctor, typename APIFunctor>
void TestAPI::checkJSAndAPIMatch(const JSFunctor& jsFunctor, const APIFunctor& apiFunctor, const char* description)
{
    JSValueRef exception = nullptr;
    JSValueRef result = apiFunctor(&exception);
    ScriptResult jsResult = jsFunctor();
    if (!jsResult) {
        check(exception, "JS and API calls should both throw an exception while ", description);
        check(functionReturnsTrue("(function(a, b) { return a.constructor === b.constructor; })", exception, jsResult.error()), "JS and API calls should both throw the same exception while ", description);
    } else {
        check(!exception, "JS and API calls should both not throw an exception while ", description);
        check(JSValueIsStrictEqual(context, result, jsResult.value()), "JS result and API calls should return the same value while ", description);
    }
}

APIVector<JSObjectRef> TestAPI::interestingObjects()
{
    APIVector<JSObjectRef> result(context);
    JSObjectRef array = JSValueToObject(context, evaluateScript(
        "[{}, [], { [Symbol.iterator]: 1 }, new Date(), new String('str'), new Map(), new Set(), new WeakMap(), new WeakSet(), new Error(), new Number(42), new Boolean(), { get length() { throw new Error(); } }];").value(), nullptr);

    APIString lengthString("length");
    unsigned length = JSValueToNumber(context, JSObjectGetProperty(context, array, lengthString, nullptr), nullptr);
    for (unsigned i = 0; i < length; i++) {
        JSObjectRef object = JSValueToObject(context, JSObjectGetPropertyAtIndex(context, array, i, nullptr), nullptr);
        ASSERT(object);
        result.append(object);
    }

    return result;
}

APIVector<JSValueRef> TestAPI::interestingKeys()
{
    APIVector<JSValueRef> result(context);
    JSObjectRef array = JSValueToObject(context, evaluateScript("[{}, [], 1, Symbol.iterator, 'length']").value(), nullptr);

    APIString lengthString("length");
    unsigned length = JSValueToNumber(context, JSObjectGetProperty(context, array, lengthString, nullptr), nullptr);
    for (unsigned i = 0; i < length; i++) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, array, i, nullptr);
        ASSERT(value);
        result.append(value);
    }

    return result;
}

static const char* isSymbolFunction = "(function isSymbol(symbol) { return typeof(symbol) === 'symbol'; })";
static const char* getSymbolDescription = "(function getSymbolDescription(symbol) { return symbol.description; })";
static const char* getFunction = "(function get(object, key) { return object[key]; })";
static const char* setFunction = "(function set(object, key, value) { object[key] = value; })";
static const char* isBigIntFunction = "(function isBigInt(bigint) { return typeof(bigint) === 'bigint'; })";
static const char* createBigIntFunction = "(function bigInt(x) { print(x); return BigInt(x); })";
static const char* createNegBigIntFunction = "(function bigInt(x) { print(x); return -BigInt(x); })";

void TestAPI::checkIsBigIntType(JSValueRef value)
{
    check(JSValueGetType(context, value) == kJSTypeBigInt, "value is a bigint (JSValueGetType)");
    check(JSValueIsBigInt(context, value), "value is a bigint (JSValueIsBigInt)");
    check(functionReturnsTrue(isBigIntFunction, value), "value is a bigint (isBigIntFunction)");
}

void TestAPI::basicSymbol()
{
    // Can't call Symbol as a constructor since it's not subclassable.
    auto result = evaluateScript("Symbol('dope');");
    check(JSValueGetType(context, result.value()) == kJSTypeSymbol, "dope get type is a symbol");
    check(JSValueIsSymbol(context, result.value()), "dope is a symbol");
}

void TestAPI::symbolsTypeof()
{
    {
        JSValueRef symbol = JSValueMakeSymbol(context, nullptr);
        check(functionReturnsTrue(isSymbolFunction, symbol), "JSValueMakeSymbol makes a symbol value");
    }
    {
        APIString description("dope");
        JSValueRef symbol = JSValueMakeSymbol(context, description);
        check(functionReturnsTrue(isSymbolFunction, symbol), "JSValueMakeSymbol makes a symbol value");
    }
}

void TestAPI::symbolsDescription()
{
    {
        JSValueRef symbol = JSValueMakeSymbol(context, nullptr);
        auto result = callFunction(getSymbolDescription, symbol);
        check(JSValueIsStrictEqual(context, result.value(), JSValueMakeUndefined(context)), "JSValueMakeSymbol with nullptr description produces a symbol value without description");
    }
    {
        APIString description("dope");
        JSValueRef symbol = JSValueMakeSymbol(context, description);
        auto result = callFunction(getSymbolDescription, symbol);
        check(JSValueIsStrictEqual(context, result.value(), JSValueMakeString(context, description)), "JSValueMakeSymbol with description string produces a symbol value with description");
    }
}

void TestAPI::symbolsGetPropertyForKey()
{
    auto objects = interestingObjects();
    auto keys = interestingKeys();

    for (auto& object : objects) {
        dataLogLn("\nnext object: ", toJS(context, object));
        for (auto& key : keys) {
            dataLogLn("Using key: ", toJS(context, key));
            checkJSAndAPIMatch(
            [&] {
                return callFunction(getFunction, object, key);
            }, [&] (JSValueRef* exception) {
                return JSObjectGetPropertyForKey(context, object, key, exception);
            }, "checking get property keys");
        }
    }
}

void TestAPI::symbolsSetPropertyForKey()
{
    auto jsObjects = interestingObjects();
    auto apiObjects = interestingObjects();
    auto keys = interestingKeys();

    JSValueRef theAnswer = JSValueMakeNumber(context, 42);
    for (size_t i = 0; i < jsObjects.size(); i++) {
        for (auto& key : keys) {
            JSObjectRef jsObject = jsObjects[i];
            JSObjectRef apiObject = apiObjects[i];
            checkJSAndAPIMatch(
                [&] {
                    return callFunction(setFunction, jsObject, key, theAnswer);
                } , [&] (JSValueRef* exception) {
                    JSObjectSetPropertyForKey(context, apiObject, key, theAnswer, kJSPropertyAttributeNone, exception);
                    return JSValueMakeUndefined(context);
                }, "setting property keys to the answer");
            // Check get is the same on API object.
            checkJSAndAPIMatch(
                [&] {
                    return callFunction(getFunction, apiObject, key);
                }, [&] (JSValueRef* exception) {
                    return JSObjectGetPropertyForKey(context, apiObject, key, exception);
                }, "getting property keys from API objects");
            // Check get is the same on respective objects.
            checkJSAndAPIMatch(
                [&] {
                    return callFunction(getFunction, jsObject, key);
                }, [&] (JSValueRef* exception) {
                    return JSObjectGetPropertyForKey(context, apiObject, key, exception);
                }, "getting property keys from respective objects");
        }
    }
}

void TestAPI::symbolsHasPropertyForKey()
{
    const char* hasFunction = "(function has(object, key) { return key in object; })";
    auto objects = interestingObjects();
    auto keys = interestingKeys();

    JSValueRef theAnswer = JSValueMakeNumber(context, 42);
    for (auto& object : objects) {
        dataLogLn("\nNext object: ", toJS(context, object));
        for (auto& key : keys) {
            dataLogLn("Using key: ", toJS(context, key));
            checkJSAndAPIMatch(
                [&] {
                    return callFunction(hasFunction, object, key);
                }, [&] (JSValueRef* exception) {
                    return JSValueMakeBoolean(context, JSObjectHasPropertyForKey(context, object, key, exception));
                }, "checking has property keys unset");

            check(!!callFunction(setFunction, object, key, theAnswer), "set property to the answer");

            checkJSAndAPIMatch(
                [&] {
                    return callFunction(hasFunction, object, key);
                }, [&] (JSValueRef* exception) {
                    return JSValueMakeBoolean(context, JSObjectHasPropertyForKey(context, object, key, exception));
                }, "checking has property keys set");
        }
    }
}


void TestAPI::symbolsDeletePropertyForKey()
{
    const char* deleteFunction = "(function del(object, key) { return delete object[key]; })";
    auto objects = interestingObjects();
    auto keys = interestingKeys();

    JSValueRef theAnswer = JSValueMakeNumber(context, 42);
    for (auto& object : objects) {
        dataLogLn("\nNext object: ", toJS(context, object));
        for (auto& key : keys) {
            dataLogLn("Using key: ", toJS(context, key));
            checkJSAndAPIMatch(
                [&] {
                    return callFunction(deleteFunction, object, key);
                }, [&] (JSValueRef* exception) {
                    return JSValueMakeBoolean(context, JSObjectDeletePropertyForKey(context, object, key, exception));
                }, "checking has property keys unset");

            check(!!callFunction(setFunction, object, key, theAnswer), "set property to the answer");

            checkJSAndAPIMatch(
                [&] {
                    return callFunction(deleteFunction, object, key);
                }, [&] (JSValueRef* exception) {
                    return JSValueMakeBoolean(context, JSObjectDeletePropertyForKey(context, object, key, exception));
                }, "checking has property keys set");
        }
    }
}

void TestAPI::promiseResolveTrue()
{
    JSObjectRef resolve;
    JSObjectRef reject;
    JSValueRef exception = nullptr;
    JSObjectRef promise = JSObjectMakeDeferredPromise(context, &resolve, &reject, &exception);
    check(!exception, "No exception should be thrown creating a deferred promise");

    // Ugh, we don't have any C API that takes blocks... so we do this hack to capture the runner.
    static TestAPI* tester = this;
    static bool passedTrueCalled = false;

    APIString trueString("passedTrue");
    auto passedTrue = [](JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*) -> JSValueRef {
        tester->check(argumentCount && JSValueIsStrictEqual(ctx, arguments[0], JSValueMakeBoolean(ctx, true)), "function should have been called back with true");
        passedTrueCalled = true;
        return JSValueMakeUndefined(ctx);
    };

    APIString thenString("then");
    JSValueRef thenFunction = JSObjectGetProperty(context, promise, thenString, &exception);
    check(!exception && thenFunction && JSValueIsObject(context, thenFunction), "Promise should have a then object property");

    JSValueRef passedTrueFunction = JSObjectMakeFunctionWithCallback(context, trueString, passedTrue);
    JSObjectCallAsFunction(context, const_cast<JSObjectRef>(thenFunction), promise, 1, &passedTrueFunction, &exception);
    check(!exception, "No exception should be thrown setting up callback");

    auto trueValue = JSValueMakeBoolean(context, true);
    JSObjectCallAsFunction(context, resolve, resolve, 1, &trueValue, &exception);
    check(!exception, "No exception should be thrown resolving promise");
    check(passedTrueCalled, "then response function should have been called.");
}

void TestAPI::promiseRejectTrue()
{
    JSObjectRef resolve;
    JSObjectRef reject;
    JSValueRef exception = nullptr;
    JSObjectRef promise = JSObjectMakeDeferredPromise(context, &resolve, &reject, &exception);
    check(!exception, "No exception should be thrown creating a deferred promise");

    // Ugh, we don't have any C API that takes blocks... so we do this hack to capture the runner.
    static TestAPI* tester = this;
    static bool passedTrueCalled = false;

    APIString trueString("passedTrue");
    auto passedTrue = [](JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*) -> JSValueRef {
        tester->check(argumentCount && JSValueIsStrictEqual(ctx, arguments[0], JSValueMakeBoolean(ctx, true)), "function should have been called back with true");
        passedTrueCalled = true;
        return JSValueMakeUndefined(ctx);
    };

    APIString catchString("catch");
    JSValueRef catchFunction = JSObjectGetProperty(context, promise, catchString, &exception);
    check(!exception && catchFunction && JSValueIsObject(context, catchFunction), "Promise should have a catch object property");

    JSValueRef passedTrueFunction = JSObjectMakeFunctionWithCallback(context, trueString, passedTrue);
    JSObjectCallAsFunction(context, const_cast<JSObjectRef>(catchFunction), promise, 1, &passedTrueFunction, &exception);
    check(!exception, "No exception should be thrown setting up callback");

    auto trueValue = JSValueMakeBoolean(context, true);
    JSObjectCallAsFunction(context, reject, reject, 1, &trueValue, &exception);
    check(!exception, "No exception should be thrown rejecting promise");
    check(passedTrueCalled, "catch response function should have been called.");
}

void TestAPI::promiseUnhandledRejection()
{
    JSObjectRef reject = nullptr;
    JSValueRef exception = nullptr;
    static auto promise = JSObjectMakeDeferredPromise(context, nullptr, &reject, &exception);
    check(!exception, "creating a (reject-only) deferred promise should not throw");
    static auto reason = JSValueMakeString(context, APIString("reason"));

    static TestAPI* tester = this;
    static bool callbackCalled = false;
    auto callback = [](JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*) -> JSValueRef {
        if (callbackCalled)
            return JSValueMakeUndefined(ctx);
        tester->check(argumentCount && JSValueIsStrictEqual(ctx, arguments[0], promise), "callback should receive rejected promise as first argument");
        tester->check(argumentCount > 1 && JSValueIsStrictEqual(ctx, arguments[1], reason), "callback should receive rejection reason as second argument");
        tester->check(argumentCount == 2, "callback should not receive a third argument");
        callbackCalled = true;
        return JSValueMakeUndefined(ctx);
    };
    auto callbackFunction = JSObjectMakeFunctionWithCallback(context, APIString("callback"), callback);

    JSGlobalContextSetUnhandledRejectionCallback(context, callbackFunction, &exception);
    check(!exception, "setting unhandled rejection callback should not throw");

    JSObjectCallAsFunction(context, reject, reject, 1, &reason, &exception);
    check(!exception && callbackCalled, "unhandled rejection callback should be called upon unhandled rejection");
}

void TestAPI::promiseUnhandledRejectionFromUnhandledRejectionCallback()
{
    static JSObjectRef reject;
    static JSValueRef exception = nullptr;
    JSObjectMakeDeferredPromise(context, nullptr, &reject, &exception);
    check(!exception, "creating a (reject-only) deferred promise should not throw");

    static auto callbackCallCount = 0;
    auto callback = [](JSContextRef ctx, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*) -> JSValueRef {
        if (!callbackCallCount)
            JSObjectCallAsFunction(ctx, reject, reject, 0, nullptr, &exception);
        callbackCallCount++;
        return JSValueMakeUndefined(ctx);
    };
    auto callbackFunction = JSObjectMakeFunctionWithCallback(context, APIString("callback"), callback);

    JSGlobalContextSetUnhandledRejectionCallback(context, callbackFunction, &exception);
    check(!exception, "setting unhandled rejection callback should not throw");

    callFunction("(function () { Promise.reject(); })");
    check(!exception && callbackCallCount == 2, "unhandled rejection from unhandled rejection callback should also trigger the callback");
}

void TestAPI::promiseEarlyHandledRejections()
{
    JSValueRef exception = nullptr;
    
    static bool callbackCalled = false;
    auto callback = [](JSContextRef ctx, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*) -> JSValueRef {
        callbackCalled = true;
        return JSValueMakeUndefined(ctx);
    };
    auto callbackFunction = JSObjectMakeFunctionWithCallback(context, APIString("callback"), callback);

    JSGlobalContextSetUnhandledRejectionCallback(context, callbackFunction, &exception);
    check(!exception, "setting unhandled rejection callback should not throw");

    callFunction("(function () { const p = Promise.reject(); p.catch(() => {}); })");
    check(!callbackCalled, "unhandled rejection callback should not be called for synchronous early-handled rejection");

    callFunction("(function () { const p = Promise.reject(); Promise.resolve().then(() => { p.catch(() => {}); }); })");
    check(!callbackCalled, "unhandled rejection callback should not be called for asynchronous early-handled rejection");
}

void TestAPI::promiseDrainDoesNotEatExceptions()
{
#if PLATFORM(COCOA)
    bool useLegacyDrain = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DoesNotDrainTheMicrotaskQueueWhenCallingObjC);
    if (useLegacyDrain)
        return;
#endif

    ScriptResult result = callFunction("(function() { Promise.resolve().then(() => { throw 2; }); throw 1; })");
    check(!result, "function should throw an error");
    check(JSValueIsNumber(context, result.error()) && JSValueToNumber(context, result.error(), nullptr) == 1, "exception payload should have been 1");
}

void TestAPI::topCallFrameAccess()
{
    {
        JSObjectRef function = JSValueToObject(context, evaluateScript("(function () { })").value(), nullptr);
        APIString argumentsString("arguments");
        auto arguments = JSObjectGetProperty(context, function, argumentsString, nullptr);
        check(JSValueIsNull(context, arguments), "vm.topCallFrame access from C++ world should use nullptr internally for arguments");
    }
    {
        JSObjectRef arguments = JSValueToObject(context, evaluateScript("(function ok(v) { return ok.arguments; })(42)").value(), nullptr);
        check(!JSValueIsNull(context, arguments), "vm.topCallFrame is materialized and we found the caller function's arguments");
    }
    {
        JSObjectRef function = JSValueToObject(context, evaluateScript("(function () { })").value(), nullptr);
        APIString callerString("caller");
        auto caller = JSObjectGetProperty(context, function, callerString, nullptr);
        check(JSValueIsNull(context, caller), "vm.topCallFrame access from C++ world should use nullptr internally for caller");
    }
    {
        JSObjectRef caller = JSValueToObject(context, evaluateScript("(function () { return (function ok(v) { return ok.caller; })(42); })()").value(), nullptr);
        check(!JSValueIsNull(context, caller), "vm.topCallFrame is materialized and we found the caller function's caller");
    }
    {
        JSObjectRef caller = JSValueToObject(context, evaluateScript("(function ok(v) { return ok.caller; })(42)").value(), nullptr);
        check(JSValueIsNull(context, caller), "vm.topCallFrame is materialized and we found the caller function's caller, but the caller is global code");
    }
}

void TestAPI::markedJSValueArrayAndGC()
{
    auto testMarkedJSValueArray = [&] (unsigned count) {
        auto* globalObject = toJS(context);
        JSC::JSLockHolder locker(globalObject->vm());
        JSC::MarkedJSValueRefArray values(context, count);
        for (unsigned index = 0; index < count; ++index) {
            JSValueRef string = JSValueMakeString(context, APIString(makeString("Prefix"_s, index)));
            values[index] = string;
        }
        JSSynchronousGarbageCollectForDebugging(context);
        bool ok = true;
        for (unsigned index = 0; index < count; ++index) {
            JSValueRef string = JSValueMakeString(context, APIString(makeString("Prefix"_s, index)));
            if (!JSValueIsStrictEqual(context, values[index], string))
                ok = false;
        }
        check(ok, "Held JSString should be alive and correct.");
    };
    testMarkedJSValueArray(4);
    testMarkedJSValueArray(1000);
}

void TestAPI::classDefinitionWithJSSubclass()
{
    const static JSClassDefinition definition = kJSClassDefinitionEmpty;
    static JSClassRef jsClass = JSClassCreate(&definition);

    auto constructor = [] (JSContextRef ctx, JSObjectRef, size_t, const JSValueRef*, JSValueRef*) -> JSObjectRef {
        return JSObjectMake(ctx, jsClass, nullptr);
    };

    JSObjectRef Superclass = JSObjectMakeConstructor(context, jsClass, constructor);

    ScriptResult result = callFunction("(function (Superclass) { class Subclass extends Superclass { method() { return 'value'; } }; return new Subclass(); })", Superclass);
    check(!!result, "creating a subclass should not throw.");
    check(JSValueIsObject(context, result.value()), "result of construction should have been an object.");
    JSObjectRef subclass = const_cast<JSObjectRef>(result.value());

    check(JSObjectHasProperty(context, subclass, APIString("method")), "subclass should have derived classes functions.");
    check(functionReturnsTrue("(function (subclass, Superclass) { return subclass instanceof Superclass; })", subclass, Superclass), "JS subclass should instanceof the Superclass");

    JSClassRelease(jsClass);
}

void TestAPI::proxyReturnedWithJSSubclassing()
{
    const static JSClassDefinition definition = kJSClassDefinitionEmpty;
    static JSClassRef jsClass = JSClassCreate(&definition);
    static TestAPI& test = *this;

    auto constructor = [] (JSContextRef ctx, JSObjectRef, size_t, const JSValueRef*, JSValueRef*) -> JSObjectRef {
        ScriptResult result = test.callFunction("(function (object) { return new Proxy(object, { getPrototypeOf: () => { globalThis.triggeredProxy = true; return object.__proto__; }}); })", JSObjectMake(ctx, jsClass, nullptr));
        test.check(!!result, "creating a proxy should not throw");
        test.check(JSValueIsObject(ctx, result.value()), "result of proxy creation should have been an object.");
        return const_cast<JSObjectRef>(result.value());
    };

    JSObjectRef Superclass = JSObjectMakeConstructor(context, jsClass, constructor);

    ScriptResult result = callFunction("(function (Superclass) { class Subclass extends Superclass { method() { return 'value'; } }; return new Subclass(); })", Superclass);
    check(!!result, "creating a subclass should not throw.");
    check(JSValueIsObject(context, result.value()), "result of construction should have been an object.");
    JSObjectRef subclass = const_cast<JSObjectRef>(result.value());

    check(scriptResultIs(evaluateScript("globalThis.triggeredProxy"), JSValueMakeUndefined(context)), "creating a subclass should not have triggered the proxy");
    check(functionReturnsTrue("(function (subclass, Superclass) { return subclass.__proto__ == Superclass.prototype; })", subclass, Superclass), "proxy's prototype should match Superclass.prototype");
}

void TestAPI::testJSObjectSetOnGlobalObjectSubclassDefinition()
{
    JSClassDefinition globalClassDef = kJSClassDefinitionEmpty;
    globalClassDef.className = "CustomGlobalClass";
    JSClassRef globalClassRef = JSClassCreate(&globalClassDef);

    JSContextRef context = JSGlobalContextCreate(globalClassRef);
    JSObjectRef newObject = JSObjectMake(context, nullptr, nullptr);

    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    APIString propertyName("myObject");
    JSObjectSetProperty(context, globalObject, propertyName, newObject, 0, nullptr);

    check(JSEvaluateScript(context, propertyName, globalObject, nullptr, 1, nullptr) == newObject, "Setting a property on a custom global object should set the property");
}

void TestAPI::checkThrownException(JSValueRef* exception, const ASCIILiteral& expectedMessage, const char* description)
{
    check(exception, description, " throws an exception");
    JSC::JSGlobalObject* globalObject = toJS(context);
    JSC::JSValue exceptionValue = toJS(globalObject, *exception);
    check(exceptionValue.toWTFString(globalObject) == expectedMessage, description, " has correct exception message");
}

void TestAPI::testBigInt()
{
    {
        auto result = evaluateScript("BigInt(42);");
        checkIsBigIntType(result.value());
    }

    {
        double number = 42;
        const char* description = "checking JSValueMakeBigIntFromNumber with 42";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, number));
            }, [&] (JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithDouble(context, number, exception);
                checkIsBigIntType(bigint);
                double toNumberResult = JSValueToNumber(context, bigint, exception);
                check(toNumberResult == number, description, ", toNumberResult equals to number"_s);
                check(JSValueCompareDouble(context, bigint, number, exception) == kJSRelationConditionEqual, description, ", should kJSRelationConditionEqual to number"_s);
                check(JSValueCompareDouble(context, bigint, number + 1, exception) == kJSRelationConditionLessThan, description, ", should kJSRelationConditionLessThan number + 1"_s);
                check(JSValueCompareDouble(context, bigint, number - 1, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan number - 1"_s);
                check(JSValueCompareDouble(context, bigint, JSC::PNaN, exception) == kJSRelationConditionUndefined, description, ", should equal to kJSRelationConditionUndefined"_s);
                return bigint;
            }, description);
    }

    {
        double number = (1ULL << 32) + 1;
        const char* description = "checking JSValueMakeBigIntFromNumber with 2 ^ 32 + 1";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, number));
            }, [&] (JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithDouble(context, number, exception);
                checkIsBigIntType(bigint);
                double toNumberResult = JSValueToNumber(context, bigint, exception);
                check(toNumberResult == number, description, ", toNumberResult equals to number"_s);
                check(JSValueCompareDouble(context, bigint, number, exception) == kJSRelationConditionEqual, description, ", should kJSRelationConditionEqual to number"_s);
                check(JSValueCompareDouble(context, bigint, number + 1, exception) == kJSRelationConditionLessThan, description, ", should kJSRelationConditionLessThan number + 1"_s);
                check(JSValueCompareDouble(context, bigint, number - 1, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan number - 1"_s);
                return bigint;
            }, description);
    }

    {
        double number = JSC::PNaN;
        const char* description = "checking JSValueMakeBigIntFromNumber with NaN";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, number));
            }, [&] (JSValueRef* exception) {
                JSBigIntCreateWithDouble(context, number, exception);
                checkThrownException(exception, "RangeError: Not an integer"_s, description);
                return nullptr;
            }, description);
    }

    {
        double number = 1.1;
        const char* description = "checking JSValueMakeBigIntFromNumber with 1.1";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, number));
            }, [&] (JSValueRef* exception) {
                JSBigIntCreateWithDouble(context, number, exception);
                checkThrownException(exception, "RangeError: Not an integer"_s, description);
                return nullptr;
            }, description);
    }

    {
        double doubleMax = std::numeric_limits<double>::max();
        const char* description = "checking JSValueMakeBigIntFromNumber with max";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, doubleMax));
            }, [&] (JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithDouble(context, doubleMax, exception);
                checkIsBigIntType(bigint);
                double toNumberResult = JSValueToNumber(context, bigint, exception);
                check(toNumberResult == doubleMax, description, ", toNumberResult equals to doubleMax"_s);

                JSValueRef doubleMaxPlusOneBigInt = evaluateScript("BigInt(Number.MAX_VALUE) + 1n").value();
                double toDoubleResult = JSValueToNumber(context, doubleMaxPlusOneBigInt, exception);
                double toDoubleExpected = JSValueToNumber(context, evaluateScript("Number(BigInt(Number.MAX_VALUE) + 1n)").value(), exception);
                check(toDoubleResult == toDoubleExpected, description, ", JSValueToNumber"_s);
                check(JSValueCompareDouble(context, doubleMaxPlusOneBigInt, doubleMax, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan doubleMax"_s);
                return bigint;
            }, description);
    }

    {
        double doubleLowest = std::numeric_limits<double>::lowest();
        const char* description = "checking JSValueMakeBigIntFromNumber with lowest";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, doubleLowest));
            }, [&] (JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithDouble(context, doubleLowest, exception);
                checkIsBigIntType(bigint);
                double toNumberResult = JSValueToNumber(context, bigint, exception);
                check(toNumberResult == doubleLowest, description, ", toNumberResult equals to doubleLowest"_s);
                return bigint;
            }, description);
    }

    int64_t int64Max = std::numeric_limits<int64_t>::max();
    int64_t int64Min = std::numeric_limits<int64_t>::min();
    {

        JSStringRef int64MaxStr = JSStringCreateWithUTF8CString("0x7fffffffffffffff");
        JSStringRef int64MaxPlusOneStr = JSStringCreateWithUTF8CString("0x8000000000000000");
        const char* description = "checking JSBigIntCreateWithInt64 with int64 max";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeString(context, int64MaxStr));
            },
            [&](JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithInt64(context, int64Max, exception);
                checkIsBigIntType(bigint);
                int64_t result = JSValueToInt64(context, bigint, exception);
                check(result == int64Max, description, ", result equals to int64Max"_s);
                check(JSValueCompareInt64(context, bigint, int64Max, exception) == kJSRelationConditionEqual, description, ", should kJSRelationConditionEqual to int64Max"_s);
                check(JSValueCompareInt64(context, bigint, int64Max - 1, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan int64Max -1"_s);

                JSValueRef int64MaxPlusOneBigInt = JSBigIntCreateWithString(context, int64MaxPlusOneStr, exception);
                int64_t toInt64Result = JSValueToInt64(context, int64MaxPlusOneBigInt, exception);
                check(toInt64Result == int64Min, description, ", JSValueToInt64"_s);
                check(JSValueCompareInt64(context, int64MaxPlusOneBigInt, int64Max, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan uint64Max"_s);
                return bigint;
            },
            description);

        JSStringRelease(int64MaxStr);
        JSStringRelease(int64MaxPlusOneStr);
    }

    {
        JSStringRef int64MinStr = JSStringCreateWithUTF8CString("0x8000000000000000");
        JSStringRef int64MinMinusOneStr = JSStringCreateWithUTF8CString("-9223372036854775809");
        const char* description = "checking JSBigIntCreateWithInt64 with int64 min";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createNegBigIntFunction, JSValueMakeString(context, int64MinStr));
            },
            [&](JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithInt64(context, int64Min, exception);
                checkIsBigIntType(bigint);
                int64_t result = JSValueToInt64(context, bigint, exception);
                check(result == int64Min, description, ", result equals to int64Min"_s);
                check(JSValueCompareInt64(context, bigint, int64Min, exception) == kJSRelationConditionEqual, description, ", should kJSRelationConditionEqual to int64Min"_s);
                check(JSValueCompareInt64(context, bigint, int64Min + 1, exception) == kJSRelationConditionLessThan, description, ", should kJSRelationConditionLessThan int64Min + 1"_s);

                JSValueRef int64MinMinusOneBigInt = JSBigIntCreateWithString(context, int64MinMinusOneStr, exception);
                int64_t toInt64Result = JSValueToInt64(context, int64MinMinusOneBigInt, exception);
                check(toInt64Result == 0x7fffffffffffffff, description, ", JSValueToInt64"_s);
                check(JSValueCompareInt64(context, int64MinMinusOneBigInt, int64Min, exception) == kJSRelationConditionLessThan, description, ", should kJSRelationConditionLessThan int64Min"_s);
                return bigint;
            },
            description);

        JSStringRelease(int64MinStr);
        JSStringRelease(int64MinMinusOneStr);
    }

    {
        const char* description = "checking JSValueToInt64 with Number(42.0)";
        JSValueRef exceptionRef = nullptr;
        JSValueRef* exception = &exceptionRef;
        JSValueRef number = JSValueMakeNumber(context, 42.0);
        int64_t result = JSValueToInt64(context, number, exception);
        check(result == 42, description, " has correct result");
    }

    uint64_t uint64Max = std::numeric_limits<uint64_t>::max();
    uint64_t uint64Min = std::numeric_limits<uint64_t>::min();
    {
        JSStringRef uint64MaxStr = JSStringCreateWithUTF8CString("0xffffffffffffffff");
        JSStringRef uint64MaxPlusOneStr = JSStringCreateWithUTF8CString("0x10000000000000000");
        const char* description = "checking JSBigIntCreateWithUInt64 with int64 max";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeString(context, uint64MaxStr));
            },
            [&](JSValueRef* exception) {
                JSValueRef uint64MaxBigInt = JSBigIntCreateWithUInt64(context, uint64Max, exception);
                checkIsBigIntType(uint64MaxBigInt);
                uint64_t result = JSValueToUInt64(context, uint64MaxBigInt, exception);
                check(result == uint64Max, description, ", result equals to uint64Max"_s);
                check(JSValueCompareUInt64(context, uint64MaxBigInt, uint64Max, exception) == kJSRelationConditionEqual, description, ", should kJSRelationConditionEqual to uint64Max"_s);
                check(JSValueCompareUInt64(context, uint64MaxBigInt, uint64Max - 1, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan uint64Max - 1"_s);

                JSValueRef uint64MaxPlusOneBigInt = JSBigIntCreateWithString(context, uint64MaxPlusOneStr, exception);
                uint64_t toUInt64Result = JSValueToUInt64(context, uint64MaxPlusOneBigInt, exception);
                check(!toUInt64Result, description, ", JSValueToUInt64"_s);
                check(JSValueCompareUInt64(context, uint64MaxPlusOneBigInt, uint64Max, exception) == kJSRelationConditionGreaterThan, description, ", should kJSRelationConditionGreaterThan uint64Max"_s);
                return uint64MaxBigInt;
            },
            description);

        JSStringRelease(uint64MaxStr);
        JSStringRelease(uint64MaxPlusOneStr);
    }

    {
        JSStringRef uint64MinStr = JSStringCreateWithUTF8CString("0x0");
        JSStringRef uint64MinMinusOneStr = JSStringCreateWithUTF8CString("-1");
        const char* description = "checking JSBigIntCreateWithUInt64 with int64 min";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeString(context, uint64MinStr));
            },
            [&](JSValueRef* exception) {
                JSValueRef uint64MinBigInt = JSBigIntCreateWithUInt64(context, uint64Min, exception);
                checkIsBigIntType(uint64MinBigInt);
                uint64_t result = JSValueToUInt64(context, uint64MinBigInt, exception);
                check(result == uint64Min, description, ", result equals to uint64Min"_s);
                check(JSValueCompareUInt64(context, uint64MinBigInt, uint64Min, exception) == kJSRelationConditionEqual, description, ", should kJSRelationConditionEqual to uint64Min"_s);
                check(JSValueCompareUInt64(context, uint64MinBigInt, uint64Min + 1, exception) == kJSRelationConditionLessThan, description, ", should kJSRelationConditionLessThan uint64Min + 1"_s);

                JSValueRef uint64MinMinusOneBigInt = JSBigIntCreateWithString(context, uint64MinMinusOneStr, exception);
                uint64_t toUInt64Result = JSValueToUInt64(context, uint64MinMinusOneBigInt, exception);
                check(toUInt64Result == uint64Max, description, ", JSValueToUInt64"_s);
                check(JSValueCompareUInt64(context, uint64MinMinusOneBigInt, uint64Min, exception) == kJSRelationConditionLessThan, description, ", should kJSRelationConditionLessThan uint64Min"_s);
                return uint64MinBigInt;
            },
            description);

        JSStringRelease(uint64MinStr);
        JSStringRelease(uint64MinMinusOneStr);
    }

    {
        JSValueRef exceptionRef = nullptr;
        JSValueRef* exception = &exceptionRef;
        JSValueRef jsUndefined = JSValueMakeUndefined(context);
        JSValueRef jsNull = JSValueMakeNull(context);
        JSValueRef jsTrue = JSValueMakeBoolean(context, true);
        JSValueRef jsFalse = JSValueMakeBoolean(context, false);
        JSValueRef jsZero = JSValueMakeNumber(context, 0);
        JSValueRef jsOne = JSValueMakeNumber(context, 1);
        JSValueRef jsOneThird = JSValueMakeNumber(context, 1.0 / 3.0);
        JSObjectRef jsObject = JSObjectMake(context, nullptr, nullptr);
        JSValueRef jsNaN = JSValueMakeNumber(context, std::numeric_limits<double>::quiet_NaN());

        auto testToInt = [&](auto func, const char* description) {
            check(!func(context, jsUndefined, exception), description, ", with jsUndefined should be 0"_s);
            check(!func(context, jsNull, exception), description, ", with jsNull should be 0"_s);
            check(func(context, jsTrue, exception), description, ", with jsTrue should be 1"_s);
            check(!func(context, jsFalse, exception), description, ", with jsFalse should be 1"_s);
            check(!func(context, jsZero, exception), description, ", with jsZero should be 0"_s);
            check(func(context, jsOne, exception), description, ", with jsOne should be 0"_s);
            check(!func(context, jsOneThird, exception), description, ", with jsOneThird should be 0"_s);
            check(!func(context, jsObject, exception), description, ", with jsObject should be 0"_s);
            check(!func(context, jsNaN, exception), description, ", with jsObject should be 0"_s);
        };

        testToInt(JSValueToUInt64, "checking JSValueToUInt64 with other types");
        testToInt(JSValueToInt64, "checking JSValueToInt64 with other types");
        testToInt(JSValueToUInt32, "checking JSValueToUInt32 with other types");
        testToInt(JSValueToInt32, "checking JSValueToInt32 with other types");

        auto testCompareWithZero = [&](auto compare, const char* description) {
            unsigned right = 0;
            check(kJSRelationConditionUndefined == compare(context, jsUndefined, right, exception), description, ", with jsUndefined should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionEqual == compare(context, jsNull, right, exception), description, ", with jsNull should be kJSRelationConditionEqual"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsTrue, right, exception), description, ", with jsTrue should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionEqual == compare(context, jsFalse, right, exception), description, ", with jsFalse should be kJSRelationConditionEqual"_s);
            check(kJSRelationConditionEqual == compare(context, jsZero, right, exception), description, ", with jsZero should be kJSRelationConditionEqual"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsOne, right, exception), description, ", with jsOne should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsOneThird, right, exception), description, ", with jsOneThird should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionUndefined == compare(context, jsObject, right, exception), description, ", with jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNaN, right, exception), description, ", with jsNaN should be kJSRelationConditionUndefined"_s);
        };

        auto testCompareWithNumberSafeMin = [&](auto compare, const char* description) {
            int64_t right = -9007199254740991;
            check(kJSRelationConditionUndefined == compare(context, jsUndefined, right, exception), description, ", with jsUndefined should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsNull, right, exception), description, ", with jsNull should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsTrue, right, exception), description, ", with jsTrue should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsFalse, right, exception), description, ", with jsFalse should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsZero, right, exception), description, ", with jsZero should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsOne, right, exception), description, ", with jsOne should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsOneThird, right, exception), description, ", with jsOneThird should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionUndefined == compare(context, jsObject, right, exception), description, ", with jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNaN, right, exception), description, ", with jsNaN should be kJSRelationConditionUndefined"_s);
        };

        auto testCompareWithNumberSafeMax = [&](auto compare, const char* description) {
            int64_t right = 9007199254740991;
            check(kJSRelationConditionUndefined == compare(context, jsUndefined, right, exception), description, ", with jsUndefined should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionLessThan == compare(context, jsNull, right, exception), description, ", with jsNull should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsTrue, right, exception), description, ", with jsTrue should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsFalse, right, exception), description, ", with jsFalse should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsZero, right, exception), description, ", with jsZero should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsOne, right, exception), description, ", with jsOne should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsOneThird, right, exception), description, ", with jsOneThird should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionUndefined == compare(context, jsObject, right, exception), description, ", with jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNaN, right, exception), description, ", with jsNaN should be kJSRelationConditionUndefined"_s);
        };

        auto testCompareWithDoubleLowest = [&](auto compare, const char* description) {
            double right = std::numeric_limits<double>::lowest();
            check(kJSRelationConditionUndefined == compare(context, jsUndefined, right, exception), description, ", with jsUndefined should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsNull, right, exception), description, ", with jsNull should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsTrue, right, exception), description, ", with jsTrue should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsFalse, right, exception), description, ", with jsFalse should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsZero, right, exception), description, ", with jsZero should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsOne, right, exception), description, ", with jsOne should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionGreaterThan == compare(context, jsOneThird, right, exception), description, ", with jsOneThird should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionUndefined == compare(context, jsObject, right, exception), description, ", with jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNaN, right, exception), description, ", with jsNaN should be kJSRelationConditionUndefined"_s);
        };

        auto testCompareWithDoubleMax = [&](auto compare, const char* description) {
            double right = std::numeric_limits<double>::max();
            check(kJSRelationConditionUndefined == compare(context, jsUndefined, right, exception), description, ", with jsUndefined should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionLessThan == compare(context, jsNull, right, exception), description, ", with jsNull should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsTrue, right, exception), description, ", with jsTrue should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsFalse, right, exception), description, ", with jsFalse should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsZero, right, exception), description, ", with jsZero should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsOne, right, exception), description, ", with jsOne should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionLessThan == compare(context, jsOneThird, right, exception), description, ", with jsOneThird should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionUndefined == compare(context, jsObject, right, exception), description, ", with jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNaN, right, exception), description, ", with jsNaN should be kJSRelationConditionUndefined"_s);
        };

        auto testCompareWithDoubleNaN = [&](auto compare, const char* description) {
            double right = std::numeric_limits<double>::quiet_NaN();
            check(kJSRelationConditionUndefined == compare(context, jsUndefined, right, exception), description, ", with jsUndefined should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNull, right, exception), description, ", with jsNull should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsTrue, right, exception), description, ", with jsTrue should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsFalse, right, exception), description, ", with jsFalse should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsZero, right, exception), description, ", with jsZero should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsOne, right, exception), description, ", with jsOne should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsOneThird, right, exception), description, ", with jsOneThird should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsObject, right, exception), description, ", with jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == compare(context, jsNaN, right, exception), description, ", with jsNaN should be kJSRelationConditionUndefined"_s);
        };

        auto testJSValueCompare = [&](const char* description) {
            check(kJSRelationConditionEqual == JSValueCompare(context, jsUndefined, jsNull, exception), description, ", with jsUndefined and jsNull should be kJSRelationConditionEqual"_s);
            check(kJSRelationConditionGreaterThan == JSValueCompare(context, jsTrue, jsFalse, exception), description, ", with jsTrue and jsFalse should be kJSRelationConditionGreaterThan"_s);
            check(kJSRelationConditionLessThan == JSValueCompare(context, jsZero, jsOne, exception), description, ", with jsZero and jsOne should be kJSRelationConditionLessThan"_s);
            check(kJSRelationConditionUndefined == JSValueCompare(context, jsOneThird, jsObject, exception), description, ", with jsOneThird and jsObject should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == JSValueCompare(context, jsOneThird, jsNaN, exception), description, ", with jsOneThird and jsNaN should be kJSRelationConditionUndefined"_s);
            check(kJSRelationConditionUndefined == JSValueCompare(context, jsNaN, jsOneThird, exception), description, ", with jsNaN and jsOneThird should be kJSRelationConditionUndefined"_s);
        };

        testCompareWithZero(JSValueCompareUInt64, "checking JSValueCompareUInt64 with other types and zero");
        testCompareWithZero(JSValueCompareInt64, "checking JSValueCompareInt64 with other types and zero");
        testCompareWithZero(JSValueCompareDouble, "checking JSValueCompareDouble with other types and zero");

        testCompareWithNumberSafeMin(JSValueCompareInt64, "checking JSValueCompareInt64 with other types and Number.MIN_SAFE_INTEGER");
        testCompareWithNumberSafeMin(JSValueCompareDouble, "checking JSValueCompareDouble with other types and Number.MIN_SAFE_INTEGER");

        testCompareWithNumberSafeMax(JSValueCompareUInt64, "checking JSValueCompareUInt64 with other types and Number.MAX_SAFE_INTEGER");
        testCompareWithNumberSafeMax(JSValueCompareInt64, "checking JSValueCompareInt64 with other types and Number.MAX_SAFE_INTEGER");
        testCompareWithNumberSafeMax(JSValueCompareDouble, "checking JSValueCompareDouble with other types and Number.MAX_SAFE_INTEGER");

        testCompareWithDoubleLowest(JSValueCompareDouble, "checking JSValueCompareDouble with other types and double lowest");
        testCompareWithDoubleMax(JSValueCompareDouble, "checking JSValueCompareDouble with other types and double max");
        testCompareWithDoubleNaN(JSValueCompareDouble, "checking JSValueCompareDouble with other types and double nan");

        testJSValueCompare("checking JSValueCompare");
    }

    {
        const char* description = "checking JSValueToUInt64 with Number(42.0)";
        JSValueRef exceptionRef = nullptr;
        JSValueRef* exception = &exceptionRef;
        JSValueRef number = JSValueMakeNumber(context, 42.0);
        uint64_t result = JSValueToUInt64(context, number, exception);
        check(result == 42, description, " has correct result");
    }

    {
        JSStringRef str = JSStringCreateWithUTF8CString("0x2a");
        const char* description = "checking JSValueMakeBigIntFromString with 0x2a";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeString(context, str));
            },
            [&](JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithString(context, str, exception);
                checkIsBigIntType(bigint);
                return bigint;
            },
            description);

        JSStringRelease(str);
    }

    {
        JSStringRef str = JSStringCreateWithUTF8CString("0h2a");
        const char* description = "checking JSValueMakeBigIntFromString with 0h2a";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeString(context, str));
            },
            [&](JSValueRef* exception) {
                JSBigIntCreateWithString(context, str, exception);
                checkThrownException(exception, "SyntaxError: Failed to parse String to BigInt"_s, description);
                return nullptr;
            },
            description);

        JSStringRelease(str);
    }

    {
        const char* description = "checking JSValueToStringCopy with BigInt(0x2a)";
        checkJSAndAPIMatch(
            [&] {
                return callFunction(createBigIntFunction, JSValueMakeNumber(context, 0x2a));
            }, [&] (JSValueRef* exception) {
                JSValueRef bigint = JSBigIntCreateWithUInt64(context, 0x2a, exception);
                checkIsBigIntType(bigint);
                JSStringRef string = JSValueToStringCopy(context, bigint, exception);
                check(string->string() == "42"_s, description, " has correct string");
                JSStringRelease(string);
                return bigint;
            }, description);
    }

    {
        const char* description = "checking JSValueCompare";
        JSValueRef exceptionRef = nullptr;
        JSValueRef* exception = &exceptionRef;
        JSValueRef bigInt42 = evaluateScript("BigInt(42)").value();
        JSValueRef bigInt43 = evaluateScript("BigInt(43)").value();
        check(JSValueCompare(context, bigInt42, bigInt43, exception) == kJSRelationConditionLessThan, description, ", should be kJSRelationConditionLessThan"_s);
        check(JSValueCompare(context, bigInt43, bigInt42, exception) == kJSRelationConditionGreaterThan, description, ", should be kJSRelationConditionGreaterThan"_s);
        check(JSValueCompare(context, bigInt42, bigInt42, exception) == kJSRelationConditionEqual, description, ", should be kJSRelationConditionLessThan"_s);
        check(JSValueCompare(context, bigInt42, bigInt42, exception) == kJSRelationConditionEqual, description, ", should be kJSRelationConditionLessThan"_s);
    }
}

void configureJSCForTesting()
{
    JSC::Config::configureForTesting();
}

#define RUN(test) do {                                 \
        if (!shouldRun(#test))                         \
            break;                                     \
        tasks.append(                                  \
            createSharedTask<void(TestAPI&)>(          \
                [&] (TestAPI& tester) {                \
                    tester.test;                       \
                    dataLog(#test ": OK!\n");          \
                }));                                   \
    } while (false)

int testCAPIViaCpp(const char* filter)
{
    dataLogLn("Starting C-API tests in C++");

    Deque<RefPtr<SharedTask<void(TestAPI&)>>> tasks;

    auto shouldRun = [&] (const char* testName) -> bool {
        return !filter || WTF::findIgnoringASCIICaseWithoutLength(testName, filter) != WTF::notFound;
    };

    RUN(topCallFrameAccess());
    RUN(basicSymbol());
    RUN(symbolsTypeof());
    RUN(symbolsDescription());
    RUN(symbolsGetPropertyForKey());
    RUN(symbolsSetPropertyForKey());
    RUN(symbolsHasPropertyForKey());
    RUN(symbolsDeletePropertyForKey());
    RUN(promiseResolveTrue());
    RUN(promiseRejectTrue());
    RUN(promiseUnhandledRejection());
    RUN(promiseUnhandledRejectionFromUnhandledRejectionCallback());
    RUN(promiseDrainDoesNotEatExceptions());
    RUN(promiseEarlyHandledRejections());
    RUN(markedJSValueArrayAndGC());
    RUN(classDefinitionWithJSSubclass());
    RUN(proxyReturnedWithJSSubclassing());
    RUN(testJSObjectSetOnGlobalObjectSubclassDefinition());

    if (tasks.isEmpty())
        return 0;

    Lock lock;

    static Atomic<int> failed { 0 };
    Vector<Ref<Thread>> threads;
    for (unsigned i = filter ? 1 : WTF::numberOfProcessorCores(); i--;) {
        threads.append(Thread::create(
            "Testapi via C++ thread"_s,
            [&] () {
                TestAPI tester;
                for (;;) {
                    RefPtr<SharedTask<void(TestAPI&)>> task;
                    {
                        Locker locker { lock };
                        if (tasks.isEmpty())
                            break;
                        task = tasks.takeFirst();
                    }

                    task->run(tester);
                }
                failed.exchangeAdd(tester.failed());
            }));
    }

    for (auto& thread : threads)
        thread->waitForCompletion();

    dataLogLn("C-API tests in C++ had ", failed.load(), " failures");
    return failed.load();
}
