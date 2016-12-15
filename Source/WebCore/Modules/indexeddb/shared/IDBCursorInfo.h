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

#include "IDBKeyRangeData.h"
#include "IDBResourceIdentifier.h"

namespace WebCore {

class IDBTransaction;

namespace IndexedDB {
enum class CursorDirection;
enum class CursorSource;
enum class CursorType;
}

struct IDBKeyRangeData;

enum class CursorDuplicity {
    Duplicates,
    NoDuplicates,
};

class IDBCursorInfo {
public:
    static IDBCursorInfo objectStoreCursor(IDBTransaction&, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IndexedDB::CursorDirection, IndexedDB::CursorType);
    static IDBCursorInfo indexCursor(IDBTransaction&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, IndexedDB::CursorDirection, IndexedDB::CursorType);

    IDBResourceIdentifier identifier() const { return m_cursorIdentifier; }
    uint64_t sourceIdentifier() const { return m_sourceIdentifier; }
    uint64_t objectStoreIdentifier() const { return m_objectStoreIdentifier; }

    IndexedDB::CursorSource cursorSource() const { return m_source; }
    IndexedDB::CursorDirection cursorDirection() const { return m_direction; }
    IndexedDB::CursorType cursorType() const { return m_type; }
    const IDBKeyRangeData& range() const { return m_range; }

    bool isDirectionForward() const;
    CursorDuplicity duplicity() const;

    IDBCursorInfo isolatedCopy() const;

    WEBCORE_EXPORT IDBCursorInfo();
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBCursorInfo&);

#if !LOG_DISABLED
    String loggingString() const;
#endif

private:
    IDBCursorInfo(IDBTransaction&, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IndexedDB::CursorDirection, IndexedDB::CursorType);
    IDBCursorInfo(IDBTransaction&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, IndexedDB::CursorDirection, IndexedDB::CursorType);

    IDBCursorInfo(const IDBResourceIdentifier&, const IDBResourceIdentifier&, uint64_t, uint64_t, const IDBKeyRangeData&, IndexedDB::CursorSource, IndexedDB::CursorDirection, IndexedDB::CursorType);

    IDBResourceIdentifier m_cursorIdentifier;
    IDBResourceIdentifier m_transactionIdentifier;
    uint64_t m_objectStoreIdentifier { 0 };
    uint64_t m_sourceIdentifier { 0 };

    IDBKeyRangeData m_range;

    IndexedDB::CursorSource m_source;
    IndexedDB::CursorDirection m_direction;
    IndexedDB::CursorType m_type;
};

template<class Encoder>
void IDBCursorInfo::encode(Encoder& encoder) const
{
    encoder << m_cursorIdentifier << m_transactionIdentifier << m_objectStoreIdentifier << m_sourceIdentifier << m_range;

    encoder.encodeEnum(m_source);
    encoder.encodeEnum(m_direction);
    encoder.encodeEnum(m_type);
}

template<class Decoder>
bool IDBCursorInfo::decode(Decoder& decoder, IDBCursorInfo& info)
{
    if (!decoder.decode(info.m_cursorIdentifier))
        return false;

    if (!decoder.decode(info.m_transactionIdentifier))
        return false;

    if (!decoder.decode(info.m_objectStoreIdentifier))
        return false;

    if (!decoder.decode(info.m_sourceIdentifier))
        return false;

    if (!decoder.decode(info.m_range))
        return false;

    if (!decoder.decodeEnum(info.m_source))
        return false;

    if (!decoder.decodeEnum(info.m_direction))
        return false;

    if (!decoder.decodeEnum(info.m_type))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
