/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "AssistedNodeInformation.h"
#include "DataReference.h"
#include "DragControllerAction.h"
#include "DrawingArea.h"
#include "DrawingAreaMessages.h"
#include "EditorState.h"
#include "EventDispatcher.h"
#include "FindController.h"
#include "FormDataReference.h"
#include "GeolocationPermissionRequestManager.h"
#include "InjectUserScriptImmediately.h"
#include "InjectedBundle.h"
#include "InjectedBundleBackForwardList.h"
#include "InjectedBundleScriptWorld.h"
#include "LibWebRTCProvider.h"
#include "LoadParameters.h"
#include "Logging.h"
#include "NetscapePlugin.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NotificationPermissionRequestManager.h"
#include "PageBanner.h"
#include "PluginProcessAttributes.h"
#include "PluginProxy.h"
#include "PluginView.h"
#include "PrintInfo.h"
#include "RemoteWebInspectorUI.h"
#include "RemoteWebInspectorUIMessages.h"
#include "SessionState.h"
#include "SessionStateConversion.h"
#include "SessionTracker.h"
#include "ShareSheetCallbackID.h"
#include "ShareableBitmap.h"
#include "SharedBufferDataReference.h"
#include "UserMediaPermissionRequestManager.h"
#include "ViewGestureGeometryCollector.h"
#include "VisitedLinkTableController.h"
#include "WKBundleAPICast.h"
#include "WKRetainPtr.h"
#include "WKSharedAPICast.h"
#include "WebAlternativeTextClient.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListProxy.h"
#include "WebCacheStorageProvider.h"
#include "WebChromeClient.h"
#include "WebColorChooser.h"
#include "WebContextMenu.h"
#include "WebContextMenuClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebDataListSuggestionPicker.h"
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
#include "WebGamepadProvider.h"
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
#include "WebPaymentCoordinator.h"
#include "WebPerformanceLoggingClient.h"
#include "WebPlugInClient.h"
#include "WebPluginInfoProvider.h"
#include "WebPopupMenu.h"
#include "WebPreferencesDefinitions.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebProgressTrackerClient.h"
#include "WebSocketProvider.h"
#include "WebStorageNamespaceProvider.h"
#include "WebURLSchemeHandlerProxy.h"
#include "WebUndoStep.h"
#include "WebUserContentController.h"
#include "WebUserMediaClient.h"
#include "WebValidationMessageClient.h"
#include "WebsiteDataStoreParameters.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ProfilerDatabase.h>
#include <JavaScriptCore/SamplingProfiler.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/Chrome.h>
#include <WebCore/CommonVM.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/DataTransfer.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/Editing.h>
#include <WebCore/Editor.h>
#include <WebCore/ElementIterator.h>
#include <WebCore/EventHandler.h>
#include <WebCore/EventNames.h>
#include <WebCore/File.h>
#include <WebCore/FocusController.h>
#include <WebCore/FontAttributeChanges.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext3D.h>
#include <WebCore/HTMLAttachmentElement.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLImageElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLMenuElement.h>
#include <WebCore/HTMLMenuItemElement.h>
#include <WebCore/HTMLOListElement.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTMLPlugInImageElement.h>
#include <WebCore/HTMLUListElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/InspectorController.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PingLoader.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PrintContext.h>
#include <WebCore/PromisedAttachmentInfo.h>
#include <WebCore/Range.h>
#include <WebCore/RemoteDOMWindow.h>
#include <WebCore/RemoteFrame.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerProvider.h>
#include <WebCore/Settings.h>
#include <WebCore/ShadowRoot.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/StyleProperties.h>
#include <WebCore/SubframeLoader.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/UserInputBridge.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/UserTypingGestureIndicator.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/VisibleUnits.h>
#include <WebCore/WebGLStateTracker.h>
#include <WebCore/markup.h>
#include <pal/SessionID.h>
#include <wtf/ProcessID.h>
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/text/TextStream.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResult.h"
#endif

#if ENABLE(MHTML)
#include <WebCore/MHTMLArchive.h>
#endif

#if ENABLE(POINTER_LOCK)
#include <WebCore/PointerLockController.h>
#endif

#if PLATFORM(COCOA)
#include "PDFPlugin.h"
#include "PlaybackSessionManager.h"
#include "RemoteLayerTreeTransaction.h"
#include "TouchBarMenuData.h"
#include "TouchBarMenuItemData.h"
#include "VideoFullscreenManager.h"
#include "WKStringCF.h"
#include <WebCore/LegacyWebArchive.h>
#include <wtf/MachSendRight.h>
#endif

#if PLATFORM(GTK)
#include "WebPrintOperationGtk.h"
#include "WebSelectionData.h"
#include <gtk/gtk.h>
#endif

#if PLATFORM(IOS)
#include "RemoteLayerTreeDrawingArea.h"
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/Icon.h>
#include <pal/spi/cocoa/CoreTextSPI.h>
#endif

#if PLATFORM(MAC)
#include <WebCore/LocalDefaultSystemAppearance.h>
#include <pal/spi/cf/CFUtilitiesSPI.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if ENABLE(DATA_DETECTION)
#include <WebCore/DataDetection.h>
#endif

#if ENABLE(VIDEO) && USE(GSTREAMER)
#include <WebCore/MediaPlayerRequestInstallMissingPluginsCallback.h>
#endif

#if ENABLE(WEB_AUTHN)
#include "WebAuthenticatorCoordinator.h"
#include <WebCore/AuthenticatorCoordinator.h>
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

static const Seconds pageScrollHysteresisDuration { 300_ms };
static const Seconds initialLayerVolatilityTimerInterval { 20_ms };
static const Seconds maximumLayerVolatilityTimerInterval { 2_s };

#define RELEASE_LOG_IF_ALLOWED(...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Layers, __VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), Layers, __VA_ARGS__)

class SendStopResponsivenessTimer {
public:
    ~SendStopResponsivenessTimer()
    {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopResponsivenessTimer(), 0);
    }
};

class DeferredPageDestructor {
public:
    static void createDeferredPageDestructor(std::unique_ptr<Page> page, WebPage* webPage)
    {
        new DeferredPageDestructor(WTFMove(page), webPage);
    }

private:
    DeferredPageDestructor(std::unique_ptr<Page> page, WebPage* webPage)
        : m_page(WTFMove(page))
        , m_webPage(webPage)
    {
        tryDestruction();
    }

    void tryDestruction()
    {
        if (m_page->insideNestedRunLoop()) {
            m_page->whenUnnested([this] { tryDestruction(); });
            return;
        }

        m_page = nullptr;
        m_webPage = nullptr;
        delete this;
    }

    std::unique_ptr<Page> m_page;
    RefPtr<WebPage> m_webPage;
};

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageCounter, ("WebPage"));

Ref<WebPage> WebPage::create(uint64_t pageID, WebPageCreationParameters&& parameters)
{
    Ref<WebPage> page = adoptRef(*new WebPage(pageID, WTFMove(parameters)));

    if (page->pageGroup()->isVisibleToInjectedBundle() && WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->didCreatePage(page.ptr());

    return page;
}

WebPage::WebPage(uint64_t pageID, WebPageCreationParameters&& parameters)
    : m_pageID(pageID)
    , m_viewSize(parameters.viewSize)
    , m_alwaysShowsHorizontalScroller { parameters.alwaysShowsHorizontalScroller }
    , m_alwaysShowsVerticalScroller { parameters.alwaysShowsVerticalScroller }
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    , m_determinePrimarySnapshottedPlugInTimer(RunLoop::main(), this, &WebPage::determinePrimarySnapshottedPlugInTimerFired)
#endif
    , m_layerHostingMode(parameters.layerHostingMode)
#if PLATFORM(COCOA)
    , m_viewGestureGeometryCollector(makeUniqueRef<ViewGestureGeometryCollector>(*this))
#elif HAVE(ACCESSIBILITY) && PLATFORM(GTK)
    , m_accessibilityObject(nullptr)
#endif
    , m_setCanStartMediaTimer(RunLoop::main(), this, &WebPage::setCanStartMediaTimerFired)
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuClient(std::make_unique<API::InjectedBundle::PageContextMenuClient>())
#endif
    , m_editorClient { std::make_unique<API::InjectedBundle::EditorClient>() }
    , m_formClient(std::make_unique<API::InjectedBundle::FormClient>())
    , m_loaderClient(std::make_unique<API::InjectedBundle::PageLoaderClient>())
    , m_resourceLoadClient(std::make_unique<API::InjectedBundle::ResourceLoadClient>())
    , m_uiClient(std::make_unique<API::InjectedBundle::PageUIClient>())
    , m_findController(makeUniqueRef<FindController>(this))
    , m_userContentController(WebUserContentController::getOrCreate(parameters.userContentControllerID))
#if ENABLE(GEOLOCATION)
    , m_geolocationPermissionRequestManager(makeUniqueRef<GeolocationPermissionRequestManager>(*this))
#endif
#if ENABLE(MEDIA_STREAM)
    , m_userMediaPermissionRequestManager { std::make_unique<UserMediaPermissionRequestManager>(*this) }
#endif
    , m_pageScrolledHysteresis([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) pageStoppedScrolling(); }, pageScrollHysteresisDuration)
    , m_canRunBeforeUnloadConfirmPanel(parameters.canRunBeforeUnloadConfirmPanel)
    , m_canRunModal(parameters.canRunModal)
#if PLATFORM(IOS)
    , m_forceAlwaysUserScalable(parameters.ignoresViewportScaleLimits)
    , m_screenSize(parameters.screenSize)
    , m_availableScreenSize(parameters.availableScreenSize)
    , m_overrideScreenSize(parameters.overrideScreenSize)
#endif
    , m_layerVolatilityTimer(*this, &WebPage::layerVolatilityTimerFired)
    , m_activityState(parameters.activityState)
    , m_processSuppressionEnabled(true)
    , m_userActivity("Process suppression disabled for page.")
    , m_userActivityHysteresis([this](PAL::HysteresisState) { updateUserActivity(); })
    , m_userInterfaceLayoutDirection(parameters.userInterfaceLayoutDirection)
    , m_overrideContentSecurityPolicy { parameters.overrideContentSecurityPolicy }
    , m_cpuLimit(parameters.cpuLimit)
{
    ASSERT(m_pageID);

    m_pageGroup = WebProcess::singleton().webPageGroup(parameters.pageGroupData);

#if PLATFORM(IOS)
    DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

    PageConfiguration pageConfiguration(
        makeUniqueRef<WebEditorClient>(this),
        WebSocketProvider::create(),
        makeUniqueRef<WebKit::LibWebRTCProvider>(),
        WebProcess::singleton().cacheStorageProvider()
    );
    pageConfiguration.chromeClient = new WebChromeClient(*this);
#if ENABLE(CONTEXT_MENUS)
    pageConfiguration.contextMenuClient = new WebContextMenuClient(this);
#endif
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
    pageConfiguration.diagnosticLoggingClient = std::make_unique<WebDiagnosticLoggingClient>(*this);
    pageConfiguration.performanceLoggingClient = std::make_unique<WebPerformanceLoggingClient>(*this);

    pageConfiguration.webGLStateTracker = std::make_unique<WebGLStateTracker>([this](bool isUsingHighPerformanceWebGL) {
        send(Messages::WebPageProxy::SetIsUsingHighPerformanceWebGL(isUsingHighPerformanceWebGL));
    });

#if PLATFORM(COCOA)
    pageConfiguration.validationMessageClient = std::make_unique<WebValidationMessageClient>(*this);
#endif

    pageConfiguration.applicationCacheStorage = &WebProcess::singleton().applicationCacheStorage();
    pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroup->pageGroupID());
    pageConfiguration.pluginInfoProvider = &WebPluginInfoProvider::singleton();
    pageConfiguration.storageNamespaceProvider = WebStorageNamespaceProvider::getOrCreate(m_pageGroup->pageGroupID());
    pageConfiguration.userContentProvider = m_userContentController.ptr();
    pageConfiguration.visitedLinkStore = VisitedLinkTableController::getOrCreate(parameters.visitedLinkTableID);

#if ENABLE(APPLE_PAY)
    pageConfiguration.paymentCoordinatorClient = new WebPaymentCoordinator(*this);
#endif

#if ENABLE(WEB_AUTHN)
    pageConfiguration.authenticatorCoordinatorClient = std::make_unique<WebAuthenticatorCoordinator>(*this);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    pageConfiguration.applicationManifest = parameters.applicationManifest;
#endif

    m_page = std::make_unique<Page>(WTFMove(pageConfiguration));
    updatePreferences(parameters.store);

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
    m_drawingArea->updatePreferences(parameters.store);

    setBackgroundExtendsBeyondPage(parameters.backgroundExtendsBeyondPage);

#if ENABLE(GEOLOCATION)
    WebCore::provideGeolocationTo(m_page.get(), *new WebGeolocationClient(*this));
#endif
#if ENABLE(NOTIFICATIONS)
    WebCore::provideNotification(m_page.get(), new WebNotificationClient(this));
#endif
#if ENABLE(MEDIA_STREAM)
    WebCore::provideUserMediaTo(m_page.get(), new WebUserMediaClient(*this));
#endif

    m_page->setControlledByAutomation(parameters.controlledByAutomation);

#if ENABLE(REMOTE_INSPECTOR)
    m_page->setRemoteInspectionAllowed(parameters.allowsRemoteInspection);
    m_page->setRemoteInspectionNameOverride(parameters.remoteInspectionNameOverride);
#endif

    m_page->setCanStartMedia(false);
    m_mayStartMediaWhenInWindow = parameters.mayStartMediaWhenInWindow;

    m_page->setGroupName(m_pageGroup->identifier());
    m_page->setDeviceScaleFactor(parameters.deviceScaleFactor);
    m_page->setUserInterfaceLayoutDirection(m_userInterfaceLayoutDirection);
#if PLATFORM(IOS)
    m_page->setTextAutosizingWidth(parameters.textAutosizingWidth);
#endif

    platformInitialize();

    setUseFixedLayout(parameters.useFixedLayout);

    setDrawsBackground(parameters.drawsBackground);

    setUnderlayColor(parameters.underlayColor);

    setPaginationMode(parameters.paginationMode);
    setPaginationBehavesLikeColumns(parameters.paginationBehavesLikeColumns);
    setPageLength(parameters.pageLength);
    setGapBetweenPages(parameters.gapBetweenPages);
    setPaginationLineGridEnabled(parameters.paginationLineGridEnabled);
#if PLATFORM(MAC)
    setUseSystemAppearance(parameters.useSystemAppearance);
    setUseDarkAppearance(parameters.useDarkAppearance);
#endif
    // If the page is created off-screen, its visibilityState should be prerender.
    m_page->setActivityState(m_activityState);
    if (!isVisible())
        m_page->setIsPrerender();

    updateIsInWindow(true);

    setViewLayoutSize(parameters.viewLayoutSize);
    setAutoSizingShouldExpandToViewHeight(parameters.autoSizingShouldExpandToViewHeight);
    setViewportSizeForCSSViewportUnits(parameters.viewportSizeForCSSViewportUnits);
    
    setScrollPinningBehavior(parameters.scrollPinningBehavior);
    if (parameters.scrollbarOverlayStyle)
        m_scrollbarOverlayStyle = static_cast<ScrollbarOverlayStyle>(parameters.scrollbarOverlayStyle.value());
    else
        m_scrollbarOverlayStyle = std::optional<ScrollbarOverlayStyle>();

    setTopContentInset(parameters.topContentInset);

    m_userAgent = parameters.userAgent;
    
    if (!parameters.itemStates.isEmpty())
        restoreSessionInternal(parameters.itemStates, WasRestoredByAPIRequest::No, WebBackForwardListProxy::OverwriteExistingItem::Yes);

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
    webProcess.addMessageReceiver(Messages::WebInspector::messageReceiverName(), m_pageID, *this);
    webProcess.addMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_pageID, *this);
    webProcess.addMessageReceiver(Messages::RemoteWebInspectorUI::messageReceiverName(), m_pageID, *this);
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


#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (WebMediaKeyStorageManager* manager = webProcess.supplement<WebMediaKeyStorageManager>())
        m_page->settings().setMediaKeysStorageDirectory(manager->mediaKeyStorageDirectory());
#endif
    m_page->settings().setAppleMailPaginationQuirkEnabled(parameters.appleMailPaginationQuirkEnabled);
    
    if (parameters.viewScaleFactor != 1)
        scaleView(parameters.viewScaleFactor);

    m_page->addLayoutMilestones(parameters.observedLayoutMilestones);

#if PLATFORM(COCOA)
    m_page->settings().setContentDispositionAttachmentSandboxEnabled(true);
    setSmartInsertDeleteEnabled(parameters.smartInsertDeleteEnabled);
#endif

#if ENABLE(SERVICE_WORKER)
    if (parameters.hasRegisteredServiceWorkers)
        ServiceWorkerProvider::singleton().setMayHaveRegisteredServiceWorkers();
#endif

    m_needsFontAttributes = parameters.needsFontAttributes;

#if ENABLE(WEB_RTC)
    if (!parameters.iceCandidateFilteringEnabled)
        disableICECandidateFiltering();
#if USE(LIBWEBRTC)
    if (parameters.enumeratingAllNetworkInterfacesEnabled)
        enableEnumeratingAllNetworkInterfaces();
#endif
#endif

    for (auto iterator : parameters.urlSchemeHandlers)
        registerURLSchemeHandler(iterator.value, iterator.key);

    m_userContentController->addUserContentWorlds(parameters.userContentWorlds);
    m_userContentController->addUserScripts(WTFMove(parameters.userScripts), InjectUserScriptImmediately::No);
    m_userContentController->addUserStyleSheets(parameters.userStyleSheets);
    m_userContentController->addUserScriptMessageHandlers(parameters.messageHandlers);
