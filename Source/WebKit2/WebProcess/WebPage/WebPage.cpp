/*
 * Copyright (C) 2010, 2011, 2012, 2013-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "WebPage.h"

#include "APIArray.h"
#include "APIGeometry.h"
#include "Arguments.h"
#include "DataReference.h"
#include "DragControllerAction.h"
#include "DrawingArea.h"
#include "DrawingAreaMessages.h"
#include "EditorState.h"
#include "EventDispatcher.h"
#include "InjectedBundle.h"
#include "InjectedBundleBackForwardList.h"
#include "Logging.h"
#include "NetscapePlugin.h"
#include "NotificationPermissionRequestManager.h"
#include "PageBanner.h"
#include "PluginProcessAttributes.h"
#include "PluginProxy.h"
#include "PluginView.h"
#include "PrintInfo.h"
#include "SessionState.h"
#include "SessionStateConversion.h"
#include "SessionTracker.h"
#include "ShareableBitmap.h"
#include "VisitedLinkTableController.h"
#include "WKBundleAPICast.h"
#include "WKRetainPtr.h"
#include "WKSharedAPICast.h"
#include "WebAlternativeTextClient.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListProxy.h"
#include "WebChromeClient.h"
#include "WebColorChooser.h"
#include "WebContextMenu.h"
#include "WebContextMenuClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseProvider.h"
#include "WebDiagnosticLoggingClient.h"
#include "WebDocumentLoader.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebEventFactory.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebFullScreenManager.h"
#include "WebFullScreenManagerMessages.h"
#include "WebGeolocationClient.h"
#include "WebImage.h"
#include "WebInspector.h"
#include "WebInspectorClient.h"
#include "WebInspectorMessages.h"
#include "WebInspectorUI.h"
#include "WebInspectorUIMessages.h"
#include "WebMediaKeyStorageManager.h"
#include "WebNotificationClient.h"
#include "WebOpenPanelResultListener.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroupProxy.h"
#include "WebPageMessages.h"
#include "WebPageOverlay.h"
#include "WebPageProxyMessages.h"
#include "WebPlugInClient.h"
#include "WebPopupMenu.h"
#include "WebPreferencesDefinitions.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebProgressTrackerClient.h"
#include "WebStorageNamespaceProvider.h"
#include "WebUndoStep.h"
#include "WebUserContentController.h"
#include "WebUserMediaClient.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/Chrome.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/DataTransfer.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/ElementIterator.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/FormState.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTMLPlugInImageElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MainFrame.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PageThrottler.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PrintContext.h>
#include <WebCore/Range.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/SessionID.h>
#include <WebCore/Settings.h>
#include <WebCore/ShadowRoot.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/StyleProperties.h>
#include <WebCore/SubframeLoader.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UserInputBridge.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/VisibleUnits.h>
#include <WebCore/markup.h>
#include <bindings/ScriptValue.h>
#include <profiler/ProfilerDatabase.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSCJSValue.h>
#include <runtime/JSLock.h>
#include <wtf/RunLoop.h>
#include <wtf/TemporaryChange.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResult.h"
#endif

#if ENABLE(MHTML)
#include <WebCore/MHTMLArchive.h>
#endif

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryClient.h"
#endif

#if ENABLE(VIBRATION)
#include "WebVibrationClient.h"
#endif

#if ENABLE(PROXIMITY_EVENTS)
#include "WebDeviceProximityClient.h"
#endif

#if PLATFORM(COCOA)
#include "PDFPlugin.h"
#include "RemoteLayerTreeTransaction.h"
#include "WKStringCF.h"
#include "WebVideoFullscreenManager.h"
#include <WebCore/LegacyWebArchive.h>
#endif

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#include "DataObjectGtk.h"
#include "WebPrintOperationGtk.h"
#endif

#if PLATFORM(IOS)
#include "RemoteLayerTreeDrawingArea.h"
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/CoreTextSPI.h>
#include <WebCore/Icon.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
#include "LayerTreeHost.h"
#endif

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
#include "CoordinatedLayerTreeHostMessages.h"
#endif

#if ENABLE(DATA_DETECTION)
#include <WebCore/DataDetection.h>
#endif

#if ENABLE(VIDEO) && USE(GSTREAMER)
#include <WebCore/MediaPlayerRequestInstallMissingPluginsCallback.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebPageIncludes.h>
#endif

using namespace JSC;
using namespace WebCore;

namespace WebKit {

static const double pageScrollHysteresisSeconds = 0.3;

class SendStopResponsivenessTimer {
public:
    SendStopResponsivenessTimer(WebPage* page)
        : m_page(page)
    {
    }
    
    ~SendStopResponsivenessTimer()
    {
        m_page->send(Messages::WebPageProxy::StopResponsivenessTimer());
    }

private:
    WebPage* m_page;
};

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageCounter, ("WebPage"));

Ref<WebPage> WebPage::create(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    Ref<WebPage> page = adoptRef(*new WebPage(pageID, parameters));

    if (page->pageGroup()->isVisibleToInjectedBundle() && WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->didCreatePage(page.ptr());

    return page;
}

WebPage::WebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
    : m_pageID(pageID)
    , m_viewSize(parameters.viewSize)
    , m_hasSeenPlugin(false)
    , m_useFixedLayout(false)
    , m_drawsBackground(true)
    , m_isInRedo(false)
    , m_isClosed(false)
    , m_tabToLinks(false)
    , m_asynchronousPluginInitializationEnabled(false)
    , m_asynchronousPluginInitializationEnabledForAllPlugins(false)
    , m_artificialPluginInitializationDelayEnabled(false)
    , m_scrollingPerformanceLoggingEnabled(false)
    , m_mainFrameIsScrollable(true)
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    , m_readyToFindPrimarySnapshottedPlugin(false)
    , m_didFindPrimarySnapshottedPlugin(false)
    , m_numberOfPrimarySnapshotDetectionAttempts(0)
    , m_determinePrimarySnapshottedPlugInTimer(RunLoop::main(), this, &WebPage::determinePrimarySnapshottedPlugInTimerFired)
#endif
    , m_layerHostingMode(parameters.layerHostingMode)
#if PLATFORM(COCOA)
    , m_pdfPluginEnabled(false)
    , m_hasCachedWindowFrame(false)
    , m_viewGestureGeometryCollector(*this)
#elif HAVE(ACCESSIBILITY) && (PLATFORM(GTK) || PLATFORM(EFL))
    , m_accessibilityObject(nullptr)
#endif
    , m_setCanStartMediaTimer(RunLoop::main(), this, &WebPage::setCanStartMediaTimerFired)
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuClient(std::make_unique<API::InjectedBundle::PageContextMenuClient>())
#endif
    , m_formClient(std::make_unique<API::InjectedBundle::FormClient>())
    , m_uiClient(std::make_unique<API::InjectedBundle::PageUIClient>())
    , m_findController(this)
#if ENABLE(INPUT_TYPE_COLOR)
    , m_activeColorChooser(0)
#endif
    , m_userContentController(WebUserContentController::getOrCreate(parameters.userContentControllerID))
#if ENABLE(GEOLOCATION)
    , m_geolocationPermissionRequestManager(this)
#endif
#if ENABLE(MEDIA_STREAM)
    , m_userMediaPermissionRequestManager(*this)
#endif
    , m_pageScrolledHysteresis([this](HysteresisState state) { if (state == HysteresisState::Stopped) pageStoppedScrolling(); }, pageScrollHysteresisSeconds)
    , m_canRunBeforeUnloadConfirmPanel(parameters.canRunBeforeUnloadConfirmPanel)
    , m_canRunModal(parameters.canRunModal)
    , m_isRunningModal(false)
#if ENABLE(DRAG_SUPPORT)
    , m_isStartingDrag(false)
#endif
    , m_cachedMainFrameIsPinnedToLeftSide(true)
    , m_cachedMainFrameIsPinnedToRightSide(true)
    , m_cachedMainFrameIsPinnedToTopSide(true)
    , m_cachedMainFrameIsPinnedToBottomSide(true)
    , m_canShortCircuitHorizontalWheelEvents(false)
    , m_hasWheelEventHandlers(false)
    , m_cachedPageCount(0)
    , m_autoSizingShouldExpandToViewHeight(false)
#if ENABLE(CONTEXT_MENUS)
    , m_isShowingContextMenu(false)
#endif
#if PLATFORM(IOS)
    , m_selectionAnchor(Start)
    , m_hasReceivedVisibleContentRectsAfterDidCommitLoad(false)
    , m_scaleWasSetByUIProcess(false)
    , m_userHasChangedPageScaleFactor(false)
    , m_hasStablePageScaleFactor(true)
    , m_userIsInteracting(false)
    , m_hasPendingBlurNotification(false)
    , m_useTestingViewportConfiguration(false)
    , m_isInStableState(true)
    , m_oldestNonStableUpdateVisibleContentRectsTimestamp(std::chrono::milliseconds::zero())
    , m_estimatedLatency(std::chrono::milliseconds::zero())
    , m_screenSize(parameters.screenSize)
    , m_availableScreenSize(parameters.availableScreenSize)
    , m_deviceOrientation(0)
    , m_inDynamicSizeUpdate(false)
    , m_volatilityTimer(*this, &WebPage::volatilityTimerFired)
#endif
    , m_backgroundColor(Color::white)
    , m_maximumRenderingSuppressionToken(0)
    , m_scrollPinningBehavior(DoNotPin)
    , m_useAsyncScrolling(false)
    , m_viewState(parameters.viewState)
    , m_processSuppressionEnabled(true)
    , m_userActivity("Process suppression disabled for page.")
    , m_pendingNavigationID(0)
#if ENABLE(WEBGL)
    , m_systemWebGLPolicy(WebGLAllowCreation)
#endif
    , m_mainFrameProgressCompleted(false)
    , m_shouldDispatchFakeMouseMoveEvents(true)
{
    ASSERT(m_pageID);

    m_pageGroup = WebProcess::singleton().webPageGroup(parameters.pageGroupData);

#if PLATFORM(IOS)
    Settings::setShouldManageAudioSessionCategory(true);
#endif

    PageConfiguration pageConfiguration;
    pageConfiguration.chromeClient = new WebChromeClient(this);
#if ENABLE(CONTEXT_MENUS)
    pageConfiguration.contextMenuClient = new WebContextMenuClient(this);
#endif
    pageConfiguration.editorClient = new WebEditorClient(this);
#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient = new WebDragClient(this);
#endif
    pageConfiguration.backForwardClient = WebBackForwardListProxy::create(this);
    pageConfiguration.inspectorClient = new WebInspectorClient(this);
#if USE(AUTOCORRECTION_PANEL)
    pageConfiguration.alternativeTextClient = new WebAlternativeTextClient(this);
#endif

    pageConfiguration.plugInClient = new WebPlugInClient(*this);
    pageConfiguration.loaderClientForMainFrame = new WebFrameLoaderClient;
    pageConfiguration.progressTrackerClient = new WebProgressTrackerClient(*this);
    pageConfiguration.diagnosticLoggingClient = new WebDiagnosticLoggingClient(*this);

    pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroup->pageGroupID());
    pageConfiguration.storageNamespaceProvider = WebStorageNamespaceProvider::getOrCreate(m_pageGroup->pageGroupID());
    pageConfiguration.userContentController = &m_userContentController->userContentController();
    pageConfiguration.visitedLinkStore = VisitedLinkTableController::getOrCreate(parameters.visitedLinkTableID);

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebPageInitialization.h>
#endif

    m_page = std::make_unique<Page>(pageConfiguration);

    m_drawingArea = DrawingArea::create(*this, parameters);
    m_drawingArea->setPaintingEnabled(false);
    m_drawingArea->setShouldScaleViewToFitDocument(parameters.shouldScaleViewToFitDocument);

#if ENABLE(ASYNC_SCROLLING)
    m_useAsyncScrolling = parameters.store.getBoolValueForKey(WebPreferencesKey::threadedScrollingEnabledKey());
    if (!m_drawingArea->supportsAsyncScrolling())
        m_useAsyncScrolling = false;
    m_page->settings().setScrollingCoordinatorEnabled(m_useAsyncScrolling);
#endif

    m_mainFrame = WebFrame::createWithCoreMainFrame(this, &m_page->mainFrame());

#if ENABLE(BATTERY_STATUS)
    WebCore::provideBatteryTo(m_page.get(), new WebBatteryClient(this));
#endif
#if ENABLE(GEOLOCATION)
    WebCore::provideGeolocationTo(m_page.get(), new WebGeolocationClient(this));
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::provideNotification(m_page.get(), new WebNotificationClient(this));
#endif
#if ENABLE(VIBRATION)
    WebCore::provideVibrationTo(m_page.get(), new WebVibrationClient(this));
#endif
#if ENABLE(PROXIMITY_EVENTS)
    WebCore::provideDeviceProximityTo(m_page.get(), new WebDeviceProximityClient(this));
#endif
#if ENABLE(MEDIA_STREAM)
    WebCore::provideUserMediaTo(m_page.get(), new WebUserMediaClient(*this));
#endif

#if ENABLE(REMOTE_INSPECTOR)
    m_page->setRemoteInspectionAllowed(parameters.allowsRemoteInspection);
    m_page->setRemoteInspectionNameOverride(parameters.remoteInspectionNameOverride);
#endif

    m_page->setCanStartMedia(false);
    m_mayStartMediaWhenInWindow = parameters.mayStartMediaWhenInWindow;

    m_page->setGroupName(m_pageGroup->identifier());
    m_page->setDeviceScaleFactor(parameters.deviceScaleFactor);
#if PLATFORM(IOS)
    m_page->setTextAutosizingWidth(parameters.textAutosizingWidth);
#endif

    updatePreferences(parameters.store);
    platformInitialize();

    setUseFixedLayout(parameters.useFixedLayout);

    setDrawsBackground(parameters.drawsBackground);

    setUnderlayColor(parameters.underlayColor);

    setPaginationMode(parameters.paginationMode);
    setPaginationBehavesLikeColumns(parameters.paginationBehavesLikeColumns);
    setPageLength(parameters.pageLength);
    setGapBetweenPages(parameters.gapBetweenPages);
    setPaginationLineGridEnabled(parameters.paginationLineGridEnabled);
    
    // If the page is created off-screen, its visibilityState should be prerender.
    m_page->setViewState(m_viewState);
    if (!isVisible())
        m_page->setIsPrerender();
    updateUserActivity();

    updateIsInWindow(true);

    setMinimumLayoutSize(parameters.minimumLayoutSize);
    setAutoSizingShouldExpandToViewHeight(parameters.autoSizingShouldExpandToViewHeight);
    
    setScrollPinningBehavior(parameters.scrollPinningBehavior);
    if (parameters.scrollbarOverlayStyle)
        m_scrollbarOverlayStyle = static_cast<ScrollbarOverlayStyle>(parameters.scrollbarOverlayStyle.value());
    else
        m_scrollbarOverlayStyle = WTF::Optional<ScrollbarOverlayStyle>();

    setBackgroundExtendsBeyondPage(parameters.backgroundExtendsBeyondPage);

    setTopContentInset(parameters.topContentInset);

    m_userAgent = parameters.userAgent;

    WebBackForwardListProxy::setHighestItemIDFromUIProcess(parameters.highestUsedBackForwardItemID);
    
    if (!parameters.itemStates.isEmpty())
        restoreSessionInternal(parameters.itemStates, WasRestoredByAPIRequest::No);

    if (parameters.sessionID.isValid())
        setSessionID(parameters.sessionID);

    m_drawingArea->setPaintingEnabled(true);
    
    setMediaVolume(parameters.mediaVolume);

    setMuted(parameters.muted);

    // We use the DidFirstVisuallyNonEmptyLayout milestone to determine when to unfreeze the layer tree.
    m_page->addLayoutMilestones(DidFirstLayout | DidFirstVisuallyNonEmptyLayout);

    auto& webProcess = WebProcess::singleton();
    webProcess.addMessageReceiver(Messages::WebPage::messageReceiverName(), m_pageID, *this);

    // FIXME: This should be done in the object constructors, and the objects themselves should be message receivers.
#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    webProcess.addMessageReceiver(Messages::CoordinatedLayerTreeHost::messageReceiverName(), m_pageID, *this);
#endif
    webProcess.addMessageReceiver(Messages::WebInspector::messageReceiverName(), m_pageID, *this);
    webProcess.addMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_pageID, *this);
#if ENABLE(FULLSCREEN_API)
    webProcess.addMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_pageID, *this);
#endif

#ifndef NDEBUG
    webPageCounter.increment();
#endif

#if ENABLE(ASYNC_SCROLLING)
    if (m_useAsyncScrolling)
        webProcess.eventDispatcher().addScrollingTreeForPage(this);
#endif

    for (auto& mimeType : parameters.mimeTypesWithCustomContentProviders)
        m_mimeTypesWithCustomContentProviders.add(mimeType);


#if ENABLE(ENCRYPTED_MEDIA_V2)
    if (WebMediaKeyStorageManager* manager = webProcess.supplement<WebMediaKeyStorageManager>())
        m_page->settings().setMediaKeysStorageDirectory(manager->mediaKeyStorageDirectory());
#endif
    m_page->settings().setAppleMailPaginationQuirkEnabled(parameters.appleMailPaginationQuirkEnabled);

    if (parameters.viewScaleFactor != 1)
        scaleView(parameters.viewScaleFactor);

#if PLATFORM(IOS)
    m_page->settings().setContentDispositionAttachmentSandboxEnabled(true);
#endif
}

void WebPage::reinitializeWebPage(const WebPageCreationParameters& parameters)
{
    if (m_viewState != parameters.viewState)
        setViewState(parameters.viewState, false, Vector<uint64_t>());
    if (m_layerHostingMode != parameters.layerHostingMode)
        setLayerHostingMode(static_cast<unsigned>(parameters.layerHostingMode));
}

void WebPage::setPageActivityState(PageActivityState::Flags activityState)
{
    PageActivityState::Flags changed = m_activityState ^ activityState;
    m_activityState = activityState;

    if (changed)
        updateUserActivity();
}

void WebPage::updateUserActivity()
{
    // Start the activity to prevent AppNap if the page activity is in progress,
    // the page is visible and non-idle, or process suppression is disabled.
    if (m_activityState || !(m_viewState & ViewState::IsVisuallyIdle) || !m_processSuppressionEnabled)
        m_userActivity.start();
    else
        m_userActivity.stop();
}

WebPage::~WebPage()
{
    if (m_backForwardList)
        m_backForwardList->detach();

    ASSERT(!m_page);

    auto& webProcess = WebProcess::singleton();
#if ENABLE(ASYNC_SCROLLING)
    if (m_useAsyncScrolling)
        webProcess.eventDispatcher().removeScrollingTreeForPage(this);
#endif

    platformDetach();
    
    m_sandboxExtensionTracker.invalidate();

    for (auto* pluginView : m_pluginViews)
        pluginView->webPageDestroyed();

#if !PLATFORM(IOS)
    if (m_headerBanner)
        m_headerBanner->detachFromPage();
    if (m_footerBanner)
        m_footerBanner->detachFromPage();
#endif // !PLATFORM(IOS)

    webProcess.removeMessageReceiver(Messages::WebPage::messageReceiverName(), m_pageID);

    // FIXME: This should be done in the object destructors, and the objects themselves should be message receivers.
#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    webProcess.removeMessageReceiver(Messages::CoordinatedLayerTreeHost::messageReceiverName(), m_pageID);
#endif
    webProcess.removeMessageReceiver(Messages::WebInspector::messageReceiverName(), m_pageID);
    webProcess.removeMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_pageID);
#if ENABLE(FULLSCREEN_API)
    webProcess.removeMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_pageID);
#endif

#ifndef NDEBUG
    webPageCounter.decrement();
#endif
}

void WebPage::dummy(bool&)
{
}

IPC::Connection* WebPage::messageSenderConnection()
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t WebPage::messageSenderDestinationID()
{
    return pageID();
}

#if ENABLE(CONTEXT_MENUS)
void WebPage::setInjectedBundleContextMenuClient(std::unique_ptr<API::InjectedBundle::PageContextMenuClient> contextMenuClient)
{
    if (!contextMenuClient) {
        m_contextMenuClient = std::make_unique<API::InjectedBundle::PageContextMenuClient>();
        return;
    }

    m_contextMenuClient = WTFMove(contextMenuClient);
}
#endif

void WebPage::initializeInjectedBundleEditorClient(WKBundlePageEditorClientBase* client)
{
    m_editorClient.initialize(client);
}

void WebPage::setInjectedBundleFormClient(std::unique_ptr<API::InjectedBundle::FormClient> formClient)
{
    if (!formClient) {
        m_formClient = std::make_unique<API::InjectedBundle::FormClient>();
        return;
    }

    m_formClient = WTFMove(formClient);
}

void WebPage::initializeInjectedBundleLoaderClient(WKBundlePageLoaderClientBase* client)
{
    m_loaderClient.initialize(client);

    // It would be nice to get rid of this code and transition all clients to using didLayout instead of
    // didFirstLayoutInFrame and didFirstVisuallyNonEmptyLayoutInFrame. In the meantime, this is required
    // for backwards compatibility.
    LayoutMilestones milestones = 0;
    if (client) {
        if (m_loaderClient.client().didFirstLayoutForFrame)
            milestones |= WebCore::DidFirstLayout;
        if (m_loaderClient.client().didFirstVisuallyNonEmptyLayoutForFrame)
            milestones |= WebCore::DidFirstVisuallyNonEmptyLayout;
    }

    if (milestones)
        listenForLayoutMilestones(milestones);
}

void WebPage::initializeInjectedBundlePolicyClient(WKBundlePagePolicyClientBase* client)
{
    m_policyClient.initialize(client);
}

void WebPage::initializeInjectedBundleResourceLoadClient(WKBundlePageResourceLoadClientBase* client)
{
    m_resourceLoadClient.initialize(client);
}

void WebPage::setInjectedBundleUIClient(std::unique_ptr<API::InjectedBundle::PageUIClient> uiClient)
{
    if (!uiClient) {
        m_uiClient = std::make_unique<API::InjectedBundle::PageUIClient>();
        return;
    }

    m_uiClient = WTFMove(uiClient);
}

#if ENABLE(FULLSCREEN_API)
void WebPage::initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClientBase* client)
{
    m_fullScreenClient.initialize(client);
}
#endif

void WebPage::initializeInjectedBundleDiagnosticLoggingClient(WKBundlePageDiagnosticLoggingClientBase* client)
{
    m_logDiagnosticMessageClient.initialize(client);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
PassRefPtr<Plugin> WebPage::createPlugin(WebFrame* frame, HTMLPlugInElement* pluginElement, const Plugin::Parameters& parameters, String& newMIMEType)
{
    String frameURLString = frame->coreFrame()->loader().documentLoader()->responseURL().string();
    String pageURLString = m_page->mainFrame().loader().documentLoader()->responseURL().string();

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    HTMLPlugInImageElement& pluginImageElement = downcast<HTMLPlugInImageElement>(*pluginElement);
    unsigned pluginArea = 0;
    PluginProcessType processType = pluginElement->displayState() == HTMLPlugInElement::WaitingForSnapshot && !(plugInIsPrimarySize(pluginImageElement, pluginArea) && !plugInIntersectsSearchRect(pluginImageElement)) ? PluginProcessTypeSnapshot : PluginProcessTypeNormal;
#else
    PluginProcessType processType = pluginElement->displayState() == HTMLPlugInElement::WaitingForSnapshot ? PluginProcessTypeSnapshot : PluginProcessTypeNormal;
#endif

    bool allowOnlyApplicationPlugins = !frame->coreFrame()->loader().subframeLoader().allowPlugins();

    uint64_t pluginProcessToken;
    uint32_t pluginLoadPolicy;
    String unavailabilityDescription;
    if (!sendSync(Messages::WebPageProxy::FindPlugin(parameters.mimeType, static_cast<uint32_t>(processType), parameters.url.string(), frameURLString, pageURLString, allowOnlyApplicationPlugins), Messages::WebPageProxy::FindPlugin::Reply(pluginProcessToken, newMIMEType, pluginLoadPolicy, unavailabilityDescription)))
        return nullptr;

    PluginModuleLoadPolicy loadPolicy = static_cast<PluginModuleLoadPolicy>(pluginLoadPolicy);
    bool isBlockedPlugin = (loadPolicy == PluginModuleBlockedForSecurity) || (loadPolicy == PluginModuleBlockedForCompatibility);

    if (isBlockedPlugin || !pluginProcessToken) {
#if ENABLE(PDFKIT_PLUGIN)
        String path = parameters.url.path();
        if (shouldUsePDFPlugin() && (MIMETypeRegistry::isPDFOrPostScriptMIMEType(parameters.mimeType) || (parameters.mimeType.isEmpty() && (path.endsWith(".pdf", false) || path.endsWith(".ps", false))))) {
            RefPtr<PDFPlugin> pdfPlugin = PDFPlugin::create(frame);
            return pdfPlugin.release();
        }
#else
        UNUSED_PARAM(frame);
#endif
    }

    if (isBlockedPlugin) {
        bool replacementObscured = false;
        if (is<RenderEmbeddedObject>(*pluginElement->renderer())) {
            auto& renderObject = downcast<RenderEmbeddedObject>(*pluginElement->renderer());
            renderObject.setPluginUnavailabilityReasonWithDescription(RenderEmbeddedObject::InsecurePluginVersion, unavailabilityDescription);
            replacementObscured = renderObject.isReplacementObscured();
            renderObject.setUnavailablePluginIndicatorIsHidden(replacementObscured);
        }

        send(Messages::WebPageProxy::DidBlockInsecurePluginVersion(parameters.mimeType, parameters.url.string(), frameURLString, pageURLString, replacementObscured));
        return nullptr;
    }

    if (!pluginProcessToken)
        return nullptr;

    bool isRestartedProcess = (pluginElement->displayState() == HTMLPlugInElement::Restarting || pluginElement->displayState() == HTMLPlugInElement::RestartingWithPendingMouseClick);
    return PluginProxy::create(pluginProcessToken, isRestartedProcess);
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL) && !PLATFORM(COCOA)
WebCore::WebGLLoadPolicy WebPage::webGLPolicyForURL(WebFrame*, const String& /* url */)
{
    return WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebPage::resolveWebGLPolicyForURL(WebFrame*, const String& /* url */)
{
    return WebGLAllowCreation;
}
#endif

EditorState WebPage::editorState(IncludePostLayoutDataHint shouldIncludePostLayoutData) const
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    EditorState result;

    if (PluginView* pluginView = focusedPluginViewForFrame(frame)) {
        if (!pluginView->getSelectionString().isNull()) {
            result.selectionIsNone = false;
            result.selectionIsRange = true;
            result.isInPlugin = true;
            return result;
        }
    }

    const VisibleSelection& selection = frame.selection().selection();

    result.selectionIsNone = selection.isNone();
    result.selectionIsRange = selection.isRange();
    result.isContentEditable = selection.isContentEditable();
    result.isContentRichlyEditable = selection.isContentRichlyEditable();
    result.isInPasswordField = selection.isInPasswordField();
    result.hasComposition = frame.editor().hasComposition();
    result.shouldIgnoreCompositionSelectionChange = frame.editor().ignoreCompositionSelectionChange();

#if PLATFORM(COCOA)
    if (shouldIncludePostLayoutData == IncludePostLayoutDataHint::Yes && result.isContentEditable) {
        auto& postLayoutData = result.postLayoutData();
        if (!selection.isNone()) {
            Node* nodeToRemove;
            if (RenderStyle* style = Editor::styleForSelectionStart(&frame, nodeToRemove)) {
                if (style->fontCascade().weight() >= FontWeightBold)
                    postLayoutData.typingAttributes |= AttributeBold;
                if (style->fontCascade().italic() == FontItalicOn)
                    postLayoutData.typingAttributes |= AttributeItalics;

                RefPtr<EditingStyle> typingStyle = frame.selection().typingStyle();
                if (typingStyle && typingStyle->style()) {
                    String value = typingStyle->style()->getPropertyValue(CSSPropertyWebkitTextDecorationsInEffect);
                if (value.contains("underline"))
                    postLayoutData.typingAttributes |= AttributeUnderline;
                } else {
                    if (style->textDecorationsInEffect() & TextDecorationUnderline)
                        postLayoutData.typingAttributes |= AttributeUnderline;
                }

                if (style->visitedDependentColor(CSSPropertyColor).isValid())
                    postLayoutData.textColor = style->visitedDependentColor(CSSPropertyColor);

                switch (style->textAlign()) {
                case RIGHT:
                case WEBKIT_RIGHT:
                    postLayoutData.textAlignment = RightAlignment;
                    break;
                case LEFT:
                case WEBKIT_LEFT:
                    postLayoutData.textAlignment = LeftAlignment;
                    break;
                case CENTER:
                case WEBKIT_CENTER:
                    postLayoutData.textAlignment = CenterAlignment;
                    break;
                case JUSTIFY:
                    postLayoutData.textAlignment = JustifiedAlignment;
                    break;
                case TASTART:
                    postLayoutData.textAlignment = style->isLeftToRightDirection() ? LeftAlignment : RightAlignment;
                    break;
                case TAEND:
                    postLayoutData.textAlignment = style->isLeftToRightDirection() ? RightAlignment : LeftAlignment;
                    break;
                }

                if (nodeToRemove)
                    nodeToRemove->remove(ASSERT_NO_EXCEPTION);
            }
        }
    }
#endif

    platformEditorState(frame, result, shouldIncludePostLayoutData);

    return result;
}

