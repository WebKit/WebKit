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
#ifndef DatabaseTask_h
#define DatabaseTask_h

#include "DatabaseBackend.h"
#include "DatabaseBasicTypes.h"
#include "DatabaseError.h"
#include "SQLTransactionBackend.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
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
    Mutex m_synchronousMutex;
    ThreadCondition m_synchronousCondition;
#ifndef NDEBUG
    bool m_hasCheckedForTermination;
#endif
};

class DatabaseTask {
    WTF_MAKE_NONCOPYABLE(DatabaseTask); WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~DatabaseTask();

#if PLATFORM(IOS)
    virtual bool shouldPerformWhilePaused() const = 0;
#endif

    void performTask();

    DatabaseBackend* database() const { return m_database; }
#ifndef NDEBUG
    bool hasSynchronizer() const { return m_synchronizer; }
    bool hasCheckedForTermination() const { return m_synchronizer->hasCheckedForTermination(); }
#endif

protected:
    DatabaseTask(DatabaseBackend*, DatabaseTaskSynchronizer*);

private:
    virtual void doPerformTask() = 0;

    DatabaseBackend* m_database;
    DatabaseTaskSynchronizer* m_synchronizer;

#if !LOG_DISABLED
    virtual const char* debugTaskName() const = 0;
    bool m_complete;
#endif
};

class DatabaseBackend::DatabaseOpenTask : public DatabaseTask {
public:
    DatabaseOpenTask(DatabaseBackend*, bool setVersionInNewDatabase, DatabaseTaskSynchronizer*, DatabaseError&, String& errorMessage, bool& success);

#if PLATFORM(IOS)
    virtual bool shouldPerformWhilePaused() const override { return true; }
#endif

private:
    virtual void doPerformTask() override;
#if !LOG_DISABLED
    virtual const char* debugTaskName() const override;
#endif

    bool m_setVersionInNewDatabase;
    DatabaseError& m_error;
    String& m_errorMessage;
    bool& m_success;
};

class DatabaseBackend::DatabaseCloseTask : public DatabaseTask {
public:
    DatabaseCloseTask(DatabaseBackend*, DatabaseTaskSynchronizer*);

#if PLATFORM(IOS)
    virtual bool shouldPerformWhilePaused() const override { return true; }
#endif

private:
    virtual void doPerformTask() override;
#if !LOG_DISABLED
    virtual const char* debugTaskName() const override;
#endif
};

class DatabaseBackend::DatabaseTransactionTask : public DatabaseTask {
public:
    explicit DatabaseTransactionTask(PassRefPtr<SQLTransactionBackend>);
    virtual ~DatabaseTransactionTask();

#if PLATFORM(IOS)
    virtual bool shouldPerformWhilePaused() const override;
#endif

    SQLTransactionBackend* transaction() const { return m_transaction.get(); }

private:
    virtual void doPerformTask() override;
#if !LOG_DISABLED
    virtual const char* debugTaskName() const override;
#endif

    RefPtr<SQLTransactionBackend> m_transaction;
    bool m_didPerformTask;
};

class DatabaseBackend::DatabaseTableNamesTask : public DatabaseTask {
public:
    DatabaseTableNamesTask(DatabaseBackend*, DatabaseTaskSynchronizer*, Vector<String>& names);

#if PLATFORM(IOS)
    virtual bool shouldPerformWhilePaused() const override { return true; }
#endif

private:
    virtual void doPerformTask() override;
#if !LOG_DISABLED
    virtual const char* debugTaskName() const override;
#endif

    Vector<String>& m_tableNames;
};

} // namespace WebCore

#endif // DatabaseTask_h
