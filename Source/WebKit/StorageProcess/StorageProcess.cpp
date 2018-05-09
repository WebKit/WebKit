/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "ChildProcessMessages.h"
#include "Logging.h"
#include "StorageProcessCreationParameters.h"
#include "StorageProcessMessages.h"
#include "StorageProcessProxyMessages.h"
#include "StorageToWebProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWOriginStore.h"
#include "WebSWServerConnection.h"
#include "WebSWServerToContextConnection.h"
#include "WebsiteData.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SWServerWorker.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerClientIdentifier.h>
#include <WebCore/TextEncoding.h>
#include <pal/SessionID.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/MainThread.h>

#if ENABLE(SERVICE_WORKER)
#include "WebSWServerToContextConnectionMessages.h"
#endif

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

#if ENABLE(SERVICE_WORKER)
WebSWServerToContextConnection* StorageProcess::connectionToContextProcessFromIPCConnection(IPC::Connection& connection)
{
    for (auto& serverToContextConnection : m_serverToContextConnections.values()) {
        if (serverToContextConnection->ipcConnection() == &connection)
            return serverToContextConnection.get();
    }
    return nullptr;
}
#endif

#if ENABLE(SERVICE_WORKER)
void StorageProcess::connectionToContextProcessWasClosed(Ref<WebSWServerToContextConnection>&& serverToContextConnection)
{
    auto& securityOrigin = serverToContextConnection->securityOrigin();

    serverToContextConnection->connectionClosed();
    m_serverToContextConnections.remove(securityOrigin);

    for (auto& swServer : m_swServers.values())
        swServer->markAllWorkersForOriginAsTerminated(securityOrigin);

    if (needsServerToContextConnectionForOrigin(securityOrigin)) {
        RELEASE_LOG(ServiceWorker, "Connection to service worker process was closed but is still needed, relaunching it");
        createServerToContextConnection(securityOrigin, std::nullopt);
    }
}

bool StorageProcess::needsServerToContextConnectionForOrigin(const SecurityOriginData& securityOrigin) const
{
    return WTF::anyOf(m_swServers.values(), [&](auto& swServer) {
        return swServer->needsServerToContextConnectionForOrigin(securityOrigin);
    });
}
#endif

void StorageProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::StorageProcess::messageReceiverName()) {
        didReceiveStorageProcessMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::ChildProcess::messageReceiverName()) {
        ChildProcess::didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerToContextConnection::messageReceiverName()) {
        ASSERT(parentProcessHasServiceWorkerEntitlement());
        if (!parentProcessHasServiceWorkerEntitlement())
            return;
        if (auto* webSWConnection = connectionToContextProcessFromIPCConnection(connection)) {
            webSWConnection->didReceiveMessage(connection, decoder);
            return;
        }
    }
#endif
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

    auto addResult = m_idbDatabasePaths.ensure(parameters.sessionID, [path = parameters.indexedDatabaseDirectory] {
        return path;
    });
    if (addResult.isNewEntry) {
        SandboxExtension::consumePermanently(parameters.indexedDatabaseDirectoryExtensionHandle);
        if (!parameters.indexedDatabaseDirectory.isEmpty())
            postStorageTask(createCrossThreadTask(*this, &StorageProcess::ensurePathExists, parameters.indexedDatabaseDirectory));
    }
#endif
#if ENABLE(SERVICE_WORKER)
    if (!parentProcessHasServiceWorkerEntitlement())
        return;

    addResult = m_swDatabasePaths.ensure(parameters.sessionID, [path = parameters.serviceWorkerRegistrationDirectory] {
        return path;
    });
    if (addResult.isNewEntry) {
        SandboxExtension::consumePermanently(parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
        if (!parameters.serviceWorkerRegistrationDirectory.isEmpty())
            postStorageTask(createCrossThreadTask(*this, &StorageProcess::ensurePathExists, parameters.serviceWorkerRegistrationDirectory));
    }

    for (auto& scheme : parameters.urlSchemesServiceWorkersCanHandle)
        registerURLSchemeServiceWorkersCanHandle(scheme);

    m_shouldDisableServiceWorkerProcessTerminationDelay = parameters.shouldDisableServiceWorkerProcessTerminationDelay;
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

void StorageProcess::createStorageToWebProcessConnection(bool isServiceWorkerProcess, WebCore::SecurityOriginData&& securityOrigin)
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();
    m_storageToWebProcessConnections.append(StorageToWebProcessConnection::create(socketPair.server));
    parentProcessConnection()->send(Messages::StorageProcessProxy::DidCreateStorageToWebProcessConnection(IPC::Attachment(socketPair.client)), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        LOG_ERROR("Could not allocate mach port, error %x", kr);
        CRASH();
    }

    // Create a listening connection.
    m_storageToWebProcessConnections.append(StorageToWebProcessConnection::create(IPC::Connection::Identifier(listeningPort)));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::StorageProcessProxy::DidCreateStorageToWebProcessConnection(clientPort), 0);