String WebPage::renderTreeExternalRepresentation() const
{
    return externalRepresentation(m_mainFrame->coreFrame(), RenderAsTextBehaviorNormal);
}

String WebPage::renderTreeExternalRepresentationForPrinting() const
{
    return externalRepresentation(m_mainFrame->coreFrame(), RenderAsTextPrintingMode);
}

uint64_t WebPage::renderTreeSize() const
{
    if (!m_page)
        return 0;
    return m_page->renderTreeSize();
}

void WebPage::setTracksRepaints(bool trackRepaints)
{
    if (FrameView* view = mainFrameView())
        view->setTracksRepaints(trackRepaints);
}

bool WebPage::isTrackingRepaints() const
{
    if (FrameView* view = mainFrameView())
        return view->isTrackingRepaints();

    return false;
}

void WebPage::resetTrackedRepaints()
{
    if (FrameView* view = mainFrameView())
        view->resetTrackedRepaints();
}

Ref<API::Array> WebPage::trackedRepaintRects()
{
    FrameView* view = mainFrameView();
    if (!view)
        return API::Array::create();

    Vector<RefPtr<API::Object>> repaintRects;
    repaintRects.reserveInitialCapacity(view->trackedRepaintRects().size());

    for (const auto& repaintRect : view->trackedRepaintRects())
        repaintRects.uncheckedAppend(API::Rect::create(toAPI(repaintRect)));

    return API::Array::create(WTFMove(repaintRects));
}

PluginView* WebPage::focusedPluginViewForFrame(Frame& frame)
{
    if (!frame.document()->isPluginDocument())
        return 0;

    PluginDocument* pluginDocument = static_cast<PluginDocument*>(frame.document());

    if (pluginDocument->focusedElement() != pluginDocument->pluginElement())
        return 0;

    PluginView* pluginView = static_cast<PluginView*>(pluginDocument->pluginWidget());
    return pluginView;
}

PluginView* WebPage::pluginViewForFrame(Frame* frame)
{
    if (!frame->document()->isPluginDocument())
        return 0;

    PluginDocument* pluginDocument = static_cast<PluginDocument*>(frame->document());
    PluginView* pluginView = static_cast<PluginView*>(pluginDocument->pluginWidget());
    return pluginView;
}

void WebPage::executeEditingCommand(const String& commandName, const String& argument)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (PluginView* pluginView = focusedPluginViewForFrame(frame)) {
        pluginView->handleEditingCommand(commandName, argument);
        return;
    }
    
    frame.editor().command(commandName).execute(argument);
}

void WebPage::setEditable(bool editable)
{
    m_page->setEditable(editable);
    m_page->setTabKeyCyclesThroughElements(!editable);
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (editable) {
        frame.editor().applyEditingStyleToBodyElement();
        // If the page is made editable and the selection is empty, set it to something.
        if (frame.selection().isNone())
            frame.selection().setSelectionFromNone();
    }
}

bool WebPage::isEditingCommandEnabled(const String& commandName)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (PluginView* pluginView = focusedPluginViewForFrame(frame))
        return pluginView->isEditingCommandEnabled(commandName);
    
    Editor::Command command = frame.editor().command(commandName);
    return command.isSupported() && command.isEnabled();
}
    
void WebPage::clearMainFrameName()
{
    if (Frame* frame = mainFrame())
        frame->tree().clearName();
}

void WebPage::enterAcceleratedCompositingMode(GraphicsLayer* layer)
{
    m_drawingArea->setRootCompositingLayer(layer);
}

void WebPage::exitAcceleratedCompositingMode()
{
    m_drawingArea->setRootCompositingLayer(0);
}

void WebPage::close()
{
    if (m_isClosed)
        return;

    m_isClosed = true;

    // If there is still no URL, then we never loaded anything in this page, so nothing to report.
    if (!mainWebFrame()->url().isEmpty())
        reportUsedFeatures();

    if (pageGroup()->isVisibleToInjectedBundle() && WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->willDestroyPage(this);

    if (m_inspector) {
        m_inspector->disconnectFromPage();
        m_inspector = nullptr;
    }

#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = nullptr;
#endif

    if (m_activePopupMenu) {
        m_activePopupMenu->disconnectFromPage();
        m_activePopupMenu = nullptr;
    }

    if (m_activeOpenPanelResultListener) {
        m_activeOpenPanelResultListener->disconnectFromPage();
        m_activeOpenPanelResultListener = nullptr;
    }

#if ENABLE(INPUT_TYPE_COLOR)
    if (m_activeColorChooser) {
        m_activeColorChooser->disconnectFromPage();
        m_activeColorChooser = nullptr;
    }
#endif

#if PLATFORM(GTK)
    if (m_printOperation) {
        m_printOperation->disconnectFromPage();
        m_printOperation = nullptr;
    }
#endif

#if ENABLE(VIDEO) && USE(GSTREAMER)
    if (m_installMediaPluginsCallback) {
        m_installMediaPluginsCallback->invalidate();
        m_installMediaPluginsCallback = nullptr;
    }
#endif

    m_sandboxExtensionTracker.invalidate();

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    m_determinePrimarySnapshottedPlugInTimer.stop();
#endif

#if ENABLE(CONTEXT_MENUS)
    m_contextMenuClient = std::make_unique<API::InjectedBundle::PageContextMenuClient>();
#endif
    m_editorClient.initialize(0);
    m_formClient = std::make_unique<API::InjectedBundle::FormClient>();
    m_loaderClient.initialize(0);
    m_policyClient.initialize(0);
    m_resourceLoadClient.initialize(0);
    m_uiClient = std::make_unique<API::InjectedBundle::PageUIClient>();
#if ENABLE(FULLSCREEN_API)
    m_fullScreenClient.initialize(0);
#endif
    m_logDiagnosticMessageClient.initialize(0);

    m_printContext = nullptr;
    m_mainFrame->coreFrame()->loader().detachFromParent();
    m_page = nullptr;
    m_drawingArea = nullptr;

    bool isRunningModal = m_isRunningModal;
    m_isRunningModal = false;

    // The WebPage can be destroyed by this call.
    WebProcess::singleton().removeWebPage(m_pageID);

    WebProcess::singleton().updateActivePages();

    if (isRunningModal)
        RunLoop::main().stop();
}

void WebPage::tryClose()
{
    SendStopResponsivenessTimer stopper(this);

    if (!corePage()->userInputBridge().tryClosePage()) {
        send(Messages::WebPageProxy::StopResponsivenessTimer());
        return;
    }

    send(Messages::WebPageProxy::ClosePage(true));
}

void WebPage::sendClose()
{
    send(Messages::WebPageProxy::ClosePage(false));
}

void WebPage::loadURLInFrame(const String& url, uint64_t frameID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    frame->coreFrame()->loader().load(FrameLoadRequest(frame->coreFrame(), ResourceRequest(URL(URL(), url)), ShouldOpenExternalURLsPolicy::ShouldNotAllow));
}

void WebPage::loadRequest(uint64_t navigationID, const ResourceRequest& request, const SandboxExtension::Handle& sandboxExtensionHandle, uint64_t shouldOpenExternalURLsPolicy, const UserData& userData)
{
    SendStopResponsivenessTimer stopper(this);

    m_pendingNavigationID = navigationID;

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient.willLoadURLRequest(this, request, WebProcess::singleton().transformHandlesToObjects(userData.object()).get());

    // Initate the load in WebCore.
    FrameLoadRequest frameLoadRequest(m_mainFrame->coreFrame(), request, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
    ShouldOpenExternalURLsPolicy externalURLsPolicy = static_cast<ShouldOpenExternalURLsPolicy>(shouldOpenExternalURLsPolicy);
    frameLoadRequest.setShouldOpenExternalURLsPolicy(externalURLsPolicy);

    corePage()->userInputBridge().loadRequest(frameLoadRequest);

    ASSERT(!m_pendingNavigationID);
}

void WebPage::loadDataImpl(uint64_t navigationID, PassRefPtr<SharedBuffer> sharedBuffer, const String& MIMEType, const String& encodingName, const URL& baseURL, const URL& unreachableURL, const UserData& userData)
{
    SendStopResponsivenessTimer stopper(this);

    m_pendingNavigationID = navigationID;

    ResourceRequest request(baseURL);
    ResourceResponse response(URL(), MIMEType, sharedBuffer->size(), encodingName);
    SubstituteData substituteData(sharedBuffer, unreachableURL, response, SubstituteData::SessionHistoryVisibility::Hidden);

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient.willLoadDataRequest(this, request, const_cast<SharedBuffer*>(substituteData.content()), substituteData.mimeType(), substituteData.textEncoding(), substituteData.failingURL(), WebProcess::singleton().transformHandlesToObjects(userData.object()).get());

    // Initate the load in WebCore.
    m_mainFrame->coreFrame()->loader().load(FrameLoadRequest(m_mainFrame->coreFrame(), request, ShouldOpenExternalURLsPolicy::ShouldNotAllow, substituteData));
}

void WebPage::loadString(uint64_t navigationID, const String& htmlString, const String& MIMEType, const URL& baseURL, const URL& unreachableURL, const UserData& userData)
{
    if (!htmlString.isNull() && htmlString.is8Bit()) {
        RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters8()), htmlString.length() * sizeof(LChar));
        loadDataImpl(navigationID, sharedBuffer, MIMEType, ASCIILiteral("latin1"), baseURL, unreachableURL, userData);
    } else {
        RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters16()), htmlString.length() * sizeof(UChar));
        loadDataImpl(navigationID, sharedBuffer, MIMEType, ASCIILiteral("utf-16"), baseURL, unreachableURL, userData);
    }
}

void WebPage::loadData(uint64_t navigationID, const IPC::DataReference& data, const String& MIMEType, const String& encodingName, const String& baseURLString, const UserData& userData)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(data.data()), data.size());
    URL baseURL = baseURLString.isEmpty() ? blankURL() : URL(URL(), baseURLString);
    loadDataImpl(navigationID, sharedBuffer, MIMEType, encodingName, baseURL, URL(), userData);
}

void WebPage::loadHTMLString(uint64_t navigationID, const String& htmlString, const String& baseURLString, const UserData& userData)
{
    URL baseURL = baseURLString.isEmpty() ? blankURL() : URL(URL(), baseURLString);
    loadString(navigationID, htmlString, ASCIILiteral("text/html"), baseURL, URL(), userData);
}

void WebPage::loadAlternateHTMLString(const String& htmlString, const String& baseURLString, const String& unreachableURLString, const String& provisionalLoadErrorURLString, const UserData& userData)
{
    URL baseURL = baseURLString.isEmpty() ? blankURL() : URL(URL(), baseURLString);
    URL unreachableURL = unreachableURLString.isEmpty() ? URL() : URL(URL(), unreachableURLString);
    URL provisionalLoadErrorURL = provisionalLoadErrorURLString.isEmpty() ? URL() : URL(URL(), provisionalLoadErrorURLString);
    m_mainFrame->coreFrame()->loader().setProvisionalLoadErrorBeingHandledURL(provisionalLoadErrorURL);
    loadString(0, htmlString, ASCIILiteral("text/html"), baseURL, unreachableURL, userData);
    m_mainFrame->coreFrame()->loader().setProvisionalLoadErrorBeingHandledURL({ });
}

void WebPage::loadPlainTextString(const String& string, const UserData& userData)
{
    loadString(0, string, ASCIILiteral("text/plain"), blankURL(), URL(), userData);
}

void WebPage::loadWebArchiveData(const IPC::DataReference& webArchiveData, const UserData& userData)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(webArchiveData.data()), webArchiveData.size() * sizeof(uint8_t));
    loadDataImpl(0, sharedBuffer, ASCIILiteral("application/x-webarchive"), ASCIILiteral("utf-16"), blankURL(), URL(), userData);
}

void WebPage::navigateToPDFLinkWithSimulatedClick(const String& url, IntPoint documentPoint, IntPoint screenPoint)
{
    Frame* mainFrame = m_mainFrame->coreFrame();
    Document* mainFrameDocument = mainFrame->document();
    if (!mainFrameDocument)
        return;

    const int singleClick = 1;
    RefPtr<MouseEvent> mouseEvent = MouseEvent::create(eventNames().clickEvent, true, true, currentTime(), nullptr, singleClick, screenPoint.x(), screenPoint.y(), documentPoint.x(), documentPoint.y(),
#if ENABLE(POINTER_LOCK)
        0, 0,
#endif
        false, false, false, false, 0, nullptr, 0, nullptr);

    mainFrame->loader().urlSelected(mainFrameDocument->completeURL(url), emptyString(), mouseEvent.get(), LockHistory::No, LockBackForwardList::No, ShouldSendReferrer::MaybeSendReferrer, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
}

void WebPage::stopLoadingFrame(uint64_t frameID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    corePage()->userInputBridge().stopLoadingFrame(frame->coreFrame());
}

void WebPage::stopLoading()
{
    SendStopResponsivenessTimer stopper(this);

    corePage()->userInputBridge().stopLoadingFrame(m_mainFrame->coreFrame());
}

bool WebPage::defersLoading() const
{
    return m_page->defersLoading();
}

void WebPage::setDefersLoading(bool defersLoading)
{
    m_page->setDefersLoading(defersLoading);
}

void WebPage::reload(uint64_t navigationID, bool reloadFromOrigin, bool contentBlockersEnabled, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    ASSERT(!m_mainFrame->coreFrame()->loader().frameHasLoaded() || !m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    corePage()->userInputBridge().reloadFrame(m_mainFrame->coreFrame(), reloadFromOrigin, contentBlockersEnabled);
}

void WebPage::goForward(uint64_t navigationID, uint64_t backForwardItemID)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    ASSERT(!m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    m_page->goToItem(*item, FrameLoadType::Forward);
}

void WebPage::goBack(uint64_t navigationID, uint64_t backForwardItemID)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    ASSERT(!m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    m_page->goToItem(*item, FrameLoadType::Back);
}

void WebPage::goToBackForwardItem(uint64_t navigationID, uint64_t backForwardItemID)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    ASSERT(!m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    m_page->goToItem(*item, FrameLoadType::IndexedBackForward);
}

void WebPage::tryRestoreScrollPosition()
{
    m_page->mainFrame().loader().history().restoreScrollPositionAndViewState();
}

void WebPage::layoutIfNeeded()
{
    if (m_mainFrame->coreFrame()->view())
        m_mainFrame->coreFrame()->view()->updateLayoutAndStyleIfNeededRecursive();
}

WebPage* WebPage::fromCorePage(Page* page)
{
    return static_cast<WebChromeClient&>(page->chrome().client()).page();
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    if (m_viewSize == viewSize)
        return;

    FrameView* view = m_page->mainFrame().view();
    view->resize(viewSize);
    m_drawingArea->setNeedsDisplay();
    
    m_viewSize = viewSize;

#if USE(COORDINATED_GRAPHICS)
    if (view->useFixedLayout())
        sendViewportAttributesChanged();
#endif
}

#if USE(COORDINATED_GRAPHICS)
void WebPage::setFixedVisibleContentRect(const IntRect& rect)
{
    ASSERT(m_useFixedLayout);

    m_page->mainFrame().view()->setFixedVisibleContentRect(rect);
}

void WebPage::sendViewportAttributesChanged()
{
    ASSERT(m_useFixedLayout);

    // Viewport properties have no impact on zero sized fixed viewports.
    if (m_viewSize.isEmpty())
        return;

    // Recalculate the recommended layout size, when the available size (device pixel) changes.
    Settings& settings = m_page->settings();

    int minimumLayoutFallbackWidth = std::max(settings.layoutFallbackWidth(), m_viewSize.width());

    // If unset  we use the viewport dimensions. This fits with the behavior of desktop browsers.
    int deviceWidth = (settings.deviceWidth() > 0) ? settings.deviceWidth() : m_viewSize.width();
    int deviceHeight = (settings.deviceHeight() > 0) ? settings.deviceHeight() : m_viewSize.height();

    ViewportAttributes attr = computeViewportAttributes(m_page->viewportArguments(), minimumLayoutFallbackWidth, deviceWidth, deviceHeight, 1, m_viewSize);

    FrameView* view = m_page->mainFrame().view();

    // If no layout was done yet set contentFixedOrigin to (0,0).
    IntPoint contentFixedOrigin = view->didFirstLayout() ? view->fixedVisibleContentRect().location() : IntPoint();

    // Put the width and height to the viewport width and height. In css units however.
    // Use FloatSize to avoid truncated values during scale.
    FloatSize contentFixedSize = m_viewSize;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    // CSS viewport descriptors might be applied to already affected viewport size
    // if the page enables/disables stylesheets, so need to keep initial viewport size.
    view->setInitialViewportSize(roundedIntSize(contentFixedSize));
#endif

    contentFixedSize.scale(1 / attr.initialScale);
    setFixedVisibleContentRect(IntRect(contentFixedOrigin, roundedIntSize(contentFixedSize)));

    attr.initialScale = m_page->viewportArguments().zoom; // Resets auto (-1) if no value was set by user.

    // This also takes care of the relayout.
    setFixedLayoutSize(roundedIntSize(attr.layoutSize));

#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_drawingArea->layerTreeHost())
        m_drawingArea->layerTreeHost()->didChangeViewportProperties(attr);
#else
    send(Messages::WebPageProxy::DidChangeViewportProperties(attr));
#endif
}
#endif

void WebPage::scrollMainFrameIfNotAtMaxScrollPosition(const IntSize& scrollOffset)
{
    FrameView* frameView = m_page->mainFrame().view();

    ScrollPosition scrollPosition = frameView->scrollPosition();
    ScrollPosition maximumScrollPosition = frameView->maximumScrollPosition();

    // If the current scroll position in a direction is the max scroll position 
    // we don't want to scroll at all.
    IntSize newScrollOffset;
    if (scrollPosition.x() < maximumScrollPosition.x())
        newScrollOffset.setWidth(scrollOffset.width());
    if (scrollPosition.y() < maximumScrollPosition.y())
        newScrollOffset.setHeight(scrollOffset.height());

    if (newScrollOffset.isZero())
        return;

    frameView->setScrollPosition(frameView->scrollPosition() + newScrollOffset);
}

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);

    m_mainFrame->coreFrame()->view()->paint(graphicsContext, rect);
}

