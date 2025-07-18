/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "JavaScriptEvaluationResult.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "APINodeInfo.h"
#include "APINumber.h"
#include "APISerializedScriptValue.h"
#include "APIString.h"
#include "WKSharedAPICast.h"
#include "WebFrame.h"
#include <WebCore/ExceptionDetails.h>
#include <WebCore/JSWebKitNodeInfo.h>
#include <WebCore/SerializedScriptValue.h>

namespace WebKit {

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JSObjectID root, HashMap<JSObjectID, Value>&& map)
    : m_map(WTFMove(map))
    , m_root(root) { }

RefPtr<API::Object> JavaScriptEvaluationResult::toAPI(Value&& root)
{
    return WTF::switchOn(WTFMove(root), [] (EmptyType) -> RefPtr<API::Object> {
        return nullptr;
    }, [] (bool value) -> RefPtr<API::Object> {
        return API::Boolean::create(value);
    }, [] (double value) -> RefPtr<API::Object> {
        return API::Double::create(value);
    }, [] (String&& value) -> RefPtr<API::Object> {
        return API::String::create(value);
    }, [] (Seconds value) -> RefPtr<API::Object> {
        return API::Double::create(value.seconds());
    }, [&] (Vector<JSObjectID>&& vector) -> RefPtr<API::Object> {
        Ref array = API::Array::create();
        m_arrays.append({ WTFMove(vector), array });
        return { WTFMove(array) };
    }, [&] (HashMap<JSObjectID, JSObjectID>&& map) -> RefPtr<API::Object> {
        Ref dictionary = API::Dictionary::create();
        m_dictionaries.append({ WTFMove(map), dictionary });
        return { WTFMove(dictionary) };
    }, [] (NodeInfo&& nodeInfo) -> RefPtr<API::Object> {
        return nullptr;
    });
}

RefPtr<API::Object> JavaScriptEvaluationResult::toAPI()
{
    for (auto [identifier, variant] : std::exchange(m_map, { }))
        m_instantiatedObjects.add(identifier, toAPI(WTFMove(variant)));
    for (auto [vector, array] : std::exchange(m_arrays, { })) {
        for (auto identifier : vector) {
            if (RefPtr object = m_instantiatedObjects.get(identifier))
                Ref { array }->append(object.releaseNonNull());
        }
    }
    for (auto [map, dictionary] : std::exchange(m_dictionaries, { })) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            RefPtr key = dynamicDowncast<API::String>(m_instantiatedObjects.get(keyIdentifier));
            if (!key)
                continue;
            RefPtr value = m_instantiatedObjects.get(valueIdentifier);
            if (!value)
                continue;
            Ref { dictionary }->add(key->string(), WTFMove(value));
        }
    }
    return std::exchange(m_instantiatedObjects, { }).take(m_root);
}

WKRetainPtr<WKTypeRef> JavaScriptEvaluationResult::toWK()
{
    return WebKit::toAPI(toAPI().get());
}

JSObjectID JavaScriptEvaluationResult::addObjectToMap(JSGlobalContextRef context, JSValueRef object)
{
    if (!object) {
        if (!m_nullObjectID) {
            m_nullObjectID = JSObjectID::generate();
            m_map.add(*m_nullObjectID, Value { EmptyType::Undefined });
        }
        return *m_nullObjectID;
    }

    Protected<JSValueRef> value(context, object);
    auto it = m_jsObjectsInMap.find(value);
    if (it != m_jsObjectsInMap.end())
        return it->value;

    auto identifier = JSObjectID::generate();
    m_jsObjectsInMap.set(WTFMove(value), identifier);
    m_map.add(identifier, toValue(context, object));
    return identifier;
}

