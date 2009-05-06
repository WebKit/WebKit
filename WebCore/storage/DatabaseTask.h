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
#ifndef DatabaseTask_h
#define DatabaseTask_h

#if ENABLE(DATABASE)
#include "ExceptionCode.h"
#include "PlatformString.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

class Database;
class DatabaseThread;
class SQLValue;
class SQLCallback;
class SQLTransaction;
class VersionChangeCallback;

class DatabaseTask : public ThreadSafeShared<DatabaseTask>
{
    friend class Database;
public:
    virtual ~DatabaseTask();

    void performTask();

    Database* database() const { return m_database; }
    bool isComplete() const { return m_complete; }

protected:
    DatabaseTask(Database*);

private:
    virtual void doPerformTask() = 0;
#ifndef NDEBUG
    virtual const char* debugTaskName() const = 0;
#endif

    void lockForSynchronousScheduling();
    void waitForSynchronousCompletion();

    Database* m_database;

    bool m_complete;

    OwnPtr<Mutex> m_synchronousMutex;
    OwnPtr<ThreadCondition> m_synchronousCondition;
};

class DatabaseOpenTask : public DatabaseTask
{
public:
    static PassRefPtr<DatabaseOpenTask> create(Database* db) { return adoptRef(new DatabaseOpenTask(db)); }

    ExceptionCode exceptionCode() const { return m_code; }
    bool openSuccessful() const { return m_success; }

private:
    DatabaseOpenTask(Database*);

    virtual void doPerformTask();
#ifndef NDEBUG
    virtual const char* debugTaskName() const;
#endif

    ExceptionCode m_code;
    bool m_success;
};

class DatabaseCloseTask : public DatabaseTask
{
public:
    static PassRefPtr<DatabaseCloseTask> create(Database* db) { return adoptRef(new DatabaseCloseTask(db)); }

private:
    DatabaseCloseTask(Database*);

    virtual void doPerformTask();
#ifndef NDEBUG
    virtual const char* debugTaskName() const;
#endif
};

class DatabaseTransactionTask : public DatabaseTask
{
public:
    static PassRefPtr<DatabaseTransactionTask> create(PassRefPtr<SQLTransaction> transaction) { return adoptRef(new DatabaseTransactionTask(transaction)); }

    SQLTransaction* transaction() const { return m_transaction.get(); }

    virtual ~DatabaseTransactionTask();
private:
    DatabaseTransactionTask(PassRefPtr<SQLTransaction>);

    virtual void doPerformTask();
#ifndef NDEBUG
    virtual const char* debugTaskName() const;
#endif

    RefPtr<SQLTransaction> m_transaction;
};

class DatabaseTableNamesTask : public DatabaseTask
{
public:
    static PassRefPtr<DatabaseTableNamesTask> create(Database* db) { return adoptRef(new DatabaseTableNamesTask(db)); }

    Vector<String>& tableNames() { return m_tableNames; }

private:
    DatabaseTableNamesTask(Database*);

    virtual void doPerformTask();
#ifndef NDEBUG
    virtual const char* debugTaskName() const;
#endif

    Vector<String> m_tableNames;
};

} // namespace WebCore

#endif // ENABLE(DATABASE)
#endif // DatabaseTask_h
