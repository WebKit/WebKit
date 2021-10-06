/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#include "WebProcessProxy.h"

#include "APIFrameHandle.h"
#include "APIPageGroupHandle.h"
#include "APIPageHandle.h"
#include "AuthenticatorManager.h"
#include "DataReference.h"
#include "DownloadProxyMap.h"
#include "LoadParameters.h"
#include "Logging.h"
#include "PluginInfoStore.h"
#include "PluginProcessManager.h"
#include "ProvisionalPageProxy.h"
#include "SpeechRecognitionPermissionRequest.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManager.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManagerMessages.h"
#include "SpeechRecognitionServerMessages.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "UserData.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardListItem.h"
#include "WebInspectorUtilities.h"
#include "WebNavigationDataStore.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPasteboardProxy.h"
#include "WebPreferencesKeys.h"
#include "WebProcessCache.h"
#include "WebProcessDataStoreParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxyMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/PrewarmInformation.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/SuddenTermination.h>
#include <pal/system/Sound.h>
#include <stdio.h>
#include <wtf/Algorithms.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#include "PDFPlugin.h"
#include "UserMediaCaptureManagerProxy.h"
#include <WebCore/VersionChecks.h>
#endif

#if PLATFORM(MAC)
#include "HighPerformanceGPUManager.h"
#endif

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#if ENABLE(ROUTING_ARBITRATION)
#include "AudioSessionRoutingArbitratorProxy.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromWebProcess(url), connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection(), completion)

#define WEBPROCESSPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [PID=%i] WebProcessProxy::" fmt, this, processIdentifier(), ##__VA_ARGS__)
#define WEBPROCESSPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [PID=%i] WebProcessProxy::" fmt, this, processIdentifier(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

static unsigned s_maxProcessCount { 400 };

static ListHashSet<WebProcessProxy*>& liveProcessesLRU()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<ListHashSet<WebProcessProxy*>> processes;
    return processes;
}

void WebProcessProxy::setProcessCountLimit(unsigned limit)
{
    s_maxProcessCount = limit;
}

bool WebProcessProxy::hasReachedProcessCountLimit()
{
    return liveProcessesLRU().size() >= s_maxProcessCount;
}

static bool isMainThreadOrCheckDisabled()
{
#if PLATFORM(IOS_FAMILY)
    return LIKELY(RunLoop::isMain()) || !linkedOnOrAfter(WebCore::SDKVersion::FirstWithMainThreadReleaseAssertionInWebPageProxy);
#elif PLATFORM(MAC)
    return LIKELY(RunLoop::isMain()) || !linkedOnOrAfter(WebCore::SDKVersion::FirstWithMainThreadReleaseAssertionInWebPageProxy);
#else
    return RunLoop::isMain();
#endif
}

HashMap<ProcessIdentifier, WebProcessProxy*>& WebProcessProxy::allProcesses()
{
    ASSERT(isMainThreadOrCheckDisabled());
    static NeverDestroyed<HashMap<ProcessIdentifier, WebProcessProxy*>> map;
    return map;
}

WebProcessProxy* WebProcessProxy::processForIdentifier(ProcessIdentifier identifier)
{
    return allProcesses().get(identifier);
}

static WebProcessProxy::WebPageProxyMap& globalPageMap()
{
    ASSERT(isMainThreadOrCheckDisabled());
    static NeverDestroyed<WebProcessProxy::WebPageProxyMap> pageMap;
    return pageMap;
}

void WebProcessProxy::forWebPagesWithOrigin(PAL::SessionID sessionID, const SecurityOriginData& origin, const Function<void(WebPageProxy&)>& callback)
{
    for (auto* page : globalPageMap().values()) {
        if (page->sessionID() != sessionID || SecurityOriginData::fromURL(URL { { }, page->currentURL() }) != origin)
            continue;
        callback(*page);
    }
}

Ref<WebProcessProxy> WebProcessProxy::create(WebProcessPool& processPool, WebsiteDataStore* websiteDataStore, IsPrewarmed isPrewarmed, CrossOriginMode crossOriginMode, ShouldLaunchProcess shouldLaunchProcess)
{
    auto proxy = adoptRef(*new WebProcessProxy(processPool, websiteDataStore, isPrewarmed, crossOriginMode));
    if (shouldLaunchProcess == ShouldLaunchProcess::Yes) {
        if (liveProcessesLRU().size() >= s_maxProcessCount) {
            for (auto& processPool : WebProcessPool::allProcessPools())
                processPool->webProcessCache().clear();
            if (liveProcessesLRU().size() >= s_maxProcessCount)
                liveProcessesLRU().first()->requestTermination(ProcessTerminationReason::ExceededProcessCountLimit);
        }
        ASSERT(liveProcessesLRU().size() < s_maxProcessCount);
        liveProcessesLRU().add(proxy.ptr());
        proxy->connect();
    }
    return proxy;
}

#if ENABLE(SERVICE_WORKER)
Ref<WebProcessProxy> WebProcessProxy::createForServiceWorkers(WebProcessPool& processPool, RegistrableDomain&& registrableDomain, WebsiteDataStore& websiteDataStore)
{
    auto proxy = adoptRef(*new WebProcessProxy(processPool, &websiteDataStore, IsPrewarmed::No, CrossOriginMode::Shared));
    proxy->m_registrableDomain = WTFMove(registrableDomain);
    proxy->enableServiceWorkers(processPool.userContentControllerIdentifierForServiceWorkers());
    proxy->connect();
    return proxy;
}
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
class UIProxyForCapture final : public UserMediaCaptureManagerProxy::ConnectionProxy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UIProxyForCapture(WebProcessProxy& process) : m_process(process) { }
private:
    void addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& receiver) final { m_process.addMessageReceiver(messageReceiverName, receiver); }
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName) final { m_process.removeMessageReceiver(messageReceiverName); }
    IPC::Connection& connection() final { return *m_process.connection(); }
    Logger& logger() final
    {
        if (!m_logger) {
            m_logger = Logger::create(this);
            m_logger->setEnabled(this, m_process.sessionID().isAlwaysOnLoggingAllowed());
        }
        return *m_logger;
    }
    bool willStartCapture(CaptureDevice::DeviceType) const final
    {
        // FIXME: We should validate this is granted.
        return true;
    }

    RefPtr<Logger> m_logger;
    WebProcessProxy& m_process;
};
#endif

WebProcessProxy::WebProcessProxy(WebProcessPool& processPool, WebsiteDataStore* websiteDataStore, IsPrewarmed isPrewarmed, CrossOriginMode crossOriginMode)
    : AuxiliaryProcessProxy(processPool.alwaysRunsAtBackgroundPriority())
    , m_backgroundResponsivenessTimer(*this)
    , m_processPool(processPool, isPrewarmed == IsPrewarmed::Yes ? IsWeak::Yes : IsWeak::No)
    , m_mayHaveUniversalFileReadSandboxExtension(false)
    , m_numberOfTimesSuddenTerminationWasDisabled(0)
    , m_throttler(*this, processPool.shouldTakeUIBackgroundAssertion())
#if ENABLE(ROUTING_ARBITRATION)
    , m_routingArbitrator(makeUniqueRef<AudioSessionRoutingArbitratorProxy>(*this))
#endif
    , m_isResponsive(NoOrMaybe::Maybe)
    , m_visiblePageCounter([this](RefCounterEvent) { updateBackgroundResponsivenessTimer(); })
    , m_websiteDataStore(websiteDataStore)
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    , m_userMediaCaptureManagerProxy(makeUnique<UserMediaCaptureManagerProxy>(makeUniqueRef<UIProxyForCapture>(*this)))
#endif
    , m_isPrewarmed(isPrewarmed == IsPrewarmed::Yes)
    , m_crossOriginMode(crossOriginMode)
    , m_shutdownPreventingScopeCounter([this](RefCounterEvent event) { if (event == RefCounterEvent::Decrement) maybeShutDown(); })
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());
    WEBPROCESSPROXY_RELEASE_LOG(Process, "constructor:");

    auto result = allProcesses().add(coreProcessIdentifier(), this);
    ASSERT_UNUSED(result, result.isNewEntry);

    WebPasteboardProxy::singleton().addWebProcessProxy(*this);

    platformInitialize();
}

#if !PLATFORM(IOS_FAMILY)
void WebProcessProxy::platformInitialize()
{
}
#endif

WebProcessProxy::~WebProcessProxy()
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());
    ASSERT(m_pageURLRetainCountMap.isEmpty());
    WEBPROCESSPROXY_RELEASE_LOG(Process, "destructor:");

    liveProcessesLRU().remove(this);

    for (auto identifier : m_speechRecognitionServerMap.keys())
        removeMessageReceiver(Messages::SpeechRecognitionServer::messageReceiverName(), identifier);

#if ENABLE(MEDIA_STREAM)
    if (m_speechRecognitionRemoteRealtimeMediaSourceManager)
        removeMessageReceiver(Messages::SpeechRecognitionRemoteRealtimeMediaSourceManager::messageReceiverName());