static std::optional<std::pair<JSGlobalContextRef, JSValueRef>> roundTripThroughSerializedScriptValue(JSGlobalContextRef serializationContext, JSGlobalContextRef deserializationContext, JSValueRef value)
{
    // FIXME: Make the SerializedScriptValue roundtrip allow JSWebKitNodeInfo to allow arrays of WebKitNodeInfo.
    auto* globalObject = ::toJS(serializationContext);
    JSC::JSValue jsValue = ::toJS(globalObject, value);
    if (auto* object = jsValue.isObject() ? jsValue.toObject(globalObject) : nullptr) {
        if (object->inherits<WebCore::JSWebKitNodeInfo>())
            return { { serializationContext, value } };
    }

    if (RefPtr serialized = WebCore::SerializedScriptValue::create(serializationContext, value, nullptr))
        return { { deserializationContext, serialized->deserialize(deserializationContext, nullptr) } };
    return std::nullopt;
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(JSGlobalContextRef context, JSValueRef value)
{
    JSRetainPtr deserializationContext = API::SerializedScriptValue::deserializationContext();

    auto result = roundTripThroughSerializedScriptValue(context, deserializationContext.get(), value);
    if (!result)
        return std::nullopt;
    return { JavaScriptEvaluationResult { result->first, result->second } };
}

// Similar to JSValue's valueToObjectWithoutCopy.
auto JavaScriptEvaluationResult::toValue(JSGlobalContextRef context, JSValueRef value) -> Value
{
    if (!JSValueIsObject(context, value)) {
        if (JSValueIsBoolean(context, value))
            return JSValueToBoolean(context, value);
        if (JSValueIsNumber(context, value)) {
            value = JSValueMakeNumber(context, JSValueToNumber(context, value, 0));
            return JSValueToNumber(context, value, 0);
        }
        if (JSValueIsString(context, value)) {
            auto* globalObject = ::toJS(context);
            JSC::JSValue jsValue = ::toJS(globalObject, value);
            return jsValue.toWTFString(globalObject);
        }
        if (JSValueIsNull(context, value))
            return EmptyType::Null;
        return EmptyType::Undefined;
    }

    JSObjectRef object = JSValueToObject(context, value, 0);

    if (auto* info = jsDynamicCast<WebCore::JSWebKitNodeInfo*>(::toJS(::toJS(context), object))) {
        Ref nodeInfo { info->wrapped() };
        return NodeInfo { nodeInfo->nodeIdentifier(), nodeInfo->contentFrameIdentifier() };
    }

    if (JSValueIsDate(context, object))
        return Seconds(JSValueToNumber(context, object, 0) / 1000.0);

    if (JSValueIsArray(context, object)) {
        SUPPRESS_UNCOUNTED_ARG JSValueRef lengthPropertyName = JSValueMakeString(context, adopt(JSStringCreateWithUTF8CString("length")).get());
        JSValueRef lengthValue = JSObjectGetPropertyForKey(context, object, lengthPropertyName, nullptr);
        double lengthDouble = JSValueToNumber(context, lengthValue, nullptr);
        if (lengthDouble < 0 || lengthDouble > static_cast<double>(std::numeric_limits<size_t>::max()))
            return EmptyType::Undefined;

        size_t length = lengthDouble;
        Vector<JSObjectID> vector;
        if (!vector.tryReserveInitialCapacity(length))
            return EmptyType::Undefined;

        for (size_t i = 0; i < length; ++i)
            vector.append(addObjectToMap(context, JSObjectGetPropertyAtIndex(context, object, i, nullptr)));
        return { WTFMove(vector) };
    }

    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, object);
    size_t length = JSPropertyNameArrayGetCount(names);
    HashMap<JSObjectID, JSObjectID> map;
    for (size_t i = 0; i < length; i++) {
        JSRetainPtr<JSStringRef> key = JSPropertyNameArrayGetNameAtIndex(names, i);
        SUPPRESS_UNCOUNTED_ARG map.add(addObjectToMap(context, JSValueMakeString(context, key.get())), addObjectToMap(context, JSObjectGetPropertyForKey(context, object, JSValueMakeString(context, key.get()), nullptr)));
    }
    JSPropertyNameArrayRelease(names);
    return { WTFMove(map) };
}

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JSGlobalContextRef context, JSValueRef value)
    : m_root(addObjectToMap(context, value))
{
    m_jsObjectsInMap.clear();
    m_nullObjectID = std::nullopt;
}

