/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#include "TransactionOperation.h"

#include "IDBActiveDOMObjectInlines.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include <JavaScriptCore/HeapInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBClient {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TransactionOperation);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TransactionOperationImpl);

TransactionOperation::TransactionOperation(IDBTransaction& transaction)
    : m_transaction(transaction)
    , m_identifier(transaction.connectionProxy())
    , m_operationID(transaction.generateOperationID())
    , m_scriptExecutionContextIdentifier(transaction.database().scriptExecutionContextIdentifier())
{
}

TransactionOperation::TransactionOperation(IDBTransaction& transaction, IDBRequest& request)
    : TransactionOperation(transaction)
{
    m_objectStoreIdentifier = request.sourceObjectStoreIdentifier();
    m_indexIdentifier = request.sourceIndexIdentifier();
    if (m_indexIdentifier)
        m_indexRecordType = request.requestedIndexRecordType();
    if (RefPtr cursor = request.pendingCursor())
        m_cursorIdentifier = cursor->info().identifier();

    request.setTransactionOperationID(m_operationID);
    m_idbRequest = request;
}

void TransactionOperation::transitionToComplete(const IDBResultData& data, RefPtr<TransactionOperation>&& lastRef)
{
    ASSERT(isMainThread());

    if (canCurrentThreadAccessThreadLocalData(originThread()))
        transitionToCompleteOnThisThread(data);
    else {
        m_transaction->performCallbackOnOriginThread(*this, &TransactionOperation::transitionToCompleteOnThisThread, data);
        m_transaction->callFunctionOnOriginThread([lastRef = WTFMove(lastRef)]() {
        });
    }
}

} // namespace IDBClient
} // namespace WebCore