#if ENABLE(CONTENT_EXTENSIONS)
    m_userContentController->addContentRuleLists(parameters.contentRuleLists);
#endif

#if PLATFORM(IOS)
    setViewportConfigurationViewLayoutSize(parameters.viewportConfigurationViewLayoutSize);
    setMaximumUnobscuredSize(parameters.maximumUnobscuredSize);
#endif
}

#if ENABLE(WEB_RTC)
void WebPage::disableICECandidateFiltering()
{
    m_page->disableICECandidateFiltering();
}

void WebPage::enableICECandidateFiltering()
{
    m_page->enableICECandidateFiltering();
}

#if USE(LIBWEBRTC)
void WebPage::disableEnumeratingAllNetworkInterfaces()
{
    m_page->libWebRTCProvider().disableEnumeratingAllNetworkInterfaces();
}

void WebPage::enableEnumeratingAllNetworkInterfaces()
{
    m_page->libWebRTCProvider().enableEnumeratingAllNetworkInterfaces();
}
#endif
#endif

void WebPage::reinitializeWebPage(WebPageCreationParameters&& parameters)
{
    if (!m_drawingArea) {
        m_drawingArea = DrawingArea::create(*this, parameters);
        m_drawingArea->setPaintingEnabled(false);
        m_drawingArea->setShouldScaleViewToFitDocument(parameters.shouldScaleViewToFitDocument);
        m_drawingArea->updatePreferences(parameters.store);
        m_drawingArea->setPaintingEnabled(true);
    }

    if (m_activityState != parameters.activityState)
        setActivityState(parameters.activityState, ActivityStateChangeAsynchronous, Vector<CallbackID>());
    if (m_layerHostingMode != parameters.layerHostingMode)
        setLayerHostingMode(parameters.layerHostingMode);
}

void WebPage::updateThrottleState()
{
    bool isActive = m_activityState.containsAny({ ActivityState::IsLoading, ActivityState::IsAudible, ActivityState::IsCapturingMedia, ActivityState::WindowIsActive });
    bool isVisuallyIdle = m_activityState.contains(ActivityState::IsVisuallyIdle);
    bool pageSuppressed = m_processSuppressionEnabled && !isActive && isVisuallyIdle;

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if (!pageSuppressed) {
        // App nap must be manually enabled when not running the NSApplication run loop.
        static std::once_flag onceKey;
        std::call_once(onceKey, [] {
            __CFRunLoopSetOptionsReason(__CFRunLoopOptionsEnableAppNap, CFSTR("Finished checkin as application - enable app nap"));
        });
    }
#endif
    // The UserActivity keeps the processes runnable. So if the page should be suppressed, stop the activity.
    // If the page should not be supressed, start it.
    if (pageSuppressed)
        m_userActivityHysteresis.stop();
    else
        m_userActivityHysteresis.start();
}

void WebPage::updateUserActivity()
{
    if (m_userActivityHysteresis.state() == PAL::HysteresisState::Started)
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
    webProcess.removeMessageReceiver(Messages::WebInspector::messageReceiverName(), m_pageID);
    webProcess.removeMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_pageID);
    webProcess.removeMessageReceiver(Messages::RemoteWebInspectorUI::messageReceiverName(), m_pageID);
#if ENABLE(FULLSCREEN_API)
    webProcess.removeMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_pageID);
#endif

#ifndef NDEBUG
    webPageCounter.decrement();
#endif
    
#if (PLATFORM(IOS) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (m_playbackSessionManager)
        m_playbackSessionManager->invalidate();

    if (m_videoFullscreenManager)
        m_videoFullscreenManager->invalidate();
#endif
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
void WebPage::setInjectedBundleContextMenuClient(std::unique_ptr<API::InjectedBundle::PageContextMenuClient>&& contextMenuClient)
{
    if (!contextMenuClient) {
        m_contextMenuClient = std::make_unique<API::InjectedBundle::PageContextMenuClient>();
        return;
    }

    m_contextMenuClient = WTFMove(contextMenuClient);
}
#endif

void WebPage::setInjectedBundleEditorClient(std::unique_ptr<API::InjectedBundle::EditorClient>&& editorClient)
{
    if (!editorClient) {
        m_editorClient = std::make_unique<API::InjectedBundle::EditorClient>();
        return;
    }

    m_editorClient = WTFMove(editorClient);
}

void WebPage::setInjectedBundleFormClient(std::unique_ptr<API::InjectedBundle::FormClient>&& formClient)
{
    if (!formClient) {
        m_formClient = std::make_unique<API::InjectedBundle::FormClient>();
        return;
    }

    m_formClient = WTFMove(formClient);
}

void WebPage::setInjectedBundlePageLoaderClient(std::unique_ptr<API::InjectedBundle::PageLoaderClient>&& loaderClient)
{
    if (!loaderClient) {
        m_loaderClient = std::make_unique<API::InjectedBundle::PageLoaderClient>();
        return;
    }

    m_loaderClient = WTFMove(loaderClient);

    // It would be nice to get rid of this code and transition all clients to using didLayout instead of
    // didFirstLayoutInFrame and didFirstVisuallyNonEmptyLayoutInFrame. In the meantime, this is required
    // for backwards compatibility.
    if (auto milestones = m_loaderClient->layoutMilestones())
        listenForLayoutMilestones(milestones);
}

void WebPage::initializeInjectedBundlePolicyClient(WKBundlePagePolicyClientBase* client)
{
    m_policyClient.initialize(client);
}

void WebPage::setInjectedBundleResourceLoadClient(std::unique_ptr<API::InjectedBundle::ResourceLoadClient>&& client)
{
    if (!m_resourceLoadClient)
        m_resourceLoadClient = std::make_unique<API::InjectedBundle::ResourceLoadClient>();
    else
        m_resourceLoadClient = WTFMove(client);
}

void WebPage::setInjectedBundleUIClient(std::unique_ptr<API::InjectedBundle::PageUIClient>&& uiClient)
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

#if ENABLE(NETSCAPE_PLUGIN_API)
RefPtr<Plugin> WebPage::createPlugin(WebFrame* frame, HTMLPlugInElement* pluginElement, const Plugin::Parameters& parameters, String& newMIMEType)
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
    bool isUnsupported;
    if (!sendSync(Messages::WebPageProxy::FindPlugin(parameters.mimeType, static_cast<uint32_t>(processType), parameters.url.string(), frameURLString, pageURLString, allowOnlyApplicationPlugins), Messages::WebPageProxy::FindPlugin::Reply(pluginProcessToken, newMIMEType, pluginLoadPolicy, unavailabilityDescription, isUnsupported)))
        return nullptr;

    PluginModuleLoadPolicy loadPolicy = static_cast<PluginModuleLoadPolicy>(pluginLoadPolicy);
    bool isBlockedPlugin = (loadPolicy == PluginModuleBlockedForSecurity) || (loadPolicy == PluginModuleBlockedForCompatibility);

    if (isUnsupported || isBlockedPlugin || !pluginProcessToken) {
#if ENABLE(PDFKIT_PLUGIN)
        String path = parameters.url.path();
        if (shouldUsePDFPlugin() && (MIMETypeRegistry::isPDFOrPostScriptMIMEType(parameters.mimeType) || (parameters.mimeType.isEmpty() && (path.endsWithIgnoringASCIICase(".pdf") || path.endsWithIgnoringASCIICase(".ps")))))
            return PDFPlugin::create(*frame);
#endif
    }

    if (isUnsupported) {
        pluginElement->setReplacement(RenderEmbeddedObject::UnsupportedPlugin, unavailabilityDescription);
        return nullptr;
    }

    if (isBlockedPlugin) {
        bool isReplacementObscured = pluginElement->setReplacement(RenderEmbeddedObject::InsecurePluginVersion, unavailabilityDescription);
        send(Messages::WebPageProxy::DidBlockInsecurePluginVersion(parameters.mimeType, parameters.url.string(), frameURLString, pageURLString, isReplacementObscured));
        return nullptr;
    }

    if (!pluginProcessToken)
        return nullptr;

    bool isRestartedProcess = (pluginElement->displayState() == HTMLPlugInElement::Restarting || pluginElement->displayState() == HTMLPlugInElement::RestartingWithPendingMouseClick);
    return PluginProxy::create(pluginProcessToken, isRestartedProcess);
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL) && !PLATFORM(MAC)
WebCore::WebGLLoadPolicy WebPage::webGLPolicyForURL(WebFrame*, const URL&)
{
    return WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebPage::resolveWebGLPolicyForURL(WebFrame*, const URL&)
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
    const Editor& editor = frame.editor();

    result.selectionIsNone = selection.isNone();
    result.selectionIsRange = selection.isRange();
    result.isContentEditable = selection.isContentEditable();
    result.isContentRichlyEditable = selection.isContentRichlyEditable();
    result.isInPasswordField = selection.isInPasswordField();
    result.hasComposition = editor.hasComposition();
    result.shouldIgnoreSelectionChanges = editor.ignoreSelectionChanges();

    if (auto* document = frame.document())
        result.originIdentifierForPasteboard = document->originIdentifierForPasteboard();

    bool canIncludePostLayoutData = frame.view() && !frame.view()->needsLayout();
    if (shouldIncludePostLayoutData == IncludePostLayoutDataHint::Yes && canIncludePostLayoutData) {
        auto& postLayoutData = result.postLayoutData();
        postLayoutData.canCut = editor.canCut();
        postLayoutData.canCopy = editor.canCopy();
        postLayoutData.canPaste = editor.canPaste();

        if (m_needsFontAttributes)
            postLayoutData.fontAttributes = editor.fontAttributesAtSelectionStart();

#if PLATFORM(COCOA)
        if (result.isContentEditable && !selection.isNone()) {
            if (auto editingStyle = EditingStyle::styleAtSelectionStart(selection)) {
                if (editingStyle->hasStyle(CSSPropertyFontWeight, "bold"))
                    postLayoutData.typingAttributes |= AttributeBold;

                if (editingStyle->hasStyle(CSSPropertyFontStyle, "italic") || editingStyle->hasStyle(CSSPropertyFontStyle, "oblique"))
                    postLayoutData.typingAttributes |= AttributeItalics;

                if (editingStyle->hasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"))
                    postLayoutData.typingAttributes |= AttributeUnderline;

                if (auto* styleProperties = editingStyle->style()) {
                    bool isLeftToRight = styleProperties->propertyAsValueID(CSSPropertyDirection) == CSSValueLtr;
                    switch (styleProperties->propertyAsValueID(CSSPropertyTextAlign)) {
                    case CSSValueRight:
                    case CSSValueWebkitRight:
                        postLayoutData.textAlignment = RightAlignment;
                        break;
                    case CSSValueLeft:
                    case CSSValueWebkitLeft:
                        postLayoutData.textAlignment = LeftAlignment;
                        break;
                    case CSSValueCenter:
                    case CSSValueWebkitCenter:
                        postLayoutData.textAlignment = CenterAlignment;
                        break;
                    case CSSValueJustify:
                        postLayoutData.textAlignment = JustifiedAlignment;
                        break;
                    case CSSValueStart:
                        postLayoutData.textAlignment = isLeftToRight ? LeftAlignment : RightAlignment;
                        break;
                    case CSSValueEnd:
                        postLayoutData.textAlignment = isLeftToRight ? RightAlignment : LeftAlignment;
                        break;
                    default:
                        break;
                    }
                    if (auto textColor = styleProperties->propertyAsColor(CSSPropertyColor))
                        postLayoutData.textColor = *textColor;
                }
            }

            if (auto* enclosingListElement = enclosingList(selection.start().containerNode())) {
                if (is<HTMLUListElement>(*enclosingListElement))
                    postLayoutData.enclosingListType = UnorderedList;
                else if (is<HTMLOListElement>(*enclosingListElement))
                    postLayoutData.enclosingListType = OrderedList;
                else
                    ASSERT_NOT_REACHED();
            }
        }
#endif
    }

    platformEditorState(frame, result, shouldIncludePostLayoutData);

    m_lastEditorStateWasContentEditable = result.isContentEditable ? EditorStateIsContentEditable::Yes : EditorStateIsContentEditable::No;

    return result;
}

void WebPage::changeFontAttributes(WebCore::FontAttributeChanges&& changes)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().selection().isContentEditable())
        frame.editor().applyStyleToSelection(changes.createEditingStyle(), changes.editAction(), Editor::ColorFilterMode::InvertColor);
}

void WebPage::changeFont(WebCore::FontChanges&& changes)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().selection().isContentEditable())
        frame.editor().applyStyleToSelection(changes.createEditingStyle(), EditAction::SetFont, Editor::ColorFilterMode::InvertColor);
}

void WebPage::executeEditCommandWithCallback(const String& commandName, const String& argument, CallbackID callbackID)
{
    executeEditCommand(commandName, argument);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::updateEditorStateAfterLayoutIfEditabilityChanged()
{
    // FIXME: We should update EditorStateIsContentEditable to track whether the state is richly
    // editable or plainttext-only.
    if (m_lastEditorStateWasContentEditable == EditorStateIsContentEditable::Unset)
        return;

    Frame& frame = m_page->focusController().focusedOrMainFrame();
    EditorStateIsContentEditable editorStateIsContentEditable = frame.selection().selection().isContentEditable() ? EditorStateIsContentEditable::Yes : EditorStateIsContentEditable::No;
    if (m_lastEditorStateWasContentEditable != editorStateIsContentEditable)
        sendPartialEditorStateAndSchedulePostLayoutUpdate();
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
    if (!is<PluginDocument>(frame.document()))
        return nullptr;

    auto& pluginDocument = downcast<PluginDocument>(*frame.document());
    if (pluginDocument.focusedElement() != pluginDocument.pluginElement())
        return nullptr;

    return pluginViewForFrame(&frame);
}

PluginView* WebPage::pluginViewForFrame(Frame* frame)
{
    if (!frame || !is<PluginDocument>(frame->document()))
        return nullptr;

    auto& document = downcast<PluginDocument>(*frame->document());
    return static_cast<PluginView*>(document.pluginWidget());
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
    if (m_drawingArea)
        m_drawingArea->setRootCompositingLayer(0);
}

void WebPage::close()
{
    if (m_isClosed)
        return;

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveStorageAccessForAllFramesOnPage(sessionID(), m_pageID), 0);
    
    m_isClosed = true;

    // If there is still no URL, then we never loaded anything in this page, so nothing to report.
    if (!mainWebFrame()->url().isEmpty()) {
        reportUsedFeatures();

        WebProcess::singleton().sendPrewarmInformation(toRegistrableDomain(mainWebFrame()->url()));
    }

    if (pageGroup()->isVisibleToInjectedBundle() && WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->willDestroyPage(this);

    if (m_inspector) {
        m_inspector->disconnectFromPage();
        m_inspector = nullptr;
    }

    m_page->inspectorController().disconnectAllFrontends();

#if ENABLE(MEDIA_STREAM)
    m_userMediaPermissionRequestManager = nullptr;
#endif

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
    m_editorClient = std::make_unique<API::InjectedBundle::EditorClient>();
    m_formClient = std::make_unique<API::InjectedBundle::FormClient>();
    m_loaderClient = std::make_unique<API::InjectedBundle::PageLoaderClient>();
    m_policyClient.initialize(0);
    m_resourceLoadClient = std::make_unique<API::InjectedBundle::ResourceLoadClient>();
    m_uiClient = std::make_unique<API::InjectedBundle::PageUIClient>();
#if ENABLE(FULLSCREEN_API)
    m_fullScreenClient.initialize(0);
#endif

    m_printContext = nullptr;
    m_mainFrame->coreFrame()->loader().detachFromParent();
    m_drawingArea = nullptr;

    DeferredPageDestructor::createDeferredPageDestructor(WTFMove(m_page), this);

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
    SendStopResponsivenessTimer stopper;

    if (!corePage()->userInputBridge().tryClosePage())
        return;

    send(Messages::WebPageProxy::ClosePage(true));
}

void WebPage::sendClose()
{
    send(Messages::WebPageProxy::ClosePage(false));
}

void WebPage::loadURLInFrame(WebCore::URL&& url, uint64_t frameID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    frame->coreFrame()->loader().load(FrameLoadRequest(*frame->coreFrame(), ResourceRequest(url), ShouldOpenExternalURLsPolicy::ShouldNotAllow));
}

#if !PLATFORM(COCOA)
void WebPage::platformDidReceiveLoadParameters(const LoadParameters& loadParameters)
{
}
#endif

void WebPage::loadRequest(LoadParameters&& loadParameters)
{
    SendStopResponsivenessTimer stopper;

    m_pendingNavigationID = loadParameters.navigationID;

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), WTFMove(loadParameters.sandboxExtensionHandle));

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient->willLoadURLRequest(*this, loadParameters.request, WebProcess::singleton().transformHandlesToObjects(loadParameters.userData.object()).get());

    platformDidReceiveLoadParameters(loadParameters);

    // Initate the load in WebCore.
    FrameLoadRequest frameLoadRequest { *m_mainFrame->coreFrame(), loadParameters.request, ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    ShouldOpenExternalURLsPolicy externalURLsPolicy = static_cast<ShouldOpenExternalURLsPolicy>(loadParameters.shouldOpenExternalURLsPolicy);
    frameLoadRequest.setShouldOpenExternalURLsPolicy(externalURLsPolicy);
    frameLoadRequest.setShouldTreatAsContinuingLoad(loadParameters.shouldTreatAsContinuingLoad);

    corePage()->userInputBridge().loadRequest(WTFMove(frameLoadRequest));

    ASSERT(!m_pendingNavigationID);
}

