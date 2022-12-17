/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebSharedWorkerContextManagerConnection.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "RemoteWebLockRegistry.h"
#include "RemoteWorkerFrameLoaderClient.h"
#include "RemoteWorkerLibWebRTCProvider.h"
#include "WebBadgeClient.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCacheStorageProvider.h"
#include "WebCompiledContentRuleListData.h"
#include "WebDatabaseProvider.h"
#include "WebPage.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include "WebSharedWorkerServerToContextConnectionMessages.h"
#include "WebSocketProvider.h"
#include "WebUserContentController.h"
#include <WebCore/EmptyClients.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SharedWorkerContextManager.h>
#include <WebCore/SharedWorkerThreadProxy.h>
#include <WebCore/UserAgent.h>
#include <WebCore/WorkerFetchResult.h>
#include <WebCore/WorkerInitializationData.h>

namespace WebKit {

WebSharedWorkerContextManagerConnection::WebSharedWorkerContextManagerConnection(Ref<IPC::Connection>&& connectionToNetworkProcess, WebCore::RegistrableDomain&& registrableDomain, PageGroupIdentifier pageGroupID, WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier pageID, const WebPreferencesStore& preferencesStore, RemoteWorkerInitializationData&& initializationData)
    : m_connectionToNetworkProcess(WTFMove(connectionToNetworkProcess))
    , m_registrableDomain(WTFMove(registrableDomain))
    , m_pageGroupID(pageGroupID)
    , m_webPageProxyID(webPageProxyID)
    , m_pageID(pageID)
#if PLATFORM(COCOA)
    , m_userAgent(WebCore::standardUserAgentWithApplicationName({ }))
#else
    , m_userAgent(WebCore::standardUserAgent())
#endif
    , m_userContentController(WebUserContentController::getOrCreate(initializationData.userContentControllerIdentifier))
{
#if ENABLE(CONTENT_EXTENSIONS)
    m_userContentController->addContentRuleLists(WTFMove(initializationData.contentRuleLists));
#endif

    updatePreferencesStore(preferencesStore);
    WebProcess::singleton().disableTermination();
}

WebSharedWorkerContextManagerConnection::~WebSharedWorkerContextManagerConnection() = default;

void WebSharedWorkerContextManagerConnection::establishConnection(CompletionHandler<void()>&& completionHandler)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::EstablishSharedWorkerContextConnection { m_webPageProxyID, m_registrableDomain }, WTFMove(completionHandler), 0);
}

void WebSharedWorkerContextManagerConnection::postExceptionToWorkerObject(WebCore::SharedWorkerIdentifier sharedWorkerIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_connectionToNetworkProcess->send(Messages::WebSharedWorkerServerToContextConnection::PostExceptionToWorkerObject { sharedWorkerIdentifier, errorMessage, lineNumber, columnNumber, sourceURL }, 0);
}

void WebSharedWorkerContextManagerConnection::updatePreferencesStore(const WebPreferencesStore& store)
{
    WebPage::updatePreferencesGenerated(store);
    m_preferencesStore = store;
}

void WebSharedWorkerContextManagerConnection::launchSharedWorker(WebCore::ClientOrigin&& origin, WebCore::SharedWorkerIdentifier sharedWorkerIdentifier, WebCore::WorkerOptions&& workerOptions, WebCore::WorkerFetchResult&& workerFetchResult, WebCore::WorkerInitializationData&& initializationData)
{
    RELEASE_LOG(SharedWorker, "WebSharedWorkerContextManagerConnection::launchSharedWorker: sharedWorkerIdentifier=%" PRIu64, sharedWorkerIdentifier.toUInt64());
    auto pageConfiguration = WebCore::pageConfigurationWithEmptyClients(WebProcess::singleton().sessionID());

    pageConfiguration.badgeClient = WebBadgeClient::create();
    pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroupID);
    pageConfiguration.socketProvider = WebSocketProvider::create(m_webPageProxyID);
    pageConfiguration.broadcastChannelRegistry = WebProcess::singleton().broadcastChannelRegistry();
    pageConfiguration.userContentProvider = m_userContentController;
#if ENABLE(WEB_RTC)
    pageConfiguration.webRTCProvider = makeUniqueRef<RemoteWorkerLibWebRTCProvider>();
#endif

    pageConfiguration.loaderClientForMainFrame = makeUniqueRef<RemoteWorkerFrameLoaderClient>(m_webPageProxyID, m_pageID, m_userAgent);

    auto page = makeUniqueRef<WebCore::Page>(WTFMove(pageConfiguration));
    if (m_preferencesStore) {
        WebPage::updateSettingsGenerated(*m_preferencesStore, page->settings());
        page->settings().setStorageBlockingPolicy(static_cast<WebCore::StorageBlockingPolicy>(m_preferencesStore->getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey())));
    }

    if (!initializationData.userAgent.isEmpty())
        initializationData.userAgent = m_userAgent;

    if (!initializationData.clientIdentifier)
        initializationData.clientIdentifier = WebCore::ScriptExecutionContextIdentifier::generate();

    page->setupForRemoteWorker(workerFetchResult.responseURL, origin.topOrigin, workerFetchResult.referrerPolicy);
    auto sharedWorkerThreadProxy = WebCore::SharedWorkerThreadProxy::create(WTFMove(page), sharedWorkerIdentifier, origin, WTFMove(workerFetchResult), WTFMove(workerOptions), WTFMove(initializationData), WebProcess::singleton().cacheStorageProvider());

    WebCore::SharedWorkerContextManager::singleton().registerSharedWorkerThread(WTFMove(sharedWorkerThreadProxy));
}

void WebSharedWorkerContextManagerConnection::close()
{
    RELEASE_LOG(SharedWorker, "WebSharedWorkerContextManagerConnection::close: Shared worker process is requested to stop all shared workers (already stopped = %d)", isClosed());
    if (isClosed())
        return;

    setAsClosed();

    m_connectionToNetworkProcess->send(Messages::NetworkConnectionToWebProcess::CloseSharedWorkerContextConnection { }, 0);
    WebCore::SharedWorkerContextManager::singleton().stopAllSharedWorkers();
    WebProcess::singleton().enableTermination();
}

} // namespace WebKit
