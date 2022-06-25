/*
 * Copyright (C) 2007, 2013, 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#pragma once

#include "SQLTransactionStateMachine.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Database;
class OriginLock;
class SQLError;
class SQLiteTransaction;
class SQLStatement;
class SQLTransaction;
class SQLTransactionWrapper;

class SQLTransactionBackend : public SQLTransactionStateMachine<SQLTransactionBackend> {
public:
    explicit SQLTransactionBackend(SQLTransaction&);
    ~SQLTransactionBackend();

    void notifyDatabaseThreadIsShuttingDown();

    // API called from the frontend published via SQLTransactionBackend:
    void requestTransitToState(SQLTransactionState);

private:
    friend class SQLTransaction;

    void doCleanup();

    // State Machine functions:
    StateFunction stateFunctionFor(SQLTransactionState) override;
    void computeNextStateAndCleanupIfNeeded();

    // State functions:
    void acquireLock();
    void openTransactionAndPreflight();
    void runStatements();
    void cleanupAndTerminate();
    void cleanupAfterTransactionErrorCallback();

    NO_RETURN_DUE_TO_ASSERT void unreachableState();

    SQLTransaction& m_frontend;
};

} // namespace WebCore