void WebPage::loadDataImpl(uint64_t navigationID, Ref<SharedBuffer>&& sharedBuffer, const String& MIMEType, const String& encodingName, const URL& baseURL, const URL& unreachableURL, const UserData& userData)
{
    SendStopResponsivenessTimer stopper;

    m_pendingNavigationID = navigationID;

    ResourceRequest request(baseURL);
    ResourceResponse response(URL(), MIMEType, sharedBuffer->size(), encodingName);
    SubstituteData substituteData(WTFMove(sharedBuffer), unreachableURL, response, SubstituteData::SessionHistoryVisibility::Hidden);

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient->willLoadDataRequest(*this, request, const_cast<SharedBuffer*>(substituteData.content()), substituteData.mimeType(), substituteData.textEncoding(), substituteData.failingURL(), WebProcess::singleton().transformHandlesToObjects(userData.object()).get());

    // Initate the load in WebCore.
    m_mainFrame->coreFrame()->loader().load(FrameLoadRequest(*m_mainFrame->coreFrame(), request, ShouldOpenExternalURLsPolicy::ShouldNotAllow, substituteData));
}

void WebPage::loadData(LoadParameters&& loadParameters)
{
    platformDidReceiveLoadParameters(loadParameters);

    auto sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(loadParameters.data.data()), loadParameters.data.size());
    URL baseURL = loadParameters.baseURLString.isEmpty() ? blankURL() : URL(URL(), loadParameters.baseURLString);
    loadDataImpl(loadParameters.navigationID, WTFMove(sharedBuffer), loadParameters.MIMEType, loadParameters.encodingName, baseURL, URL(), loadParameters.userData);
}

void WebPage::loadAlternateHTML(const LoadParameters& loadParameters)
{
    platformDidReceiveLoadParameters(loadParameters);

    URL baseURL = loadParameters.baseURLString.isEmpty() ? blankURL() : URL(URL(), loadParameters.baseURLString);
    URL unreachableURL = loadParameters.unreachableURLString.isEmpty() ? URL() : URL(URL(), loadParameters.unreachableURLString);
    URL provisionalLoadErrorURL = loadParameters.provisionalLoadErrorURLString.isEmpty() ? URL() : URL(URL(), loadParameters.provisionalLoadErrorURLString);
    auto sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(loadParameters.data.data()), loadParameters.data.size());
    m_mainFrame->coreFrame()->loader().setProvisionalLoadErrorBeingHandledURL(provisionalLoadErrorURL);    
    loadDataImpl(loadParameters.navigationID, WTFMove(sharedBuffer), loadParameters.MIMEType, loadParameters.encodingName, baseURL, unreachableURL, loadParameters.userData);
    m_mainFrame->coreFrame()->loader().setProvisionalLoadErrorBeingHandledURL({ });
}

void WebPage::navigateToPDFLinkWithSimulatedClick(const String& url, IntPoint documentPoint, IntPoint screenPoint)
{
    Frame* mainFrame = m_mainFrame->coreFrame();
    Document* mainFrameDocument = mainFrame->document();
    if (!mainFrameDocument)
        return;

    const int singleClick = 1;
    // FIXME: Set modifier keys.
    // FIXME: This should probably set IsSimulated::Yes.
    RefPtr<MouseEvent> mouseEvent = MouseEvent::create(eventNames().clickEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, Event::IsComposed::Yes,
        MonotonicTime::now(), nullptr, singleClick, screenPoint, documentPoint, { }, { }, 0, 0, nullptr, 0, WebCore::NoTap, nullptr);

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
    SendStopResponsivenessTimer stopper;

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

void WebPage::reload(uint64_t navigationID, uint32_t reloadOptions, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper;

    ASSERT(!m_mainFrame->coreFrame()->loader().frameHasLoaded() || !m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    m_sandboxExtensionTracker.beginReload(m_mainFrame.get(), WTFMove(sandboxExtensionHandle));
    corePage()->userInputBridge().reloadFrame(m_mainFrame->coreFrame(), OptionSet<ReloadOption>::fromRaw(reloadOptions));

    if (m_pendingNavigationID) {
        // This can happen if FrameLoader::reload() returns early because the document URL is empty.
        // The reload does nothing so we need to reset the pending navigation. See webkit.org/b/153210.
        m_pendingNavigationID = 0;
    }
}

void WebPage::goToBackForwardItem(uint64_t navigationID, const BackForwardItemIdentifier& backForwardItemID, FrameLoadType backForwardType, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    SendStopResponsivenessTimer stopper;

    ASSERT(isBackForwardLoadType(backForwardType));

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    LOG(Loading, "In WebProcess pid %i, WebPage %" PRIu64 " is navigating to back/forward URL %s", getCurrentProcessID(), m_pageID, item->url().string().utf8().data());

    ASSERT(!m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    m_page->goToItem(*item, backForwardType, shouldTreatAsContinuingLoad);
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
    return &static_cast<WebChromeClient&>(page->chrome().client()).page();
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    if (m_viewSize == viewSize)
        return;

    m_viewSize = viewSize;
    FrameView* view = m_page->mainFrame().view();
    view->resize(viewSize);
    m_drawingArea->setNeedsDisplay();

#if USE(COORDINATED_GRAPHICS)
    if (view->useFixedLayout())
        sendViewportAttributesChanged(m_page->viewportArguments());
#endif
}

#if USE(COORDINATED_GRAPHICS)
void WebPage::sendViewportAttributesChanged(const ViewportArguments& viewportArguments)
{
    FrameView* view = m_page->mainFrame().view();
    ASSERT(view && view->useFixedLayout());

    // Viewport properties have no impact on zero sized fixed viewports.
    if (m_viewSize.isEmpty())
        return;

    // Recalculate the recommended layout size, when the available size (device pixel) changes.
    Settings& settings = m_page->settings();

    int minimumLayoutFallbackWidth = std::max(settings.layoutFallbackWidth(), m_viewSize.width());

    // If unset  we use the viewport dimensions. This fits with the behavior of desktop browsers.
    int deviceWidth = (settings.deviceWidth() > 0) ? settings.deviceWidth() : m_viewSize.width();
    int deviceHeight = (settings.deviceHeight() > 0) ? settings.deviceHeight() : m_viewSize.height();

    ViewportAttributes attr = computeViewportAttributes(viewportArguments, minimumLayoutFallbackWidth, deviceWidth, deviceHeight, 1, m_viewSize);

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
    view->setFixedVisibleContentRect(IntRect(contentFixedOrigin, roundedIntSize(contentFixedSize)));

    attr.initialScale = m_page->viewportArguments().zoom; // Resets auto (-1) if no value was set by user.

    // This also takes care of the relayout.
    setFixedLayoutSize(roundedIntSize(attr.layoutSize));

#if USE(COORDINATED_GRAPHICS_THREADED)
    m_drawingArea->didChangeViewportAttributes(WTFMove(attr));
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
#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(m_page->useSystemAppearance(), m_page->useDarkAppearance());
#endif

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
    m_drawingArea->deviceOrPageScaleFactorChanged();
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
    scrollPositionAtNewScale.scale(scaleRatio);
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
        scrollPositionAtNewScale.scale(scaleRatio);
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

    if (findController().isShowingOverlay()) {
        // We must have updated layout to get the selection rects right.
        layoutIfNeeded();
        findController().deviceScaleFactorDidChange();
    }

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    m_drawingArea->deviceOrPageScaleFactorChanged();
#endif
}

float WebPage::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

void WebPage::accessibilitySettingsDidChange()
{
    m_page->accessibilitySettingsDidChange();
}

#if ENABLE(ACCESSIBILITY_EVENTS)
void WebPage::updateAccessibilityEventsEnabled(bool enabled)
{
    m_page->settings().setAccessibilityEventsEnabled(enabled);
}
#endif

void WebPage::setUseFixedLayout(bool fixed)
{
    // Do not overwrite current settings if initially setting it to false.
    if (m_useFixedLayout == fixed)
        return;
    m_useFixedLayout = fixed;

#if !PLATFORM(IOS)
    m_page->settings().setFixedElementsLayoutRelativeToFrame(fixed);
#endif

    FrameView* view = mainFrameView();
    if (!view)
        return;

    view->setUseFixedLayout(fixed);
    if (!fixed)
        setFixedLayoutSize(IntSize());

    send(Messages::WebPageProxy::UseFixedLayoutDidChange(fixed));
}

bool WebPage::setFixedLayoutSize(const IntSize& size)
{
    FrameView* view = mainFrameView();
    if (!view || view->fixedLayoutSize() == size)
        return false;

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_pageID << " setFixedLayoutSize " << size);
    view->setFixedLayoutSize(size);

    send(Messages::WebPageProxy::FixedLayoutSizeDidChange(size));
    return true;
}

IntSize WebPage::fixedLayoutSize() const
{
    FrameView* view = mainFrameView();
    if (!view)
        return IntSize();
    return view->fixedLayoutSize();
}

void WebPage::disabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations)
{
#if PLATFORM(IOS)
    if (m_viewportConfiguration.setDisabledAdaptations(disabledAdaptations))
        viewportConfigurationChanged();
#else
    UNUSED_PARAM(disabledAdaptations);
#endif
}

void WebPage::viewportPropertiesDidChange(const ViewportArguments& viewportArguments)
{
#if PLATFORM(IOS)
    if (!m_page->settings().shouldIgnoreMetaViewport() && m_viewportConfiguration.setViewportArguments(viewportArguments))
        viewportConfigurationChanged();
#endif

#if USE(COORDINATED_GRAPHICS)
    FrameView* view = m_page->mainFrame().view();
    if (view && view->useFixedLayout())
        sendViewportAttributesChanged(viewportArguments);
#if USE(COORDINATED_GRAPHICS_THREADED)
    else
        m_drawingArea->didChangeViewportAttributes(ViewportAttributes());
#endif
#endif

#if !PLATFORM(IOS) && !USE(COORDINATED_GRAPHICS)
    UNUSED_PARAM(viewportArguments);
#endif
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

void WebPage::setHeaderPageBanner(PageBanner* pageBanner)
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

void WebPage::setFooterPageBanner(PageBanner* pageBanner)
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

void WebPage::setHeaderBannerHeightForTesting(int height)
{
#if ENABLE(RUBBER_BANDING)
    corePage()->addHeaderWithHeight(height);
#endif
}

void WebPage::setFooterBannerHeightForTesting(int height)
{
#if ENABLE(RUBBER_BANDING)
    corePage()->addFooterWithHeight(height);
#endif
}

#endif // !PLATFORM(IOS)

void WebPage::takeSnapshot(IntRect snapshotRect, IntSize bitmapSize, uint32_t options, CallbackID callbackID)
{
    SnapshotOptions snapshotOptions = static_cast<SnapshotOptions>(options);
    snapshotOptions |= SnapshotOptionsShareable;

    RefPtr<WebImage> image = snapshotAtSize(snapshotRect, bitmapSize, snapshotOptions);

    ShareableBitmap::Handle handle;
    if (image)
        image->bitmap().createHandle(handle, SharedMemory::Protection::ReadOnly);

    send(Messages::WebPageProxy::ImageCallback(handle, callbackID));
}

RefPtr<WebImage> WebPage::scaledSnapshotWithOptions(const IntRect& rect, double additionalScaleFactor, SnapshotOptions options)
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

static void paintSnapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options, Frame& frame, FrameView& frameView, GraphicsContext& graphicsContext)
{
    IntRect snapshotRect = rect;
    float horizontalScaleFactor = static_cast<float>(bitmapSize.width()) / rect.width();
    float verticalScaleFactor = static_cast<float>(bitmapSize.height()) / rect.height();
    float scaleFactor = std::max(horizontalScaleFactor, verticalScaleFactor);

    if (options & SnapshotOptionsPrinting) {
        PrintContext::spoolAllPagesWithBoundaries(frame, graphicsContext, snapshotRect.size());
        return;
    }

    Color documentBackgroundColor = frameView.documentBackgroundColor();
    Color backgroundColor = (frame.settings().backgroundShouldExtendBeyondPage() && documentBackgroundColor.isValid()) ? documentBackgroundColor : frameView.baseBackgroundColor();
    graphicsContext.fillRect(IntRect(IntPoint(), bitmapSize), backgroundColor);

    if (!(options & SnapshotOptionsExcludeDeviceScaleFactor)) {
        double deviceScaleFactor = frame.page()->deviceScaleFactor();
        graphicsContext.applyDeviceScaleFactor(deviceScaleFactor);
        scaleFactor /= deviceScaleFactor;
    }

    graphicsContext.scale(scaleFactor);
    graphicsContext.translate(-snapshotRect.location());

    FrameView::SelectionInSnapshot shouldPaintSelection = FrameView::IncludeSelection;
    if (options & SnapshotOptionsExcludeSelectionHighlighting)
        shouldPaintSelection = FrameView::ExcludeSelection;

    FrameView::CoordinateSpaceForSnapshot coordinateSpace = FrameView::DocumentCoordinates;
    if (options & SnapshotOptionsInViewCoordinates)
        coordinateSpace = FrameView::ViewCoordinates;

    frameView.paintContentsForSnapshot(graphicsContext, snapshotRect, shouldPaintSelection, coordinateSpace);

    if (options & SnapshotOptionsPaintSelectionRectangle) {
        FloatRect selectionRectangle = frame.selection().selectionBounds();
        graphicsContext.setStrokeColor(Color(0xFF, 0, 0));
        graphicsContext.strokeRect(selectionRectangle, 1);
    }
}

static ShareableBitmap::Configuration snapshotOptionsToBitmapConfiguration(SnapshotOptions options, WebPage& page)
{
    ShareableBitmap::Configuration configuration;
#if USE(CG)
    if (options & SnapshotOptionsUseScreenColorSpace)
        configuration.colorSpace.cgColorSpace = screenColorSpace(page.corePage()->mainFrame().view());
#endif
    return configuration;
}

RefPtr<WebImage> WebPage::snapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options)
{
    Frame* coreFrame = m_mainFrame->coreFrame();
    if (!coreFrame)
        return nullptr;

    FrameView* frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    auto snapshot = WebImage::create(bitmapSize, snapshotOptionsToImageOptions(options), snapshotOptionsToBitmapConfiguration(options, *this));
    if (!snapshot)
        return nullptr;
    auto graphicsContext = snapshot->bitmap().createGraphicsContext();

    paintSnapshotAtSize(rect, bitmapSize, options, *coreFrame, *frameView, *graphicsContext);

    return snapshot;
}

#if USE(CF)
RetainPtr<CFDataRef> WebPage::pdfSnapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options)
{
    Frame* coreFrame = m_mainFrame->coreFrame();
    if (!coreFrame)
        return nullptr;

    FrameView* frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    auto data = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));

#if USE(CG)
    auto dataConsumer = adoptCF(CGDataConsumerCreateWithCFData(data.get()));
    auto mediaBox = CGRectMake(0, 0, bitmapSize.width(), bitmapSize.height());
    auto pdfContext = adoptCF(CGPDFContextCreate(dataConsumer.get(), &mediaBox, nullptr));

    CGPDFContextBeginPage(pdfContext.get(), nullptr);

    GraphicsContext graphicsContext { pdfContext.get() };
    graphicsContext.scale({ 1, -1 });
    graphicsContext.translate(0, -bitmapSize.height());
    paintSnapshotAtSize(rect, bitmapSize, options, *coreFrame, *frameView, graphicsContext);

    CGPDFContextEndPage(pdfContext.get());
    CGPDFContextClose(pdfContext.get());
#endif

    return WTFMove(data);
}
#endif

RefPtr<WebImage> WebPage::snapshotNode(WebCore::Node& node, SnapshotOptions options, unsigned maximumPixelCount)
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

    auto snapshot = WebImage::create(snapshotSize, snapshotOptionsToImageOptions(options), snapshotOptionsToBitmapConfiguration(options, *this));
    if (!snapshot)
        return nullptr;
    auto graphicsContext = snapshot->bitmap().createGraphicsContext();

    if (!(options & SnapshotOptionsExcludeDeviceScaleFactor)) {
        double deviceScaleFactor = corePage()->deviceScaleFactor();
        graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
        scaleFactor /= deviceScaleFactor;
    }

    graphicsContext->scale(scaleFactor);
    graphicsContext->translate(-snapshotRect.location());

    Color savedBackgroundColor = frameView->baseBackgroundColor();
    frameView->setBaseBackgroundColor(Color::transparent);
    frameView->setNodeToDraw(&node);

    frameView->paintContentsForSnapshot(*graphicsContext, snapshotRect, FrameView::ExcludeSelection, FrameView::DocumentCoordinates);

    frameView->setBaseBackgroundColor(savedBackgroundColor);
    frameView->setNodeToDraw(nullptr);

    return snapshot;
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
    PlatformMouseEvent mousePressEvent(point, point, RightButton, PlatformEvent::MousePressed, 1, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, WebCore::NoTap);
    corePage()->userInputBridge().handleMousePressEvent(mousePressEvent);
    bool handled = corePage()->userInputBridge().handleContextMenuEvent(mousePressEvent, corePage()->mainFrame());
    auto* menu = handled ? contextMenu() : nullptr;
    PlatformMouseEvent mouseReleaseEvent(point, point, RightButton, PlatformEvent::MouseReleased, 1, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, WebCore::NoTap);
    corePage()->userInputBridge().handleMouseReleaseEvent(mouseReleaseEvent);

    return menu;
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

    drawingArea->setLayerTreeStateIsFrozen(frozen || m_isSuspended);
}

