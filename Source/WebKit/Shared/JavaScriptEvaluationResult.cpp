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
#include "APIJSHandle.h"
#include "APINumber.h"
#include "APISerializedNode.h"
#include "APIString.h"
#include "InjectedBundleScriptWorld.h"
#include "Logging.h"
#include "WKSharedAPICast.h"
#include "WebFrame.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/Document.h>
#include <WebCore/ExceptionDetails.h>
#include <WebCore/JSWebKitJSHandle.h>
#include <WebCore/JSWebKitSerializedNode.h>
#include <WebCore/ScriptWrappableInlines.h>
#include <WebCore/SerializedScriptValue.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebKit {

class JavaScriptEvaluationResult::JSExtractor {
public:
    Map takeMap() { return WTFMove(m_map); }
    std::optional<JSObjectID> addObjectToMap(JSGlobalContextRef, JSValueRef);

private:
    void extractJSValue(JSGlobalContextRef, JSValueRef);

    struct UnsupportedTypeTag { };
    struct PendingCollectionTag { };
    using ExtractionResult = Variant <
        Value,
        UnsupportedTypeTag,
        PendingCollectionTag
    >;

    ExtractionResult jsValueToExtractedValue(JSGlobalContextRef, JSValueRef);

    Map m_map;
    HashMap<Protected<JSValueRef>, JSObjectID> m_valuesInMap;

    static constexpr size_t maximumNestingLevel { 40000 };
    size_t m_currentNestingLevel { 0 };

    Vector<Protected<JSValueRef>> m_futureJSValuesToExtract;
    Vector<Protected<JSValueRef>> m_currentJSValuesToExtract;
    HashMap<Protected<JSValueRef>, Vector<Protected<JSValueRef>>> m_jsArraysToExtract;
    HashMap<Protected<JSValueRef>, HashMap<Protected<JSValueRef>, Protected<JSValueRef>>> m_jsObjectsToExtract;
};

class JavaScriptEvaluationResult::JSInserter {
public:
    using Dictionaries = Vector<std::pair<ObjectMap, Protected<JSObjectRef>>>;
    using Arrays = Vector<std::pair<Vector<JSObjectID>, Protected<JSValueRef>>>;
    JSValueRef toJS(JSGlobalContextRef, Value&&);
    Dictionaries takeDictionaries() { return WTFMove(m_dictionaries); }
    Arrays takeArrays() { return WTFMove(m_arrays); }
private:
    Dictionaries m_dictionaries;
    Arrays m_arrays;
};

class JavaScriptEvaluationResult::APIExtractor {
public:
    Map takeMap() { return WTFMove(m_map); }
    JSObjectID addObjectToMap(API::Object&);
private:
    Value toValue(API::Object&);

    HashMap<Ref<API::Object>, JSObjectID> m_objectsInMap;
    Map m_map;
};

class JavaScriptEvaluationResult::APIInserter {
public:
    using Dictionaries = Vector<std::pair<ObjectMap, Ref<API::Dictionary>>>;
    using Arrays = Vector<std::pair<Vector<JSObjectID>, Ref<API::Array>>>;
    RefPtr<API::Object> toAPI(Value&&);
    Dictionaries takeDictionaries() { return WTFMove(m_dictionaries); }
    Arrays takeArrays() { return WTFMove(m_arrays); }
private:
    Dictionaries m_dictionaries;
    Arrays m_arrays;
};

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JSObjectID root, Map&& map)
    : m_map(WTFMove(map))
    , m_root(root) { }

RefPtr<API::Object> JavaScriptEvaluationResult::APIInserter::toAPI(Value&& root)
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
    }, [&] (ObjectMap&& map) -> RefPtr<API::Object> {
        Ref dictionary = API::Dictionary::create();
        m_dictionaries.append({ WTFMove(map), dictionary });
        return { WTFMove(dictionary) };
    }, [] (UniqueRef<JSHandleInfo>&& info) -> RefPtr<API::Object> {
        return API::JSHandle::create(WTFMove(info.get()));
    }, [] (UniqueRef<WebCore::SerializedNode>&& node) -> RefPtr<API::Object> {
        return API::SerializedNode::create(WTFMove(node.get()));
    });
}