#endif

    auto result = allProcesses().remove(coreProcessIdentifier());
    ASSERT_UNUSED(result, result);

    WebPasteboardProxy::singleton().removeWebProcessProxy(*this);

#if HAVE(CVDISPLAYLINK)
    if (state() == State::Running)
        processPool().stopDisplayLinks(*connection());
#endif

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);
    for (auto& callback : isResponsiveCallbacks)
        callback(false);

    if (m_webConnection)
        m_webConnection->invalidate();

    while (m_numberOfTimesSuddenTerminationWasDisabled-- > 0)
        WebCore::enableSuddenTermination();

#if PLATFORM(MAC)
    HighPerformanceGPUManager::singleton().removeProcessRequiringHighPerformance(*this);
#endif

    platformDestroy();
}

#if !PLATFORM(IOS_FAMILY)
void WebProcessProxy::platformDestroy()
{
}
#endif

void WebProcessProxy::setIsInProcessCache(bool value)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "setIsInProcessCache(%d)", value);
    if (value) {
        RELEASE_ASSERT(m_pageMap.isEmpty());
        RELEASE_ASSERT(!m_suspendedPageCount);
        RELEASE_ASSERT(m_provisionalPages.isEmpty());
    }

    ASSERT(m_isInProcessCache != value);
    m_isInProcessCache = value;

    send(Messages::WebProcess::SetIsInProcessCache(m_isInProcessCache), 0);

    if (m_isInProcessCache) {
        // WebProcessProxy objects normally keep the process pool alive but we do not want this to be the case
        // for cached processes or it would leak the pool.
        m_processPool.setIsWeak(IsWeak::Yes);
    } else {
        RELEASE_ASSERT(m_processPool);
        m_processPool.setIsWeak(IsWeak::No);
    }
}

void WebProcessProxy::setWebsiteDataStore(WebsiteDataStore& dataStore)
{
    ASSERT(!m_websiteDataStore);
    WEBPROCESSPROXY_RELEASE_LOG(Process, "setWebsiteDataStore() dataStore=%p, sessionID=%" PRIu64, &dataStore, dataStore.sessionID().toUInt64());
    m_websiteDataStore = &dataStore;
#if PLATFORM(COCOA)
    dataStore.sendNetworkProcessXPCEndpointToWebProcess(*this);
#endif
    updateRegistrationWithDataStore();
    send(Messages::WebProcess::SetWebsiteDataStoreParameters(processPool().webProcessDataStoreParameters(*this, dataStore)), 0);
}

bool WebProcessProxy::isDummyProcessProxy() const
{
    return m_websiteDataStore && processPool().dummyProcessProxy(m_websiteDataStore->sessionID()) == this;
}

void WebProcessProxy::updateRegistrationWithDataStore()
{
    if (!m_websiteDataStore)
        return;
    
    bool shouldBeRegistered = pageCount() || provisionalPageCount();
    if (shouldBeRegistered)
        m_websiteDataStore->registerProcess(*this);
    else
        m_websiteDataStore->unregisterProcess(*this);
}

void WebProcessProxy::addProvisionalPageProxy(ProvisionalPageProxy& provisionalPage)
{
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "addProvisionalPageProxy: provisionalPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &provisionalPage, provisionalPage.page().identifier().toUInt64(), provisionalPage.webPageID().toUInt64());

    ASSERT(!m_isInProcessCache);
    ASSERT(!m_provisionalPages.contains(&provisionalPage));
    markProcessAsRecentlyUsed();
    m_provisionalPages.add(&provisionalPage);
    updateRegistrationWithDataStore();
}

void WebProcessProxy::removeProvisionalPageProxy(ProvisionalPageProxy& provisionalPage)
{
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "removeProvisionalPageProxy: provisionalPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &provisionalPage, provisionalPage.page().identifier().toUInt64(), provisionalPage.webPageID().toUInt64());

    ASSERT(m_provisionalPages.contains(&provisionalPage));
    m_provisionalPages.remove(&provisionalPage);
    updateRegistrationWithDataStore();
    if (m_provisionalPages.isEmpty())
        maybeShutDown();
}

void WebProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Web;

    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);

    if (!m_processPool->customWebContentServiceBundleIdentifier().isEmpty())
        launchOptions.customWebContentServiceBundleIdentifier = m_processPool->customWebContentServiceBundleIdentifier().ascii();
    if (WebKit::isInspectorProcessPool(processPool()))
        launchOptions.extraInitializationData.add("inspector-process"_s, "1"_s);

    LOG(Language, "WebProcessProxy is getting launch options.");
    auto overrideLanguages = m_processPool->configuration().overrideLanguages();
    if (overrideLanguages.isEmpty()) {
        LOG(Language, "overrideLanguages() reports empty. Calling platformOverrideLanguages()");
        overrideLanguages = platformOverrideLanguages();
    }
    if (!overrideLanguages.isEmpty()) {
        StringBuilder languageString;
        for (size_t i = 0; i < overrideLanguages.size(); ++i) {
            if (i)
                languageString.append(',');
            languageString.append(overrideLanguages[i]);
        }
        LOG_WITH_STREAM(Language, stream << "Setting WebProcess's launch OverrideLanguages to " << languageString);
        launchOptions.extraInitializationData.add("OverrideLanguages"_s, languageString.toString());
    } else
        LOG(Language, "overrideLanguages is still empty. Not setting WebProcess's launch OverrideLanguages.");

    launchOptions.nonValidInjectedCodeAllowed = shouldAllowNonValidInjectedCode();

    if (isPrewarmed())
        launchOptions.extraInitializationData.add("is-prewarmed"_s, "1"_s);

#if PLATFORM(PLAYSTATION)
    launchOptions.processPath = m_processPool->webProcessPath();
    launchOptions.userId = m_processPool->userId();
#endif

    if (processPool().shouldMakeNextWebProcessLaunchFailForTesting()) {
        processPool().setShouldMakeNextWebProcessLaunchFailForTesting(false);
        launchOptions.shouldMakeProcessLaunchFailForTesting = true;
    }

    if (m_serviceWorkerInformation) {
        launchOptions.extraInitializationData.add("service-worker-process"_s, "1"_s);
        launchOptions.extraInitializationData.add("registrable-domain"_s, m_registrableDomain->string());
    }
}

#if !PLATFORM(GTK) && !PLATFORM(WPE)
void WebProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
}
#endif

bool WebProcessProxy::shouldSendPendingMessage(const PendingMessage& message)
{
    if (message.encoder->messageName() == IPC::MessageName::WebPage_LoadRequestWaitingForProcessLaunch) {
        auto buffer = message.encoder->buffer();
        auto bufferSize = message.encoder->bufferSize();
        auto decoder = IPC::Decoder::create(buffer, bufferSize, nullptr, { });
        ASSERT(decoder);
        if (!decoder)
            return false;

        LoadParameters loadParameters;
        URL resourceDirectoryURL;
        WebPageProxyIdentifier pageID;
        bool checkAssumedReadAccessToResourceURL;
        if (decoder->decode(loadParameters) && decoder->decode(resourceDirectoryURL) && decoder->decode(pageID) && decoder->decode(checkAssumedReadAccessToResourceURL)) {
            if (auto* page = WebProcessProxy::webPage(pageID)) {
                page->maybeInitializeSandboxExtensionHandle(static_cast<WebProcessProxy&>(*this), loadParameters.request.url(), resourceDirectoryURL, loadParameters.sandboxExtensionHandle, checkAssumedReadAccessToResourceURL);
                send(Messages::WebPage::LoadRequest(loadParameters), decoder->destinationID());
            }
        } else
            ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

void WebProcessProxy::connectionWillOpen(IPC::Connection& connection)
{
    ASSERT(this->connection() == &connection);

    // Throttling IPC messages coming from the WebProcesses so that the UIProcess stays responsive, even
    // if one of the WebProcesses misbehaves.
    connection.enableIncomingMessagesThrottling();

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the UIProcess, which would be unsafe.
    connection.setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShimProxy::singleton().initializeConnection(connection);
#endif
}

void WebProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "processWillShutDown:");
    ASSERT_UNUSED(connection, this->connection() == &connection);

#if HAVE(CVDISPLAYLINK)
    processPool().stopDisplayLinks(connection);
#endif
}

void WebProcessProxy::shutDown()
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());
    WEBPROCESSPROXY_RELEASE_LOG(Process, "shutDown:");

    if (m_isInProcessCache) {
        processPool().webProcessCache().removeProcess(*this, WebProcessCache::ShouldShutDownProcess::No);
        ASSERT(!m_isInProcessCache);
    }

    shutDownProcess();

    if (m_webConnection) {
        m_webConnection->invalidate();
        m_webConnection = nullptr;
    }

    m_backgroundResponsivenessTimer.invalidate();
    m_activityForHoldingLockedFiles = nullptr;
    m_audibleMediaActivity = std::nullopt;

    for (auto& frame : copyToVector(m_frameMap.values()))
        frame->webProcessWillShutDown();
    m_frameMap.clear();

    for (auto* webUserContentControllerProxy : m_webUserContentControllerProxies)
        webUserContentControllerProxy->removeProcess(*this);
    m_webUserContentControllerProxies.clear();

    m_userInitiatedActionMap.clear();
    m_sleepDisablers.clear();