void WebPage::callVolatilityCompletionHandlers(bool succeeded)
{
    auto completionHandlers = WTFMove(m_markLayersAsVolatileCompletionHandlers);
    for (auto& completionHandler : completionHandlers)
        completionHandler(succeeded);
}

void WebPage::layerVolatilityTimerFired()
{
    Seconds newInterval = m_layerVolatilityTimer.repeatInterval() * 2.;
    bool didSucceed = markLayersVolatileImmediatelyIfPossible();
    if (didSucceed || newInterval > maximumLayerVolatilityTimerInterval) {
        m_layerVolatilityTimer.stop();
        if (didSucceed)
            RELEASE_LOG_IF_ALLOWED("%p - WebPage - Succeeded in marking layers as volatile", this);
        else
            RELEASE_LOG_IF_ALLOWED("%p - WebPage - Failed to mark layers as volatile within %gms", this, maximumLayerVolatilityTimerInterval.milliseconds());
        callVolatilityCompletionHandlers(didSucceed);
        return;
    }

    RELEASE_LOG_ERROR_IF_ALLOWED("%p - WebPage - Failed to mark all layers as volatile, will retry in %g ms", this, newInterval.milliseconds());
    m_layerVolatilityTimer.startRepeating(newInterval);
}

bool WebPage::markLayersVolatileImmediatelyIfPossible()
{
    return !drawingArea() || drawingArea()->markLayersVolatileImmediatelyIfPossible();
}

void WebPage::markLayersVolatile(WTF::Function<void (bool)>&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("%p - WebPage::markLayersVolatile()", this);

    if (m_layerVolatilityTimer.isActive())
        m_layerVolatilityTimer.stop();

    if (completionHandler)
        m_markLayersAsVolatileCompletionHandlers.append(WTFMove(completionHandler));

    bool didSucceed = markLayersVolatileImmediatelyIfPossible();
    if (didSucceed || m_isSuspendedUnderLock) {
        if (didSucceed)
            RELEASE_LOG_IF_ALLOWED("%p - WebPage - Successfully marked layers as volatile", this);
        else {
            // If we get suspended when locking the screen, it is expected that some IOSurfaces cannot be marked as purgeable so we do not keep retrying.
            RELEASE_LOG_IF_ALLOWED("%p - WebPage - Did what we could to mark IOSurfaces as purgeable after locking the screen", this);
        }
        callVolatilityCompletionHandlers(didSucceed);
        return;
    }

    RELEASE_LOG_IF_ALLOWED("%p - Failed to mark all layers as volatile, will retry in %g ms", this, initialLayerVolatilityTimerInterval.milliseconds());
    m_layerVolatilityTimer.startRepeating(initialLayerVolatilityTimerInterval);
}

void WebPage::cancelMarkLayersVolatile()
{
    RELEASE_LOG_IF_ALLOWED("%p - WebPage::cancelMarkLayersVolatile()", this);
    m_layerVolatilityTimer.stop();
    m_markLayersAsVolatileCompletionHandlers.clear();
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

    bool handled = page->corePage()->userInputBridge().handleContextMenuEvent(platformMouseEvent, *frame);
    if (handled)
        page->contextMenu()->show();

    return handled;
}

void WebPage::contextMenuForKeyEvent()
{
    corePage()->contextMenuController().clearContextMenu();

    Frame& frame = m_page->focusController().focusedOrMainFrame();
    bool handled = frame.eventHandler().sendContextMenuEventForKey();
    if (handled)
        contextMenu()->show();
}
#endif

static bool handleMouseEvent(const WebMouseEvent& mouseEvent, WebPage* page)
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
#if PLATFORM(COCOA)
            // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
            // button is currently pressed. It is possible that neither of those things will be true since on
            // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one
            // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
            // efficient scrollbars-only version of the event.
            if (!(page->corePage()->focusController().isActive() || (mouseEvent.button() != WebMouseEvent::NoButton)))
                return page->corePage()->userInputBridge().handleMouseMoveOnScrollbarEvent(platformMouseEvent);
#endif
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
    SetForScope<bool> userIsInteractingChange { m_userIsInteracting, true };

    m_userActivityHysteresis.impulse();

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

    if (!handled) {
        CurrentEvent currentEvent(mouseEvent);
        handled = handleMouseEvent(mouseEvent, this);
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
    m_userActivityHysteresis.impulse();

    CurrentEvent currentEvent(wheelEvent);

    bool handled = handleWheelEvent(wheelEvent, m_page.get());

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
    SetForScope<bool> userIsInteractingChange { m_userIsInteracting, true };

    m_userActivityHysteresis.impulse();

    CurrentEvent currentEvent(keyboardEvent);

    bool handled = handleKeyEvent(keyboardEvent, m_page.get());
    // FIXME: Platform default behaviors should be performed during normal DOM event dispatch (in most cases, in default keydown event handler).
    if (!handled)
        handled = performDefaultBehaviorForKeyEvent(keyboardEvent);

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(keyboardEvent.type()), handled));
}

void WebPage::validateCommand(const String& commandName, CallbackID callbackID)
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

void WebPage::setNeedsFontAttributes(bool needsFontAttributes)
{
    if (m_needsFontAttributes == needsFontAttributes)
        return;

    m_needsFontAttributes = needsFontAttributes;

    if (m_needsFontAttributes)
        sendPartialEditorStateAndSchedulePostLayoutUpdate();
}

void WebPage::restoreSessionInternal(const Vector<BackForwardListItemState>& itemStates, WasRestoredByAPIRequest restoredByAPIRequest, WebBackForwardListProxy::OverwriteExistingItem overwrite)
{
    for (const auto& itemState : itemStates) {
        auto historyItem = toHistoryItem(itemState);
        historyItem->setWasRestoredFromSession(restoredByAPIRequest == WasRestoredByAPIRequest::Yes);
        static_cast<WebBackForwardListProxy*>(corePage()->backForward().client())->addItemFromUIProcess(itemState.identifier, WTFMove(historyItem), m_pageID, overwrite);
    }
}

void WebPage::restoreSession(const Vector<BackForwardListItemState>& itemStates)
{
    restoreSessionInternal(itemStates, WasRestoredByAPIRequest::Yes, WebBackForwardListProxy::OverwriteExistingItem::No);
}

void WebPage::updateBackForwardListForReattach(const Vector<WebKit::BackForwardListItemState>& itemStates)
{
    restoreSessionInternal(itemStates, WasRestoredByAPIRequest::No, WebBackForwardListProxy::OverwriteExistingItem::Yes);
}

void WebPage::requestFontAttributesAtSelectionStart(CallbackID callbackID)
{
    auto attributes = m_page->focusController().focusedOrMainFrame().editor().fontAttributesAtSelectionStart();
    send(Messages::WebPageProxy::FontAttributesCallback(attributes, callbackID));
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
    SetForScope<bool> userIsInteractingChange { m_userIsInteracting, true };

    m_lastInteractionLocation = touchEvent.position();
    CurrentEvent currentEvent(touchEvent);
    handled = handleTouchEvent(touchEvent, m_page.get());
    updatePotentialTapSecurityOrigin(touchEvent, handled);
}

void WebPage::touchEventSync(const WebTouchEvent& touchEvent, bool& handled)
{
    EventDispatcher::TouchEventQueue queuedEvents;
    WebProcess::singleton().eventDispatcher().getQueuedTouchEventsForPage(*this, queuedEvents);
    dispatchAsynchronousTouchEvents(queuedEvents);

    dispatchTouchEvent(touchEvent, handled);
}

void WebPage::updatePotentialTapSecurityOrigin(const WebTouchEvent& touchEvent, bool wasHandled)
{
    if (wasHandled)
        return;

    if (!touchEvent.isPotentialTap())
        return;

    if (touchEvent.type() != WebEvent::TouchStart)
        return;

    auto& mainFrame = m_page->mainFrame();
    auto document = mainFrame.document();
    if (!document)
        return;

    if (!document->handlingTouchEvent())
        return;

    Frame* touchEventTargetFrame = &mainFrame;
    while (auto subframe = touchEventTargetFrame->eventHandler().touchEventTargetSubframe())
        touchEventTargetFrame = subframe;

    auto& touches = touchEventTargetFrame->eventHandler().touches();
    if (touches.isEmpty())
        return;

    ASSERT(touches.size() == 1);

    if (auto targetDocument = touchEventTargetFrame->document())
        m_potentialTapSecurityOrigin = &targetDocument->securityOrigin();
}
#elif ENABLE(TOUCH_EVENTS)
void WebPage::touchEvent(const WebTouchEvent& touchEvent)
{
    CurrentEvent currentEvent(touchEvent);

    bool handled = handleTouchEvent(touchEvent, m_page.get());

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
    CurrentEvent currentEvent(gestureEvent);
    bool handled = handleGestureEvent(gestureEvent, m_page.get());
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
    frame.selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignCenterAlways);
    findController().showFindIndicatorInSelection();
}

bool WebPage::isControlledByAutomation() const
{
    return m_page->isControlledByAutomation();
}

void WebPage::setControlledByAutomation(bool controlled)
{
    m_page->setControlledByAutomation(controlled);
}

void WebPage::insertNewlineInQuotedContent()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().isNone())
        return;
    frame.editor().insertParagraphSeparatorInQuotedContent();
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

    if (FrameView* frameView = mainFrameView()) {
        Color backgroundColor = drawsBackground ? Color::white : Color::transparent;
        bool isTransparent = !drawsBackground;
        frameView->updateBackgroundRecursively(backgroundColor, isTransparent);
    }

    m_drawingArea->pageBackgroundTransparencyChanged();
    m_drawingArea->setNeedsDisplay();
}

#if PLATFORM(COCOA)
void WebPage::setTopContentInsetFenced(float contentInset, IPC::Attachment fencePort)
{
    if (fencePort.disposition() != MACH_MSG_TYPE_MOVE_SEND) {
        LOG(Layers, "WebPage::setTopContentInsetFenced(%g, fencePort) Received an invalid fence port: %d, disposition: %d", contentInset, fencePort.port(), fencePort.disposition());
        return;
    }

    m_drawingArea->addFence(MachSendRight::create(fencePort.port()));

    setTopContentInset(contentInset);

    deallocateSendRightSafely(fencePort.port());
}
#endif

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

void WebPage::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& event, CallbackID callbackID)
{
    if (!m_page)
        return;

    SetForScope<bool> userIsInteractingChange { m_userIsInteracting, true };

    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.document()->setFocusedElement(0);

    if (isKeyboardEventValid && event.type() == WebEvent::KeyDown) {
        PlatformKeyboardEvent platformEvent(platform(event));
        platformEvent.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown);
        m_page->focusController().setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, &KeyboardEvent::create(platformEvent, &frame.windowProxy()).get());

        send(Messages::WebPageProxy::VoidCallback(callbackID));
        return;
    }

    m_page->focusController().setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, nullptr);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::setCanStartMediaTimerFired()
{
    if (m_page)
        m_page->setCanStartMedia(true);
}

void WebPage::updateIsInWindow(bool isInitialState)
{
    bool isInWindow = m_activityState.contains(WebCore::ActivityState::IsInWindow);

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
            m_setCanStartMediaTimer.startOneShot(0_s);

        WebProcess::singleton().pageDidEnterWindow(m_pageID);
    }

    if (isInWindow)
        layoutIfNeeded();
}

void WebPage::visibilityDidChange()
{
    bool isVisible = m_activityState.contains(ActivityState::IsVisible);
    if (!isVisible) {
        // We save the document / scroll state when backgrounding a tab so that we are able to restore it
        // if it gets terminated while in the background.
        if (auto* frame = m_mainFrame->coreFrame())
            frame->loader().history().saveDocumentAndScrollState();
    }
}

void WebPage::setActivityState(OptionSet<ActivityState::Flag> activityState, ActivityStateChangeID activityStateChangeID, const Vector<CallbackID>& callbackIDs)
{
    LOG_WITH_STREAM(ActivityState, stream << "WebPage " << pageID() << " setActivityState to " << activityState);

    auto changed = m_activityState ^ activityState;
    m_activityState = activityState;

    if (changed)
        updateThrottleState();

    {
        SetForScope<bool> currentlyChangingActivityState { m_changingActivityState, true };
        m_page->setActivityState(activityState);
    }
    
    for (auto* pluginView : m_pluginViews)
        pluginView->activityStateDidChange(changed);

    m_drawingArea->activityStateDidChange(changed, activityStateChangeID, callbackIDs);
    WebProcess::singleton().pageActivityStateDidChange(m_pageID, changed);

    if (changed & ActivityState::IsInWindow)
        updateIsInWindow();

    if (changed & ActivityState::IsVisible)
        visibilityDidChange();
}

void WebPage::setLayerHostingMode(LayerHostingMode layerHostingMode)
{
    m_layerHostingMode = layerHostingMode;

    m_drawingArea->setLayerHostingMode(m_layerHostingMode);

    for (auto* pluginView : m_pluginViews)
        pluginView->setLayerHostingMode(m_layerHostingMode);
}

void WebPage::setSessionID(PAL::SessionID sessionID)
{
    if (sessionID.isEphemeral())
        WebProcess::singleton().addWebsiteDataStore(WebsiteDataStoreParameters::privateSessionParameters(sessionID));
    m_page->setSessionID(sessionID);
}

void WebPage::didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, PolicyAction policyAction, uint64_t navigationID, const DownloadID& downloadID, std::optional<WebsitePoliciesData>&& websitePolicies)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    if (policyAction == PolicyAction::Suspend) {
        ASSERT(frame == m_mainFrame);
        setIsSuspended(true);

        WebProcess::singleton().sendPrewarmInformation(toRegistrableDomain(mainWebFrame()->url()));
    }
    frame->didReceivePolicyDecision(listenerID, policyAction, navigationID, downloadID, WTFMove(websitePolicies));
}

void WebPage::continueWillSubmitForm(uint64_t frameID, uint64_t listenerID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    frame->continueWillSubmitForm(listenerID);
}

void WebPage::didStartPageTransition()
{
    setLayerTreeStateIsFrozen(true);

#if PLATFORM(MAC)
    bool hasPreviouslyFocusedDueToUserInteraction = m_hasEverFocusedElementDueToUserInteractionSincePageTransition;
#endif
    m_hasEverFocusedElementDueToUserInteractionSincePageTransition = false;
    m_isAssistingNodeDueToUserInteraction = false;
    m_lastEditorStateWasContentEditable = EditorStateIsContentEditable::Unset;
#if PLATFORM(MAC)
    if (hasPreviouslyFocusedDueToUserInteraction)
        send(Messages::WebPageProxy::SetHasHadSelectionChangesFromUserInteraction(m_hasEverFocusedElementDueToUserInteractionSincePageTransition));
    if (m_needsHiddenContentEditableQuirk) {
        m_needsHiddenContentEditableQuirk = false;
        send(Messages::WebPageProxy::SetNeedsHiddenContentEditableQuirk(m_needsHiddenContentEditableQuirk));
    }
    if (m_needsPlainTextQuirk) {
        m_needsPlainTextQuirk = false;
        send(Messages::WebPageProxy::SetNeedsPlainTextQuirk(m_needsPlainTextQuirk));
    }
#endif
}

void WebPage::didCompletePageTransition()
{
    // FIXME: Layer tree freezing should be managed entirely in the UI process side.
    setLayerTreeStateIsFrozen(false);

    bool isInitialEmptyDocument = !m_mainFrame;
    send(Messages::WebPageProxy::DidCompletePageTransition(isInitialEmptyDocument));
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
    if (frame) {
        String userAgent = m_loaderClient->userAgentForURL(*frame, webcoreURL);
        if (!userAgent.isEmpty())
            return userAgent;
    }

    String userAgent = platformUserAgent(webcoreURL);
    if (!userAgent.isEmpty())
        return userAgent;
    return m_userAgent;
}
    
void WebPage::setUserAgent(const String& userAgent)
{
    if (m_userAgent == userAgent)
        return;

    m_userAgent = userAgent;

    if (m_page)
        m_page->userAgentChanged();
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

void WebPage::runJavaScript(const String& script, bool forceUserGesture, std::optional<String> worldName, CallbackID callbackID)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    RefPtr<SerializedScriptValue> serializedResultValue;
    JSLockHolder lock(commonVM());
    bool hadException = true;
    ExceptionDetails details;
    auto* world = worldName ? InjectedBundleScriptWorld::find(worldName.value()) : &InjectedBundleScriptWorld::normalWorld();
    if (world) {
        if (JSValue resultValue = m_mainFrame->coreFrame()->script().executeScriptInWorld(world->coreWorld(), script, forceUserGesture, &details)) {
            hadException = false;
            serializedResultValue = SerializedScriptValue::create(m_mainFrame->jsContextForWorld(world),
                toRef(m_mainFrame->coreFrame()->script().globalObject(world->coreWorld())->globalExec(), resultValue), nullptr);
        }
    }

    IPC::DataReference dataReference;
    if (serializedResultValue)
        dataReference = serializedResultValue->data();
    send(Messages::WebPageProxy::ScriptValueCallback(dataReference, hadException, details, callbackID));
}

void WebPage::runJavaScriptInMainFrame(const String& script, bool forceUserGesture, CallbackID callbackID)
{
    runJavaScript(script, forceUserGesture, std::nullopt, callbackID);
}

void WebPage::runJavaScriptInMainFrameScriptWorld(const String& script, bool forceUserGesture, const String& worldName, CallbackID callbackID)
{
    runJavaScript(script, forceUserGesture, worldName, callbackID);
}