double WebPage::textZoomFactor() const
{
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->requiresUnifiedScaleFactor()) {
        if (pluginView->handlesPageScaleFactor())
            return pluginView->pageScaleFactor();
        return pageScaleFactor();
    }

    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->textZoomFactor();
}

void WebPage::setTextZoomFactor(double zoomFactor)
{
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->requiresUnifiedScaleFactor()) {
        if (pluginView->handlesPageScaleFactor())
            pluginView->setPageScaleFactor(zoomFactor, IntPoint());
        else
            scalePage(zoomFactor, IntPoint());
        return;
    }

    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setTextZoomFactor(static_cast<float>(zoomFactor));
}

double WebPage::pageZoomFactor() const
{
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->requiresUnifiedScaleFactor()) {
        if (pluginView->handlesPageScaleFactor())
            return pluginView->pageScaleFactor();
        return pageScaleFactor();
    }

    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void WebPage::setPageZoomFactor(double zoomFactor)
{
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->requiresUnifiedScaleFactor()) {
        if (pluginView->handlesPageScaleFactor())
            pluginView->setPageScaleFactor(zoomFactor, IntPoint());
        else
            scalePage(zoomFactor, IntPoint());
        return;
    }

    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setPageZoomFactor(static_cast<float>(zoomFactor));
}

void WebPage::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->requiresUnifiedScaleFactor()) {
        if (pluginView->handlesPageScaleFactor())
            pluginView->setPageScaleFactor(pageZoomFactor, IntPoint());
        else
            scalePage(pageZoomFactor, IntPoint());
        return;
    }

    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    return frame->setPageAndTextZoomFactors(static_cast<float>(pageZoomFactor), static_cast<float>(textZoomFactor));
}

void WebPage::windowScreenDidChange(uint32_t displayID)
{
    m_page->chrome().windowScreenDidChange(static_cast<PlatformDisplayID>(displayID));
}

void WebPage::scalePage(double scale, const IntPoint& origin)
{
    double totalScale = scale * viewScaleFactor();
    bool willChangeScaleFactor = totalScale != totalScaleFactor();

#if PLATFORM(IOS)
    if (willChangeScaleFactor) {
        if (!m_inDynamicSizeUpdate)
            m_dynamicSizeUpdateHistory.clear();
        m_scaleWasSetByUIProcess = false;
    }
#endif
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->handlesPageScaleFactor()) {
        // If the main-frame plugin wants to handle the page scale factor, make sure to reset WebCore's page scale.
        // Otherwise, we can end up with an immutable but non-1 page scale applied by WebCore on top of whatever the plugin does.
        if (m_page->pageScaleFactor() != 1) {
            m_page->setPageScaleFactor(1, origin);
            for (auto* pluginView : m_pluginViews)
                pluginView->pageScaleFactorDidChange();
        }

        pluginView->setPageScaleFactor(totalScale, origin);
        return;
    }

    m_page->setPageScaleFactor(totalScale, origin);

    // We can't early return before setPageScaleFactor because the origin might be different.
    if (!willChangeScaleFactor)
        return;

    for (auto* pluginView : m_pluginViews)
        pluginView->pageScaleFactorDidChange();

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    if (m_drawingArea->layerTreeHost())
        m_drawingArea->layerTreeHost()->deviceOrPageScaleFactorChanged();
#endif

    send(Messages::WebPageProxy::PageScaleFactorDidChange(scale));
}

void WebPage::scalePageInViewCoordinates(double scale, IntPoint centerInViewCoordinates)
{
    double totalScale = scale * viewScaleFactor();
    if (totalScale == totalScaleFactor())
        return;

    IntPoint scrollPositionAtNewScale = mainFrameView()->rootViewToContents(-centerInViewCoordinates);
    double scaleRatio = scale / pageScaleFactor();
    scrollPositionAtNewScale.scale(scaleRatio, scaleRatio);
    scalePage(scale, scrollPositionAtNewScale);
}

double WebPage::totalScaleFactor() const
{
    PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame());
    if (pluginView && pluginView->handlesPageScaleFactor())
        return pluginView->pageScaleFactor();

    return m_page->pageScaleFactor();
}

double WebPage::pageScaleFactor() const
{
    return totalScaleFactor() / viewScaleFactor();
}

double WebPage::viewScaleFactor() const
{
    return m_page->viewScaleFactor();
}

void WebPage::scaleView(double scale)
{
    if (viewScaleFactor() == scale)
        return;

    float pageScale = pageScaleFactor();

    IntPoint scrollPositionAtNewScale;
    if (FrameView* mainFrameView = m_page->mainFrame().view()) {
        double scaleRatio = scale / viewScaleFactor();
        scrollPositionAtNewScale = mainFrameView->scrollPosition();
        scrollPositionAtNewScale.scale(scaleRatio, scaleRatio);
    }

    m_page->setViewScaleFactor(scale);
    scalePage(pageScale, scrollPositionAtNewScale);
}

void WebPage::setDeviceScaleFactor(float scaleFactor)
{
    if (scaleFactor == m_page->deviceScaleFactor())
        return;

    m_page->setDeviceScaleFactor(scaleFactor);

    // Tell all our plug-in views that the device scale factor changed.
#if PLATFORM(MAC)
    for (auto* pluginView : m_pluginViews)
        pluginView->setDeviceScaleFactor(scaleFactor);

    updateHeaderAndFooterLayersForDeviceScaleChange(scaleFactor);
#endif

    if (m_findController.isShowingOverlay()) {
        // We must have updated layout to get the selection rects right.
        layoutIfNeeded();
        m_findController.deviceScaleFactorDidChange();
    }

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    if (m_drawingArea->layerTreeHost())
        m_drawingArea->layerTreeHost()->deviceOrPageScaleFactorChanged();
#endif
}

float WebPage::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

void WebPage::setUseFixedLayout(bool fixed)
{
    // Do not overwrite current settings if initially setting it to false.
    if (m_useFixedLayout == fixed)
        return;
    m_useFixedLayout = fixed;

#if !PLATFORM(IOS)
    m_page->settings().setFixedElementsLayoutRelativeToFrame(fixed);
#endif
#if USE(COORDINATED_GRAPHICS)
    m_page->settings().setAcceleratedCompositingForFixedPositionEnabled(fixed);
    m_page->settings().setFixedPositionCreatesStackingContext(fixed);
    m_page->settings().setDelegatesPageScaling(fixed);
    m_page->settings().setScrollingCoordinatorEnabled(fixed);
#endif

#if USE(COORDINATED_GRAPHICS) && ENABLE(SMOOTH_SCROLLING)
    // Delegated scrolling will be enabled when the FrameView is created if fixed layout is enabled.
    // Ensure we don't do animated scrolling in the WebProcess in that case.
    m_page->settings().setScrollAnimatorEnabled(!fixed);
#endif

    FrameView* view = mainFrameView();
    if (!view)
        return;

#if USE(COORDINATED_GRAPHICS)
    view->setDelegatesScrolling(fixed);
    view->setPaintsEntireContents(fixed);
#endif
    view->setUseFixedLayout(fixed);
    if (!fixed)
        setFixedLayoutSize(IntSize());

    send(Messages::WebPageProxy::UseFixedLayoutDidChange(fixed));
}

void WebPage::setFixedLayoutSize(const IntSize& size)
{
    FrameView* view = mainFrameView();
    if (!view || view->fixedLayoutSize() == size)
        return;

    view->setFixedLayoutSize(size);

    send(Messages::WebPageProxy::FixedLayoutSizeDidChange(size));
}

IntSize WebPage::fixedLayoutSize() const
{
    FrameView* view = mainFrameView();
    if (!view)
        return IntSize();
    return view->fixedLayoutSize();
}

void WebPage::listenForLayoutMilestones(uint32_t milestones)
{
    if (!m_page)
        return;
    m_page->addLayoutMilestones(static_cast<LayoutMilestones>(milestones));
}

void WebPage::setSuppressScrollbarAnimations(bool suppressAnimations)
{
    m_page->setShouldSuppressScrollbarAnimations(suppressAnimations);
}
    
void WebPage::setEnableVerticalRubberBanding(bool enableVerticalRubberBanding)
{
    m_page->setVerticalScrollElasticity(enableVerticalRubberBanding ? ScrollElasticityAllowed : ScrollElasticityNone);
}
    
void WebPage::setEnableHorizontalRubberBanding(bool enableHorizontalRubberBanding)
{
    m_page->setHorizontalScrollElasticity(enableHorizontalRubberBanding ? ScrollElasticityAllowed : ScrollElasticityNone);
}

void WebPage::setBackgroundExtendsBeyondPage(bool backgroundExtendsBeyondPage)
{
    if (m_page->settings().backgroundShouldExtendBeyondPage() != backgroundExtendsBeyondPage)
        m_page->settings().setBackgroundShouldExtendBeyondPage(backgroundExtendsBeyondPage);
}

void WebPage::setPaginationMode(uint32_t mode)
{
    Pagination pagination = m_page->pagination();
    pagination.mode = static_cast<Pagination::Mode>(mode);
    m_page->setPagination(pagination);
}

void WebPage::setPaginationBehavesLikeColumns(bool behavesLikeColumns)
{
    Pagination pagination = m_page->pagination();
    pagination.behavesLikeColumns = behavesLikeColumns;
    m_page->setPagination(pagination);
}

void WebPage::setPageLength(double pageLength)
{
    Pagination pagination = m_page->pagination();
    pagination.pageLength = pageLength;
    m_page->setPagination(pagination);
}

void WebPage::setGapBetweenPages(double gap)
{
    Pagination pagination = m_page->pagination();
    pagination.gap = gap;
    m_page->setPagination(pagination);
}

void WebPage::setPaginationLineGridEnabled(bool lineGridEnabled)
{
    m_page->setPaginationLineGridEnabled(lineGridEnabled);
}

void WebPage::postInjectedBundleMessage(const String& messageName, const UserData& userData)
{
    auto& webProcess = WebProcess::singleton();
    InjectedBundle* injectedBundle = webProcess.injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->didReceiveMessageToPage(this, messageName, webProcess.transformHandlesToObjects(userData.object()).get());
}

#if !PLATFORM(IOS)
void WebPage::setHeaderPageBanner(PassRefPtr<PageBanner> pageBanner)
{
    if (m_headerBanner)
        m_headerBanner->detachFromPage();

    m_headerBanner = pageBanner;

    if (m_headerBanner)
        m_headerBanner->addToPage(PageBanner::Header, this);
}

PageBanner* WebPage::headerPageBanner()
{
    return m_headerBanner.get();
}

void WebPage::setFooterPageBanner(PassRefPtr<PageBanner> pageBanner)
{
    if (m_footerBanner)
        m_footerBanner->detachFromPage();

    m_footerBanner = pageBanner;

    if (m_footerBanner)
        m_footerBanner->addToPage(PageBanner::Footer, this);
}

PageBanner* WebPage::footerPageBanner()
{
    return m_footerBanner.get();
}

void WebPage::hidePageBanners()
{
    if (m_headerBanner)
        m_headerBanner->hide();
    if (m_footerBanner)
        m_footerBanner->hide();
}

void WebPage::showPageBanners()
{
    if (m_headerBanner)
        m_headerBanner->showIfHidden();
    if (m_footerBanner)
        m_footerBanner->showIfHidden();
}
#endif // !PLATFORM(IOS)

void WebPage::takeSnapshot(IntRect snapshotRect, IntSize bitmapSize, uint32_t options, uint64_t callbackID)
{
    SnapshotOptions snapshotOptions = static_cast<SnapshotOptions>(options);
    snapshotOptions |= SnapshotOptionsShareable;

    RefPtr<WebImage> image = snapshotAtSize(snapshotRect, bitmapSize, snapshotOptions);

    ShareableBitmap::Handle handle;
    if (image)
        image->bitmap()->createHandle(handle, SharedMemory::Protection::ReadOnly);

    send(Messages::WebPageProxy::ImageCallback(handle, callbackID));
}

PassRefPtr<WebImage> WebPage::scaledSnapshotWithOptions(const IntRect& rect, double additionalScaleFactor, SnapshotOptions options)
{
    IntRect snapshotRect = rect;
    IntSize bitmapSize = snapshotRect.size();
    if (options & SnapshotOptionsPrinting) {
        ASSERT(additionalScaleFactor == 1);
        Frame* coreFrame = m_mainFrame->coreFrame();
        if (!coreFrame)
            return nullptr;
        bitmapSize.setHeight(PrintContext::numberOfPages(*coreFrame, bitmapSize) * (bitmapSize.height() + 1) - 1);
    } else {
        double scaleFactor = additionalScaleFactor;
        if (!(options & SnapshotOptionsExcludeDeviceScaleFactor))
            scaleFactor *= corePage()->deviceScaleFactor();
        bitmapSize.scale(scaleFactor);
    }

    return snapshotAtSize(rect, bitmapSize, options);
}

PassRefPtr<WebImage> WebPage::snapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options)
{
    Frame* coreFrame = m_mainFrame->coreFrame();
    if (!coreFrame)
        return nullptr;

    FrameView* frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    IntRect snapshotRect = rect;
    float horizontalScaleFactor = static_cast<float>(bitmapSize.width()) / rect.width();
    float verticalScaleFactor = static_cast<float>(bitmapSize.height()) / rect.height();
    float scaleFactor = std::max(horizontalScaleFactor, verticalScaleFactor);

    RefPtr<WebImage> snapshot = WebImage::create(bitmapSize, snapshotOptionsToImageOptions(options));
    if (!snapshot->bitmap())
        return nullptr;

    auto graphicsContext = snapshot->bitmap()->createGraphicsContext();

    if (options & SnapshotOptionsPrinting) {
        PrintContext::spoolAllPagesWithBoundaries(*coreFrame, *graphicsContext, snapshotRect.size());
        return snapshot.release();
    }

    Color documentBackgroundColor = frameView->documentBackgroundColor();
    Color backgroundColor = (coreFrame->settings().backgroundShouldExtendBeyondPage() && documentBackgroundColor.isValid()) ? documentBackgroundColor : frameView->baseBackgroundColor();
    graphicsContext->fillRect(IntRect(IntPoint(), bitmapSize), backgroundColor);

    if (!(options & SnapshotOptionsExcludeDeviceScaleFactor)) {
        double deviceScaleFactor = corePage()->deviceScaleFactor();
        graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
        scaleFactor /= deviceScaleFactor;
    }

    graphicsContext->scale(FloatSize(scaleFactor, scaleFactor));
    graphicsContext->translate(-snapshotRect.x(), -snapshotRect.y());

    FrameView::SelectionInSnapshot shouldPaintSelection = FrameView::IncludeSelection;
    if (options & SnapshotOptionsExcludeSelectionHighlighting)
        shouldPaintSelection = FrameView::ExcludeSelection;

    FrameView::CoordinateSpaceForSnapshot coordinateSpace = FrameView::DocumentCoordinates;
    if (options & SnapshotOptionsInViewCoordinates)
        coordinateSpace = FrameView::ViewCoordinates;

    frameView->paintContentsForSnapshot(*graphicsContext, snapshotRect, shouldPaintSelection, coordinateSpace);

    if (options & SnapshotOptionsPaintSelectionRectangle) {
        FloatRect selectionRectangle = m_mainFrame->coreFrame()->selection().selectionBounds();
        graphicsContext->setStrokeColor(Color(0xFF, 0, 0));
        graphicsContext->strokeRect(selectionRectangle, 1);
    }
    
    return snapshot.release();
}

PassRefPtr<WebImage> WebPage::snapshotNode(WebCore::Node& node, SnapshotOptions options, unsigned maximumPixelCount)
{
    Frame* coreFrame = m_mainFrame->coreFrame();
    if (!coreFrame)
        return nullptr;

    FrameView* frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    if (!node.renderer())
        return nullptr;

    LayoutRect topLevelRect;
    IntRect snapshotRect = snappedIntRect(node.renderer()->paintingRootRect(topLevelRect));
    if (snapshotRect.isEmpty())
        return nullptr;

    double scaleFactor = 1;
    IntSize snapshotSize = snapshotRect.size();
    unsigned maximumHeight = maximumPixelCount / snapshotSize.width();
    if (maximumHeight < static_cast<unsigned>(snapshotSize.height())) {
        scaleFactor = static_cast<double>(maximumHeight) / snapshotSize.height();
        snapshotSize = IntSize(snapshotSize.width() * scaleFactor, maximumHeight);
    }

    RefPtr<WebImage> snapshot = WebImage::create(snapshotSize, snapshotOptionsToImageOptions(options));
    if (!snapshot->bitmap())
        return nullptr;

    auto graphicsContext = snapshot->bitmap()->createGraphicsContext();

    if (!(options & SnapshotOptionsExcludeDeviceScaleFactor)) {
        double deviceScaleFactor = corePage()->deviceScaleFactor();
        graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
        scaleFactor /= deviceScaleFactor;
    }

    graphicsContext->scale(FloatSize(scaleFactor, scaleFactor));
    graphicsContext->translate(-snapshotRect.x(), -snapshotRect.y());

    Color savedBackgroundColor = frameView->baseBackgroundColor();
    frameView->setBaseBackgroundColor(Color::transparent);
    frameView->setNodeToDraw(&node);

    frameView->paintContentsForSnapshot(*graphicsContext, snapshotRect, FrameView::ExcludeSelection, FrameView::DocumentCoordinates);

    frameView->setBaseBackgroundColor(savedBackgroundColor);
    frameView->setNodeToDraw(nullptr);

    return snapshot.release();
}

void WebPage::pageDidScroll()
{
#if PLATFORM(IOS)
    if (!m_inDynamicSizeUpdate)
        m_dynamicSizeUpdateHistory.clear();
#endif
    m_uiClient->pageDidScroll(this);

    m_pageScrolledHysteresis.impulse();

    send(Messages::WebPageProxy::PageDidScroll());
}

void WebPage::pageStoppedScrolling()
{
    // Maintain the current history item's scroll position up-to-date.
    if (Frame* frame = m_mainFrame->coreFrame())
        frame->loader().history().saveScrollPositionAndViewStateToItem(frame->loader().history().currentItem());
}

#if USE(COORDINATED_GRAPHICS)
void WebPage::pageDidRequestScroll(const IntPoint& point)
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    drawingArea()->scroll(IntRect(point, IntSize()), IntSize());
#elif USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    send(Messages::WebPageProxy::PageDidRequestScroll(point));
#endif
}
#endif

#if ENABLE(CONTEXT_MENUS)
WebContextMenu* WebPage::contextMenu()
{
    if (!m_contextMenu)
        m_contextMenu = WebContextMenu::create(this);
    return m_contextMenu.get();
}

WebContextMenu* WebPage::contextMenuAtPointInWindow(const IntPoint& point)
{
    corePage()->contextMenuController().clearContextMenu();
    
    // Simulate a mouse click to generate the correct menu.
    PlatformMouseEvent mouseEvent(point, point, RightButton, PlatformEvent::MousePressed, 1, false, false, false, false, currentTime(), WebCore::ForceAtClick);
    bool handled = corePage()->userInputBridge().handleContextMenuEvent(mouseEvent, &corePage()->mainFrame());
    if (!handled)
        return 0;

    return contextMenu();
}
#endif

// Events 

static const WebEvent* g_currentEvent = 0;

// FIXME: WebPage::currentEvent is used by the plug-in code to avoid having to convert from DOM events back to
// WebEvents. When we get the event handling sorted out, this should go away and the Widgets should get the correct
// platform events passed to the event handler code.
const WebEvent* WebPage::currentEvent()
{
    return g_currentEvent;
}

void WebPage::setLayerTreeStateIsFrozen(bool frozen)
{
    auto* drawingArea = this->drawingArea();
    if (!drawingArea)
        return;

    drawingArea->setLayerTreeStateIsFrozen(frozen);
}

bool WebPage::markLayersVolatileImmediatelyIfPossible()
{
    auto* drawingArea = this->drawingArea();
    if (!drawingArea)
        return true;

    return drawingArea->markLayersVolatileImmediatelyIfPossible();
}

class CurrentEvent {
public:
    explicit CurrentEvent(const WebEvent& event)
        : m_previousCurrentEvent(g_currentEvent)
    {
        g_currentEvent = &event;
    }

    ~CurrentEvent()
    {
        g_currentEvent = m_previousCurrentEvent;
    }

private:
    const WebEvent* m_previousCurrentEvent;
};

#if ENABLE(CONTEXT_MENUS)
static bool isContextClick(const PlatformMouseEvent& event)
{
#if PLATFORM(COCOA)
    return WebEventFactory::shouldBeHandledAsContextClick(event);
#else
    return event.button() == WebCore::RightButton;
#endif
}

static bool handleContextMenuEvent(const PlatformMouseEvent& platformMouseEvent, WebPage* page)
{
    IntPoint point = page->corePage()->mainFrame().view()->windowToContents(platformMouseEvent.position());
    HitTestResult result = page->corePage()->mainFrame().eventHandler().hitTestResultAtPoint(point);

    Frame* frame = &page->corePage()->mainFrame();
    if (result.innerNonSharedNode())
        frame = result.innerNonSharedNode()->document().frame();

    bool handled = page->corePage()->userInputBridge().handleContextMenuEvent(platformMouseEvent, frame);
    if (handled)
        page->contextMenu()->show();

    return handled;
}
#endif

static bool handleMouseEvent(const WebMouseEvent& mouseEvent, WebPage* page, bool onlyUpdateScrollbars)
{
    Frame& frame = page->corePage()->mainFrame();
    if (!frame.view())
        return false;

    PlatformMouseEvent platformMouseEvent = platform(mouseEvent);

    switch (platformMouseEvent.type()) {
        case PlatformEvent::MousePressed: {
#if ENABLE(CONTEXT_MENUS)
            if (isContextClick(platformMouseEvent))
                page->corePage()->contextMenuController().clearContextMenu();
#endif

            bool handled = page->corePage()->userInputBridge().handleMousePressEvent(platformMouseEvent);
#if ENABLE(CONTEXT_MENUS)
            if (isContextClick(platformMouseEvent))
                handled = handleContextMenuEvent(platformMouseEvent, page);
#endif
            return handled;
        }
        case PlatformEvent::MouseReleased:
            return page->corePage()->userInputBridge().handleMouseReleaseEvent(platformMouseEvent);

        case PlatformEvent::MouseMoved:
            if (onlyUpdateScrollbars)
                return page->corePage()->userInputBridge().handleMouseMoveOnScrollbarEvent(platformMouseEvent);
            return page->corePage()->userInputBridge().handleMouseMoveEvent(platformMouseEvent);

        case PlatformEvent::MouseForceChanged:
        case PlatformEvent::MouseForceDown:
        case PlatformEvent::MouseForceUp:
            return page->corePage()->userInputBridge().handleMouseForceEvent(platformMouseEvent);

        default:
            ASSERT_NOT_REACHED();
            return false;
    }
}

