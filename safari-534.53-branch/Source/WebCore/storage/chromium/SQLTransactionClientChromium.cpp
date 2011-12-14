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

#if ENABLE(DATABASE)

#include "AbstractDatabase.h"
#include "DatabaseObserver.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

class NotifyDatabaseChangedTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<NotifyDatabaseChangedTask> create(AbstractDatabase *database)
    {
        return adoptPtr(new NotifyDatabaseChangedTask(database));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        WebCore::DatabaseObserver::databaseModified(m_database.get());
    }

private:
    NotifyDatabaseChangedTask(PassRefPtr<AbstractDatabase> database)
        : m_database(database)
    {
    }

    RefPtr<AbstractDatabase> m_database;
};

void SQLTransactionClient::didCommitWriteTransaction(AbstractDatabase* database)
{
    if (!database->scriptExecutionContext()->isContextThread()) {
        database->scriptExecutionContext()->postTask(NotifyDatabaseChangedTask::create(database));
        return;
    }

    WebCore::DatabaseObserver::databaseModified(database);
}

void SQLTransactionClient::didExecuteStatement(AbstractDatabase* database)
{
    // This method is called after executing every statement that changes the DB.
    // Chromium doesn't need to do anything at that point.
}

bool SQLTransactionClient::didExceedQuota(AbstractDatabase* database)
{
    // Chromium does not allow users to manually change the quota for an origin (for now, at least).
    // Don't do anything.
    ASSERT(database->scriptExecutionContext()->isContextThread());
    return false;
}

}

#endif // ENABLE(DATABASE)