void WebPage::getContentsAsString(CallbackID callbackID)
{
    String resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

#if ENABLE(MHTML)
void WebPage::getContentsAsMHTMLData(CallbackID callbackID)
{
    send(Messages::WebPageProxy::DataCallback({ MHTMLArchive::generateMHTMLData(m_page.get()) }, callbackID));
}
#endif

void WebPage::getRenderTreeExternalRepresentation(CallbackID callbackID)
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

void WebPage::getSelectionAsWebArchiveData(CallbackID callbackID)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data;
    if (Frame* frame = frameWithSelection(m_page.get()))
        data = LegacyWebArchive::createFromSelection(frame)->rawDataRepresentation();
#endif

    IPC::SharedBufferDataReference dataReference;
#if PLATFORM(COCOA)
    if (data)
        dataReference = { CFDataGetBytePtr(data.get()), static_cast<size_t>(CFDataGetLength(data.get())) };
#endif
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::getSelectionOrContentsAsString(CallbackID callbackID)
{
    WebFrame* focusedOrMainFrame = WebFrame::fromCoreFrame(m_page->focusController().focusedOrMainFrame());
    String resultString = focusedOrMainFrame->selectionAsString();
    if (resultString.isEmpty())
        resultString = focusedOrMainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getSourceForFrame(uint64_t frameID, CallbackID callbackID)
{
    String resultString;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID))
       resultString = frame->source();

    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getMainResourceDataOfFrame(uint64_t frameID, CallbackID callbackID)
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

    IPC::SharedBufferDataReference dataReference;
    if (buffer)
        dataReference = { *buffer };
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

static RefPtr<SharedBuffer> resourceDataForFrame(Frame* frame, const URL& resourceURL)
{
    DocumentLoader* loader = frame->loader().documentLoader();
    if (!loader)
        return nullptr;

    RefPtr<ArchiveResource> subresource = loader->subresource(resourceURL);
    if (!subresource)
        return nullptr;

    return &subresource->data();
}

void WebPage::getResourceDataFromFrame(uint64_t frameID, const String& resourceURLString, CallbackID callbackID)
{
    RefPtr<SharedBuffer> buffer;
    if (auto* frame = WebProcess::singleton().webFrame(frameID)) {
        URL resourceURL(URL(), resourceURLString);
        buffer = resourceDataForFrame(frame->coreFrame(), resourceURL);
    }

    IPC::SharedBufferDataReference dataReference;
    if (buffer)
        dataReference = { *buffer };
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::getWebArchiveOfFrame(uint64_t frameID, CallbackID callbackID)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID))
        data = frame->webArchiveData(nullptr, nullptr);
#else
    UNUSED_PARAM(frameID);
#endif

    IPC::SharedBufferDataReference dataReference;
#if PLATFORM(COCOA)
    if (data)
        dataReference = { CFDataGetBytePtr(data.get()), static_cast<size_t>(CFDataGetLength(data.get())) };
#endif
    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::forceRepaintWithoutCallback()
{
    m_drawingArea->forceRepaint();
}

void WebPage::forceRepaint(CallbackID callbackID)
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
    updatePreferencesGenerated(store);

    Settings& settings = m_page->settings();

    bool requiresUserGestureForMedia = store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForMediaPlaybackKey());
    settings.setVideoPlaybackRequiresUserGesture(requiresUserGestureForMedia || store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForVideoPlaybackKey()));
    settings.setAudioPlaybackRequiresUserGesture(requiresUserGestureForMedia || store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForAudioPlaybackKey()));
    settings.setLayoutInterval(Seconds(store.getDoubleValueForKey(WebPreferencesKey::layoutIntervalKey())));
    settings.setUserInterfaceDirectionPolicy(static_cast<WebCore::UserInterfaceDirectionPolicy>(store.getUInt32ValueForKey(WebPreferencesKey::userInterfaceDirectionPolicyKey())));
    settings.setSystemLayoutDirection(static_cast<TextDirection>(store.getUInt32ValueForKey(WebPreferencesKey::systemLayoutDirectionKey())));
    settings.setJavaScriptRuntimeFlags(static_cast<RuntimeFlags>(store.getUInt32ValueForKey(WebPreferencesKey::javaScriptRuntimeFlagsKey())));
    settings.setStorageBlockingPolicy(static_cast<SecurityOrigin::StorageBlockingPolicy>(store.getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey())));
    settings.setFrameFlattening(store.getBoolValueForKey(WebPreferencesKey::frameFlatteningEnabledKey()) ? WebCore::FrameFlattening::FullyEnabled : WebCore::FrameFlattening::Disabled);
    settings.setEditableLinkBehavior(static_cast<WebCore::EditableLinkBehavior>(store.getUInt32ValueForKey(WebPreferencesKey::editableLinkBehaviorKey())));
#if ENABLE(DATA_DETECTION)
    settings.setDataDetectorTypes(static_cast<DataDetectorTypes>(store.getUInt32ValueForKey(WebPreferencesKey::dataDetectorTypesKey())));
#endif

    DatabaseManager::singleton().setIsAvailable(store.getBoolValueForKey(WebPreferencesKey::databasesEnabledKey()));

    m_tabToLinks = store.getBoolValueForKey(WebPreferencesKey::tabsToLinksKey());
    m_asynchronousPluginInitializationEnabled = store.getBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledKey());
    m_asynchronousPluginInitializationEnabledForAllPlugins = store.getBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledForAllPluginsKey());
    m_artificialPluginInitializationDelayEnabled = store.getBoolValueForKey(WebPreferencesKey::artificialPluginInitializationDelayEnabledKey());

    m_scrollingPerformanceLoggingEnabled = store.getBoolValueForKey(WebPreferencesKey::scrollingPerformanceLoggingEnabledKey());
    settings.setScrollingPerformanceLoggingEnabled(m_scrollingPerformanceLoggingEnabled);

    if (store.getBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey()) && !usesEphemeralSession())
        setSessionID(PAL::SessionID::legacyPrivateSessionID());
    else if (!store.getBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey()) && sessionID() == PAL::SessionID::legacyPrivateSessionID())
        setSessionID(PAL::SessionID::defaultSessionID());

    bool processSuppressionEnabled = store.getBoolValueForKey(WebPreferencesKey::pageVisibilityBasedProcessSuppressionEnabledKey());
    if (m_processSuppressionEnabled != processSuppressionEnabled) {
        m_processSuppressionEnabled = processSuppressionEnabled;
        updateThrottleState();
    }

#if PLATFORM(COCOA)
    m_pdfPluginEnabled = store.getBoolValueForKey(WebPreferencesKey::pdfPluginEnabledKey());
#endif
#if ENABLE(PAYMENT_REQUEST)
    settings.setPaymentRequestEnabled(store.getBoolValueForKey(WebPreferencesKey::applePayEnabledKey()));
#endif

    // FIXME: This is both a RuntimeEnabledFeatures (generated) and a setting. It should pick one.
    settings.setInteractiveFormValidationEnabled(store.getBoolValueForKey(WebPreferencesKey::interactiveFormValidationEnabledKey()));

#if PLATFORM(IOS)
    m_ignoreViewportScalingConstraints = store.getBoolValueForKey(WebPreferencesKey::ignoreViewportScalingConstraintsKey());
    m_viewportConfiguration.setCanIgnoreScalingConstraints(m_ignoreViewportScalingConstraints);
    setForceAlwaysUserScalable(m_forceAlwaysUserScalable || store.getBoolValueForKey(WebPreferencesKey::forceAlwaysUserScalableKey()));

    settings.setUseImageDocumentForSubframePDF(true);
#if HAVE(AVKIT)
    DeprecatedGlobalSettings::setAVKitEnabled(true);
#endif
#endif

#if ENABLE(SERVICE_WORKER)
    if (store.getBoolValueForKey(WebPreferencesKey::serviceWorkersEnabledKey())) {
        ASSERT(parentProcessHasServiceWorkerEntitlement());
        if (!parentProcessHasServiceWorkerEntitlement())
            RuntimeEnabledFeatures::sharedFeatures().setServiceWorkerEnabled(false);
    }
#endif

    settings.setLayoutViewportHeightExpansionFactor(store.getDoubleValueForKey(WebPreferencesKey::layoutViewportHeightExpansionFactorKey()));

    if (m_drawingArea)
        m_drawingArea->updatePreferences(store);
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
    FrameView* frameView = corePage()->mainFrame().view();
    if (!frameView)
        return;

    layerTransaction.setContentsSize(frameView->contentsSize());
    layerTransaction.setScrollOrigin(frameView->scrollOrigin());
    layerTransaction.setPageScaleFactor(corePage()->pageScaleFactor());
    layerTransaction.setRenderTreeSize(corePage()->renderTreeSize());
    layerTransaction.setPageExtendedBackgroundColor(corePage()->pageExtendedBackgroundColor());

    layerTransaction.setBaseLayoutViewportSize(frameView->baseLayoutViewportSize());
    layerTransaction.setMinStableLayoutViewportOrigin(frameView->minStableLayoutViewportOrigin());
    layerTransaction.setMaxStableLayoutViewportOrigin(frameView->maxStableLayoutViewportOrigin());

#if PLATFORM(IOS)
    layerTransaction.setScaleWasSetByUIProcess(scaleWasSetByUIProcess());
    layerTransaction.setMinimumScaleFactor(m_viewportConfiguration.minimumScale());
    layerTransaction.setMaximumScaleFactor(m_viewportConfiguration.maximumScale());
    layerTransaction.setInitialScaleFactor(m_viewportConfiguration.initialScale());
    layerTransaction.setViewportMetaTagWidth(m_viewportConfiguration.viewportArguments().width);
    layerTransaction.setViewportMetaTagWidthWasExplicit(m_viewportConfiguration.viewportArguments().widthWasExplicit);
    layerTransaction.setViewportMetaTagCameFromImageDocument(m_viewportConfiguration.viewportArguments().type == ViewportArguments::ImageDocument);
    layerTransaction.setAvoidsUnsafeArea(m_viewportConfiguration.avoidsUnsafeArea());
    layerTransaction.setIsInStableState(m_isInStableState);
    layerTransaction.setAllowsUserScaling(allowsUserScaling());
    if (m_pendingDynamicViewportSizeUpdateID) {
        layerTransaction.setDynamicViewportSizeUpdateID(*m_pendingDynamicViewportSizeUpdateID);
        m_pendingDynamicViewportSizeUpdateID = std::nullopt;
    }
    if (m_lastTransactionPageScaleFactor != layerTransaction.pageScaleFactor()) {
        m_lastTransactionPageScaleFactor = layerTransaction.pageScaleFactor();
        m_lastTransactionIDWithScaleChange = layerTransaction.transactionID();
    }
#endif

    layerTransaction.setScrollPosition(frameView->scrollPosition());

    if (m_hasPendingEditorStateUpdate) {
        layerTransaction.setEditorState(editorState());
        m_hasPendingEditorStateUpdate = false;
    }
}

void WebPage::didFlushLayerTreeAtTime(MonotonicTime timestamp)
{
#if PLATFORM(IOS)
    if (m_oldestNonStableUpdateVisibleContentRectsTimestamp != MonotonicTime()) {
        Seconds elapsed = timestamp - m_oldestNonStableUpdateVisibleContentRectsTimestamp;
        m_oldestNonStableUpdateVisibleContentRectsTimestamp = MonotonicTime();

        m_estimatedLatency = m_estimatedLatency * 0.80 + elapsed * 0.20;
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

RemoteWebInspectorUI* WebPage::remoteInspectorUI()
{
    if (m_isClosed)
        return nullptr;
    if (!m_remoteInspectorUI)
        m_remoteInspectorUI = RemoteWebInspectorUI::create(*this);
    return m_remoteInspectorUI.get();
}

void WebPage::inspectorFrontendCountChanged(unsigned count)
{
    send(Messages::WebPageProxy::DidChangeInspectorFrontendCount(count));
}

#if (PLATFORM(IOS) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
PlaybackSessionManager& WebPage::playbackSessionManager()
{
    if (!m_playbackSessionManager)
        m_playbackSessionManager = PlaybackSessionManager::create(*this);
    return *m_playbackSessionManager;
}

VideoFullscreenManager& WebPage::videoFullscreenManager()
{
    if (!m_videoFullscreenManager)
        m_videoFullscreenManager = VideoFullscreenManager::create(*this, playbackSessionManager());
    return *m_videoFullscreenManager;
}

void WebPage::videoControlsManagerDidChange()
{
#if ENABLE(FULLSCREEN_API)
    if (auto* manager = fullScreenManager())
        manager->videoControlsManagerDidChange();
#endif
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

void WebPage::addConsoleMessage(uint64_t frameID, MessageSource messageSource, MessageLevel messageLevel, const String& message, uint64_t requestID)
{
    if (auto* frame = WebProcess::singleton().webFrame(frameID))
        frame->addConsoleMessage(messageSource, messageLevel, message, requestID);
}

void WebPage::sendCSPViolationReport(uint64_t frameID, const WebCore::URL& reportURL, IPC::FormDataReference&& reportData)
{
    auto report = reportData.takeData();
    if (!report)
        return;
    if (auto* frame = WebProcess::singleton().webFrame(frameID))
        PingLoader::sendViolationReport(*frame->coreFrame(), reportURL, report.releaseNonNull(), ViolationReportType::ContentSecurityPolicy);
}

void WebPage::enqueueSecurityPolicyViolationEvent(uint64_t frameID, SecurityPolicyViolationEvent::Init&& eventInit)
{
    auto* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    auto* coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;
    if (auto* document = coreFrame->document())
        document->enqueueSecurityPolicyViolationEvent(WTFMove(eventInit));
}

NotificationPermissionRequestManager* WebPage::notificationPermissionRequestManager()
{
    if (m_notificationPermissionRequestManager)
        return m_notificationPermissionRequestManager.get();

    m_notificationPermissionRequestManager = NotificationPermissionRequestManager::create(this);
    return m_notificationPermissionRequestManager.get();
}

#if ENABLE(DRAG_SUPPORT)

#if PLATFORM(GTK)
void WebPage::performDragControllerAction(DragControllerAction action, const IntPoint& clientPosition, const IntPoint& globalPosition, uint64_t draggingSourceOperationMask, WebSelectionData&& selection, uint32_t flags)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(DragOperationNone, false, 0, { }));
        return;
    }

    DragData dragData(selection.selectionData.ptr(), clientPosition, globalPosition, static_cast<DragOperation>(draggingSourceOperationMask), static_cast<DragApplicationFlags>(flags));
    switch (action) {
    case DragControllerAction::Entered: {
        DragOperation resolvedDragOperation = m_page->dragController().dragEntered(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted(), { }));
        return;
    }
    case DragControllerAction::Updated: {
        DragOperation resolvedDragOperation = m_page->dragController().dragEntered(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted(), { }));
        return;
    }
    case DragControllerAction::Exited:
        m_page->dragController().dragExited(dragData);
        return;

    case DragControllerAction::PerformDragOperation: {
        m_page->dragController().performDragOperation(dragData);
        return;
    }
    }
    ASSERT_NOT_REACHED();
}
#else
void WebPage::performDragControllerAction(DragControllerAction action, const WebCore::DragData& dragData, SandboxExtension::Handle&& sandboxExtensionHandle, SandboxExtension::HandleArray&& sandboxExtensionsHandleArray)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(DragOperationNone, false, 0, { }));
        return;
    }

    switch (action) {
    case DragControllerAction::Entered: {
        DragOperation resolvedDragOperation = m_page->dragController().dragEntered(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted(), m_page->dragCaretController().caretRectInRootViewCoordinates()));
        return;
    }
    case DragControllerAction::Updated: {
        DragOperation resolvedDragOperation = m_page->dragController().dragUpdated(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(resolvedDragOperation, m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted(), m_page->dragCaretController().caretRectInRootViewCoordinates()));
        return;
    }
    case DragControllerAction::Exited:
        m_page->dragController().dragExited(dragData);
        send(Messages::WebPageProxy::DidPerformDragControllerAction(DragOperationNone, false, 0, { }));
        return;
        
    case DragControllerAction::PerformDragOperation: {
        ASSERT(!m_pendingDropSandboxExtension);

        m_pendingDropSandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
        for (size_t i = 0; i < sandboxExtensionsHandleArray.size(); i++) {
            if (RefPtr<SandboxExtension> extension = SandboxExtension::create(WTFMove(sandboxExtensionsHandleArray[i])))
                m_pendingDropExtensionsForFileUpload.append(extension);
        }

        bool handled = m_page->dragController().performDragOperation(dragData);

        // If we started loading a local file, the sandbox extension tracker would have adopted this
        // pending drop sandbox extension. If not, we'll play it safe and clear it.
        m_pendingDropSandboxExtension = nullptr;

        m_pendingDropExtensionsForFileUpload.clear();
        send(Messages::WebPageProxy::DidPerformDragOperation(handled));
        return;
    }
    }
    ASSERT_NOT_REACHED();
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
    PlatformMouseEvent event(adjustedClientPosition, adjustedGlobalPosition, LeftButton, PlatformEvent::MouseMoved, 0, false, false, false, false, WallTime::now(), 0, WebCore::NoTap);
    m_page->mainFrame().eventHandler().dragSourceEndedAt(event, (DragOperation)operation);

    send(Messages::WebPageProxy::DidEndDragging());
}

void WebPage::willPerformLoadDragDestinationAction()
{
    m_sandboxExtensionTracker.willPerformLoadDragDestinationAction(WTFMove(m_pendingDropSandboxExtension));
}

void WebPage::mayPerformUploadDragDestinationAction()
{
    for (size_t i = 0; i < m_pendingDropExtensionsForFileUpload.size(); i++)
        m_pendingDropExtensionsForFileUpload[i]->consumePermanently();
    m_pendingDropExtensionsForFileUpload.clear();
}