void WebPage::mouseEvent(const WebMouseEvent& mouseEvent)
{
    m_page->pageThrottler().didReceiveUserInput();

    bool shouldHandleEvent = true;

#if ENABLE(CONTEXT_MENUS)
    // Don't try to handle any pending mouse events if a context menu is showing.
    if (m_isShowingContextMenu)
        shouldHandleEvent = false;
#endif
#if ENABLE(DRAG_SUPPORT)
    if (m_isStartingDrag)
        shouldHandleEvent = false;
#endif

    if (!shouldHandleEvent) {
        send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), false));
        return;
    }

    bool handled = false;

#if !PLATFORM(IOS)
    if (!handled && m_headerBanner)
        handled = m_headerBanner->mouseEvent(mouseEvent);
    if (!handled && m_footerBanner)
        handled = m_footerBanner->mouseEvent(mouseEvent);
#endif // !PLATFORM(IOS)

    if (!handled && canHandleUserEvents()) {
        CurrentEvent currentEvent(mouseEvent);

        // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
        // button is currently pressed. It is possible that neither of those things will be true since on
        // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one
        // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
        // efficient scrollbars-only version of the event.
        bool onlyUpdateScrollbars = !(m_page->focusController().isActive() || (mouseEvent.button() != WebMouseEvent::NoButton));
        handled = handleMouseEvent(mouseEvent, this, onlyUpdateScrollbars);
    }

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), handled));
}

static bool handleWheelEvent(const WebWheelEvent& wheelEvent, Page* page)
{
    Frame& frame = page->mainFrame();
    if (!frame.view())
        return false;

    PlatformWheelEvent platformWheelEvent = platform(wheelEvent);
    return page->userInputBridge().handleWheelEvent(platformWheelEvent);
}

void WebPage::wheelEvent(const WebWheelEvent& wheelEvent)
{
    m_page->pageThrottler().didReceiveUserInput();

    bool handled = false;

    if (canHandleUserEvents()) {
        CurrentEvent currentEvent(wheelEvent);

        handled = handleWheelEvent(wheelEvent, m_page.get());
    }
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(wheelEvent.type()), handled));
}

static bool handleKeyEvent(const WebKeyboardEvent& keyboardEvent, Page* page)
{
    if (!page->mainFrame().view())
        return false;

    if (keyboardEvent.type() == WebEvent::Char && keyboardEvent.isSystemKey())
        return page->userInputBridge().handleAccessKeyEvent(platform(keyboardEvent));
    return page->userInputBridge().handleKeyEvent(platform(keyboardEvent));
}

void WebPage::keyEvent(const WebKeyboardEvent& keyboardEvent)
{
    m_page->pageThrottler().didReceiveUserInput();

    bool handled = false;

    if (canHandleUserEvents()) {
        CurrentEvent currentEvent(keyboardEvent);

        handled = handleKeyEvent(keyboardEvent, m_page.get());
        // FIXME: Platform default behaviors should be performed during normal DOM event dispatch (in most cases, in default keydown event handler).
        if (!handled)
            handled = performDefaultBehaviorForKeyEvent(keyboardEvent);
    }
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(keyboardEvent.type()), handled));
}

void WebPage::validateCommand(const String& commandName, uint64_t callbackID)
{
    bool isEnabled = false;
    int32_t state = 0;
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (PluginView* pluginView = focusedPluginViewForFrame(frame))
        isEnabled = pluginView->isEditingCommandEnabled(commandName);
    else {
        Editor::Command command = frame.editor().command(commandName);
        state = command.state();
        isEnabled = command.isSupported() && command.isEnabled();
    }

    send(Messages::WebPageProxy::ValidateCommandCallback(commandName, isEnabled, state, callbackID));
}

void WebPage::executeEditCommand(const String& commandName, const String& argument)
{
    executeEditingCommand(commandName, argument);
}

void WebPage::restoreSessionInternal(const Vector<BackForwardListItemState>& itemStates, WasRestoredByAPIRequest restoredByAPIRequest)
{
    for (const auto& itemState : itemStates) {
        auto historyItem = toHistoryItem(itemState.pageState);
        historyItem->setWasRestoredFromSession(restoredByAPIRequest == WasRestoredByAPIRequest::Yes);
        WebBackForwardListProxy::addItemFromUIProcess(itemState.identifier, WTFMove(historyItem), m_pageID);
    }
}

void WebPage::restoreSession(const Vector<BackForwardListItemState>& itemStates)
{
    restoreSessionInternal(itemStates, WasRestoredByAPIRequest::Yes);
}

#if ENABLE(TOUCH_EVENTS)
static bool handleTouchEvent(const WebTouchEvent& touchEvent, Page* page)
{
    if (!page->mainFrame().view())
        return false;

    return page->mainFrame().eventHandler().handleTouchEvent(platform(touchEvent));
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPage::dispatchTouchEvent(const WebTouchEvent& touchEvent, bool& handled)
{
    m_userIsInteracting = true;

    m_lastInteractionLocation = touchEvent.position();
    CurrentEvent currentEvent(touchEvent);
    handled = handleTouchEvent(touchEvent, m_page.get());

    m_userIsInteracting = false;
}

void WebPage::touchEventSync(const WebTouchEvent& touchEvent, bool& handled)
{
    EventDispatcher::TouchEventQueue queuedEvents;
    WebProcess::singleton().eventDispatcher().getQueuedTouchEventsForPage(*this, queuedEvents);
    dispatchAsynchronousTouchEvents(queuedEvents);

    dispatchTouchEvent(touchEvent, handled);
}
#elif ENABLE(TOUCH_EVENTS)
void WebPage::touchEvent(const WebTouchEvent& touchEvent)
{

    bool handled = false;
    if (canHandleUserEvents()) {
        CurrentEvent currentEvent(touchEvent);

        handled = handleTouchEvent(touchEvent, m_page.get());
    }
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(touchEvent.type()), handled));
}
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
static bool handleGestureEvent(const WebGestureEvent& event, Page* page)
{
    if (!page->mainFrame().view())
        return false;

    return page->mainFrame().eventHandler().handleGestureEvent(platform(event));
}

void WebPage::gestureEvent(const WebGestureEvent& gestureEvent)
{
    bool handled = false;
    if (canHandleUserEvents()) {
        CurrentEvent currentEvent(gestureEvent);
        handled = handleGestureEvent(gestureEvent, m_page.get());
    }
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(gestureEvent.type()), handled));
}

#endif

bool WebPage::scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    return page->userInputBridge().scrollRecursively(direction, granularity);
}

bool WebPage::logicalScroll(Page* page, ScrollLogicalDirection direction, ScrollGranularity granularity)
{
    return page->userInputBridge().logicalScrollRecursively(direction, granularity);
}

bool WebPage::scrollBy(uint32_t scrollDirection, uint32_t scrollGranularity)
{
    return scroll(m_page.get(), static_cast<ScrollDirection>(scrollDirection), static_cast<ScrollGranularity>(scrollGranularity));
}

void WebPage::centerSelectionInVisibleArea()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.selection().revealSelection(ScrollAlignment::alignCenterAlways);
    m_findController.showFindIndicatorInSelection();
}

#if ENABLE(REMOTE_INSPECTOR)
void WebPage::setAllowsRemoteInspection(bool allow)
{
    m_page->setRemoteInspectionAllowed(allow);
}

void WebPage::setRemoteInspectionNameOverride(const String& name)
{
    m_page->setRemoteInspectionNameOverride(name);
}
#endif

void WebPage::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree().traverseNext()) {
        if (FrameView* view = coreFrame->view())
            view->setTransparent(!drawsBackground);
    }

    m_drawingArea->pageBackgroundTransparencyChanged();
    m_drawingArea->setNeedsDisplay();
}

void WebPage::setTopContentInset(float contentInset)
{
    if (contentInset == m_page->topContentInset())
        return;

    m_page->setTopContentInset(contentInset);

    for (auto* pluginView : m_pluginViews)
        pluginView->topContentInsetDidChange();
}

void WebPage::viewWillStartLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (FrameView* view = frame.view())
        view->willStartLiveResize();
}

void WebPage::viewWillEndLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (FrameView* view = frame.view())
        view->willEndLiveResize();
}

void WebPage::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& event, uint64_t callbackID)
{
    if (!m_page)
        return;

#if PLATFORM(IOS)
    TemporaryChange<bool> userIsInteractingChange { m_userIsInteracting, true };
#endif

    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.document()->setFocusedElement(0);

    if (isKeyboardEventValid && event.type() == WebEvent::KeyDown) {
        PlatformKeyboardEvent platformEvent(platform(event));
        platformEvent.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown);
        m_page->focusController().setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, &KeyboardEvent::create(platformEvent, frame.document()->defaultView()).get());

        send(Messages::WebPageProxy::VoidCallback(callbackID));
        return;
    }

    m_page->focusController().setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, 0);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::setCanStartMediaTimerFired()
{
    if (m_page)
        m_page->setCanStartMedia(true);
}

inline bool WebPage::canHandleUserEvents() const
{
#if USE(COORDINATED_GRAPHICS)
    // Should apply only if the area was frozen by didStartPageTransition().
    return !m_drawingArea->layerTreeStateIsFrozen();
#endif
    return true;
}

void WebPage::updateIsInWindow(bool isInitialState)
{
    bool isInWindow = m_viewState & WebCore::ViewState::IsInWindow;

    if (!isInWindow) {
        m_setCanStartMediaTimer.stop();
        m_page->setCanStartMedia(false);
        
        // The WebProcess does not yet know about this page; no need to tell it we're leaving the window.
        if (!isInitialState)
            WebProcess::singleton().pageWillLeaveWindow(m_pageID);
    } else {
        // Defer the call to Page::setCanStartMedia() since it ends up sending a synchronous message to the UI process
        // in order to get plug-in connections, and the UI process will be waiting for the Web process to update the backing
        // store after moving the view into a window, until it times out and paints white. See <rdar://problem/9242771>.
        if (m_mayStartMediaWhenInWindow)
            m_setCanStartMediaTimer.startOneShot(0);

        WebProcess::singleton().pageDidEnterWindow(m_pageID);
    }

    if (isInWindow)
        layoutIfNeeded();
}

void WebPage::setViewState(ViewState::Flags viewState, bool wantsDidUpdateViewState, const Vector<uint64_t>& callbackIDs)
{
    ViewState::Flags changed = m_viewState ^ viewState;
    m_viewState = viewState;

    if (changed)
        updateUserActivity();

    m_page->setViewState(viewState);
    for (auto* pluginView : m_pluginViews)
        pluginView->viewStateDidChange(changed);

    m_drawingArea->viewStateDidChange(changed, wantsDidUpdateViewState, callbackIDs);

    if (changed & ViewState::IsInWindow)
        updateIsInWindow();
}

void WebPage::setLayerHostingMode(unsigned layerHostingMode)
{
    m_layerHostingMode = static_cast<LayerHostingMode>(layerHostingMode);

    m_drawingArea->setLayerHostingMode(m_layerHostingMode);

    for (auto* pluginView : m_pluginViews)
        pluginView->setLayerHostingMode(m_layerHostingMode);
}

void WebPage::setSessionID(SessionID sessionID)
{
    if (sessionID.isEphemeral())
        WebProcess::singleton().ensurePrivateBrowsingSession(sessionID);
    m_page->setSessionID(sessionID);
}

void WebPage::didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction, uint64_t navigationID, DownloadID downloadID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    frame->didReceivePolicyDecision(listenerID, static_cast<PolicyAction>(policyAction), navigationID, downloadID);
}

void WebPage::didStartPageTransition()
{
    m_drawingArea->setLayerTreeStateIsFrozen(true);
#if PLATFORM(IOS)
    m_hasFocusedDueToUserInteraction = false;
#endif
}

void WebPage::didCompletePageTransition()
{
#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    // m_mainFrame can be null since r170163.
    if (m_mainFrame && m_mainFrame->coreFrame()->view()->delegatesScrolling()) {
        // Wait until the UI process sent us the visible rect it wants rendered.
        send(Messages::WebPageProxy::PageTransitionViewportReady());
    } else
#endif
        
    m_drawingArea->setLayerTreeStateIsFrozen(false);
}

void WebPage::show()
{
    send(Messages::WebPageProxy::ShowPage());
}

String WebPage::userAgent(const URL& webCoreURL) const
{
    return userAgent(nullptr, webCoreURL);
}

String WebPage::userAgent(WebFrame* frame, const URL& webcoreURL) const
{
    if (frame && m_loaderClient.client().userAgentForURL) {
        API::String* apiString = m_loaderClient.userAgentForURL(frame, API::URL::create(webcoreURL).ptr());
        if (apiString)
            return apiString->string();
    }

    String userAgent = platformUserAgent(webcoreURL);
    if (!userAgent.isEmpty())
        return userAgent;
    return m_userAgent;
}
    
void WebPage::setUserAgent(const String& userAgent)
{
    m_userAgent = userAgent;
}

void WebPage::suspendActiveDOMObjectsAndAnimations()
{
    m_page->suspendActiveDOMObjectsAndAnimations();
}

void WebPage::resumeActiveDOMObjectsAndAnimations()
{
    m_page->resumeActiveDOMObjectsAndAnimations();
}

IntPoint WebPage::screenToRootView(const IntPoint& point)
{
    IntPoint windowPoint;
    sendSync(Messages::WebPageProxy::ScreenToRootView(point), Messages::WebPageProxy::ScreenToRootView::Reply(windowPoint));
    return windowPoint;
}
    
IntRect WebPage::rootViewToScreen(const IntRect& rect)
{
    IntRect screenRect;
    sendSync(Messages::WebPageProxy::RootViewToScreen(rect), Messages::WebPageProxy::RootViewToScreen::Reply(screenRect));
    return screenRect;
}
    
#if PLATFORM(IOS)
IntPoint WebPage::accessibilityScreenToRootView(const IntPoint& point)
{
    IntPoint windowPoint;
    sendSync(Messages::WebPageProxy::AccessibilityScreenToRootView(point), Messages::WebPageProxy::AccessibilityScreenToRootView::Reply(windowPoint));
    return windowPoint;
}

IntRect WebPage::rootViewToAccessibilityScreen(const IntRect& rect)
{
    IntRect screenRect;
    sendSync(Messages::WebPageProxy::RootViewToAccessibilityScreen(rect), Messages::WebPageProxy::RootViewToAccessibilityScreen::Reply(screenRect));
    return screenRect;
}
#endif

KeyboardUIMode WebPage::keyboardUIMode()
{
    bool fullKeyboardAccessEnabled = WebProcess::singleton().fullKeyboardAccessEnabled();
    return static_cast<KeyboardUIMode>((fullKeyboardAccessEnabled ? KeyboardAccessFull : KeyboardAccessDefault) | (m_tabToLinks ? KeyboardAccessTabsToLinks : 0));
}

void WebPage::runJavaScriptInMainFrame(const String& script, uint64_t callbackID)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    RefPtr<SerializedScriptValue> serializedResultValue;
    JSLockHolder lock(JSDOMWindow::commonVM());
    bool hadException = true;
    ExceptionDetails details;
    if (JSValue resultValue = m_mainFrame->coreFrame()->script().executeScript(script, true, &details).jsValue()) {
        hadException = false;
        serializedResultValue = SerializedScriptValue::create(m_mainFrame->jsContext(),
            toRef(m_mainFrame->coreFrame()->script().globalObject(mainThreadNormalWorld())->globalExec(), resultValue), nullptr);
    }

    IPC::DataReference dataReference;
    if (serializedResultValue)
        dataReference = serializedResultValue->data();
    send(Messages::WebPageProxy::ScriptValueCallback(dataReference, hadException, details, callbackID));
}

void WebPage::getContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

#if ENABLE(MHTML)
void WebPage::getContentsAsMHTMLData(uint64_t callbackID)
{
    RefPtr<SharedBuffer> buffer = MHTMLArchive::generateMHTMLData(m_page.get());

    // FIXME: Use SharedBufferDataReference.
    IPC::DataReference dataReference;
    if (buffer)
        dataReference = IPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}
#endif

void WebPage::getRenderTreeExternalRepresentation(uint64_t callbackID)
{
    String resultString = renderTreeExternalRepresentation();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

static Frame* frameWithSelection(Page* page)
{
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->selection().isRange())
            return frame;
    }

    return 0;
}

void WebPage::getSelectionAsWebArchiveData(uint64_t callbackID)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data;
    if (Frame* frame = frameWithSelection(m_page.get()))
        data = LegacyWebArchive::createFromSelection(frame)->rawDataRepresentation();
#endif

    IPC::DataReference dataReference;
#if PLATFORM(COCOA)
    if (data)
        dataReference = IPC::DataReference(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
#endif
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::getSelectionOrContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->selectionAsString();
    if (resultString.isEmpty())
        resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getSourceForFrame(uint64_t frameID, uint64_t callbackID)
{
    String resultString;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID))
       resultString = frame->source();

    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getMainResourceDataOfFrame(uint64_t frameID, uint64_t callbackID)
{
    RefPtr<SharedBuffer> buffer;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID)) {
        if (PluginView* pluginView = pluginViewForFrame(frame->coreFrame()))
            buffer = pluginView->liveResourceData();
        if (!buffer) {
            if (DocumentLoader* loader = frame->coreFrame()->loader().documentLoader())
                buffer = loader->mainResourceData();
        }
    }

    // FIXME: Use SharedBufferDataReference.
    IPC::DataReference dataReference;
    if (buffer)
        dataReference = IPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

static PassRefPtr<SharedBuffer> resourceDataForFrame(Frame* frame, const URL& resourceURL)
{
    DocumentLoader* loader = frame->loader().documentLoader();
    if (!loader)
        return 0;

    RefPtr<ArchiveResource> subresource = loader->subresource(resourceURL);
    if (!subresource)
        return 0;

    return subresource->data();
}

void WebPage::getResourceDataFromFrame(uint64_t frameID, const String& resourceURLString, uint64_t callbackID)
{
    RefPtr<SharedBuffer> buffer;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID)) {
        URL resourceURL(URL(), resourceURLString);
        buffer = resourceDataForFrame(frame->coreFrame(), resourceURL);
        if (!buffer) {
            // Try to get the resource data from the cache.
            buffer = cachedResponseDataForURL(resourceURL);
        }
    }

    // FIXME: Use SharedBufferDataReference.
    IPC::DataReference dataReference;
    if (buffer)
        dataReference = IPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::getWebArchiveOfFrame(uint64_t frameID, uint64_t callbackID)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID))
        data = frame->webArchiveData(nullptr, nullptr);
#else
    UNUSED_PARAM(frameID);
#endif

    IPC::DataReference dataReference;
#if PLATFORM(COCOA)
    if (data)
        dataReference = IPC::DataReference(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
#endif
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::forceRepaintWithoutCallback()
{
    m_drawingArea->forceRepaint();
}

void WebPage::forceRepaint(uint64_t callbackID)
{
    if (m_drawingArea->forceRepaintAsync(callbackID))
        return;

    forceRepaintWithoutCallback();
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    WebPreferencesStore::removeTestRunnerOverrides();
    updatePreferences(store);
}

void WebPage::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_page->settings();

    m_tabToLinks = store.getBoolValueForKey(WebPreferencesKey::tabsToLinksKey());
    m_asynchronousPluginInitializationEnabled = store.getBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledKey());
    m_asynchronousPluginInitializationEnabledForAllPlugins = store.getBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledForAllPluginsKey());
    m_artificialPluginInitializationDelayEnabled = store.getBoolValueForKey(WebPreferencesKey::artificialPluginInitializationDelayEnabledKey());

    m_scrollingPerformanceLoggingEnabled = store.getBoolValueForKey(WebPreferencesKey::scrollingPerformanceLoggingEnabledKey());

#if PLATFORM(COCOA)
    m_pdfPluginEnabled = store.getBoolValueForKey(WebPreferencesKey::pdfPluginEnabledKey());
#endif

    // FIXME: This should be generated from macro expansion for all preferences,
    // but we currently don't match the naming of WebCore exactly so we are
    // handrolling the boolean and integer preferences until that is fixed.

#define INITIALIZE_SETTINGS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) settings.set##KeyUpper(store.get##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key()));

    FOR_EACH_WEBKIT_STRING_PREFERENCE(INITIALIZE_SETTINGS)

#undef INITIALIZE_SETTINGS

    settings.setScriptEnabled(store.getBoolValueForKey(WebPreferencesKey::javaScriptEnabledKey()));
    settings.setScriptMarkupEnabled(store.getBoolValueForKey(WebPreferencesKey::javaScriptMarkupEnabledKey()));
    settings.setLoadsImagesAutomatically(store.getBoolValueForKey(WebPreferencesKey::loadsImagesAutomaticallyKey()));
    settings.setLoadsSiteIconsIgnoringImageLoadingSetting(store.getBoolValueForKey(WebPreferencesKey::loadsSiteIconsIgnoringImageLoadingPreferenceKey()));
    settings.setPluginsEnabled(store.getBoolValueForKey(WebPreferencesKey::pluginsEnabledKey()));
    settings.setJavaEnabled(store.getBoolValueForKey(WebPreferencesKey::javaEnabledKey()));
    settings.setJavaEnabledForLocalFiles(store.getBoolValueForKey(WebPreferencesKey::javaEnabledForLocalFilesKey()));    
    settings.setOfflineWebApplicationCacheEnabled(store.getBoolValueForKey(WebPreferencesKey::offlineWebApplicationCacheEnabledKey()));
    settings.setLocalStorageEnabled(store.getBoolValueForKey(WebPreferencesKey::localStorageEnabledKey()));
    settings.setXSSAuditorEnabled(store.getBoolValueForKey(WebPreferencesKey::xssAuditorEnabledKey()));
    settings.setFrameFlatteningEnabled(store.getBoolValueForKey(WebPreferencesKey::frameFlatteningEnabledKey()));
    if (store.getBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey()) && !usesEphemeralSession())
        setSessionID(SessionID::legacyPrivateSessionID());
    else if (!store.getBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey()) && sessionID() == SessionID::legacyPrivateSessionID())
        setSessionID(SessionID::defaultSessionID());
    settings.setDeveloperExtrasEnabled(store.getBoolValueForKey(WebPreferencesKey::developerExtrasEnabledKey()));
    settings.setJavaScriptRuntimeFlags(RuntimeFlags(store.getUInt32ValueForKey(WebPreferencesKey::javaScriptRuntimeFlagsKey())));
    settings.setTextAreasAreResizable(store.getBoolValueForKey(WebPreferencesKey::textAreasAreResizableKey()));
    settings.setNeedsSiteSpecificQuirks(store.getBoolValueForKey(WebPreferencesKey::needsSiteSpecificQuirksKey()));
    settings.setJavaScriptCanOpenWindowsAutomatically(store.getBoolValueForKey(WebPreferencesKey::javaScriptCanOpenWindowsAutomaticallyKey()));
    settings.setForceFTPDirectoryListings(store.getBoolValueForKey(WebPreferencesKey::forceFTPDirectoryListingsKey()));
    settings.setDNSPrefetchingEnabled(store.getBoolValueForKey(WebPreferencesKey::dnsPrefetchingEnabledKey()));
    settings.setDOMTimersThrottlingEnabled(store.getBoolValueForKey(WebPreferencesKey::domTimersThrottlingEnabledKey()));
