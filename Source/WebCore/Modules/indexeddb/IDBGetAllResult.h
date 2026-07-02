/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyPath.h>
#include <WebCore/IDBValue.h>
#include <WebCore/IndexedDB.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class IDBGetAllResult {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(IDBGetAllResult, WEBCORE_EXPORT);
public:
    IDBGetAllResult() = default;

    IDBGetAllResult(IndexedDB::GetAllType type, const std::optional<IDBKeyPath>& keyPath)
        : m_type(type)
        , m_keyPath(keyPath)
    {
    }

    enum IsolatedCopyTag { IsolatedCopy };
    IDBGetAllResult(const IDBGetAllResult&, IsolatedCopyTag);
    IDBGetAllResult isolatedCopy() const;

    IndexedDB::GetAllType type() const { return m_type; }
    const std::optional<IDBKeyPath>& keyPath() const LIFETIME_BOUND { return m_keyPath; }
    WEBCORE_EXPORT const Vector<IDBKeyData>& NODELETE keys() const;
    WEBCORE_EXPORT const Vector<IDBKeyData>& NODELETE primaryKeys() const;
    WEBCORE_EXPORT const Vector<IDBValue>& NODELETE values() const;

    void addKey(IDBKeyData&&);
    void addPrimaryKey(IDBKeyData&&);
    void addValue(IDBValue&&);

private:
    friend struct IPC::ArgumentCoder<IDBGetAllResult>;
    IDBGetAllResult(IndexedDB::GetAllType type, Vector<IDBKeyData>&& keys, Vector<IDBKeyData>&& primaryKeys, Vector<IDBValue>&& values, std::optional<IDBKeyPath>&& keyPath)
        : m_type(type)
        , m_keys(WTF::move(keys))
        , m_primaryKeys(WTF::move(primaryKeys))
        , m_values(WTF::move(values))
        , m_keyPath(WTF::move(keyPath))
    {
    }

    static void isolatedCopy(const IDBGetAllResult& source, IDBGetAllResult& destination);

    IndexedDB::GetAllType m_type { IndexedDB::GetAllType::Keys };
    Vector<IDBKeyData> m_keys;
    Vector<IDBKeyData> m_primaryKeys;
    Vector<IDBValue> m_values;
    std::optional<IDBKeyPath> m_keyPath;
};

} // namespace WebCore