void WebPage::didStartDrag()
{
    m_isStartingDrag = false;
    m_page->mainFrame().eventHandler().didStartDrag();
}

void WebPage::dragCancelled()
{
    m_isStartingDrag = false;
    m_page->mainFrame().eventHandler().dragCancelled();
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

bool WebPage::isAlwaysOnLoggingAllowed() const
{
    return corePage() && corePage()->isAlwaysOnLoggingAllowed();
}

void WebPage::unapplyEditCommand(uint64_t stepID)
{
    WebUndoStep* step = webUndoStep(stepID);
    if (!step)
        return;

    step->step().unapply();
}

void WebPage::reapplyEditCommand(uint64_t stepID)
{
    WebUndoStep* step = webUndoStep(stepID);
    if (!step)
        return;

    m_isInRedo = true;
    step->step().reapply();
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

#if ENABLE(DATALIST_ELEMENT)

void WebPage::setActiveDataListSuggestionPicker(WebDataListSuggestionPicker* dataListSuggestionPicker)
{
    m_activeDataListSuggestionPicker = dataListSuggestionPicker;
}

void WebPage::didSelectDataListOption(const String& selectedOption)
{
    m_activeDataListSuggestionPicker->didSelectOption(selectedOption);
}

void WebPage::didCloseSuggestions()
{
    if (m_activeDataListSuggestionPicker)
        m_activeDataListSuggestionPicker->didCloseSuggestions();
    m_activeDataListSuggestionPicker = nullptr;
}

#endif

void WebPage::setActiveOpenPanelResultListener(Ref<WebOpenPanelResultListener>&& openPanelResultListener)
{
    m_activeOpenPanelResultListener = WTFMove(openPanelResultListener);
}

bool WebPage::findStringFromInjectedBundle(const String& target, FindOptions options)
{
    return m_page->findString(target, core(options));
}

void WebPage::findString(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    findController().findString(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::findStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    findController().findStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::getImageForFindMatch(uint32_t matchIndex)
{
    findController().getImageForFindMatch(matchIndex);
}

void WebPage::selectFindMatch(uint32_t matchIndex)
{
    findController().selectFindMatch(matchIndex);
}

void WebPage::hideFindUI()
{
    findController().hideFindUI();
}

void WebPage::countStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    findController().countStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
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
void WebPage::extendSandboxForFilesFromOpenPanel(SandboxExtension::HandleArray&& handles)
{
    for (size_t i = 0; i < handles.size(); ++i) {
        bool result = SandboxExtension::consumePermanently(handles[i]);
        if (!result) {
            // We have reports of cases where this fails for some unknown reason, <rdar://problem/10156710>.
            WTFLogAlways("WebPage::extendSandboxForFileFromOpenPanel(): Could not consume a sandbox extension");
        }
    }
}
#endif

#if ENABLE(GEOLOCATION)
void WebPage::didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed)
{
    geolocationPermissionRequestManager().didReceiveGeolocationPermissionDecision(geolocationID, allowed);
}
#endif

void WebPage::didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed)
{
    notificationPermissionRequestManager()->didReceiveNotificationPermissionDecision(notificationID, allowed);
}

#if ENABLE(MEDIA_STREAM)

#if !PLATFORM(IOS)
void WebPage::prepareToSendUserMediaPermissionRequest()
{
}
#endif

void WebPage::userMediaAccessWasGranted(uint64_t userMediaID, WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice, String&& mediaDeviceIdentifierHashSalt)
{
    m_userMediaPermissionRequestManager->userMediaAccessWasGranted(userMediaID, WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(mediaDeviceIdentifierHashSalt));
}

void WebPage::userMediaAccessWasDenied(uint64_t userMediaID, uint64_t reason, String&& invalidConstraint)
{
    m_userMediaPermissionRequestManager->userMediaAccessWasDenied(userMediaID, static_cast<UserMediaRequest::MediaAccessDenialReason>(reason), WTFMove(invalidConstraint));
}

void WebPage::didCompleteMediaDeviceEnumeration(uint64_t userMediaID, const Vector<CaptureDevice>& devices, String&& deviceIdentifierHashSalt, bool originHasPersistentAccess)
{
    m_userMediaPermissionRequestManager->didCompleteMediaDeviceEnumeration(userMediaID, devices, WTFMove(deviceIdentifierHashSalt), originHasPersistentAccess);
}

void WebPage::captureDevicesChanged(DeviceAccessState accessState)
{
    m_userMediaPermissionRequestManager->captureDevicesChanged(accessState);
}

#if ENABLE(SANDBOX_EXTENSIONS)
void WebPage::grantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&& extensions)
{
    m_userMediaPermissionRequestManager->grantUserMediaDeviceSandboxExtensions(WTFMove(extensions));
}

void WebPage::revokeUserMediaDeviceSandboxExtensions(const Vector<String>& extensionIDs)
{
    m_userMediaPermissionRequestManager->revokeUserMediaDeviceSandboxExtensions(extensionIDs);
}
#endif
#endif

#if !PLATFORM(IOS)
void WebPage::advanceToNextMisspelling(bool startBeforeSelection)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().advanceToNextMisspelling(startBeforeSelection);
}
#endif

bool WebPage::hasRichlyEditableSelection() const
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (m_page->dragCaretController().isContentRichlyEditable())
        return true;

    return frame.selection().selection().isContentRichlyEditable();
}

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

#if PLATFORM(COCOA)
    m_viewGestureGeometryCollector->mainFrameDidLayout();
#endif
#if PLATFORM(IOS)
    if (FrameView* frameView = mainFrameView()) {
        IntSize newContentSize = frameView->contentsSize();
        LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_pageID << " mainFrameDidLayout setting content size to " << newContentSize);
        if (m_viewportConfiguration.setContentsSize(newContentSize))
            viewportConfigurationChanged();
    }
    findController().redraw();
#endif
}

void WebPage::addPluginView(PluginView* pluginView)
{
    ASSERT(!m_pluginViews.contains(pluginView));

    m_pluginViews.add(pluginView);
    m_hasSeenPlugin = true;
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    LOG(Plugins, "Primary Plug-In Detection: triggering detection from addPluginView(%p)", pluginView);
    m_determinePrimarySnapshottedPlugInTimer.startOneShot(0_s);
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

void WebPage::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
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

    if (decoder.messageReceiverName() == Messages::RemoteWebInspectorUI::messageReceiverName()) {
        if (RemoteWebInspectorUI* remoteInspectorUI = this->remoteInspectorUI())
            remoteInspectorUI->didReceiveMessage(connection, decoder);
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

void WebPage::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
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

void WebPage::SandboxExtensionTracker::willPerformLoadDragDestinationAction(RefPtr<SandboxExtension>&& pendingDropSandboxExtension)
{
    setPendingProvisionalSandboxExtension(WTFMove(pendingDropSandboxExtension));
}

void WebPage::SandboxExtensionTracker::beginLoad(WebFrame* frame, SandboxExtension::Handle&& handle)
{
    ASSERT_UNUSED(frame, frame->isMainFrame());

    setPendingProvisionalSandboxExtension(SandboxExtension::create(WTFMove(handle)));
}

void WebPage::SandboxExtensionTracker::beginReload(WebFrame* frame, SandboxExtension::Handle&& handle)
{
    ASSERT_UNUSED(frame, frame->isMainFrame());

    // Maintain existing provisional SandboxExtension in case of a reload, if the new handle is null. This is needed
    // because the UIProcess sends us a null handle if it already sent us a handle for this path in the past.
    if (auto sandboxExtension = SandboxExtension::create(WTFMove(handle)))
        setPendingProvisionalSandboxExtension(WTFMove(sandboxExtension));
}

void WebPage::SandboxExtensionTracker::setPendingProvisionalSandboxExtension(RefPtr<SandboxExtension>&& pendingProvisionalSandboxExtension)
{
    m_pendingProvisionalSandboxExtension = WTFMove(pendingProvisionalSandboxExtension);
}

static bool shouldReuseCommittedSandboxExtension(WebFrame* frame)
{
    ASSERT(frame->isMainFrame());

    FrameLoader& frameLoader = frame->coreFrame()->loader();
    FrameLoadType frameLoadType = frameLoader.loadType();

    // If the page is being reloaded, it should reuse whatever extension is committed.
    if (isReload(frameLoadType))
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

    m_provisionalSandboxExtension = WTFMove(m_pendingProvisionalSandboxExtension);
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

    m_committedSandboxExtension = WTFMove(m_provisionalSandboxExtension);

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

    return false;
}

void WebPage::setCustomTextEncodingName(const String& encoding)
{
    m_page->mainFrame().loader().reloadWithOverrideEncoding(encoding);
}

void WebPage::didRemoveBackForwardItem(const BackForwardItemIdentifier& itemID)
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
    PluginView* pluginView = pluginViewForFrame(coreFrame);
    if (!pluginView)
        return nullptr;

    return pluginView->pdfDocumentForPrinting();
}

void WebPage::setUseSystemAppearance(bool useSystemAppearance)
{
    corePage()->setUseSystemAppearance(useSystemAppearance);
}
    
void WebPage::setUseDarkAppearance(bool useDarkAppearance)
{
    corePage()->setUseDarkAppearance(useDarkAppearance);
}
#endif

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
#endif

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

void WebPage::computePagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, CallbackID callbackID)
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
void WebPage::drawRectToImage(uint64_t frameID, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, CallbackID callbackID)
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

        auto bitmap = ShareableBitmap::createShareable(imageSize, { });
        if (!bitmap) {
            ASSERT_NOT_REACHED();
            return;
        }
        auto graphicsContext = bitmap->createGraphicsContext();

        float printingScale = static_cast<float>(imageSize.width()) / rect.width();
        graphicsContext->scale(printingScale);

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

        image = WebImage::create(bitmap.releaseNonNull());
    }
#endif

    ShareableBitmap::Handle handle;

    if (image)
        image->bitmap().createHandle(handle, SharedMemory::Protection::ReadOnly);

    send(Messages::WebPageProxy::ImageCallback(handle, callbackID));
}

void WebPage::drawPagesToPDF(uint64_t frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, CallbackID callbackID)
{
    RetainPtr<CFMutableDataRef> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, first, count, pdfPageData);
    send(Messages::WebPageProxy::DataCallback({ CFDataGetBytePtr(pdfPageData.get()), static_cast<size_t>(CFDataGetLength(pdfPageData.get())) }, callbackID));
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
void WebPage::drawPagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, CallbackID callbackID)
{
    beginPrinting(frameID, printInfo);
    if (m_printContext && m_printOperation) {
        m_printOperation->startPrint(m_printContext.get(), callbackID);
        return;
    }

    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::didFinishPrintOperation(const WebCore::ResourceError& error, CallbackID callbackID)
{
    send(Messages::WebPageProxy::PrintFinishedCallback(error, callbackID));
    m_printOperation = nullptr;
}
#endif

void WebPage::savePDFToFileInDownloadsFolder(const String& suggestedFilename, const URL& originatingURL, const uint8_t* data, unsigned long size)
{
    send(Messages::WebPageProxy::SavePDFToFileInDownloadsFolder(suggestedFilename, originatingURL, IPC::DataReference(data, size)));
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

    if (m_mainFrameProgressCompleted && !UserGestureIndicator::processingUserGesture())
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

void WebPage::setMuted(MediaProducer::MutedStateFlags state)
{
    m_page->setMuted(state);
}

void WebPage::stopMediaCapture()
{
#if ENABLE(MEDIA_STREAM)
    m_page->stopMediaCapture();
#endif
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
        m_setCanStartMediaTimer.startOneShot(0_s);
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
    if (SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(request.url().protocol().toStringWithoutCopying()))
        return true;

    if (request.url().protocolIsBlob())
        return true;

    return platformCanHandleRequest(request);
}

#if PLATFORM(COCOA)
void WebPage::handleAlternativeTextUIResult(const String& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().handleAlternativeTextUIResult(result);
}
#endif

void WebPage::simulateMouseDown(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, WallTime time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseDown, static_cast<WebMouseEvent::Button>(button), 0, position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time, WebCore::ForceAtClick, WebMouseEvent::NoTap));
}

void WebPage::simulateMouseUp(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, WallTime time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseUp, static_cast<WebMouseEvent::Button>(button), 0, position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time, WebCore::ForceAtClick, WebMouseEvent::NoTap));
}

void WebPage::simulateMouseMotion(WebCore::IntPoint position, WallTime time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseMove, WebMouseEvent::NoButton, 0, position, position, 0, 0, 0, 0, WebMouseEvent::Modifiers(), time, 0, WebMouseEvent::NoTap));
}

void WebPage::setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length, bool suppressUnderline)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.editor().canEdit())
        return;

    Vector<CompositionUnderline> underlines;
    if (!suppressUnderline)
        underlines.append(CompositionUnderline(0, compositionString.length(), CompositionUnderlineColor::TextColor, Color(Color::black), false));

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

Frame* WebPage::mainFrame() const
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
    bool isUnsupported = false;
    if (!sendSync(Messages::WebPageProxy::FindPlugin(response.mimeType(), PluginProcessTypeNormal, response.url().string(), response.url().string(), response.url().string(), allowOnlyApplicationPlugins), Messages::WebPageProxy::FindPlugin::Reply(pluginProcessToken, newMIMEType, pluginLoadPolicy, unavailabilityDescription, isUnsupported)))
        return false;

    ASSERT(!isUnsupported);
    bool isBlockedPlugin = (pluginLoadPolicy == PluginModuleBlockedForSecurity) || (pluginLoadPolicy == PluginModuleBlockedForCompatibility);
    return !isUnsupported && !isBlockedPlugin && pluginProcessToken;
#else
    UNUSED_PARAM(response);
    return false;
#endif
}

bool WebPage::shouldUseCustomContentProviderForResponse(const ResourceResponse& response)
{
    auto& mimeType = response.mimeType();
    if (mimeType.isNull())
        return false;

    // If a plug-in exists that claims to support this response, it should take precedence over the custom content provider.
    // canPluginHandleResponse() is called last because it performs synchronous IPC.
    return m_mimeTypesWithCustomContentProviders.contains(mimeType) && !canPluginHandleResponse(response);
}

#if PLATFORM(COCOA)

