/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#include "DownloadProxy.h"
#include "DownloadProxyMap.h"
#include "DownloadProxyMessages.h"
#include "ImmutableArray.h"
#include "Logging.h"
#include "MutableDictionary.h"
#include "SandboxExtension.h"
#include "StatisticsData.h"
#include "TextChecker.h"
#include "WKContextPrivate.h"
#include "WebApplicationCacheManagerProxy.h"
#include "WebContextMessageKinds.h"
#include "WebContextMessages.h"
#include "WebContextUserMessageCoders.h"
#include "WebCookieManagerProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseManagerProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebIconDatabase.h"
#include "WebKeyValueStorageManagerProxy.h"
#include "WebMediaCacheManagerProxy.h"
#include "WebNotificationManagerProxy.h"
#include "WebPluginSiteDataManager.h"
#include "WebPageGroup.h"
#include "WebMemorySampler.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebResourceCacheManagerProxy.h"
#include <WebCore/InitializeLogging.h>
#include <WebCore/Language.h>
#include <WebCore/LinkHash.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RunLoop.h>
#include <runtime/InitializeThreading.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryManagerProxy.h"
#endif

#if ENABLE(NETWORK_INFO)
#include "WebNetworkInfoManagerProxy.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessManager.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#endif

#if USE(SOUP)
#include "WebSoupRequestManagerProxy.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;

namespace WebKit {

static const double sharedSecondaryProcessShutdownTimeout = 60;

unsigned WebContext::m_privateBrowsingEnterCount;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webContextCounter, ("WebContext"));

PassRefPtr<WebContext> WebContext::create(const String& injectedBundlePath)
{
    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();
#if PLATFORM(MAC)
    WebContext::initializeProcessSuppressionSupport();
#endif
    return adoptRef(new WebContext(ProcessModelSharedSecondaryProcess, injectedBundlePath));
}

static Vector<WebContext*>& contexts()
{
    DEFINE_STATIC_LOCAL(Vector<WebContext*>, contexts, ());

    return contexts;
}

const Vector<WebContext*>& WebContext::allContexts()
{
    return contexts();
}

WebContext::WebContext(ProcessModel processModel, const String& injectedBundlePath)
    : m_processModel(processModel)
    , m_webProcessCountLimit(UINT_MAX)
    , m_haveInitialEmptyProcess(false)
    , m_defaultPageGroup(WebPageGroup::create())
    , m_injectedBundlePath(injectedBundlePath)
    , m_visitedLinkProvider(this)
    , m_plugInAutoStartProvider(this)
    , m_alwaysUsesComplexTextCodePath(false)
    , m_shouldUseFontSmoothing(true)
    , m_cacheModel(CacheModelDocumentViewer)
    , m_memorySamplerEnabled(false)
    , m_memorySamplerInterval(1400.0)
#if PLATFORM(WIN)
    , m_shouldPaintNativeControls(true)
    , m_initialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicyAlways)
#endif
#if USE(SOUP)
    , m_initialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain)
#endif
    , m_processTerminationEnabled(true)
#if ENABLE(NETWORK_PROCESS)
    , m_usesNetworkProcess(false)
#endif
{
    platformInitialize();

    addMessageReceiver(Messages::WebContext::messageReceiverName(), this);
    addMessageReceiver(CoreIPC::MessageKindTraits<WebContextLegacyMessage::Kind>::messageReceiverName(), this);

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    m_applicationCacheManagerProxy = WebApplicationCacheManagerProxy::create(this);
#if ENABLE(BATTERY_STATUS)
    m_batteryManagerProxy = WebBatteryManagerProxy::create(this);
#endif
    m_cookieManagerProxy = WebCookieManagerProxy::create(this);
#if ENABLE(SQL_DATABASE)
    m_databaseManagerProxy = WebDatabaseManagerProxy::create(this);
#endif
    m_geolocationManagerProxy = WebGeolocationManagerProxy::create(this);
    m_iconDatabase = WebIconDatabase::create(this);
    m_keyValueStorageManagerProxy = WebKeyValueStorageManagerProxy::create(this);
    m_mediaCacheManagerProxy = WebMediaCacheManagerProxy::create(this);
#if ENABLE(NETWORK_INFO)
    m_networkInfoManagerProxy = WebNetworkInfoManagerProxy::create(this);
#endif
    m_notificationManagerProxy = WebNotificationManagerProxy::create(this);
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginSiteDataManager = WebPluginSiteDataManager::create(this);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
    m_resourceCacheManagerProxy = WebResourceCacheManagerProxy::create(this);
#if USE(SOUP)
    m_soupRequestManagerProxy = WebSoupRequestManagerProxy::create(this);
#endif
    
    contexts().append(this);

    addLanguageChangeObserver(this, languageChanged);

#if !LOG_DISABLED
    WebCore::initializeLoggingChannelsIfNecessary();
    WebKit::initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED

#ifndef NDEBUG
    webContextCounter.increment();
#endif
}

