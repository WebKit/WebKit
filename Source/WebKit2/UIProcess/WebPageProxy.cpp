/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "APIFindClient.h"
#include "APIFormClient.h"
#include "APILoaderClient.h"
#include "APIPolicyClient.h"
#include "APIUIClient.h"
#include "APIURLRequest.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "DataReference.h"
#include "DownloadProxy.h"
#include "DrawingAreaProxy.h"
#include "DrawingAreaProxyMessages.h"
#include "EventDispatcherMessages.h"
#include "Logging.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "NavigationActionData.h"
#include "NotificationPermissionRequest.h"
#include "NotificationPermissionRequestManager.h"
#include "PageClient.h"
#include "PluginInformation.h"
#include "PluginProcessManager.h"
#include "PrintInfo.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "WKContextPrivate.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCertificateInfo.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebContextUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebEditCommandProxy.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFullScreenManagerProxy.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebInspectorProxy.h"
#include "WebInspectorProxyMessages.h"
#include "WebNotificationManagerProxy.h"
#include "WebOpenPanelParameters.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroup.h"
#include "WebPageGroupData.h"
#include "WebPageMessages.h"
#include "WebPageProxyMessages.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include "WebPreferences.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include "WebSecurityOrigin.h"
#include "WebUserContentControllerProxy.h"
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/WindowFeatures.h>
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

#if ENABLE(ASYNC_SCROLLING)
#include "RemoteScrollingCoordinatorProxy.h"
#endif

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedLayerTreeHostProxyMessages.h"
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#endif

#if USE(SOUP) && !ENABLE(CUSTOM_PROTOCOLS)
#include "WebSoupRequestManagerProxy.h"
#endif

#if ENABLE(VIBRATION)
#include "WebVibrationProxy.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessMessages.h"
#endif

#if PLATFORM(COCOA)
#include "ViewSnapshotStore.h"
#include <WebCore/RunLoopObserver.h>
#endif

#if PLATFORM(IOS)
#include "WebVideoFullscreenManagerProxy.h"
#include "WebVideoFullscreenManagerProxyMessages.h"
#endif

#if USE(CAIRO)
#include <WebCore/CairoUtilities.h>
#endif

// This controls what strategy we use for mouse wheel coalescing.
#define MERGE_WHEEL_EVENTS 1

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_process->connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(m_process->checkURLReceivedFromWebProcess(url), m_process->connection())

using namespace WebCore;

// Represents the number of wheel events we can hold in the queue before we start pushing them preemptively.
static const unsigned wheelEventQueueSizeThreshold = 10;

namespace WebKit {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageProxyCounter, ("WebPageProxy"));

class ExceededDatabaseQuotaRecords {
    WTF_MAKE_NONCOPYABLE(ExceededDatabaseQuotaRecords); WTF_MAKE_FAST_ALLOCATED;
    friend class NeverDestroyed<ExceededDatabaseQuotaRecords>;
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

    static ExceededDatabaseQuotaRecords& shared();

    std::unique_ptr<Record> createRecord(uint64_t frameID, String originIdentifier,
        String databaseName, String displayName, uint64_t currentQuota,
        uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, 
        PassRefPtr<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply>);

    void add(std::unique_ptr<Record>);
    bool areBeingProcessed() const { return !!m_currentRecord; }
    Record* next();

private:
    ExceededDatabaseQuotaRecords() { }
    ~ExceededDatabaseQuotaRecords() { }

    Deque<std::unique_ptr<Record>> m_records;
    std::unique_ptr<Record> m_currentRecord;
};

ExceededDatabaseQuotaRecords& ExceededDatabaseQuotaRecords::shared()
{
    static NeverDestroyed<ExceededDatabaseQuotaRecords> records;
    return records;
}

std::unique_ptr<ExceededDatabaseQuotaRecords::Record> ExceededDatabaseQuotaRecords::createRecord(
    uint64_t frameID, String originIdentifier, String databaseName, String displayName,
    uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage,
    uint64_t expectedUsage, PassRefPtr<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply> reply)
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
    record->reply = reply;
    return WTF::move(record);
}

void ExceededDatabaseQuotaRecords::add(std::unique_ptr<ExceededDatabaseQuotaRecords::Record> record)
{
    m_records.append(WTF::move(record));
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

PassRefPtr<WebPageProxy> WebPageProxy::create(PageClient& pageClient, WebProcessProxy& process, uint64_t pageID, const WebPageConfiguration& configuration)
{
    return adoptRef(new WebPageProxy(pageClient, process, pageID, configuration));
}

WebPageProxy::WebPageProxy(PageClient& pageClient, WebProcessProxy& process, uint64_t pageID, const WebPageConfiguration& configuration)
    : m_pageClient(pageClient)
    , m_loaderClient(std::make_unique<API::LoaderClient>())
    , m_policyClient(std::make_unique<API::PolicyClient>())
    , m_formClient(std::make_unique<API::FormClient>())
    , m_uiClient(std::make_unique<API::UIClient>())
    , m_findClient(std::make_unique<API::FindClient>())
    , m_process(process)
    , m_pageGroup(*configuration.pageGroup)
    , m_preferences(*configuration.preferences)
    , m_userContentController(configuration.userContentController)
    , m_visitedLinkProvider(*configuration.visitedLinkProvider)
    , m_mainFrame(nullptr)
    , m_userAgent(standardUserAgent())
#if PLATFORM(IOS)
    , m_deviceOrientation(0)
    , m_dynamicViewportSizeUpdateWaitingForTarget(false)
    , m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit(false)
    , m_dynamicViewportSizeUpdateLayerTreeTransactionID(0)
#endif
    , m_geolocationPermissionRequestManager(*this)
    , m_notificationPermissionRequestManager(*this)
    , m_viewState(ViewState::NoFlags)
    , m_viewWasEverInWindow(false)
    , m_backForwardList(WebBackForwardList::create(*this))
    , m_maintainsInactiveSelection(false)
    , m_loadStateAtProcessExit(FrameLoadState::State::Finished)
#if PLATFORM(MAC) && !USE(ASYNC_NSTEXTINPUTCLIENT)
    , m_temporarilyClosedComposition(false)
#endif
    , m_textZoomFactor(1)
    , m_pageZoomFactor(1)
    , m_pageScaleFactor(1)
    , m_intrinsicDeviceScaleFactor(1)
    , m_customDeviceScaleFactor(0)
    , m_topContentInset(0)
    , m_layerHostingMode(LayerHostingMode::InProcess)
    , m_drawsBackground(true)
    , m_drawsTransparentBackground(false)
    , m_useFixedLayout(false)
    , m_suppressScrollbarAnimations(false)
    , m_paginationMode(Pagination::Unpaginated)
    , m_paginationBehavesLikeColumns(false)
    , m_pageLength(0)
    , m_gapBetweenPages(0)
    , m_isValid(true)
    , m_isClosed(false)
    , m_canRunModal(false)
    , m_isInPrintingMode(false)
    , m_isPerformingDOMPrintOperation(false)
    , m_inDecidePolicyForResponseSync(false)
    , m_decidePolicyForResponseRequest(0)
    , m_syncMimeTypePolicyActionIsValid(false)
    , m_syncMimeTypePolicyAction(PolicyUse)
    , m_syncMimeTypePolicyDownloadID(0)
    , m_inDecidePolicyForNavigationAction(false)
    , m_syncNavigationActionPolicyActionIsValid(false)
    , m_syncNavigationActionPolicyAction(PolicyUse)
    , m_syncNavigationActionPolicyDownloadID(0)
    , m_processingMouseMoveEvent(false)
#if ENABLE(TOUCH_EVENTS)
    , m_isTrackingTouchEvents(false)
#endif
    , m_pageID(pageID)
    , m_session(*configuration.session)
    , m_isPageSuspended(false)
    , m_addsVisitedLinks(true)
#if ENABLE(REMOTE_INSPECTOR)
    , m_allowsRemoteInspection(true)
#endif
#if PLATFORM(COCOA)
    , m_isSmartInsertDeleteEnabled(TextChecker::isSmartInsertDeleteEnabled())
#endif
    , m_spellDocumentTag(0)
    , m_hasSpellDocumentTag(false)
    , m_pendingLearnOrIgnoreWordMessageCount(0)
    , m_mainFrameHasCustomContentProvider(false)
#if ENABLE(DRAG_SUPPORT)
    , m_currentDragOperation(DragOperationNone)
    , m_currentDragIsOverFileInput(false)
    , m_currentDragNumberOfFilesToBeAccepted(0)
#endif
    , m_pageLoadState(*this)
    , m_delegatesScrolling(false)
    , m_mainFrameHasHorizontalScrollbar(false)
    , m_mainFrameHasVerticalScrollbar(false)
    , m_canShortCircuitHorizontalWheelEvents(true)
    , m_mainFrameIsPinnedToLeftSide(true)
    , m_mainFrameIsPinnedToRightSide(true)
    , m_mainFrameIsPinnedToTopSide(true)
    , m_mainFrameIsPinnedToBottomSide(true)
    , m_shouldUseImplicitRubberBandControl(false)
    , m_rubberBandsAtLeft(true)
    , m_rubberBandsAtRight(true)
    , m_rubberBandsAtTop(true)
    , m_rubberBandsAtBottom(true)
    , m_enableVerticalRubberBanding(true)
    , m_enableHorizontalRubberBanding(true)
    , m_backgroundExtendsBeyondPage(false)
    , m_shouldRecordNavigationSnapshots(false)
    , m_isShowingNavigationGestureSnapshot(false)
    , m_pageCount(0)
    , m_renderTreeSize(0)
    , m_shouldSendEventsSynchronously(false)
    , m_suppressVisibilityUpdates(false)
    , m_autoSizingShouldExpandToViewHeight(false)
    , m_mediaVolume(1)
    , m_mayStartMediaWhenInWindow(true)
    , m_scrollPinningBehavior(DoNotPin)
    , m_navigationID(0)
    , m_configurationPreferenceValues(configuration.preferenceValues)
    , m_potentiallyChangedViewStateFlags(ViewState::NoFlags)
    , m_viewStateChangeWantsSynchronousReply(false)
{
    if (m_process->state() == WebProcessProxy::State::Running) {
        if (m_userContentController)
            m_process->addWebUserContentControllerProxy(*m_userContentController);
        m_process->addVisitedLinkProvider(m_visitedLinkProvider.get());
    }

    updateViewState();
    updateActivityToken();
    
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    m_layerHostingMode = m_viewState & ViewState::IsInWindow ? m_pageClient.viewLayerHostingMode() : LayerHostingMode::OutOfProcess;
#endif

    platformInitialize();

#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif

    WebContext::statistics().wkPageCount++;

    m_preferences->addPage(*this);
    m_pageGroup->addPage(this);

#if ENABLE(INSPECTOR)
    m_inspector = WebInspectorProxy::create(this);
#endif
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = WebFullScreenManagerProxy::create(*this, m_pageClient.fullScreenManagerProxyClient());
#endif
#if PLATFORM(IOS)
    m_videoFullscreenManager = WebVideoFullscreenManagerProxy::create(*this);
#endif
#if ENABLE(VIBRATION)
    m_vibration = WebVibrationProxy::create(this);
#endif

    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID, *this);

    // FIXME: If we ever expose the session storage size as a preference, we need to pass it here.
    IPC::Connection* connection = m_process->state() == WebProcessProxy::State::Running ? m_process->connection() : nullptr;
    m_process->context().storageManager().createSessionStorageNamespace(m_pageID, connection, std::numeric_limits<unsigned>::max());
    setSession(*configuration.session);

#if PLATFORM(COCOA)
    const CFIndex viewStateChangeRunLoopOrder = (CFIndex)RunLoopObserver::WellKnownRunLoopOrders::CoreAnimationCommit - 1;
    m_viewStateChangeDispatcher = RunLoopObserver::create(viewStateChangeRunLoopOrder, [this]() {
        this->dispatchViewStateChange();
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

    WebContext::statistics().wkPageCount--;

    if (m_hasSpellDocumentTag)
        TextChecker::closeSpellDocumentWithTag(m_spellDocumentTag);

    m_preferences->removePage(*this);
    m_pageGroup->removePage(this);

#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif
}

PlatformProcessIdentifier WebPageProxy::processIdentifier() const
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
    if (&preferences == &m_preferences.get())
        return;

    m_preferences->removePage(*this);
    m_preferences = preferences;
    m_preferences->addPage(*this);

    preferencesDidChange();
}

void WebPageProxy::setLoaderClient(std::unique_ptr<API::LoaderClient> loaderClient)
{
    if (!loaderClient) {
        m_loaderClient = std::make_unique<API::LoaderClient>();
        return;
    }

    m_loaderClient = WTF::move(loaderClient);
}

void WebPageProxy::setPolicyClient(std::unique_ptr<API::PolicyClient> policyClient)
{
    if (!policyClient) {
        m_policyClient = std::make_unique<API::PolicyClient>();
        return;
    }

    m_policyClient = WTF::move(policyClient);
}

void WebPageProxy::setFormClient(std::unique_ptr<API::FormClient> formClient)
{
    if (!formClient) {
        m_formClient = std::make_unique<API::FormClient>();
        return;
    }

    m_formClient = WTF::move(formClient);
}

void WebPageProxy::setUIClient(std::unique_ptr<API::UIClient> uiClient)
{
    if (!uiClient) {
        m_uiClient = std::make_unique<API::UIClient>();
        return;
    }

    m_uiClient = WTF::move(uiClient);

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetCanRunBeforeUnloadConfirmPanel(m_uiClient->canRunBeforeUnloadConfirmPanel()), m_pageID);
    setCanRunModal(m_uiClient->canRunModal());
}

void WebPageProxy::setFindClient(std::unique_ptr<API::FindClient> findClient)
{
    if (!findClient) {
        m_findClient = std::make_unique<API::FindClient>();
        return;
    }
    
    m_findClient = WTF::move(findClient);
}

void WebPageProxy::initializeFindMatchesClient(const WKPageFindMatchesClientBase* client)
{
    m_findMatchesClient.initialize(client);
}

#if ENABLE(CONTEXT_MENUS)
void WebPageProxy::initializeContextMenuClient(const WKPageContextMenuClientBase* client)
{
    m_contextMenuClient.initialize(client);
}
#endif

