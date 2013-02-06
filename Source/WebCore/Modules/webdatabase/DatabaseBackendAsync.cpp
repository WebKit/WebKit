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
#include "DatabaseBackendAsync.h"

#if ENABLE(SQL_DATABASE)

#include "DatabaseBackendContext.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"

namespace WebCore {

DatabaseBackendAsync::DatabaseBackendAsync(PassRefPtr<DatabaseBackendContext> databaseContext, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBackend(databaseContext, name, expectedVersion, displayName, estimatedSize, DatabaseType::Async)
{
}

bool DatabaseBackendAsync::openAndVerifyVersion(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    DatabaseTaskSynchronizer synchronizer;
    if (!databaseContext()->databaseThread() || databaseContext()->databaseThread()->terminationRequested(&synchronizer))
        return false;

#if PLATFORM(CHROMIUM)
    DatabaseTracker::tracker().prepareToOpenDatabase(this);
#endif
    bool success = false;
    OwnPtr<DatabaseOpenTask> task = DatabaseOpenTask::create(this, setVersionInNewDatabase, &synchronizer, error, errorMessage, success);
    databaseContext()->databaseThread()->scheduleImmediateTask(task.release());
    synchronizer.waitForTaskCompletion();

    return success;
}

bool DatabaseBackendAsync::performOpenAndVerify(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    if (DatabaseBackend::performOpenAndVerify(setVersionInNewDatabase, error, errorMessage)) {
        if (databaseContext()->databaseThread())
            databaseContext()->databaseThread()->recordDatabaseOpen(this);

        return true;
    }

    return false;
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
