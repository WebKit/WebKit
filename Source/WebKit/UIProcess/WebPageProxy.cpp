/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "WebPageProxy.h"

#include "APIArray.h"
#include "APIAttachment.h"
#include "APIContextMenuClient.h"
#include "APIFindClient.h"
#include "APIFindMatchesClient.h"
#include "APIFormClient.h"
#include "APIFrameInfo.h"
#include "APIFullscreenClient.h"
#include "APIGeometry.h"
#include "APIHistoryClient.h"
#include "APIHitTestResult.h"
#include "APIIconLoadingClient.h"
#include "APILegacyContextHistoryClient.h"
#include "APILoaderClient.h"
#include "APINavigation.h"
#include "APINavigationAction.h"
#include "APINavigationClient.h"
#include "APINavigationResponse.h"
#include "APIOpenPanelParameters.h"
#include "APIPageConfiguration.h"
#include "APIPolicyClient.h"
#include "APISecurityOrigin.h"
#include "APIUIClient.h"
#include "APIURLRequest.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "DataReference.h"
#include "DownloadProxy.h"
#include "DrawingAreaProxy.h"
#include "DrawingAreaProxyMessages.h"
#include "EventDispatcherMessages.h"
#include "FrameInfoData.h"
#include "LoadParameters.h"
#include "Logging.h"
#include "NativeWebGestureEvent.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "NavigationActionData.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "NotificationPermissionRequest.h"
#include "NotificationPermissionRequestManager.h"
#include "OptionalCallbackID.h"
#include "PageClient.h"
#include "PluginInformation.h"
#include "PluginProcessManager.h"
#include "PrintInfo.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "UIMessagePortChannelProvider.h"
#include "URLSchemeTaskParameters.h"
#include "UserMediaPermissionRequestProxy.h"
#include "UserMediaProcessManager.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCertificateInfo.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebEditCommandProxy.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFullScreenManagerProxy.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebImage.h"
#include "WebInspectorProxy.h"
#include "WebInspectorUtilities.h"
#include "WebNavigationState.h"
#include "WebNotificationManagerProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroup.h"
#include "WebPageGroupData.h"
#include "WebPageMessages.h"
#include "WebPageProxyMessages.h"
#include "WebPaymentCoordinatorProxy.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include "WebPreferences.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include "WebURLSchemeHandler.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/AttachmentTypes.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/EventNames.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/JSDOMBinding.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/LengthBox.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaStreamRequest.h>
#include <WebCore/PerformanceLoggingClient.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/URL.h>
#include <WebCore/URLParser.h>
#include <WebCore/ValidationBubble.h>
#include <WebCore/WindowFeatures.h>
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringView.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "APIApplicationManifest.h"
#endif

#if ENABLE(ASYNC_SCROLLING)
#include "RemoteScrollingCoordinatorProxy.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteLayerTreeScrollingPerformanceData.h"
#include "TouchBarMenuData.h"
#include "TouchBarMenuItemData.h"
#include "VideoFullscreenManagerProxy.h"
#include "VideoFullscreenManagerProxyMessages.h"
#include "ViewSnapshotStore.h"
#include <WebCore/MachSendRight.h>
#include <WebCore/RunLoopObserver.h>
#include <WebCore/TextIndicatorWindow.h>
#endif

#if PLATFORM(GTK)
#include "WebSelectionData.h"
#endif

#if USE(CAIRO)
#include <WebCore/CairoUtilities.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
#include <WebCore/MediaPlaybackTarget.h>
#include <WebCore/WebMediaSessionManager.h>
#endif

#if ENABLE(MEDIA_SESSION)
#include "WebMediaSessionFocusManager.h"
#include "WebMediaSessionMetadata.h"
#include <WebCore/MediaSessionMetadata.h>
#endif

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManagerProxy.h"
#endif

// This controls what strategy we use for mouse wheel coalescing.
#define MERGE_WHEEL_EVENTS 1

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_process->connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(m_process->checkURLReceivedFromWebProcess(url), m_process->connection())

#define RELEASE_LOG_IF_ALLOWED(channel, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, __VA_ARGS__)

using namespace WebCore;

// Represents the number of wheel events we can hold in the queue before we start pushing them preemptively.
static const unsigned wheelEventQueueSizeThreshold = 10;

namespace WebKit {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageProxyCounter, ("WebPageProxy"));

class ExceededDatabaseQuotaRecords {
    WTF_MAKE_NONCOPYABLE(ExceededDatabaseQuotaRecords); WTF_MAKE_FAST_ALLOCATED;
    friend NeverDestroyed<ExceededDatabaseQuotaRecords>;
public:
    struct Record {
        uint64_t frameID;
        String originIdentifier;
        String databaseName;
        String displayName;
        uint64_t currentQuota;
        uint64_t currentOriginUsage;
        uint64_t currentDatabaseUsage;
        uint64_t expectedUsage;
        RefPtr<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply> reply;
    };

    static ExceededDatabaseQuotaRecords& singleton();

    std::unique_ptr<Record> createRecord(uint64_t frameID, String originIdentifier,
        String databaseName, String displayName, uint64_t currentQuota,
        uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, 
        Ref<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply>&&);

    void add(std::unique_ptr<Record>);
    bool areBeingProcessed() const { return !!m_currentRecord; }
    Record* next();

private:
    ExceededDatabaseQuotaRecords() { }
    ~ExceededDatabaseQuotaRecords() { }

    Deque<std::unique_ptr<Record>> m_records;
    std::unique_ptr<Record> m_currentRecord;
};

ExceededDatabaseQuotaRecords& ExceededDatabaseQuotaRecords::singleton()
{
    static NeverDestroyed<ExceededDatabaseQuotaRecords> records;
    return records;
}

std::unique_ptr<ExceededDatabaseQuotaRecords::Record> ExceededDatabaseQuotaRecords::createRecord(
    uint64_t frameID, String originIdentifier, String databaseName, String displayName,
    uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage,
    uint64_t expectedUsage, Ref<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply>&& reply)
{
    auto record = std::make_unique<Record>();
    record->frameID = frameID;
    record->originIdentifier = originIdentifier;
    record->databaseName = databaseName;
    record->displayName = displayName;
    record->currentQuota = currentQuota;
    record->currentOriginUsage = currentOriginUsage;
    record->currentDatabaseUsage = currentDatabaseUsage;
    record->expectedUsage = expectedUsage;
    record->reply = WTFMove(reply);
    return record;
}

void ExceededDatabaseQuotaRecords::add(std::unique_ptr<ExceededDatabaseQuotaRecords::Record> record)
{
    m_records.append(WTFMove(record));
}

ExceededDatabaseQuotaRecords::Record* ExceededDatabaseQuotaRecords::next()
{
    m_currentRecord = nullptr;
    if (!m_records.isEmpty())
        m_currentRecord = m_records.takeFirst();
    return m_currentRecord.get();
}

#if !LOG_DISABLED
static const char* webKeyboardEventTypeString(WebEvent::Type type)
{
    switch (type) {
    case WebEvent::KeyDown:
        return "KeyDown";
    
    case WebEvent::KeyUp:
        return "KeyUp";
    
    case WebEvent::RawKeyDown:
        return "RawKeyDown";
    
    case WebEvent::Char:
        return "Char";
    
    default:
        ASSERT_NOT_REACHED();
        return "<unknown>";
    }
}
#endif // !LOG_DISABLED

class PageClientProtector {
    WTF_MAKE_NONCOPYABLE(PageClientProtector);
public:
    PageClientProtector(PageClient& pageClient)
        : m_pageClient(pageClient)
    {
        m_pageClient.refView();
    }

    ~PageClientProtector()
    {
        m_pageClient.derefView();
    }

private:
    PageClient& m_pageClient;
};

Ref<WebPageProxy> WebPageProxy::create(PageClient& pageClient, WebProcessProxy& process, uint64_t pageID, Ref<API::PageConfiguration>&& configuration)
{
    return adoptRef(*new WebPageProxy(pageClient, process, pageID, WTFMove(configuration)));
}

WebPageProxy::WebPageProxy(PageClient& pageClient, WebProcessProxy& process, uint64_t pageID, Ref<API::PageConfiguration>&& configuration)
    : m_pageClient(pageClient)
    , m_configuration(WTFMove(configuration))
    , m_loaderClient(std::make_unique<API::LoaderClient>())
    , m_policyClient(std::make_unique<API::PolicyClient>())
    , m_iconLoadingClient(std::make_unique<API::IconLoadingClient>())
    , m_formClient(std::make_unique<API::FormClient>())
    , m_uiClient(std::make_unique<API::UIClient>())
    , m_findClient(std::make_unique<API::FindClient>())
    , m_findMatchesClient(std::make_unique<API::FindMatchesClient>())
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuClient(std::make_unique<API::ContextMenuClient>())
#endif
    , m_navigationState(std::make_unique<WebNavigationState>())
    , m_process(process)
    , m_pageGroup(*m_configuration->pageGroup())
    , m_preferences(*m_configuration->preferences())
    , m_userContentController(*m_configuration->userContentController())
    , m_visitedLinkStore(*m_configuration->visitedLinkStore())
    , m_websiteDataStore(m_configuration->websiteDataStore()->websiteDataStore())
    , m_userAgent(standardUserAgent())
    , m_overrideContentSecurityPolicy { m_configuration->overrideContentSecurityPolicy() }
    , m_treatsSHA1CertificatesAsInsecure(m_configuration->treatsSHA1SignedCertificatesAsInsecure())
#if ENABLE(FULLSCREEN_API)
    , m_fullscreenClient(std::make_unique<API::FullscreenClient>())
#endif
    , m_geolocationPermissionRequestManager(*this)
    , m_notificationPermissionRequestManager(*this)
#if PLATFORM(IOS)
    , m_alwaysRunsAtForegroundPriority(m_configuration->alwaysRunsAtForegroundPriority())
#endif
    , m_initialCapitalizationEnabled(m_configuration->initialCapitalizationEnabled())
    , m_cpuLimit(m_configuration->cpuLimit())
    , m_backForwardList(WebBackForwardList::create(*this))
    , m_waitsForPaintAfterViewDidMoveToWindow(m_configuration->waitsForPaintAfterViewDidMoveToWindow())
    , m_pageID(pageID)
    , m_controlledByAutomation(m_configuration->isControlledByAutomation())
#if PLATFORM(COCOA)
    , m_isSmartInsertDeleteEnabled(TextChecker::isSmartInsertDeleteEnabled())
#endif
    , m_pageLoadState(*this)
    , m_configurationPreferenceValues(m_configuration->preferenceValues())
{
    m_webProcessLifetimeTracker.addObserver(m_visitedLinkStore);
    m_webProcessLifetimeTracker.addObserver(m_websiteDataStore);

    updateActivityState();
    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();
    
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    m_layerHostingMode = m_activityState & ActivityState::IsInWindow ? m_pageClient.viewLayerHostingMode() : LayerHostingMode::OutOfProcess;
#endif

    platformInitialize();

#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif

    WebProcessPool::statistics().wkPageCount++;

    m_preferences->addPage(*this);
    m_pageGroup->addPage(this);

    m_inspector = WebInspectorProxy::create(this);
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = WebFullScreenManagerProxy::create(*this, m_pageClient.fullScreenManagerProxyClient());
#endif
#if PLATFORM(IOS) && HAVE(AVKIT) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    m_playbackSessionManager = PlaybackSessionManagerProxy::create(*this);
    m_videoFullscreenManager = VideoFullscreenManagerProxy::create(*this, *m_playbackSessionManager);
#endif

#if ENABLE(APPLE_PAY)
    m_paymentCoordinator = std::make_unique<WebPaymentCoordinatorProxy>(*this);
#endif

    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID, *this);

#if PLATFORM(COCOA)
    m_activityStateChangeDispatcher = std::make_unique<RunLoopObserver>(static_cast<CFIndex>(RunLoopObserver::WellKnownRunLoopOrders::ActivityStateChange), [this] {
        this->dispatchActivityStateChange();
    });
#endif
}

WebPageProxy::~WebPageProxy()
{
    ASSERT(m_process->webPage(m_pageID) != this);
#if !ASSERT_DISABLED
    for (WebPageProxy* page : m_process->pages())
        ASSERT(page != this);
#endif

    if (!m_isClosed)
        close();

    WebProcessPool::statistics().wkPageCount--;

    if (m_hasSpellDocumentTag)
        TextChecker::closeSpellDocumentWithTag(m_spellDocumentTag);

    m_preferences->removePage(*this);
    m_pageGroup->removePage(this);

#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif
}

PAL::SessionID WebPageProxy::sessionID() const
{
    return m_websiteDataStore->sessionID();
}

void WebPageProxy::changeWebsiteDataStore(WebsiteDataStore& websiteDataStore)
{
    m_process->processPool().pageEndUsingWebsiteDataStore(*this);
    m_websiteDataStore = websiteDataStore;
    m_process->processPool().pageBeginUsingWebsiteDataStore(*this);
}

const API::PageConfiguration& WebPageProxy::configuration() const
{
    return m_configuration.get();
}

ProcessID WebPageProxy::processIdentifier() const
{
    if (m_isClosed)
        return 0;

    return m_process->processIdentifier();
}

bool WebPageProxy::isValid() const
{
    // A page that has been explicitly closed is never valid.
    if (m_isClosed)
        return false;

    return m_isValid;
}

void WebPageProxy::setPreferences(WebPreferences& preferences)
{
    if (&preferences == m_preferences.ptr())
        return;

    m_preferences->removePage(*this);
    m_preferences = preferences;
    m_preferences->addPage(*this);

    preferencesDidChange();
}
    
void WebPageProxy::setHistoryClient(std::unique_ptr<API::HistoryClient>&& historyClient)
{
    m_historyClient = WTFMove(historyClient);
}

void WebPageProxy::setNavigationClient(std::unique_ptr<API::NavigationClient>&& navigationClient)
{
    m_navigationClient = WTFMove(navigationClient);
}

void WebPageProxy::setLoaderClient(std::unique_ptr<API::LoaderClient>&& loaderClient)
{
    if (!loaderClient) {
        m_loaderClient = std::make_unique<API::LoaderClient>();
        return;
    }

    m_loaderClient = WTFMove(loaderClient);
}

void WebPageProxy::setPolicyClient(std::unique_ptr<API::PolicyClient>&& policyClient)
{
    if (!policyClient) {
        m_policyClient = std::make_unique<API::PolicyClient>();
        return;
    }

    m_policyClient = WTFMove(policyClient);
}

void WebPageProxy::setFormClient(std::unique_ptr<API::FormClient>&& formClient)
{
    if (!formClient) {
        m_formClient = std::make_unique<API::FormClient>();
        return;
    }

    m_formClient = WTFMove(formClient);
}

void WebPageProxy::setUIClient(std::unique_ptr<API::UIClient>&& uiClient)
{
    if (!uiClient) {
        m_uiClient = std::make_unique<API::UIClient>();
        return;
    }

    m_uiClient = WTFMove(uiClient);

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetCanRunBeforeUnloadConfirmPanel(m_uiClient->canRunBeforeUnloadConfirmPanel()), m_pageID);
    setCanRunModal(m_uiClient->canRunModal());
}

void WebPageProxy::setIconLoadingClient(std::unique_ptr<API::IconLoadingClient>&& iconLoadingClient)
{
    bool hasClient = iconLoadingClient.get();
    if (!iconLoadingClient)
        m_iconLoadingClient = std::make_unique<API::IconLoadingClient>();
    else
        m_iconLoadingClient = WTFMove(iconLoadingClient);

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetUseIconLoadingClient(hasClient), m_pageID);
}

void WebPageProxy::setFindClient(std::unique_ptr<API::FindClient>&& findClient)
{
    if (!findClient) {
        m_findClient = std::make_unique<API::FindClient>();
        return;
    }
    
    m_findClient = WTFMove(findClient);
}

void WebPageProxy::setFindMatchesClient(std::unique_ptr<API::FindMatchesClient>&& findMatchesClient)
{
    if (!findMatchesClient) {
        m_findMatchesClient = std::make_unique<API::FindMatchesClient>();
        return;
    }

    m_findMatchesClient = WTFMove(findMatchesClient);
}

void WebPageProxy::setDiagnosticLoggingClient(std::unique_ptr<API::DiagnosticLoggingClient>&& diagnosticLoggingClient)
{
    m_diagnosticLoggingClient = WTFMove(diagnosticLoggingClient);
}

#if ENABLE(CONTEXT_MENUS)
void WebPageProxy::setContextMenuClient(std::unique_ptr<API::ContextMenuClient>&& contextMenuClient)
{
    if (!contextMenuClient) {
        m_contextMenuClient = std::make_unique<API::ContextMenuClient>();
        return;
    }

    m_contextMenuClient = WTFMove(contextMenuClient);
}
#endif

void WebPageProxy::setInjectedBundleClient(const WKPageInjectedBundleClientBase* client)
{
    if (!client) {
        m_injectedBundleClient = nullptr;
        return;
    }

    m_injectedBundleClient = std::make_unique<WebPageInjectedBundleClient>();
    m_injectedBundleClient->initialize(client);
}

void WebPageProxy::handleMessage(IPC::Connection& connection, const String& messageName, const WebKit::UserData& messageBody)
{
    ASSERT(m_process->connection() == &connection);

    if (!m_injectedBundleClient)
        return;

    m_injectedBundleClient->didReceiveMessageFromInjectedBundle(this, messageName, m_process->transformHandlesToObjects(messageBody.object()).get());
}

void WebPageProxy::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, UserData& returnUserData)
{
    ASSERT(m_process->connection() == &connection);

    if (!m_injectedBundleClient)
        return;

    RefPtr<API::Object> returnData;
    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(this, messageName, m_process->transformHandlesToObjects(messageBody.object()).get(), returnData);
    returnUserData = UserData(m_process->transformObjectsToHandles(returnData.get()));
}

void WebPageProxy::reattachToWebProcess()
{
    ASSERT(!m_isClosed);
    ASSERT(!isValid());
    ASSERT(m_process->state() == WebProcessProxy::State::Terminated);

    m_isValid = true;
    m_process->removeWebPage(*this, m_pageID);
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID);

    m_process = m_process->processPool().createNewWebProcessRespectingProcessCountLimit(m_websiteDataStore.get());

    ASSERT(m_process->state() != ChildProcessProxy::State::Terminated);
    if (m_process->state() == ChildProcessProxy::State::Running)
        processDidFinishLaunching();
    m_process->addExistingWebPage(*this, m_pageID);
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID, *this);

    updateActivityState();
    updateThrottleState();

    m_inspector = WebInspectorProxy::create(this);
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = WebFullScreenManagerProxy::create(*this, m_pageClient.fullScreenManagerProxyClient());
#endif
#if PLATFORM(IOS) && HAVE(AVKIT) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    m_playbackSessionManager = PlaybackSessionManagerProxy::create(*this);
    m_videoFullscreenManager = VideoFullscreenManagerProxy::create(*this, *m_playbackSessionManager);
#endif

#if ENABLE(APPLE_PAY)
    m_paymentCoordinator = std::make_unique<WebPaymentCoordinatorProxy>(*this);
#endif

    initializeWebPage();

    m_pageClient.didRelaunchProcess();
    m_drawingArea->waitForBackingStoreUpdateOnNextPaint();
}