#if !PLATFORM(MAC)
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

    m_applicationCacheManagerProxy->invalidate();
    m_applicationCacheManagerProxy->clearContext();

#if ENABLE(BATTERY_STATUS)
    m_batteryManagerProxy->invalidate();
    m_batteryManagerProxy->clearContext();
#endif

    m_cookieManagerProxy->invalidate();
    m_cookieManagerProxy->clearContext();

#if ENABLE(SQL_DATABASE)
    m_databaseManagerProxy->invalidate();
    m_databaseManagerProxy->clearContext();
#endif
    
    m_geolocationManagerProxy->invalidate();
    m_geolocationManagerProxy->clearContext();

    m_iconDatabase->invalidate();
    m_iconDatabase->clearContext();
    
    m_keyValueStorageManagerProxy->invalidate();
    m_keyValueStorageManagerProxy->clearContext();

    m_mediaCacheManagerProxy->invalidate();
    m_mediaCacheManagerProxy->clearContext();

#if ENABLE(NETWORK_INFO)
    m_networkInfoManagerProxy->invalidate();
    m_networkInfoManagerProxy->clearContext();
#endif
    
    m_notificationManagerProxy->invalidate();
    m_notificationManagerProxy->clearContext();

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginSiteDataManager->invalidate();
    m_pluginSiteDataManager->clearContext();
#endif

    m_resourceCacheManagerProxy->invalidate();
    m_resourceCacheManagerProxy->clearContext();

#if USE(SOUP)
    m_soupRequestManagerProxy->invalidate();
    m_soupRequestManagerProxy->clearContext();
#endif

    invalidateCallbackMap(m_dictionaryCallbacks);

    platformInvalidateContext();
    
#ifndef NDEBUG
    webContextCounter.decrement();
#endif
}

void WebContext::initializeInjectedBundleClient(const WKContextInjectedBundleClient* client)
{
    m_injectedBundleClient.initialize(client);
}

void WebContext::initializeConnectionClient(const WKContextConnectionClient* client)
{
    m_connectionClient.initialize(client);
}

void WebContext::initializeHistoryClient(const WKContextHistoryClient* client)
{
    m_historyClient.initialize(client);

    sendToAllProcesses(Messages::WebProcess::SetShouldTrackVisitedLinks(m_historyClient.shouldTrackVisitedLinks()));
}

void WebContext::initializeDownloadClient(const WKContextDownloadClient* client)
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

#if !ENABLE(PLUGIN_PROCESS)
    // Plugin process is required for multiple web process mode.
    if (processModel != ProcessModelSharedSecondaryProcess)
        CRASH();
#endif

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

WebProcessProxy* WebContext::deprecatedSharedProcess()
{
    ASSERT(m_processModel == ProcessModelSharedSecondaryProcess);
    if (m_processes.isEmpty())
        return 0;
    return m_processes[0].get();
}

void WebContext::languageChanged(void* context)
{
    static_cast<WebContext*>(context)->languageChanged();
}

void WebContext::languageChanged()
{
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged(userPreferredLanguages()));
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
static bool anyContextUsesNetworkProcess()
{
    const Vector<WebContext*>& contexts = WebContext::allContexts();
    for (size_t i = 0, count = contexts.size(); i < count; ++i)
    if (contexts[i]->usesNetworkProcess())
        return true;

    return false;
}
#endif

void WebContext::willStartUsingPrivateBrowsing()
{
    if (m_privateBrowsingEnterCount++)
        return;

#if ENABLE(NETWORK_PROCESS)
    if (anyContextUsesNetworkProcess())
        NetworkProcessManager::shared().process()->send(Messages::NetworkProcess::EnsurePrivateBrowsingSession(), 0);
#endif

    const Vector<WebContext*>& contexts = allContexts();
    for (size_t i = 0, count = contexts.size(); i < count; ++i)
        contexts[i]->sendToAllProcesses(Messages::WebProcess::EnsurePrivateBrowsingSession());
}

