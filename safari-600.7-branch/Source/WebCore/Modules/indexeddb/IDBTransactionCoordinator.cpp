/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBTransactionCoordinator.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseCallbacks.h"
#include "IDBTransactionBackend.h"

namespace WebCore {

IDBTransactionCoordinator::IDBTransactionCoordinator()
{
}

IDBTransactionCoordinator::~IDBTransactionCoordinator()
{
}

void IDBTransactionCoordinator::didCreateTransaction(IDBTransactionBackend* transaction)
{
    ASSERT(!m_transactions.contains(transaction));
    m_transactions.add(transaction, transaction);
}

void IDBTransactionCoordinator::didStartTransaction(IDBTransactionBackend* transaction)
{
    ASSERT(m_transactions.contains(transaction));

    m_queuedTransactions.add(transaction);
    processStartedTransactions();
}

void IDBTransactionCoordinator::didFinishTransaction(IDBTransactionBackend* transaction)
{
    ASSERT(m_transactions.contains(transaction));

    if (m_queuedTransactions.contains(transaction)) {
        ASSERT(!m_startedTransactions.contains(transaction));
        m_queuedTransactions.remove(transaction);
    } else if (m_startedTransactions.contains(transaction))
        m_startedTransactions.remove(transaction);

    m_transactions.remove(transaction);

    processStartedTransactions();
}

#ifndef NDEBUG
// Verifies internal consistiency while returning whether anything is found.
bool IDBTransactionCoordinator::isActive(IDBTransactionBackend* transaction)
{
    bool found = false;
    if (m_queuedTransactions.contains(transaction))
        found = true;
    if (m_startedTransactions.contains(transaction)) {
        ASSERT(!found);
        found = true;
    }
    ASSERT(found == m_transactions.contains(transaction));
    return found;
}
#endif

void IDBTransactionCoordinator::processStartedTransactions()
{
    if (m_queuedTransactions.isEmpty())
        return;

    ASSERT(m_startedTransactions.isEmpty() || (*m_startedTransactions.begin())->mode() != IndexedDB::TransactionMode::VersionChange);

    ListHashSet<IDBTransactionBackend*>::const_iterator it = m_queuedTransactions.begin();
    while (it != m_queuedTransactions.end()) {
        IDBTransactionBackend* transaction = *it;
        ++it;
        if (canRunTransaction(transaction)) {
            m_queuedTransactions.remove(transaction);
            m_startedTransactions.add(transaction);
            transaction->run();
        }
    }
}

#if USE(LEVELDB)
static bool doScopesOverlap(const HashSet<int64_t>& scope1, const HashSet<int64_t>& scope2)
{
    for (HashSet<int64_t>::const_iterator it = scope1.begin(); it != scope1.end(); ++it) {
        if (scope2.contains(*it))
            return true;
    }
    return false;
}
#endif

bool IDBTransactionCoordinator::canRunTransaction(IDBTransactionBackend* transaction)
{
    ASSERT(m_queuedTransactions.contains(transaction));

#if USE(LEVELDB)
    switch (transaction->mode()) {
    case IndexedDB::TransactionMode::VersionChange:
        ASSERT(m_queuedTransactions.size() == 1);
        ASSERT(m_startedTransactions.isEmpty());
        return true;

    case IndexedDB::TransactionMode::ReadOnly:
        return true;

    case IndexedDB::TransactionMode::ReadWrite:
        for (HashSet<IDBTransactionBackend*>::const_iterator it = m_startedTransactions.begin(); it != m_startedTransactions.end(); ++it) {
            if ((*it)->mode() == IndexedDB::TransactionMode::ReadWrite && doScopesOverlap(transaction->scope(), (*it)->scope()))
                return false;
        }
        for (ListHashSet<IDBTransactionBackend*>::const_iterator it = m_queuedTransactions.begin(); *it != transaction; ++it) {
            ASSERT(it != m_queuedTransactions.end());
            if ((*it)->mode() == IndexedDB::TransactionMode::ReadWrite && doScopesOverlap(transaction->scope(), (*it)->scope()))
                return false;
        }
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
#else
    UNUSED_PARAM(transaction);
    return !m_startedTransactions.size();
#endif
}

};

#endif // ENABLE(INDEXED_DATABASE)