RefPtr<API::Navigation> WebPageProxy::reattachToWebProcessForReload()
{
    if (m_isClosed)
        return nullptr;
    
    ASSERT(!isValid());
    reattachToWebProcess();

    if (!m_backForwardList->currentItem())
        return nullptr;

    auto navigation = m_navigationState->createReloadNavigation();

    // We allow stale content when reloading a WebProcess that's been killed or crashed.
    m_process->send(Messages::WebPage::GoToBackForwardItem(navigation->navigationID(), m_backForwardList->currentItem()->itemID()), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

RefPtr<API::Navigation> WebPageProxy::reattachToWebProcessWithItem(WebBackForwardListItem* item)
{
    if (m_isClosed)
        return nullptr;

    ASSERT(!isValid());
    reattachToWebProcess();

    if (!item)
        return nullptr;

    if (item != m_backForwardList->currentItem())
        m_backForwardList->goToItem(*item);

    auto navigation = m_navigationState->createBackForwardNavigation();

    m_process->send(Messages::WebPage::GoToBackForwardItem(navigation->navigationID(), item->itemID()), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

void WebPageProxy::initializeWebPage()
{
    ASSERT(isValid());

    const BackForwardListItemVector& items = m_backForwardList->entries();
    for (size_t i = 0; i < items.size(); ++i)
        m_process->registerNewWebBackForwardListItem(items[i].get());

    m_drawingArea = m_pageClient.createDrawingAreaProxy();
    ASSERT(m_drawingArea);

#if ENABLE(ASYNC_SCROLLING)
    if (m_drawingArea->type() == DrawingAreaTypeRemoteLayerTree) {
        m_scrollingCoordinatorProxy = std::make_unique<RemoteScrollingCoordinatorProxy>(*this);
#if PLATFORM(IOS)
        // On iOS, main frame scrolls are sent in terms of visible rect updates.
        m_scrollingCoordinatorProxy->setPropagatesMainFrameScrolls(false);
#endif
    }
#endif

    process().send(Messages::WebProcess::CreateWebPage(m_pageID, creationParameters()), 0);

    m_needsToFinishInitializingWebPageAfterProcessLaunch = true;
    finishInitializingWebPageAfterProcessLaunch();
}

void WebPageProxy::finishInitializingWebPageAfterProcessLaunch()
{
    if (!m_needsToFinishInitializingWebPageAfterProcessLaunch)
        return;
    if (m_process->state() != WebProcessProxy::State::Running)
        return;

    m_needsToFinishInitializingWebPageAfterProcessLaunch = false;
    m_process->addVisitedLinkStore(m_visitedLinkStore);
}

void WebPageProxy::close()
{
    if (m_isClosed)
        return;

    m_isClosed = true;

    if (m_activePopupMenu)
        m_activePopupMenu->cancelTracking();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willClosePage(*this);
    }

#if ENABLE(CONTEXT_MENUS)
    m_activeContextMenu = nullptr;
#endif

    m_backForwardList->pageClosed();
    m_pageClient.pageClosed();

    m_process->disconnectFramesFromPage(this);

    resetState(ResetStateReason::PageInvalidated);

    m_loaderClient = std::make_unique<API::LoaderClient>();
    m_navigationClient = nullptr;
    m_policyClient = std::make_unique<API::PolicyClient>();
    m_iconLoadingClient = std::make_unique<API::IconLoadingClient>();
    m_formClient = std::make_unique<API::FormClient>();
    m_uiClient = std::make_unique<API::UIClient>();
    m_findClient = std::make_unique<API::FindClient>();
    m_findMatchesClient = std::make_unique<API::FindMatchesClient>();
    m_diagnosticLoggingClient = nullptr;
#if ENABLE(CONTEXT_MENUS)
    m_contextMenuClient = std::make_unique<API::ContextMenuClient>();
#endif
#if ENABLE(FULLSCREEN_API)
    m_fullscreenClient = std::make_unique<API::FullscreenClient>();
#endif

    m_webProcessLifetimeTracker.pageWasInvalidated();

    m_process->send(Messages::WebPage::Close(), m_pageID);
    m_process->removeWebPage(*this, m_pageID);
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID);
    m_process->processPool().supplement<WebNotificationManagerProxy>()->clearNotifications(this);

    // Null out related WebPageProxy to avoid leaks.
    m_configuration->setRelatedPage(nullptr);

#if PLATFORM(IOS)
    // Make sure we don't hold a process assertion after getting closed.
    m_activityToken = nullptr;
#endif

    stopAllURLSchemeTasks();
}

bool WebPageProxy::tryClose()
{
    if (!isValid())
        return true;

    // Close without delay if the process allows it. Our goal is to terminate
    // the process, so we check a per-process status bit.
    if (m_process->isSuddenTerminationEnabled())
        return true;

    m_process->send(Messages::WebPage::TryClose(), m_pageID);
    m_process->responsivenessTimer().start();
    return false;
}

bool WebPageProxy::maybeInitializeSandboxExtensionHandle(const URL& url, SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (!url.isLocalFile())
        return false;

    if (m_process->hasAssumedReadAccessToURL(url))
        return false;

    // Inspector resources are in a directory with assumed access.
    ASSERT_WITH_SECURITY_IMPLICATION(!WebKit::isInspectorPage(*this));

    SandboxExtension::createHandle("/", SandboxExtension::Type::ReadOnly, sandboxExtensionHandle);
    return true;
}

#if !PLATFORM(COCOA)
void WebPageProxy::addPlatformLoadParameters(LoadParameters&)
{
}
#endif

RefPtr<API::Navigation> WebPageProxy::loadRequest(ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, API::Object* userData)
{
    if (m_isClosed)
        return nullptr;

    auto navigation = m_navigationState->createLoadRequestNavigation(ResourceRequest(request));

    auto transaction = m_pageLoadState.transaction();

    auto url = request.url();
    m_pageLoadState.setPendingAPIRequestURL(transaction, url);

    if (!isValid())
        reattachToWebProcess();

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTFMove(request);
    loadParameters.shouldOpenExternalURLsPolicy = (uint64_t)shouldOpenExternalURLsPolicy;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    bool createdExtension = maybeInitializeSandboxExtensionHandle(url, loadParameters.sandboxExtensionHandle);
    if (createdExtension)
        m_process->willAcquireUniversalFileReadSandboxExtension();
    addPlatformLoadParameters(loadParameters);

    m_process->send(Messages::WebPage::LoadRequest(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

RefPtr<API::Navigation> WebPageProxy::loadFile(const String& fileURLString, const String& resourceDirectoryURLString, API::Object* userData)
{
    if (m_isClosed)
        return nullptr;

    if (!isValid())
        reattachToWebProcess();

    URL fileURL = URL(URL(), fileURLString);
    if (!fileURL.isLocalFile())
        return nullptr;

    URL resourceDirectoryURL;
    if (resourceDirectoryURLString.isNull())
        resourceDirectoryURL = URL(ParsedURLString, ASCIILiteral("file:///"));
    else {
        resourceDirectoryURL = URL(URL(), resourceDirectoryURLString);
        if (!resourceDirectoryURL.isLocalFile())
            return nullptr;
    }

    auto navigation = m_navigationState->createLoadRequestNavigation(ResourceRequest(fileURL));

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, fileURLString);

    String resourceDirectoryPath = resourceDirectoryURL.fileSystemPath();

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = fileURL;
    loadParameters.shouldOpenExternalURLsPolicy = (uint64_t)ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    SandboxExtension::createHandle(resourceDirectoryPath, SandboxExtension::Type::ReadOnly, loadParameters.sandboxExtensionHandle);
    addPlatformLoadParameters(loadParameters);

    m_process->assumeReadAccessToBaseURL(resourceDirectoryURL);
    m_process->send(Messages::WebPage::LoadRequest(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

RefPtr<API::Navigation> WebPageProxy::loadData(API::Data* data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData)
{
    if (m_isClosed)
        return nullptr;

    auto navigation = m_navigationState->createLoadDataNavigation();

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, !baseURL.isEmpty() ? baseURL : blankURL().string());

    if (!isValid())
        reattachToWebProcess();

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.data = data->dataReference();
    loadParameters.MIMEType = MIMEType;
    loadParameters.encodingName = encoding;
    loadParameters.baseURLString = baseURL;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(loadParameters);

    m_process->assumeReadAccessToBaseURL(baseURL);
    m_process->send(Messages::WebPage::LoadData(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

// FIXME: Get rid of loadHTMLString and just use loadData instead.
RefPtr<API::Navigation> WebPageProxy::loadHTMLString(const String& htmlString, const String& baseURL, API::Object* userData)
{
    if (m_isClosed)
        return nullptr;

    auto navigation = m_navigationState->createLoadDataNavigation();

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, !baseURL.isEmpty() ? baseURL : blankURL().string());

    if (!isValid())
        reattachToWebProcess();

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.string = htmlString;
    loadParameters.MIMEType = ASCIILiteral("text/html");
    loadParameters.baseURLString = baseURL;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(loadParameters);

    m_process->assumeReadAccessToBaseURL(baseURL);
    m_process->send(Messages::WebPage::LoadString(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

void WebPageProxy::loadAlternateHTMLString(const String& htmlString, const WebCore::URL& baseURL, const WebCore::URL& unreachableURL, API::Object* userData)
{
    // When the UIProcess is in the process of handling a failing provisional load, do not attempt to
    // start a second alternative HTML load as this will prevent the page load state from being
    // handled properly.
    if (m_isClosed || m_isLoadingAlternateHTMLStringForFailingProvisionalLoad)
        return;

    if (!m_failingProvisionalLoadURL.isEmpty())
        m_isLoadingAlternateHTMLStringForFailingProvisionalLoad = true;

    if (!isValid())
        reattachToWebProcess();

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, unreachableURL);
    m_pageLoadState.setUnreachableURL(transaction, unreachableURL);

    if (m_mainFrame)
        m_mainFrame->setUnreachableURL(unreachableURL);

    LoadParameters loadParameters;
    loadParameters.navigationID = 0;
    loadParameters.string = htmlString;
    loadParameters.baseURLString = baseURL;
    loadParameters.unreachableURLString = unreachableURL;
    loadParameters.provisionalLoadErrorURLString = m_failingProvisionalLoadURL;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(loadParameters);

    m_process->assumeReadAccessToBaseURL(baseURL);
    m_process->assumeReadAccessToBaseURL(unreachableURL);
    m_process->send(Messages::WebPage::LoadAlternateHTMLString(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();
}

void WebPageProxy::loadPlainTextString(const String& string, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setPendingAPIRequestURL(transaction, blankURL().string());

    LoadParameters loadParameters;
    loadParameters.navigationID = 0;
    loadParameters.string = string;
    loadParameters.MIMEType = ASCIILiteral("text/plain");
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(loadParameters);

    m_process->send(Messages::WebPage::LoadString(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();
}

void WebPageProxy::loadWebArchiveData(API::Data* webArchiveData, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setPendingAPIRequestURL(transaction, blankURL().string());

    LoadParameters loadParameters;
    loadParameters.navigationID = 0;
    loadParameters.data = webArchiveData->dataReference();
    loadParameters.MIMEType = ASCIILiteral("application/x-webarchive");
    loadParameters.encodingName = ASCIILiteral("utf-16");
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(loadParameters);

    m_process->send(Messages::WebPage::LoadData(loadParameters), m_pageID);
    m_process->responsivenessTimer().start();
}

void WebPageProxy::navigateToPDFLinkWithSimulatedClick(const String& url, IntPoint documentPoint, IntPoint screenPoint)
{
    if (m_isClosed)
        return;

    if (WebCore::protocolIsJavaScript(url))
        return;

    if (!isValid())
        reattachToWebProcess();

    m_process->send(Messages::WebPage::NavigateToPDFLinkWithSimulatedClick(url, documentPoint, screenPoint), m_pageID);
    m_process->responsivenessTimer().start();
}

void WebPageProxy::stopLoading()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::StopLoading(), m_pageID);
    m_process->responsivenessTimer().start();
}

RefPtr<API::Navigation> WebPageProxy::reload(OptionSet<WebCore::ReloadOption> options)
{
    SandboxExtension::Handle sandboxExtensionHandle;

    String url = currentURL();
    if (!url.isEmpty()) {
        auto transaction = m_pageLoadState.transaction();
        m_pageLoadState.setPendingAPIRequestURL(transaction, url);

        // We may not have an extension yet if back/forward list was reinstated after a WebProcess crash or a browser relaunch
        bool createdExtension = maybeInitializeSandboxExtensionHandle(URL(URL(), url), sandboxExtensionHandle);
        if (createdExtension)
            m_process->willAcquireUniversalFileReadSandboxExtension();
    }

    if (!isValid())
        return reattachToWebProcessForReload();
    
    auto navigation = m_navigationState->createReloadNavigation();

    m_process->send(Messages::WebPage::Reload(navigation->navigationID(), options.toRaw(), sandboxExtensionHandle), m_pageID);
    m_process->responsivenessTimer().start();

    return WTFMove(navigation);
}

void WebPageProxy::recordAutomaticNavigationSnapshot()
{
    if (m_suppressAutomaticNavigationSnapshotting)
        return;

    if (WebBackForwardListItem* item = m_backForwardList->currentItem())
        recordNavigationSnapshot(*item);
}

void WebPageProxy::recordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (!m_shouldRecordNavigationSnapshots)
        return;

#if PLATFORM(COCOA)
    ViewSnapshotStore::singleton().recordSnapshot(*this, item);
#else
    UNUSED_PARAM(item);
#endif
}

RefPtr<API::Navigation> WebPageProxy::goForward()
{
    WebBackForwardListItem* forwardItem = m_backForwardList->forwardItem();
    if (!forwardItem)
        return nullptr;

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, forwardItem->url());

    if (!isValid())
        return reattachToWebProcessWithItem(forwardItem);

    RefPtr<API::Navigation> navigation;
    if (!m_backForwardList->currentItem()->itemIsInSameDocument(*forwardItem))
        navigation = m_navigationState->createBackForwardNavigation();

    m_process->send(Messages::WebPage::GoForward(navigation ? navigation->navigationID() : 0, forwardItem->itemID()), m_pageID);
    m_process->responsivenessTimer().start();

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::goBack()
{
    WebBackForwardListItem* backItem = m_backForwardList->backItem();
    if (!backItem)
        return nullptr;

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, backItem->url());

    if (!isValid())
        return reattachToWebProcessWithItem(backItem);

    RefPtr<API::Navigation> navigation;
    if (!m_backForwardList->currentItem()->itemIsInSameDocument(*backItem))
        navigation = m_navigationState->createBackForwardNavigation();

    m_process->send(Messages::WebPage::GoBack(navigation ? navigation->navigationID() : 0, backItem->itemID()), m_pageID);
    m_process->responsivenessTimer().start();

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::goToBackForwardItem(WebBackForwardListItem* item)
{
    if (!isValid())
        return reattachToWebProcessWithItem(item);

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, item->url());

    RefPtr<API::Navigation> navigation;
    if (!m_backForwardList->currentItem()->itemIsInSameDocument(*item))
        navigation = m_navigationState->createBackForwardNavigation();

    m_process->send(Messages::WebPage::GoToBackForwardItem(navigation ? navigation->navigationID() : 0, item->itemID()), m_pageID);
    m_process->responsivenessTimer().start();

    return navigation;
}

void WebPageProxy::tryRestoreScrollPosition()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::TryRestoreScrollPosition(), m_pageID);
}

void WebPageProxy::didChangeBackForwardList(WebBackForwardListItem* added, Vector<Ref<WebBackForwardListItem>>&& removed)
{
    PageClientProtector protector(m_pageClient);

    if (!m_navigationClient || !m_navigationClient->didChangeBackForwardList(*this, added, removed))
        m_loaderClient->didChangeBackForwardList(*this, added, WTFMove(removed));

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setCanGoBack(transaction, m_backForwardList->backItem());
    m_pageLoadState.setCanGoForward(transaction, m_backForwardList->forwardItem());
}

void WebPageProxy::willGoToBackForwardListItem(uint64_t itemID, bool inPageCache, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    if (auto* item = m_process->webBackForwardItem(itemID)) {
        if (m_navigationClient && m_navigationClient->willGoToBackForwardListItem(*this, *item, inPageCache, m_process->transformHandlesToObjects(userData.object()).get()))
            return;
        m_loaderClient->willGoToBackForwardListItem(*this, *item, m_process->transformHandlesToObjects(userData.object()).get());
    }
}

bool WebPageProxy::shouldKeepCurrentBackForwardListItemInList(WebBackForwardListItem& item)
{
    PageClientProtector protector(m_pageClient);

    return m_loaderClient->shouldKeepCurrentBackForwardListItemInList(*this, item);
}

bool WebPageProxy::canShowMIMEType(const String& mimeType)
{
    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

#if ENABLE(NETSCAPE_PLUGIN_API)
    String newMimeType = mimeType;
    PluginModuleInfo plugin = m_process->processPool().pluginInfoStore().findPlugin(newMimeType, URL());
    if (!plugin.path.isNull() && m_preferences->pluginsEnabled())
        return true;
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if PLATFORM(COCOA)
    // On Mac, we can show PDFs.
    if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(mimeType) && !WebProcessPool::omitPDFSupport())
        return true;
#endif // PLATFORM(COCOA)

    return false;
}

void WebPageProxy::setControlledByAutomation(bool controlled)
{
    if (m_controlledByAutomation == controlled)
        return;

    m_controlledByAutomation = controlled;

    if (isValid())
        m_process->send(Messages::WebPage::SetControlledByAutomation(controlled), m_pageID);
}

#if ENABLE(REMOTE_INSPECTOR)
void WebPageProxy::setAllowsRemoteInspection(bool allow)
{
    if (m_allowsRemoteInspection == allow)
        return;

    m_allowsRemoteInspection = allow;

    if (isValid())
        m_process->send(Messages::WebPage::SetAllowsRemoteInspection(allow), m_pageID);
}

void WebPageProxy::setRemoteInspectionNameOverride(const String& name)
{
    if (m_remoteInspectionNameOverride == name)
        return;

    m_remoteInspectionNameOverride = name;

    if (isValid())
        m_process->send(Messages::WebPage::SetRemoteInspectionNameOverride(m_remoteInspectionNameOverride), m_pageID);
}

#endif

void WebPageProxy::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    if (isValid())
        m_process->send(Messages::WebPage::SetDrawsBackground(drawsBackground), m_pageID);
}

void WebPageProxy::setTopContentInset(float contentInset)
{
    if (m_topContentInset == contentInset)
        return;

    m_topContentInset = contentInset;

    if (!isValid())
        return;
#if PLATFORM(COCOA)
    MachSendRight fence = m_drawingArea->createFence();

    auto fenceAttachment = IPC::Attachment(fence.leakSendRight(), MACH_MSG_TYPE_MOVE_SEND);
    m_process->send(Messages::WebPage::SetTopContentInsetFenced(contentInset, fenceAttachment), m_pageID);
#else
    m_process->send(Messages::WebPage::SetTopContentInset(contentInset), m_pageID);
#endif
}

void WebPageProxy::setUnderlayColor(const Color& color)
{
    if (m_underlayColor == color)
        return;

    m_underlayColor = color;

    if (isValid())
        m_process->send(Messages::WebPage::SetUnderlayColor(color), m_pageID);
}

void WebPageProxy::viewWillStartLiveResize()
{
    if (!isValid())
        return;
#if ENABLE(INPUT_TYPE_COLOR_POPOVER) && ENABLE(INPUT_TYPE_COLOR)
    if (m_colorPicker)
        endColorPicker();
#endif
    m_process->send(Messages::WebPage::ViewWillStartLiveResize(), m_pageID);
}

void WebPageProxy::viewWillEndLiveResize()
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::ViewWillEndLiveResize(), m_pageID);
}

void WebPageProxy::setViewNeedsDisplay(const Region& region)
{
    m_pageClient.setViewNeedsDisplay(region);
}

void WebPageProxy::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, bool isProgrammaticScroll)
{
    m_pageClient.requestScroll(scrollPosition, scrollOrigin, isProgrammaticScroll);
}

WebCore::FloatPoint WebPageProxy::viewScrollPosition() const
{
    return m_pageClient.viewScrollPosition();
}

void WebPageProxy::setSuppressVisibilityUpdates(bool flag)
{
    if (m_suppressVisibilityUpdates == flag)
        return;
    m_suppressVisibilityUpdates = flag;

    if (!m_suppressVisibilityUpdates) {
#if PLATFORM(COCOA)
        m_activityStateChangeDispatcher->schedule();
#else
        dispatchActivityStateChange();
#endif
    }
}

void WebPageProxy::updateActivityState(ActivityState::Flags flagsToUpdate)
{
    m_activityState &= ~flagsToUpdate;
    if (flagsToUpdate & ActivityState::IsFocused && m_pageClient.isViewFocused())
        m_activityState |= ActivityState::IsFocused;
    if (flagsToUpdate & ActivityState::WindowIsActive && m_pageClient.isViewWindowActive())
        m_activityState |= ActivityState::WindowIsActive;
    if (flagsToUpdate & ActivityState::IsVisible && m_pageClient.isViewVisible())
        m_activityState |= ActivityState::IsVisible;
    if (flagsToUpdate & ActivityState::IsVisibleOrOccluded && m_pageClient.isViewVisibleOrOccluded())
        m_activityState |= ActivityState::IsVisibleOrOccluded;
    if (flagsToUpdate & ActivityState::IsInWindow && m_pageClient.isViewInWindow())
        m_activityState |= ActivityState::IsInWindow;
    if (flagsToUpdate & ActivityState::IsVisuallyIdle && m_pageClient.isVisuallyIdle())
        m_activityState |= ActivityState::IsVisuallyIdle;
    if (flagsToUpdate & ActivityState::IsAudible && m_mediaState & MediaProducer::IsPlayingAudio && !(m_mutedState & MediaProducer::AudioIsMuted))
        m_activityState |= ActivityState::IsAudible;
    if (flagsToUpdate & ActivityState::IsLoading && m_pageLoadState.isLoading())
        m_activityState |= ActivityState::IsLoading;
    if (flagsToUpdate & ActivityState::IsCapturingMedia && m_mediaState & (MediaProducer::HasActiveAudioCaptureDevice | MediaProducer::HasActiveVideoCaptureDevice))
        m_activityState |= ActivityState::IsCapturingMedia;
}

void WebPageProxy::activityStateDidChange(ActivityState::Flags mayHaveChanged, bool wantsSynchronousReply, ActivityStateChangeDispatchMode dispatchMode)
{
    m_potentiallyChangedActivityStateFlags |= mayHaveChanged;
    m_activityStateChangeWantsSynchronousReply = m_activityStateChangeWantsSynchronousReply || wantsSynchronousReply;

    if (m_suppressVisibilityUpdates && dispatchMode != ActivityStateChangeDispatchMode::Immediate)
        return;

#if PLATFORM(COCOA)
    bool isNewlyInWindow = !isInWindow() && (mayHaveChanged & ActivityState::IsInWindow) && m_pageClient.isViewInWindow();
    if (dispatchMode == ActivityStateChangeDispatchMode::Immediate || isNewlyInWindow) {
        dispatchActivityStateChange();
        return;
    }
    m_activityStateChangeDispatcher->schedule();
#else
    UNUSED_PARAM(dispatchMode);
    dispatchActivityStateChange();
#endif
}

void WebPageProxy::viewDidLeaveWindow()
{
#if ENABLE(INPUT_TYPE_COLOR_POPOVER) && ENABLE(INPUT_TYPE_COLOR)
    // When leaving the current page, close the popover color well.
    if (m_colorPicker)
        endColorPicker();
#endif
#if PLATFORM(IOS) && HAVE(AVKIT) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    // When leaving the current page, close the video fullscreen.
    if (m_videoFullscreenManager)
        m_videoFullscreenManager->requestHideAndExitFullscreen();
#endif
}

void WebPageProxy::viewDidEnterWindow()
{
    LayerHostingMode layerHostingMode = m_pageClient.viewLayerHostingMode();
    if (m_layerHostingMode != layerHostingMode) {
        m_layerHostingMode = layerHostingMode;
        m_process->send(Messages::WebPage::SetLayerHostingMode(layerHostingMode), m_pageID);
    }
}

void WebPageProxy::dispatchActivityStateChange()
{
#if PLATFORM(COCOA)
    m_activityStateChangeDispatcher->invalidate();
#endif

    if (!isValid())
        return;

    // If the visibility state may have changed, then so may the visually idle & occluded agnostic state.
    if (m_potentiallyChangedActivityStateFlags & ActivityState::IsVisible)
        m_potentiallyChangedActivityStateFlags |= ActivityState::IsVisibleOrOccluded | ActivityState::IsVisuallyIdle;

    // Record the prior view state, update the flags that may have changed,
    // and check which flags have actually changed.
    ActivityState::Flags previousActivityState = m_activityState;
    updateActivityState(m_potentiallyChangedActivityStateFlags);
    ActivityState::Flags changed = m_activityState ^ previousActivityState;

    if ((m_potentiallyChangedActivityStateFlags & ActivityState::IsVisible) && isViewVisible())
        viewIsBecomingVisible();

    bool isNowInWindow = (changed & ActivityState::IsInWindow) && isInWindow();
    // We always want to wait for the Web process to reply if we've been in-window before and are coming back in-window.
    if (m_viewWasEverInWindow && isNowInWindow) {
        if (m_drawingArea->hasVisibleContent() && m_waitsForPaintAfterViewDidMoveToWindow && !m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow)
            m_activityStateChangeWantsSynchronousReply = true;
        m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow = false;
    }

    // Don't wait synchronously if the view state is not visible. (This matters in particular on iOS, where a hidden page may be suspended.)
    if (!(m_activityState & ActivityState::IsVisible))
        m_activityStateChangeWantsSynchronousReply = false;

    if (changed || m_activityStateChangeWantsSynchronousReply || !m_nextActivityStateChangeCallbacks.isEmpty())
        m_process->send(Messages::WebPage::SetActivityState(m_activityState, m_activityStateChangeWantsSynchronousReply, m_nextActivityStateChangeCallbacks), m_pageID);

    m_nextActivityStateChangeCallbacks.clear();

    // This must happen after the SetActivityState message is sent, to ensure the page visibility event can fire.
    updateThrottleState();

#if ENABLE(POINTER_LOCK)
    if (((changed & ActivityState::IsVisible) && !isViewVisible()) || ((changed & ActivityState::WindowIsActive) && !m_pageClient.isViewWindowActive())
        || ((changed & ActivityState::IsFocused) && !(m_activityState & ActivityState::IsFocused)))
        requestPointerUnlock();
#endif

    if (changed & ActivityState::IsVisible) {
        if (isViewVisible())
            m_visiblePageToken = m_process->visiblePageToken();
        else {
            m_visiblePageToken = nullptr;

            // If we've started the responsiveness timer as part of telling the web process to update the backing store
            // state, it might not send back a reply (since it won't paint anything if the web page is hidden) so we
            // stop the unresponsiveness timer here.
            m_process->responsivenessTimer().stop();
        }
    }

    if (changed & ActivityState::IsInWindow) {
        if (isInWindow())
            viewDidEnterWindow();
        else
            viewDidLeaveWindow();
    }

    updateBackingStoreDiscardableState();

    if (m_activityStateChangeWantsSynchronousReply)
        waitForDidUpdateActivityState();

    m_potentiallyChangedActivityStateFlags = ActivityState::NoFlags;
    m_activityStateChangeWantsSynchronousReply = false;
    m_viewWasEverInWindow |= isNowInWindow;
}

bool WebPageProxy::isAlwaysOnLoggingAllowed() const
{
    return sessionID().isAlwaysOnLoggingAllowed();
}

void WebPageProxy::updateThrottleState()
{
    bool processSuppressionEnabled = m_preferences->pageVisibilityBasedProcessSuppressionEnabled();

    // If process suppression is not enabled take a token on the process pool to disable suppression of support processes.
    if (!processSuppressionEnabled)
        m_preventProcessSuppressionCount = m_process->processPool().processSuppressionDisabledForPageCount();
    else if (!m_preventProcessSuppressionCount)
        m_preventProcessSuppressionCount = nullptr;

    if (m_activityState & ActivityState::IsVisuallyIdle)
        m_pageIsUserObservableCount = nullptr;
    else if (!m_pageIsUserObservableCount)
        m_pageIsUserObservableCount = m_process->processPool().userObservablePageCount();

#if PLATFORM(IOS)
    bool isCapturingMedia = m_activityState & ActivityState::IsCapturingMedia;
    bool isAudible = m_activityState & ActivityState::IsAudible;
    if (!isViewVisible() && !m_alwaysRunsAtForegroundPriority && !isCapturingMedia && !isAudible) {
        if (m_activityToken) {
            RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "%p - UIProcess is releasing a foreground assertion because the view is no longer visible", this);
            m_activityToken = nullptr;
        }
    } else if (!m_activityToken) {
        if (isViewVisible())
            RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "%p - UIProcess is taking a foreground assertion because the view is visible", this);
        else if (isAudible)
            RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "%p - UIProcess is taking a foreground assertion because we are playing audio", this);
        else if (isCapturingMedia)
            RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "%p - UIProcess is taking a foreground assertion because media capture is active", this);
        else
            RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "%p - UIProcess is taking a foreground assertion even though the view is not visible because m_alwaysRunsAtForegroundPriority is true", this);
        m_activityToken = m_process->throttler().foregroundActivityToken();
    }
#endif
}

void WebPageProxy::updateHiddenPageThrottlingAutoIncreases()
{
    if (!m_preferences->hiddenPageDOMTimerThrottlingAutoIncreases())
        m_hiddenPageDOMTimerThrottlingAutoIncreasesCount = nullptr;
    else if (!m_hiddenPageDOMTimerThrottlingAutoIncreasesCount)
        m_hiddenPageDOMTimerThrottlingAutoIncreasesCount = m_process->processPool().hiddenPageThrottlingAutoIncreasesCount();
}

void WebPageProxy::layerHostingModeDidChange()
{
    if (!isValid())
        return;

    LayerHostingMode layerHostingMode = m_pageClient.viewLayerHostingMode();
    if (m_layerHostingMode == layerHostingMode)
        return;

    m_layerHostingMode = layerHostingMode;
    m_process->send(Messages::WebPage::SetLayerHostingMode(layerHostingMode), m_pageID);
}

void WebPageProxy::waitForDidUpdateActivityState()
{
    if (!isValid())
        return;

    if (m_process->state() != WebProcessProxy::State::Running)
        return;

    // If we have previously timed out with no response from the WebProcess, don't block the UIProcess again until it starts responding.
    if (m_waitingForDidUpdateActivityState)
        return;

#if PLATFORM(IOS)
    // Hail Mary check. Should not be possible (dispatchActivityStateChange should force async if not visible,
    // and if visible we should be holding an assertion) - but we should never block on a suspended process.
    if (!m_activityToken) {
        ASSERT_NOT_REACHED();
        return;
    }
#endif

    m_waitingForDidUpdateActivityState = true;

    m_drawingArea->waitForDidUpdateActivityState();
}

IntSize WebPageProxy::viewSize() const
{
    return m_pageClient.viewSize();
}

void WebPageProxy::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& keyboardEvent, WTF::Function<void (CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SetInitialFocus(forward, isKeyboardEventValid, keyboardEvent, callbackID), m_pageID);
}

void WebPageProxy::clearSelection()
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::ClearSelection(), m_pageID);
}

void WebPageProxy::restoreSelectionInFocusedEditableElement()
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::RestoreSelectionInFocusedEditableElement(), m_pageID);
}

void WebPageProxy::validateCommand(const String& commandName, WTF::Function<void (const String&, bool, int32_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), false, 0, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::ValidateCommand(commandName, callbackID), m_pageID);
}

bool WebPageProxy::maintainsInactiveSelection() const
{
    // Regardless of what the client wants to do, keep selections if a local Inspector is open.
    // Otherwise, there is no way to use the console to inspect the state of a selection.
    if (inspector() && inspector()->isVisible())
        return true;

    return m_maintainsInactiveSelection;
}

void WebPageProxy::setMaintainsInactiveSelection(bool newValue)
{
    m_maintainsInactiveSelection = newValue;
}

void WebPageProxy::executeEditCommand(const String& commandName, const String& argument, WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::ExecuteEditCommandWithCallback(commandName, argument, callbackID), m_pageID);
}
    
void WebPageProxy::executeEditCommand(const String& commandName, const String& argument)
{
    static NeverDestroyed<String> ignoreSpellingCommandName(MAKE_STATIC_STRING_IMPL("ignoreSpelling"));

    if (!isValid())
        return;

    if (commandName == ignoreSpellingCommandName)
        ++m_pendingLearnOrIgnoreWordMessageCount;

    m_process->send(Messages::WebPage::ExecuteEditCommand(commandName, argument), m_pageID);
}

void WebPageProxy::setEditable(bool editable)
{
    if (editable == m_isEditable)
        return;
    if (!isValid())
        return;

    m_isEditable = editable;
    m_process->send(Messages::WebPage::SetEditable(editable), m_pageID);
}
    
void WebPageProxy::setMediaStreamCaptureMuted(bool muted)
{
    if (muted)
        setMuted(m_mutedState | WebCore::MediaProducer::CaptureDevicesAreMuted);
    else
        setMuted(m_mutedState & ~WebCore::MediaProducer::CaptureDevicesAreMuted);
}

void WebPageProxy::activateMediaStreamCaptureInPage()
{
#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().muteCaptureMediaStreamsExceptIn(*this);
#endif
    setMuted(m_mutedState & ~WebCore::MediaProducer::CaptureDevicesAreMuted);
}

#if !PLATFORM(IOS)
void WebPageProxy::didCommitLayerTree(const RemoteLayerTreeTransaction&)
{
}

void WebPageProxy::layerTreeCommitComplete()
{
}
#endif

#if ENABLE(DRAG_SUPPORT)
void WebPageProxy::dragEntered(DragData& dragData, const String& dragStorageName)
{
    performDragControllerAction(DragControllerAction::Entered, dragData, dragStorageName, { }, { });
}

void WebPageProxy::dragUpdated(DragData& dragData, const String& dragStorageName)
{
    performDragControllerAction(DragControllerAction::Updated, dragData, dragStorageName, { }, { });
}

void WebPageProxy::dragExited(DragData& dragData, const String& dragStorageName)
{
    performDragControllerAction(DragControllerAction::Exited, dragData, dragStorageName, { }, { });
}

void WebPageProxy::performDragOperation(DragData& dragData, const String& dragStorageName, SandboxExtension::Handle&& sandboxExtensionHandle, SandboxExtension::HandleArray&& sandboxExtensionsForUpload)
{
    performDragControllerAction(DragControllerAction::PerformDragOperation, dragData, dragStorageName, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionsForUpload));
}

void WebPageProxy::performDragControllerAction(DragControllerAction action, DragData& dragData, const String& dragStorageName, SandboxExtension::Handle&& sandboxExtensionHandle, SandboxExtension::HandleArray&& sandboxExtensionsForUpload)
{
    if (!isValid())
        return;
#if PLATFORM(GTK)
    UNUSED_PARAM(dragStorageName);
    UNUSED_PARAM(sandboxExtensionHandle);
    UNUSED_PARAM(sandboxExtensionsForUpload);

    String url = dragData.asURL();
    if (!url.isEmpty())
        m_process->assumeReadAccessToBaseURL(url);

    ASSERT(dragData.platformData());
    WebSelectionData selection(*dragData.platformData());
    m_process->send(Messages::WebPage::PerformDragControllerAction(action, dragData.clientPosition(), dragData.globalPosition(), dragData.draggingSourceOperationMask(), selection, dragData.flags()), m_pageID);
#else
    m_process->send(Messages::WebPage::PerformDragControllerAction(action, dragData, sandboxExtensionHandle, sandboxExtensionsForUpload), m_pageID);
#endif
}

void WebPageProxy::didPerformDragControllerAction(uint64_t dragOperation, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted, const IntRect& insertionRect)
{
    MESSAGE_CHECK(dragOperation <= DragOperationDelete);

    m_currentDragOperation = static_cast<DragOperation>(dragOperation);
    m_currentDragIsOverFileInput = mouseIsOverFileInput;
    m_currentDragNumberOfFilesToBeAccepted = numberOfItemsToBeAccepted;
    setDragCaretRect(insertionRect);
}

#if PLATFORM(GTK)
void WebPageProxy::startDrag(WebSelectionData&& selection, uint64_t dragOperation, const ShareableBitmap::Handle& dragImageHandle)
{
    RefPtr<ShareableBitmap> dragImage = !dragImageHandle.isNull() ? ShareableBitmap::create(dragImageHandle) : nullptr;
    m_pageClient.startDrag(WTFMove(selection.selectionData), static_cast<WebCore::DragOperation>(dragOperation), WTFMove(dragImage));

    didStartDrag();
}
#endif

void WebPageProxy::dragEnded(const IntPoint& clientPosition, const IntPoint& globalPosition, uint64_t operation)
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::DragEnded(clientPosition, globalPosition, operation), m_pageID);
    setDragCaretRect({ });
}

void WebPageProxy::didStartDrag()
{
    if (isValid())
        m_process->send(Messages::WebPage::DidStartDrag(), m_pageID);
}
    
void WebPageProxy::dragCancelled()
{
    if (isValid())
        m_process->send(Messages::WebPage::DragCancelled(), m_pageID);
}

void WebPageProxy::didEndDragging()
{
    resetCurrentDragInformation();
}

void WebPageProxy::resetCurrentDragInformation()
{
    m_currentDragOperation = WebCore::DragOperationNone;
    m_currentDragIsOverFileInput = false;
    m_currentDragNumberOfFilesToBeAccepted = 0;
    setDragCaretRect({ });
}