#if ENABLE(WEB_ARCHIVE)
    settings.setWebArchiveDebugModeEnabled(store.getBoolValueForKey(WebPreferencesKey::webArchiveDebugModeEnabledKey()));
#endif
    settings.setLocalFileContentSniffingEnabled(store.getBoolValueForKey(WebPreferencesKey::localFileContentSniffingEnabledKey()));
    settings.setUsesPageCache(store.getBoolValueForKey(WebPreferencesKey::usesPageCacheKey()));
    settings.setPageCacheSupportsPlugins(store.getBoolValueForKey(WebPreferencesKey::pageCacheSupportsPluginsKey()));
    settings.setAuthorAndUserStylesEnabled(store.getBoolValueForKey(WebPreferencesKey::authorAndUserStylesEnabledKey()));
    settings.setPaginateDuringLayoutEnabled(store.getBoolValueForKey(WebPreferencesKey::paginateDuringLayoutEnabledKey()));
    settings.setDOMPasteAllowed(store.getBoolValueForKey(WebPreferencesKey::domPasteAllowedKey()));
    settings.setJavaScriptCanAccessClipboard(store.getBoolValueForKey(WebPreferencesKey::javaScriptCanAccessClipboardKey()));
    settings.setShouldPrintBackgrounds(store.getBoolValueForKey(WebPreferencesKey::shouldPrintBackgroundsKey()));
    settings.setWebSecurityEnabled(store.getBoolValueForKey(WebPreferencesKey::webSecurityEnabledKey()));
    settings.setAllowUniversalAccessFromFileURLs(store.getBoolValueForKey(WebPreferencesKey::allowUniversalAccessFromFileURLsKey()));
    settings.setAllowFileAccessFromFileURLs(store.getBoolValueForKey(WebPreferencesKey::allowFileAccessFromFileURLsKey()));

    settings.setMinimumFontSize(store.getDoubleValueForKey(WebPreferencesKey::minimumFontSizeKey()));
    settings.setMinimumLogicalFontSize(store.getDoubleValueForKey(WebPreferencesKey::minimumLogicalFontSizeKey()));
    settings.setDefaultFontSize(store.getDoubleValueForKey(WebPreferencesKey::defaultFontSizeKey()));
    settings.setDefaultFixedFontSize(store.getDoubleValueForKey(WebPreferencesKey::defaultFixedFontSizeKey()));
    settings.setLayoutFallbackWidth(store.getUInt32ValueForKey(WebPreferencesKey::layoutFallbackWidthKey()));
    settings.setDeviceWidth(store.getUInt32ValueForKey(WebPreferencesKey::deviceWidthKey()));
    settings.setDeviceHeight(store.getUInt32ValueForKey(WebPreferencesKey::deviceHeightKey()));
    settings.setEditableLinkBehavior(static_cast<WebCore::EditableLinkBehavior>(store.getUInt32ValueForKey(WebPreferencesKey::editableLinkBehaviorKey())));
    settings.setShowsToolTipOverTruncatedText(store.getBoolValueForKey(WebPreferencesKey::showsToolTipOverTruncatedTextKey()));

    settings.setAcceleratedCompositingForOverflowScrollEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedCompositingForOverflowScrollEnabledKey()));
    settings.setAcceleratedCompositingEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedCompositingEnabledKey()));
    settings.setAcceleratedDrawingEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedDrawingEnabledKey()));
    settings.setDisplayListDrawingEnabled(store.getBoolValueForKey(WebPreferencesKey::displayListDrawingEnabledKey()));
    settings.setCanvasUsesAcceleratedDrawing(store.getBoolValueForKey(WebPreferencesKey::canvasUsesAcceleratedDrawingKey()));
    settings.setShowDebugBorders(store.getBoolValueForKey(WebPreferencesKey::compositingBordersVisibleKey()));
    settings.setShowRepaintCounter(store.getBoolValueForKey(WebPreferencesKey::compositingRepaintCountersVisibleKey()));
    settings.setShowTiledScrollingIndicator(store.getBoolValueForKey(WebPreferencesKey::tiledScrollingIndicatorVisibleKey()));
    settings.setVisibleDebugOverlayRegions(store.getUInt32ValueForKey(WebPreferencesKey::visibleDebugOverlayRegionsKey()));
    settings.setUseGiantTiles(store.getBoolValueForKey(WebPreferencesKey::useGiantTilesKey()));
    
    settings.setAggressiveTileRetentionEnabled(store.getBoolValueForKey(WebPreferencesKey::aggressiveTileRetentionEnabledKey()));
    settings.setTemporaryTileCohortRetentionEnabled(store.getBoolValueForKey(WebPreferencesKey::temporaryTileCohortRetentionEnabledKey()));
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    RuntimeEnabledFeatures::sharedFeatures().setAnimationTriggersEnabled(store.getBoolValueForKey(WebPreferencesKey::cssAnimationTriggersEnabledKey()));
#endif
#if ENABLE(WEB_ANIMATIONS)
    RuntimeEnabledFeatures::sharedFeatures().setWebAnimationsEnabled(store.getBoolValueForKey(WebPreferencesKey::webAnimationsEnabledKey()));
#endif
    RuntimeEnabledFeatures::sharedFeatures().setCSSRegionsEnabled(store.getBoolValueForKey(WebPreferencesKey::cssRegionsEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setCSSCompositingEnabled(store.getBoolValueForKey(WebPreferencesKey::cssCompositingEnabledKey()));
    settings.setWebGLEnabled(store.getBoolValueForKey(WebPreferencesKey::webGLEnabledKey()));
    settings.setForceSoftwareWebGLRendering(store.getBoolValueForKey(WebPreferencesKey::forceSoftwareWebGLRenderingKey()));
    settings.setAccelerated2dCanvasEnabled(store.getBoolValueForKey(WebPreferencesKey::accelerated2dCanvasEnabledKey()));
    settings.setRequiresUserGestureForMediaPlayback(store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForMediaPlaybackKey()));
    settings.setAudioPlaybackRequiresUserGesture(store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForAudioPlaybackKey()));
    settings.setAllowsInlineMediaPlayback(store.getBoolValueForKey(WebPreferencesKey::allowsInlineMediaPlaybackKey()));
    settings.setInlineMediaPlaybackRequiresPlaysInlineAttribute(store.getBoolValueForKey(WebPreferencesKey::inlineMediaPlaybackRequiresPlaysInlineAttributeKey()));
    settings.setInvisibleAutoplayNotPermitted(store.getBoolValueForKey(WebPreferencesKey::invisibleAutoplayNotPermittedKey()));
    settings.setMediaDataLoadsAutomatically(store.getBoolValueForKey(WebPreferencesKey::mediaDataLoadsAutomaticallyKey()));
    settings.setAllowsPictureInPictureMediaPlayback(store.getBoolValueForKey(WebPreferencesKey::allowsPictureInPictureMediaPlaybackKey()));
    settings.setMediaControlsScaleWithPageZoom(store.getBoolValueForKey(WebPreferencesKey::mediaControlsScaleWithPageZoomKey()));
    settings.setMockScrollbarsEnabled(store.getBoolValueForKey(WebPreferencesKey::mockScrollbarsEnabledKey()));
    settings.setHyperlinkAuditingEnabled(store.getBoolValueForKey(WebPreferencesKey::hyperlinkAuditingEnabledKey()));
    settings.setRequestAnimationFrameEnabled(store.getBoolValueForKey(WebPreferencesKey::requestAnimationFrameEnabledKey()));
#if ENABLE(SMOOTH_SCROLLING)
    settings.setScrollAnimatorEnabled(store.getBoolValueForKey(WebPreferencesKey::scrollAnimatorEnabledKey()));
#endif
    settings.setForceUpdateScrollbarsOnMainThreadForPerformanceTesting(store.getBoolValueForKey(WebPreferencesKey::forceUpdateScrollbarsOnMainThreadForPerformanceTestingKey()));
    settings.setInteractiveFormValidationEnabled(store.getBoolValueForKey(WebPreferencesKey::interactiveFormValidationEnabledKey()));
    settings.setSpatialNavigationEnabled(store.getBoolValueForKey(WebPreferencesKey::spatialNavigationEnabledKey()));

    settings.setHttpEquivEnabled(store.getBoolValueForKey(WebPreferencesKey::httpEquivEnabledKey()));

    DatabaseManager::singleton().setIsAvailable(store.getBoolValueForKey(WebPreferencesKey::databasesEnabledKey()));

#if ENABLE(FULLSCREEN_API)
    settings.setFullScreenEnabled(store.getBoolValueForKey(WebPreferencesKey::fullScreenEnabledKey()));
#endif

#if USE(AVFOUNDATION)
    settings.setAVFoundationEnabled(store.getBoolValueForKey(WebPreferencesKey::isAVFoundationEnabledKey()));
    settings.setAVFoundationNSURLSessionEnabled(store.getBoolValueForKey(WebPreferencesKey::isAVFoundationNSURLSessionEnabledKey()));
#endif

#if PLATFORM(COCOA)
    settings.setQTKitEnabled(store.getBoolValueForKey(WebPreferencesKey::isQTKitEnabledKey()));
#endif

#if PLATFORM(IOS) && HAVE(AVKIT)
    settings.setAVKitEnabled(true);
#endif

#if ENABLE(IOS_TEXT_AUTOSIZING)
    settings.setMinimumZoomFontSize(store.getDoubleValueForKey(WebPreferencesKey::minimumZoomFontSizeKey()));
#endif

#if ENABLE(WEB_AUDIO)
    settings.setWebAudioEnabled(store.getBoolValueForKey(WebPreferencesKey::webAudioEnabledKey()));
#endif

#if ENABLE(MEDIA_STREAM)
    settings.setMediaStreamEnabled(store.getBoolValueForKey(WebPreferencesKey::mediaStreamEnabledKey()));
#endif

#if ENABLE(SERVICE_CONTROLS)
    settings.setImageControlsEnabled(store.getBoolValueForKey(WebPreferencesKey::imageControlsEnabledKey()));
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    settings.setAllowsAirPlayForMediaPlayback(store.getBoolValueForKey(WebPreferencesKey::allowsAirPlayForMediaPlaybackKey()));
#endif

#if ENABLE(RESOURCE_USAGE)
    settings.setResourceUsageOverlayVisible(store.getBoolValueForKey(WebPreferencesKey::resourceUsageOverlayVisibleKey()));
#endif

    settings.setSuppressesIncrementalRendering(store.getBoolValueForKey(WebPreferencesKey::suppressesIncrementalRenderingKey()));
    settings.setIncrementalRenderingSuppressionTimeoutInSeconds(store.getDoubleValueForKey(WebPreferencesKey::incrementalRenderingSuppressionTimeoutKey()));
    settings.setBackspaceKeyNavigationEnabled(store.getBoolValueForKey(WebPreferencesKey::backspaceKeyNavigationEnabledKey()));
    settings.setWantsBalancedSetDefersLoadingBehavior(store.getBoolValueForKey(WebPreferencesKey::wantsBalancedSetDefersLoadingBehaviorKey()));
    settings.setCaretBrowsingEnabled(store.getBoolValueForKey(WebPreferencesKey::caretBrowsingEnabledKey()));

#if ENABLE(VIDEO_TRACK)
    settings.setShouldDisplaySubtitles(store.getBoolValueForKey(WebPreferencesKey::shouldDisplaySubtitlesKey()));
    settings.setShouldDisplayCaptions(store.getBoolValueForKey(WebPreferencesKey::shouldDisplayCaptionsKey()));
    settings.setShouldDisplayTextDescriptions(store.getBoolValueForKey(WebPreferencesKey::shouldDisplayTextDescriptionsKey()));
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    settings.setNotificationsEnabled(store.getBoolValueForKey(WebPreferencesKey::notificationsEnabledKey()));
#endif

    settings.setShouldRespectImageOrientation(store.getBoolValueForKey(WebPreferencesKey::shouldRespectImageOrientationKey()));
    settings.setStorageBlockingPolicy(static_cast<SecurityOrigin::StorageBlockingPolicy>(store.getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey())));
    settings.setCookieEnabled(store.getBoolValueForKey(WebPreferencesKey::cookieEnabledKey()));

    settings.setDiagnosticLoggingEnabled(store.getBoolValueForKey(WebPreferencesKey::diagnosticLoggingEnabledKey()));

    settings.setScrollingPerformanceLoggingEnabled(m_scrollingPerformanceLoggingEnabled);

    settings.setPlugInSnapshottingEnabled(store.getBoolValueForKey(WebPreferencesKey::plugInSnapshottingEnabledKey()));
    settings.setSnapshotAllPlugIns(store.getBoolValueForKey(WebPreferencesKey::snapshotAllPlugInsKey()));
    settings.setAutostartOriginPlugInSnapshottingEnabled(store.getBoolValueForKey(WebPreferencesKey::autostartOriginPlugInSnapshottingEnabledKey()));
    settings.setPrimaryPlugInSnapshotDetectionEnabled(store.getBoolValueForKey(WebPreferencesKey::primaryPlugInSnapshotDetectionEnabledKey()));
    settings.setUsesEncodingDetector(store.getBoolValueForKey(WebPreferencesKey::usesEncodingDetectorKey()));

#if ENABLE(TEXT_AUTOSIZING)
    settings.setTextAutosizingEnabled(store.getBoolValueForKey(WebPreferencesKey::textAutosizingEnabledKey()));
#endif

    settings.setLogsPageMessagesToSystemConsoleEnabled(store.getBoolValueForKey(WebPreferencesKey::logsPageMessagesToSystemConsoleEnabledKey()));
    settings.setAsynchronousSpellCheckingEnabled(store.getBoolValueForKey(WebPreferencesKey::asynchronousSpellCheckingEnabledKey()));

    settings.setSmartInsertDeleteEnabled(store.getBoolValueForKey(WebPreferencesKey::smartInsertDeleteEnabledKey()));
    settings.setSelectTrailingWhitespaceEnabled(store.getBoolValueForKey(WebPreferencesKey::selectTrailingWhitespaceEnabledKey()));
    settings.setShowsURLsInToolTips(store.getBoolValueForKey(WebPreferencesKey::showsURLsInToolTipsEnabledKey()));

    settings.setHiddenPageDOMTimerThrottlingEnabled(store.getBoolValueForKey(WebPreferencesKey::hiddenPageDOMTimerThrottlingEnabledKey()));
    settings.setHiddenPageDOMTimerThrottlingAutoIncreases(store.getBoolValueForKey(WebPreferencesKey::hiddenPageDOMTimerThrottlingAutoIncreasesKey()));

    settings.setHiddenPageCSSAnimationSuspensionEnabled(store.getBoolValueForKey(WebPreferencesKey::hiddenPageCSSAnimationSuspensionEnabledKey()));
    settings.setLowPowerVideoAudioBufferSizeEnabled(store.getBoolValueForKey(WebPreferencesKey::lowPowerVideoAudioBufferSizeEnabledKey()));
    settings.setSimpleLineLayoutEnabled(store.getBoolValueForKey(WebPreferencesKey::simpleLineLayoutEnabledKey()));
    settings.setSimpleLineLayoutDebugBordersEnabled(store.getBoolValueForKey(WebPreferencesKey::simpleLineLayoutDebugBordersEnabledKey()));
    
    settings.setNewBlockInsideInlineModelEnabled(store.getBoolValueForKey(WebPreferencesKey::newBlockInsideInlineModelEnabledKey()));
    settings.setAntialiasedFontDilationEnabled(store.getBoolValueForKey(WebPreferencesKey::antialiasedFontDilationEnabledKey()));
    
    settings.setSubpixelCSSOMElementMetricsEnabled(store.getBoolValueForKey(WebPreferencesKey::subpixelCSSOMElementMetricsEnabledKey()));

    settings.setUseLegacyTextAlignPositionedElementBehavior(store.getBoolValueForKey(WebPreferencesKey::useLegacyTextAlignPositionedElementBehaviorKey()));

#if ENABLE(MEDIA_SOURCE)
    settings.setMediaSourceEnabled(store.getBoolValueForKey(WebPreferencesKey::mediaSourceEnabledKey()));
#endif

#if ENABLE(MEDIA_STREAM)
    settings.setMockCaptureDevicesEnabled(store.getBoolValueForKey(WebPreferencesKey::mockCaptureDevicesEnabledKey()));
#endif

    settings.setShouldConvertPositionStyleOnCopy(store.getBoolValueForKey(WebPreferencesKey::shouldConvertPositionStyleOnCopyKey()));

    settings.setStandalone(store.getBoolValueForKey(WebPreferencesKey::standaloneKey()));
    settings.setTelephoneNumberParsingEnabled(store.getBoolValueForKey(WebPreferencesKey::telephoneNumberParsingEnabledKey()));
    settings.setAllowMultiElementImplicitSubmission(store.getBoolValueForKey(WebPreferencesKey::allowMultiElementImplicitSubmissionKey()));
    settings.setAlwaysUseAcceleratedOverflowScroll(store.getBoolValueForKey(WebPreferencesKey::alwaysUseAcceleratedOverflowScrollKey()));

    settings.setPasswordEchoEnabled(store.getBoolValueForKey(WebPreferencesKey::passwordEchoEnabledKey()));
    settings.setPasswordEchoDurationInSeconds(store.getDoubleValueForKey(WebPreferencesKey::passwordEchoDurationKey()));
    
    settings.setLayoutInterval(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(store.getDoubleValueForKey(WebPreferencesKey::layoutIntervalKey()))));
    settings.setMaxParseDuration(store.getDoubleValueForKey(WebPreferencesKey::maxParseDurationKey()));

    settings.setEnableInheritURIQueryComponent(store.getBoolValueForKey(WebPreferencesKey::enableInheritURIQueryComponentKey()));

    settings.setShouldDispatchJavaScriptWindowOnErrorEvents(true);

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebPagePreferences.cpp>
#endif

#if PLATFORM(IOS)
    settings.setUseImageDocumentForSubframePDF(true);
#endif

#if ENABLE(DATA_DETECTION)
    settings.setDataDetectorTypes(static_cast<DataDetectorTypes>(store.getUInt32ValueForKey(WebPreferencesKey::dataDetectorTypesKey())));
#endif
#if ENABLE(GAMEPAD)
    RuntimeEnabledFeatures::sharedFeatures().setGamepadsEnabled(store.getBoolValueForKey(WebPreferencesKey::gamepadsEnabledKey()));
#endif

#if ENABLE(SERVICE_CONTROLS)
    settings.setServiceControlsEnabled(store.getBoolValueForKey(WebPreferencesKey::serviceControlsEnabledKey()));
#endif

    bool processSuppressionEnabled = store.getBoolValueForKey(WebPreferencesKey::pageVisibilityBasedProcessSuppressionEnabledKey());
    if (m_processSuppressionEnabled != processSuppressionEnabled) {
        m_processSuppressionEnabled = processSuppressionEnabled;
        updateUserActivity();
    }

    platformPreferencesDidChange(store);

    if (m_drawingArea)
        m_drawingArea->updatePreferences(store);

#if PLATFORM(IOS)
    m_ignoreViewportScalingConstraints = store.getBoolValueForKey(WebPreferencesKey::ignoreViewportScalingConstraintsKey());
    m_viewportConfiguration.setCanIgnoreScalingConstraints(m_ignoreViewportScalingConstraints);
    m_viewportConfiguration.setForceAlwaysUserScalable(store.getBoolValueForKey(WebPreferencesKey::forceAlwaysUserScalableKey()));
#endif
}

#if ENABLE(DATA_DETECTION)
void WebPage::setDataDetectionResults(NSArray *detectionResults)
{
    DataDetectionResult dataDetectionResult;
    dataDetectionResult.results = detectionResults;
    send(Messages::WebPageProxy::SetDataDetectionResult(dataDetectionResult));
}
#endif

#if PLATFORM(COCOA)
void WebPage::willCommitLayerTree(RemoteLayerTreeTransaction& layerTransaction)
{
    layerTransaction.setContentsSize(corePage()->mainFrame().view()->contentsSize());
    layerTransaction.setScrollOrigin(corePage()->mainFrame().view()->scrollOrigin());
    layerTransaction.setPageScaleFactor(corePage()->pageScaleFactor());
    layerTransaction.setRenderTreeSize(corePage()->renderTreeSize());
    layerTransaction.setPageExtendedBackgroundColor(corePage()->pageExtendedBackgroundColor());
#if PLATFORM(IOS)
    layerTransaction.setScaleWasSetByUIProcess(scaleWasSetByUIProcess());
    layerTransaction.setMinimumScaleFactor(m_viewportConfiguration.minimumScale());
    layerTransaction.setMaximumScaleFactor(m_viewportConfiguration.maximumScale());
    layerTransaction.setInitialScaleFactor(m_viewportConfiguration.initialScale());
    layerTransaction.setViewportMetaTagWidth(m_viewportConfiguration.viewportArguments().width);
    layerTransaction.setViewportMetaTagWidthWasExplicit(m_viewportConfiguration.viewportArguments().widthWasExplicit);
    layerTransaction.setViewportMetaTagCameFromImageDocument(m_viewportConfiguration.viewportArguments().type == ViewportArguments::ImageDocument);
    layerTransaction.setAllowsUserScaling(allowsUserScaling());
#endif
#if PLATFORM(MAC)
    layerTransaction.setScrollPosition(corePage()->mainFrame().view()->scrollPosition());
#endif
}

void WebPage::didFlushLayerTreeAtTime(std::chrono::milliseconds timestamp)
{
#if PLATFORM(IOS)
    if (m_oldestNonStableUpdateVisibleContentRectsTimestamp != std::chrono::milliseconds::zero()) {
        std::chrono::milliseconds elapsed = timestamp - m_oldestNonStableUpdateVisibleContentRectsTimestamp;
        m_oldestNonStableUpdateVisibleContentRectsTimestamp = std::chrono::milliseconds::zero();

        m_estimatedLatency = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(m_estimatedLatency.count() * 0.80 + elapsed.count() * 0.20));
    }
#else
    UNUSED_PARAM(timestamp);
#endif
}
#endif

WebInspector* WebPage::inspector(LazyCreationPolicy behavior)
{
    if (m_isClosed)
        return nullptr;
    if (!m_inspector && behavior == LazyCreationPolicy::CreateIfNeeded)
        m_inspector = WebInspector::create(this);
    return m_inspector.get();
}

WebInspectorUI* WebPage::inspectorUI()
{
    if (m_isClosed)
        return nullptr;
    if (!m_inspectorUI)
        m_inspectorUI = WebInspectorUI::create(*this);
    return m_inspectorUI.get();
}

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
WebVideoFullscreenManager* WebPage::videoFullscreenManager()
{
    if (!m_videoFullscreenManager)
        m_videoFullscreenManager = WebVideoFullscreenManager::create(this);
    return m_videoFullscreenManager.get();
}
#endif

#if PLATFORM(IOS)
void WebPage::setAllowsMediaDocumentInlinePlayback(bool allows)
{
    m_page->setAllowsMediaDocumentInlinePlayback(allows);
}
#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManager* WebPage::fullScreenManager()
{
    if (!m_fullScreenManager)
        m_fullScreenManager = WebFullScreenManager::create(this);
    return m_fullScreenManager.get();
}
#endif

NotificationPermissionRequestManager* WebPage::notificationPermissionRequestManager()
{
    if (m_notificationPermissionRequestManager)
        return m_notificationPermissionRequestManager.get();

    m_notificationPermissionRequestManager = NotificationPermissionRequestManager::create(this);
    return m_notificationPermissionRequestManager.get();
}

