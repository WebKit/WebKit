/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DatabaseProcess.h"

#if ENABLE(DATABASE_PROCESS)

#include "AsyncTask.h"
#include "DatabaseProcessCreationParameters.h"
#include "DatabaseProcessProxyMessages.h"
#include "DatabaseToWebProcessConnection.h"
#include "UniqueIDBDatabase.h"
#include <WebCore/FileSystem.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

DatabaseProcess& DatabaseProcess::shared()
{
    static NeverDestroyed<DatabaseProcess> databaseProcess;
    return databaseProcess;
}

DatabaseProcess::DatabaseProcess()
    : m_queue(adoptRef(*WorkQueue::create("com.apple.WebKit.DatabaseProcess").leakRef()))
{
}

DatabaseProcess::~DatabaseProcess()
{
}

void DatabaseProcess::initializeConnection(IPC::Connection* connection)
{
    ChildProcess::initializeConnection(connection);
}

bool DatabaseProcess::shouldTerminate()
{
    return true;
}

void DatabaseProcess::didClose(IPC::Connection*)
{
    RunLoop::current().stop();
}

void DatabaseProcess::didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference, IPC::StringReference)
{
    RunLoop::current().stop();
}

PassRefPtr<UniqueIDBDatabase> DatabaseProcess::getOrCreateUniqueIDBDatabase(const UniqueIDBDatabaseIdentifier& identifier)
{
    auto addResult = m_idbDatabases.add(identifier, nullptr);

    if (!addResult.isNewEntry)
        return addResult.iterator->value;

    RefPtr<UniqueIDBDatabase> database = UniqueIDBDatabase::create(identifier);
    addResult.iterator->value = database.get();
    return database.release();
}

void DatabaseProcess::removeUniqueIDBDatabase(const UniqueIDBDatabase& database)
{
    const UniqueIDBDatabaseIdentifier& identifier = database.identifier();
    ASSERT(m_idbDatabases.contains(identifier));

    m_idbDatabases.remove(identifier);
}

void DatabaseProcess::initializeDatabaseProcess(const DatabaseProcessCreationParameters& parameters)
{
    m_indexedDatabaseDirectory = parameters.indexedDatabaseDirectory;

    ensureIndexedDatabaseRelativePathExists(StringImpl::empty());
}

void DatabaseProcess::ensureIndexedDatabaseRelativePathExists(const String& relativePath)
{
    postDatabaseTask(createAsyncTask(*this, &DatabaseProcess::ensurePathExists, absoluteIndexedDatabasePathFromDatabaseRelativePath(relativePath)));
}

void DatabaseProcess::ensurePathExists(const String& path)
{
    ASSERT(!RunLoop::isMain());

    if (!makeAllDirectories(path))
        LOG_ERROR("Failed to make all directories for path '%s'", path.utf8().data());
}

String DatabaseProcess::absoluteIndexedDatabasePathFromDatabaseRelativePath(const String& relativePath)
{
    // FIXME: pathByAppendingComponent() was originally designed to append individual atomic components.
    // We don't have a function designed to append a multi-component subpath, but we should.
    return pathByAppendingComponent(m_indexedDatabaseDirectory, relativePath);
}

void DatabaseProcess::postDatabaseTask(std::unique_ptr<AsyncTask> task)
{
    ASSERT(RunLoop::isMain());

    MutexLocker locker(m_databaseTaskMutex);

    m_databaseTasks.append(std::move(task));

    m_queue->dispatch(bind(&DatabaseProcess::performNextDatabaseTask, this));
}

void DatabaseProcess::performNextDatabaseTask()
{
    ASSERT(!RunLoop::isMain());

    std::unique_ptr<AsyncTask> task;
    {
        MutexLocker locker(m_databaseTaskMutex);
        ASSERT(!m_databaseTasks.isEmpty());
        task = m_databaseTasks.takeFirst();
    }

    task->performTask();
}

void DatabaseProcess::createDatabaseToWebProcessConnection()
{
#if OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    RefPtr<DatabaseToWebProcessConnection> connection = DatabaseToWebProcessConnection::create(IPC::Connection::Identifier(listeningPort));
    m_databaseToWebProcessConnections.append(connection.release());

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidCreateDatabaseToWebProcessConnection(clientPort), 0);
#else
    notImplemented();
#endif
}

#if !PLATFORM(COCOA)
void DatabaseProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void DatabaseProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void DatabaseProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