void WebContext::willStopUsingPrivateBrowsing()
{
    // If the client asks to disable private browsing without enabling it first, it may be resetting a persistent preference,
    // so it is still necessary to destroy any existing private browsing session.
    if (m_privateBrowsingEnterCount && --m_privateBrowsingEnterCount)
        return;

#if ENABLE(NETWORK_PROCESS)
    if (anyContextUsesNetworkProcess())
        NetworkProcessManager::shared().process()->send(Messages::NetworkProcess::DestroyPrivateBrowsingSession(), 0);
#endif

    const Vector<WebContext*>& contexts = allContexts();
    for (size_t i = 0, count = contexts.size(); i < count; ++i)
        contexts[i]->sendToAllProcesses(Messages::WebProcess::DestroyPrivateBrowsingSession());
}

WebProcessProxy* WebContext::ensureSharedWebProcess()
{
    ASSERT(m_processModel == ProcessModelSharedSecondaryProcess);
    if (m_processes.isEmpty())
        createNewWebProcess();
    return m_processes[0].get();
}

WebProcessProxy* WebContext::createNewWebProcess()
{
#if ENABLE(NETWORK_PROCESS)
    if (m_usesNetworkProcess)
        NetworkProcessManager::shared().ensureNetworkProcess();
#endif

    RefPtr<WebProcessProxy> process = WebProcessProxy::create(this);

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
        SandboxExtension::createHandle(parameters.cookieStorageDirectory, SandboxExtension::ReadWrite, parameters.cookieStorageDirectoryExtensionHandle);

    parameters.shouldTrackVisitedLinks = m_historyClient.shouldTrackVisitedLinks();
    parameters.cacheModel = m_cacheModel;
    parameters.languages = userPreferredLanguages();

    copyToVector(m_schemesToRegisterAsEmptyDocument, parameters.urlSchemesRegistererdAsEmptyDocument);
    copyToVector(m_schemesToRegisterAsSecure, parameters.urlSchemesRegisteredAsSecure);
    copyToVector(m_schemesToSetDomainRelaxationForbiddenFor, parameters.urlSchemesForWhichDomainRelaxationIsForbidden);
    copyToVector(m_schemesToRegisterAsLocal, parameters.urlSchemesRegisteredAsLocal);
    copyToVector(m_schemesToRegisterAsNoAccess, parameters.urlSchemesRegisteredAsNoAccess);
    copyToVector(m_schemesToRegisterAsDisplayIsolated, parameters.urlSchemesRegisteredAsDisplayIsolated);
    copyToVector(m_schemesToRegisterAsCORSEnabled, parameters.urlSchemesRegisteredAsCORSEnabled);

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;
    
    parameters.iconDatabaseEnabled = !iconDatabasePath().isEmpty();

    parameters.terminationTimeout = (m_processModel == ProcessModelSharedSecondaryProcess) ? sharedSecondaryProcessShutdownTimeout : 0;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = WebURLRequest::defaultTimeoutInterval();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    m_notificationManagerProxy->populateCopyOfNotificationPermissions(parameters.notificationPermissions);
#endif

#if ENABLE(NETWORK_PROCESS)
    parameters.usesNetworkProcess = m_usesNetworkProcess;
#endif

    parameters.plugInAutoStartOrigins = m_plugInAutoStartProvider.autoStartOriginsCopy();

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    RefPtr<APIObject> injectedBundleInitializationUserData = m_injectedBundleClient.getInjectedBundleInitializationUserData(this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    process->send(Messages::WebProcess::InitializeWebProcess(parameters, WebContextUserMessageEncoder(injectedBundleInitializationUserData.get())), 0);

    m_processes.append(process);

    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        for (size_t i = 0; i != m_messagesToInjectedBundlePostedToEmptyContext.size(); ++i) {
            pair<String, RefPtr<APIObject> >& message = m_messagesToInjectedBundlePostedToEmptyContext[i];

            OwnPtr<CoreIPC::ArgumentEncoder> messageData = CoreIPC::ArgumentEncoder::create();

            messageData->encode(message.first);
            messageData->encode(WebContextUserMessageEncoder(message.second.get()));
            process->send(Messages::WebProcess::PostInjectedBundleMessage(CoreIPC::DataReference(messageData->buffer(), messageData->bufferSize())), 0);
        }
        m_messagesToInjectedBundlePostedToEmptyContext.clear();
    } else
        ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

    return process.get();
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
    Vector<RefPtr<WebProcessProxy> > processes = m_processes;
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

    if (!m_downloads.isEmpty())
        return false;

    if (!m_applicationCacheManagerProxy->shouldTerminate(process))
        return false;
    if (!m_cookieManagerProxy->shouldTerminate(process))
        return false;
#if ENABLE(SQL_DATABASE)
    if (!m_databaseManagerProxy->shouldTerminate(process))
        return false;
#endif
    if (!m_keyValueStorageManagerProxy->shouldTerminate(process))
        return false;
    if (!m_mediaCacheManagerProxy->shouldTerminate(process))
        return false;
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (!m_pluginSiteDataManager->shouldTerminate(process))
        return false;
#endif
    if (!m_resourceCacheManagerProxy->shouldTerminate(process))
        return false;

    return true;
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
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::WriteOnly, sampleLogSandboxHandle);
        
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

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> All the invalidation calls below are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    if (m_processModel != ProcessModelSharedSecondaryProcess) {
        RefPtr<WebProcessProxy> protect(process);
        m_processes.remove(m_processes.find(process));
        return;
    }

    // Invalidate all outstanding downloads.
    for (HashMap<uint64_t, RefPtr<DownloadProxy> >::iterator::Values it = m_downloads.begin().values(), end = m_downloads.end().values(); it != end; ++it) {
        (*it)->processDidClose();
        (*it)->invalidate();
    }

    m_downloads.clear();

    m_applicationCacheManagerProxy->invalidate();
