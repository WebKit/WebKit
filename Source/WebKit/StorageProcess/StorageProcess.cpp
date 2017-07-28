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
#include "StorageProcess.h"

#include "StorageProcessCreationParameters.h"
#include "StorageProcessMessages.h"
#include "StorageProcessProxyMessages.h"
#include "StorageToWebProcessConnection.h"
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

StorageProcess& StorageProcess::singleton()
{
    static NeverDestroyed<StorageProcess> process;
    return process;
}

StorageProcess::StorageProcess()
    : m_queue(WorkQueue::create("com.apple.WebKit.StorageProcess"))
{
    // Make sure the UTF8Encoding encoding and the text encoding maps have been built on the main thread before a background thread needs it.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=135365 - Need a more explicit way of doing this besides accessing the UTF8Encoding.
    UTF8Encoding();
}

StorageProcess::~StorageProcess()
{
}

void StorageProcess::initializeConnection(IPC::Connection* connection)
{
    ChildProcess::initializeConnection(connection);
}

bool StorageProcess::shouldTerminate()
{
    return true;
}

void StorageProcess::didClose(IPC::Connection&)
{
    stopRunLoop();
}

void StorageProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::StorageProcess::messageReceiverName()) {
        didReceiveStorageProcessMessage(connection, decoder);
        return;
    }
}

#if ENABLE(INDEXED_DATABASE)
IDBServer::IDBServer& StorageProcess::idbServer(SessionID sessionID)
{
    auto addResult = m_idbServers.add(sessionID, nullptr);
    if (!addResult.isNewEntry) {
        ASSERT(addResult.iterator->value);
        return *addResult.iterator->value;
    }

    auto path = m_idbDatabasePaths.get(sessionID);
    // There should already be a registered path for this SessionID.
    // If there's not, then where did this SessionID come from?
    ASSERT(!path.isEmpty());

    addResult.iterator->value = IDBServer::IDBServer::create(path, StorageProcess::singleton());
    return *addResult.iterator->value;
}
#endif

void StorageProcess::initializeWebsiteDataStore(const StorageProcessCreationParameters& parameters)
{
#if ENABLE(INDEXED_DATABASE)
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********

    auto addResult = m_idbDatabasePaths.add(parameters.sessionID, String());
    if (!addResult.isNewEntry)
        return;

    addResult.iterator->value = parameters.indexedDatabaseDirectory;
    SandboxExtension::consumePermanently(parameters.indexedDatabaseDirectoryExtensionHandle);

    postStorageTask(createCrossThreadTask(*this, &StorageProcess::ensurePathExists, parameters.indexedDatabaseDirectory));
#endif
}

void StorageProcess::ensurePathExists(const String& path)
{
    ASSERT(!RunLoop::isMain());

    if (!makeAllDirectories(path))
        LOG_ERROR("Failed to make all directories for path '%s'", path.utf8().data());
}

void StorageProcess::postStorageTask(CrossThreadTask&& task)
{
    ASSERT(RunLoop::isMain());

    LockHolder locker(m_storageTaskMutex);

    m_storageTasks.append(WTFMove(task));

    m_queue->dispatch([this] {
        performNextStorageTask();
    });
}

void StorageProcess::performNextStorageTask()
{
    ASSERT(!RunLoop::isMain());

    CrossThreadTask task;
    {
        LockHolder locker(m_storageTaskMutex);
        ASSERT(!m_storageTasks.isEmpty());
        task = m_storageTasks.takeFirst();
    }

    task.performTask();
}

void StorageProcess::createStorageToWebProcessConnection()
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();
    m_databaseToWebProcessConnections.append(StorageToWebProcessConnection::create(socketPair.server));
    parentProcessConnection()->send(Messages::StorageProcessProxy::DidCreateStorageToWebProcessConnection(IPC::Attachment(socketPair.client)), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    m_databaseToWebProcessConnections.append(StorageToWebProcessConnection::create(IPC::Connection::Identifier(listeningPort)));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::StorageProcessProxy::DidCreateStorageToWebProcessConnection(clientPort), 0);
#else
    notImplemented();
