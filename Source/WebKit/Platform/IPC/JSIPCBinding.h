/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "Decoder.h"
#include "HandleMessage.h"
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

template<typename T, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const T&)
{
    return JSC::jsUndefined();
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const String& value)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, "String"_s));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "value"_s), JSC::jsNontrivialString(vm, value));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return object;
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const URL& value)
{
    return jsValueForDecodedArgumentValue(globalObject, value.string());
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const WebCore::RegistrableDomain& value)
{
    return jsValueForDecodedArgumentValue(globalObject, value.string());
}

template<typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, T)
{
}

template<typename E, std::enable_if_t<std::is_enum<E>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, E value)
{
    return jsValueForDecodedArgumentValue(globalObject, static_cast<std::underlying_type_t<E>>(value));
}

template<typename NumericType>
JSC::JSValue jsValueForDecodedNumericArgumentValue(JSC::JSGlobalObject* globalObject, NumericType value, const String& type)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, type));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "value"_s), JSC::JSValue(value));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return object;
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, bool value)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, "bool"));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "value"_s), JSC::jsBoolean(value));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return object;
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, double value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "double");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, float value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "float");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, int8_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "int8_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, int16_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "int16_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, int32_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "int32_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, int64_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "int64_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, uint8_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "uint8_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, uint16_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "uint16_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, uint32_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "uint32_t");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, uint64_t value)
{
    return jsValueForDecodedNumericArgumentValue(globalObject, value, "uint64_t");
}

template<typename U>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const ObjectIdentifier<U>& value)
{
    return jsValueForDecodedArgumentValue(globalObject, value.toUInt64());
}

template<typename RectType>
JSC::JSValue jsValueForDecodedArgumentRect(JSC::JSGlobalObject* globalObject, const RectType& value, const String& type)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, type));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "x"_s), JSC::JSValue(value.x()));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "y"_s), JSC::JSValue(value.y()));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "width"_s), JSC::JSValue(value.width()));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "height"_s), JSC::JSValue(value.height()));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return object;
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const WebCore::IntRect& value)
{
    return jsValueForDecodedArgumentRect(globalObject, value, "IntRect");
}

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const WebCore::FloatRect& value)
{
    return jsValueForDecodedArgumentRect(globalObject, value, "FloatRect");
}

template<size_t remainingSize, typename... Elements>
struct DecodedArgumentJSValueConverter {
    static bool convert(JSC::JSGlobalObject* globalObject, JSC::JSArray* array, const std::tuple<Elements...>& tuple)
    {
        auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

        auto jsValue = jsValueForDecodedArgumentValue(globalObject, std::get<sizeof...(Elements) - remainingSize>(tuple));
        if (jsValue.isEmpty())
            return false;

        unsigned index = sizeof...(Elements) - remainingSize;
        array->putDirectIndex(globalObject, index, jsValue);
        RETURN_IF_EXCEPTION(scope, false);

        return DecodedArgumentJSValueConverter<remainingSize - 1, Elements...>::convert(globalObject, array, tuple);
    }
};

template<typename... Elements>
struct DecodedArgumentJSValueConverter<0, Elements...> {
    static bool convert(JSC::JSGlobalObject*, JSC::JSArray*, const std::tuple<Elements...>&)
    {
        return true;
    }
};

template<typename... Elements>
static JSC::JSValue jsValueForArgumentTuple(JSC::JSGlobalObject* globalObject, const std::tuple<Elements...>& tuple)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* array = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    if (!DecodedArgumentJSValueConverter<sizeof...(Elements), Elements...>::convert(globalObject, array, tuple))
        return JSC::JSValue();
    return JSC::JSValue(array);
}

template<typename T>
static Optional<JSC::JSValue> jsValueForDecodedArguments(JSC::JSGlobalObject* globalObject, IPC::Decoder& decoder)
{
    Optional<typename IPC::CodingType<T>::Type> arguments;
    decoder >> arguments;
    if (!arguments)
        return WTF::nullopt;
    return jsValueForArgumentTuple(globalObject, *arguments);
}