void WebPageProxy::reattachToWebProcess()
{
    ASSERT(!m_isClosed);
    ASSERT(!isValid());
    ASSERT(m_process->state() == WebProcessProxy::State::Terminated);

    m_isValid = true;
    m_process->removeWebPage(m_pageID);
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID);

    if (m_process->context().processModel() == ProcessModelSharedSecondaryProcess)
        m_process = m_process->context().ensureSharedWebProcess();
    else
        m_process = m_process->context().createNewWebProcessRespectingProcessCountLimit();

    ASSERT(m_process->state() != ChildProcessProxy::State::Terminated);
    if (m_process->state() == ChildProcessProxy::State::Running)
        processDidFinishLaunching();
    m_process->addExistingWebPage(this, m_pageID);
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID, *this);

    updateViewState();
    updateActivityToken();

#if ENABLE(INSPECTOR)
    m_inspector = WebInspectorProxy::create(this);
#endif
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = WebFullScreenManagerProxy::create(*this, m_pageClient.fullScreenManagerProxyClient());
#endif
#if PLATFORM(IOS)
    m_videoFullscreenManager = WebVideoFullscreenManagerProxy::create(*this);
#endif

    initializeWebPage();

    m_pageClient.didRelaunchProcess();
    m_drawingArea->waitForBackingStoreUpdateOnNextPaint();
}

uint64_t WebPageProxy::reattachToWebProcessWithItem(WebBackForwardListItem* item)
{
    if (m_isClosed)
        return 0;

    if (item && item != m_backForwardList->currentItem())
        m_backForwardList->goToItem(item);

    ASSERT(!isValid());
    reattachToWebProcess();

    if (!item)
        return 0;

    uint64_t navigationID = generateNavigationID();

    m_process->send(Messages::WebPage::GoToBackForwardItem(navigationID, item->itemID()), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

void WebPageProxy::setSession(API::Session& session)
{
    m_session = session;
    m_process->send(Messages::WebPage::SetSessionID(session.getID()), m_pageID);

#if ENABLE(NETWORK_PROCESS)
    if (session.isEphemeral())
        m_process->context().sendToNetworkingProcess(Messages::NetworkProcess::EnsurePrivateBrowsingSession(session.getID()));
#endif
}

void WebPageProxy::initializeWebPage()
{
    ASSERT(isValid());

    BackForwardListItemVector items = m_backForwardList->entries();
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

#if ENABLE(INSPECTOR_SERVER)
    if (pageGroup().preferences().developerExtrasEnabled())
        inspector()->enableRemoteInspection();
#endif

    process().send(Messages::WebProcess::CreateWebPage(m_pageID, creationParameters()), 0);

#if PLATFORM(COCOA)
    send(Messages::WebPage::SetSmartInsertDeleteEnabled(m_isSmartInsertDeleteEnabled));
#endif
}

bool WebPageProxy::isProcessSuppressible() const
{
    return (m_viewState & ViewState::IsVisuallyIdle) && m_preferences->pageVisibilityBasedProcessSuppressionEnabled();
}

void WebPageProxy::close()
{
    if (m_isClosed)
        return;

    m_isClosed = true;

    m_backForwardList->pageClosed();
    m_pageClient.pageClosed();

    m_process->disconnectFramesFromPage(this);

    resetState(ResetStateReason::PageInvalidated);

    m_loaderClient = std::make_unique<API::LoaderClient>();
    m_policyClient = std::make_unique<API::PolicyClient>();
    m_formClient = std::make_unique<API::FormClient>();
    m_uiClient = std::make_unique<API::UIClient>();
#if PLATFORM(EFL)
    m_uiPopupMenuClient.initialize(0);
#endif
    m_findClient = std::make_unique<API::FindClient>();
    m_findMatchesClient.initialize(0);
#if ENABLE(CONTEXT_MENUS)
    m_contextMenuClient.initialize(0);
#endif

    m_process->send(Messages::WebPage::Close(), m_pageID);
    m_process->removeWebPage(m_pageID);
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID);
    m_process->context().storageManager().destroySessionStorageNamespace(m_pageID);
    m_process->context().supplement<WebNotificationManagerProxy>()->clearNotifications(this);
}

bool WebPageProxy::tryClose()
{
    if (!isValid())
        return true;

    m_process->send(Messages::WebPage::TryClose(), m_pageID);
    m_process->responsivenessTimer()->start();
    return false;
}

bool WebPageProxy::maybeInitializeSandboxExtensionHandle(const URL& url, SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (!url.isLocalFile())
        return false;

    if (m_process->hasAssumedReadAccessToURL(url))
        return false;

#if ENABLE(INSPECTOR)
    // Inspector resources are in a directory with assumed access.
    ASSERT_WITH_SECURITY_IMPLICATION(!WebInspectorProxy::isInspectorPage(*this));
#endif

    SandboxExtension::createHandle("/", SandboxExtension::ReadOnly, sandboxExtensionHandle);
    return true;
}

uint64_t WebPageProxy::loadRequest(const ResourceRequest& request, API::Object* userData)
{
    if (m_isClosed)
        return 0;

    uint64_t navigationID = generateNavigationID();

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, request.url());

    if (!isValid())
        reattachToWebProcess();

    SandboxExtension::Handle sandboxExtensionHandle;
    bool createdExtension = maybeInitializeSandboxExtensionHandle(request.url(), sandboxExtensionHandle);
    if (createdExtension)
        m_process->willAcquireUniversalFileReadSandboxExtension();
    m_process->send(Messages::WebPage::LoadRequest(navigationID, request, sandboxExtensionHandle, WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

void WebPageProxy::loadFile(const String& fileURLString, const String& resourceDirectoryURLString, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    URL fileURL = URL(URL(), fileURLString);
    if (!fileURL.isLocalFile())
        return;

    URL resourceDirectoryURL;
    if (resourceDirectoryURLString.isNull())
        resourceDirectoryURL = URL(ParsedURLString, ASCIILiteral("file:///"));
    else {
        resourceDirectoryURL = URL(URL(), resourceDirectoryURLString);
        if (!resourceDirectoryURL.isLocalFile())
            return;
    }

    String resourceDirectoryPath = resourceDirectoryURL.fileSystemPath();

    SandboxExtension::Handle sandboxExtensionHandle;
    SandboxExtension::createHandle(resourceDirectoryPath, SandboxExtension::ReadOnly, sandboxExtensionHandle);
    m_process->assumeReadAccessToBaseURL(resourceDirectoryURL);
    m_process->send(Messages::WebPage::LoadRequest(generateNavigationID(), fileURL, sandboxExtensionHandle, WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();
}

void WebPageProxy::loadData(API::Data* data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    m_process->assumeReadAccessToBaseURL(baseURL);
    m_process->send(Messages::WebPage::LoadData(data->dataReference(), MIMEType, encoding, baseURL, WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();
}

uint64_t WebPageProxy::loadHTMLString(const String& htmlString, const String& baseURL, API::Object* userData)
{
    if (m_isClosed)
        return 0;

    uint64_t navigationID = generateNavigationID();

    auto transaction = m_pageLoadState.transaction();

    String pendingAPIRequestURL = baseURL.isEmpty() ? baseURL : ASCIILiteral("about:blank");
    m_pageLoadState.setPendingAPIRequestURL(transaction, pendingAPIRequestURL);

    if (!isValid())
        reattachToWebProcess();

    m_process->assumeReadAccessToBaseURL(baseURL);
    m_process->send(Messages::WebPage::LoadHTMLString(navigationID, htmlString, baseURL, WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

void WebPageProxy::loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setUnreachableURL(transaction, unreachableURL);

    if (m_mainFrame)
        m_mainFrame->setUnreachableURL(unreachableURL);

    m_process->assumeReadAccessToBaseURL(baseURL);
    m_process->send(Messages::WebPage::LoadAlternateHTMLString(htmlString, baseURL, unreachableURL, WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();
}

void WebPageProxy::loadPlainTextString(const String& string, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    m_process->send(Messages::WebPage::LoadPlainTextString(string, WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();
}

void WebPageProxy::loadWebArchiveData(API::Data* webArchiveData, API::Object* userData)
{
    if (m_isClosed)
        return;

    if (!isValid())
        reattachToWebProcess();

    m_process->send(Messages::WebPage::LoadWebArchiveData(webArchiveData->dataReference(), WebContextUserMessageEncoder(userData, process())), m_pageID);
    m_process->responsivenessTimer()->start();
}

void WebPageProxy::stopLoading()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::StopLoading(), m_pageID);
    m_process->responsivenessTimer()->start();
}

uint64_t WebPageProxy::reload(bool reloadFromOrigin)
{
    SandboxExtension::Handle sandboxExtensionHandle;

    if (m_backForwardList->currentItem()) {
        String url = m_backForwardList->currentItem()->url();
        auto transaction = m_pageLoadState.transaction();
        m_pageLoadState.setPendingAPIRequestURL(transaction, url);

        // We may not have an extension yet if back/forward list was reinstated after a WebProcess crash or a browser relaunch
        bool createdExtension = maybeInitializeSandboxExtensionHandle(URL(URL(), url), sandboxExtensionHandle);
        if (createdExtension)
            m_process->willAcquireUniversalFileReadSandboxExtension();
    }

    if (!isValid())
        return reattachToWebProcessWithItem(m_backForwardList->currentItem());

    uint64_t navigationID = generateNavigationID();

    m_process->send(Messages::WebPage::Reload(navigationID, reloadFromOrigin, sandboxExtensionHandle), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

void WebPageProxy::recordNavigationSnapshot()
{
    if (!m_shouldRecordNavigationSnapshots)
        return;

#if PLATFORM(COCOA)
    ViewSnapshotStore::shared().recordSnapshot(*this);
#endif
}

uint64_t WebPageProxy::goForward()
{
    WebBackForwardListItem* forwardItem = m_backForwardList->forwardItem();
    if (!forwardItem)
        return 0;

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, forwardItem->url());

    if (!isValid())
        return reattachToWebProcessWithItem(forwardItem);

    uint64_t navigationID = generateNavigationID();

    m_process->send(Messages::WebPage::GoForward(navigationID, forwardItem->itemID()), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

uint64_t WebPageProxy::goBack()
{
    WebBackForwardListItem* backItem = m_backForwardList->backItem();
    if (!backItem)
        return 0;

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, backItem->url());

    if (!isValid())
        return reattachToWebProcessWithItem(backItem);

    uint64_t navigationID = generateNavigationID();

    m_process->send(Messages::WebPage::GoBack(navigationID, backItem->itemID()), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

uint64_t WebPageProxy::goToBackForwardItem(WebBackForwardListItem* item)
{
    if (!isValid())
        return reattachToWebProcessWithItem(item);

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequestURL(transaction, item->url());

    uint64_t navigationID = generateNavigationID();

    m_process->send(Messages::WebPage::GoToBackForwardItem(navigationID, item->itemID()), m_pageID);
    m_process->responsivenessTimer()->start();

    return navigationID;
}

void WebPageProxy::tryRestoreScrollPosition()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::TryRestoreScrollPosition(), m_pageID);
}

void WebPageProxy::didChangeBackForwardList(WebBackForwardListItem* added, Vector<RefPtr<WebBackForwardListItem>> removed)
{
    m_loaderClient->didChangeBackForwardList(this, added, WTF::move(removed));

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setCanGoBack(transaction, m_backForwardList->backItem());
    m_pageLoadState.setCanGoForward(transaction, m_backForwardList->forwardItem());
}

void WebPageProxy::willGoToBackForwardListItem(uint64_t itemID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    if (WebBackForwardListItem* item = m_process->webBackForwardItem(itemID))
        m_loaderClient->willGoToBackForwardListItem(this, item, userData.get());
}

bool WebPageProxy::shouldKeepCurrentBackForwardListItemInList(WebBackForwardListItem* item)
{
    return m_loaderClient->shouldKeepCurrentBackForwardListItemInList(this, item);
}

bool WebPageProxy::canShowMIMEType(const String& mimeType)
{
    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

#if ENABLE(NETSCAPE_PLUGIN_API)
    String newMimeType = mimeType;
    PluginModuleInfo plugin = m_process->context().pluginInfoStore().findPlugin(newMimeType, URL());
    if (!plugin.path.isNull() && m_preferences->pluginsEnabled())
        return true;
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if PLATFORM(COCOA)
    // On Mac, we can show PDFs.
    if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(mimeType) && !WebContext::omitPDFSupport())
        return true;
#endif // PLATFORM(COCOA)

    return false;
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
#endif

void WebPageProxy::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    if (isValid())
        m_process->send(Messages::WebPage::SetDrawsBackground(drawsBackground), m_pageID);
}

void WebPageProxy::setDrawsTransparentBackground(bool drawsTransparentBackground)
{
    if (m_drawsTransparentBackground == drawsTransparentBackground)
        return;

    m_drawsTransparentBackground = drawsTransparentBackground;

    if (isValid())
        m_process->send(Messages::WebPage::SetDrawsTransparentBackground(drawsTransparentBackground), m_pageID);
}

void WebPageProxy::setTopContentInset(float contentInset)
{
    if (m_topContentInset == contentInset)
        return;

    m_topContentInset = contentInset;

    if (isValid())
        m_process->send(Messages::WebPage::SetTopContentInset(contentInset), m_pageID);
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
#if ENABLE(INPUT_TYPE_COLOR_POPOVER)
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

void WebPageProxy::setViewNeedsDisplay(const IntRect& rect)
{
    m_pageClient.setViewNeedsDisplay(rect);
}

void WebPageProxy::displayView()
{
    m_pageClient.displayView();
}

bool WebPageProxy::canScrollView()
{
    return m_pageClient.canScrollView();
}

void WebPageProxy::scrollView(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    m_pageClient.scrollView(scrollRect, scrollOffset);
}

void WebPageProxy::requestScroll(const FloatPoint& scrollPosition, bool isProgrammaticScroll)
{
    m_pageClient.requestScroll(scrollPosition, isProgrammaticScroll);
}

void WebPageProxy::updateViewState(ViewState::Flags flagsToUpdate)
{
    m_viewState &= ~flagsToUpdate;
    if (flagsToUpdate & ViewState::IsFocused && m_pageClient.isViewFocused())
        m_viewState |= ViewState::IsFocused;
    if (flagsToUpdate & ViewState::WindowIsActive && m_pageClient.isViewWindowActive())
        m_viewState |= ViewState::WindowIsActive;
    if (flagsToUpdate & ViewState::IsVisible && m_pageClient.isViewVisible())
        m_viewState |= ViewState::IsVisible;
    if (flagsToUpdate & ViewState::IsVisibleOrOccluded && m_pageClient.isViewVisibleOrOccluded())
        m_viewState |= ViewState::IsVisibleOrOccluded;
    if (flagsToUpdate & ViewState::IsInWindow && m_pageClient.isViewInWindow()) {
        m_viewState |= ViewState::IsInWindow;
        m_viewWasEverInWindow = true;
    }
    if (flagsToUpdate & ViewState::IsVisuallyIdle && m_pageClient.isVisuallyIdle())
        m_viewState |= ViewState::IsVisuallyIdle;
}

void WebPageProxy::installViewStateChangeCompletionHandler(void (^completionHandler)())
{
    if (!isValid()) {
        completionHandler();
        return;
    }

    auto copiedCompletionHandler = Block_copy(completionHandler);
    RefPtr<VoidCallback> voidCallback = VoidCallback::create([copiedCompletionHandler] (CallbackBase::Error) {
        copiedCompletionHandler();
        Block_release(copiedCompletionHandler);
    }, std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    uint64_t callbackID = m_callbacks.put(voidCallback.release());
    m_nextViewStateChangeCallbacks.append(callbackID);
}

void WebPageProxy::viewStateDidChange(ViewState::Flags mayHaveChanged, bool wantsSynchronousReply, ViewStateChangeDispatchMode dispatchMode)
{
    m_potentiallyChangedViewStateFlags |= mayHaveChanged;
    m_viewStateChangeWantsSynchronousReply = m_viewStateChangeWantsSynchronousReply || wantsSynchronousReply;

#if PLATFORM(COCOA)
    bool isNewlyInWindow = !isInWindow() && (mayHaveChanged & ViewState::IsInWindow) && m_pageClient.isViewInWindow();
    if (dispatchMode == ViewStateChangeDispatchMode::Immediate || isNewlyInWindow) {
        dispatchViewStateChange();
        return;
    }
    m_viewStateChangeDispatcher->schedule();
#else
    dispatchViewStateChange();
#endif
}

void WebPageProxy::viewDidLeaveWindow()
{
#if ENABLE(INPUT_TYPE_COLOR_POPOVER)
    // When leaving the current page, close the popover color well.
    if (m_colorPicker)
        endColorPicker();
#endif
#if PLATFORM(IOS)
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
        m_process->send(Messages::WebPage::SetLayerHostingMode(static_cast<unsigned>(layerHostingMode)), m_pageID);
    }
}

void WebPageProxy::dispatchViewStateChange()
{
#if PLATFORM(COCOA)
    m_viewStateChangeDispatcher->invalidate();
#endif

    if (!isValid())
        return;

    // If the visibility state may have changed, then so may the visually idle & occluded agnostic state.
    if (m_potentiallyChangedViewStateFlags & ViewState::IsVisible)
        m_potentiallyChangedViewStateFlags |= ViewState::IsVisibleOrOccluded | ViewState::IsVisuallyIdle;

    // Record the prior view state, update the flags that may have changed,
    // and check which flags have actually changed.
    ViewState::Flags previousViewState = m_viewState;
    updateViewState(m_potentiallyChangedViewStateFlags);
    ViewState::Flags changed = m_viewState ^ previousViewState;

    // We always want to wait for the Web process to reply if we've been in-window before and are coming back in-window.
    if (m_viewWasEverInWindow && (changed & ViewState::IsInWindow) && isInWindow())
        m_viewStateChangeWantsSynchronousReply = true;

    if (changed || m_viewStateChangeWantsSynchronousReply)
        m_process->send(Messages::WebPage::SetViewState(m_viewState, m_viewStateChangeWantsSynchronousReply, m_nextViewStateChangeCallbacks), m_pageID);

    m_nextViewStateChangeCallbacks.clear();

    // This must happen after the SetViewState message is sent, to ensure the page visibility event can fire.
    updateActivityToken();

    if (changed & ViewState::IsVisuallyIdle)
        m_process->pageSuppressibilityChanged(this);

    // If we've started the responsiveness timer as part of telling the web process to update the backing store
    // state, it might not send back a reply (since it won't paint anything if the web page is hidden) so we
    // stop the unresponsiveness timer here.
    if ((changed & ViewState::IsVisible) && !isViewVisible())
        m_process->responsivenessTimer()->stop();

    if (changed & ViewState::IsInWindow) {
        if (isInWindow())
            viewDidEnterWindow();
        else
            viewDidLeaveWindow();
    }

    updateBackingStoreDiscardableState();

    if (m_viewStateChangeWantsSynchronousReply)
        waitForDidUpdateViewState();

    m_potentiallyChangedViewStateFlags = ViewState::NoFlags;
    m_viewStateChangeWantsSynchronousReply = false;
}

void WebPageProxy::updateActivityToken()
{
#if PLATFORM(IOS)
    if (!isViewVisible())
        m_activityToken = nullptr;
    else if (!m_activityToken)
        m_activityToken = std::make_unique<ProcessThrottler::ForegroundActivityToken>(m_process->throttler());
#endif
}

void WebPageProxy::layerHostingModeDidChange()
{
    if (!isValid())
        return;

    LayerHostingMode layerHostingMode = m_pageClient.viewLayerHostingMode();
    if (m_layerHostingMode == layerHostingMode)
        return;

    m_layerHostingMode = layerHostingMode;
    m_process->send(Messages::WebPage::SetLayerHostingMode(static_cast<unsigned>(layerHostingMode)), m_pageID);
}

void WebPageProxy::waitForDidUpdateViewState()
{
    if (!isValid())
        return;

    if (m_process->state() != WebProcessProxy::State::Running)
        return;

    // If we have previously timed out with no response from the WebProcess, don't block the UIProcess again until it starts responding.
    if (m_waitingForDidUpdateViewState)
        return;

    m_waitingForDidUpdateViewState = true;

    m_drawingArea->waitForDidUpdateViewState();
}

IntSize WebPageProxy::viewSize() const
{
    return m_pageClient.viewSize();
}

void WebPageProxy::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& keyboardEvent)
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetInitialFocus(forward, isKeyboardEventValid, keyboardEvent), m_pageID);
}

void WebPageProxy::setWindowResizerSize(const IntSize& windowResizerSize)
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::SetWindowResizerSize(windowResizerSize), m_pageID);
}
    
void WebPageProxy::clearSelection()
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::ClearSelection(), m_pageID);
}

void WebPageProxy::validateCommand(const String& commandName, std::function<void (const String&, bool, int32_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), false, 0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::ValidateCommand(commandName, callbackID), m_pageID);
}

void WebPageProxy::setMaintainsInactiveSelection(bool newValue)
{
    m_maintainsInactiveSelection = newValue;
}
    
void WebPageProxy::executeEditCommand(const String& commandName)
{
    static NeverDestroyed<String> ignoreSpellingCommandName(ASCIILiteral("ignoreSpelling"));

    if (!isValid())
        return;

    if (commandName == ignoreSpellingCommandName)
        ++m_pendingLearnOrIgnoreWordMessageCount;

    m_process->send(Messages::WebPage::ExecuteEditCommand(commandName), m_pageID);
}

#if !PLATFORM(IOS)
void WebPageProxy::didCommitLayerTree(const RemoteLayerTreeTransaction&)
{
}
#endif

#if USE(TILED_BACKING_STORE)
void WebPageProxy::commitPageTransitionViewport()
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::CommitPageTransitionViewport(), m_pageID);
}
#endif

#if ENABLE(DRAG_SUPPORT)
void WebPageProxy::dragEntered(DragData& dragData, const String& dragStorageName)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    SandboxExtension::HandleArray sandboxExtensionHandleEmptyArray;
    performDragControllerAction(DragControllerActionEntered, dragData, dragStorageName, sandboxExtensionHandle, sandboxExtensionHandleEmptyArray);
}

void WebPageProxy::dragUpdated(DragData& dragData, const String& dragStorageName)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    SandboxExtension::HandleArray sandboxExtensionHandleEmptyArray;
    performDragControllerAction(DragControllerActionUpdated, dragData, dragStorageName, sandboxExtensionHandle, sandboxExtensionHandleEmptyArray);
}

void WebPageProxy::dragExited(DragData& dragData, const String& dragStorageName)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    SandboxExtension::HandleArray sandboxExtensionHandleEmptyArray;
    performDragControllerAction(DragControllerActionExited, dragData, dragStorageName, sandboxExtensionHandle, sandboxExtensionHandleEmptyArray);
}

