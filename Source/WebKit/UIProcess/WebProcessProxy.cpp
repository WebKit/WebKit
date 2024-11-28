/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
#include "APIPageConfiguration.h"
#include "APIPageHandle.h"
#include "APIUIClient.h"
#include "AuthenticatorManager.h"
#include "DownloadProxyMap.h"
#include "GPUProcessConnectionParameters.h"
#include "GoToBackForwardItemParameters.h"
#include "LoadParameters.h"
#include "Logging.h"
#include "ModelProcessConnectionParameters.h"
#include "ModelProcessProxy.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkProcessMessages.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "PageLoadState.h"
#include "PlatformXRSystem.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "RemoteWorkerType.h"
#include "ServiceWorkerNotificationHandler.h"
#include "SpeechRecognitionPermissionRequest.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManager.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManagerMessages.h"
#include "SpeechRecognitionServerMessages.h"
#include "SuspendedPageProxy.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "UserData.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardListFrameItem.h"
#include "WebBackForwardListItem.h"
#include "WebCompiledContentRuleList.h"
#include "WebFrameProxy.h"
#include "WebInspectorUtilities.h"
#include "WebLockRegistryProxy.h"
#include "WebNavigationDataStore.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPasteboardProxy.h"
#include "WebPermissionControllerMessages.h"
#include "WebPermissionControllerProxy.h"
#include "WebPreferencesDefaultValues.h"
#include "WebPreferencesKeys.h"
#include "WebProcessCache.h"
#include "WebProcessDataStoreParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxyMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include <WebCore/AudioSession.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/PermissionName.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/PrewarmInformation.h>
#include <WebCore/PublicSuffixStore.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/SuddenTermination.h>
#include <WebCore/WrappedCryptoKey.h>
#include <optional>
#include <pal/system/Sound.h>
#include <stdio.h>
#include <wtf/Algorithms.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WeakListHashSet.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "UserMediaCaptureManagerProxy.h"
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if ENABLE(GPU_PROCESS)
#include "APIPageConfiguration.h"
#endif

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#if ENABLE(ROUTING_ARBITRATION)
#include "AudioSessionRoutingArbitratorProxy.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/Device.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromWebProcess(url), connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection(), completion)

#define WEBPROCESSPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [PID=%i] WebProcessProxy::" fmt, static_cast<const void*>(this), processID(), ##__VA_ARGS__)
#define WEBPROCESSPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [PID=%i] WebProcessProxy::" fmt, static_cast<const void*>(this), processID(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

static unsigned s_maxProcessCount { 400 };

static WeakListHashSet<WebProcessProxy>& liveProcessesLRU()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<WeakListHashSet<WebProcessProxy>> processes;
    return processes;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebProcessProxy);

void WebProcessProxy::setProcessCountLimit(unsigned limit)
{
    s_maxProcessCount = limit;
}

bool WebProcessProxy::hasReachedProcessCountLimit()
{
    return liveProcessesLRU().computeSize() >= s_maxProcessCount;
}

static bool isMainThreadOrCheckDisabled()
{
#if PLATFORM(IOS_FAMILY)
    return LIKELY(RunLoop::isMain()) || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MainThreadReleaseAssertionInWebPageProxy);
#elif PLATFORM(MAC)
    return LIKELY(RunLoop::isMain()) || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MainThreadReleaseAssertionInWebPageProxy);
#else
    return RunLoop::isMain();
#endif
}

WebProcessProxy::WebProcessProxyMap& WebProcessProxy::allProcessMap()
{
    ASSERT(isMainThreadOrCheckDisabled());
    static NeverDestroyed<WebProcessProxy::WebProcessProxyMap> map;
    return map;
}

Vector<Ref<WebProcessProxy>> WebProcessProxy::allProcesses()
{
    return WTF::map(allProcessMap(), [] (auto& keyValue) -> Ref<WebProcessProxy> {
        return keyValue.value.get();
    });
}

RefPtr<WebProcessProxy> WebProcessProxy::processForIdentifier(ProcessIdentifier identifier)
{
    return allProcessMap().get(identifier);
}

RefPtr<WebProcessProxy> WebProcessProxy::processForConnection(const IPC::Connection& connection)
{
    return dynamicDowncast<WebProcessProxy>(AuxiliaryProcessProxy::fromConnection(connection));
}

auto WebProcessProxy::globalPageMap() -> WebPageProxyMap&
{
    ASSERT(isMainThreadOrCheckDisabled());
    static NeverDestroyed<WebPageProxyMap> pageMap;
    return pageMap;
}

Vector<Ref<WebPageProxy>> WebProcessProxy::globalPages()
{
    return WTF::map(globalPageMap(), [] (auto& keyValue) -> Ref<WebPageProxy> {
        return keyValue.value.get();
    });
}

Vector<Ref<WebPageProxy>> WebProcessProxy::pages() const
{
    auto pages = mainPages();
    for (Ref remotePage : m_remotePages) {
        if (RefPtr page = remotePage->page())
            pages.append(page.releaseNonNull());
    }
    return pages;
}

Vector<Ref<WebPageProxy>> WebProcessProxy::mainPages() const
{
    return WTF::map(m_pageMap, [] (auto& keyValue) -> Ref<WebPageProxy> {
        return keyValue.value.get();
    });
}

Vector<WeakPtr<RemotePageProxy>> WebProcessProxy::remotePages() const
{
    return WTF::copyToVector(m_remotePages);
}

void WebProcessProxy::forWebPagesWithOrigin(PAL::SessionID sessionID, const SecurityOriginData& origin, const Function<void(WebPageProxy&)>& callback)
{
    for (Ref page : globalPages()) {
        if (page->sessionID() != sessionID || SecurityOriginData::fromURLWithoutStrictOpaqueness(URL { page->currentURL() }) != origin)
            continue;
        callback(page);
    }
}

Vector<std::pair<WebCore::ProcessIdentifier, WebCore::RegistrableDomain>> WebProcessProxy::allowedFirstPartiesForCookies()
{
    Vector<std::pair<WebCore::ProcessIdentifier, WebCore::RegistrableDomain>> result;
    for (Ref page : globalPages())
        result.append(std::make_pair(page->legacyMainFrameProcess().coreProcessIdentifier(), RegistrableDomain(URL(page->currentURL()))));
    return result;
}

Ref<WebProcessProxy> WebProcessProxy::create(WebProcessPool& processPool, WebsiteDataStore* websiteDataStore, LockdownMode lockdownMode, IsPrewarmed isPrewarmed, CrossOriginMode crossOriginMode, ShouldLaunchProcess shouldLaunchProcess)
{
    Ref proxy = adoptRef(*new WebProcessProxy(processPool, websiteDataStore, isPrewarmed, crossOriginMode, lockdownMode));
    if (shouldLaunchProcess == ShouldLaunchProcess::Yes) {
        if (liveProcessesLRU().computeSize() >= s_maxProcessCount) {
            for (auto& processPool : WebProcessPool::allProcessPools())
                processPool->checkedWebProcessCache()->clear();
            if (liveProcessesLRU().computeSize() >= s_maxProcessCount)
                Ref { liveProcessesLRU().first() }->requestTermination(ProcessTerminationReason::ExceededProcessCountLimit);
        }
        ASSERT(liveProcessesLRU().computeSize() < s_maxProcessCount);
        liveProcessesLRU().add(proxy.get());
        proxy->connect();
    }
    return proxy;
}

Ref<WebProcessProxy> WebProcessProxy::createForRemoteWorkers(RemoteWorkerType workerType, WebProcessPool& processPool, Site&& site, WebsiteDataStore& websiteDataStore, LockdownMode lockdownMode)
{
    Ref proxy = adoptRef(*new WebProcessProxy(processPool, &websiteDataStore, IsPrewarmed::No, CrossOriginMode::Shared, lockdownMode));
    proxy->m_site = WTFMove(site);
    proxy->enableRemoteWorkers(workerType, processPool.userContentControllerIdentifierForRemoteWorkers());
    proxy->connect();
    return proxy;
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
class UIProxyForCapture final : public UserMediaCaptureManagerProxy::ConnectionProxy {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(UIProxyForCapture);
public:
    explicit UIProxyForCapture(WebProcessProxy& process)
        : m_process(process)
    { }

private:
    void addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& receiver) final { protectedProcess()->addMessageReceiver(messageReceiverName, receiver); }
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName) final { protectedProcess()->removeMessageReceiver(messageReceiverName); }
    IPC::Connection& connection() final { return m_process->connection(); }

    Logger& logger() final
    {
        return protectedProcess()->logger();
    }

    bool willStartCapture(CaptureDevice::DeviceType) const final
    {
        // FIXME: We should validate this is granted.
        return true;
    }

    const WebCore::ProcessIdentity& resourceOwner() const final
    {
        // FIXME: should obtain WebContent process identity from WebContent.
        static NeverDestroyed<WebCore::ProcessIdentity> dummy;
        return dummy.get();
    }

    Ref<WebProcessProxy> protectedProcess() const { return m_process.get(); }

    WeakRef<WebProcessProxy> m_process;
};
#endif

WebProcessProxy::WebProcessProxy(WebProcessPool& processPool, WebsiteDataStore* websiteDataStore, IsPrewarmed isPrewarmed, CrossOriginMode crossOriginMode, LockdownMode lockdownMode)
    : AuxiliaryProcessProxy(processPool.shouldTakeUIBackgroundAssertion() ? ShouldTakeUIBackgroundAssertion::Yes : ShouldTakeUIBackgroundAssertion::No
    , processPool.alwaysRunsAtBackgroundPriority() ? AlwaysRunsAtBackgroundPriority::Yes : AlwaysRunsAtBackgroundPriority::No)
    , m_backgroundResponsivenessTimer(*this)
    , m_processPool(processPool, isPrewarmed == IsPrewarmed::Yes ? IsWeak::Yes : IsWeak::No)
    , m_mayHaveUniversalFileReadSandboxExtension(false)
    , m_numberOfTimesSuddenTerminationWasDisabled(0)
    , m_isResponsive(NoOrMaybe::Maybe)
    , m_visiblePageCounter([this](RefCounterEvent) { updateBackgroundResponsivenessTimer(); })
    , m_websiteDataStore(websiteDataStore)
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    , m_userMediaCaptureManagerProxy(UserMediaCaptureManagerProxy::create(makeUniqueRef<UIProxyForCapture>(*this)))
#endif
    , m_isPrewarmed(isPrewarmed == IsPrewarmed::Yes)
    , m_lockdownMode(lockdownMode)
    , m_crossOriginMode(crossOriginMode)
    , m_shutdownPreventingScopeCounter([this](RefCounterEvent event) { if (event == RefCounterEvent::Decrement) maybeShutDown(); })
    , m_webLockRegistry(websiteDataStore ? makeUniqueWithoutRefCountedCheck<WebLockRegistryProxy>(*this) : nullptr)
    , m_webPermissionController(makeUniqueRefWithoutRefCountedCheck<WebPermissionControllerProxy>(*this))
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());
    WEBPROCESSPROXY_RELEASE_LOG(Process, "constructor:");

    auto result = allProcessMap().add(coreProcessIdentifier(), *this);
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

    liveProcessesLRU().remove(*this);

    for (auto identifier : m_speechRecognitionServerMap.keys())
        removeMessageReceiver(Messages::SpeechRecognitionServer::messageReceiverName(), identifier);

#if ENABLE(MEDIA_STREAM)
    if (m_speechRecognitionRemoteRealtimeMediaSourceManager)
        removeMessageReceiver(Messages::SpeechRecognitionRemoteRealtimeMediaSourceManager::messageReceiverName());
#endif

    if (auto completionHandler = std::exchange(m_sharedPreferencesForWebProcessCompletionHandler, { }))
        completionHandler(false);

    auto result = allProcessMap().remove(coreProcessIdentifier());
    ASSERT_UNUSED(result, result);

    WebPasteboardProxy::singleton().removeWebProcessProxy(*this);

#if HAVE(DISPLAY_LINK)
    if (RefPtr<WebProcessPool> processPool = m_processPool.get())
        processPool->displayLinks().stopDisplayLinks(m_displayLinkClient);
#endif

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);
    for (auto& callback : isResponsiveCallbacks)
        callback(false);

    while (m_numberOfTimesSuddenTerminationWasDisabled-- > 0)
        WebCore::enableSuddenTermination();

    platformDestroy();
}

#if !PLATFORM(IOS_FAMILY)
void WebProcessProxy::platformDestroy()
{
}
#endif

