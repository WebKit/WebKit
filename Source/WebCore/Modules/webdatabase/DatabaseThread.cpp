/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DatabaseThread.h"

#include "Database.h"
#include "DatabaseTask.h"
#include "Logging.h"
#include "SQLTransactionClient.h"
#include "SQLTransactionCoordinator.h"
#include <wtf/AutodrainedPool.h>

namespace WebCore {

DatabaseThread::DatabaseThread()
    : m_threadID(0)
    , m_transactionClient(std::make_unique<SQLTransactionClient>())
    , m_transactionCoordinator(std::make_unique<SQLTransactionCoordinator>())
    , m_cleanupSync(nullptr)
{
    m_selfRef = this;
}

DatabaseThread::~DatabaseThread()
{
    // The DatabaseThread will only be destructed when both its owner
    // DatabaseContext has deref'ed it, and the databaseThread() thread function
    // has deref'ed the DatabaseThread object. The DatabaseContext destructor
    // will take care of ensuring that a termination request has been issued.
    // The termination request will trigger an orderly shutdown of the thread
    // function databaseThread(). In shutdown, databaseThread() will deref the
    // DatabaseThread before returning.
    ASSERT(terminationRequested());
}

bool DatabaseThread::start()
{
    LockHolder lock(m_threadCreationMutex);

    if (m_threadID)
        return true;

    m_threadID = createThread(DatabaseThread::databaseThreadStart, this, "WebCore: Database");

    return m_threadID;
}

void DatabaseThread::requestTermination(DatabaseTaskSynchronizer *cleanupSync)
{
    m_cleanupSync = cleanupSync;
    LOG(StorageAPI, "DatabaseThread %p was asked to terminate\n", this);
    m_queue.kill();
}

bool DatabaseThread::terminationRequested(DatabaseTaskSynchronizer* taskSynchronizer) const
{
#ifndef NDEBUG
    if (taskSynchronizer)
        taskSynchronizer->setHasCheckedForTermination();
#else
    UNUSED_PARAM(taskSynchronizer);
#endif

    return m_queue.killed();
}

void DatabaseThread::databaseThreadStart(void* vDatabaseThread)
{
    DatabaseThread* dbThread = static_cast<DatabaseThread*>(vDatabaseThread);
    dbThread->databaseThread();
}

void DatabaseThread::databaseThread()
{
    {
        // Wait for DatabaseThread::start() to complete.
        LockHolder lock(m_threadCreationMutex);
        LOG(StorageAPI, "Started DatabaseThread %p", this);
    }

    while (auto task = m_queue.waitForMessage()) {
        AutodrainedPool pool;

        task->performTask();
    }

    // Clean up the list of all pending transactions on this database thread
    m_transactionCoordinator->shutdown();

    LOG(StorageAPI, "About to detach thread %i and clear the ref to DatabaseThread %p, which currently has %i ref(s)", m_threadID, this, refCount());

    // Close the databases that we ran transactions on. This ensures that if any transactions are still open, they are rolled back and we don't leave the database in an
    // inconsistent or locked state.
    if (m_openDatabaseSet.size() > 0) {
        // As the call to close will modify the original set, we must take a copy to iterate over.
        DatabaseSet openSetCopy;
        openSetCopy.swap(m_openDatabaseSet);
        for (auto& openDatabase : openSetCopy)
            openDatabase->close();
    }

    // Detach the thread so its resources are no longer of any concern to anyone else
    detachThread(m_threadID);

    DatabaseTaskSynchronizer* cleanupSync = m_cleanupSync;

    // Clear the self refptr, possibly resulting in deletion
    m_selfRef = nullptr;

    if (cleanupSync) // Someone wanted to know when we were done cleaning up.
        cleanupSync->taskCompleted();
}

void DatabaseThread::recordDatabaseOpen(Database* database)
{
    ASSERT(currentThread() == m_threadID);
    ASSERT(database);
    ASSERT(!m_openDatabaseSet.contains(database));
    m_openDatabaseSet.add(database);
}

void DatabaseThread::recordDatabaseClosed(Database* database)
{
    ASSERT(currentThread() == m_threadID);
    ASSERT(database);
    ASSERT(m_queue.killed() || m_openDatabaseSet.contains(database));
    m_openDatabaseSet.remove(database);
}

void DatabaseThread::scheduleTask(std::unique_ptr<DatabaseTask> task)
{
    ASSERT(!task->hasSynchronizer() || task->hasCheckedForTermination());
    m_queue.append(WTFMove(task));
}

void DatabaseThread::scheduleImmediateTask(std::unique_ptr<DatabaseTask> task)
{
    ASSERT(!task->hasSynchronizer() || task->hasCheckedForTermination());
    m_queue.prepend(WTFMove(task));
}

class SameDatabasePredicate {
public:
    SameDatabasePredicate(const Database* database) : m_database(database) { }
    bool operator()(const DatabaseTask& task) const { return &task.database() == m_database; }
private:
    const Database* m_database;
};

void DatabaseThread::unscheduleDatabaseTasks(Database* database)
{
    // Note that the thread loop is running, so some tasks for the database
    // may still be executed. This is unavoidable.
    SameDatabasePredicate predicate(database);
    m_queue.removeIf(predicate);
}

bool DatabaseThread::hasPendingDatabaseActivity() const
{
    for (auto& database : m_openDatabaseSet) {
        if (database->hasPendingCreationEvent() || database->hasPendingTransaction())
            return true;
    }
    return false;
}

} // namespace WebCore
