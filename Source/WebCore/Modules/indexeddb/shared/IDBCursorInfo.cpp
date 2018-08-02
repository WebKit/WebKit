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

#include "config.h"
#include "IDBCursorInfo.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabase.h"
#include "IDBTransaction.h"
#include "IndexedDB.h"

namespace WebCore {

IDBCursorInfo IDBCursorInfo::objectStoreCursor(IDBTransaction& transaction, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range, IndexedDB::CursorDirection direction, IndexedDB::CursorType type)
{
    return { transaction, objectStoreIdentifier, range, direction, type };
}

IDBCursorInfo IDBCursorInfo::indexCursor(IDBTransaction& transaction, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& range, IndexedDB::CursorDirection direction, IndexedDB::CursorType type)
{
    return { transaction, objectStoreIdentifier, indexIdentifier, range, direction, type };
}

IDBCursorInfo::IDBCursorInfo()
{
}

IDBCursorInfo::IDBCursorInfo(IDBTransaction& transaction, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range, IndexedDB::CursorDirection direction, IndexedDB::CursorType type)
    : m_cursorIdentifier(transaction.database().connectionProxy())
    , m_transactionIdentifier(transaction.info().identifier())
    , m_objectStoreIdentifier(objectStoreIdentifier)
    , m_sourceIdentifier(objectStoreIdentifier)
    , m_range(range)
    , m_source(IndexedDB::CursorSource::ObjectStore)
    , m_direction(direction)
    , m_type(type)
{
}

IDBCursorInfo::IDBCursorInfo(IDBTransaction& transaction, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& range, IndexedDB::CursorDirection direction, IndexedDB::CursorType type)
    : m_cursorIdentifier(transaction.database().connectionProxy())
    , m_transactionIdentifier(transaction.info().identifier())
    , m_objectStoreIdentifier(objectStoreIdentifier)
    , m_sourceIdentifier(indexIdentifier)
    , m_range(range)
    , m_source(IndexedDB::CursorSource::Index)
    , m_direction(direction)
    , m_type(type)
{
}

IDBCursorInfo::IDBCursorInfo(const IDBResourceIdentifier& cursorIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t sourceIdentifier, const IDBKeyRangeData& range, IndexedDB::CursorSource source, IndexedDB::CursorDirection direction, IndexedDB::CursorType type)
    : m_cursorIdentifier(cursorIdentifier)
    , m_transactionIdentifier(transactionIdentifier)
    , m_objectStoreIdentifier(objectStoreIdentifier)
    , m_sourceIdentifier(sourceIdentifier)
    , m_range(range)
    , m_source(source)
    , m_direction(direction)
    , m_type(type)
{
}

bool IDBCursorInfo::isDirectionForward() const
{
    return m_direction == IndexedDB::CursorDirection::Next || m_direction == IndexedDB::CursorDirection::Nextunique;
}

CursorDuplicity IDBCursorInfo::duplicity() const
{
    return m_direction == IndexedDB::CursorDirection::Nextunique || m_direction == IndexedDB::CursorDirection::Prevunique ? CursorDuplicity::NoDuplicates : CursorDuplicity::Duplicates;
}

IDBCursorInfo IDBCursorInfo::isolatedCopy() const
{
    return { m_cursorIdentifier.isolatedCopy(), m_transactionIdentifier.isolatedCopy(), m_objectStoreIdentifier, m_sourceIdentifier, m_range.isolatedCopy(), m_source, m_direction, m_type };
}

#if !LOG_DISABLED
String IDBCursorInfo::loggingString() const
{
    if (m_source == IndexedDB::CursorSource::Index)
        return String::format("<Crsr: %s Idx %" PRIu64 ", OS %" PRIu64 ", tx %s>", m_cursorIdentifier.loggingString().utf8().data(), m_sourceIdentifier, m_objectStoreIdentifier, m_transactionIdentifier.loggingString().utf8().data());

    return String::format("<Crsr: %s OS %" PRIu64 ", tx %s>", m_cursorIdentifier.loggingString().utf8().data(), m_objectStoreIdentifier, m_transactionIdentifier.loggingString().utf8().data());
}
#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
