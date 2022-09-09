/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "IDBIndexInfo.h"
#include "IDBKeyPath.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBObjectStoreInfo {
public:
    WEBCORE_EXPORT IDBObjectStoreInfo();
    WEBCORE_EXPORT IDBObjectStoreInfo(uint64_t identifier, const String& name, std::optional<IDBKeyPath>&&, bool autoIncrement, HashMap<uint64_t, IDBIndexInfo>&& = { });

    uint64_t identifier() const { return m_identifier; }
    const String& name() const { return m_name; }
    const std::optional<IDBKeyPath>& keyPath() const { return m_keyPath; }
    bool autoIncrement() const { return m_autoIncrement; }

    void rename(const String& newName) { m_name = newName; }

    WEBCORE_EXPORT IDBObjectStoreInfo isolatedCopy() const &;
    WEBCORE_EXPORT IDBObjectStoreInfo isolatedCopy() &&;

    IDBIndexInfo createNewIndex(uint64_t indexID, const String& name, IDBKeyPath&&, bool unique, bool multiEntry);
    void addExistingIndex(const IDBIndexInfo&);
    bool hasIndex(const String& name) const;
    bool hasIndex(uint64_t indexIdentifier) const;
    IDBIndexInfo* infoForExistingIndex(const String& name);
    IDBIndexInfo* infoForExistingIndex(uint64_t identifier);

    Vector<String> indexNames() const;
    const HashMap<uint64_t, IDBIndexInfo>& indexMap() const { return m_indexMap; }

    void deleteIndex(const String& indexName);
    void deleteIndex(uint64_t indexIdentifier);

#if !LOG_DISABLED
    String loggingString(int indent = 0) const;
    String condensedLoggingString() const;
#endif

private:
    uint64_t m_identifier { 0 };
    String m_name;
    std::optional<IDBKeyPath> m_keyPath;
    bool m_autoIncrement { false };

    HashMap<uint64_t, IDBIndexInfo> m_indexMap;
};

} // namespace WebCore
