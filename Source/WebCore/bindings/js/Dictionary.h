/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <heap/Strong.h>
#include <heap/StrongInlines.h>
#include <interpreter/CallFrame.h>
#include <runtime/JSCInlines.h>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSObject;
class JSValue;
}

namespace WebCore {

class ArrayValue;

class Dictionary {
public:
    Dictionary();
    Dictionary(JSC::ExecState*, JSC::JSObject*);
    Dictionary(JSC::ExecState*, JSC::JSValue);

    // Returns true if a value was found for the provided property.
    template<typename Result> bool get(const char* propertyName, Result&) const;
    template<typename Result> bool get(const String& propertyName, Result&) const;
    template<typename Result> std::optional<Result> get(const char* propertyName) const;
    
    bool isObject() const { return isValid(); }
    bool isUndefinedOrNull() const { return !isValid(); }

    bool getOwnPropertyNames(Vector<String>&) const;
    bool getWithUndefinedOrNullCheck(const char* propertyName, String&) const;

    JSC::ExecState* execState() const { return m_state; }
    JSC::JSObject* initializerObject() const { return m_initializerObject.get(); }

private:
    bool isValid() const { return m_state && m_initializerObject; }

    enum GetPropertyResult {
        ExceptionThrown,
        NoPropertyFound,
        PropertyFound
    };

    template <typename Result>
    GetPropertyResult tryGetPropertyAndResult(const char* propertyName, Result& context) const;
    GetPropertyResult tryGetProperty(const char* propertyName, JSC::JSValue&) const;

    static void convertValue(JSC::ExecState&, JSC::JSValue, bool& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, int& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, unsigned& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, unsigned short& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, double& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, String& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, Vector<String>& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, Dictionary& result);
    static void convertValue(JSC::ExecState&, JSC::JSValue, ArrayValue& result);

    JSC::ExecState* m_state { nullptr };
    JSC::Strong<JSC::JSObject> m_initializerObject;
};

template<typename Result> bool Dictionary::get(const char* propertyName, Result& result) const
{
    return isValid() && tryGetPropertyAndResult(propertyName, result) == PropertyFound;
}

template<typename Result> bool Dictionary::get(const String& propertyName, Result& result) const
{
    return get(propertyName.utf8().data(), result);
}

template<typename Result> std::optional<Result> Dictionary::get(const char* propertyName) const
{
    Result result;
    if (!get(propertyName, result))
        return std::nullopt;
    return result;
}

template <>
inline bool Dictionary::get(const char* propertyName, JSC::JSValue& finalResult) const
{
    return tryGetProperty(propertyName, finalResult) == PropertyFound;
}

inline bool Dictionary::getWithUndefinedOrNullCheck(const char* propertyName, String& result) const
{
    ASSERT(isValid());

    JSC::JSValue value;
    if (!get(propertyName, value) || value.isUndefinedOrNull())
        return false;

    result = value.toWTFString(m_state);
    return true;
}

template <typename Result>
Dictionary::GetPropertyResult Dictionary::tryGetPropertyAndResult(const char* propertyName, Result& finalResult) const
{
    auto scope = DECLARE_THROW_SCOPE(m_state->vm());

    JSC::JSValue value;
    GetPropertyResult getPropertyResult = tryGetProperty(propertyName, value);
    switch (getPropertyResult) {
    case ExceptionThrown:
        return getPropertyResult;
    case PropertyFound: {
        Result result;
        convertValue(*m_state, value, result);

        RETURN_IF_EXCEPTION(scope, ExceptionThrown);

        finalResult = result;
        break;
    }
    case NoPropertyFound:
        break;
    }

    return getPropertyResult;
}

}
