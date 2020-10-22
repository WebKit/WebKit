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

#if ENABLE(IPC_TESTING_API)

#include "Decoder.h"
#include "HandleMessage.h"
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class URL;

}

namespace WebCore {

class FloatRect;
class IntRect;
class RegistrableDomain;

}

namespace IPC {

template<typename T, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const T&)
{
    return JSC::jsUndefined();
}

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const String&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const URL&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const WebCore::RegistrableDomain&);

template<typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, T)
{
}

template<typename E, std::enable_if_t<std::is_enum<E>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, E value)
{
    return jsValueForDecodedArgumentValue(globalObject, static_cast<std::underlying_type_t<E>>(value));
}

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, bool);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, double);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, float);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, int8_t);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, int16_t);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, int32_t);

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, int64_t);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, uint8_t);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, uint16_t);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, uint32_t);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, uint64_t);

template<typename U>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const ObjectIdentifier<U>& value)
{
    return jsValueForDecodedArgumentValue(globalObject, value.toUInt64());
}

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const WebCore::IntRect&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, const WebCore::FloatRect&);

template<typename U>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, const OptionSet<U>& value)
{    
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto result = jsValueForDecodedArgumentValue(globalObject, value.toRaw());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    result.getObject()->putDirect(vm, JSC::Identifier::fromString(vm, "isOptionSet"_s), JSC::jsBoolean(true));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return result;
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

}

#endif