#if !PLATFORM(GTK) && !PLATFORM(COCOA)
bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document().frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return false;

    Editor::Command command = frame->editor().command(interpretKeyEvent(evt));

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(evt);
    }

    if (command.execute(evt))
        return true;

    // Don't allow text insertion for nodes that cannot edit.
    if (!frame->editor().canEdit())
        return false;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (evt->charCode() < ' ')
        return false;

    return frame->editor().insertText(evt->keyEvent()->text(), evt);
}
#endif

#if ENABLE(DRAG_SUPPORT)

#if PLATFORM(GTK)
void WebPage::performDragControllerAction(uint64_t action, WebCore::DragData dragData)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(DragOperationNone, false, 0));
        DataObjectGtk* data = const_cast<DataObjectGtk*>(dragData.platformData());
        data->deref();
        return;
    }

    switch (action) {
    case DragControllerActionEntered: {
        DragOperation resolvedDragOperation = m_page->dragController().dragEntered(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted()));
        break;
    }
    case DragControllerActionUpdated: {
        DragOperation resolvedDragOperation = m_page->dragController().dragEntered(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted()));
        break;
    }
    case DragControllerActionExited:
        m_page->dragController().dragExited(dragData);
        break;

    case DragControllerActionPerformDragOperation: {
        m_page->dragController().performDragOperation(dragData);
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
    // DragData does not delete its platformData so we need to do that here.
    DataObjectGtk* data = const_cast<DataObjectGtk*>(dragData.platformData());
    data->deref();
}

#else
void WebPage::performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const String& dragStorageName, uint32_t flags, const SandboxExtension::Handle& sandboxExtensionHandle, const SandboxExtension::HandleArray& sandboxExtensionsHandleArray)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(DragOperationNone, false, 0));
        return;
    }

    DragData dragData(dragStorageName, clientPosition, globalPosition, static_cast<DragOperation>(draggingSourceOperationMask), static_cast<DragApplicationFlags>(flags));
    switch (action) {
    case DragControllerActionEntered: {
        DragOperation resolvedDragOperation = m_page->dragController().dragEntered(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted()));
        break;

    }
    case DragControllerActionUpdated: {
        DragOperation resolvedDragOperation = m_page->dragController().dragUpdated(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted()));
        break;
    }
    case DragControllerActionExited:
        m_page->dragController().dragExited(dragData);
        break;
        
    case DragControllerActionPerformDragOperation: {
        ASSERT(!m_pendingDropSandboxExtension);

        m_pendingDropSandboxExtension = SandboxExtension::create(sandboxExtensionHandle);
        for (size_t i = 0; i < sandboxExtensionsHandleArray.size(); i++) {
            if (RefPtr<SandboxExtension> extension = SandboxExtension::create(sandboxExtensionsHandleArray[i]))
                m_pendingDropExtensionsForFileUpload.append(extension);
        }

        m_page->dragController().performDragOperation(dragData);

        // If we started loading a local file, the sandbox extension tracker would have adopted this
        // pending drop sandbox extension. If not, we'll play it safe and clear it.
        m_pendingDropSandboxExtension = nullptr;

        m_pendingDropExtensionsForFileUpload.clear();
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
}
#endif

void WebPage::dragEnded(WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t operation)
{
    IntPoint adjustedClientPosition(clientPosition.x() + m_page->dragController().dragOffset().x(), clientPosition.y() + m_page->dragController().dragOffset().y());
    IntPoint adjustedGlobalPosition(globalPosition.x() + m_page->dragController().dragOffset().x(), globalPosition.y() + m_page->dragController().dragOffset().y());

    m_page->dragController().dragEnded();
    FrameView* view = m_page->mainFrame().view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(adjustedClientPosition, adjustedGlobalPosition, LeftButton, PlatformEvent::MouseMoved, 0, false, false, false, false, currentTime(), 0);
    m_page->mainFrame().eventHandler().dragSourceEndedAt(event, (DragOperation)operation);
}

void WebPage::willPerformLoadDragDestinationAction()
{
    m_sandboxExtensionTracker.willPerformLoadDragDestinationAction(m_pendingDropSandboxExtension.release());
}

void WebPage::mayPerformUploadDragDestinationAction()
{
    for (size_t i = 0; i < m_pendingDropExtensionsForFileUpload.size(); i++)
        m_pendingDropExtensionsForFileUpload[i]->consumePermanently();
    m_pendingDropExtensionsForFileUpload.clear();
}
    
#endif // ENABLE(DRAG_SUPPORT)

WebUndoStep* WebPage::webUndoStep(uint64_t stepID)
{
    return m_undoStepMap.get(stepID);
}

void WebPage::addWebUndoStep(uint64_t stepID, WebUndoStep* entry)
{
    m_undoStepMap.set(stepID, entry);
}

void WebPage::removeWebEditCommand(uint64_t stepID)
{
    m_undoStepMap.remove(stepID);
}

void WebPage::unapplyEditCommand(uint64_t stepID)
{
    WebUndoStep* step = webUndoStep(stepID);
    if (!step)
        return;

    step->step()->unapply();
}

void WebPage::reapplyEditCommand(uint64_t stepID)
{
    WebUndoStep* step = webUndoStep(stepID);
    if (!step)
        return;

    m_isInRedo = true;
    step->step()->reapply();
    m_isInRedo = false;
}

void WebPage::didRemoveEditCommand(uint64_t commandID)
{
    removeWebEditCommand(commandID);
}

void WebPage::setActivePopupMenu(WebPopupMenu* menu)
{
    m_activePopupMenu = menu;
}

#if ENABLE(INPUT_TYPE_COLOR)
void WebPage::setActiveColorChooser(WebColorChooser* colorChooser)
{
    m_activeColorChooser = colorChooser;
}

void WebPage::didEndColorPicker()
{
    m_activeColorChooser->didEndChooser();
}

void WebPage::didChooseColor(const WebCore::Color& color)
{
    m_activeColorChooser->didChooseColor(color);
}
#endif

void WebPage::setActiveOpenPanelResultListener(PassRefPtr<WebOpenPanelResultListener> openPanelResultListener)
{
    m_activeOpenPanelResultListener = openPanelResultListener;
}

bool WebPage::findStringFromInjectedBundle(const String& target, FindOptions options)
{
    return m_page->findString(target, options);
}

void WebPage::findString(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.findString(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::findStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.findStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::getImageForFindMatch(uint32_t matchIndex)
{
    m_findController.getImageForFindMatch(matchIndex);
}

void WebPage::selectFindMatch(uint32_t matchIndex)
{
    m_findController.selectFindMatch(matchIndex);
}

void WebPage::hideFindUI()
{
    m_findController.hideFindUI();
}

void WebPage::countStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.countStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::didChangeSelectedIndexForActivePopupMenu(int32_t newIndex)
{
    changeSelectedIndex(newIndex);
    m_activePopupMenu = nullptr;
}

void WebPage::changeSelectedIndex(int32_t index)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->didChangeSelectedIndex(index);
}

#if PLATFORM(IOS)
void WebPage::didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>& files, const String& displayString, const IPC::DataReference& iconData)
{
    if (!m_activeOpenPanelResultListener)
        return;

    RefPtr<Icon> icon;
    if (!iconData.isEmpty()) {
        RetainPtr<CFDataRef> dataRef = adoptCF(CFDataCreate(nullptr, iconData.data(), iconData.size()));
        RetainPtr<CGDataProviderRef> imageProviderRef = adoptCF(CGDataProviderCreateWithCFData(dataRef.get()));
        RetainPtr<CGImageRef> imageRef = adoptCF(CGImageCreateWithJPEGDataProvider(imageProviderRef.get(), nullptr, true, kCGRenderingIntentDefault));
        icon = Icon::createIconForImage(imageRef.get());
    }

    m_activeOpenPanelResultListener->didChooseFilesWithDisplayStringAndIcon(files, displayString, icon.get());
    m_activeOpenPanelResultListener = nullptr;
}
#endif

void WebPage::didChooseFilesForOpenPanel(const Vector<String>& files)
{
    if (!m_activeOpenPanelResultListener)
        return;

    m_activeOpenPanelResultListener->didChooseFiles(files);
    m_activeOpenPanelResultListener = nullptr;
}

void WebPage::didCancelForOpenPanel()
{
    m_activeOpenPanelResultListener = nullptr;
}

#if ENABLE(SANDBOX_EXTENSIONS)
void WebPage::extendSandboxForFileFromOpenPanel(const SandboxExtension::Handle& handle)
{
    bool result = SandboxExtension::consumePermanently(handle);
    if (!result) {
        // We have reports of cases where this fails for some unknown reason, <rdar://problem/10156710>.
        WTFLogAlways("WebPage::extendSandboxForFileFromOpenPanel(): Could not consume a sandbox extension");
    }
}
#endif

#if ENABLE(GEOLOCATION)
void WebPage::didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed)
{
    m_geolocationPermissionRequestManager.didReceiveGeolocationPermissionDecision(geolocationID, allowed);
}
#endif

void WebPage::didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed)
{
    notificationPermissionRequestManager()->didReceiveNotificationPermissionDecision(notificationID, allowed);
}

#if ENABLE(MEDIA_STREAM)
void WebPage::didReceiveUserMediaPermissionDecision(uint64_t userMediaID, bool allowed, const String& audioDeviceUID, const String& videoDeviceUID)
{
    m_userMediaPermissionRequestManager.didReceiveUserMediaPermissionDecision(userMediaID, allowed, audioDeviceUID, videoDeviceUID);
}

void WebPage::didCompleteUserMediaPermissionCheck(uint64_t userMediaID, const String& mediaDeviceIdentifierHashSalt, bool allowed)
{
    m_userMediaPermissionRequestManager.didCompleteUserMediaPermissionCheck(userMediaID, mediaDeviceIdentifierHashSalt, allowed);
}
#endif

#if !PLATFORM(IOS)
void WebPage::advanceToNextMisspelling(bool startBeforeSelection)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().advanceToNextMisspelling(startBeforeSelection);
}
#endif

void WebPage::changeSpellingToWord(const String& word)
{
    replaceSelectionWithText(&m_page->focusController().focusedOrMainFrame(), word);
}

void WebPage::unmarkAllMisspellings()
{
    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            document->markers().removeMarkers(DocumentMarker::Spelling);
    }
}

void WebPage::unmarkAllBadGrammar()
{
    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            document->markers().removeMarkers(DocumentMarker::Grammar);
    }
}

#if USE(APPKIT)
void WebPage::uppercaseWord()
{
    m_page->focusController().focusedOrMainFrame().editor().uppercaseWord();
}

void WebPage::lowercaseWord()
{
    m_page->focusController().focusedOrMainFrame().editor().lowercaseWord();
}

void WebPage::capitalizeWord()
{
    m_page->focusController().focusedOrMainFrame().editor().capitalizeWord();
}
#endif
    
void WebPage::setTextForActivePopupMenu(int32_t index)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->setTextForIndex(index);
}

#if PLATFORM(GTK)
void WebPage::failedToShowPopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->client()->popupDidHide();
}
#endif

#if ENABLE(CONTEXT_MENUS)
void WebPage::didSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    if (!m_contextMenu)
        return;

    m_contextMenu->itemSelected(item);
    m_contextMenu = nullptr;
}
#endif

void WebPage::replaceSelectionWithText(Frame* frame, const String& text)
{
    bool selectReplacement = true;
    bool smartReplace = false;
    return frame->editor().replaceSelectionWithText(text, selectReplacement, smartReplace);
}

#if !PLATFORM(IOS)
void WebPage::clearSelection()
{
    m_page->focusController().focusedOrMainFrame().selection().clear();
}
#endif

void WebPage::restoreSelectionInFocusedEditableElement()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isNone())
        return;

    if (auto document = frame.document()) {
        if (auto element = document->focusedElement())
            element->updateFocusAppearance(SelectionRestorationMode::Restore, SelectionRevealMode::DoNotReveal);
    }
}

bool WebPage::mainFrameHasCustomContentProvider() const
{
    if (Frame* frame = mainFrame()) {
        WebFrameLoaderClient* webFrameLoaderClient = toWebFrameLoaderClient(frame->loader().client());
        ASSERT(webFrameLoaderClient);
        return webFrameLoaderClient->frameHasCustomContentProvider();
    }

    return false;
}

void WebPage::addMIMETypeWithCustomContentProvider(const String& mimeType)
{
    m_mimeTypesWithCustomContentProviders.add(mimeType);
}

void WebPage::updateMainFrameScrollOffsetPinning()
{
    Frame& frame = m_page->mainFrame();
    ScrollPosition scrollPosition = frame.view()->scrollPosition();
    ScrollPosition maximumScrollPosition = frame.view()->maximumScrollPosition();
    ScrollPosition minimumScrollPosition = frame.view()->minimumScrollPosition();

    bool isPinnedToLeftSide = (scrollPosition.x() <= minimumScrollPosition.x());
    bool isPinnedToRightSide = (scrollPosition.x() >= maximumScrollPosition.x());
    bool isPinnedToTopSide = (scrollPosition.y() <= minimumScrollPosition.y());
    bool isPinnedToBottomSide = (scrollPosition.y() >= maximumScrollPosition.y());

    if (isPinnedToLeftSide != m_cachedMainFrameIsPinnedToLeftSide || isPinnedToRightSide != m_cachedMainFrameIsPinnedToRightSide || isPinnedToTopSide != m_cachedMainFrameIsPinnedToTopSide || isPinnedToBottomSide != m_cachedMainFrameIsPinnedToBottomSide) {
        send(Messages::WebPageProxy::DidChangeScrollOffsetPinningForMainFrame(isPinnedToLeftSide, isPinnedToRightSide, isPinnedToTopSide, isPinnedToBottomSide));
        
        m_cachedMainFrameIsPinnedToLeftSide = isPinnedToLeftSide;
        m_cachedMainFrameIsPinnedToRightSide = isPinnedToRightSide;
        m_cachedMainFrameIsPinnedToTopSide = isPinnedToTopSide;
        m_cachedMainFrameIsPinnedToBottomSide = isPinnedToBottomSide;
    }
}

void WebPage::mainFrameDidLayout()
{
    unsigned pageCount = m_page->pageCount();
    if (pageCount != m_cachedPageCount) {
        send(Messages::WebPageProxy::DidChangePageCount(pageCount));
        m_cachedPageCount = pageCount;
    }

#if PLATFORM(MAC)
    m_viewGestureGeometryCollector.mainFrameDidLayout();
#endif
#if PLATFORM(IOS)
    if (FrameView* frameView = mainFrameView()) {
        IntSize newContentSize = frameView->contentsSize();
        if (m_viewportConfiguration.setContentsSize(newContentSize))
            viewportConfigurationChanged();
    }
    m_findController.redraw();
#endif
}

void WebPage::addPluginView(PluginView* pluginView)
{
    ASSERT(!m_pluginViews.contains(pluginView));

    m_pluginViews.add(pluginView);
    m_hasSeenPlugin = true;
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    LOG(Plugins, "Primary Plug-In Detection: triggering detection from addPluginView(%p)", pluginView);
    m_determinePrimarySnapshottedPlugInTimer.startOneShot(0);
#endif
}

void WebPage::removePluginView(PluginView* pluginView)
{
    ASSERT(m_pluginViews.contains(pluginView));

    m_pluginViews.remove(pluginView);
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    LOG(Plugins, "Primary Plug-In Detection: removePluginView(%p)", pluginView);
#endif
}

void WebPage::sendSetWindowFrame(const FloatRect& windowFrame)
{
#if PLATFORM(COCOA)
    m_hasCachedWindowFrame = false;
#endif
    send(Messages::WebPageProxy::SetWindowFrame(windowFrame));
}

#if PLATFORM(COCOA)
void WebPage::windowAndViewFramesChanged(const FloatRect& windowFrameInScreenCoordinates, const FloatRect& windowFrameInUnflippedScreenCoordinates, const FloatRect& viewFrameInWindowCoordinates, const FloatPoint& accessibilityViewCoordinates)
{
    m_windowFrameInScreenCoordinates = windowFrameInScreenCoordinates;
    m_windowFrameInUnflippedScreenCoordinates = windowFrameInUnflippedScreenCoordinates;
    m_viewFrameInWindowCoordinates = viewFrameInWindowCoordinates;
    m_accessibilityPosition = accessibilityViewCoordinates;
    
    // Tell all our plug-in views that the window and view frames have changed.
    for (auto* pluginView : m_pluginViews)
        pluginView->windowAndViewFramesChanged(enclosingIntRect(windowFrameInScreenCoordinates), enclosingIntRect(viewFrameInWindowCoordinates));

    m_hasCachedWindowFrame = !m_windowFrameInUnflippedScreenCoordinates.isEmpty();
}
#endif

void WebPage::setMainFrameIsScrollable(bool isScrollable)
{
    m_mainFrameIsScrollable = isScrollable;
    m_drawingArea->mainFrameScrollabilityChanged(isScrollable);

    if (FrameView* frameView = m_mainFrame->coreFrame()->view()) {
        frameView->setCanHaveScrollbars(isScrollable);
        frameView->setProhibitsScrolling(!isScrollable);
    }
}

bool WebPage::windowIsFocused() const
{
    return m_page->focusController().isActive();
}

bool WebPage::windowAndWebPageAreFocused() const
{
    if (!isVisible())
        return false;

    return m_page->focusController().isFocused() && m_page->focusController().isActive();
}

void WebPage::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    if (decoder.messageReceiverName() == Messages::CoordinatedLayerTreeHost::messageReceiverName()) {
        if (m_drawingArea)
            m_drawingArea->didReceiveCoordinatedLayerTreeHostMessage(connection, decoder);
        return;
    }
#endif

    if (decoder.messageReceiverName() == Messages::WebInspector::messageReceiverName()) {
        if (WebInspector* inspector = this->inspector())
            inspector->didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::WebInspectorUI::messageReceiverName()) {
        if (WebInspectorUI* inspectorUI = this->inspectorUI())
            inspectorUI->didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(FULLSCREEN_API)
    if (decoder.messageReceiverName() == Messages::WebFullScreenManager::messageReceiverName()) {
        fullScreenManager()->didReceiveMessage(connection, decoder);
        return;
    }
#endif

    didReceiveWebPageMessage(connection, decoder);
}

void WebPage::didReceiveSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{   
    didReceiveSyncWebPageMessage(connection, decoder, replyEncoder);
}
    
InjectedBundleBackForwardList* WebPage::backForwardList()
{
    if (!m_backForwardList)
        m_backForwardList = InjectedBundleBackForwardList::create(this);
    return m_backForwardList.get();
}

#if ENABLE(ASYNC_SCROLLING)
ScrollingCoordinator* WebPage::scrollingCoordinator() const
{
    return m_page->scrollingCoordinator();
}
#endif

WebPage::SandboxExtensionTracker::~SandboxExtensionTracker()
{
    invalidate();
}

void WebPage::SandboxExtensionTracker::invalidate()
{
    m_pendingProvisionalSandboxExtension = nullptr;

    if (m_provisionalSandboxExtension) {
        m_provisionalSandboxExtension->revoke();
        m_provisionalSandboxExtension = nullptr;
    }

    if (m_committedSandboxExtension) {
        m_committedSandboxExtension->revoke();
        m_committedSandboxExtension = nullptr;
    }
}

void WebPage::SandboxExtensionTracker::willPerformLoadDragDestinationAction(PassRefPtr<SandboxExtension> pendingDropSandboxExtension)
{
    setPendingProvisionalSandboxExtension(pendingDropSandboxExtension);
}

void WebPage::SandboxExtensionTracker::beginLoad(WebFrame* frame, const SandboxExtension::Handle& handle)
{
    ASSERT_UNUSED(frame, frame->isMainFrame());

    setPendingProvisionalSandboxExtension(SandboxExtension::create(handle));
}

void WebPage::SandboxExtensionTracker::setPendingProvisionalSandboxExtension(PassRefPtr<SandboxExtension> pendingProvisionalSandboxExtension)
{
    m_pendingProvisionalSandboxExtension = pendingProvisionalSandboxExtension;    
}

static bool shouldReuseCommittedSandboxExtension(WebFrame* frame)
{
    ASSERT(frame->isMainFrame());

    FrameLoader& frameLoader = frame->coreFrame()->loader();
    FrameLoadType frameLoadType = frameLoader.loadType();

    // If the page is being reloaded, it should reuse whatever extension is committed.
    if (frameLoadType == FrameLoadType::Reload || frameLoadType == FrameLoadType::ReloadFromOrigin)
        return true;

    DocumentLoader* documentLoader = frameLoader.documentLoader();
    DocumentLoader* provisionalDocumentLoader = frameLoader.provisionalDocumentLoader();
    if (!documentLoader || !provisionalDocumentLoader)
        return false;

    if (documentLoader->url().isLocalFile() && provisionalDocumentLoader->url().isLocalFile())
        return true;

    return false;
}

void WebPage::SandboxExtensionTracker::didStartProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // We should only reuse the commited sandbox extension if it is not null. It can be
    // null if the last load was for an error page.
    if (m_committedSandboxExtension && shouldReuseCommittedSandboxExtension(frame))
        m_pendingProvisionalSandboxExtension = m_committedSandboxExtension;

    ASSERT(!m_provisionalSandboxExtension);

    m_provisionalSandboxExtension = m_pendingProvisionalSandboxExtension.release();
    if (!m_provisionalSandboxExtension)
        return;

    ASSERT(!m_provisionalSandboxExtension || frame->coreFrame()->loader().provisionalDocumentLoader()->url().isLocalFile());

    m_provisionalSandboxExtension->consume();
}

void WebPage::SandboxExtensionTracker::didCommitProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (m_committedSandboxExtension)
        m_committedSandboxExtension->revoke();

    m_committedSandboxExtension = m_provisionalSandboxExtension.release();

    // We can also have a non-null m_pendingProvisionalSandboxExtension if a new load is being started.
    // This extension is not cleared, because it does not pertain to the failed load, and will be needed.
}

void WebPage::SandboxExtensionTracker::didFailProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->revoke();
    m_provisionalSandboxExtension = nullptr;

    // We can also have a non-null m_pendingProvisionalSandboxExtension if a new load is being started
    // (notably, if the current one fails because the new one cancels it). This extension is not cleared,
    // because it does not pertain to the failed load, and will be needed.
}

bool WebPage::hasLocalDataForURL(const URL& url)
{
    if (url.isLocalFile())
        return true;

    DocumentLoader* documentLoader = m_page->mainFrame().loader().documentLoader();
    if (documentLoader && documentLoader->subresource(url))
        return true;

    return platformHasLocalDataForURL(url);
}

void WebPage::setCustomTextEncodingName(const String& encoding)
{
    m_page->mainFrame().loader().reloadWithOverrideEncoding(encoding);
}

void WebPage::didRemoveBackForwardItem(uint64_t itemID)
{
    WebBackForwardListProxy::removeItem(itemID);
}

#if PLATFORM(COCOA)

bool WebPage::isSpeaking()
{
    bool result;
    return sendSync(Messages::WebPageProxy::GetIsSpeaking(), Messages::WebPageProxy::GetIsSpeaking::Reply(result)) && result;
}

void WebPage::speak(const String& string)
{
    send(Messages::WebPageProxy::Speak(string));
}

void WebPage::stopSpeaking()
{
    send(Messages::WebPageProxy::StopSpeaking());
}

#endif