static bool isSerializable(API::Object* object)
{
    if (!object)
        return false;

    switch (object->type()) {
    case API::Object::Type::String:
    case API::Object::Type::Boolean:
    case API::Object::Type::Double:
    case API::Object::Type::UInt64:
    case API::Object::Type::Int64:
    case API::Object::Type::JSHandle:
    case API::Object::Type::SerializedNode:
        return true;
    case API::Object::Type::Array:
        return std::ranges::all_of(downcast<API::Array>(object)->elements(), [] (const RefPtr<API::Object>& element) {
            return isSerializable(element.get());
        });
    case API::Object::Type::Dictionary:
        return std::ranges::all_of(downcast<API::Dictionary>(object)->map(), [] (const KeyValuePair<String, RefPtr<API::Object>>& pair) {
            return isSerializable(pair.value.get());
        });
    default:
        return false;
    }
}

auto JavaScriptEvaluationResult::APIExtractor::toValue(API::Object& object) -> Value
{
    switch (object.type()) {
    case API::Object::Type::String:
        return downcast<API::String>(object).string();
    case API::Object::Type::Boolean:
        return downcast<API::Boolean>(object).value();
    case API::Object::Type::Double:
        return downcast<API::Double>(object).value();
    case API::Object::Type::UInt64:
        return static_cast<double>(downcast<API::UInt64>(object).value());
    case API::Object::Type::Int64:
        return static_cast<double>(downcast<API::Int64>(object).value());
    case API::Object::Type::JSHandle:
        return makeUniqueRef<JSHandleInfo>(downcast<API::JSHandle>(object).info());
    case API::Object::Type::SerializedNode:
        return makeUniqueRef<WebCore::SerializedNode>(downcast<API::SerializedNode>(object).coreSerializedNode());
    case API::Object::Type::Array: {
        Vector<JSObjectID> vector;
        for (RefPtr element : downcast<API::Array>(object).elements()) {
            if (element)
                vector.append(addObjectToMap(*element));
        }
        return { WTFMove(vector) };
    }
    case API::Object::Type::Dictionary: {
        ObjectMap map;
        for (auto& [key, value] : downcast<API::Dictionary>(object).map()) {
            if (RefPtr protectedValue = value)
                map.set(addObjectToMap(API::String::create(key).get()), addObjectToMap(*protectedValue));
        }
        return { WTFMove(map) };
    }
    default:
        // This object has been null checked and went through isSerializable which only supports these types.
        ASSERT_NOT_REACHED();
        return EmptyType::Undefined;
    }
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(API::Object* object)
{
    if (!object)
        return jsUndefined();
    if (!isSerializable(object))
        return std::nullopt;
    APIExtractor extractor;
    auto root = extractor.addObjectToMap(*object);
    return JavaScriptEvaluationResult { root, extractor.takeMap() };
}

JSObjectID JavaScriptEvaluationResult::APIExtractor::addObjectToMap(API::Object& object)
{
    auto it = m_objectsInMap.find(object);
    if (it != m_objectsInMap.end())
        return it->value;

    auto identifier = JSObjectID::generate();
    m_objectsInMap.set(object, identifier);
    m_map.add(identifier, toValue(object));
    return identifier;
}

RefPtr<API::Object> JavaScriptEvaluationResult::toAPI()
{
    HashMap<JSObjectID, RefPtr<API::Object>> instantiatedObjects;
    APIInserter inserter;

    for (auto&& [identifier, value] : std::exchange(m_map, { }))
        instantiatedObjects.add(identifier, inserter.toAPI(WTFMove(value)));

    for (auto [vector, array] : inserter.takeArrays()) {
        for (auto identifier : vector) {
            if (RefPtr object = instantiatedObjects.get(identifier))
                Ref { array }->append(object.releaseNonNull());
        }
    }

    for (auto [map, dictionary] : inserter.takeDictionaries()) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            RefPtr key = dynamicDowncast<API::String>(instantiatedObjects.get(keyIdentifier));
            if (!key)
                continue;
            RefPtr value = instantiatedObjects.get(valueIdentifier);
            if (!value)
                continue;
            Ref { dictionary }->add(key->string(), WTFMove(value));
        }
    }

    return std::exchange(instantiatedObjects, { }).take(m_root);
}

