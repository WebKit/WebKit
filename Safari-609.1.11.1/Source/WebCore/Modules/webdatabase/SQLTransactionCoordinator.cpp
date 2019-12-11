/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SQLTransactionCoordinator.h"

#include "Database.h"
#include "SQLTransaction.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include <wtf/Deque.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static String getDatabaseIdentifier(SQLTransaction& transaction)
{
    return transaction.database().securityOrigin().databaseIdentifier();
}

SQLTransactionCoordinator::SQLTransactionCoordinator()
    : m_isShuttingDown(false)
{
}

void SQLTransactionCoordinator::processPendingTransactions(CoordinationInfo& info)
{
    if (info.activeWriteTransaction || info.pendingTransactions.isEmpty())
        return;

    RefPtr<SQLTransaction> firstPendingTransaction = info.pendingTransactions.first();
    if (firstPendingTransaction->isReadOnly()) {
        do {
            firstPendingTransaction = info.pendingTransactions.takeFirst();
            info.activeReadTransactions.add(firstPendingTransaction);
            firstPendingTransaction->lockAcquired();
        } while (!info.pendingTransactions.isEmpty() && info.pendingTransactions.first()->isReadOnly());
    } else if (info.activeReadTransactions.isEmpty()) {
        info.pendingTransactions.removeFirst();
        info.activeWriteTransaction = firstPendingTransaction;
        firstPendingTransaction->lockAcquired();
    }
}

void SQLTransactionCoordinator::acquireLock(SQLTransaction& transaction)
{
    ASSERT(!m_isShuttingDown);

    String dbIdentifier = getDatabaseIdentifier(transaction);

    CoordinationInfoMap::iterator coordinationInfoIterator = m_coordinationInfoMap.find(dbIdentifier);
    if (coordinationInfoIterator == m_coordinationInfoMap.end()) {
        // No pending transactions for this DB
        coordinationInfoIterator = m_coordinationInfoMap.add(dbIdentifier, CoordinationInfo()).iterator;
    }

    CoordinationInfo& info = coordinationInfoIterator->value;
    info.pendingTransactions.append(&transaction);
    processPendingTransactions(info);
}

void SQLTransactionCoordinator::releaseLock(SQLTransaction& transaction)
{
    if (m_isShuttingDown)
        return;

    String dbIdentifier = getDatabaseIdentifier(transaction);

    CoordinationInfoMap::iterator coordinationInfoIterator = m_coordinationInfoMap.find(dbIdentifier);
    ASSERT(coordinationInfoIterator != m_coordinationInfoMap.end());
    CoordinationInfo& info = coordinationInfoIterator->value;

    if (transaction.isReadOnly()) {
        ASSERT(info.activeReadTransactions.contains(&transaction));
        info.activeReadTransactions.remove(&transaction);
    } else {
        ASSERT(info.activeWriteTransaction == &transaction);
        info.activeWriteTransaction = nullptr;
    }

    processPendingTransactions(info);
}

void SQLTransactionCoordinator::shutdown()
{
    // Prevent releaseLock() from accessing / changing the coordinationInfo
    // while we're shutting down.
    m_isShuttingDown = true;

    // Notify all transactions in progress that the database thread is shutting down
    for (auto& info : m_coordinationInfoMap.values()) {
        // Clean up transactions that have reached "lockAcquired":
        // Transaction phase 4 cleanup. See comment on "What happens if a
        // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
        if (info.activeWriteTransaction)
            info.activeWriteTransaction->notifyDatabaseThreadIsShuttingDown();
        for (auto& transaction : info.activeReadTransactions)
            transaction->notifyDatabaseThreadIsShuttingDown();

        // Clean up transactions that have NOT reached "lockAcquired":
        // Transaction phase 3 cleanup. See comment on "What happens if a
        // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
        while (!info.pendingTransactions.isEmpty()) {
            RefPtr<SQLTransaction> transaction = info.pendingTransactions.first();
            transaction->notifyDatabaseThreadIsShuttingDown();
        }
    }

    // Clean up all pending transactions for all databases
    m_coordinationInfoMap.clear();
}

} // namespace WebCore
