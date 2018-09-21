/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "JSCJSValueInlines.h"
#include "JSObject.h"

#include <JavaScriptCore/JavaScript.h>
#include <wtf/DataLog.h>
#include <wtf/Expected.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

extern "C" int testCAPIViaCpp();

class APIString {
    WTF_MAKE_NONCOPYABLE(APIString);
public:

    APIString(const char* string)
        : m_string(JSStringCreateWithUTF8CString(string))
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

            JSC::ExecState* exec = toJS(ctx);
            for (unsigned i = 0; i < argumentCount; i++)
                dataLog(toJS(exec, arguments[i]));
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
    int run();
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

    // Ways to make sets of interesting things.
    APIVector<JSObjectRef> interestingObjects();
    APIVector<JSValueRef> interestingKeys();

    int failed { 0 };
    APIContext context;
};

int testCAPIViaCpp()
{
    TestAPI tester;
    return tester.run();
}

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

template<typename... Strings>
bool TestAPI::check(bool condition, Strings... messages)
{
    if (!condition) {
        dataLogLn(messages..., ": FAILED");
        failed++;
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

int TestAPI::run()
{
    dataLogLn("Starting C-API tests in C++");
    const char* isSymbolFunction = "(function isSymbol(symbol) { return typeof(symbol) === 'symbol'; })";
    const char* getFunction = "(function get(object, key) { return object[key]; })";
    const char* setFunction = "(function set(object, key, value) { object[key] = value; })";
    const char* hasFunction = "(function has(object, key) { return key in object; })";
    const char* deleteFunction = "(function del(object, key) { return delete object[key]; })";

    JSC::ExecState* exec = toJS(context);

    {
        // Can't call Symbol as a constructor since it's not subclassable.
        auto result = evaluateScript("Symbol('dope');");
        check(JSValueGetType(context, result.value()) == kJSTypeSymbol, "dope get type is a symbol");
        check(JSValueIsSymbol(context, result.value()), "dope is a symbol");
    }

    {
        APIString description("dope");
        JSValueRef symbol = JSValueMakeSymbol(context, description);
        check(functionReturnsTrue(isSymbolFunction, symbol), "JSValueMakeSymbol makes a symbol value");
    }

    {
        auto objects = interestingObjects();
        auto keys = interestingKeys();

        for (auto& object : objects) {
            dataLogLn("\nnext object: ", toJS(exec, object));
            for (auto& key : keys) {
                dataLogLn("Using key: ", toJS(exec, key));
                checkJSAndAPIMatch(
                    [&] {
                        return callFunction(getFunction, object, key);
                    }, [&] (JSValueRef* exception) {
                        return JSObjectGetPropertyForKey(context, object, key, exception);
                    }, "checking get property keys");
            }
        }
    }

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

    {
        auto objects = interestingObjects();
        auto keys = interestingKeys();

        JSValueRef theAnswer = JSValueMakeNumber(context, 42);
        for (auto& object : objects) {
            dataLogLn("\nNext object: ", toJS(exec, object));
            for (auto& key : keys) {
                dataLogLn("Using key: ", toJS(exec, key));
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

    {
        auto objects = interestingObjects();
        auto keys = interestingKeys();

        JSValueRef theAnswer = JSValueMakeNumber(context, 42);
        for (auto& object : objects) {
            dataLogLn("\nNext object: ", toJS(exec, object));
            for (auto& key : keys) {
                dataLogLn("Using key: ", toJS(exec, key));
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

    dataLogLn("C-API tests in C++ had ", failed, " failures");
    return failed;
}