void WebProcessProxy::setIsInProcessCache(bool value, WillShutDown willShutDown)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "setIsInProcessCache(%d)", value);
    if (value) {
        RELEASE_ASSERT(m_pageMap.isEmpty());
        RELEASE_ASSERT(m_suspendedPages.isEmptyIgnoringNullReferences());
        RELEASE_ASSERT(m_provisionalPages.isEmptyIgnoringNullReferences());
        m_previouslyApprovedFilePaths.clear();
    }

    ASSERT(m_isInProcessCache != value);
    m_isInProcessCache = value;

    // No point in doing anything else if the process is about to shut down.
    ASSERT(willShutDown == WillShutDown::No || !value);
    if (willShutDown == WillShutDown::Yes)
        return;

    // The WebProcess might be task_suspended at this point, so use sendWithAsyncReply to resume
    // the process via a background activity long enough to process the IPC if necessary.
    sendWithAsyncReply(Messages::WebProcess::SetIsInProcessCache(m_isInProcessCache), []() { });

    if (m_isInProcessCache) {
        // WebProcessProxy objects normally keep the process pool alive but we do not want this to be the case
        // for cached processes or it would leak the pool.
        m_processPool.setIsWeak(IsWeak::Yes);
    } else {
        RELEASE_ASSERT(m_processPool);
        m_processPool.setIsWeak(IsWeak::No);
    }

    updateRuntimeStatistics();
}

void WebProcessProxy::setWebsiteDataStore(WebsiteDataStore& dataStore)
{
    ASSERT(!m_websiteDataStore);
    WEBPROCESSPROXY_RELEASE_LOG(Process, "setWebsiteDataStore() dataStore=%p, sessionID=%" PRIu64, &dataStore, dataStore.sessionID().toUInt64());
#if PLATFORM(COCOA)
    if (!m_websiteDataStore)
        dataStore.protectedNetworkProcess()->sendXPCEndpointToProcess(*this);
#endif
    m_websiteDataStore = &dataStore;
    logger().setEnabled(this, isAlwaysOnLoggingAllowed());
    updateRegistrationWithDataStore();
    send(Messages::WebProcess::SetWebsiteDataStoreParameters(protectedProcessPool()->webProcessDataStoreParameters(*this, dataStore)), 0);

    // Delay construction of the WebLockRegistryProxy until the WebProcessProxy has a data store since the data store holds the
    // LocalWebLockRegistry.
    m_webLockRegistry = makeUniqueWithoutRefCountedCheck<WebLockRegistryProxy>(*this);
}

bool WebProcessProxy::isDummyProcessProxy() const
{
    return m_websiteDataStore && protectedProcessPool()->dummyProcessProxy(m_websiteDataStore->sessionID()) == this;
}

void WebProcessProxy::updateRegistrationWithDataStore()
{
    if (RefPtr dataStore = websiteDataStore()) {
        if (pageCount() || provisionalPageCount())
            dataStore->registerProcess(*this);
        else
            dataStore->unregisterProcess(*this);
    }
}

void WebProcessProxy::initializeWebProcess(WebProcessCreationParameters&& parameters)
{
    sendWithAsyncReply(Messages::WebProcess::InitializeWebProcess(WTFMove(parameters)), [weakThis = WeakPtr { *this }, initializationActivityAndGrant = initializationActivityAndGrant()] (ProcessIdentity processIdentity) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_processIdentity = WTFMove(processIdentity);
    }, 0);
}

void WebProcessProxy::initializePreferencesForGPUAndNetworkProcesses(const WebPageProxy& page)
{
    if (!m_sharedPreferencesForWebProcess.version) {
        updateSharedPreferencesForWebProcess(page.preferences().store());
        ASSERT(m_sharedPreferencesForWebProcess.version);
    } else {
#if ASSERT_ENABLED
        auto sharedPreferencesForWebProcess = m_sharedPreferencesForWebProcess;
        ASSERT(!WebKit::updateSharedPreferencesForWebProcess(sharedPreferencesForWebProcess, page.preferences().store()));
#endif
    }

}

bool WebProcessProxy::hasSameGPUAndNetworkProcessPreferencesAs(const API::PageConfiguration& pageConfiguration) const
{
    if (m_sharedPreferencesForWebProcess.version) {
        auto sharedPreferencesForWebProcess = m_sharedPreferencesForWebProcess;
        if (WebKit::updateSharedPreferencesForWebProcess(sharedPreferencesForWebProcess, pageConfiguration.preferences().store()))
            return false;
    }
    return true;
}

void WebProcessProxy::addProvisionalPageProxy(ProvisionalPageProxy& provisionalPage)
{
    ASSERT(provisionalPage.page());
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "addProvisionalPageProxy: provisionalPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &provisionalPage, provisionalPage.page()->identifier().toUInt64(), provisionalPage.webPageID().toUInt64());

    ASSERT(!m_isInProcessCache);
    ASSERT(!m_provisionalPages.contains(provisionalPage));
    markProcessAsRecentlyUsed();
    m_provisionalPages.add(provisionalPage);
    initializePreferencesForGPUAndNetworkProcesses(*provisionalPage.protectedPage());
    updateRegistrationWithDataStore();
}

void WebProcessProxy::removeProvisionalPageProxy(ProvisionalPageProxy& provisionalPage)
{
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "removeProvisionalPageProxy: provisionalPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &provisionalPage, provisionalPage.page() ? provisionalPage.page()->identifier().toUInt64() : 0, provisionalPage.webPageID().toUInt64());

    ASSERT(m_provisionalPages.contains(provisionalPage));
    m_provisionalPages.remove(provisionalPage);
    updateRegistrationWithDataStore();
    if (m_provisionalPages.isEmptyIgnoringNullReferences()) {
        if (RefPtr page = provisionalPage.page())
            reportProcessDisassociatedWithPageIfNecessary(page->identifier());
        maybeShutDown();
    }
}

void WebProcessProxy::addRemotePageProxy(RemotePageProxy& remotePage)
{
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "addRemotePageProxy: remotePage=%p", &remotePage);

    ASSERT(!m_isInProcessCache);
    ASSERT(!m_remotePages.contains(remotePage));
    m_remotePages.add(remotePage);
    markProcessAsRecentlyUsed();
    initializePreferencesForGPUAndNetworkProcesses(*remotePage.protectedPage());
}

void WebProcessProxy::removeRemotePageProxy(RemotePageProxy& remotePage)
{
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "removeRemotePageProxy: remotePage=%p", &remotePage);
    m_remotePages.remove(remotePage);
    if (m_remotePages.isEmptyIgnoringNullReferences())
        maybeShutDown();
}

void WebProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Web;

    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);

    if (WebKit::isInspectorProcessPool(protectedProcessPool()))
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("inspector-process"_s, "1"_s);

    launchOptions.nonValidInjectedCodeAllowed = shouldAllowNonValidInjectedCode();

    if (isPrewarmed())
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("is-prewarmed"_s, "1"_s);

#if PLATFORM(PLAYSTATION)
    launchOptions.processPath = m_processPool->webProcessPath();
    launchOptions.userId = m_processPool->userId();
#endif

    if (processPool().shouldMakeNextWebProcessLaunchFailForTesting()) {
        protectedProcessPool()->setShouldMakeNextWebProcessLaunchFailForTesting(false);
        launchOptions.shouldMakeProcessLaunchFailForTesting = true;
    }

    if (m_serviceWorkerInformation) {
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("service-worker-process"_s, "1"_s);
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("registrable-domain"_s, m_site->domain().string());
    }

    if (shouldEnableLockdownMode())
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("enable-lockdown-mode"_s, "1"_s);
}

#if !PLATFORM(GTK) && !PLATFORM(WPE)
void WebProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    AuxiliaryProcessProxy::platformGetLaunchOptions(launchOptions);
}
#endif

bool WebProcessProxy::shouldSendPendingMessage(const PendingMessage& message)
{
    if (message.encoder->messageName() == IPC::MessageName::WebPage_LoadRequestWaitingForProcessLaunch) {
        auto decoder = IPC::Decoder::create(message.encoder->span(), { });
        ASSERT(decoder);
        if (!decoder)
            return false;

        auto loadParameters = decoder->decode<LoadParameters>();
        auto resourceDirectoryURL = decoder->decode<URL>();
        auto pageID = decoder->decode<WebPageProxyIdentifier>();
        auto checkAssumedReadAccessToResourceURL = decoder->decode<bool>();
        auto destinationID = decoder->destinationID();
        if (loadParameters && resourceDirectoryURL && pageID && checkAssumedReadAccessToResourceURL) {
            if (RefPtr page = WebProcessProxy::webPage(*pageID)) {
                auto url = loadParameters->request.url();
                page->maybeInitializeSandboxExtensionHandle(static_cast<WebProcessProxy&>(*this), url, *resourceDirectoryURL,  *checkAssumedReadAccessToResourceURL, [weakThis = WeakPtr { *this }, destinationID, loadParameters = WTFMove(loadParameters)] (std::optional<SandboxExtension::Handle> sandboxExtension) mutable {
                    if (!weakThis)
                        return;
                    if (sandboxExtension)
                        loadParameters->sandboxExtensionHandle = WTFMove(*sandboxExtension);
                    weakThis->send(Messages::WebPage::LoadRequest(WTFMove(*loadParameters)), destinationID);
                });
            }
        } else
            ASSERT_NOT_REACHED();
        return false;
    } else if (message.encoder->messageName() == IPC::MessageName::WebPage_GoToBackForwardItemWaitingForProcessLaunch) {
        auto decoder = IPC::Decoder::create(message.encoder->span(), { });
        ASSERT(decoder);
        if (!decoder)
            return false;

        auto parameters = decoder->decode<GoToBackForwardItemParameters>();
        if (!parameters)
            return false;
        auto pageID = decoder->decode<WebPageProxyIdentifier>();
        if (!pageID)
            return false;
        auto destinationID = decoder->destinationID();
        auto frameState = parameters->frameState;
        auto completionHandler = [weakThis = WeakPtr { *this }, parameters = WTFMove(parameters), destinationID] (std::optional<SandboxExtension::Handle> sandboxExtension) mutable {
            if (!weakThis)
                return;
            if (sandboxExtension)
                parameters->sandboxExtensionHandle = WTFMove(*sandboxExtension);
            weakThis->send(Messages::WebPage::GoToBackForwardItem(WTFMove(*parameters)), destinationID);
        };
        if (RefPtr page = WebProcessProxy::webPage(*pageID)) {
            if (RefPtr item = WebBackForwardListItem::itemForID(*frameState->identifier))
                page->maybeInitializeSandboxExtensionHandle(static_cast<WebProcessProxy&>(*this), URL { item->url() }, item->resourceDirectoryURL(), true, WTFMove(completionHandler));
        } else
            completionHandler(std::nullopt);
        return false;
    }
    return true;
}

void WebProcessProxy::connectionWillOpen(IPC::Connection& connection)
{
    ASSERT(&this->connection() == &connection);

    // Throttling IPC messages coming from the WebProcesses so that the UIProcess stays responsive, even
    // if one of the WebProcesses misbehaves.
    connection.enableIncomingMessagesThrottling();

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the UIProcess, which would be unsafe.
    connection.setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);

#if HAVE(DISPLAY_LINK)
    m_displayLinkClient.setConnection(&connection);
#endif
}

void WebProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "processWillShutDown:");
    ASSERT_UNUSED(connection, &this->connection() == &connection);

#if HAVE(DISPLAY_LINK)
    m_displayLinkClient.setConnection(nullptr);
    Ref<WebProcessPool> { processPool() }->displayLinks().stopDisplayLinks(m_displayLinkClient);
#endif
}

#if HAVE(DISPLAY_LINK)
std::optional<unsigned> WebProcessProxy::nominalFramesPerSecondForDisplay(WebCore::PlatformDisplayID displayID)
{
    return processPool().displayLinks().nominalFramesPerSecondForDisplay(displayID);
}

void WebProcessProxy::startDisplayLink(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    protectedProcessPool()->displayLinks().startDisplayLink(m_displayLinkClient, observerID, displayID, preferredFramesPerSecond);
}

void WebProcessProxy::stopDisplayLink(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID)
{
    protectedProcessPool()->displayLinks().stopDisplayLink(m_displayLinkClient, observerID, displayID);
}

void WebProcessProxy::setDisplayLinkPreferredFramesPerSecond(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    protectedProcessPool()->displayLinks().setDisplayLinkPreferredFramesPerSecond(m_displayLinkClient, observerID, displayID, preferredFramesPerSecond);
}

void WebProcessProxy::setDisplayLinkForDisplayWantsFullSpeedUpdates(WebCore::PlatformDisplayID displayID, bool wantsFullSpeedUpdates)
{
    protectedProcessPool()->displayLinks().setDisplayLinkForDisplayWantsFullSpeedUpdates(m_displayLinkClient, displayID, wantsFullSpeedUpdates);
}
#endif

void WebProcessProxy::shutDown()
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());
    WEBPROCESSPROXY_RELEASE_LOG(Process, "shutDown:");

    if (m_isInProcessCache) {
        protectedProcessPool()->checkedWebProcessCache()->removeProcess(*this, WebProcessCache::ShouldShutDownProcess::No);
        ASSERT(!m_isInProcessCache);
    }

    shutDownProcess();

    m_backgroundResponsivenessTimer.invalidate();
    m_audibleMediaActivity = std::nullopt;
    m_mediaStreamingActivity = std::nullopt;
    m_foregroundToken = nullptr;
    m_backgroundToken = nullptr;

    for (Ref page : mainPages())
        page->disconnectFramesFromPage();

    for (Ref webUserContentControllerProxy : m_webUserContentControllerProxies)
        webUserContentControllerProxy->removeProcess(*this);
    m_webUserContentControllerProxies.clear();

    m_userInitiatedActionMap.clear();

    if (RefPtr webLockRegistry = m_webLockRegistry.get())
        webLockRegistry->processDidExit();