#if !ENABLE(DATA_INTERACTION)

void WebPageProxy::setDragCaretRect(const IntRect& dragCaretRect)
{
    m_currentDragCaretRect = dragCaretRect;
}

#endif

#endif // ENABLE(DRAG_SUPPORT)

void WebPageProxy::handleMouseEvent(const NativeWebMouseEvent& event)
{
    if (!isValid())
        return;

    if (m_pageClient.windowIsFrontWindowUnderMouse(event))
        setToolTip(String());

    // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
    if (event.type() != WebEvent::MouseMove)
        m_process->responsivenessTimer().start();
    else {
        if (m_processingMouseMoveEvent) {
            m_nextMouseMoveEvent = std::make_unique<NativeWebMouseEvent>(event);
            return;
        }

        m_processingMouseMoveEvent = true;
    }

    // <https://bugs.webkit.org/show_bug.cgi?id=57904> We need to keep track of the mouse down event in the case where we
    // display a popup menu for select elements. When the user changes the selected item,
    // we fake a mouse up event by using this stored down event. This event gets cleared
    // when the mouse up message is received from WebProcess.
    if (event.type() == WebEvent::MouseDown)
        m_currentlyProcessedMouseDownEvent = std::make_unique<NativeWebMouseEvent>(event);

    m_process->send(Messages::WebPage::MouseEvent(event), m_pageID);
}

#if MERGE_WHEEL_EVENTS
static bool canCoalesce(const WebWheelEvent& a, const WebWheelEvent& b)
{
    if (a.position() != b.position())
        return false;
    if (a.globalPosition() != b.globalPosition())
        return false;
    if (a.modifiers() != b.modifiers())
        return false;
    if (a.granularity() != b.granularity())
        return false;
#if PLATFORM(COCOA)
    if (a.phase() != b.phase())
        return false;
    if (a.momentumPhase() != b.momentumPhase())
        return false;
    if (a.hasPreciseScrollingDeltas() != b.hasPreciseScrollingDeltas())
        return false;
#endif

    return true;
}

static WebWheelEvent coalesce(const WebWheelEvent& a, const WebWheelEvent& b)
{
    ASSERT(canCoalesce(a, b));

    FloatSize mergedDelta = a.delta() + b.delta();
    FloatSize mergedWheelTicks = a.wheelTicks() + b.wheelTicks();

#if PLATFORM(COCOA)
    FloatSize mergedUnacceleratedScrollingDelta = a.unacceleratedScrollingDelta() + b.unacceleratedScrollingDelta();

    return WebWheelEvent(WebEvent::Wheel, b.position(), b.globalPosition(), mergedDelta, mergedWheelTicks, b.granularity(), b.directionInvertedFromDevice(), b.phase(), b.momentumPhase(), b.hasPreciseScrollingDeltas(), b.scrollCount(), mergedUnacceleratedScrollingDelta, b.modifiers(), b.timestamp());
#else
    return WebWheelEvent(WebEvent::Wheel, b.position(), b.globalPosition(), mergedDelta, mergedWheelTicks, b.granularity(), b.modifiers(), b.timestamp());
#endif
}
#endif // MERGE_WHEEL_EVENTS

static WebWheelEvent coalescedWheelEvent(Deque<NativeWebWheelEvent>& queue, Vector<NativeWebWheelEvent>& coalescedEvents)
{
    ASSERT(!queue.isEmpty());
    ASSERT(coalescedEvents.isEmpty());

#if MERGE_WHEEL_EVENTS
    NativeWebWheelEvent firstEvent = queue.takeFirst();
    coalescedEvents.append(firstEvent);

    WebWheelEvent event = firstEvent;
    while (!queue.isEmpty() && canCoalesce(event, queue.first())) {
        NativeWebWheelEvent firstEvent = queue.takeFirst();
        coalescedEvents.append(firstEvent);
        event = coalesce(event, firstEvent);
    }

    return event;
#else
    while (!queue.isEmpty())
        coalescedEvents.append(queue.takeFirst());
    return coalescedEvents.last();
#endif
}

void WebPageProxy::handleWheelEvent(const NativeWebWheelEvent& event)
{
#if ENABLE(ASYNC_SCROLLING)
    if (m_scrollingCoordinatorProxy && m_scrollingCoordinatorProxy->handleWheelEvent(platform(event)))
        return;
#endif

    if (!isValid())
        return;

    hideValidationMessage();

    if (!m_currentlyProcessedWheelEvents.isEmpty()) {
        m_wheelEventQueue.append(event);
        if (!shouldProcessWheelEventNow(event))
            return;
        // The queue has too many wheel events, so push a new event.
    }

    if (!m_wheelEventQueue.isEmpty()) {
        processNextQueuedWheelEvent();
        return;
    }

    auto coalescedWheelEvent = std::make_unique<Vector<NativeWebWheelEvent>>();
    coalescedWheelEvent->append(event);
    m_currentlyProcessedWheelEvents.append(WTFMove(coalescedWheelEvent));
    sendWheelEvent(event);
}

void WebPageProxy::processNextQueuedWheelEvent()
{
    auto nextCoalescedEvent = std::make_unique<Vector<NativeWebWheelEvent>>();
    WebWheelEvent nextWheelEvent = coalescedWheelEvent(m_wheelEventQueue, *nextCoalescedEvent.get());
    m_currentlyProcessedWheelEvents.append(WTFMove(nextCoalescedEvent));
    sendWheelEvent(nextWheelEvent);
}

void WebPageProxy::sendWheelEvent(const WebWheelEvent& event)
{
    m_process->send(
        Messages::EventDispatcher::WheelEvent(
            m_pageID,
            event,
            shouldUseImplicitRubberBandControl() ? !m_backForwardList->backItem() : rubberBandsAtLeft(),
            shouldUseImplicitRubberBandControl() ? !m_backForwardList->forwardItem() : rubberBandsAtRight(),
            rubberBandsAtTop(),
            rubberBandsAtBottom()
        ), 0);

    // Manually ping the web process to check for responsiveness since our wheel
    // event will dispatch to a non-main thread, which always responds.
    m_process->isResponsive(nullptr);
}

bool WebPageProxy::shouldProcessWheelEventNow(const WebWheelEvent& event) const
{
#if PLATFORM(GTK)
    // Don't queue events representing a non-trivial scrolling phase to
    // avoid having them trapped in the queue, potentially preventing a
    // scrolling session to beginning or end correctly.
    // This is only needed by platforms whose WebWheelEvent has this phase
    // information (Cocoa and GTK+) but Cocoa was fine without it.
    if (event.phase() == WebWheelEvent::Phase::PhaseNone
        || event.phase() == WebWheelEvent::Phase::PhaseChanged
        || event.momentumPhase() == WebWheelEvent::Phase::PhaseNone
        || event.momentumPhase() == WebWheelEvent::Phase::PhaseChanged)
        return true;
#else
    UNUSED_PARAM(event);
#endif
    if (m_wheelEventQueue.size() >= wheelEventQueueSizeThreshold)
        return true;
    return false;
}

void WebPageProxy::handleKeyboardEvent(const NativeWebKeyboardEvent& event)
{
    if (!isValid())
        return;
    
    LOG(KeyHandling, "WebPageProxy::handleKeyboardEvent: %s", webKeyboardEventTypeString(event.type()));

    m_keyEventQueue.append(event);

    m_process->responsivenessTimer().start();
    if (m_keyEventQueue.size() == 1) { // Otherwise, sent from DidReceiveEvent message handler.
        LOG(KeyHandling, " UI process: sent keyEvent from handleKeyboardEvent");
        m_process->send(Messages::WebPage::KeyEvent(event), m_pageID);
    }
}

WebPreferencesStore WebPageProxy::preferencesStore() const
{
    if (m_configurationPreferenceValues.isEmpty())
        return m_preferences->store();

    WebPreferencesStore store = m_preferences->store();
    for (const auto& preference : m_configurationPreferenceValues)
        store.m_values.set(preference.key, preference.value);

    return store;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebPageProxy::findPlugin(const String& mimeType, uint32_t processType, const String& urlString, const String& frameURLString, const String& pageURLString, bool allowOnlyApplicationPlugins, uint64_t& pluginProcessToken, String& newMimeType, uint32_t& pluginLoadPolicy, String& unavailabilityDescription)
{
    PageClientProtector protector(m_pageClient);

    MESSAGE_CHECK_URL(urlString);

    newMimeType = mimeType.convertToASCIILowercase();
    pluginLoadPolicy = PluginModuleLoadNormally;

    PluginData::AllowedPluginTypes allowedPluginTypes = allowOnlyApplicationPlugins ? PluginData::OnlyApplicationPlugins : PluginData::AllPlugins;
    PluginModuleInfo plugin = m_process->processPool().pluginInfoStore().findPlugin(newMimeType, URL(URL(), urlString), allowedPluginTypes);
    if (!plugin.path) {
        pluginProcessToken = 0;
        return;
    }

    pluginLoadPolicy = PluginInfoStore::defaultLoadPolicyForPlugin(plugin);

#if PLATFORM(COCOA)
    auto pluginInformation = createPluginInformationDictionary(plugin, frameURLString, String(), pageURLString, String(), String());
    if (m_navigationClient)
        pluginLoadPolicy = m_navigationClient->decidePolicyForPluginLoad(*this, static_cast<PluginModuleLoadPolicy>(pluginLoadPolicy), pluginInformation.get(), unavailabilityDescription);
    else
        pluginLoadPolicy = m_loaderClient->pluginLoadPolicy(*this, static_cast<PluginModuleLoadPolicy>(pluginLoadPolicy), pluginInformation.get(), unavailabilityDescription);
#else
    UNUSED_PARAM(frameURLString);
    UNUSED_PARAM(pageURLString);
    UNUSED_PARAM(unavailabilityDescription);
#endif

    PluginProcessSandboxPolicy pluginProcessSandboxPolicy = PluginProcessSandboxPolicyNormal;
    switch (pluginLoadPolicy) {
    case PluginModuleLoadNormally:
        pluginProcessSandboxPolicy = PluginProcessSandboxPolicyNormal;
        break;
    case PluginModuleLoadUnsandboxed:
        pluginProcessSandboxPolicy = PluginProcessSandboxPolicyUnsandboxed;
        break;

    case PluginModuleBlockedForSecurity:
    case PluginModuleBlockedForCompatibility:
        pluginProcessToken = 0;
        return;
    }

    pluginProcessToken = PluginProcessManager::singleton().pluginProcessToken(plugin, static_cast<PluginProcessType>(processType), pluginProcessSandboxPolicy);
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(TOUCH_EVENTS)

static TrackingType mergeTrackingTypes(TrackingType a, TrackingType b)
{
    if (static_cast<uintptr_t>(b) > static_cast<uintptr_t>(a))
        return b;
    return a;
}

void WebPageProxy::updateTouchEventTracking(const WebTouchEvent& touchStartEvent)
{
#if ENABLE(ASYNC_SCROLLING)
    const EventNames& names = eventNames();
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        IntPoint location = touchPoint.location();
        auto updateTrackingType = [this, location](TrackingType& trackingType, const AtomicString& eventName) {
            if (trackingType == TrackingType::Synchronous)
                return;

            TrackingType trackingTypeForLocation = m_scrollingCoordinatorProxy->eventTrackingTypeForPoint(eventName, location);

            trackingType = mergeTrackingTypes(trackingType, trackingTypeForLocation);
        };
        updateTrackingType(m_touchEventTracking.touchForceChangedTracking, names.touchforcechangeEvent);
        updateTrackingType(m_touchEventTracking.touchStartTracking, names.touchstartEvent);
        updateTrackingType(m_touchEventTracking.touchMoveTracking, names.touchmoveEvent);
        updateTrackingType(m_touchEventTracking.touchEndTracking, names.touchendEvent);
    }
#else
    UNUSED_PARAM(touchStartEvent);
    m_touchEventTracking.touchForceChangedTracking = TrackingType::Synchronous;
    m_touchEventTracking.touchStartTracking = TrackingType::Synchronous;
    m_touchEventTracking.touchMoveTracking = TrackingType::Synchronous;
    m_touchEventTracking.touchEndTracking = TrackingType::Synchronous;
#endif // ENABLE(ASYNC_SCROLLING)
}

TrackingType WebPageProxy::touchEventTrackingType(const WebTouchEvent& touchStartEvent) const
{
    // We send all events if any type is needed, we just do it asynchronously for the types that are not tracked.
    //
    // Touch events define a sequence with strong dependencies. For example, we can expect
    // a TouchMove to only appear after a TouchStart, and the ids of the touch points is consistent between
    // the two.
    //
    // WebCore should not have to set up its state correctly after some events were dismissed.
    // For example, we don't want to send a TouchMoved without a TouchPressed.
    // We send everything, WebCore updates its internal state and dispatch what is needed to the page.
    TrackingType globalTrackingType = m_touchEventTracking.isTrackingAnything() ? TrackingType::Asynchronous : TrackingType::NotTracking;

    globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchEventTracking.touchForceChangedTracking);
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        switch (touchPoint.state()) {
        case WebPlatformTouchPoint::TouchReleased:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchEventTracking.touchEndTracking);
            break;
        case WebPlatformTouchPoint::TouchPressed:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchEventTracking.touchStartTracking);
            break;
        case WebPlatformTouchPoint::TouchMoved:
        case WebPlatformTouchPoint::TouchStationary:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchEventTracking.touchMoveTracking);
            break;
        case WebPlatformTouchPoint::TouchCancelled:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, TrackingType::Asynchronous);
            break;
        }
    }

    return globalTrackingType;
}

#endif

#if ENABLE(MAC_GESTURE_EVENTS)
void WebPageProxy::handleGestureEvent(const NativeWebGestureEvent& event)
{
    if (!isValid())
        return;

    m_gestureEventQueue.append(event);
    // FIXME: Consider doing some coalescing here.
    m_process->responsivenessTimer().start();

    m_process->send(Messages::EventDispatcher::GestureEvent(m_pageID, event), 0);
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPageProxy::handleTouchEventSynchronously(NativeWebTouchEvent& event)
{
    if (!isValid())
        return;

    TraceScope scope(SyncTouchEventStart, SyncTouchEventEnd);

    if (event.type() == WebEvent::TouchStart)
        updateTouchEventTracking(event);

    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking)
        return;

    if (touchEventsTrackingType == TrackingType::Asynchronous) {
        // We can end up here if a native gesture has not started but the event handlers are passive.
        //
        // The client of WebPageProxy asks the event to be sent synchronously since the touch event
        // can prevent a native gesture.
        // But, here we know that all events handlers that can handle this events are passive.
        // We can use asynchronous dispatch and pretend to the client that the page does nothing with the events.
        event.setCanPreventNativeGestures(false);
        handleTouchEventAsynchronously(event);
        didReceiveEvent(event.type(), false);
        return;
    }

    m_process->responsivenessTimer().start();
    bool handled = false;
    m_process->sendSync(Messages::WebPage::TouchEventSync(event), Messages::WebPage::TouchEventSync::Reply(handled), m_pageID);
    didReceiveEvent(event.type(), handled);
    m_pageClient.doneWithTouchEvent(event, handled);
    m_process->responsivenessTimer().stop();

    if (event.allTouchPointsAreReleased())
        m_touchEventTracking.reset();
}

void WebPageProxy::handleTouchEventAsynchronously(const NativeWebTouchEvent& event)
{
    if (!isValid())
        return;

    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking)
        return;

    m_process->send(Messages::EventDispatcher::TouchEvent(m_pageID, event), 0);

    if (event.allTouchPointsAreReleased())
        m_touchEventTracking.reset();
}

#elif ENABLE(TOUCH_EVENTS)
void WebPageProxy::handleTouchEvent(const NativeWebTouchEvent& event)
{
    if (!isValid())
        return;

    if (event.type() == WebEvent::TouchStart)
        updateTouchEventTracking(event);

    if (touchEventTrackingType(event) == TrackingType::NotTracking)
        return;

    // If the page is suspended, which should be the case during panning, pinching
    // and animation on the page itself (kinetic scrolling, tap to zoom) etc, then
    // we do not send any of the events to the page even if is has listeners.
    if (!m_isPageSuspended) {
        m_touchEventQueue.append(event);
        m_process->responsivenessTimer().start();
        m_process->send(Messages::WebPage::TouchEvent(event), m_pageID);
    } else {
        if (m_touchEventQueue.isEmpty()) {
            bool isEventHandled = false;
            m_pageClient.doneWithTouchEvent(event, isEventHandled);
        } else {
            // We attach the incoming events to the newest queued event so that all
            // the events are delivered in the correct order when the event is dequed.
            QueuedTouchEvents& lastEvent = m_touchEventQueue.last();
            lastEvent.deferredTouchEvents.append(event);
        }
    }

    if (event.allTouchPointsAreReleased())
        m_touchEventTracking.reset();
}
#endif // ENABLE(TOUCH_EVENTS)

void WebPageProxy::scrollBy(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::ScrollBy(direction, granularity), m_pageID);
}

void WebPageProxy::centerSelectionInVisibleArea()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::CenterSelectionInVisibleArea(), m_pageID);
}

void WebPageProxy::receivedPolicyDecision(PolicyAction action, WebFrameProxy& frame, uint64_t listenerID, API::Navigation* navigation, std::optional<WebsitePoliciesData>&& websitePolicies)
{
    if (!isValid())
        return;

    auto transaction = m_pageLoadState.transaction();

    if (action == PolicyAction::Ignore)
        m_pageLoadState.clearPendingAPIRequestURL(transaction);

    if (navigation && navigation->shouldForceDownload() && action == PolicyAction::Use)
        action = PolicyAction::Download;

    DownloadID downloadID = { };
    if (action == PolicyAction::Download) {
        // Create a download proxy.
        const ResourceRequest& downloadRequest = m_decidePolicyForResponseRequest ? *m_decidePolicyForResponseRequest : ResourceRequest();
        DownloadProxy* download = m_process->processPool().createDownloadProxy(downloadRequest, this);
        if (navigation) {
            download->setWasUserInitiated(navigation->wasUserInitiated());
            download->setRedirectChain(navigation->takeRedirectChain());
        }

        downloadID = download->downloadID();
        handleDownloadRequest(download);
    }

    // If we received a policy decision while in decidePolicyForResponse the decision will
    // be sent back to the web process by decidePolicyForResponse.
    if (m_inDecidePolicyForResponseSync) {
        m_syncMimeTypePolicyActionIsValid = true;
        m_syncMimeTypePolicyAction = action;
        m_syncMimeTypePolicyDownloadID = downloadID;
        return;
    }

    // If we received a policy decision while in decidePolicyForNavigationAction the decision will 
    // be sent back to the web process by decidePolicyForNavigationAction. 
    if (m_inDecidePolicyForNavigationAction) {
        m_syncNavigationActionPolicyActionIsValid = true;
        m_syncNavigationActionPolicyAction = action;
        m_syncNavigationActionPolicyDownloadID = downloadID;
        m_syncNavigationActionPolicyWebsitePolicies = WTFMove(websitePolicies);
        return;
    }
    
    m_process->send(Messages::WebPage::DidReceivePolicyDecision(frame.frameID(), listenerID, action, navigation ? navigation->navigationID() : 0, downloadID, websitePolicies), m_pageID);
}

void WebPageProxy::setUserAgent(String&& userAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = WTFMove(userAgent);

#if ENABLE(SERVICE_WORKER)
    // We update the service worker there at the moment to be sure we use values used by actual web pages.
    // FIXME: Refactor this when we have a better User-Agent story.
    process().processPool().updateServiceWorkerUserAgent(m_userAgent);
#endif

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetUserAgent(m_userAgent), m_pageID);
}

void WebPageProxy::setApplicationNameForUserAgent(const String& applicationName)
{
    if (m_applicationNameForUserAgent == applicationName)
        return;

    m_applicationNameForUserAgent = applicationName;
    if (!m_customUserAgent.isEmpty())
        return;

    setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
}

void WebPageProxy::setCustomUserAgent(const String& customUserAgent)
{
    if (m_customUserAgent == customUserAgent)
        return;

    m_customUserAgent = customUserAgent;

    if (m_customUserAgent.isEmpty()) {
        setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
        return;
    }

    setUserAgent(String { m_customUserAgent });
}

void WebPageProxy::resumeActiveDOMObjectsAndAnimations()
{
    if (!isValid() || !m_isPageSuspended)
        return;

    m_isPageSuspended = false;

    m_process->send(Messages::WebPage::ResumeActiveDOMObjectsAndAnimations(), m_pageID);
}

void WebPageProxy::suspendActiveDOMObjectsAndAnimations()
{
    if (!isValid() || m_isPageSuspended)
        return;

    m_isPageSuspended = true;

    m_process->send(Messages::WebPage::SuspendActiveDOMObjectsAndAnimations(), m_pageID);
}

bool WebPageProxy::supportsTextEncoding() const
{
    // FIXME (118840): We should probably only support this for text documents, not all non-image documents.
    return m_mainFrame && !m_mainFrame->isDisplayingStandaloneImageDocument();
}

void WebPageProxy::setCustomTextEncodingName(const String& encodingName)
{
    if (m_customTextEncodingName == encodingName)
        return;
    m_customTextEncodingName = encodingName;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetCustomTextEncodingName(encodingName), m_pageID);
}

SessionState WebPageProxy::sessionState(WTF::Function<bool (WebBackForwardListItem&)>&& filter) const
{
    SessionState sessionState;

    sessionState.backForwardListState = m_backForwardList->backForwardListState(WTFMove(filter));

    String provisionalURLString = m_pageLoadState.pendingAPIRequestURL();
    if (provisionalURLString.isEmpty())
        provisionalURLString = m_pageLoadState.provisionalURL();

    if (!provisionalURLString.isEmpty())
        sessionState.provisionalURL = URL(URL(), provisionalURLString);

    sessionState.renderTreeSize = renderTreeSize();
    return sessionState;
}

RefPtr<API::Navigation> WebPageProxy::restoreFromSessionState(SessionState sessionState, bool navigate)
{
    m_sessionRestorationRenderTreeSize = 0;
    m_hitRenderTreeSizeThreshold = false;

    bool hasBackForwardList = !!sessionState.backForwardListState.currentIndex;

    if (hasBackForwardList) {
        m_backForwardList->restoreFromState(WTFMove(sessionState.backForwardListState));

        for (const auto& entry : m_backForwardList->entries())
            process().registerNewWebBackForwardListItem(entry.get());

        process().send(Messages::WebPage::RestoreSession(m_backForwardList->itemStates()), m_pageID);

        auto transaction = m_pageLoadState.transaction();
        m_pageLoadState.setCanGoBack(transaction, m_backForwardList->backItem());
        m_pageLoadState.setCanGoForward(transaction, m_backForwardList->forwardItem());

        // The back / forward list was restored from a sessionState so we don't want to snapshot the current
        // page when navigating away. Suppress navigation snapshotting until the next load has committed
        m_suppressAutomaticNavigationSnapshotting = true;
    }

    // FIXME: Navigating should be separate from state restoration.
    if (navigate) {
        m_sessionRestorationRenderTreeSize = sessionState.renderTreeSize;
        if (!m_sessionRestorationRenderTreeSize)
            m_hitRenderTreeSizeThreshold = true; // If we didn't get data on renderTreeSize, just don't fire the milestone.

        if (!sessionState.provisionalURL.isNull())
            return loadRequest(sessionState.provisionalURL);

        if (hasBackForwardList) {
            // FIXME: Do we have to null check the back forward list item here?
            if (WebBackForwardListItem* item = m_backForwardList->currentItem())
                return goToBackForwardItem(item);
        }
    }

    return nullptr;
}

bool WebPageProxy::supportsTextZoom() const
{
    // FIXME (118840): This should also return false for standalone media and plug-in documents.
    if (!m_mainFrame || m_mainFrame->isDisplayingStandaloneImageDocument())
        return false;

    return true;
}
 
void WebPageProxy::setTextZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_textZoomFactor == zoomFactor)
        return;

    m_textZoomFactor = zoomFactor;
    m_process->send(Messages::WebPage::SetTextZoomFactor(m_textZoomFactor), m_pageID); 
}

void WebPageProxy::setPageZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == zoomFactor)
        return;

    hideValidationMessage();

    m_pageZoomFactor = zoomFactor;
    m_process->send(Messages::WebPage::SetPageZoomFactor(m_pageZoomFactor), m_pageID); 
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    hideValidationMessage();

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;
    m_process->send(Messages::WebPage::SetPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor), m_pageID); 
}

double WebPageProxy::pageZoomFactor() const
{
    // Zoom factor for non-PDF pages persists across page loads. We maintain a separate member variable for PDF
    // zoom which ensures that we don't use the PDF zoom for a normal page.
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginZoomFactor;
    return m_pageZoomFactor;
}

double WebPageProxy::pageScaleFactor() const
{
    // PDF documents use zoom and scale factors to size themselves appropriately in the window. We store them
    // separately but decide which to return based on the main frame.
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginScaleFactor;
    return m_pageScaleFactor;
}

void WebPageProxy::scalePage(double scale, const IntPoint& origin)
{
    ASSERT(scale > 0);

    if (!isValid())
        return;

    m_pageScaleFactor = scale;
    m_process->send(Messages::WebPage::ScalePage(scale, origin), m_pageID);
}

void WebPageProxy::scalePageInViewCoordinates(double scale, const IntPoint& centerInViewCoordinates)
{
    ASSERT(scale > 0);

    if (!isValid())
        return;

    m_pageScaleFactor = scale;
    m_process->send(Messages::WebPage::ScalePageInViewCoordinates(scale, centerInViewCoordinates), m_pageID);
}

void WebPageProxy::scaleView(double scale)
{
    ASSERT(scale > 0);

    if (!isValid())
        return;

    m_viewScaleFactor = scale;
    m_process->send(Messages::WebPage::ScaleView(scale), m_pageID);
}

void WebPageProxy::setIntrinsicDeviceScaleFactor(float scaleFactor)
{
    if (m_intrinsicDeviceScaleFactor == scaleFactor)
        return;

    m_intrinsicDeviceScaleFactor = scaleFactor;

    if (m_drawingArea)
        m_drawingArea->deviceScaleFactorDidChange();
}

void WebPageProxy::windowScreenDidChange(PlatformDisplayID displayID)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::WindowScreenDidChange(displayID), m_pageID);
}

float WebPageProxy::deviceScaleFactor() const
{
    return m_customDeviceScaleFactor.value_or(m_intrinsicDeviceScaleFactor);
}

void WebPageProxy::setCustomDeviceScaleFactor(float customScaleFactor)
{
    if (!isValid())
        return;

    // FIXME: Remove this once we bump cairo requirements to support HiDPI.
    // https://bugs.webkit.org/show_bug.cgi?id=133378
#if USE(CAIRO) && !HAVE(CAIRO_SURFACE_SET_DEVICE_SCALE)
    return;
#endif

    if (m_customDeviceScaleFactor && m_customDeviceScaleFactor.value() == customScaleFactor)
        return;

    float oldScaleFactor = deviceScaleFactor();

    // A value of 0 clears the customScaleFactor.
    if (customScaleFactor)
        m_customDeviceScaleFactor = customScaleFactor;
    else
        m_customDeviceScaleFactor = std::nullopt;

    if (deviceScaleFactor() != oldScaleFactor)
        m_drawingArea->deviceScaleFactorDidChange();
}

void WebPageProxy::accessibilitySettingsDidChange()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::AccessibilitySettingsDidChange(), m_pageID);
}

void WebPageProxy::setUseFixedLayout(bool fixed)
{
    if (!isValid())
        return;

    // This check is fine as the value is initialized in the web
    // process as part of the creation parameters.
    if (fixed == m_useFixedLayout)
        return;

    m_useFixedLayout = fixed;
    if (!fixed)
        m_fixedLayoutSize = IntSize();
    m_process->send(Messages::WebPage::SetUseFixedLayout(fixed), m_pageID);
}

void WebPageProxy::setFixedLayoutSize(const IntSize& size)
{
    if (!isValid())
        return;

    if (size == m_fixedLayoutSize)
        return;

    m_fixedLayoutSize = size;
    m_process->send(Messages::WebPage::SetFixedLayoutSize(size), m_pageID);
}

