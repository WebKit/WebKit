/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SQLiteTransaction.h"

#include "Logging.h"
#include "SQLiteDatabase.h"
#include "SQLiteDatabaseTracker.h"

namespace WebCore {

SQLiteTransaction::SQLiteTransaction(SQLiteDatabase& db, bool readOnly)
    : m_db(db)
    , m_inProgress(false)
    , m_readOnly(readOnly)
{
}

SQLiteTransaction::~SQLiteTransaction()
{
    if (m_inProgress)
        rollback();
}

void SQLiteTransaction::begin()
{
    if (!m_inProgress) {
        ASSERT(!m_db->m_transactionInProgress);
        // Call BEGIN IMMEDIATE for a write transaction to acquire
        // a RESERVED lock on the DB file. Otherwise, another write
        // transaction (on another connection) could make changes
        // to the same DB file before this transaction gets to execute
        // any statements. If that happens, this transaction will fail.
        // http://www.sqlite.org/lang_transaction.html
        // http://www.sqlite.org/lockingv3.html#locking
        SQLiteDatabaseTracker::incrementTransactionInProgressCount();
        int result = SQLITE_OK;
        if (m_readOnly)
            result = m_db->execute("BEGIN"_s);
        else
            result = m_db->execute("BEGIN IMMEDIATE"_s);
        if (result == SQLITE_DONE)
            m_inProgress = true;
        else
            RELEASE_LOG_ERROR(SQLDatabase, "SQLiteTransaction::begin: Failed to begin transaction (error %d)", result);
        m_db->m_transactionInProgress = m_inProgress;
        if (!m_inProgress)
            SQLiteDatabaseTracker::decrementTransactionInProgressCount();
    } else
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteTransaction::begin: Transaction is already in progress");
}

void SQLiteTransaction::commit()
{
    if (m_inProgress) {
        ASSERT(m_db->m_transactionInProgress);
        m_inProgress = !m_db->executeCommand("COMMIT"_s);
        m_db->m_transactionInProgress = m_inProgress;
        if (!m_inProgress)
            SQLiteDatabaseTracker::decrementTransactionInProgressCount();
    }
}

void SQLiteTransaction::rollback()
{
    // We do not use the 'm_inProgress = m_db->executeCommand("ROLLBACK")' construct here,
    // because m_inProgress should always be set to false after a ROLLBACK, and
    // m_db->executeCommand("ROLLBACK") can sometimes harmlessly fail, thus returning
    // a non-zero/true result (http://www.sqlite.org/lang_transaction.html).
    if (m_inProgress) {
        ASSERT(m_db->m_transactionInProgress);
        m_db->executeCommand("ROLLBACK"_s);
        m_inProgress = false;
        m_db->m_transactionInProgress = false;
        SQLiteDatabaseTracker::decrementTransactionInProgressCount();
    }
}

void SQLiteTransaction::stop()
{
    if (m_inProgress) {
        m_inProgress = false;
        m_db->m_transactionInProgress = false;
        SQLiteDatabaseTracker::decrementTransactionInProgressCount();
    }
}

bool SQLiteTransaction::wasRolledBackBySqlite() const
{
    // According to http://www.sqlite.org/c3ref/get_autocommit.html,
    // the auto-commit flag should be off in the middle of a transaction
    return m_inProgress && m_db->isAutoCommitOn();
}

} // namespace WebCore
