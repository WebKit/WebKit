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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "config.h"
#include "DatabaseSync.h"

#if ENABLE(DATABASE)
#include "DatabaseCallback.h"
#include "DatabaseTracker.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "SQLTransactionSyncCallback.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

PassRefPtr<DatabaseSync> DatabaseSync::openDatabaseSync(ScriptExecutionContext* context, const String& name, const String& expectedVersion, const String& displayName,
                                                        unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback, ExceptionCode& ec)
{
    ASSERT(context->isContextThread());

    if (!DatabaseTracker::tracker().canEstablishDatabase(context, name, displayName, estimatedSize)) {
        LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(), context->securityOrigin()->toString().ascii().data());
        return 0;
    }

    RefPtr<DatabaseSync> database = adoptRef(new DatabaseSync(context, name, expectedVersion, displayName, estimatedSize));

    if (!database->performOpenAndVerify(!creationCallback, ec)) {
        LOG(StorageAPI, "Failed to open and verify version (expected %s) of database %s", expectedVersion.ascii().data(), database->databaseDebugName().ascii().data());
        DatabaseTracker::tracker().removeOpenDatabase(database.get());
        return 0;
    }

    DatabaseTracker::tracker().setDatabaseDetails(context->securityOrigin(), name, displayName, estimatedSize);

    if (database->isNew() && creationCallback.get()) {
        database->m_expectedVersion = "";
        LOG(StorageAPI, "Invoking the creation callback for database %p\n", database.get());
        creationCallback->handleEvent(context, database.get());
    }

    return database;
}

DatabaseSync::DatabaseSync(ScriptExecutionContext* context, const String& name, const String& expectedVersion,
                           const String& displayName, unsigned long estimatedSize)
    : AbstractDatabase(context, name, expectedVersion, displayName, estimatedSize)
{
}

DatabaseSync::~DatabaseSync()
{
    ASSERT(m_scriptExecutionContext->isContextThread());
    if (opened()) {
      DatabaseTracker::tracker().removeOpenDatabase(this);
      closeDatabase();
    }
}

void DatabaseSync::changeVersion(const String&, const String&, PassRefPtr<SQLTransactionSyncCallback>, ExceptionCode&)
{
    ASSERT(m_scriptExecutionContext->isContextThread());
}

void DatabaseSync::transaction(PassRefPtr<SQLTransactionSyncCallback>, bool, ExceptionCode&)
{
    ASSERT(m_scriptExecutionContext->isContextThread());
}

void DatabaseSync::markAsDeletedAndClose()
{
    // FIXME: need to do something similar to closeImmediately(), but in a sync way
}

class CloseSyncDatabaseOnContextThreadTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<CloseSyncDatabaseOnContextThreadTask> create(PassRefPtr<DatabaseSync> database)
    {
        return new CloseSyncDatabaseOnContextThreadTask(database);
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_database->closeImmediately();
    }

private:
    CloseSyncDatabaseOnContextThreadTask(PassRefPtr<DatabaseSync> database)
        : m_database(database)
    {
    }

    RefPtr<DatabaseSync> m_database;
};

void DatabaseSync::closeImmediately()
{
    if (!m_scriptExecutionContext->isContextThread()) {
        m_scriptExecutionContext->postTask(CloseSyncDatabaseOnContextThreadTask::create(this));
        return;
    }

    if (!opened())
        return;

    DatabaseTracker::tracker().removeOpenDatabase(this);

    closeDatabase();
}

} // namespace WebCore

#endif // ENABLE(DATABASE)
