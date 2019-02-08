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

#include "IDBDatabaseInfo.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransactionMode.h"
#include "IndexedDB.h"
#include <wtf/Vector.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionProxy;
}

namespace IDBServer {
class IDBConnectionToClient;
}

class IDBTransactionInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static IDBTransactionInfo clientTransaction(const IDBClient::IDBConnectionProxy&, const Vector<String>& objectStores, IDBTransactionMode);
    static IDBTransactionInfo versionChange(const IDBServer::IDBConnectionToClient&, const IDBDatabaseInfo& originalDatabaseInfo, uint64_t newVersion);

    IDBTransactionInfo(const IDBTransactionInfo&);

    enum IsolatedCopyTag { IsolatedCopy };
    IDBTransactionInfo(const IDBTransactionInfo&, IsolatedCopyTag);

    IDBTransactionInfo isolatedCopy() const;

    const IDBResourceIdentifier& identifier() const { return m_identifier; }

    IDBTransactionMode mode() const { return m_mode; }
    uint64_t newVersion() const { return m_newVersion; }

    const Vector<String>& objectStores() const { return m_objectStores; }

    IDBDatabaseInfo* originalDatabaseInfo() const { return m_originalDatabaseInfo.get(); }

    WEBCORE_EXPORT IDBTransactionInfo();
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBTransactionInfo&);

#if !LOG_DISABLED
    String loggingString() const;
#endif

private:
    IDBTransactionInfo(const IDBResourceIdentifier&);

    static void isolatedCopy(const IDBTransactionInfo& source, IDBTransactionInfo& destination);

    IDBResourceIdentifier m_identifier;

    IDBTransactionMode m_mode { IDBTransactionMode::Readonly };
    uint64_t m_newVersion { 0 };
    Vector<String> m_objectStores;
    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfo;
};

template<class Encoder>
void IDBTransactionInfo::encode(Encoder& encoder) const
{
    encoder << m_identifier << m_newVersion << m_objectStores;
    encoder.encodeEnum(m_mode);

    encoder << !!m_originalDatabaseInfo;
    if (m_originalDatabaseInfo)
        encoder << *m_originalDatabaseInfo;
}

template<class Decoder>
bool IDBTransactionInfo::decode(Decoder& decoder, IDBTransactionInfo& info)
{
    if (!decoder.decode(info.m_identifier))
        return false;

    if (!decoder.decode(info.m_newVersion))
        return false;

    if (!decoder.decode(info.m_objectStores))
        return false;

    if (!decoder.decodeEnum(info.m_mode))
        return false;

    bool hasObject;
    if (!decoder.decode(hasObject))
        return false;

    if (hasObject) {
        std::unique_ptr<IDBDatabaseInfo> object = std::make_unique<IDBDatabaseInfo>();
        if (!decoder.decode(*object))
            return false;
        info.m_originalDatabaseInfo = WTFMove(object);
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
