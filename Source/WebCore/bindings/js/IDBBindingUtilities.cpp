/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Michael Pruett <michael@68k.org>
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBBindingUtilities.h"

#include "ExceptionCode.h"
#include "IDBIndexInfo.h"
#include "IDBKey.h"
#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "IDBObjectStoreInfo.h"
#include "IDBValue.h"
#include "IndexKey.h"
#include "JSBlob.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertDate.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMExceptionHandling.h"
#include "JSFile.h"
#include "Logging.h"
#include "MessagePort.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "SharedBuffer.h"
#include "ThreadSafeDataBuffer.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/ObjectConstructor.h>

namespace WebCore {
using namespace JSC;

static bool get(JSGlobalObject& lexicalGlobalObject, JSValue object, const String& keyPathElement, JSValue& result)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (object.isString() && keyPathElement == "length") {
        result = jsNumber(asString(object)->length());
        return true;
    }
    if (!object.isObject())
        return false;

    auto* obj = asObject(object);
    Identifier identifier = Identifier::fromString(vm, keyPathElement);
    if (obj->inherits<JSArray>(vm) && keyPathElement == "length") {
        result = obj->get(&lexicalGlobalObject, identifier);
        RETURN_IF_EXCEPTION(scope, false);
        return true;
    }
    if (obj->inherits<JSBlob>(vm) && (keyPathElement == "size" || keyPathElement == "type")) {
        if (keyPathElement == "size") {
            result = jsNumber(jsCast<JSBlob*>(obj)->wrapped().size());
            return true;
        }
        if (keyPathElement == "type") {
            result = jsString(vm, jsCast<JSBlob*>(obj)->wrapped().type());
            return true;
        }
    }
    if (obj->inherits<JSFile>(vm)) {
        if (keyPathElement == "name") {
            result = jsString(vm, jsCast<JSFile*>(obj)->wrapped().name());
            return true;
        }
        if (keyPathElement == "lastModified") {
            result = jsNumber(jsCast<JSFile*>(obj)->wrapped().lastModified());
            return true;
        }
        if (keyPathElement == "lastModifiedDate") {
            result = jsDate(lexicalGlobalObject, jsCast<JSFile*>(obj)->wrapped().lastModified());
            return true;
        }
    }

    PropertyDescriptor descriptor;
    bool found = obj->getOwnPropertyDescriptor(&lexicalGlobalObject, identifier, descriptor);
    RETURN_IF_EXCEPTION(scope, false);
    if (!found)
        return false;
    if (!descriptor.enumerable())
        return false;

    result = obj->get(&lexicalGlobalObject, identifier);
    RETURN_IF_EXCEPTION(scope, false);
    return true;
}

static bool canSet(JSValue object, const String& keyPathElement)
{
    UNUSED_PARAM(keyPathElement);
    return object.isObject();
}

static bool set(VM& vm, JSValue& object, const String& keyPathElement, JSValue jsValue)
{
    if (!canSet(object, keyPathElement))
        return false;
    Identifier identifier = Identifier::fromString(vm, keyPathElement);
    asObject(object)->putDirect(vm, identifier, jsValue);
    return true;
}

JSValue toJS(JSGlobalObject& lexicalGlobalObject, JSGlobalObject& globalObject, IDBKey* key)
{
    if (!key) {
        // This must be undefined, not null.
        // Spec: http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBKeyRange
        return jsUndefined();
    }

    VM& vm = lexicalGlobalObject.vm();
    Locker<JSLock> locker(vm.apiLock());
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (key->type()) {
    case IndexedDB::KeyType::Array: {
        auto& inArray = key->array();
        unsigned size = inArray.size();
        auto outArray = constructEmptyArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), size);
        RETURN_IF_EXCEPTION(scope, JSValue());
        for (size_t i = 0; i < size; ++i) {
            outArray->putDirectIndex(&lexicalGlobalObject, i, toJS(lexicalGlobalObject, globalObject, inArray.at(i).get()));
            RETURN_IF_EXCEPTION(scope, JSValue());
        }
        return outArray;
    }
    case IndexedDB::KeyType::Binary: {
        auto* data = key->binary().data();
        if (!data) {
            ASSERT_NOT_REACHED();
            return jsNull();
        }

        auto arrayBuffer = ArrayBuffer::create(data->data(), data->size());
        Structure* structure = globalObject.arrayBufferStructure(arrayBuffer->sharingMode());
        if (!structure)
            return jsNull();

        return JSArrayBuffer::create(lexicalGlobalObject.vm(), structure, WTFMove(arrayBuffer));
    }
    case IndexedDB::KeyType::String:
        return jsStringWithCache(vm, key->string());
    case IndexedDB::KeyType::Date:
        // FIXME: This should probably be toJS<IDLDate>(...) as per:
        // http://w3c.github.io/IndexedDB/#request-convert-a-key-to-a-value
        RELEASE_AND_RETURN(scope, toJS<IDLNullable<IDLDate>>(lexicalGlobalObject, key->date()));
    case IndexedDB::KeyType::Number:
        return jsNumber(key->number());
    case IndexedDB::KeyType::Min:
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Invalid:
        ASSERT_NOT_REACHED();
        return jsUndefined();
    }

    ASSERT_NOT_REACHED();
    return jsUndefined();
}