#if ENABLE(ROUTING_ARBITRATION)
    m_routingArbitrator->processDidTerminate();
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    m_hasIssuedAttachmentElementRelatedSandboxExtensions = false;
#endif

    m_processPool->disconnectProcess(*this);
}

WebPageProxy* WebProcessProxy::webPage(WebPageProxyIdentifier pageID)
{
    return globalPageMap().get(pageID);
}

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
void WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed()
{
    for (auto& page : globalPageMap())
        page.value->postMessageToInjectedBundle("WebsiteDataScanForRegistrableDomainsFinished", nullptr);
}

void WebProcessProxy::notifyWebsiteDataScanForRegistrableDomainsFinished()
{
    for (auto& page : globalPageMap())
        page.value->postMessageToInjectedBundle("WebsiteDataScanForRegistrableDomainsFinished", nullptr);
}

void WebProcessProxy::notifyWebsiteDataDeletionForRegistrableDomainsFinished()
{
    for (auto& page : globalPageMap())
        page.value->postMessageToInjectedBundle("WebsiteDataDeletionForRegistrableDomainsFinished", nullptr);
}

void WebProcessProxy::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebProcess::SetThirdPartyCookieBlockingMode(thirdPartyCookieBlockingMode), WTFMove(completionHandler));
}
#endif

Ref<WebPageProxy> WebProcessProxy::createWebPage(PageClient& pageClient, Ref<API::PageConfiguration>&& pageConfiguration)
{
    Ref<WebPageProxy> webPage = WebPageProxy::create(pageClient, *this, WTFMove(pageConfiguration));

    addExistingWebPage(webPage.get(), BeginsUsingDataStore::Yes);

    return webPage;
}

void WebProcessProxy::addExistingWebPage(WebPageProxy& webPage, BeginsUsingDataStore beginsUsingDataStore)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "addExistingWebPage: webPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &webPage, webPage.identifier().toUInt64(), webPage.webPageID().toUInt64());

    ASSERT(!m_pageMap.contains(webPage.identifier()));
    ASSERT(!globalPageMap().contains(webPage.identifier()));
    RELEASE_ASSERT(!m_isInProcessCache);
    ASSERT(!m_websiteDataStore || m_websiteDataStore == &webPage.websiteDataStore());

    if (beginsUsingDataStore == BeginsUsingDataStore::Yes) {
        RELEASE_ASSERT(m_processPool);
        m_processPool->pageBeginUsingWebsiteDataStore(webPage.identifier(), webPage.websiteDataStore());
    }

    markProcessAsRecentlyUsed();
    m_pageMap.set(webPage.identifier(), &webPage);
    globalPageMap().set(webPage.identifier(), &webPage);

    updateRegistrationWithDataStore();
    updateBackgroundResponsivenessTimer();
}

void WebProcessProxy::markIsNoLongerInPrewarmedPool()
{
    ASSERT(m_isPrewarmed);
    WEBPROCESSPROXY_RELEASE_LOG(Process, "markIsNoLongerInPrewarmedPool:");

    m_isPrewarmed = false;
    RELEASE_ASSERT(m_processPool);
    m_processPool.setIsWeak(IsWeak::No);

    send(Messages::WebProcess::MarkIsNoLongerPrewarmed(), 0);
}

void WebProcessProxy::removeWebPage(WebPageProxy& webPage, EndsUsingDataStore endsUsingDataStore)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "removeWebPage: webPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &webPage, webPage.identifier().toUInt64(), webPage.webPageID().toUInt64());
    auto* removedPage = m_pageMap.take(webPage.identifier());
    ASSERT_UNUSED(removedPage, removedPage == &webPage);
    removedPage = globalPageMap().take(webPage.identifier());
    ASSERT_UNUSED(removedPage, removedPage == &webPage);

    if (endsUsingDataStore == EndsUsingDataStore::Yes)
        m_processPool->pageEndUsingWebsiteDataStore(webPage.identifier(), webPage.websiteDataStore());

    removeVisitedLinkStoreUser(webPage.visitedLinkStore(), webPage.identifier());
    updateRegistrationWithDataStore();
    updateAudibleMediaAssertions();
    updateBackgroundResponsivenessTimer();

    maybeShutDown();
}

void WebProcessProxy::addVisitedLinkStoreUser(VisitedLinkStore& visitedLinkStore, WebPageProxyIdentifier pageID)
{
    auto& users = m_visitedLinkStoresWithUsers.ensure(&visitedLinkStore, [] {
        return HashSet<WebPageProxyIdentifier> { };
    }).iterator->value;

    ASSERT(!users.contains(pageID));
    users.add(pageID);

    if (users.size() == 1)
        visitedLinkStore.addProcess(*this);
}

void WebProcessProxy::removeVisitedLinkStoreUser(VisitedLinkStore& visitedLinkStore, WebPageProxyIdentifier pageID)
{
    auto it = m_visitedLinkStoresWithUsers.find(&visitedLinkStore);
    if (it == m_visitedLinkStoresWithUsers.end())
        return;

    auto& users = it->value;
    users.remove(pageID);
    if (users.isEmpty()) {
        m_visitedLinkStoresWithUsers.remove(it);
        visitedLinkStore.removeProcess(*this);
    }
}

void WebProcessProxy::addWebUserContentControllerProxy(WebUserContentControllerProxy& proxy)
{
    m_webUserContentControllerProxies.add(&proxy);
    proxy.addProcess(*this);
}

void WebProcessProxy::didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy& proxy)
{
    ASSERT(m_webUserContentControllerProxies.contains(&proxy));
    m_webUserContentControllerProxies.remove(&proxy);
}

void WebProcessProxy::assumeReadAccessToBaseURL(WebPageProxy& page, const String& urlString)
{
    URL url(URL(), urlString);
    if (!url.isLocalFile())
        return;

    // There's a chance that urlString does not point to a directory.
    // Get url's base URL to add to m_localPathsWithAssumedReadAccess.
    auto path = url.truncatedForUseAsBase().fileSystemPath();
    if (path.isNull())
        return;

    // Client loads an alternate string. This doesn't grant universal file read, but the web process is assumed
    // to have read access to this directory already.
    m_localPathsWithAssumedReadAccess.add(path);
    page.addPreviouslyVisitedPath(path);
}

bool WebProcessProxy::hasAssumedReadAccessToURL(const URL& url) const
{
    if (!url.isLocalFile())
        return false;

    String path = url.fileSystemPath();
    auto startsWithURLPath = [&path](const String& assumedAccessPath) {
        // There are no ".." components, because URL removes those.
        return path.startsWith(assumedAccessPath);
    };

    auto& platformPaths = platformPathsWithAssumedReadAccess();
    auto platformPathsEnd = platformPaths.end();
    if (std::find_if(platformPaths.begin(), platformPathsEnd, startsWithURLPath) != platformPathsEnd)
        return true;

    auto localPathsEnd = m_localPathsWithAssumedReadAccess.end();
    if (std::find_if(m_localPathsWithAssumedReadAccess.begin(), localPathsEnd, startsWithURLPath) != localPathsEnd)
        return true;

    return false;
}

bool WebProcessProxy::checkURLReceivedFromWebProcess(const String& urlString, CheckBackForwardList checkBackForwardList)
{
    return checkURLReceivedFromWebProcess(URL(URL(), urlString), checkBackForwardList);
}

bool WebProcessProxy::checkURLReceivedFromWebProcess(const URL& url, CheckBackForwardList checkBackForwardList)
{
    // FIXME: Consider checking that the URL is valid. Currently, WebProcess sends invalid URLs in many cases, but it probably doesn't have good reasons to do that.

    // Any other non-file URL is OK.
    if (!url.isLocalFile())
        return true;

    // Any file URL is also OK if we've loaded a file URL through API before, granting universal read access.
    if (m_mayHaveUniversalFileReadSandboxExtension)
        return true;

    // If we loaded a string with a file base URL before, loading resources from that subdirectory is fine.
    if (hasAssumedReadAccessToURL(url))
        return true;

    // Items in back/forward list have been already checked.
    // One case where we don't have sandbox extensions for file URLs in b/f list is if the list has been reinstated after a crash or a browser restart.
    if (checkBackForwardList == CheckBackForwardList::Yes) {
        String path = url.fileSystemPath();
        for (auto& item : WebBackForwardListItem::allItems().values()) {
            URL itemURL(URL(), item->url());
            if (itemURL.isLocalFile() && itemURL.fileSystemPath() == path)
                return true;
            URL itemOriginalURL(URL(), item->originalURL());
            if (itemOriginalURL.isLocalFile() && itemOriginalURL.fileSystemPath() == path)
                return true;
        }
    }

    // A Web process that was never asked to load a file URL should not ever ask us to do anything with a file URL.
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Loading, "checkURLReceivedFromWebProcess: Received an unexpected URL from the web process");
    return false;
}

