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
#include "WebSWOriginStore.h"
#include "WebSWServerConnection.h"
#include "WebsiteData.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerClientIdentifier.h>
#include <WebCore/TextEncoding.h>
#include <pal/SessionID.h>
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
IDBServer::IDBServer& StorageProcess::idbServer(PAL::SessionID sessionID)
{
    auto addResult = m_idbServers.add(sessionID, nullptr);
    if (!addResult.isNewEntry) {
        ASSERT(addResult.iterator->value);
        return *addResult.iterator->value;
    }

    auto path = m_idbDatabasePaths.get(sessionID);
    // There should already be a registered path for this PAL::SessionID.
    // If there's not, then where did this PAL::SessionID come from?
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

    if (!FileSystem::makeAllDirectories(path))
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
    m_storageToWebProcessConnections.append(StorageToWebProcessConnection::create(socketPair.server));
    parentProcessConnection()->send(Messages::StorageProcessProxy::DidCreateStorageToWebProcessConnection(IPC::Attachment(socketPair.client)), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    m_storageToWebProcessConnections.append(StorageToWebProcessConnection::create(IPC::Connection::Identifier(listeningPort)));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::StorageProcessProxy::DidCreateStorageToWebProcessConnection(clientPort), 0);
#else
    notImplemented();
#endif
}

void StorageProcess::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID)
{
    auto completionHandler = [this, callbackID](const WebsiteData& websiteData) {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
    };

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations))
        notImplemented();
#endif

#if ENABLE(INDEXED_DATABASE)
    String path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, completionHandler = WTFMove(completionHandler), path = WTFMove(path)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), securityOrigins = indexedDatabaseOrigins(path)] {
                WebsiteData websiteData;
                for (const auto& securityOrigin : securityOrigins)
                    websiteData.entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });

                completionHandler(websiteData);
            });
        }));
        return;
    }
#endif

    completionHandler({ });
}

void StorageProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
{
    auto completionHandler = [this, callbackID]() {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    };

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        if (auto* store = swOriginStoreForSession(sessionID))
            store->clear();
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        idbServer(sessionID).closeAndDeleteDatabasesModifiedSince(modifiedSince, WTFMove(completionHandler));
        return;
    }
#endif

    completionHandler();
}

void StorageProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& securityOriginDatas, uint64_t callbackID)
{
    auto completionHandler = [this, callbackID]() {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    };

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        if (auto* store = swOriginStoreForSession(sessionID)) {
            for (auto& originData : securityOriginDatas)
                store->remove(originData.securityOrigin());
        }
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    if (!websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        idbServer(sessionID).closeAndDeleteDatabasesForOrigins(securityOriginDatas, WTFMove(completionHandler));
        return;
    }
#endif

    completionHandler();
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
    FileSystem::deleteFile(path);

    if (auto extension = m_blobTemporaryFileSandboxExtensions.take(path))
        extension->revoke();
}

Vector<WebCore::SecurityOriginData> StorageProcess::indexedDatabaseOrigins(const String& path)
{
    if (path.isEmpty())
        return { };

    Vector<WebCore::SecurityOriginData> securityOrigins;
    for (auto& originPath : FileSystem::listDirectory(path, "*")) {
        String databaseIdentifier = FileSystem::pathGetFileName(originPath);

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

#if ENABLE(SERVICE_WORKER)
SWServer& StorageProcess::swServerForSession(PAL::SessionID sessionID)
{
    auto result = m_swServers.add(sessionID, nullptr);
    if (result.isNewEntry)
        result.iterator->value = std::make_unique<SWServer>();

    ASSERT(result.iterator->value);
    return *result.iterator->value;
}

IPC::Connection* StorageProcess::workerContextProcessConnection()
{
    return m_workerContextProcessConnection.get();
}

void StorageProcess::createWorkerContextProcessConnection()
{
    if (m_waitingForWorkerContextProcessConnection)
        return;
    
    m_waitingForWorkerContextProcessConnection = true;
    parentProcessConnection()->send(Messages::StorageProcessProxy::GetWorkerContextProcessConnection(), 0);
}

WebSWOriginStore& StorageProcess::ensureSWOriginStoreForSession(PAL::SessionID sessionID)
{
    return *m_swOriginStores.ensure(sessionID, [] {
        return std::make_unique<WebSWOriginStore>();
    }).iterator->value;
}

WebSWOriginStore* StorageProcess::swOriginStoreForSession(PAL::SessionID sessionID) const
{
    auto it = m_swOriginStores.find(sessionID);
    if (it == m_swOriginStores.end())
        return nullptr;
    return it->value.get();
}

void StorageProcess::didGetWorkerContextProcessConnection(IPC::Attachment&& encodedConnectionIdentifier)
{
    ASSERT(m_waitingForWorkerContextProcessConnection);
    m_waitingForWorkerContextProcessConnection = false;

#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::Identifier connectionIdentifier = encodedConnectionIdentifier.releaseFileDescriptor();
#elif OS(DARWIN)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
#else
    ASSERT_NOT_REACHED();
#endif

    if (IPC::Connection::identifierIsNull(connectionIdentifier)) {
        LOG_ERROR("StorageProcess::didGetWorkerContextProcessConnection - Received null connection identifier");
        return;
    }

    m_workerContextProcessConnection = IPC::Connection::createClientConnection(connectionIdentifier, *this);
    m_workerContextProcessConnection->open();
    
    for (auto& connection : m_storageToWebProcessConnections)
        connection->workerContextProcessConnectionCreated();
}

void StorageProcess::serviceWorkerContextFailedToStart(uint64_t serverConnectionIdentifier, const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& message)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->scriptContextFailedToStart(registrationKey, serviceWorkerIdentifier, message);
}

void StorageProcess::serviceWorkerContextStarted(uint64_t serverConnectionIdentifier, const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->scriptContextStarted(registrationKey, serviceWorkerIdentifier);
}

void StorageProcess::didFailFetch(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didFailFetch(fetchIdentifier);
}

void StorageProcess::didNotHandleFetch(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didNotHandleFetch(fetchIdentifier);
}

void StorageProcess::didReceiveFetchResponse(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier, const WebCore::ResourceResponse& response)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchResponse(fetchIdentifier, response);
}

void StorageProcess::didReceiveFetchData(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier, const IPC::DataReference& data, int64_t encodedDataLength)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchData(fetchIdentifier, data, encodedDataLength);
}

void StorageProcess::didFinishFetch(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didFinishFetch(fetchIdentifier);
}

void StorageProcess::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, const IPC::DataReference& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    if (auto* connection = m_swServerConnections.get(destinationIdentifier.serverConnectionIdentifier))
        connection->postMessageToServiceWorkerClient(destinationIdentifier.scriptExecutionContextIdentifier, message, sourceIdentifier, sourceOrigin);
}

void StorageProcess::registerSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(!m_swServerConnections.contains(connection.identifier()));
    m_swServerConnections.add(connection.identifier(), &connection);
    ensureSWOriginStoreForSession(connection.sessionID()).registerSWServerConnection(connection);
}

void StorageProcess::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(m_swServerConnections.get(connection.identifier()) == &connection);
    m_swServerConnections.remove(connection.identifier());
    if (auto* originStore = swOriginStoreForSession(connection.sessionID()))
        originStore->unregisterSWServerConnection(connection);
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