static const size_t maximumDepth = 2000;

static RefPtr<IDBKey> createIDBKeyFromValue(JSGlobalObject& lexicalGlobalObject, JSValue value, Vector<JSArray*>& stack)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isNumber() && !std::isnan(value.asNumber()))
        return IDBKey::createNumber(value.asNumber());

    if (value.isString()) {
        auto string = asString(value)->value(&lexicalGlobalObject);
        RETURN_IF_EXCEPTION(scope, { });
        return IDBKey::createString(WTFMove(string));
    }

    if (value.inherits<DateInstance>(vm)) {
        auto dateValue = valueToDate(vm, value);
        if (!std::isnan(dateValue))
            return IDBKey::createDate(dateValue);
    }

    if (value.isObject()) {
        JSObject* object = asObject(value);
        if (auto* array = jsDynamicCast<JSArray*>(vm, object)) {
            size_t length = array->length();

            if (stack.contains(array))
                return nullptr;

            if (stack.size() >= maximumDepth)
                return nullptr;

            stack.append(array);

            Vector<RefPtr<IDBKey>> subkeys;
            for (size_t i = 0; i < length; i++) {
                JSValue item = array->getIndex(&lexicalGlobalObject, i);
                RETURN_IF_EXCEPTION(scope, { });
                RefPtr<IDBKey> subkey = createIDBKeyFromValue(lexicalGlobalObject, item, stack);
                RETURN_IF_EXCEPTION(scope, { });
                if (!subkey)
                    subkeys.append(IDBKey::createInvalid());
                else
                    subkeys.append(subkey);
            }

            stack.removeLast();
            return IDBKey::createArray(subkeys);
        }

        if (auto* arrayBuffer = jsDynamicCast<JSArrayBuffer*>(vm, value))
            return IDBKey::createBinary(*arrayBuffer);

        if (auto* arrayBufferView = jsDynamicCast<JSArrayBufferView*>(vm, value))
            return IDBKey::createBinary(*arrayBufferView);
    }
    return nullptr;
}

static Ref<IDBKey> createIDBKeyFromValue(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    Vector<JSArray*> stack;
    RefPtr<IDBKey> key = createIDBKeyFromValue(lexicalGlobalObject, value, stack);
    if (key)
        return *key;
    return IDBKey::createInvalid();
}

static JSValue getNthValueOnKeyPath(JSGlobalObject& lexicalGlobalObject, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    JSValue currentValue(rootValue);
    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; i++) {
        JSValue parentValue(currentValue);
        if (!get(lexicalGlobalObject, parentValue, keyPathElements[i], currentValue))
            return jsUndefined();
    }
    return currentValue;
}

static RefPtr<IDBKey> internalCreateIDBKeyFromScriptValueAndKeyPath(JSGlobalObject& lexicalGlobalObject, JSValue value, const String& keyPath)
{
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    JSValue jsValue = value;
    jsValue = getNthValueOnKeyPath(lexicalGlobalObject, jsValue, keyPathElements, keyPathElements.size());
    if (jsValue.isUndefined())
        return nullptr;
    return createIDBKeyFromValue(lexicalGlobalObject, jsValue);
}

static JSValue ensureNthValueOnKeyPath(JSGlobalObject& lexicalGlobalObject, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    JSValue currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; i++) {
        JSValue parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(lexicalGlobalObject, parentValue, keyPathElement, currentValue)) {
            JSObject* object = constructEmptyObject(&lexicalGlobalObject);
            if (!set(lexicalGlobalObject.vm(), parentValue, keyPathElement, JSValue(object)))
                return jsUndefined();
            currentValue = JSValue(object);
        }
    }

    return currentValue;
}