#if ENABLE(ROUTING_ARBITRATION)
    if (RefPtr routingArbitrator = m_routingArbitrator.get())
        routingArbitrator->processDidTerminate();
#endif

    Ref<WebProcessPool> { processPool() }->disconnectProcess(*this);
}

RefPtr<WebPageProxy> WebProcessProxy::webPage(WebPageProxyIdentifier pageID)
{
    return globalPageMap().get(pageID);
}

RefPtr<WebPageProxy> WebProcessProxy::webPage(PageIdentifier pageID)
{
    for (Ref page : globalPages()) {
        if (page->webPageIDInMainFrameProcess() == pageID)
            return page;
    }

    return nullptr;
}

RefPtr<WebPageProxy> WebProcessProxy::audioCapturingWebPage()
{
    for (Ref page : globalPages()) {
        if (page->hasActiveAudioStream())
            return page.ptr();
    }
    return nullptr;
}

#if ENABLE(WEBXR) && !USE(OPENXR)
RefPtr<WebPageProxy> WebProcessProxy::webPageWithActiveXRSession()
{
    for (Ref page : globalPages()) {
        if (page->xrSystem() && page->xrSystem()->hasActiveSession())
            return page;
    }
    return nullptr;
}
#endif

void WebProcessProxy::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebProcess::SetThirdPartyCookieBlockingMode(thirdPartyCookieBlockingMode), WTFMove(completionHandler));
}

Ref<WebPageProxy> WebProcessProxy::createWebPage(PageClient& pageClient, Ref<API::PageConfiguration>&& pageConfiguration)
{
    Ref webPage = WebPageProxy::create(pageClient, *this, WTFMove(pageConfiguration));

    addExistingWebPage(webPage.get(), BeginsUsingDataStore::Yes);

    return webPage;
}

bool WebProcessProxy::shouldTakeNearSuspendedAssertion() const
{
#if USE(RUNNINGBOARD)
    if (m_pageMap.isEmpty()) {
        // The setting come from pages but this process has no page, we thus use the default
        // setting value.
        return defaultShouldTakeNearSuspendedAssertion();
    }

    for (auto& page : m_pageMap.values()) {
        bool processSuppressionEnabled = page->preferences().pageVisibilityBasedProcessSuppressionEnabled();
        bool nearSuspendedAssertionsEnabled = page->preferences().shouldTakeNearSuspendedAssertions();
        if (nearSuspendedAssertionsEnabled || !processSuppressionEnabled)
            return true;
    }
#endif
    return false;
}

bool WebProcessProxy::shouldDropNearSuspendedAssertionAfterDelay() const
{
    if (m_pageMap.isEmpty()) {
        // The setting come from pages but this process has no page, we thus use the default setting value.
        return defaultShouldDropNearSuspendedAssertionAfterDelay();
    }
    return WTF::anyOf(m_pageMap.values(), [](auto& page) { return page->preferences().shouldDropNearSuspendedAssertionAfterDelay(); });
}

void WebProcessProxy::addExistingWebPage(WebPageProxy& webPage, BeginsUsingDataStore beginsUsingDataStore)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "addExistingWebPage: webPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &webPage, webPage.identifier().toUInt64(), webPage.webPageIDInMainFrameProcess().toUInt64());

    ASSERT(!m_pageMap.contains(webPage.identifier()));
    ASSERT(!globalPageMap().contains(webPage.identifier()));
    RELEASE_ASSERT(!m_isInProcessCache);
    ASSERT(!m_websiteDataStore || websiteDataStore() == &webPage.websiteDataStore());

    bool wasStandaloneServiceWorkerProcess = isStandaloneServiceWorkerProcess();

    if (beginsUsingDataStore == BeginsUsingDataStore::Yes) {
        RELEASE_ASSERT(m_processPool);
        protectedProcessPool()->pageBeginUsingWebsiteDataStore(webPage, webPage.protectedWebsiteDataStore());
    }

    initializePreferencesForGPUAndNetworkProcesses(webPage);

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    if (webPage.preferences().backgroundWebContentRunningBoardThrottlingEnabled())
        setRunningBoardThrottlingEnabled();
#endif
    markProcessAsRecentlyUsed();
    m_pageMap.set(webPage.identifier(), webPage);
    globalPageMap().set(webPage.identifier(), webPage);

    logger().setEnabled(this, isAlwaysOnLoggingAllowed());

    Ref protectedThrottler = throttler();
    protectedThrottler->setShouldTakeNearSuspendedAssertion(shouldTakeNearSuspendedAssertion());
    protectedThrottler->setShouldDropNearSuspendedAssertionAfterDelay(shouldDropNearSuspendedAssertionAfterDelay());

    updateRegistrationWithDataStore();
    updateBackgroundResponsivenessTimer();
    protectedWebsiteDataStore()->propagateSettingUpdatesToNetworkProcess();

    // If this was previously a standalone worker process with no pages we need to call didChangeThrottleState()
    // to update our process assertions on the network process since standalone worker processes do not hold
    // assertions on the network process
    if (wasStandaloneServiceWorkerProcess)
        didChangeThrottleState(throttler().currentState());
}

void WebProcessProxy::markIsNoLongerInPrewarmedPool()
{
    ASSERT(m_isPrewarmed);
    WEBPROCESSPROXY_RELEASE_LOG(Process, "markIsNoLongerInPrewarmedPool:");

    m_isPrewarmed = false;
    RELEASE_ASSERT(m_processPool);
    m_processPool.setIsWeak(IsWeak::No);

    send(Messages::WebProcess::MarkIsNoLongerPrewarmed(), 0);

    updateRuntimeStatistics();
}

void WebProcessProxy::removeWebPage(WebPageProxy& webPage, EndsUsingDataStore endsUsingDataStore)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "removeWebPage: webPage=%p, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, &webPage, webPage.identifier().toUInt64(), webPage.webPageIDInMainFrameProcess().toUInt64());
    RefPtr removedPage = m_pageMap.take(webPage.identifier()).get();
    ASSERT_UNUSED(removedPage, removedPage == &webPage);
    removedPage = globalPageMap().take(webPage.identifier()).get();
    ASSERT_UNUSED(removedPage, removedPage == &webPage);

    logger().setEnabled(this, isAlwaysOnLoggingAllowed());

    reportProcessDisassociatedWithPageIfNecessary(webPage.identifier());

    if (endsUsingDataStore == EndsUsingDataStore::Yes)
        protectedProcessPool()->pageEndUsingWebsiteDataStore(webPage, webPage.protectedWebsiteDataStore());

    removeVisitedLinkStoreUser(webPage.protectedVisitedLinkStore(), webPage.identifier());
    updateRegistrationWithDataStore();
    updateAudibleMediaAssertions();
    updateMediaStreamingActivity();
    updateBackgroundResponsivenessTimer();
    protectedWebsiteDataStore()->propagateSettingUpdatesToNetworkProcess();

    maybeShutDown();
}

void WebProcessProxy::addVisitedLinkStoreUser(VisitedLinkStore& visitedLinkStore, WebPageProxyIdentifier pageID)
{
    auto& users = m_visitedLinkStoresWithUsers.ensure(visitedLinkStore, [] {
        return HashSet<WebPageProxyIdentifier> { };
    }).iterator->value;

    ASSERT(!users.contains(pageID));
    users.add(pageID);

    if (users.size() == 1)
        visitedLinkStore.addProcess(*this);
}

void WebProcessProxy::removeVisitedLinkStoreUser(VisitedLinkStore& visitedLinkStore, WebPageProxyIdentifier pageID)
{
    auto it = m_visitedLinkStoresWithUsers.find(visitedLinkStore);
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
    m_webUserContentControllerProxies.add(proxy);
    proxy.addProcess(*this);
}

void WebProcessProxy::didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy& proxy)
{
    ASSERT(m_webUserContentControllerProxies.contains(proxy));
    m_webUserContentControllerProxies.remove(proxy);
}

static bool networkProcessWillCheckBlobFileAccess()
{
#if PLATFORM(COCOA)
    return WTF::linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::BlobFileAccessEnforcement);
#else
    return true;
#endif
}

void WebProcessProxy::assumeReadAccessToBaseURL(WebPageProxy& page, const String& urlString, CompletionHandler<void()>&& completionHandler, bool directoryOnly)
{
    URL url { urlString };
    if (!url.protocolIsFile())
        return completionHandler();

    // There's a chance that urlString does not point to a directory.
    // Get url's base URL to add to m_localPathsWithAssumedReadAccess.
    auto path = url.truncatedForUseAsBase().fileSystemPath();
    if (path.isNull())
        return completionHandler();

    RefPtr dataStore = websiteDataStore();
    if (!dataStore)
        return completionHandler();
    auto afterAllowAccess = [weakThis = WeakPtr { *this }, weakPage = WeakPtr { page }, path, completionHandler = WTFMove(completionHandler)] mutable {
        if (!weakThis || !weakPage)
            return completionHandler();

        // Client loads an alternate string. This doesn't grant universal file read, but the web process is assumed
        // to have read access to this directory already.
        weakThis->m_localPathsWithAssumedReadAccess.add(path);
        weakPage->addPreviouslyVisitedPath(path);
        completionHandler();
    };

    if (!networkProcessWillCheckBlobFileAccess())
        return afterAllowAccess();

    if (directoryOnly)
        afterAllowAccess();
    else
        dataStore->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AllowFileAccessFromWebProcess(coreProcessIdentifier(), path), WTFMove(afterAllowAccess));
}

void WebProcessProxy::assumeReadAccessToBaseURLs(WebPageProxy& page, const Vector<String>& urls, CompletionHandler<void()>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore();
    if (!dataStore)
        return completionHandler();
    Vector<String> paths;
    for (auto& urlString : urls) {
        URL url { urlString };
        if (!url.protocolIsFile())
            continue;

        // There's a chance that urlString does not point to a directory.
        // Get url's base URL to add to m_localPathsWithAssumedReadAccess.
        auto path = url.truncatedForUseAsBase().fileSystemPath();
        if (path.isNull())
            return completionHandler();
        paths.append(path);
    }
    if (!paths.size())
        return completionHandler();

    if (!networkProcessWillCheckBlobFileAccess())
        return completionHandler();

    dataStore->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AllowFilesAccessFromWebProcess(coreProcessIdentifier(), WTFMove(paths)), [weakThis = WeakPtr { *this }, weakPage = WeakPtr { page }, paths, completionHandler = WTFMove(completionHandler)] mutable {
        if (!weakThis || !weakPage)
            return completionHandler();

        // Client loads an alternate string. This doesn't grant universal file read, but the web process is assumed
        // to have read access to this directory already.
        for (auto& path : paths) {
            weakThis->m_localPathsWithAssumedReadAccess.add(path);
            weakPage->addPreviouslyVisitedPath(path);
        }
        completionHandler();
    });
}