#if ENABLE(BATTERY_STATUS)
    m_batteryManagerProxy->invalidate();
#endif
    m_cookieManagerProxy->invalidate();
#if ENABLE(SQL_DATABASE)
    m_databaseManagerProxy->invalidate();
#endif
    m_geolocationManagerProxy->invalidate();
    m_keyValueStorageManagerProxy->invalidate();
    m_mediaCacheManagerProxy->invalidate();
#if ENABLE(NETWORK_INFO)
    m_networkInfoManagerProxy->invalidate();
#endif
    m_notificationManagerProxy->invalidate();
    m_resourceCacheManagerProxy->invalidate();
#if USE(SOUP)
    m_soupRequestManagerProxy->invalidate();
#endif

    // When out of process plug-ins are enabled, we don't want to invalidate the plug-in site data
    // manager just because the web process crashes since it's not involved.
#if ENABLE(NETSCAPE_PLUGIN_API) && !ENABLE(PLUGIN_PROCESS)
    m_pluginSiteDataManager->invalidate();
#endif

    // The vector may have the last reference to process proxy, which in turn may have the last reference to the context.
    // Since vector elements are destroyed in place, we would recurse into WebProcessProxy destructor
    // if it were invoked from Vector::remove(). RefPtr delays destruction until it's safe.
    RefPtr<WebProcessProxy> protect(process);
    m_processes.remove(m_processes.find(process));
}

WebProcessProxy* WebContext::createNewWebProcessRespectingProcessCountLimit()
{
    if (m_processes.size() < m_webProcessCountLimit)
        return createNewWebProcess();

    // Choose a process with fewest pages, to achieve flat distribution.
    WebProcessProxy* result = 0;
    unsigned fewestPagesSeen = UINT_MAX;
    for (unsigned i = 0; i < m_processes.size(); ++i) {
        if (fewestPagesSeen > m_processes[i]->pages().size()) {
            result = m_processes[i].get();
            fewestPagesSeen = m_processes[i]->pages().size();
        }
    }
    return result;
}

PassRefPtr<WebPageProxy> WebContext::createWebPage(PageClient* pageClient, WebPageGroup* pageGroup, WebPageProxy* relatedPage)
{
    RefPtr<WebProcessProxy> process;
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        process = ensureSharedWebProcess();
    } else {
        if (m_haveInitialEmptyProcess) {
            process = m_processes.last();
            m_haveInitialEmptyProcess = false;
        } else if (relatedPage) {
            // Sharing processes, e.g. when creating the page via window.open().
            process = relatedPage->process();
        } else
            process = createNewWebProcessRespectingProcessCountLimit();
    }

    if (!pageGroup)
        pageGroup = m_defaultPageGroup.get();

    return process->createWebPage(pageClient, this, pageGroup);
}