void WebPageProxy::performDragOperation(DragData& dragData, const String& dragStorageName, const SandboxExtension::Handle& sandboxExtensionHandle, const SandboxExtension::HandleArray& sandboxExtensionsForUpload)
{
    performDragControllerAction(DragControllerActionPerformDragOperation, dragData, dragStorageName, sandboxExtensionHandle, sandboxExtensionsForUpload);
}

void WebPageProxy::performDragControllerAction(DragControllerAction action, DragData& dragData, const String& dragStorageName, const SandboxExtension::Handle& sandboxExtensionHandle, const SandboxExtension::HandleArray& sandboxExtensionsForUpload)
{
    if (!isValid())
        return;
#if PLATFORM(GTK)
    UNUSED_PARAM(dragStorageName);
    UNUSED_PARAM(sandboxExtensionHandle);
    UNUSED_PARAM(sandboxExtensionsForUpload);
    m_process->send(Messages::WebPage::PerformDragControllerAction(action, dragData), m_pageID);
#else
    m_process->send(Messages::WebPage::PerformDragControllerAction(action, dragData.clientPosition(), dragData.globalPosition(), dragData.draggingSourceOperationMask(), dragStorageName, dragData.flags(), sandboxExtensionHandle, sandboxExtensionsForUpload), m_pageID);
#endif
}

void WebPageProxy::didPerformDragControllerAction(uint64_t dragOperation, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted)
{
    MESSAGE_CHECK(dragOperation <= DragOperationDelete);

    m_currentDragOperation = static_cast<DragOperation>(dragOperation);
    m_currentDragIsOverFileInput = mouseIsOverFileInput;
    m_currentDragNumberOfFilesToBeAccepted = numberOfItemsToBeAccepted;
}

#if PLATFORM(GTK)
void WebPageProxy::startDrag(const DragData& dragData, const ShareableBitmap::Handle& dragImageHandle)
{
    RefPtr<ShareableBitmap> dragImage = 0;
    if (!dragImageHandle.isNull()) {
        dragImage = ShareableBitmap::create(dragImageHandle);
        if (!dragImage)
            return;
    }

    m_pageClient.startDrag(dragData, dragImage.release());
}
#endif

void WebPageProxy::dragEnded(const IntPoint& clientPosition, const IntPoint& globalPosition, uint64_t operation)
{
    if (!isValid())
        return;
    m_process->send(Messages::WebPage::DragEnded(clientPosition, globalPosition, operation), m_pageID);
}
#endif // ENABLE(DRAG_SUPPORT)

void WebPageProxy::handleMouseEvent(const NativeWebMouseEvent& event)
{
    if (!isValid())
        return;

    // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
    if (event.type() != WebEvent::MouseMove)
        m_process->responsivenessTimer()->start();
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

    if (m_shouldSendEventsSynchronously) {
        bool handled = false;
        m_process->sendSync(Messages::WebPage::MouseEventSyncForTesting(event), Messages::WebPage::MouseEventSyncForTesting::Reply(handled), m_pageID);
        didReceiveEvent(event.type(), handled);
    } else
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

    if (!m_currentlyProcessedWheelEvents.isEmpty()) {
        m_wheelEventQueue.append(event);
        if (m_wheelEventQueue.size() < wheelEventQueueSizeThreshold)
            return;
        // The queue has too many wheel events, so push a new event.
    }

    if (!m_wheelEventQueue.isEmpty()) {
        processNextQueuedWheelEvent();
        return;
    }

    auto coalescedWheelEvent = std::make_unique<Vector<NativeWebWheelEvent>>();
    coalescedWheelEvent->append(event);
    m_currentlyProcessedWheelEvents.append(WTF::move(coalescedWheelEvent));
    sendWheelEvent(event);
}

void WebPageProxy::processNextQueuedWheelEvent()
{
    auto nextCoalescedEvent = std::make_unique<Vector<NativeWebWheelEvent>>();
    WebWheelEvent nextWheelEvent = coalescedWheelEvent(m_wheelEventQueue, *nextCoalescedEvent.get());
    m_currentlyProcessedWheelEvents.append(WTF::move(nextCoalescedEvent));
    sendWheelEvent(nextWheelEvent);
}

void WebPageProxy::sendWheelEvent(const WebWheelEvent& event)
{
    m_process->responsivenessTimer()->start();

    if (m_shouldSendEventsSynchronously) {
        bool handled = false;
        m_process->sendSync(Messages::WebPage::WheelEventSyncForTesting(event), Messages::WebPage::WheelEventSyncForTesting::Reply(handled), m_pageID);
        didReceiveEvent(event.type(), handled);
        return;
    }

    m_process->send(
        Messages::EventDispatcher::WheelEvent(
            m_pageID,
            event,
            shouldUseImplicitRubberBandControl() ? !m_backForwardList->backItem() : rubberBandsAtLeft(),
            shouldUseImplicitRubberBandControl() ? !m_backForwardList->forwardItem() : rubberBandsAtRight(),
            rubberBandsAtTop(),
            rubberBandsAtBottom()
        ), 0);
}

void WebPageProxy::handleKeyboardEvent(const NativeWebKeyboardEvent& event)
{
    if (!isValid())
        return;
    
    LOG(KeyHandling, "WebPageProxy::handleKeyboardEvent: %s", webKeyboardEventTypeString(event.type()));

    m_keyEventQueue.append(event);

    m_process->responsivenessTimer()->start();
    if (m_shouldSendEventsSynchronously) {
        bool handled = false;
        m_process->sendSync(Messages::WebPage::KeyEventSyncForTesting(event), Messages::WebPage::KeyEventSyncForTesting::Reply(handled), m_pageID);
        didReceiveEvent(event.type(), handled);
    } else if (m_keyEventQueue.size() == 1) // Otherwise, sent from DidReceiveEvent message handler.
        m_process->send(Messages::WebPage::KeyEvent(event), m_pageID);
}