void WebPage::setTextAsync(const String& text)
{
    auto frame = makeRef(m_page->focusController().focusedOrMainFrame());
    if (frame->selection().selection().isContentEditable()) {
        UserTypingGestureIndicator indicator(frame.get());
        frame->selection().selectAll();
        if (text.isEmpty())
            frame->editor().deleteSelectionWithSmartDelete(false);
        else
            frame->editor().insertText(text, nullptr, TextEventInputKeyboard);
        return;
    }

    if (is<HTMLInputElement>(m_assistedNode.get())) {
        downcast<HTMLInputElement>(*m_assistedNode).setValueForUser(text);
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebPage::insertTextAsync(const String& text, const EditingRange& replacementEditingRange, bool registerUndoGroup, uint32_t editingRangeIsRelativeTo, bool suppressSelectionUpdate)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    Ref<Frame> protector(frame);

    bool replacesText = false;
    if (replacementEditingRange.location != notFound) {
        if (auto replacementRange = rangeFromEditingRange(frame, replacementEditingRange, static_cast<EditingRangeIsRelativeTo>(editingRangeIsRelativeTo))) {
            SetForScope<bool> isSelectingTextWhileInsertingAsynchronously(m_isSelectingTextWhileInsertingAsynchronously, suppressSelectionUpdate);
            frame.selection().setSelection(VisibleSelection(*replacementRange, SEL_DEFAULT_AFFINITY));
            replacesText = replacementEditingRange.length;
        }
    }
    
    if (registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping());
    
    if (!frame.editor().hasComposition()) {
        // An insertText: might be handled by other responders in the chain if we don't handle it.
        // One example is space bar that results in scrolling down the page.
        frame.editor().insertText(text, nullptr, replacesText ? TextEventInputAutocompletion : TextEventInputKeyboard);
    } else
        frame.editor().confirmComposition(text);
}

void WebPage::getMarkedRangeAsync(CallbackID callbackID)
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

void WebPage::getSelectedRangeAsync(CallbackID callbackID)
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

void WebPage::characterIndexForPointAsync(const WebCore::IntPoint& point, CallbackID callbackID)
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

void WebPage::firstRectForCharacterRangeAsync(const EditingRange& editingRange, CallbackID callbackID)
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

void WebPage::setCompositionAsync(const String& text, const Vector<CompositionUnderline>& underlines, const EditingRange& selection, const EditingRange& replacementEditingRange)
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

    Ref<Frame> protector(*targetFrame);

    if (replacementLength > 0) {
        // The layout needs to be uptodate before setting a selection
        targetFrame->document()->updateLayout();

        Element* scope = targetFrame->selection().selection().rootEditableElement();
        RefPtr<Range> replacementRange = TextIterator::rangeFromLocationAndLength(scope, replacementStart, replacementLength);
        targetFrame->editor().setIgnoreSelectionChanges(true);
        targetFrame->selection().setSelection(VisibleSelection(*replacementRange, SEL_DEFAULT_AFFINITY));
        targetFrame->editor().setIgnoreSelectionChanges(false);
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

#if PLATFORM(MAC)
static bool needsHiddenContentEditableQuirk(bool needsQuirks, const URL& url)
{
    if (!needsQuirks)
        return false;

    return equalLettersIgnoringASCIICase(url.host(), "docs.google.com");
}

static bool needsPlainTextQuirk(bool needsQuirks, const URL& url)
{
    if (!needsQuirks)
        return false;

    auto host = url.host();

    if (equalLettersIgnoringASCIICase(host, "twitter.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "onedrive.live.com"))
        return true;

    String path = url.path();
    if (equalLettersIgnoringASCIICase(host, "www.icloud.com") && (path.contains("notes") || url.fragmentIdentifier().contains("notes")))
        return true;

    if (equalLettersIgnoringASCIICase(host, "trix-editor.org"))
        return true;

    return false;
}
#endif

void WebPage::didApplyStyle()
{
    sendEditorStateUpdate();
}

void WebPage::didChangeContents()
{
    sendEditorStateUpdate();
}

void WebPage::didChangeSelection()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    // The act of getting Dictionary Popup info can make selection changes that we should not propagate to the UIProcess.
    // Specifically, if there is a caret selection, it will change to a range selection of the word around the caret. And
    // then it will change back.
    if (frame.editor().isGettingDictionaryPopupInfo())
        return;

    // Similarly, we don't want to propagate changes to the web process when inserting text asynchronously, since we will
    // end up with a range selection very briefly right before inserting the text.
    if (m_isSelectingTextWhileInsertingAsynchronously)
        return;

#if PLATFORM(MAC)
    bool hasPreviouslyFocusedDueToUserInteraction = m_hasEverFocusedElementDueToUserInteractionSincePageTransition;
    m_hasEverFocusedElementDueToUserInteractionSincePageTransition |= m_userIsInteracting;

    if (!hasPreviouslyFocusedDueToUserInteraction && m_hasEverFocusedElementDueToUserInteractionSincePageTransition) {
        if (needsHiddenContentEditableQuirk(m_page->settings().needsSiteSpecificQuirks(), m_page->mainFrame().document()->url())) {
            m_needsHiddenContentEditableQuirk = true;
            send(Messages::WebPageProxy::SetNeedsHiddenContentEditableQuirk(m_needsHiddenContentEditableQuirk));
        }

        if (needsPlainTextQuirk(m_page->settings().needsSiteSpecificQuirks(), m_page->mainFrame().document()->url())) {
            m_needsPlainTextQuirk = true;
            send(Messages::WebPageProxy::SetNeedsPlainTextQuirk(m_needsPlainTextQuirk));
        }

        send(Messages::WebPageProxy::SetHasHadSelectionChangesFromUserInteraction(m_hasEverFocusedElementDueToUserInteractionSincePageTransition));
    }

    // Abandon the current inline input session if selection changed for any other reason but an input method direct action.
    // FIXME: This logic should be in WebCore.
    // FIXME: Many changes that affect composition node do not go through didChangeSelection(). We need to do something when DOM manipulation affects the composition, because otherwise input method's idea about it will be different from Editor's.
    // FIXME: We can't cancel composition when selection changes to NoSelection, but we probably should.
    if (frame.editor().hasComposition() && !frame.editor().ignoreSelectionChanges() && !frame.selection().isNone()) {
        frame.editor().cancelComposition();
        discardedComposition();
        return;
    }
#endif

    sendPartialEditorStateAndSchedulePostLayoutUpdate();
}

void WebPage::resetAssistedNodeForFrame(WebFrame* frame)
{
    if (!m_assistedNode)
        return;
    if (frame->isMainFrame() || m_assistedNode->document().frame() == frame->coreFrame()) {
#if PLATFORM(IOS)
        send(Messages::WebPageProxy::StopAssistingNode());
#elif PLATFORM(MAC)
        send(Messages::WebPageProxy::SetEditableElementIsFocused(false));
#endif
        m_assistedNode = nullptr;
    }
}

void WebPage::elementDidFocus(WebCore::Node* node)
{
    if (m_assistedNode == node && m_isAssistingNodeDueToUserInteraction)
        return;

    if (node->hasTagName(WebCore::HTMLNames::selectTag) || node->hasTagName(WebCore::HTMLNames::inputTag) || node->hasTagName(WebCore::HTMLNames::textareaTag) || node->hasEditableStyle()) {
        m_assistedNode = node;
        m_isAssistingNodeDueToUserInteraction |= m_userIsInteracting;

#if PLATFORM(IOS)

#if ENABLE(FULLSCREEN_API)
        if (node->document().webkitIsFullScreen())
            node->document().webkitCancelFullScreen();
#endif

        ++m_currentAssistedNodeIdentifier;
        AssistedNodeInformation information;
        getAssistedNodeInformation(information);
        RefPtr<API::Object> userData;

        m_formClient->willBeginInputSession(this, downcast<Element>(node), WebFrame::fromCoreFrame(*node->document().frame()), m_userIsInteracting, userData);

        send(Messages::WebPageProxy::StartAssistingNode(information, m_userIsInteracting, m_hasPendingBlurNotification, m_changingActivityState, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
#elif PLATFORM(MAC)
        if (node->hasTagName(WebCore::HTMLNames::selectTag))
            send(Messages::WebPageProxy::SetEditableElementIsFocused(false));
        else
            send(Messages::WebPageProxy::SetEditableElementIsFocused(true));
#endif
        m_hasPendingBlurNotification = false;
    }
}

void WebPage::elementDidBlur(WebCore::Node* node)
{
    if (m_assistedNode == node) {
        m_hasPendingBlurNotification = true;
        RefPtr<WebPage> protectedThis(this);
        callOnMainThread([protectedThis] {
            if (protectedThis->m_hasPendingBlurNotification) {
#if PLATFORM(IOS)
                protectedThis->send(Messages::WebPageProxy::StopAssistingNode());
#elif PLATFORM(MAC)
                protectedThis->send(Messages::WebPageProxy::SetEditableElementIsFocused(false));
#endif
            }
            protectedThis->m_hasPendingBlurNotification = false;
        });

        m_isAssistingNodeDueToUserInteraction = false;
        m_assistedNode = nullptr;
    }
}

void WebPage::didUpdateComposition()
{
    sendEditorStateUpdate();
}

void WebPage::didEndUserTriggeredSelectionChanges()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.editor().ignoreSelectionChanges())
        sendEditorStateUpdate();
}

void WebPage::discardedComposition()
{
    send(Messages::WebPageProxy::CompositionWasCanceled());
    sendEditorStateUpdate();
}

void WebPage::canceledComposition()
{
    send(Messages::WebPageProxy::CompositionWasCanceled());
    sendEditorStateUpdate();
}

void WebPage::setAlwaysShowsHorizontalScroller(bool alwaysShowsHorizontalScroller)
{
    if (alwaysShowsHorizontalScroller == m_alwaysShowsHorizontalScroller)
        return;

    m_alwaysShowsHorizontalScroller = alwaysShowsHorizontalScroller;
    auto view = corePage()->mainFrame().view();
    if (!alwaysShowsHorizontalScroller)
        view->setHorizontalScrollbarLock(false);
    view->setHorizontalScrollbarMode(alwaysShowsHorizontalScroller ? ScrollbarAlwaysOn : m_mainFrameIsScrollable ? ScrollbarAuto : ScrollbarAlwaysOff, alwaysShowsHorizontalScroller || !m_mainFrameIsScrollable);
}

void WebPage::setAlwaysShowsVerticalScroller(bool alwaysShowsVerticalScroller)
{
    if (alwaysShowsVerticalScroller == m_alwaysShowsVerticalScroller)
        return;

    m_alwaysShowsVerticalScroller = alwaysShowsVerticalScroller;
    auto view = corePage()->mainFrame().view();
    if (!alwaysShowsVerticalScroller)
        view->setVerticalScrollbarLock(false);
    view->setVerticalScrollbarMode(alwaysShowsVerticalScroller ? ScrollbarAlwaysOn : m_mainFrameIsScrollable ? ScrollbarAuto : ScrollbarAlwaysOff, alwaysShowsVerticalScroller || !m_mainFrameIsScrollable);
}

void WebPage::setViewLayoutSize(const IntSize& viewLayoutSize)
{
    if (m_viewLayoutSize == viewLayoutSize)
        return;

    m_viewLayoutSize = viewLayoutSize;
    if (viewLayoutSize.width() <= 0) {
        corePage()->mainFrame().view()->enableAutoSizeMode(false, IntSize(), IntSize());
        return;
    }

    int viewLayoutWidth = viewLayoutSize.width();
    int viewLayoutHeight = std::max(viewLayoutSize.height(), 1);

    int maximumSize = std::numeric_limits<int>::max();

    corePage()->mainFrame().view()->enableAutoSizeMode(true, IntSize(viewLayoutWidth, viewLayoutHeight), IntSize(maximumSize, maximumSize));
}

void WebPage::setAutoSizingShouldExpandToViewHeight(bool shouldExpand)
{
    if (m_autoSizingShouldExpandToViewHeight == shouldExpand)
        return;

    m_autoSizingShouldExpandToViewHeight = shouldExpand;

    corePage()->mainFrame().view()->setAutoSizeFixedMinimumHeight(shouldExpand ? m_viewSize.height() : 0);
}

void WebPage::setViewportSizeForCSSViewportUnits(std::optional<WebCore::IntSize> viewportSize)
{
    if (m_viewportSizeForCSSViewportUnits == viewportSize)
        return;

    m_viewportSizeForCSSViewportUnits = viewportSize;
    if (m_viewportSizeForCSSViewportUnits)
        corePage()->mainFrame().view()->setViewportSizeForCSSViewportUnits(*m_viewportSizeForCSSViewportUnits);
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

bool WebPage::isSelectTrailingWhitespaceEnabled() const
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

bool WebPage::canShowResponse(const WebCore::ResourceResponse& response) const
{
    return canShowMIMEType(response.mimeType(), [&](auto& mimeType, auto allowedPlugins) {
        return m_page->pluginData().supportsWebVisibleMimeTypeForURL(mimeType, allowedPlugins, response.url());
    });
}

bool WebPage::canShowMIMEType(const String& mimeType) const
{
    return canShowMIMEType(mimeType, [&](auto& mimeType, auto allowedPlugins) {
        return m_page->pluginData().supportsWebVisibleMimeType(mimeType, allowedPlugins);
    });
}

bool WebPage::canShowMIMEType(const String& mimeType, const Function<bool(const String&, PluginData::AllowedPluginTypes)>& pluginsSupport) const
{
    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

    if (!mimeType.isNull() && m_mimeTypesWithCustomContentProviders.contains(mimeType))
        return true;

    if (corePage()->mainFrame().loader().subframeLoader().allowPlugins() && pluginsSupport(mimeType, PluginData::AllPlugins))
        return true;

    // We can use application plugins even if plugins aren't enabled.
    if (pluginsSupport(mimeType, PluginData::OnlyApplicationPlugins))
        return true;

    return false;
}

void WebPage::addTextCheckingRequest(uint64_t requestID, Ref<TextCheckingRequest>&& request)
{
    m_pendingTextCheckingRequestMap.add(requestID, WTFMove(request));
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
#endif
    resetAssistedNodeForFrame(frame);

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
    m_estimatedLatency = Seconds(1.0 / 60);

#if ENABLE(IOS_TOUCH_EVENTS)
    WebProcess::singleton().eventDispatcher().clearQueuedTouchEventsForPage(*this);
#endif

    resetViewportDefaultConfiguration(frame);
    const Frame* coreFrame = frame->coreFrame();
    
    bool viewportChanged = false;

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_pageID << " didCommitLoad setting content size to " << coreFrame->view()->contentsSize());
    if (m_viewportConfiguration.setContentsSize(coreFrame->view()->contentsSize()))
        viewportChanged = true;

    if (!m_page->settings().shouldIgnoreMetaViewport() && m_viewportConfiguration.setViewportArguments(coreFrame->document()->viewportArguments()))
        viewportChanged = true;

    if (viewportChanged)
        viewportConfigurationChanged();
#endif

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    resetPrimarySnapshottedPlugIn();
#endif

#if USE(OS_STATE)
    m_loadCommitTime = WallTime::now();
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
    m_determinePrimarySnapshottedPlugInTimer.startOneShot(0_s);
#else
    UNUSED_PARAM(frame);
#endif
}

void WebPage::didInsertMenuElement(HTMLMenuElement& element)
{
#if PLATFORM(COCOA)
    sendTouchBarMenuDataAddedUpdate(element);
#else
    UNUSED_PARAM(element);
#endif
}

void WebPage::didRemoveMenuElement(HTMLMenuElement& element)
{
#if PLATFORM(COCOA)
    sendTouchBarMenuDataRemovedUpdate(element);
#else
    UNUSED_PARAM(element);
#endif
}

void WebPage::didInsertMenuItemElement(HTMLMenuItemElement& element)
{
#if PLATFORM(COCOA)
    sendTouchBarMenuItemDataAddedUpdate(element);
#else
    UNUSED_PARAM(element);
#endif
}

void WebPage::didRemoveMenuItemElement(HTMLMenuItemElement& element)
{
#if PLATFORM(COCOA)
    sendTouchBarMenuItemDataRemovedUpdate(element);
#else
    UNUSED_PARAM(element);
#endif
}

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
static const int primarySnapshottedPlugInSearchLimit = 3000;
static const float primarySnapshottedPlugInSearchBucketSize = 1.1;
static const int primarySnapshottedPlugInMinimumWidth = 400;
static const int primarySnapshottedPlugInMinimumHeight = 300;
static const unsigned maxPrimarySnapshottedPlugInDetectionAttempts = 2;
static const Seconds deferredPrimarySnapshottedPlugInDetectionDelay = 3_s;
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

    RefPtr<FrameView> mainFrameView = corePage()->mainFrame().view();
    if (!mainFrameView)
        return;

    IntRect searchRect = IntRect(IntPoint(), corePage()->mainFrame().view()->contentsSize());
    searchRect.intersect(IntRect(IntPoint(), IntSize(primarySnapshottedPlugInSearchLimit, primarySnapshottedPlugInSearchLimit)));

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowUserAgentShadowContent);

    RefPtr<HTMLPlugInImageElement> candidatePlugIn;
    unsigned candidatePlugInArea = 0;

    for (RefPtr<Frame> frame = &corePage()->mainFrame(); frame; frame = frame->tree().traverseNextRendered()) {
        if (!frame->loader().subframeLoader().containsPlugins())
            continue;
        if (!frame->document() || !frame->view())
            continue;

        Vector<Ref<HTMLPlugInImageElement>> nonPlayingPlugInImageElements;
        for (auto& plugInImageElement : descendantsOfType<HTMLPlugInImageElement>(*frame->document())) {
            if (plugInImageElement.displayState() == HTMLPlugInElement::Playing)
                continue;
            nonPlayingPlugInImageElements.append(plugInImageElement);
        }

        for (auto& plugInImageElement : nonPlayingPlugInImageElements) {
            auto pluginRenderer = plugInImageElement->renderer();
            if (!pluginRenderer || !pluginRenderer->isBox())
                continue;
            auto& pluginRenderBox = downcast<RenderBox>(*pluginRenderer);
            if (!plugInIntersectsSearchRect(plugInImageElement.get()))
                continue;

            IntRect plugInRectRelativeToView = plugInImageElement->clientRect();
            ScrollPosition scrollPosition = mainFrameView->documentScrollPositionRelativeToViewOrigin();
            IntRect plugInRectRelativeToTopDocument(plugInRectRelativeToView.location() + scrollPosition, plugInRectRelativeToView.size());
            HitTestResult hitTestResult(plugInRectRelativeToTopDocument.center());

            if (!mainFrameView->renderView())
                return;
            mainFrameView->renderView()->hitTest(request, hitTestResult);

            RefPtr<Element> element = hitTestResult.targetElement();
            if (!element)
                continue;

            IntRect elementRectRelativeToView = element->clientRect();
            IntRect elementRectRelativeToTopDocument(elementRectRelativeToView.location() + scrollPosition, elementRectRelativeToView.size());
            LayoutRect inflatedPluginRect = plugInRectRelativeToTopDocument;
            LayoutUnit xOffset = (inflatedPluginRect.width() * overlappingImageBoundsScale - inflatedPluginRect.width()) / 2;
            LayoutUnit yOffset = (inflatedPluginRect.height() * overlappingImageBoundsScale - inflatedPluginRect.height()) / 2;
            inflatedPluginRect.inflateX(xOffset);
            inflatedPluginRect.inflateY(yOffset);

            if (element != plugInImageElement.ptr()) {
                if (!(is<HTMLImageElement>(*element)
                    && inflatedPluginRect.contains(elementRectRelativeToTopDocument)
                    && elementRectRelativeToTopDocument.width() > pluginRenderBox.width() * minimumOverlappingImageToPluginDimensionScale
                    && elementRectRelativeToTopDocument.height() > pluginRenderBox.height() * minimumOverlappingImageToPluginDimensionScale))
                    continue;
                LOG(Plugins, "Primary Plug-In Detection: Plug-in is hidden by an image that is roughly aligned with it, autoplaying regardless of whether or not it's actually the primary plug-in.");
                plugInImageElement->restartSnapshottedPlugIn();
            }

            if (plugInIsPrimarySize(plugInImageElement, candidatePlugInArea))
                candidatePlugIn = WTFMove(plugInImageElement);
        }
    }
    if (!candidatePlugIn) {
        LOG(Plugins, "Primary Plug-In Detection: fail - did not find a candidate plug-in.");
        if (m_numberOfPrimarySnapshotDetectionAttempts < maxPrimarySnapshottedPlugInDetectionAttempts) {
            LOG(Plugins, "Primary Plug-In Detection: will attempt again in %.1f s.", deferredPrimarySnapshottedPlugInDetectionDelay.value());
            m_determinePrimarySnapshottedPlugInTimer.startOneShot(deferredPrimarySnapshottedPlugInDetectionDelay);
        }
        return;
    }

    LOG(Plugins, "Primary Plug-In Detection: success - found a candidate plug-in - inform it.");
    m_didFindPrimarySnapshottedPlugin = true;
    m_primaryPlugInPageOrigin = m_page->mainFrame().document()->baseURL().host().toString();
    m_primaryPlugInOrigin = candidatePlugIn->loadedUrl().host().toString();
    m_primaryPlugInMimeType = candidatePlugIn->serviceType();

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
    auto& mainFrame = corePage()->mainFrame();
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
    auto* renderer = plugInImageElement.renderer();
    if (!is<RenderBox>(renderer))
        return false;

    auto& box = downcast<RenderBox>(*renderer);
    if (box.contentWidth() < primarySnapshottedPlugInMinimumWidth || box.contentHeight() < primarySnapshottedPlugInMinimumHeight)
        return false;

    LayoutUnit contentArea = box.contentWidth() * box.contentHeight();
    if (contentArea > candidatePlugInArea * primarySnapshottedPlugInSearchBucketSize) {
        candidatePlugInArea = contentArea.toUnsigned();
        return true;
    }

    return false;
}

#endif // ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)

RefPtr<Range> WebPage::currentSelectionAsRange()
{
    auto* frame = frameWithSelection(m_page.get());
    if (!frame)
        return nullptr;

    return frame->selection().toNormalizedRange();
}

void WebPage::reportUsedFeatures()
{
    Vector<String> namedFeatures;
    m_loaderClient->featuresUsedInPage(*this, namedFeatures);
}

void WebPage::sendEditorStateUpdate()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.editor().ignoreSelectionChanges())
        return;

    m_hasPendingEditorStateUpdate = false;

    // If we immediately dispatch an EditorState update to the UI process, layout may not be up to date yet.
    // If that is the case, just send what we have (i.e. don't include post-layout data) and wait until the
    // next layer tree commit to compute and send the complete EditorState over.
    auto state = editorState();
    send(Messages::WebPageProxy::EditorStateChanged(state), pageID());

    if (state.isMissingPostLayoutData) {
        m_hasPendingEditorStateUpdate = true;
        m_drawingArea->scheduleCompositingLayerFlush();
    }
}

