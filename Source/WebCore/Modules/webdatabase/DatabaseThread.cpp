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
#include "DatabaseThread.h"

#if ENABLE(SQL_DATABASE)

#include "Database.h"
#include "DatabaseTask.h"
#include "Logging.h"
#include "SQLTransactionClient.h"
#include "SQLTransactionCoordinator.h"
#include <wtf/AutodrainedPool.h>

namespace WebCore {

DatabaseThread::DatabaseThread()
    : m_threadID(0)
#if PLATFORM(IOS)
    , m_paused(false)
#endif
    , m_transactionClient(std::make_unique<SQLTransactionClient>())
    , m_transactionCoordinator(std::make_unique<SQLTransactionCoordinator>())
    , m_cleanupSync(0)
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
    MutexLocker lock(m_threadCreationMutex);

    if (m_threadID)
        return true;

    m_threadID = createThread(DatabaseThread::databaseThreadStart, this, "WebCore: Database");

    return m_threadID;
}

void DatabaseThread::requestTermination(DatabaseTaskSynchronizer *cleanupSync)
{
    m_cleanupSync = cleanupSync;
    LOG(StorageAPI, "DatabaseThread %p was asked to terminate\n", this);
#if PLATFORM(IOS)
    m_pausedQueue.kill();
#endif
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

#if PLATFORM(IOS)
class DatabaseUnpauseTask : public DatabaseTask {
public:
    static std::unique_ptr<DatabaseUnpauseTask> create(DatabaseThread* thread)
    {
        return std::unique_ptr<DatabaseUnpauseTask>(new DatabaseUnpauseTask(thread));
    }
    
    virtual bool shouldPerformWhilePaused() const 
    {
        // Since we're not locking the DatabaseThread::m_paused in the main database thread loop, it's possible that
        // a DatabaseUnpauseTask might be added to the m_pausedQueue and performed from within ::handlePausedQueue.
        // To protect against this, we allow it to be performed even if the database is paused.
        // If the thread is paused when it is being performed, the tasks from the paused queue will simply be
        // requeued instead of performed.
        return true;
    }

private:
    DatabaseUnpauseTask(DatabaseThread* thread)
        : DatabaseTask(0, 0)
        , m_thread(thread)
    {}

    virtual void doPerformTask()
    {
        m_thread->handlePausedQueue();
    }
#if !LOG_DISABLED
    virtual const char* debugTaskName() const { return "DatabaseUnpauseTask"; }
#endif

    DatabaseThread* m_thread;
};


void DatabaseThread::setPaused(bool paused)
{
    if (m_paused == paused)
        return;

    MutexLocker pausedLocker(m_pausedMutex);
    m_paused = paused;
    if (!m_paused)
        scheduleTask(DatabaseUnpauseTask::create(this));
}

void DatabaseThread::handlePausedQueue()
{
    Vector<std::unique_ptr<DatabaseTask> > pausedTasks;
    while (auto task = m_pausedQueue.tryGetMessage())
        pausedTasks.append(std::move(task));

    for (unsigned i = 0; i < pausedTasks.size(); ++i) {
        AutodrainedPool pool;

        std::unique_ptr<DatabaseTask> task(pausedTasks[i].release());
        {
            MutexLocker pausedLocker(m_pausedMutex);
            if (m_paused) {
                m_pausedQueue.append(std::move(task));
                continue;
            }
        }
            
        if (terminationRequested())
            break;
    
        task->performTask();
    }
}
#endif //PLATFORM(IOS)


void DatabaseThread::databaseThread()
{
    {
        // Wait for DatabaseThread::start() to complete.
        MutexLocker lock(m_threadCreationMutex);
        LOG(StorageAPI, "Started DatabaseThread %p", this);
    }

    while (auto task = m_queue.waitForMessage()) {
        AutodrainedPool pool;

#if PLATFORM(IOS)
        if (!m_paused || task->shouldPerformWhilePaused())
            task->performTask();
        else
            m_pausedQueue.append(std::move(task));
#else
        task->performTask();
#endif
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
        DatabaseSet::iterator end = openSetCopy.end();
        for (DatabaseSet::iterator it = openSetCopy.begin(); it != end; ++it)
            (*it).get()->close();
    }

    // Detach the thread so its resources are no longer of any concern to anyone else
    detachThread(m_threadID);

    DatabaseTaskSynchronizer* cleanupSync = m_cleanupSync;

    // Clear the self refptr, possibly resulting in deletion
    m_selfRef = 0;

    if (cleanupSync) // Someone wanted to know when we were done cleaning up.
        cleanupSync->taskCompleted();
}

void DatabaseThread::recordDatabaseOpen(DatabaseBackend* database)
{
    ASSERT(currentThread() == m_threadID);
    ASSERT(database);
    ASSERT(!m_openDatabaseSet.contains(database));
    m_openDatabaseSet.add(database);
}

void DatabaseThread::recordDatabaseClosed(DatabaseBackend* database)
{
    ASSERT(currentThread() == m_threadID);
    ASSERT(database);
    ASSERT(m_queue.killed() || m_openDatabaseSet.contains(database));
    m_openDatabaseSet.remove(database);
}

void DatabaseThread::scheduleTask(std::unique_ptr<DatabaseTask> task)
{
    ASSERT(!task->hasSynchronizer() || task->hasCheckedForTermination());
    m_queue.append(std::move(task));
}

void DatabaseThread::scheduleImmediateTask(std::unique_ptr<DatabaseTask> task)
{
    ASSERT(!task->hasSynchronizer() || task->hasCheckedForTermination());
    m_queue.prepend(std::move(task));
}

class SameDatabasePredicate {
public:
    SameDatabasePredicate(const DatabaseBackend* database) : m_database(database) { }
    bool operator()(const DatabaseTask& task) const { return task.database() == m_database; }
private:
    const DatabaseBackend* m_database;
};

void DatabaseThread::unscheduleDatabaseTasks(DatabaseBackend* database)
{
    // Note that the thread loop is running, so some tasks for the database
    // may still be executed. This is unavoidable.
    SameDatabasePredicate predicate(database);
    m_queue.removeIf(predicate);
}
} // namespace WebCore
#endif
