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

#ifndef IDBObjectStoreInfo_h
#define IDBObjectStoreInfo_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBIndexInfo.h"
#include "IDBKeyPath.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBKeyPath;

class IDBObjectStoreInfo {
public:
    IDBObjectStoreInfo();
    IDBObjectStoreInfo(uint64_t identifier, const String& name, const IDBKeyPath&, bool autoIncrement);

    uint64_t identifier() const { return m_identifier; }
    const String& name() const { return m_name; }
    const IDBKeyPath& keyPath() const { return m_keyPath; }
    bool autoIncrement() const { return m_autoIncrement; }
    uint64_t maxIndexID() const { return m_maxIndexID; }

    IDBObjectStoreInfo isolatedCopy() const;

    IDBIndexInfo createNewIndex(const String& name, const IDBKeyPath&, bool unique, bool multiEntry);
    void addExistingIndex(const IDBIndexInfo&);
    bool hasIndex(const String& name) const;
    bool hasIndex(uint64_t indexIdentifier) const;
    IDBIndexInfo* infoForExistingIndex(const String& name);

    Vector<String> indexNames() const;

    void deleteIndex(const String& indexName);

#ifndef NDEBUG
    String loggingString(int indent = 0) const;
#endif

private:
    uint64_t m_identifier { 0 };
    String m_name;
    IDBKeyPath m_keyPath;
    bool m_autoIncrement { false };
    uint64_t m_maxIndexID { 0 };

    HashMap<uint64_t, IDBIndexInfo> m_indexMap;

};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBObjectStoreInfo_h
