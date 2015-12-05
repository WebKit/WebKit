/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Michael Pruett <michael@68k.org>
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

#include "DOMRequestState.h"
#include "IDBIndexInfo.h"
#include "IDBIndexMetadata.h"
#include "IDBKey.h"
#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "IndexKey.h"
#include "JSDOMBinding.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include "ThreadSafeDataBuffer.h"

#include <runtime/DateInstance.h>
#include <runtime/ObjectConstructor.h>

using namespace JSC;

namespace WebCore {

static bool get(ExecState* exec, JSValue object, const String& keyPathElement, JSValue& result)
{
    if (object.isString() && keyPathElement == "length") {
        result = jsNumber(object.toString(exec)->length());
        return true;
    }
    if (!object.isObject())
        return false;
    Identifier identifier = Identifier::fromString(&exec->vm(), keyPathElement.utf8().data());
    if (!asObject(object)->hasProperty(exec, identifier))
        return false;
    result = asObject(object)->get(exec, identifier);
    return true;
}

static bool canSet(JSValue object, const String& keyPathElement)
{
    UNUSED_PARAM(keyPathElement);
    return object.isObject();
}

static bool set(ExecState* exec, JSValue& object, const String& keyPathElement, JSValue jsValue)
{
    if (!canSet(object, keyPathElement))
        return false;
    Identifier identifier = Identifier::fromString(&exec->vm(), keyPathElement.utf8().data());
    asObject(object)->putDirect(exec->vm(), identifier, jsValue);
    return true;
}

JSValue idbKeyDataToJSValue(JSC::ExecState& exec, const IDBKeyData& keyData)
{
    if (keyData.isNull())
        return jsUndefined();

    Locker<JSLock> locker(exec.vm().apiLock());

    switch (keyData.type()) {
    case KeyType::Array:
        {
            const Vector<IDBKeyData>& inArray = keyData.array();
            size_t size = inArray.size();
            JSArray* outArray = constructEmptyArray(&exec, 0, exec.lexicalGlobalObject(), size);
            for (size_t i = 0; i < size; ++i) {
                auto& arrayKey = inArray.at(i);
                outArray->putDirectIndex(&exec, i, idbKeyDataToJSValue(exec, arrayKey));
            }
            return JSValue(outArray);
        }
    case KeyType::String:
        return jsStringWithCache(&exec, keyData.string());
    case KeyType::Date:
        return jsDateOrNull(&exec, keyData.date());
    case KeyType::Number:
        return jsNumber(keyData.number());
    case KeyType::Min:
    case KeyType::Max:
    case KeyType::Invalid:
        ASSERT_NOT_REACHED();
        return jsUndefined();
    }

    ASSERT_NOT_REACHED();
    return jsUndefined();

}

static JSValue idbKeyToJSValue(ExecState* exec, JSGlobalObject* globalObject, IDBKey* key)
{
    if (!key || !exec) {
        // This should be undefined, not null.
        // Spec: http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBKeyRange
        return jsUndefined();
    }

    Locker<JSLock> locker(exec->vm().apiLock());

    switch (key->type()) {
    case KeyType::Array:
        {
            const Vector<RefPtr<IDBKey>>& inArray = key->array();
            size_t size = inArray.size();
            JSArray* outArray = constructEmptyArray(exec, 0, globalObject, size);
            for (size_t i = 0; i < size; ++i) {
                IDBKey* arrayKey = inArray.at(i).get();
                outArray->putDirectIndex(exec, i, idbKeyToJSValue(exec, globalObject, arrayKey));
            }
            return JSValue(outArray);
        }
    case KeyType::String:
        return jsStringWithCache(exec, key->string());
    case KeyType::Date:
        return jsDateOrNull(exec, key->date());
    case KeyType::Number:
        return jsNumber(key->number());
    case KeyType::Min:
    case KeyType::Max:
    case KeyType::Invalid:
        ASSERT_NOT_REACHED();
        return jsUndefined();
    }

    ASSERT_NOT_REACHED();
    return jsUndefined();
}

static const size_t maximumDepth = 2000;

static RefPtr<IDBKey> createIDBKeyFromValue(ExecState* exec, JSValue value, Vector<JSArray*>& stack)
{
    if (value.isNumber() && !std::isnan(value.toNumber(exec)))
        return IDBKey::createNumber(value.toNumber(exec));
    if (value.isString())
        return IDBKey::createString(value.toString(exec)->value(exec));
    if (value.inherits(DateInstance::info()) && !std::isnan(valueToDate(exec, value)))
        return IDBKey::createDate(valueToDate(exec, value));
    if (value.isObject()) {
        JSObject* object = asObject(value);
        if (isJSArray(object) || object->inherits(JSArray::info())) {
            JSArray* array = asArray(object);
            size_t length = array->length();

            if (stack.contains(array))
                return nullptr;
            if (stack.size() >= maximumDepth)
                return nullptr;
            stack.append(array);

            Vector<RefPtr<IDBKey>> subkeys;
            for (size_t i = 0; i < length; i++) {
                JSValue item = array->getIndex(exec, i);
                RefPtr<IDBKey> subkey = createIDBKeyFromValue(exec, item, stack);
                if (!subkey)
                    subkeys.append(IDBKey::createInvalid());
                else
                    subkeys.append(subkey);
            }

            stack.removeLast();
            return IDBKey::createArray(subkeys);
        }
    }
    return nullptr;
}

static RefPtr<IDBKey> createIDBKeyFromValue(ExecState* exec, JSValue value)
{
    Vector<JSArray*> stack;
    RefPtr<IDBKey> key = createIDBKeyFromValue(exec, value, stack);
    if (key)
        return key;
    return IDBKey::createInvalid();
}

IDBKeyPath idbKeyPathFromValue(ExecState* exec, JSValue keyPathValue)
{
    IDBKeyPath keyPath;
    if (isJSArray(keyPathValue))
        keyPath = IDBKeyPath(toNativeArray<String>(exec, keyPathValue));
    else
        keyPath = IDBKeyPath(keyPathValue.toString(exec)->value(exec));
    return keyPath;
}

static JSValue getNthValueOnKeyPath(ExecState* exec, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    JSValue currentValue(rootValue);
    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; i++) {
        JSValue parentValue(currentValue);
        if (!get(exec, parentValue, keyPathElements[i], currentValue))
            return jsUndefined();
    }
    return currentValue;
}