#if !PLATFORM(COCOA)
bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return false;
}

Vector<String> WebProcessProxy::platformOverrideLanguages() const
{
    return { };
}
#endif

bool WebProcessProxy::hasProvisionalPageWithID(WebPageProxyIdentifier pageID) const
{
    for (auto* provisionalPage : m_provisionalPages) {
        if (provisionalPage->page().identifier() == pageID)
            return true;
    }
    return false;
}

bool WebProcessProxy::isAllowedToUpdateBackForwardItem(WebBackForwardListItem& item) const
{
    if (m_pageMap.contains(item.pageID()))
        return true;

    if (hasProvisionalPageWithID(item.pageID()))
        return true;

    if (item.suspendedPage() && item.suspendedPage()->page().identifier() == item.pageID() && &item.suspendedPage()->process() == this)
        return true;

    return false;
}

void WebProcessProxy::updateBackForwardItem(const BackForwardListItemState& itemState)
{
    auto* item = WebBackForwardListItem::itemForID(itemState.identifier);
    if (!item || !isAllowedToUpdateBackForwardItem(*item))
        return;

    item->setPageState(PageState { itemState.pageState });

    if (!!item->backForwardCacheEntry() != itemState.hasCachedPage) {
        if (itemState.hasCachedPage)
            processPool().backForwardCache().addEntry(*item, coreProcessIdentifier());
        else if (!item->suspendedPage())
            processPool().backForwardCache().removeEntry(*item);
    }
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPlugins(bool refresh, CompletionHandler<void(Vector<PluginInfo>&& plugins, Vector<PluginInfo>&& applicationPlugins, std::optional<Vector<WebCore::SupportedPluginIdentifier>>&& supportedPluginIdentifiers)>&& completionHandler)
{
    if (refresh)
        m_processPool->pluginInfoStore().refresh();

    auto supportedPluginIdentifiers = m_processPool->pluginInfoStore().supportedPluginIdentifiers();

    Vector<PluginInfo> plugins;
    Vector<PluginModuleInfo> pluginModules = m_processPool->pluginInfoStore().plugins();
    for (size_t i = 0; i < pluginModules.size(); ++i)
        plugins.append(pluginModules[i].info);

    Vector<PluginInfo> applicationPlugins;
#if ENABLE(PDFKIT_PLUGIN)
    // Add built-in PDF last, so that it's not used when a real plug-in is installed.
    if (!m_processPool->omitPDFSupport()) {
        plugins.append(PDFPlugin::pluginInfo());
        applicationPlugins.append(PDFPlugin::pluginInfo());
    }
#endif
    completionHandler(WTFMove(plugins), WTFMove(applicationPlugins), WTFMove(supportedPluginIdentifiers));
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPluginProcessConnection(uint64_t pluginProcessToken, Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply&& reply)
{
    MESSAGE_CHECK(HashSet<uint64_t>::isValidValue(pluginProcessToken));
    bool success = PluginProcessManager::singleton().getPluginProcessConnection(pluginProcessToken, WTFMove(reply));
    MESSAGE_CHECK(success);
}
#endif

void WebProcessProxy::getNetworkProcessConnection(Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&& reply)
{
    websiteDataStore().getNetworkProcessConnection(*this, WTFMove(reply));
}

#if ENABLE(GPU_PROCESS)
void WebProcessProxy::getGPUProcessConnection(GPUProcessConnectionParameters&& parameters, Messages::WebProcessProxy::GetGPUProcessConnection::DelayedReply&& reply)
{
    m_processPool->getGPUProcessConnection(*this, WTFMove(parameters), WTFMove(reply));
}

void WebProcessProxy::gpuProcessExited(GPUProcessTerminationReason reason)
{
    if (reason == GPUProcessTerminationReason::IdleExit)
        WEBPROCESSPROXY_RELEASE_LOG(Process, "gpuProcessExited: reason=idle-exit");
    else
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "gpuProcessExited: reason=%u", static_cast<unsigned>(reason));

    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->gpuProcessExited(reason);
}
#endif

#if ENABLE(WEB_AUTHN)
void WebProcessProxy::getWebAuthnProcessConnection(Messages::WebProcessProxy::GetWebAuthnProcessConnection::DelayedReply&& reply)
{
    MESSAGE_CHECK_COMPLETION(messageSourceIsValidWebContentProcess(), reply({ }));
    m_processPool->getWebAuthnProcessConnection(*this, WTFMove(reply));
}
#endif

#if !PLATFORM(MAC)
bool WebProcessProxy::shouldAllowNonValidInjectedCode() const
{
    return false;
}
#endif

void WebProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_processPool->dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcessProxy::messageReceiverName()) {
        didReceiveWebProcessProxyMessage(connection, decoder);
        return;
    }

    // FIXME: Add unhandled message logging.
}

bool WebProcessProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;

    if (m_processPool->dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;

    if (decoder.messageReceiverName() == Messages::WebProcessProxy::messageReceiverName())
        return didReceiveSyncWebProcessProxyMessage(connection, decoder, replyEncoder);

    // FIXME: Add unhandled message logging.
    return false;
}

void WebProcessProxy::didClose(IPC::Connection& connection)
{
#if OS(DARWIN)
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "didClose: (web process %d crash)", connection.remoteProcessID());
#else
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "didClose (web process crash)");
#endif

    processDidTerminateOrFailedToLaunch(ProcessTerminationReason::Crash);
}

void WebProcessProxy::processDidTerminateOrFailedToLaunch(ProcessTerminationReason reason)
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "processDidTerminateOrFailedToLaunch: reason=%u", static_cast<unsigned>(reason));

    // Protect ourselves, as the call to shutDown() below may otherwise cause us
    // to be deleted before we can finish our work.
    Ref protectedThis { *this };

    liveProcessesLRU().remove(this);

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    m_userMediaCaptureManagerProxy->clear();
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    m_hasIssuedAttachmentElementRelatedSandboxExtensions = false;
#endif

    if (auto* webConnection = this->webConnection())
        webConnection->didClose();

    auto pages = copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values());
    auto provisionalPages = WTF::map(m_provisionalPages, [](auto* provisionalPage) { return makeWeakPtr(provisionalPage); });

    auto isResponsiveCallbacks = std::exchange(m_isResponsiveCallbacks, { });
    for (auto& callback : isResponsiveCallbacks)
        callback(false);

    if (isStandaloneServiceWorkerProcess())
        processPool().serviceWorkerProcessCrashed(*this);

    shutDown();

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (pages.size() == 1 && reason == ProcessTerminationReason::Crash) {
        auto& page = *pages[0];
        String domain = topPrivatelyControlledDomain(URL({ }, page.currentURL()).host().toString());
        if (!domain.isEmpty())
            page.logDiagnosticMessageWithEnhancedPrivacy(WebCore::DiagnosticLoggingKeys::domainCausingCrashKey(), domain, WebCore::ShouldSample::No);
    }
#endif

#if ENABLE(ROUTING_ARBITRATION)
    m_routingArbitrator->processDidTerminate();
#endif

    // There is a nested transaction in WebPageProxy::resetStateAfterProcessExited() that we don't want to commit before the client call below (dispatchProcessDidTerminate).
    Vector<PageLoadState::Transaction> pageLoadStateTransactions;
    for (auto& page : pages) {
        pageLoadStateTransactions.append(page->pageLoadState().transaction());
        page->resetStateAfterProcessTermination(reason);
    }

    for (auto& provisionalPage : provisionalPages) {
        if (provisionalPage)
            provisionalPage->processDidTerminate();
    }

    for (auto& page : pages)
        page->dispatchProcessDidTerminate(reason);

    m_sleepDisablers.clear();
}

void WebProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    logInvalidMessage(connection, messageName);

    WebProcessPool::didReceiveInvalidMessage(messageName);

#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif

    // Terminate the WebContent process.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void WebProcessProxy::didBecomeUnresponsive()
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "didBecomeUnresponsive:");

    Ref protectedThis { *this };

    m_isResponsive = NoOrMaybe::No;

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);

    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->processDidBecomeUnresponsive();

    bool isWebProcessResponsive = false;
    for (auto& callback : isResponsiveCallbacks)
        callback(isWebProcessResponsive);

    // If the web process becomes unresponsive and only runs service workers, kill it ourselves since there are no native clients to do it.
    if (isRunningServiceWorkers() && m_pageMap.isEmpty()) {
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(PerformanceLogging, "didBecomeUnresponsive: Terminating service worker-only web process because it is unresponsive");
        disableServiceWorkers();
        terminate();
    }
}

void WebProcessProxy::didBecomeResponsive()
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "didBecomeResponsive:");
    m_isResponsive = NoOrMaybe::Maybe;

    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->processDidBecomeResponsive();
}

void WebProcessProxy::willChangeIsResponsive()
{
    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->willChangeProcessIsResponsive();
}

void WebProcessProxy::didChangeIsResponsive()
{
    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->didChangeProcessIsResponsive();
}

