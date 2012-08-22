/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "ImmutableArray.h"
#include "InjectedBundleMessageKinds.h"
#include "Logging.h"
#include "MutableDictionary.h"
#include "SandboxExtension.h"
#include "StatisticsData.h"
#include "TextChecker.h"
#include "WKContextPrivate.h"
#include "WebApplicationCacheManagerProxy.h"
#include "WebContextMessageKinds.h"
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
#include <WebCore/Language.h>
#include <WebCore/LinkHash.h>
#include <WebCore/Logging.h>
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

#if USE(SOUP)
#include "WebSoupRequestManagerProxy.h"
#endif

#if ENABLE(VIBRATION)
#include "WebVibrationProxy.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;

namespace WebKit {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webContextCounter, ("WebContext"));

PassRefPtr<WebContext> WebContext::create(const String& injectedBundlePath)
{
    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();
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
    , m_defaultPageGroup(WebPageGroup::create())
    , m_injectedBundlePath(injectedBundlePath)
    , m_visitedLinkProvider(this)
    , m_alwaysUsesComplexTextCodePath(false)
    , m_shouldUseFontSmoothing(true)
    , m_cacheModel(CacheModelDocumentViewer)
    , m_memorySamplerEnabled(false)
    , m_memorySamplerInterval(1400.0)
    , m_applicationCacheManagerProxy(WebApplicationCacheManagerProxy::create(this))
#if ENABLE(BATTERY_STATUS)
    , m_batteryManagerProxy(WebBatteryManagerProxy::create(this))
#endif
    , m_cookieManagerProxy(WebCookieManagerProxy::create(this))
#if ENABLE(SQL_DATABASE)
    , m_databaseManagerProxy(WebDatabaseManagerProxy::create(this))
#endif
    , m_geolocationManagerProxy(WebGeolocationManagerProxy::create(this))
    , m_iconDatabase(WebIconDatabase::create(this))
    , m_keyValueStorageManagerProxy(WebKeyValueStorageManagerProxy::create(this))
    , m_mediaCacheManagerProxy(WebMediaCacheManagerProxy::create(this))
#if ENABLE(NETWORK_INFO)
    , m_networkInfoManagerProxy(WebNetworkInfoManagerProxy::create(this))
#endif
    , m_notificationManagerProxy(WebNotificationManagerProxy::create(this))
    , m_pluginSiteDataManager(WebPluginSiteDataManager::create(this))
    , m_resourceCacheManagerProxy(WebResourceCacheManagerProxy::create(this))
#if USE(SOUP)
    , m_soupRequestManagerProxy(WebSoupRequestManagerProxy::create(this))
#endif
#if ENABLE(VIBRATION)
    , m_vibrationProxy(WebVibrationProxy::create(this))
#endif
#if PLATFORM(WIN)
    , m_shouldPaintNativeControls(true)
    , m_initialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicyAlways)
#endif
    , m_processTerminationEnabled(true)
{
#if !LOG_DISABLED
    WebKit::initializeLogChannelsIfNecessary();
#endif

    contexts().append(this);

    addLanguageChangeObserver(this, languageChanged);

#if !LOG_DISABLED
    WebCore::initializeLoggingChannelsIfNecessary();
#endif // !LOG_DISABLED

#ifndef NDEBUG
    webContextCounter.increment();
#endif
}

