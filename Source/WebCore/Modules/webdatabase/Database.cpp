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
#include "Database.h"

#include "ChangeVersionData.h"
#include "ChangeVersionWrapper.h"
#include "DatabaseCallback.h"
#include "DatabaseContext.h"
#include "DatabaseManager.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "JSDOMWindow.h"
#include "Logging.h"
#include "Page.h"
#include "SQLError.h"
#include "SQLTransaction.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionErrorCallback.h"
#include "SQLiteStatement.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "VoidCallback.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS)
#include "SQLiteDatabaseTracker.h"
#endif

namespace WebCore {

PassRefPtr<Database> Database::create(ScriptExecutionContext*, PassRefPtr<DatabaseBackendBase> backend)
{
    // FIXME: Currently, we're only simulating the backend by return the
    // frontend database as its own the backend. When we split the 2 apart,
    // this create() function should be changed to be a factory method for
    // instantiating the backend.
    return static_cast<Database*>(backend.get());
}

Database::Database(PassRefPtr<DatabaseContext> databaseContext, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBackend(databaseContext.get(), name, expectedVersion, displayName, estimatedSize)
    , m_scriptExecutionContext(databaseContext->scriptExecutionContext())
    , m_databaseContext(databaseContext)
    , m_deleted(false)
{
    m_databaseThreadSecurityOrigin = m_contextThreadSecurityOrigin->isolatedCopy();
    setFrontend(this);

    ASSERT(m_databaseContext->databaseThread());
}

Database::~Database()
{
    // The reference to the ScriptExecutionContext needs to be cleared on the JavaScript thread.  If we're on that thread already, we can just let the RefPtr's destruction do the dereffing.
    if (!m_scriptExecutionContext->isContextThread()) {
        // Grab a pointer to the script execution here because we're releasing it when we pass it to
        // DerefContextTask::create.
        PassRefPtr<ScriptExecutionContext> passedContext = m_scriptExecutionContext.release();
        passedContext->postTask({ScriptExecutionContext::Task::CleanupTask, [passedContext] (ScriptExecutionContext& context) {
            ASSERT_UNUSED(context, &context == passedContext);
            RefPtr<ScriptExecutionContext> scriptExecutionContext(passedContext);
        }});
    }
}

bool Database::openAndVerifyVersion(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    DatabaseTaskSynchronizer synchronizer;
    if (!databaseContext()->databaseThread() || databaseContext()->databaseThread()->terminationRequested(&synchronizer))
        return false;

    bool success = false;
    auto task = std::make_unique<DatabaseOpenTask>(this, setVersionInNewDatabase, &synchronizer, error, errorMessage, success);
    databaseContext()->databaseThread()->scheduleImmediateTask(WTF::move(task));
    synchronizer.waitForTaskCompletion();

    return success;
}

void Database::close()
{
    ASSERT(databaseContext()->databaseThread());
    ASSERT(currentThread() == databaseContext()->databaseThread()->getThreadID());

    {
        MutexLocker locker(m_transactionInProgressMutex);

        // Clean up transactions that have not been scheduled yet:
        // Transaction phase 1 cleanup. See comment on "What happens if a
        // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
        RefPtr<SQLTransactionBackend> transaction;
        while (!m_transactionQueue.isEmpty()) {
            transaction = m_transactionQueue.takeFirst();
            transaction->notifyDatabaseThreadIsShuttingDown();
        }

        m_isTransactionQueueEnabled = false;
        m_transactionInProgress = false;
    }

    closeDatabase();

    // DatabaseThread keeps databases alive by referencing them in its
    // m_openDatabaseSet. DatabaseThread::recordDatabaseClose() will remove
    // this database from that set (which effectively deref's it). We hold on
    // to it with a local pointer here for a liitle longer, so that we can
    // unschedule any DatabaseTasks that refer to it before the database gets
    // deleted.
    Ref<DatabaseBackend> protect(*this);
    databaseContext()->databaseThread()->recordDatabaseClosed(this);
    databaseContext()->databaseThread()->unscheduleDatabaseTasks(this);
}

bool Database::performOpenAndVerify(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    if (DatabaseBackendBase::performOpenAndVerify(setVersionInNewDatabase, error, errorMessage)) {
        if (databaseContext()->databaseThread())
            databaseContext()->databaseThread()->recordDatabaseOpen(this);

        return true;
    }

    return false;
}

void Database::scheduleTransaction()
{
    ASSERT(!m_transactionInProgressMutex.tryLock()); // Locked by caller.
    RefPtr<SQLTransactionBackend> transaction;

    if (m_isTransactionQueueEnabled && !m_transactionQueue.isEmpty())
        transaction = m_transactionQueue.takeFirst();

    if (transaction && databaseContext()->databaseThread()) {
        auto task = std::make_unique<DatabaseTransactionTask>(transaction);
        LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for transaction %p\n", task.get(), task->transaction());
        m_transactionInProgress = true;
        databaseContext()->databaseThread()->scheduleTask(WTF::move(task));
    } else
        m_transactionInProgress = false;
}

PassRefPtr<SQLTransactionBackend> Database::runTransaction(PassRefPtr<SQLTransaction> transaction, bool readOnly, const ChangeVersionData* data)
{
    MutexLocker locker(m_transactionInProgressMutex);
    if (!m_isTransactionQueueEnabled)
        return 0;

    RefPtr<SQLTransactionWrapper> wrapper;
    if (data)
        wrapper = ChangeVersionWrapper::create(data->oldVersion(), data->newVersion());

    RefPtr<SQLTransactionBackend> transactionBackend = SQLTransactionBackend::create(this, transaction, wrapper, readOnly);
    m_transactionQueue.append(transactionBackend);
    if (!m_transactionInProgress)
        scheduleTransaction();

    return transactionBackend;
}

void Database::scheduleTransactionStep(SQLTransactionBackend* transaction)
{
    if (!databaseContext()->databaseThread())
        return;

    auto task = std::make_unique<DatabaseTransactionTask>(transaction);
    LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for the transaction step\n", task.get());
    databaseContext()->databaseThread()->scheduleTask(WTF::move(task));
}

void Database::inProgressTransactionCompleted()
{
    MutexLocker locker(m_transactionInProgressMutex);
    m_transactionInProgress = false;
    scheduleTransaction();
}

bool Database::hasPendingTransaction()
{
    MutexLocker locker(m_transactionInProgressMutex);
    return m_transactionInProgress || !m_transactionQueue.isEmpty();
}

SQLTransactionClient* Database::transactionClient() const
{
    return databaseContext()->databaseThread()->transactionClient();
}

SQLTransactionCoordinator* Database::transactionCoordinator() const
{
    return databaseContext()->databaseThread()->transactionCoordinator();
}

Database* Database::from(DatabaseBackend* backend)
{
    return static_cast<Database*>(backend->m_frontend);
}

PassRefPtr<DatabaseBackend> Database::backend()
{
    return this;
}

String Database::version() const
{
    if (m_deleted)
        return String();
    return DatabaseBackendBase::version();
}

void Database::markAsDeletedAndClose()
{
    if (m_deleted || !databaseContext()->databaseThread())
        return;

    LOG(StorageAPI, "Marking %s (%p) as deleted", stringIdentifier().ascii().data(), this);
    m_deleted = true;

    DatabaseTaskSynchronizer synchronizer;
    if (databaseContext()->databaseThread()->terminationRequested(&synchronizer)) {
        LOG(StorageAPI, "Database handle %p is on a terminated DatabaseThread, cannot be marked for normal closure\n", this);
        return;
    }

    auto task = std::make_unique<DatabaseCloseTask>(this, &synchronizer);
    databaseContext()->databaseThread()->scheduleImmediateTask(WTF::move(task));
    synchronizer.waitForTaskCompletion();
}

void Database::closeImmediately()
{
    ASSERT(m_scriptExecutionContext->isContextThread());
    DatabaseThread* databaseThread = databaseContext()->databaseThread();
    if (databaseThread && !databaseThread->terminationRequested() && opened()) {
        logErrorMessage("forcibly closing database");
        auto task = std::make_unique<DatabaseCloseTask>(this, nullptr);
        databaseThread->scheduleImmediateTask(WTF::move(task));
    }
}

void Database::changeVersion(const String& oldVersion, const String& newVersion,
                             PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                             PassRefPtr<VoidCallback> successCallback)
{
    ChangeVersionData data(oldVersion, newVersion);
    runTransaction(callback, errorCallback, successCallback, false, &data);
}

void Database::transaction(PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback, PassRefPtr<VoidCallback> successCallback)
{
    runTransaction(callback, errorCallback, successCallback, false);
}

void Database::readTransaction(PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback, PassRefPtr<VoidCallback> successCallback)
{
    runTransaction(callback, errorCallback, successCallback, true);
}

void Database::runTransaction(RefPtr<SQLTransactionCallback>&& callback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<VoidCallback>&& successCallback, bool readOnly, const ChangeVersionData* changeVersionData)
{
    RefPtr<SQLTransaction> transaction = SQLTransaction::create(*this, WTF::move(callback), WTF::move(successCallback), errorCallback.copyRef(), readOnly);

    RefPtr<SQLTransactionBackend> transactionBackend = runTransaction(transaction.release(), readOnly, changeVersionData);
    if (!transactionBackend && errorCallback) {
        WTF::RefPtr<SQLTransactionErrorCallback> errorCallbackProtector = WTF::move(errorCallback);
        m_scriptExecutionContext->postTask([errorCallbackProtector](ScriptExecutionContext&) {
            errorCallbackProtector->handleEvent(SQLError::create(SQLError::UNKNOWN_ERR, "database has been closed").ptr());
        });
    }
}

void Database::scheduleTransactionCallback(SQLTransaction* transaction)
{
    RefPtr<SQLTransaction> transactionProtector(transaction);
    m_scriptExecutionContext->postTask([transactionProtector] (ScriptExecutionContext&) {
        transactionProtector->performPendingCallback();
    });
}

Vector<String> Database::performGetTableNames()
{
    disableAuthorizer();

    SQLiteStatement statement(sqliteDatabase(), "SELECT name FROM sqlite_master WHERE type='table';");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to retrieve list of tables for database %s", databaseDebugName().ascii().data());
        enableAuthorizer();
        return Vector<String>();
    }

    Vector<String> tableNames;
    int result;
    while ((result = statement.step()) == SQLITE_ROW) {
        String name = statement.getColumnText(0);
        if (name != databaseInfoTableName())
            tableNames.append(name);
    }

    enableAuthorizer();

    if (result != SQLITE_DONE) {
        LOG_ERROR("Error getting tables for database %s", databaseDebugName().ascii().data());
        return Vector<String>();
    }

    return tableNames;
}

void Database::logErrorMessage(const String& message)
{
    m_scriptExecutionContext->addConsoleMessage(MessageSource::Storage, MessageLevel::Error, message);
}

Vector<String> Database::tableNames()
{
    // FIXME: Not using isolatedCopy on these strings looks ok since threads take strict turns
    // in dealing with them. However, if the code changes, this may not be true anymore.
    Vector<String> result;
    DatabaseTaskSynchronizer synchronizer;
    if (!databaseContext()->databaseThread() || databaseContext()->databaseThread()->terminationRequested(&synchronizer))
        return result;

    auto task = std::make_unique<DatabaseTableNamesTask>(this, &synchronizer, result);
    databaseContext()->databaseThread()->scheduleImmediateTask(WTF::move(task));
    synchronizer.waitForTaskCompletion();

    return result;
}

SecurityOrigin* Database::securityOrigin() const
{
    if (m_scriptExecutionContext->isContextThread())
        return m_contextThreadSecurityOrigin.get();
    if (currentThread() == databaseContext()->databaseThread()->getThreadID())
        return m_databaseThreadSecurityOrigin.get();
    return 0;
}

} // namespace WebCore