#if PLATFORM(COCOA)
void WebPage::sendTouchBarMenuDataRemovedUpdate(HTMLMenuElement& element)
{
    send(Messages::WebPageProxy::TouchBarMenuDataChanged(TouchBarMenuData { }));
}

void WebPage::sendTouchBarMenuDataAddedUpdate(HTMLMenuElement& element)
{
    send(Messages::WebPageProxy::TouchBarMenuDataChanged(TouchBarMenuData {element}));
}

void WebPage::sendTouchBarMenuItemDataAddedUpdate(HTMLMenuItemElement& element)
{
    send(Messages::WebPageProxy::TouchBarMenuItemDataAdded(TouchBarMenuItemData {element}));
}

void WebPage::sendTouchBarMenuItemDataRemovedUpdate(HTMLMenuItemElement& element)
{
    send(Messages::WebPageProxy::TouchBarMenuItemDataRemoved(TouchBarMenuItemData {element}));
}
#endif

void WebPage::sendPartialEditorStateAndSchedulePostLayoutUpdate()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.editor().ignoreSelectionChanges())
        return;

    send(Messages::WebPageProxy::EditorStateChanged(editorState(IncludePostLayoutDataHint::No)), pageID());

    if (m_hasPendingEditorStateUpdate)
        return;

    // Flag the next layer flush to compute and propagate an EditorState to the UI process.
    m_hasPendingEditorStateUpdate = true;
    m_drawingArea->scheduleCompositingLayerFlush();
}

void WebPage::flushPendingEditorStateUpdate()
{
    if (!m_hasPendingEditorStateUpdate)
        return;

    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.editor().ignoreSelectionChanges())
        return;

    sendEditorStateUpdate();
}

void WebPage::updateWebsitePolicies(WebsitePoliciesData&& websitePolicies)
{
    if (!m_page)
        return;

    auto* documentLoader = m_page->mainFrame().loader().documentLoader();
    if (!documentLoader)
        return;

    WebsitePoliciesData::applyToDocumentLoader(WTFMove(websitePolicies), *documentLoader);
    
#if ENABLE(VIDEO)
    m_page->updateMediaElementRateChangeRestrictions();
#endif
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

void WebPage::setScrollbarOverlayStyle(std::optional<uint32_t> scrollbarStyle)
{
    if (scrollbarStyle)
        m_scrollbarOverlayStyle = static_cast<ScrollbarOverlayStyle>(scrollbarStyle.value());
    else
        m_scrollbarOverlayStyle = std::optional<ScrollbarOverlayStyle>();
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

void WebPage::getBytecodeProfile(CallbackID callbackID)
{
    if (LIKELY(!commonVM().m_perBytecodeProfiler)) {
        send(Messages::WebPageProxy::StringCallback(String(), callbackID));
        return;
    }

    String result = commonVM().m_perBytecodeProfiler->toJSON();
    ASSERT(result.length());
    send(Messages::WebPageProxy::StringCallback(result, callbackID));
}

void WebPage::getSamplingProfilerOutput(CallbackID callbackID)
{
#if ENABLE(SAMPLING_PROFILER)
    SamplingProfiler* samplingProfiler = commonVM().samplingProfiler();
    if (!samplingProfiler) {
        send(Messages::WebPageProxy::InvalidateStringCallback(callbackID));
        return;
    }

    StringPrintStream result;
    samplingProfiler->reportTopFunctions(result);
    samplingProfiler->reportTopBytecodes(result);
    send(Messages::WebPageProxy::StringCallback(result.toString(), callbackID));
#else
    send(Messages::WebPageProxy::InvalidateStringCallback(callbackID));
#endif
}

RefPtr<WebCore::Range> WebPage::rangeFromEditingRange(WebCore::Frame& frame, const EditingRange& range, EditingRangeIsRelativeTo editingRangeIsRelativeTo)
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

void WebPage::postMessageIgnoringFullySynchronousMode(const String& messageName, API::Object* messageBody)
{
    send(Messages::WebPageProxy::HandleMessage(messageName, UserData(WebProcess::singleton().transformObjectsToHandles(messageBody))), pageID(), IPC::SendOption::IgnoreFullySynchronousMode);
}

void WebPage::postSynchronousMessageForTesting(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    UserData returnUserData;

    auto& webProcess = WebProcess::singleton();
    if (!sendSync(Messages::WebPageProxy::HandleSynchronousMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))), Messages::WebPageProxy::HandleSynchronousMessage::Reply(returnUserData), Seconds::infinity(), IPC::SendSyncOption::UseFullySynchronousModeForTesting))
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

void WebPage::addUserScript(String&& source, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserScriptInjectionTime injectionTime)
{
    WebCore::UserScript userScript { WTFMove(source), URL(WebCore::blankURL()), Vector<String>(), Vector<String>(), injectionTime, injectedFrames };

    m_userContentController->addUserScript(InjectedBundleScriptWorld::normalWorld(), WTFMove(userScript));
}

void WebPage::addUserStyleSheet(const String& source, WebCore::UserContentInjectedFrames injectedFrames)
{
    WebCore::UserStyleSheet userStyleSheet{ source, WebCore::blankURL(), Vector<String>(), Vector<String>(), injectedFrames, UserStyleUserLevel };

    m_userContentController->addUserStyleSheet(InjectedBundleScriptWorld::normalWorld(), WTFMove(userStyleSheet));
}

void WebPage::removeAllUserContent()
{
    m_userContentController->removeAllUserContent();
}

void WebPage::dispatchDidReachLayoutMilestone(WebCore::LayoutMilestones milestones)
{
    RefPtr<API::Object> userData;
    injectedBundleLoaderClient().didReachLayoutMilestone(*this, milestones, userData);

    // Clients should not set userData for this message, and it won't be passed through.
    ASSERT(!userData);

    // The drawing area might want to defer dispatch of didLayout to the UI process.
    if (m_drawingArea && m_drawingArea->dispatchDidReachLayoutMilestone(milestones))
        return;

    send(Messages::WebPageProxy::DidReachLayoutMilestone(milestones));
}

void WebPage::didRestoreScrollPosition()
{
    send(Messages::WebPageProxy::DidRestoreScrollPosition());
}

void WebPage::setResourceCachingDisabled(bool disabled)
{
    m_page->setResourceCachingDisabled(disabled);
}

void WebPage::setUserInterfaceLayoutDirection(uint32_t direction)
{
    m_userInterfaceLayoutDirection = static_cast<WebCore::UserInterfaceLayoutDirection>(direction);
    m_page->setUserInterfaceLayoutDirection(m_userInterfaceLayoutDirection);
}

#if ENABLE(GAMEPAD)

void WebPage::gamepadActivity(const Vector<GamepadData>& gamepadDatas, bool shouldMakeGamepadsVisible)
{
    WebGamepadProvider::singleton().gamepadActivity(gamepadDatas, shouldMakeGamepadsVisible);
}

#endif

#if ENABLE(POINTER_LOCK)
void WebPage::didAcquirePointerLock()
{
    corePage()->pointerLockController().didAcquirePointerLock();
}

void WebPage::didNotAcquirePointerLock()
{
    corePage()->pointerLockController().didNotAcquirePointerLock();
}

void WebPage::didLosePointerLock()
{
    corePage()->pointerLockController().didLosePointerLock();
}
#endif

void WebPage::didGetLoadDecisionForIcon(bool decision, CallbackID loadIdentifier, OptionalCallbackID newCallbackID)
{
    if (auto* documentLoader = corePage()->mainFrame().loader().documentLoader())
        documentLoader->didGetLoadDecisionForIcon(decision, loadIdentifier.toInteger(), newCallbackID.toInteger());
}

void WebPage::setUseIconLoadingClient(bool useIconLoadingClient)
{
    static_cast<WebFrameLoaderClient&>(corePage()->mainFrame().loader().client()).setUseIconLoadingClient(useIconLoadingClient);
}

WebURLSchemeHandlerProxy* WebPage::urlSchemeHandlerForScheme(const String& scheme)
{
    return m_schemeToURLSchemeHandlerProxyMap.get(scheme);
}

void WebPage::stopAllURLSchemeTasks()
{
    HashSet<WebURLSchemeHandlerProxy*> handlers;
    for (auto& handler : m_schemeToURLSchemeHandlerProxyMap.values())
        handlers.add(handler.get());

    for (auto* handler : handlers)
        handler->stopAllTasks();
}

void WebPage::registerURLSchemeHandler(uint64_t handlerIdentifier, const String& scheme)
{
    auto schemeResult = m_schemeToURLSchemeHandlerProxyMap.add(scheme, WebURLSchemeHandlerProxy::create(*this, handlerIdentifier));
    m_identifierToURLSchemeHandlerProxyMap.add(handlerIdentifier, schemeResult.iterator->value.get());
}

void WebPage::urlSchemeTaskDidPerformRedirection(uint64_t handlerIdentifier, uint64_t taskIdentifier, ResourceResponse&& response, ResourceRequest&& request)
{
    auto* handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);
    
    handler->taskDidPerformRedirection(taskIdentifier, WTFMove(response), WTFMove(request));
}
    
void WebPage::urlSchemeTaskDidReceiveResponse(uint64_t handlerIdentifier, uint64_t taskIdentifier, const ResourceResponse& response)
{
    auto* handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidReceiveResponse(taskIdentifier, response);
}

void WebPage::urlSchemeTaskDidReceiveData(uint64_t handlerIdentifier, uint64_t taskIdentifier, const IPC::DataReference& data)
{
    auto* handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidReceiveData(taskIdentifier, data.size(), data.data());
}

void WebPage::urlSchemeTaskDidComplete(uint64_t handlerIdentifier, uint64_t taskIdentifier, const ResourceError& error)
{
    auto* handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidComplete(taskIdentifier, error);
}

void WebPage::setIsSuspended(bool suspended)
{
    if (m_isSuspended == suspended)
        return;

    m_isSuspended = suspended;

    setLayerTreeStateIsFrozen(true);
}

void WebPage::tearDownDrawingAreaForSuspend()
{
    if (!m_isSuspended)
        return;
    m_drawingArea = nullptr;
}

void WebPage::frameBecameRemote(uint64_t frameID, GlobalFrameIdentifier&& remoteFrameIdentifier, GlobalWindowIdentifier&& remoteWindowIdentifier)
{
    RefPtr<WebFrame> frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    if (frame->page() != this)
        return;

    auto* coreFrame = frame->coreFrame();
    auto* previousWindow = coreFrame->window();
    if (!previousWindow)
        return;

    auto remoteFrame = RemoteFrame::create(WTFMove(remoteFrameIdentifier));
    auto remoteWindow = RemoteDOMWindow::create(remoteFrame.copyRef(), WTFMove(remoteWindowIdentifier));

    remoteFrame->setOpener(frame->coreFrame()->loader().opener());

    auto jsWindowProxies = frame->coreFrame()->windowProxy().releaseJSWindowProxies();
    remoteFrame->windowProxy().setJSWindowProxies(WTFMove(jsWindowProxies));
    remoteFrame->windowProxy().setDOMWindow(remoteWindow.ptr());

    coreFrame->setView(nullptr);
    coreFrame->willDetachPage();
    coreFrame->detachFromPage();

    if (frame->isMainFrame())
        close();
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
static uint64_t nextRequestStorageAccessContextId()
{
    static uint64_t nextContextId = 0;
    return ++nextContextId;
}

void WebPage::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, CompletionHandler<void(bool)>&& callback)
{
    auto contextId = nextRequestStorageAccessContextId();
    auto addResult = m_storageAccessResponseCallbackMap.add(contextId, WTFMove(callback));
    ASSERT(addResult.isNewEntry);
    if (addResult.iterator->value)
        send(Messages::WebPageProxy::HasStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, contextId));
    else
        callback(false);
}
    
void WebPage::requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, CompletionHandler<void(bool)>&& callback)
{
    auto contextId = nextRequestStorageAccessContextId();
    auto addResult = m_storageAccessResponseCallbackMap.add(contextId, WTFMove(callback));
    ASSERT(addResult.isNewEntry);
    if (addResult.iterator->value) {
        bool promptEnabled = RuntimeEnabledFeatures::sharedFeatures().storageAccessPromptsEnabled();
        send(Messages::WebPageProxy::RequestStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, contextId, promptEnabled));
    } else
        callback(false);
}

void WebPage::storageAccessResponse(bool wasGranted, uint64_t contextId)
{
    auto callback = m_storageAccessResponseCallbackMap.take(contextId);
    ASSERT(callback);
    callback(wasGranted);
}
#endif
    
static ShareSheetCallbackID nextShareSheetCallbackID()
{
    static ShareSheetCallbackID nextCallbackID = 0;
    return ++nextCallbackID;
}
    
void WebPage::showShareSheet(ShareDataWithParsedURL& shareData, WTF::CompletionHandler<void(bool)>&& callback)
{
    ShareSheetCallbackID callbackID = nextShareSheetCallbackID();
    auto addResult = m_shareSheetResponseCallbackMap.add(callbackID, WTFMove(callback));
    ASSERT(addResult.isNewEntry);
    if (addResult.iterator->value)
        send(Messages::WebPageProxy::ShowShareSheet(WTFMove(shareData), callbackID));
    else
        callback(false);
}

void WebPage::didCompleteShareSheet(bool wasGranted, ShareSheetCallbackID callbackID)
{
    auto callback = m_shareSheetResponseCallbackMap.take(callbackID);
    callback(wasGranted);
}

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPage::insertAttachment(const String& identifier, std::optional<uint64_t>&& fileSize, const String& fileName, const String& contentType, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().insertAttachment(identifier, WTFMove(fileSize), fileName, contentType);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::updateAttachmentAttributes(const String& identifier, std::optional<uint64_t>&& fileSize, const String& contentType, const String& fileName, const IPC::DataReference& enclosingImageData, CallbackID callbackID)
{
    if (auto attachment = attachmentElementWithIdentifier(identifier)) {
        attachment->document().updateLayout();
        attachment->updateAttributes(WTFMove(fileSize), contentType, fileName);
        attachment->updateEnclosingImageWithData(contentType, SharedBuffer::create(enclosingImageData.data(), enclosingImageData.size()));
    }
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

RefPtr<HTMLAttachmentElement> WebPage::attachmentElementWithIdentifier(const String& identifier) const
{
    // FIXME: Handle attachment elements in subframes too as well.
    auto* frame = mainFrame();
    if (!frame || !frame->document())
        return nullptr;

    return frame->document()->attachmentForIdentifier(identifier);
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(APPLICATION_MANIFEST)
void WebPage::getApplicationManifest(CallbackID callbackID)
{
    ASSERT(callbackID.isValid());
    Document* mainFrameDocument = m_mainFrame->coreFrame()->document();
    DocumentLoader* loader = mainFrameDocument ? mainFrameDocument->loader() : nullptr;

    if (!loader) {
        send(Messages::WebPageProxy::ApplicationManifestCallback(std::nullopt, callbackID));
        return;
    }

    auto coreCallbackID = loader->loadApplicationManifest();
    if (!coreCallbackID) {
        send(Messages::WebPageProxy::ApplicationManifestCallback(std::nullopt, callbackID));
        return;
    }

    m_applicationManifestFetchCallbackMap.add(coreCallbackID, callbackID.toInteger());
}

void WebPage::didFinishLoadingApplicationManifest(uint64_t coreCallbackID, const std::optional<WebCore::ApplicationManifest>& manifest)
{
    auto callbackID = CallbackID::fromInteger(m_applicationManifestFetchCallbackMap.take(coreCallbackID));
    send(Messages::WebPageProxy::ApplicationManifestCallback(manifest, callbackID));
}
#endif // ENABLE(APPLICATION_MANIFEST)

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