#if ENABLE(IPC_TESTING_API)
void WebProcessProxy::setIgnoreInvalidMessageForTesting()
{
    if (state() == State::Running)
        connection()->setIgnoreInvalidMessageForTesting();
    m_ignoreInvalidMessageForTesting = true;
}
#endif

void WebProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "didFinishLaunching:");
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());

    Ref protectedThis { *this };
    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "didFinishLaunching: Invalid connection identifier (web process failed to launch)");
        processDidTerminateOrFailedToLaunch(ProcessTerminationReason::Crash);
        return;
    }

#if PLATFORM(COCOA)
    if (m_websiteDataStore)
        m_websiteDataStore->sendNetworkProcessXPCEndpointToWebProcess(*this);
#endif

    RELEASE_ASSERT(!m_webConnection);
    m_webConnection = WebConnectionToWebProcess::create(this);

    m_processPool->processDidFinishLaunching(*this);
    m_backgroundResponsivenessTimer.updateState();

#if ENABLE(IPC_TESTING_API)
    if (m_ignoreInvalidMessageForTesting)
        connection()->setIgnoreInvalidMessageForTesting();
#endif

#if PLATFORM(IOS_FAMILY)
    if (connection()) {
        if (xpc_connection_t xpcConnection = connection()->xpcConnection())
            m_throttler.didConnectToProcess(xpc_connection_get_pid(xpcConnection));
    }
#endif

#if PLATFORM(COCOA)
    unblockAccessibilityServerIfNeeded();
#if ENABLE(REMOTE_INSPECTOR)
    enableRemoteInspectorIfNeeded();
#endif
#endif
}

WebFrameProxy* WebProcessProxy::webFrame(FrameIdentifier frameID) const
{
    if (!WebFrameProxyMap::isValidKey(frameID))
        return nullptr;

    return m_frameMap.get(frameID);
}

bool WebProcessProxy::canCreateFrame(FrameIdentifier frameID) const
{
    return WebFrameProxyMap::isValidKey(frameID) && !m_frameMap.contains(frameID);
}

void WebProcessProxy::frameCreated(FrameIdentifier frameID, WebFrameProxy& frameProxy)
{
    m_frameMap.set(frameID, &frameProxy);
}

void WebProcessProxy::didDestroyFrame(FrameIdentifier frameID)
{
    // If the page is closed before it has had the chance to send the DidCreateMainFrame message
    // back to the UIProcess, then the frameDestroyed message will still be received because it
    // gets sent directly to the WebProcessProxy.
    ASSERT(WebFrameProxyMap::isValidKey(frameID));
#if ENABLE(WEB_AUTHN)
    if (auto* frame = webFrame(frameID)) {
        if (auto* page = frame->page())
            page->websiteDataStore().authenticatorManager().cancelRequest(page->webPageID(), frameID);
    }
#endif
    if (auto* automationSession = m_processPool->automationSession())
        automationSession->didDestroyFrame(frameID);
    m_frameMap.remove(frameID);
}

void WebProcessProxy::disconnectFramesFromPage(WebPageProxy* page)
{
    for (auto& frame : copyToVector(m_frameMap.values())) {
        if (frame->page() == page)
            frame->webProcessWillShutDown();
    }
}

size_t WebProcessProxy::frameCountInPage(WebPageProxy* page) const
{
    size_t result = 0;
    for (auto& frame : m_frameMap.values()) {
        if (frame->page() == page)
            ++result;
    }
    return result;
}

auto WebProcessProxy::visiblePageToken() const -> VisibleWebPageToken
{
    return m_visiblePageCounter.count();
}

RefPtr<API::UserInitiatedAction> WebProcessProxy::userInitiatedActivity(uint64_t identifier)
{
    if (!UserInitiatedActionMap::isValidKey(identifier) || !identifier)
        return nullptr;

    auto result = m_userInitiatedActionMap.ensure(identifier, [] { return API::UserInitiatedAction::create(); });
    return result.iterator->value;
}

bool WebProcessProxy::isResponsive() const
{
    return responsivenessTimer().isResponsive() && m_backgroundResponsivenessTimer.isResponsive();
}

void WebProcessProxy::didDestroyUserGestureToken(uint64_t identifier)
{
    ASSERT(UserInitiatedActionMap::isValidKey(identifier));
    m_userInitiatedActionMap.remove(identifier);
}

bool WebProcessProxy::canBeAddedToWebProcessCache() const
{
    if (isRunningServiceWorkers()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canBeAddedToWebProcessCache: Not adding to process cache because the process is running workers");
        return false;
    }

    if (m_crossOriginMode == CrossOriginMode::Isolated) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canBeAddedToWebProcessCache: Not adding to process cache because the process is cross-origin isolated");
        return false;
    }

    if (WebKit::isInspectorProcessPool(processPool()))
        return false;

    return true;
}

void WebProcessProxy::maybeShutDown()
{
    if (isDummyProcessProxy() && m_pageMap.isEmpty()) {
        ASSERT(state() == State::Terminated);
        m_processPool->disconnectProcess(*this);
        return;
    }

    if (state() == State::Terminated || !canTerminateAuxiliaryProcess())
        return;

    if (canBeAddedToWebProcessCache() && processPool().webProcessCache().addProcessIfPossible(*this))
        return;

    shutDown();
}

bool WebProcessProxy::canTerminateAuxiliaryProcess()
{
    if (!m_pageMap.isEmpty() || m_suspendedPageCount || !m_provisionalPages.isEmpty() || m_isInProcessCache || m_shutdownPreventingScopeCounter.value()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canTerminateAuxiliaryProcess: returns false (pageCount=%u, provisionalPageCount=%u, m_suspendedPageCount=%u, m_isInProcessCache=%d, m_shutdownPreventingScopeCounter=%lu)", m_pageMap.size(), m_provisionalPages.size(), m_suspendedPageCount, m_isInProcessCache, m_shutdownPreventingScopeCounter.value());
        return false;
    }

    if (isRunningServiceWorkers()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canTerminateAuxiliaryProcess: returns false because process is running service workers");
        return false;
    }

    if (!m_processPool->shouldTerminate(*this)) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canTerminateAuxiliaryProcess: returns false because process termination is disabled");
        return false;
    }

    WEBPROCESSPROXY_RELEASE_LOG(Process, "canTerminateAuxiliaryProcess: returns true");
    return true;
}

void WebProcessProxy::shouldTerminate(CompletionHandler<void(bool)>&& completionHandler)
{
    bool shouldTerminate = canTerminateAuxiliaryProcess();
    if (shouldTerminate) {
        // We know that the web process is going to terminate so start shutting it down in the UI process.
        shutDown();
    }
    completionHandler(shouldTerminate);
}

void WebProcessProxy::updateTextCheckerState()
{
    if (canSendMessage())
        send(Messages::WebProcess::SetTextCheckerState(TextChecker::state()), 0);
}

void WebProcessProxy::windowServerConnectionStateChanged()
{
    for (const auto& page : m_pageMap.values())
        page->activityStateDidChange(ActivityState::IsVisuallyIdle);
}

#if HAVE(MOUSE_DEVICE_OBSERVATION)

void WebProcessProxy::notifyHasMouseDeviceChanged(bool hasMouseDevice)
{
    ASSERT(isMainRunLoop());
    for (auto* webProcessProxy : WebProcessProxy::allProcesses().values())
        webProcessProxy->send(Messages::WebProcess::SetHasMouseDevice(hasMouseDevice), 0);
}

#endif // HAVE(MOUSE_DEVICE_OBSERVATION)

#if HAVE(STYLUS_DEVICE_OBSERVATION)

void WebProcessProxy::notifyHasStylusDeviceChanged(bool hasStylusDevice)
{
    ASSERT(isMainRunLoop());
    for (auto* webProcessProxy : WebProcessProxy::allProcesses().values())
        webProcessProxy->send(Messages::WebProcess::SetHasStylusDevice(hasStylusDevice), 0);
}

#endif // HAVE(STYLUS_DEVICE_OBSERVATION)

void WebProcessProxy::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void(WebsiteData)>&& completionHandler)
{
    ASSERT(canSendMessage());
    ASSERT_UNUSED(sessionID, sessionID == this->sessionID());

    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "fetchWebsiteData: Taking a background assertion because the Web process is fetching Website data");

    sendWithAsyncReply(Messages::WebProcess::FetchWebsiteData(dataTypes), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (auto reply) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        completionHandler(WTFMove(reply));
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "fetchWebsiteData: Releasing a background assertion because the Web process is done fetching Website data");
    });
}

void WebProcessProxy::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(canSendMessage());
    ASSERT_UNUSED(sessionID, sessionID == this->sessionID());

    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "deleteWebsiteData: Taking a background assertion because the Web process is deleting Website data");

    sendWithAsyncReply(Messages::WebProcess::DeleteWebsiteData(dataTypes, modifiedSince), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        completionHandler();
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "deleteWebsiteData: Releasing a background assertion because the Web process is done deleting Website data");
    });
}