void WebPageProxy::setAlwaysShowsHorizontalScroller(bool alwaysShowsHorizontalScroller)
{
    if (!isValid())
        return;

    if (alwaysShowsHorizontalScroller == m_alwaysShowsHorizontalScroller)
        return;

    m_alwaysShowsHorizontalScroller = alwaysShowsHorizontalScroller;
    m_process->send(Messages::WebPage::SetAlwaysShowsHorizontalScroller(alwaysShowsHorizontalScroller), m_pageID);
}

void WebPageProxy::setAlwaysShowsVerticalScroller(bool alwaysShowsVerticalScroller)
{
    if (!isValid())
        return;

    if (alwaysShowsVerticalScroller == m_alwaysShowsVerticalScroller)
        return;

    m_alwaysShowsVerticalScroller = alwaysShowsVerticalScroller;
    m_process->send(Messages::WebPage::SetAlwaysShowsVerticalScroller(alwaysShowsVerticalScroller), m_pageID);
}

void WebPageProxy::listenForLayoutMilestones(WebCore::LayoutMilestones milestones)
{
    if (!isValid())
        return;
    
    if (milestones == m_observedLayoutMilestones)
        return;

    m_observedLayoutMilestones = milestones;
    m_process->send(Messages::WebPage::ListenForLayoutMilestones(milestones), m_pageID);
}

void WebPageProxy::setSuppressScrollbarAnimations(bool suppressAnimations)
{
    if (!isValid())
        return;

    if (suppressAnimations == m_suppressScrollbarAnimations)
        return;

    m_suppressScrollbarAnimations = suppressAnimations;
    m_process->send(Messages::WebPage::SetSuppressScrollbarAnimations(suppressAnimations), m_pageID);
}

bool WebPageProxy::rubberBandsAtLeft() const
{
    return m_rubberBandsAtLeft;
}

void WebPageProxy::setRubberBandsAtLeft(bool rubberBandsAtLeft)
{
    m_rubberBandsAtLeft = rubberBandsAtLeft;
}

bool WebPageProxy::rubberBandsAtRight() const
{
    return m_rubberBandsAtRight;
}

void WebPageProxy::setRubberBandsAtRight(bool rubberBandsAtRight)
{
    m_rubberBandsAtRight = rubberBandsAtRight;
}

bool WebPageProxy::rubberBandsAtTop() const
{
    return m_rubberBandsAtTop;
}

void WebPageProxy::setRubberBandsAtTop(bool rubberBandsAtTop)
{
    m_rubberBandsAtTop = rubberBandsAtTop;
}

bool WebPageProxy::rubberBandsAtBottom() const
{
    return m_rubberBandsAtBottom;
}

void WebPageProxy::setRubberBandsAtBottom(bool rubberBandsAtBottom)
{
    m_rubberBandsAtBottom = rubberBandsAtBottom;
}
    
void WebPageProxy::setEnableVerticalRubberBanding(bool enableVerticalRubberBanding)
{
    if (enableVerticalRubberBanding == m_enableVerticalRubberBanding)
        return;

    m_enableVerticalRubberBanding = enableVerticalRubberBanding;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetEnableVerticalRubberBanding(enableVerticalRubberBanding), m_pageID);
}
    
bool WebPageProxy::verticalRubberBandingIsEnabled() const
{
    return m_enableVerticalRubberBanding;
}
    
void WebPageProxy::setEnableHorizontalRubberBanding(bool enableHorizontalRubberBanding)
{
    if (enableHorizontalRubberBanding == m_enableHorizontalRubberBanding)
        return;

    m_enableHorizontalRubberBanding = enableHorizontalRubberBanding;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetEnableHorizontalRubberBanding(enableHorizontalRubberBanding), m_pageID);
}
    
bool WebPageProxy::horizontalRubberBandingIsEnabled() const
{
    return m_enableHorizontalRubberBanding;
}

void WebPageProxy::setBackgroundExtendsBeyondPage(bool backgroundExtendsBeyondPage)
{
    if (backgroundExtendsBeyondPage == m_backgroundExtendsBeyondPage)
        return;

    m_backgroundExtendsBeyondPage = backgroundExtendsBeyondPage;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetBackgroundExtendsBeyondPage(backgroundExtendsBeyondPage), m_pageID);
}

bool WebPageProxy::backgroundExtendsBeyondPage() const
{
    return m_backgroundExtendsBeyondPage;
}

void WebPageProxy::setPaginationMode(WebCore::Pagination::Mode mode)
{
    if (mode == m_paginationMode)
        return;

    m_paginationMode = mode;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetPaginationMode(mode), m_pageID);
}

void WebPageProxy::setPaginationBehavesLikeColumns(bool behavesLikeColumns)
{
    if (behavesLikeColumns == m_paginationBehavesLikeColumns)
        return;

    m_paginationBehavesLikeColumns = behavesLikeColumns;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetPaginationBehavesLikeColumns(behavesLikeColumns), m_pageID);
}

void WebPageProxy::setPageLength(double pageLength)
{
    if (pageLength == m_pageLength)
        return;

    m_pageLength = pageLength;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetPageLength(pageLength), m_pageID);
}

void WebPageProxy::setGapBetweenPages(double gap)
{
    if (gap == m_gapBetweenPages)
        return;

    m_gapBetweenPages = gap;

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetGapBetweenPages(gap), m_pageID);
}

void WebPageProxy::setPaginationLineGridEnabled(bool lineGridEnabled)
{
    if (lineGridEnabled == m_paginationLineGridEnabled)
        return;
    
    m_paginationLineGridEnabled = lineGridEnabled;
    
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetPaginationLineGridEnabled(lineGridEnabled), m_pageID);
}

void WebPageProxy::pageScaleFactorDidChange(double scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
}

void WebPageProxy::pluginScaleFactorDidChange(double pluginScaleFactor)
{
    m_pluginScaleFactor = pluginScaleFactor;
}

void WebPageProxy::pluginZoomFactorDidChange(double pluginZoomFactor)
{
    m_pluginZoomFactor = pluginZoomFactor;
}

void WebPageProxy::findStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    if (string.isEmpty()) {
        didFindStringMatches(string, Vector<Vector<WebCore::IntRect>> (), 0);
        return;
    }

    m_process->send(Messages::WebPage::FindStringMatches(string, options, maxMatchCount), m_pageID);
}

void WebPageProxy::findString(const String& string, FindOptions options, unsigned maxMatchCount)
{
    m_process->send(Messages::WebPage::FindString(string, options, maxMatchCount), m_pageID);
}

void WebPageProxy::getImageForFindMatch(int32_t matchIndex)
{
    m_process->send(Messages::WebPage::GetImageForFindMatch(matchIndex), m_pageID);
}

void WebPageProxy::selectFindMatch(int32_t matchIndex)
{
    m_process->send(Messages::WebPage::SelectFindMatch(matchIndex), m_pageID);
}

void WebPageProxy::hideFindUI()
{
    m_process->send(Messages::WebPage::HideFindUI(), m_pageID);
}

void WebPageProxy::countStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::CountStringMatches(string, options, maxMatchCount), m_pageID);
}

void WebPageProxy::runJavaScriptInMainFrame(const String& script, bool forceUserGesture, WTF::Function<void (API::SerializedScriptValue*, bool hadException, const ExceptionDetails&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, false, { }, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::RunJavaScriptInMainFrame(script, forceUserGesture, callbackID), m_pageID);
}

void WebPageProxy::getRenderTreeExternalRepresentation(WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetRenderTreeExternalRepresentation(callbackID), m_pageID);
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetSourceForFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getContentsAsString(WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::getBytecodeProfile(WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetBytecodeProfile(callbackID), m_pageID);
}

void WebPageProxy::getSamplingProfilerOutput(WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetSamplingProfilerOutput(callbackID), m_pageID);
}

void WebPageProxy::isWebProcessResponsive(WTF::Function<void (bool isWebProcessResponsive)>&& callbackFunction)
{
    if (!isValid()) {
        RunLoop::main().dispatch([callbackFunction = WTFMove(callbackFunction)] {
            bool isWebProcessResponsive = true;
            callbackFunction(isWebProcessResponsive);
        });
        return;
    }

    m_process->isResponsive(WTFMove(callbackFunction));
}

#if ENABLE(MHTML)
void WebPageProxy::getContentsAsMHTMLData(Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetContentsAsMHTMLData(callbackID), m_pageID);
}
#endif

void WebPageProxy::getSelectionOrContentsAsString(WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetSelectionOrContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::getSelectionAsWebArchiveData(Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetSelectionAsWebArchiveData(callbackID), m_pageID);
}

void WebPageProxy::getMainResourceDataOfFrame(WebFrameProxy* frame, Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid() || !frame) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetMainResourceDataOfFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getResourceDataFromFrame(WebFrameProxy* frame, API::URL* resourceURL, Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetResourceDataFromFrame(frame->frameID(), resourceURL->string(), callbackID), m_pageID);
}

void WebPageProxy::getWebArchiveOfFrame(WebFrameProxy* frame, Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetWebArchiveOfFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::forceRepaint(RefPtr<VoidCallback>&& callback)
{
    if (!isValid()) {
        // FIXME: If the page is invalid we should not call the callback. It'd be better to just return false from forceRepaint.
        callback->invalidate(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    Function<void(CallbackBase::Error)> didForceRepaintCallback = [this, callback = WTFMove(callback)](CallbackBase::Error error) mutable {
        if (error != CallbackBase::Error::None) {
            callback->invalidate(error);
            return;
        }

        if (!isValid()) {
            callback->invalidate(CallbackBase::Error::OwnerWasInvalidated);
            return;
        }
    
        callAfterNextPresentationUpdate([callback = WTFMove(callback)](CallbackBase::Error error) {
            if (error != CallbackBase::Error::None) {
                callback->invalidate(error);
                return;
            }

            callback->performCallback();
        });
    };

    auto callbackID = m_callbacks.put(WTFMove(didForceRepaintCallback), m_process->throttler().backgroundActivityToken());
    m_drawingArea->waitForBackingStoreUpdateOnNextPaint();
    m_process->send(Messages::WebPage::ForceRepaint(callbackID), m_pageID); 
}

static OptionSet<IPC::SendOption> printingSendOptions(bool isPerformingDOMPrintOperation)
{
    if (isPerformingDOMPrintOperation)
        return IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply;

    return { };
}

void WebPageProxy::preferencesDidChange()
{
    if (!isValid())
        return;

    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();

    m_pageClient.preferencesDidChange();

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification
    // even if nothing changed in UI process, so that overrides get removed.

    // Preferences need to be updated during synchronous printing to make "print backgrounds" preference work when toggled from a print dialog checkbox.
    m_process->send(Messages::WebPage::PreferencesDidChange(preferencesStore()), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}

void WebPageProxy::didCreateMainFrame(uint64_t frameID)
{
    PageClientProtector protector(m_pageClient);

    MESSAGE_CHECK(!m_mainFrame);
    MESSAGE_CHECK(m_process->canCreateFrame(frameID));

    m_mainFrame = WebFrameProxy::create(this, frameID);

    // Add the frame to the process wide map.
    m_process->frameCreated(frameID, m_mainFrame.get());
}

void WebPageProxy::didCreateSubframe(uint64_t frameID)
{
    PageClientProtector protector(m_pageClient);

    MESSAGE_CHECK(m_mainFrame);
    MESSAGE_CHECK(m_process->canCreateFrame(frameID));
    
    RefPtr<WebFrameProxy> subFrame = WebFrameProxy::create(this, frameID);

    // Add the frame to the process wide map.
    m_process->frameCreated(frameID, subFrame.get());
}

double WebPageProxy::estimatedProgress() const
{
    return m_pageLoadState.estimatedProgress();
}

void WebPageProxy::didStartProgress()
{
    PageClientProtector protector(m_pageClient);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didStartProgress(transaction);

    m_pageLoadState.commitChanges();
    m_loaderClient->didStartProgress(*this);
}

void WebPageProxy::didChangeProgress(double value)
{
    PageClientProtector protector(m_pageClient);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didChangeProgress(transaction, value);

    m_pageLoadState.commitChanges();
    m_loaderClient->didChangeProgress(*this);
}

void WebPageProxy::didFinishProgress()
{
    PageClientProtector protector(m_pageClient);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didFinishProgress(transaction);

    m_pageLoadState.commitChanges();
    m_loaderClient->didFinishProgress(*this);
}

void WebPageProxy::setNetworkRequestsInProgress(bool networkRequestsInProgress)
{
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setNetworkRequestsInProgress(transaction, networkRequestsInProgress);
}

void WebPageProxy::hasInsecureContent(HasInsecureContent& hasInsecureContent)
{
    hasInsecureContent = m_pageLoadState.committedHasInsecureContent() ? HasInsecureContent::Yes : HasInsecureContent::No;
}

void WebPageProxy::didDestroyNavigation(uint64_t navigationID)
{
    PageClientProtector protector(m_pageClient);

    // FIXME: Message check the navigationID.
    m_navigationState->didDestroyNavigation(navigationID);
}

void WebPageProxy::didStartProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, WebCore::URL&& url, WebCore::URL&& unreachableURL, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.clearPendingAPIRequestURL(transaction);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(url);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = &navigationState().navigation(navigationID);

    if (frame->isMainFrame()) {
        m_pageLoadState.didStartProvisionalLoad(transaction, url, unreachableURL);
        m_pageClient.didStartProvisionalLoadForMainFrame();
        hideValidationMessage();
    }

    frame->setUnreachableURL(unreachableURL);
    frame->didStartProvisionalLoad(url);

    m_pageLoadState.commitChanges();
    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didStartProvisionalNavigation(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didStartProvisionalLoadForFrame(*this, *frame, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, WebCore::URL&& url, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(url);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (navigationID) {
        navigation = &navigationState().navigation(navigationID);
        navigation->appendRedirectionURL(url);
    }

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, url);

    frame->didReceiveServerRedirectForProvisionalLoad(url);

    m_pageLoadState.commitChanges();
    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didReceiveServerRedirectForProvisionalNavigation(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didReceiveServerRedirectForProvisionalLoadForFrame(*this, *frame, frame->isMainFrame() ? navigation.get() : nullptr, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::willPerformClientRedirectForFrame(uint64_t frameID, const String& url, double delay)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->willPerformClientRedirect(*this, url, delay);
    }
}

void WebPageProxy::didCancelClientRedirectForFrame(uint64_t frameID)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didCancelClientRedirect(*this);
    }
}

void WebPageProxy::didChangeProvisionalURLForFrame(uint64_t frameID, uint64_t, WebCore::URL&& url)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->frameLoadState().state() == FrameLoadState::State::Provisional);
    MESSAGE_CHECK_URL(url);

    auto transaction = m_pageLoadState.transaction();

    // Internally, we handle this the same way we handle a server redirect. There are no client callbacks
    // for this, but if this is the main frame, clients may observe a change to the page's URL.
    if (frame->isMainFrame())
        m_pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, url);

    frame->didReceiveServerRedirectForProvisionalLoad(url);
}

void WebPageProxy::didFailProvisionalLoadForFrame(uint64_t frameID, const SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, const String& provisionalURL, const ResourceError& error, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().takeNavigation(navigationID);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame()) {
        m_pageLoadState.didFailProvisionalLoad(transaction);
        m_pageClient.didFailProvisionalLoadForMainFrame();
    }

    frame->didFailProvisionalLoad();

    m_pageLoadState.commitChanges();

    ASSERT(!m_failingProvisionalLoadURL);
    m_failingProvisionalLoadURL = provisionalURL;

    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didFailProvisionalNavigationWithError(*this, *frame, navigation.get(), error, m_process->transformHandlesToObjects(userData.object()).get());
        else {
            // FIXME: Get the main frame's current navigation.
            m_navigationClient->didFailProvisionalLoadInSubframeWithError(*this, *frame, frameSecurityOrigin, nullptr, error, m_process->transformHandlesToObjects(userData.object()).get());
        }
    } else
        m_loaderClient->didFailProvisionalLoadWithErrorForFrame(*this, *frame, navigation.get(), error, m_process->transformHandlesToObjects(userData.object()).get());

    m_failingProvisionalLoadURL = { };
}

void WebPageProxy::clearLoadDependentCallbacks()
{
    HashSet<CallbackID> loadDependentStringCallbackIDs = WTFMove(m_loadDependentStringCallbackIDs);
    for (auto& callbackID : loadDependentStringCallbackIDs) {
        if (auto callback = m_callbacks.take<StringCallback>(callbackID))
            callback->invalidate();
    }
}

void WebPageProxy::didCommitLoadForFrame(uint64_t frameID, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, uint32_t opaqueFrameLoadType, const WebCore::CertificateInfo& certificateInfo, bool containsPluginDocument, std::optional<HasInsecureContent> hasInsecureContent, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = &navigationState().navigation(navigationID);

#if PLATFORM(IOS)
    if (frame->isMainFrame()) {
        m_hasReceivedLayerTreeTransactionAfterDidCommitLoad = false;
        m_firstLayerTreeTransactionIdAfterDidCommitLoad = downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea()).nextLayerTreeTransactionID();
    }
#endif

    auto transaction = m_pageLoadState.transaction();
    Ref<WebCertificateInfo> webCertificateInfo = WebCertificateInfo::create(certificateInfo);
    bool markPageInsecure = hasInsecureContent ? hasInsecureContent.value() == HasInsecureContent::Yes : m_treatsSHA1CertificatesAsInsecure && certificateInfo.containsNonRootSHA1SignedCertificate();

    if (frame->isMainFrame()) {
        m_pageLoadState.didCommitLoad(transaction, webCertificateInfo, markPageInsecure);
        m_suppressAutomaticNavigationSnapshotting = false;
    } else if (markPageInsecure)
        m_pageLoadState.didDisplayOrRunInsecureContent(transaction);

#if USE(APPKIT)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME: A load going on in one frame shouldn't affect text editing in other frames on the page.
    m_pageClient.resetSecureInputState();
#endif

    clearLoadDependentCallbacks();

    frame->didCommitLoad(mimeType, webCertificateInfo, containsPluginDocument);

    if (frame->isMainFrame()) {
        m_mainFrameHasCustomContentProvider = frameHasCustomContentProvider;

        if (m_mainFrameHasCustomContentProvider) {
            // Always assume that the main frame is pinned here, since the custom representation view will handle
            // any wheel events and dispatch them to the WKView when necessary.
            m_mainFrameIsPinnedToLeftSide = true;
            m_mainFrameIsPinnedToRightSide = true;
            m_mainFrameIsPinnedToTopSide = true;
            m_mainFrameIsPinnedToBottomSide = true;

            m_uiClient->pinnedStateDidChange(*this);
        }
        m_pageClient.didCommitLoadForMainFrame(mimeType, frameHasCustomContentProvider);
    }

    // Even if WebPage has the default pageScaleFactor (and therefore doesn't reset it),
    // WebPageProxy's cache of the value can get out of sync (e.g. in the case where a
    // plugin is handling page scaling itself) so we should reset it to the default
    // for standard main frame loads.
    if (frame->isMainFrame()) {
        if (static_cast<FrameLoadType>(opaqueFrameLoadType) == FrameLoadType::Standard) {
            m_pageScaleFactor = 1;
            m_pluginScaleFactor = 1;
            m_mainFramePluginHandlesPageScaleGesture = false;
        }
    }

#if ENABLE(POINTER_LOCK)
    if (frame->isMainFrame())
        requestPointerUnlock();
