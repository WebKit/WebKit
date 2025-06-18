/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "IDBKey.h"

#include "IDBKeyData.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(IDBKey);

using IDBKeyVector = Vector<RefPtr<IDBKey>>;

Ref<IDBKey> IDBKey::createBinary(const ThreadSafeDataBuffer& buffer)
{
    return adoptRef(*new IDBKey(buffer));
}

Ref<IDBKey> IDBKey::createBinary(JSC::JSArrayBuffer& arrayBuffer)
{
    RefPtr buffer = arrayBuffer.impl();
    if (buffer && buffer->isDetached())
        return createInvalid();
    return adoptRef(*new IDBKey(ThreadSafeDataBuffer::copyData(buffer->span())));
}

Ref<IDBKey> IDBKey::createBinary(JSC::JSArrayBufferView& arrayBufferView)
{
    if (arrayBufferView.isDetached())
        return createInvalid();
    auto bufferView = arrayBufferView.possiblySharedImpl();
    if (!bufferView)
        return createInvalid();
    return adoptRef(*new IDBKey(ThreadSafeDataBuffer::copyData(bufferView->span())));
}

IDBKey::IDBKey(IndexedDB::KeyType type, double number)
    : m_type(type)
    , m_value(number)
    , m_sizeEstimate(OverheadSize + sizeof(double))
{
}

IDBKey::IDBKey(const String& value)
    : m_type(IndexedDB::KeyType::String)
    , m_value(value)
    , m_sizeEstimate(OverheadSize + value.length() * sizeof(char16_t))
{
}

IDBKey::IDBKey(const IDBKeyVector& keyArray, size_t arraySize)
    : m_type(IndexedDB::KeyType::Array)
    , m_value(keyArray)
    , m_sizeEstimate(OverheadSize + arraySize)
{
}

IDBKey::IDBKey(const ThreadSafeDataBuffer& buffer)
    : m_type(IndexedDB::KeyType::Binary)
    , m_value(buffer)
    , m_sizeEstimate(OverheadSize + buffer.size())
{
}

IDBKey::~IDBKey() = default;

bool IDBKey::isValid() const
{
    if (m_type == IndexedDB::KeyType::Invalid)
        return false;

    if (m_type == IndexedDB::KeyType::Array) {
        for (auto& key : std::get<IDBKeyVector>(m_value)) {
            if (!key->isValid())
                return false;
        }
    }

    return true;
}

std::weak_ordering IDBKey::compare(const IDBKey& other) const
{
    if (m_type != other.m_type)
        return other.m_type <=> m_type;

    switch (m_type) {
    case IndexedDB::KeyType::Array: {
        auto& array = std::get<IDBKeyVector>(m_value);
        auto& otherArray = std::get<IDBKeyVector>(other.m_value);
        for (size_t i = 0; i < array.size() && i < otherArray.size(); ++i) {
            if (auto result = array[i]->compare(*otherArray[i]); is_neq(result))
                return result;
        }
        return array.size() <=> otherArray.size();
    }
    case IndexedDB::KeyType::Binary:
        return compareBinaryKeyData(std::get<ThreadSafeDataBuffer>(m_value), std::get<ThreadSafeDataBuffer>(other.m_value));
    case IndexedDB::KeyType::String:
        return codePointCompare(std::get<String>(m_value), std::get<String>(other.m_value));
    case IndexedDB::KeyType::Date:
    case IndexedDB::KeyType::Number:
        return weakOrderingCast(std::get<double>(m_value) <=> std::get<double>(other.m_value));
    case IndexedDB::KeyType::Invalid:
    case IndexedDB::KeyType::Min:
    case IndexedDB::KeyType::Max:
        ASSERT_NOT_REACHED();
        return std::weak_ordering::equivalent;
    }

    ASSERT_NOT_REACHED();
    return std::weak_ordering::equivalent;
}

bool IDBKey::isLessThan(const IDBKey& other) const
{
    return is_lt(compare(other));
}

bool IDBKey::isEqual(const IDBKey& other) const
{
    return is_eq(compare(other));
}

#if !LOG_DISABLED
String IDBKey::loggingString() const
{
    return IDBKeyData(this).loggingString();
}
#endif

} // namespace WebCore