void WebProcessProxy::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(canSendMessage());
    ASSERT_UNUSED(sessionID, sessionID == this->sessionID());

    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "deleteWebsiteDataForOrigins: Taking a background assertion because the Web process is deleting Website data for several origins");

    sendWithAsyncReply(Messages::WebProcess::DeleteWebsiteDataForOrigins(dataTypes, origins), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        completionHandler();
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "deleteWebsiteDataForOrigins: Releasing a background assertion because the Web process is done deleting Website data for several origins");
    });
}

void WebProcessProxy::requestTermination(ProcessTerminationReason reason)
{
    if (state() == State::Terminated)
        return;

    Ref protectedThis { *this };
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "requestTermination: reason=%d", reason);

    AuxiliaryProcessProxy::terminate();

    processDidTerminateOrFailedToLaunch(reason);
}

void WebProcessProxy::enableSuddenTermination()
{
    if (state() != State::Running)
        return;

    ASSERT(m_numberOfTimesSuddenTerminationWasDisabled);
    WebCore::enableSuddenTermination();
    --m_numberOfTimesSuddenTerminationWasDisabled;
}

void WebProcessProxy::disableSuddenTermination()
{
    if (state() != State::Running)
        return;

    WebCore::disableSuddenTermination();
    ++m_numberOfTimesSuddenTerminationWasDisabled;
}

RefPtr<API::Object> WebProcessProxy::transformHandlesToObjects(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        Transformer(WebProcessProxy& webProcessProxy)
            : m_webProcessProxy(webProcessProxy)
        {
        }

        bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle:
                return static_cast<const API::FrameHandle&>(object).isAutoconverting();

            case API::Object::Type::PageHandle:
                return static_cast<const API::PageHandle&>(object).isAutoconverting();

            case API::Object::Type::PageGroupHandle:
#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
#endif
                return true;

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle:
                ASSERT(static_cast<API::FrameHandle&>(object).isAutoconverting());
                return m_webProcessProxy.webFrame(static_cast<API::FrameHandle&>(object).frameID());

            case API::Object::Type::PageGroupHandle:
                return WebPageGroup::get(static_cast<API::PageGroupHandle&>(object).webPageGroupData().pageGroupID);

            case API::Object::Type::PageHandle:
                ASSERT(static_cast<API::PageHandle&>(object).isAutoconverting());
                return m_webProcessProxy.webPage(static_cast<API::PageHandle&>(object).pageProxyID());

#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
                return m_webProcessProxy.transformHandlesToObjects(static_cast<ObjCObjectGraph&>(object));
#endif
            default:
                return &object;
            }
        }

        WebProcessProxy& m_webProcessProxy;
    };

    return UserData::transform(object, Transformer(*this));
}

RefPtr<API::Object> WebProcessProxy::transformObjectsToHandles(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::Frame:
            case API::Object::Type::Page:
            case API::Object::Type::PageGroup:
#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
#endif
                return true;

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::Frame:
                return API::FrameHandle::createAutoconverting(static_cast<const WebFrameProxy&>(object).frameID());

            case API::Object::Type::Page:
                return API::PageHandle::createAutoconverting(static_cast<const WebPageProxy&>(object).identifier(), static_cast<const WebPageProxy&>(object).webPageID());

            case API::Object::Type::PageGroup:
                return API::PageGroupHandle::create(WebPageGroupData(static_cast<const WebPageGroup&>(object).data()));

#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
                return transformObjectsToHandles(static_cast<ObjCObjectGraph&>(object));
#endif

            default:
                return &object;
            }
        }
    };

    return UserData::transform(object, Transformer());
}

void WebProcessProxy::sendPrepareToSuspend(IsSuspensionImminent isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "sendPrepareToSuspend: isSuspensionImminent=%d", isSuspensionImminent == IsSuspensionImminent::Yes);
    sendWithAsyncReply(Messages::WebProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void WebProcessProxy::sendProcessDidResume()
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "sendProcessDidResume:");
    if (canSendMessage())
        send(Messages::WebProcess::ProcessDidResume(), 0);
}

void WebProcessProxy::didSetAssertionType(ProcessAssertionType type)
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didSetAssertionType: type=%u", type);

    if (isStandaloneServiceWorkerProcess()) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didSetAssertionType: Release all assertions for network process because this is a service worker process without page");
        m_foregroundToken = nullptr;
        m_backgroundToken = nullptr;
        return;
    }

    ASSERT(!m_backgroundToken || !m_foregroundToken);

    switch (type) {
    case ProcessAssertionType::Suspended:
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didSetAssertionType(Suspended) Release all assertions for network process");
        m_foregroundToken = nullptr;
        m_backgroundToken = nullptr;
#if PLATFORM(IOS_FAMILY)
        for (auto& page : m_pageMap.values())
            page->processWillBecomeSuspended();
#endif
        break;

    case ProcessAssertionType::Background:
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didSetAssertionType(Background) Taking background assertion for network process");
        m_backgroundToken = processPool().backgroundWebProcessToken();
        m_foregroundToken = nullptr;
        break;
    
    case ProcessAssertionType::Foreground:
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didSetAssertionType(Foreground) Taking foreground assertion for network process");
        m_foregroundToken = processPool().foregroundWebProcessToken();
        m_backgroundToken = nullptr;
#if PLATFORM(IOS_FAMILY)
        for (auto& page : m_pageMap.values())
            page->processWillBecomeForeground();
#endif
        break;
    
    case ProcessAssertionType::MediaPlayback:
    case ProcessAssertionType::UnboundedNetworking:
    case ProcessAssertionType::FinishTaskInterruptable:
        ASSERT_NOT_REACHED();
    }

    ASSERT(!m_backgroundToken || !m_foregroundToken);
}

void WebProcessProxy::updateAudibleMediaAssertions()
{
    bool newHasAudibleWebPage = WTF::anyOf(m_pageMap.values(), [] (auto& page) { return page->isPlayingAudio(); });

    bool hasAudibleMediaActivity = !!m_audibleMediaActivity;
    if (hasAudibleMediaActivity == newHasAudibleWebPage)
        return;

    if (newHasAudibleWebPage) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: Taking MediaPlayback assertion for WebProcess");
        m_audibleMediaActivity = AudibleMediaActivity {
            ProcessAssertion::create(processIdentifier(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback),
            processPool().webProcessWithAudibleMediaToken()
        };
    } else {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: Releasing MediaPlayback assertion for WebProcess");
        m_audibleMediaActivity = std::nullopt;
    }
}

void WebProcessProxy::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
    if (!isHoldingLockedFiles) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "setIsHoldingLockedFiles: UIProcess is releasing a background assertion because the WebContent process is no longer holding locked files");
        m_activityForHoldingLockedFiles = nullptr;
        return;
    }
    if (!m_activityForHoldingLockedFiles) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "setIsHoldingLockedFiles: UIProcess is taking a background assertion because the WebContent process is holding locked files");
        m_activityForHoldingLockedFiles = m_throttler.backgroundActivity("Holding locked files"_s).moveToUniquePtr();
    }
}

void WebProcessProxy::isResponsive(CompletionHandler<void(bool isWebProcessResponsive)>&& callback)
{
    if (m_isResponsive == NoOrMaybe::No) {
        if (callback) {
            RunLoop::main().dispatch([callback = WTFMove(callback)]() mutable {
                bool isWebProcessResponsive = false;
                callback(isWebProcessResponsive);
            });
        }
        return;
    }

    if (callback)
        m_isResponsiveCallbacks.append(WTFMove(callback));

    checkForResponsiveness([weakThis = makeWeakPtr(*this)]() mutable {
        if (!weakThis)
            return;

        for (auto& isResponsive : std::exchange(weakThis->m_isResponsiveCallbacks, { }))
            isResponsive(true);
    });
}

void WebProcessProxy::isResponsiveWithLazyStop()
{
    if (m_isResponsive == NoOrMaybe::No)
        return;

    if (!responsivenessTimer().hasActiveTimer()) {
        // We do not send a ping if we are already waiting for the WebProcess.
        // Spamming pings on a slow web process is not helpful.
        checkForResponsiveness([weakThis = makeWeakPtr(*this)]() mutable {
            if (!weakThis)
                return;

            for (auto& isResponsive : std::exchange(weakThis->m_isResponsiveCallbacks, { }))
                isResponsive(true);
        }, UseLazyStop::Yes);
    }
}

bool WebProcessProxy::shouldConfigureJSCForTesting() const
{
    return processPool().configuration().shouldConfigureJSCForTesting();
}

bool WebProcessProxy::isJITEnabled() const
{
    return processPool().configuration().isJITEnabled();
}

void WebProcessProxy::didReceiveBackgroundResponsivenessPing()
{
    m_backgroundResponsivenessTimer.didReceiveBackgroundResponsivenessPong();
}

void WebProcessProxy::processTerminated()
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "processTerminated:");
    m_backgroundResponsivenessTimer.processTerminated();
}

void WebProcessProxy::logDiagnosticMessageForResourceLimitTermination(const String& limitKey)
{
    if (pageCount())
        (*pages().begin())->logDiagnosticMessage(DiagnosticLoggingKeys::simulatedPageCrashKey(), limitKey, ShouldSample::No);
}

