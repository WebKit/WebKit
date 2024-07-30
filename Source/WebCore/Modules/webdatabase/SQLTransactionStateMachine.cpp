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
#include "SQLTransactionStateMachine.h"

#include "Logging.h"
#include <wtf/Assertions.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

#if !LOG_DISABLED
ASCIILiteral nameForSQLTransactionState(SQLTransactionState state)
{
    switch (state) {
    case SQLTransactionState::End:
        return "end"_s;
    case SQLTransactionState::Idle:
        return "idle"_s;
    case SQLTransactionState::AcquireLock:
        return "acquireLock"_s;
    case SQLTransactionState::OpenTransactionAndPreflight:
        return "openTransactionAndPreflight"_s;
    case SQLTransactionState::RunStatements:
        return "runStatements"_s;
    case SQLTransactionState::PostflightAndCommit:
        return "postflightAndCommit"_s;
    case SQLTransactionState::CleanupAndTerminate:
        return "cleanupAndTerminate"_s;
    case SQLTransactionState::CleanupAfterTransactionErrorCallback:
        return "cleanupAfterTransactionErrorCallback"_s;
    case SQLTransactionState::DeliverTransactionCallback:
        return "deliverTransactionCallback"_s;
    case SQLTransactionState::DeliverTransactionErrorCallback:
        return "deliverTransactionErrorCallback"_s;
    case SQLTransactionState::DeliverStatementCallback:
        return "deliverStatementCallback"_s;
    case SQLTransactionState::DeliverQuotaIncreaseCallback:
        return "deliverQuotaIncreaseCallback"_s;
    case SQLTransactionState::DeliverSuccessCallback:
        return "deliverSuccessCallback"_s;
    default:
        return "UNKNOWN"_s;
    }
}
#endif

} // namespace WebCore
