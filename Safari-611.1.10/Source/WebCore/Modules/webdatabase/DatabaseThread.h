/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include <memory>
#include <wtf/HashSet.h>
#include <wtf/MessageQueue.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class Database;
class DatabaseTask;
class DatabaseTaskSynchronizer;
class Document;
class SQLTransactionCoordinator;

class DatabaseThread : public ThreadSafeRefCounted<DatabaseThread> {
public:
    static Ref<DatabaseThread> create() { return adoptRef(*new DatabaseThread); }
    ~DatabaseThread();

    void start();
    void requestTermination(DatabaseTaskSynchronizer* cleanupSync);
    bool terminationRequested(DatabaseTaskSynchronizer* = nullptr) const;

    void scheduleTask(std::unique_ptr<DatabaseTask>&&);
    void scheduleImmediateTask(std::unique_ptr<DatabaseTask>&&); // This just adds the task to the front of the queue - the caller needs to be extremely careful not to create deadlocks when waiting for completion.
    void unscheduleDatabaseTasks(Database&);
    bool hasPendingDatabaseActivity() const;

    void recordDatabaseOpen(Database&);
    void recordDatabaseClosed(Database&);
    Thread* getThread() { return m_thread.get(); }

    SQLTransactionCoordinator* transactionCoordinator() { return m_transactionCoordinator.get(); }

private:
    DatabaseThread();

    void databaseThread();

    Lock m_threadCreationMutex;
    RefPtr<Thread> m_thread;
    RefPtr<DatabaseThread> m_selfRef;

    MessageQueue<DatabaseTask> m_queue;

    // This set keeps track of the open databases that have been used on this thread.
    using DatabaseSet = HashSet<RefPtr<Database>>;
    mutable Lock m_openDatabaseSetMutex;
    DatabaseSet m_openDatabaseSet;

    std::unique_ptr<SQLTransactionCoordinator> m_transactionCoordinator;
    DatabaseTaskSynchronizer* m_cleanupSync { nullptr };
};

} // namespace WebCore
