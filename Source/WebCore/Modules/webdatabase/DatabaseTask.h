/*
 * Copyright (C) 2007, 2008, 2013, 2015 Apple Inc. All rights reserved.
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
#ifndef DatabaseTask_h
#define DatabaseTask_h

#include "DatabaseBasicTypes.h"
#include "DatabaseError.h"
#include "SQLTransactionBackend.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Can be used to wait until DatabaseTask is completed.
// Has to be passed into DatabaseTask::create to be associated with the task.
class DatabaseTaskSynchronizer {
    WTF_MAKE_NONCOPYABLE(DatabaseTaskSynchronizer);
public:
    DatabaseTaskSynchronizer();

    // Called from main thread to wait until task is completed.
    void waitForTaskCompletion();

    // Called by the task.
    void taskCompleted();

#ifndef NDEBUG
    bool hasCheckedForTermination() const { return m_hasCheckedForTermination; }
    void setHasCheckedForTermination() { m_hasCheckedForTermination = true; }
#endif

private:
    bool m_taskCompleted;
    Lock m_synchronousMutex;
    Condition m_synchronousCondition;
#ifndef NDEBUG
    bool m_hasCheckedForTermination;
#endif
};

class DatabaseTask {
    WTF_MAKE_NONCOPYABLE(DatabaseTask); WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~DatabaseTask();

    void performTask();

    Database& database() const { return m_database; }
#ifndef NDEBUG
    bool hasSynchronizer() const { return m_synchronizer; }
    bool hasCheckedForTermination() const { return m_synchronizer->hasCheckedForTermination(); }
#endif

protected:
    DatabaseTask(Database&, DatabaseTaskSynchronizer*);

private:
    virtual void doPerformTask() = 0;

    Database& m_database;
    DatabaseTaskSynchronizer* m_synchronizer;

#if !LOG_DISABLED
    virtual const char* debugTaskName() const = 0;
    bool m_complete;
#endif
};

class DatabaseOpenTask : public DatabaseTask {
public:
    DatabaseOpenTask(Database&, bool setVersionInNewDatabase, DatabaseTaskSynchronizer&, DatabaseError&, String& errorMessage, bool& success);

private:
    void doPerformTask() override;
#if !LOG_DISABLED
    const char* debugTaskName() const override;
#endif

    bool m_setVersionInNewDatabase;
    DatabaseError& m_error;
    String& m_errorMessage;
    bool& m_success;
};

class DatabaseCloseTask : public DatabaseTask {
public:
    DatabaseCloseTask(Database&, DatabaseTaskSynchronizer&);

private:
    void doPerformTask() override;
#if !LOG_DISABLED
    const char* debugTaskName() const override;
#endif
};

class DatabaseTransactionTask : public DatabaseTask {
public:
    explicit DatabaseTransactionTask(RefPtr<SQLTransaction>&&);
    virtual ~DatabaseTransactionTask();

    SQLTransaction* transaction() const { return m_transaction.get(); }

private:
    void doPerformTask() override;
#if !LOG_DISABLED
    const char* debugTaskName() const override;
#endif

    RefPtr<SQLTransaction> m_transaction;
    bool m_didPerformTask;
};

class DatabaseTableNamesTask : public DatabaseTask {
public:
    DatabaseTableNamesTask(Database&, DatabaseTaskSynchronizer&, Vector<String>& names);

private:
    void doPerformTask() override;
#if !LOG_DISABLED
    const char* debugTaskName() const override;
#endif

    Vector<String>& m_tableNames;
};

} // namespace WebCore

#endif // DatabaseTask_h