#endif

    m_pageLoadState.commitChanges();
    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didCommitNavigation(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didCommitLoadForFrame(*this, *frame, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didFinishDocumentLoadForFrame(uint64_t frameID, uint64_t navigationID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->documentLoadedForFrame(*frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = &navigationState().navigation(navigationID);

    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didFinishDocumentLoad(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didFinishDocumentLoadForFrame(*this, *frame, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didFinishLoadForFrame(uint64_t frameID, uint64_t navigationID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = &navigationState().navigation(navigationID);

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        m_pageLoadState.didFinishLoad(transaction);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    frame->didFinishLoad();

    m_pageLoadState.commitChanges();
    if (m_navigationClient) {
        if (isMainFrame)
            m_navigationClient->didFinishNavigation(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didFinishLoadForFrame(*this, *frame, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());

    if (isMainFrame)
        m_pageClient.didFinishLoadForMainFrame();

    m_isLoadingAlternateHTMLStringForFailingProvisionalLoad = false;
}

void WebPageProxy::didFailLoadForFrame(uint64_t frameID, uint64_t navigationID, const ResourceError& error, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = &navigationState().navigation(navigationID);

    clearLoadDependentCallbacks();

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();

    if (isMainFrame)
        m_pageLoadState.didFailLoad(transaction);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    frame->didFailLoad();

    m_pageLoadState.commitChanges();
    if (m_navigationClient) {
        if (frame->isMainFrame())
            m_navigationClient->didFailNavigationWithError(*this, *frame, navigation.get(), error, m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didFailLoadWithErrorForFrame(*this, *frame, navigation.get(), error, m_process->transformHandlesToObjects(userData.object()).get());

    if (isMainFrame)
        m_pageClient.didFailLoadForMainFrame();
}

void WebPageProxy::didSameDocumentNavigationForFrame(uint64_t frameID, uint64_t navigationID, uint32_t opaqueSameDocumentNavigationType, WebCore::URL&& url, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(url);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = &navigationState().navigation(navigationID);

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        m_pageLoadState.didSameDocumentNavigation(transaction, url);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    m_pageLoadState.clearPendingAPIRequestURL(transaction);
    frame->didSameDocumentNavigation(url);

    m_pageLoadState.commitChanges();

    SameDocumentNavigationType navigationType = static_cast<SameDocumentNavigationType>(opaqueSameDocumentNavigationType);
    if (m_navigationClient) {
        if (isMainFrame)
            m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_loaderClient->didSameDocumentNavigationForFrame(*this, *frame, navigation.get(), navigationType, m_process->transformHandlesToObjects(userData.object()).get());

    if (isMainFrame)
        m_pageClient.didSameDocumentNavigationForMainFrame(navigationType);
}

void WebPageProxy::didChangeMainDocument(uint64_t frameID)
{
#if ENABLE(MEDIA_STREAM)
    userMediaPermissionRequestManager().resetAccess(frameID);
#else
    UNUSED_PARAM(frameID);
#endif
}

void WebPageProxy::viewIsBecomingVisible()
{
#if ENABLE(MEDIA_STREAM)
    userMediaPermissionRequestManager().processPregrantedRequests();
#endif
}

void WebPageProxy::didReceiveTitleForFrame(uint64_t frameID, const String& title, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.setTitle(transaction, title);

    frame->didChangeTitle(title);
    
    m_pageLoadState.commitChanges();
    m_loaderClient->didReceiveTitleForFrame(*this, title, *frame, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didFirstLayoutForFrame(uint64_t frameID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didFirstLayoutForFrame(*this, *frame, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didFirstVisuallyNonEmptyLayoutForFrame(*this, *frame, m_process->transformHandlesToObjects(userData.object()).get());

    if (frame->isMainFrame())
        m_pageClient.didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void WebPageProxy::didLayoutForCustomContentProvider()
{
    didReachLayoutMilestone(DidFirstLayout | DidFirstVisuallyNonEmptyLayout | DidHitRelevantRepaintedObjectsAreaThreshold);
}

void WebPageProxy::didReachLayoutMilestone(uint32_t layoutMilestones)
{
    PageClientProtector protector(m_pageClient);

    if (m_navigationClient)
        m_navigationClient->renderingProgressDidChange(*this, static_cast<LayoutMilestones>(layoutMilestones));
    else
        m_loaderClient->didReachLayoutMilestone(*this, static_cast<LayoutMilestones>(layoutMilestones));
}

void WebPageProxy::didDisplayInsecureContentForFrame(uint64_t frameID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didDisplayOrRunInsecureContent(transaction);
    m_pageLoadState.commitChanges();

    if (m_navigationClient)
        m_navigationClient->didDisplayInsecureContent(*this, m_process->transformHandlesToObjects(userData.object()).get());
    else
        m_loaderClient->didDisplayInsecureContentForFrame(*this, *frame, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didRunInsecureContentForFrame(uint64_t frameID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didDisplayOrRunInsecureContent(transaction);
    m_pageLoadState.commitChanges();

    if (m_navigationClient)
        m_navigationClient->didRunInsecureContent(*this, m_process->transformHandlesToObjects(userData.object()).get());
    else
        m_loaderClient->didRunInsecureContentForFrame(*this, *frame, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didDetectXSSForFrame(uint64_t frameID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didDetectXSSForFrame(*this, *frame, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::mainFramePluginHandlesPageScaleGestureDidChange(bool mainFramePluginHandlesPageScaleGesture)
{
    m_mainFramePluginHandlesPageScaleGesture = mainFramePluginHandlesPageScaleGesture;
}

void WebPageProxy::frameDidBecomeFrameSet(uint64_t frameID, bool value)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->setIsFrameSet(value);
    if (frame->isMainFrame())
        m_frameSetLargestFrame = value ? m_mainFrame : 0;
}

void WebPageProxy::decidePolicyForNavigationAction(uint64_t frameID, const SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, NavigationActionData&& navigationActionData, const FrameInfoData& originatingFrameInfoData, uint64_t originatingPageID, const WebCore::ResourceRequest& originalRequest, ResourceRequest&& request, uint64_t listenerID, const UserData& userData, bool& receivedPolicyAction, uint64_t& newNavigationID, WebCore::PolicyAction& policyAction, DownloadID& downloadID, std::optional<WebsitePoliciesData>& websitePolicies)
{
    PageClientProtector protector(m_pageClient);

    auto transaction = m_pageLoadState.transaction();

    bool fromAPI = request.url() == m_pageLoadState.pendingAPIRequestURL();
    if (!fromAPI)
        m_pageLoadState.clearPendingAPIRequestURL(transaction);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(request.url());
    MESSAGE_CHECK_URL(originalRequest.url());
    
    Ref<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!navigationID) {
        auto navigation = m_navigationState->createLoadRequestNavigation(ResourceRequest(request));
        newNavigationID = navigation->navigationID();
        navigation->setWasUserInitiated(!!navigationActionData.userGestureTokenIdentifier);
        navigation->setShouldForceDownload(!navigationActionData.downloadAttribute.isNull());
        listener->setNavigation(WTFMove(navigation));
    } else {
        auto& navigation = m_navigationState->navigation(navigationID);
        navigation.setWasUserInitiated(!!navigationActionData.userGestureTokenIdentifier);
        navigation.setShouldForceDownload(!navigationActionData.downloadAttribute.isNull());
        listener->setNavigation(navigation);
    }

#if ENABLE(CONTENT_FILTERING)
    if (frame->didHandleContentFilterUnblockNavigation(request)) {
        receivedPolicyAction = true;
        policyAction = PolicyAction::Ignore;
        return;
    }
#endif

    ASSERT(!m_inDecidePolicyForNavigationAction);
    
    m_inDecidePolicyForNavigationAction = true;
    m_syncNavigationActionPolicyActionIsValid = false;

    WebFrameProxy* originatingFrame = m_process->webFrame(originatingFrameInfoData.frameID);

    if (m_navigationClient) {
        auto destinationFrameInfo = API::FrameInfo::create(*frame, frameSecurityOrigin.securityOrigin());
        RefPtr<API::FrameInfo> sourceFrameInfo;
        if (!fromAPI && originatingFrame == frame)
            sourceFrameInfo = destinationFrameInfo.copyRef();
        else if (!fromAPI)
            sourceFrameInfo = API::FrameInfo::create(originatingFrameInfoData, originatingPageID ? m_process->webPage(originatingPageID) : nullptr);

        auto userInitiatedActivity = m_process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
        bool shouldOpenAppLinks = !m_shouldSuppressAppLinksInNextNavigationPolicyDecision && destinationFrameInfo->isMainFrame() && !hostsAreEqual(URL(ParsedURLString, m_mainFrame->url()), request.url()) && navigationActionData.navigationType != WebCore::NavigationType::BackForward;

        auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), destinationFrameInfo.ptr(), std::nullopt, WTFMove(request), originalRequest.url(), shouldOpenAppLinks, WTFMove(userInitiatedActivity));

        m_navigationClient->decidePolicyForNavigationAction(*this, WTFMove(navigationAction), WTFMove(listener), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_policyClient->decidePolicyForNavigationAction(*this, frame, WTFMove(navigationActionData), originatingFrame, originalRequest, WTFMove(request), WTFMove(listener), m_process->transformHandlesToObjects(userData.object()).get());

    m_shouldSuppressAppLinksInNextNavigationPolicyDecision = false;
    m_inDecidePolicyForNavigationAction = false;

    // Check if we received a policy decision already. If we did, we can just pass it back.
    receivedPolicyAction = m_syncNavigationActionPolicyActionIsValid;
    if (m_syncNavigationActionPolicyActionIsValid) {
        policyAction = m_syncNavigationActionPolicyAction;
        downloadID = m_syncNavigationActionPolicyDownloadID;
        websitePolicies = WTFMove(m_syncNavigationActionPolicyWebsitePolicies);
    }
}

void WebPageProxy::decidePolicyForNewWindowAction(uint64_t frameID, const SecurityOriginData& frameSecurityOrigin, NavigationActionData&& navigationActionData, ResourceRequest&& request, const String& frameName, uint64_t listenerID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(request.url());

    Ref<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);

    if (m_navigationClient) {
        RefPtr<API::FrameInfo> sourceFrameInfo;
        if (frame)
            sourceFrameInfo = API::FrameInfo::create(*frame, frameSecurityOrigin.securityOrigin());

        auto userInitiatedActivity = m_process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
        bool shouldOpenAppLinks = !hostsAreEqual(URL(ParsedURLString, m_mainFrame->url()), request.url());
        auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), nullptr, frameName, WTFMove(request), { }, shouldOpenAppLinks, WTFMove(userInitiatedActivity));

        m_navigationClient->decidePolicyForNavigationAction(*this, navigationAction.get(), WTFMove(listener), m_process->transformHandlesToObjects(userData.object()).get());

    } else
        m_policyClient->decidePolicyForNewWindowAction(*this, *frame, navigationActionData, request, frameName, WTFMove(listener), m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::decidePolicyForResponse(uint64_t frameID, const SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, uint64_t listenerID, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(request.url());
    MESSAGE_CHECK_URL(response.url());

    Ref<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (navigationID)
        listener->setNavigation(m_navigationState->navigation(navigationID));

    if (m_navigationClient) {
        auto navigationResponse = API::NavigationResponse::create(API::FrameInfo::create(*frame, frameSecurityOrigin.securityOrigin()).get(), request, response, canShowMIMEType);
        m_navigationClient->decidePolicyForNavigationResponse(*this, WTFMove(navigationResponse), WTFMove(listener), m_process->transformHandlesToObjects(userData.object()).get());
    } else
        m_policyClient->decidePolicyForResponse(*this, *frame, response, request, canShowMIMEType, WTFMove(listener), m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::decidePolicyForResponseSync(uint64_t frameID, const SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, uint64_t listenerID, const UserData& userData, bool& receivedPolicyAction, WebCore::PolicyAction& policyAction, DownloadID& downloadID)
{
    PageClientProtector protector(m_pageClient);

    ASSERT(!m_inDecidePolicyForResponseSync);

    m_inDecidePolicyForResponseSync = true;
    m_decidePolicyForResponseRequest = &request;
    m_syncMimeTypePolicyActionIsValid = false;
    
    decidePolicyForResponse(frameID, frameSecurityOrigin, navigationID, response, request, canShowMIMEType, listenerID, userData);
    
    m_inDecidePolicyForResponseSync = false;
    m_decidePolicyForResponseRequest = nullptr;
    
    // Check if we received a policy decision already. If we did, we can just pass it back.
    receivedPolicyAction = m_syncMimeTypePolicyActionIsValid;
    if (m_syncMimeTypePolicyActionIsValid) {
        policyAction = m_syncMimeTypePolicyAction;
        downloadID = m_syncMimeTypePolicyDownloadID;
    }
}

void WebPageProxy::unableToImplementPolicy(uint64_t frameID, const ResourceError& error, const UserData& userData)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_policyClient->unableToImplementPolicy(*this, *frame, error, m_process->transformHandlesToObjects(userData.object()).get());
}

// FormClient

void WebPageProxy::willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const Vector<std::pair<String, String>>& textFieldValues, uint64_t listenerID, const UserData& userData)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    WebFrameProxy* sourceFrame = m_process->webFrame(sourceFrameID);
    MESSAGE_CHECK(sourceFrame);

    m_formClient->willSubmitForm(*this, *frame, *sourceFrame, textFieldValues, m_process->transformHandlesToObjects(userData.object()).get(), [this, protectedThis = makeRef(*this), frameID, listenerID]() {
        m_process->send(Messages::WebPage::ContinueWillSubmitForm(frameID, listenerID), m_pageID);
    });
}

void WebPageProxy::contentRuleListNotification(WebCore::URL&& url, Vector<String>&& identifiers, Vector<String>&& notifications)
{
    if (m_navigationClient)
        m_navigationClient->contentRuleListNotification(*this, WTFMove(url), WTFMove(identifiers), WTFMove(notifications));
}
    
void WebPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, uint64_t frameID) 
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == this);

    if (m_historyClient) {
        if (frame->isMainFrame())
            m_historyClient->didNavigateWithNavigationData(*this, store);
    } else
        m_loaderClient->didNavigateWithNavigationData(*this, store, *frame);
    process().processPool().historyClient().didNavigateWithNavigationData(process().processPool(), *this, store, *frame);
}

void WebPageProxy::didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    PageClientProtector protector(m_pageClient);

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == this);

    MESSAGE_CHECK_URL(sourceURLString);
    MESSAGE_CHECK_URL(destinationURLString);

    if (m_historyClient) {
        if (frame->isMainFrame())
            m_historyClient->didPerformClientRedirect(*this, sourceURLString, destinationURLString);
    } else
        m_loaderClient->didPerformClientRedirect(*this, sourceURLString, destinationURLString, *frame);
    process().processPool().historyClient().didPerformClientRedirect(process().processPool(), *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    PageClientProtector protector(m_pageClient);

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == this);

    MESSAGE_CHECK_URL(sourceURLString);
    MESSAGE_CHECK_URL(destinationURLString);

    if (m_historyClient) {
        if (frame->isMainFrame())
            m_historyClient->didPerformServerRedirect(*this, sourceURLString, destinationURLString);
    } else
        m_loaderClient->didPerformServerRedirect(*this, sourceURLString, destinationURLString, *frame);
    process().processPool().historyClient().didPerformServerRedirect(process().processPool(), *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didUpdateHistoryTitle(const String& title, const String& url, uint64_t frameID)
{
    PageClientProtector protector(m_pageClient);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == this);

    MESSAGE_CHECK_URL(url);

    if (m_historyClient) {
        if (frame->isMainFrame())
            m_historyClient->didUpdateHistoryTitle(*this, title, url);
    } else
        m_loaderClient->didUpdateHistoryTitle(*this, title, url, *frame);
    process().processPool().historyClient().didUpdateHistoryTitle(process().processPool(), *this, title, url, *frame);
}

// UIClient

void WebPageProxy::createNewPage(const FrameInfoData& originatingFrameInfoData, uint64_t originatingPageID, ResourceRequest&& request, WindowFeatures&& windowFeatures, NavigationActionData&& navigationActionData, Ref<Messages::WebPageProxy::CreateNewPage::DelayedReply>&& reply)
{
    MESSAGE_CHECK(m_process->webFrame(originatingFrameInfoData.frameID));
    auto originatingFrameInfo = API::FrameInfo::create(originatingFrameInfoData, m_process->webPage(originatingPageID));

    auto mainFrameURL = m_mainFrame->url();

    m_uiClient->createNewPage(*this, WTFMove(originatingFrameInfo), WTFMove(request), WTFMove(windowFeatures), WTFMove(navigationActionData), [this, protectedThis = RefPtr<WebPageProxy>(this), mainFrameURL, request, reply = WTFMove(reply)](RefPtr<WebPageProxy> newPage) {
        if (!newPage) {
            reply->send(0, { });
            return;
        }

        reply->send(newPage->pageID(), newPage->creationParameters());

        WebsiteDataStore::cloneSessionData(*this, *newPage);
        newPage->m_shouldSuppressAppLinksInNextNavigationPolicyDecision = hostsAreEqual(URL(ParsedURLString, mainFrameURL), request.url());
    });
}
    
void WebPageProxy::showPage()
{
    m_uiClient->showPage(this);
}

void WebPageProxy::fullscreenMayReturnToInline()
{
    m_uiClient->fullscreenMayReturnToInline(this);
}

void WebPageProxy::didEnterFullscreen()
{
    m_uiClient->didEnterFullscreen(this);
}

void WebPageProxy::didExitFullscreen()
{
    m_uiClient->didExitFullscreen(this);
}

void WebPageProxy::closePage(bool stopResponsivenessTimer)
{
    if (stopResponsivenessTimer)
        m_process->responsivenessTimer().stop();

    m_pageClient.clearAllEditCommands();
    m_uiClient->close(this);
}

void WebPageProxy::runJavaScriptAlert(uint64_t frameID, const SecurityOriginData& securityOrigin, const String& message, Ref<Messages::WebPageProxy::RunJavaScriptAlert::DelayedReply>&& reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptAlert() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }
    m_uiClient->runJavaScriptAlert(this, message, frame, securityOrigin, [reply = WTFMove(reply)] {
        reply->send();
    });
}

void WebPageProxy::runJavaScriptConfirm(uint64_t frameID, const SecurityOriginData& securityOrigin, const String& message, Ref<Messages::WebPageProxy::RunJavaScriptConfirm::DelayedReply>&& reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptConfirm() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }

    m_uiClient->runJavaScriptConfirm(this, message, frame, securityOrigin, [reply = WTFMove(reply)](bool result) {
        reply->send(result);
    });
}

void WebPageProxy::runJavaScriptPrompt(uint64_t frameID, const SecurityOriginData& securityOrigin, const String& message, const String& defaultValue, Ref<Messages::WebPageProxy::RunJavaScriptPrompt::DelayedReply>&& reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptPrompt() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }

    m_uiClient->runJavaScriptPrompt(this, message, defaultValue, frame, securityOrigin, [reply = WTFMove(reply)](const String& result) {
        reply->send(result);
    });
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient->setStatusText(this, text);
}

void WebPageProxy::mouseDidMoveOverElement(WebHitTestResultData&& hitTestResultData, uint32_t opaqueModifiers, UserData&& userData)
{
    m_lastMouseMoveHitTestResult = API::HitTestResult::create(hitTestResultData);

    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);

    m_uiClient->mouseDidMoveOverElement(*this, hitTestResultData, modifiers, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::connectionWillOpen(IPC::Connection& connection)
{
    ASSERT(&connection == m_process->connection());

    m_webProcessLifetimeTracker.connectionWillOpen(connection);
}

void WebPageProxy::webProcessWillShutDown()
{
    m_webProcessLifetimeTracker.webProcessWillShutDown();
}

void WebPageProxy::processDidFinishLaunching()
{
    ASSERT(m_process->state() == WebProcessProxy::State::Running);
    finishInitializingWebPageAfterProcessLaunch();
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebPageProxy::unavailablePluginButtonClicked(uint32_t opaquePluginUnavailabilityReason, const String& mimeType, const String& pluginURLString, const String& pluginspageAttributeURLString, const String& frameURLString, const String& pageURLString)
{
    MESSAGE_CHECK_URL(pluginURLString);
    MESSAGE_CHECK_URL(pluginspageAttributeURLString);
    MESSAGE_CHECK_URL(frameURLString);
    MESSAGE_CHECK_URL(pageURLString);

    String newMimeType = mimeType;
    PluginModuleInfo plugin = m_process->processPool().pluginInfoStore().findPlugin(newMimeType, URL(URL(), pluginURLString));
    auto pluginInformation = createPluginInformationDictionary(plugin, frameURLString, mimeType, pageURLString, pluginspageAttributeURLString, pluginURLString);

    WKPluginUnavailabilityReason pluginUnavailabilityReason = kWKPluginUnavailabilityReasonPluginMissing;
    switch (static_cast<RenderEmbeddedObject::PluginUnavailabilityReason>(opaquePluginUnavailabilityReason)) {
    case RenderEmbeddedObject::PluginMissing:
        pluginUnavailabilityReason = kWKPluginUnavailabilityReasonPluginMissing;
        break;
    case RenderEmbeddedObject::InsecurePluginVersion:
        pluginUnavailabilityReason = kWKPluginUnavailabilityReasonInsecurePluginVersion;
        break;
    case RenderEmbeddedObject::PluginCrashed:
        pluginUnavailabilityReason = kWKPluginUnavailabilityReasonPluginCrashed;
        break;
    case RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy:
        ASSERT_NOT_REACHED();
    }

    m_uiClient->unavailablePluginButtonClicked(*this, pluginUnavailabilityReason, pluginInformation.get());
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
void WebPageProxy::webGLPolicyForURL(URL&& url, Ref<Messages::WebPageProxy::WebGLPolicyForURL::DelayedReply>&& reply)
{
    if (m_navigationClient) {
        m_navigationClient->webGLLoadPolicy(*this, url, [reply = WTFMove(reply)](WebGLLoadPolicy policy) {
            reply->send(static_cast<uint32_t>(policy));
        });
    } else
        reply->send(static_cast<uint32_t>(m_loaderClient->webGLLoadPolicy(*this, url)));
}

void WebPageProxy::resolveWebGLPolicyForURL(URL&& url, Ref<Messages::WebPageProxy::ResolveWebGLPolicyForURL::DelayedReply>&& reply)
{
    if (m_navigationClient) {
        m_navigationClient->resolveWebGLLoadPolicy(*this, url, [reply = WTFMove(reply)](WebGLLoadPolicy policy) {
            reply->send(static_cast<uint32_t>(policy));
        });
    } else
        reply->send(static_cast<uint32_t>(m_loaderClient->resolveWebGLLoadPolicy(*this, url)));
}
#endif // ENABLE(WEBGL)

void WebPageProxy::setToolbarsAreVisible(bool toolbarsAreVisible)
{
    m_uiClient->setToolbarsAreVisible(*this, toolbarsAreVisible);
}

void WebPageProxy::getToolbarsAreVisible(Ref<Messages::WebPageProxy::GetToolbarsAreVisible::DelayedReply>&& reply)
{
    m_uiClient->toolbarsAreVisible(*this, [reply = WTFMove(reply)](bool visible) {
        reply->send(visible);
    });
}

void WebPageProxy::setMenuBarIsVisible(bool menuBarIsVisible)
{
    m_uiClient->setMenuBarIsVisible(*this, menuBarIsVisible);
}

void WebPageProxy::getMenuBarIsVisible(Ref<Messages::WebPageProxy::GetMenuBarIsVisible::DelayedReply>&& reply)
{
    m_uiClient->menuBarIsVisible(*this, [reply = WTFMove(reply)] (bool visible) {
        reply->send(visible);
    });
}

void WebPageProxy::setStatusBarIsVisible(bool statusBarIsVisible)
{
    m_uiClient->setStatusBarIsVisible(*this, statusBarIsVisible);
}

void WebPageProxy::getStatusBarIsVisible(Ref<Messages::WebPageProxy::GetStatusBarIsVisible::DelayedReply>&& reply)
{
    m_uiClient->statusBarIsVisible(*this, [reply = WTFMove(reply)] (bool visible) {
        reply->send(visible);
    });
}

void WebPageProxy::setIsResizable(bool isResizable)
{
    m_uiClient->setIsResizable(*this, isResizable);
}

void WebPageProxy::setWindowFrame(const FloatRect& newWindowFrame)
{
    m_uiClient->setWindowFrame(*this, m_pageClient.convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(Ref<Messages::WebPageProxy::GetWindowFrame::DelayedReply>&& reply)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = makeRef(*this), reply = WTFMove(reply)] (FloatRect frame) {
        reply->send(m_pageClient.convertToUserSpace(frame));
    });
}

void WebPageProxy::getWindowFrameWithCallback(Function<void(FloatRect)>&& completionHandler)
{
    m_uiClient->windowFrame(*this, WTFMove(completionHandler));
}

void WebPageProxy::screenToRootView(const IntPoint& screenPoint, Ref<Messages::WebPageProxy::ScreenToRootView::DelayedReply>&& reply)
{
    reply->send(m_pageClient.screenToRootView(screenPoint));
}
    
void WebPageProxy::rootViewToScreen(const IntRect& viewRect, Ref<Messages::WebPageProxy::RootViewToScreen::DelayedReply>&& reply)
{
    reply->send(m_pageClient.rootViewToScreen(viewRect));
}
    
#if PLATFORM(IOS)
void WebPageProxy::accessibilityScreenToRootView(const IntPoint& screenPoint, IntPoint& windowPoint)
{
    windowPoint = m_pageClient.accessibilityScreenToRootView(screenPoint);
}

void WebPageProxy::rootViewToAccessibilityScreen(const IntRect& viewRect, IntRect& result)
{
    result = m_pageClient.rootViewToAccessibilityScreen(viewRect);
}
#endif

void WebPageProxy::runBeforeUnloadConfirmPanel(uint64_t frameID, const SecurityOriginData& securityOrigin, const String& message, RefPtr<Messages::WebPageProxy::RunBeforeUnloadConfirmPanel::DelayedReply> reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runBeforeUnloadConfirmPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }

    m_uiClient->runBeforeUnloadConfirmPanel(this, message, frame, securityOrigin, [reply](bool result) { reply->send(result); });
}

void WebPageProxy::didChangeViewportProperties(const ViewportAttributes& attr)
{
    m_pageClient.didChangeViewportProperties(attr);
}

void WebPageProxy::pageDidScroll()
{
    m_uiClient->pageDidScroll(this);

#if PLATFORM(IOS)
    // Do not hide the validation message if the scrolling was caused by the keyboard showing up.
    if (m_isKeyboardAnimatingIn)
        return;
#endif
    hideValidationMessage();
}

void WebPageProxy::runOpenPanel(uint64_t frameID, const SecurityOriginData& frameSecurityOrigin, const FileChooserSettings& settings)
{
    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = nullptr;
    }

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    Ref<API::OpenPanelParameters> parameters = API::OpenPanelParameters::create(settings);
    m_openPanelResultListener = WebOpenPanelResultListenerProxy::create(this);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->handleRunOpenPanel(*this, *frame, parameters.get(), *m_openPanelResultListener);

        // Don't show a file chooser, since automation will be unable to interact with it.
        return;
    }

    // Since runOpenPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    if (!m_uiClient->runOpenPanel(this, frame, frameSecurityOrigin, parameters.ptr(), m_openPanelResultListener.get())) {
        if (!m_pageClient.handleRunOpenPanel(this, frame, parameters.ptr(), m_openPanelResultListener.get()))
            didCancelForOpenPanel();
    }
}

void WebPageProxy::printFrame(uint64_t frameID)
{
    ASSERT(!m_isPerformingDOMPrintOperation);
    m_isPerformingDOMPrintOperation = true;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_uiClient->printFrame(*this, *frame);

    endPrinting(); // Send a message synchronously while m_isPerformingDOMPrintOperation is still true.
    m_isPerformingDOMPrintOperation = false;
}

void WebPageProxy::printMainFrame()
{
    printFrame(m_mainFrame->frameID());
}

void WebPageProxy::setMediaVolume(float volume)
{
    if (volume == m_mediaVolume)
        return;
    
    m_mediaVolume = volume;
    
    if (!isValid())
        return;
    
    m_process->send(Messages::WebPage::SetMediaVolume(volume), m_pageID);    
}

void WebPageProxy::setMuted(WebCore::MediaProducer::MutedStateFlags state)
{
    m_mutedState = state;

    if (!isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    bool hasMutedCaptureStreams = m_mediaState & (WebCore::MediaProducer::HasMutedAudioCaptureDevice | WebCore::MediaProducer::HasMutedVideoCaptureDevice);
    if (hasMutedCaptureStreams && !(state & WebCore::MediaProducer::CaptureDevicesAreMuted))
        UserMediaProcessManager::singleton().muteCaptureMediaStreamsExceptIn(*this);
#endif

    m_process->send(Messages::WebPage::SetMuted(state), m_pageID);
    activityStateDidChange(ActivityState::IsAudible | ActivityState::IsCapturingMedia);
}

void WebPageProxy::setMediaCaptureEnabled(bool enabled)
{
    m_mediaCaptureEnabled = enabled;

    if (!isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().setCaptureEnabled(enabled);
#endif
}

void WebPageProxy::stopMediaCapture()
{
    if (!isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    m_process->send(Messages::WebPage::StopMediaCapture(), m_pageID);
#endif
}

#if ENABLE(MEDIA_SESSION)
void WebPageProxy::handleMediaEvent(MediaEventType eventType)
{
    if (!isValid())
        return;
    
    m_process->send(Messages::WebPage::HandleMediaEvent(eventType), m_pageID);
}

void WebPageProxy::setVolumeOfMediaElement(double volume, uint64_t elementID)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetVolumeOfMediaElement(volume, elementID), m_pageID);
}
#endif

void WebPageProxy::setMayStartMediaWhenInWindow(bool mayStartMedia)
{
    if (mayStartMedia == m_mayStartMediaWhenInWindow)
        return;

    m_mayStartMediaWhenInWindow = mayStartMedia;

    if (!isValid())
        return;

    process().send(Messages::WebPage::SetMayStartMediaWhenInWindow(mayStartMedia), m_pageID);
}

void WebPageProxy::handleDownloadRequest(DownloadProxy* download)
{
    m_pageClient.handleDownloadRequest(download);
}

void WebPageProxy::didChangeContentSize(const IntSize& size)
{
    m_pageClient.didChangeContentSize(size);
}

#if ENABLE(INPUT_TYPE_COLOR)
void WebPageProxy::showColorPicker(const WebCore::Color& initialColor, const IntRect& elementRect)
{
#if ENABLE(INPUT_TYPE_COLOR_POPOVER)
    // A new popover color well needs to be created (and the previous one destroyed) for
    // each activation of a color element.
    m_colorPicker = nullptr;
#endif
    if (!m_colorPicker)
        m_colorPicker = m_pageClient.createColorPicker(this, initialColor, elementRect);
    m_colorPicker->showColorPicker(initialColor);
}

void WebPageProxy::setColorPickerColor(const WebCore::Color& color)
{
    ASSERT(m_colorPicker);

    m_colorPicker->setSelectedColor(color);
}

void WebPageProxy::endColorPicker()
{
    ASSERT(m_colorPicker);

    m_colorPicker->endPicker();
}

void WebPageProxy::didChooseColor(const WebCore::Color& color)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::DidChooseColor(color), m_pageID);
}

void WebPageProxy::didEndColorPicker()
{
    if (!isValid())
        return;

#if ENABLE(INPUT_TYPE_COLOR)
    if (m_colorPicker) {
        m_colorPicker->invalidate();
        m_colorPicker = nullptr;
    }
#endif

    m_process->send(Messages::WebPage::DidEndColorPicker(), m_pageID);
}
#endif

WebInspectorProxy* WebPageProxy::inspector() const
{
    if (isClosed() || !isValid())
        return nullptr;
    return m_inspector.get();
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxy* WebPageProxy::fullScreenManager()
{
    return m_fullScreenManager.get();
}

void WebPageProxy::setFullscreenClient(std::unique_ptr<API::FullscreenClient>&& client)
{
    if (!client) {
        m_fullscreenClient = std::make_unique<API::FullscreenClient>();
        return;
    }

    m_fullscreenClient = WTFMove(client);
}
#endif
    
#if (PLATFORM(IOS) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
PlaybackSessionManagerProxy* WebPageProxy::playbackSessionManager()
{
    return m_playbackSessionManager.get();
}

VideoFullscreenManagerProxy* WebPageProxy::videoFullscreenManager()
{
    return m_videoFullscreenManager.get();
}
#endif

#if PLATFORM(IOS)
bool WebPageProxy::allowsMediaDocumentInlinePlayback() const
{
    return m_allowsMediaDocumentInlinePlayback;
}

void WebPageProxy::setAllowsMediaDocumentInlinePlayback(bool allows)
{
    if (m_allowsMediaDocumentInlinePlayback == allows)
        return;
    m_allowsMediaDocumentInlinePlayback = allows;

    m_process->send(Messages::WebPage::SetAllowsMediaDocumentInlinePlayback(allows), m_pageID);
}
#endif

void WebPageProxy::setHasHadSelectionChangesFromUserInteraction(bool hasHadUserSelectionChanges)
{
    m_hasHadSelectionChangesFromUserInteraction = hasHadUserSelectionChanges;
}

void WebPageProxy::setNeedsHiddenContentEditableQuirk(bool needsHiddenContentEditableQuirk)
{
    m_needsHiddenContentEditableQuirk = needsHiddenContentEditableQuirk;
}

void WebPageProxy::setNeedsPlainTextQuirk(bool needsPlainTextQuirk)
{
    m_needsPlainTextQuirk = needsPlainTextQuirk;
}

// BackForwardList

void WebPageProxy::backForwardAddItem(uint64_t itemID)
{
    m_backForwardList->addItem(m_process->webBackForwardItem(itemID));
}

void WebPageProxy::backForwardGoToItem(uint64_t itemID, SandboxExtension::Handle& sandboxExtensionHandle)
{
    WebBackForwardListItem* item = m_process->webBackForwardItem(itemID);
    if (!item)
        return;

    bool createdExtension = maybeInitializeSandboxExtensionHandle(URL(URL(), item->url()), sandboxExtensionHandle);
    if (createdExtension)
        m_process->willAcquireUniversalFileReadSandboxExtension();
    m_backForwardList->goToItem(*item);
}

void WebPageProxy::backForwardItemAtIndex(int32_t index, uint64_t& itemID)
{
    WebBackForwardListItem* item = m_backForwardList->itemAtIndex(index);
    itemID = item ? item->itemID() : 0;
}

void WebPageProxy::backForwardBackListCount(int32_t& count)
{
    count = m_backForwardList->backListCount();
}

void WebPageProxy::backForwardForwardListCount(int32_t& count)
{
    count = m_backForwardList->forwardListCount();
}

void WebPageProxy::compositionWasCanceled()
{
#if PLATFORM(COCOA)
    m_pageClient.notifyInputContextAboutDiscardedComposition();
#endif
}

// Undo management

void WebPageProxy::registerEditCommandForUndo(uint64_t commandID, uint32_t editAction)
{
    registerEditCommand(WebEditCommandProxy::create(commandID, static_cast<EditAction>(editAction), this), Undo);
}
    
void WebPageProxy::registerInsertionUndoGrouping()
{
#if USE(INSERTION_UNDO_GROUPING)
    m_pageClient.registerInsertionUndoGrouping();
#endif
}

void WebPageProxy::canUndoRedo(uint32_t action, bool& result)
{
    result = m_pageClient.canUndoRedo(static_cast<UndoOrRedo>(action));
}

void WebPageProxy::executeUndoRedo(uint32_t action, bool& result)
{
    m_pageClient.executeUndoRedo(static_cast<UndoOrRedo>(action));
    result = true;
}

void WebPageProxy::clearAllEditCommands()
{
    m_pageClient.clearAllEditCommands();
}

void WebPageProxy::didCountStringMatches(const String& string, uint32_t matchCount)
{
    m_findClient->didCountStringMatches(this, string, matchCount);
}

void WebPageProxy::didGetImageForFindMatch(const ShareableBitmap::Handle& contentImageHandle, uint32_t matchIndex)
{
    auto bitmap = ShareableBitmap::create(contentImageHandle);
    if (!bitmap) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_findMatchesClient->didGetImageForMatchResult(this, WebImage::create(bitmap.releaseNonNull()).ptr(), matchIndex);
}

void WebPageProxy::setTextIndicator(const TextIndicatorData& indicatorData, uint64_t lifetime)
{
    // FIXME: Make TextIndicatorWindow a platform-independent presentational thing ("TextIndicatorPresentation"?).
#if PLATFORM(COCOA)
    m_pageClient.setTextIndicator(TextIndicator::create(indicatorData), static_cast<TextIndicatorWindowLifetime>(lifetime));
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebPageProxy::clearTextIndicator()
{
#if PLATFORM(COCOA)
    m_pageClient.clearTextIndicator(TextIndicatorWindowDismissalAnimation::FadeOut);
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebPageProxy::setTextIndicatorAnimationProgress(float progress)
{
#if PLATFORM(COCOA)
    m_pageClient.setTextIndicatorAnimationProgress(progress);
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebPageProxy::didFindString(const String& string, const Vector<WebCore::IntRect>& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrapAround)
{
    m_findClient->didFindString(this, string, matchRects, matchCount, matchIndex, didWrapAround);
}

void WebPageProxy::didFindStringMatches(const String& string, const Vector<Vector<WebCore::IntRect>>& matchRects, int32_t firstIndexAfterSelection)
{
    m_findMatchesClient->didFindStringMatches(this, string, matchRects, firstIndexAfterSelection);
}

void WebPageProxy::didFailToFindString(const String& string)
{
    m_findClient->didFailToFindString(this, string);
}

bool WebPageProxy::sendMessage(std::unique_ptr<IPC::Encoder> encoder, OptionSet<IPC::SendOption> sendOptions)
{
    return m_process->sendMessage(WTFMove(encoder), sendOptions);
}

IPC::Connection* WebPageProxy::messageSenderConnection()
{
    return m_process->connection();
}

uint64_t WebPageProxy::messageSenderDestinationID()
{
    return m_pageID;
}

void WebPageProxy::valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex)
{
    m_process->send(Messages::WebPage::DidChangeSelectedIndexForActivePopupMenu(newSelectedIndex), m_pageID);
}

void WebPageProxy::setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index)
{
    m_process->send(Messages::WebPage::SetTextForActivePopupMenu(index), m_pageID);
}

NativeWebMouseEvent* WebPageProxy::currentlyProcessedMouseDownEvent()
{
    return m_currentlyProcessedMouseDownEvent.get();
}

void WebPageProxy::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    process().send(Messages::WebPage::PostInjectedBundleMessage(messageName, UserData(process().transformObjectsToHandles(messageBody).get())), m_pageID);
}

#if PLATFORM(GTK)
void WebPageProxy::failedToShowPopupMenu()
{
    m_process->send(Messages::WebPage::FailedToShowPopupMenu(), m_pageID);
}
#endif

void WebPageProxy::showPopupMenu(const IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    if (m_activePopupMenu) {
        m_activePopupMenu->hidePopupMenu();
        m_activePopupMenu->invalidate();
        m_activePopupMenu = nullptr;
    }

    m_activePopupMenu = m_pageClient.createPopupMenuProxy(*this);

    if (!m_activePopupMenu)
        return;

    // Since showPopupMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    // Showing a popup menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref<WebPageProxy> protect(*this);
    m_activePopupMenu->showPopupMenu(rect, static_cast<TextDirection>(textDirection), m_pageScaleFactor, items, data, selectedIndex);
}

void WebPageProxy::hidePopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->hidePopupMenu();
    m_activePopupMenu->invalidate();
    m_activePopupMenu = nullptr;
}

#if ENABLE(CONTEXT_MENUS)
void WebPageProxy::showContextMenu(ContextMenuContextData&& contextMenuContextData, const UserData& userData)
{
    // Showing a context menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref<WebPageProxy> protect(*this);

    m_activeContextMenuContextData = contextMenuContextData;

    m_activeContextMenu = m_pageClient.createContextMenuProxy(*this, WTFMove(contextMenuContextData), userData);

    // Since showContextMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    // m_activeContextMenu might get cleared if WebPageProxy code is re-entered from the menu runloop or delegates.
    Ref<WebContextMenuProxy> protector(*m_activeContextMenu);
    m_activeContextMenu->show();

    // No matter the result of internalShowContextMenu, always notify the WebProcess that the menu is hidden so it starts handling mouse events again.
    m_process->send(Messages::WebPage::ContextMenuHidden(), m_pageID);
}

void WebPageProxy::contextMenuItemSelected(const WebContextMenuItemData& item)
{
    // Application custom items don't need to round-trip through to WebCore in the WebProcess.
    if (item.action() >= ContextMenuItemBaseApplicationTag) {
        m_contextMenuClient->customContextMenuItemSelected(*this, item);
        return;
    }

#if PLATFORM(COCOA)
    if (item.action() == ContextMenuItemTagSmartCopyPaste) {
        setSmartInsertDeleteEnabled(!isSmartInsertDeleteEnabled());
        return;
    }
    if (item.action() == ContextMenuItemTagSmartQuotes) {
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
        m_process->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagSmartDashes) {
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
        m_process->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagSmartLinks) {
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
        m_process->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagTextReplacement) {
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
        m_process->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagCorrectSpellingAutomatically) {
        TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);
        m_process->updateTextCheckerState();
        return;        
    }
    if (item.action() == ContextMenuItemTagShowSubstitutions) {
        TextChecker::toggleSubstitutionsPanelIsShowing();
        return;
    }
#endif
    if (item.action() == ContextMenuItemTagDownloadImageToDisk) {
        m_process->processPool().download(this, URL(URL(), m_activeContextMenuContextData.webHitTestResultData().absoluteImageURL));
        return;    
    }
    if (item.action() == ContextMenuItemTagDownloadLinkToDisk) {
        auto& hitTestResult = m_activeContextMenuContextData.webHitTestResultData();
        m_process->processPool().download(this, URL(URL(), hitTestResult.absoluteLinkURL), hitTestResult.linkSuggestedFilename);
        return;
    }
    if (item.action() == ContextMenuItemTagDownloadMediaToDisk) {
        m_process->processPool().download(this, URL(URL(), m_activeContextMenuContextData.webHitTestResultData().absoluteMediaURL));
        return;
    }
    if (item.action() == ContextMenuItemTagCheckSpellingWhileTyping) {
        TextChecker::setContinuousSpellCheckingEnabled(!TextChecker::state().isContinuousSpellCheckingEnabled);
        m_process->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagCheckGrammarWithSpelling) {
        TextChecker::setGrammarCheckingEnabled(!TextChecker::state().isGrammarCheckingEnabled);
        m_process->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagShowSpellingPanel) {
        if (!TextChecker::spellingUIIsShowing())
            advanceToNextMisspelling(true);
        TextChecker::toggleSpellingUIIsShowing();
        return;
    }
    if (item.action() == ContextMenuItemTagLearnSpelling || item.action() == ContextMenuItemTagIgnoreSpelling)
        ++m_pendingLearnOrIgnoreWordMessageCount;

    m_process->send(Messages::WebPage::DidSelectItemFromActiveContextMenu(item), m_pageID);
}

void WebPageProxy::handleContextMenuKeyEvent()
{
    m_process->send(Messages::WebPage::ContextMenuForKeyEvent(), m_pageID);
}
#endif // ENABLE(CONTEXT_MENUS)

#if PLATFORM(IOS)
void WebPageProxy::didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>& fileURLs, const String& displayString, const API::Data* iconData)
{
    if (!isValid())
        return;

#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::HandleArray sandboxExtensionHandles;
    sandboxExtensionHandles.allocate(fileURLs.size());
    for (size_t i = 0; i < fileURLs.size(); ++i)
        SandboxExtension::createHandle(fileURLs[i], SandboxExtension::Type::ReadOnly, sandboxExtensionHandles[i]);

    m_process->send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(sandboxExtensionHandles), m_pageID);
#endif

    m_process->send(Messages::WebPage::DidChooseFilesForOpenPanelWithDisplayStringAndIcon(fileURLs, displayString, iconData ? iconData->dataReference() : IPC::DataReference()), m_pageID);

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = nullptr;
}
#endif

void WebPageProxy::didChooseFilesForOpenPanel(const Vector<String>& fileURLs)
{
    if (!isValid())
        return;

#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::HandleArray sandboxExtensionHandles;
    sandboxExtensionHandles.allocate(fileURLs.size());
    for (size_t i = 0; i < fileURLs.size(); ++i) {
        bool createdExtension = SandboxExtension::createHandle(fileURLs[i], SandboxExtension::Type::ReadOnly, sandboxExtensionHandles[i]);
        if (!createdExtension) {
            // This can legitimately fail if a directory containing the file is deleted after the file was chosen.
            // We also have reports of cases where this likely fails for some unknown reason, <rdar://problem/10156710>.
            WTFLogAlways("WebPageProxy::didChooseFilesForOpenPanel: could not create a sandbox extension for '%s'\n", fileURLs[i].utf8().data());
            continue;
        }
    }

    m_process->send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(sandboxExtensionHandles), m_pageID);
#endif

    m_process->send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs), m_pageID);

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = nullptr;
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::DidCancelForOpenPanel(), m_pageID);
    
    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = nullptr;
}

