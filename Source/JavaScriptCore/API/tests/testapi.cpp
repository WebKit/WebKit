/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/text/StringCommon.h>

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
    void topCallFrameAccess();
    void markedJSValueArrayAndGC();
    void classDefinitionWithJSSubclass();
    void proxyReturnedWithJSSubclassing();

    int failed() const { return m_failed; }

private:

    template<typename... Strings>
    bool check(bool condition, Strings... message);

    template<typename JSFunctor, typename APIFunctor>
    void checkJSAndAPIMatch(const JSFunctor&, const APIFunctor&, const char* description);

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

#if COMPILER(MSVC)
template<>
TestAPI::ScriptResult TestAPI::callFunction(const char* functionSource)
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
        JSValueRef result = JSObjectCallAsFunction(context, functionObject, functionObject, 0, nullptr, &exception);
        if (!exception)
            return ScriptResult(result);
    }

    RELEASE_ASSERT(exception);
    return Unexpected<JSValueRef>(exception);
}
#endif

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
            JSValueRef string = JSValueMakeString(context, APIString(makeString("Prefix", index)));
            values[index] = string;
        }
        JSSynchronousGarbageCollectForDebugging(context);
        bool ok = true;
        for (unsigned index = 0; index < count; ++index) {
            JSValueRef string = JSValueMakeString(context, APIString(makeString("Prefix", index)));
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
    RUN(promiseEarlyHandledRejections());
    RUN(markedJSValueArrayAndGC());
    RUN(classDefinitionWithJSSubclass());
    RUN(proxyReturnedWithJSSubclassing());

    if (tasks.isEmpty()) {
        dataLogLn("Filtered all tests: ERROR");
        return 1;
    }

    Lock lock;

    static Atomic<int> failed { 0 };
    Vector<Ref<Thread>> threads;
    for (unsigned i = filter ? 1 : WTF::numberOfProcessorCores(); i--;) {
        threads.append(Thread::create(
            "Testapi via C++ thread",
            [&] () {
                TestAPI tester;
                for (;;) {
                    RefPtr<SharedTask<void(TestAPI&)>> task;
                    {
                        LockHolder locker(lock);
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