bool WebProcessProxy::hasAssumedReadAccessToURL(const URL& url) const
{
    if (!url.protocolIsFile())
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
    if (!url.protocolIsFile())
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
            URL itemURL { item->url() };
            if (itemURL.protocolIsFile() && itemURL.fileSystemPath() == path)
                return true;
            URL itemOriginalURL { item->originalURL() };
            if (itemOriginalURL.protocolIsFile() && itemOriginalURL.fileSystemPath() == path)
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

bool WebProcessProxy::shouldDisableJITCage() const
{
    return false;
}
#endif // !PLATFORM(COCOA)

bool WebProcessProxy::hasProvisionalPageWithID(WebPageProxyIdentifier pageID) const
{
    for (Ref provisionalPage : m_provisionalPages) {
        if (provisionalPage->page() && provisionalPage->page()->identifier() == pageID)
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

void WebProcessProxy::updateBackForwardItem(Ref<FrameState>&& frameState)
{
    RefPtr frameItem = frameState->identifier ? WebBackForwardListFrameItem::itemForID(*frameState->identifier) : nullptr;
    if (!frameItem)
        return;

    RefPtr item = frameItem->backForwardListItem();
    if (!item || !isAllowedToUpdateBackForwardItem(*item))
        return;

    if (!!item->backForwardCacheEntry() != frameState->hasCachedPage) {
        if (frameState->hasCachedPage)
            protectedProcessPool()->protectedBackForwardCache()->addEntry(*item, coreProcessIdentifier());
        else if (!item->suspendedPage())
            protectedProcessPool()->protectedBackForwardCache()->removeEntry(*item);
    }

    frameItem->setFrameState(WTFMove(frameState));
}

void WebProcessProxy::getNetworkProcessConnection(CompletionHandler<void(NetworkProcessConnectionInfo&&)>&& reply)
{
    RefPtr dataStore = websiteDataStore();
    if (!dataStore) {
        ASSERT_NOT_REACHED();
        RELEASE_LOG_FAULT(Process, "WebProcessProxy should always have a WebsiteDataStore when used by a web process requesting a network process connection");
        return reply({ });
    }
    dataStore->getNetworkProcessConnection(*this, WTFMove(reply));
}

#if ENABLE(GPU_PROCESS)

void WebProcessProxy::createGPUProcessConnection(GPUProcessConnectionIdentifier identifier, IPC::Connection::Handle&& connectionHandle)
{
    WebKit::GPUProcessConnectionParameters parameters;
#if ASSERT_ENABLED && HAVE(TASK_IDENTITY_TOKEN)
    if (!WebCore::isMemoryAttributionDisabled())
        ASSERT(m_processIdentity);
#endif
    parameters.webProcessIdentity = m_processIdentity;
    parameters.sharedPreferencesForWebProcess = m_sharedPreferencesForWebProcess;
#if ENABLE(IPC_TESTING_API)
    parameters.ignoreInvalidMessageForTesting = ignoreInvalidMessageForTesting();
#endif
    parameters.isLockdownModeEnabled = lockdownMode() == WebProcessProxy::LockdownMode::Enabled;
    ASSERT(!m_gpuProcessConnectionIdentifier);
    m_gpuProcessConnectionIdentifier = identifier;
    protectedProcessPool()->createGPUProcessConnection(*this, WTFMove(connectionHandle), WTFMove(parameters));
}

void WebProcessProxy::gpuProcessConnectionDidBecomeUnresponsive(GPUProcessConnectionIdentifier identifier)
{
    if (identifier != m_gpuProcessConnectionIdentifier)
        return;
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "gpuProcessConnectionDidBecomeUnresponsive");
    if (RefPtr process = protectedProcessPool()->gpuProcess())
        process->childConnectionDidBecomeUnresponsive();
}

void WebProcessProxy::gpuProcessDidFinishLaunching()
{
    for (Ref page : pages())
        page->gpuProcessDidFinishLaunching();
}

void WebProcessProxy::gpuProcessExited(ProcessTerminationReason reason)
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "gpuProcessExited: reason=%" PUBLIC_LOG_STRING, processTerminationReasonToString(reason).characters());
    m_gpuProcessConnectionIdentifier = std::nullopt;
    for (Ref page : pages())
        page->gpuProcessExited(reason);
}
#endif

#if ENABLE(MODEL_PROCESS)
void WebProcessProxy::createModelProcessConnection(IPC::Connection::Handle&& connectionIdentifier, WebKit::ModelProcessConnectionParameters&& parameters)
{
    bool anyPageHasModelProcessEnabled = false;
    for (auto& page : m_pageMap.values())
        anyPageHasModelProcessEnabled |= page->preferences().modelElementEnabled() && page->preferences().modelProcessEnabled();
    MESSAGE_CHECK(anyPageHasModelProcessEnabled);

    parameters.sharedPreferencesForWebProcess = m_sharedPreferencesForWebProcess;
    MESSAGE_CHECK(parameters.sharedPreferencesForWebProcess.modelElementEnabled);
    MESSAGE_CHECK(parameters.sharedPreferencesForWebProcess.modelProcessEnabled);

#if ENABLE(IPC_TESTING_API)
    parameters.ignoreInvalidMessageForTesting = ignoreInvalidMessageForTesting();
#endif

#if HAVE(AUDIT_TOKEN)
    parameters.presentingApplicationAuditToken = m_processPool->configuration().presentingApplicationProcessToken();
#endif

    protectedProcessPool()->createModelProcessConnection(*this, WTFMove(connectionIdentifier), WTFMove(parameters));
}

void WebProcessProxy::modelProcessDidFinishLaunching()
{
    for (auto& page : m_pageMap.values())
        page->modelProcessDidFinishLaunching();
}

void WebProcessProxy::modelProcessExited(ProcessTerminationReason reason)
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "modelProcessExited: reason=%{public}s", processTerminationReasonToString(reason).characters());

    for (auto& page : m_pageMap.values())
        page->modelProcessExited(reason);
}

#endif // ENABLE(MODEL_PROCESS)

#if !PLATFORM(MAC)
bool WebProcessProxy::shouldAllowNonValidInjectedCode() const
{
    return false;
}
#endif

bool WebProcessProxy::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    // If AuxiliaryProcessProxy gets .messages.in, use WantsDispatchMessages and remove this.
    if (AuxiliaryProcessProxy::dispatchMessage(connection, decoder))
        return true;
    if (protectedProcessPool()->dispatchMessage(connection, decoder))
        return true;

    // FIXME: Add unhandled message logging.
    // WebProcessProxy will receive messages to instances that were removed from
    // the message receiver map. Filter these out.
    return true;
}

bool WebProcessProxy::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    // If AuxiliaryProcessProxy gets .messages.in, use WantsDispatchMessages and remove this.
    if (AuxiliaryProcessProxy::dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;
    if (protectedProcessPool()->dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;
    // WebProcessProxy will receive messages to instances that were removed from
    // the message receiver map. Mark all messages as handled. Unreplied messages
    // will be cancelled by the caller.
    return true;
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

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
Ref<UserMediaCaptureManagerProxy> WebProcessProxy::protectedUserMediaCaptureManagerProxy()
{
    return m_userMediaCaptureManagerProxy.get();
}
#endif

void WebProcessProxy::processDidTerminateOrFailedToLaunch(ProcessTerminationReason reason)
{
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "processDidTerminateOrFailedToLaunch: reason=%" PUBLIC_LOG_STRING, processTerminationReasonToString(reason).characters());

    // Protect ourselves, as the call to shutDown() below may otherwise cause us
    // to be deleted before we can finish our work.
    Ref protectedThis { *this };

    liveProcessesLRU().remove(*this);

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    m_userMediaCaptureManagerProxy->clear();
#endif

    auto pages = mainPages();

    Vector<Ref<ProvisionalPageProxy>> provisionalPages;
    m_provisionalPages.forEach([&] (auto& page) {
        provisionalPages.append(page);
    });

    auto isResponsiveCallbacks = std::exchange(m_isResponsiveCallbacks, { });
    for (auto& callback : isResponsiveCallbacks)
        callback(false);

    if (isStandaloneServiceWorkerProcess())
        protectedProcessPool()->serviceWorkerProcessCrashed(*this, reason);

    shutDown();

    // FIXME: Perhaps this should consider ProcessTerminationReasons ExceededMemoryLimit, ExceededCPULimit, Unresponsive as well.
    if (pages.size() == 1 && reason == ProcessTerminationReason::Crash) {
        auto& page = pages[0];
        auto domain = PublicSuffixStore::singleton().topPrivatelyControlledDomain(URL({ }, page->currentURL()).host());
        if (!domain.isEmpty())
            page->logDiagnosticMessageWithEnhancedPrivacy(WebCore::DiagnosticLoggingKeys::domainCausingCrashKey(), domain, WebCore::ShouldSample::No);
    }

#if ENABLE(ROUTING_ARBITRATION)
    if (RefPtr routingArbitrator = m_routingArbitrator.get())
        routingArbitrator->processDidTerminate();
#endif

    // There is a nested transaction in WebPageProxy::resetStateAfterProcessExited() that we don't want to commit before the client call below (dispatchProcessDidTerminate).
    auto pageLoadStateTransactions = WTF::map(pages, [&](auto& page) {
        auto transaction = page->pageLoadState().transaction();
        page->resetStateAfterProcessTermination(reason);
        return transaction;
    });

    for (auto& provisionalPage : provisionalPages)
        provisionalPage->processDidTerminate();

    for (auto& page : pages)
        page->dispatchProcessDidTerminate(*this, reason);

    for (Ref remotePage : m_remotePages)
        remotePage->processDidTerminate(*this, reason);
}

void WebProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName, int32_t indexOfObjectFailingDecoding)
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

    for (Ref page : pages())
        page->processDidBecomeUnresponsive();

    bool isWebProcessResponsive = false;
    for (auto& callback : isResponsiveCallbacks)
        callback(isWebProcessResponsive);

    // If the web process becomes unresponsive and only runs service/shared workers, kill it ourselves since there are no native clients to do it.
    if (isRunningWorkers() && m_pageMap.isEmpty()) {
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(PerformanceLogging, "didBecomeUnresponsive: Terminating worker-only web process because it is unresponsive");
        disableRemoteWorkers({ RemoteWorkerType::ServiceWorker, RemoteWorkerType::SharedWorker });
        terminate();
    }
}

void WebProcessProxy::didBecomeResponsive()
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "didBecomeResponsive:");
    m_isResponsive = NoOrMaybe::Maybe;

    for (Ref page : pages())
        page->processDidBecomeResponsive();
}

void WebProcessProxy::willChangeIsResponsive()
{
    for (Ref page : pages())
        page->willChangeProcessIsResponsive();
}

void WebProcessProxy::didChangeIsResponsive()
{
    for (Ref page : pages())
        page->didChangeProcessIsResponsive();
}

#if ENABLE(IPC_TESTING_API)
void WebProcessProxy::setIgnoreInvalidMessageForTesting()
{
    if (state() == State::Running)
        protectedConnection()->setIgnoreInvalidMessageForTesting();
    m_ignoreInvalidMessageForTesting = true;
}
#endif

void WebProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    WEBPROCESSPROXY_RELEASE_LOG(Process, "didFinishLaunching:");
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());

    Ref protectedThis { *this };
    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!connectionIdentifier) {
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "didFinishLaunching: Invalid connection identifier (web process failed to launch)");
        processDidTerminateOrFailedToLaunch(ProcessTerminationReason::Crash);
        return;
    }

#if PLATFORM(COCOA)
    if (RefPtr websiteDataStore = m_websiteDataStore)
        websiteDataStore->protectedNetworkProcess()->sendXPCEndpointToProcess(*this);
#endif

    protectedProcessPool()->processDidFinishLaunching(*this);
    m_backgroundResponsivenessTimer.updateState();

#if ENABLE(IPC_TESTING_API)
    if (m_ignoreInvalidMessageForTesting)
        protectedConnection()->setIgnoreInvalidMessageForTesting();
#endif

#if USE(RUNNINGBOARD) && PLATFORM(MAC)
    for (Ref page : mainPages()) {
        if (page->preferences().backgroundWebContentRunningBoardThrottlingEnabled())
            setRunningBoardThrottlingEnabled();
    }
#endif // USE(RUNNINGBOARD) && PLATFORM(MAC)

    Ref protectedThrottler = throttler();
    protectedThrottler->setShouldTakeNearSuspendedAssertion(shouldTakeNearSuspendedAssertion());
    protectedThrottler->setShouldDropNearSuspendedAssertionAfterDelay(shouldDropNearSuspendedAssertionAfterDelay());

#if PLATFORM(COCOA)
    unblockAccessibilityServerIfNeeded();
#if ENABLE(REMOTE_INSPECTOR)
    enableRemoteInspectorIfNeeded();
#endif
#endif

    beginResponsivenessChecks();
}

void WebProcessProxy::didDestroyFrame(IPC::Connection& connection, FrameIdentifier frameID, WebPageProxyIdentifier pageID)
{
    if (RefPtr page = m_pageMap.get(pageID))
        page->didDestroyFrame(connection, frameID);
}

auto WebProcessProxy::visiblePageToken() const -> VisibleWebPageToken
{
    return m_visiblePageCounter.count();
}

void WebProcessProxy::addPreviouslyApprovedFileURL(const URL& url)
{
    ASSERT(url.protocolIsFile());
    auto fileSystemPath = url.fileSystemPath();
    if (!fileSystemPath.isEmpty())
        m_previouslyApprovedFilePaths.add(fileSystemPath);
}

bool WebProcessProxy::wasPreviouslyApprovedFileURL(const URL& url) const
{
    ASSERT(url.protocolIsFile());
    auto fileSystemPath = url.fileSystemPath();
    if (fileSystemPath.isEmpty())
        return false;
    return m_previouslyApprovedFilePaths.contains(fileSystemPath);
}

void WebProcessProxy::recordUserGestureAuthorizationToken(PageIdentifier pageID, WTF::UUID authorizationToken)
{
    if (!UserInitiatedActionByAuthorizationTokenMap::isValidKey(authorizationToken) || !authorizationToken)
        return;

    m_userInitiatedActionByAuthorizationTokenMap.ensure(pageID, [] {
        return UserInitiatedActionByAuthorizationTokenMap { };
    }).iterator->value.ensure(authorizationToken, [authorizationToken] {
        Ref action = API::UserInitiatedAction::create();
        action->setAuthorizationToken(authorizationToken);
        return action;
    });
}

RefPtr<API::UserInitiatedAction> WebProcessProxy::userInitiatedActivity(std::optional<UserGestureTokenIdentifier> identifier)
{
    if (!identifier)
        return nullptr;

    auto result = m_userInitiatedActionMap.ensure(*identifier, [] {
        return API::UserInitiatedAction::create();
    });
    return result.iterator->value;
}