void WebProcessProxy::didExceedInactiveMemoryLimitWhileActive()
{
    for (auto& page : pages())
        page->didExceedInactiveMemoryLimitWhileActive();
}

void WebProcessProxy::didExceedActiveMemoryLimit()
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(PerformanceLogging, "didExceedActiveMemoryLimit: Terminating WebProcess because it has exceeded the active memory limit");
    logDiagnosticMessageForResourceLimitTermination(DiagnosticLoggingKeys::exceededActiveMemoryLimitKey());
    requestTermination(ProcessTerminationReason::ExceededMemoryLimit);
}

void WebProcessProxy::didExceedInactiveMemoryLimit()
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(PerformanceLogging, "didExceedInactiveMemoryLimit: Terminating WebProcess because it has exceeded the inactive memory limit");
    logDiagnosticMessageForResourceLimitTermination(DiagnosticLoggingKeys::exceededInactiveMemoryLimitKey());
    requestTermination(ProcessTerminationReason::ExceededMemoryLimit);
}

void WebProcessProxy::didExceedCPULimit()
{
    Ref protectedThis { *this };

    for (auto& page : pages()) {
        if (page->isPlayingAudio()) {
            WEBPROCESSPROXY_RELEASE_LOG(PerformanceLogging, "didExceedCPULimit: WebProcess has exceeded the background CPU limit but we are not terminating it because there is audio playing");
            return;
        }

        if (page->hasActiveAudioStream() || page->hasActiveVideoStream()) {
            WEBPROCESSPROXY_RELEASE_LOG(PerformanceLogging, "didExceedCPULimit: WebProcess has exceeded the background CPU limit but we are not terminating it because it is capturing audio / video");
            return;
        }
    }

    bool hasVisiblePage = false;
    for (auto& page : pages()) {
        if (page->isViewVisible()) {
            page->didExceedBackgroundCPULimitWhileInForeground();
            hasVisiblePage = true;
        }
    }

    // We only notify the client that the process exceeded the CPU limit when it is visible, we do not terminate it.
    if (hasVisiblePage)
        return;

    WEBPROCESSPROXY_RELEASE_LOG_ERROR(PerformanceLogging, "didExceedCPULimit: Terminating background WebProcess that has exceeded the background CPU limit");
    logDiagnosticMessageForResourceLimitTermination(DiagnosticLoggingKeys::exceededBackgroundCPULimitKey());
    requestTermination(ProcessTerminationReason::ExceededCPULimit);
}

void WebProcessProxy::updateBackgroundResponsivenessTimer()
{
    m_backgroundResponsivenessTimer.updateState();
}

#if !PLATFORM(COCOA)
const MemoryCompactLookupOnlyRobinHoodHashSet<String>& WebProcessProxy::platformPathsWithAssumedReadAccess()
{
    static NeverDestroyed<MemoryCompactLookupOnlyRobinHoodHashSet<String>> platformPathsWithAssumedReadAccess;
    return platformPathsWithAssumedReadAccess;
}
#endif

void WebProcessProxy::didCollectPrewarmInformation(const WebCore::RegistrableDomain& domain, const WebCore::PrewarmInformation& prewarmInformation)
{
    MESSAGE_CHECK(!domain.isEmpty());
    processPool().didCollectPrewarmInformation(domain, prewarmInformation);
}

void WebProcessProxy::activePagesDomainsForTesting(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebProcess::GetActivePagesOriginsForTesting(), WTFMove(completionHandler));
}

void WebProcessProxy::didStartProvisionalLoadForMainFrame(const URL& url)
{
    RELEASE_ASSERT(!isInProcessCache());
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "didStartProvisionalLoadForMainFrame:");

    // This process has been used for several registrable domains already.
    if (m_registrableDomain && m_registrableDomain->isEmpty())
        return;

    if (url.protocolIsAbout())
        return;

    if (!url.protocolIsInHTTPFamily() && !processPool().configuration().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol()) {
        // Unless the processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol flag is set, we don't process swap on navigations withing the same
        // non HTTP(s) protocol. For this reason, we ignore the registrable domain and processes are not eligible for the process cache.
        m_registrableDomain = WebCore::RegistrableDomain { };
        return;
    }

    auto registrableDomain = WebCore::RegistrableDomain { url };
    if (m_registrableDomain && *m_registrableDomain != registrableDomain) {
#if ENABLE(SERVICE_WORKER)
        disableServiceWorkers();
#endif
        // Null out registrable domain since this process has now been used for several domains.
        m_registrableDomain = WebCore::RegistrableDomain { };
        return;
    }

    // Associate the process with this registrable domain.
    m_registrableDomain = WTFMove(registrableDomain);
}

void WebProcessProxy::incrementSuspendedPageCount()
{
    ++m_suspendedPageCount;
    WEBPROCESSPROXY_RELEASE_LOG(Process, "incrementSuspendedPageCount: m_suspendedPageCount=%u", m_suspendedPageCount);
    if (m_suspendedPageCount == 1)
        send(Messages::WebProcess::SetHasSuspendedPageProxy(true), 0);
}

void WebProcessProxy::decrementSuspendedPageCount()
{
    ASSERT(m_suspendedPageCount);
    --m_suspendedPageCount;
    WEBPROCESSPROXY_RELEASE_LOG(Process, "decrementSuspendedPageCount: m_suspendedPageCount=%u", m_suspendedPageCount);
    if (!m_suspendedPageCount) {
        send(Messages::WebProcess::SetHasSuspendedPageProxy(false), 0);
        maybeShutDown();
    }
}

WebProcessPool* WebProcessProxy::processPoolIfExists() const
{
    if (m_isPrewarmed || m_isInProcessCache)
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "processPoolIfExists: trying to get WebProcessPool from an inactive WebProcessProxy");
    else
        ASSERT(m_processPool);
    return m_processPool.get();
}

WebProcessPool& WebProcessProxy::processPool() const
{
    ASSERT(m_processPool);
    return *m_processPool.get();
}

PAL::SessionID WebProcessProxy::sessionID() const
{
    ASSERT(m_websiteDataStore);
    return m_websiteDataStore->sessionID();
}

void WebProcessProxy::createSpeechRecognitionServer(SpeechRecognitionServerIdentifier identifier)
{
    WebPageProxy* targetPage = nullptr;
    for (auto* page : pages()) {
        if (page && page->webPageID() == identifier) {
            targetPage = page;
            break;
        }
    }

    if (!targetPage)
        return;

    ASSERT(!m_speechRecognitionServerMap.contains(identifier));
    MESSAGE_CHECK(!m_speechRecognitionServerMap.contains(identifier));

    auto& speechRecognitionServer = m_speechRecognitionServerMap.add(identifier, nullptr).iterator->value;
    auto permissionChecker = [weakPage = makeWeakPtr(targetPage)](auto& request, auto&& completionHandler) mutable {
        if (!weakPage) {
            completionHandler(WebCore::SpeechRecognitionError { SpeechRecognitionErrorType::NotAllowed, "Page no longer exists"_s });
            return;
        }

        weakPage->requestSpeechRecognitionPermission(request, WTFMove(completionHandler));
    };
    auto checkIfMockCaptureDevicesEnabled = [weakPage = makeWeakPtr(targetPage)]() {
        return weakPage && weakPage->preferences().mockCaptureDevicesEnabled();
    };

#if ENABLE(MEDIA_STREAM)
    auto createRealtimeMediaSource = [weakPage = makeWeakPtr(targetPage)]() {
        return weakPage ? weakPage->createRealtimeMediaSourceForSpeechRecognition() : CaptureSourceOrError { "Page is invalid" };
    };
    speechRecognitionServer = makeUnique<SpeechRecognitionServer>(*connection(), identifier, WTFMove(permissionChecker), WTFMove(checkIfMockCaptureDevicesEnabled), WTFMove(createRealtimeMediaSource));
#else
    speechRecognitionServer = makeUnique<SpeechRecognitionServer>(*connection(), identifier, WTFMove(permissionChecker), WTFMove(checkIfMockCaptureDevicesEnabled));
#endif

    addMessageReceiver(Messages::SpeechRecognitionServer::messageReceiverName(), identifier, *speechRecognitionServer);
}

void WebProcessProxy::destroySpeechRecognitionServer(SpeechRecognitionServerIdentifier identifier)
{
    if (auto server = m_speechRecognitionServerMap.take(identifier))
        removeMessageReceiver(Messages::SpeechRecognitionServer::messageReceiverName(), identifier);
}

#if ENABLE(MEDIA_STREAM)

SpeechRecognitionRemoteRealtimeMediaSourceManager& WebProcessProxy::ensureSpeechRecognitionRemoteRealtimeMediaSourceManager()
{
    if (!m_speechRecognitionRemoteRealtimeMediaSourceManager) {
        m_speechRecognitionRemoteRealtimeMediaSourceManager = makeUnique<SpeechRecognitionRemoteRealtimeMediaSourceManager>(*connection());
        addMessageReceiver(Messages::SpeechRecognitionRemoteRealtimeMediaSourceManager::messageReceiverName(), *m_speechRecognitionRemoteRealtimeMediaSourceManager);
    }

    return *m_speechRecognitionRemoteRealtimeMediaSourceManager;
}