WebContext::~WebContext()
{
    ASSERT(contexts().find(this) != notFound);
    contexts().remove(contexts().find(this));

    removeLanguageChangeObserver(this);

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

    m_pluginSiteDataManager->invalidate();
    m_pluginSiteDataManager->clearContext();

    m_resourceCacheManagerProxy->invalidate();
    m_resourceCacheManagerProxy->clearContext();

#if USE(SOUP)
    m_soupRequestManagerProxy->invalidate();
    m_soupRequestManagerProxy->clearContext();
#endif

#if ENABLE(VIBRATION)
    m_vibrationProxy->invalidate();
    m_vibrationProxy->clearContext();
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

void WebContext::ensureSharedWebProcess()
{
    if (m_processes.isEmpty())
        m_processes.append(createNewWebProcess());
}

PassRefPtr<WebProcessProxy> WebContext::createNewWebProcess()
{
    RefPtr<WebProcessProxy> process = WebProcessProxy::create(this);

    WebProcessCreationParameters parameters;

    if (!injectedBundlePath().isEmpty()) {
        parameters.injectedBundlePath = injectedBundlePath();
        SandboxExtension::createHandle(parameters.injectedBundlePath, SandboxExtension::ReadOnly, parameters.injectedBundlePathExtensionHandle);
    }

    parameters.shouldTrackVisitedLinks = m_historyClient.shouldTrackVisitedLinks();
    parameters.cacheModel = m_cacheModel;
    parameters.languages = userPreferredLanguages();
    parameters.applicationCacheDirectory = applicationCacheDirectory();
    parameters.databaseDirectory = databaseDirectory();
    parameters.localStorageDirectory = localStorageDirectory();

#if PLATFORM(MAC)
    parameters.presenterApplicationPid = getpid();
#endif

    copyToVector(m_schemesToRegisterAsEmptyDocument, parameters.urlSchemesRegistererdAsEmptyDocument);
    copyToVector(m_schemesToRegisterAsSecure, parameters.urlSchemesRegisteredAsSecure);
    copyToVector(m_schemesToSetDomainRelaxationForbiddenFor, parameters.urlSchemesForWhichDomainRelaxationIsForbidden);

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;
    
    parameters.iconDatabaseEnabled = !iconDatabasePath().isEmpty();

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = WebURLRequest::defaultTimeoutInterval();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    m_notificationManagerProxy->populateCopyOfNotificationPermissions(parameters.notificationPermissions);
#endif

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    RefPtr<APIObject> injectedBundleInitializationUserData = m_injectedBundleClient.getInjectedBundleInitializationUserData(this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    process->send(Messages::WebProcess::InitializeWebProcess(parameters, WebContextUserMessageEncoder(injectedBundleInitializationUserData.get())), 0);

    for (size_t i = 0; i != m_pendingMessagesToPostToInjectedBundle.size(); ++i) {
        pair<String, RefPtr<APIObject> >& message = m_pendingMessagesToPostToInjectedBundle[i];
        process->deprecatedSend(InjectedBundleMessage::PostMessage, 0, CoreIPC::In(message.first, WebContextUserMessageEncoder(message.second.get())));
    }
    // FIXME (Multi-WebProcess): What does this mean in the brave new world?
    m_pendingMessagesToPostToInjectedBundle.clear();

    return process.release();
}

void WebContext::warmInitialProcess()  
{
    ASSERT(m_processes.isEmpty());
    m_processes.append(createNewWebProcess());
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
    if (!m_pluginSiteDataManager->shouldTerminate(process))
        return false;
    if (!m_resourceCacheManagerProxy->shouldTerminate(process))
        return false;

    return true;
}

void WebContext::processDidFinishLaunching(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    m_visitedLinkProvider.processDidFinishLaunching();

    // Sometimes the memorySampler gets initialized after process initialization has happened but before the process has finished launching
    // so check if it needs to be started here
    if (m_memorySamplerEnabled) {
        SandboxExtension::Handle sampleLogSandboxHandle;        
        double now = WTF::currentTime();
        String sampleLogFilePath = String::format("WebProcess%llu", static_cast<unsigned long long>(now));
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::WriteOnly, sampleLogSandboxHandle);
        
        process->send(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    m_connectionClient.didCreateConnection(this, process->webConnection());
}

void WebContext::disconnectProcess(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    m_visitedLinkProvider.processDidClose();

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
#if ENABLE(VIBRATION)
    m_vibrationProxy->invalidate();
#endif

    // When out of process plug-ins are enabled, we don't want to invalidate the plug-in site data
    // manager just because the web process crashes since it's not involved.
#if !ENABLE(PLUGIN_PROCESS)
    m_pluginSiteDataManager->invalidate();
#endif

    // This can cause the web context to be destroyed.
    m_processes.remove(m_processes.find(process));
}

PassRefPtr<WebPageProxy> WebContext::createWebPage(PageClient* pageClient, WebPageGroup* pageGroup)
{
    RefPtr<WebProcessProxy> process;
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        ensureSharedWebProcess();
        process = m_processes[0];
    } else {
        // FIXME (Multi-WebProcess): Add logic for sharing a process.
        process = createNewWebProcess();
        m_processes.append(process);
    }

    if (!pageGroup)
        pageGroup = m_defaultPageGroup.get();

    return process->createWebPage(pageClient, this, pageGroup);
}

WebProcessProxy* WebContext::relaunchProcessIfNecessary()
{
    if (m_processModel == ProcessModelSharedSecondaryProcess) {
        ensureSharedWebProcess();
        return m_processes[0].get();
    } else {
        // FIXME (Multi-WebProcess): What should this do in this model?
        return 0;
    }
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
        // FIXME: (Multi-WebProcess): Implement.
        return 0;
    }
}

void WebContext::postMessageToInjectedBundle(const String& messageName, APIObject* messageBody)
{
    if (m_processes.isEmpty()) {
        m_pendingMessagesToPostToInjectedBundle.append(std::make_pair(messageName, messageBody));
        return;
    }

    for (size_t i = 0; i < m_processes.size(); ++i) {
        // FIXME (Multi-WebProcess): Evolve m_pendingMessagesToPostToInjectedBundle to work with multiple secondary processes.
        if (!m_processes[i]->canSendMessage()) {
            m_pendingMessagesToPostToInjectedBundle.append(std::make_pair(messageName, messageBody));
            continue;
        }
        // FIXME: We should consider returning false from this function if the messageBody cannot be encoded.
        m_processes[i]->deprecatedSend(InjectedBundleMessage::PostMessage, 0, CoreIPC::In(messageName, WebContextUserMessageEncoder(messageBody)));
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

void WebContext::setAdditionalPluginsDirectory(const String& directory)
{
    Vector<String> directories;
    directories.append(directory);

    m_pluginInfoStore.setAdditionalPluginsDirectories(directories);
}

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

    LinkHash linkHash = visitedLinkHash(visitedURL.characters(), visitedURL.length());
    addVisitedLinkHash(linkHash);
}

void WebContext::addVisitedLinkHash(LinkHash linkHash)
{
    m_visitedLinkProvider.addVisitedLink(linkHash);
}

DownloadProxy* WebContext::createDownloadProxy()
{
    RefPtr<DownloadProxy> downloadProxy = DownloadProxy::create(this);
    m_downloads.set(downloadProxy->downloadID(), downloadProxy);
    return downloadProxy.get();
}

void WebContext::downloadFinished(DownloadProxy* downloadProxy)
{
    ASSERT(m_downloads.contains(downloadProxy->downloadID()));

    downloadProxy->invalidate();
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

void WebContext::didReceiveMessage(WebProcessProxy* process, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebContext>()) {
        didReceiveWebContextMessage(process->connection(), messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassDownloadProxy>()) {
        if (DownloadProxy* downloadProxy = m_downloads.get(arguments->destinationID()).get())
            downloadProxy->didReceiveDownloadProxyMessage(process->connection(), messageID, arguments);
        
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebApplicationCacheManagerProxy>()) {
        m_applicationCacheManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }

#if ENABLE(BATTERY_STATUS)
    if (messageID.is<CoreIPC::MessageClassWebBatteryManagerProxy>()) {
        m_batteryManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }
#endif

    if (messageID.is<CoreIPC::MessageClassWebCookieManagerProxy>()) {
        m_cookieManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }

#if ENABLE(SQL_DATABASE)
    if (messageID.is<CoreIPC::MessageClassWebDatabaseManagerProxy>()) {
        m_databaseManagerProxy->didReceiveWebDatabaseManagerProxyMessage(process->connection(), messageID, arguments);
        return;
    }
#endif

    if (messageID.is<CoreIPC::MessageClassWebGeolocationManagerProxy>()) {
        m_geolocationManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }
    
    if (messageID.is<CoreIPC::MessageClassWebIconDatabase>()) {
        m_iconDatabase->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebKeyValueStorageManagerProxy>()) {
        m_keyValueStorageManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebMediaCacheManagerProxy>()) {
        m_mediaCacheManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }

#if ENABLE(NETWORK_INFO)
    if (messageID.is<CoreIPC::MessageClassWebNetworkInfoManagerProxy>()) {
        m_networkInfoManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }
#endif
    
    if (messageID.is<CoreIPC::MessageClassWebNotificationManagerProxy>()) {
        m_notificationManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebResourceCacheManagerProxy>()) {
        m_resourceCacheManagerProxy->didReceiveWebResourceCacheManagerProxyMessage(process->connection(), messageID, arguments);
        return;
    }

#if USE(SOUP)
    if (messageID.is<CoreIPC::MessageClassWebSoupRequestManagerProxy>()) {
        m_soupRequestManagerProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }
#endif

#if ENABLE(VIBRATION)
    if (messageID.is<CoreIPC::MessageClassWebVibrationProxy>()) {
        m_vibrationProxy->didReceiveMessage(process->connection(), messageID, arguments);
        return;
    }
#endif

    switch (messageID.get<WebContextLegacyMessage::Kind>()) {
        case WebContextLegacyMessage::PostMessage: {
            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageDecoder(messageBody, process);
            if (!arguments->decode(CoreIPC::Out(messageName, messageDecoder)))
                return;

            didReceiveMessageFromInjectedBundle(messageName, messageBody.get());
            return;
        }
        case WebContextLegacyMessage::PostSynchronousMessage:
            ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
}

void WebContext::didReceiveSyncMessage(WebProcessProxy* process, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{
    if (messageID.is<CoreIPC::MessageClassWebContext>()) {
        didReceiveSyncWebContextMessage(process->connection(), messageID, arguments, reply);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassDownloadProxy>()) {
        if (DownloadProxy* downloadProxy = m_downloads.get(arguments->destinationID()).get())
            downloadProxy->didReceiveSyncDownloadProxyMessage(process->connection(), messageID, arguments, reply);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebIconDatabase>()) {
        m_iconDatabase->didReceiveSyncMessage(process->connection(), messageID, arguments, reply);
        return;
    }

#if ENABLE(NETWORK_INFO)
    if (messageID.is<CoreIPC::MessageClassWebNetworkInfoManagerProxy>()) {
        m_networkInfoManagerProxy->didReceiveSyncMessage(process->connection(), messageID, arguments, reply);
        return;
    }
#endif
    
    switch (messageID.get<WebContextLegacyMessage::Kind>()) {
        case WebContextLegacyMessage::PostSynchronousMessage: {
            // FIXME: We should probably encode something in the case that the arguments do not decode correctly.

            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageDecoder(messageBody, process);
            if (!arguments->decode(CoreIPC::Out(messageName, messageDecoder)))
                return;

            RefPtr<APIObject> returnData;
            didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody.get(), returnData);
            reply->encode(CoreIPC::In(WebContextUserMessageEncoder(returnData.get())));
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

void WebContext::setHTTPPipeliningEnabled(bool enabled)
{
#if PLATFORM(MAC)
    ResourceRequest::setHTTPPipeliningEnabled(enabled);
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
        // FIXME (Multi-WebProcess): Implement.
        callback->invalidate();
    }
}

static PassRefPtr<MutableDictionary> createDictionaryFromHashMap(const HashMap<String, uint64_t>& map)
{
    RefPtr<MutableDictionary> result = MutableDictionary::create();
    HashMap<String, uint64_t>::const_iterator end = map.end();
    for (HashMap<String, uint64_t>::const_iterator it = map.begin(); it != end; ++it)
        result->set(it->first, RefPtr<WebUInt64>(WebUInt64::create(it->second)).get());
    
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

} // namespace WebKit
