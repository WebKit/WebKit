/*
* Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "IDBValue.h"
#include "ScriptWrappable.h"
#include <optional>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class IDBRecord : public ScriptWrappable, public RefCounted<IDBRecord> {
    WTF_MAKE_TZONE_ALLOCATED(IDBRecord);
public:
    static Ref<IDBRecord> create(IDBKeyData&& key, IDBKeyData&& primaryKey, IDBValue&& value, const std::optional<IDBKeyPath>& keyPath)
    {
        return adoptRef(*new IDBRecord(WTF::move(key), WTF::move(primaryKey), WTF::move(value), keyPath));
    }

    const IDBKeyData& key() const { return m_key; }
    const IDBKeyData& primaryKey() const { return m_primaryKey; }
    const IDBValue& value() const { return m_value; }
    const std::optional<IDBKeyPath>& keyPath() const { return m_keyPath; }

private:
    IDBRecord(IDBKeyData&& key, IDBKeyData&& primaryKey, IDBValue&& value, const std::optional<IDBKeyPath>& keyPath)
        : m_key(WTF::move(key))
        , m_primaryKey(WTF::move(primaryKey))
        , m_value(WTF::move(value))
        , m_keyPath(keyPath)
    {
    }

    IDBKeyData m_key;
    IDBKeyData m_primaryKey;
    IDBValue m_value;
    std::optional<IDBKeyPath> m_keyPath;
};

} // namespace WebCore
