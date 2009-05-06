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
#include "DatabaseTask.h"

#if ENABLE(DATABASE)

#include "Database.h"
#include "Logging.h"

namespace WebCore {

DatabaseTask::DatabaseTask(Database* database)
    : m_database(database)
    , m_complete(false)
{
}

DatabaseTask::~DatabaseTask()
{
}

void DatabaseTask::performTask()
{
    // Database tasks are meant to be used only once, so make sure this one hasn't been performed before.
    ASSERT(!m_complete);

    LOG(StorageAPI, "Performing %s %p\n", debugTaskName(), this);

    m_database->resetAuthorizer();
    doPerformTask();
    m_database->performPolicyChecks();

    if (m_synchronousMutex)
        m_synchronousMutex->lock();

    m_complete = true;

    if (m_synchronousMutex) {
        m_synchronousCondition->signal();
        m_synchronousMutex->unlock();
    }
}

void DatabaseTask::lockForSynchronousScheduling()
{
    // Called from main thread.
    ASSERT(!m_synchronousMutex);
    ASSERT(!m_synchronousCondition);
    m_synchronousMutex.set(new Mutex);
    m_synchronousCondition.set(new ThreadCondition);
}

void DatabaseTask::waitForSynchronousCompletion()
{
    // Called from main thread.
    m_synchronousMutex->lock();
    if (!m_complete)
        m_synchronousCondition->wait(*m_synchronousMutex);
    m_synchronousMutex->unlock();
}

// *** DatabaseOpenTask ***
// Opens the database file and verifies the version matches the expected version.

DatabaseOpenTask::DatabaseOpenTask(Database* database)
    : DatabaseTask(database)
    , m_code(0)
    , m_success(false)
{
}

void DatabaseOpenTask::doPerformTask()
{
    m_success = database()->performOpenAndVerify(m_code);
}

#ifndef NDEBUG
const char* DatabaseOpenTask::debugTaskName() const
{
    return "DatabaseOpenTask";
}
#endif

// *** DatabaseCloseTask ***
// Closes the database.

DatabaseCloseTask::DatabaseCloseTask(Database* database)
    : DatabaseTask(database)
{
}

void DatabaseCloseTask::doPerformTask()
{
    database()->close();
}

#ifndef NDEBUG
const char* DatabaseCloseTask::debugTaskName() const
{
    return "DatabaseCloseTask";
}
#endif

// *** DatabaseTransactionTask ***
// Starts a transaction that will report its results via a callback.

DatabaseTransactionTask::DatabaseTransactionTask(PassRefPtr<SQLTransaction> transaction)
    : DatabaseTask(transaction->database())
    , m_transaction(transaction)
{
}

DatabaseTransactionTask::~DatabaseTransactionTask()
{
}

void DatabaseTransactionTask::doPerformTask()
{
    if (m_transaction->performNextStep()) {
        // The transaction is complete, we can move on to the next one.
        MutexLocker locker(m_transaction->database()->m_transactionInProgressMutex);
        m_transaction->database()->scheduleTransaction();
    }
}

#ifndef NDEBUG
const char* DatabaseTransactionTask::debugTaskName() const
{
    return "DatabaseTransactionTask";
}
#endif

// *** DatabaseTableNamesTask ***
// Retrieves a list of all tables in the database - for WebInspector support.

DatabaseTableNamesTask::DatabaseTableNamesTask(Database* database)
    : DatabaseTask(database)
{
}

void DatabaseTableNamesTask::doPerformTask()
{
    m_tableNames = database()->performGetTableNames();
}

#ifndef NDEBUG
const char* DatabaseTableNamesTask::debugTaskName() const
{
    return "DatabaseTableNamesTask";
}
#endif

} // namespace WebCore

#endif