JSValueRef JavaScriptEvaluationResult::toJS(JSGlobalContextRef context, Value&& root)
{
    return WTF::switchOn(WTFMove(root), [&] (EmptyType emptyType) -> JSValueRef {
        switch (emptyType) {
        case EmptyType::Undefined:
            return JSValueMakeUndefined(context);
        case EmptyType::Null:
            return JSValueMakeNull(context);
        }
    }, [&] (bool value) -> JSValueRef {
        return JSValueMakeBoolean(context, value);
    }, [&] (double value) -> JSValueRef {
        return JSValueMakeNumber(context, value);
    }, [&] (String&& value) -> JSValueRef {
        auto string = OpaqueJSString::tryCreate(WTFMove(value));
        return JSValueMakeString(context, string.get());
    }, [&] (Seconds value) -> JSValueRef {
        JSValueRef argument = JSValueMakeNumber(context, value.value() * 1000.0);
        return JSObjectMakeDate(context, 1, &argument, 0);
    }, [&] (Vector<JSObjectID>&& vector) -> JSValueRef {
        JSValueRef array = JSObjectMakeArray(context, 0, nullptr, 0);
        m_jsArrays.append({ WTFMove(vector), Protected<JSValueRef>(context, array) });
        return array;
    }, [&] (HashMap<JSObjectID, JSObjectID>&& map) -> JSValueRef {
        JSObjectRef dictionary = JSObjectMake(context, 0, 0);
        m_jsDictionaries.append({ WTFMove(map), Protected<JSObjectRef>(context, dictionary) });
        return dictionary;
    }, [] (NodeInfo&&) -> JSValueRef {
        // FIXME: Implement with checks for the element being in the expected frame.
        return nullptr;
    });
}

Protected<JSValueRef> JavaScriptEvaluationResult::toJS(JSGlobalContextRef context)
{
    for (auto [identifier, variant] : std::exchange(m_map, { }))
        m_instantiatedJSObjects.add(identifier, Protected<JSValueRef>(context, toJS(context, WTFMove(variant))));
    for (auto& [vector, array] : std::exchange(m_jsArrays, { })) {
        JSObjectRef jsArray = JSValueToObject(context, array.get(), 0);
        for (size_t index = 0; index < vector.size(); ++index) {
            auto identifier = vector[index];
            if (Protected<JSValueRef> element = m_instantiatedJSObjects.get(identifier))
                JSObjectSetPropertyAtIndex(context, jsArray, index, element.get(), 0);
        }
    }
    for (auto& [map, dictionary] : std::exchange(m_jsDictionaries, { })) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            Protected<JSValueRef> key = m_instantiatedJSObjects.get(keyIdentifier);
            if (!key)
                continue;
            ASSERT(JSValueIsString(context, key.get()));
            SUPPRESS_UNCOUNTED_ARG auto keyString = adopt(JSValueToStringCopy(context, key.get(), nullptr));
            if (!keyString)
                continue;
            Protected<JSValueRef> value = m_instantiatedJSObjects.get(valueIdentifier);
            if (!value)
                continue;
            SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, dictionary.get(), keyString.get(), value.get(), 0, 0);
        }
    }
    return std::exchange(m_instantiatedJSObjects, { }).take(m_root);
}

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JavaScriptEvaluationResult&&) = default;

JavaScriptEvaluationResult& JavaScriptEvaluationResult::operator=(JavaScriptEvaluationResult&&) = default;

JavaScriptEvaluationResult::~JavaScriptEvaluationResult() = default;

String JavaScriptEvaluationResult::toString() const
{
    auto it = m_map.find(m_root);
    if (it == m_map.end())
        return { };
    auto* string = std::get_if<String>(&it->value);
    if (!string)
        return { };
    return *string;
}

} // namespace WebKit