RefPtr<API::UserInitiatedAction> WebProcessProxy::userInitiatedActivity(PageIdentifier pageID, std::optional<WTF::UUID> authorizationToken, std::optional<UserGestureTokenIdentifier> identifier)
{
    if (!identifier)
        return nullptr;

    if (authorizationToken) {
        auto authorizationTokenMapByPageIterator = m_userInitiatedActionByAuthorizationTokenMap.find(pageID);
        if (authorizationTokenMapByPageIterator != m_userInitiatedActionByAuthorizationTokenMap.end()) {
            auto it = authorizationTokenMapByPageIterator->value.find(*authorizationToken);
            if (it != authorizationTokenMapByPageIterator->value.end()) {
                auto result = m_userInitiatedActionMap.ensure(*identifier, [it] {
                    return it->value;
                });
                return result.iterator->value;
            }
        }
    }

    return userInitiatedActivity(identifier);
}

void WebProcessProxy::consumeIfNotVerifiablyFromUIProcess(PageIdentifier pageID, API::UserInitiatedAction& action, std::optional<WTF::UUID> authToken)
{
    auto authorizationTokenMapByPageIterator = m_userInitiatedActionByAuthorizationTokenMap.find(pageID);
    if (authorizationTokenMapByPageIterator != m_userInitiatedActionByAuthorizationTokenMap.end()) {
        if (authToken && authorizationTokenMapByPageIterator->value.contains(*authToken)) {
            m_userInitiatedActionByAuthorizationTokenMap.remove(authorizationTokenMapByPageIterator);
            return;
        }
    }
    action.setConsumed();
}

bool WebProcessProxy::isResponsive() const
{
    return responsivenessTimer().isResponsive() && m_backgroundResponsivenessTimer.isResponsive();
}

void WebProcessProxy::didDestroyUserGestureToken(PageIdentifier pageID, UserGestureTokenIdentifier identifier)
{
    auto authorizationTokenMapByPageIterator = m_userInitiatedActionByAuthorizationTokenMap.find(pageID);
    if (authorizationTokenMapByPageIterator != m_userInitiatedActionByAuthorizationTokenMap.end()) {
        if (auto removed = m_userInitiatedActionMap.take(identifier); removed && removed->authorizationToken()) {
            authorizationTokenMapByPageIterator->value.remove(*removed->authorizationToken());
            if (authorizationTokenMapByPageIterator->value.isEmpty())
                m_userInitiatedActionByAuthorizationTokenMap.remove(authorizationTokenMapByPageIterator);
        }
    }
}

bool WebProcessProxy::canBeAddedToWebProcessCache() const
{
#if PLATFORM(IOS_FAMILY)
    // Don't add the Web process to the cache if there are still assertions being held, preventing it from suspending.
    // This is a fix for a regression in page load speed we see on http://www.youtube.com when adding it to the cache.
    if (PAL::deviceClassIsSmallScreen() && throttler().shouldBeRunnable()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canBeAddedToWebProcessCache: Not adding to process cache because the process is runnable");
        return false;
    }
#endif

    if (isRunningServiceWorkers()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canBeAddedToWebProcessCache: Not adding to process cache because the process is running workers");
        return false;
    }

    if (m_crossOriginMode == CrossOriginMode::Isolated) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canBeAddedToWebProcessCache: Not adding to process cache because the process is cross-origin isolated");
        return false;
    }

    if (m_usedForSiteIsolation) {
        // FIXME: The WebProcessCache is organized by RegistrableDomain not Site, and it is only set when the main frame loads a URL with that domain,
        // so processes used for iframes are not correctly reused. Implement this in a way that doesn't break PLT.
        return false;
    }

    if (WebKit::isInspectorProcessPool(protectedProcessPool()))
        return false;

    return true;
}

void WebProcessProxy::maybeShutDown()
{
    if (isDummyProcessProxy() && m_pageMap.isEmpty()) {
        ASSERT(state() == State::Terminated);
        protectedProcessPool()->disconnectProcess(*this);
        return;
    }

    if (state() == State::Terminated || !canTerminateAuxiliaryProcess())
        return;

    if (canBeAddedToWebProcessCache() && protectedProcessPool()->checkedWebProcessCache()->addProcessIfPossible(*this))
        return;

    shutDown();
}

bool WebProcessProxy::canTerminateAuxiliaryProcess()
{
    if (!m_pageMap.isEmpty()
        || !m_remotePages.isEmptyIgnoringNullReferences()
        || !m_suspendedPages.isEmptyIgnoringNullReferences()
        || !m_provisionalPages.isEmptyIgnoringNullReferences()
        || m_isInProcessCache
        || m_shutdownPreventingScopeCounter.value()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canTerminateAuxiliaryProcess: returns false (pageCount=%u, provisionalPageCount=%u, suspendedPageCount=%u, m_isInProcessCache=%d, m_shutdownPreventingScopeCounter=%lu)", m_pageMap.size(), m_provisionalPages.computeSize(), m_suspendedPages.computeSize(), m_isInProcessCache, m_shutdownPreventingScopeCounter.value());
        return false;
    }

    if (isRunningServiceWorkers()) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "canTerminateAuxiliaryProcess: returns false because process is running service workers");
        return false;
    }

    if (!protectedProcessPool()->shouldTerminate(*this)) {
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
    for (Ref page : pages())
        page->activityStateDidChange(ActivityState::IsVisuallyIdle);
}

#if HAVE(MOUSE_DEVICE_OBSERVATION)

void WebProcessProxy::notifyHasMouseDeviceChanged(bool hasMouseDevice)
{
    ASSERT(isMainRunLoop());
    for (auto webProcessProxy : WebProcessProxy::allProcesses())
        webProcessProxy->send(Messages::WebProcess::SetHasMouseDevice(hasMouseDevice), 0);
}

#endif // HAVE(MOUSE_DEVICE_OBSERVATION)

#if HAVE(STYLUS_DEVICE_OBSERVATION)

void WebProcessProxy::notifyHasStylusDeviceChanged(bool hasStylusDevice)
{
    ASSERT(isMainRunLoop());
    for (auto webProcessProxy : WebProcessProxy::allProcesses())
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
    WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "requestTermination: reason=%d", static_cast<int>(reason));

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

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle: {
                ASSERT(static_cast<API::FrameHandle&>(object).isAutoconverting());
                auto frameID = static_cast<API::FrameHandle&>(object).frameID();
                return WebFrameProxy::webFrame(frameID);
            }
            case API::Object::Type::PageHandle:
                ASSERT(static_cast<API::PageHandle&>(object).isAutoconverting());
                return protectedProcess()->webPage(static_cast<API::PageHandle&>(object).pageProxyID());

            default:
                return &object;
            }
        }

        Ref<WebProcessProxy> protectedProcess() const { return m_webProcessProxy.get(); }

        WeakRef<WebProcessProxy> m_webProcessProxy;
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
                return API::PageHandle::createAutoconverting(static_cast<const WebPageProxy&>(object).identifier(), static_cast<const WebPageProxy&>(object).webPageIDInMainFrameProcess());

            default:
                return &object;
            }
        }
    };

    return UserData::transform(object, Transformer());
}

void WebProcessProxy::sendPrepareToSuspend(IsSuspensionImminent isSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "sendPrepareToSuspend: isSuspensionImminent=%d", isSuspensionImminent == IsSuspensionImminent::Yes);
    sendWithAsyncReply(Messages::WebProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes, MonotonicTime::now() + Seconds(remainingRunTime)), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void WebProcessProxy::sendProcessDidResume(ResumeReason)
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "sendProcessDidResume:");
    if (canSendMessage())
        send(Messages::WebProcess::ProcessDidResume(), 0);
}

void WebProcessProxy::setThrottleStateForTesting(ProcessThrottleState state)
{
    m_areThrottleStateChangesEnabled = true;
    didChangeThrottleState(state);
    m_areThrottleStateChangesEnabled = false;
}

void WebProcessProxy::didChangeThrottleState(ProcessThrottleState type)
{
    AuxiliaryProcessProxy::didChangeThrottleState(type);

    auto scope = makeScopeExit([this]() {
        updateRuntimeStatistics();
    });

    if (UNLIKELY(!m_areThrottleStateChangesEnabled))
        return;
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didChangeThrottleState: type=%u", (unsigned)type);

    if (isStandaloneServiceWorkerProcess()) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didChangeThrottleState: Release all assertions for network process because this is a service worker process without page");
        m_foregroundToken = nullptr;
        m_backgroundToken = nullptr;
        return;
    }

    ASSERT(!m_backgroundToken || !m_foregroundToken);

    switch (type) {
    case ProcessThrottleState::Suspended:
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didChangeThrottleState(Suspended) Release all assertions for network process");
        m_foregroundToken = nullptr;
        m_backgroundToken = nullptr;
#if PLATFORM(IOS_FAMILY)
        for (Ref page : pages())
            page->processWillBecomeSuspended();
#endif
        break;

    case ProcessThrottleState::Background:
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didChangeThrottleState(Background) Taking background assertion for network process");
        m_backgroundToken = protectedProcessPool()->backgroundWebProcessToken();
        m_foregroundToken = nullptr;
        break;
    
    case ProcessThrottleState::Foreground:
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "didChangeThrottleState(Foreground) Taking foreground assertion for network process");
        m_foregroundToken = protectedProcessPool()->foregroundWebProcessToken();
        m_backgroundToken = nullptr;
#if PLATFORM(IOS_FAMILY)
        for (Ref page : pages())
            page->processWillBecomeForeground();
#endif
        break;
    }

    ASSERT(!m_backgroundToken || !m_foregroundToken);
    m_backgroundResponsivenessTimer.updateState();
}

void WebProcessProxy::didDropLastAssertion()
{
    m_backgroundResponsivenessTimer.updateState();
    updateRuntimeStatistics();
}