void WebProcessProxy::muteCaptureInPagesExcept(WebCore::PageIdentifier pageID)
{
#if PLATFORM(COCOA)
    for (auto* page : globalPageMap().values()) {
        if (page->webPageID() != pageID)
            page->setMediaStreamCaptureMuted(true);
    }
#else
    UNUSED_PARAM(pageID);
#endif
}

#endif

void WebProcessProxy::pageMutedStateChanged(WebCore::PageIdentifier identifier, WebCore::MediaProducer::MutedStateFlags flags)
{
    bool mutedForCapture = flags.containsAny(MediaProducer::AudioAndVideoCaptureIsMuted);
    if (!mutedForCapture)
        return;

    if (auto speechRecognitionServer = m_speechRecognitionServerMap.get(identifier))
        speechRecognitionServer->mute();
}

void WebProcessProxy::pageIsBecomingInvisible(WebCore::PageIdentifier identifier)
{
#if ENABLE(MEDIA_STREAM)
    if (!RealtimeMediaSourceCenter::shouldInterruptAudioOnPageVisibilityChange())
        return;
#endif

    if (auto speechRecognitionServer = m_speechRecognitionServerMap.get(identifier))
        speechRecognitionServer->mute();
}

#if PLATFORM(WATCHOS)

void WebProcessProxy::startBackgroundActivityForFullscreenInput()
{
    if (m_backgroundActivityForFullscreenFormControls)
        return;

    m_backgroundActivityForFullscreenFormControls = m_throttler.backgroundActivity("Fullscreen input"_s).moveToUniquePtr();
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "startBackgroundActivityForFullscreenInput: UIProcess is taking a background assertion because it is presenting fullscreen UI for form controls.");
}

void WebProcessProxy::endBackgroundActivityForFullscreenInput()
{
    if (!m_backgroundActivityForFullscreenFormControls)
        return;

    m_backgroundActivityForFullscreenFormControls = nullptr;
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "endBackgroundActivityForFullscreenInput: UIProcess is releasing a background assertion because it has dismissed fullscreen UI for form controls.");
}

#endif

#if ENABLE(SERVICE_WORKER)
void WebProcessProxy::establishServiceWorkerContext(const WebPreferencesStore& store, CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "establishServiceWorkerContext: Started");
    markProcessAsRecentlyUsed();
    sendWithAsyncReply(Messages::WebProcess::EstablishWorkerContextConnectionToNetworkProcess { processPool().defaultPageGroup().pageGroupID(), m_serviceWorkerInformation->serviceWorkerPageProxyID, m_serviceWorkerInformation->serviceWorkerPageID, store, *m_registrableDomain, m_serviceWorkerInformation->initializationData }, [this, weakThis = makeWeakPtr(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        if (weakThis)
            WEBPROCESSPROXY_RELEASE_LOG(Loading, "establishServiceWorkerContext: Finished");
        completionHandler();
    }, 0);
}

void WebProcessProxy::setServiceWorkerUserAgent(const String& userAgent)
{
    ASSERT(m_serviceWorkerInformation);
    send(Messages::WebSWContextManagerConnection::SetUserAgent { userAgent }, 0);
}

void WebProcessProxy::updateServiceWorkerPreferencesStore(const WebPreferencesStore& store)
{
    ASSERT(m_serviceWorkerInformation);
    send(Messages::WebSWContextManagerConnection::UpdatePreferencesStore { store }, 0);
}

void WebProcessProxy::updateServiceWorkerProcessAssertion()
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateServiceWorkerProcessAssertion:");
    ASSERT(m_serviceWorkerInformation);
    if (!m_serviceWorkerInformation)
        return;

    bool shouldTakeForegroundActivity = WTF::anyOf(m_serviceWorkerInformation->clientProcesses, [&](auto& process) {
        return &process != this && !!process.m_foregroundToken;
    });
    if (shouldTakeForegroundActivity) {
        if (!ProcessThrottler::isValidForegroundActivity(m_serviceWorkerInformation->activity))
            m_serviceWorkerInformation->activity = m_throttler.foregroundActivity("Service Worker for foreground view(s)"_s);
        return;
    }

    bool shouldTakeBackgroundActivity = WTF::anyOf(m_serviceWorkerInformation->clientProcesses, [&](auto& process) {
        return &process != this && !!process.m_backgroundToken;
    });
    if (shouldTakeBackgroundActivity) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_serviceWorkerInformation->activity))
            m_serviceWorkerInformation->activity = m_throttler.backgroundActivity("Service Worker for background view(s)"_s);
        return;
    }
    m_serviceWorkerInformation->activity = nullptr;
}

void WebProcessProxy::registerServiceWorkerClientProcess(WebProcessProxy& proxy)
{
    if (!m_serviceWorkerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(ServiceWorker, "registerServiceWorkerClientProcess: clientProcess=%p, clientPID=%d", &proxy, proxy.processIdentifier());
    m_serviceWorkerInformation->clientProcesses.add(proxy);
    updateServiceWorkerProcessAssertion();
}

void WebProcessProxy::unregisterServiceWorkerClientProcess(WebProcessProxy& proxy)
{
    if (!m_serviceWorkerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(ServiceWorker, "unregisterServiceWorkerClientProcess: clientProcess=%p, clientPID=%d", &proxy, proxy.processIdentifier());
    m_serviceWorkerInformation->clientProcesses.remove(proxy);
    updateServiceWorkerProcessAssertion();
}

bool WebProcessProxy::hasServiceWorkerForegroundActivityForTesting() const
{
    return m_serviceWorkerInformation ? ProcessThrottler::isValidForegroundActivity(m_serviceWorkerInformation->activity) : false;
}

bool WebProcessProxy::hasServiceWorkerBackgroundActivityForTesting() const
{
    return m_serviceWorkerInformation ? ProcessThrottler::isValidBackgroundActivity(m_serviceWorkerInformation->activity) : false;
}
#endif // ENABLE(SERVICE_WORKER)

void WebProcessProxy::disableServiceWorkers()
{
    if (!m_serviceWorkerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(ServiceWorker, "disableServiceWorkers:");
    m_serviceWorkerInformation = { };
    updateBackgroundResponsivenessTimer();

#if ENABLE(SERVICE_WORKER)
    processPool().removeFromServiceWorkerProcesses(*this);
    send(Messages::WebSWContextManagerConnection::Close { }, 0);
#endif

    maybeShutDown();
}

#if ENABLE(CONTENT_EXTENSIONS)
static Vector<std::pair<String, WebCompiledContentRuleListData>> contentRuleListsFromIdentifier(const std::optional<UserContentControllerIdentifier>& userContentControllerIdentifier)
{
    if (!userContentControllerIdentifier) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto* userContentController = WebUserContentControllerProxy::get(*userContentControllerIdentifier);
    if (!userContentController)
        return { };

    return userContentController->contentRuleListData();
}
#endif

void WebProcessProxy::enableServiceWorkers(const UserContentControllerIdentifier& userContentControllerIdentifier)
{
    ASSERT(m_registrableDomain && !m_registrableDomain->isEmpty());
    ASSERT(!m_serviceWorkerInformation);
    WEBPROCESSPROXY_RELEASE_LOG(ServiceWorker, "enableServiceWorkers:");
    m_serviceWorkerInformation = ServiceWorkerInformation {
        WebPageProxyIdentifier::generate(),
        PageIdentifier::generate(),
        ServiceWorkerInitializationData {
            userContentControllerIdentifier,
#if ENABLE(CONTENT_EXTENSIONS)
            contentRuleListsFromIdentifier(userContentControllerIdentifier),
#endif
        },
        nullptr,
        { }
    };
    updateBackgroundResponsivenessTimer();
#if ENABLE(SERVICE_WORKER)
    updateServiceWorkerProcessAssertion();
#endif
}

void WebProcessProxy::didCreateSleepDisabler(SleepDisablerIdentifier identifier, const String& reason, bool display)
{
    MESSAGE_CHECK(!reason.isNull());
    auto sleepDisabler = makeUnique<WebCore::SleepDisabler>(reason.utf8().data(), display ? PAL::SleepDisabler::Type::Display : PAL::SleepDisabler::Type::System);
    m_sleepDisablers.add(identifier, WTFMove(sleepDisabler));
}

void WebProcessProxy::didDestroySleepDisabler(SleepDisablerIdentifier identifier)
{
    m_sleepDisablers.remove(identifier);
}

bool WebProcessProxy::hasSleepDisabler() const
{
    return !m_sleepDisablers.isEmpty();
}

void WebProcessProxy::markProcessAsRecentlyUsed()
{
    if (liveProcessesLRU().contains(this))
        liveProcessesLRU().appendOrMoveToLast(this);
}

void WebProcessProxy::systemBeep()
{
    PAL::systemBeep();
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK_COMPLETION
#undef WEBPROCESSPROXY_RELEASE_LOG
#undef WEBPROCESSPROXY_RELEASE_LOG_ERROR