#endif
}

void StorageProcess::fetchWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID)
{
#if ENABLE(INDEXED_DATABASE)
    auto completionHandler = [this, callbackID](const WebsiteData& websiteData) {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
    };

    String path = m_idbDatabasePaths.get(sessionID);
    if (path.isEmpty() || !websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        completionHandler({ });
        return;
    }

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, completionHandler = WTFMove(completionHandler), path = WTFMove(path)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), securityOrigins = indexedDatabaseOrigins(path)] {
                WebsiteData websiteData;
                for (const auto& securityOrigin : securityOrigins)
                    websiteData.entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });

                completionHandler(websiteData);
            });
        }));
    }
#endif
}

void StorageProcess::deleteWebsiteData(WebCore::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
{
#if ENABLE(INDEXED_DATABASE)
    auto completionHandler = [this, callbackID]() {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    };

    if (!websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        completionHandler();
        return;
    }

    idbServer(sessionID).closeAndDeleteDatabasesModifiedSince(modifiedSince, WTFMove(completionHandler));
#endif
}

void StorageProcess::deleteWebsiteDataForOrigins(WebCore::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& securityOriginDatas, uint64_t callbackID)
{
#if ENABLE(INDEXED_DATABASE)
    auto completionHandler = [this, callbackID]() {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    };

    if (!websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        completionHandler();
        return;
    }

    idbServer(sessionID).closeAndDeleteDatabasesForOrigins(securityOriginDatas, WTFMove(completionHandler));
#endif
}

#if ENABLE(SANDBOX_EXTENSIONS)
void StorageProcess::grantSandboxExtensionsForBlobs(const Vector<String>& paths, const SandboxExtension::HandleArray& handles)
{
    ASSERT(paths.size() == handles.size());

    for (size_t i = 0; i < paths.size(); ++i) {
        auto result = m_blobTemporaryFileSandboxExtensions.add(paths[i], SandboxExtension::create(handles[i]));
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}
#endif

#if ENABLE(INDEXED_DATABASE)
void StorageProcess::prepareForAccessToTemporaryFile(const String& path)
{
    if (auto extension = m_blobTemporaryFileSandboxExtensions.get(path))
        extension->consume();
}

void StorageProcess::accessToTemporaryFileComplete(const String& path)
{
    // We've either hard linked the temporary blob file to the database directory, copied it there,
    // or the transaction is being aborted.
    // In any of those cases, we can delete the temporary blob file now.
    deleteFile(path);

    if (auto extension = m_blobTemporaryFileSandboxExtensions.take(path))
        extension->revoke();
}

Vector<WebCore::SecurityOriginData> StorageProcess::indexedDatabaseOrigins(const String& path)
{
    if (path.isEmpty())
        return { };

    Vector<WebCore::SecurityOriginData> securityOrigins;
    for (auto& originPath : listDirectory(path, "*")) {
        String databaseIdentifier = pathGetFileName(originPath);

        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            securityOrigins.append(WTFMove(*securityOrigin));
    }

    return securityOrigins;
}

#endif

#if ENABLE(SANDBOX_EXTENSIONS)
void StorageProcess::getSandboxExtensionsForBlobFiles(const Vector<String>& filenames, WTF::Function<void (SandboxExtension::HandleArray&&)>&& completionHandler)
{
    static uint64_t lastRequestID;

    uint64_t requestID = ++lastRequestID;
    m_sandboxExtensionForBlobsCompletionHandlers.set(requestID, WTFMove(completionHandler));
    parentProcessConnection()->send(Messages::StorageProcessProxy::GetSandboxExtensionsForBlobFiles(requestID, filenames), 0);
}

void StorageProcess::didGetSandboxExtensionsForBlobFiles(uint64_t requestID, SandboxExtension::HandleArray&& handles)
{
    if (auto handler = m_sandboxExtensionForBlobsCompletionHandlers.take(requestID))
        handler(WTFMove(handles));
}
#endif

#if !PLATFORM(COCOA)
void StorageProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void StorageProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void StorageProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}
#endif

} // namespace WebKit