void WebProcessProxy::prepareToDropLastAssertion(CompletionHandler<void()>&& completionHandler)
{
#if !ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER) && ENABLE(WEBPROCESS_CACHE)
    if (isInProcessCache() || !m_suspendedPages.isEmptyIgnoringNullReferences() || (canTerminateAuxiliaryProcess() && canBeAddedToWebProcessCache())) {
        // We avoid freeing caches if:
        //
        //  1. The process is already in the WebProcess cache.
        //  2. The process is already in the back/forward cache.
        //  3. The process might end up in the process cache (canTerminateAuxiliaryProcess() && canBeAddedToWebProcessCache())
        //
        // The idea here is that we want these cached processes to retain useful data if they're
        // reused. They have a low jetsam priority and will be killed by our low memory handler or
        // the kernel if necessary.
        return completionHandler();
    }
    // When the WebProcess cache is enabled, instead of freeing caches in the PrepareToSuspend
    // we free caches here just before we drop our last process assertion.
    sendWithAsyncReply(Messages::WebProcess::ReleaseMemory(), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
#else
    completionHandler();
#endif
}

String WebProcessProxy::environmentIdentifier() const
{
    if (m_environmentIdentifier.isEmpty()) {
        StringBuilder builder;
        builder.append(clientName());
        builder.append(processID());
        m_environmentIdentifier = builder.toString();
    }
    return m_environmentIdentifier;
}

void WebProcessProxy::updateAudibleMediaAssertions()
{
#if ENABLE(EXTENSION_CAPABILITIES)
    if (PlatformMediaSessionManager::mediaCapabilityGrantsEnabled())
        return;
#endif

    bool hasAudibleWebPage = WTF::anyOf(pages(), [] (auto& page) {
        return page->isPlayingAudio();
    });

    if (!!m_audibleMediaActivity == hasAudibleWebPage)
        return;

    if (hasAudibleWebPage) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: Taking MediaPlayback assertion for WebProcess");
        m_audibleMediaActivity = AudibleMediaActivity {
            ProcessAssertion::create(*this, "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback),
            protectedProcessPool()->webProcessWithAudibleMediaToken()
        };
    } else {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: Releasing MediaPlayback assertion for WebProcess");
        m_audibleMediaActivity = std::nullopt;
    }
}

void WebProcessProxy::updateMediaStreamingActivity()
{
    bool hasMediaStreamingWebPage = WTF::anyOf(pages(), [] (auto& page) {
        return page->hasMediaStreaming();
    });

    if (!!m_mediaStreamingActivity == hasMediaStreamingWebPage)
        return;

    if (hasMediaStreamingWebPage) {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateMediaStreamingActivity: Start Media Networking Activity for WebProcess");
        m_mediaStreamingActivity = protectedProcessPool()->webProcessWithMediaStreamingToken();
    } else {
        WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateMediaStreamingActivity: Stop Media Networking Activity for WebProcess");
        m_mediaStreamingActivity = std::nullopt;
    }
}

void WebProcessProxy::isResponsive(CompletionHandler<void(bool isWebProcessResponsive)>&& callback)
{
    if (m_isResponsive == NoOrMaybe::No) {
        if (callback) {
            RunLoop::protectedMain()->dispatch([callback = WTFMove(callback)]() mutable {
                bool isWebProcessResponsive = false;
                callback(isWebProcessResponsive);
            });
        }
        return;
    }

    if (callback)
        m_isResponsiveCallbacks.append(WTFMove(callback));

    checkForResponsiveness([weakThis = WeakPtr { *this }]() mutable {
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
        checkForResponsiveness([weakThis = WeakPtr { *this }]() mutable {
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
    if (pageCount()) {
        if (RefPtr page = pages()[0].ptr())
            page->logDiagnosticMessage(DiagnosticLoggingKeys::simulatedPageCrashKey(), limitKey, ShouldSample::No);
    }
}

void WebProcessProxy::memoryPressureStatusChanged(SystemMemoryPressureStatus status)
{
    m_memoryPressureStatus = status;

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    if (RefPtr pool = m_processPool.get())
        pool->memoryPressureStatusChangedForProcess(*this, status);
#endif
}

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)

void WebProcessProxy::updateWebProcessSuspensionDelay()
{
    for (Ref page : pages())
        page->updateWebProcessSuspensionDelay();
}

#endif

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

void WebProcessProxy::didExceedMemoryFootprintThreshold(size_t footprint)
{
    WEBPROCESSPROXY_RELEASE_LOG(PerformanceLogging, "didExceedMemoryFootprintThreshold: WebProcess exceeded notification threshold (current footprint: %zu MB)", footprint >> 20);

    RefPtr dataStore = protectedWebsiteDataStore();
    if (!dataStore)
        return;

    String domain;
    bool wasPrivateRelayed = false;
    bool hasAllowedToRunInTheBackgroundActivity = false;

    for (Ref page : this->pages()) {
        auto pageDomain = PublicSuffixStore::singleton().topPrivatelyControlledDomain(URL({ }, page->currentURL()).host());
        if (domain.isEmpty())
            domain = WTFMove(pageDomain);
        else if (domain != pageDomain)
            domain = "multiple"_s;

        wasPrivateRelayed = wasPrivateRelayed || page->protectedPageLoadState()->wasPrivateRelayed();
        hasAllowedToRunInTheBackgroundActivity = hasAllowedToRunInTheBackgroundActivity || page->hasAllowedToRunInTheBackgroundActivity();
    }

    if (domain.isEmpty())
        domain = "unknown"_s;

    auto activeTime = totalForegroundTime() + totalBackgroundTime() + totalSuspendedTime();
    dataStore->client().didExceedMemoryFootprintThreshold(footprint, domain, pageCount(), activeTime, throttler().currentState() == ProcessThrottleState::Foreground, wasPrivateRelayed ? WebCore::WasPrivateRelayed::Yes : WebCore::WasPrivateRelayed::No, hasAllowedToRunInTheBackgroundActivity ? WebsiteDataStoreClient::CanSuspend::No : WebsiteDataStoreClient::CanSuspend::Yes);
}

void WebProcessProxy::didExceedCPULimit()
{
    Ref protectedThis { *this };

    for (Ref page : pages()) {
        if (page->isPlayingAudio()) {
            WEBPROCESSPROXY_RELEASE_LOG(PerformanceLogging, "didExceedCPULimit: WebProcess has exceeded the background CPU limit but we are not terminating it because there is audio playing");
            return;
        }

        if (page->hasActiveAudioStream() || page->hasActiveVideoStream()) {
            WEBPROCESSPROXY_RELEASE_LOG(PerformanceLogging, "didExceedCPULimit: WebProcess has exceeded the background CPU limit but we are not terminating it because it is capturing audio / video");
            return;
        }

        if (page->isViewVisible()) {
            // We only notify the client that the process exceeded the CPU limit when it is visible, we do not terminate it.
            WEBPROCESSPROXY_RELEASE_LOG(PerformanceLogging, "didExceedCPULimit: WebProcess has exceeded the background CPU limit but we are not terminating it because it has a visible page");
            return;
        }
    }

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    // This background WebProcess is using too much CPU so we try to suspend it if possible.
    if (runningBoardThrottlingEnabled() && !throttler().isSuspended() && !isRunningServiceWorkers()) {
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(PerformanceLogging, "didExceedCPULimit: Suspending background WebProcess that has exceeded the background CPU limit");
        throttler().invalidateAllActivitiesAndDropAssertion();
        return;
    }
#endif

    // We were unable to suspend the process or we are running service workers so we're terminating it.
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
    protectedProcessPool()->didCollectPrewarmInformation(domain, prewarmInformation);
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
    if (m_site && m_site->isEmpty())
        return;

    if (url.protocolIsAbout())
        return;

    if (!url.protocolIsInHTTPFamily() && !processPool().configuration().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol()) {
        // Unless the processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol flag is set, we don't process swap on navigations withing the same
        // non HTTP(s) protocol. For this reason, we ignore the registrable domain and processes are not eligible for the process cache.
        m_site = Site { { }, { } };
        return;
    }

    auto site = WebCore::Site { url };
    RefPtr dataStore = websiteDataStore();
    if (dataStore && m_site && *m_site != site) {
        if (isRunningServiceWorkers())
            dataStore->protectedNetworkProcess()->terminateRemoteWorkerContextConnectionWhenPossible(RemoteWorkerType::ServiceWorker, dataStore->sessionID(), m_site->domain(), coreProcessIdentifier());
        if (isRunningSharedWorkers())
            dataStore->protectedNetworkProcess()->terminateRemoteWorkerContextConnectionWhenPossible(RemoteWorkerType::SharedWorker, dataStore->sessionID(), m_site->domain(), coreProcessIdentifier());

        // Null out registrable domain since this process has now been used for several domains.
        m_site = Site { { }, { } };
        return;
    }

    // Associate the process with this site.
    m_site = WTFMove(site);
}

void WebProcessProxy::addSuspendedPageProxy(SuspendedPageProxy& suspendedPage)
{
    m_suspendedPages.add(suspendedPage);
    auto suspendedPageCount = this->suspendedPageCount();
    WEBPROCESSPROXY_RELEASE_LOG(Process, "addSuspendedPageProxy: suspendedPageCount=%u", suspendedPageCount);
    if (suspendedPageCount == 1)
        send(Messages::WebProcess::SetHasSuspendedPageProxy(true), 0);
}

void WebProcessProxy::removeSuspendedPageProxy(SuspendedPageProxy& suspendedPage)
{
    ASSERT(m_suspendedPages.contains(suspendedPage));
    m_suspendedPages.remove(suspendedPage);
    auto suspendedPageCount = this->suspendedPageCount();
    WEBPROCESSPROXY_RELEASE_LOG(Process, "removeSuspendedPageProxy: suspendedPageCount=%u", suspendedPageCount);
    if (!suspendedPageCount) {
        reportProcessDisassociatedWithPageIfNecessary(suspendedPage.page().identifier());
        send(Messages::WebProcess::SetHasSuspendedPageProxy(false), 0);
        maybeShutDown();
    }
}

void WebProcessProxy::reportProcessDisassociatedWithPageIfNecessary(WebPageProxyIdentifier pageID)
{
    if (isAssociatedWithPage(pageID))
        return;

    if (RefPtr page = webPage(pageID))
        page->processIsNoLongerAssociatedWithPage(*this);
}

bool WebProcessProxy::isAssociatedWithPage(WebPageProxyIdentifier pageID) const
{
    if (m_pageMap.contains(pageID))
        return true;
    for (Ref provisionalPage : m_provisionalPages) {
        if (provisionalPage->page() && provisionalPage->page()->identifier() == pageID)
            return true;
    }
    for (auto& suspendedPage : m_suspendedPages) {
        if (suspendedPage.page().identifier() == pageID)
            return true;
    }
    return false;
}

WebProcessPool* WebProcessProxy::processPoolIfExists() const
{
    if (m_isPrewarmed || m_isInProcessCache)
        WEBPROCESSPROXY_RELEASE_LOG_ERROR(Process, "processPoolIfExists: trying to get WebProcessPool from an inactive WebProcessProxy");
    else
        ASSERT(m_processPool);
    return m_processPool.get();
}

void WebProcessProxy::enableMediaPlaybackIfNecessary()
{
    if (!m_sharedPreferencesForWebProcess.mediaPlaybackEnabled)
        return;

#if USE(AUDIO_SESSION)
    if (!WebCore::AudioSession::enableMediaPlayback())
        return;
#endif

#if ENABLE(ROUTING_ARBITRATION)
    ASSERT(!m_routingArbitrator);
    m_routingArbitrator = makeUniqueWithoutRefCountedCheck<AudioSessionRoutingArbitratorProxy>(*this);
#endif
}

std::optional<SharedPreferencesForWebProcess> WebProcessProxy::updateSharedPreferencesForWebProcess(const WebPreferencesStore& preferencesStore)
{
    if (WebKit::updateSharedPreferencesForWebProcess(m_sharedPreferencesForWebProcess, preferencesStore)) {
        ++m_sharedPreferencesForWebProcess.version;
        enableMediaPlaybackIfNecessary();

        return m_sharedPreferencesForWebProcess;
    }
    return std::nullopt;
}

void WebProcessProxy::didSyncSharedPreferencesForWebProcessWithNetworkProcess(uint64_t syncedSharedPreferencesVersion)
{
    m_sharedPreferencesVersionInNetworkProcess = syncedSharedPreferencesVersion;
    if (m_sharedPreferencesVersionInNetworkProcess < m_awaitedSharedPreferencesVersion
#if ENABLE(GPU_PROCESS)
        || m_sharedPreferencesVersionInGPUProcess < m_awaitedSharedPreferencesVersion
#endif
#if ENABLE(MODEL_PROCESS)
        || m_sharedPreferencesVersionInModelProcess < m_awaitedSharedPreferencesVersion
#endif
        )
        return;
    auto completionHandler = std::exchange(m_sharedPreferencesForWebProcessCompletionHandler, { });
    if (!completionHandler)
        return;
    completionHandler(true);
    m_awaitedSharedPreferencesVersion = 0;
}

#if ENABLE(GPU_PROCESS)
void WebProcessProxy::didSyncSharedPreferencesForWebProcessWithGPUProcess(uint64_t syncedSharedPreferencesVersion)
{
    m_sharedPreferencesVersionInGPUProcess = syncedSharedPreferencesVersion;
    if (m_sharedPreferencesVersionInNetworkProcess < m_awaitedSharedPreferencesVersion
        || m_sharedPreferencesVersionInGPUProcess < m_awaitedSharedPreferencesVersion
#if ENABLE(MODEL_PROCESS)
        || m_sharedPreferencesVersionInModelProcess < m_awaitedSharedPreferencesVersion
#endif
        )
        return;
    auto completionHandler = std::exchange(m_sharedPreferencesForWebProcessCompletionHandler, { });
    if (!completionHandler)
        return;
    completionHandler(true);
    m_awaitedSharedPreferencesVersion = 0;
}
#endif

#if ENABLE(MODEL_PROCESS)
void WebProcessProxy::didSyncSharedPreferencesForWebProcessWithModelProcess(uint64_t syncedSharedPreferencesVersion)
{
    m_sharedPreferencesVersionInModelProcess = syncedSharedPreferencesVersion;
    if (m_sharedPreferencesVersionInNetworkProcess < m_awaitedSharedPreferencesVersion
#if ENABLE(GPU_PROCESS)
        || m_sharedPreferencesVersionInGPUProcess < m_awaitedSharedPreferencesVersion
#endif
        || m_sharedPreferencesVersionInModelProcess < m_awaitedSharedPreferencesVersion)
        return;
    auto completionHandler = std::exchange(m_sharedPreferencesForWebProcessCompletionHandler, { });
    if (!completionHandler)
        return;
    completionHandler(true);
    m_awaitedSharedPreferencesVersion = 0;
}
#endif

void WebProcessProxy::waitForSharedPreferencesForWebProcessToSync(uint64_t sharedPreferencesVersion, CompletionHandler<void(bool success)>&& completionHandler)
{
    ASSERT(!m_sharedPreferencesForWebProcessCompletionHandler);
    ASSERT(!m_awaitedSharedPreferencesVersion);
    if (m_sharedPreferencesVersionInNetworkProcess >= sharedPreferencesVersion
#if ENABLE(GPU_PROCESS)
        || m_sharedPreferencesVersionInGPUProcess >= m_awaitedSharedPreferencesVersion
#endif
        )
        return completionHandler(true);
    m_awaitedSharedPreferencesVersion = sharedPreferencesVersion;
    m_sharedPreferencesForWebProcessCompletionHandler = WTFMove(completionHandler);
}

PAL::SessionID WebProcessProxy::sessionID() const
{
    ASSERT(m_websiteDataStore);
    return m_websiteDataStore->sessionID();
}

void WebProcessProxy::createSpeechRecognitionServer(SpeechRecognitionServerIdentifier identifier)
{
    RefPtr<WebPageProxy> targetPage;
    for (Ref page : pages()) {
        if (page->webPageIDInMainFrameProcess() == identifier) {
            targetPage = WTFMove(page);
            break;
        }
    }

    if (!targetPage)
        return;

    ASSERT(!m_speechRecognitionServerMap.contains(identifier));
    MESSAGE_CHECK(!m_speechRecognitionServerMap.contains(identifier));

    auto permissionChecker = [weakPage = WeakPtr { targetPage }](auto& request, SpeechRecognitionPermissionRequestCallback&& completionHandler) mutable {
        RefPtr page = weakPage.get();
        if (!page) {
            completionHandler(WebCore::SpeechRecognitionError { SpeechRecognitionErrorType::NotAllowed, "Page no longer exists"_s });
            return;
        }

        page->requestSpeechRecognitionPermission(request, WTFMove(completionHandler));
    };
    auto checkIfMockCaptureDevicesEnabled = [weakPage = WeakPtr { targetPage }]() {
        return weakPage && weakPage->protectedPreferences()->mockCaptureDevicesEnabled();
    };

    m_speechRecognitionServerMap.ensure(identifier, [&]() {
#if ENABLE(MEDIA_STREAM)
        auto createRealtimeMediaSource = [weakPage = WeakPtr { targetPage }]() {
            return weakPage ? weakPage->createRealtimeMediaSourceForSpeechRecognition() : CaptureSourceOrError { { "Page is invalid"_s, WebCore::MediaAccessDenialReason::InvalidAccess } };
        };
        Ref speechRecognitionServer = SpeechRecognitionServer::create(*this, identifier, WTFMove(permissionChecker), WTFMove(checkIfMockCaptureDevicesEnabled), WTFMove(createRealtimeMediaSource));
#else
        Ref speechRecognitionServer = SpeechRecognitionServer::create(*this, identifier, WTFMove(permissionChecker), WTFMove(checkIfMockCaptureDevicesEnabled));
#endif
        addMessageReceiver(Messages::SpeechRecognitionServer::messageReceiverName(), identifier, speechRecognitionServer);
        return speechRecognitionServer;
    });

}

void WebProcessProxy::destroySpeechRecognitionServer(SpeechRecognitionServerIdentifier identifier)
{
    if (RefPtr server = m_speechRecognitionServerMap.take(identifier))
        removeMessageReceiver(Messages::SpeechRecognitionServer::messageReceiverName(), identifier);
}

#if ENABLE(MEDIA_STREAM)

SpeechRecognitionRemoteRealtimeMediaSourceManager& WebProcessProxy::ensureSpeechRecognitionRemoteRealtimeMediaSourceManager()
{
    if (!m_speechRecognitionRemoteRealtimeMediaSourceManager) {
        m_speechRecognitionRemoteRealtimeMediaSourceManager = makeUniqueWithoutRefCountedCheck<SpeechRecognitionRemoteRealtimeMediaSourceManager>(*this);
        addMessageReceiver(Messages::SpeechRecognitionRemoteRealtimeMediaSourceManager::messageReceiverName(), *m_speechRecognitionRemoteRealtimeMediaSourceManager);
    }

    return *m_speechRecognitionRemoteRealtimeMediaSourceManager;
}

void WebProcessProxy::muteCaptureInPagesExcept(WebCore::PageIdentifier pageID)
{
#if PLATFORM(COCOA)
    for (Ref page : globalPages()) {
        if (page->webPageIDInMainFrameProcess() != pageID)
            page->setMediaStreamCaptureMuted(true);
    }
#else
    UNUSED_PARAM(pageID);
#endif
}

#endif

void WebProcessProxy::pageMutedStateChanged(WebCore::PageIdentifier identifier, WebCore::MediaProducerMutedStateFlags flags)
{
    bool mutedForCapture = flags.containsAny(MediaProducer::AudioAndVideoCaptureIsMuted);
    if (!mutedForCapture)
        return;

    if (RefPtr speechRecognitionServer = m_speechRecognitionServerMap.get(identifier))
        speechRecognitionServer->mute();
}

void WebProcessProxy::pageIsBecomingInvisible(WebCore::PageIdentifier identifier)
{
#if ENABLE(MEDIA_STREAM)
    if (!RealtimeMediaSourceCenter::shouldInterruptAudioOnPageVisibilityChange())
        return;
#endif

    if (RefPtr speechRecognitionServer = m_speechRecognitionServerMap.get(identifier))
        speechRecognitionServer->mute();
}

#if PLATFORM(WATCHOS)

void WebProcessProxy::startBackgroundActivityForFullscreenInput()
{
    if (m_backgroundActivityForFullscreenFormControls)
        return;

    m_backgroundActivityForFullscreenFormControls = throttler().backgroundActivity("Fullscreen input"_s);
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

void WebProcessProxy::establishRemoteWorkerContext(RemoteWorkerType workerType, const WebPreferencesStore& store, const Site& site, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, CompletionHandler<void()>&& completionHandler)
{
    updateSharedPreferencesForWebProcess(store);
    WEBPROCESSPROXY_RELEASE_LOG(Loading, "establishRemoteWorkerContext: Started (workerType=%" PUBLIC_LOG_STRING ")", workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared");
    markProcessAsRecentlyUsed();
    auto& remoteWorkerInformation = workerType == RemoteWorkerType::ServiceWorker ? m_serviceWorkerInformation : m_sharedWorkerInformation;
    sendWithAsyncReply(Messages::WebProcess::EstablishRemoteWorkerContextConnectionToNetworkProcess { workerType, processPool().defaultPageGroup().pageGroupID(), remoteWorkerInformation->remoteWorkerPageProxyID, remoteWorkerInformation->remoteWorkerPageID, store, site, serviceWorkerPageIdentifier, remoteWorkerInformation->initializationData }, [this, weakThis = WeakPtr { *this }, workerType, completionHandler = WTFMove(completionHandler)]() mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
        UNUSED_PARAM(workerType);
#endif
        if (weakThis)
            WEBPROCESSPROXY_RELEASE_LOG(Loading, "establishRemoteWorkerContext: Finished (workerType=%" PUBLIC_LOG_STRING ")", workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared");
        completionHandler();
    }, 0);
}

void WebProcessProxy::setRemoteWorkerUserAgent(const String& userAgent)
{
    if (m_serviceWorkerInformation)
        send(Messages::WebSWContextManagerConnection::SetUserAgent { userAgent }, 0);
    if (m_sharedWorkerInformation)
        send(Messages::WebSharedWorkerContextManagerConnection::SetUserAgent { userAgent }, 0);
}

void WebProcessProxy::updateRemoteWorkerPreferencesStore(const WebPreferencesStore& store)
{
    if (m_serviceWorkerInformation)
        send(Messages::WebSWContextManagerConnection::UpdatePreferencesStore { store }, 0);
    if (m_sharedWorkerInformation)
        send(Messages::WebSharedWorkerContextManagerConnection::UpdatePreferencesStore { store }, 0);
}

void WebProcessProxy::updateServiceWorkerProcessAssertion()
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateServiceWorkerProcessAssertion");

    if (!m_serviceWorkerInformation->hasBackgroundProcessing) {
        m_serviceWorkerInformation->activity = nullptr;
        return;
    }

    bool shouldTakeForegroundActivity = WTF::anyOf(m_serviceWorkerInformation->clientProcesses, [&](auto& process) {
        return &process != this && !!process.m_foregroundToken;
    });
    if (shouldTakeForegroundActivity) {
        if (!ProcessThrottler::isValidForegroundActivity(m_serviceWorkerInformation->activity.get()))
            m_serviceWorkerInformation->activity = protectedThrottler()->foregroundActivity("Service Worker for foreground view(s)"_s);
        return;
    }

    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "Service Worker for background processing");
    if (!ProcessThrottler::isValidBackgroundActivity(m_serviceWorkerInformation->activity.get()))
        m_serviceWorkerInformation->activity = protectedThrottler()->backgroundActivity("Service Worker for background processing"_s);
}

void WebProcessProxy::updateSharedWorkerProcessAssertion()
{
    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "updateSharedWorkerProcessAssertion");

    bool shouldTakeForegroundActivity = WTF::anyOf(m_sharedWorkerInformation->clientProcesses, [&](auto& process) {
        return &process != this && !!process.m_foregroundToken;
    });
    if (shouldTakeForegroundActivity) {
        if (!ProcessThrottler::isValidForegroundActivity(m_sharedWorkerInformation->activity.get()))
            m_sharedWorkerInformation->activity = protectedThrottler()->foregroundActivity("Shared Worker for foreground view(s)"_s);
        return;
    }

    bool shouldTakeBackgroundActivity = WTF::anyOf(m_sharedWorkerInformation->clientProcesses, [&](auto& process) {
        return &process != this && !!process.m_backgroundToken;
    });
    if (shouldTakeBackgroundActivity) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_sharedWorkerInformation->activity.get()))
            m_sharedWorkerInformation->activity = protectedThrottler()->backgroundActivity("Shared Worker for background view(s)"_s);
        return;
    }

    m_sharedWorkerInformation->activity = nullptr;
}