void WebPageProxy::advanceToNextMisspelling(bool startBeforeSelection)
{
    m_process->send(Messages::WebPage::AdvanceToNextMisspelling(startBeforeSelection), m_pageID);
}

void WebPageProxy::changeSpellingToWord(const String& word)
{
    if (word.isEmpty())
        return;

    m_process->send(Messages::WebPage::ChangeSpellingToWord(word), m_pageID);
}

void WebPageProxy::registerEditCommand(Ref<WebEditCommandProxy>&& commandProxy, UndoOrRedo undoOrRedo)
{
    m_pageClient.registerEditCommand(WTFMove(commandProxy), undoOrRedo);
}

void WebPageProxy::addEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.add(command);
}

void WebPageProxy::removeEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.remove(command);

    if (!isValid())
        return;
    m_process->send(Messages::WebPage::DidRemoveEditCommand(command->commandID()), m_pageID);
}

bool WebPageProxy::canUndo()
{
    bool result;
    canUndoRedo(Undo, result);
    return result;
}

bool WebPageProxy::canRedo()
{
    bool result;
    canUndoRedo(Redo, result);
    return result;
}

bool WebPageProxy::isValidEditCommand(WebEditCommandProxy* command)
{
    return m_editCommandSet.find(command) != m_editCommandSet.end();
}

int64_t WebPageProxy::spellDocumentTag()
{
    if (!m_hasSpellDocumentTag) {
        m_spellDocumentTag = TextChecker::uniqueSpellDocumentTag(this);
        m_hasSpellDocumentTag = true;
    }

    return m_spellDocumentTag;
}

#if USE(UNIFIED_TEXT_CHECKING)
void WebPageProxy::checkTextOfParagraph(const String& text, uint64_t checkingTypes, int32_t insertionPoint, Vector<TextCheckingResult>& results)
{
    results = TextChecker::checkTextOfParagraph(spellDocumentTag(), text, insertionPoint, checkingTypes, m_initialCapitalizationEnabled);
}
#endif

void WebPageProxy::checkSpellingOfString(const String& text, int32_t& misspellingLocation, int32_t& misspellingLength)
{
    TextChecker::checkSpellingOfString(spellDocumentTag(), text, misspellingLocation, misspellingLength);
}

void WebPageProxy::checkGrammarOfString(const String& text, Vector<GrammarDetail>& grammarDetails, int32_t& badGrammarLocation, int32_t& badGrammarLength)
{
    TextChecker::checkGrammarOfString(spellDocumentTag(), text, grammarDetails, badGrammarLocation, badGrammarLength);
}

void WebPageProxy::spellingUIIsShowing(bool& isShowing)
{
    isShowing = TextChecker::spellingUIIsShowing();
}

void WebPageProxy::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    TextChecker::updateSpellingUIWithMisspelledWord(spellDocumentTag(), misspelledWord);
}

void WebPageProxy::updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const GrammarDetail& grammarDetail)
{
    TextChecker::updateSpellingUIWithGrammarString(spellDocumentTag(), badGrammarPhrase, grammarDetail);
}

void WebPageProxy::getGuessesForWord(const String& word, const String& context, int32_t insertionPoint, Vector<String>& guesses)
{
    TextChecker::getGuessesForWord(spellDocumentTag(), word, context, insertionPoint, guesses, m_initialCapitalizationEnabled);
}

void WebPageProxy::learnWord(const String& word)
{
    MESSAGE_CHECK(m_pendingLearnOrIgnoreWordMessageCount);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::learnWord(spellDocumentTag(), word);
}

void WebPageProxy::ignoreWord(const String& word)
{
    MESSAGE_CHECK(m_pendingLearnOrIgnoreWordMessageCount);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::ignoreWord(spellDocumentTag(), word);
}

void WebPageProxy::requestCheckingOfString(uint64_t requestID, const TextCheckingRequestData& request, int32_t insertionPoint)
{
    TextChecker::requestCheckingOfString(TextCheckerCompletion::create(requestID, request, this), insertionPoint);
}

void WebPageProxy::didFinishCheckingText(uint64_t requestID, const Vector<WebCore::TextCheckingResult>& result)
{
    m_process->send(Messages::WebPage::DidFinishCheckingText(requestID, result), m_pageID);
}

void WebPageProxy::didCancelCheckingText(uint64_t requestID)
{
    m_process->send(Messages::WebPage::DidCancelCheckingText(requestID), m_pageID);
}
// Other

void WebPageProxy::setFocus(bool focused)
{
    if (focused)
        m_uiClient->focus(this);
    else
        m_uiClient->unfocus(this);
}

void WebPageProxy::takeFocus(uint32_t direction)
{
    m_uiClient->takeFocus(this, (static_cast<FocusDirection>(direction) == FocusDirectionForward) ? kWKFocusDirectionForward : kWKFocusDirectionBackward);
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    m_pageClient.toolTipChanged(oldToolTip, m_toolTip);
}

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    m_pageClient.setCursor(cursor);
}

void WebPageProxy::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    m_pageClient.setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
}

void WebPageProxy::didReceiveEvent(uint32_t opaqueType, bool handled)
{
    WebEvent::Type type = static_cast<WebEvent::Type>(opaqueType);

    switch (type) {
    case WebEvent::NoType:
    case WebEvent::MouseMove:
    case WebEvent::Wheel:
        break;

    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
    case WebEvent::MouseForceChanged:
    case WebEvent::MouseForceDown:
    case WebEvent::MouseForceUp:
    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char:
#if ENABLE(TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel:
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEvent::GestureStart:
    case WebEvent::GestureChange:
    case WebEvent::GestureEnd:
#endif
        m_process->responsivenessTimer().stop();
        break;
    }

    switch (type) {
    case WebEvent::NoType:
        break;
    case WebEvent::MouseMove:
        m_processingMouseMoveEvent = false;
        if (m_nextMouseMoveEvent)
            handleMouseEvent(*std::exchange(m_nextMouseMoveEvent, nullptr));
        break;
    case WebEvent::MouseDown:
        break;
    case WebEvent::MouseUp:
        m_currentlyProcessedMouseDownEvent = nullptr;
        break;
    case WebEvent::MouseForceChanged:
    case WebEvent::MouseForceDown:
    case WebEvent::MouseForceUp:
        break;

    case WebEvent::Wheel: {
        MESSAGE_CHECK(!m_currentlyProcessedWheelEvents.isEmpty());

        std::unique_ptr<Vector<NativeWebWheelEvent>> oldestCoalescedEvent = m_currentlyProcessedWheelEvents.takeFirst();

        // FIXME: Dispatch additional events to the didNotHandleWheelEvent client function.
        if (!handled) {
            m_uiClient->didNotHandleWheelEvent(this, oldestCoalescedEvent->last());
            m_pageClient.wheelEventWasNotHandledByWebCore(oldestCoalescedEvent->last());
        }

        if (!m_wheelEventQueue.isEmpty())
            processNextQueuedWheelEvent();
        break;
    }

    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char: {
        LOG(KeyHandling, "WebPageProxy::didReceiveEvent: %s (queue empty %d)", webKeyboardEventTypeString(type), m_keyEventQueue.isEmpty());

        MESSAGE_CHECK(!m_keyEventQueue.isEmpty());
        NativeWebKeyboardEvent event = m_keyEventQueue.takeFirst();

        MESSAGE_CHECK(type == event.type());

        if (!m_keyEventQueue.isEmpty()) {
            LOG(KeyHandling, " UI process: sent keyEvent from didReceiveEvent");
            m_process->send(Messages::WebPage::KeyEvent(m_keyEventQueue.first()), m_pageID);
        }

        // The call to doneWithKeyEvent may close this WebPage.
        // Protect against this being destroyed.
        Ref<WebPageProxy> protect(*this);

        m_pageClient.doneWithKeyEvent(event, handled);
        if (!handled)
            m_uiClient->didNotHandleKeyEvent(this, event);

        // Notify the session after -[NSApp sendEvent:] has a crack at turning the event into an action.
        if (m_keyEventQueue.isEmpty()) {
            if (auto* automationSession = process().processPool().automationSession())
                automationSession->keyboardEventsFlushedForPage(*this);
        }
        break;
    }
#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEvent::GestureStart:
    case WebEvent::GestureChange:
    case WebEvent::GestureEnd: {
        MESSAGE_CHECK(!m_gestureEventQueue.isEmpty());
        NativeWebGestureEvent event = m_gestureEventQueue.takeFirst();

        MESSAGE_CHECK(type == event.type());

        if (!handled)
            m_pageClient.gestureEventWasNotHandledByWebCore(event);
        break;
    }
        break;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel:
        break;
#elif ENABLE(TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel: {
        MESSAGE_CHECK(!m_touchEventQueue.isEmpty());
        QueuedTouchEvents queuedEvents = m_touchEventQueue.takeFirst();

        MESSAGE_CHECK(type == queuedEvents.forwardedEvent.type());

        m_pageClient.doneWithTouchEvent(queuedEvents.forwardedEvent, handled);
        for (size_t i = 0; i < queuedEvents.deferredTouchEvents.size(); ++i) {
            bool isEventHandled = false;
            m_pageClient.doneWithTouchEvent(queuedEvents.deferredTouchEvents.at(i), isEventHandled);
        }
        break;
    }
#endif
    }
}

void WebPageProxy::voidCallback(CallbackID callbackID)
{
    auto callback = m_callbacks.take<VoidCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallback();
}

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::attachmentInfoCallback(const WebCore::AttachmentInfo& info, CallbackID callbackID)
{
    auto callback = m_callbacks.take<AttachmentInfoCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    MESSAGE_CHECK_URL(info.filePath);
    callback->performCallbackWithReturnValue(info);
}

#endif

void WebPageProxy::dataCallback(const IPC::DataReference& dataReference, CallbackID callbackID)
{
    auto callback = m_callbacks.take<DataCallback>(callbackID);
    if (!callback)
        return;

    callback->performCallbackWithReturnValue(API::Data::create(dataReference.data(), dataReference.size()).ptr());
}

void WebPageProxy::imageCallback(const ShareableBitmap::Handle& bitmapHandle, CallbackID callbackID)
{
    auto callback = m_callbacks.take<ImageCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(bitmapHandle);
}

void WebPageProxy::stringCallback(const String& resultString, CallbackID callbackID)
{
    auto callback = m_callbacks.take<StringCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    m_loadDependentStringCallbackIDs.remove(callbackID);

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::invalidateStringCallback(CallbackID callbackID)
{
    auto callback = m_callbacks.take<StringCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    m_loadDependentStringCallbackIDs.remove(callbackID);

    callback->invalidate();
}

void WebPageProxy::scriptValueCallback(const IPC::DataReference& dataReference, bool hadException, const ExceptionDetails& details, CallbackID callbackID)
{
    auto callback = m_callbacks.take<ScriptValueCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    if (dataReference.isEmpty()) {
        callback->performCallbackWithReturnValue(nullptr, hadException, details);
        return;
    }

    Vector<uint8_t> data;
    data.reserveInitialCapacity(dataReference.size());
    data.append(dataReference.data(), dataReference.size());

    callback->performCallbackWithReturnValue(API::SerializedScriptValue::adopt(WTFMove(data)).ptr(), hadException, details);
}

void WebPageProxy::computedPagesCallback(const Vector<IntRect>& pageRects, double totalScaleFactorForPrinting, CallbackID callbackID)
{
    auto callback = m_callbacks.take<ComputedPagesCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(pageRects, totalScaleFactorForPrinting);
}

void WebPageProxy::validateCommandCallback(const String& commandName, bool isEnabled, int state, CallbackID callbackID)
{
    auto callback = m_callbacks.take<ValidateCommandCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(commandName.impl(), isEnabled, state);
}

void WebPageProxy::unsignedCallback(uint64_t result, CallbackID callbackID)
{
    auto callback = m_callbacks.take<UnsignedCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    callback->performCallbackWithReturnValue(result);
}

void WebPageProxy::editingRangeCallback(const EditingRange& range, CallbackID callbackID)
{
    MESSAGE_CHECK(range.isValid());

    auto callback = m_callbacks.take<EditingRangeCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    callback->performCallbackWithReturnValue(range);
}

#if ENABLE(APPLICATION_MANIFEST)
void WebPageProxy::applicationManifestCallback(const std::optional<WebCore::ApplicationManifest>& manifestOrNull, CallbackID callbackID)
{
    auto callback = m_callbacks.take<ApplicationManifestCallback>(callbackID);
    if (!callback)
        return;

    callback->performCallbackWithReturnValue(manifestOrNull);
}
#endif

#if PLATFORM(COCOA)
void WebPageProxy::machSendRightCallback(const MachSendRight& sendRight, CallbackID callbackID)
{
    auto callback = m_callbacks.take<MachSendRightCallback>(callbackID);
    if (!callback)
        return;

    callback->performCallbackWithReturnValue(sendRight);
}
#endif

inline API::DiagnosticLoggingClient* WebPageProxy::effectiveDiagnosticLoggingClient(ShouldSample shouldSample)
{
    // Diagnostic logging is disabled for ephemeral sessions for privacy reasons.
    if (sessionID().isEphemeral())
        return nullptr;

    return DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample) ? diagnosticLoggingClient() : nullptr;
}

void WebPageProxy::logDiagnosticMessage(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessage(this, message, description);
}

void WebPageProxy::logDiagnosticMessageWithResult(const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithResult(this, message, description, static_cast<WebCore::DiagnosticLoggingResultType>(result));
}

void WebPageProxy::logDiagnosticMessageWithValue(const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithValue(this, message, description, String::number(value, significantFigures));
}

void WebPageProxy::logDiagnosticMessageWithEnhancedPrivacy(const String& message, const String& description, ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithEnhancedPrivacy(this, message, description);
}

void WebPageProxy::logScrollingEvent(uint32_t eventType, MonotonicTime timestamp, uint64_t data)
{
    PerformanceLoggingClient::ScrollingEvent event = static_cast<PerformanceLoggingClient::ScrollingEvent>(eventType);

    switch (event) {
    case PerformanceLoggingClient::ScrollingEvent::ExposedTilelessArea:
        WTFLogAlways("SCROLLING: Exposed tileless area. Time: %f Unfilled Pixels: %llu\n", timestamp.secondsSinceEpoch().value(), (unsigned long long)data);
        break;
    case PerformanceLoggingClient::ScrollingEvent::FilledTile:
        WTFLogAlways("SCROLLING: Filled visible fresh tile. Time: %f Unfilled Pixels: %llu\n", timestamp.secondsSinceEpoch().value(), (unsigned long long)data);
        break;
    case PerformanceLoggingClient::ScrollingEvent::SwitchedScrollingMode:
        if (data)
            WTFLogAlways("SCROLLING: Switching to main-thread scrolling mode. Time: %f Reason(s): %s\n", timestamp.secondsSinceEpoch().value(), PerformanceLoggingClient::synchronousScrollingReasonsAsString(data).utf8().data());
        else
            WTFLogAlways("SCROLLING: Switching to threaded scrolling mode. Time: %f\n", timestamp.secondsSinceEpoch().value());
        break;
    }
}

void WebPageProxy::rectForCharacterRangeCallback(const IntRect& rect, const EditingRange& actualRange, CallbackID callbackID)
{
    MESSAGE_CHECK(actualRange.isValid());

    auto callback = m_callbacks.take<RectForCharacterRangeCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    callback->performCallbackWithReturnValue(rect, actualRange);
}

#if PLATFORM(GTK)
void WebPageProxy::printFinishedCallback(const ResourceError& printError, CallbackID callbackID)
{
    auto callback = m_callbacks.take<PrintFinishedCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(API::Error::create(printError).ptr());
}
#endif

void WebPageProxy::focusedFrameChanged(uint64_t frameID)
{
    if (!frameID) {
        m_focusedFrame = nullptr;
        return;
    }

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_focusedFrame = frame;
}

void WebPageProxy::frameSetLargestFrameChanged(uint64_t frameID)
{
    if (!frameID) {
        m_frameSetLargestFrame = nullptr;
        return;
    }

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_frameSetLargestFrame = frame;
}

void WebPageProxy::processDidBecomeUnresponsive()
{
    if (!isValid())
        return;

    updateBackingStoreDiscardableState();

    if (m_navigationClient)
        m_navigationClient->processDidBecomeUnresponsive(*this);
    else
        m_loaderClient->processDidBecomeUnresponsive(*this);
}

void WebPageProxy::processDidBecomeResponsive()
{
    if (!isValid())
        return;
    
    updateBackingStoreDiscardableState();

    if (m_navigationClient)
        m_navigationClient->processDidBecomeResponsive(*this);
    else
        m_loaderClient->processDidBecomeResponsive(*this);
}

void WebPageProxy::willChangeProcessIsResponsive()
{
    m_pageLoadState.willChangeProcessIsResponsive();
}

void WebPageProxy::didChangeProcessIsResponsive()
{
    m_pageLoadState.didChangeProcessIsResponsive();
}

String WebPageProxy::currentURL() const
{
    String url = m_pageLoadState.activeURL();
    if (url.isEmpty() && m_backForwardList->currentItem())
        url = m_backForwardList->currentItem()->url();
    return url;
}

void WebPageProxy::processDidTerminate(ProcessTerminationReason reason)
{
    ASSERT(m_isValid);

#if PLATFORM(IOS)
    if (m_process->isUnderMemoryPressure()) {
        String domain = WebCore::topPrivatelyControlledDomain(WebCore::URL(WebCore::ParsedURLString, currentURL()).host());
        if (!domain.isEmpty())
            logDiagnosticMessageWithEnhancedPrivacy(WebCore::DiagnosticLoggingKeys::domainCausingJetsamKey(), domain, WebCore::ShouldSample::No);
    }
#endif

    // There is a nested transaction in resetStateAfterProcessExited() that we don't want to commit before the client call.
    PageLoadState::Transaction transaction = m_pageLoadState.transaction();

    resetStateAfterProcessExited();

    navigationState().clearAllNavigations();

    if (m_navigationClient)
        m_navigationClient->processDidTerminate(*this, reason);
    else if (reason != ProcessTerminationReason::RequestedByClient)
        m_loaderClient->processDidCrash(*this);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->terminate();
    }

    stopAllURLSchemeTasks();
}

void WebPageProxy::stopAllURLSchemeTasks()
{
    HashSet<WebURLSchemeHandler*> handlers;
    for (auto& handler : m_urlSchemeHandlersByScheme.values())
        handlers.add(handler.ptr());

    for (auto* handler : handlers)
        handler->stopAllTasksForPage(*this);
}

#if PLATFORM(IOS)
void WebPageProxy::processWillBecomeSuspended()
{
    if (!isValid())
        return;

    m_hasNetworkRequestsOnSuspended = m_pageLoadState.networkRequestsInProgress();
    if (m_hasNetworkRequestsOnSuspended)
        setNetworkRequestsInProgress(false);
}

void WebPageProxy::processWillBecomeForeground()
{
    if (!isValid())
        return;

    if (m_hasNetworkRequestsOnSuspended) {
        setNetworkRequestsInProgress(true);
        m_hasNetworkRequestsOnSuspended = false;
    }
}
#endif

void WebPageProxy::resetState(ResetStateReason resetStateReason)
{
    m_mainFrame = nullptr;
#if PLATFORM(COCOA)
    m_scrollingPerformanceData = nullptr;
#endif
    m_drawingArea = nullptr;
    hideValidationMessage();

    if (m_inspector) {
        m_inspector->invalidate();
        m_inspector = nullptr;
    }

#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenManager) {
        m_fullScreenManager->invalidate();
        m_fullScreenManager = nullptr;
    }
#endif

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = nullptr;
    }

#if ENABLE(TOUCH_EVENTS)
    m_touchEventTracking.reset();
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    if (m_colorPicker) {
        m_colorPicker->invalidate();
        m_colorPicker = nullptr;
    }
#endif

#if ENABLE(GEOLOCATION)
    m_geolocationPermissionRequestManager.invalidateRequests();
#endif

#if ENABLE(MEDIA_STREAM)
    m_userMediaPermissionRequestManager = nullptr;
#endif

    m_notificationPermissionRequestManager.invalidateRequests();

    m_toolTip = String();

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    m_mainFrameIsPinnedToLeftSide = true;
    m_mainFrameIsPinnedToRightSide = true;
    m_mainFrameIsPinnedToTopSide = true;
    m_mainFrameIsPinnedToBottomSide = true;

    m_visibleScrollerThumbRect = IntRect();

#if (PLATFORM(IOS) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (m_playbackSessionManager) {
        m_playbackSessionManager->invalidate();
        m_playbackSessionManager = nullptr;
    }
    if (m_videoFullscreenManager) {
        m_videoFullscreenManager->invalidate();
        m_videoFullscreenManager = nullptr;
    }
#endif

