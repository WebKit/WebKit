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

#if ENABLE(INDEXED_DATABASE)

#include "IDBObjectStoreInfo.h"
#include <wtf/HashMap.h>

namespace WebCore {

class IDBDatabaseInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBDatabaseInfo(const String& name, uint64_t version);

    enum IsolatedCopyTag { IsolatedCopy };
    IDBDatabaseInfo(const IDBDatabaseInfo&, IsolatedCopyTag);

    IDBDatabaseInfo isolatedCopy() const;

    const String& name() const { return m_name; }

    void setVersion(uint64_t version) { m_version = version; }
    uint64_t version() const { return m_version; }

    bool hasObjectStore(const String& name) const;
    IDBObjectStoreInfo createNewObjectStore(const String& name, Optional<IDBKeyPath>&&, bool autoIncrement);
    void addExistingObjectStore(const IDBObjectStoreInfo&);
    IDBObjectStoreInfo* infoForExistingObjectStore(uint64_t objectStoreIdentifier);
    IDBObjectStoreInfo* infoForExistingObjectStore(const String& objectStoreName);
    const IDBObjectStoreInfo* infoForExistingObjectStore(uint64_t objectStoreIdentifier) const;
    const IDBObjectStoreInfo* infoForExistingObjectStore(const String& objectStoreName) const;

    void renameObjectStore(uint64_t objectStoreIdentifier, const String& newName);

    Vector<String> objectStoreNames() const;

    void deleteObjectStore(const String& objectStoreName);
    void deleteObjectStore(uint64_t objectStoreIdentifier);

    WEBCORE_EXPORT IDBDatabaseInfo();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBDatabaseInfo&);

#if !LOG_DISABLED
    String loggingString() const;
#endif

private:
    IDBObjectStoreInfo* getInfoForExistingObjectStore(const String& objectStoreName);
    IDBObjectStoreInfo* getInfoForExistingObjectStore(uint64_t objectStoreIdentifier);

    String m_name;
    uint64_t m_version { 0 };
    uint64_t m_maxObjectStoreID { 0 };

    HashMap<uint64_t, IDBObjectStoreInfo> m_objectStoreMap;

};

template<class Encoder>
void IDBDatabaseInfo::encode(Encoder& encoder) const
{
    encoder << m_name << m_version << m_maxObjectStoreID << m_objectStoreMap;
}

template<class Decoder>
bool IDBDatabaseInfo::decode(Decoder& decoder, IDBDatabaseInfo& info)
{
    if (!decoder.decode(info.m_name))
        return false;

    if (!decoder.decode(info.m_version))
        return false;

    if (!decoder.decode(info.m_maxObjectStoreID))
        return false;

    if (!decoder.decode(info.m_objectStoreMap))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