#if PLATFORM(MAC)
RetainPtr<PDFDocument> WebPage::pdfDocumentForPrintingFrame(Frame* coreFrame)
{
    Document* document = coreFrame->document();
    if (!is<PluginDocument>(document))
        return nullptr;

    PluginView* pluginView = static_cast<PluginView*>(downcast<PluginDocument>(*document).pluginWidget());
    if (!pluginView)
        return nullptr;

    return pluginView->pdfDocumentForPrinting();
}
#endif // PLATFORM(MAC)

void WebPage::beginPrinting(uint64_t frameID, const PrintInfo& printInfo)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    Frame* coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;

#if PLATFORM(MAC)
    if (pdfDocumentForPrintingFrame(coreFrame))
        return;
#endif // PLATFORM(MAC)

    if (!m_printContext)
        m_printContext = std::make_unique<PrintContext>(coreFrame);

    drawingArea()->setLayerTreeStateIsFrozen(true);
    m_printContext->begin(printInfo.availablePaperWidth, printInfo.availablePaperHeight);

    float fullPageHeight;
    m_printContext->computePageRects(FloatRect(0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight), 0, 0, printInfo.pageSetupScaleFactor, fullPageHeight, true);

#if PLATFORM(GTK)
    if (!m_printOperation)
        m_printOperation = WebPrintOperationGtk::create(this, printInfo);
#endif
}

void WebPage::endPrinting()
{
    drawingArea()->setLayerTreeStateIsFrozen(false);
    m_printContext = nullptr;
}

void WebPage::computePagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, uint64_t callbackID)
{
    Vector<IntRect> resultPageRects;
    double resultTotalScaleFactorForPrinting = 1;
    computePagesForPrintingImpl(frameID, printInfo, resultPageRects, resultTotalScaleFactorForPrinting);
    send(Messages::WebPageProxy::ComputedPagesCallback(resultPageRects, resultTotalScaleFactorForPrinting, callbackID));
}

void WebPage::computePagesForPrintingImpl(uint64_t frameID, const PrintInfo& printInfo, Vector<WebCore::IntRect>& resultPageRects, double& resultTotalScaleFactorForPrinting)
{
    ASSERT(resultPageRects.isEmpty());

    beginPrinting(frameID, printInfo);

    if (m_printContext) {
        resultPageRects = m_printContext->pageRects();
        resultTotalScaleFactorForPrinting = m_printContext->computeAutomaticScaleFactor(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight)) * printInfo.pageSetupScaleFactor;
    }
#if PLATFORM(COCOA)
    else
        computePagesForPrintingPDFDocument(frameID, printInfo, resultPageRects);
#endif // PLATFORM(COCOA)

    // If we're asked to print, we should actually print at least a blank page.
    if (resultPageRects.isEmpty())
        resultPageRects.append(IntRect(0, 0, 1, 1));
}

#if PLATFORM(COCOA)
void WebPage::drawRectToImage(uint64_t frameID, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, uint64_t callbackID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;

    RefPtr<WebImage> image;

#if USE(CG)
    if (coreFrame) {
#if PLATFORM(MAC)
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame));
#else
        ASSERT(coreFrame->document()->printing());
#endif

        RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(imageSize, ShareableBitmap::SupportsAlpha);
        auto graphicsContext = bitmap->createGraphicsContext();

        float printingScale = static_cast<float>(imageSize.width()) / rect.width();
        graphicsContext->scale(FloatSize(printingScale, printingScale));

#if PLATFORM(MAC)
        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame)) {
            ASSERT(!m_printContext);
            graphicsContext->scale(FloatSize(1, -1));
            graphicsContext->translate(0, -rect.height());
            drawPDFDocument(graphicsContext->platformContext(), pdfDocument.get(), printInfo, rect);
        } else
#endif
        {
            m_printContext->spoolRect(*graphicsContext, rect);
        }

        image = WebImage::create(bitmap.release());
    }
#endif

    ShareableBitmap::Handle handle;

    if (image)
        image->bitmap()->createHandle(handle, SharedMemory::Protection::ReadOnly);

    send(Messages::WebPageProxy::ImageCallback(handle, callbackID));
}

void WebPage::drawPagesToPDF(uint64_t frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, uint64_t callbackID)
{
    RetainPtr<CFMutableDataRef> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, first, count, pdfPageData);
    send(Messages::WebPageProxy::DataCallback(IPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get())), callbackID));
}

void WebPage::drawPagesToPDFImpl(uint64_t frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, RetainPtr<CFMutableDataRef>& pdfPageData)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;

    pdfPageData = adoptCF(CFDataCreateMutable(0, 0));

#if USE(CG)
    if (coreFrame) {

#if PLATFORM(MAC)
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame));
#else
        ASSERT(coreFrame->document()->printing());
#endif

        // FIXME: Use CGDataConsumerCreate with callbacks to avoid copying the data.
        RetainPtr<CGDataConsumerRef> pdfDataConsumer = adoptCF(CGDataConsumerCreateWithCFData(pdfPageData.get()));

        CGRect mediaBox = (m_printContext && m_printContext->pageCount()) ? m_printContext->pageRect(0) : CGRectMake(0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight);
        RetainPtr<CGContextRef> context = adoptCF(CGPDFContextCreate(pdfDataConsumer.get(), &mediaBox, 0));

#if PLATFORM(MAC)
        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame)) {
            ASSERT(!m_printContext);
            drawPagesToPDFFromPDFDocument(context.get(), pdfDocument.get(), printInfo, first, count);
        } else
#endif
        {
            size_t pageCount = m_printContext->pageCount();
            for (uint32_t page = first; page < first + count; ++page) {
                if (page >= pageCount)
                    break;

                RetainPtr<CFDictionaryRef> pageInfo = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
                CGPDFContextBeginPage(context.get(), pageInfo.get());

                GraphicsContext ctx(context.get());
                ctx.scale(FloatSize(1, -1));
                ctx.translate(0, -m_printContext->pageRect(page).height());
                m_printContext->spoolPage(ctx, page, m_printContext->pageRect(page).width());

                CGPDFContextEndPage(context.get());
            }
        }
        CGPDFContextClose(context.get());
    }
#endif
}

#elif PLATFORM(GTK)
void WebPage::drawPagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, uint64_t callbackID)
{
    beginPrinting(frameID, printInfo);
    if (m_printContext && m_printOperation) {
        m_printOperation->startPrint(m_printContext.get(), callbackID);
        return;
    }

    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::didFinishPrintOperation(const WebCore::ResourceError& error, uint64_t callbackID)
{
    send(Messages::WebPageProxy::PrintFinishedCallback(error, callbackID));
    m_printOperation = nullptr;
}
#endif

void WebPage::savePDFToFileInDownloadsFolder(const String& suggestedFilename, const String& originatingURLString, const uint8_t* data, unsigned long size)
{
    send(Messages::WebPageProxy::SavePDFToFileInDownloadsFolder(suggestedFilename, originatingURLString, IPC::DataReference(data, size)));
}

#if PLATFORM(COCOA)
void WebPage::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, const String& originatingURLString, const uint8_t* data, unsigned long size, const String& pdfUUID)
{
    send(Messages::WebPageProxy::SavePDFToTemporaryFolderAndOpenWithNativeApplication(suggestedFilename, originatingURLString, IPC::DataReference(data, size), pdfUUID));
}
#endif

void WebPage::addResourceRequest(unsigned long identifier, const WebCore::ResourceRequest& request)
{
    if (!request.url().protocolIsInHTTPFamily())
        return;

    if (m_mainFrameProgressCompleted && !ScriptController::processingUserGesture())
        return;

    ASSERT(!m_trackedNetworkResourceRequestIdentifiers.contains(identifier));
    bool wasEmpty = m_trackedNetworkResourceRequestIdentifiers.isEmpty();
    m_trackedNetworkResourceRequestIdentifiers.add(identifier);
    if (wasEmpty)
        send(Messages::WebPageProxy::SetNetworkRequestsInProgress(true));
}

void WebPage::removeResourceRequest(unsigned long identifier)
{
    if (!m_trackedNetworkResourceRequestIdentifiers.remove(identifier))
        return;

    if (m_trackedNetworkResourceRequestIdentifiers.isEmpty())
        send(Messages::WebPageProxy::SetNetworkRequestsInProgress(false));
}

void WebPage::setMediaVolume(float volume)
{
    m_page->setMediaVolume(volume);
}

void WebPage::setMuted(bool muted)
{
    m_page->setMuted(muted);
}

#if ENABLE(MEDIA_SESSION)
void WebPage::handleMediaEvent(uint32_t eventType)
{
    m_page->handleMediaEvent(static_cast<MediaEventType>(eventType));
}

void WebPage::setVolumeOfMediaElement(double volume, uint64_t elementID)
{
    m_page->setVolumeOfMediaElement(volume, elementID);
}
#endif

void WebPage::setMayStartMediaWhenInWindow(bool mayStartMedia)
{
    if (mayStartMedia == m_mayStartMediaWhenInWindow)
        return;

    m_mayStartMediaWhenInWindow = mayStartMedia;
    if (m_mayStartMediaWhenInWindow && m_page->isInWindow())
        m_setCanStartMediaTimer.startOneShot(0);
}

void WebPage::runModal()
{
    if (m_isClosed)
        return;
    if (m_isRunningModal)
        return;

    m_isRunningModal = true;
    send(Messages::WebPageProxy::RunModal());
#if !ASSERT_DISABLED
    Ref<WebPage> protector(*this);
#endif
    RunLoop::run();
    ASSERT(!m_isRunningModal);
}

bool WebPage::canHandleRequest(const WebCore::ResourceRequest& request)
{
    if (SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(request.url().protocol()))
        return true;

    if (request.url().protocolIs("blob"))
        return true;

    return platformCanHandleRequest(request);
}

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
void WebPage::commitPageTransitionViewport()
{
    m_drawingArea->setLayerTreeStateIsFrozen(false);
}
#endif

#if PLATFORM(COCOA)
void WebPage::handleAlternativeTextUIResult(const String& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().handleAlternativeTextUIResult(result);
}
#endif

void WebPage::simulateMouseDown(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseDown, static_cast<WebMouseEvent::Button>(button), position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time, WebCore::ForceAtClick));
}

void WebPage::simulateMouseUp(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseUp, static_cast<WebMouseEvent::Button>(button), position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time, WebCore::ForceAtClick));
}

void WebPage::simulateMouseMotion(WebCore::IntPoint position, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseMove, WebMouseEvent::NoButton, position, position, 0, 0, 0, 0, WebMouseEvent::Modifiers(), time, 0));
}

void WebPage::setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.editor().canEdit())
        return;

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, compositionString.length(), Color(Color::black), false));
    frame.editor().setComposition(compositionString, underlines, from, from + length);
}

bool WebPage::hasCompositionForTesting()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    return frame.editor().hasComposition();
}

void WebPage::confirmCompositionForTesting(const String& compositionString)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.editor().canEdit())
        return;

    if (compositionString.isNull())
        frame.editor().confirmComposition();
    frame.editor().confirmComposition(compositionString);
}

void WebPage::wheelEventHandlersChanged(bool hasHandlers)
{
    if (m_hasWheelEventHandlers == hasHandlers)
        return;

    m_hasWheelEventHandlers = hasHandlers;
    recomputeShortCircuitHorizontalWheelEventsState();
}

static bool hasEnabledHorizontalScrollbar(ScrollableArea* scrollableArea)
{
    if (Scrollbar* scrollbar = scrollableArea->horizontalScrollbar())
        return scrollbar->enabled();

    return false;
}

static bool pageContainsAnyHorizontalScrollbars(Frame* mainFrame)
{
    if (FrameView* frameView = mainFrame->view()) {
        if (hasEnabledHorizontalScrollbar(frameView))
            return true;
    }

    for (Frame* frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView)
            continue;

        const HashSet<ScrollableArea*>* scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (HashSet<ScrollableArea*>::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            if (!scrollableArea->scrollbarsCanBeActive())
                continue;

            if (hasEnabledHorizontalScrollbar(scrollableArea))
                return true;
        }
    }

    return false;
}

void WebPage::recomputeShortCircuitHorizontalWheelEventsState()
{
    bool canShortCircuitHorizontalWheelEvents = !m_hasWheelEventHandlers;

    if (canShortCircuitHorizontalWheelEvents) {
        // Check if we have any horizontal scroll bars on the page.
        if (pageContainsAnyHorizontalScrollbars(mainFrame()))
            canShortCircuitHorizontalWheelEvents = false;
    }

    if (m_canShortCircuitHorizontalWheelEvents == canShortCircuitHorizontalWheelEvents)
        return;

    m_canShortCircuitHorizontalWheelEvents = canShortCircuitHorizontalWheelEvents;
    send(Messages::WebPageProxy::SetCanShortCircuitHorizontalWheelEvents(m_canShortCircuitHorizontalWheelEvents));
}

MainFrame* WebPage::mainFrame() const
{
    return m_page ? &m_page->mainFrame() : nullptr;
}

FrameView* WebPage::mainFrameView() const
{
    if (Frame* frame = mainFrame())
        return frame->view();
    
    return nullptr;
}

void WebPage::setScrollingPerformanceLoggingEnabled(bool enabled)
{
    m_scrollingPerformanceLoggingEnabled = enabled;

    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    frameView->setScrollingPerformanceLoggingEnabled(enabled);
}

bool WebPage::canPluginHandleResponse(const ResourceResponse& response)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    uint32_t pluginLoadPolicy;
    bool allowOnlyApplicationPlugins = !m_mainFrame->coreFrame()->loader().subframeLoader().allowPlugins();

    uint64_t pluginProcessToken;
    String newMIMEType;
    String unavailabilityDescription;
    if (!sendSync(Messages::WebPageProxy::FindPlugin(response.mimeType(), PluginProcessTypeNormal, response.url().string(), response.url().string(), response.url().string(), allowOnlyApplicationPlugins), Messages::WebPageProxy::FindPlugin::Reply(pluginProcessToken, newMIMEType, pluginLoadPolicy, unavailabilityDescription)))
        return false;

    bool isBlockedPlugin = (pluginLoadPolicy == PluginModuleBlockedForSecurity) || (pluginLoadPolicy == PluginModuleBlockedForCompatibility);
    return !isBlockedPlugin && pluginProcessToken;
#else
    UNUSED_PARAM(response);
    return false;
#endif
}

bool WebPage::shouldUseCustomContentProviderForResponse(const ResourceResponse& response)
{
    // If a plug-in exists that claims to support this response, it should take precedence over the custom content provider.
    return m_mimeTypesWithCustomContentProviders.contains(response.mimeType()) && !canPluginHandleResponse(response);
}

#if PLATFORM(COCOA)

void WebPage::insertTextAsync(const String& text, const EditingRange& replacementEditingRange, bool registerUndoGroup, uint32_t editingRangeIsRelativeTo)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (replacementEditingRange.location != notFound) {
        RefPtr<Range> replacementRange = rangeFromEditingRange(frame, replacementEditingRange, static_cast<EditingRangeIsRelativeTo>(editingRangeIsRelativeTo));
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection(*replacementRange, SEL_DEFAULT_AFFINITY));
    }
    
    if (registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping());
    
    if (!frame.editor().hasComposition()) {
        // An insertText: might be handled by other responders in the chain if we don't handle it.
        // One example is space bar that results in scrolling down the page.
        frame.editor().insertText(text, nullptr);
    } else
        frame.editor().confirmComposition(text);
}

void WebPage::getMarkedRangeAsync(uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    RefPtr<Range> range = frame.editor().compositionRange();
    size_t location;
    size_t length;
    if (!range || !TextIterator::getLocationAndLengthFromRange(frame.selection().rootEditableElementOrDocumentElement(), range.get(), location, length)) {
        location = notFound;
        length = 0;
    }

    send(Messages::WebPageProxy::EditingRangeCallback(EditingRange(location, length), callbackID));
}

void WebPage::getSelectedRangeAsync(uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    size_t location;
    size_t length;
    RefPtr<Range> range = frame.selection().toNormalizedRange();
    if (!range || !TextIterator::getLocationAndLengthFromRange(frame.selection().rootEditableElementOrDocumentElement(), range.get(), location, length)) {
        location = notFound;
        length = 0;
    }

    send(Messages::WebPageProxy::EditingRangeCallback(EditingRange(location, length), callbackID));
}

void WebPage::characterIndexForPointAsync(const WebCore::IntPoint& point, uint64_t callbackID)
{
    uint64_t index = notFound;

    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(point);
    Frame* frame = result.innerNonSharedNode() ? result.innerNodeFrame() : &m_page->focusController().focusedOrMainFrame();
    
    RefPtr<Range> range = frame->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (range) {
        size_t location;
        size_t length;
        if (TextIterator::getLocationAndLengthFromRange(frame->selection().rootEditableElementOrDocumentElement(), range.get(), location, length))
            index = static_cast<uint64_t>(location);
    }

    send(Messages::WebPageProxy::UnsignedCallback(index, callbackID));
}

void WebPage::firstRectForCharacterRangeAsync(const EditingRange& editingRange, uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    IntRect result(IntPoint(0, 0), IntSize(0, 0));
    
    RefPtr<Range> range = rangeFromEditingRange(frame, editingRange);
    if (!range) {
        send(Messages::WebPageProxy::RectForCharacterRangeCallback(result, EditingRange(notFound, 0), callbackID));
        return;
    }

    result = frame.view()->contentsToWindow(frame.editor().firstRectForRange(range.get()));

    // FIXME: Update actualRange to match the range of first rect.
    send(Messages::WebPageProxy::RectForCharacterRangeCallback(result, editingRange, callbackID));
}

void WebPage::setCompositionAsync(const String& text, Vector<CompositionUnderline> underlines, const EditingRange& selection, const EditingRange& replacementEditingRange)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (frame.selection().selection().isContentEditable()) {
        RefPtr<Range> replacementRange;
        if (replacementEditingRange.location != notFound) {
            replacementRange = rangeFromEditingRange(frame, replacementEditingRange);
            if (replacementRange)
                frame.selection().setSelection(VisibleSelection(*replacementRange, SEL_DEFAULT_AFFINITY));
        }

        frame.editor().setComposition(text, underlines, selection.location, selection.location + selection.length);
    }
}

void WebPage::confirmCompositionAsync()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().confirmComposition();
}

#endif // PLATFORM(COCOA)

#if PLATFORM(GTK)
static Frame* targetFrameForEditing(WebPage* page)
{
    Frame& targetFrame = page->corePage()->focusController().focusedOrMainFrame();

    Editor& editor = targetFrame.editor();
    if (!editor.canEdit())
        return nullptr;

    if (editor.hasComposition()) {
        // We should verify the parent node of this IME composition node are
        // editable because JavaScript may delete a parent node of the composition
        // node. In this case, WebKit crashes while deleting texts from the parent
        // node, which doesn't exist any longer.
        if (auto range = editor.compositionRange()) {
            if (!range->startContainer().isContentEditable())
                return nullptr;
        }
    }
    return &targetFrame;
}

void WebPage::confirmComposition(const String& compositionString, int64_t selectionStart, int64_t selectionLength)
{
    Frame* targetFrame = targetFrameForEditing(this);
    if (!targetFrame) {
        send(Messages::WebPageProxy::EditorStateChanged(editorState()));
        return;
    }

    targetFrame->editor().confirmComposition(compositionString);

    if (selectionStart == -1) {
        send(Messages::WebPageProxy::EditorStateChanged(editorState()));
        return;
    }

    Element* scope = targetFrame->selection().selection().rootEditableElement();
    RefPtr<Range> selectionRange = TextIterator::rangeFromLocationAndLength(scope, selectionStart, selectionLength);
    ASSERT_WITH_MESSAGE(selectionRange, "Invalid selection: [%lld:%lld] in text of length %d", static_cast<long long>(selectionStart), static_cast<long long>(selectionLength), scope->innerText().length());

    if (selectionRange) {
        VisibleSelection selection(*selectionRange, SEL_DEFAULT_AFFINITY);
        targetFrame->selection().setSelection(selection);
    }
    send(Messages::WebPageProxy::EditorStateChanged(editorState()));
}

void WebPage::setComposition(const String& text, const Vector<CompositionUnderline>& underlines, uint64_t selectionStart, uint64_t selectionLength, uint64_t replacementStart, uint64_t replacementLength)
{
    Frame* targetFrame = targetFrameForEditing(this);
    if (!targetFrame || !targetFrame->selection().selection().isContentEditable()) {
        send(Messages::WebPageProxy::EditorStateChanged(editorState()));
        return;
    }

    if (replacementLength > 0) {
        // The layout needs to be uptodate before setting a selection
        targetFrame->document()->updateLayout();

        Element* scope = targetFrame->selection().selection().rootEditableElement();
        RefPtr<Range> replacementRange = TextIterator::rangeFromLocationAndLength(scope, replacementStart, replacementLength);
        targetFrame->editor().setIgnoreCompositionSelectionChange(true);
        targetFrame->selection().setSelection(VisibleSelection(*replacementRange, SEL_DEFAULT_AFFINITY));
        targetFrame->editor().setIgnoreCompositionSelectionChange(false);
    }

    targetFrame->editor().setComposition(text, underlines, selectionStart, selectionStart + selectionLength);
    send(Messages::WebPageProxy::EditorStateChanged(editorState()));
}

void WebPage::cancelComposition()
{
    if (Frame* targetFrame = targetFrameForEditing(this))
        targetFrame->editor().cancelComposition();
    send(Messages::WebPageProxy::EditorStateChanged(editorState()));
}
#endif

void WebPage::didChangeSelection()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    FrameView* view = frame.view();

    // If there is a layout pending, we should avoid populating EditorState that require layout to be done or it will
    // trigger a synchronous layout every time the selection changes. sendPostLayoutEditorStateIfNeeded() will be called
    // to send the full editor state after layout is done if we send a partial editor state here.
    auto editorState = this->editorState(view && view->needsLayout() ? IncludePostLayoutDataHint::No : IncludePostLayoutDataHint::Yes);
    m_isEditorStateMissingPostLayoutData = editorState.isMissingPostLayoutData;

#if PLATFORM(MAC)
    // Abandon the current inline input session if selection changed for any other reason but an input method direct action.
    // FIXME: This logic should be in WebCore.
    // FIXME: Many changes that affect composition node do not go through didChangeSelection(). We need to do something when DOM manipulation affects the composition, because otherwise input method's idea about it will be different from Editor's.
    // FIXME: We can't cancel composition when selection changes to NoSelection, but we probably should.
    if (frame.editor().hasComposition() && !frame.editor().ignoreCompositionSelectionChange() && !frame.selection().isNone()) {
        frame.editor().cancelComposition();
        send(Messages::WebPageProxy::CompositionWasCanceled(editorState));
    } else
        send(Messages::WebPageProxy::EditorStateChanged(editorState));
#else
    send(Messages::WebPageProxy::EditorStateChanged(editorState), pageID(), IPC::DispatchMessageEvenWhenWaitingForSyncReply);
#endif

#if PLATFORM(IOS)
    m_drawingArea->scheduleCompositingLayerFlush();
#endif
}

void WebPage::sendPostLayoutEditorStateIfNeeded()
{
    if (!m_isEditorStateMissingPostLayoutData)
        return;

    send(Messages::WebPageProxy::EditorStateChanged(editorState(IncludePostLayoutDataHint::Yes)), pageID(), IPC::DispatchMessageEvenWhenWaitingForSyncReply);
    m_isEditorStateMissingPostLayoutData = false;
}

void WebPage::discardedComposition()
{
    send(Messages::WebPageProxy::CompositionWasCanceled(editorState()));
}

