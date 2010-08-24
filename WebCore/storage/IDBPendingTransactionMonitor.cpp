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
#include "IDBPendingTransactionMonitor.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

Vector<int>* IDBPendingTransactionMonitor::m_ids = 0;

bool IDBPendingTransactionMonitor::hasPendingTransactions()
{
    return m_ids && m_ids->size();
}

void IDBPendingTransactionMonitor::addPendingTransaction(int id)
{
    if (!m_ids)
        m_ids = new Vector<int>();
    m_ids->append(id);
}

void IDBPendingTransactionMonitor::removePendingTransaction(int id)
{
    m_ids->remove(id);
    if (!m_ids->size()) {
        delete m_ids;
        m_ids = 0;
    }
}

void IDBPendingTransactionMonitor::clearPendingTransactions()
{
    if (!m_ids)
        return;

    m_ids->clear();
    delete m_ids;
    m_ids = 0;
}

const Vector<int>& IDBPendingTransactionMonitor::pendingTransactions()
{
    return *m_ids;
}

};
#endif // ENABLE(INDEXED_DATABASE)