uint64_t WebPageProxy::generateNavigationID()
{
    return ++m_navigationID;
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
    MESSAGE_CHECK_URL(urlString);

    newMimeType = mimeType.lower();
    pluginLoadPolicy = PluginModuleLoadNormally;

    PluginData::AllowedPluginTypes allowedPluginTypes = allowOnlyApplicationPlugins ? PluginData::OnlyApplicationPlugins : PluginData::AllPlugins;
    PluginModuleInfo plugin = m_process->context().pluginInfoStore().findPlugin(newMimeType, URL(URL(), urlString), allowedPluginTypes);
    if (!plugin.path) {
        pluginProcessToken = 0;
        return;
    }

    pluginLoadPolicy = PluginInfoStore::defaultLoadPolicyForPlugin(plugin);

#if PLATFORM(COCOA)
    RefPtr<ImmutableDictionary> pluginInformation = createPluginInformationDictionary(plugin, frameURLString, String(), pageURLString, String(), String());
    pluginLoadPolicy = m_loaderClient->pluginLoadPolicy(this, static_cast<PluginModuleLoadPolicy>(pluginLoadPolicy), pluginInformation.get(), unavailabilityDescription);
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

    case PluginModuleBlocked:
        pluginProcessToken = 0;
        return;
    }

    pluginProcessToken = PluginProcessManager::shared().pluginProcessToken(plugin, static_cast<PluginProcessType>(processType), pluginProcessSandboxPolicy);
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(TOUCH_EVENTS)

bool WebPageProxy::shouldStartTrackingTouchEvents(const WebTouchEvent& touchStartEvent) const
{
#if ENABLE(ASYNC_SCROLLING)
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        if (m_scrollingCoordinatorProxy->isPointInNonFastScrollableRegion(touchPoint.location()))
            return true;
    }

    return false;
#else
    UNUSED_PARAM(touchStartEvent);
#endif // ENABLE(ASYNC_SCROLLING)
    return true;
}

#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPageProxy::handleTouchEventSynchronously(const NativeWebTouchEvent& event)
{
    if (!isValid())
        return;

    if (event.type() == WebEvent::TouchStart)
        m_isTrackingTouchEvents = shouldStartTrackingTouchEvents(event);

    if (!m_isTrackingTouchEvents)
        return;

    m_process->responsivenessTimer()->start();
    bool handled = false;
    m_process->sendSync(Messages::WebPage::TouchEventSync(event), Messages::WebPage::TouchEventSync::Reply(handled), m_pageID);
    didReceiveEvent(event.type(), handled);
    m_pageClient.doneWithTouchEvent(event, handled);
    m_process->responsivenessTimer()->stop();

    if (event.allTouchPointsAreReleased())
        m_isTrackingTouchEvents = false;
}

void WebPageProxy::handleTouchEventAsynchronously(const NativeWebTouchEvent& event)
{
    if (!isValid())
        return;

    if (!m_isTrackingTouchEvents)
        return;

    m_process->send(Messages::EventDispatcher::TouchEvent(m_pageID, event), 0);

    if (event.allTouchPointsAreReleased())
        m_isTrackingTouchEvents = false;
}