void WebPage::setMinimumLayoutSize(const IntSize& minimumLayoutSize)
{
    if (m_minimumLayoutSize == minimumLayoutSize)
        return;

    m_minimumLayoutSize = minimumLayoutSize;
    if (minimumLayoutSize.width() <= 0) {
        corePage()->mainFrame().view()->enableAutoSizeMode(false, IntSize(), IntSize());
        return;
    }

    int minimumLayoutWidth = minimumLayoutSize.width();
    int minimumLayoutHeight = std::max(minimumLayoutSize.height(), 1);

    int maximumSize = std::numeric_limits<int>::max();

    corePage()->mainFrame().view()->enableAutoSizeMode(true, IntSize(minimumLayoutWidth, minimumLayoutHeight), IntSize(maximumSize, maximumSize));
}

void WebPage::setAutoSizingShouldExpandToViewHeight(bool shouldExpand)
{
    if (m_autoSizingShouldExpandToViewHeight == shouldExpand)
        return;

    m_autoSizingShouldExpandToViewHeight = shouldExpand;

    corePage()->mainFrame().view()->setAutoSizeFixedMinimumHeight(shouldExpand ? m_viewSize.height() : 0);
}

bool WebPage::isSmartInsertDeleteEnabled()
{
    return m_page->settings().smartInsertDeleteEnabled();
}

void WebPage::setSmartInsertDeleteEnabled(bool enabled)
{
    if (m_page->settings().smartInsertDeleteEnabled() != enabled) {
        m_page->settings().setSmartInsertDeleteEnabled(enabled);
        setSelectTrailingWhitespaceEnabled(!enabled);
    }
}

bool WebPage::isSelectTrailingWhitespaceEnabled()
{
    return m_page->settings().selectTrailingWhitespaceEnabled();
}

void WebPage::setSelectTrailingWhitespaceEnabled(bool enabled)
{
    if (m_page->settings().selectTrailingWhitespaceEnabled() != enabled) {
        m_page->settings().setSelectTrailingWhitespaceEnabled(enabled);
        setSmartInsertDeleteEnabled(!enabled);
    }
}

bool WebPage::canShowMIMEType(const String& MIMEType) const
{
    if (MIMETypeRegistry::canShowMIMEType(MIMEType))
        return true;

    if (m_mimeTypesWithCustomContentProviders.contains(MIMEType))
        return true;

    const PluginData& pluginData = m_page->pluginData();
    if (pluginData.supportsWebVisibleMimeType(MIMEType, PluginData::AllPlugins) && corePage()->mainFrame().loader().subframeLoader().allowPlugins())
        return true;

    // We can use application plugins even if plugins aren't enabled.
    if (pluginData.supportsWebVisibleMimeType(MIMEType, PluginData::OnlyApplicationPlugins))
        return true;

    return false;
}

void WebPage::addTextCheckingRequest(uint64_t requestID, PassRefPtr<TextCheckingRequest> request)
{
    m_pendingTextCheckingRequestMap.add(requestID, request);
}

void WebPage::didFinishCheckingText(uint64_t requestID, const Vector<TextCheckingResult>& result)
{
    RefPtr<TextCheckingRequest> request = m_pendingTextCheckingRequestMap.take(requestID);
    if (!request)
        return;

    request->didSucceed(result);
}

void WebPage::didCancelCheckingText(uint64_t requestID)
{
    RefPtr<TextCheckingRequest> request = m_pendingTextCheckingRequestMap.take(requestID);
    if (!request)
        return;

    request->didCancel();
}

void WebPage::willReplaceMultipartContent(const WebFrame& frame)
{
#if PLATFORM(IOS)
    if (!frame.isMainFrame())
        return;

    m_previousExposedContentRect = m_drawingArea->exposedContentRect();
#endif
}

void WebPage::didReplaceMultipartContent(const WebFrame& frame)
{
#if PLATFORM(IOS)
    if (!frame.isMainFrame())
        return;

    // Restore the previous exposed content rect so that it remains fixed when replacing content
    // from multipart/x-mixed-replace streams.
    m_drawingArea->setExposedContentRect(m_previousExposedContentRect);
#endif
}

void WebPage::didCommitLoad(WebFrame* frame)
{
#if PLATFORM(IOS)
    frame->setFirstLayerTreeTransactionIDAfterDidCommitLoad(downcast<RemoteLayerTreeDrawingArea>(*m_drawingArea).nextTransactionID());
    cancelPotentialTapInFrame(*frame);
    resetAssistedNodeForFrame(frame);
#endif

    if (!frame->isMainFrame())
        return;

    // If previous URL is invalid, then it's not a real page that's being navigated away from.
    // Most likely, this is actually the first load to be committed in this page.
    if (frame->coreFrame()->loader().previousURL().isValid())
        reportUsedFeatures();

    // Only restore the scale factor for standard frame loads (of the main frame).
    if (frame->coreFrame()->loader().loadType() == FrameLoadType::Standard) {
        Page* page = frame->coreFrame()->page();

#if PLATFORM(MAC)
        // As a very special case, we disable non-default layout modes in WKView for main-frame PluginDocuments.
        // Ideally we would only worry about this in WKView or the WKViewLayoutStrategies, but if we allow
        // a round-trip to the UI process, you'll see the wrong scale temporarily. So, we reset it here, and then
        // again later from the UI process.
        if (frame->coreFrame()->document()->isPluginDocument()) {
            scaleView(1);
            setUseFixedLayout(false);
        }
#endif

        if (page && page->pageScaleFactor() != 1)
            scalePage(1, IntPoint());
    }
#if PLATFORM(IOS)
    m_hasReceivedVisibleContentRectsAfterDidCommitLoad = false;
    m_scaleWasSetByUIProcess = false;
    m_userHasChangedPageScaleFactor = false;
    m_estimatedLatency = std::chrono::milliseconds(1000 / 60);

#if ENABLE(IOS_TOUCH_EVENTS)
    WebProcess::singleton().eventDispatcher().clearQueuedTouchEventsForPage(*this);
#endif

    resetViewportDefaultConfiguration(frame);
    const Frame* coreFrame = frame->coreFrame();
    
    bool viewportChanged = false;
    if (m_viewportConfiguration.setContentsSize(coreFrame->view()->contentsSize()))
        viewportChanged = true;

    if (m_viewportConfiguration.setViewportArguments(coreFrame->document()->viewportArguments()))
        viewportChanged = true;

    if (viewportChanged)
        viewportConfigurationChanged();
#endif

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    resetPrimarySnapshottedPlugIn();
#endif

    WebProcess::singleton().updateActivePages();

    updateMainFrameScrollOffsetPinning();
}

void WebPage::didFinishLoad(WebFrame* frame)
{
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    if (!frame->isMainFrame())
        return;

    m_readyToFindPrimarySnapshottedPlugin = true;
    LOG(Plugins, "Primary Plug-In Detection: triggering detection from didFinishLoad (marking as ready to detect).");
    m_determinePrimarySnapshottedPlugInTimer.startOneShot(0);
#else
    UNUSED_PARAM(frame);
#endif
}

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
static const int primarySnapshottedPlugInSearchLimit = 3000;
static const float primarySnapshottedPlugInSearchBucketSize = 1.1;
static const int primarySnapshottedPlugInMinimumWidth = 400;
static const int primarySnapshottedPlugInMinimumHeight = 300;
static const unsigned maxPrimarySnapshottedPlugInDetectionAttempts = 2;
static const int deferredPrimarySnapshottedPlugInDetectionDelay = 3;
static const float overlappingImageBoundsScale = 1.1;
static const float minimumOverlappingImageToPluginDimensionScale = .9;

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
void WebPage::determinePrimarySnapshottedPlugInTimerFired()
{
    if (!m_page)
        return;
    
    Settings& settings = m_page->settings();
    if (!settings.snapshotAllPlugIns() && settings.primaryPlugInSnapshotDetectionEnabled())
        determinePrimarySnapshottedPlugIn();
}
#endif

void WebPage::determinePrimarySnapshottedPlugIn()
{
    if (!m_page->settings().plugInSnapshottingEnabled())
        return;

    LOG(Plugins, "Primary Plug-In Detection: began.");

    if (!m_readyToFindPrimarySnapshottedPlugin) {
        LOG(Plugins, "Primary Plug-In Detection: exiting - not ready to find plugins.");
        return;
    }

    if (!m_hasSeenPlugin) {
        LOG(Plugins, "Primary Plug-In Detection: exiting - we never saw a plug-in get added to the page.");
        return;
    }

    if (m_didFindPrimarySnapshottedPlugin) {
        LOG(Plugins, "Primary Plug-In Detection: exiting - we've already found a primary plug-in.");
        return;
    }

    ++m_numberOfPrimarySnapshotDetectionAttempts;

    layoutIfNeeded();

    MainFrame& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view())
        return;
    if (!mainFrame.view()->renderView())
        return;
    RenderView& mainRenderView = *mainFrame.view()->renderView();

    IntRect searchRect = IntRect(IntPoint(), corePage()->mainFrame().view()->contentsSize());
    searchRect.intersect(IntRect(IntPoint(), IntSize(primarySnapshottedPlugInSearchLimit, primarySnapshottedPlugInSearchLimit)));

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent);

    HTMLPlugInImageElement* candidatePlugIn = nullptr;
    unsigned candidatePlugInArea = 0;

    for (Frame* frame = &mainFrame; frame; frame = frame->tree().traverseNextRendered()) {
        if (!frame->loader().subframeLoader().containsPlugins())
            continue;
        if (!frame->document() || !frame->view())
            continue;
        for (auto& plugInImageElement : descendantsOfType<HTMLPlugInImageElement>(*frame->document())) {
            if (plugInImageElement.displayState() == HTMLPlugInElement::Playing)
                continue;

            auto pluginRenderer = plugInImageElement.renderer();
            if (!pluginRenderer || !pluginRenderer->isBox())
                continue;
            auto& pluginRenderBox = downcast<RenderBox>(*pluginRenderer);
            if (!plugInIntersectsSearchRect(plugInImageElement))
                continue;

            IntRect plugInRectRelativeToView = plugInImageElement.clientRect();
            ScrollPosition scrollPosition = mainFrame.view()->documentScrollPositionRelativeToViewOrigin();
            IntRect plugInRectRelativeToTopDocument(plugInRectRelativeToView.location() + scrollPosition, plugInRectRelativeToView.size());
            HitTestResult hitTestResult(plugInRectRelativeToTopDocument.center());
            mainRenderView.hitTest(request, hitTestResult);

            Element* element = hitTestResult.innerElement();
            if (!element)
                continue;

            IntRect elementRectRelativeToView = element->clientRect();
            IntRect elementRectRelativeToTopDocument(elementRectRelativeToView.location() + scrollPosition, elementRectRelativeToView.size());
            LayoutRect inflatedPluginRect = plugInRectRelativeToTopDocument;
            LayoutUnit xOffset = (inflatedPluginRect.width() * overlappingImageBoundsScale - inflatedPluginRect.width()) / 2;
            LayoutUnit yOffset = (inflatedPluginRect.height() * overlappingImageBoundsScale - inflatedPluginRect.height()) / 2;
            inflatedPluginRect.inflateX(xOffset);
            inflatedPluginRect.inflateY(yOffset);

            if (element != &plugInImageElement) {
                if (!(is<HTMLImageElement>(*element)
                    && inflatedPluginRect.contains(elementRectRelativeToTopDocument)
                    && elementRectRelativeToTopDocument.width() > pluginRenderBox.width() * minimumOverlappingImageToPluginDimensionScale
                    && elementRectRelativeToTopDocument.height() > pluginRenderBox.height() * minimumOverlappingImageToPluginDimensionScale))
                    continue;
                LOG(Plugins, "Primary Plug-In Detection: Plug-in is hidden by an image that is roughly aligned with it, autoplaying regardless of whether or not it's actually the primary plug-in.");
                plugInImageElement.restartSnapshottedPlugIn();
            }

            if (plugInIsPrimarySize(plugInImageElement, candidatePlugInArea))
                candidatePlugIn = &plugInImageElement;
        }
    }
    if (!candidatePlugIn) {
        LOG(Plugins, "Primary Plug-In Detection: fail - did not find a candidate plug-in.");
        if (m_numberOfPrimarySnapshotDetectionAttempts < maxPrimarySnapshottedPlugInDetectionAttempts) {
            LOG(Plugins, "Primary Plug-In Detection: will attempt again in %ds.", deferredPrimarySnapshottedPlugInDetectionDelay);
            m_determinePrimarySnapshottedPlugInTimer.startOneShot(deferredPrimarySnapshottedPlugInDetectionDelay);
        }
        return;
    }

    LOG(Plugins, "Primary Plug-In Detection: success - found a candidate plug-in - inform it.");
    m_didFindPrimarySnapshottedPlugin = true;
    m_primaryPlugInPageOrigin = m_page->mainFrame().document()->baseURL().host();
    m_primaryPlugInOrigin = candidatePlugIn->loadedUrl().host();
    m_primaryPlugInMimeType = candidatePlugIn->loadedMimeType();

    candidatePlugIn->setIsPrimarySnapshottedPlugIn(true);
}

void WebPage::resetPrimarySnapshottedPlugIn()
{
    m_readyToFindPrimarySnapshottedPlugin = false;
    m_didFindPrimarySnapshottedPlugin = false;
    m_numberOfPrimarySnapshotDetectionAttempts = 0;
    m_hasSeenPlugin = false;
}

bool WebPage::matchesPrimaryPlugIn(const String& pageOrigin, const String& pluginOrigin, const String& mimeType) const
{
    if (!m_didFindPrimarySnapshottedPlugin)
        return false;

    return (pageOrigin == m_primaryPlugInPageOrigin && pluginOrigin == m_primaryPlugInOrigin && mimeType == m_primaryPlugInMimeType);
}

bool WebPage::plugInIntersectsSearchRect(HTMLPlugInImageElement& plugInImageElement)
{
    MainFrame& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view())
        return false;
    if (!mainFrame.view()->renderView())
        return false;

    IntRect searchRect = IntRect(IntPoint(), corePage()->mainFrame().view()->contentsSize());
    searchRect.intersect(IntRect(IntPoint(), IntSize(primarySnapshottedPlugInSearchLimit, primarySnapshottedPlugInSearchLimit)));

    IntRect plugInRectRelativeToView = plugInImageElement.clientRect();
    if (plugInRectRelativeToView.isEmpty())
        return false;
    ScrollPosition scrollPosition = mainFrame.view()->documentScrollPositionRelativeToViewOrigin();
    IntRect plugInRectRelativeToTopDocument(plugInRectRelativeToView.location() + toIntSize(scrollPosition), plugInRectRelativeToView.size());

    return plugInRectRelativeToTopDocument.intersects(searchRect);
}

bool WebPage::plugInIsPrimarySize(WebCore::HTMLPlugInImageElement& plugInImageElement, unsigned& candidatePlugInArea)
{
    auto& pluginRenderBox = downcast<RenderBox>(*plugInImageElement.renderer());
    if (pluginRenderBox.contentWidth() < primarySnapshottedPlugInMinimumWidth || pluginRenderBox.contentHeight() < primarySnapshottedPlugInMinimumHeight)
        return false;

    LayoutUnit contentArea = pluginRenderBox.contentWidth() * pluginRenderBox.contentHeight();
    if (contentArea > candidatePlugInArea * primarySnapshottedPlugInSearchBucketSize) {
        candidatePlugInArea = contentArea;
        return true;
    }
    return false;
}
#endif // ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)

PassRefPtr<Range> WebPage::currentSelectionAsRange()
{
    Frame* frame = frameWithSelection(m_page.get());
    if (!frame)
        return 0;

    return frame->selection().toNormalizedRange();
}

void WebPage::reportUsedFeatures()
{
    Vector<String> namedFeatures;
    m_loaderClient.featuresUsedInPage(this, namedFeatures);
}

unsigned WebPage::extendIncrementalRenderingSuppression()
{
    unsigned token = m_maximumRenderingSuppressionToken + 1;
    while (!HashSet<unsigned>::isValidValue(token) || m_activeRenderingSuppressionTokens.contains(token))
        token++;

    m_activeRenderingSuppressionTokens.add(token);
    m_page->mainFrame().view()->setVisualUpdatesAllowedByClient(false);

    m_maximumRenderingSuppressionToken = token;

    return token;
}

void WebPage::stopExtendingIncrementalRenderingSuppression(unsigned token)
{
    if (!m_activeRenderingSuppressionTokens.remove(token))
        return;

    m_page->mainFrame().view()->setVisualUpdatesAllowedByClient(!shouldExtendIncrementalRenderingSuppression());
}
    
void WebPage::setScrollPinningBehavior(uint32_t pinning)
{
    m_scrollPinningBehavior = static_cast<ScrollPinningBehavior>(pinning);
    m_page->mainFrame().view()->setScrollPinningBehavior(m_scrollPinningBehavior);
}

void WebPage::setScrollbarOverlayStyle(WTF::Optional<uint32_t> scrollbarStyle)
{
    if (scrollbarStyle)
        m_scrollbarOverlayStyle = static_cast<ScrollbarOverlayStyle>(scrollbarStyle.value());
    else
        m_scrollbarOverlayStyle = WTF::Optional<ScrollbarOverlayStyle>();
    m_page->mainFrame().view()->recalculateScrollbarOverlayStyle();
}

Ref<DocumentLoader> WebPage::createDocumentLoader(Frame& frame, const ResourceRequest& request, const SubstituteData& substituteData)
{
    Ref<WebDocumentLoader> documentLoader = WebDocumentLoader::create(request, substituteData);

    if (frame.isMainFrame()) {
        if (m_pendingNavigationID) {
            documentLoader->setNavigationID(m_pendingNavigationID);
            m_pendingNavigationID = 0;
        }
    }

    return WTFMove(documentLoader);
}

void WebPage::updateCachedDocumentLoader(WebDocumentLoader& documentLoader, Frame& frame)
{
    if (m_pendingNavigationID && frame.isMainFrame()) {
        documentLoader.setNavigationID(m_pendingNavigationID);
        m_pendingNavigationID = 0;
    }
}

void WebPage::getBytecodeProfile(uint64_t callbackID)
{
    if (!JSDOMWindow::commonVM().m_perBytecodeProfiler) {
        send(Messages::WebPageProxy::StringCallback(String(), callbackID));
        return;
    }

    String result = JSDOMWindow::commonVM().m_perBytecodeProfiler->toJSON();
    ASSERT(result.length());
    send(Messages::WebPageProxy::StringCallback(result, callbackID));
}

PassRefPtr<WebCore::Range> WebPage::rangeFromEditingRange(WebCore::Frame& frame, const EditingRange& range, EditingRangeIsRelativeTo editingRangeIsRelativeTo)
{
    ASSERT(range.location != notFound);

    // Sanitize the input, because TextIterator::rangeFromLocationAndLength takes signed integers.
    if (range.location > INT_MAX)
        return 0;
    int length;
    if (range.length <= INT_MAX && range.location + range.length <= INT_MAX)
        length = static_cast<int>(range.length);
    else
        length = INT_MAX - range.location;

    if (editingRangeIsRelativeTo == EditingRangeIsRelativeTo::EditableRoot) {
        // Our critical assumption is that this code path is called by input methods that
        // concentrate on a given area containing the selection.
        // We have to do this because of text fields and textareas. The DOM for those is not
        // directly in the document DOM, so serialization is problematic. Our solution is
        // to use the root editable element of the selection start as the positional base.
        // That fits with AppKit's idea of an input context.
        return TextIterator::rangeFromLocationAndLength(frame.selection().rootEditableElementOrDocumentElement(), static_cast<int>(range.location), length);
    }

    ASSERT(editingRangeIsRelativeTo == EditingRangeIsRelativeTo::Paragraph);

    const VisibleSelection& selection = frame.selection().selection();
    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return 0;

    RefPtr<Range> paragraphRange = makeRange(startOfParagraph(selection.visibleStart()), selection.visibleEnd());
    if (!paragraphRange)
        return 0;

    ContainerNode& rootNode = paragraphRange.get()->startContainer().treeScope().rootNode();
    int paragraphStartIndex = TextIterator::rangeLength(Range::create(rootNode.document(), &rootNode, 0, &paragraphRange->startContainer(), paragraphRange->startOffset()).ptr());
    return TextIterator::rangeFromLocationAndLength(&rootNode, paragraphStartIndex + static_cast<int>(range.location), length);
}
    
void WebPage::didChangeScrollOffsetForFrame(Frame* frame)
{
    if (!frame->isMainFrame())
        return;

    // If this is called when tearing down a FrameView, the WebCore::Frame's
    // current FrameView will be null.
    if (!frame->view())
        return;

    updateMainFrameScrollOffsetPinning();
}

void WebPage::postMessage(const String& messageName, API::Object* messageBody)
{
    send(Messages::WebPageProxy::HandleMessage(messageName, UserData(WebProcess::singleton().transformObjectsToHandles(messageBody))));
}

void WebPage::postSynchronousMessageForTesting(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    UserData returnUserData;

    auto& webProcess = WebProcess::singleton();
    if (!sendSync(Messages::WebPageProxy::HandleSynchronousMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))), Messages::WebPageProxy::HandleSynchronousMessage::Reply(returnUserData), std::chrono::milliseconds::max(), IPC::SyncMessageSendFlags::UseFullySynchronousModeForTesting))
        returnData = nullptr;
    else
        returnData = webProcess.transformHandlesToObjects(returnUserData.object());
}

void WebPage::clearWheelEventTestTrigger()
{
    if (!m_page)
        return;

    m_page->clearTrigger();
}

void WebPage::setShouldScaleViewToFitDocument(bool shouldScaleViewToFitDocument)
{
    if (!m_drawingArea)
        return;

    m_drawingArea->setShouldScaleViewToFitDocument(shouldScaleViewToFitDocument);
}

void WebPage::imageOrMediaDocumentSizeChanged(const IntSize& newSize)
{
    send(Messages::WebPageProxy::ImageOrMediaDocumentSizeChanged(newSize));
}

void WebPage::addUserScript(const String& source, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserScriptInjectionTime injectionTime)
{
    WebCore::UserScript userScript(source, WebCore::blankURL(), Vector<String>(), Vector<String>(), injectionTime, injectedFrames);

    m_userContentController->userContentController().addUserScript(mainThreadNormalWorld(), std::make_unique<WebCore::UserScript>(userScript));
}

void WebPage::addUserStyleSheet(const String& source, WebCore::UserContentInjectedFrames injectedFrames)
{
    WebCore::UserStyleSheet userStyleSheet(source, WebCore::blankURL(), Vector<String>(), Vector<String>(), injectedFrames, UserStyleUserLevel);

    m_userContentController->userContentController().addUserStyleSheet(mainThreadNormalWorld(), std::make_unique<WebCore::UserStyleSheet>(userStyleSheet), InjectInExistingDocuments);
}

void WebPage::removeAllUserContent()
{
    m_userContentController->userContentController().removeAllUserContent();
}

void WebPage::dispatchDidLayout(WebCore::LayoutMilestones milestones)
{
    RefPtr<API::Object> userData;
    injectedBundleLoaderClient().didLayout(this, milestones, userData);

    // Clients should not set userData for this message, and it won't be passed through.
    ASSERT(!userData);

    // The drawing area might want to defer dispatch of didLayout to the UI process.
    if (m_drawingArea && m_drawingArea->dispatchDidLayout(milestones))
        return;

    send(Messages::WebPageProxy::DidLayout(milestones));
}

void WebPage::didRestoreScrollPosition()
{
    send(Messages::WebPageProxy::DidRestoreScrollPosition());
}

} // namespace WebKit