DownloadProxy* WebContext::download(WebPageProxy* initiatingPage, const ResourceRequest& request)
{
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        ensureSharedWebProcess();

        DownloadProxy* download = createDownloadProxy();
        uint64_t initiatingPageID = initiatingPage ? initiatingPage->pageID() : 0;

#if PLATFORM(QT)
        ASSERT(initiatingPage); // Our design does not suppport downloads without a WebPage.
        initiatingPage->handleDownloadRequest(download);
#endif

        m_processes[0]->send(Messages::WebProcess::DownloadRequest(download->downloadID(), initiatingPageID, request), 0);
        return download;

    } else {
        // FIXME (Multi-WebProcess): <rdar://problem/12239483> Make downloading work.
        return 0;
    }
}

void WebContext::postMessageToInjectedBundle(const String& messageName, APIObject* messageBody)
{
    if (m_processes.isEmpty()) {
        if (m_processModel == ProcessModelSharedSecondaryProcess)
            m_messagesToInjectedBundlePostedToEmptyContext.append(std::make_pair(messageName, messageBody));
        return;
    }

    // FIXME: Return early if the message body contains any references to WKPageRefs/WKFrameRefs etc. since they're local to a process.

    OwnPtr<CoreIPC::ArgumentEncoder> messageData = CoreIPC::ArgumentEncoder::create();
    messageData->encode(messageName);
    messageData->encode(WebContextUserMessageEncoder(messageBody));

    for (size_t i = 0; i < m_processes.size(); ++i) {
        m_processes[i]->send(Messages::WebProcess::PostInjectedBundleMessage(CoreIPC::DataReference(messageData->buffer(), messageData->bufferSize())), 0);
    }
}

// InjectedBundle client

void WebContext::didReceiveMessageFromInjectedBundle(const String& messageName, APIObject* messageBody)
{
    m_injectedBundleClient.didReceiveMessageFromInjectedBundle(this, messageName, messageBody);
}

void WebContext::didReceiveSynchronousMessageFromInjectedBundle(const String& messageName, APIObject* messageBody, RefPtr<APIObject>& returnData)
{
    m_injectedBundleClient.didReceiveSynchronousMessageFromInjectedBundle(this, messageName, messageBody, returnData);
}

void WebContext::populateVisitedLinks()
{
    m_historyClient.populateVisitedLinks(this);
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

void WebContext::setCacheModel(CacheModel cacheModel)
{
    m_cacheModel = cacheModel;
    sendToAllProcesses(Messages::WebProcess::SetCacheModel(static_cast<uint32_t>(m_cacheModel)));
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
    DownloadProxy* downloadProxy = DownloadProxyMap::shared().createDownloadProxy(this);
    m_downloads.set(downloadProxy->downloadID(), downloadProxy);
    addMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadProxy->downloadID(), downloadProxy);
    return downloadProxy;
}

void WebContext::downloadFinished(DownloadProxy* downloadProxy)
{
    ASSERT(m_downloads.contains(downloadProxy->downloadID()));

    downloadProxy->invalidate();
    removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadProxy->downloadID());
    m_downloads.remove(downloadProxy->downloadID());
}

// FIXME: This is not the ideal place for this function.
HashSet<String, CaseFoldingHash> WebContext::pdfAndPostScriptMIMETypes()
{
    HashSet<String, CaseFoldingHash> mimeTypes;

    mimeTypes.add("application/pdf");
    mimeTypes.add("application/postscript");
    mimeTypes.add("text/pdf");
    
    return mimeTypes;
}

void WebContext::addMessageReceiver(CoreIPC::StringReference messageReceiverName, CoreIPC::MessageReceiver* messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void WebContext::addMessageReceiver(CoreIPC::StringReference messageReceiverName, uint64_t destinationID, CoreIPC::MessageReceiver* messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void WebContext::removeMessageReceiver(CoreIPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

bool WebContext::dispatchMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, messageID, decoder);
}

bool WebContext::dispatchSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, messageID, decoder, replyEncoder);
}

