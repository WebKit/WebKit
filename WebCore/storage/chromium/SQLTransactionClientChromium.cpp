/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "SQLTransactionClient.h"

#include "Database.h"
#include "DatabaseObserver.h"
#include "DatabaseThread.h"
#include "Document.h"
#include "SQLTransaction.h"
#include <wtf/MainThread.h>

static void notifyDatabaseChanged(void* context) {
    WebCore::Database* database = static_cast<WebCore::Database*>(context);
    WebCore::DatabaseObserver::databaseModified(database);
    database->deref();  // ref()'d in didCommitTransaction()
}

namespace WebCore {

void SQLTransactionClient::didCommitTransaction(SQLTransaction* transaction)
{
    ASSERT(currentThread() == transaction->database()->document()->databaseThread()->getThreadID());
    if (!transaction->isReadOnly()) {
        transaction->database()->ref();  // deref()'d in notifyDatabaseChanged()
        callOnMainThread(notifyDatabaseChanged, transaction->database());
    }
}

void SQLTransactionClient::didExecuteStatement(SQLTransaction* transaction)
{
    // This method is called after executing every statement that changes the DB.
    // Chromium doesn't need to do anything at that point.
    ASSERT(currentThread() == transaction->database()->document()->databaseThread()->getThreadID());
}

bool SQLTransactionClient::didExceedQuota(SQLTransaction*)
{
    // Chromium does not allow users to manually change the quota for an origin (for now, at least).
    // Don't do anything.
    ASSERT(isMainThread());
    return false;
}

}