std::optional<JSObjectID> JavaScriptEvaluationResult::JSExtractor::addObjectToMap(JSGlobalContextRef context, JSValueRef rootValue)
{
    ASSERT(context);
    ASSERT(rootValue);

    // Extract the root value.
    // If it is a collection (Array or Object), then we'll handle its sub-objects iteratively.
    extractJSValue(context, rootValue);

    // The m_futureJSValuesToExtract set contains all sub-objects from the previous nesting level's collections.
    // We extract all objects from the current nesting level before moving one level deeper,
    // iterating until we either run out of objects or hit our nesting limit.
    while (!m_futureJSValuesToExtract.isEmpty()) {
        if (++m_currentNestingLevel > maximumNestingLevel) {
            RELEASE_LOG(IPC, "When serializing JavaScript object for IPC to the UI Process, reached maxiumum nesting limit of %lu", maximumNestingLevel);
            return std::nullopt;
        }

        RELEASE_ASSERT(m_currentJSValuesToExtract.isEmpty());
        m_currentJSValuesToExtract = std::exchange(m_futureJSValuesToExtract, { });

        while (!m_currentJSValuesToExtract.isEmpty()) {
            auto protectedValue = m_currentJSValuesToExtract.takeLast();
            extractJSValue(context, protectedValue.get());
        }
    }

    // For every Array we encountered previously we kept a placeholder Vector of JSValueRefs.
    // Now that we have the JSObjectID for every JSValueRef, we can fill in the final Array values.
    for (auto& [arrayObject, valueVector] : m_jsArraysToExtract) {
        auto iterator = m_valuesInMap.find(arrayObject);
        ASSERT(iterator != m_valuesInMap.end());

        Vector<JSObjectID> mapVector;
        for (auto& valueEntry : valueVector) {
            auto identifierIterator = m_valuesInMap.find(valueEntry);
            if (identifierIterator == m_valuesInMap.end())
                continue;
            mapVector.append(identifierIterator->value);
        }

        auto result = m_map.add(iterator->value, WTFMove(mapVector));
        RELEASE_ASSERT(result.isNewEntry);
    }

    // For every Object we encountered previously we kept a placeholder HashMap of JSValueRefs.
    // Now that we have the JSObjectID for every JSValueRef, we can fill in the final Object values.
    for (auto& [object, valueMap] : m_jsObjectsToExtract) {
        auto iterator = m_valuesInMap.find(object);
        ASSERT(iterator != m_valuesInMap.end());

        ObjectMap objectMap;
        for (auto& [key, value] : valueMap) {
            auto keyIdentifier = m_valuesInMap.find(key);
            if (keyIdentifier == m_valuesInMap.end())
                continue;
            auto valueIdentifier = m_valuesInMap.find(value);
            if (valueIdentifier == m_valuesInMap.end())
                continue;

            objectMap.set(keyIdentifier->value, valueIdentifier->value);
        }

        auto result = m_map.add(iterator->value, WTFMove(objectMap));
        RELEASE_ASSERT(result.isNewEntry);
    }

    return m_valuesInMap.get({ context, rootValue });
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(JSGlobalContextRef context, JSValueRef value)
{
    if (!context || !value) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    JSExtractor extractor;
    if (auto root = extractor.addObjectToMap(context, value)) {
#if PLATFORM(COCOA)
        if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::JavaScriptEvaluationResultWithoutSerializedScriptValue)
            && !WebCore::SerializedScriptValue::create(context, value, nullptr))
            return std::nullopt;
#endif
        return JavaScriptEvaluationResult { *root, extractor.takeMap() };
    }
    return std::nullopt;
}

void JavaScriptEvaluationResult::JSExtractor::extractJSValue(JSGlobalContextRef context, JSValueRef value)
{
    auto addResult = m_valuesInMap.ensure({ context, value }, [] {
        return JSObjectID::generate();
    });

    if (!addResult.isNewEntry)
        return;
    auto identifier = addResult.iterator->value;
    auto extractedValue = jsValueToExtractedValue(context, value);

    WTF::switchOn(WTFMove(extractedValue), [&] (Value&& value) {
        auto result = m_map.set(identifier, WTFMove(value));
        RELEASE_ASSERT(result.isNewEntry);
    }, [&] (UnsupportedTypeTag) {
        // We already committed an entry into the values map for this object identifier,
        // but it failed to extract. So remove it from the values map.
        m_valuesInMap.remove({ context, value });
    }, [] (PendingCollectionTag) {
        // This space intentionally left blank, as Array/Object values will be finished up later.
    });
}

