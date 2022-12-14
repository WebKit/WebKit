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
#include "SharedMemory.h"
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSObjectInlines.h>
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

class Semaphore;

template<typename T, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value>* = nullptr>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, T&&)
{
    return JSC::jsUndefined();
}

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, String&&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, URL&&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, WebCore::RegistrableDomain&&);

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, IPC::Semaphore&&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, WebKit::SharedMemory::Handle&&);

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
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, size_t);

template<typename U>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, ObjectIdentifier<U>&& value)
{
    return jsValueForDecodedArgumentValue(globalObject, value.toUInt64());
}

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, WebCore::IntRect&&);
template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject*, WebCore::FloatRect&&);

template<typename U>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, OptionSet<U>&& value)
{    
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto result = jsValueForDecodedArgumentValue(globalObject, value.toRaw());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    result.getObject()->putDirect(vm, JSC::Identifier::fromString(vm, "isOptionSet"_s), JSC::jsBoolean(true));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return result;
}

bool putJSValueForDecodedArgumentAtIndexOrArrayBufferIfUndefined(JSC::JSGlobalObject*, JSC::JSArray*, unsigned index, JSC::JSValue, const uint8_t* buffer, size_t length);

template<typename... Elements>
std::optional<JSC::JSValue> putJSValueForDecodeArgumentInArray(JSC::JSGlobalObject*, IPC::Decoder&, JSC::JSArray*, size_t currentIndex, std::tuple<Elements...>*);

template<>
inline std::optional<JSC::JSValue> putJSValueForDecodeArgumentInArray(JSC::JSGlobalObject* globalObject, IPC::Decoder& decoder, JSC::JSArray* array, size_t currentIndex, std::tuple<>*)
{
    return JSC::JSValue { array };
}

template<typename T, typename... Elements>
std::optional<JSC::JSValue> putJSValueForDecodeArgumentInArray(JSC::JSGlobalObject* globalObject, IPC::Decoder& decoder, JSC::JSArray* array, size_t currentIndex, std::tuple<T, Elements...>*)
{
    auto startingBufferPosition = decoder.currentBufferPosition();
    std::optional<T> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    auto jsValue = jsValueForDecodedArgumentValue(globalObject, WTFMove(*value));
    if (jsValue.isEmpty())
        return jsValue;

    putJSValueForDecodedArgumentAtIndexOrArrayBufferIfUndefined(globalObject, array, currentIndex, jsValue,
        decoder.buffer() + startingBufferPosition, decoder.currentBufferPosition() - startingBufferPosition);

    std::tuple<Elements...>* dummyArguments = nullptr;
    return putJSValueForDecodeArgumentInArray<Elements...>(globalObject, decoder, array, currentIndex + 1, dummyArguments);
}

template<typename T>
static std::optional<JSC::JSValue> jsValueForDecodedArguments(JSC::JSGlobalObject* globalObject, IPC::Decoder& decoder)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto* array = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    T* dummyArguments = nullptr;
    return putJSValueForDecodeArgumentInArray<>(globalObject, decoder, array, 0, dummyArguments);
}

// The bindings implementation will call the function templates below to decode a message.
// These function templates are specialized by each message in their own generated file.
// Each implementation will just call the above `jsValueForDecodedArguments()` function.
// This has the benefit that upon compilation, only the message receiver implementation files are
// recompiled when the message argument types change.
// The bindings implementation, e.g. the caller of jsValueForDecodedMessage<>, does not need
// to know all the message argument types, and need to be recompiled only when the message itself
// changes.
template<MessageName>
std::optional<JSC::JSValue> jsValueForDecodedMessage(JSC::JSGlobalObject*, IPC::Decoder&);
template<MessageName>
std::optional<JSC::JSValue> jsValueForDecodedMessageReply(JSC::JSGlobalObject*, IPC::Decoder&);

}

#endif
