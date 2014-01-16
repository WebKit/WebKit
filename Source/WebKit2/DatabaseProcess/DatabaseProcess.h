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

#ifndef DatabaseProcess_h
#define DatabaseProcess_h

#if ENABLE(DATABASE_PROCESS)

#include "ChildProcess.h"
#include "UniqueIDBDatabaseIdentifier.h"
#include <wtf/NeverDestroyed.h>

class WorkQueue;

namespace WebKit {

class AsyncTask;
class DatabaseToWebProcessConnection;
class UniqueIDBDatabase;

struct DatabaseProcessCreationParameters;

class DatabaseProcess : public ChildProcess  {
    WTF_MAKE_NONCOPYABLE(DatabaseProcess);
    friend class NeverDestroyed<DatabaseProcess>;
public:
    static DatabaseProcess& shared();

    const String& indexedDatabaseDirectory() const { return m_indexedDatabaseDirectory; }

    PassRefPtr<UniqueIDBDatabase> getOrCreateUniqueIDBDatabase(const UniqueIDBDatabaseIdentifier&);
    void removeUniqueIDBDatabase(const UniqueIDBDatabase&);

    void ensureIndexedDatabaseRelativePathExists(const String&);
    String absoluteIndexedDatabasePathFromDatabaseRelativePath(const String&);

    WorkQueue& queue() { return m_queue.get(); }

    ~DatabaseProcess();

private:
    DatabaseProcess();

    // ChildProcess
    virtual void initializeProcess(const ChildProcessInitializationParameters&) override;
    virtual void initializeProcessName(const ChildProcessInitializationParameters&) override;
    virtual void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    virtual void initializeConnection(IPC::Connection*) override;
    virtual bool shouldTerminate() override;

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didClose(IPC::Connection*) override;
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // Message Handlers
    void initializeDatabaseProcess(const DatabaseProcessCreationParameters&);
    void createDatabaseToWebProcessConnection();

    void postDatabaseTask(std::unique_ptr<AsyncTask>);

    // For execution on work queue thread only
    void performNextDatabaseTask();
    void ensurePathExists(const String&);

    Vector<RefPtr<DatabaseToWebProcessConnection>> m_databaseToWebProcessConnections;

    Ref<WorkQueue> m_queue;

    String m_indexedDatabaseDirectory;

    HashMap<UniqueIDBDatabaseIdentifier, RefPtr<UniqueIDBDatabase>> m_idbDatabases;

    Deque<std::unique_ptr<AsyncTask>> m_databaseTasks;
    Mutex m_databaseTaskMutex;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)

#endif // DatabaseProcess_h