#elif ENABLE(TOUCH_EVENTS)
void WebPageProxy::handleTouchEvent(const NativeWebTouchEvent& event)
{
    if (!isValid())
        return;

    if (event.type() == WebEvent::TouchStart)
        m_isTrackingTouchEvents = shouldStartTrackingTouchEvents(event);

    if (!m_isTrackingTouchEvents)
        return;

    // If the page is suspended, which should be the case during panning, pinching
    // and animation on the page itself (kinetic scrolling, tap to zoom) etc, then
    // we do not send any of the events to the page even if is has listeners.
    if (!m_isPageSuspended) {
        m_touchEventQueue.append(event);
        m_process->responsivenessTimer()->start();
        if (m_shouldSendEventsSynchronously) {
            bool handled = false;
            m_process->sendSync(Messages::WebPage::TouchEventSyncForTesting(event), Messages::WebPage::TouchEventSyncForTesting::Reply(handled), m_pageID);
            didReceiveEvent(event.type(), handled);
        } else
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
        m_isTrackingTouchEvents = false;
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

void WebPageProxy::receivedPolicyDecision(PolicyAction action, WebFrameProxy* frame, uint64_t listenerID, uint64_t navigationID)
{
    if (!isValid())
        return;

    auto transaction = m_pageLoadState.transaction();

    if (action == PolicyIgnore)
        m_pageLoadState.clearPendingAPIRequestURL(transaction);

    uint64_t downloadID = 0;
    if (action == PolicyDownload) {
        // Create a download proxy.
        DownloadProxy* download = m_process->context().createDownloadProxy();
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
        return;
    }
    
    m_process->send(Messages::WebPage::DidReceivePolicyDecision(frame->frameID(), listenerID, action, navigationID, downloadID), m_pageID);
}

void WebPageProxy::setUserAgent(const String& userAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = userAgent;

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

    setUserAgent(m_customUserAgent);
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

void WebPageProxy::terminateProcess()
{
    // requestTermination() is a no-op for launching processes, so we get into an inconsistent state by calling resetStateAfterProcessExited().
    // FIXME: A client can terminate the page at any time, so we should do something more meaningful than assert and fall apart in release builds.
    // See also <https://bugs.webkit.org/show_bug.cgi?id=136012>.
    ASSERT(m_process->state() != WebProcessProxy::State::Launching);

    // NOTE: This uses a check of m_isValid rather than calling isValid() since
    // we want this to run even for pages being closed or that already closed.
    if (!m_isValid)
        return;

    m_process->requestTermination();
    resetStateAfterProcessExited();
}

SessionState WebPageProxy::sessionState(const std::function<bool (WebBackForwardListItem&)>& filter) const
{
    SessionState sessionState;

    sessionState.backForwardListState = m_backForwardList->backForwardListState(filter);

    String provisionalURLString = m_pageLoadState.pendingAPIRequestURL();
    if (provisionalURLString.isEmpty())
        provisionalURLString = m_pageLoadState.provisionalURL();

    if (!provisionalURLString.isEmpty())
        sessionState.provisionalURL = URL(URL(), provisionalURLString);

    return sessionState;
}

uint64_t WebPageProxy::restoreFromSessionState(SessionState sessionState, bool navigate)
{
    bool hasBackForwardList = !!sessionState.backForwardListState.currentIndex;

    if (hasBackForwardList) {
        m_backForwardList->restoreFromState(WTF::move(sessionState.backForwardListState));

        for (const auto& entry : m_backForwardList->entries())
            process().registerNewWebBackForwardListItem(entry.get());

        process().send(Messages::WebPage::RestoreSession(m_backForwardList->itemStates()), m_pageID);
    }

    if (navigate) {
        // FIXME: Navigating should be separate from state restoration.

        if (!sessionState.provisionalURL.isNull())
            return loadRequest(sessionState.provisionalURL);

        if (hasBackForwardList) {
            // FIXME: Do we have to null check the back forward list item here?
            if (WebBackForwardListItem* item = m_backForwardList->currentItem())
                return goToBackForwardItem(item);
        }
    }

    return 0;
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

    m_pageZoomFactor = zoomFactor;
    m_process->send(Messages::WebPage::SetPageZoomFactor(m_pageZoomFactor), m_pageID); 
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;
    m_process->send(Messages::WebPage::SetPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor), m_pageID); 
}

void WebPageProxy::scalePage(double scale, const IntPoint& origin)
{
    if (!isValid())
        return;

    m_pageScaleFactor = scale;
    m_process->send(Messages::WebPage::ScalePage(scale, origin), m_pageID);
}

void WebPageProxy::scalePageInViewCoordinates(double scale, const IntPoint& centerInViewCoordinates)
{
    if (!isValid())
        return;

    m_pageScaleFactor = scale;
    m_process->send(Messages::WebPage::ScalePageInViewCoordinates(scale, centerInViewCoordinates), m_pageID);
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
    if (m_customDeviceScaleFactor)
        return m_customDeviceScaleFactor;
    return m_intrinsicDeviceScaleFactor;
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

    if (m_customDeviceScaleFactor == customScaleFactor)
        return;

    float oldScaleFactor = deviceScaleFactor();

    m_customDeviceScaleFactor = customScaleFactor;

    if (deviceScaleFactor() != oldScaleFactor)
        m_drawingArea->deviceScaleFactorDidChange();
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

void WebPageProxy::listenForLayoutMilestones(WebCore::LayoutMilestones milestones)
{
    if (!isValid())
        return;

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

void WebPageProxy::pageScaleFactorDidChange(double scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
}

void WebPageProxy::pageZoomFactorDidChange(double zoomFactor)
{
    m_pageZoomFactor = zoomFactor;
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

void WebPageProxy::runJavaScriptInMainFrame(const String& script, std::function<void (WebSerializedScriptValue*, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::RunJavaScriptInMainFrame(script, callbackID), m_pageID);
}

void WebPageProxy::getRenderTreeExternalRepresentation(std::function<void (const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetRenderTreeExternalRepresentation(callbackID), m_pageID);
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, std::function<void (const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetSourceForFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getContentsAsString(std::function<void (const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::getBytecodeProfile(std::function<void (const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_process->send(Messages::WebPage::GetBytecodeProfile(callbackID), m_pageID);
}
    
#if ENABLE(MHTML)
void WebPageProxy::getContentsAsMHTMLData(std::function<void (API::Data*, CallbackBase::Error)> callbackFunction, bool useBinaryEncoding)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetContentsAsMHTMLData(callbackID, useBinaryEncoding), m_pageID);
}
#endif

void WebPageProxy::getSelectionOrContentsAsString(std::function<void (const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetSelectionOrContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::getSelectionAsWebArchiveData(std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetSelectionAsWebArchiveData(callbackID), m_pageID);
}

void WebPageProxy::getMainResourceDataOfFrame(WebFrameProxy* frame, std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!isValid() || !frame) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetMainResourceDataOfFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getResourceDataFromFrame(WebFrameProxy* frame, API::URL* resourceURL, std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetResourceDataFromFrame(frame->frameID(), resourceURL->string(), callbackID), m_pageID);
}

void WebPageProxy::getWebArchiveOfFrame(WebFrameProxy* frame, std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::GetWebArchiveOfFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::forceRepaint(PassRefPtr<VoidCallback> prpCallback)
{
    RefPtr<VoidCallback> callback = prpCallback;
    if (!isValid()) {
        // FIXME: If the page is invalid we should not call the callback. It'd be better to just return false from forceRepaint.
        callback->invalidate(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_callbacks.put(callback);
    m_drawingArea->waitForBackingStoreUpdateOnNextPaint();
    m_process->send(Messages::WebPage::ForceRepaint(callbackID), m_pageID); 
}

void WebPageProxy::preferencesDidChange()
{
    if (!isValid())
        return;

#if ENABLE(INSPECTOR_SERVER)
    if (m_preferences->developerExtrasEnabled())
        inspector()->enableRemoteInspection();
#endif

    m_process->pagePreferencesChanged(this);

    m_pageClient.preferencesDidChange();

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification
    // even if nothing changed in UI process, so that overrides get removed.

    // Preferences need to be updated during synchronous printing to make "print backgrounds" preference work when toggled from a print dialog checkbox.
    m_process->send(Messages::WebPage::PreferencesDidChange(preferencesStore()), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::didCreateMainFrame(uint64_t frameID)
{
    MESSAGE_CHECK(!m_mainFrame);
    MESSAGE_CHECK(m_process->canCreateFrame(frameID));

    m_mainFrame = WebFrameProxy::create(this, frameID);

    // Add the frame to the process wide map.
    m_process->frameCreated(frameID, m_mainFrame.get());
}

void WebPageProxy::didCreateSubframe(uint64_t frameID)
{
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
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didStartProgress(transaction);

    m_pageLoadState.commitChanges();
    m_loaderClient->didStartProgress(this);
}

void WebPageProxy::didChangeProgress(double value)
{
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didChangeProgress(transaction, value);

    m_pageLoadState.commitChanges();
    m_loaderClient->didChangeProgress(this);
}

void WebPageProxy::didFinishProgress()
{
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didFinishProgress(transaction);

    m_pageLoadState.commitChanges();
    m_loaderClient->didFinishProgress(this);
}

void WebPageProxy::setNetworkRequestsInProgress(bool networkRequestsInProgress)
{
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setNetworkRequestsInProgress(transaction, networkRequestsInProgress);
}

void WebPageProxy::didDestroyNavigation(uint64_t navigationID)
{
    m_loaderClient->didDestroyNavigation(this, navigationID);
}

void WebPageProxy::didStartProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, const String& url, const String& unreachableURL, IPC::MessageDecoder& decoder)
{
    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.clearPendingAPIRequestURL(transaction);

    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(url);

    if (frame->isMainFrame())
        m_pageLoadState.didStartProvisionalLoad(transaction, url, unreachableURL);

    frame->setUnreachableURL(unreachableURL);
    frame->didStartProvisionalLoad(url);

    m_pageLoadState.commitChanges();
    m_loaderClient->didStartProvisionalLoadForFrame(this, frame, navigationID, userData.get());
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, const String& url, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(url);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, url);

    frame->didReceiveServerRedirectForProvisionalLoad(url);

    m_pageLoadState.commitChanges();
    m_loaderClient->didReceiveServerRedirectForProvisionalLoadForFrame(this, frame, navigationID, userData.get());
}

void WebPageProxy::didFailProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, const ResourceError& error, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didFailProvisionalLoad(transaction);

    frame->didFailProvisionalLoad();

    m_pageLoadState.commitChanges();
    m_loaderClient->didFailProvisionalLoadWithErrorForFrame(this, frame, navigationID, error, userData.get());
}

void WebPageProxy::clearLoadDependentCallbacks()
{
    Vector<uint64_t> callbackIDsCopy;
    copyToVector(m_loadDependentStringCallbackIDs, callbackIDsCopy);
    m_loadDependentStringCallbackIDs.clear();

    for (size_t i = 0; i < callbackIDsCopy.size(); ++i) {
        auto callback = m_callbacks.take<StringCallback>(callbackIDsCopy[i]);
        if (callback)
            callback->invalidate();
    }
}

void WebPageProxy::didCommitLoadForFrame(uint64_t frameID, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, uint32_t opaqueFrameLoadType, const WebCore::CertificateInfo& certificateInfo, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didCommitLoad(transaction);

#if USE(APPKIT)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME: A load going on in one frame shouldn't affect text editing in other frames on the page.
    m_pageClient.resetSecureInputState();
    m_pageClient.dismissContentRelativeChildWindows();
#endif

    clearLoadDependentCallbacks();

    frame->didCommitLoad(mimeType, certificateInfo);

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
    if (frame->isMainFrame() && static_cast<FrameLoadType>(opaqueFrameLoadType) == FrameLoadType::Standard)
        m_pageScaleFactor = 1;

    m_pageLoadState.commitChanges();
    m_loaderClient->didCommitLoadForFrame(this, frame, navigationID, userData.get());
}

void WebPageProxy::didFinishDocumentLoadForFrame(uint64_t frameID, uint64_t navigationID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didFinishDocumentLoadForFrame(this, frame, navigationID, userData.get());
}

void WebPageProxy::didFinishLoadForFrame(uint64_t frameID, uint64_t navigationID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        m_pageLoadState.didFinishLoad(transaction);

    frame->didFinishLoad();

    m_pageLoadState.commitChanges();
    m_loaderClient->didFinishLoadForFrame(this, frame, navigationID, userData.get());

    if (isMainFrame)
        m_pageClient.didFinishLoadForMainFrame();
}

void WebPageProxy::didFailLoadForFrame(uint64_t frameID, uint64_t navigationID, const ResourceError& error, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    clearLoadDependentCallbacks();

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didFailLoad(transaction);

    frame->didFailLoad();

    m_pageLoadState.commitChanges();
    m_loaderClient->didFailLoadWithErrorForFrame(this, frame, navigationID, error, userData.get());
}

void WebPageProxy::didSameDocumentNavigationForFrame(uint64_t frameID, uint64_t navigationID, uint32_t opaqueSameDocumentNavigationType, const String& url, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(url);

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        m_pageLoadState.didSameDocumentNavigation(transaction, url);

    m_pageLoadState.clearPendingAPIRequestURL(transaction);
    frame->didSameDocumentNavigation(url);

    m_pageLoadState.commitChanges();

    SameDocumentNavigationType navigationType = static_cast<SameDocumentNavigationType>(opaqueSameDocumentNavigationType);
    m_loaderClient->didSameDocumentNavigationForFrame(this, frame, navigationID, navigationType, userData.get());

    if (isMainFrame)
        m_pageClient.didSameDocumentNavigationForMainFrame(navigationType);
}

void WebPageProxy::didReceiveTitleForFrame(uint64_t frameID, const String& title, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.setTitle(transaction, title);

    frame->didChangeTitle(title);
    
    m_pageLoadState.commitChanges();
    m_loaderClient->didReceiveTitleForFrame(this, title, frame, userData.get());
}

void WebPageProxy::didFirstLayoutForFrame(uint64_t frameID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didFirstLayoutForFrame(this, frame, userData.get());
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didFirstVisuallyNonEmptyLayoutForFrame(this, frame, userData.get());

    if (frame->isMainFrame())
        m_pageClient.didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void WebPageProxy::didLayout(uint32_t layoutMilestones, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    m_loaderClient->didLayout(this, static_cast<LayoutMilestones>(layoutMilestones), userData.get());
}

void WebPageProxy::didRemoveFrameFromHierarchy(uint64_t frameID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didRemoveFrameFromHierarchy(this, frame, userData.get());
}

void WebPageProxy::didDisplayInsecureContentForFrame(uint64_t frameID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didDisplayOrRunInsecureContent(transaction);

    m_pageLoadState.commitChanges();
    m_loaderClient->didDisplayInsecureContentForFrame(this, frame, userData.get());
}

void WebPageProxy::didRunInsecureContentForFrame(uint64_t frameID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didDisplayOrRunInsecureContent(transaction);

    m_pageLoadState.commitChanges();
    m_loaderClient->didRunInsecureContentForFrame(this, frame, userData.get());
}

void WebPageProxy::didDetectXSSForFrame(uint64_t frameID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient->didDetectXSSForFrame(this, frame, userData.get());
}

void WebPageProxy::frameDidBecomeFrameSet(uint64_t frameID, bool value)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->setIsFrameSet(value);
    if (frame->isMainFrame())
        m_frameSetLargestFrame = value ? m_mainFrame : 0;
}

void WebPageProxy::decidePolicyForNavigationAction(uint64_t frameID, uint64_t navigationID, const NavigationActionData& navigationActionData, uint64_t originatingFrameID, const WebCore::ResourceRequest& originalRequest, const ResourceRequest& request, uint64_t listenerID, IPC::MessageDecoder& decoder, bool& receivedPolicyAction, uint64_t& newNavigationID, uint64_t& policyAction, uint64_t& downloadID)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    auto transaction = m_pageLoadState.transaction();

    if (request.url() != m_pageLoadState.pendingAPIRequestURL())
        m_pageLoadState.clearPendingAPIRequestURL(transaction);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(request.url());
    MESSAGE_CHECK_URL(originalRequest.url());

    WebFrameProxy* originatingFrame = m_process->webFrame(originatingFrameID);
    
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!navigationID && frame->isMainFrame()) {
        newNavigationID = generateNavigationID();
        listener->setNavigationID(newNavigationID);
    }

#if USE(CONTENT_FILTERING)
    if (frame->contentFilterDidHandleNavigationAction(request)) {
        receivedPolicyAction = true;
        policyAction = PolicyIgnore;
        return;
    }
#endif

    ASSERT(!m_inDecidePolicyForNavigationAction);

    m_inDecidePolicyForNavigationAction = true;
    m_syncNavigationActionPolicyActionIsValid = false;

    m_policyClient->decidePolicyForNavigationAction(this, frame, navigationActionData, originatingFrame, originalRequest, request, WTF::move(listener), userData.get());

    m_inDecidePolicyForNavigationAction = false;

    // Check if we received a policy decision already. If we did, we can just pass it back.
    receivedPolicyAction = m_syncNavigationActionPolicyActionIsValid;
    if (m_syncNavigationActionPolicyActionIsValid) {
        policyAction = m_syncNavigationActionPolicyAction;
        downloadID = m_syncNavigationActionPolicyDownloadID;
    }
}

void WebPageProxy::decidePolicyForNewWindowAction(uint64_t frameID, const NavigationActionData& navigationActionData, const ResourceRequest& request, const String& frameName, uint64_t listenerID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(request.url());

    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);

    m_policyClient->decidePolicyForNewWindowAction(this, frame, navigationActionData, request, frameName, WTF::move(listener), userData.get());
}

void WebPageProxy::decidePolicyForResponse(uint64_t frameID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, uint64_t listenerID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK_URL(request.url());
    MESSAGE_CHECK_URL(response.url());

    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);

    m_policyClient->decidePolicyForResponse(this, frame, response, request, canShowMIMEType, WTF::move(listener), userData.get());
}

void WebPageProxy::decidePolicyForResponseSync(uint64_t frameID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, uint64_t listenerID, IPC::MessageDecoder& decoder, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID)
{
    ASSERT(!m_inDecidePolicyForResponseSync);

    m_inDecidePolicyForResponseSync = true;
    m_decidePolicyForResponseRequest = &request;
    m_syncMimeTypePolicyActionIsValid = false;

    decidePolicyForResponse(frameID, response, request, canShowMIMEType, listenerID, decoder);

    m_inDecidePolicyForResponseSync = false;
    m_decidePolicyForResponseRequest = 0;

    // Check if we received a policy decision already. If we did, we can just pass it back.
    receivedPolicyAction = m_syncMimeTypePolicyActionIsValid;
    if (m_syncMimeTypePolicyActionIsValid) {
        policyAction = m_syncMimeTypePolicyAction;
        downloadID = m_syncMimeTypePolicyDownloadID;
    }
}

void WebPageProxy::unableToImplementPolicy(uint64_t frameID, const ResourceError& error, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;
    
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_policyClient->unableToImplementPolicy(this, frame, error, userData.get());
}

// FormClient

void WebPageProxy::willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const Vector<std::pair<String, String>>& textFieldValues, uint64_t listenerID, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    WebFrameProxy* sourceFrame = m_process->webFrame(sourceFrameID);
    MESSAGE_CHECK(sourceFrame);

    RefPtr<WebFormSubmissionListenerProxy> listener = frame->setUpFormSubmissionListenerProxy(listenerID);
    if (!m_formClient->willSubmitForm(this, frame, sourceFrame, textFieldValues, userData.get(), listener.get()))
        listener->continueSubmission();
}

// UIClient

void WebPageProxy::createNewPage(uint64_t frameID, const ResourceRequest& request, const WindowFeatures& windowFeatures, const NavigationActionData& navigationActionData, uint64_t& newPageID, WebPageCreationParameters& newPageParameters)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebPageProxy> newPage = m_uiClient->createNewPage(this, frame, request, windowFeatures, navigationActionData);
    if (!newPage) {
        newPageID = 0;
        return;
    }

    newPageID = newPage->pageID();
    newPageParameters = newPage->creationParameters();
    process().context().storageManager().cloneSessionStorageNamespace(m_pageID, newPage->pageID());
}
    
void WebPageProxy::showPage()
{
    m_uiClient->showPage(this);
}

void WebPageProxy::closePage(bool stopResponsivenessTimer)
{
    if (stopResponsivenessTimer)
        m_process->responsivenessTimer()->stop();

    m_pageClient.clearAllEditCommands();
    m_uiClient->close(this);
}

void WebPageProxy::runJavaScriptAlert(uint64_t frameID, const String& message, RefPtr<Messages::WebPageProxy::RunJavaScriptAlert::DelayedReply> reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptAlert() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    m_uiClient->runJavaScriptAlert(this, message, frame, [reply]{ reply->send(); });
}

void WebPageProxy::runJavaScriptConfirm(uint64_t frameID, const String& message, RefPtr<Messages::WebPageProxy::RunJavaScriptConfirm::DelayedReply> reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptConfirm() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    m_uiClient->runJavaScriptConfirm(this, message, frame, [reply](bool result) { reply->send(result); });
}

void WebPageProxy::runJavaScriptPrompt(uint64_t frameID, const String& message, const String& defaultValue, RefPtr<Messages::WebPageProxy::RunJavaScriptPrompt::DelayedReply> reply)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptPrompt() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    m_uiClient->runJavaScriptPrompt(this, message, defaultValue, frame, [reply](const String& result) { reply->send(result); });
}

void WebPageProxy::shouldInterruptJavaScript(bool& result)
{
    // Since shouldInterruptJavaScript() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    result = m_uiClient->shouldInterruptJavaScript(this);
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient->setStatusText(this, text);
}

void WebPageProxy::mouseDidMoveOverElement(const WebHitTestResult::Data& hitTestResultData, uint32_t opaqueModifiers, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    m_lastMouseMoveHitTestResult = WebHitTestResult::create(hitTestResultData);

    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);

    m_uiClient->mouseDidMoveOverElement(this, hitTestResultData, modifiers, userData.get());
}

void WebPageProxy::connectionWillOpen(IPC::Connection* connection)
{
    ASSERT(connection == m_process->connection());

    m_process->context().storageManager().setAllowedSessionStorageNamespaceConnection(m_pageID, connection);
}

void WebPageProxy::connectionWillClose(IPC::Connection* connection)
{
    ASSERT_UNUSED(connection, connection == m_process->connection());

    m_process->context().storageManager().setAllowedSessionStorageNamespaceConnection(m_pageID, 0);
}

void WebPageProxy::processDidFinishLaunching()
{
    ASSERT(m_process->state() == WebProcessProxy::State::Running);

    if (m_userContentController)
        m_process->addWebUserContentControllerProxy(*m_userContentController);
    m_process->addVisitedLinkProvider(m_visitedLinkProvider.get());
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebPageProxy::unavailablePluginButtonClicked(uint32_t opaquePluginUnavailabilityReason, const String& mimeType, const String& pluginURLString, const String& pluginspageAttributeURLString, const String& frameURLString, const String& pageURLString)
{
    MESSAGE_CHECK_URL(pluginURLString);
    MESSAGE_CHECK_URL(pluginspageAttributeURLString);
    MESSAGE_CHECK_URL(frameURLString);
    MESSAGE_CHECK_URL(pageURLString);

    RefPtr<ImmutableDictionary> pluginInformation;
    String newMimeType = mimeType;
    PluginModuleInfo plugin = m_process->context().pluginInfoStore().findPlugin(newMimeType, URL(URL(), pluginURLString));
    pluginInformation = createPluginInformationDictionary(plugin, frameURLString, mimeType, pageURLString, pluginspageAttributeURLString, pluginURLString);

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

    m_uiClient->unavailablePluginButtonClicked(this, pluginUnavailabilityReason, pluginInformation.get());
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
void WebPageProxy::webGLPolicyForURL(const String& url, uint32_t& loadPolicy)
{
    loadPolicy = static_cast<uint32_t>(m_loaderClient->webGLLoadPolicy(this, url));
}

void WebPageProxy::resolveWebGLPolicyForURL(const String& url, uint32_t& loadPolicy)
{
    loadPolicy = static_cast<uint32_t>(m_loaderClient->resolveWebGLLoadPolicy(this, url));
}
#endif // ENABLE(WEBGL)

void WebPageProxy::setToolbarsAreVisible(bool toolbarsAreVisible)
{
    m_uiClient->setToolbarsAreVisible(this, toolbarsAreVisible);
}

void WebPageProxy::getToolbarsAreVisible(bool& toolbarsAreVisible)
{
    toolbarsAreVisible = m_uiClient->toolbarsAreVisible(this);
}

void WebPageProxy::setMenuBarIsVisible(bool menuBarIsVisible)
{
    m_uiClient->setMenuBarIsVisible(this, menuBarIsVisible);
}

void WebPageProxy::getMenuBarIsVisible(bool& menuBarIsVisible)
{
    menuBarIsVisible = m_uiClient->menuBarIsVisible(this);
}

void WebPageProxy::setStatusBarIsVisible(bool statusBarIsVisible)
{
    m_uiClient->setStatusBarIsVisible(this, statusBarIsVisible);
}

void WebPageProxy::getStatusBarIsVisible(bool& statusBarIsVisible)
{
    statusBarIsVisible = m_uiClient->statusBarIsVisible(this);
}

void WebPageProxy::setIsResizable(bool isResizable)
{
    m_uiClient->setIsResizable(this, isResizable);
}

void WebPageProxy::getIsResizable(bool& isResizable)
{
    isResizable = m_uiClient->isResizable(this);
}

void WebPageProxy::setWindowFrame(const FloatRect& newWindowFrame)
{
    m_uiClient->setWindowFrame(this, m_pageClient.convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(FloatRect& newWindowFrame)
{
    newWindowFrame = m_pageClient.convertToUserSpace(m_uiClient->windowFrame(this));
}
    
void WebPageProxy::screenToRootView(const IntPoint& screenPoint, IntPoint& windowPoint)
{
    windowPoint = m_pageClient.screenToRootView(screenPoint);
}
    
void WebPageProxy::rootViewToScreen(const IntRect& viewRect, IntRect& result)
{
    result = m_pageClient.rootViewToScreen(viewRect);
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
    
void WebPageProxy::runBeforeUnloadConfirmPanel(const String& message, uint64_t frameID, bool& shouldClose)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runBeforeUnloadConfirmPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    shouldClose = m_uiClient->runBeforeUnloadConfirmPanel(this, message, frame);
}

#if USE(TILED_BACKING_STORE)
void WebPageProxy::pageDidRequestScroll(const IntPoint& point)
{
    m_pageClient.pageDidRequestScroll(point);
}

void WebPageProxy::pageTransitionViewportReady()
{
    m_pageClient.pageTransitionViewportReady();
}

void WebPageProxy::didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect)
{
    m_pageClient.didRenderFrame(contentsSize, coveredRect);
}

#endif

void WebPageProxy::didChangeViewportProperties(const ViewportAttributes& attr)
{
    m_pageClient.didChangeViewportProperties(attr);
}

void WebPageProxy::pageDidScroll()
{
    m_uiClient->pageDidScroll(this);
#if PLATFORM(MAC)
    m_pageClient.dismissContentRelativeChildWindows();
#endif
}

void WebPageProxy::runOpenPanel(uint64_t frameID, const FileChooserSettings& settings)
{
    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = 0;
    }

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebOpenPanelParameters> parameters = WebOpenPanelParameters::create(settings);
    m_openPanelResultListener = WebOpenPanelResultListenerProxy::create(this);

    // Since runOpenPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    if (!m_uiClient->runOpenPanel(this, frame, parameters.get(), m_openPanelResultListener.get())) {
        if (!m_pageClient.handleRunOpenPanel(this, frame, parameters.get(), m_openPanelResultListener.get()))
            didCancelForOpenPanel();
    }
}

void WebPageProxy::printFrame(uint64_t frameID)
{
    ASSERT(!m_isPerformingDOMPrintOperation);
    m_isPerformingDOMPrintOperation = true;

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_uiClient->printFrame(this, frame);

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

#if PLATFORM(EFL)
void WebPageProxy::didChangeContentSize(const IntSize& size)
{
    m_pageClient.didChangeContentSize(size);
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
void WebPageProxy::showColorPicker(const WebCore::Color& initialColor, const IntRect& elementRect)
{
#if ENABLE(INPUT_TYPE_COLOR_POPOVER)
    // A new popover color well needs to be created (and the previous one destroyed) for
    // each activation of a color element.
    m_colorPicker = 0;
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

    if (m_colorPicker) {
        m_colorPicker->invalidate();
        m_colorPicker = nullptr;
    }

    m_process->send(Messages::WebPage::DidEndColorPicker(), m_pageID);
}
#endif

void WebPageProxy::didDraw()
{
    m_uiClient->didDraw(this);
}

// Inspector

#if ENABLE(INSPECTOR)

WebInspectorProxy* WebPageProxy::inspector()
{
    if (isClosed() || !isValid())
        return 0;
    return m_inspector.get();
}

#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxy* WebPageProxy::fullScreenManager()
{
    return m_fullScreenManager.get();
}
#endif
    
#if PLATFORM(IOS)
RefPtr<WebVideoFullscreenManagerProxy> WebPageProxy::videoFullscreenManager()
{
    return m_videoFullscreenManager;
}
#endif

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
    m_backForwardList->goToItem(item);
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

void WebPageProxy::editorStateChanged(const EditorState& editorState)
{
#if PLATFORM(COCOA)
    bool couldChangeSecureInputState = m_editorState.isInPasswordField != editorState.isInPasswordField || m_editorState.selectionIsNone;
#endif
#if PLATFORM(MAC) && !USE(ASYNC_NSTEXTINPUTCLIENT)
    bool closedComposition = !editorState.shouldIgnoreCompositionSelectionChange && !editorState.hasComposition && (m_editorState.hasComposition || m_temporarilyClosedComposition);
    m_temporarilyClosedComposition = editorState.shouldIgnoreCompositionSelectionChange && (m_temporarilyClosedComposition || m_editorState.hasComposition) && !editorState.hasComposition;
#endif

    m_editorState = editorState;

#if PLATFORM(COCOA)
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !editorState.selectionIsNone)
        m_pageClient.updateSecureInputState();
#endif

    if (editorState.shouldIgnoreCompositionSelectionChange)
        return;

#if PLATFORM(MAC) && !USE(ASYNC_NSTEXTINPUTCLIENT)
    if (closedComposition)
        m_pageClient.notifyInputContextAboutDiscardedComposition();
    if (editorState.hasComposition) {
        // Abandon the current inline input session if selection changed for any other reason but an input method changing the composition.
        // FIXME: This logic should be in WebCore, no need to round-trip to UI process to cancel the composition.
        cancelComposition();
        m_pageClient.notifyInputContextAboutDiscardedComposition();
    }
#elif PLATFORM(IOS)
    // We always need to notify the client on iOS to make sure the selection is redrawn,
    // even during composition to support phrase boundary gesture.
    notifyRevealedSelection();
#elif PLATFORM(EFL) || PLATFORM(GTK)
    m_pageClient.updateTextInputState();
#endif
}

void WebPageProxy::compositionWasCanceled(const EditorState& editorState)
{
#if PLATFORM(COCOA)
    m_pageClient.notifyInputContextAboutDiscardedComposition();
#endif
    editorStateChanged(editorState);
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
    m_findMatchesClient.didGetImageForMatchResult(this, WebImage::create(ShareableBitmap::create(contentImageHandle)).get(), matchIndex);
}

void WebPageProxy::setTextIndicator(const TextIndicatorData& indicatorData, bool fadeOut)
{
    m_pageClient.setTextIndicator(TextIndicator::create(indicatorData), fadeOut);
}

void WebPageProxy::clearTextIndicator()
{
    m_pageClient.setTextIndicator(nullptr, false);
}

void WebPageProxy::setTextIndicatorAnimationProgress(float progress)
{
    m_pageClient.setTextIndicatorAnimationProgress(progress);
}

void WebPageProxy::didFindString(const String& string, uint32_t matchCount, int32_t matchIndex)
{
    m_findClient->didFindString(this, string, matchCount, matchIndex);
}

void WebPageProxy::didFindStringMatches(const String& string, const Vector<Vector<WebCore::IntRect>>& matchRects, int32_t firstIndexAfterSelection)
{
    Vector<RefPtr<API::Object>> matches;
    matches.reserveInitialCapacity(matchRects.size());

    for (const auto& rects : matchRects) {
        Vector<RefPtr<API::Object>> apiRects;
        apiRects.reserveInitialCapacity(rects.size());

        for (const auto& rect : rects)
            apiRects.uncheckedAppend(API::Rect::create(toAPI(rect)));

        matches.uncheckedAppend(API::Array::create(WTF::move(apiRects)));
    }

    m_findMatchesClient.didFindStringMatches(this, string, API::Array::create(WTF::move(matches)).get(), firstIndexAfterSelection);
}

void WebPageProxy::didFailToFindString(const String& string)
{
    m_findClient->didFailToFindString(this, string);
}

bool WebPageProxy::sendMessage(std::unique_ptr<IPC::MessageEncoder> encoder, unsigned messageSendFlags)
{
    return m_process->sendMessage(WTF::move(encoder), messageSendFlags);
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
    process().send(Messages::WebPage::PostInjectedBundleMessage(messageName, WebContextUserMessageEncoder(messageBody, process())), m_pageID);
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
#if PLATFORM(EFL)
        m_uiPopupMenuClient.hidePopupMenu(this);
#else
        m_activePopupMenu->hidePopupMenu();
#endif
        m_activePopupMenu->invalidate();
        m_activePopupMenu = 0;
    }

    m_activePopupMenu = m_pageClient.createPopupMenuProxy(this);

    if (!m_activePopupMenu)
        return;

    // Since showPopupMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

#if PLATFORM(EFL)
    UNUSED_PARAM(data);
    m_uiPopupMenuClient.showPopupMenu(this, m_activePopupMenu.get(), rect, static_cast<TextDirection>(textDirection), m_pageScaleFactor, items, selectedIndex);
#else
    RefPtr<WebPopupMenuProxy> protectedActivePopupMenu = m_activePopupMenu;

    protectedActivePopupMenu->showPopupMenu(rect, static_cast<TextDirection>(textDirection), m_pageScaleFactor, items, data, selectedIndex);

    // Since Efl doesn't use a nested mainloop to show the popup and get the answer, we need to keep the client pointer valid.
    // FIXME: The above comment doesn't make any sense since this code is compiled out for EFL.
    protectedActivePopupMenu->invalidate();
    protectedActivePopupMenu = 0;
#endif
}

void WebPageProxy::hidePopupMenu()
{
    if (!m_activePopupMenu)
        return;

#if PLATFORM(EFL)
    m_uiPopupMenuClient.hidePopupMenu(this);
#else
    m_activePopupMenu->hidePopupMenu();
#endif
    m_activePopupMenu->invalidate();
    m_activePopupMenu = 0;
}

#if ENABLE(CONTEXT_MENUS)
void WebPageProxy::showContextMenu(const IntPoint& menuLocation, const ContextMenuContextData& contextMenuContextData, const Vector<WebContextMenuItemData>& proposedItems, IPC::MessageDecoder& decoder)
{
    internalShowContextMenu(menuLocation, contextMenuContextData, proposedItems, ContextMenuClientEligibility::EligibleForClient, &decoder);
    
    // No matter the result of internalShowContextMenu, always notify the WebProcess that the menu is hidden so it starts handling mouse events again.
    m_process->send(Messages::WebPage::ContextMenuHidden(), m_pageID);
}

void WebPageProxy::internalShowContextMenu(const IntPoint& menuLocation, const ContextMenuContextData& contextMenuContextData, const Vector<WebContextMenuItemData>& proposedItems, ContextMenuClientEligibility clientEligibility, IPC::MessageDecoder* decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (decoder && !decoder->decode(messageDecoder))
        return;

    m_activeContextMenuContextData = contextMenuContextData;

    if (!m_contextMenuClient.hideContextMenu(this) && m_activeContextMenu) {
        m_activeContextMenu->hideContextMenu();
        m_activeContextMenu = 0;
    }

    m_activeContextMenu = m_pageClient.createContextMenuProxy(this);
    if (!m_activeContextMenu)
        return;

    // Since showContextMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    // Unless this is an image control, give the PageContextMenuClient one last swipe at changing the menu.
    Vector<WebContextMenuItemData> items;
    bool useProposedItems = true;
    bool askClientToChangeMenu = clientEligibility == ContextMenuClientEligibility::EligibleForClient;
#if ENABLE(SERVICE_CONTROLS)
    if (!contextMenuContextData.controlledImageHandle().isNull())
        askClientToChangeMenu = false;
#endif

    if (askClientToChangeMenu && m_contextMenuClient.getContextMenuFromProposedMenu(this, proposedItems, items, contextMenuContextData.webHitTestResultData(), userData.get()))
        useProposedItems = false;

    const Vector<WebContextMenuItemData>& itemsToShow = useProposedItems ? proposedItems : items;
    if (!m_contextMenuClient.showContextMenu(this, menuLocation, itemsToShow))
        m_activeContextMenu->showContextMenu(menuLocation, itemsToShow, contextMenuContextData);

    m_contextMenuClient.contextMenuDismissed(this);
}

void WebPageProxy::contextMenuItemSelected(const WebContextMenuItemData& item)
{
    // Application custom items don't need to round-trip through to WebCore in the WebProcess.
    if (item.action() >= ContextMenuItemBaseApplicationTag) {
        m_contextMenuClient.customContextMenuItemSelected(this, item);
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
        m_process->context().download(this, URL(URL(), m_activeContextMenuContextData.webHitTestResultData().absoluteImageURL));
        return;    
    }
    if (item.action() == ContextMenuItemTagDownloadLinkToDisk) {
        m_process->context().download(this, URL(URL(), m_activeContextMenuContextData.webHitTestResultData().absoluteLinkURL));
        return;
    }
    if (item.action() == ContextMenuItemTagDownloadMediaToDisk) {
        m_process->context().download(this, URL(URL(), m_activeContextMenuContextData.webHitTestResultData().absoluteMediaURL));
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
#endif // ENABLE(CONTEXT_MENUS)

#if PLATFORM(IOS)
void WebPageProxy::didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>& fileURLs, const String& displayString, const API::Data* iconData)
{
    if (!isValid())
        return;

#if ENABLE(SANDBOX_EXTENSIONS)
    // FIXME: The sandbox extensions should be sent with the DidChooseFilesForOpenPanel message. This
    // is gated on a way of passing SandboxExtension::Handles in a Vector.
    for (size_t i = 0; i < fileURLs.size(); ++i) {
        SandboxExtension::Handle sandboxExtensionHandle;
        SandboxExtension::createHandle(fileURLs[i], SandboxExtension::ReadOnly, sandboxExtensionHandle);
        m_process->send(Messages::WebPage::ExtendSandboxForFileFromOpenPanel(sandboxExtensionHandle), m_pageID);
    }
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
    // FIXME: The sandbox extensions should be sent with the DidChooseFilesForOpenPanel message. This
    // is gated on a way of passing SandboxExtension::Handles in a Vector.
    for (size_t i = 0; i < fileURLs.size(); ++i) {
        SandboxExtension::Handle sandboxExtensionHandle;
        SandboxExtension::createHandle(fileURLs[i], SandboxExtension::ReadOnly, sandboxExtensionHandle);
        m_process->send(Messages::WebPage::ExtendSandboxForFileFromOpenPanel(sandboxExtensionHandle), m_pageID);
    }
#endif

    m_process->send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs), m_pageID);

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = 0;
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!isValid())
        return;

    m_process->send(Messages::WebPage::DidCancelForOpenPanel(), m_pageID);
    
    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = 0;
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

void WebPageProxy::registerEditCommand(PassRefPtr<WebEditCommandProxy> commandProxy, UndoOrRedo undoOrRedo)
{
    m_pageClient.registerEditCommand(commandProxy, undoOrRedo);
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
void WebPageProxy::checkTextOfParagraph(const String& text, uint64_t checkingTypes, Vector<TextCheckingResult>& results)
{
    results = TextChecker::checkTextOfParagraph(spellDocumentTag(), text, checkingTypes);
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

void WebPageProxy::getGuessesForWord(const String& word, const String& context, Vector<String>& guesses)
{
    TextChecker::getGuessesForWord(spellDocumentTag(), word, context, guesses);
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

void WebPageProxy::requestCheckingOfString(uint64_t requestID, const TextCheckingRequestData& request)
{
    TextChecker::requestCheckingOfString(TextCheckerCompletion::create(requestID, request, this));
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
    // The Web process may have asked to change the cursor when the view was in an active window, but
    // if it is no longer in a window or the window is not active, then the cursor should not change.
    if (m_pageClient.isViewWindowActive())
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
        break;

    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
    case WebEvent::Wheel:
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
        m_process->responsivenessTimer()->stop();
        break;
    }

    switch (type) {
    case WebEvent::NoType:
        break;
    case WebEvent::MouseMove:
        m_processingMouseMoveEvent = false;
        if (m_nextMouseMoveEvent) {
            handleMouseEvent(*m_nextMouseMoveEvent);
            m_nextMouseMoveEvent = nullptr;
        }
        break;
    case WebEvent::MouseDown:
        break;
    case WebEvent::MouseUp:
        m_currentlyProcessedMouseDownEvent = nullptr;
        break;

    case WebEvent::Wheel: {
        MESSAGE_CHECK(!m_currentlyProcessedWheelEvents.isEmpty());

        std::unique_ptr<Vector<NativeWebWheelEvent>> oldestCoalescedEvent = m_currentlyProcessedWheelEvents.takeFirst();

        // FIXME: Dispatch additional events to the didNotHandleWheelEvent client function.
        if (!handled) {
            if (m_uiClient->implementsDidNotHandleWheelEvent())
                m_uiClient->didNotHandleWheelEvent(this, oldestCoalescedEvent->last());
#if PLATFORM(COCOA)
            m_pageClient.wheelEventWasNotHandledByWebCore(oldestCoalescedEvent->last());
#endif
        }

        if (!m_wheelEventQueue.isEmpty())
            processNextQueuedWheelEvent();
        break;
    }

    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char: {
        LOG(KeyHandling, "WebPageProxy::didReceiveEvent: %s", webKeyboardEventTypeString(type));

        MESSAGE_CHECK(!m_keyEventQueue.isEmpty());
        NativeWebKeyboardEvent event = m_keyEventQueue.takeFirst();

        MESSAGE_CHECK(type == event.type());

        if (!m_keyEventQueue.isEmpty())
            m_process->send(Messages::WebPage::KeyEvent(m_keyEventQueue.first()), m_pageID);

        m_pageClient.doneWithKeyEvent(event, handled);
        if (handled)
            break;

        if (m_uiClient->implementsDidNotHandleKeyEvent())
            m_uiClient->didNotHandleKeyEvent(this, event);
        break;
    }
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

void WebPageProxy::stopResponsivenessTimer()
{
    m_process->responsivenessTimer()->stop();
}

void WebPageProxy::voidCallback(uint64_t callbackID)
{
    auto callback = m_callbacks.take<VoidCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallback();
}

void WebPageProxy::dataCallback(const IPC::DataReference& dataReference, uint64_t callbackID)
{
    auto callback = m_callbacks.take<DataCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(API::Data::create(dataReference.data(), dataReference.size()).get());
}

void WebPageProxy::imageCallback(const ShareableBitmap::Handle& bitmapHandle, uint64_t callbackID)
{
    auto callback = m_callbacks.take<ImageCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(bitmapHandle);
}

void WebPageProxy::stringCallback(const String& resultString, uint64_t callbackID)
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

void WebPageProxy::scriptValueCallback(const IPC::DataReference& dataReference, uint64_t callbackID)
{
    auto callback = m_callbacks.take<ScriptValueCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    Vector<uint8_t> data;
    data.reserveInitialCapacity(dataReference.size());
    data.append(dataReference.data(), dataReference.size());

    callback->performCallbackWithReturnValue(data.size() ? WebSerializedScriptValue::adopt(data).get() : 0);
}

void WebPageProxy::computedPagesCallback(const Vector<IntRect>& pageRects, double totalScaleFactorForPrinting, uint64_t callbackID)
{
    auto callback = m_callbacks.take<ComputedPagesCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(pageRects, totalScaleFactorForPrinting);
}

void WebPageProxy::validateCommandCallback(const String& commandName, bool isEnabled, int state, uint64_t callbackID)
{
    auto callback = m_callbacks.take<ValidateCommandCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(commandName.impl(), isEnabled, state);
}

void WebPageProxy::unsignedCallback(uint64_t result, uint64_t callbackID)
{
    auto callback = m_callbacks.take<UnsignedCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    callback->performCallbackWithReturnValue(result);
}

void WebPageProxy::editingRangeCallback(const EditingRange& range, uint64_t callbackID)
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

void WebPageProxy::rectForCharacterRangeCallback(const IntRect& rect, const EditingRange& actualRange, uint64_t callbackID)
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
void WebPageProxy::printFinishedCallback(const ResourceError& printError, uint64_t callbackID)
{
    auto callback = m_callbacks.take<PrintFinishedCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    RefPtr<API::Error> error = API::Error::create(printError);
    callback->performCallbackWithReturnValue(error.get());
}
#endif

void WebPageProxy::focusedFrameChanged(uint64_t frameID)
{
    if (!frameID) {
        m_focusedFrame = 0;
        return;
    }

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_focusedFrame = frame;
}

void WebPageProxy::frameSetLargestFrameChanged(uint64_t frameID)
{
    if (!frameID) {
        m_frameSetLargestFrame = 0;
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

    m_loaderClient->processDidBecomeUnresponsive(this);
}

void WebPageProxy::interactionOccurredWhileProcessUnresponsive()
{
    if (!isValid())
        return;

    m_loaderClient->interactionOccurredWhileProcessUnresponsive(this);
}

void WebPageProxy::processDidBecomeResponsive()
{
    if (!isValid())
        return;
    
    updateBackingStoreDiscardableState();

    m_loaderClient->processDidBecomeResponsive(this);
}

void WebPageProxy::processDidCrash()
{
    ASSERT(m_isValid);

    // There is a nested transaction in resetStateAfterProcessExited() that we don't want to commit before the client call.
    PageLoadState::Transaction transaction = m_pageLoadState.transaction();

    resetStateAfterProcessExited();

    m_loaderClient->processDidCrash(this);
}

void WebPageProxy::resetState(ResetStateReason resetStateReason)
{
    m_mainFrame = nullptr;
    m_drawingArea = nullptr;

#if ENABLE(INSPECTOR)
    if (m_inspector) {
        m_inspector->invalidate();
        m_inspector = nullptr;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenManager) {
        m_fullScreenManager->invalidate();
        m_fullScreenManager = nullptr;
    }
#endif

#if ENABLE(VIBRATION)
    m_vibration->invalidate();
#endif

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = nullptr;
    }

#if ENABLE(TOUCH_EVENTS)
    m_isTrackingTouchEvents = false;
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

    m_notificationPermissionRequestManager.invalidateRequests();

    m_toolTip = String();

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    m_mainFrameIsPinnedToLeftSide = true;
    m_mainFrameIsPinnedToRightSide = true;
    m_mainFrameIsPinnedToTopSide = true;
    m_mainFrameIsPinnedToBottomSide = true;

    m_visibleScrollerThumbRect = IntRect();

#if PLATFORM(IOS)
    if (m_videoFullscreenManager) {
        m_videoFullscreenManager->invalidate();
        m_videoFullscreenManager = nullptr;
    }

    m_lastVisibleContentRectUpdate = VisibleContentRectUpdateInfo();
    m_dynamicViewportSizeUpdateWaitingForTarget = false;
    m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit = false;
    m_dynamicViewportSizeUpdateLayerTreeTransactionID = 0;
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

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();

    m_activePopupMenu = 0;
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

    m_isValid = false;
    m_isPageSuspended = false;

    if (m_mainFrame) {
        m_urlAtProcessExit = m_mainFrame->url();
        m_loadStateAtProcessExit = m_mainFrame->frameLoadState().m_state;
    }

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

    // FIXME: Reset m_editorState.
    // FIXME: Notify input methods about abandoned composition.
#if PLATFORM(MAC) && !USE(ASYNC_NSTEXTINPUTCLIENT)
    m_temporarilyClosedComposition = false;
#endif

#if PLATFORM(MAC)
    m_pageClient.dismissContentRelativeChildWindows();
#endif

    PageLoadState::Transaction transaction = m_pageLoadState.transaction();
    m_pageLoadState.reset(transaction);
}

WebPageCreationParameters WebPageProxy::creationParameters()
{
    WebPageCreationParameters parameters;

    parameters.viewSize = m_pageClient.viewSize();
    parameters.viewState = m_viewState;
    parameters.drawingAreaType = m_drawingArea->type();
    parameters.store = preferencesStore();
    parameters.pageGroupData = m_pageGroup->data();
    parameters.drawsBackground = m_drawsBackground;
    parameters.drawsTransparentBackground = m_drawsTransparentBackground;
    parameters.underlayColor = m_underlayColor;
    parameters.useFixedLayout = m_useFixedLayout;
    parameters.fixedLayoutSize = m_fixedLayoutSize;
    parameters.suppressScrollbarAnimations = m_suppressScrollbarAnimations;
    parameters.paginationMode = m_paginationMode;
    parameters.paginationBehavesLikeColumns = m_paginationBehavesLikeColumns;
    parameters.pageLength = m_pageLength;
    parameters.gapBetweenPages = m_gapBetweenPages;
    parameters.userAgent = userAgent();
    parameters.itemStates = m_backForwardList->itemStates();
    parameters.sessionID = m_session->getID();
    parameters.highestUsedBackForwardItemID = WebBackForwardListItem::highedUsedItemID();
    parameters.userContentControllerID = m_userContentController ? m_userContentController->identifier() : 0;
    parameters.visitedLinkTableID = m_visitedLinkProvider->identifier();
    parameters.canRunBeforeUnloadConfirmPanel = m_uiClient->canRunBeforeUnloadConfirmPanel();
    parameters.canRunModal = m_canRunModal;
    parameters.deviceScaleFactor = deviceScaleFactor();
    parameters.topContentInset = m_topContentInset;
    parameters.mediaVolume = m_mediaVolume;
    parameters.mayStartMediaWhenInWindow = m_mayStartMediaWhenInWindow;
    parameters.minimumLayoutSize = m_minimumLayoutSize;
    parameters.autoSizingShouldExpandToViewHeight = m_autoSizingShouldExpandToViewHeight;
    parameters.scrollPinningBehavior = m_scrollPinningBehavior;
    parameters.backgroundExtendsBeyondPage = m_backgroundExtendsBeyondPage;
    parameters.layerHostingMode = m_layerHostingMode;
#if ENABLE(REMOTE_INSPECTOR)
    parameters.allowsRemoteInspection = m_allowsRemoteInspection;
#endif
#if PLATFORM(MAC)
    parameters.colorSpace = m_pageClient.colorSpace();
#endif
#if PLATFORM(IOS)
    parameters.screenSize = screenSize();
    parameters.availableScreenSize = availableScreenSize();
    parameters.textAutosizingWidth = textAutosizingWidth();
    parameters.mimeTypesWithCustomContentProviders = m_pageClient.mimeTypesWithCustomContentProviders();
#endif

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

void WebPageProxy::canAuthenticateAgainstProtectionSpaceInFrame(uint64_t frameID, const ProtectionSpace& coreProtectionSpace, bool& canAuthenticate)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebProtectionSpace> protectionSpace = WebProtectionSpace::create(coreProtectionSpace);

    canAuthenticate = m_loaderClient->canAuthenticateAgainstProtectionSpaceInFrame(this, frame, protectionSpace.get());
}

void WebPageProxy::didReceiveAuthenticationChallenge(uint64_t frameID, const AuthenticationChallenge& coreChallenge, uint64_t challengeID)
{
    didReceiveAuthenticationChallengeProxy(frameID, AuthenticationChallengeProxy::create(coreChallenge, challengeID, m_process->connection()));
}

void WebPageProxy::didReceiveAuthenticationChallengeProxy(uint64_t frameID, PassRefPtr<AuthenticationChallengeProxy> prpAuthenticationChallenge)
{
    ASSERT(prpAuthenticationChallenge);

    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<AuthenticationChallengeProxy> authenticationChallenge = prpAuthenticationChallenge;
    m_loaderClient->didReceiveAuthenticationChallengeInFrame(this, frame, authenticationChallenge.get());
}

void WebPageProxy::exceededDatabaseQuota(uint64_t frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, PassRefPtr<Messages::WebPageProxy::ExceededDatabaseQuota::DelayedReply> reply)
{
    ExceededDatabaseQuotaRecords& records = ExceededDatabaseQuotaRecords::shared();
    std::unique_ptr<ExceededDatabaseQuotaRecords::Record> newRecord = records.createRecord(frameID,
        originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage,
        currentDatabaseUsage, expectedUsage, reply);
    records.add(WTF::move(newRecord));

    if (records.areBeingProcessed())
        return;

    ExceededDatabaseQuotaRecords::Record* record = records.next();
    while (record) {
        WebFrameProxy* frame = m_process->webFrame(record->frameID);
        MESSAGE_CHECK(frame);

        RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(SecurityOrigin::createFromDatabaseIdentifier(record->originIdentifier));
        auto currentReply = record->reply;
        m_uiClient->exceededDatabaseQuota(this, frame, origin.get(),
            record->databaseName, record->displayName, record->currentQuota,
            record->currentOriginUsage, record->currentDatabaseUsage, record->expectedUsage,
            [currentReply](unsigned long long newQuota) { currentReply->send(newQuota); });

        record = records.next();
    }
}

void WebPageProxy::reachedApplicationCacheOriginQuota(const String& originIdentifier, uint64_t currentQuota, uint64_t totalBytesNeeded, PassRefPtr<Messages::WebPageProxy::ReachedApplicationCacheOriginQuota::DelayedReply> reply)
{
    RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::createFromDatabaseIdentifier(originIdentifier);
    MESSAGE_CHECK(securityOrigin);

    m_uiClient->reachedApplicationCacheOriginQuota(this, *securityOrigin.get(), currentQuota, totalBytesNeeded, [reply](unsigned long long newQuota) { reply->send(newQuota); });
}

void WebPageProxy::requestGeolocationPermissionForFrame(uint64_t geolocationID, uint64_t frameID, String originIdentifier)
{
    WebFrameProxy* frame = m_process->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // FIXME: Geolocation should probably be using toString() as its string representation instead of databaseIdentifier().
    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(SecurityOrigin::createFromDatabaseIdentifier(originIdentifier));
    RefPtr<GeolocationPermissionRequestProxy> request = m_geolocationPermissionRequestManager.createRequest(geolocationID);

    if (m_uiClient->decidePolicyForGeolocationPermissionRequest(this, frame, origin.get(), request.get()))
        return;

    if (m_pageClient.decidePolicyForGeolocationPermissionRequest(*frame, *origin, *request))
        return;

    request->deny();
}

void WebPageProxy::requestNotificationPermission(uint64_t requestID, const String& originString)
{
    if (!isRequestIDValid(requestID))
        return;

    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::createFromString(originString);
    RefPtr<NotificationPermissionRequest> request = m_notificationPermissionRequestManager.createRequest(requestID);
    
    if (!m_uiClient->decidePolicyForNotificationPermissionRequest(this, origin.get(), request.get()))
        request->deny();
}

void WebPageProxy::showNotification(const String& title, const String& body, const String& iconURL, const String& tag, const String& lang, const String& dir, const String& originString, uint64_t notificationID)
{
    m_process->context().supplement<WebNotificationManagerProxy>()->show(this, title, body, iconURL, tag, lang, dir, originString, notificationID);
}

void WebPageProxy::cancelNotification(uint64_t notificationID)
{
    m_process->context().supplement<WebNotificationManagerProxy>()->cancel(this, notificationID);
}

void WebPageProxy::clearNotifications(const Vector<uint64_t>& notificationIDs)
{
    m_process->context().supplement<WebNotificationManagerProxy>()->clearNotifications(this, notificationIDs);
}

void WebPageProxy::didDestroyNotification(uint64_t notificationID)
{
    m_process->context().supplement<WebNotificationManagerProxy>()->didDestroyNotification(this, notificationID);
}

float WebPageProxy::headerHeight(WebFrameProxy* frame)
{
    if (frame->isDisplayingPDFDocument())
        return 0;
    return m_uiClient->headerHeight(this, frame);
}

float WebPageProxy::footerHeight(WebFrameProxy* frame)
{
    if (frame->isDisplayingPDFDocument())
        return 0;
    return m_uiClient->footerHeight(this, frame);
}

void WebPageProxy::drawHeader(WebFrameProxy* frame, const FloatRect& rect)
{
    if (frame->isDisplayingPDFDocument())
        return;
    m_uiClient->drawHeader(this, frame, rect);
}

void WebPageProxy::drawFooter(WebFrameProxy* frame, const FloatRect& rect)
{
    if (frame->isDisplayingPDFDocument())
        return;
    m_uiClient->drawFooter(this, frame, rect);
}

void WebPageProxy::runModal()
{
    // Since runModal() can (and probably will) spin a nested run loop we need to turn off the responsiveness timer.
    m_process->responsivenessTimer()->stop();

    // Our Connection's run loop might have more messages waiting to be handled after this RunModal message.
    // To make sure they are handled inside of the the nested modal run loop we must first signal the Connection's
    // run loop so we're guaranteed that it has a chance to wake up.
    // See http://webkit.org/b/89590 for more discussion.
    m_process->connection()->wakeUpRunLoop();

    m_uiClient->runModal(this);
}

void WebPageProxy::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    m_visibleScrollerThumbRect = scrollerThumb;
}

void WebPageProxy::recommendedScrollbarStyleDidChange(int32_t newStyle)
{
#if USE(APPKIT)
    m_pageClient.recommendedScrollbarStyleDidChange(newStyle);
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
    m_mainFrameIsPinnedToLeftSide = pinnedToLeftSide;
    m_mainFrameIsPinnedToRightSide = pinnedToRightSide;
    m_mainFrameIsPinnedToTopSide = pinnedToTopSide;
    m_mainFrameIsPinnedToBottomSide = pinnedToBottomSide;

    m_uiClient->pinnedStateDidChange(*this);
}

void WebPageProxy::didChangePageCount(unsigned pageCount)
{
    m_pageCount = pageCount;
}

void WebPageProxy::pageExtendedBackgroundColorDidChange(Color backgroundColor)
{
    m_pageExtendedBackgroundColor = backgroundColor;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebPageProxy::didFailToInitializePlugin(const String& mimeType, const String& frameURLString, const String& pageURLString)
{
    m_loaderClient->didFailToInitializePlugin(this, createPluginInformationDictionary(mimeType, frameURLString, pageURLString).get());
}

void WebPageProxy::didBlockInsecurePluginVersion(const String& mimeType, const String& pluginURLString, const String& frameURLString, const String& pageURLString, bool replacementObscured)
{
    RefPtr<ImmutableDictionary> pluginInformation;

#if PLATFORM(COCOA) && ENABLE(NETSCAPE_PLUGIN_API)
    String newMimeType = mimeType;
    PluginModuleInfo plugin = m_process->context().pluginInfoStore().findPlugin(newMimeType, URL(URL(), pluginURLString));
    pluginInformation = createPluginInformationDictionary(plugin, frameURLString, mimeType, pageURLString, String(), String(), replacementObscured);
#else
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(pluginURLString);
    UNUSED_PARAM(frameURLString);
    UNUSED_PARAM(pageURLString);
    UNUSED_PARAM(replacementObscured);
#endif

    m_loaderClient->didBlockInsecurePluginVersion(this, pluginInformation.get());
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

bool WebPageProxy::willHandleHorizontalScrollEvents() const
{
    return !m_canShortCircuitHorizontalWheelEvents;
}

void WebPageProxy::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
    m_pageClient.didFinishLoadingDataForCustomContentProvider(suggestedFilename, dataReference);
}

void WebPageProxy::backForwardRemovedItem(uint64_t itemID)
{
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
    m_process->send(Messages::WebPage::BeginPrinting(frame->frameID(), printInfo), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::endPrinting()
{
    if (!m_isInPrintingMode)
        return;

    m_isInPrintingMode = false;
    m_process->send(Messages::WebPage::EndPrinting(), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::computePagesForPrinting(WebFrameProxy* frame, const PrintInfo& printInfo, PassRefPtr<ComputedPagesCallback> prpCallback)
{
    RefPtr<ComputedPagesCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_callbacks.put(callback);
    m_isInPrintingMode = true;
    m_process->send(Messages::WebPage::ComputePagesForPrinting(frame->frameID(), printInfo, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

#if PLATFORM(COCOA)
void WebPageProxy::drawRectToImage(WebFrameProxy* frame, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, PassRefPtr<ImageCallback> prpCallback)
{
    RefPtr<ImageCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_callbacks.put(callback);
    m_process->send(Messages::WebPage::DrawRectToImage(frame->frameID(), printInfo, rect, imageSize, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::drawPagesToPDF(WebFrameProxy* frame, const PrintInfo& printInfo, uint32_t first, uint32_t count, PassRefPtr<DataCallback> prpCallback)
{
    RefPtr<DataCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_callbacks.put(callback);
    m_process->send(Messages::WebPage::DrawPagesToPDF(frame->frameID(), printInfo, first, count, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}
#elif PLATFORM(GTK)
void WebPageProxy::drawPagesForPrinting(WebFrameProxy* frame, const PrintInfo& printInfo, PassRefPtr<PrintFinishedCallback> didPrintCallback)
{
    RefPtr<PrintFinishedCallback> callback = didPrintCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_callbacks.put(callback);
    m_isInPrintingMode = true;
    m_process->send(Messages::WebPage::DrawPagesForPrinting(frame->frameID(), printInfo, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? IPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}
#endif

void WebPageProxy::updateBackingStoreDiscardableState()
{
    ASSERT(isValid());

    bool isDiscardable;

    if (!m_process->responsivenessTimer()->isResponsive())
        isDiscardable = false;
    else
        isDiscardable = !m_pageClient.isViewWindowActive() || !isViewVisible();

    m_drawingArea->setBackingStoreIsDiscardable(isDiscardable);
}

void WebPageProxy::saveDataToFileInDownloadsFolder(const String& suggestedFilename, const String& mimeType, const String& originatingURLString, API::Data* data)
{
    m_uiClient->saveDataToFileInDownloadsFolder(this, suggestedFilename, mimeType, originatingURLString, data);
}

void WebPageProxy::savePDFToFileInDownloadsFolder(const String& suggestedFilename, const String& originatingURLString, const IPC::DataReference& dataReference)
{
    if (!suggestedFilename.endsWith(".pdf", false))
        return;

    RefPtr<API::Data> data = API::Data::create(dataReference.data(), dataReference.size());

    saveDataToFileInDownloadsFolder(suggestedFilename, "application/pdf", originatingURLString, data.get());
}

void WebPageProxy::setMinimumLayoutSize(const IntSize& minimumLayoutSize)
{
    if (m_minimumLayoutSize == minimumLayoutSize)
        return;

    m_minimumLayoutSize = minimumLayoutSize;

    if (!isValid())
        return;

    m_process->send(Messages::WebPage::SetMinimumLayoutSize(minimumLayoutSize), m_pageID, 0);
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

    m_process->send(Messages::WebPage::SetAutoSizingShouldExpandToViewHeight(shouldExpand), m_pageID, 0);
}

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

void WebPageProxy::recordAutocorrectionResponse(int32_t responseType, const String& replacedString, const String& replacementString)
{
    m_pageClient.recordAutocorrectionResponse((AutocorrectionResponseType)responseType, replacedString, replacementString);
}

void WebPageProxy::handleAlternativeTextUIResult(const String& result)
{
    if (!isClosed())
        m_process->send(Messages::WebPage::HandleAlternativeTextUIResult(result), m_pageID, 0);
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

#endif // PLATFORM(MAC)

#if PLATFORM(COCOA)
PassRefPtr<ViewSnapshot> WebPageProxy::takeViewSnapshot()
{
    return m_pageClient.takeViewSnapshot();
}
#endif

#if USE(SOUP) && !ENABLE(CUSTOM_PROTOCOLS)
void WebPageProxy::didReceiveURIRequest(String uriString, uint64_t requestID)
{
    m_process->context().supplement<WebSoupRequestManagerProxy>()->didReceiveURIRequest(uriString, this, requestID);
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

#if ENABLE(SUBTLE_CRYPTO)
void WebPageProxy::wrapCryptoKey(const Vector<uint8_t>& key, bool& succeeded, Vector<uint8_t>& wrappedKey)
{
    Vector<uint8_t> masterKey;

    if (RefPtr<API::Data> keyData = m_loaderClient->webCryptoMasterKey(*this))
        masterKey = keyData->dataReference().vector();
    else if (!getDefaultWebCryptoMasterKey(masterKey)) {
        succeeded = false;
        return;
    }

    succeeded = wrapSerializedCryptoKey(masterKey, key, wrappedKey);
}

void WebPageProxy::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, bool& succeeded, Vector<uint8_t>& key)
{
    Vector<uint8_t> masterKey;

    if (RefPtr<API::Data> keyData = m_loaderClient->webCryptoMasterKey(*this))
        masterKey = keyData->dataReference().vector();
    else if (!getDefaultWebCryptoMasterKey(masterKey)) {
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

void WebPageProxy::insertTextAsync(const String& text, const EditingRange& replacementRange, bool registerUndoGroup)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::InsertTextAsync(text, replacementRange, registerUndoGroup), m_pageID);
}

void WebPageProxy::getMarkedRangeAsync(std::function<void (EditingRange, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    process().send(Messages::WebPage::GetMarkedRangeAsync(callbackID), m_pageID);
}

void WebPageProxy::getSelectedRangeAsync(std::function<void (EditingRange, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    process().send(Messages::WebPage::GetSelectedRangeAsync(callbackID), m_pageID);
}

void WebPageProxy::characterIndexForPointAsync(const WebCore::IntPoint& point, std::function<void (uint64_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    process().send(Messages::WebPage::CharacterIndexForPointAsync(point, callbackID), m_pageID);
}

void WebPageProxy::firstRectForCharacterRangeAsync(const EditingRange& range, std::function<void (const WebCore::IntRect&, const EditingRange&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(WebCore::IntRect(), EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    process().send(Messages::WebPage::FirstRectForCharacterRangeAsync(range, callbackID), m_pageID);
}

void WebPageProxy::setCompositionAsync(const String& text, Vector<CompositionUnderline> underlines, const EditingRange& selectionRange, const EditingRange& replacementRange)
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

#endif

void WebPageProxy::takeSnapshot(IntRect rect, IntSize bitmapSize, SnapshotOptions options, std::function<void (const ShareableBitmap::Handle&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(ShareableBitmap::Handle(), CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTF::move(callbackFunction), std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_process->throttler()));
    m_process->send(Messages::WebPage::TakeSnapshot(rect, bitmapSize, options, callbackID), m_pageID);
}

void WebPageProxy::navigationGestureDidBegin()
{
    m_isShowingNavigationGestureSnapshot = true;
    m_pageClient.navigationGestureDidBegin();
}

void WebPageProxy::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    m_pageClient.navigationGestureWillEnd(willNavigate, item);
}

void WebPageProxy::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    m_pageClient.navigationGestureDidEnd(willNavigate, item);
}

void WebPageProxy::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    m_pageClient.willRecordNavigationSnapshot(item);
}

void WebPageProxy::navigationGestureSnapshotWasRemoved()
{
    m_isShowingNavigationGestureSnapshot = false;
}

void WebPageProxy::willChangeCurrentHistoryItemForMainFrame()
{
    recordNavigationSnapshot();
}

void WebPageProxy::removeNavigationGestureSnapshot()
{
    m_pageClient.removeNavigationGestureSnapshot();
}

void WebPageProxy::performActionMenuHitTestAtLocation(FloatPoint point, bool forImmediateAction)
{
    m_process->send(Messages::WebPage::PerformActionMenuHitTestAtLocation(point, forImmediateAction), m_pageID);
}

void WebPageProxy::selectLastActionMenuRange()
{
    m_process->send(Messages::WebPage::SelectLastActionMenuRange(), m_pageID);
}

void WebPageProxy::focusAndSelectLastActionMenuHitTestResult()
{
    m_process->send(Messages::WebPage::FocusAndSelectLastActionMenuHitTestResult(), m_pageID);
}

void WebPageProxy::didPerformActionMenuHitTest(const ActionMenuHitTestResult& result, IPC::MessageDecoder& decoder)
{
    RefPtr<API::Object> userData;
    WebContextUserMessageDecoder messageDecoder(userData, process());
    if (!decoder.decode(messageDecoder))
        return;

    m_pageClient.didPerformActionMenuHitTest(result, userData.get());
}

void WebPageProxy::setShouldDispatchFakeMouseMoveEvents(bool shouldDispatchFakeMouseMoveEvents)
{
    m_process->send(Messages::WebPage::SetShouldDispatchFakeMouseMoveEvents(shouldDispatchFakeMouseMoveEvents), m_pageID);
}

} // namespace WebKit
