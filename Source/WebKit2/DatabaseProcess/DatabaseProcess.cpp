/*
 * Copyright (C) 2013, 2014, 2015, 2016 Apple Inc. All rights reserved.
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

#include "DatabaseProcessCreationParameters.h"
#include "DatabaseProcessMessages.h"
#include "DatabaseProcessProxyMessages.h"
#include "DatabaseToWebProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebsiteData.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SessionID.h>
#include <WebCore/TextEncoding.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

DatabaseProcess& DatabaseProcess::singleton()
{
    static NeverDestroyed<DatabaseProcess> databaseProcess;
    return databaseProcess;
}

DatabaseProcess::DatabaseProcess()
    : m_queue(WorkQueue::create("com.apple.WebKit.DatabaseProcess"))
{
    // Make sure the UTF8Encoding encoding and the text encoding maps have been built on the main thread before a background thread needs it.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=135365 - Need a more explicit way of doing this besides accessing the UTF8Encoding.
    UTF8Encoding();
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

void DatabaseProcess::didClose(IPC::Connection&)
{
    stopRunLoop();
}

void DatabaseProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::DatabaseProcess::messageReceiverName()) {
        didReceiveDatabaseProcessMessage(connection, decoder);
        return;
    }
}

void DatabaseProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
    stopRunLoop();
}

#if ENABLE(INDEXED_DATABASE)
IDBServer::IDBServer& DatabaseProcess::idbServer()
{
    if (!m_idbServer)
        m_idbServer = IDBServer::IDBServer::create(indexedDatabaseDirectory(), DatabaseProcess::singleton());

    return *m_idbServer;
}
#endif

void DatabaseProcess::initializeDatabaseProcess(const DatabaseProcessCreationParameters& parameters)
{
#if ENABLE(INDEXED_DATABASE)
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********

    m_indexedDatabaseDirectory = parameters.indexedDatabaseDirectory;
    SandboxExtension::consumePermanently(parameters.indexedDatabaseDirectoryExtensionHandle);

    ensureIndexedDatabaseRelativePathExists(StringImpl::empty());
#endif
}

#if ENABLE(INDEXED_DATABASE)
void DatabaseProcess::ensureIndexedDatabaseRelativePathExists(const String& relativePath)
{
    postDatabaseTask(createCrossThreadTask(*this, &DatabaseProcess::ensurePathExists, absoluteIndexedDatabasePathFromDatabaseRelativePath(relativePath)));
}
#endif

void DatabaseProcess::ensurePathExists(const String& path)
{
    ASSERT(!RunLoop::isMain());

    if (!makeAllDirectories(path))
        LOG_ERROR("Failed to make all directories for path '%s'", path.utf8().data());
}

#if ENABLE(INDEXED_DATABASE)
String DatabaseProcess::absoluteIndexedDatabasePathFromDatabaseRelativePath(const String& relativePath)
{
    // FIXME: pathByAppendingComponent() was originally designed to append individual atomic components.
    // We don't have a function designed to append a multi-component subpath, but we should.
    return pathByAppendingComponent(m_indexedDatabaseDirectory, relativePath);
}
#endif

void DatabaseProcess::postDatabaseTask(CrossThreadTask&& task)
{
    ASSERT(RunLoop::isMain());

    LockHolder locker(m_databaseTaskMutex);

    m_databaseTasks.append(WTFMove(task));

    m_queue->dispatch([this] {
        performNextDatabaseTask();
    });
}

void DatabaseProcess::performNextDatabaseTask()
{
    ASSERT(!RunLoop::isMain());

    CrossThreadTask task;
    {
        LockHolder locker(m_databaseTaskMutex);
        ASSERT(!m_databaseTasks.isEmpty());
        task = m_databaseTasks.takeFirst();
    }

    task.performTask();
}

void DatabaseProcess::createDatabaseToWebProcessConnection()
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();
    m_databaseToWebProcessConnections.append(DatabaseToWebProcessConnection::create(socketPair.server));
    parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidCreateDatabaseToWebProcessConnection(IPC::Attachment(socketPair.client)), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    auto connection = DatabaseToWebProcessConnection::create(IPC::Connection::Identifier(listeningPort));
    m_databaseToWebProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidCreateDatabaseToWebProcessConnection(clientPort), 0);
#else
    notImplemented();
#endif
}

void DatabaseProcess::fetchWebsiteData(SessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID)
{
#if ENABLE(INDEXED_DATABASE)
    auto completionHandler = [this, callbackID](const WebsiteData& websiteData) {
        parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
    };

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postDatabaseTask(CrossThreadTask([this, websiteDataTypes, completionHandler = WTFMove(completionHandler)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), securityOrigins = indexedDatabaseOrigins()] {
                WebsiteData websiteData;
                for (const auto& securityOrigin : securityOrigins)
                    websiteData.entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });

                completionHandler(websiteData);
            });
        }));
    }
#endif
}

void DatabaseProcess::deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
{
#if ENABLE(INDEXED_DATABASE)
    auto completionHandler = [this, callbackID]() {
        parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    };

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases))
        idbServer().closeAndDeleteDatabasesModifiedSince(modifiedSince, WTFMove(completionHandler));
#endif
}

void DatabaseProcess::deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& securityOriginDatas, uint64_t callbackID)
{
#if ENABLE(INDEXED_DATABASE)
    auto completionHandler = [this, callbackID]() {
        parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    };

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases))
        idbServer().closeAndDeleteDatabasesForOrigins(securityOriginDatas, WTFMove(completionHandler));
#endif
}

#if ENABLE(SANDBOX_EXTENSIONS)
void DatabaseProcess::grantSandboxExtensionsForBlobs(const Vector<String>& paths, const SandboxExtension::HandleArray& handles)
{
    ASSERT(paths.size() == handles.size());

    for (size_t i = 0; i < paths.size(); ++i) {
        auto result = m_blobTemporaryFileSandboxExtensions.add(paths[i], SandboxExtension::create(handles[i]));
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}
#endif

#if ENABLE(INDEXED_DATABASE)
void DatabaseProcess::prepareForAccessToTemporaryFile(const String& path)
{
    if (auto extension = m_blobTemporaryFileSandboxExtensions.get(path))
        extension->consume();
}

void DatabaseProcess::accessToTemporaryFileComplete(const String& path)
{
    // We've either hard linked the temporary blob file to the database directory, copied it there,
    // or the transaction is being aborted.
    // In any of those cases, we can delete the temporary blob file now.
    deleteFile(path);

    if (auto extension = m_blobTemporaryFileSandboxExtensions.take(path))
        extension->revoke();
}

Vector<RefPtr<WebCore::SecurityOrigin>> DatabaseProcess::indexedDatabaseOrigins()
{
    if (m_indexedDatabaseDirectory.isEmpty())
        return { };

    Vector<RefPtr<WebCore::SecurityOrigin>> securityOrigins;
    for (auto& originPath : listDirectory(m_indexedDatabaseDirectory, "*")) {
        String databaseIdentifier = pathGetFileName(originPath);

        if (auto securityOrigin = SecurityOrigin::maybeCreateFromDatabaseIdentifier(databaseIdentifier))
            securityOrigins.append(WTFMove(securityOrigin));
    }

    return securityOrigins;
}

#endif

#if ENABLE(SANDBOX_EXTENSIONS)
void DatabaseProcess::getSandboxExtensionsForBlobFiles(const Vector<String>& filenames, std::function<void (SandboxExtension::HandleArray&&)> completionHandler)
{
    static uint64_t lastRequestID;

    uint64_t requestID = ++lastRequestID;
    m_sandboxExtensionForBlobsCompletionHandlers.set(requestID, completionHandler);
    parentProcessConnection()->send(Messages::DatabaseProcessProxy::GetSandboxExtensionsForBlobFiles(requestID, filenames), 0);
}

void DatabaseProcess::didGetSandboxExtensionsForBlobFiles(uint64_t requestID, SandboxExtension::HandleArray&& handles)
{
    if (auto handler = m_sandboxExtensionForBlobsCompletionHandlers.take(requestID))
        handler(WTFMove(handles));
}
#endif

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
