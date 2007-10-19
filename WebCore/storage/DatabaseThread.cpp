/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "AutodrainedPool.h"
#include "Database.h"
#include "DatabaseTask.h"
#include "Logging.h"

namespace WebCore {

DatabaseThread::DatabaseThread(Document* document)
    : m_threadID(0)
    , m_terminationRequested(false)
{
    m_selfRef = this;
}

DatabaseThread::~DatabaseThread()
{
    // FIXME: Any cleanup required here?  Since the thread deletes itself after running its detached course, I don't think so.  Lets be sure.
}

bool DatabaseThread::start()
{
    if (m_threadID)
        return true;

    m_threadID = createThread(DatabaseThread::databaseThreadStart, this);

    return m_threadID;
}

void DatabaseThread::documentGoingAway()
{
    // This document is going away, so this thread is going away.
    // -Clear records of all Databases out
    // -Leave the thread main loop
    // -Detach the thread so the thread itself runs out and releases all thread resources
    // -Clear the RefPtr<> self so if noone else is hanging on the thread, the object itself is deleted.

    LOG(StorageAPI, "Document owning DatabaseThread %p is going away - starting thread shutdown", this);

    // Clear all database records out, and let Databases know that this DatabaseThread is going away
    {
        MutexLocker locker(m_databaseWorkMutex);

        // FIXME - The policy we're enforcing right here is that when a document goes away, any pending
        // sql queries that were queued up also go away.  Is this appropriate?

        HashSet<RefPtr<Database> >::iterator i = m_activeDatabases.begin();
        HashSet<RefPtr<Database> >::iterator end = m_activeDatabases.end();

        for (; i != end; ++i) {
            (*i)->databaseThreadGoingAway();
            Deque<RefPtr<DatabaseTask> >* databaseQueue = m_databaseTaskQueues.get((*i).get());
            ASSERT(databaseQueue);
            m_databaseTaskQueues.remove((*i).get());
            delete databaseQueue;
        }
        ASSERT(m_databaseTaskQueues.isEmpty());
        m_activeDatabases.clear();
    }

    // Request the thread to cleanup and shutdown
    m_terminationRequested = true;
    wakeWorkThread();
}

void DatabaseThread::databaseGoingAway(Database* database)
{
    MutexLocker locker(m_databaseWorkMutex);
    if (!m_activeDatabases.contains(database))
        return;

    // FIXME - The policy we're enforcing right here is that when a document goes away, any pending
    // sql queries that were queued up also go away.  Is this appropriate?
    Deque<RefPtr<DatabaseTask> >* databaseQueue = m_databaseTaskQueues.get(database);
    ASSERT(databaseQueue);
    m_databaseTaskQueues.remove(database);
    delete databaseQueue;

    m_activeDatabases.remove(database);
}

void* DatabaseThread::databaseThreadStart(void* vDatabaseThread)
{
    DatabaseThread* dbThread = static_cast<DatabaseThread*>(vDatabaseThread);
    return dbThread->databaseThread();
}

void* DatabaseThread::databaseThread()
{
    LOG(StorageAPI, "Starting DatabaseThread %p", this);

    while (!m_terminationRequested) {
        m_threadMutex.unlock();
        AutodrainedPool pool;

        LOG(StorageAPI, "Iteration of main loop for DatabaseThread %p", this);

        bool result;
        do {
            result = dispatchNextTaskIdentifier();
            if (m_terminationRequested)
                break;
        } while(result);

        if (m_terminationRequested)
            break;

        pool.cycle();
        m_threadMutex.lock();
        m_threadCondition.wait(m_threadMutex);
    }
    m_threadMutex.unlock();

    LOG(StorageAPI, "About to detach thread %i and clear the ref to DatabaseThread %p, which currently has %i ref(s)", m_threadID, this, refCount());

    // Detach the thread so its resources are no longer of any concern to anyone else
    detachThread(m_threadID);

    // Clear the self refptr, possibly resulting in deletion
    m_selfRef = 0;

    return 0;
}

bool DatabaseThread::dispatchNextTaskIdentifier()
{
    Database* workDatabase = 0;
    RefPtr<DatabaseTask> task;

    {
        MutexLocker locker(m_databaseWorkMutex);
        while (!task && m_globalQueue.size()) {
            Database* database = m_globalQueue.first();
            m_globalQueue.removeFirst();

            Deque<RefPtr<DatabaseTask> >* databaseQueue = m_databaseTaskQueues.get(database);
            ASSERT(databaseQueue || !m_activeDatabases.contains(database));
            if (!databaseQueue)
                continue;

            ASSERT(databaseQueue->size());
            task = databaseQueue->first();
            workDatabase = database;
            databaseQueue->removeFirst();
        }
    }

    if (task) {
        ASSERT(workDatabase);
        workDatabase->resetAuthorizer();
        task->performTask(workDatabase);
        return true;
    }

    return false;
}

void DatabaseThread::scheduleTask(Database* database, DatabaseTask* task)
{
    if (m_terminationRequested) {
        LOG(StorageAPI, "Attempted to schedule task %p from database %p on DatabaseThread %p after it was requested to terminate", task, database, this);
        return;
    }

    MutexLocker locker(m_databaseWorkMutex);

    Deque<RefPtr<DatabaseTask> >* databaseQueue = 0;

    if (!m_activeDatabases.contains(database)) {
        m_activeDatabases.add(database);
        databaseQueue = new Deque<RefPtr<DatabaseTask> >;
        m_databaseTaskQueues.set(database, databaseQueue);
    }

    if (!databaseQueue)
        databaseQueue = m_databaseTaskQueues.get(database);

    ASSERT(databaseQueue);

    databaseQueue->append(task);
    m_globalQueue.append(database);

    wakeWorkThread();
}

void DatabaseThread::scheduleImmediateTask(Database* database, DatabaseTask* task)
{
    if (m_terminationRequested) {
        LOG_ERROR("Attempted to schedule immediate task %p from database %p on DatabaseThread %p after it was requested to terminate", task, database, this);
        return;
    }
    MutexLocker locker(m_databaseWorkMutex);

    Deque<RefPtr<DatabaseTask> >* databaseQueue = 0;

    if (!m_activeDatabases.contains(database)) {
        m_activeDatabases.add(database);
        databaseQueue = new Deque<RefPtr<DatabaseTask> >;
        m_databaseTaskQueues.set(database, databaseQueue);
    }

    if (!databaseQueue)
        databaseQueue = m_databaseTaskQueues.get(database);

    ASSERT(databaseQueue);

    databaseQueue->prepend(task);
    m_globalQueue.prepend(database);

    wakeWorkThread();
}

void DatabaseThread::wakeWorkThread()
{
    MutexLocker locker(m_threadMutex);
    m_threadCondition.signal();
}

} // namespace WebCore
