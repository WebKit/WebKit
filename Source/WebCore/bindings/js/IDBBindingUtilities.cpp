/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Michael Pruett <michael@68k.org>
 * Copyright (C) 2014, 2015, 2016 Apple Inc. All rights reserved.
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

#include "IDBIndexInfo.h"
#include "IDBKey.h"
#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "IDBValue.h"
#include "IndexKey.h"
#include "JSDOMBinding.h"
#include "JSDOMStringList.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "SharedBuffer.h"
#include "ThreadSafeDataBuffer.h"
#include <runtime/DateInstance.h>
#include <runtime/ObjectConstructor.h>

using namespace JSC;

namespace WebCore {

static bool get(ExecState& exec, JSValue object, const String& keyPathElement, JSValue& result)
{
    if (object.isString() && keyPathElement == "length") {
        result = jsNumber(object.toString(&exec)->length());
        return true;
    }
    if (!object.isObject())
        return false;
    Identifier identifier = Identifier::fromString(&exec.vm(), keyPathElement);
    if (!asObject(object)->hasProperty(&exec, identifier))
        return false;
    result = asObject(object)->get(&exec, identifier);
    return true;
}

static bool canSet(JSValue object, const String& keyPathElement)
{
    UNUSED_PARAM(keyPathElement);
    return object.isObject();
}

static bool set(ExecState& exec, JSValue& object, const String& keyPathElement, JSValue jsValue)
{
    if (!canSet(object, keyPathElement))
        return false;
    Identifier identifier = Identifier::fromString(&exec.vm(), keyPathElement);
    asObject(object)->putDirect(exec.vm(), identifier, jsValue);
    return true;
}

JSValue toJS(ExecState& state, JSGlobalObject& globalObject, IDBKey* key)
{
    if (!key) {
        // This must be undefined, not null.
        // Spec: http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBKeyRange
        return jsUndefined();
    }

    VM& vm = state.vm();
    Locker<JSLock> locker(vm.apiLock());

    switch (key->type()) {
    case KeyType::Array: {
        auto& inArray = key->array();
        unsigned size = inArray.size();
        auto outArray = constructEmptyArray(&state, 0, &globalObject, size);
        if (UNLIKELY(vm.exception()))
            return jsUndefined();
        for (size_t i = 0; i < size; ++i)
            outArray->putDirectIndex(&state, i, toJS(state, globalObject, inArray.at(i).get()));
        return outArray;
    }
    case KeyType::String:
        return jsStringWithCache(&state, key->string());
    case KeyType::Date:
        return jsDateOrNull(&state, key->date());
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

static RefPtr<IDBKey> createIDBKeyFromValue(ExecState& exec, JSValue value, Vector<JSArray*>& stack)
{
    if (value.isNumber() && !std::isnan(value.toNumber(&exec)))
        return IDBKey::createNumber(value.toNumber(&exec));

    if (value.isString())
        return IDBKey::createString(value.toString(&exec)->value(&exec));

    if (value.inherits(DateInstance::info()) && !std::isnan(valueToDate(&exec, value)))
        return IDBKey::createDate(valueToDate(&exec, value));

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
                JSValue item = array->getIndex(&exec, i);
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

static Ref<IDBKey> createIDBKeyFromValue(ExecState& exec, JSValue value)
{
    Vector<JSArray*> stack;
    RefPtr<IDBKey> key = createIDBKeyFromValue(exec, value, stack);
    if (key)
        return *key;
    return IDBKey::createInvalid();
}

IDBKeyPath idbKeyPathFromValue(ExecState& exec, JSValue keyPathValue)
{
    IDBKeyPath keyPath;
    if (isJSArray(keyPathValue))
        keyPath = IDBKeyPath(toNativeArray<String>(exec, keyPathValue));
    else
        keyPath = IDBKeyPath(keyPathValue.toWTFString(&exec));
    return keyPath;
}

static JSValue getNthValueOnKeyPath(ExecState& exec, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
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

static RefPtr<IDBKey> internalCreateIDBKeyFromScriptValueAndKeyPath(ExecState& exec, const JSValue& value, const String& keyPath)
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

static JSValue ensureNthValueOnKeyPath(ExecState& exec, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
{
    JSValue currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; i++) {
        JSValue parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(exec, parentValue, keyPathElement, currentValue)) {
            JSObject* object = constructEmptyObject(&exec);
            if (!set(exec, parentValue, keyPathElement, JSValue(object)))
                return jsUndefined();
            currentValue = JSValue(object);
        }
    }

    return currentValue;
}

static bool canInjectNthValueOnKeyPath(ExecState& exec, JSValue rootValue, const Vector<String>& keyPathElements, size_t index)
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

bool injectIDBKeyIntoScriptValue(ExecState& exec, const IDBKeyData& keyData, JSValue value, const IDBKeyPath& keyPath)
{
    LOG(IndexedDB, "injectIDBKeyIntoScriptValue");

    ASSERT(keyPath.type() == IDBKeyPath::Type::String);

    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (keyPathElements.isEmpty())
        return false;

    JSValue parent = ensureNthValueOnKeyPath(exec, value, keyPathElements, keyPathElements.size() - 1);
    if (parent.isUndefined())
        return false;

    auto key = keyData.maybeCreateIDBKey();
    if (!key)
        return false;

    if (!set(exec, parent, keyPathElements.last(), toJS(exec, *exec.lexicalGlobalObject(), key.get())))
        return false;

    return true;
}


RefPtr<IDBKey> maybeCreateIDBKeyFromScriptValueAndKeyPath(ExecState& exec, const JSValue& value, const IDBKeyPath& keyPath)
{
    ASSERT(!keyPath.isNull());

    if (keyPath.type() == IDBKeyPath::Type::Array) {
        const Vector<String>& array = keyPath.array();
        Vector<RefPtr<IDBKey>> result;
        result.reserveInitialCapacity(array.size());
        for (auto& string : array) {
            RefPtr<IDBKey> key = internalCreateIDBKeyFromScriptValueAndKeyPath(exec, value, string);
            if (!key)
                return nullptr;
            result.uncheckedAppend(WTFMove(key));
        }
        return IDBKey::createArray(WTFMove(result));
    }

    ASSERT(keyPath.type() == IDBKeyPath::Type::String);
    return internalCreateIDBKeyFromScriptValueAndKeyPath(exec, value, keyPath.string());
}

bool canInjectIDBKeyIntoScriptValue(ExecState& exec, const JSValue& scriptValue, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "canInjectIDBKeyIntoScriptValue");

    ASSERT(keyPath.type() == IDBKeyPath::Type::String);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseError::None);

    if (!keyPathElements.size())
        return false;

    return canInjectNthValueOnKeyPath(exec, scriptValue, keyPathElements, keyPathElements.size() - 1);
}

JSValue deserializeIDBValueToJSValue(ExecState& exec, const IDBValue& value)
{
    // FIXME: I think it's peculiar to use undefined to mean "null data" and null to mean "empty data".
    // But I am not changing this at the moment because at least some callers are specifically checking isUndefined.

    if (!value.data().data())
        return jsUndefined();

    auto& data = *value.data().data();
    if (data.isEmpty())
        return jsNull();

    auto serializedValue = SerializedScriptValue::createFromWireBytes(Vector<uint8_t>(data));

    exec.vm().apiLock().lock();
    JSValue result = serializedValue->deserialize(&exec, exec.lexicalGlobalObject(), 0, NonThrowing, value.blobURLs(), value.blobFilePaths());
    exec.vm().apiLock().unlock();

    return result;
}

Ref<IDBKey> scriptValueToIDBKey(ExecState& exec, const JSValue& scriptValue)
{
    return createIDBKeyFromValue(exec, scriptValue);
}

JSC::JSValue idbKeyDataToScriptValue(JSC::ExecState& exec, const IDBKeyData& keyData)
{
    RefPtr<IDBKey> key = keyData.maybeCreateIDBKey();
    return toJS(exec, *exec.lexicalGlobalObject(), key.get());
}

static Vector<IDBKeyData> createKeyPathArray(ExecState& exec, JSValue value, const IDBIndexInfo& info)
{
    Vector<IDBKeyData> keys;

    switch (info.keyPath().type()) {
    case IDBKeyPath::Type::Array:
        for (auto& entry : info.keyPath().array()) {
            auto key = internalCreateIDBKeyFromScriptValueAndKeyPath(exec, value, entry);
            if (!key)
                return { };
            keys.append(key.get());
        }
        break;
    case IDBKeyPath::Type::String: {
        auto idbKey = internalCreateIDBKeyFromScriptValueAndKeyPath(exec, value, info.keyPath().string());
        if (!idbKey)
            return { };

        if (info.multiEntry() && idbKey->type() == IndexedDB::Array) {
            for (auto& key : idbKey->array())
                keys.append(key.get());
        } else
            keys.append(idbKey.get());

        break;
    }
    case IDBKeyPath::Type::Null:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return keys;
}

void generateIndexKeyForValue(ExecState& exec, const IDBIndexInfo& info, JSValue value, IndexKey& outKey)
{
    auto keyDatas = createKeyPathArray(exec, value, info);

    if (keyDatas.isEmpty())
        return;

    outKey = IndexKey(WTFMove(keyDatas));
}

JSValue toJS(ExecState& state, JSDOMGlobalObject& globalObject, const IDBKeyPath& value)
{
    switch (value.type()) {
    case IDBKeyPath::Type::Null:
        return jsNull();
    case IDBKeyPath::Type::String:
        return jsStringWithCache(&state, value.string());
    case IDBKeyPath::Type::Array:
        auto keyPaths = DOMStringList::create();
        for (auto& path : value.array())
            keyPaths->append(path);
        return toJS(&state, &globalObject, keyPaths);
    }

    ASSERT_NOT_REACHED();
    return jsNull();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
