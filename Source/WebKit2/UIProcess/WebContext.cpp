/*
 * Copyright (C) 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "WebContext.h"

#include "APIArray.h"
#include "APIHistoryClient.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "MutableDictionary.h"
#include "SandboxExtension.h"
#include "StatisticsData.h"
#include "TextChecker.h"
#include "WKContextPrivate.h"
#include "WebApplicationCacheManagerProxy.h"
#include "WebContextMessageKinds.h"
#include "WebContextMessages.h"
#include "WebContextSupplement.h"
#include "WebContextUserMessageCoders.h"
#include "WebCookieManagerProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseManagerProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebIconDatabase.h"
#include "WebKeyValueStorageManager.h"
#include "WebKit2Initialize.h"
#include "WebMediaCacheManagerProxy.h"
#include "WebNotificationManagerProxy.h"
#include "WebPluginSiteDataManager.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"
#include "WebMemorySampler.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebResourceCacheManagerProxy.h"
#include <WebCore/Language.h>
#include <WebCore/LinkHash.h>
#include <WebCore/Logging.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SessionID.h>
#include <runtime/JSCInlines.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryManagerProxy.h"
#endif

#if ENABLE(NETWORK_INFO)
#include "WebNetworkInfoManagerProxy.h"
#endif

#if ENABLE(DATABASE_PROCESS)
#include "DatabaseProcessCreationParameters.h"
#include "DatabaseProcessMessages.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#endif

#if ENABLE(CUSTOM_PROTOCOLS)
#include "CustomProtocolManagerMessages.h"
#endif

#if USE(SOUP)
#if ENABLE(CUSTOM_PROTOCOLS)
#include "WebSoupCustomProtocolRequestManager.h"
#else
#include "WebSoupRequestManagerProxy.h"
#endif
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;

namespace WebKit {

static const double sharedSecondaryProcessShutdownTimeout = 60;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webContextCounter, ("WebContext"));

PassRefPtr<WebContext> WebContext::create(const String& injectedBundlePath)
{
    InitializeWebKit2();
    return adoptRef(new WebContext(injectedBundlePath));
}

static Vector<WebContext*>& contexts()
{
    static NeverDestroyed<Vector<WebContext*>> contexts;
    return contexts;
}

const Vector<WebContext*>& WebContext::allContexts()
{
    return contexts();
}

WebContext::WebContext(const String& injectedBundlePath)
    : m_processModel(ProcessModelSharedSecondaryProcess)
    , m_webProcessCountLimit(UINT_MAX)
    , m_haveInitialEmptyProcess(false)
    , m_processWithPageCache(0)
    , m_defaultPageGroup(WebPageGroup::createNonNull())
    , m_injectedBundlePath(injectedBundlePath)
    , m_historyClient(std::make_unique<API::HistoryClient>())
    , m_visitedLinkProvider(this)
    , m_plugInAutoStartProvider(this)
    , m_alwaysUsesComplexTextCodePath(false)
    , m_shouldUseFontSmoothing(true)
    , m_cacheModel(CacheModelDocumentViewer)
    , m_memorySamplerEnabled(false)
    , m_memorySamplerInterval(1400.0)
    , m_storageManager(StorageManager::create())
#if USE(SOUP)
    , m_initialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain)
#endif
    , m_shouldUseTestingNetworkSession(false)
    , m_processTerminationEnabled(true)
#if ENABLE(NETWORK_PROCESS)
    , m_usesNetworkProcess(false)
#endif
#if USE(SOUP)
    , m_ignoreTLSErrors(true)
#endif
    , m_memoryCacheDisabled(false)
{
    platformInitialize();

    addMessageReceiver(Messages::WebContext::messageReceiverName(), *this);
    addMessageReceiver(WebContextLegacyMessages::messageReceiverName(), *this);

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    m_iconDatabase = WebIconDatabase::create(this);
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginSiteDataManager = WebPluginSiteDataManager::create(this);
#endif // ENABLE(NETSCAPE_PLUGIN_API)

    addSupplement<WebApplicationCacheManagerProxy>();
    addSupplement<WebCookieManagerProxy>();
    addSupplement<WebGeolocationManagerProxy>();
    addSupplement<WebKeyValueStorageManager>();
    addSupplement<WebMediaCacheManagerProxy>();
    addSupplement<WebNotificationManagerProxy>();
    addSupplement<WebResourceCacheManagerProxy>();
#if ENABLE(SQL_DATABASE)
    addSupplement<WebDatabaseManagerProxy>();
#endif
#if USE(SOUP)
#if ENABLE(CUSTOM_PROTOCOLS)
    addSupplement<WebSoupCustomProtocolRequestManager>();
#else
    addSupplement<WebSoupRequestManagerProxy>();
#endif
#endif
#if ENABLE(BATTERY_STATUS)
    addSupplement<WebBatteryManagerProxy>();
#endif
#if ENABLE(NETWORK_INFO)
    addSupplement<WebNetworkInfoManagerProxy>();
#endif

    contexts().append(this);

    addLanguageChangeObserver(this, languageChanged);

#if !LOG_DISABLED
    WebCore::initializeLoggingChannelsIfNecessary();
    WebKit::initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginInfoStore.setClient(this);
#endif

#ifndef NDEBUG
    webContextCounter.increment();
#endif

    m_storageManager->setLocalStorageDirectory(localStorageDirectory());
}

#if !PLATFORM(COCOA)
void WebContext::platformInitialize()
{
}
#endif

WebContext::~WebContext()
{
    ASSERT(contexts().find(this) != notFound);
    contexts().remove(contexts().find(this));

    removeLanguageChangeObserver(this);

    m_messageReceiverMap.invalidate();

    WebContextSupplementMap::const_iterator it = m_supplements.begin();
    WebContextSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it) {
        it->value->contextDestroyed();
        it->value->clearContext();
    }

    m_iconDatabase->invalidate();
    m_iconDatabase->clearContext();
    
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginSiteDataManager->invalidate();
    m_pluginSiteDataManager->clearContext();
#endif

    invalidateCallbackMap(m_dictionaryCallbacks);

    platformInvalidateContext();

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginInfoStore.setClient(0);
#endif

#ifndef NDEBUG
    webContextCounter.decrement();
#endif
}

void WebContext::initializeClient(const WKContextClientBase* client)
{
    m_client.initialize(client);
}

void WebContext::initializeInjectedBundleClient(const WKContextInjectedBundleClientBase* client)
{
    m_injectedBundleClient.initialize(client);
}

void WebContext::initializeConnectionClient(const WKContextConnectionClientBase* client)
{
    m_connectionClient.initialize(client);
}

void WebContext::setHistoryClient(std::unique_ptr<API::HistoryClient> historyClient)
{
    if (!historyClient)
        m_historyClient = std::make_unique<API::HistoryClient>();
    else
        m_historyClient = std::move(historyClient);

    sendToAllProcesses(Messages::WebProcess::SetShouldTrackVisitedLinks(m_historyClient->shouldTrackVisitedLinks()));
}

void WebContext::initializeDownloadClient(const WKContextDownloadClientBase* client)
{
    m_downloadClient.initialize(client);
}

void WebContext::setProcessModel(ProcessModel processModel)
{
    // Guard against API misuse.
    if (!m_processes.isEmpty())
        CRASH();
    if (processModel != ProcessModelSharedSecondaryProcess && !m_messagesToInjectedBundlePostedToEmptyContext.isEmpty())
        CRASH();

    m_processModel = processModel;
}

void WebContext::setMaximumNumberOfProcesses(unsigned maximumNumberOfProcesses)
{
    // Guard against API misuse.
    if (!m_processes.isEmpty())
        CRASH();

    if (maximumNumberOfProcesses == 0)
        m_webProcessCountLimit = UINT_MAX;
    else
        m_webProcessCountLimit = maximumNumberOfProcesses;
}

IPC::Connection* WebContext::networkingProcessConnection()
{
    switch (m_processModel) {
    case ProcessModelSharedSecondaryProcess:
#if ENABLE(NETWORK_PROCESS)
        if (m_usesNetworkProcess)
            return m_networkProcess->connection();
#endif
        return m_processes[0]->connection();
    case ProcessModelMultipleSecondaryProcesses:
#if ENABLE(NETWORK_PROCESS)
        ASSERT(m_usesNetworkProcess);
        return m_networkProcess->connection();
#else
        break;
#endif
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebContext::languageChanged(void* context)
{
    static_cast<WebContext*>(context)->languageChanged();
}

void WebContext::languageChanged()
{
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged(userPreferredLanguages()));
#if USE(SOUP) && ENABLE(NETWORK_PROCESS)
    if (m_usesNetworkProcess && m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::UserPreferredLanguagesChanged(userPreferredLanguages()), 0);
#endif
}

void WebContext::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    sendToAllProcesses(Messages::WebProcess::FullKeyboardAccessModeChanged(fullKeyboardAccessEnabled));
}

void WebContext::textCheckerStateChanged()
{
    sendToAllProcesses(Messages::WebProcess::SetTextCheckerState(TextChecker::state()));
}

void WebContext::setUsesNetworkProcess(bool usesNetworkProcess)
{
#if ENABLE(NETWORK_PROCESS)
    m_usesNetworkProcess = usesNetworkProcess;
#else
    UNUSED_PARAM(usesNetworkProcess);
#endif
}

bool WebContext::usesNetworkProcess() const
{
#if ENABLE(NETWORK_PROCESS)
    return m_usesNetworkProcess;
#else
    return false;
#endif
}

#if ENABLE(NETWORK_PROCESS)
void WebContext::ensureNetworkProcess()
{
    if (m_networkProcess)
        return;

    m_networkProcess = NetworkProcessProxy::create(*this);

    NetworkProcessCreationParameters parameters;

    parameters.privateBrowsingEnabled = WebPreferences::anyPagesAreUsingPrivateBrowsing();

    parameters.cacheModel = m_cacheModel;

    parameters.diskCacheDirectory = diskCacheDirectory();
    if (!parameters.diskCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.diskCacheDirectory, parameters.diskCacheDirectoryExtensionHandle);

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    // Add any platform specific parameters
    platformInitializeNetworkProcess(parameters);

    // Initialize the network process.
    m_networkProcess->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);

#if PLATFORM(COCOA)
    m_networkProcess->send(Messages::NetworkProcess::SetQOS(networkProcessLatencyQOS(), networkProcessThroughputQOS()), 0);
#endif
}

void WebContext::networkProcessCrashed(NetworkProcessProxy* networkProcessProxy)
{
    ASSERT(m_networkProcess);
    ASSERT(networkProcessProxy == m_networkProcess.get());

    WebContextSupplementMap::const_iterator it = m_supplements.begin();
    WebContextSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it)
        it->value->processDidClose(networkProcessProxy);

    m_networkProcess = nullptr;

    m_client.networkProcessDidCrash(this);
}

void WebContext::getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply)
{
    ASSERT(reply);

    ensureNetworkProcess();
    ASSERT(m_networkProcess);

    m_networkProcess->getNetworkProcessConnection(reply);
}
#endif

#if ENABLE(DATABASE_PROCESS)
void WebContext::ensureDatabaseProcess()
{
    if (m_databaseProcess)
        return;

    m_databaseProcess = DatabaseProcessProxy::create(this);

    DatabaseProcessCreationParameters parameters;

    // Indexed databases exist in a subdirectory of the "database directory path."
    // Currently, the top level of that directory contains entities related to WebSQL databases.
    // We should fix this, and move WebSQL into a subdirectory (https://bugs.webkit.org/show_bug.cgi?id=124807)
    // In the meantime, an entity name prefixed with three underscores will not conflict with any WebSQL entities.
    parameters.indexedDatabaseDirectory = pathByAppendingComponent(databaseDirectory(), "___IndexedDB");

    m_databaseProcess->send(Messages::DatabaseProcess::InitializeDatabaseProcess(parameters), 0);
}

void WebContext::getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply)
{
    ASSERT(reply);

    ensureDatabaseProcess();

    m_databaseProcess->getDatabaseProcessConnection(reply);
}
#endif

void WebContext::willStartUsingPrivateBrowsing()
{
    const Vector<WebContext*>& contexts = allContexts();
    for (size_t i = 0, count = contexts.size(); i < count; ++i)
        contexts[i]->setAnyPageGroupMightHavePrivateBrowsingEnabled(true);
}

void WebContext::willStopUsingPrivateBrowsing()
{
    const Vector<WebContext*>& contexts = allContexts();
    for (size_t i = 0, count = contexts.size(); i < count; ++i)
        contexts[i]->setAnyPageGroupMightHavePrivateBrowsingEnabled(false);
}

void WebContext::windowServerConnectionStateChanged()
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i)
        m_processes[i]->windowServerConnectionStateChanged();
}

void WebContext::setAnyPageGroupMightHavePrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    m_iconDatabase->setPrivateBrowsingEnabled(privateBrowsingEnabled);

#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && networkProcess()) {
        if (privateBrowsingEnabled)
            networkProcess()->send(Messages::NetworkProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);
        else
            networkProcess()->send(Messages::NetworkProcess::DestroyPrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);
    }
#endif // ENABLED(NETWORK_PROCESS)

    if (privateBrowsingEnabled)
        sendToAllProcesses(Messages::WebProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()));
    else
        sendToAllProcesses(Messages::WebProcess::DestroyPrivateBrowsingSession(SessionID::legacyPrivateSessionID()));
}

void (*s_invalidMessageCallback)(WKStringRef messageName);

void WebContext::setInvalidMessageCallback(void (*invalidMessageCallback)(WKStringRef messageName))
{
    s_invalidMessageCallback = invalidMessageCallback;
}

void WebContext::didReceiveInvalidMessage(const IPC::StringReference& messageReceiverName, const IPC::StringReference& messageName)
{
    if (!s_invalidMessageCallback)
        return;

    StringBuilder messageNameStringBuilder;
    messageNameStringBuilder.append(messageReceiverName.data(), messageReceiverName.size());
    messageNameStringBuilder.append(".");
    messageNameStringBuilder.append(messageName.data(), messageName.size());

    s_invalidMessageCallback(toAPI(API::String::create(messageNameStringBuilder.toString()).get()));
}

void WebContext::processDidCachePage(WebProcessProxy* process)
{
    if (m_processWithPageCache && m_processWithPageCache != process)
        m_processWithPageCache->releasePageCache();
    m_processWithPageCache = process;
}

WebProcessProxy& WebContext::ensureSharedWebProcess()
{
    ASSERT(m_processModel == ProcessModelSharedSecondaryProcess);
    if (m_processes.isEmpty())
        createNewWebProcess();
    return *m_processes[0];
}

WebProcessProxy& WebContext::createNewWebProcess()
{
#if ENABLE(NETWORK_PROCESS)
    if (m_usesNetworkProcess)
        ensureNetworkProcess();
#endif

    RefPtr<WebProcessProxy> process = WebProcessProxy::create(*this);

    WebProcessCreationParameters parameters;

    parameters.injectedBundlePath = injectedBundlePath();
    if (!parameters.injectedBundlePath.isEmpty())
        SandboxExtension::createHandle(parameters.injectedBundlePath, SandboxExtension::ReadOnly, parameters.injectedBundlePathExtensionHandle);

    parameters.applicationCacheDirectory = applicationCacheDirectory();
    if (!parameters.applicationCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.applicationCacheDirectory, parameters.applicationCacheDirectoryExtensionHandle);

    parameters.databaseDirectory = databaseDirectory();
    if (!parameters.databaseDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.databaseDirectory, parameters.databaseDirectoryExtensionHandle);

    parameters.localStorageDirectory = localStorageDirectory();
    if (!parameters.localStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.localStorageDirectory, parameters.localStorageDirectoryExtensionHandle);

    parameters.diskCacheDirectory = diskCacheDirectory();
    if (!parameters.diskCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.diskCacheDirectory, parameters.diskCacheDirectoryExtensionHandle);

    parameters.cookieStorageDirectory = cookieStorageDirectory();
    if (!parameters.cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    parameters.shouldTrackVisitedLinks = m_historyClient->shouldTrackVisitedLinks();
    parameters.cacheModel = m_cacheModel;
    parameters.languages = userPreferredLanguages();

    copyToVector(m_schemesToRegisterAsEmptyDocument, parameters.urlSchemesRegistererdAsEmptyDocument);
    copyToVector(m_schemesToRegisterAsSecure, parameters.urlSchemesRegisteredAsSecure);
    copyToVector(m_schemesToSetDomainRelaxationForbiddenFor, parameters.urlSchemesForWhichDomainRelaxationIsForbidden);
    copyToVector(m_schemesToRegisterAsLocal, parameters.urlSchemesRegisteredAsLocal);
    copyToVector(m_schemesToRegisterAsNoAccess, parameters.urlSchemesRegisteredAsNoAccess);
    copyToVector(m_schemesToRegisterAsDisplayIsolated, parameters.urlSchemesRegisteredAsDisplayIsolated);
    copyToVector(m_schemesToRegisterAsCORSEnabled, parameters.urlSchemesRegisteredAsCORSEnabled);
#if ENABLE(CACHE_PARTITIONING)
    copyToVector(m_schemesToRegisterAsCachePartitioned, parameters.urlSchemesRegisteredAsCachePartitioned);
#endif

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;
    
    parameters.iconDatabaseEnabled = !iconDatabasePath().isEmpty();

    parameters.terminationTimeout = (m_processModel == ProcessModelSharedSecondaryProcess) ? sharedSecondaryProcessShutdownTimeout : 0;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = API::URLRequest::defaultTimeoutInterval();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    // FIXME: There should be a generic way for supplements to add to the intialization parameters.
    supplement<WebNotificationManagerProxy>()->populateCopyOfNotificationPermissions(parameters.notificationPermissions);
#endif

#if ENABLE(NETWORK_PROCESS)
    parameters.usesNetworkProcess = m_usesNetworkProcess;
#endif

    parameters.plugInAutoStartOriginHashes = m_plugInAutoStartProvider.autoStartOriginHashesCopy();
    copyToVector(m_plugInAutoStartProvider.autoStartOrigins(), parameters.plugInAutoStartOrigins);

    parameters.memoryCacheDisabled = m_memoryCacheDisabled;

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    RefPtr<API::Object> injectedBundleInitializationUserData = m_injectedBundleClient.getInjectedBundleInitializationUserData(this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    process->send(Messages::WebProcess::InitializeWebProcess(parameters, WebContextUserMessageEncoder(injectedBundleInitializationUserData.get(), *process)), 0);

#if PLATFORM(COCOA)
    process->send(Messages::WebProcess::SetQOS(webProcessLatencyQOS(), webProcessThroughputQOS()), 0);
#endif

    if (WebPreferences::anyPagesAreUsingPrivateBrowsing())
        process->send(Messages::WebProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);

    m_processes.append(process);

    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        for (size_t i = 0; i != m_messagesToInjectedBundlePostedToEmptyContext.size(); ++i) {
            std::pair<String, RefPtr<API::Object>>& message = m_messagesToInjectedBundlePostedToEmptyContext[i];

            IPC::ArgumentEncoder messageData;

            messageData.encode(message.first);
            messageData.encode(WebContextUserMessageEncoder(message.second.get(), *process));
            process->send(Messages::WebProcess::PostInjectedBundleMessage(IPC::DataReference(messageData.buffer(), messageData.bufferSize())), 0);
        }
        m_messagesToInjectedBundlePostedToEmptyContext.clear();
    } else
        ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

    return *process;
}

void WebContext::warmInitialProcess()  
{
    if (m_haveInitialEmptyProcess) {
        ASSERT(!m_processes.isEmpty());
        return;
    }

    if (m_processes.size() >= m_webProcessCountLimit)
        return;

    createNewWebProcess();
    m_haveInitialEmptyProcess = true;
}

void WebContext::enableProcessTermination()
{
    m_processTerminationEnabled = true;
    Vector<RefPtr<WebProcessProxy>> processes = m_processes;
    for (size_t i = 0; i < processes.size(); ++i) {
        if (shouldTerminate(processes[i].get()))
            processes[i]->terminate();
    }
}

bool WebContext::shouldTerminate(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (!m_processTerminationEnabled)
        return false;

    for (const auto& supplement : m_supplements.values()) {
        if (!supplement->shouldTerminate(process))
            return false;
    }

    return true;
}

void WebContext::processWillOpenConnection(WebProcessProxy* process)
{
    m_storageManager->processWillOpenConnection(process);
}

void WebContext::processWillCloseConnection(WebProcessProxy* process)
{
    m_storageManager->processWillCloseConnection(process);
}

void WebContext::processDidFinishLaunching(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    m_visitedLinkProvider.processDidFinishLaunching(process);

    // Sometimes the memorySampler gets initialized after process initialization has happened but before the process has finished launching
    // so check if it needs to be started here
    if (m_memorySamplerEnabled) {
        SandboxExtension::Handle sampleLogSandboxHandle;        
        double now = WTF::currentTime();
        String sampleLogFilePath = String::format("WebProcess%llupid%d", static_cast<unsigned long long>(now), process->processIdentifier());
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::ReadWrite, sampleLogSandboxHandle);
        
        process->send(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    m_connectionClient.didCreateConnection(this, process->webConnection());
}

void WebContext::disconnectProcess(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    m_visitedLinkProvider.processDidClose(process);

    if (m_haveInitialEmptyProcess && process == m_processes.last())
        m_haveInitialEmptyProcess = false;

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls below are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    if (m_processModel != ProcessModelSharedSecondaryProcess) {
        RefPtr<WebProcessProxy> protect(process);
        if (m_processWithPageCache == process)
            m_processWithPageCache = 0;

        static_cast<WebContextSupplement*>(supplement<WebGeolocationManagerProxy>())->processDidClose(process);

        m_processes.remove(m_processes.find(process));
        return;
    }

    WebContextSupplementMap::const_iterator it = m_supplements.begin();
    WebContextSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it)
        it->value->processDidClose(process);

    // The vector may have the last reference to process proxy, which in turn may have the last reference to the context.
    // Since vector elements are destroyed in place, we would recurse into WebProcessProxy destructor
    // if it were invoked from Vector::remove(). RefPtr delays destruction until it's safe.
    RefPtr<WebProcessProxy> protect(process);
    if (m_processWithPageCache == process)
        m_processWithPageCache = 0;
    m_processes.remove(m_processes.find(process));
}

WebProcessProxy& WebContext::createNewWebProcessRespectingProcessCountLimit()
{
    if (m_processes.size() < m_webProcessCountLimit)
        return createNewWebProcess();

    // Choose a process with fewest pages, to achieve flat distribution.
    WebProcessProxy* result = nullptr;
    unsigned fewestPagesSeen = UINT_MAX;
    for (unsigned i = 0; i < m_processes.size(); ++i) {
        if (fewestPagesSeen > m_processes[i]->pages().size()) {
            result = m_processes[i].get();
            fewestPagesSeen = m_processes[i]->pages().size();
        }
    }
    return *result;
}

PassRefPtr<WebPageProxy> WebContext::createWebPage(PageClient& pageClient, WebPageConfiguration configuration)
{
    if (!configuration.pageGroup)
        configuration.pageGroup = &m_defaultPageGroup.get();
    if (!configuration.preferences)
        configuration.preferences = &configuration.pageGroup->preferences();
    if (!configuration.session)
        configuration.session = configuration.preferences->privateBrowsingEnabled() ? &API::Session::legacyPrivateSession() : &API::Session::defaultSession();

    RefPtr<WebProcessProxy> process;
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        process = &ensureSharedWebProcess();
    } else {
        if (m_haveInitialEmptyProcess) {
            process = m_processes.last();
            m_haveInitialEmptyProcess = false;
        } else if (configuration.relatedPage) {
            // Sharing processes, e.g. when creating the page via window.open().
            process = &configuration.relatedPage->process();
        } else
            process = &createNewWebProcessRespectingProcessCountLimit();
    }

    return process->createWebPage(pageClient, std::move(configuration));
}

DownloadProxy* WebContext::download(WebPageProxy* initiatingPage, const ResourceRequest& request)
{
    DownloadProxy* downloadProxy = createDownloadProxy();
    uint64_t initiatingPageID = initiatingPage ? initiatingPage->pageID() : 0;

#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && networkProcess()) {
        // FIXME (NetworkProcess): Replicate whatever FrameLoader::setOriginalURLForDownloadRequest does with the request here.
        networkProcess()->send(Messages::NetworkProcess::DownloadRequest(downloadProxy->downloadID(), request), 0);
        return downloadProxy;
    }
#endif

    m_processes[0]->send(Messages::WebProcess::DownloadRequest(downloadProxy->downloadID(), initiatingPageID, request), 0);
    return downloadProxy;
}

void WebContext::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    if (m_processes.isEmpty()) {
        if (m_processModel == ProcessModelSharedSecondaryProcess)
            m_messagesToInjectedBundlePostedToEmptyContext.append(std::make_pair(messageName, messageBody));
        return;
    }

    for (auto& process : m_processes) {
        // FIXME: Return early if the message body contains any references to WKPageRefs/WKFrameRefs etc. since they're local to a process.
        IPC::ArgumentEncoder messageData;
        messageData.encode(messageName);
        messageData.encode(WebContextUserMessageEncoder(messageBody, *process.get()));

        process->send(Messages::WebProcess::PostInjectedBundleMessage(IPC::DataReference(messageData.buffer(), messageData.bufferSize())), 0);
    }
}

// InjectedBundle client

void WebContext::didReceiveMessageFromInjectedBundle(const String& messageName, API::Object* messageBody)
{
    m_injectedBundleClient.didReceiveMessageFromInjectedBundle(this, messageName, messageBody);
}

void WebContext::didReceiveSynchronousMessageFromInjectedBundle(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    m_injectedBundleClient.didReceiveSynchronousMessageFromInjectedBundle(this, messageName, messageBody, returnData);
}

void WebContext::populateVisitedLinks()
{
    m_historyClient->populateVisitedLinks(this);
}

WebContext::Statistics& WebContext::statistics()
{
    static Statistics statistics = Statistics();

    return statistics;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebContext::setAdditionalPluginsDirectory(const String& directory)
{
    Vector<String> directories;
    directories.append(directory);

    m_pluginInfoStore.setAdditionalPluginsDirectories(directories);
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

void WebContext::setAlwaysUsesComplexTextCodePath(bool alwaysUseComplexText)
{
    m_alwaysUsesComplexTextCodePath = alwaysUseComplexText;
    sendToAllProcesses(Messages::WebProcess::SetAlwaysUsesComplexTextCodePath(alwaysUseComplexText));
}

void WebContext::setShouldUseFontSmoothing(bool useFontSmoothing)
{
    m_shouldUseFontSmoothing = useFontSmoothing;
    sendToAllProcesses(Messages::WebProcess::SetShouldUseFontSmoothing(useFontSmoothing));
}

void WebContext::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsEmptyDocument(urlScheme));
}

void WebContext::registerURLSchemeAsSecure(const String& urlScheme)
{
    m_schemesToRegisterAsSecure.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme));
}

void WebContext::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme)
{
    m_schemesToSetDomainRelaxationForbiddenFor.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::SetDomainRelaxationForbiddenForURLScheme(urlScheme));
}

void WebContext::registerURLSchemeAsLocal(const String& urlScheme)
{
    m_schemesToRegisterAsLocal.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsLocal(urlScheme));
}

void WebContext::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    m_schemesToRegisterAsNoAccess.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
}

void WebContext::registerURLSchemeAsDisplayIsolated(const String& urlScheme)
{
    m_schemesToRegisterAsDisplayIsolated.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsDisplayIsolated(urlScheme));
}

void WebContext::registerURLSchemeAsCORSEnabled(const String& urlScheme)
{
    m_schemesToRegisterAsCORSEnabled.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCORSEnabled(urlScheme));
}

#if ENABLE(CUSTOM_PROTOCOLS)
HashSet<String>& WebContext::globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String>> set;
    return set;
}

void WebContext::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    String schemeLower = urlScheme.lower();
    globalURLSchemesWithCustomProtocolHandlers().add(schemeLower);
    for (auto* context : allContexts())
        context->registerSchemeForCustomProtocol(schemeLower);
}

void WebContext::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    String schemeLower = urlScheme.lower();
    globalURLSchemesWithCustomProtocolHandlers().remove(schemeLower);
    for (auto* context : allContexts())
        context->unregisterSchemeForCustomProtocol(schemeLower);
}
#endif

#if ENABLE(CACHE_PARTITIONING)
void WebContext::registerURLSchemeAsCachePartitioned(const String& urlScheme)
{
    m_schemesToRegisterAsCachePartitioned.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCachePartitioned(urlScheme));
}
#endif

void WebContext::setCacheModel(CacheModel cacheModel)
{
    m_cacheModel = cacheModel;
    sendToAllProcesses(Messages::WebProcess::SetCacheModel(static_cast<uint32_t>(m_cacheModel)));

    // FIXME: Inform the Network Process if in use.
}

void WebContext::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    sendToAllProcesses(Messages::WebProcess::SetDefaultRequestTimeoutInterval(timeoutInterval));
}

void WebContext::addVisitedLink(const String& visitedURL)
{
    if (visitedURL.isEmpty())
        return;

    LinkHash linkHash = visitedLinkHash(visitedURL);
    addVisitedLinkHash(linkHash);
}

void WebContext::addVisitedLinkHash(LinkHash linkHash)
{
    m_visitedLinkProvider.addVisitedLink(linkHash);
}

DownloadProxy* WebContext::createDownloadProxy()
{
#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess()) {
        ensureNetworkProcess();
        ASSERT(m_networkProcess);
        return m_networkProcess->createDownloadProxy();
    }
#endif

    return ensureSharedWebProcess().createDownloadProxy();
}

void WebContext::addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void WebContext::addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void WebContext::removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

bool WebContext::dispatchMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool WebContext::dispatchSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void WebContext::didReceiveMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::WebContext::messageReceiverName()) {
        didReceiveWebContextMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == WebContextLegacyMessages::messageReceiverName()
        && decoder.messageName() == WebContextLegacyMessages::postMessageMessageName()) {
        String messageName;
        RefPtr<API::Object> messageBody;
        WebContextUserMessageDecoder messageBodyDecoder(messageBody, *WebProcessProxy::fromConnection(connection));
        if (!decoder.decode(messageName))
            return;
        if (!decoder.decode(messageBodyDecoder))
            return;

        didReceiveMessageFromInjectedBundle(messageName, messageBody.get());
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebContext::didReceiveSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (decoder.messageReceiverName() == Messages::WebContext::messageReceiverName()) {
        didReceiveSyncWebContextMessage(connection, decoder, replyEncoder);
        return;
    }

    if (decoder.messageReceiverName() == WebContextLegacyMessages::messageReceiverName()
        && decoder.messageName() == WebContextLegacyMessages::postSynchronousMessageMessageName()) {
        // FIXME: We should probably encode something in the case that the arguments do not decode correctly.

        WebProcessProxy* process = WebProcessProxy::fromConnection(connection);

        String messageName;
        RefPtr<API::Object> messageBody;
        WebContextUserMessageDecoder messageBodyDecoder(messageBody, *process);
        if (!decoder.decode(messageName))
            return;
        if (!decoder.decode(messageBodyDecoder))
            return;

        RefPtr<API::Object> returnData;
        didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody.get(), returnData);
        replyEncoder->encode(WebContextUserMessageEncoder(returnData.get(), *process));
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebContext::setEnhancedAccessibility(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetEnhancedAccessibility(flag));
}
    
void WebContext::startMemorySampler(const double interval)
{    
    // For new WebProcesses we will also want to start the Memory Sampler
    m_memorySamplerEnabled = true;
    m_memorySamplerInterval = interval;
    
    // For UIProcess
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::shared()->start(interval);
#endif
    
    // For WebProcess
    SandboxExtension::Handle sampleLogSandboxHandle;    
    double now = WTF::currentTime();
    String sampleLogFilePath = String::format("WebProcess%llu", static_cast<unsigned long long>(now));
    sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::ReadWrite, sampleLogSandboxHandle);
    
    sendToAllProcesses(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, interval));
}

void WebContext::stopMemorySampler()
{    
    // For WebProcess
    m_memorySamplerEnabled = false;
    
    // For UIProcess
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::shared()->stop();
#endif

    sendToAllProcesses(Messages::WebProcess::StopMemorySampler());
}

String WebContext::applicationCacheDirectory() const
{
    if (!m_overrideApplicationCacheDirectory.isEmpty())
        return m_overrideApplicationCacheDirectory;

    return platformDefaultApplicationCacheDirectory();
}

String WebContext::databaseDirectory() const
{
    if (!m_overrideDatabaseDirectory.isEmpty())
        return m_overrideDatabaseDirectory;

    return platformDefaultDatabaseDirectory();
}

void WebContext::setIconDatabasePath(const String& path)
{
    m_overrideIconDatabasePath = path;
    m_iconDatabase->setDatabasePath(path);
}

String WebContext::iconDatabasePath() const
{
    if (!m_overrideIconDatabasePath.isEmpty())
        return m_overrideIconDatabasePath;

    return platformDefaultIconDatabasePath();
}

void WebContext::setLocalStorageDirectory(const String& directory)
{
    m_overrideLocalStorageDirectory = directory;
    m_storageManager->setLocalStorageDirectory(localStorageDirectory());
}

String WebContext::localStorageDirectory() const
{
    if (!m_overrideLocalStorageDirectory.isEmpty())
        return m_overrideLocalStorageDirectory;

    return platformDefaultLocalStorageDirectory();
}

String WebContext::diskCacheDirectory() const
{
    if (!m_overrideDiskCacheDirectory.isEmpty())
        return m_overrideDiskCacheDirectory;

    return platformDefaultDiskCacheDirectory();
}

String WebContext::cookieStorageDirectory() const
{
    if (!m_overrideCookieStorageDirectory.isEmpty())
        return m_overrideCookieStorageDirectory;

    return platformDefaultCookieStorageDirectory();
}

void WebContext::useTestingNetworkSession()
{
    ASSERT(m_processes.isEmpty());
#if ENABLE(NETWORK_PROCESS)
    ASSERT(!m_networkProcess);

    if (m_networkProcess)
        return;
#endif

    if (!m_processes.isEmpty())
        return;

    m_shouldUseTestingNetworkSession = true;
}

void WebContext::allowSpecificHTTPSCertificateForHost(const WebCertificateInfo* certificate, const String& host)
{
#if ENABLE(NETWORK_PROCESS)
    if (m_usesNetworkProcess && m_networkProcess) {
        m_networkProcess->send(Messages::NetworkProcess::AllowSpecificHTTPSCertificateForHost(certificate->certificateInfo(), host), 0);
        return;
    }
#endif

#if USE(SOUP)
    m_processes[0]->send(Messages::WebProcess::AllowSpecificHTTPSCertificateForHost(certificate->certificateInfo(), host), 0);
    return;
#else
    UNUSED_PARAM(certificate);
    UNUSED_PARAM(host);
#endif

#if !PLATFORM(IOS)
    ASSERT_NOT_REACHED();
#endif
}

void WebContext::setHTTPPipeliningEnabled(bool enabled)
{
#if PLATFORM(COCOA)
    ResourceRequest::setHTTPPipeliningEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

bool WebContext::httpPipeliningEnabled() const
{
#if PLATFORM(COCOA)
    return ResourceRequest::httpPipeliningEnabled();
#else
    return false;
#endif
}

void WebContext::getStatistics(uint32_t statisticsMask, PassRefPtr<DictionaryCallback> callback)
{
    if (!statisticsMask) {
        callback->invalidate();
        return;
    }

    RefPtr<StatisticsRequest> request = StatisticsRequest::create(callback);

    if (statisticsMask & StatisticsRequestTypeWebContent)
        requestWebContentStatistics(request.get());
    
    if (statisticsMask & StatisticsRequestTypeNetworking)
        requestNetworkingStatistics(request.get());
}

void WebContext::requestWebContentStatistics(StatisticsRequest* request)
{
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        if (m_processes.isEmpty())
            return;
        
        uint64_t requestID = request->addOutstandingRequest();
        m_statisticsRequests.set(requestID, request);
        m_processes[0]->send(Messages::WebProcess::GetWebCoreStatistics(requestID), 0);

    } else {
        // FIXME (Multi-WebProcess) <rdar://problem/13200059>: Make getting statistics from multiple WebProcesses work.
    }
}

void WebContext::requestNetworkingStatistics(StatisticsRequest* request)
{
    bool networkProcessUnavailable;
#if ENABLE(NETWORK_PROCESS)
    networkProcessUnavailable = !m_usesNetworkProcess || !m_networkProcess;
#else
    networkProcessUnavailable = true;
#endif

    if (networkProcessUnavailable) {
        LOG_ERROR("Attempt to get NetworkProcess statistics but the NetworkProcess is unavailable");
        return;
    }

#if ENABLE(NETWORK_PROCESS)
    uint64_t requestID = request->addOutstandingRequest();
    m_statisticsRequests.set(requestID, request);
    m_networkProcess->send(Messages::NetworkProcess::GetNetworkProcessStatistics(requestID), 0);
#else
    UNUSED_PARAM(request);
#endif
}

#if !PLATFORM(COCOA)
void WebContext::dummy(bool&)
{
}
#endif

void WebContext::didGetStatistics(const StatisticsData& statisticsData, uint64_t requestID)
{
    RefPtr<StatisticsRequest> request = m_statisticsRequests.take(requestID);
    if (!request) {
        LOG_ERROR("Cannot report networking statistics.");
        return;
    }

    request->completedRequest(requestID, statisticsData);
}
    
void WebContext::garbageCollectJavaScriptObjects()
{
    sendToAllProcesses(Messages::WebProcess::GarbageCollectJavaScriptObjects());
}

void WebContext::setJavaScriptGarbageCollectorTimerEnabled(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetJavaScriptGarbageCollectorTimerEnabled(flag));
}

void WebContext::addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash)
{
    m_plugInAutoStartProvider.addAutoStartOriginHash(pageOrigin, plugInOriginHash);
}

void WebContext::plugInDidReceiveUserInteraction(unsigned plugInOriginHash)
{
    m_plugInAutoStartProvider.didReceiveUserInteraction(plugInOriginHash);
}

PassRefPtr<ImmutableDictionary> WebContext::plugInAutoStartOriginHashes() const
{
    return m_plugInAutoStartProvider.autoStartOriginsTableCopy();
}

void WebContext::setPlugInAutoStartOriginHashes(ImmutableDictionary& dictionary)
{
    m_plugInAutoStartProvider.setAutoStartOriginsTable(dictionary);
}

void WebContext::setPlugInAutoStartOrigins(API::Array& array)
{
    m_plugInAutoStartProvider.setAutoStartOriginsArray(array);
}

void WebContext::setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(ImmutableDictionary& dictionary, double time)
{
    m_plugInAutoStartProvider.setAutoStartOriginsFilteringOutEntriesAddedAfterTime(dictionary, time);
}

#if ENABLE(CUSTOM_PROTOCOLS)
void WebContext::registerSchemeForCustomProtocol(const String& scheme)
{
    sendToNetworkingProcess(Messages::CustomProtocolManager::RegisterScheme(scheme));
}

void WebContext::unregisterSchemeForCustomProtocol(const String& scheme)
{
    sendToNetworkingProcess(Messages::CustomProtocolManager::UnregisterScheme(scheme));
}
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebContext::pluginInfoStoreDidLoadPlugins(PluginInfoStore* store)
{
#ifdef NDEBUG
    UNUSED_PARAM(store);
#endif
    ASSERT(store == &m_pluginInfoStore);

    Vector<PluginModuleInfo> pluginModules = m_pluginInfoStore.plugins();

    Vector<RefPtr<API::Object>> plugins;
    plugins.reserveInitialCapacity(pluginModules.size());

    for (const auto& pluginModule : pluginModules) {
        ImmutableDictionary::MapType map;
        map.set(ASCIILiteral("path"), API::String::create(pluginModule.path));
        map.set(ASCIILiteral("name"), API::String::create(pluginModule.info.name));
        map.set(ASCIILiteral("file"), API::String::create(pluginModule.info.file));
        map.set(ASCIILiteral("desc"), API::String::create(pluginModule.info.desc));

        Vector<RefPtr<API::Object>> mimeTypes;
        mimeTypes.reserveInitialCapacity(pluginModule.info.mimes.size());
        for (const auto& mimeClassInfo : pluginModule.info.mimes)
            mimeTypes.uncheckedAppend(API::String::create(mimeClassInfo.type));
        map.set(ASCIILiteral("mimes"), API::Array::create(std::move(mimeTypes)));

#if PLATFORM(COCOA)
        map.set(ASCIILiteral("bundleId"), API::String::create(pluginModule.bundleIdentifier));
        map.set(ASCIILiteral("version"), API::String::create(pluginModule.versionString));
#endif

        plugins.uncheckedAppend(ImmutableDictionary::create(std::move(map)));
    }

    m_client.plugInInformationBecameAvailable(this, API::Array::create(std::move(plugins)).get());
}
#endif
    
void WebContext::setMemoryCacheDisabled(bool disabled)
{
    m_memoryCacheDisabled = disabled;
    sendToAllProcesses(Messages::WebProcess::SetMemoryCacheDisabled(disabled));
}

} // namespace WebKit