auto JavaScriptEvaluationResult::JSExtractor::jsValueToExtractedValue(JSGlobalContextRef context, JSValueRef value) -> ExtractionResult
{
    using namespace WebCore;

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
    JSC::JSGlobalObject* globalObject = ::toJS(context);
    JSC::JSObject* jsObject = ::toJS(globalObject, object).toObject(globalObject);

    if (auto* info = jsDynamicCast<JSWebKitJSHandle*>(jsObject)) {
        RELEASE_ASSERT(globalObject->template inherits<WebCore::JSDOMGlobalObject>());
        auto* domGlobalObject = jsCast<WebCore::JSDOMGlobalObject*>(globalObject);
        RefPtr document = dynamicDowncast<Document>(domGlobalObject->scriptExecutionContext());
        RefPtr frame = WebFrame::webFrame(document->frameID());
        RefPtr world = InjectedBundleScriptWorld::get(domGlobalObject->world());
        Ref ref { info->wrapped() };
        WebCore::WebKitJSHandle::jsHandleSentToAnotherProcess(ref->identifier());
        return makeUniqueRef<JSHandleInfo>(ref->identifier(), world->identifier(), frame->info(), ref->windowFrameIdentifier());
    }

    if (auto* node = jsDynamicCast<JSWebKitSerializedNode*>(jsObject)) {
        Ref serializedNode { node->wrapped() };
        return makeUniqueRef<SerializedNode>(serializedNode->serializedNode());
    }

    if (JSValueIsDate(context, object))
        return Seconds(JSValueToNumber(context, object, 0) / 1000.0);

    if (JSValueIsArray(context, object)) {
        JSValueRef exception { nullptr };
        SUPPRESS_UNCOUNTED_ARG JSValueRef lengthPropertyName = JSValueMakeString(context, adopt(JSStringCreateWithUTF8CString("length")).get());
        JSValueRef lengthValue = JSObjectGetPropertyForKey(context, object, lengthPropertyName, &exception);
        if (exception)
            return UnsupportedTypeTag { };
        double lengthDouble = JSValueToNumber(context, lengthValue, &exception);
        if (exception)
            return JSExtractor::UnsupportedTypeTag { };
        if (lengthDouble < 0 || lengthDouble > static_cast<double>(std::numeric_limits<size_t>::max()))
            return EmptyType::Undefined;

        size_t length = lengthDouble;
        Vector<Protected<JSValueRef>> vector;
        if (!vector.tryReserveInitialCapacity(length))
            return EmptyType::Undefined;

        for (size_t i = 0; i < length; ++i) {
            JSValueRef exception { nullptr };
            JSValueRef element = JSObjectGetPropertyAtIndex(context, object, i, &exception);
            if (!element || exception)
                return UnsupportedTypeTag { };
            m_futureJSValuesToExtract.append({ context, element });
            vector.append({ context, element });

        }

        // The m_jsArraysToExtract map remembers what this extracted Array should eventually look like.
        // We'll do a final step later to fill it in with the correct extracted values.
        m_jsArraysToExtract.add({ context, object }, WTFMove(vector));
        return PendingCollectionTag { };
    }

    switch (SerializedScriptValue::deserializationBehavior(*jsObject)) {
    case SerializedScriptValue::DeserializationBehavior::Fail:
        return UnsupportedTypeTag { };
    case SerializedScriptValue::DeserializationBehavior::Succeed:
        break;
    case SerializedScriptValue::DeserializationBehavior::LegacyMapToNull:
        return EmptyType::Null;
    case SerializedScriptValue::DeserializationBehavior::LegacyMapToUndefined:
        return EmptyType::Undefined;
    case SerializedScriptValue::DeserializationBehavior::LegacyMapToEmptyObject:
        return Value { ObjectMap { } };
    }

    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, object);
    size_t length = JSPropertyNameArrayGetCount(names);
    HashMap<Protected<JSValueRef>, Protected<JSValueRef>> map;
    for (size_t i = 0; i < length; i++) {
        JSRetainPtr<JSStringRef> key = JSPropertyNameArrayGetNameAtIndex(names, i);
        SUPPRESS_UNCOUNTED_ARG JSValueRef keyJSValue = JSValueMakeString(context, key.get());
        JSValueRef exception { nullptr };
        SUPPRESS_UNCOUNTED_ARG JSValueRef valueJSValue = JSObjectGetPropertyForKey(context, object, keyJSValue, &exception);
        if (exception)
            continue;

        m_futureJSValuesToExtract.append({ context, keyJSValue });
        m_futureJSValuesToExtract.append({ context, valueJSValue });
        map.add(Protected<JSValueRef> { context, keyJSValue }, Protected<JSValueRef> { context, valueJSValue });
    }
    JSPropertyNameArrayRelease(names);

    // The m_jsObjectsToExtract map remembers what this extracted Object should eventually look like.
    // We'll do a final step later to fill it in with the correct extracted values.
    m_jsObjectsToExtract.add(Protected<JSValueRef> { context, object }, WTFMove(map));

    return PendingCollectionTag { };
}

