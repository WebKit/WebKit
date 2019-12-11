/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "SecurityOrigin.h"
#include "ServiceWorkerRegistrationKey.h"
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RegistrationStore;
class SQLiteDatabase;
struct ServiceWorkerContextData;

WEBCORE_EXPORT String serviceWorkerRegistrationDatabaseFilename(const String& databaseDirectory);

class RegistrationDatabase : public ThreadSafeRefCounted<RegistrationDatabase, WTF::DestructionThread::Main> {
WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RegistrationDatabase> create(RegistrationStore& store, String&& databaseDirectory)
    {
        return adoptRef(*new RegistrationDatabase(store, WTFMove(databaseDirectory)));
    }

    ~RegistrationDatabase();

    void pushChanges(const HashMap<ServiceWorkerRegistrationKey, Optional<ServiceWorkerContextData>>&, CompletionHandler<void()>&&);
    void clearAll(CompletionHandler<void()>&&);
    void close(CompletionHandler<void()>&&);

private:
    RegistrationDatabase(RegistrationStore&, String&& databaseDirectory);
    
    String databaseDirectoryIsolatedCopy() const { return m_databaseDirectory.isolatedCopy(); }

    void postTaskToWorkQueue(Function<void()>&&);

    // Methods to be run on the work queue.
    void openSQLiteDatabase(const String& fullFilename);
    String ensureValidRecordsTable();
    String importRecords();
    void importRecordsIfNecessary();
    void doPushChanges(const Vector<ServiceWorkerContextData>&, const Vector<ServiceWorkerRegistrationKey>&);
    void doClearOrigin(const SecurityOrigin&);

    // Replies to the main thread.
    void addRegistrationToStore(ServiceWorkerContextData&&);
    void databaseFailedToOpen();
    void databaseOpenedAndRecordsImported(); 

    Ref<WorkQueue> m_workQueue;
    WeakPtr<RegistrationStore> m_store;
    String m_databaseDirectory;
    String m_databaseFilePath;
    std::unique_ptr<SQLiteDatabase> m_database;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