void WebProcessProxy::updateRemoteWorkerProcessAssertion(RemoteWorkerType workerType)
{
    ASSERT(workerType == RemoteWorkerType::SharedWorker ? !!m_sharedWorkerInformation : !!m_serviceWorkerInformation);
    switch (workerType) {
    case RemoteWorkerType::ServiceWorker:
        updateServiceWorkerProcessAssertion();
        break;
    case RemoteWorkerType::SharedWorker:
        updateSharedWorkerProcessAssertion();
        break;
    }
}

void WebProcessProxy::registerRemoteWorkerClientProcess(RemoteWorkerType workerType, WebProcessProxy& proxy)
{
    auto& workerInformation = workerType == RemoteWorkerType::SharedWorker ? m_sharedWorkerInformation : m_serviceWorkerInformation;
    if (!workerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(Worker, "registerWorkerClientProcess: workerType=%" PUBLIC_LOG_STRING ", clientProcess=%p, clientPID=%d", workerType == RemoteWorkerType::SharedWorker ? "shared" : "service", &proxy, proxy.processID());
    workerInformation->clientProcesses.add(proxy);
    updateRemoteWorkerProcessAssertion(workerType);
}

void WebProcessProxy::unregisterRemoteWorkerClientProcess(RemoteWorkerType workerType, WebProcessProxy& proxy)
{
    auto& workerInformation = workerType == RemoteWorkerType::SharedWorker ? m_sharedWorkerInformation : m_serviceWorkerInformation;
    if (!workerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(Worker, "unregisterWorkerClientProcess: workerType=%" PUBLIC_LOG_STRING ", clientProcess=%p, clientPID=%d", workerType == RemoteWorkerType::SharedWorker ? "shared" : "service", &proxy, proxy.processID());
    workerInformation->clientProcesses.remove(proxy);
    updateRemoteWorkerProcessAssertion(workerType);
}

bool WebProcessProxy::hasServiceWorkerForegroundActivityForTesting() const
{
    return m_serviceWorkerInformation && ProcessThrottler::isValidForegroundActivity(m_serviceWorkerInformation->activity.get());
}

bool WebProcessProxy::hasServiceWorkerBackgroundActivityForTesting() const
{
    return m_serviceWorkerInformation && ProcessThrottler::isValidBackgroundActivity(m_serviceWorkerInformation->activity.get());
}

void WebProcessProxy::startServiceWorkerBackgroundProcessing()
{
    if (!m_serviceWorkerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "startServiceWorkerBackgroundProcessing");
    m_serviceWorkerInformation->hasBackgroundProcessing = true;
    updateRemoteWorkerProcessAssertion(RemoteWorkerType::ServiceWorker);
}

void WebProcessProxy::endServiceWorkerBackgroundProcessing()
{
    if (!m_serviceWorkerInformation)
        return;

    WEBPROCESSPROXY_RELEASE_LOG(ProcessSuspension, "endServiceWorkerBackgroundProcessing");
    m_serviceWorkerInformation->hasBackgroundProcessing = false;
    updateRemoteWorkerProcessAssertion(RemoteWorkerType::ServiceWorker);
}

void WebProcessProxy::disableRemoteWorkers(OptionSet<RemoteWorkerType> workerTypes)
{
    bool didDisableWorkers = false;

    if (workerTypes.contains(RemoteWorkerType::SharedWorker) && m_sharedWorkerInformation) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "disableWorkers: Disabling shared workers");
        m_sharedWorkerInformation = { };
        didDisableWorkers = true;
    }

    if (workerTypes.contains(RemoteWorkerType::ServiceWorker) && m_serviceWorkerInformation) {
        WEBPROCESSPROXY_RELEASE_LOG(Process, "disableWorkers: Disabling service workers");
        removeMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_serviceWorkerInformation->remoteWorkerPageID);
        m_serviceWorkerInformation = { };
        didDisableWorkers = true;
    }

    if (!didDisableWorkers)
        return;

    updateBackgroundResponsivenessTimer();

    if (!isRunningWorkers())
        protectedProcessPool()->removeRemoteWorkerProcess(*this);

    if (workerTypes.contains(RemoteWorkerType::SharedWorker))
        send(Messages::WebSharedWorkerContextManagerConnection::Close { }, 0);

    if (workerTypes.contains(RemoteWorkerType::ServiceWorker))
        send(Messages::WebSWContextManagerConnection::Close { }, 0);

    maybeShutDown();
}