#if PLATFORM(IOS)
    m_firstLayerTreeTransactionIdAfterDidCommitLoad = 0;
    m_lastVisibleContentRectUpdate = VisibleContentRectUpdateInfo();
    m_dynamicViewportSizeUpdateWaitingForTarget = false;
    m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit = false;
    m_dynamicViewportSizeUpdateLayerTreeTransactionID = 0;
    m_hasNetworkRequestsOnSuspended = false;
    m_isKeyboardAnimatingIn = false;
    m_isScrollingOrZooming = false;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
    m_pageClient.mediaSessionManager().removeAllPlaybackTargetPickerClients(*this);
#endif

#if ENABLE(APPLE_PAY)
    m_paymentCoordinator = nullptr;
#endif

    CallbackBase::Error error;
    switch (resetStateReason) {
    case ResetStateReason::PageInvalidated:
        error = CallbackBase::Error::OwnerWasInvalidated;
        break;

    case ResetStateReason::WebProcessExited:
        error = CallbackBase::Error::ProcessExited;
        break;
    }

    m_callbacks.invalidate(error);
    m_loadDependentStringCallbackIDs.clear();

    auto editCommandVector = copyToVector(m_editCommandSet);
    m_editCommandSet.clear();

    for (auto& editCommand : editCommandVector)
        editCommand->invalidate();

    m_activePopupMenu = nullptr;
    m_mediaState = MediaProducer::IsNotPlaying;

#if ENABLE(POINTER_LOCK)
    requestPointerUnlock();
#endif
}

void WebPageProxy::resetStateAfterProcessExited()
{
    if (!isValid())
        return;

    // FIXME: It's weird that resetStateAfterProcessExited() is called even though the process is launching.
    ASSERT(m_process->state() == WebProcessProxy::State::Launching || m_process->state() == WebProcessProxy::State::Terminated);

#if PLATFORM(IOS)
    m_activityToken = nullptr;
#endif
    m_pageIsUserObservableCount = nullptr;
    m_visiblePageToken = nullptr;

    m_isValid = false;
    m_isPageSuspended = false;

    m_needsToFinishInitializingWebPageAfterProcessLaunch = false;

    m_editorState = EditorState();

    m_pageClient.processDidExit();

    resetState(ResetStateReason::WebProcessExited);

    m_pageClient.clearAllEditCommands();
    m_pendingLearnOrIgnoreWordMessageCount = 0;

    // Can't expect DidReceiveEvent notifications from a crashed web process.
    m_keyEventQueue.clear();
    m_wheelEventQueue.clear();
    m_currentlyProcessedWheelEvents.clear();

    m_nextMouseMoveEvent = nullptr;
    m_currentlyProcessedMouseDownEvent = nullptr;

    m_processingMouseMoveEvent = false;

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    m_touchEventQueue.clear();
#endif

    PageLoadState::Transaction transaction = m_pageLoadState.transaction();
    m_pageLoadState.reset(transaction);

    m_process->processTerminated();
}

WebPageCreationParameters WebPageProxy::creationParameters()
{
    WebPageCreationParameters parameters;

    parameters.viewSize = m_pageClient.viewSize();
    parameters.activityState = m_activityState;
    parameters.drawingAreaType = m_drawingArea->type();
    parameters.store = preferencesStore();
    parameters.pageGroupData = m_pageGroup->data();
    parameters.drawsBackground = m_drawsBackground;
    parameters.isEditable = m_isEditable;
    parameters.underlayColor = m_underlayColor;
    parameters.useFixedLayout = m_useFixedLayout;
    parameters.fixedLayoutSize = m_fixedLayoutSize;
    parameters.alwaysShowsHorizontalScroller = m_alwaysShowsHorizontalScroller;
    parameters.alwaysShowsVerticalScroller = m_alwaysShowsVerticalScroller;
    parameters.suppressScrollbarAnimations = m_suppressScrollbarAnimations;
    parameters.paginationMode = m_paginationMode;
    parameters.paginationBehavesLikeColumns = m_paginationBehavesLikeColumns;
    parameters.pageLength = m_pageLength;
    parameters.gapBetweenPages = m_gapBetweenPages;
    parameters.paginationLineGridEnabled = m_paginationLineGridEnabled;
    parameters.userAgent = userAgent();
    parameters.itemStates = m_backForwardList->itemStates();
    parameters.sessionID = sessionID();
    parameters.highestUsedBackForwardItemID = WebBackForwardListItem::highestUsedItemID();
    parameters.userContentControllerID = m_userContentController->identifier();
    parameters.visitedLinkTableID = m_visitedLinkStore->identifier();
    parameters.websiteDataStoreID = m_websiteDataStore->identifier();
    parameters.canRunBeforeUnloadConfirmPanel = m_uiClient->canRunBeforeUnloadConfirmPanel();
    parameters.canRunModal = m_canRunModal;
    parameters.deviceScaleFactor = deviceScaleFactor();
    parameters.viewScaleFactor = m_viewScaleFactor;
    parameters.topContentInset = m_topContentInset;
    parameters.mediaVolume = m_mediaVolume;
    parameters.muted = m_mutedState;
    parameters.mayStartMediaWhenInWindow = m_mayStartMediaWhenInWindow;
    parameters.minimumLayoutSize = m_minimumLayoutSize;
    parameters.autoSizingShouldExpandToViewHeight = m_autoSizingShouldExpandToViewHeight;
    parameters.viewportSizeForCSSViewportUnits = m_viewportSizeForCSSViewportUnits;
    parameters.scrollPinningBehavior = m_scrollPinningBehavior;
    if (m_scrollbarOverlayStyle)
        parameters.scrollbarOverlayStyle = m_scrollbarOverlayStyle.value();
    else
        parameters.scrollbarOverlayStyle = std::nullopt;
    parameters.backgroundExtendsBeyondPage = m_backgroundExtendsBeyondPage;
    parameters.layerHostingMode = m_layerHostingMode;
    parameters.controlledByAutomation = m_controlledByAutomation;
#if ENABLE(REMOTE_INSPECTOR)
    parameters.allowsRemoteInspection = m_allowsRemoteInspection;
    parameters.remoteInspectionNameOverride = m_remoteInspectionNameOverride;
#endif
#if PLATFORM(MAC)
    parameters.colorSpace = m_pageClient.colorSpace();
#endif
#if PLATFORM(IOS)
    parameters.screenSize = screenSize();
    parameters.availableScreenSize = availableScreenSize();
    parameters.textAutosizingWidth = textAutosizingWidth();
    parameters.mimeTypesWithCustomContentProviders = m_pageClient.mimeTypesWithCustomContentProviders();
    parameters.ignoresViewportScaleLimits = m_forceAlwaysUserScalable;
    parameters.viewportConfigurationMinimumLayoutSize = m_viewportConfigurationMinimumLayoutSize;
    parameters.maximumUnobscuredSize = m_maximumUnobscuredSize;
#endif

#if PLATFORM(MAC)
    parameters.appleMailPaginationQuirkEnabled = appleMailPaginationQuirkEnabled();
#else
    parameters.appleMailPaginationQuirkEnabled = false;
#endif
    
#if PLATFORM(MAC)
    // FIXME: Need to support iOS too, but there is no isAppleMail for iOS.
    parameters.appleMailLinesClampEnabled = appleMailLinesClampEnabled();
#else
    parameters.appleMailLinesClampEnabled = false;
#endif

#if PLATFORM(COCOA)
    parameters.smartInsertDeleteEnabled = m_isSmartInsertDeleteEnabled;
#endif
    parameters.shouldScaleViewToFitDocument = m_shouldScaleViewToFitDocument;
    parameters.userInterfaceLayoutDirection = m_pageClient.userInterfaceLayoutDirection();
    parameters.observedLayoutMilestones = m_observedLayoutMilestones;
    parameters.overrideContentSecurityPolicy = m_overrideContentSecurityPolicy;
    parameters.cpuLimit = m_cpuLimit;

    for (auto& iterator : m_urlSchemeHandlersByScheme)
        parameters.urlSchemeHandlers.set(iterator.key, iterator.value->identifier());

#if ENABLE(WEB_RTC)
    parameters.iceCandidateFilteringEnabled = m_preferences->iceCandidateFilteringEnabled();
#if USE(LIBWEBRTC)
    parameters.enumeratingAllNetworkInterfacesEnabled = m_preferences->enumeratingAllNetworkInterfacesEnabled();
#endif
#endif

#if ENABLE(APPLICATION_MANIFEST)
    parameters.applicationManifest = m_configuration->applicationManifest() ? std::optional<WebCore::ApplicationManifest>(m_configuration->applicationManifest()->applicationManifest()) : std::nullopt;
#endif

    m_process->addWebUserContentControllerProxy(m_userContentController, parameters);

    return parameters;
}

void WebPageProxy::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    m_pageClient.enterAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::exitAcceleratedCompositingMode()
{
    m_pageClient.exitAcceleratedCompositingMode();
}

void WebPageProxy::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    m_pageClient.updateAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::backForwardClear()
{
    m_backForwardList->clear();
}

#if ENABLE(GAMEPAD)

void WebPageProxy::gamepadActivity(const Vector<GamepadData>& gamepadDatas, bool shouldMakeGamepadsVisible)
{
    m_process->send(Messages::WebPage::GamepadActivity(gamepadDatas, shouldMakeGamepadsVisible), m_pageID);
}

#endif

void WebPageProxy::canAuthenticateAgainstProtectionSpace(uint64_t loaderID, uint64_t frameID, const ProtectionSpace& coreProtectionSpace)
{
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebProtectionSpace> protectionSpace = WebProtectionSpace::create(coreProtectionSpace);

    bool canAuthenticate;
    if (m_navigationClient)
        canAuthenticate = m_navigationClient->canAuthenticateAgainstProtectionSpace(*this, protectionSpace.get());
    else
        canAuthenticate = m_loaderClient->canAuthenticateAgainstProtectionSpaceInFrame(*this, *frame, protectionSpace.get());

    m_process->processPool().sendToNetworkingProcess(Messages::NetworkProcess::ContinueCanAuthenticateAgainstProtectionSpace(loaderID, canAuthenticate));
#endif
}

void WebPageProxy::didReceiveAuthenticationChallengeProxy(uint64_t frameID, Ref<AuthenticationChallengeProxy>&& authenticationChallenge)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    if (m_navigationClient)
        m_navigationClient->didReceiveAuthenticationChallenge(*this, authenticationChallenge.get());
    else
        m_loaderClient->didReceiveAuthenticationChallengeInFrame(*this, *frame, authenticationChallenge.get());
}

void WebPageProxy::exceededDatabaseQuota(uint64_t frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, Ref<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply>&& reply)
{
    ExceededDatabaseQuotaRecords& records = ExceededDatabaseQuotaRecords::singleton();
    std::unique_ptr<ExceededDatabaseQuotaRecords::Record> newRecord = records.createRecord(frameID,
        originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage,
        currentDatabaseUsage, expectedUsage, WTFMove(reply));
    records.add(WTFMove(newRecord));

    if (records.areBeingProcessed())
        return;

    ExceededDatabaseQuotaRecords::Record* record = records.next();
    while (record) {
        WebFrameProxy* frame = m_process->webFrame(record->frameID);
        MESSAGE_CHECK(frame);

        RefPtr<API::SecurityOrigin> origin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(record->originIdentifier)->securityOrigin());
        auto currentReply = record->reply;
        m_uiClient->exceededDatabaseQuota(this, frame, origin.get(),
            record->databaseName, record->displayName, record->currentQuota,
            record->currentOriginUsage, record->currentDatabaseUsage, record->expectedUsage,
            [currentReply](unsigned long long newQuota) { currentReply->send(newQuota); });

        record = records.next();
    }
}

void WebPageProxy::reachedApplicationCacheOriginQuota(const String& originIdentifier, uint64_t currentQuota, uint64_t totalBytesNeeded, Ref<Messages::WebPageProxy::ReachedApplicationCacheOriginQuota::DelayedReply>&& reply)
{
    Ref<SecurityOrigin> securityOrigin = SecurityOriginData::fromDatabaseIdentifier(originIdentifier)->securityOrigin();
    m_uiClient->reachedApplicationCacheOriginQuota(this, securityOrigin.get(), currentQuota, totalBytesNeeded, [reply = WTFMove(reply)](unsigned long long newQuota) { reply->send(newQuota); });
}

void WebPageProxy::requestGeolocationPermissionForFrame(uint64_t geolocationID, uint64_t frameID, String originIdentifier)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // FIXME: Geolocation should probably be using toString() as its string representation instead of databaseIdentifier().
    auto origin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(originIdentifier)->securityOrigin());
    auto request = m_geolocationPermissionRequestManager.createRequest(geolocationID);
    Function<void(bool)> completionHandler = [request = WTFMove(request)](bool allowed) {
        if (allowed)
            request->allow();
        else
            request->deny();
    };

    // FIXME: Once iOS migrates to the new WKUIDelegate SPI, clean this up
    // and make it one UIClient call that calls the completionHandler with false
    // if there is no delegate instead of returning the completionHandler
    // for other code paths to try.
    m_uiClient->decidePolicyForGeolocationPermissionRequest(*this, *frame, origin.get(), completionHandler);
#if PLATFORM(IOS)
    if (completionHandler)
        m_pageClient.decidePolicyForGeolocationPermissionRequest(*frame, origin.get(), completionHandler);
#endif
    if (completionHandler)
        completionHandler(false);
}

#if ENABLE(MEDIA_STREAM)
UserMediaPermissionRequestManagerProxy& WebPageProxy::userMediaPermissionRequestManager()
{
    if (m_userMediaPermissionRequestManager)
        return *m_userMediaPermissionRequestManager;

    m_userMediaPermissionRequestManager = std::make_unique<UserMediaPermissionRequestManagerProxy>(*this);
    return *m_userMediaPermissionRequestManager;
}
#endif

void WebPageProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, const WebCore::SecurityOriginData&  userMediaDocumentOriginData, const WebCore::SecurityOriginData& topLevelDocumentOriginData, const WebCore::MediaStreamRequest& request)
{
#if ENABLE(MEDIA_STREAM)
    MESSAGE_CHECK(m_process->webFrame(frameID));

    userMediaPermissionRequestManager().requestUserMediaPermissionForFrame(userMediaID, frameID, userMediaDocumentOriginData.securityOrigin(), topLevelDocumentOriginData.securityOrigin(), request);
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOriginData);
    UNUSED_PARAM(topLevelDocumentOriginData);
    UNUSED_PARAM(request);
#endif
}

void WebPageProxy::enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, const WebCore::SecurityOriginData& userMediaDocumentOriginData, const WebCore::SecurityOriginData& topLevelDocumentOriginData)
{
#if ENABLE(MEDIA_STREAM)
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    userMediaPermissionRequestManager().enumerateMediaDevicesForFrame(userMediaID, frameID, userMediaDocumentOriginData.securityOrigin(), topLevelDocumentOriginData.securityOrigin());
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOriginData);
    UNUSED_PARAM(topLevelDocumentOriginData);
#endif
}

void WebPageProxy::clearUserMediaState()
{
#if ENABLE(MEDIA_STREAM)
    userMediaPermissionRequestManager().clearCachedState();
#endif
}

void WebPageProxy::requestNotificationPermission(uint64_t requestID, const String& originString)
{
    if (!isRequestIDValid(requestID))
        return;

    auto origin = API::SecurityOrigin::createFromString(originString);
    auto request = m_notificationPermissionRequestManager.createRequest(requestID);
    
    m_uiClient->decidePolicyForNotificationPermissionRequest(*this, origin.get(), [request = WTFMove(request)](bool allowed) {
        if (allowed)
            request->allow();
        else
            request->deny();
    });
}

void WebPageProxy::showNotification(const String& title, const String& body, const String& iconURL, const String& tag, const String& lang, WebCore::NotificationDirection dir, const String& originString, uint64_t notificationID)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->show(this, title, body, iconURL, tag, lang, dir, originString, notificationID);
}

void WebPageProxy::cancelNotification(uint64_t notificationID)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->cancel(this, notificationID);
}

void WebPageProxy::clearNotifications(const Vector<uint64_t>& notificationIDs)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->clearNotifications(this, notificationIDs);
}

void WebPageProxy::didDestroyNotification(uint64_t notificationID)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->didDestroyNotification(this, notificationID);
}

float WebPageProxy::headerHeight(WebFrameProxy& frame)
{
    if (frame.isDisplayingPDFDocument())
        return 0;
    return m_uiClient->headerHeight(*this, frame);
}

float WebPageProxy::footerHeight(WebFrameProxy& frame)
{
    if (frame.isDisplayingPDFDocument())
        return 0;
    return m_uiClient->footerHeight(*this, frame);
}

void WebPageProxy::drawHeader(WebFrameProxy& frame, FloatRect&& rect)
{
    if (frame.isDisplayingPDFDocument())
        return;
    m_uiClient->drawHeader(*this, frame, WTFMove(rect));
}

void WebPageProxy::drawFooter(WebFrameProxy& frame, FloatRect&& rect)
{
    if (frame.isDisplayingPDFDocument())
        return;
    m_uiClient->drawFooter(*this, frame, WTFMove(rect));
}

void WebPageProxy::runModal()
{
    // Since runModal() can (and probably will) spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer().stop();

    // Our Connection's run loop might have more messages waiting to be handled after this RunModal message.
    // To make sure they are handled inside of the nested modal run loop we must first signal the Connection's
    // run loop so we're guaranteed that it has a chance to wake up.
    // See http://webkit.org/b/89590 for more discussion.
    m_process->connection()->wakeUpRunLoop();

    m_uiClient->runModal(*this);
}

void WebPageProxy::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    m_visibleScrollerThumbRect = scrollerThumb;
}

void WebPageProxy::recommendedScrollbarStyleDidChange(int32_t newStyle)
{
#if USE(APPKIT)
    m_pageClient.recommendedScrollbarStyleDidChange(static_cast<WebCore::ScrollbarStyle>(newStyle));
#else
    UNUSED_PARAM(newStyle);
#endif
}

void WebPageProxy::didChangeScrollbarsForMainFrame(bool hasHorizontalScrollbar, bool hasVerticalScrollbar)
{
    m_mainFrameHasHorizontalScrollbar = hasHorizontalScrollbar;
    m_mainFrameHasVerticalScrollbar = hasVerticalScrollbar;
}

void WebPageProxy::didChangeScrollOffsetPinningForMainFrame(bool pinnedToLeftSide, bool pinnedToRightSide, bool pinnedToTopSide, bool pinnedToBottomSide)
{
    m_pageClient.pinnedStateWillChange();
    m_mainFrameIsPinnedToLeftSide = pinnedToLeftSide;
    m_mainFrameIsPinnedToRightSide = pinnedToRightSide;
    m_mainFrameIsPinnedToTopSide = pinnedToTopSide;
    m_mainFrameIsPinnedToBottomSide = pinnedToBottomSide;
    m_pageClient.pinnedStateDidChange();

    m_uiClient->pinnedStateDidChange(*this);
}

void WebPageProxy::didChangePageCount(unsigned pageCount)
{
    m_pageCount = pageCount;
}

void WebPageProxy::pageExtendedBackgroundColorDidChange(const Color& backgroundColor)
{
    m_pageExtendedBackgroundColor = backgroundColor;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebPageProxy::didFailToInitializePlugin(const String& mimeType, const String& frameURLString, const String& pageURLString)
{
    if (m_navigationClient->didFailToInitializePlugIn(*this, createPluginInformationDictionary(mimeType, frameURLString, pageURLString).get()))
        return;
    m_loaderClient->didFailToInitializePlugin(*this, createPluginInformationDictionary(mimeType, frameURLString, pageURLString).get());
}

void WebPageProxy::didBlockInsecurePluginVersion(const String& mimeType, const String& pluginURLString, const String& frameURLString, const String& pageURLString, bool replacementObscured)
{
    RefPtr<API::Dictionary> pluginInformation;

#if PLATFORM(COCOA) && ENABLE(NETSCAPE_PLUGIN_API)
    String newMimeType = mimeType;
    PluginModuleInfo plugin = m_process->processPool().pluginInfoStore().findPlugin(newMimeType, URL(URL(), pluginURLString));
    pluginInformation = createPluginInformationDictionary(plugin, frameURLString, mimeType, pageURLString, String(), String(), replacementObscured);
#else
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(pluginURLString);
    UNUSED_PARAM(frameURLString);
    UNUSED_PARAM(pageURLString);
    UNUSED_PARAM(replacementObscured);
#endif

    m_loaderClient->didBlockInsecurePluginVersion(*this, pluginInformation.get());
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

bool WebPageProxy::willHandleHorizontalScrollEvents() const
{
    return !m_canShortCircuitHorizontalWheelEvents;
}

void WebPageProxy::updateWebsitePolicies(WebsitePoliciesData&& websitePolicies)
{
    m_process->send(Messages::WebPage::UpdateWebsitePolicies(websitePolicies), m_pageID);
}

void WebPageProxy::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
    m_pageClient.didFinishLoadingDataForCustomContentProvider(suggestedFilename, dataReference);
}

void WebPageProxy::backForwardRemovedItem(uint64_t itemID)
{
    m_process->removeBackForwardItem(itemID);
    m_process->send(Messages::WebPage::DidRemoveBackForwardItem(itemID), m_pageID);
}

void WebPageProxy::setCanRunModal(bool canRunModal)
{
    if (!isValid())
        return;

    // It's only possible to change the state for a WebPage which
    // already qualifies for running modal child web pages, otherwise
    // there's no other possibility than not allowing it.
    m_canRunModal = m_uiClient->canRunModal() && canRunModal;
    m_process->send(Messages::WebPage::SetCanRunModal(m_canRunModal), m_pageID);
}

bool WebPageProxy::canRunModal()
{
    return isValid() ? m_canRunModal : false;
}

void WebPageProxy::beginPrinting(WebFrameProxy* frame, const PrintInfo& printInfo)
{
    if (m_isInPrintingMode)
        return;

    m_isInPrintingMode = true;
    m_process->send(Messages::WebPage::BeginPrinting(frame->frameID(), printInfo), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}

void WebPageProxy::endPrinting()
{
    if (!m_isInPrintingMode)
        return;

    m_isInPrintingMode = false;
    m_process->send(Messages::WebPage::EndPrinting(), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}

void WebPageProxy::computePagesForPrinting(WebFrameProxy* frame, const PrintInfo& printInfo, Ref<ComputedPagesCallback>&& callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    auto callbackID = callback->callbackID();
    m_callbacks.put(WTFMove(callback));
    m_isInPrintingMode = true;
    m_process->send(Messages::WebPage::ComputePagesForPrinting(frame->frameID(), printInfo, callbackID), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}

#if PLATFORM(COCOA)
void WebPageProxy::drawRectToImage(WebFrameProxy* frame, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, Ref<ImageCallback>&& callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    auto callbackID = callback->callbackID();
    m_callbacks.put(WTFMove(callback));
    m_process->send(Messages::WebPage::DrawRectToImage(frame->frameID(), printInfo, rect, imageSize, callbackID), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}

void WebPageProxy::drawPagesToPDF(WebFrameProxy* frame, const PrintInfo& printInfo, uint32_t first, uint32_t count, Ref<DataCallback>&& callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    auto callbackID = callback->callbackID();
    m_callbacks.put(WTFMove(callback));
    m_process->send(Messages::WebPage::DrawPagesToPDF(frame->frameID(), printInfo, first, count, callbackID), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}
#elif PLATFORM(GTK)
void WebPageProxy::drawPagesForPrinting(WebFrameProxy* frame, const PrintInfo& printInfo, Ref<PrintFinishedCallback>&& callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    auto callbackID = callback->callbackID();
    m_callbacks.put(WTFMove(callback));
    m_isInPrintingMode = true;
    m_process->send(Messages::WebPage::DrawPagesForPrinting(frame->frameID(), printInfo, callbackID), m_pageID, printingSendOptions(m_isPerformingDOMPrintOperation));
}
#endif

void WebPageProxy::updateBackingStoreDiscardableState()
{
    ASSERT(isValid());

    if (!m_drawingArea)
        return;

    bool isDiscardable;

    if (!m_process->isResponsive())
        isDiscardable = false;
    else
        isDiscardable = !m_pageClient.isViewWindowActive() || !isViewVisible();

    m_drawingArea->setBackingStoreIsDiscardable(isDiscardable);
}

void WebPageProxy::saveDataToFileInDownloadsFolder(String&& suggestedFilename, String&& mimeType, URL&& originatingURLString, API::Data& data)
{
    m_uiClient->saveDataToFileInDownloadsFolder(this, suggestedFilename, mimeType, originatingURLString, data);
}

void WebPageProxy::savePDFToFileInDownloadsFolder(String&& suggestedFilename, URL&& originatingURL, const IPC::DataReference& dataReference)
{
    if (!suggestedFilename.endsWithIgnoringASCIICase(".pdf"))
        return;

    saveDataToFileInDownloadsFolder(WTFMove(suggestedFilename), ASCIILiteral("application/pdf"), WTFMove(originatingURL),
        API::Data::create(dataReference.data(), dataReference.size()).get());
}

void WebPageProxy::setMinimumLayoutSize(const IntSize& minimumLayoutSize)
{
    if (m_minimumLayoutSize == minimumLayoutSize)
        return;

    m_minimumLayoutSize = minimumLayoutSize;

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetMinimumLayoutSize(minimumLayoutSize), m_pageID);
    m_drawingArea->minimumLayoutSizeDidChange();

#if USE(APPKIT)
    if (m_minimumLayoutSize.width() <= 0)
        intrinsicContentSizeDidChange(IntSize(-1, -1));
#endif
}

void WebPageProxy::setAutoSizingShouldExpandToViewHeight(bool shouldExpand)
{
    if (m_autoSizingShouldExpandToViewHeight == shouldExpand)
        return;

    m_autoSizingShouldExpandToViewHeight = shouldExpand;

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetAutoSizingShouldExpandToViewHeight(shouldExpand), m_pageID);
}

void WebPageProxy::setViewportSizeForCSSViewportUnits(const IntSize& viewportSize)
{
    if (m_viewportSizeForCSSViewportUnits && *m_viewportSizeForCSSViewportUnits == viewportSize)
        return;

    m_viewportSizeForCSSViewportUnits = viewportSize;

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetViewportSizeForCSSViewportUnits(viewportSize), m_pageID);
}

#if USE(AUTOMATIC_TEXT_REPLACEMENT)

void WebPageProxy::toggleSmartInsertDelete()
{
    if (TextChecker::isTestingMode())
        TextChecker::setSmartInsertDeleteEnabled(!TextChecker::isSmartInsertDeleteEnabled());
}

void WebPageProxy::toggleAutomaticQuoteSubstitution()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
}

void WebPageProxy::toggleAutomaticLinkDetection()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
}

void WebPageProxy::toggleAutomaticDashSubstitution()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
}

void WebPageProxy::toggleAutomaticTextReplacement()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
}

#endif

#if PLATFORM(MAC)

void WebPageProxy::substitutionsPanelIsShowing(bool& isShowing)
{
    isShowing = TextChecker::substitutionsPanelIsShowing();
}

void WebPageProxy::showCorrectionPanel(int32_t panelType, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    m_pageClient.showCorrectionPanel((AlternativeTextType)panelType, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
}

void WebPageProxy::dismissCorrectionPanel(int32_t reason)
{
    m_pageClient.dismissCorrectionPanel((ReasonForDismissingAlternativeText)reason);
}

void WebPageProxy::dismissCorrectionPanelSoon(int32_t reason, String& result)
{
    result = m_pageClient.dismissCorrectionPanelSoon((ReasonForDismissingAlternativeText)reason);
}

void WebPageProxy::recordAutocorrectionResponse(int32_t response, const String& replacedString, const String& replacementString)
{
    m_pageClient.recordAutocorrectionResponse(static_cast<AutocorrectionResponse>(response), replacedString, replacementString);
}

void WebPageProxy::handleAlternativeTextUIResult(const String& result)
{
    if (!isClosed())
        m_process->send(Messages::WebPage::HandleAlternativeTextUIResult(result), m_pageID);
}

#if USE(DICTATION_ALTERNATIVES)
void WebPageProxy::showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, uint64_t dictationContext)
{
    m_pageClient.showDictationAlternativeUI(boundingBoxOfDictatedText, dictationContext);
}

void WebPageProxy::removeDictationAlternatives(uint64_t dictationContext)
{
    m_pageClient.removeDictationAlternatives(dictationContext);
}