static RefPtr<IDBKey> internalCreateIDBKeyFromScriptValueAndKeyPath(ExecState* exec, const JSC::JSValue& value, const String& keyPath)
{
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    JSValue jsValue = value;
    jsValue = getNthValueOnKeyPath(exec, jsValue, keyPathElements, keyPathElements.size());
    if (jsValue.isUndefined())
        return nullptr;
    return createIDBKeyFromValue(exec, jsValue);
}

static JSValue ensureNthValueOnKeyPath(ExecState* exec, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    JSValue currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; i++) {
        JSValue parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(exec, parentValue, keyPathElement, currentValue)) {
            JSObject* object = constructEmptyObject(exec);
            if (!set(exec, parentValue, keyPathElement, JSValue(object)))
                return jsUndefined();
            currentValue = JSValue(object);
        }
    }

    return currentValue;
}

static bool canInjectNthValueOnKeyPath(ExecState* exec, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    if (!rootValue.isObject())
        return false;

    JSValue currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        JSValue parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(exec, parentValue, keyPathElement, currentValue))
            return canSet(parentValue, keyPathElement);
    }
    return true;
}

bool injectIDBKeyIntoScriptValue(DOMRequestState* requestState, PassRefPtr<IDBKey> key, Deprecated::ScriptValue& value, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "injectIDBKeyIntoScriptValue");

    ASSERT(keyPath.type() == IndexedDB::KeyPathType::String);

    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (keyPathElements.isEmpty())
        return false;

    ExecState* exec = requestState->exec();

    JSValue parent = ensureNthValueOnKeyPath(exec, value.jsValue(), keyPathElements, keyPathElements.size() - 1);
    if (parent.isUndefined())
        return false;

    if (!set(exec, parent, keyPathElements.last(), idbKeyToJSValue(exec, exec->lexicalGlobalObject(), key.get())))
        return false;

    return true;
}

bool injectIDBKeyIntoScriptValue(JSC::ExecState& exec, const IDBKeyData& keyData, JSC::JSValue value, const IDBKeyPath& keyPath)
{
    LOG(IndexedDB, "injectIDBKeyIntoScriptValue");

    ASSERT(keyPath.type() == IndexedDB::KeyPathType::String);

    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (keyPathElements.isEmpty())
        return false;

    JSValue parent = ensureNthValueOnKeyPath(&exec, value, keyPathElements, keyPathElements.size() - 1);
    if (parent.isUndefined())
        return false;

    auto key = keyData.maybeCreateIDBKey();
    if (!key)
        return false;

    if (!set(&exec, parent, keyPathElements.last(), idbKeyToJSValue(&exec, exec.lexicalGlobalObject(), key.get())))
        return false;

    return true;
}