#else
    notImplemented();
#endif

#if ENABLE(SERVICE_WORKER)
    if (isServiceWorkerProcess && !m_storageToWebProcessConnections.isEmpty()) {
        ASSERT(parentProcessHasServiceWorkerEntitlement());
        ASSERT(m_waitingForServerToContextProcessConnection);
        auto contextConnection = WebSWServerToContextConnection::create(securityOrigin, m_storageToWebProcessConnections.last()->connection());
        auto addResult = m_serverToContextConnections.add(WTFMove(securityOrigin), contextConnection.copyRef());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);

        m_waitingForServerToContextProcessConnection = false;

        for (auto* server : SWServer::allServers())
            server->serverToContextConnectionCreated(contextConnection);
    }
#else
    UNUSED_PARAM(isServiceWorkerProcess);
#endif
}

void StorageProcess::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID)
{
    auto websiteData = std::make_unique<WebsiteData>();
    WebsiteData* rawWebsiteData = websiteData.get();
    auto callbackAggregator = CallbackAggregator::create([this, websiteData = WTFMove(websiteData), callbackID]() {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidFetchWebsiteData(callbackID, *websiteData), 0);
    });

    String path;
#if ENABLE(SERVICE_WORKER)
    path = m_swDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([rawWebsiteData, callbackAggregator = callbackAggregator.copyRef()](const HashSet<SecurityOriginData>& origins) mutable {
            for (auto& origin : origins)
                rawWebsiteData->entries.append({ origin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, callbackAggregator = callbackAggregator.copyRef(), path = WTFMove(path), rawWebsiteData]() mutable {
            RunLoop::main().dispatch([callbackAggregator = WTFMove(callbackAggregator), rawWebsiteData, securityOrigins = indexedDatabaseOrigins(path)] {
                for (const auto& securityOrigin : securityOrigins)
                    rawWebsiteData->entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });
            });
        }));
    }
#endif
}

void StorageProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, uint64_t callbackID)
{
    auto callbackAggregator = CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    });

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations))
        swServerForSession(sessionID).clearAll([callbackAggregator = callbackAggregator.copyRef()] { });
#endif

#if ENABLE(INDEXED_DATABASE)
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases))
        idbServer(sessionID).closeAndDeleteDatabasesModifiedSince(modifiedSince, [callbackAggregator = WTFMove(callbackAggregator)] { });
#endif
}

void StorageProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& securityOrigins, uint64_t callbackID)
{
    auto callbackAggregator = CallbackAggregator::create([this, callbackID]() {
        parentProcessConnection()->send(Messages::StorageProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    });

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        auto& server = swServerForSession(sessionID);
        for (auto& securityOrigin : securityOrigins)
            server.clear(securityOrigin, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    if (!websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases))
        idbServer(sessionID).closeAndDeleteDatabasesForOrigins(securityOrigins, [callbackAggregator = WTFMove(callbackAggregator)] { });
#endif
}

#if ENABLE(SANDBOX_EXTENSIONS)
void StorageProcess::grantSandboxExtensionsForBlobs(const Vector<String>& paths, SandboxExtension::HandleArray&& handles)
{
    ASSERT(paths.size() == handles.size());

    for (size_t i = 0; i < paths.size(); ++i) {
        auto result = m_blobTemporaryFileSandboxExtensions.add(paths[i], SandboxExtension::create(WTFMove(handles[i])));
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
    ASSERT(sessionID.isValid());

    // Use the same SWServer for all ephemeral sessions.
    if (sessionID.isEphemeral())
        sessionID = PAL::SessionID::legacyPrivateSessionID();

    auto result = m_swServers.add(sessionID, nullptr);
    if (!result.isNewEntry) {
        ASSERT(result.iterator->value);
        return *result.iterator->value;
    }

    auto path = m_swDatabasePaths.get(sessionID);
    // There should already be a registered path for this PAL::SessionID.
    // If there's not, then where did this PAL::SessionID come from?
    ASSERT(sessionID.isEphemeral() || !path.isEmpty());

    result.iterator->value = std::make_unique<SWServer>(makeUniqueRef<WebSWOriginStore>(), WTFMove(path), sessionID);
    if (m_shouldDisableServiceWorkerProcessTerminationDelay)
        result.iterator->value->disableServiceWorkerProcessTerminationDelay();
    return *result.iterator->value;
}

WebSWOriginStore& StorageProcess::swOriginStoreForSession(PAL::SessionID sessionID)
{
    return static_cast<WebSWOriginStore&>(swServerForSession(sessionID).originStore());
}

WebSWServerToContextConnection* StorageProcess::serverToContextConnectionForOrigin(const SecurityOriginData& securityOrigin)
{
    return m_serverToContextConnections.get(securityOrigin);
}

void StorageProcess::createServerToContextConnection(const SecurityOriginData& securityOrigin, std::optional<PAL::SessionID> sessionID)
{
    if (m_waitingForServerToContextProcessConnection)
        return;
    
    m_waitingForServerToContextProcessConnection = true;
    if (sessionID)
        parentProcessConnection()->send(Messages::StorageProcessProxy::EstablishWorkerContextConnectionToStorageProcessForExplicitSession(securityOrigin, *sessionID), 0);
    else
        parentProcessConnection()->send(Messages::StorageProcessProxy::EstablishWorkerContextConnectionToStorageProcess(securityOrigin), 0);
}

void StorageProcess::didFailFetch(SWServerConnectionIdentifier serverConnectionIdentifier, uint64_t fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didFailFetch(fetchIdentifier);
}

void StorageProcess::didNotHandleFetch(SWServerConnectionIdentifier serverConnectionIdentifier, uint64_t fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didNotHandleFetch(fetchIdentifier);
}

void StorageProcess::didReceiveFetchResponse(SWServerConnectionIdentifier serverConnectionIdentifier, uint64_t fetchIdentifier, const WebCore::ResourceResponse& response)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchResponse(fetchIdentifier, response);
}

void StorageProcess::didReceiveFetchData(SWServerConnectionIdentifier serverConnectionIdentifier, uint64_t fetchIdentifier, const IPC::DataReference& data, int64_t encodedDataLength)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchData(fetchIdentifier, data, encodedDataLength);
}

void StorageProcess::didReceiveFetchFormData(SWServerConnectionIdentifier serverConnectionIdentifier, uint64_t fetchIdentifier, const IPC::FormDataReference& formData)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchFormData(fetchIdentifier, formData);
}

void StorageProcess::didFinishFetch(SWServerConnectionIdentifier serverConnectionIdentifier, uint64_t fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didFinishFetch(fetchIdentifier);
}

void StorageProcess::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, MessageWithMessagePorts&& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    if (auto* connection = m_swServerConnections.get(destinationIdentifier.serverConnectionIdentifier))
        connection->postMessageToServiceWorkerClient(destinationIdentifier.contextIdentifier, WTFMove(message), sourceIdentifier, sourceOrigin);
}

void StorageProcess::postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destination, WebCore::MessageWithMessagePorts&& message, const WebCore::ServiceWorkerOrClientIdentifier& source, SWServerConnectionIdentifier connectionIdentifier)
{
    if (auto* connection = m_swServerConnections.get(connectionIdentifier))
        connection->postMessageToServiceWorker(destination, WTFMove(message), source);
}

void StorageProcess::registerSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(parentProcessHasServiceWorkerEntitlement());
    ASSERT(!m_swServerConnections.contains(connection.identifier()));
    m_swServerConnections.add(connection.identifier(), &connection);
    swOriginStoreForSession(connection.sessionID()).registerSWServerConnection(connection);
}

void StorageProcess::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(m_swServerConnections.get(connection.identifier()) == &connection);
    m_swServerConnections.remove(connection.identifier());
    swOriginStoreForSession(connection.sessionID()).unregisterSWServerConnection(connection);
}

void StorageProcess::swContextConnectionMayNoLongerBeNeeded(WebSWServerToContextConnection& serverToContextConnection)
{
    auto& securityOrigin = serverToContextConnection.securityOrigin();
    if (needsServerToContextConnectionForOrigin(securityOrigin))
        return;

    RELEASE_LOG(ServiceWorker, "Service worker process is no longer needed, terminating it");
    serverToContextConnection.terminate();

    serverToContextConnection.connectionClosed();
    m_serverToContextConnections.remove(securityOrigin);
}

void StorageProcess::disableServiceWorkerProcessTerminationDelay()
{
    if (m_shouldDisableServiceWorkerProcessTerminationDelay)
        return;

    m_shouldDisableServiceWorkerProcessTerminationDelay = true;
    for (auto& swServer : m_swServers.values())
        swServer->disableServiceWorkerProcessTerminationDelay();
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