void WebPageProxy::dictationAlternatives(uint64_t dictationContext, Vector<String>& result)
{
    result = m_pageClient.dictationAlternatives(dictationContext);
}
#endif

void WebPageProxy::setEditableElementIsFocused(bool editableElementIsFocused)
{
    m_pageClient.setEditableElementIsFocused(editableElementIsFocused);
}

#endif // PLATFORM(MAC)

#if PLATFORM(COCOA)
RefPtr<ViewSnapshot> WebPageProxy::takeViewSnapshot()
{
    return m_pageClient.takeViewSnapshot();
}
#endif

#if PLATFORM(GTK)
void WebPageProxy::setComposition(const String& text, Vector<CompositionUnderline> underlines, uint64_t selectionStart, uint64_t selectionEnd, uint64_t replacementRangeStart, uint64_t replacementRangeEnd)
{
    // FIXME: We need to find out how to proper handle the crashes case.
    if (!isValid())
        return;

    process().send(Messages::WebPage::SetComposition(text, underlines, selectionStart, selectionEnd, replacementRangeStart, replacementRangeEnd), m_pageID);
}

void WebPageProxy::confirmComposition(const String& compositionString, int64_t selectionStart, int64_t selectionLength)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::ConfirmComposition(compositionString, selectionStart, selectionLength), m_pageID);
}

void WebPageProxy::cancelComposition()
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::CancelComposition(), m_pageID);
}
#endif // PLATFORM(GTK)

void WebPageProxy::didSaveToPageCache()
{
    m_process->didSaveToPageCache();
}

void WebPageProxy::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    if (m_scrollPinningBehavior == pinning)
        return;
    
    m_scrollPinningBehavior = pinning;

    if (isValid())
        m_process->send(Messages::WebPage::SetScrollPinningBehavior(pinning), m_pageID);
}

void WebPageProxy::setOverlayScrollbarStyle(std::optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle)
{
    if (!m_scrollbarOverlayStyle && !scrollbarStyle)
        return;

    if ((m_scrollbarOverlayStyle && scrollbarStyle) && m_scrollbarOverlayStyle.value() == scrollbarStyle.value())
        return;

    m_scrollbarOverlayStyle = scrollbarStyle;

    std::optional<uint32_t> scrollbarStyleForMessage;
    if (scrollbarStyle)
        scrollbarStyleForMessage = static_cast<ScrollbarOverlayStyle>(scrollbarStyle.value());

    if (isValid())
        m_process->send(Messages::WebPage::SetScrollbarOverlayStyle(scrollbarStyleForMessage), m_pageID);
}

#if ENABLE(SUBTLE_CRYPTO)
void WebPageProxy::wrapCryptoKey(const Vector<uint8_t>& key, bool& succeeded, Vector<uint8_t>& wrappedKey)
{
    PageClientProtector protector(m_pageClient);

    Vector<uint8_t> masterKey;

    if (m_navigationClient) {
        if (RefPtr<API::Data> keyData = m_navigationClient->webCryptoMasterKey(*this))
            masterKey = keyData->dataReference().vector();
    } else if (!getDefaultWebCryptoMasterKey(masterKey)) {
        succeeded = false;
        return;
    }

    succeeded = wrapSerializedCryptoKey(masterKey, key, wrappedKey);
}

void WebPageProxy::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, bool& succeeded, Vector<uint8_t>& key)
{
    PageClientProtector protector(m_pageClient);

    Vector<uint8_t> masterKey;

    if (m_navigationClient) {
        if (RefPtr<API::Data> keyData = m_navigationClient->webCryptoMasterKey(*this))
            masterKey = keyData->dataReference().vector();
    } else if (!getDefaultWebCryptoMasterKey(masterKey)) {
        succeeded = false;
        return;
    }

    succeeded = unwrapSerializedCryptoKey(masterKey, wrappedKey, key);
}
#endif

void WebPageProxy::addMIMETypeWithCustomContentProvider(const String& mimeType)
{
    m_process->send(Messages::WebPage::AddMIMETypeWithCustomContentProvider(mimeType), m_pageID);
}

#if PLATFORM(COCOA)

void WebPageProxy::setTextAsync(const String& text)
{
    if (isValid())
        process().send(Messages::WebPage::SetTextAsync(text), m_pageID);
}

void WebPageProxy::insertTextAsync(const String& text, const EditingRange& replacementRange, bool registerUndoGroup, EditingRangeIsRelativeTo editingRangeIsRelativeTo, bool suppressSelectionUpdate)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::InsertTextAsync(text, replacementRange, registerUndoGroup, static_cast<uint32_t>(editingRangeIsRelativeTo), suppressSelectionUpdate), m_pageID);
}

void WebPageProxy::getMarkedRangeAsync(WTF::Function<void (EditingRange, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    process().send(Messages::WebPage::GetMarkedRangeAsync(callbackID), m_pageID);
}

void WebPageProxy::getSelectedRangeAsync(WTF::Function<void (EditingRange, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    process().send(Messages::WebPage::GetSelectedRangeAsync(callbackID), m_pageID);
}

void WebPageProxy::characterIndexForPointAsync(const WebCore::IntPoint& point, WTF::Function<void (uint64_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    process().send(Messages::WebPage::CharacterIndexForPointAsync(point, callbackID), m_pageID);
}

void WebPageProxy::firstRectForCharacterRangeAsync(const EditingRange& range, WTF::Function<void (const WebCore::IntRect&, const EditingRange&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(WebCore::IntRect(), EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    process().send(Messages::WebPage::FirstRectForCharacterRangeAsync(range, callbackID), m_pageID);
}

void WebPageProxy::setCompositionAsync(const String& text, const Vector<CompositionUnderline>& underlines, const EditingRange& selectionRange, const EditingRange& replacementRange)
{
    if (!isValid()) {
        // If this fails, we should call -discardMarkedText on input context to notify the input method.
        // This will happen naturally later, as part of reloading the page.
        return;
    }

    process().send(Messages::WebPage::SetCompositionAsync(text, underlines, selectionRange, replacementRange), m_pageID);
}

void WebPageProxy::confirmCompositionAsync()
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::ConfirmCompositionAsync(), m_pageID);
}

void WebPageProxy::setScrollPerformanceDataCollectionEnabled(bool enabled)
{
    if (enabled == m_scrollPerformanceDataCollectionEnabled)
        return;

    m_scrollPerformanceDataCollectionEnabled = enabled;

    if (m_scrollPerformanceDataCollectionEnabled && !m_scrollingPerformanceData)
        m_scrollingPerformanceData = std::make_unique<RemoteLayerTreeScrollingPerformanceData>(downcast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea));
    else if (!m_scrollPerformanceDataCollectionEnabled)
        m_scrollingPerformanceData = nullptr;
}
#endif

void WebPageProxy::takeSnapshot(IntRect rect, IntSize bitmapSize, SnapshotOptions options, WTF::Function<void (const ShareableBitmap::Handle&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(ShareableBitmap::Handle(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::TakeSnapshot(rect, bitmapSize, options, callbackID), m_pageID);
}

void WebPageProxy::navigationGestureDidBegin()
{
    PageClientProtector protector(m_pageClient);

    m_isShowingNavigationGestureSnapshot = true;
    m_pageClient.navigationGestureDidBegin();

    if (m_navigationClient)
        m_navigationClient->didBeginNavigationGesture(*this);
    else
        m_loaderClient->navigationGestureDidBegin(*this);
}

void WebPageProxy::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    PageClientProtector protector(m_pageClient);

    m_pageClient.navigationGestureWillEnd(willNavigate, item);

    if (m_navigationClient)
        m_navigationClient->willEndNavigationGesture(*this, willNavigate, item);
    else
        m_loaderClient->navigationGestureWillEnd(*this, willNavigate, item);
}

void WebPageProxy::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    PageClientProtector protector(m_pageClient);

    m_pageClient.navigationGestureDidEnd(willNavigate, item);

    if (m_navigationClient)
        m_navigationClient->didEndNavigationGesture(*this, willNavigate, item);
    else
        m_loaderClient->navigationGestureDidEnd(*this, willNavigate, item);
}

void WebPageProxy::navigationGestureDidEnd()
{
    PageClientProtector protector(m_pageClient);

    m_pageClient.navigationGestureDidEnd();
}

void WebPageProxy::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    PageClientProtector protector(m_pageClient);

    m_pageClient.willRecordNavigationSnapshot(item);
}

void WebPageProxy::navigationGestureSnapshotWasRemoved()
{
    m_isShowingNavigationGestureSnapshot = false;

    m_pageClient.didRemoveNavigationGestureSnapshot();

    if (m_navigationClient)
        m_navigationClient->didRemoveNavigationGestureSnapshot(*this);
}

void WebPageProxy::isPlayingMediaDidChange(MediaProducer::MediaStateFlags newState, uint64_t sourceElementID)
{
#if ENABLE(MEDIA_SESSION)
    WebMediaSessionFocusManager* focusManager = process().processPool().supplement<WebMediaSessionFocusManager>();
    ASSERT(focusManager);
    focusManager->updatePlaybackAttributesFromMediaState(this, sourceElementID, newState);
#endif

    if (newState == m_mediaState)
        return;

#if ENABLE(MEDIA_STREAM)
    WebCore::MediaProducer::MediaStateFlags oldMediaCaptureState = m_mediaState & WebCore::MediaProducer::MediaCaptureMask;
    WebCore::MediaProducer::MediaStateFlags newMediaCaptureState = newState & WebCore::MediaProducer::MediaCaptureMask;
#endif

    MediaProducer::MediaStateFlags playingMediaMask = MediaProducer::IsPlayingAudio | MediaProducer::IsPlayingVideo;
    MediaProducer::MediaStateFlags oldState = m_mediaState;

    bool playingAudioChanges = (oldState & MediaProducer::IsPlayingAudio) != (newState & MediaProducer::IsPlayingAudio);
    if (playingAudioChanges)
        m_pageClient.isPlayingAudioWillChange();
    m_mediaState = newState;
    if (playingAudioChanges)
        m_pageClient.isPlayingAudioDidChange();

#if ENABLE(MEDIA_STREAM)
    if (oldMediaCaptureState != newMediaCaptureState) {
        m_uiClient->mediaCaptureStateDidChange(m_mediaState);
        userMediaPermissionRequestManager().captureStateChanged(oldMediaCaptureState, newMediaCaptureState);
    }
#endif

    activityStateDidChange(ActivityState::IsAudible | ActivityState::IsCapturingMedia);

    playingMediaMask |= WebCore::MediaProducer::MediaCaptureMask;
    if ((oldState & playingMediaMask) != (m_mediaState & playingMediaMask))
        m_uiClient->isPlayingMediaDidChange(*this);

#if PLATFORM(MAC)
    if ((oldState & MediaProducer::HasAudioOrVideo) != (m_mediaState & MediaProducer::HasAudioOrVideo))
        videoControlsManagerDidChange();
#endif
}

#if PLATFORM(MAC)
void WebPageProxy::videoControlsManagerDidChange()
{
    m_pageClient.videoControlsManagerDidChange();
}

bool WebPageProxy::hasActiveVideoForControlsManager() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return m_playbackSessionManager && m_playbackSessionManager->controlsManagerInterface();
#else
    return false;
#endif
}

void WebPageProxy::requestControlledElementID() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_playbackSessionManager)
        m_playbackSessionManager->requestControlledElementID();
#endif
}

void WebPageProxy::handleControlledElementIDResponse(const String& identifier) const
{
    m_pageClient.handleControlledElementIDResponse(identifier);
}

bool WebPageProxy::isPlayingVideoInEnhancedFullscreen() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoFullscreenManager && m_videoFullscreenManager->isPlayingVideoInEnhancedFullscreen();
#else
    return false;
#endif
}
#endif

#if PLATFORM(COCOA)
void WebPageProxy::requestActiveNowPlayingSessionInfo(Ref<NowPlayingInfoCallback>&& callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    auto callbackID = callback->callbackID();
    m_callbacks.put(WTFMove(callback));

    m_process->send(Messages::WebPage::RequestActiveNowPlayingSessionInfo(callbackID), m_pageID);
}

void WebPageProxy::nowPlayingInfoCallback(bool hasActiveSession, bool registeredAsNowPlayingApplication, const String& title, double duration, double elapsedTime, uint64_t uniqueIdentifier, CallbackID callbackID)
{
    auto callback = m_callbacks.take<NowPlayingInfoCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(hasActiveSession, registeredAsNowPlayingApplication, title, duration, elapsedTime, uniqueIdentifier);
}
#endif

#if ENABLE(MEDIA_SESSION)
void WebPageProxy::hasMediaSessionWithActiveMediaElementsDidChange(bool state)
{
    m_hasMediaSessionWithActiveMediaElements = state;
}

void WebPageProxy::mediaSessionMetadataDidChange(const WebCore::MediaSessionMetadata& metadata)
{
    Ref<WebMediaSessionMetadata> webMetadata = WebMediaSessionMetadata::create(metadata);
    m_uiClient->mediaSessionMetadataDidChange(*this, webMetadata.ptr());
}

void WebPageProxy::focusedContentMediaElementDidChange(uint64_t elementID)
{
    WebMediaSessionFocusManager* focusManager = process().processPool().supplement<WebMediaSessionFocusManager>();
    ASSERT(focusManager);
    focusManager->setFocusedMediaElement(*this, elementID);
}
#endif

void WebPageProxy::handleAutoplayEvent(WebCore::AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    m_uiClient->handleAutoplayEvent(*this, event, flags);
}

#if PLATFORM(MAC)
void WebPageProxy::performImmediateActionHitTestAtLocation(FloatPoint point)
{
    m_process->send(Messages::WebPage::PerformImmediateActionHitTestAtLocation(point), m_pageID);
}

void WebPageProxy::immediateActionDidUpdate()
{
    m_process->send(Messages::WebPage::ImmediateActionDidUpdate(), m_pageID);
}

void WebPageProxy::immediateActionDidCancel()
{
    m_process->send(Messages::WebPage::ImmediateActionDidCancel(), m_pageID);
}

void WebPageProxy::immediateActionDidComplete()
{
    m_process->send(Messages::WebPage::ImmediateActionDidComplete(), m_pageID);
}

void WebPageProxy::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, const UserData& userData)
{
    m_pageClient.didPerformImmediateActionHitTest(result, contentPreventsDefault, m_process->transformHandlesToObjects(userData.object()).get());
}

void* WebPageProxy::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    return m_pageClient.immediateActionAnimationControllerForHitTestResult(hitTestResult, type, userData);
}

void WebPageProxy::installActivityStateChangeCompletionHandler(WTF::Function<void ()>&& completionHandler)
{
    if (!isValid()) {
        completionHandler();
        return;
    }

    auto voidCallback = VoidCallback::create([completionHandler = WTFMove(completionHandler)] (CallbackBase::Error) {
        completionHandler();
    }, m_process->throttler().backgroundActivityToken());
    auto callbackID = m_callbacks.put(WTFMove(voidCallback));
    m_nextActivityStateChangeCallbacks.append(callbackID);
}

void WebPageProxy::handleAcceptedCandidate(WebCore::TextCheckingResult acceptedCandidate)
{
    m_process->send(Messages::WebPage::HandleAcceptedCandidate(acceptedCandidate), m_pageID);
}

void WebPageProxy::didHandleAcceptedCandidate()
{
    m_pageClient.didHandleAcceptedCandidate();
}

void WebPageProxy::setHeaderBannerHeightForTesting(int height)
{
    m_process->send(Messages::WebPage::SetHeaderBannerHeightForTesting(height), m_pageID);
}

void WebPageProxy::setFooterBannerHeightForTesting(int height)
{
    m_process->send(Messages::WebPage::SetFooterBannerHeightForTesting(height), m_pageID);
}

#endif

void WebPageProxy::imageOrMediaDocumentSizeChanged(const WebCore::IntSize& newSize)
{
    m_uiClient->imageOrMediaDocumentSizeChanged(newSize);
}

void WebPageProxy::setShouldDispatchFakeMouseMoveEvents(bool shouldDispatchFakeMouseMoveEvents)
{
    m_process->send(Messages::WebPage::SetShouldDispatchFakeMouseMoveEvents(shouldDispatchFakeMouseMoveEvents), m_pageID);
}

void WebPageProxy::handleAutoFillButtonClick(const UserData& userData)
{
    m_uiClient->didClickAutoFillButton(*this, m_process->transformHandlesToObjects(userData.object()).get());
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
void WebPageProxy::addPlaybackTargetPickerClient(uint64_t contextId)
{
    m_pageClient.mediaSessionManager().addPlaybackTargetPickerClient(*this, contextId);
}

void WebPageProxy::removePlaybackTargetPickerClient(uint64_t contextId)
{
    m_pageClient.mediaSessionManager().removePlaybackTargetPickerClient(*this, contextId);
}

void WebPageProxy::showPlaybackTargetPicker(uint64_t contextId, const WebCore::FloatRect& rect, bool hasVideo)
{
    m_pageClient.mediaSessionManager().showPlaybackTargetPicker(*this, contextId, m_pageClient.rootViewToScreen(IntRect(rect)), hasVideo);
}

void WebPageProxy::playbackTargetPickerClientStateDidChange(uint64_t contextId, WebCore::MediaProducer::MediaStateFlags state)
{
    m_pageClient.mediaSessionManager().clientStateDidChange(*this, contextId, state);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    m_pageClient.mediaSessionManager().setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerState(const String& name, WebCore::MediaPlaybackTargetContext::State state)
{
    m_pageClient.mediaSessionManager().setMockMediaPlaybackTargetPickerState(name, state);
}

void WebPageProxy::setPlaybackTarget(uint64_t contextId, Ref<MediaPlaybackTarget>&& target)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::PlaybackTargetSelected(contextId, target->targetContext()), m_pageID);
}

void WebPageProxy::externalOutputDeviceAvailableDidChange(uint64_t contextId, bool available)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::PlaybackTargetAvailabilityDidChange(contextId, available), m_pageID);
}

void WebPageProxy::setShouldPlayToPlaybackTarget(uint64_t contextId, bool shouldPlay)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetShouldPlayToPlaybackTarget(contextId, shouldPlay), m_pageID);
}
#endif

void WebPageProxy::didExceedInactiveMemoryLimitWhileActive()
{
    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "%p - WebPageProxy::didExceedInactiveMemoryLimitWhileActive()", this);
    m_uiClient->didExceedBackgroundResourceLimitWhileInForeground(*this, kWKResourceLimitMemory);
}

void WebPageProxy::didExceedBackgroundCPULimitWhileInForeground()
{
    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "%p - WebPageProxy::didExceedBackgroundCPULimitWhileInForeground()", this);
    m_uiClient->didExceedBackgroundResourceLimitWhileInForeground(*this, kWKResourceLimitCPU);
}

void WebPageProxy::didChangeBackgroundColor()
{
    m_pageClient.didChangeBackgroundColor();
}

void WebPageProxy::clearWheelEventTestTrigger()
{
    if (!isValid())
        return;
    
    m_process->send(Messages::WebPage::ClearWheelEventTestTrigger(), m_pageID);
}

void WebPageProxy::callAfterNextPresentationUpdate(WTF::Function<void (CallbackBase::Error)>&& callback)
{
    if (!isValid() || !m_drawingArea) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    m_drawingArea->dispatchAfterEnsuringDrawing(WTFMove(callback));
}

void WebPageProxy::setShouldScaleViewToFitDocument(bool shouldScaleViewToFitDocument)
{
    if (m_shouldScaleViewToFitDocument == shouldScaleViewToFitDocument)
        return;

    m_shouldScaleViewToFitDocument = shouldScaleViewToFitDocument;

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetShouldScaleViewToFitDocument(shouldScaleViewToFitDocument), m_pageID);
}

void WebPageProxy::didRestoreScrollPosition()
{
    m_pageClient.didRestoreScrollPosition();
}

void WebPageProxy::getLoadDecisionForIcon(const WebCore::LinkIcon& icon, CallbackID loadIdentifier)
{
    m_iconLoadingClient->getLoadDecisionForIcon(icon, [this, protectedThis = RefPtr<WebPageProxy>(this), loadIdentifier](WTF::Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction) {
        if (!isValid()) {
            if (callbackFunction)
                callbackFunction(nullptr, CallbackBase::Error::Unknown);
            return;
        }

        bool decision = (bool)callbackFunction;
        auto newCallbackIdentifier = decision ? OptionalCallbackID(m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken())) : OptionalCallbackID();

        m_process->send(Messages::WebPage::DidGetLoadDecisionForIcon(decision, loadIdentifier, newCallbackIdentifier), m_pageID);
    });
}

void WebPageProxy::finishedLoadingIcon(CallbackID callbackID, const IPC::DataReference& data)
{
    dataCallback(data, callbackID);
}

void WebPageProxy::setResourceCachingDisabled(bool disabled)
{
    if (m_isResourceCachingDisabled == disabled)
        return;

    m_isResourceCachingDisabled = disabled;

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetResourceCachingDisabled(disabled), m_pageID);
}

WebCore::UserInterfaceLayoutDirection WebPageProxy::userInterfaceLayoutDirection()
{
    return m_pageClient.userInterfaceLayoutDirection();
}

void WebPageProxy::setUserInterfaceLayoutDirection(WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection)
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetUserInterfaceLayoutDirection(static_cast<uint32_t>(userInterfaceLayoutDirection)), m_pageID);
}

void WebPageProxy::hideValidationMessage()
{
#if PLATFORM(COCOA)
    m_validationBubble = nullptr;
#endif
}
    
#if ENABLE(POINTER_LOCK)
void WebPageProxy::requestPointerLock()
{
    ASSERT(!m_isPointerLockPending);
    ASSERT(!m_isPointerLocked);
    m_isPointerLockPending = true;

    if (!isViewVisible() || !(m_activityState & ActivityState::IsFocused)) {
        didDenyPointerLock();
        return;
    }
    m_uiClient->requestPointerLock(this);
}
    
void WebPageProxy::didAllowPointerLock()
{
    ASSERT(m_isPointerLockPending && !m_isPointerLocked);
    m_isPointerLocked = true;
    m_isPointerLockPending = false;
#if PLATFORM(MAC)
    CGDisplayHideCursor(CGMainDisplayID());
    CGAssociateMouseAndMouseCursorPosition(false);
#endif
    m_process->send(Messages::WebPage::DidAcquirePointerLock(), m_pageID);
}
    
void WebPageProxy::didDenyPointerLock()
{
    ASSERT(m_isPointerLockPending && !m_isPointerLocked);
    m_isPointerLockPending = false;
    m_process->send(Messages::WebPage::DidNotAcquirePointerLock(), m_pageID);
}

void WebPageProxy::requestPointerUnlock()
{
    if (m_isPointerLocked) {
#if PLATFORM(MAC)
        CGAssociateMouseAndMouseCursorPosition(true);
        CGDisplayShowCursor(CGMainDisplayID());
#endif
        m_uiClient->didLosePointerLock(this);
        m_process->send(Messages::WebPage::DidLosePointerLock(), m_pageID);
    }

    if (m_isPointerLockPending) {
        m_uiClient->didLosePointerLock(this);
        m_process->send(Messages::WebPage::DidNotAcquirePointerLock(), m_pageID);
    }

    m_isPointerLocked = false;
    m_isPointerLockPending = false;
}
#endif

void WebPageProxy::setURLSchemeHandlerForScheme(Ref<WebURLSchemeHandler>&& handler, const String& scheme)
{
    auto canonicalizedScheme = URLParser::maybeCanonicalizeScheme(scheme);
    ASSERT(canonicalizedScheme);
    ASSERT(!URLParser::isSpecialScheme(canonicalizedScheme.value()));

    auto schemeResult = m_urlSchemeHandlersByScheme.add(canonicalizedScheme.value(), handler.get());
    ASSERT_UNUSED(schemeResult, schemeResult.isNewEntry);

    auto identifier = handler->identifier();
    auto identifierResult = m_urlSchemeHandlersByIdentifier.add(identifier, WTFMove(handler));
    ASSERT_UNUSED(identifierResult, identifierResult.isNewEntry);

    m_process->send(Messages::WebPage::RegisterURLSchemeHandler(identifier, canonicalizedScheme.value()), m_pageID);
}

WebURLSchemeHandler* WebPageProxy::urlSchemeHandlerForScheme(const String& scheme)
{
    return scheme.isNull() ? nullptr : m_urlSchemeHandlersByScheme.get(scheme);
}

void WebPageProxy::startURLSchemeTask(URLSchemeTaskParameters&& parameters)
{
    auto iterator = m_urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK(iterator != m_urlSchemeHandlersByIdentifier.end());

    iterator->value->startTask(*this, parameters.taskIdentifier, WTFMove(parameters.request));
}

void WebPageProxy::stopURLSchemeTask(uint64_t handlerIdentifier, uint64_t taskIdentifier)
{
    auto iterator = m_urlSchemeHandlersByIdentifier.find(handlerIdentifier);
    MESSAGE_CHECK(iterator != m_urlSchemeHandlersByIdentifier.end());

    iterator->value->stopTask(*this, taskIdentifier);
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
void WebPageProxy::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, uint64_t webProcessContextId)
{
    m_websiteDataStore->hasStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, [this, webProcessContextId] (bool hasAccess) {
        m_process->send(Messages::WebPage::StorageAccessResponse(hasAccess, webProcessContextId), m_pageID);
    });
}

void WebPageProxy::requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, uint64_t webProcessContextId)
{
    ASSERT(pageID == m_pageID);
    m_websiteDataStore->requestStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, [this, webProcessContextId] (bool wasGranted) {
        m_process->send(Messages::WebPage::StorageAccessResponse(wasGranted, webProcessContextId), m_pageID);
    });
}
#endif

#if PLATFORM(COCOA)
void WebPageProxy::touchBarMenuDataChanged(const TouchBarMenuData& touchBarMenuData)
{
    m_touchBarMenuData = touchBarMenuData;
}

void WebPageProxy::touchBarMenuItemDataAdded(const TouchBarMenuItemData& touchBarMenuItemData)
{
    m_touchBarMenuData.addMenuItem(touchBarMenuItemData);
}

void WebPageProxy::touchBarMenuItemDataRemoved(const TouchBarMenuItemData& touchBarMenuItemData)
{
    m_touchBarMenuData.removeMenuItem(touchBarMenuItemData);
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::insertAttachment(const String& identifier, const AttachmentDisplayOptions& options, const String& filename, std::optional<String> contentType, SharedBuffer& data, Function<void(CallbackBase::Error)>&& callback)
{
    if (!isValid()) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::InsertAttachment(identifier, options, filename, contentType, IPC::SharedBufferDataReference { &data }, callbackID), m_pageID);
}

void WebPageProxy::requestAttachmentInfo(const String& identifier, Function<void(const AttachmentInfo&, CallbackBase::Error)>&& callback)
{
    if (!isValid()) {
        callback({ }, CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::RequestAttachmentInfo(identifier, callbackID), m_pageID);
}

void WebPageProxy::setAttachmentDisplayOptions(const String& identifier, AttachmentDisplayOptions options, Function<void(CallbackBase::Error)>&& callback)
{
    if (!isValid()) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SetAttachmentDisplayOptions(identifier, options, callbackID), m_pageID);
}

void WebPageProxy::setAttachmentDataAndContentType(const String& identifier, SharedBuffer& data, std::optional<String>&& newContentType, std::optional<String>&& newFilename, Function<void(CallbackBase::Error)>&& callback)
{
    if (!isValid()) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SetAttachmentDataAndContentType(identifier, IPC::SharedBufferDataReference { &data }, WTFMove(newContentType), WTFMove(newFilename), callbackID), m_pageID);
}

void WebPageProxy::didInsertAttachment(const String& identifier, const String& source)
{
    m_pageClient.didInsertAttachment(identifier, source);
}

void WebPageProxy::didRemoveAttachment(const String& identifier)
{
    m_pageClient.didRemoveAttachment(identifier);
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::writeBlobToFilePath(const URL& url, const String& path, Function<void(bool success)>&& callback)
{
    if (!isValid()) {
        callback(false);
        return;
    }

    m_process->processPool().ensureNetworkProcess().writeBlobToFilePath(url, path, WTFMove(callback));
}

#if ENABLE(APPLICATION_MANIFEST)
void WebPageProxy::getApplicationManifest(Function<void(const std::optional<WebCore::ApplicationManifest>&, CallbackBase::Error)>&& callbackFunction)
{
    if (!isValid()) {
        callbackFunction(std::nullopt, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetApplicationManifest(callbackID), m_pageID);
}
#endif

} // namespace WebKit