JSValueRef JavaScriptEvaluationResult::JSInserter::toJS(JSGlobalContextRef context, Value&& root)
{
    auto globalObjectTuple = [] (auto context) {
        auto* lexicalGlobalObject = ::toJS(context);
        RELEASE_ASSERT(lexicalGlobalObject->template inherits<WebCore::JSDOMGlobalObject>());
        auto* domGlobalObject = jsCast<WebCore::JSDOMGlobalObject*>(lexicalGlobalObject);
        RefPtr document = dynamicDowncast<WebCore::Document>(domGlobalObject->scriptExecutionContext());
        RELEASE_ASSERT(document);
        return std::make_tuple(lexicalGlobalObject, domGlobalObject, WTFMove(document));
    };

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
        JSValueRef exception { nullptr };
        JSValueRef argument = JSValueMakeNumber(context, value.value() * 1000.0);
        JSObjectRef date = JSObjectMakeDate(context, 1, &argument, &exception);
        if (date && !exception)
            return date;
        ASSERT_NOT_REACHED();
        return JSValueMakeUndefined(context);
    }, [&] (Vector<JSObjectID>&& vector) -> JSValueRef {
        JSValueRef exception { nullptr };
        JSValueRef array = JSObjectMakeArray(context, 0, nullptr, &exception);
        if (array && !exception) {
            m_arrays.append({ WTFMove(vector), Protected<JSValueRef>(context, array) });
            return array;
        }
        ASSERT_NOT_REACHED();
        return JSValueMakeUndefined(context);
    }, [&] (ObjectMap&& map) -> JSValueRef {
        if (JSObjectRef dictionary = JSObjectMake(context, nullptr, nullptr)) {
            m_dictionaries.append({ WTFMove(map), Protected<JSObjectRef>(context, dictionary) });
            return dictionary;
        }
        ASSERT_NOT_REACHED();
        return JSValueMakeUndefined(context);
    }, [&] (UniqueRef<JSHandleInfo>&& info) -> JSValueRef {
        auto* object = WebCore::WebKitJSHandle::objectForIdentifier(info.get().identifier);
        if (!object)
            return JSValueMakeUndefined(context);
        auto [lexicalGlobalObject, domGlobalObject, document] = globalObjectTuple(context);
        if (lexicalGlobalObject != object->globalObject())
            return JSValueMakeUndefined(context);
        return ::toRef(object);
    }, [&] (UniqueRef<WebCore::SerializedNode>&& serializedNode) -> JSValueRef {
        auto [lexicalGlobalObject, domGlobalObject, document] = globalObjectTuple(context);
        return ::toRef(lexicalGlobalObject, WebCore::SerializedNode::deserialize(WTFMove(serializedNode.get()), lexicalGlobalObject, domGlobalObject, *document));
    });
}

Protected<JSValueRef> JavaScriptEvaluationResult::toJS(JSGlobalContextRef context)
{
    HashMap<JSObjectID, Protected<JSValueRef>> instantiatedJSObjects;
    JSInserter inserter;

    for (auto&& [identifier, value] : std::exchange(m_map, { }))
        instantiatedJSObjects.add(identifier, Protected<JSValueRef>(context, inserter.toJS(context, WTFMove(value))));

    for (auto& [vector, array] : inserter.takeArrays()) {
        JSObjectRef jsArray = JSValueToObject(context, array.get(), 0);
        for (size_t index = 0; index < vector.size(); ++index) {
            auto identifier = vector[index];
            if (Protected<JSValueRef> element = instantiatedJSObjects.get(identifier))
                JSObjectSetPropertyAtIndex(context, jsArray, index, element.get(), 0);
        }
    }

    for (auto& [map, dictionary] : inserter.takeDictionaries()) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            Protected<JSValueRef> key = instantiatedJSObjects.get(keyIdentifier);
            if (!key)
                continue;
            ASSERT(JSValueIsString(context, key.get()));
            JSValueRef exception { nullptr };
            SUPPRESS_UNCOUNTED_ARG auto keyString = adopt(JSValueToStringCopy(context, key.get(), &exception));
            if (!keyString || exception)
                continue;
            Protected<JSValueRef> value = instantiatedJSObjects.get(valueIdentifier);
            if (!value)
                continue;
            SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, dictionary.get(), keyString.get(), value.get(), 0, 0);
        }
    }

    return std::exchange(instantiatedJSObjects, { }).take(m_root);
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

JavaScriptEvaluationResult JavaScriptEvaluationResult::jsUndefined()
{
    auto root = JSObjectID::generate();
    Map map;
    map.set(root, EmptyType::Undefined);
    return { root, WTFMove(map) };
}

} // namespace WebKit