RefPtr<IDBKey> createIDBKeyFromScriptValueAndKeyPath(ExecState* exec, const Deprecated::ScriptValue& value, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "createIDBKeyFromScriptValueAndKeyPath");
    ASSERT(!keyPath.isNull());

    if (keyPath.type() == IndexedDB::KeyPathType::Array) {
        Vector<RefPtr<IDBKey>> result;
        const Vector<String>& array = keyPath.array();
        for (size_t i = 0; i < array.size(); i++) {
            RefPtr<IDBKey> key = internalCreateIDBKeyFromScriptValueAndKeyPath(exec, value, array[i]);
            if (!key)
                return nullptr;
            result.append(key);
        }
        return IDBKey::createArray(result);
    }

    ASSERT(keyPath.type() == IndexedDB::KeyPathType::String);
    return internalCreateIDBKeyFromScriptValueAndKeyPath(exec, value, keyPath.string());
}

RefPtr<IDBKey> maybeCreateIDBKeyFromScriptValueAndKeyPath(ExecState& exec, const Deprecated::ScriptValue& value, const IDBKeyPath& keyPath)
{
    ASSERT(!keyPath.isNull());

    if (keyPath.type() == IndexedDB::KeyPathType::Array) {
        Vector<RefPtr<IDBKey>> result;
        const Vector<String>& array = keyPath.array();
        for (size_t i = 0; i < array.size(); i++) {
            RefPtr<IDBKey> key = internalCreateIDBKeyFromScriptValueAndKeyPath(&exec, value, array[i]);
            if (!key)
                return nullptr;
            result.append(key);
        }
        return IDBKey::createArray(result);
    }

    ASSERT(keyPath.type() == IndexedDB::KeyPathType::String);
    return internalCreateIDBKeyFromScriptValueAndKeyPath(&exec, value, keyPath.string());
}

RefPtr<IDBKey> maybeCreateIDBKeyFromScriptValueAndKeyPath(ExecState& exec, const JSC::JSValue& value, const IDBKeyPath& keyPath)
{
    ASSERT(!keyPath.isNull());

    if (keyPath.type() == IndexedDB::KeyPathType::Array) {
        const Vector<String>& array = keyPath.array();
        Vector<RefPtr<IDBKey>> result;
        result.reserveInitialCapacity(array.size());
        for (auto& string : array) {
            RefPtr<IDBKey> key = internalCreateIDBKeyFromScriptValueAndKeyPath(&exec, value, string);
            if (!key)
                return nullptr;
            result.uncheckedAppend(WTF::move(key));
        }
        return IDBKey::createArray(WTF::move(result));
    }

    ASSERT(keyPath.type() == IndexedDB::KeyPathType::String);
    return internalCreateIDBKeyFromScriptValueAndKeyPath(&exec, value, keyPath.string());
}

bool canInjectIDBKeyIntoScriptValue(DOMRequestState* requestState, const JSC::JSValue& scriptValue, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "canInjectIDBKeyIntoScriptValue");

    JSC::ExecState* exec = requestState->exec();
    if (!exec)
        return false;

    return canInjectIDBKeyIntoScriptValue(*exec, scriptValue, keyPath);
}

bool canInjectIDBKeyIntoScriptValue(JSC::ExecState& execState, const JSC::JSValue& scriptValue, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "canInjectIDBKeyIntoScriptValue");

    ASSERT(keyPath.type() == IndexedDB::KeyPathType::String);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (!keyPathElements.size())
        return false;

    return canInjectNthValueOnKeyPath(&execState, scriptValue, keyPathElements, keyPathElements.size() - 1);
}

Deprecated::ScriptValue deserializeIDBValue(DOMRequestState* requestState, PassRefPtr<SerializedScriptValue> prpValue)
{
    ExecState* exec = requestState->exec();
    RefPtr<SerializedScriptValue> serializedValue = prpValue;
    JSValue result;
    if (serializedValue)
        result = serializedValue->deserialize(exec, exec->lexicalGlobalObject(), 0);
    else
        result = jsNull();
    return Deprecated::ScriptValue(exec->vm(), result);
}

Deprecated::ScriptValue deserializeIDBValueData(ScriptExecutionContext& context, const ThreadSafeDataBuffer& valueData)
{
    DOMRequestState state(&context);
    auto* execState = state.exec();

    if (!execState)
        return Deprecated::ScriptValue();

    return Deprecated::ScriptValue(execState->vm(), deserializeIDBValueDataToJSValue(*execState, valueData));
}

JSC::JSValue deserializeIDBValueDataToJSValue(JSC::ExecState& exec, const ThreadSafeDataBuffer& valueData)
{
    if (!valueData.data())
        return jsUndefined();

    const Vector<uint8_t>& data = *valueData.data();
    JSValue result;
    if (data.size()) {
        RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::createFromWireBytes(data);

        exec.vm().apiLock().lock();
        result = serializedValue->deserialize(&exec, exec.lexicalGlobalObject(), 0, NonThrowing);
        exec.vm().apiLock().unlock();
    } else
        result = jsNull();

    return result;
}

