/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SQLiteIDBTransaction.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include <WebCore/IndexedDB.h>
#include <WebCore/SQLiteTransaction.h>

using namespace WebCore;

namespace WebKit {

SQLiteIDBTransaction::SQLiteIDBTransaction(const IDBIdentifier& transactionIdentifier, IndexedDB::TransactionMode mode)
    : m_identifier(transactionIdentifier)
    , m_mode(mode)
{
}

SQLiteIDBTransaction::~SQLiteIDBTransaction()
{
    if (inProgress())
        m_sqliteTransaction->rollback();
}


bool SQLiteIDBTransaction::begin(SQLiteDatabase& database)
{
    ASSERT(!m_sqliteTransaction);
    m_sqliteTransaction = std::make_unique<SQLiteTransaction>(database, m_mode == IndexedDB::TransactionMode::ReadOnly);

    m_sqliteTransaction->begin();

    return m_sqliteTransaction->inProgress();
}

bool SQLiteIDBTransaction::commit()
{
    ASSERT(m_sqliteTransaction);
    if (!m_sqliteTransaction->inProgress())
        return false;

    m_sqliteTransaction->commit();

    return !m_sqliteTransaction->inProgress();
}

bool SQLiteIDBTransaction::reset()
{
    m_sqliteTransaction = nullptr;

    return true;
}

bool SQLiteIDBTransaction::rollback()
{
    ASSERT(m_sqliteTransaction);
    if (m_sqliteTransaction->inProgress())
        m_sqliteTransaction->rollback();

    return true;
}

bool SQLiteIDBTransaction::inProgress() const
{
    return m_sqliteTransaction && m_sqliteTransaction->inProgress();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