#if ENABLE(CONTENT_EXTENSIONS)
static Vector<std::pair<WebCompiledContentRuleListData, URL>> contentRuleListsFromIdentifier(const std::optional<UserContentControllerIdentifier>& userContentControllerIdentifier)
{
    if (!userContentControllerIdentifier) {
        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr userContentController = WebUserContentControllerProxy::get(*userContentControllerIdentifier);
    if (!userContentController)
        return { };

    return userContentController->contentRuleListData();
}
#endif

void WebProcessProxy::enableRemoteWorkers(RemoteWorkerType workerType, const UserContentControllerIdentifier& userContentControllerIdentifier)
{
    WEBPROCESSPROXY_RELEASE_LOG(ServiceWorker, "enableWorkers: workerType=%u", static_cast<unsigned>(workerType));
    auto& workerInformation = workerType == RemoteWorkerType::SharedWorker ? m_sharedWorkerInformation : m_serviceWorkerInformation;
    ASSERT(!workerInformation);

    workerInformation = RemoteWorkerInformation {
        WebPageProxyIdentifier::generate(),
        PageIdentifier::generate(),
        RemoteWorkerInitializationData {
            userContentControllerIdentifier,
#if ENABLE(CONTENT_EXTENSIONS)
            contentRuleListsFromIdentifier(userContentControllerIdentifier),
#endif
        },
        nullptr,
        { }
    };

    protectedProcessPool()->addRemoteWorkerProcess(*this);

    if (workerType == RemoteWorkerType::ServiceWorker)
        addMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_serviceWorkerInformation->remoteWorkerPageID, ServiceWorkerNotificationHandler::singleton());

    updateBackgroundResponsivenessTimer();

    updateRemoteWorkerProcessAssertion(workerType);
}

void WebProcessProxy::markProcessAsRecentlyUsed()
{
    liveProcessesLRU().moveToLastIfPresent(*this);
}

void WebProcessProxy::systemBeep()
{
    PAL::systemBeep();
}

RefPtr<WebsiteDataStore> WebProcessProxy::protectedWebsiteDataStore() const
{
    return m_websiteDataStore;
}

void WebProcessProxy::getNotifications(const URL& registrationURL, const String& tag, CompletionHandler<void(Vector<NotificationData>&&)>&& callback)
{
    if (RefPtr websiteDataStore = m_websiteDataStore; websiteDataStore->hasClientGetDisplayedNotifications()) {
        auto callbackHandlingTags = [tag, callback = WTFMove(callback)] (Vector<NotificationData>&& notifications) mutable {
            if (tag.isEmpty()) {
                callback(WTFMove(notifications));
                return;
            }

            Vector<NotificationData> filteredNotifications;
            for (auto& notification : notifications) {
                if (tag == notification.tag)
                    filteredNotifications.append(notification);
            }

            callback(WTFMove(filteredNotifications));
        };
        websiteDataStore->getNotifications(registrationURL, WTFMove(callbackHandlingTags));
        return;
    }

    WebNotificationManagerProxy::protectedSharedServiceWorkerManager()->getNotifications(registrationURL, tag, sessionID(), WTFMove(callback));
}

void WebProcessProxy::getWebCryptoMasterKey(CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    m_websiteDataStore->client().webCryptoMasterKey([completionHandler = WTFMove(completionHandler)](std::optional<Vector<uint8_t>>&& key) mutable {
        if (key)
            return completionHandler(WTFMove(key));
        return completionHandler(WebCore::defaultWebCryptoMasterKey());
    });
}

void WebProcessProxy::wrapCryptoKey(Vector<uint8_t>&& key, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    getWebCryptoMasterKey([key = WTFMove(key), completionHandler = WTFMove(completionHandler)](std::optional<Vector<uint8_t>> && masterKey) mutable {
#if PLATFORM(COCOA)
        if (!masterKey)
            return completionHandler(std::nullopt);
#endif
        Vector<uint8_t> wrappedKey;
        const Vector<uint8_t> blankMasterKey;
        if (wrapSerializedCryptoKey(masterKey.value_or(blankMasterKey), key, wrappedKey))
            return completionHandler(WTFMove(wrappedKey));
        completionHandler(std::nullopt);
    });
}

void WebProcessProxy::unwrapCryptoKey(WrappedCryptoKey&& wrappedKey, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    getWebCryptoMasterKey([wrappedKey = WTFMove(wrappedKey), completionHandler = WTFMove(completionHandler)](std::optional<Vector<uint8_t>> && masterKey) mutable {
#if PLATFORM(COCOA)
        if (!masterKey)
            return completionHandler(std::nullopt);
#endif
        const Vector<uint8_t> blankMasterKey;
        if (auto key = WebCore::unwrapCryptoKey(masterKey.value_or(blankMasterKey), wrappedKey))
            return completionHandler(WTFMove(key));
        completionHandler(std::nullopt);
    });

}
void WebProcessProxy::setAppBadge(std::optional<WebPageProxyIdentifier> pageIdentifier, const SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    if (!pageIdentifier) {
        protectedWebsiteDataStore()->workerUpdatedAppBadge(origin, badge);
        return;
    }

    // This page might have gone away since the WebContent process sent this message,
    // and that's just fine.
    if (RefPtr page = m_pageMap.get(*pageIdentifier))
        page->uiClient().updateAppBadge(*page, origin, badge);
}

void WebProcessProxy::setClientBadge(WebPageProxyIdentifier pageIdentifier, const SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    // This page might have gone away since the WebContent process sent this message,
    // and that's just fine.
    if (RefPtr page = m_pageMap.get(pageIdentifier))
        page->uiClient().updateClientBadge(*page, origin, badge);
}

const WeakHashSet<WebProcessProxy>* WebProcessProxy::serviceWorkerClientProcesses() const
{
    if (m_serviceWorkerInformation)
        return &m_serviceWorkerInformation.value().clientProcesses;
    return nullptr;
}

const WeakHashSet<WebProcessProxy>* WebProcessProxy::sharedWorkerClientProcesses() const
{
    if (m_sharedWorkerInformation)
        return &m_sharedWorkerInformation.value().clientProcesses;
    return nullptr;
}

void WebProcessProxy::permissionChanged(WebCore::PermissionName permissionName, const WebCore::SecurityOriginData& topOrigin)
{
    auto webProcessPools = WebKit::WebProcessPool::allProcessPools();

    for (auto& webProcessPool : webProcessPools) {
        for (Ref webProcessProxy : webProcessPool->processes())
            webProcessProxy->processPermissionChanged(permissionName, topOrigin);
    }
}

void WebProcessProxy::processPermissionChanged(WebCore::PermissionName permissionName, const WebCore::SecurityOriginData& topOrigin)
{
#if ENABLE(MEDIA_STREAM)
    if (permissionName == WebCore::PermissionName::Camera || permissionName == WebCore::PermissionName::Microphone) {
        for (auto& page : m_pageMap.values()) {
            if (SecurityOriginData::fromURLWithoutStrictOpaqueness(URL { page->currentURL() }) == topOrigin)
                page->clearUserMediaPermissionRequestHistory(permissionName);
        }
    }
#endif
    send(Messages::WebPermissionController::permissionChanged(permissionName, topOrigin), 0);
}

Logger& WebProcessProxy::logger()
{
    if (!m_logger) {
        Ref logger = Logger::create(this);
        m_logger = logger.copyRef();
        logger->setEnabled(this, isAlwaysOnLoggingAllowed());
    }
    return *m_logger;
}

void WebProcessProxy::resetState()
{
    m_hasCommittedAnyProvisionalLoads = false;
    m_hasCommittedAnyMeaningfulProvisionalLoads = false;
}

Seconds WebProcessProxy::totalForegroundTime() const
{
    if (m_throttleStateForStatistics == ProcessThrottleState::Foreground && m_throttleStateForStatisticsTimestamp)
        return m_totalForegroundTime + (MonotonicTime::now() - m_throttleStateForStatisticsTimestamp);
    return m_totalForegroundTime;
}

Seconds WebProcessProxy::totalBackgroundTime() const
{
    if (m_throttleStateForStatistics == ProcessThrottleState::Background && m_throttleStateForStatisticsTimestamp)
        return m_totalBackgroundTime + (MonotonicTime::now() - m_throttleStateForStatisticsTimestamp);
    return m_totalBackgroundTime;
}

Seconds WebProcessProxy::totalSuspendedTime() const
{
    if (m_throttleStateForStatistics == ProcessThrottleState::Suspended && m_throttleStateForStatisticsTimestamp)
        return m_totalSuspendedTime + (MonotonicTime::now() - m_throttleStateForStatisticsTimestamp);
    return m_totalSuspendedTime;
}

void WebProcessProxy::updateRuntimeStatistics()
{
    auto newState = ProcessThrottleState::Suspended;
    auto newTimestamp = MonotonicTime { };

    // We only start a new interval for foreground/background/suspended time if the process isn't
    // prewarmed or in the process cache.
    if (!isPrewarmed() && !isInProcessCache()) {
        // ProcessThrottleState can be misleading, as it can claim the process is suspended even
        // when the process is holding an assertion that actually prevents suspension. So we only
        // transition to the suspended state if the process is actually holding no assertions
        // (when `ProcessThrottler::isSuspended()` returns true).
        newState = throttler().currentState();
        if (newState == ProcessThrottleState::Suspended && !throttler().isSuspended())
            newState = ProcessThrottleState::Background;

        newTimestamp = MonotonicTime::now();
    }

    if (m_throttleStateForStatisticsTimestamp) {
        auto delta = MonotonicTime::now() - m_throttleStateForStatisticsTimestamp;
        switch (m_throttleStateForStatistics) {
        case ProcessThrottleState::Suspended:
            m_totalSuspendedTime += delta;
            break;
        case ProcessThrottleState::Background:
            m_totalBackgroundTime += delta;
            break;
        case ProcessThrottleState::Foreground:
            m_totalForegroundTime += delta;
            break;
        }
    }

    m_throttleStateForStatistics = newState;
    m_throttleStateForStatisticsTimestamp = newTimestamp;

    if (RefPtr pool = m_processPool.get()) {
        if (pool->webProcessStateUpdatesForPageClientEnabled()) {
            for (Ref page : mainPages())
                page->processDidUpdateThrottleState();
        }
    }
}

bool WebProcessProxy::isAlwaysOnLoggingAllowed() const
{
    return WTF::allOf(pages(), [](auto& page) {
        return page->isAlwaysOnLoggingAllowed();
    });
}

TextStream& operator<<(TextStream& ts, const WebProcessProxy& process)
{
    auto appendCount = [&ts](unsigned value, ASCIILiteral description) {
        if (value)
            ts << ", " << description << ": " << value;
    };
    auto appendIf = [&ts](bool value, ASCIILiteral description) {
        if (value)
            ts << ", " << description;
    };

    ts << "pid: " << process.processID();
    appendCount(process.pageCount(), "pages"_s);
    appendCount(process.visiblePageCount(), "visible-pages"_s);
    appendCount(process.provisionalPageCount(), "provisional-pages"_s);
    appendCount(process.suspendedPageCount(), "suspended-pages"_s);
    appendIf(process.isPrewarmed(), "prewarmed"_s);
    appendIf(process.isInProcessCache(), "in-process-cache"_s);
    appendIf(process.isRunningServiceWorkers(), "has-service-worker"_s);
    appendIf(process.isRunningSharedWorkers(), "has-shared-worker"_s);
    appendIf(process.memoryPressureStatus() == SystemMemoryPressureStatus::Warning, "warning-memory-pressure"_s);
    appendIf(process.memoryPressureStatus() == SystemMemoryPressureStatus::Critical, "critical-memory-pressure"_s);
    ts << ", " << process.protectedThrottler().get();

#if PLATFORM(COCOA)
    auto description = [](ProcessThrottleState state) -> ASCIILiteral {
        switch (state) {
        case ProcessThrottleState::Foreground: return "foreground"_s;
        case ProcessThrottleState::Background: return "background"_s;
        case ProcessThrottleState::Suspended: return "suspended"_s;
        }
        return "unknown"_s;
    };

    if (auto taskInfo = process.taskInfo()) {
        ts << ", state: " << description(taskInfo->state);
        ts << ", phys_footprint_mb: " << (taskInfo->physicalFootprint / MB) << " MB";
    }
#endif

    return ts;
}

#if ENABLE(WEBXR)
const WebCore::ProcessIdentity& WebProcessProxy::processIdentity()
{
    return m_processIdentity;
}
#endif


} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK_COMPLETION
#undef WEBPROCESSPROXY_RELEASE_LOG
#undef WEBPROCESSPROXY_RELEASE_LOG_ERROR