Deprecated::ScriptValue deserializeIDBValueBuffer(DOMRequestState* requestState, PassRefPtr<SharedBuffer> prpBuffer, bool keyIsDefined)
{
    if (prpBuffer) {
        Vector<uint8_t> value;
        value.append(prpBuffer->data(), prpBuffer->size());
        return deserializeIDBValueBuffer(requestState->exec(), value, keyIsDefined);
    }

    return Deprecated::ScriptValue(requestState->exec()->vm(), jsNull());
}

static JSValue idbValueDataToJSValue(JSC::ExecState& exec, const Vector<uint8_t>& buffer)
{
    if (buffer.isEmpty())
        return jsNull();

    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::createFromWireBytes(buffer);
    return serializedValue->deserialize(&exec, exec.lexicalGlobalObject(), 0, NonThrowing);
}

Deprecated::ScriptValue deserializeIDBValueBuffer(JSC::ExecState* exec, const Vector<uint8_t>& buffer, bool keyIsDefined)
{
    ASSERT(exec);

    // If the key doesn't exist, then the value must be undefined (as opposed to null).
    if (!keyIsDefined) {
        // We either shouldn't have a buffer or it should be of size 0.
        ASSERT(!buffer.size());
        return Deprecated::ScriptValue(exec->vm(), jsUndefined());
    }

    JSValue result = idbValueDataToJSValue(*exec, buffer);
    return Deprecated::ScriptValue(exec->vm(), result);
}

JSValue idbValueDataToJSValue(JSC::ExecState& exec, const ThreadSafeDataBuffer& valueData)
{
    if (!valueData.data())
        return jsUndefined();

    return idbValueDataToJSValue(exec, *valueData.data());
}

Deprecated::ScriptValue idbKeyToScriptValue(DOMRequestState* requestState, PassRefPtr<IDBKey> key)
{
    ExecState* exec = requestState->exec();
    if (!exec)
        return { };

    return Deprecated::ScriptValue(exec->vm(), idbKeyToJSValue(exec, jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()), key.get()));
}

RefPtr<IDBKey> scriptValueToIDBKey(DOMRequestState* requestState, const JSC::JSValue& scriptValue)
{
    ExecState* exec = requestState->exec();
    return createIDBKeyFromValue(exec, scriptValue);
}

RefPtr<IDBKey> scriptValueToIDBKey(ExecState& exec, const JSC::JSValue& scriptValue)
{
    return createIDBKeyFromValue(&exec, scriptValue);
}

Deprecated::ScriptValue idbKeyDataToScriptValue(ScriptExecutionContext* context, const IDBKeyData& keyData)
{
    RefPtr<IDBKey> key = keyData.maybeCreateIDBKey();
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, key.get());
}

void generateIndexKeysForValue(ExecState* exec, const IDBIndexMetadata& indexMetadata, const Deprecated::ScriptValue& objectValue, Vector<IDBKeyData>& indexKeys)
{
    RefPtr<IDBKey> indexKey = createIDBKeyFromScriptValueAndKeyPath(exec, objectValue, indexMetadata.keyPath);

    if (!indexKey)
        return;

    if (!indexMetadata.multiEntry || indexKey->type() != KeyType::Array) {
        if (!indexKey->isValid())
            return;

        indexKeys.append(IDBKeyData(indexKey.get()));
    } else {
        ASSERT(indexMetadata.multiEntry);
        ASSERT(indexKey->type() == KeyType::Array);
        indexKey = IDBKey::createMultiEntryArray(indexKey->array());

        if (!indexKey->isValid())
            return;

        for (auto& i : indexKey->array())
            indexKeys.append(IDBKeyData(i.get()));
    }
}

static Vector<IDBKeyData> createKeyPathArray(ExecState& exec, JSValue value, const IDBIndexInfo& info)
{
    Vector<IDBKeyData> keys;

    switch (info.keyPath().type()) {
    case IndexedDB::KeyPathType::Array:
        for (auto& entry : info.keyPath().array()) {
            auto key = internalCreateIDBKeyFromScriptValueAndKeyPath(&exec, value, entry);
            if (!key)
                return { };
            keys.append(key.get());
        }
        break;
    case IndexedDB::KeyPathType::String: {
        auto idbKey = internalCreateIDBKeyFromScriptValueAndKeyPath(&exec, value, info.keyPath().string());
        if (!idbKey)
            return { };

        if (info.multiEntry() && idbKey->type() == IndexedDB::Array) {
            for (auto& key : idbKey->array())
                keys.append(key.get());
        } else
            keys.append(idbKey.get());

        break;
    }
    case IndexedDB::KeyPathType::Null:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return keys;
}

void generateIndexKeyForValue(ExecState& exec, const IDBIndexInfo& info, JSValue value, IndexKey& outKey)
{
    auto keyDatas = createKeyPathArray(exec, value, info);

    if (keyDatas.isEmpty())
        return;

    outKey = IndexKey(WTF::move(keyDatas));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