static bool canInjectNthValueOnKeyPath(JSGlobalObject& lexicalGlobalObject, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    if (!rootValue.isObject())
        return false;

    JSValue currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i <= index; ++i) {
        JSValue parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(lexicalGlobalObject, parentValue, keyPathElement, currentValue))
            return canSet(parentValue, keyPathElement);
    }
    return true;
}

bool injectIDBKeyIntoScriptValue(JSGlobalObject& lexicalGlobalObject, const IDBKeyData& keyData, JSValue value, const IDBKeyPath& keyPath)
{
    LOG(IndexedDB, "injectIDBKeyIntoScriptValue");

    ASSERT(WTF::holds_alternative<String>(keyPath));

    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(WTF::get<String>(keyPath), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (keyPathElements.isEmpty())
        return false;

    JSValue parent = ensureNthValueOnKeyPath(lexicalGlobalObject, value, keyPathElements, keyPathElements.size() - 1);
    if (parent.isUndefined())
        return false;

    auto key = keyData.maybeCreateIDBKey();
    if (!key)
        return false;

    // Do not set if object already has the correct property value.
    JSValue existingKey;
    if (get(lexicalGlobalObject, parent, keyPathElements.last(), existingKey) && !key->compare(createIDBKeyFromValue(lexicalGlobalObject, existingKey)))
        return true;
    if (!set(lexicalGlobalObject.vm(), parent, keyPathElements.last(), toJS(lexicalGlobalObject, lexicalGlobalObject, key.get())))
        return false;

    return true;
}


RefPtr<IDBKey> maybeCreateIDBKeyFromScriptValueAndKeyPath(JSGlobalObject& lexicalGlobalObject, JSValue value, const IDBKeyPath& keyPath)
{
    if (WTF::holds_alternative<Vector<String>>(keyPath)) {
        auto& array = WTF::get<Vector<String>>(keyPath);
        Vector<RefPtr<IDBKey>> result;
        result.reserveInitialCapacity(array.size());
        for (auto& string : array) {
            RefPtr<IDBKey> key = internalCreateIDBKeyFromScriptValueAndKeyPath(lexicalGlobalObject, value, string);
            if (!key)
                return nullptr;
            result.uncheckedAppend(WTFMove(key));
        }
        return IDBKey::createArray(WTFMove(result));
    }

    return internalCreateIDBKeyFromScriptValueAndKeyPath(lexicalGlobalObject, value, WTF::get<String>(keyPath));
}

bool canInjectIDBKeyIntoScriptValue(JSGlobalObject& lexicalGlobalObject, JSValue scriptValue, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "canInjectIDBKeyIntoScriptValue");

    ASSERT(WTF::holds_alternative<String>(keyPath));
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(WTF::get<String>(keyPath), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (!keyPathElements.size())
        return false;

    return canInjectNthValueOnKeyPath(lexicalGlobalObject, scriptValue, keyPathElements, keyPathElements.size() - 1);
}

static JSValue deserializeIDBValueToJSValue(JSGlobalObject& lexicalGlobalObject, JSC::JSGlobalObject& globalObject, const IDBValue& value)
{
    // FIXME: I think it's peculiar to use undefined to mean "null data" and null to mean "empty data".
    // But I am not changing this at the moment because at least some callers are specifically checking isUndefined.

    if (!value.data().data())
        return jsUndefined();

    auto& data = *value.data().data();
    if (data.isEmpty())
        return jsNull();

    auto serializedValue = SerializedScriptValue::createFromWireBytes(Vector<uint8_t>(data));

    lexicalGlobalObject.vm().apiLock().lock();
    Vector<RefPtr<MessagePort>> messagePorts;
    JSValue result = serializedValue->deserialize(lexicalGlobalObject, &globalObject, messagePorts, value.blobURLs(), value.blobFilePaths(), SerializationErrorMode::NonThrowing);
    lexicalGlobalObject.vm().apiLock().unlock();

    return result;
}

JSValue deserializeIDBValueToJSValue(JSGlobalObject& lexicalGlobalObject, const IDBValue& value)
{
    return deserializeIDBValueToJSValue(lexicalGlobalObject, lexicalGlobalObject, value);
}

JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, const IDBValue& value)
{
    ASSERT(lexicalGlobalObject);
    return deserializeIDBValueToJSValue(*lexicalGlobalObject, *globalObject, value);
}

Ref<IDBKey> scriptValueToIDBKey(JSGlobalObject& lexicalGlobalObject, JSValue scriptValue)
{
    return createIDBKeyFromValue(lexicalGlobalObject, scriptValue);
}

JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, const IDBKeyData& keyData)
{
    ASSERT(lexicalGlobalObject);
    ASSERT(globalObject);

    return toJS(*lexicalGlobalObject, *globalObject, keyData.maybeCreateIDBKey().get());
}

static Vector<IDBKeyData> createKeyPathArray(JSGlobalObject& lexicalGlobalObject, JSValue value, const IDBIndexInfo& info, Optional<IDBKeyPath> objectStoreKeyPath, const IDBKeyData& objectStoreKey)
{
    auto visitor = WTF::makeVisitor([&](const String& string) -> Vector<IDBKeyData> {
        // Value doesn't contain auto-generated key, so we need to manually add key if it is possibly auto-generated.
        if (objectStoreKeyPath && WTF::holds_alternative<String>(objectStoreKeyPath.value()) && IDBKeyPath(string) == objectStoreKeyPath.value())
            return { objectStoreKey };

        auto idbKey = internalCreateIDBKeyFromScriptValueAndKeyPath(lexicalGlobalObject, value, string);
        if (!idbKey)
            return { };

        Vector<IDBKeyData> keys;
        if (info.multiEntry() && idbKey->type() == IndexedDB::Array) {
            for (auto& key : idbKey->array())
                keys.append(key.get());
        } else
            keys.append(idbKey.get());
        return keys;
    }, [&](const Vector<String>& vector) -> Vector<IDBKeyData> {
        Vector<IDBKeyData> keys;
        for (auto& entry : vector) {
            if (objectStoreKeyPath && WTF::holds_alternative<String>(objectStoreKeyPath.value()) && IDBKeyPath(entry) == objectStoreKeyPath.value())
                keys.append(objectStoreKey);
            else {
                auto key = internalCreateIDBKeyFromScriptValueAndKeyPath(lexicalGlobalObject, value, entry);
                if (!key || !key->isValid())
                    return { };
                keys.append(key.get());
            }
        }
        return keys;
    });

    return WTF::visit(visitor, info.keyPath());
}

void generateIndexKeyForValue(JSGlobalObject& lexicalGlobalObject, const IDBIndexInfo& info, JSValue value, IndexKey& outKey, const Optional<IDBKeyPath>& objectStoreKeyPath, const IDBKeyData& objectStoreKey)
{
    auto keyDatas = createKeyPathArray(lexicalGlobalObject, value, info, objectStoreKeyPath, objectStoreKey);
    if (keyDatas.isEmpty())
        return;

    outKey = IndexKey(WTFMove(keyDatas));
}

IndexIDToIndexKeyMap generateIndexKeyMapForValue(JSC::JSGlobalObject& lexicalGlobalObject, const IDBObjectStoreInfo& storeInfo, const IDBKeyData& key, const IDBValue& value)
{
    auto& indexMap = storeInfo.indexMap();
    auto indexCount = indexMap.size();
    if (!indexCount)
        return IndexIDToIndexKeyMap { };

    JSLockHolder locker(lexicalGlobalObject.vm());
    auto jsValue = deserializeIDBValueToJSValue(lexicalGlobalObject, value);
    if (jsValue.isUndefinedOrNull())
        return IndexIDToIndexKeyMap { };

    IndexIDToIndexKeyMap indexKeys;
    indexKeys.reserveInitialCapacity(indexCount);

    for (const auto& entry : indexMap) {
        IndexKey indexKey;
        generateIndexKeyForValue(lexicalGlobalObject, entry.value, jsValue, indexKey, storeInfo.keyPath(), key);

        if (indexKey.isNull())
            continue;

        indexKeys.add(entry.key, WTFMove(indexKey));
    }

    return indexKeys;
}

Optional<JSC::JSValue> deserializeIDBValueWithKeyInjection(JSGlobalObject& lexicalGlobalObject, const IDBValue& value, const IDBKeyData& key, const Optional<IDBKeyPath>& keyPath)
{
    auto jsValue = deserializeIDBValueToJSValue(lexicalGlobalObject, value);
    if (jsValue.isUndefined() || !keyPath || !WTF::holds_alternative<String>(keyPath.value()) || !isIDBKeyPathValid(keyPath.value()))
        return jsValue;

    JSLockHolder locker(lexicalGlobalObject.vm());
    if (!injectIDBKeyIntoScriptValue(lexicalGlobalObject, key, jsValue, keyPath.value())) {
        auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
        propagateException(lexicalGlobalObject, throwScope, Exception(UnknownError, "Cannot inject key into script value"_s));
        return WTF::nullopt;
    }

    return jsValue;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