void WebContext::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (messageID.is<CoreIPC::MessageClassWebContext>()) {
        didReceiveWebContextMessage(connection, messageID, decoder);
        return;
    }

    switch (messageID.get<WebContextLegacyMessage::Kind>()) {
        case WebContextLegacyMessage::PostMessage: {
            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageBodyDecoder(messageBody, WebProcessProxy::fromConnection(connection));
            if (!decoder.decode(messageName))
                return;
            if (!decoder.decode(messageBodyDecoder))
                return;
            
            didReceiveMessageFromInjectedBundle(messageName, messageBody.get());
            return;
        }
        case WebContextLegacyMessage::PostSynchronousMessage:
            ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
}

void WebContext::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder)
{
    if (messageID.is<CoreIPC::MessageClassWebContext>()) {
        didReceiveSyncWebContextMessage(connection, messageID, decoder, replyEncoder);
        return;
    }

    switch (messageID.get<WebContextLegacyMessage::Kind>()) {
        case WebContextLegacyMessage::PostSynchronousMessage: {
            // FIXME: We should probably encode something in the case that the arguments do not decode correctly.

            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageBodyDecoder(messageBody, WebProcessProxy::fromConnection(connection));
            if (!decoder.decode(messageName))
                return;
            if (!decoder.decode(messageBodyDecoder))
                return;

            RefPtr<APIObject> returnData;
            didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody.get(), returnData);
            replyEncoder->encode(WebContextUserMessageEncoder(returnData.get()));
            return;
        }
        case WebContextLegacyMessage::PostMessage:
            ASSERT_NOT_REACHED();
    }
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
    sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::WriteOnly, sampleLogSandboxHandle);
    
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

void WebContext::setHTTPPipeliningEnabled(bool enabled)
{
#if PLATFORM(MAC)
    ResourceRequest::setHTTPPipeliningEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

bool WebContext::httpPipeliningEnabled() const
{
#if PLATFORM(MAC)
    return ResourceRequest::httpPipeliningEnabled();
#else
    return false;
#endif
}

void WebContext::getWebCoreStatistics(PassRefPtr<DictionaryCallback> callback)
{
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        if (m_processes.isEmpty()) {
            callback->invalidate();
            return;
        }
        
        uint64_t callbackID = callback->callbackID();
        m_dictionaryCallbacks.set(callbackID, callback.get());
        m_processes[0]->send(Messages::WebProcess::GetWebCoreStatistics(callbackID), 0);

    } else {
        // FIXME (Multi-WebProcess): <rdar://problem/12239483> Make downloading work.
        callback->invalidate();
    }
}

static PassRefPtr<MutableDictionary> createDictionaryFromHashMap(const HashMap<String, uint64_t>& map)
{
    RefPtr<MutableDictionary> result = MutableDictionary::create();
    HashMap<String, uint64_t>::const_iterator end = map.end();
    for (HashMap<String, uint64_t>::const_iterator it = map.begin(); it != end; ++it)
        result->set(it->key, RefPtr<WebUInt64>(WebUInt64::create(it->value)).get());
    
    return result;
}

#if !PLATFORM(MAC)
void WebContext::dummy(bool&)
{
}
#endif

void WebContext::didGetWebCoreStatistics(const StatisticsData& statisticsData, uint64_t callbackID)
{
    RefPtr<DictionaryCallback> callback = m_dictionaryCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    RefPtr<MutableDictionary> statistics = createDictionaryFromHashMap(statisticsData.statisticsNumbers);
    statistics->set("JavaScriptProtectedObjectTypeCounts", createDictionaryFromHashMap(statisticsData.javaScriptProtectedObjectTypeCounts).get());
    statistics->set("JavaScriptObjectTypeCounts", createDictionaryFromHashMap(statisticsData.javaScriptObjectTypeCounts).get());
    
    size_t cacheStatisticsCount = statisticsData.webCoreCacheStatistics.size();
    Vector<RefPtr<APIObject> > cacheStatisticsVector(cacheStatisticsCount);
    for (size_t i = 0; i < cacheStatisticsCount; ++i)
        cacheStatisticsVector[i] = createDictionaryFromHashMap(statisticsData.webCoreCacheStatistics[i]);
    statistics->set("WebCoreCacheStatistics", ImmutableArray::adopt(cacheStatisticsVector).get());
    
    callback->performCallbackWithReturnValue(statistics.get());
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
    m_plugInAutoStartProvider.addAutoStartOrigin(pageOrigin, plugInOriginHash);
}

} // namespace WebKit
