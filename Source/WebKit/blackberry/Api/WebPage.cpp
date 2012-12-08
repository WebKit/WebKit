/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebPage.h"

#include "ApplicationCacheStorage.h"
#include "AuthenticationChallengeManager.h"
#include "AutofillManager.h"
#include "BackForwardController.h"
#include "BackForwardListImpl.h"
#include "BackingStoreClient.h"
#include "BackingStore_p.h"
#if ENABLE(BATTERY_STATUS)
#include "BatteryClientBlackBerry.h"
#endif
#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClientBlackBerry.h"
#include "ContextMenuClientBlackBerry.h"
#include "CookieManager.h"
#include "CredentialManager.h"
#include "CredentialStorage.h"
#include "CredentialTransformData.h"
#include "DOMSupport.h"
#include "Database.h"
#include "DatabaseSync.h"
#include "DatabaseTracker.h"
#include "DefaultTapHighlight.h"
#include "DeviceMotionClientBlackBerry.h"
#include "DeviceOrientationClientBlackBerry.h"
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
#include "DeviceOrientationClientMock.h"
#endif
#include "DragClientBlackBerry.h"
// FIXME: We should be using DumpRenderTreeClient, but I'm not sure where we should
// create the DRT_BB object. See PR #120355.
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
#include "DumpRenderTreeBlackBerry.h"
#endif
#include "EditorClientBlackBerry.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClientBlackBerry.h"
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
#include "GeolocationClientMock.h"
#endif
#include "GeolocationClientBlackBerry.h"
#include "GroupSettings.h"
#include "HTMLAreaElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTTPParsers.h"
#include "HistoryItem.h"
#include "IconDatabaseClientBlackBerry.h"
#include "ImageDocument.h"
#include "InPageSearchManager.h"
#include "InRegionScrollableArea.h"
#include "InRegionScroller_p.h"
#include "InputHandler.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorClientBlackBerry.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "InspectorOverlay.h"
#include "JavaScriptVariant_p.h"
#include "LayerWebKitThread.h"
#if ENABLE(NETWORK_INFO)
#include "NetworkInfoClientBlackBerry.h"
#endif
#include "NetworkManager.h"
#include "NodeRenderStyle.h"
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
#include "NavigatorContentUtilsClientBlackBerry.h"
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "NotificationClientBlackBerry.h"
#endif
#include "Page.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "PagePopupBlackBerry.h"
#include "PlatformTouchEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginDatabase.h"
#include "PluginView.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#if ENABLE(FULLSCREEN_API)
#include "RenderFullScreen.h"
#endif
#include "RenderText.h"
#include "RenderThemeBlackBerry.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "ScrollTypes.h"
#include "SecurityPolicy.h"
#include "SelectionHandler.h"
#include "SelectionOverlay.h"
#include "Settings.h"
#include "Storage.h"
#include "StorageNamespace.h"
#include "SurfacePool.h"
#include "Text.h"
#include "ThreadCheck.h"
#include "TouchEventHandler.h"
#include "TransformationMatrix.h"
#if ENABLE(MEDIA_STREAM)
#include "UserMediaClientImpl.h"
#endif
#if ENABLE(VIBRATION)
#include "VibrationClientBlackBerry.h"
#endif
#include "VisiblePosition.h"
#include "WebCookieJar.h"
#if ENABLE(WEBDOM)
#include "WebDOMDocument.h"
#endif
#include "WebKitThreadViewportAccessor.h"
#include "WebKitVersion.h"
#include "WebOverlay.h"
#include "WebOverlay_p.h"
#include "WebPageClient.h"
#include "WebSocket.h"
#include "WebViewportArguments.h"
#include "npapi.h"
#include "runtime_root.h"

#if ENABLE(VIDEO)
#include "MediaPlayer.h"
#include "MediaPlayerPrivateBlackBerry.h"
#endif

#if USE(SKIA)
#include "PlatformContextSkia.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "FrameLayers.h"
#include "WebPageCompositorClient.h"
#include "WebPageCompositor_p.h"
#endif

#include <BlackBerryPlatformDeviceInfo.h>
#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformKeyboardEvent.h>
#include <BlackBerryPlatformMessageClient.h>
#include <BlackBerryPlatformMouseEvent.h>
#include <BlackBerryPlatformScreen.h>
#include <BlackBerryPlatformSettings.h>
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <SharedPointer.h>
#include <sys/keycodes.h>
#include <unicode/ustring.h> // platform ICU

#include <wtf/text/CString.h>

#ifndef USER_PROCESSES
#include <memalloc.h>
#endif

#if ENABLE(ACCELERATED_2D_CANVAS)
#include "GrContext.h"
#include "SharedGraphicsContext3D.h"
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "PlatformScreen.h"
#endif

#define DEBUG_TOUCH_EVENTS 0
#define DEBUG_WEBPAGE_LOAD 0
#define DEBUG_AC_COMMIT 0

using namespace std;
using namespace WebCore;

typedef const unsigned short* CUShortPtr;

namespace BlackBerry {
namespace WebKit {

static Vector<WebPage*>* visibleWebPages()
{
    static Vector<WebPage*>* s_visibleWebPages = 0; // Initially, no web page is visible.
    if (!s_visibleWebPages)
        s_visibleWebPages = new Vector<WebPage*>;
    return s_visibleWebPages;
}

const unsigned blockZoomMargin = 3; // Add 3 pixel margin on each side.
static int blockClickRadius = 0;
static double maximumBlockZoomScale = 3; // This scale can be clamped by the maximumScale set for the page.

const double manualScrollInterval = 0.1; // The time interval during which we associate user action with scrolling.

const IntSize minimumLayoutSize(10, 10); // Needs to be a small size, greater than 0, that we can grow the layout from.

const double minimumExpandingRatio = 0.15;

const double minimumZoomToFitScale = 0.25;
const double maximumImageDocumentZoomToFitScale = 2;

// Helper function to parse a URL and fill in missing parts.
static KURL parseUrl(const String& url)
{
    String urlString(url);
    KURL kurl = KURL(KURL(), urlString);
    if (kurl.protocol().isEmpty()) {
        urlString.insert("http://", 0);
        kurl = KURL(KURL(), urlString);
    }

    return kurl;
}

// Helper functions to convert to and from WebCore types.
static inline WebCore::PlatformEvent::Type toWebCoreMouseEventType(const BlackBerry::Platform::MouseEvent::Type type)
{
    switch (type) {
    case BlackBerry::Platform::MouseEvent::MouseButtonDown:
        return WebCore::PlatformEvent::MousePressed;
    case Platform::MouseEvent::MouseButtonUp:
        return WebCore::PlatformEvent::MouseReleased;
    case Platform::MouseEvent::MouseMove:
    default:
        return WebCore::PlatformEvent::MouseMoved;
    }
}

static inline ResourceRequestCachePolicy toWebCoreCachePolicy(Platform::NetworkRequest::CachePolicy policy)
{
    switch (policy) {
    case Platform::NetworkRequest::UseProtocolCachePolicy:
        return UseProtocolCachePolicy;
    case Platform::NetworkRequest::ReloadIgnoringCacheData:
        return ReloadIgnoringCacheData;
    case Platform::NetworkRequest::ReturnCacheDataElseLoad:
        return ReturnCacheDataElseLoad;
    case Platform::NetworkRequest::ReturnCacheDataDontLoad:
        return ReturnCacheDataDontLoad;
    default:
        ASSERT_NOT_REACHED();
        return UseProtocolCachePolicy;
    }
}

#if ENABLE(EVENT_MODE_METATAGS)
static inline Platform::CursorEventMode toPlatformCursorEventMode(CursorEventMode mode)
{
    switch (mode) {
    case ProcessedCursorEvents:
        return Platform::ProcessedCursorEvents;
    case NativeCursorEvents:
        return Platform::NativeCursorEvents;
    default:
        ASSERT_NOT_REACHED();
        return Platform::ProcessedCursorEvents;
    }
}

static inline Platform::TouchEventMode toPlatformTouchEventMode(TouchEventMode mode)
{
    switch (mode) {
    case ProcessedTouchEvents:
        return Platform::ProcessedTouchEvents;
    case NativeTouchEvents:
        return Platform::NativeTouchEvents;
    case PureTouchEventsWithMouseConversion:
        return Platform::PureTouchEventsWithMouseConversion;
    default:
        ASSERT_NOT_REACHED();
        return Platform::ProcessedTouchEvents;
    }
}
#endif

static inline HistoryItem* historyItemFromBackForwardId(WebPage::BackForwardId id)
{
    return reinterpret_cast<HistoryItem*>(id);
}

static inline WebPage::BackForwardId backForwardIdFromHistoryItem(HistoryItem* item)
{
    return reinterpret_cast<WebPage::BackForwardId>(item);
}

void WebPage::setUserViewportArguments(const WebViewportArguments& viewportArguments)
{
    d->m_userViewportArguments = *(viewportArguments.d);
}

void WebPage::resetUserViewportArguments()
{
    d->m_userViewportArguments = ViewportArguments();
}

template <bool WebPagePrivate::* isActive>
class DeferredTask: public WebPagePrivate::DeferredTaskBase {
public:
    static void finishOrCancel(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->*isActive = false;
    }
protected:
    DeferredTask(WebPagePrivate* webPagePrivate)
        : DeferredTaskBase(webPagePrivate, isActive)
    {
    }
    typedef DeferredTask<isActive> DeferredTaskType;
};

void WebPage::autofillTextField(const BlackBerry::Platform::String& item)
{
    if (!d->m_webSettings->isFormAutofillEnabled())
        return;

    d->m_autofillManager->autofillTextField(item);
}

void WebPage::enableQnxJavaScriptObject(bool enabled)
{
    d->m_enableQnxJavaScriptObject = enabled;
}

BlackBerry::Platform::String WebPage::renderTreeAsText()
{
    return externalRepresentation(d->m_mainFrame);
}

WebPagePrivate::WebPagePrivate(WebPage* webPage, WebPageClient* client, const IntRect& rect)
    : m_webPage(webPage)
    , m_client(client)
    , m_inspectorClient(0)
    , m_page(0) // Initialized by init.
    , m_mainFrame(0) // Initialized by init.
    , m_currentContextNode(0)
    , m_webSettings(0) // Initialized by init.
    , m_cookieJar(0)
    , m_visible(false)
    , m_activationState(ActivationActive)
    , m_shouldResetTilesWhenShown(false)
    , m_shouldZoomToInitialScaleAfterLoadFinished(false)
    , m_userScalable(true)
    , m_userPerformedManualZoom(false)
    , m_userPerformedManualScroll(false)
    , m_contentsSizeChanged(false)
    , m_overflowExceedsContentsSize(false)
    , m_resetVirtualViewportOnCommitted(true)
    , m_shouldUseFixedDesktopMode(false)
    , m_preventIdleDimmingCount(0)
#if ENABLE(TOUCH_EVENTS)
    , m_preventDefaultOnTouchStart(false)
#endif
    , m_nestedLayoutFinishedCount(0)
    , m_actualVisibleWidth(rect.width())
    , m_actualVisibleHeight(rect.height())
    , m_defaultLayoutSize(minimumLayoutSize)
    , m_didRestoreFromPageCache(false)
    , m_viewMode(WebPagePrivate::Desktop) // Default to Desktop mode for PB.
    , m_loadState(WebPagePrivate::None)
    , m_transformationMatrix(new TransformationMatrix())
    , m_backingStore(0) // Initialized by init.
    , m_backingStoreClient(0) // Initialized by init.
    , m_webkitThreadViewportAccessor(0) // Initialized by init.
    , m_inPageSearchManager(new InPageSearchManager(this))
    , m_inputHandler(new InputHandler(this))
    , m_selectionHandler(new SelectionHandler(this))
    , m_touchEventHandler(new TouchEventHandler(this))
#if ENABLE(EVENT_MODE_METATAGS)
    , m_cursorEventMode(ProcessedCursorEvents)
    , m_touchEventMode(ProcessedTouchEvents)
#endif
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO)
    , m_scaleBeforeFullScreen(-1.0)
    , m_xScrollOffsetBeforeFullScreen(-1)
#endif
    , m_currentCursor(Platform::CursorNone)
    , m_dumpRenderTree(0) // Lazy initialization.
    , m_initialScale(-1.0)
    , m_minimumScale(-1.0)
    , m_maximumScale(-1.0)
    , m_forceRespectViewportArguments(false)
    , m_blockZoomFinalScale(1.0)
    , m_anchorInNodeRectRatio(-1, -1)
    , m_currentBlockZoomNode(0)
    , m_currentBlockZoomAdjustedNode(0)
    , m_shouldReflowBlock(false)
    , m_shouldConstrainScrollingToContentEdge(true)
    , m_lastUserEventTimestamp(0.0)
    , m_pluginMouseButtonPressed(false)
    , m_pluginMayOpenNewTab(false)
#if USE(ACCELERATED_COMPOSITING)
    , m_rootLayerCommitTimer(adoptPtr(new Timer<WebPagePrivate>(this, &WebPagePrivate::rootLayerCommitTimerFired)))
    , m_needsOneShotDrawingSynchronization(false)
    , m_needsCommit(false)
    , m_suspendRootLayerCommit(false)
#endif
    , m_pendingOrientation(-1)
    , m_fullscreenVideoNode(0)
    , m_hasInRegionScrollableAreas(false)
    , m_updateDelegatedOverlaysDispatched(false)
    , m_enableQnxJavaScriptObject(false)
    , m_deferredTasksTimer(this, &WebPagePrivate::deferredTasksTimerFired)
    , m_selectPopup(0)
    , m_autofillManager(AutofillManager::create(this))
    , m_documentStyleRecalcPostponed(false)
    , m_documentChildNeedsStyleRecalc(false)
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    , m_notificationManager(this)
#endif
{
    static bool isInitialized = false;
    if (!isInitialized) {
        isInitialized = true;
        BlackBerry::Platform::DeviceInfo::instance();
        defaultUserAgent();
    }

    AuthenticationChallengeManager::instance()->pageCreated(this);
    clearCachedHitTestResult();
}

WebPage::WebPage(WebPageClient* client, const BlackBerry::Platform::String& pageGroupName, const Platform::IntRect& rect)
{
    globalInitialize();
    d = new WebPagePrivate(this, client, rect);
    d->init(pageGroupName);
}

WebPagePrivate::~WebPagePrivate()
{
    // Hand the backingstore back to another owner if necessary.
    m_webPage->setVisible(false);
    if (BackingStorePrivate::currentBackingStoreOwner() == m_webPage)
        BackingStorePrivate::setCurrentBackingStoreOwner(0);

    delete m_webSettings;
    m_webSettings = 0;

    delete m_cookieJar;
    m_cookieJar = 0;

    delete m_webkitThreadViewportAccessor;
    m_webkitThreadViewportAccessor = 0;

    delete m_backingStoreClient;
    m_backingStoreClient = 0;
    m_backingStore = 0;

    delete m_page;
    m_page = 0;

    delete m_transformationMatrix;
    m_transformationMatrix = 0;

    delete m_inPageSearchManager;
    m_inPageSearchManager = 0;

    delete m_selectionHandler;
    m_selectionHandler = 0;

    delete m_inputHandler;
    m_inputHandler = 0;

    delete m_touchEventHandler;
    m_touchEventHandler = 0;

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    delete m_dumpRenderTree;
    m_dumpRenderTree = 0;
#endif

    AuthenticationChallengeManager::instance()->pageDeleted(this);
}

WebPage::~WebPage()
{
    deleteGuardedObject(d);
    d = 0;
}

Page* WebPagePrivate::core(const WebPage* webPage)
{
    return webPage->d->m_page;
}

void WebPagePrivate::init(const BlackBerry::Platform::String& pageGroupName)
{
    ChromeClientBlackBerry* chromeClient = new ChromeClientBlackBerry(this);
    ContextMenuClientBlackBerry* contextMenuClient = 0;
#if ENABLE(CONTEXT_MENUS)
    contextMenuClient = new ContextMenuClientBlackBerry();
#endif
    EditorClientBlackBerry* editorClient = new EditorClientBlackBerry(this);
    DragClientBlackBerry* dragClient = 0;
#if ENABLE(DRAG_SUPPORT)
    dragClient = new DragClientBlackBerry();
#endif
#if ENABLE(INSPECTOR)
    m_inspectorClient = new InspectorClientBlackBerry(this);
#endif

    FrameLoaderClientBlackBerry* frameLoaderClient = new FrameLoaderClientBlackBerry();

    Page::PageClients pageClients;
    pageClients.chromeClient = chromeClient;
    pageClients.contextMenuClient = contextMenuClient;
    pageClients.editorClient = editorClient;
    pageClients.dragClient = dragClient;
    pageClients.inspectorClient = m_inspectorClient;

    m_page = new Page(pageClients);
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (isRunningDrt()) {
        // In case running in DumpRenderTree mode set the controller to mock provider.
        GeolocationClientMock* mock = new GeolocationClientMock();
        WebCore::provideGeolocationTo(m_page, mock);
        mock->setController(WebCore::GeolocationController::from(m_page));
    } else
#endif
        WebCore::provideGeolocationTo(m_page, new GeolocationClientBlackBerry(this));
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (getenv("drtRun"))
        WebCore::provideDeviceOrientationTo(m_page, new DeviceOrientationClientMock);
    else
#endif
        WebCore::provideDeviceOrientationTo(m_page, new DeviceOrientationClientBlackBerry(this));

    WebCore::provideDeviceMotionTo(m_page, new DeviceMotionClientBlackBerry(this));
#if ENABLE(VIBRATION)
    WebCore::provideVibrationTo(m_page, new VibrationClientBlackBerry());
#endif

#if ENABLE(BATTERY_STATUS)
    WebCore::provideBatteryTo(m_page, new WebCore::BatteryClientBlackBerry(this));
#endif

#if ENABLE(MEDIA_STREAM)
    WebCore::provideUserMediaTo(m_page, new UserMediaClientImpl(m_webPage));
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::provideNotification(m_page, new NotificationClientBlackBerry(this));
#endif

#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    WebCore::provideNavigatorContentUtilsTo(m_page, new NavigatorContentUtilsClientBlackBerry(this));
#endif

#if ENABLE(NETWORK_INFO)
    WebCore::provideNetworkInfoTo(m_page, new WebCore::NetworkInfoClientBlackBerry(this));
#endif

    m_webSettings = WebSettings::createFromStandardSettings();
    m_webSettings->setUserAgentString(defaultUserAgent());
    m_page->setDeviceScaleFactor(m_webSettings->devicePixelRatio());

    m_page->addLayoutMilestones(DidFirstVisuallyNonEmptyLayout);

#if USE(ACCELERATED_COMPOSITING)
    m_tapHighlight = DefaultTapHighlight::create(this);
    m_selectionOverlay = SelectionOverlay::create(this);
    m_page->settings()->setAcceleratedCompositingForFixedPositionEnabled(true);
#endif

    // FIXME: We explicitly call setDelegate() instead of passing ourself in createFromStandardSettings()
    // so that we only get one didChangeSettings() callback when we set the page group name. This causes us
    // to make a copy of the WebSettings since some WebSettings method make use of the page group name.
    // Instead, we shouldn't be storing the page group name in WebSettings.
    m_webSettings->setPageGroupName(pageGroupName);
    m_webSettings->setDelegate(this);
    didChangeSettings(m_webSettings);

    RefPtr<Frame> newFrame = Frame::create(m_page, /* HTMLFrameOwnerElement* */ 0, frameLoaderClient);

    m_mainFrame = newFrame.get();
    frameLoaderClient->setFrame(m_mainFrame, this);
    m_mainFrame->init();

    m_inRegionScroller = adoptPtr(new InRegionScroller(this));

#if ENABLE(WEBGL)
    m_page->settings()->setWebGLEnabled(true);
#endif
#if ENABLE(ACCELERATED_2D_CANVAS)
    m_page->settings()->setCanvasUsesAcceleratedDrawing(true);
    m_page->settings()->setAccelerated2dCanvasEnabled(true);
#endif

    m_page->settings()->setInteractiveFormValidationEnabled(true);
    m_page->settings()->setAllowUniversalAccessFromFileURLs(false);
    m_page->settings()->setAllowFileAccessFromFileURLs(false);
    m_page->settings()->setFixedPositionCreatesStackingContext(true);

    m_backingStoreClient = BackingStoreClient::create(m_mainFrame, /* parent frame */ 0, m_webPage);
    // The direct access to BackingStore is left here for convenience since it
    // is owned by BackingStoreClient and then deleted by its destructor.
    m_backingStore = m_backingStoreClient->backingStore();

    m_webkitThreadViewportAccessor = new WebKitThreadViewportAccessor(this);

    blockClickRadius = int(roundf(0.35 * Platform::Graphics::Screen::primaryScreen()->pixelsPerInch(0).width())); // The clicked rectangle area should be a fixed unit of measurement.

    m_page->settings()->setDelegateSelectionPaint(true);

#if ENABLE(REQUEST_ANIMATION_FRAME)
    m_page->windowScreenDidChange((PlatformDisplayID)0);
#endif

#if ENABLE(WEB_TIMING)
    m_page->settings()->setMemoryInfoEnabled(true);
#endif

#if USE(ACCELERATED_COMPOSITING)
    // The compositor will be needed for overlay rendering, so create it
    // unconditionally. It will allocate OpenGL objects lazily, so this incurs
    // no overhead in the unlikely case where the compositor is not needed.
    Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
            createMethodCallMessage(&WebPagePrivate::createCompositor, this));
#endif
}

class DeferredTaskLoadManualScript: public DeferredTask<&WebPagePrivate::m_wouldLoadManualScript> {
public:
    explicit DeferredTaskLoadManualScript(WebPagePrivate* webPagePrivate, const KURL& url)
        : DeferredTaskType(webPagePrivate)
    {
        webPagePrivate->m_cachedManualScript = url;
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_mainFrame->script()->executeIfJavaScriptURL(webPagePrivate->m_cachedManualScript, DoNotReplaceDocumentIfJavaScriptURL);
    }
};

void WebPagePrivate::load(const BlackBerry::Platform::String& url, const BlackBerry::Platform::String& networkToken, const BlackBerry::Platform::String& method, Platform::NetworkRequest::CachePolicy cachePolicy, const char* data, size_t dataLength, const char* const* headers, size_t headersLength, bool isInitial, bool mustHandleInternally, bool forceDownload, const BlackBerry::Platform::String& overrideContentType, const BlackBerry::Platform::String& suggestedSaveName)
{
    stopCurrentLoad();
    DeferredTaskLoadManualScript::finishOrCancel(this);

    String urlString(url);
    if (urlString.startsWith("vs:", false)) {
        urlString = urlString.substring(3);
        m_mainFrame->setInViewSourceMode(true);
    } else
        m_mainFrame->setInViewSourceMode(false);

    KURL kurl = parseUrl(urlString);
    if (protocolIs(kurl, "javascript")) {
        // Never run javascript while loading is deferred.
        if (m_page->defersLoading())
            m_deferredTasks.append(adoptPtr(new DeferredTaskLoadManualScript(this, kurl)));
        else
            m_mainFrame->script()->executeIfJavaScriptURL(kurl, DoNotReplaceDocumentIfJavaScriptURL);
        return;
    }

    if (isInitial)
        NetworkManager::instance()->setInitialURL(kurl);

    ResourceRequest request(kurl);
    request.setToken(networkToken);
    if (isInitial || mustHandleInternally)
        request.setMustHandleInternally(true);
    request.setHTTPMethod(method);
    request.setCachePolicy(toWebCoreCachePolicy(cachePolicy));
    if (!overrideContentType.empty())
        request.setOverrideContentType(overrideContentType);

    if (data)
        request.setHTTPBody(FormData::create(data, dataLength));

    for (unsigned i = 0; i + 1 < headersLength; i += 2)
        request.addHTTPHeaderField(headers[i], headers[i + 1]);

    if (forceDownload)
        request.setForceDownload(true);

    request.setSuggestedSaveName(suggestedSaveName);

    FrameLoadRequest frameRequest(m_mainFrame, request);
    frameRequest.setFrameName("");
    frameRequest.setShouldCheckNewWindowPolicy(true);
    m_mainFrame->loader()->load(frameRequest);
}

void WebPage::load(const BlackBerry::Platform::String& url, const BlackBerry::Platform::String& networkToken, bool isInitial)
{
    d->load(url, networkToken, "GET", Platform::NetworkRequest::UseProtocolCachePolicy, 0, 0, 0, 0, isInitial, false);
}

void WebPage::loadExtended(const char* url, const char* networkToken, const char* method, Platform::NetworkRequest::CachePolicy cachePolicy, const char* data, size_t dataLength, const char* const* headers, size_t headersLength, bool mustHandleInternally)
{
    d->load(url, networkToken, method, cachePolicy, data, dataLength, headers, headersLength, false, mustHandleInternally, false, "");
}

void WebPage::loadFile(const BlackBerry::Platform::String& path, const BlackBerry::Platform::String& overrideContentType)
{
    BlackBerry::Platform::String fileUrl(path);
    if (fileUrl.startsWith("/"))
        fileUrl = BlackBerry::Platform::String("file://", 7) + fileUrl;
    else if (!fileUrl.startsWith("file:///"))
        return;

    d->load(fileUrl, BlackBerry::Platform::String::emptyString(), BlackBerry::Platform::String("GET", 3), Platform::NetworkRequest::UseProtocolCachePolicy, 0, 0, 0, 0, false, false, false, overrideContentType.c_str());
}

void WebPage::download(const Platform::NetworkRequest& request)
{
    vector<const char*> headers;
    Platform::NetworkRequest::HeaderList& list = request.getHeaderListRef();
    for (unsigned i = 0; i < list.size(); i++) {
        headers.push_back(list[i].first.c_str());
        headers.push_back(list[i].second.c_str());
    }
    d->load(request.getUrlRef(), BlackBerry::Platform::String::emptyString(), "GET", Platform::NetworkRequest::UseProtocolCachePolicy, 0, 0, headers.empty() ? 0 : &headers[0], headers.size(), false, false, true, "", request.getSuggestedSaveName().c_str());
}

void WebPagePrivate::loadString(const BlackBerry::Platform::String& string, const BlackBerry::Platform::String& baseURL, const BlackBerry::Platform::String& contentType, const BlackBerry::Platform::String& failingURL)
{
    KURL kurl = parseUrl(baseURL);
    ResourceRequest request(kurl);
    WTF::RefPtr<SharedBuffer> buffer
        = SharedBuffer::create(string.c_str(), string.length());
    SubstituteData substituteData(buffer,
        extractMIMETypeFromMediaType(contentType),
        extractCharsetFromMediaType(contentType),
        !failingURL.empty() ? parseUrl(failingURL) : KURL());
    m_mainFrame->loader()->load(FrameLoadRequest(m_mainFrame, request, substituteData));
}

void WebPage::loadString(const BlackBerry::Platform::String& string, const BlackBerry::Platform::String& baseURL, const BlackBerry::Platform::String& mimeType, const BlackBerry::Platform::String& failingURL)
{
    d->loadString(string, baseURL, mimeType, failingURL);
}

bool WebPagePrivate::executeJavaScript(const BlackBerry::Platform::String& scriptUTF8, JavaScriptDataType& returnType, WebString& returnValue)
{
    BLACKBERRY_ASSERT(scriptUTF8.isUtf8());
    String script = scriptUTF8;

    if (script.isNull()) {
        returnType = JSException;
        return false;
    }

    if (script.isEmpty()) {
        returnType = JSUndefined;
        return true;
    }

    ScriptValue result = m_mainFrame->script()->executeScript(script, false);
    JSC::JSValue value = result.jsValue();
    if (!value) {
        returnType = JSException;
        return false;
    }

    if (value.isUndefined())
        returnType = JSUndefined;
    else if (value.isNull())
        returnType = JSNull;
    else if (value.isBoolean())
        returnType = JSBoolean;
    else if (value.isNumber())
        returnType = JSNumber;
    else if (value.isString())
        returnType = JSString;
    else if (value.isObject())
        returnType = JSObject;
    else
        returnType = JSUndefined;

    if (returnType == JSBoolean || returnType == JSNumber || returnType == JSString || returnType == JSObject) {
        JSC::ExecState* exec = m_mainFrame->script()->globalObject(mainThreadNormalWorld())->globalExec();
        returnValue = result.toString(exec);
    }

    return true;
}

bool WebPage::executeJavaScript(const BlackBerry::Platform::String& script, JavaScriptDataType& returnType, BlackBerry::Platform::String& returnValue)
{
    return d->executeJavaScript(script, returnType, returnValue);
}

bool WebPagePrivate::executeJavaScriptInIsolatedWorld(const ScriptSourceCode& sourceCode, JavaScriptDataType& returnType, BlackBerry::Platform::String& returnValue)
{
    if (!m_isolatedWorld)
        m_isolatedWorld = m_mainFrame->script()->createWorld();

    // Use evaluateInWorld to avoid canExecuteScripts check.
    ScriptValue result = m_mainFrame->script()->evaluateInWorld(sourceCode, m_isolatedWorld.get());
    JSC::JSValue value = result.jsValue();
    if (!value) {
        returnType = JSException;
        return false;
    }

    if (value.isUndefined())
        returnType = JSUndefined;
    else if (value.isNull())
        returnType = JSNull;
    else if (value.isBoolean())
        returnType = JSBoolean;
    else if (value.isNumber())
        returnType = JSNumber;
    else if (value.isString())
        returnType = JSString;
    else if (value.isObject())
        returnType = JSObject;
    else
        returnType = JSUndefined;

    if (returnType == JSBoolean || returnType == JSNumber || returnType == JSString || returnType == JSObject) {
        JSC::ExecState* exec = m_mainFrame->script()->globalObject(mainThreadNormalWorld())->globalExec();
        returnValue = result.toString(exec);
    }

    return true;
}

bool WebPage::executeJavaScriptInIsolatedWorld(const std::wstring& script, JavaScriptDataType& returnType, BlackBerry::Platform::String& returnValue)
{
    // On our platform wchar_t is unsigned int and UChar is unsigned short
    // so we have to convert using ICU conversion function
    int lengthCopied = 0;
    UErrorCode error = U_ZERO_ERROR;
    const int length = script.length() + 1 /*null termination char*/;
    UChar data[length];

    // FIXME: PR 138162 is giving U_INVALID_CHAR_FOUND error.
    u_strFromUTF32(data, length, &lengthCopied, reinterpret_cast<const UChar32*>(script.c_str()), script.length(), &error);
    BLACKBERRY_ASSERT(error == U_ZERO_ERROR);
    if (error != U_ZERO_ERROR) {
        Platform::logAlways(Platform::LogLevelCritical, "WebPage::executeJavaScriptInIsolatedWorld failed to convert UTF16 to JavaScript!");
        return false;
    }
    String str = String(data, lengthCopied);
    ScriptSourceCode sourceCode(str, KURL());
    return d->executeJavaScriptInIsolatedWorld(sourceCode, returnType, returnValue);
}

bool WebPage::executeJavaScriptInIsolatedWorld(const BlackBerry::Platform::String& scriptUTF8, JavaScriptDataType& returnType, BlackBerry::Platform::String& returnValue)
{
    BLACKBERRY_ASSERT(scriptUTF8.isUtf8());
    ScriptSourceCode sourceCode(scriptUTF8, KURL());
    return d->executeJavaScriptInIsolatedWorld(sourceCode, returnType, returnValue);
}

void WebPage::executeJavaScriptFunction(const std::vector<BlackBerry::Platform::String> &function, const std::vector<JavaScriptVariant> &args, JavaScriptVariant& returnValue)
{
    if (!d->m_mainFrame) {
        returnValue.setType(JavaScriptVariant::Exception);
        return;
    }

    JSC::Bindings::RootObject* root = d->m_mainFrame->script()->bindingRootObject();
    if (!root) {
        returnValue.setType(JavaScriptVariant::Exception);
        return;
    }

    JSC::ExecState* exec = root->globalObject()->globalExec();
    JSGlobalContextRef ctx = toGlobalRef(exec);

    JSC::JSLockHolder lock(exec);
    WTF::Vector<JSValueRef> argListRef(args.size());
    for (unsigned i = 0; i < args.size(); ++i)
        argListRef[i] = BlackBerryJavaScriptVariantToJSValueRef(ctx, args[i]);

    JSValueRef windowObjectValue = windowObject();
    JSObjectRef obj = JSValueToObject(ctx, windowObjectValue, 0);
    JSObjectRef thisObject = obj;
    for (unsigned i = 0; i < function.size(); ++i) {
        JSStringRef str = JSStringCreateWithUTF8CString(function[i].c_str());
        thisObject = obj;
        obj = JSValueToObject(ctx, JSObjectGetProperty(ctx, obj, str, 0), 0);
        JSStringRelease(str);
        if (!obj)
            break;
    }

    JSObjectRef functionObject = obj;
    JSValueRef result = 0;
    if (functionObject && thisObject)
        result = JSObjectCallAsFunction(ctx, functionObject, thisObject, args.size(), argListRef.data(), 0);

    if (!result) {
        returnValue.setType(JavaScriptVariant::Exception);
        return;
    }

    returnValue = JSValueRefToBlackBerryJavaScriptVariant(ctx, result);
}

void WebPagePrivate::stopCurrentLoad()
{
    // This function should contain all common code triggered by WebPage::load
    // (which stops any load in progress before starting the new load) and
    // WebPage::stoploading (the entry point for the client to stop the load
    // explicitly). If it should only be done while stopping the load
    // explicitly, it goes in WebPage::stopLoading, not here.
    m_mainFrame->loader()->stopAllLoaders();

    // Cancel any deferred script that hasn't been processed yet.
    DeferredTaskLoadManualScript::finishOrCancel(this);
}

void WebPage::stopLoading()
{
    d->stopCurrentLoad();
}

static void closeURLRecursively(Frame* frame)
{
    // Do not create more frame please.
    FrameLoaderClientBlackBerry* frameLoaderClient = static_cast<FrameLoaderClientBlackBerry*>(frame->loader()->client());
    frameLoaderClient->suppressChildFrameCreation();

    frame->loader()->closeURL();

    Vector<RefPtr<Frame>, 10> childFrames;

    for (RefPtr<Frame> childFrame = frame->tree()->firstChild(); childFrame; childFrame = childFrame->tree()->nextSibling())
        childFrames.append(childFrame);

    unsigned size = childFrames.size();
    for (unsigned i = 0; i < size; i++)
        closeURLRecursively(childFrames[i].get());
}

void WebPagePrivate::prepareToDestroy()
{
    // Before the client starts tearing itself down, dispatch the unload event
    // so it can take effect while all the client's state (e.g. scroll position)
    // is still present.
    closeURLRecursively(m_mainFrame);
}

void WebPage::prepareToDestroy()
{
    d->prepareToDestroy();
}

bool WebPage::dispatchBeforeUnloadEvent()
{
    return d->m_page->mainFrame()->loader()->shouldClose();
}

static void enableCrossSiteXHRRecursively(Frame* frame)
{
    frame->document()->securityOrigin()->grantUniversalAccess();

    Vector<RefPtr<Frame>, 10> childFrames;
    for (RefPtr<Frame> childFrame = frame->tree()->firstChild(); childFrame; childFrame = childFrame->tree()->nextSibling())
        childFrames.append(childFrame);

    unsigned size = childFrames.size();
    for (unsigned i = 0; i < size; i++)
        enableCrossSiteXHRRecursively(childFrames[i].get());
}

void WebPagePrivate::enableCrossSiteXHR()
{
    enableCrossSiteXHRRecursively(m_mainFrame);
}

void WebPage::enableCrossSiteXHR()
{
    d->enableCrossSiteXHR();
}

void WebPagePrivate::addOriginAccessWhitelistEntry(const BlackBerry::Platform::String& sourceOrigin, const BlackBerry::Platform::String& destinationOrigin, bool allowDestinationSubdomains)
{
    RefPtr<SecurityOrigin> source = SecurityOrigin::createFromString(sourceOrigin);
    if (source->isUnique())
        return;

    KURL destination(KURL(), destinationOrigin);
    SecurityPolicy::addOriginAccessWhitelistEntry(*source, destination.protocol(), destination.host(), allowDestinationSubdomains);
}

void WebPage::addOriginAccessWhitelistEntry(const BlackBerry::Platform::String& sourceOrigin, const BlackBerry::Platform::String& destinationOrigin, bool allowDestinationSubdomains)
{
    d->addOriginAccessWhitelistEntry(sourceOrigin, destinationOrigin, allowDestinationSubdomains);
}

void WebPagePrivate::removeOriginAccessWhitelistEntry(const BlackBerry::Platform::String& sourceOrigin, const BlackBerry::Platform::String& destinationOrigin, bool allowDestinationSubdomains)
{
    RefPtr<SecurityOrigin> source = SecurityOrigin::createFromString(sourceOrigin);
    if (source->isUnique())
        return;

    KURL destination(KURL(), destinationOrigin);
    SecurityPolicy::removeOriginAccessWhitelistEntry(*source, destination.protocol(), destination.host(), allowDestinationSubdomains);
}

void WebPage::removeOriginAccessWhitelistEntry(const BlackBerry::Platform::String& sourceOrigin, const BlackBerry::Platform::String& destinationOrigin, bool allowDestinationSubdomains)
{
    d->removeOriginAccessWhitelistEntry(sourceOrigin, destinationOrigin, allowDestinationSubdomains);
}

void WebPagePrivate::setLoadState(LoadState state)
{
    if (m_loadState == state)
        return;

    bool isFirstLoad = m_loadState == None;

    // See RIM Bug #1068.
    if (state == Finished && m_mainFrame && m_mainFrame->document())
        m_mainFrame->document()->updateStyleIfNeeded();

    // Dispatch the backingstore background color at important state changes.
    m_backingStore->d->setWebPageBackgroundColor(m_mainFrame && m_mainFrame->view()
        ? m_mainFrame->view()->documentBackgroundColor()
        : m_webSettings->backgroundColor());

    m_loadState = state;

#if DEBUG_WEBPAGE_LOAD
    BBLOG(Platform::LogLevelInfo, "WebPagePrivate::setLoadState %d", state);
#endif

    switch (m_loadState) {
    case Provisional:
        if (isFirstLoad) {
            // Paints the visible backingstore as white to prevent initial checkerboard on
            // the first blit.
            if (m_backingStore->d->renderVisibleContents() && !m_backingStore->d->isSuspended() && !m_backingStore->d->shouldDirectRenderingToWindow())
                m_backingStore->d->blitVisibleContents();
        }
        break;
    case Committed:
        {
#if ENABLE(ACCELERATED_2D_CANVAS)
            if (m_page->settings()->canvasUsesAcceleratedDrawing()) {
                // Free GPU resources as we're on a new page.
                // This will help us to free memory pressure.
                SharedGraphicsContext3D::get()->makeContextCurrent();
                GrContext* grContext = Platform::Graphics::getGrContext();
                grContext->freeGpuResources();
            }
#endif

#if USE(ACCELERATED_COMPOSITING)
            releaseLayerResources();
#endif

            // Suspend screen update to avoid ui thread blitting while resetting backingstore.
            // FIXME: Do we really need to suspend/resume both backingstore and screen here?
            m_backingStore->d->suspendBackingStoreUpdates();
            m_backingStore->d->suspendScreenUpdates();

            m_previousContentsSize = IntSize();
            m_backingStore->d->resetRenderQueue();
            m_backingStore->d->resetTiles();
            m_backingStore->d->setScrollingOrZooming(false, false /* shouldBlit */);
            m_shouldZoomToInitialScaleAfterLoadFinished = false;
            m_userPerformedManualZoom = false;
            m_userPerformedManualScroll = false;
            m_shouldUseFixedDesktopMode = false;
            m_forceRespectViewportArguments = false;
            if (m_resetVirtualViewportOnCommitted) // For DRT.
                m_virtualViewportSize = IntSize();
            if (m_webSettings->viewportWidth() > 0)
                m_virtualViewportSize = IntSize(m_webSettings->viewportWidth(), m_defaultLayoutSize.height());

            // Check if we have already process the meta viewport tag, this only happens on history navigation.
            // For back/forward history navigation, we should only keep these previous values if the document
            // has the meta viewport tag when the state is Committed in setLoadState.
            static ViewportArguments defaultViewportArguments;
            bool documentHasViewportArguments = false;
            if (m_mainFrame && m_mainFrame->document() && m_mainFrame->document()->viewportArguments() != defaultViewportArguments)
                documentHasViewportArguments = true;
            if (!(m_didRestoreFromPageCache && documentHasViewportArguments)) {
                m_viewportArguments = ViewportArguments();
                m_userScalable = m_webSettings->isUserScalable();
                resetScales();

                // At the moment we commit a new load, set the viewport arguments
                // to any fallback values. If there is a meta viewport in the
                // content it will overwrite the fallback arguments soon.
                dispatchViewportPropertiesDidChange(m_userViewportArguments);
                if (m_userViewportArguments != defaultViewportArguments)
                    m_forceRespectViewportArguments = true;
            } else {
                Platform::IntSize virtualViewport = recomputeVirtualViewportFromViewportArguments();
                m_webPage->setVirtualViewportSize(virtualViewport);
            }

#if ENABLE(EVENT_MODE_METATAGS)
            didReceiveCursorEventMode(ProcessedCursorEvents);
            didReceiveTouchEventMode(ProcessedTouchEvents);
#endif

            // Reset block zoom and reflow.
            resetBlockZoom();
#if ENABLE(VIEWPORT_REFLOW)
            toggleTextReflowIfEnabledForBlockZoomOnly();
#endif

            // Notify InputHandler of state change.
            m_inputHandler->setInputModeEnabled(false);

            // Set the scroll to origin here and notify the client since we'll be
            // zooming below without any real contents yet thus the contents size
            // we report to the client could make our current scroll position invalid.
            setScrollPosition(IntPoint::zero());
            notifyTransformedScrollChanged();

            // FIXME: Do we really need to suspend/resume both backingstore and screen here?
            m_backingStore->d->resumeBackingStoreUpdates();
            m_backingStore->d->resumeScreenUpdates(BackingStore::None);

            // Paints the visible backingstore as white. Note it is important we do
            // this strictly after re-setting the scroll position to origin and resetting
            // the scales otherwise the visible contents calculation is wrong and we
            // can end up blitting artifacts instead. See: RIM Bug #401.
            if (m_backingStore->d->renderVisibleContents() && !m_backingStore->d->isSuspended() && !m_backingStore->d->shouldDirectRenderingToWindow())
                m_backingStore->d->blitVisibleContents();

            // Update cursor status.
            updateCursor();

            break;
        }
    case Finished:
    case Failed:
        // Notify client of the initial zoom change.
        m_client->scaleChanged();
        m_backingStore->d->updateTiles(true /* updateVisible */, false /* immediate */);
        break;
    default:
        break;
    }
}

double WebPagePrivate::clampedScale(double scale) const
{
    if (scale < minimumScale())
        return minimumScale();
    if (scale > maximumScale())
        return maximumScale();
    return scale;
}

bool WebPagePrivate::shouldZoomAboutPoint(double scale, const FloatPoint&, bool enforceScaleClamping, double* clampedScale)
{
    if (!m_mainFrame->view())
        return false;

    if (enforceScaleClamping)
        scale = this->clampedScale(scale);

    ASSERT(clampedScale);
    *clampedScale = scale;

    if (currentScale() == scale)
        return false;

    return true;
}

bool WebPagePrivate::zoomAboutPoint(double unclampedScale, const FloatPoint& anchor, bool enforceScaleClamping, bool forceRendering, bool isRestoringZoomLevel)
{
    if (!isRestoringZoomLevel) {
        // Clear any existing block zoom.  (If we are restoring a saved zoom level on page load,
        // there is guaranteed to be no existing block zoom and we don't want to clear m_shouldReflowBlock.)
        resetBlockZoom();
    }

    // The reflow and block zoom stuff here needs to happen regardless of
    // whether we shouldZoomAboutPoint.
#if ENABLE(VIEWPORT_REFLOW)
    toggleTextReflowIfEnabledForBlockZoomOnly(m_shouldReflowBlock);
    if (m_page->settings()->isTextReflowEnabled() && m_mainFrame->view())
        setNeedsLayout();
#endif

    double scale;
    if (!shouldZoomAboutPoint(unclampedScale, anchor, enforceScaleClamping, &scale)) {
        if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled) {
            m_currentPinchZoomNode = 0;
            m_anchorInNodeRectRatio = FloatPoint(-1, -1);
        }
        return false;
    }
    TransformationMatrix zoom;
    zoom.scale(scale);

#if DEBUG_WEBPAGE_LOAD
    if (loadState() < Finished)
        BBLOG(Platform::LogLevelInfo, "WebPagePrivate::zoomAboutPoint scale %f anchor (%f, %f)", scale, anchor.x(), anchor.y());
#endif

    // Our current scroll position in float.
    FloatPoint scrollPosition = this->scrollPosition();

    // Anchor offset from scroll position in float.
    FloatPoint anchorOffset(anchor.x() - scrollPosition.x(), anchor.y() - scrollPosition.y());

    // The horizontal scaling factor and vertical scaling factor should be equal
    // to preserve aspect ratio of content.
    ASSERT(m_transformationMatrix->m11() == m_transformationMatrix->m22());

    // Need to invert the previous transform to anchor the viewport.
    double inverseScale = scale / m_transformationMatrix->m11();

    // Actual zoom.
    *m_transformationMatrix = zoom;

    // Suspend all screen updates to the backingstore.
    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    m_backingStore->d->suspendBackingStoreUpdates();
    m_backingStore->d->suspendScreenUpdates();

    updateViewportSize();

    IntPoint newScrollPosition(IntPoint(max(0, static_cast<int>(roundf(anchor.x() - anchorOffset.x() / inverseScale))),
                                        max(0, static_cast<int>(roundf(anchor.y() - anchorOffset.y() / inverseScale)))));

    if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled) {
        // This is a hack for email which has reflow always turned on.
        m_mainFrame->view()->setNeedsLayout();
        requestLayoutIfNeeded();
        if (m_currentPinchZoomNode)
            newScrollPosition = calculateReflowedScrollPosition(anchorOffset, scale == minimumScale() ? 1 : inverseScale);
         m_currentPinchZoomNode = 0;
         m_anchorInNodeRectRatio = FloatPoint(-1, -1);
    }

    setScrollPosition(newScrollPosition);

    notifyTransformChanged();

    bool isLoading = this->isLoading();

    // We need to invalidate all tiles both visible and non-visible if we're loading.
    m_backingStore->d->updateTiles(isLoading /* updateVisible */, false /* immediate */);

    bool shouldRender = !isLoading || m_userPerformedManualZoom || forceRendering;
    bool shouldClearVisibleZoom = isLoading && shouldRender;

    if (shouldClearVisibleZoom) {
        // If we are loading and rendering then we need to clear the render queue's
        // visible zoom jobs as they will be irrelevant with the render below.
        m_backingStore->d->clearVisibleZoom();
    }

    m_client->scaleChanged();

    if (m_pendingOrientation != -1)
        m_client->updateInteractionViews();

    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    m_backingStore->d->resumeBackingStoreUpdates();

    // Clear window to make sure there are no artifacts.
    if (shouldRender) {
        // Resume all screen updates to the backingstore and render+blit visible contents to screen.
        m_backingStore->d->resumeScreenUpdates(BackingStore::RenderAndBlit);
    } else {
        // Resume all screen updates to the backingstore but do not blit to the screen because we not rendering.
        m_backingStore->d->resumeScreenUpdates(BackingStore::None);
    }

    return true;
}

IntPoint WebPagePrivate::calculateReflowedScrollPosition(const FloatPoint& anchorOffset, double inverseScale)
{
    // Should only be invoked when text reflow is enabled.
    ASSERT(m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled);

    int offsetY = 0;
    int offsetX = 0;

    IntRect nodeRect = rectForNode(m_currentPinchZoomNode.get());

    if (m_currentPinchZoomNode->renderer() && m_anchorInNodeRectRatio.y() >= 0) {
        offsetY = nodeRect.height() * m_anchorInNodeRectRatio.y();
        if (m_currentPinchZoomNode->renderer()->isImage() && m_anchorInNodeRectRatio.x() > 0)
            offsetX = nodeRect.width() * m_anchorInNodeRectRatio.x() - anchorOffset.x() / inverseScale;
    }

    IntRect reflowedRect = adjustRectOffsetForFrameOffset(nodeRect, m_currentPinchZoomNode.get());

    return IntPoint(max(0, static_cast<int>(roundf(reflowedRect.x() + offsetX))),
                    max(0, static_cast<int>(roundf(reflowedRect.y() + offsetY - anchorOffset.y() / inverseScale))));
}

void WebPagePrivate::setNeedsLayout()
{
    FrameView* view = m_mainFrame->view();
    ASSERT(view);
    view->setNeedsLayout();
}

void WebPagePrivate::requestLayoutIfNeeded() const
{
    FrameView* view = m_mainFrame->view();
    ASSERT(view);
    view->updateLayoutAndStyleIfNeededRecursive();
    ASSERT(!view->needsLayout());
}

IntPoint WebPagePrivate::scrollPosition() const
{
    return m_backingStoreClient->scrollPosition();
}

IntPoint WebPagePrivate::maximumScrollPosition() const
{
    return m_backingStoreClient->maximumScrollPosition();
}

void WebPagePrivate::setScrollPosition(const IntPoint& pos)
{
    m_backingStoreClient->setScrollPosition(pos);
}

// Setting the scroll position is in transformed coordinates.
void WebPage::setDocumentScrollPosition(const Platform::IntPoint& documentScrollPosition)
{
    WebCore::IntPoint scrollPosition = documentScrollPosition;
    if (scrollPosition == d->scrollPosition())
        return;

    // If the user recently performed an event, this new scroll position
    // could possibly be a result of that. Or not, this is just a heuristic.
    if (currentTime() - d->m_lastUserEventTimestamp < manualScrollInterval)
        d->m_userPerformedManualScroll = true;

    d->m_backingStoreClient->setIsClientGeneratedScroll(true);

    // UI thread can call BackingStorePrivate::setScrollingOrZooming(false) before WebKit thread calls WebPage::setScrollPosition(),
    // in which case it will set ScrollableArea::m_constrainsScrollingToContentEdge to true earlier.
    // We can cache ScrollableArea::m_constrainsScrollingToContentEdge and always set it to false before we set scroll position in
    // WebKit thread to avoid scroll position clamping during scrolling, and restore it to what it was after that.
    bool constrainsScrollingToContentEdge = d->m_mainFrame->view()->constrainsScrollingToContentEdge();
    d->m_mainFrame->view()->setConstrainsScrollingToContentEdge(false);
    d->setScrollPosition(scrollPosition);
    d->m_mainFrame->view()->setConstrainsScrollingToContentEdge(constrainsScrollingToContentEdge);

    d->m_backingStoreClient->setIsClientGeneratedScroll(false);
}

bool WebPagePrivate::shouldSendResizeEvent()
{
    if (!m_mainFrame->document())
        return false;

    // PR#96865 : Provide an option to always send resize events, regardless of the loading
    //            status. The scenario for this are Sapphire applications which tend to
    //            maintain an open GET request to the server. This open GET results in
    //            webkit thinking that content is still arriving when at the application
    //            level it is considered fully loaded.
    //
    //            NOTE: Care must be exercised in the use of this option, as it bypasses
    //                  the sanity provided in 'isLoadingInAPISense()' below.
    //
    static const bool unrestrictedResizeEvents = Platform::Settings::instance()->unrestrictedResizeEvents();
    if (unrestrictedResizeEvents)
        return true;

    // Don't send the resize event if the document is loading. Some pages automatically reload
    // when the window is resized; Safari on iPhone often resizes the window while setting up its
    // viewport. This obviously can cause problems.
    DocumentLoader* documentLoader = m_mainFrame->loader()->documentLoader();
    if (documentLoader && documentLoader->isLoadingInAPISense())
        return false;

    return true;
}

void WebPagePrivate::willDeferLoading()
{
    m_deferredTasksTimer.stop();
    m_client->willDeferLoading();
}

void WebPagePrivate::didResumeLoading()
{
    if (!m_deferredTasks.isEmpty())
        m_deferredTasksTimer.startOneShot(0);
    m_client->didResumeLoading();
}

void WebPagePrivate::deferredTasksTimerFired(WebCore::Timer<WebPagePrivate>*)
{
    ASSERT(!m_deferredTasks.isEmpty());
    if (!m_deferredTasks.isEmpty())
        return;

    OwnPtr<DeferredTaskBase> task = m_deferredTasks[0].release();
    m_deferredTasks.remove(0);

    if (!m_deferredTasks.isEmpty())
        m_deferredTasksTimer.startOneShot(0);

    task->perform(this);
}

void WebPagePrivate::notifyInRegionScrollStopped()
{
    if (m_inRegionScroller->d->isActive()) {
        // Notify the client side to clear InRegion scrollable areas before we destroy them here.
        std::vector<Platform::ScrollViewBase*> emptyInRegionScrollableAreas;
        m_client->notifyInRegionScrollableAreasChanged(emptyInRegionScrollableAreas);
        m_inRegionScroller->d->reset();
    }
}

void WebPage::notifyInRegionScrollStopped()
{
    d->notifyInRegionScrollStopped();
}

void WebPagePrivate::setHasInRegionScrollableAreas(bool b)
{
    if (b != m_hasInRegionScrollableAreas)
        m_hasInRegionScrollableAreas = b;
}

IntSize WebPagePrivate::viewportSize() const
{
    return mapFromTransformed(transformedViewportSize());
}

IntSize WebPagePrivate::actualVisibleSize() const
{
    return mapFromTransformed(transformedActualVisibleSize());
}

bool WebPagePrivate::hasVirtualViewport() const
{
    return !m_virtualViewportSize.isEmpty();
}

void WebPagePrivate::updateViewportSize(bool setFixedReportedSize, bool sendResizeEvent)
{
    // This checks to make sure we're not calling updateViewportSize
    // during WebPagePrivate::init().
    if (!m_backingStore)
        return;
    ASSERT(m_mainFrame->view());
    IntSize visibleSize = actualVisibleSize();
    if (setFixedReportedSize)
        m_mainFrame->view()->setFixedReportedSize(visibleSize);

    IntRect frameRect = IntRect(scrollPosition(), visibleSize);
    if (frameRect != m_mainFrame->view()->frameRect()) {
        m_mainFrame->view()->setFrameRect(frameRect);
        m_mainFrame->view()->adjustViewSize();

#if ENABLE(FULLSCREEN_API)
        // If we are in fullscreen video mode, and we change the FrameView::viewportRect,
        // we need to adjust the media container to the new size.
        if (m_fullscreenVideoNode) {
            Document* document = m_fullscreenVideoNode->document();
            ASSERT(document);
            ASSERT(document->fullScreenRenderer());

            int width = m_mainFrame->view()->visibleContentRect().size().width();
            document->fullScreenRenderer()->style()->setWidth(Length(width, Fixed));
        }
#endif
    }

    // We're going to need to send a resize event to JavaScript because
    // innerWidth and innerHeight depend on fixed reported size.
    // This is how we support mobile pages where JavaScript resizes
    // the page in order to get around the fixed layout size, e.g.
    // google maps when it detects a mobile user agent.
    if (sendResizeEvent && shouldSendResizeEvent())
        m_mainFrame->eventHandler()->sendResizeEvent();

    // When the actual visible size changes, we also
    // need to reposition fixed elements.
    m_mainFrame->view()->repaintFixedElementsAfterScrolling();
}

FloatPoint WebPagePrivate::centerOfVisibleContentsRect() const
{
    // The visible contents rect in float.
    FloatRect visibleContentsRect = this->visibleContentsRect();

    // The center of the visible contents rect in float.
    return FloatPoint(visibleContentsRect.x() + visibleContentsRect.width() / 2.0,
                      visibleContentsRect.y() + visibleContentsRect.height() / 2.0);
}

IntRect WebPagePrivate::visibleContentsRect() const
{
    return m_backingStoreClient->visibleContentsRect();
}

IntSize WebPagePrivate::contentsSize() const
{
    if (!m_mainFrame->view())
        return IntSize();

    return m_backingStoreClient->contentsSize();
}

IntSize WebPagePrivate::absoluteVisibleOverflowSize() const
{
    if (!m_mainFrame->contentRenderer())
        return IntSize();

    return IntSize(m_mainFrame->contentRenderer()->rightAbsoluteVisibleOverflow(), m_mainFrame->contentRenderer()->bottomAbsoluteVisibleOverflow());
}

void WebPagePrivate::contentsSizeChanged(const IntSize& contentsSize)
{
    if (m_previousContentsSize == contentsSize)
        return;

    // This should only occur in the middle of layout so we set a flag here and
    // handle it at the end of the layout.
    m_contentsSizeChanged = true;

#if DEBUG_WEBPAGE_LOAD
    BBLOG(Platform::LogLevelInfo, "WebPagePrivate::contentsSizeChanged %dx%d", contentsSize.width(), contentsSize.height());
#endif
}

void WebPagePrivate::layoutFinished()
{
    if (!m_contentsSizeChanged && !m_overflowExceedsContentsSize)
        return;

    m_contentsSizeChanged = false; // Toggle to turn off notification again.
    m_overflowExceedsContentsSize = false;

    if (contentsSize().isEmpty())
        return;

    // The call to zoomToInitialScaleOnLoad can cause recursive layout when called from
    // the middle of a layout, but the recursion is limited by detection code in
    // setViewMode() and mitigation code in fixedLayoutSize().
    if (didLayoutExceedMaximumIterations()) {
        notifyTransformedContentsSizeChanged();
        return;
    }

    // Temporarily save the m_previousContentsSize here before updating it (in
    // notifyTransformedContentsSizeChanged()) so we can compare if our contents
    // shrunk afterwards.
    IntSize previousContentsSize = m_previousContentsSize;

    m_nestedLayoutFinishedCount++;

    if (shouldZoomToInitialScaleOnLoad()) {
        zoomToInitialScaleOnLoad();
        m_shouldZoomToInitialScaleAfterLoadFinished = false;
    } else if (loadState() != None)
        notifyTransformedContentsSizeChanged();

    m_nestedLayoutFinishedCount--;

    if (!m_nestedLayoutFinishedCount) {
        // When the contents shrinks, there is a risk that we
        // will be left at a scroll position that lies outside of the
        // contents rect. Since we allow overscrolling and neglect
        // to clamp overscroll in order to retain input focus (RIM Bug #414)
        // we need to clamp somewhere, and this is where we know the
        // contents size has changed.

        if (contentsSize() != previousContentsSize) {

            IntPoint newScrollPosition = scrollPosition();

            if (contentsSize().height() < previousContentsSize.height()) {
                IntPoint scrollPositionWithHeightShrunk = IntPoint(newScrollPosition.x(), maximumScrollPosition().y());
                newScrollPosition = newScrollPosition.shrunkTo(scrollPositionWithHeightShrunk);
            }

            if (contentsSize().width() < previousContentsSize.width()) {
                IntPoint scrollPositionWithWidthShrunk = IntPoint(maximumScrollPosition().x(), newScrollPosition.y());
                newScrollPosition = newScrollPosition.shrunkTo(scrollPositionWithWidthShrunk);
            }

            if (newScrollPosition != scrollPosition()) {
                setScrollPosition(newScrollPosition);
                notifyTransformedScrollChanged();
            }
        }
    }
}

void WebPagePrivate::zoomToInitialScaleOnLoad()
{
#if DEBUG_WEBPAGE_LOAD
    BBLOG(Platform::LogLevelInfo, "WebPagePrivate::zoomToInitialScaleOnLoad");
#endif

    bool needsLayout = false;

    // If the contents width exceeds the viewport width set to desktop mode.
    if (m_shouldUseFixedDesktopMode)
        needsLayout = setViewMode(FixedDesktop);
    else
        needsLayout = setViewMode(Desktop);

    if (needsLayout) {
        // This can cause recursive layout...
        setNeedsLayout();
    }

    if (contentsSize().isEmpty()) {
#if DEBUG_WEBPAGE_LOAD
        BBLOG(Platform::LogLevelInfo, "WebPagePrivate::zoomToInitialScaleOnLoad content is empty!");
#endif
        requestLayoutIfNeeded();
        notifyTransformedContentsSizeChanged();
        return;
    }

    bool performedZoom = false;
    bool shouldZoom = !m_userPerformedManualZoom;

    // If this is a back/forward type navigation, don't zoom to initial scale
    // but instead let the HistoryItem's saved viewport reign supreme.
    if (m_mainFrame && m_mainFrame->loader() && isBackForwardLoadType(m_mainFrame->loader()->loadType()))
        shouldZoom = false;

    if (shouldZoom && shouldZoomToInitialScaleOnLoad()) {
        // Preserve at top and at left position, to avoid scrolling
        // to a non top-left position for web page with viewport meta tag
        // that specifies an initial-scale that is zoomed in.
        FloatPoint anchor = centerOfVisibleContentsRect();
        if (!scrollPosition().x())
            anchor.setX(0);
        if (!scrollPosition().y())
            anchor.setY(0);
        performedZoom = zoomAboutPoint(initialScale(), anchor);
    }

    // zoomAboutPoint above can also toggle setNeedsLayout and cause recursive layout...
    requestLayoutIfNeeded();

    if (!performedZoom) {
        // We only notify if we didn't perform zoom, because zoom will notify on
        // its own...
        notifyTransformedContentsSizeChanged();
    }
}

double WebPagePrivate::zoomToFitScale() const
{
    int contentWidth = contentsSize().width();

    // For image document, zoom to fit the screen based on the actual image width
    // instead of the contents width within a maximum scale .
    Document* doc = m_page->mainFrame()->document();
    bool isImageDocument = doc && doc->isImageDocument();
    if (isImageDocument)
        contentWidth = static_cast<ImageDocument*>(doc)->imageSize().width();

    double zoomToFitScale = contentWidth > 0.0 ? static_cast<double>(m_actualVisibleWidth) / contentWidth : 1.0;
    int contentHeight = contentsSize().height();
    if (contentHeight * zoomToFitScale < static_cast<double>(m_defaultLayoutSize.height()))
        zoomToFitScale = contentHeight > 0 ? static_cast<double>(m_defaultLayoutSize.height()) / contentHeight : 1.0;
    zoomToFitScale = std::max(zoomToFitScale, minimumZoomToFitScale);

    if (!isImageDocument)
        return zoomToFitScale;

    return std::min(zoomToFitScale, maximumImageDocumentZoomToFitScale);
}

bool WebPagePrivate::respectViewport() const
{
    return m_forceRespectViewportArguments || contentsSize().width() <= m_virtualViewportSize.width();
}

double WebPagePrivate::initialScale() const
{

    if (m_initialScale > 0.0 && respectViewport())
        return m_initialScale;

    if (m_webSettings->isZoomToFitOnLoad())
        return zoomToFitScale();

    return 1.0;
}

double WebPage::initialScale() const
{
    return d->initialScale();
}

void WebPage::initializeIconDataBase()
{
    IconDatabaseClientBlackBerry::instance()->initIconDatabase(d->m_webSettings);
}

bool WebPage::isUserScalable() const
{
    return d->isUserScalable();
}

void WebPage::setUserScalable(bool userScalable)
{
    d->setUserScalable(userScalable);
}

double WebPage::currentScale() const
{
    return d->currentScale();
}

void WebPage::setInitialScale(double initialScale)
{
    d->setInitialScale(initialScale);
}

double WebPage::minimumScale() const
{
    return d->minimumScale();
}

void WebPage::setMinimumScale(double minimumScale)
{
    d->setMinimumScale(minimumScale);
}

void WebPage::setMaximumScale(double maximumScale)
{
    d->setMaximumScale(maximumScale);
}

double WebPagePrivate::maximumScale() const
{
    if (m_maximumScale >= zoomToFitScale() && m_maximumScale >= m_minimumScale && respectViewport())
        return m_maximumScale;

    return hasVirtualViewport() ? std::max<double>(zoomToFitScale(), 4.0) : 4.0;
}

double WebPage::maximumScale() const
{
    return d->maximumScale();
}

void WebPagePrivate::resetScales()
{
    TransformationMatrix identity;
    *m_transformationMatrix = identity;
    m_initialScale = m_webSettings->initialScale() > 0 ? m_webSettings->initialScale() : -1.0;
    m_minimumScale = -1.0;
    m_maximumScale = -1.0;

    // We have to let WebCore know about updated framerect now that we've
    // reset our scales. See: RIM Bug #401.
    updateViewportSize();
}

IntPoint WebPagePrivate::transformedScrollPosition() const
{
    return m_backingStoreClient->transformedScrollPosition();
}

IntPoint WebPagePrivate::transformedMaximumScrollPosition() const
{
    return m_backingStoreClient->transformedMaximumScrollPosition();
}

IntSize WebPagePrivate::transformedActualVisibleSize() const
{
    return IntSize(m_actualVisibleWidth, m_actualVisibleHeight);
}

Platform::ViewportAccessor* WebPage::webkitThreadViewportAccessor() const
{
    return d->m_webkitThreadViewportAccessor;
}

Platform::IntSize WebPage::viewportSize() const
{
    return d->transformedActualVisibleSize();
}

IntSize WebPagePrivate::transformedViewportSize() const
{
    return Platform::Graphics::Screen::primaryScreen()->size();
}

IntRect WebPagePrivate::transformedVisibleContentsRect() const
{
    // Usually this would be mapToTransformed(visibleContentsRect()), but
    // that results in rounding errors because we already set the WebCore
    // viewport size from our original transformedViewportSize().
    // Instead, we only transform the scroll position and take the
    // viewport size as it is, which ensures that e.g. blitting operations
    // always cover the whole widget/screen.
    return IntRect(transformedScrollPosition(), transformedViewportSize());
}

IntSize WebPagePrivate::transformedContentsSize() const
{
    // mapToTransformed() functions use this method to crop their results,
    // so we can't make use of them here. While we want rounding inside page
    // boundaries to extend rectangles and round points, we need to crop the
    // contents size to the floored values so that we don't try to display
    // or report points that are not fully covered by the actual float-point
    // contents rectangle.
    const IntSize untransformedContentsSize = contentsSize();
    const FloatPoint transformedBottomRight = m_transformationMatrix->mapPoint(
        FloatPoint(untransformedContentsSize.width(), untransformedContentsSize.height()));
    return IntSize(floorf(transformedBottomRight.x()), floorf(transformedBottomRight.y()));
}

IntPoint WebPagePrivate::mapFromContentsToViewport(const IntPoint& point) const
{
    return m_backingStoreClient->mapFromContentsToViewport(point);
}

IntPoint WebPagePrivate::mapFromViewportToContents(const IntPoint& point) const
{
    return m_backingStoreClient->mapFromViewportToContents(point);
}

IntRect WebPagePrivate::mapFromContentsToViewport(const IntRect& rect) const
{
    return m_backingStoreClient->mapFromContentsToViewport(rect);
}

IntRect WebPagePrivate::mapFromViewportToContents(const IntRect& rect) const
{
    return m_backingStoreClient->mapFromViewportToContents(rect);
}

IntPoint WebPagePrivate::mapFromTransformedContentsToTransformedViewport(const IntPoint& point) const
{
    return m_backingStoreClient->mapFromTransformedContentsToTransformedViewport(point);
}

IntPoint WebPagePrivate::mapFromTransformedViewportToTransformedContents(const IntPoint& point) const
{
    return m_backingStoreClient->mapFromTransformedViewportToTransformedContents(point);
}

IntRect WebPagePrivate::mapFromTransformedContentsToTransformedViewport(const IntRect& rect) const
{
    return m_backingStoreClient->mapFromTransformedContentsToTransformedViewport(rect);
}

IntRect WebPagePrivate::mapFromTransformedViewportToTransformedContents(const IntRect& rect) const
{
    return m_backingStoreClient->mapFromTransformedViewportToTransformedContents(rect);
}

// NOTE: PIXEL ROUNDING!
// Accurate back-and-forth rounding is not possible with information loss
// by integer points and sizes, so we always expand the resulting mapped
// float rectangles to the nearest integer. For points, we always use
// floor-rounding in mapToTransformed() so that we don't have to crop to
// the (floor'd) transformed contents size.
static inline IntPoint roundTransformedPoint(const FloatPoint &point)
{
    // Maps by rounding half towards zero.
    return IntPoint(static_cast<int>(floorf(point.x())), static_cast<int>(floorf(point.y())));
}

static inline IntPoint roundUntransformedPoint(const FloatPoint &point)
{
    // Maps by rounding half away from zero.
    return IntPoint(static_cast<int>(ceilf(point.x())), static_cast<int>(ceilf(point.y())));
}

IntPoint WebPagePrivate::mapToTransformed(const IntPoint& point) const
{
    return roundTransformedPoint(m_transformationMatrix->mapPoint(FloatPoint(point)));
}

FloatPoint WebPagePrivate::mapToTransformedFloatPoint(const FloatPoint& point) const
{
    return m_transformationMatrix->mapPoint(point);
}

IntPoint WebPagePrivate::mapFromTransformed(const IntPoint& point) const
{
    return roundUntransformedPoint(m_transformationMatrix->inverse().mapPoint(FloatPoint(point)));
}

FloatPoint WebPagePrivate::mapFromTransformedFloatPoint(const FloatPoint& point) const
{
    return m_transformationMatrix->inverse().mapPoint(point);
}

FloatRect WebPagePrivate::mapFromTransformedFloatRect(const FloatRect& rect) const
{
    return m_transformationMatrix->inverse().mapRect(rect);
}

IntSize WebPagePrivate::mapToTransformed(const IntSize& size) const
{
    return mapToTransformed(IntRect(IntPoint::zero(), size)).size();
}

IntSize WebPagePrivate::mapFromTransformed(const IntSize& size) const
{
    return mapFromTransformed(IntRect(IntPoint::zero(), size)).size();
}

IntRect WebPagePrivate::mapToTransformed(const IntRect& rect) const
{
    return enclosingIntRect(m_transformationMatrix->mapRect(FloatRect(rect)));
}

// Use this in conjunction with mapToTransformed(IntRect), in most cases.
void WebPagePrivate::clipToTransformedContentsRect(IntRect& rect) const
{
    rect.intersect(IntRect(IntPoint::zero(), transformedContentsSize()));
}

IntRect WebPagePrivate::mapFromTransformed(const IntRect& rect) const
{
    return enclosingIntRect(m_transformationMatrix->inverse().mapRect(FloatRect(rect)));
}

bool WebPagePrivate::transformedPointEqualsUntransformedPoint(const IntPoint& transformedPoint, const IntPoint& untransformedPoint)
{
    // Scaling down is always more accurate than scaling up.
    if (m_transformationMatrix->a() > 1.0)
        return transformedPoint == mapToTransformed(untransformedPoint);

    return mapFromTransformed(transformedPoint) == untransformedPoint;
}

void WebPagePrivate::notifyTransformChanged()
{
    notifyTransformedContentsSizeChanged();
    notifyTransformedScrollChanged();

    m_backingStore->d->transformChanged();
}

void WebPagePrivate::notifyTransformedContentsSizeChanged()
{
    // We mark here as the last reported content size we sent to the client.
    m_previousContentsSize = contentsSize();

    const IntSize size = transformedContentsSize();
    m_backingStore->d->contentsSizeChanged(size);
    m_client->contentsSizeChanged();
}

void WebPagePrivate::notifyTransformedScrollChanged()
{
    const IntPoint pos = transformedScrollPosition();
    m_backingStore->d->scrollChanged(pos);
    m_client->scrollChanged();
}

bool WebPagePrivate::setViewMode(ViewMode mode)
{
    if (!m_mainFrame->view())
        return false;

    m_viewMode = mode;

    // If we're in the middle of a nested layout with a recursion count above
    // some maximum threshold, then our algorithm for finding the minimum content
    // width of a given page has become dependent on the visible width.
    //
    // We need to find some method to ensure that we don't experience excessive
    // and even infinite recursion. This can even happen with valid html. The
    // former can happen when we run into inline text with few candidates for line
    // break. The latter can happen for instance if the page has a negative margin
    // set against the right border. Note: this is valid by spec and can lead to
    // a situation where there is no value for which the content width will ensure
    // no horizontal scrollbar.
    // Example: LayoutTests/css1/box_properties/margin.html
    //
    // In order to address such situations when we detect a recursion above some
    // maximum threshold we snap our fixed layout size to a defined quantum increment.
    // Eventually, either the content width will be satisfied to ensure no horizontal
    // scrollbar or this increment will run into the maximum layout size and the
    // recursion will necessarily end.
    bool snapToIncrement = didLayoutExceedMaximumIterations();

    IntSize currentSize = m_mainFrame->view()->fixedLayoutSize();
    IntSize newSize = fixedLayoutSize(snapToIncrement);
    if (currentSize == newSize)
        return false;

    // FIXME: Temp solution. We'll get back to this.
    if (m_nestedLayoutFinishedCount) {
        double widthChange = fabs(double(newSize.width() - currentSize.width()) / currentSize.width());
        double heightChange = fabs(double(newSize.height() - currentSize.height()) / currentSize.height());
        if (widthChange < 0.05 && heightChange < 0.05)
            return false;
    }

    m_mainFrame->view()->setUseFixedLayout(useFixedLayout());
    m_mainFrame->view()->setFixedLayoutSize(newSize);
    return true; // Needs re-layout!
}

int WebPagePrivate::playerID() const
{
    return m_client ? m_client->getInstanceId() : 0;
}

void WebPagePrivate::setCursor(PlatformCursor handle)
{
    if (m_currentCursor.type() != handle.type()) {
        m_currentCursor = handle;
        m_client->cursorChanged(handle.type(), handle.url().c_str(), handle.hotspot());
    }
}

Platform::NetworkStreamFactory* WebPagePrivate::networkStreamFactory()
{
    return m_client->networkStreamFactory();
}

Platform::Graphics::Window* WebPagePrivate::platformWindow() const
{
    return m_client->window();
}

void WebPagePrivate::setPreventsScreenDimming(bool keepAwake)
{
    if (keepAwake) {
        if (!m_preventIdleDimmingCount)
            m_client->setPreventsScreenIdleDimming(true);
        m_preventIdleDimmingCount++;
    } else if (m_preventIdleDimmingCount > 0) {
        m_preventIdleDimmingCount--;
        if (!m_preventIdleDimmingCount)
            m_client->setPreventsScreenIdleDimming(false);
    } else
        ASSERT_NOT_REACHED(); // SetPreventsScreenIdleDimming(false) called too many times.
}

void WebPagePrivate::showVirtualKeyboard(bool showKeyboard)
{
    m_client->showVirtualKeyboard(showKeyboard);
}

void WebPagePrivate::ensureContentVisible(bool centerInView)
{
    m_inputHandler->ensureFocusElementVisible(centerInView);
}

void WebPagePrivate::zoomToContentRect(const IntRect& rect)
{
    // Don't scale if the user is not supposed to scale.
    if (!isUserScalable())
        return;

    FloatPoint anchor = FloatPoint(rect.width() / 2.0 + rect.x(), rect.height() / 2.0 + rect.y());
    IntSize viewSize = viewportSize();

    // Calculate the scale required to scale that dimension to fit.
    double scaleH = (double)viewSize.width() / (double)rect.width();
    double scaleV = (double)viewSize.height() / (double)rect.height();

    // Choose the smaller scale factor so that all of the content is visible.
    zoomAboutPoint(min(scaleH, scaleV), anchor);
}

void WebPagePrivate::registerPlugin(PluginView* plugin, bool shouldRegister)
{
    if (shouldRegister)
        m_pluginViews.add(plugin);
    else
        m_pluginViews.remove(plugin);
}

#define FOR_EACH_PLUGINVIEW(pluginViews) \
    HashSet<PluginView*>::const_iterator it = pluginViews.begin(); \
    HashSet<PluginView*>::const_iterator last = pluginViews.end(); \
    for (; it != last; ++it)

void WebPagePrivate::notifyPageOnLoad()
{
    FOR_EACH_PLUGINVIEW(m_pluginViews)
        (*it)->handleOnLoadEvent();
}

bool WebPagePrivate::shouldPluginEnterFullScreen(PluginView* plugin, const char* windowUniquePrefix)
{
    return m_client->shouldPluginEnterFullScreen();
}

void WebPagePrivate::didPluginEnterFullScreen(PluginView* plugin, const char* windowUniquePrefix)
{
    m_fullScreenPluginView = plugin;
    m_client->didPluginEnterFullScreen();

    if (!m_client->window())
        return;

    Platform::Graphics::Window::setTransparencyDiscardFilter(windowUniquePrefix);
    m_client->window()->setSensitivityFullscreenOverride(true);
}

void WebPagePrivate::didPluginExitFullScreen(PluginView* plugin, const char* windowUniquePrefix)
{
    m_fullScreenPluginView = 0;
    m_client->didPluginExitFullScreen();

    if (!m_client->window())
        return;

    Platform::Graphics::Window::setTransparencyDiscardFilter(0);
    m_client->window()->setSensitivityFullscreenOverride(false);
}

void WebPagePrivate::onPluginStartBackgroundPlay(PluginView* plugin, const char* windowUniquePrefix)
{
    m_client->onPluginStartBackgroundPlay();
}

void WebPagePrivate::onPluginStopBackgroundPlay(PluginView* plugin, const char* windowUniquePrefix)
{
    m_client->onPluginStopBackgroundPlay();
}

bool WebPagePrivate::lockOrientation(bool landscape)
{
    return m_client->lockOrientation(landscape);
}

void WebPagePrivate::unlockOrientation()
{
    return m_client->unlockOrientation();
}

int WebPagePrivate::orientation() const
{
#if ENABLE(ORIENTATION_EVENTS)
    return m_mainFrame->orientation();
#else
#error ORIENTATION_EVENTS must be defined.
// Or a copy of the orientation value will have to be stored in these objects.
#endif
}

double WebPagePrivate::currentZoomFactor() const
{
    return currentScale();
}

int WebPagePrivate::showAlertDialog(WebPageClient::AlertType atype)
{
    return m_client->showAlertDialog(atype);
}

bool WebPagePrivate::isActive() const
{
    return m_client->isActive();
}

void WebPagePrivate::authenticationChallenge(const KURL& url, const ProtectionSpace& protectionSpace, const Credential& inputCredential)
{
    AuthenticationChallengeManager* authmgr = AuthenticationChallengeManager::instance();
    BlackBerry::Platform::String username;
    BlackBerry::Platform::String password;

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_dumpRenderTree) {
        Credential credential(inputCredential, inputCredential.persistence());
        if (m_dumpRenderTree->didReceiveAuthenticationChallenge(credential))
            authmgr->notifyChallengeResult(url, protectionSpace, AuthenticationChallengeSuccess, credential);
        else
            authmgr->notifyChallengeResult(url, protectionSpace, AuthenticationChallengeCancelled, inputCredential);
        return;
    }
#endif

#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
    if (m_webSettings->isCredentialAutofillEnabled() && !m_webSettings->isPrivateBrowsingEnabled())
        credentialManager().autofillAuthenticationChallenge(protectionSpace, username, password);
#endif

    bool isConfirmed = m_client->authenticationChallenge(protectionSpace.realm().characters(), protectionSpace.realm().length(), username, password);

#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
    Credential credential(username, password, CredentialPersistencePermanent);
    if (m_webSettings->isCredentialAutofillEnabled() && !m_webSettings->isPrivateBrowsingEnabled() && isConfirmed)
        credentialManager().saveCredentialIfConfirmed(this, CredentialTransformData(url, protectionSpace, credential));
#else
    Credential credential(username, password, CredentialPersistenceNone);
#endif

    if (isConfirmed)
        authmgr->notifyChallengeResult(url, protectionSpace, AuthenticationChallengeSuccess, credential);
    else
        authmgr->notifyChallengeResult(url, protectionSpace, AuthenticationChallengeCancelled, inputCredential);
}

PageClientBlackBerry::SaveCredentialType WebPagePrivate::notifyShouldSaveCredential(bool isNew)
{
    return static_cast<PageClientBlackBerry::SaveCredentialType>(m_client->notifyShouldSaveCredential(isNew));
}

void WebPagePrivate::syncProxyCredential(const WebCore::Credential& credential)
{
    m_client->syncProxyCredential(credential.user(), credential.password());
}

void WebPagePrivate::notifyPopupAutofillDialog(const Vector<String>& candidates)
{
    vector<BlackBerry::Platform::String> textItems;
    for (size_t i = 0; i < candidates.size(); i++)
        textItems.push_back(candidates[i]);
    m_client->notifyPopupAutofillDialog(textItems);
}

void WebPagePrivate::notifyDismissAutofillDialog()
{
    m_client->notifyDismissAutofillDialog();
}

bool WebPagePrivate::useFixedLayout() const
{
    return true;
}

Platform::WebContext WebPagePrivate::webContext(TargetDetectionStrategy strategy)
{
    Platform::WebContext context;

    RefPtr<Node> node = contextNode(strategy);
    m_currentContextNode = node;
    if (!m_currentContextNode)
        return context;

    // Send an onContextMenu event to the current context ndoe and get the result. Since we've already figured out
    // which node we want, we can send it directly to the node and not do a hit test. The onContextMenu event doesn't require
    // mouse positions so we just set the position at (0,0)
    PlatformMouseEvent mouseEvent(IntPoint(), IntPoint(), PlatformEvent::MouseMoved, 0, NoButton, TouchScreen);
    if (m_currentContextNode->dispatchMouseEvent(mouseEvent, eventNames().contextmenuEvent, 0)) {
        context.setFlag(Platform::WebContext::IsOnContextMenuPrevented);
        return context;
    }

    requestLayoutIfNeeded();

    bool nodeAllowSelectionOverride = false;
    if (Node* linkNode = node->enclosingLinkEventParentOrSelf()) {
        KURL href;
        if (linkNode->isLink() && linkNode->hasAttributes()) {
            if (Attribute* attribute = static_cast<Element*>(linkNode)->getAttributeItem(HTMLNames::hrefAttr))
                href = linkNode->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(attribute->value()));
        }

        String pattern = findPatternStringForUrl(href);
        if (!pattern.isEmpty())
            context.setPattern(pattern);

        if (!href.string().isEmpty()) {
            context.setUrl(href.string());

            // Links are non-selectable by default, but selection should be allowed
            // providing the page is selectable, use the parent to determine it.
            if (linkNode->parentNode() && linkNode->parentNode()->canStartSelection())
                nodeAllowSelectionOverride = true;
        }
    }

    if (node->isHTMLElement()) {
        HTMLImageElement* imageElement = 0;
        HTMLMediaElement* mediaElement = 0;

        if (node->hasTagName(HTMLNames::imgTag))
            imageElement = static_cast<HTMLImageElement*>(node.get());
        else if (node->hasTagName(HTMLNames::areaTag))
            imageElement = static_cast<HTMLAreaElement*>(node.get())->imageElement();

        if (static_cast<HTMLElement*>(node.get())->isMediaElement())
            mediaElement = static_cast<HTMLMediaElement*>(node.get());

        if (imageElement && imageElement->renderer()) {
            context.setFlag(Platform::WebContext::IsImage);
            // FIXME: At the mean time, we only show "Save Image" when the image data is available.
            if (CachedResource* cachedResource = imageElement->cachedImage()) {
                if (cachedResource->isLoaded() && cachedResource->data()) {
                    String url = stripLeadingAndTrailingHTMLSpaces(imageElement->getAttribute(HTMLNames::srcAttr).string());
                    context.setSrc(node->document()->completeURL(url).string());
                }
            }
            String alt = imageElement->altText();
            if (!alt.isNull())
                context.setAlt(alt);
        }

        if (mediaElement) {
            if (mediaElement->hasAudio())
                context.setFlag(Platform::WebContext::IsAudio);
            if (mediaElement->hasVideo())
                context.setFlag(Platform::WebContext::IsVideo);

            String src = stripLeadingAndTrailingHTMLSpaces(mediaElement->getAttribute(HTMLNames::srcAttr).string());
            context.setSrc(node->document()->completeURL(src).string());
        }
    }

    if (node->isTextNode()) {
        Text* curText = toText(node.get());
        if (!curText->wholeText().isEmpty())
            context.setText(curText->wholeText());
    }

    bool canStartSelection = node->canStartSelection();

    if (node->isElementNode()) {
        Element* element = static_cast<Element*>(node->shadowAncestorNode());

        if (DOMSupport::isTextBasedContentEditableElement(element)) {
            if (!canStartSelection) {
                // Input fields host node is by spec non-editable unless the field itself has content editable enabled.
                // Enable selection if the shadow tree for the input field is selectable.
                Node* nodeUnderFinger = m_touchEventHandler->lastFatFingersResult().isValid() ? m_touchEventHandler->lastFatFingersResult().node(FatFingersResult::ShadowContentAllowed) : 0;
                if (nodeUnderFinger)
                    canStartSelection = nodeUnderFinger->canStartSelection();
            }
            context.setFlag(Platform::WebContext::IsInput);
            if (element->hasTagName(HTMLNames::inputTag))
                context.setFlag(Platform::WebContext::IsSingleLine);
            if (DOMSupport::isPasswordElement(element))
                context.setFlag(Platform::WebContext::IsPassword);

            String elementText(DOMSupport::inputElementText(element));
            if (!elementText.stripWhiteSpace().isEmpty())
                context.setText(elementText);
            else if (!node->focused() && m_touchEventHandler->lastFatFingersResult().isValid() && strategy == RectBased) {
                // If an input field is empty and not focused send a mouse click so that it gets a cursor and we can paste into it.
                m_touchEventHandler->sendClickAtFatFingersPoint();
            }
        }
    }

    if (!nodeAllowSelectionOverride && !canStartSelection)
        context.resetFlag(Platform::WebContext::IsSelectable);

    if (node->isFocusable())
        context.setFlag(Platform::WebContext::IsFocusable);

    // Walk up the node tree looking for our custom webworks context attribute.
    while (node) {
        if (node->isElementNode()) {
            Element* element = static_cast<Element*>(node->shadowAncestorNode());
            String webWorksContext(DOMSupport::webWorksContext(element));
            if (!webWorksContext.stripWhiteSpace().isEmpty()) {
                context.setFlag(Platform::WebContext::IsWebWorksContext);
                context.setWebWorksContext(webWorksContext);
                break;
            }
        }
        node = node->parentNode();
    }

    return context;
}

Platform::WebContext WebPage::webContext(TargetDetectionStrategy strategy) const
{
    return d->webContext(strategy);
}

void WebPagePrivate::updateCursor()
{
    int buttonMask = 0;
    if (m_lastMouseEvent.button() == LeftButton)
        buttonMask = Platform::MouseEvent::ScreenLeftMouseButton;
    else if (m_lastMouseEvent.button() == MiddleButton)
        buttonMask = Platform::MouseEvent::ScreenMiddleMouseButton;
    else if (m_lastMouseEvent.button() == RightButton)
        buttonMask = Platform::MouseEvent::ScreenRightMouseButton;

    BlackBerry::Platform::MouseEvent event(buttonMask, buttonMask, mapToTransformed(m_lastMouseEvent.position()), mapToTransformed(m_lastMouseEvent.globalPosition()), 0, 0);
    m_webPage->mouseEvent(event);
}

IntSize WebPagePrivate::fixedLayoutSize(bool snapToIncrement) const
{
    if (hasVirtualViewport())
        return m_virtualViewportSize;

    const int defaultLayoutWidth = m_defaultLayoutSize.width();
    const int defaultLayoutHeight = m_defaultLayoutSize.height();

    int minWidth = defaultLayoutWidth;
    int maxWidth = DEFAULT_MAX_LAYOUT_WIDTH;
    int maxHeight = DEFAULT_MAX_LAYOUT_HEIGHT;

    // If the load state is none then we haven't actually got anything yet, but we need to layout
    // the entire page so that the user sees the entire page (unrendered) instead of just part of it.
    if (m_loadState == None)
        return IntSize(defaultLayoutWidth, defaultLayoutHeight);

    if (m_viewMode == FixedDesktop) {
        int width  = maxWidth;
        // if the defaultLayoutHeight is at minimum, it probably was set as 0
        // and clamped, meaning it's effectively not set.  (Even if it happened
        // to be set exactly to the minimum, it's too small to be useful.)  So
        // ignore it.
        int height;
        if (defaultLayoutHeight <= minimumLayoutSize.height())
            height = maxHeight;
        else
            height = ceilf(static_cast<float>(width) / static_cast<float>(defaultLayoutWidth) * static_cast<float>(defaultLayoutHeight));
        return IntSize(width, height);
    }

    if (m_viewMode == Desktop) {
        // If we detect an overflow larger than the contents size then use that instead since
        // it'll still be clamped by the maxWidth below...
        int width = std::max(absoluteVisibleOverflowSize().width(), contentsSize().width());
        if (m_pendingOrientation != -1 && !m_nestedLayoutFinishedCount)
            width = 0;

        if (snapToIncrement) {
            // Snap to increments of defaultLayoutWidth / 2.0.
            float factor = static_cast<float>(width) / (defaultLayoutWidth / 2.0);
            factor = ceilf(factor);
            width = (defaultLayoutWidth / 2.0) * factor;
        }

        if (width < minWidth)
            width = minWidth;
        if (width > maxWidth)
            width = maxWidth;
        int height = ceilf(static_cast<float>(width) / static_cast<float>(defaultLayoutWidth) * static_cast<float>(defaultLayoutHeight));
        return IntSize(width, height);
    }

    ASSERT_NOT_REACHED();
    return IntSize(defaultLayoutWidth, defaultLayoutHeight);
}

BackingStoreClient* WebPagePrivate::backingStoreClient() const
{
    return m_backingStoreClient;
}

void WebPagePrivate::clearDocumentData(const Document* documentGoingAway)
{
    ASSERT(documentGoingAway);
    if (m_currentContextNode && m_currentContextNode->document() == documentGoingAway)
        m_currentContextNode = 0;

    if (m_currentPinchZoomNode && m_currentPinchZoomNode->document() == documentGoingAway)
        m_currentPinchZoomNode = 0;

    if (m_currentBlockZoomAdjustedNode && m_currentBlockZoomAdjustedNode->document() == documentGoingAway)
        m_currentBlockZoomAdjustedNode = 0;

    if (m_inRegionScroller->d->isActive())
        m_inRegionScroller->d->clearDocumentData(documentGoingAway);

    if (documentGoingAway->frame())
        m_inputHandler->frameUnloaded(documentGoingAway->frame());

    Node* nodeUnderFatFinger = m_touchEventHandler->lastFatFingersResult().node();
    if (nodeUnderFatFinger && nodeUnderFatFinger->document() == documentGoingAway)
        m_touchEventHandler->resetLastFatFingersResult();

    // NOTE: m_fullscreenVideoNode, m_fullScreenPluginView and m_pluginViews
    // are cleared in other methods already.
}

typedef bool (*PredicateFunction)(RenderLayer*);
static bool isPositionedContainer(RenderLayer* layer)
{
    RenderObject* o = layer->renderer();
    return o->isRenderView() || o->isOutOfFlowPositioned() || o->isRelPositioned() || layer->hasTransform();
}

static bool isFixedPositionedContainer(RenderLayer* layer)
{
    RenderObject* o = layer->renderer();
    return o->isRenderView() || (o->isOutOfFlowPositioned() && o->style()->position() == FixedPosition);
}

static RenderLayer* findAncestorOrSelfNotMatching(PredicateFunction predicate, RenderLayer* layer)
{
    RenderLayer* curr = layer;
    while (curr && !predicate(curr))
        curr = curr->parent();

    return curr;
}

RenderLayer* WebPagePrivate::enclosingFixedPositionedAncestorOrSelfIfFixedPositioned(RenderLayer* layer)
{
    return findAncestorOrSelfNotMatching(&isFixedPositionedContainer, layer);
}

RenderLayer* WebPagePrivate::enclosingPositionedAncestorOrSelfIfPositioned(RenderLayer* layer)
{
    return findAncestorOrSelfNotMatching(&isPositionedContainer, layer);
}

static inline Frame* frameForNode(Node* node)
{
    Node* origNode = node;
    for (; node; node = node->parentNode()) {
        if (RenderObject* renderer = node->renderer()) {
            if (renderer->isRenderView()) {
                if (FrameView* view = toRenderView(renderer)->frameView()) {
                    if (Frame* frame = view->frame())
                        return frame;
                }
            }
            if (renderer->isWidget()) {
                Widget* widget = toRenderWidget(renderer)->widget();
                if (widget && widget->isFrameView()) {
                    if (Frame* frame = static_cast<FrameView*>(widget)->frame())
                        return frame;
                }
            }
        }
    }

    for (node = origNode; node; node = node->parentNode()) {
        if (Document* doc = node->document()) {
            if (Frame* frame = doc->frame())
                return frame;
        }
    }

    return 0;
}

static IntRect getNodeWindowRect(Node* node)
{
    if (Frame* frame = frameForNode(node)) {
        if (FrameView* view = frame->view())
            return view->contentsToWindow(node->getRect());
    }
    ASSERT_NOT_REACHED();
    return IntRect();
}

IntRect WebPagePrivate::getRecursiveVisibleWindowRect(ScrollView* view, bool noClipOfMainFrame)
{
    ASSERT(m_mainFrame);

    // Don't call this function asking to not clip the main frame providing only
    // the main frame. All that can be returned is the content rect which
    // isn't what this function is for.
    if (noClipOfMainFrame && view == m_mainFrame->view()) {
        ASSERT_NOT_REACHED();
        return IntRect(IntPoint::zero(), view->contentsSize());
    }

    IntRect visibleWindowRect(view->contentsToWindow(view->visibleContentRect(false)));
    if (view->parent() && !(noClipOfMainFrame && view->parent() == m_mainFrame->view())) {
        // Intersect with parent visible rect.
        visibleWindowRect.intersect(getRecursiveVisibleWindowRect(view->parent(), noClipOfMainFrame));
    }
    return visibleWindowRect;
}

void WebPagePrivate::assignFocus(Platform::FocusDirection direction)
{
    ASSERT((int) Platform::FocusDirectionNone == (int) FocusDirectionNone);
    ASSERT((int) Platform::FocusDirectionForward == (int) FocusDirectionForward);
    ASSERT((int) Platform::FocusDirectionBackward == (int) FocusDirectionBackward);

    // First we clear the focus, since we want to focus either initial or the last
    // focusable element in the webpage (according to the TABINDEX), or simply clear
    // the focus.
    clearFocusNode();

    switch (direction) {
    case FocusDirectionForward:
    case FocusDirectionBackward:
        m_page->focusController()->setInitialFocus((FocusDirection) direction, 0);
        break;
    case FocusDirectionNone:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebPage::assignFocus(Platform::FocusDirection direction)
{
    if (d->m_page->defersLoading())
       return;
    d->assignFocus(direction);
}

Platform::IntRect WebPagePrivate::focusNodeRect()
{
    Frame* frame = focusedOrMainFrame();
    if (!frame)
        return Platform::IntRect();

    Document* doc = frame->document();
    FrameView* view = frame->view();
    if (!doc || !view || view->needsLayout())
        return Platform::IntRect();

    IntRect focusRect = rectForNode(doc->focusedNode());
    focusRect = adjustRectOffsetForFrameOffset(focusRect, doc->focusedNode());
    focusRect = mapToTransformed(focusRect);
    clipToTransformedContentsRect(focusRect);
    return focusRect;
}

PassRefPtr<Node> WebPagePrivate::contextNode(TargetDetectionStrategy strategy)
{
    EventHandler* eventHandler = focusedOrMainFrame()->eventHandler();
    const FatFingersResult lastFatFingersResult = m_touchEventHandler->lastFatFingersResult();
    bool isTouching = lastFatFingersResult.isValid() && strategy == RectBased;

    // Check if we're using LinkToLink and the user is not touching the screen.
    if (m_webSettings->doesGetFocusNodeContext() && !isTouching) {
        RefPtr<Node> node;
        node = m_page->focusController()->focusedOrMainFrame()->document()->focusedNode();
        if (node) {
            IntRect visibleRect = IntRect(IntPoint(), actualVisibleSize());
            if (!visibleRect.intersects(getNodeWindowRect(node.get())))
                return 0;
        }
        return node.release();
    }

    // Check for text input.
    if (isTouching && lastFatFingersResult.isTextInput())
        return lastFatFingersResult.node(FatFingersResult::ShadowContentNotAllowed);

    if (strategy == RectBased) {
        FatFingersResult result = FatFingers(this, lastFatFingersResult.adjustedPosition(), FatFingers::Text).findBestPoint();
        return result.node(FatFingersResult::ShadowContentNotAllowed);
    }
    if (strategy == FocusBased)
        return m_inputHandler->currentFocusElement();

    IntPoint contentPos;
    if (isTouching)
        contentPos = lastFatFingersResult.adjustedPosition();
    else
        contentPos = mapFromViewportToContents(m_lastMouseEvent.position());

    HitTestResult result = eventHandler->hitTestResultAtPoint(contentPos, false /*allowShadowContent*/);
    return result.innerNode();
}

static inline int distanceBetweenPoints(IntPoint p1, IntPoint p2)
{
    // Change int to double, because (dy * dy) can cause int overflow in reality, e.g, (-46709 * -46709).
    double dx = static_cast<double>(p1.x() - p2.x());
    double dy = static_cast<double>(p1.y() - p2.y());
    return sqrt((dx * dx) + (dy * dy));
}

Node* WebPagePrivate::bestNodeForZoomUnderPoint(const IntPoint& documentPoint)
{
    IntRect clickRect(documentPoint.x() - blockClickRadius, documentPoint.y() - blockClickRadius, 2 * blockClickRadius, 2 * blockClickRadius);
    Node* originalNode = nodeForZoomUnderPoint(documentPoint);
    if (!originalNode)
        return 0;
    Node* node = bestChildNodeForClickRect(originalNode, clickRect);
    return node ? adjustedBlockZoomNodeForZoomAndExpandingRatioLimits(node) : adjustedBlockZoomNodeForZoomAndExpandingRatioLimits(originalNode);
}

Node* WebPagePrivate::bestChildNodeForClickRect(Node* parentNode, const IntRect& clickRect)
{
    if (!parentNode)
        return 0;

    int bestDistance = std::numeric_limits<int>::max();

    Node* node = parentNode->firstChild();
    Node* bestNode = 0;
    for (; node; node = node->nextSibling()) {
        IntRect rect = rectForNode(node);
        if (!clickRect.intersects(rect))
            continue;

        int distance = distanceBetweenPoints(rect.center(), clickRect.center());
        Node* bestChildNode = bestChildNodeForClickRect(node, clickRect);
        if (bestChildNode) {
            IntRect bestChildRect = rectForNode(bestChildNode);
            int bestChildDistance = distanceBetweenPoints(bestChildRect.center(), clickRect.center());
            if (bestChildDistance < distance && bestChildDistance < bestDistance) {
                bestNode = bestChildNode;
                bestDistance = bestChildDistance;
            } else {
                if (distance < bestDistance) {
                    bestNode = node;
                    bestDistance = distance;
                }
            }
        } else {
            if (distance < bestDistance) {
                bestNode = node;
                bestDistance = distance;
            }
        }
    }

    return bestNode;
}

double WebPagePrivate::maxBlockZoomScale() const
{
    return std::min(maximumBlockZoomScale, maximumScale());
}

Node* WebPagePrivate::nodeForZoomUnderPoint(const IntPoint& documentPoint)
{
    if (!m_mainFrame)
        return 0;

    HitTestResult result = m_mainFrame->eventHandler()->hitTestResultAtPoint(documentPoint, false);

    Node* node = result.innerNonSharedNode();

    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    while (!renderer) {
        node = node->parentNode();
        renderer = node->renderer();
    }

    return node;
}

Node* WebPagePrivate::adjustedBlockZoomNodeForZoomAndExpandingRatioLimits(Node* node)
{
    Node* initialNode = node;
    RenderObject* renderer = node->renderer();
    bool acceptableNodeSize = newScaleForBlockZoomRect(rectForNode(node), 1.0, 0) < maxBlockZoomScale();
    IntSize actualVisibleSize = this->actualVisibleSize();

    while (!renderer || !acceptableNodeSize) {
        node = node->parentNode();
        IntRect nodeRect = rectForNode(node);

        // Don't choose a node if the width of the node size is very close to the width of the actual visible size,
        // as block zoom can do nothing on such kind of node.
        if (!node || static_cast<double>(actualVisibleSize.width() - nodeRect.width()) / actualVisibleSize.width() < minimumExpandingRatio)
            return initialNode;

        renderer = node->renderer();
        acceptableNodeSize = newScaleForBlockZoomRect(rectForNode(node), 1.0, 0) < maxBlockZoomScale();
    }

    return node;
}

bool WebPagePrivate::compareNodesForBlockZoom(Node* n1, Node* n2)
{
    if (!n1 || !n2)
        return false;

    return (n2 == n1) || n2->isDescendantOf(n1);
}

double WebPagePrivate::newScaleForBlockZoomRect(const IntRect& rect, double oldScale, double margin)
{
    if (rect.isEmpty())
        return std::numeric_limits<double>::max();

    ASSERT(rect.width() + margin);

    double newScale = oldScale * static_cast<double>(transformedActualVisibleSize().width()) / (rect.width() + margin);

    return newScale;
}

IntRect WebPagePrivate::rectForNode(Node* node)
{
    if (!node)
        return IntRect();

    RenderObject* renderer = node->renderer();

    if (!renderer)
        return IntRect();

    // Return rect in un-transformed content coordinates.
    IntRect blockRect;

    // FIXME: Ensure this works with iframes.
    if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled && renderer->isText()) {
        RenderBlock* renderBlock = renderer->containingBlock();
        int xOffset = 0;
        int yOffset = 0;
        while (!renderBlock->isRoot()) {
            xOffset += renderBlock->x();
            yOffset += renderBlock->y();
            renderBlock = renderBlock->containingBlock();
        }
        const RenderText* renderText = toRenderText(renderer);
        IntRect linesBox = renderText->linesBoundingBox();
        blockRect = IntRect(xOffset + linesBox.x(), yOffset + linesBox.y(), linesBox.width(), linesBox.height());
    } else
        blockRect = renderer->absoluteClippedOverflowRect();

    if (renderer->isText()) {
        RenderBlock* rb = renderer->containingBlock();

        // Inefficient? Way to find width when floats intersect a block.
        int blockWidth = 0;
        int lineCount = rb->lineCount();
        for (int i = 0; i < lineCount; i++)
            blockWidth = max(blockWidth, rb->availableLogicalWidthForLine(i, false));

        blockRect.setWidth(blockWidth);
        blockRect.setX(blockRect.x() + rb->logicalLeftOffsetForLine(1, false));
    }

    // Strip off padding.
    if (renderer->style()->hasPadding()) {
        blockRect.setX(blockRect.x() + renderer->style()->paddingLeft().value());
        blockRect.setY(blockRect.y() + renderer->style()->paddingTop().value());
        blockRect.setWidth(blockRect.width() - renderer->style()->paddingRight().value());
        blockRect.setHeight(blockRect.height() - renderer->style()->paddingBottom().value());
    }

    return blockRect;
}

IntPoint WebPagePrivate::frameOffset(const Frame* frame) const
{
    ASSERT(frame);

    // FIXME: This function can be called when page is being destroyed and JS triggers selection change.
    // We could break the call chain at upper levels, but I think it is better to check the frame pointer
    // here because the pointer is explicitly cleared in WebPage::destroy().
    if (!mainFrame())
        return IntPoint();

    // Convert 0,0 in the frame's coordinate system to window coordinates to
    // get the frame's global position, and return this position in the main
    // frame's coordinates.  (So the main frame's coordinates will be 0,0.)
    return mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(IntPoint::zero()));
}

IntRect WebPagePrivate::adjustRectOffsetForFrameOffset(const IntRect& rect, const Node* node)
{
    if (!node)
        return rect;

    // Adjust the offset of the rect if it is in an iFrame/frame or set of iFrames/frames.
    // FIXME: can we just use frameOffset instead of this big routine?
    const Node* tnode = node;
    IntRect adjustedRect = rect;
    do {
        Frame* frame = tnode->document()->frame();
        if (!frame)
            continue;

        Node* ownerNode = static_cast<Node*>(frame->ownerElement());
        tnode = ownerNode;
        if (ownerNode && (ownerNode->hasTagName(HTMLNames::iframeTag) || ownerNode->hasTagName(HTMLNames::frameTag))) {
            IntRect iFrameRect;
            do {
                iFrameRect = rectForNode(ownerNode);
                adjustedRect.move(iFrameRect.x(), iFrameRect.y());
                adjustedRect.intersect(iFrameRect);
                ownerNode = ownerNode->parentNode();
            } while (iFrameRect.isEmpty() && ownerNode);
        } else
            break;
    } while (tnode = tnode->parentNode());

    return adjustedRect;
}

IntRect WebPagePrivate::blockZoomRectForNode(Node* node)
{
    if (!node || contentsSize().isEmpty())
        return IntRect();

    Node* tnode = node;
    m_currentBlockZoomAdjustedNode = tnode;

    IntRect blockRect = rectForNode(tnode);
    IntRect originalRect = blockRect;

    int originalArea = originalRect.width() * originalRect.height();
    int pageArea = contentsSize().width() * contentsSize().height();
    double blockToPageRatio = static_cast<double>(pageArea - originalArea) / pageArea;
    double blockExpansionRatio = 5.0 * blockToPageRatio * blockToPageRatio;

    if (!tnode->hasTagName(HTMLNames::imgTag) && !tnode->hasTagName(HTMLNames::inputTag) && !tnode->hasTagName(HTMLNames::textareaTag)) {
        while (tnode = tnode->parentNode()) {
            ASSERT(tnode);
            IntRect tRect = rectForNode(tnode);
            int tempBlockArea = tRect.width() * tRect.height();
            // Don't expand the block if it will be too large relative to the content.
            if (static_cast<double>(pageArea - tempBlockArea) / pageArea < minimumExpandingRatio)
                break;
            if (tRect.isEmpty())
                continue; // No renderer.
            if (tempBlockArea < 1.1 * originalArea)
                continue; // The size of this parent is very close to the child, no need to go to this parent.
            // Don't expand the block if the parent node size is already almost the size of actual visible size.
            IntSize actualSize = actualVisibleSize();
            if (static_cast<double>(actualSize.width() - tRect.width()) / actualSize.width() < minimumExpandingRatio)
                break;
            if (tempBlockArea < blockExpansionRatio * originalArea) {
                blockRect = tRect;
                m_currentBlockZoomAdjustedNode = tnode;
            } else
                break;
        }
    }

    blockRect = adjustRectOffsetForFrameOffset(blockRect, node);
    blockRect = mapToTransformed(blockRect);
    clipToTransformedContentsRect(blockRect);

    return blockRect;
}

// This function should not be called directly.
// It is called after the animation ends (see above).
void WebPagePrivate::zoomBlock()
{
    if (!m_mainFrame)
        return;

    IntPoint anchor(roundUntransformedPoint(m_finalBlockPoint));
    bool willUseTextReflow = false;

#if ENABLE(VIEWPORT_REFLOW)
    willUseTextReflow = m_webPage->settings()->textReflowMode() != WebSettings::TextReflowDisabled;
    toggleTextReflowIfEnabledForBlockZoomOnly(m_shouldReflowBlock);
    setNeedsLayout();
#endif

    TransformationMatrix zoom;
    zoom.scale(m_blockZoomFinalScale);
    *m_transformationMatrix = zoom;
    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    m_backingStore->d->suspendBackingStoreUpdates();
    m_backingStore->d->suspendScreenUpdates();
    updateViewportSize();

    FrameView* mainFrameView = m_mainFrame->view();
    bool constrainsScrollingToContentEdge = true;
    if (mainFrameView) {
        constrainsScrollingToContentEdge = mainFrameView->constrainsScrollingToContentEdge();
        mainFrameView->setConstrainsScrollingToContentEdge(m_shouldConstrainScrollingToContentEdge);
    }

#if ENABLE(VIEWPORT_REFLOW)
    requestLayoutIfNeeded();
    if (willUseTextReflow && m_shouldReflowBlock) {
        IntRect reflowedRect = rectForNode(m_currentBlockZoomAdjustedNode.get());
        reflowedRect = adjustRectOffsetForFrameOffset(reflowedRect, m_currentBlockZoomAdjustedNode.get());
        reflowedRect.move(roundTransformedPoint(m_finalBlockPointReflowOffset).x(), roundTransformedPoint(m_finalBlockPointReflowOffset).y());
        RenderObject* renderer = m_currentBlockZoomAdjustedNode->renderer();
        IntPoint topLeftPoint(reflowedRect.location());
        if (renderer && renderer->isText()) {
            ETextAlign textAlign = renderer->style()->textAlign();
            IntPoint textAnchor;
            switch (textAlign) {
            case CENTER:
            case WEBKIT_CENTER:
                textAnchor = IntPoint(reflowedRect.x() + (reflowedRect.width() - actualVisibleSize().width()) / 2, topLeftPoint.y());
                break;
            case LEFT:
            case WEBKIT_LEFT:
                textAnchor = topLeftPoint;
                break;
            case RIGHT:
            case WEBKIT_RIGHT:
                textAnchor = IntPoint(reflowedRect.x() + reflowedRect.width() - actualVisibleSize().width(), topLeftPoint.y());
                break;
            case TAAUTO:
            case JUSTIFY:
            default:
                if (renderer->style()->isLeftToRightDirection())
                    textAnchor = topLeftPoint;
                else
                    textAnchor = IntPoint(reflowedRect.x() + reflowedRect.width() - actualVisibleSize().width(), topLeftPoint.y());
                break;
            }
            setScrollPosition(textAnchor);
        } else {
            renderer->style()->isLeftToRightDirection()
                ? setScrollPosition(topLeftPoint)
                : setScrollPosition(IntPoint(reflowedRect.x() + reflowedRect.width() - actualVisibleSize().width(), topLeftPoint.y()));
        }
    } else if (willUseTextReflow) {
        IntRect finalRect = rectForNode(m_currentBlockZoomAdjustedNode.get());
        finalRect = adjustRectOffsetForFrameOffset(finalRect, m_currentBlockZoomAdjustedNode.get());
        setScrollPosition(IntPoint(0, finalRect.y() + m_finalBlockPointReflowOffset.y()));
        resetBlockZoom();
    }
#endif
    if (!willUseTextReflow) {
        setScrollPosition(anchor);
        if (!m_shouldReflowBlock)
            resetBlockZoom();
    }

    notifyTransformChanged();
    m_client->scaleChanged();

    if (mainFrameView)
        mainFrameView->setConstrainsScrollingToContentEdge(constrainsScrollingToContentEdge);

    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    m_backingStore->d->resumeBackingStoreUpdates();
    m_backingStore->d->resumeScreenUpdates(BackingStore::RenderAndBlit);
}

void WebPage::blockZoomAnimationFinished()
{
    d->zoomBlock();
}

void WebPage::resetBlockZoom()
{
    d->resetBlockZoom();
}

void WebPagePrivate::resetBlockZoom()
{
    m_currentBlockZoomNode = 0;
    m_currentBlockZoomAdjustedNode = 0;
    m_shouldReflowBlock = false;
    m_shouldConstrainScrollingToContentEdge = true;
}

void WebPage::destroyWebPageCompositor()
{
#if USE(ACCELERATED_COMPOSITING)
    // Destroy the layer renderer in a sync command before we destroy the backing store,
    // to flush any pending compositing messages on the compositing thread.
    // The backing store is indirectly deleted by the 'detachFromParent' call below.
    d->syncDestroyCompositorOnCompositingThread();
#endif
}

void WebPage::destroy()
{
    // TODO: need to verify if this call needs to be made before calling
    // Close the Inspector to resume the JS engine if it's paused.
    disableWebInspector();

    // WebPage::destroyWebPageCompositor()
    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    d->m_backingStore->d->suspendBackingStoreUpdates();
    d->m_backingStore->d->suspendScreenUpdates();

    // Close the backforward list and release the cached pages.
    d->m_page->backForward()->close();
    pageCache()->releaseAutoreleasedPagesNow();

    FrameLoader* loader = d->m_mainFrame->loader();

    // Set m_mainFrame to 0 to avoid calls back in to the backingstore during webpage deletion.
    d->m_mainFrame = 0;
    if (loader)
        loader->detachFromParent();

    deleteGuardedObject(this);
}

WebPageClient* WebPage::client() const
{
    return d->m_client;
}

int WebPage::backForwardListLength() const
{
    return d->m_page->getHistoryLength();
}

bool WebPage::canGoBackOrForward(int delta) const
{
    return d->m_page->canGoBackOrForward(delta);
}

bool WebPage::goBackOrForward(int delta)
{
    if (d->m_page->canGoBackOrForward(delta)) {
        d->m_page->goBackOrForward(delta);
        return true;
    }
    return false;
}

void WebPage::goToBackForwardEntry(BackForwardId id)
{
    HistoryItem* item = historyItemFromBackForwardId(id);
    ASSERT(item);
    d->m_page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void WebPage::reload()
{
    d->m_mainFrame->loader()->reload(/* bypassCache */ true);
}

void WebPage::reloadFromCache()
{
    d->m_mainFrame->loader()->reload(/* bypassCache */ false);
}

WebSettings* WebPage::settings() const
{
    return d->m_webSettings;
}

WebCookieJar* WebPage::cookieJar() const
{
    if (!d->m_cookieJar)
        d->m_cookieJar = new WebCookieJar();

    return d->m_cookieJar;
}

bool WebPage::isVisible() const
{
    return d->m_visible;
}

#if ENABLE(PAGE_VISIBILITY_API)
class DeferredTaskSetPageVisibilityState: public DeferredTask<&WebPagePrivate::m_wouldSetPageVisibilityState> {
public:
    explicit DeferredTaskSetPageVisibilityState(WebPagePrivate* webPagePrivate)
        : DeferredTaskType(webPagePrivate)
    {
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->setPageVisibilityState();
    }
};

void WebPagePrivate::setPageVisibilityState()
{
    if (m_page->defersLoading())
        m_deferredTasks.append(adoptPtr(new DeferredTaskSetPageVisibilityState(this)));
    else {
        DeferredTaskSetPageVisibilityState::finishOrCancel(this);

        static bool s_initialVisibilityState = true;

        m_page->setVisibilityState(m_visible && m_activationState == ActivationActive ? PageVisibilityStateVisible : PageVisibilityStateHidden, s_initialVisibilityState);
        s_initialVisibilityState = false;
    }
}
#endif

void WebPagePrivate::setVisible(bool visible)
{
    if (visible != m_visible) {
        if (visible) {
            if (m_mainFrame)
                m_mainFrame->animation()->resumeAnimations();
            if (m_page->scriptedAnimationsSuspended())
                m_page->resumeScriptedAnimations();
        } else {
            if (m_mainFrame)
                m_mainFrame->animation()->suspendAnimations();
            if (!m_page->scriptedAnimationsSuspended())
                m_page->suspendScriptedAnimations();
        }

        m_visible = visible;
    }

#if ENABLE(PAGE_VISIBILITY_API)
    setPageVisibilityState();
#endif
}

void WebPage::setVisible(bool visible)
{
    if (d->m_visible == visible)
        return;

    d->setVisible(visible);
    AuthenticationChallengeManager::instance()->pageVisibilityChanged(d, visible);

    if (!visible) {
        d->suspendBackingStore();

        // Remove this WebPage from the visible pages list.
        size_t foundIndex = visibleWebPages()->find(this);
        if (foundIndex != WTF::notFound)
            visibleWebPages()->remove(foundIndex);

        // Return the backing store to the last visible WebPage.
        if (BackingStorePrivate::currentBackingStoreOwner() == this && !visibleWebPages()->isEmpty())
            visibleWebPages()->last()->d->resumeBackingStore();

#if USE(ACCELERATED_COMPOSITING)
        // Root layer commit is not necessary for invisible tabs.
        // And release layer resources can reduce memory consumption.
        d->suspendRootLayerCommit();
#endif

        return;
    }

#if USE(ACCELERATED_COMPOSITING)
    d->resumeRootLayerCommit();
#endif

    // Push this WebPage to the top of the visible pages list.
    if (!visibleWebPages()->isEmpty() && visibleWebPages()->last() != this) {
        size_t foundIndex = visibleWebPages()->find(this);
        if (foundIndex != WTF::notFound)
            visibleWebPages()->remove(foundIndex);
    }
    visibleWebPages()->append(this);

    if (BackingStorePrivate::currentBackingStoreOwner()
        && BackingStorePrivate::currentBackingStoreOwner() != this)
        BackingStorePrivate::currentBackingStoreOwner()->d->suspendBackingStore();

    // resumeBackingStore will set the current owner to this webpage.
    // If we set the owner prematurely, then the tiles will not be reset.
    d->resumeBackingStore();
}

void WebPagePrivate::selectionChanged(Frame* frame)
{
    m_inputHandler->selectionChanged();

    // FIXME: This is a hack!
    // To ensure the selection being changed has its frame 'focused', lets
    // set it as focused ourselves (PR #104724).
    m_page->focusController()->setFocusedFrame(frame);
}

void WebPagePrivate::updateDelegatedOverlays(bool dispatched)
{
    // Track a dispatched message, we don't want to flood the webkit thread.
    // There can be as many as one more message enqued as needed but never less.
    if (dispatched)
        m_updateDelegatedOverlaysDispatched = false;
    else if (m_updateDelegatedOverlaysDispatched) {
        // Early return if there is message already pending on the webkit thread.
        return;
    }

    if (Platform::webKitThreadMessageClient()->isCurrentThread()) {
        // Must be called on the WebKit thread.
        if (m_selectionHandler->isSelectionActive())
            m_selectionHandler->selectionPositionChanged();
        if (m_inspectorOverlay)
            m_inspectorOverlay->update();

    } else if (m_selectionHandler->isSelectionActive()) {
        // Don't bother dispatching to webkit thread if selection and tap highlight are not active.
        m_updateDelegatedOverlaysDispatched = true;
        Platform::webKitThreadMessageClient()->dispatchMessage(Platform::createMethodCallMessage(&WebPagePrivate::updateDelegatedOverlays, this, true /*dispatched*/));
    }
}

void WebPage::setCaretHighlightStyle(Platform::CaretHighlightStyle style)
{
}

bool WebPage::setBatchEditingActive(bool active)
{
    return d->m_inputHandler->setBatchEditingActive(active);
}

bool WebPage::setInputSelection(unsigned start, unsigned end)
{
    if (d->m_page->defersLoading())
        return false;
    return d->m_inputHandler->setSelection(start, end);
}

int WebPage::inputCaretPosition() const
{
    return d->m_inputHandler->caretPosition();
}

class DeferredTaskPopupListSelectMultiple: public DeferredTask<&WebPagePrivate::m_wouldPopupListSelectMultiple> {
public:
    DeferredTaskPopupListSelectMultiple(WebPagePrivate* webPagePrivate, int size, const bool* selecteds)
        : DeferredTaskType(webPagePrivate)
    {
        webPagePrivate->m_cachedPopupListSelecteds.append(selecteds, size);
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_webPage->popupListClosed(webPagePrivate->m_cachedPopupListSelecteds.size(), webPagePrivate->m_cachedPopupListSelecteds.data());
    }
};

class DeferredTaskPopupListSelectSingle: public DeferredTask<&WebPagePrivate::m_wouldPopupListSelectSingle> {
public:
    explicit DeferredTaskPopupListSelectSingle(WebPagePrivate* webPagePrivate, int index)
        : DeferredTaskType(webPagePrivate)
    {
        webPagePrivate->m_cachedPopupListSelectedIndex = index;
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_webPage->popupListClosed(webPagePrivate->m_cachedPopupListSelectedIndex);
    }
};

void WebPage::popupListClosed(int size, const bool* selecteds)
{
    DeferredTaskPopupListSelectSingle::finishOrCancel(d);
    if (d->m_page->defersLoading()) {
        d->m_deferredTasks.append(adoptPtr(new DeferredTaskPopupListSelectMultiple(d, size, selecteds)));
        return;
    }
    DeferredTaskPopupListSelectMultiple::finishOrCancel(d);
    d->m_inputHandler->setPopupListIndexes(size, selecteds);
}

void WebPage::popupListClosed(int index)
{
    DeferredTaskPopupListSelectMultiple::finishOrCancel(d);
    if (d->m_page->defersLoading()) {
        d->m_deferredTasks.append(adoptPtr(new DeferredTaskPopupListSelectSingle(d, index)));
        return;
    }
    DeferredTaskPopupListSelectSingle::finishOrCancel(d);
    d->m_inputHandler->setPopupListIndex(index);
}

class DeferredTaskSetDateTimeInput: public DeferredTask<&WebPagePrivate::m_wouldSetDateTimeInput> {
public:
    explicit DeferredTaskSetDateTimeInput(WebPagePrivate* webPagePrivate, BlackBerry::Platform::String value)
        : DeferredTaskType(webPagePrivate)
    {
        webPagePrivate->m_cachedDateTimeInput = value;
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_webPage->setDateTimeInput(webPagePrivate->m_cachedDateTimeInput);
    }
};

void WebPage::setDateTimeInput(const BlackBerry::Platform::String& value)
{
    if (d->m_page->defersLoading()) {
        d->m_deferredTasks.append(adoptPtr(new DeferredTaskSetDateTimeInput(d, value)));
        return;
    }
    DeferredTaskSetDateTimeInput::finishOrCancel(d);
    d->m_inputHandler->setInputValue(value);
}

class DeferredTaskSetColorInput: public DeferredTask<&WebPagePrivate::m_wouldSetColorInput> {
public:
    explicit DeferredTaskSetColorInput(WebPagePrivate* webPagePrivate, BlackBerry::Platform::String value)
        : DeferredTaskType(webPagePrivate)
    {
        webPagePrivate->m_cachedColorInput = value;
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_webPage->setColorInput(webPagePrivate->m_cachedColorInput);
    }
};

void WebPage::setColorInput(const BlackBerry::Platform::String& value)
{
    if (d->m_page->defersLoading()) {
        d->m_deferredTasks.append(adoptPtr(new DeferredTaskSetColorInput(d, value)));
        return;
    }
    DeferredTaskSetColorInput::finishOrCancel(d);
    d->m_inputHandler->setInputValue(value);
}

void WebPage::setVirtualViewportSize(const Platform::IntSize& size)
{
    d->m_virtualViewportSize = WebCore::IntSize(size);
}

void WebPage::resetVirtualViewportOnCommitted(bool reset)
{
    d->m_resetVirtualViewportOnCommitted = reset;
}

Platform::IntSize WebPagePrivate::recomputeVirtualViewportFromViewportArguments()
{
    static const ViewportArguments defaultViewportArguments;
    if (m_viewportArguments == defaultViewportArguments)
        return IntSize();

    int desktopWidth = DEFAULT_MAX_LAYOUT_WIDTH;
    int deviceWidth = Platform::Graphics::Screen::primaryScreen()->width();
    int deviceHeight = Platform::Graphics::Screen::primaryScreen()->height();
    float devicePixelRatio = m_webSettings->devicePixelRatio();
    ViewportAttributes result = computeViewportAttributes(m_viewportArguments, desktopWidth, deviceWidth, deviceHeight, devicePixelRatio, m_defaultLayoutSize);
    m_page->setDeviceScaleFactor(devicePixelRatio);

    setUserScalable(m_webSettings->isUserScalable() && result.userScalable);
    if (result.initialScale > 0)
        setInitialScale(result.initialScale * devicePixelRatio);
    if (result.minimumScale > 0)
        setMinimumScale(result.minimumScale * devicePixelRatio);
    if (result.maximumScale > 0)
        setMaximumScale(result.maximumScale * devicePixelRatio);

    return Platform::IntSize(result.layoutSize.width(), result.layoutSize.height());
}

#if ENABLE(EVENT_MODE_METATAGS)
void WebPagePrivate::didReceiveCursorEventMode(CursorEventMode mode)
{
    if (mode != m_cursorEventMode)
        m_client->cursorEventModeChanged(toPlatformCursorEventMode(mode));
    m_cursorEventMode = mode;
}

void WebPagePrivate::didReceiveTouchEventMode(TouchEventMode mode)
{
    if (mode != m_touchEventMode)
        m_client->touchEventModeChanged(toPlatformTouchEventMode(mode));
    m_touchEventMode = mode;
}
#endif

void WebPagePrivate::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments)
{
    if (arguments == m_viewportArguments)
        return;

    // If the caller is trying to reset to default arguments, use the user supplied ones instead.
    static const ViewportArguments defaultViewportArguments;
    if (arguments == defaultViewportArguments) {
        m_viewportArguments = m_userViewportArguments;
        m_forceRespectViewportArguments = m_userViewportArguments != defaultViewportArguments;
    } else
        m_viewportArguments = arguments;

    // 0 width or height in viewport arguments makes no sense, and results in a very large initial scale.
    // In real world, a 0 width or height is usually caused by a syntax error in "content" field of viewport
    // meta tag, for example, using semicolon instead of comma as separator ("width=device-width; initial-scale=1.0").
    // We don't have a plan to tolerate the semicolon separator, but we can avoid applying 0 width/height.
    // I default it to ValueDeviceWidth rather than ValueAuto because in more cases the web site wants "device-width"
    // when they specify the viewport width.
    if (!m_viewportArguments.width)
        m_viewportArguments.width = ViewportArguments::ValueDeviceWidth;
    if (!m_viewportArguments.height)
        m_viewportArguments.height = ViewportArguments::ValueDeviceHeight;

    Platform::IntSize virtualViewport = recomputeVirtualViewportFromViewportArguments();
    m_webPage->setVirtualViewportSize(virtualViewport);

    // Reset m_userPerformedManualZoom and enable m_shouldZoomToInitialScaleAfterLoadFinished so that we can relayout
    // the page and zoom it to fit the screen when we dynamically change the meta viewport after the load is finished.
    bool isLoadFinished = loadState() == Finished;
    if (isLoadFinished) {
        m_userPerformedManualZoom = false;
        setShouldZoomToInitialScaleAfterLoadFinished(true);
    }
    if (loadState() == Committed || isLoadFinished)
        zoomToInitialScaleOnLoad();
}

void WebPagePrivate::onInputLocaleChanged(bool isRTL)
{
    if (isRTL != m_webSettings->isWritingDirectionRTL()) {
        m_webSettings->setWritingDirectionRTL(isRTL);
        m_inputHandler->handleInputLocaleChanged(isRTL);
    }
}

void WebPage::onInputLocaleChanged(bool isRTL)
{
    d->onInputLocaleChanged(isRTL);
}

void WebPagePrivate::suspendBackingStore()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_backingStore->d->isOpenGLCompositing()) {
        // A visible web page's backing store can be suspended when another web
        // page is taking over the backing store.
        if (m_visible)
            setCompositorDrawsRootLayer(true);

        return;
    }
#endif
}

void WebPagePrivate::resumeBackingStore()
{
    ASSERT(m_webPage->isVisible());

    bool directRendering = m_backingStore->d->shouldDirectRenderingToWindow();
    if (!m_backingStore->d->isActive()
        || shouldResetTilesWhenShown()
        || directRendering) {
        BackingStorePrivate::setCurrentBackingStoreOwner(m_webPage);

        // If we're OpenGL compositing, switching to accel comp drawing of the root layer
        // is a good substitute for backingstore blitting.
        if (m_backingStore->d->isOpenGLCompositing())
            setCompositorDrawsRootLayer(!m_backingStore->d->isActive());

        m_backingStore->d->orientationChanged(); // Updates tile geometry and creates visible tile buffer.
        m_backingStore->d->resetTiles();
        m_backingStore->d->updateTiles(false /* updateVisible */, false /* immediate */);

        // This value may have changed, so we need to update it.
        directRendering = m_backingStore->d->shouldDirectRenderingToWindow();
        if (m_backingStore->d->renderVisibleContents()) {
            if (!m_backingStore->d->isSuspended() && !directRendering)
                m_backingStore->d->blitVisibleContents();
            m_client->notifyPixelContentRendered(m_backingStore->d->visibleContentsRect());
        }
    } else {
        if (m_backingStore->d->isOpenGLCompositing())
           setCompositorDrawsRootLayer(false);

        // Rendering was disabled while we were hidden, so we need to update all tiles.
        m_backingStore->d->updateTiles(true /* updateVisible */, false /* immediate */);
    }

#if USE(ACCELERATED_COMPOSITING)
    setNeedsOneShotDrawingSynchronization();
#endif

    setShouldResetTilesWhenShown(false);
}

void WebPagePrivate::setScreenOrientation(int orientation)
{
    FOR_EACH_PLUGINVIEW(m_pluginViews)
        (*it)->handleOrientationEvent(orientation);

    m_pendingOrientation = -1;

#if ENABLE(ORIENTATION_EVENTS)
    if (m_mainFrame->orientation() == orientation)
        return;
    for (RefPtr<Frame> frame = m_mainFrame; frame; frame = frame->tree()->traverseNext())
        frame->sendOrientationChangeEvent(orientation);
#endif
}

void WebPage::setScreenOrientation(int orientation)
{
    d->m_pendingOrientation = orientation;
}

void WebPage::applyPendingOrientationIfNeeded()
{
    if (d->m_pendingOrientation != -1)
        d->setScreenOrientation(d->m_pendingOrientation);
}

void WebPagePrivate::setViewportSize(const IntSize& transformedActualVisibleSize, bool ensureFocusElementVisible)
{
    if (m_pendingOrientation == -1 && transformedActualVisibleSize == this->transformedActualVisibleSize())
        return;

    // Suspend all screen updates to the backingstore to make sure no-one tries to blit
    // while the window surface and the BackingStore are out of sync.
    BackingStore::ResumeUpdateOperation screenResumeOperation = BackingStore::Blit;
    m_backingStore->d->suspendScreenUpdates();
    m_backingStore->d->suspendBackingStoreUpdates();

    // The screen rotation is a major state transition that in this case is not properly
    // communicated to the backing store, since it does early return in most methods when
    // not visible.
    if (!m_visible || !m_backingStore->d->isActive())
        setShouldResetTilesWhenShown(true);

    bool hasPendingOrientation = m_pendingOrientation != -1;

    // The window buffers might have been recreated, cleared, moved, etc., so:
    m_backingStore->d->windowFrontBufferState()->clearBlittedRegion();
    m_backingStore->d->windowBackBufferState()->clearBlittedRegion();

    IntSize viewportSizeBefore = actualVisibleSize();
    FloatPoint centerOfVisibleContentsRect = this->centerOfVisibleContentsRect();
    bool newVisibleRectContainsOldVisibleRect = (m_actualVisibleHeight <= transformedActualVisibleSize.height())
                                          && (m_actualVisibleWidth <= transformedActualVisibleSize.width());

    bool atInitialScale = m_webPage->isAtInitialZoom();
    bool atTop = !scrollPosition().y();
    bool atLeft = !scrollPosition().x();

    // We need to reorient the visibleTileRect because the following code
    // could cause BackingStore::transformChanged to be called, where it
    // is used.
    // It is only dependent on the transformedViewportSize which has been
    // updated by now.
    m_backingStore->d->createVisibleTileBuffer();

    setDefaultLayoutSize(transformedActualVisibleSize);

    // Recompute our virtual viewport.
    bool needsLayout = false;
    static ViewportArguments defaultViewportArguments;
    if (m_viewportArguments != defaultViewportArguments) {
        // We may need to infer the width and height for the viewport with respect to the rotation.
        Platform::IntSize newVirtualViewport = recomputeVirtualViewportFromViewportArguments();
        ASSERT(!newVirtualViewport.isEmpty());
        m_webPage->setVirtualViewportSize(newVirtualViewport);
        m_mainFrame->view()->setUseFixedLayout(useFixedLayout());
        m_mainFrame->view()->setFixedLayoutSize(fixedLayoutSize());
        needsLayout = true;
    }

    // We switch this strictly after recomputing our virtual viewport as zoomToFitScale is dependent
    // upon these values and so is the virtual viewport recalculation.
    m_actualVisibleWidth = transformedActualVisibleSize.width();
    m_actualVisibleHeight = transformedActualVisibleSize.height();

    IntSize viewportSizeAfter = actualVisibleSize();

    IntSize offset;
    if (hasPendingOrientation) {
        offset = IntSize(roundf((viewportSizeBefore.width() - viewportSizeAfter.width()) / 2.0),
            roundf((viewportSizeBefore.height() - viewportSizeAfter.height()) / 2.0));
    }

    // As a special case, if we were anchored to the top left position at
    // the beginning of the rotation then preserve that anchor.
    if (atTop)
        offset.setHeight(0);
    if (atLeft)
        offset.setWidth(0);

    // If we're about to overscroll, cap the offset to valid content.
    IntPoint bottomRight(
        scrollPosition().x() + viewportSizeAfter.width(),
        scrollPosition().y() + viewportSizeAfter.height());

    if (bottomRight.x() + offset.width() > contentsSize().width())
        offset.setWidth(contentsSize().width() - bottomRight.x());
    if (bottomRight.y() + offset.height() > contentsSize().height())
        offset.setHeight(contentsSize().height() - bottomRight.y());
    if (scrollPosition().x() + offset.width() < 0)
        offset.setWidth(-scrollPosition().x());
    if (scrollPosition().y() + offset.height() < 0)
        offset.setHeight(-scrollPosition().y());

    // ...before scrolling, because the backing store will align its
    // tile matrix with the viewport as reported by the ScrollView.
    setScrollPosition(scrollPosition() + offset);
    notifyTransformedScrollChanged();

    m_backingStore->d->orientationChanged();
    m_backingStore->d->actualVisibleSizeChanged(transformedActualVisibleSize);

    // Update view mode only after we have updated the actual
    // visible size and reset the contents rect if necessary.
    if (setViewMode(viewMode()))
        needsLayout = true;

    bool needsLayoutToFindContentSize = hasPendingOrientation;

    // We need to update the viewport size of the WebCore::ScrollView...
    updateViewportSize(!needsLayoutToFindContentSize /* setFixedReportedSize */, false /* sendResizeEvent */);
    notifyTransformedContentsSizeChanged();

    // If automatic zooming is disabled, prevent zooming below.
    if (!m_webSettings->isZoomToFitOnLoad()) {
        atInitialScale = false;

        // Normally, if the contents size is smaller than the layout width,
        // we would zoom in. If zoom is disabled, we need to do something else,
        // or there will be artifacts due to non-rendered areas outside of the
        // contents size. If there is a virtual viewport, we are not allowed
        // to modify the fixed layout size, however.
        if (!hasVirtualViewport() && contentsSize().width() < m_defaultLayoutSize.width()) {
            m_mainFrame->view()->setUseFixedLayout(useFixedLayout());
            m_mainFrame->view()->setFixedLayoutSize(m_defaultLayoutSize);
            needsLayout = true;
        }
    }

    // Need to resume so that the backingstore will start recording the invalidated
    // rects from below.
    m_backingStore->d->resumeBackingStoreUpdates();

    // We might need to layout here to get a correct contentsSize so that zoomToFit
    // is calculated correctly.
    bool stillNeedsLayout = needsLayout;
    while (stillNeedsLayout) {
        setNeedsLayout();
        requestLayoutIfNeeded();
        stillNeedsLayout = false;

        // Emulate the zoomToFitWidthOnLoad algorithm if we're rotating.
        ++m_nestedLayoutFinishedCount;
        if (needsLayoutToFindContentSize) {
            if (setViewMode(viewMode()))
                stillNeedsLayout = true;
        }
    }
    m_nestedLayoutFinishedCount = 0;

    // As a special case if we were zoomed to the initial scale at the beginning
    // of the rotation then preserve that zoom level even when it is zoomToFit.
    double scale = atInitialScale ? initialScale() : currentScale();

    // Do our own clamping.
    scale = clampedScale(scale);

    if (needsLayoutToFindContentSize) {
        // Set the fixed reported size here so that innerWidth|innerHeight works
        // with this new scale.
        TransformationMatrix rotationMatrix;
        rotationMatrix.scale(scale);
        IntRect viewportRect = IntRect(IntPoint::zero(), transformedActualVisibleSize);
        IntRect actualVisibleRect = enclosingIntRect(rotationMatrix.inverse().mapRect(FloatRect(viewportRect)));
        m_mainFrame->view()->setFixedReportedSize(actualVisibleRect.size());
        m_mainFrame->view()->repaintFixedElementsAfterScrolling();
        requestLayoutIfNeeded();
        m_mainFrame->view()->updateFixedElementsAfterScrolling();
    }

    // We're going to need to send a resize event to JavaScript because
    // innerWidth and innerHeight depend on fixed reported size.
    // This is how we support mobile pages where JavaScript resizes
    // the page in order to get around the fixed layout size, e.g.
    // google maps when it detects a mobile user agent.
    if (shouldSendResizeEvent())
        m_mainFrame->eventHandler()->sendResizeEvent();

    // As a special case if we were anchored to the top left position at the beginning
    // of the rotation then preserve that anchor.
    FloatPoint anchor = centerOfVisibleContentsRect;
    if (atTop)
        anchor.setY(0);
    if (atLeft)
        anchor.setX(0);

    // Try and zoom here with clamping on.
    // FIXME: Determine why the above comment says "clamping on", yet we
    //   don't set enforceScaleClamping to true.
    // FIXME: Determine why ensureContentVisible() is only called for !success
    //   in the direct-rendering case, but in all cases otherwise. Chances are
    //   one of these is incorrect and we can unify two branches into one.
    if (m_backingStore->d->shouldDirectRenderingToWindow()) {
        bool success = zoomAboutPoint(scale, anchor, false /* enforceScaleClamping */, true /* forceRendering */);
        if (!success && ensureFocusElementVisible)
            ensureContentVisible(!newVisibleRectContainsOldVisibleRect);

    } else if (zoomAboutPoint(scale, anchor, false /*enforceScaleClamping*/, true /*forceRendering*/)) {
        if (ensureFocusElementVisible)
            ensureContentVisible(!newVisibleRectContainsOldVisibleRect);

    } else {

        // Suspend all updates to the backingstore.
        m_backingStore->d->suspendBackingStoreUpdates();

        // If the zoom failed, then we should still preserve the special case of scroll position.
        IntPoint scrollPosition = this->scrollPosition();
        if (atTop)
            scrollPosition.setY(0);
        if (atLeft)
            scrollPosition.setX(0);
        setScrollPosition(scrollPosition);

        // These might have been altered even if we didn't zoom so notify the client.
        notifyTransformedContentsSizeChanged();
        notifyTransformedScrollChanged();

        if (!needsLayout) {
            // The visible tiles for scroll must be up-to-date before we blit since we are not performing a layout.
            m_backingStore->d->updateTilesForScrollOrNotRenderedRegion();
        }

        if (ensureFocusElementVisible)
            ensureContentVisible(!newVisibleRectContainsOldVisibleRect);

        if (needsLayout) {
            m_backingStore->d->resetTiles();
            m_backingStore->d->updateTiles(false /* updateVisible */, false /* immediate */);
            screenResumeOperation = BackingStore::RenderAndBlit;
        }

        // If we need layout then render and blit, otherwise just blit as our viewport has changed.
        m_backingStore->d->resumeBackingStoreUpdates();
    }

    m_backingStore->d->resumeScreenUpdates(screenResumeOperation);
}

void WebPage::setViewportSize(const Platform::IntSize& viewportSize, bool ensureFocusElementVisible)
{
    d->setViewportSize(viewportSize, ensureFocusElementVisible);
}

void WebPagePrivate::setDefaultLayoutSize(const IntSize& size)
{
    IntSize screenSize = Platform::Graphics::Screen::primaryScreen()->size();
    ASSERT(size.width() <= screenSize.width() && size.height() <= screenSize.height());
    m_defaultLayoutSize = size.expandedTo(minimumLayoutSize).shrunkTo(screenSize);
}

void WebPage::setDefaultLayoutSize(const Platform::IntSize& platformSize)
{
    WebCore::IntSize size = platformSize;
    if (size == d->m_defaultLayoutSize)
        return;

    d->setDefaultLayoutSize(size);
    bool needsLayout = d->setViewMode(d->viewMode());
    if (needsLayout) {
        d->setNeedsLayout();
        if (!d->isLoading())
            d->requestLayoutIfNeeded();
    }
}

bool WebPage::mouseEvent(const Platform::MouseEvent& mouseEvent, bool* wheelDeltaAccepted)
{
    if (!d->m_mainFrame->view())
        return false;

    if (d->m_page->defersLoading())
        return false;

    PluginView* pluginView = d->m_fullScreenPluginView.get();
    if (pluginView)
        return d->dispatchMouseEventToFullScreenPlugin(pluginView, mouseEvent);

    if (mouseEvent.type() == Platform::MouseEvent::MouseAborted) {
        d->m_mainFrame->eventHandler()->setMousePressed(false);
        return false;
    }

    d->m_pluginMayOpenNewTab = true;

    d->m_lastUserEventTimestamp = currentTime();
    int clickCount = (d->m_selectionHandler->isSelectionActive() || mouseEvent.type() != Platform::MouseEvent::MouseMove) ? 1 : 0;

    // Set the button type.
    MouseButton buttonType = NoButton;
    if (mouseEvent.isLeftButton())
        buttonType = LeftButton;
    else if (mouseEvent.isRightButton())
        buttonType = RightButton;
    else if (mouseEvent.isMiddleButton())
        buttonType = MiddleButton;

    // Create our event.
    PlatformMouseEvent platformMouseEvent(d->mapFromTransformed(mouseEvent.position()), mouseEvent.screenPosition(),
                                          toWebCoreMouseEventType(mouseEvent.type()), clickCount, buttonType, PointingDevice);
    d->m_lastMouseEvent = platformMouseEvent;
    bool success = d->handleMouseEvent(platformMouseEvent);

    if (mouseEvent.wheelTicks()) {
        PlatformWheelEvent wheelEvent(d->mapFromTransformed(mouseEvent.position()), mouseEvent.screenPosition(),
                                      0, -mouseEvent.wheelDelta(),
                                      0, -mouseEvent.wheelTicks(),
                                      ScrollByPixelWheelEvent,
                                      false /* shiftKey */, false /* ctrlKey */,
                                      false /* altKey */, false /* metaKey */);
        if (wheelDeltaAccepted)
            *wheelDeltaAccepted = d->handleWheelEvent(wheelEvent);
    } else if (wheelDeltaAccepted)
        *wheelDeltaAccepted = false;

    return success;
}

bool WebPagePrivate::dispatchMouseEventToFullScreenPlugin(PluginView* plugin, const Platform::MouseEvent& event)
{
    NPEvent npEvent;
    NPMouseEvent mouseEvent;

    mouseEvent.x = event.screenPosition().x();
    mouseEvent.y = event.screenPosition().y();

    switch (event.type()) {
    case Platform::MouseEvent::MouseButtonDown:
        mouseEvent.type = MOUSE_BUTTON_DOWN;
        m_pluginMouseButtonPressed = true;
        break;
    case Platform::MouseEvent::MouseButtonUp:
        mouseEvent.type = MOUSE_BUTTON_UP;
        m_pluginMouseButtonPressed = false;
        break;
    case Platform::MouseEvent::MouseMove:
        mouseEvent.type = MOUSE_MOTION;
        break;
    default:
        return false;
    }

    mouseEvent.flags = 0;
    mouseEvent.button = m_pluginMouseButtonPressed;

    npEvent.type = NP_MouseEvent;
    npEvent.data = &mouseEvent;

    return plugin->dispatchFullScreenNPEvent(npEvent);
}

bool WebPagePrivate::handleMouseEvent(PlatformMouseEvent& mouseEvent)
{
    EventHandler* eventHandler = m_mainFrame->eventHandler();

    if (mouseEvent.type() == WebCore::PlatformEvent::MouseMoved)
        return eventHandler->mouseMoved(mouseEvent);

    if (mouseEvent.type() == WebCore::PlatformEvent::MouseScroll)
        return true;

    Node* node = 0;
    if (mouseEvent.inputMethod() == TouchScreen) {
        const FatFingersResult lastFatFingersResult = m_touchEventHandler->lastFatFingersResult();

        // Fat fingers can deal with shadow content.
        node = lastFatFingersResult.node(FatFingersResult::ShadowContentNotAllowed);
    }

    if (!node) {
        HitTestResult result = eventHandler->hitTestResultAtPoint(mapFromViewportToContents(mouseEvent.position()), false /*allowShadowContent*/);
        node = result.innerNode();
    }

    if (mouseEvent.type() == WebCore::PlatformEvent::MousePressed) {
        m_inputHandler->setInputModeEnabled();
        if (m_inputHandler->willOpenPopupForNode(node)) {
            // Do not allow any human generated mouse or keyboard events to select <option>s in the list box
            // because we use a pop up dialog to handle the actual selections. This prevents options from
            // being selected prior to displaying the pop up dialog. The contents of the listbox are for
            // display only.
            //
            // FIXME: We explicitly do not forward this event to WebCore so as to preserve symmetry with
            // the MouseEventReleased handling (below). This has the side-effect that mousedown events
            // are not fired for human generated mouse press events. See RIM Bug #1579.

            // We do focus <select>/<option> on mouse down so that a Focus event is fired and have the
            // element painted in its focus state on repaint.
            ASSERT(node->isElementNode());
            if (node->isElementNode()) {
                Element* element = static_cast<Element*>(node);
                element->focus();
            }
        } else
            eventHandler->handleMousePressEvent(mouseEvent);
    } else if (mouseEvent.type() == WebCore::PlatformEvent::MouseReleased) {
        // FIXME: For <select> and <options> elements, we explicitly do not forward this event to WebCore so
        // as to preserve symmetry with the MouseEventPressed handling (above). This has the side-effect that
        // mouseup events are not fired on such elements for human generated mouse release events. See RIM Bug #1579.
        if (!m_inputHandler->didNodeOpenPopup(node))
            eventHandler->handleMouseReleaseEvent(mouseEvent);
    }

    return true;
}

bool WebPagePrivate::handleWheelEvent(PlatformWheelEvent& wheelEvent)
{
    return m_mainFrame->eventHandler()->handleWheelEvent(wheelEvent);
}

bool WebPage::touchEvent(const Platform::TouchEvent& event)
{
#if DEBUG_TOUCH_EVENTS
    BBLOG(LogLevelCritical, "%s", event.toString().c_str());
#endif

#if ENABLE(TOUCH_EVENTS)
    if (!d->m_mainFrame)
        return false;

    if (d->m_page->defersLoading())
        return false;

    PluginView* pluginView = d->m_fullScreenPluginView.get();
    if (pluginView)
        return d->dispatchTouchEventToFullScreenPlugin(pluginView, event);

    Platform::TouchEvent tEvent = event;
    for (unsigned i = 0; i < event.m_points.size(); i++) {
        tEvent.m_points[i].m_pos = d->mapFromTransformed(tEvent.m_points[i].m_pos);
        tEvent.m_points[i].m_screenPos = tEvent.m_points[i].m_screenPos;
    }

    if (event.isSingleTap())
        d->m_pluginMayOpenNewTab = true;
    else if (tEvent.m_type == Platform::TouchEvent::TouchStart || tEvent.m_type == Platform::TouchEvent::TouchCancel)
        d->m_pluginMayOpenNewTab = false;

    if (tEvent.m_type == Platform::TouchEvent::TouchStart) {
        d->clearCachedHitTestResult();
        d->m_touchEventHandler->doFatFingers(tEvent.m_points[0]);

        // Draw tap highlight as soon as possible if we can
        Element* elementUnderFatFinger = d->m_touchEventHandler->lastFatFingersResult().nodeAsElementIfApplicable();
        if (elementUnderFatFinger)
            d->m_touchEventHandler->drawTapHighlight();
    }

    bool handled = false;

    if (!event.m_type != Platform::TouchEvent::TouchInjected)
        handled = d->m_mainFrame->eventHandler()->handleTouchEvent(PlatformTouchEvent(&tEvent));

    if (d->m_preventDefaultOnTouchStart) {
        if (tEvent.m_type == Platform::TouchEvent::TouchEnd || tEvent.m_type == Platform::TouchEvent::TouchCancel)
            d->m_preventDefaultOnTouchStart = false;
        return true;
    }

    if (handled) {
        if (tEvent.m_type == Platform::TouchEvent::TouchStart)
            d->m_preventDefaultOnTouchStart = true;
        return true;
    }

    if (event.isTouchHold())
        d->m_touchEventHandler->doFatFingers(tEvent.m_points[0]);

#endif

    return false;
}

void WebPagePrivate::setScrollOriginPoint(const Platform::IntPoint& documentScrollOrigin)
{
    m_inRegionScroller->d->reset();

    if (!m_hasInRegionScrollableAreas)
        return;

    postponeDocumentStyleRecalc();
    m_inRegionScroller->d->calculateInRegionScrollableAreasForPoint(documentScrollOrigin);
    if (!m_inRegionScroller->d->activeInRegionScrollableAreas().empty())
        m_client->notifyInRegionScrollableAreasChanged(m_inRegionScroller->d->activeInRegionScrollableAreas());
    resumeDocumentStyleRecalc();
}

void WebPage::setDocumentScrollOriginPoint(const Platform::IntPoint& documentScrollOrigin)
{
    d->setScrollOriginPoint(documentScrollOrigin);
}

void WebPage::touchPointAsMouseEvent(const Platform::TouchPoint& point)
{
    if (d->m_page->defersLoading())
        return;

    if (d->m_fullScreenPluginView.get())
        return;

    d->m_lastUserEventTimestamp = currentTime();

    Platform::TouchPoint tPoint = point;
    tPoint.m_pos = d->mapFromTransformed(tPoint.m_pos);

    d->m_touchEventHandler->handleTouchPoint(tPoint);
}

void WebPage::playSoundIfAnchorIsTarget() const
{
    d->m_touchEventHandler->playSoundIfAnchorIsTarget();
}

bool WebPagePrivate::dispatchTouchEventToFullScreenPlugin(PluginView* plugin, const Platform::TouchEvent& event)
{
    // Always convert touch events to mouse events.
    // Don't send actual touch events because no one has ever implemented them in flash.
    if (!event.neverHadMultiTouch())
        return false;

    if (event.isDoubleTap() || event.isTouchHold() || event.m_type == Platform::TouchEvent::TouchCancel) {
        NPTouchEvent npTouchEvent;

        if (event.isDoubleTap())
            npTouchEvent.type = TOUCH_EVENT_DOUBLETAP;
        else if (event.isTouchHold())
            npTouchEvent.type = TOUCH_EVENT_TOUCHHOLD;
        else if (event.m_type == Platform::TouchEvent::TouchCancel)
            npTouchEvent.type = TOUCH_EVENT_CANCEL;

        npTouchEvent.points = 0;
        npTouchEvent.size = event.m_points.size();
        if (npTouchEvent.size) {
            npTouchEvent.points = new NPTouchPoint[npTouchEvent.size];
            for (int i = 0; i < npTouchEvent.size; i++) {
                npTouchEvent.points[i].touchId = event.m_points[i].m_id;
                npTouchEvent.points[i].clientX = event.m_points[i].m_screenPos.x();
                npTouchEvent.points[i].clientY = event.m_points[i].m_screenPos.y();
                npTouchEvent.points[i].screenX = event.m_points[i].m_screenPos.x();
                npTouchEvent.points[i].screenY = event.m_points[i].m_screenPos.y();
                npTouchEvent.points[i].pageX = event.m_points[i].m_pos.x();
                npTouchEvent.points[i].pageY = event.m_points[i].m_pos.y();
            }
        }

        NPEvent npEvent;
        npEvent.type = NP_TouchEvent;
        npEvent.data = &npTouchEvent;

        plugin->dispatchFullScreenNPEvent(npEvent);
        delete[] npTouchEvent.points;
        return true;
    }

    dispatchTouchPointAsMouseEventToFullScreenPlugin(plugin, event.m_points[0]);
    return true;
}

bool WebPagePrivate::dispatchTouchPointAsMouseEventToFullScreenPlugin(PluginView* pluginView, const Platform::TouchPoint& point)
{
    NPEvent npEvent;
    NPMouseEvent mouse;

    switch (point.m_state) {
    case Platform::TouchPoint::TouchPressed:
        mouse.type = MOUSE_BUTTON_DOWN;
        break;
    case Platform::TouchPoint::TouchReleased:
        mouse.type = MOUSE_BUTTON_UP;
        break;
    case Platform::TouchPoint::TouchMoved:
        mouse.type = MOUSE_MOTION;
        break;
    case Platform::TouchPoint::TouchStationary:
        return true;
    }

    mouse.x = point.m_screenPos.x();
    mouse.y = point.m_screenPos.y();
    mouse.button = mouse.type != MOUSE_BUTTON_UP;
    mouse.flags = 0;
    npEvent.type = NP_MouseEvent;
    npEvent.data = &mouse;

    pluginView->dispatchFullScreenNPEvent(npEvent);
    return true;
}

void WebPage::touchEventCancel()
{
    d->m_pluginMayOpenNewTab = false;
    if (d->m_page->defersLoading())
        return;
}

Frame* WebPagePrivate::focusedOrMainFrame() const
{
    return m_page->focusController()->focusedOrMainFrame();
}

void WebPagePrivate::clearFocusNode()
{
    Frame* frame = focusedOrMainFrame();
    if (!frame)
        return;
    ASSERT(frame->document());

    if (frame->document()->focusedNode())
        frame->page()->focusController()->setFocusedNode(0, frame);
}

BlackBerry::Platform::String WebPage::textEncoding()
{
    Frame* frame = d->focusedOrMainFrame();
    if (!frame)
        return "";

    Document* document = frame->document();
    if (!document)
        return "";

    return document->loader()->writer()->encoding();
}

BlackBerry::Platform::String WebPage::forcedTextEncoding()
{
    Frame* frame = d->focusedOrMainFrame();
    if (!frame)
        return BlackBerry::Platform::String::emptyString();

    Document* document = frame->document();
    if (!document)
        return BlackBerry::Platform::String::emptyString();

    return document->loader()->overrideEncoding();
}

void WebPage::setForcedTextEncoding(const BlackBerry::Platform::String& encoding)
{
    if (!encoding.empty() && d->focusedOrMainFrame() && d->focusedOrMainFrame()->loader() && d->focusedOrMainFrame()->loader())
        return d->focusedOrMainFrame()->loader()->reloadWithOverrideEncoding(encoding);
}

static void handleScrolling(unsigned short character, WebPagePrivate* scroller)
{
    const int scrollFactor = 20;
    int dx = 0, dy = 0;
    switch (character) {
    case KEYCODE_LEFT:
        dx = -scrollFactor;
        break;
    case KEYCODE_RIGHT:
        dx = scrollFactor;
        break;
    case KEYCODE_UP:
        dy = -scrollFactor;
        break;
    case KEYCODE_DOWN:
        dy = scrollFactor;
        break;
    case KEYCODE_PG_UP:
        ASSERT(scroller);
        dy = scrollFactor - scroller->actualVisibleSize().height();
        break;
    case KEYCODE_PG_DOWN:
        ASSERT(scroller);
        dy = scroller->actualVisibleSize().height() - scrollFactor;
        break;
    }

    if (dx || dy) {
        // Don't use the scrollBy function because it triggers the scroll as originating from BlackBerry
        // but then it expects a separate invalidate which isn't sent in this case.
        ASSERT(scroller && scroller->m_mainFrame && scroller->m_mainFrame->view());
        IntPoint pos(scroller->scrollPosition() + IntSize(dx, dy));

        // Prevent over scrolling for arrows and Page up/down.
        if (pos.x() < 0)
            pos.setX(0);
        if (pos.y() < 0)
            pos.setY(0);
        if (pos.x() + scroller->actualVisibleSize().width() > scroller->contentsSize().width())
            pos.setX(scroller->contentsSize().width() - scroller->actualVisibleSize().width());
        if (pos.y() + scroller->actualVisibleSize().height() > scroller->contentsSize().height())
            pos.setY(scroller->contentsSize().height() - scroller->actualVisibleSize().height());

        scroller->m_mainFrame->view()->setScrollPosition(pos);
        scroller->m_client->scrollChanged();
    }
}

bool WebPage::keyEvent(const Platform::KeyboardEvent& keyboardEvent)
{
    if (!d->m_mainFrame->view())
        return false;

    if (d->m_page->defersLoading())
        return false;

    ASSERT(d->m_page->focusController());

    bool handled = d->m_inputHandler->handleKeyboardInput(keyboardEvent);

    if (!handled && keyboardEvent.type() == Platform::KeyboardEvent::KeyDown && !d->m_inputHandler->isInputMode()) {
        IntPoint previousPos = d->scrollPosition();
        handleScrolling(keyboardEvent.character(), d);
        handled = previousPos != d->scrollPosition();
    }

    return handled;
}

bool WebPage::deleteTextRelativeToCursor(unsigned int leftOffset, unsigned int rightOffset)
{
    if (d->m_page->defersLoading())
        return false;

    return d->m_inputHandler->deleteTextRelativeToCursor(leftOffset, rightOffset);
}

spannable_string_t* WebPage::selectedText(int32_t flags)
{
    return d->m_inputHandler->selectedText(flags);
}

spannable_string_t* WebPage::textBeforeCursor(int32_t length, int32_t flags)
{
    return d->m_inputHandler->textBeforeCursor(length, flags);
}

spannable_string_t* WebPage::textAfterCursor(int32_t length, int32_t flags)
{
    return d->m_inputHandler->textAfterCursor(length, flags);
}

extracted_text_t* WebPage::extractedTextRequest(extracted_text_request_t* request, int32_t flags)
{
    return d->m_inputHandler->extractedTextRequest(request, flags);
}

int32_t WebPage::setComposingRegion(int32_t start, int32_t end)
{
    return d->m_inputHandler->setComposingRegion(start, end);
}

int32_t WebPage::finishComposition()
{
    return d->m_inputHandler->finishComposition();
}

int32_t WebPage::setComposingText(spannable_string_t* spannableString, int32_t relativeCursorPosition)
{
    if (d->m_page->defersLoading())
        return -1;
    return d->m_inputHandler->setComposingText(spannableString, relativeCursorPosition);
}

int32_t WebPage::commitText(spannable_string_t* spannableString, int32_t relativeCursorPosition)
{
    if (d->m_page->defersLoading())
        return -1;
    return d->m_inputHandler->commitText(spannableString, relativeCursorPosition);
}

void WebPage::setSpellCheckingEnabled(bool enabled)
{
    static_cast<EditorClientBlackBerry*>(d->m_page->editorClient())->enableSpellChecking(enabled);
}

void WebPage::spellCheckingRequestCancelled(int32_t transactionId)
{
    d->m_inputHandler->spellCheckingRequestCancelled(transactionId);
}

void WebPage::spellCheckingRequestProcessed(int32_t transactionId, spannable_string_t* spannableString)
{
    d->m_inputHandler->spellCheckingRequestProcessed(transactionId, spannableString);
}

class DeferredTaskSelectionCancelled: public DeferredTask<&WebPagePrivate::m_wouldCancelSelection> {
public:
    explicit DeferredTaskSelectionCancelled(WebPagePrivate* webPagePrivate)
        : DeferredTaskType(webPagePrivate)
    {
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_webPage->selectionCancelled();
    }
};

void WebPage::selectionCancelled()
{
    if (d->m_page->defersLoading()) {
        d->m_deferredTasks.append(adoptPtr(new DeferredTaskSelectionCancelled(d)));
        return;
    }
    DeferredTaskSelectionCancelled::finishOrCancel(d);
    d->m_selectionHandler->cancelSelection();
}

bool WebPage::selectionContainsDocumentPoint(const Platform::IntPoint& point)
{
    return d->m_selectionHandler->selectionContains(point);
}

BlackBerry::Platform::String WebPage::title() const
{
    if (d->m_mainFrame->document())
        return d->m_mainFrame->loader()->documentLoader()->title().string();
    return BlackBerry::Platform::String::emptyString();
}

BlackBerry::Platform::String WebPage::selectedText() const
{
    return d->m_selectionHandler->selectedText();
}

BlackBerry::Platform::String WebPage::cutSelectedText()
{
    BlackBerry::Platform::String selectedText = d->m_selectionHandler->selectedText();
    if (!d->m_page->defersLoading() && !selectedText.empty())
        d->m_inputHandler->deleteSelection();
    return selectedText;
}

void WebPage::insertText(const BlackBerry::Platform::String& string)
{
    if (d->m_page->defersLoading())
        return;
    d->m_inputHandler->insertText(string);
}

void WebPage::clearCurrentInputField()
{
    if (d->m_page->defersLoading())
        return;
    d->m_inputHandler->clearField();
}

void WebPage::cut()
{
    if (d->m_page->defersLoading())
        return;
    d->m_inputHandler->cut();
}

void WebPage::copy()
{
    d->m_inputHandler->copy();
}

void WebPage::paste()
{
    if (d->m_page->defersLoading())
        return;
    d->m_inputHandler->paste();
}

void WebPage::selectAll()
{
    if (d->m_page->defersLoading())
        return;
    d->m_inputHandler->selectAll();
}

void WebPage::setDocumentSelection(const Platform::IntPoint& documentStartPoint, const Platform::IntPoint& documentEndPoint)
{
    if (d->m_page->defersLoading())
        return;

    d->m_selectionHandler->setSelection(documentStartPoint, documentEndPoint);
}

void WebPage::setDocumentCaretPosition(const Platform::IntPoint& documentCaretPosition)
{
    if (d->m_page->defersLoading())
        return;

    // Handled by selection handler as it's point based.
    d->m_selectionHandler->setCaretPosition(documentCaretPosition);
}

void WebPage::selectAtDocumentPoint(const Platform::IntPoint& documentPoint)
{
    if (d->m_page->defersLoading())
        return;

    d->m_selectionHandler->selectAtPoint(documentPoint);
}

BackingStore* WebPage::backingStore() const
{
    return d->m_backingStore;
}

InRegionScroller* WebPage::inRegionScroller() const
{
    return d->m_inRegionScroller.get();
}

void WebPagePrivate::setTextReflowAnchorPoint(const Platform::IntPoint& documentFocalPoint)
{
    // Should only be invoked when text reflow is enabled.
    ASSERT(m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled);

    m_currentPinchZoomNode = bestNodeForZoomUnderPoint(documentFocalPoint);
    if (!m_currentPinchZoomNode)
        return;

    IntRect nodeRect = rectForNode(m_currentPinchZoomNode.get());
    m_anchorInNodeRectRatio.set(
        static_cast<float>(documentFocalPoint.x() - nodeRect.x()) / nodeRect.width(),
        static_cast<float>(documentFocalPoint.y() - nodeRect.y()) / nodeRect.height());
}

bool WebPage::pinchZoomAboutPoint(double scale, const Platform::FloatPoint& documentFocalPoint)
{
    d->m_userPerformedManualZoom = true;
    d->m_userPerformedManualScroll = true;

    if (d->m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled) {
        d->setTextReflowAnchorPoint(webkitThreadViewportAccessor()->roundedDocumentContents(documentFocalPoint));

        // Theoretically, d->nodeForZoomUnderPoint(documentFocalPoint) can return null.
        if (!d->m_currentPinchZoomNode)
            return false;
    }

    return d->zoomAboutPoint(scale, documentFocalPoint);
}

#if ENABLE(VIEWPORT_REFLOW)
void WebPagePrivate::toggleTextReflowIfEnabledForBlockZoomOnly(bool shouldEnableTextReflow)
{
    if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabledOnlyForBlockZoom)
        m_page->settings()->setTextReflowEnabled(shouldEnableTextReflow);
}
#endif

bool WebPage::blockZoom(const Platform::IntPoint& documentTargetPoint)
{
    if (!d->m_mainFrame->view() || !d->isUserScalable())
        return false;

    Node* node = d->bestNodeForZoomUnderPoint(documentTargetPoint);
    if (!node)
        return false;

    IntRect nodeRect = d->rectForNode(node);
    IntRect blockRect;
    bool endOfBlockZoomMode = d->compareNodesForBlockZoom(d->m_currentBlockZoomAdjustedNode.get(), node);
    const double oldScale = d->m_transformationMatrix->m11();
    double newScale = 0;
    const double margin = endOfBlockZoomMode ? 0 : blockZoomMargin * 2 * oldScale;
    bool isFirstZoom = false;

    if (endOfBlockZoomMode) {
        // End of block zoom mode
        IntRect rect = d->blockZoomRectForNode(node);
        blockRect = IntRect(0, rect.y(), d->transformedContentsSize().width(), d->transformedContentsSize().height() - rect.y());
        d->m_shouldReflowBlock = false;
    } else {
        // Start/continue block zoom mode
        Node* tempBlockZoomAdjustedNode = d->m_currentBlockZoomAdjustedNode.get();
        blockRect = d->blockZoomRectForNode(node);

        // Don't use a block if it is too close to the size of the actual contents.
        // We allow this for images only so that they can be zoomed tight to the screen.
        if (!node->hasTagName(HTMLNames::imgTag)) {
            IntRect tRect = d->mapFromTransformed(blockRect);
            int blockArea = tRect.width() * tRect.height();
            int pageArea = d->contentsSize().width() * d->contentsSize().height();
            double blockToPageRatio = static_cast<double>(pageArea - blockArea) / pageArea;
            if (blockToPageRatio < minimumExpandingRatio) {
                // Restore old adjust node because zoom was canceled.
                d->m_currentBlockZoomAdjustedNode = tempBlockZoomAdjustedNode;
                return false;
            }
        }

        if (blockRect.isEmpty() || !blockRect.width() || !blockRect.height())
            return false;

        if (!d->m_currentBlockZoomNode.get())
            isFirstZoom = true;

        d->m_currentBlockZoomNode = node;
        d->m_shouldReflowBlock = true;
    }

    newScale = std::min(d->newScaleForBlockZoomRect(blockRect, oldScale, margin), d->maxBlockZoomScale());
    newScale = std::max(newScale, minimumScale());

#if ENABLE(VIEWPORT_REFLOW)
    // If reflowing, adjust the reflow-width of text node to make sure the font is a reasonable size.
    if (d->m_currentBlockZoomNode && d->m_shouldReflowBlock && settings()->textReflowMode() != WebSettings::TextReflowDisabled) {
        RenderObject* renderer = d->m_currentBlockZoomNode->renderer();
        if (renderer && renderer->isText()) {
            double newFontSize = renderer->style()->fontSize() * newScale;
            if (newFontSize < d->m_webSettings->defaultFontSize()) {
                newScale = std::min(static_cast<double>(d->m_webSettings->defaultFontSize()) / renderer->style()->fontSize(), d->maxBlockZoomScale());
                newScale = std::max(newScale, minimumScale());
            }
            blockRect.setWidth(oldScale * static_cast<double>(d->transformedActualVisibleSize().width()) / newScale);
            // Re-calculate the scale here to take in to account the margin.
            newScale = std::min(d->newScaleForBlockZoomRect(blockRect, oldScale, margin), d->maxBlockZoomScale());
            newScale = std::max(newScale, minimumScale()); // Still, it's not allowed to be smaller than minimum scale.
        }
    }
#endif

    // Align the zoomed block in the screen.
    double newBlockHeight = d->mapFromTransformed(blockRect).height();
    double newBlockWidth = d->mapFromTransformed(blockRect).width();
    double scaledViewportWidth = static_cast<double>(d->actualVisibleSize().width()) * oldScale / newScale;
    double scaledViewportHeight = static_cast<double>(d->actualVisibleSize().height()) * oldScale / newScale;
    double dx = std::max(0.0, (scaledViewportWidth - newBlockWidth) / 2.0);
    double dy = std::max(0.0, (scaledViewportHeight - newBlockHeight) / 2.0);

    RenderObject* renderer = d->m_currentBlockZoomAdjustedNode->renderer();
    FloatPoint anchor;
    FloatPoint topLeftPoint(d->mapFromTransformed(blockRect).location());
    if (renderer && renderer->isText()) {
        ETextAlign textAlign = renderer->style()->textAlign();
        switch (textAlign) {
        case CENTER:
        case WEBKIT_CENTER:
            anchor = FloatPoint(nodeRect.x() + (nodeRect.width() - scaledViewportWidth) / 2, topLeftPoint.y());
            break;
        case LEFT:
        case WEBKIT_LEFT:
            anchor = topLeftPoint;
            break;
        case RIGHT:
        case WEBKIT_RIGHT:
            anchor = FloatPoint(nodeRect.x() + nodeRect.width() - scaledViewportWidth, topLeftPoint.y());
            break;
        case TAAUTO:
        case JUSTIFY:
        default:
            if (renderer->style()->isLeftToRightDirection())
                anchor = topLeftPoint;
            else
                anchor = FloatPoint(nodeRect.x() + nodeRect.width() - scaledViewportWidth, topLeftPoint.y());
            break;
        }
    } else
        anchor = renderer->style()->isLeftToRightDirection() ? topLeftPoint : FloatPoint(nodeRect.x() + nodeRect.width() - scaledViewportWidth, topLeftPoint.y());

    if (newBlockHeight <= scaledViewportHeight) {
        // The block fits in the viewport so center it.
        d->m_finalBlockPoint = FloatPoint(anchor.x() - dx, anchor.y() - dy);
    } else {
        // The block is longer than the viewport so top align it and add 3 pixel margin.
        d->m_finalBlockPoint = FloatPoint(anchor.x() - dx, anchor.y() - 3);
    }

#if ENABLE(VIEWPORT_REFLOW)
    // We don't know how long the reflowed block will be so we position it at the top of the screen with a small margin.
    if (settings()->textReflowMode() != WebSettings::TextReflowDisabled) {
        d->m_finalBlockPoint = FloatPoint(anchor.x() - dx, anchor.y() - 3);
        d->m_finalBlockPointReflowOffset = FloatPoint(-dx, -3);
    }
#endif

    // Make sure that the original node rect is visible in the screen after the zoom. This is necessary because the identified block rect might
    // not be the same as the original node rect, and it could force the original node rect off the screen.
    FloatRect br(anchor, FloatSize(scaledViewportWidth, scaledViewportHeight));
    if (!br.contains(IntPoint(documentTargetPoint))) {
        d->m_finalBlockPointReflowOffset.move(0, (documentTargetPoint.y() - scaledViewportHeight / 2) - d->m_finalBlockPoint.y());
        d->m_finalBlockPoint = FloatPoint(d->m_finalBlockPoint.x(), documentTargetPoint.y() - scaledViewportHeight / 2);
    }

    // Clamp the finalBlockPoint to not cause any overflow scrolling.
    if (d->m_finalBlockPoint.x() < 0) {
        d->m_finalBlockPoint.setX(0);
        d->m_finalBlockPointReflowOffset.setX(0);
    } else if (d->m_finalBlockPoint.x() + scaledViewportWidth > d->contentsSize().width()) {
        d->m_finalBlockPoint.setX(d->contentsSize().width() - scaledViewportWidth);
        d->m_finalBlockPointReflowOffset.setX(0);
    }

    if (d->m_finalBlockPoint.y() < 0) {
        d->m_finalBlockPoint.setY(0);
        d->m_finalBlockPointReflowOffset.setY(0);
    } else if (d->m_finalBlockPoint.y() + scaledViewportHeight > d->contentsSize().height()) {
        d->m_finalBlockPoint.setY(d->contentsSize().height() - scaledViewportHeight);
        d->m_finalBlockPointReflowOffset.setY(0);
    }

    // Don't block zoom if the user is zooming and the new scale is only marginally different from the
    // oldScale with only a marginal change in scroll position. Ignore scroll difference in the special case
    // that the zoom level is the minimumScale.
    if (!endOfBlockZoomMode && abs(newScale - oldScale) / oldScale < minimumExpandingRatio) {
        const double minimumDisplacement = minimumExpandingRatio * webkitThreadViewportAccessor()->documentViewportSize().width();
        if (oldScale == d->minimumScale() || (distanceBetweenPoints(d->scrollPosition(), roundUntransformedPoint(d->m_finalBlockPoint)) < minimumDisplacement && abs(newScale - oldScale) / oldScale < 0.10)) {
            if (isFirstZoom) {
                d->resetBlockZoom();
                return false;
            }
            // Zoom out of block zoom.
            blockZoom(documentTargetPoint);
            return true;
        }
    }

    d->m_blockZoomFinalScale = newScale;

    // We set this here to make sure we don't try to re-render the page at a different zoom level during loading.
    d->m_userPerformedManualZoom = true;
    d->m_userPerformedManualScroll = true;
    d->m_client->animateBlockZoom(d->m_blockZoomFinalScale, d->m_finalBlockPoint);

    return true;
}

bool WebPage::isMaxZoomed() const
{
    return (d->currentScale() == d->maximumScale()) || !d->isUserScalable();
}

bool WebPage::isMinZoomed() const
{
    return (d->currentScale() == d->minimumScale()) || !d->isUserScalable();
}

bool WebPage::isAtInitialZoom() const
{
    return (d->currentScale() == d->initialScale()) || !d->isUserScalable();
}

class DeferredTaskSetFocused: public DeferredTask<&WebPagePrivate::m_wouldSetFocused> {
public:
    explicit DeferredTaskSetFocused(WebPagePrivate* webPagePrivate, bool focused)
        : DeferredTaskType(webPagePrivate)
    {
        webPagePrivate->m_cachedFocused = focused;
    }
private:
    virtual void performInternal(WebPagePrivate* webPagePrivate)
    {
        webPagePrivate->m_webPage->setFocused(webPagePrivate->m_cachedFocused);
    }
};

void WebPage::setFocused(bool focused)
{
    if (d->m_page->defersLoading()) {
        d->m_deferredTasks.append(adoptPtr(new DeferredTaskSetFocused(d, focused)));
        return;
    }
    DeferredTaskSetFocused::finishOrCancel(d);
    FocusController* focusController = d->m_page->focusController();
    focusController->setActive(focused);
    if (focused) {
        Frame* frame = focusController->focusedFrame();
        if (!frame)
            focusController->setFocusedFrame(d->m_mainFrame);
    }
    focusController->setFocused(focused);
}

bool WebPage::findNextString(const char* text, bool forward, bool caseSensitive, bool wrap, bool highlightAllMatches)
{
    WebCore::FindOptions findOptions = WebCore::StartInSelection;
    if (!forward)
        findOptions |= WebCore::Backwards;
    if (!caseSensitive)
        findOptions |= WebCore::CaseInsensitive;

    // The WebCore::FindOptions::WrapAround boolean actually wraps the search
    // within the current frame as opposed to the entire Document, so we have to
    // provide our own wrapping code to wrap at the whole Document level.
    return d->m_inPageSearchManager->findNextString(String::fromUTF8(text), findOptions, wrap, highlightAllMatches);
}

void WebPage::runLayoutTests()
{
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    // FIXME: do we need API to toggle this?
    d->m_page->settings()->setDeveloperExtrasEnabled(true);

    if (!d->m_dumpRenderTree)
        d->m_dumpRenderTree = new DumpRenderTree(this);
    d->m_dumpRenderTree->runTests();
#endif
}

unsigned WebPage::timeoutForJavaScriptExecution() const
{
    return Settings::timeoutForJavaScriptExecution(d->m_page->groupName());
}

void WebPage::setTimeoutForJavaScriptExecution(unsigned ms)
{
    Settings::setTimeoutForJavaScriptExecution(d->m_page->groupName(), ms);

    Document* doc = d->m_page->mainFrame()->document();
    if (!doc)
        return;

    doc->globalData()->timeoutChecker.setTimeoutInterval(ms);
}

JSContextRef WebPage::scriptContext() const
{
    if (!d->m_mainFrame)
        return 0;

    JSC::Bindings::RootObject *root = d->m_mainFrame->script()->bindingRootObject();
    if (!root)
        return 0;

    JSC::ExecState *exec = root->globalObject()->globalExec();
    return toRef(exec);
}

JSValueRef WebPage::windowObject() const
{
    return toRef(d->m_mainFrame->script()->globalObject(mainThreadNormalWorld()));
}

// Serialize only the members of HistoryItem which are needed by the client,
// and copy them into a SharedArray. Also include the HistoryItem pointer which
// will be used by the client as an opaque reference to identify the item.
void WebPage::getBackForwardList(SharedArray<BackForwardEntry>& result) const
{
    HistoryItemVector entries = static_cast<BackForwardListImpl*>(d->m_page->backForward()->client())->entries();
    result.reset(new BackForwardEntry[entries.size()], entries.size());

    for (unsigned i = 0; i < entries.size(); ++i) {
        RefPtr<HistoryItem> entry = entries[i];
        BackForwardEntry& resultEntry = result[i];
        resultEntry.url = entry->urlString();
        resultEntry.originalUrl = entry->originalURLString();
        resultEntry.title = entry->title();
        resultEntry.networkToken = entry->viewState().networkToken;
        resultEntry.lastVisitWasHTTPNonGet = entry->lastVisitWasHTTPNonGet();
        resultEntry.id = backForwardIdFromHistoryItem(entry.get());

        // FIXME: seems we can remove this now?
        // Make sure the HistoryItem is not disposed while the result list is still being used, to make sure the pointer is not reused
        // will be balanced by deref in releaseBackForwardEntry.
        entry->ref();
    }
}

void WebPage::releaseBackForwardEntry(BackForwardId id) const
{
    HistoryItem* item = historyItemFromBackForwardId(id);
    ASSERT(item);
    item->deref();
}

void WebPage::clearBrowsingData()
{
    clearMemoryCaches();
    clearAppCache(d->m_page->groupName());
    clearLocalStorage();
    clearCookieCache();
    clearHistory();
    clearPluginSiteData();
}

void WebPage::clearHistory()
{
    // Don't clear the back-forward list as we might like to keep it.
    PageGroup::removeAllVisitedLinks();
}

void WebPage::clearCookies()
{
    clearCookieCache();
}

void WebPage::clearLocalStorage()
{
    if (PageGroup* group = d->m_page->groupPtr()) {
        if (StorageNamespace* storage = group->localStorage())
            storage->clearAllOriginsForDeletion();
    }
}

void WebPage::clearCredentials()
{
#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
    if (d->m_webSettings->isCredentialAutofillEnabled())
        credentialManager().clearCredentials();
#endif
}

void WebPage::clearAutofillData()
{
    if (d->m_webSettings->isFormAutofillEnabled())
        AutofillManager::clear();
}

void WebPage::clearNeverRememberSites()
{
#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
    if (d->m_webSettings->isCredentialAutofillEnabled())
        credentialManager().clearNeverRememberSites();
#endif
}

void WebPage::clearCache()
{
    clearMemoryCaches();
    clearAppCache(d->m_page->groupName());
}

void WebPage::clearBackForwardList(bool keepCurrentPage) const
{
    BackForwardListImpl* backForwardList = static_cast<BackForwardListImpl*>(d->m_page->backForward()->client());
    RefPtr<HistoryItem> currentItem = backForwardList->currentItem();
    while (!backForwardList->entries().isEmpty())
        backForwardList->removeItem(backForwardList->entries().last().get());
    if (keepCurrentPage)
        backForwardList->addItem(currentItem);
}

bool WebPage::isEnableLocalAccessToAllCookies() const
{
    return cookieManager().canLocalAccessAllCookies();
}

void WebPage::setEnableLocalAccessToAllCookies(bool enabled)
{
    cookieManager().setCanLocalAccessAllCookies(enabled);
}

void WebPage::addVisitedLink(const unsigned short* url, unsigned int length)
{
    ASSERT(d->m_page);
    d->m_page->group().addVisitedLink(url, length);
}

#if ENABLE(WEBDOM)
WebDOMDocument WebPage::document() const
{
    if (!d->m_mainFrame)
        return WebDOMDocument();
    return WebDOMDocument(d->m_mainFrame->document());
}

WebDOMNode WebPage::nodeAtDocumentPoint(const Platform::IntPoint& documentPoint)
{
    HitTestResult result = d->m_mainFrame->eventHandler()->hitTestResultAtPoint(WebCore::IntPoint(documentPoint), false);
    Node* node = result.innerNonSharedNode();
    return WebDOMNode(node);
}

bool WebPage::getNodeRect(const WebDOMNode& node, Platform::IntRect& result)
{
    Node* nodeImpl = node.impl();
    if (nodeImpl && nodeImpl->renderer()) {
        result = nodeImpl->getRect();
        return true;
    }

    return false;
}

bool WebPage::setNodeFocus(const WebDOMNode& node, bool on)
{
    Node* nodeImpl = node.impl();

    if (nodeImpl && nodeImpl->isFocusable()) {
        Document* doc = nodeImpl->document();
        if (Page* page = doc->page()) {
            // Modify if focusing on node or turning off focused node.
            if (on) {
                page->focusController()->setFocusedNode(nodeImpl, doc->frame());
                if (nodeImpl->isElementNode())
                    static_cast<Element*>(nodeImpl)->updateFocusAppearance(true);
                d->m_inputHandler->didNodeOpenPopup(nodeImpl);
            } else if (doc->focusedNode() == nodeImpl) // && !on
                page->focusController()->setFocusedNode(0, doc->frame());

            return true;
        }
    }
    return false;
}

bool WebPage::setNodeHovered(const WebDOMNode& node, bool on)
{
    if (Node* nodeImpl = node.impl()) {
        nodeImpl->setHovered(on);
        return true;
    }
    return false;
}

bool WebPage::nodeHasHover(const WebDOMNode& node)
{
    if (Node* nodeImpl = node.impl()) {
        if (RenderStyle* style = nodeImpl->renderStyle())
            return style->affectedByHoverRules();
    }
    return false;
}
#endif

void WebPage::initPopupWebView(BlackBerry::WebKit::WebPage* webPage)
{
    d->m_selectPopup->init(webPage);
}

String WebPagePrivate::findPatternStringForUrl(const KURL& url) const
{
    if ((m_webSettings->shouldHandlePatternUrls() && protocolIs(url, "pattern"))
            || protocolIs(url, "tel")
            || protocolIs(url, "wtai")
            || protocolIs(url, "cti")
            || protocolIs(url, "mailto")
            || protocolIs(url, "sms")
            || protocolIs(url, "pin")) {
        return url;
    }
    return String();
}

bool WebPage::defersLoading() const
{
    return d->m_page->defersLoading();
}

void WebPage::notifyPagePause()
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handlePauseEvent();
}

void WebPage::notifyPageResume()
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handleResumeEvent();
}

void WebPage::notifyPageBackground()
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handleBackgroundEvent();
}

void WebPage::notifyPageForeground()
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handleForegroundEvent();
}

void WebPage::notifyPageFullScreenAllowed()
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handleFullScreenAllowedEvent();
}

void WebPage::notifyPageFullScreenExit()
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handleFullScreenExitEvent();
}

void WebPage::notifyDeviceIdleStateChange(bool enterIdle)
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
        (*it)->handleIdleEvent(enterIdle);
}

void WebPagePrivate::notifyAppActivationStateChange(ActivationStateType activationState)
{
    m_activationState = activationState;

#if ENABLE(PAGE_VISIBILITY_API)
    setPageVisibilityState();
#endif
}

void WebPage::notifyAppActivationStateChange(ActivationStateType activationState)
{
#if ENABLE(VIDEO)
    MediaPlayerPrivate::notifyAppActivatedEvent(activationState == ActivationActive);
#endif

    FOR_EACH_PLUGINVIEW(d->m_pluginViews) {
        switch (activationState) {
        case ActivationActive:
            (*it)->handleAppActivatedEvent();
            break;
        case ActivationInactive:
            (*it)->handleAppDeactivatedEvent();
            break;
        case ActivationStandby:
            (*it)->handleAppStandbyEvent();
            break;
        }
    }

    d->notifyAppActivationStateChange(activationState);
}

void WebPage::notifySwipeEvent()
{
    if (d->m_fullScreenPluginView.get())
       d->m_fullScreenPluginView->handleSwipeEvent();
    else
       notifyFullScreenVideoExited(true);
}

void WebPage::notifyScreenPowerStateChanged(bool powered)
{
    FOR_EACH_PLUGINVIEW(d->m_pluginViews)
       (*it)->handleScreenPowerEvent(powered);
}

void WebPage::notifyFullScreenVideoExited(bool done)
{
    UNUSED_PARAM(done);
#if ENABLE(VIDEO)
    if (d->m_webSettings->fullScreenVideoCapable()) {
        if (HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(d->m_fullscreenVideoNode.get()))
            mediaElement->exitFullscreen();
    } else {
#if ENABLE(FULLSCREEN_API)
        if (Element* element = static_cast<Element*>(d->m_fullscreenVideoNode.get()))
            element->document()->webkitCancelFullScreen();
#endif
    }
#endif
}

void WebPage::clearPluginSiteData()
{
    PluginDatabase* database = PluginDatabase::installedPlugins(true);

    if (!database)
        return;

    Vector<PluginPackage*> plugins = database->plugins();

    Vector<PluginPackage*>::const_iterator end = plugins.end();
    for (Vector<PluginPackage*>::const_iterator it = plugins.begin(); it != end; ++it)
        (*it)->clearSiteData(String());
}

void WebPage::onNetworkAvailabilityChanged(bool available)
{
    updateOnlineStatus(available);
}

void WebPage::onCertificateStoreLocationSet(const BlackBerry::Platform::String& caPath)
{
#if ENABLE(VIDEO)
    MediaPlayerPrivate::setCertificatePath(caPath);
#endif
}

void WebPage::enableDNSPrefetch()
{
    d->m_page->settings()->setDNSPrefetchingEnabled(true);
}

void WebPage::disableDNSPrefetch()
{
    d->m_page->settings()->setDNSPrefetchingEnabled(false);
}

bool WebPage::isDNSPrefetchEnabled() const
{
    return d->m_page->settings()->dnsPrefetchingEnabled();
}

void WebPage::enableWebInspector()
{
    if (!d->m_inspectorClient)
        return;

    d->m_page->inspectorController()->connectFrontend(d->m_inspectorClient);
    d->m_page->settings()->setDeveloperExtrasEnabled(true);
    d->setPreventsScreenDimming(true);
}

void WebPage::disableWebInspector()
{
    if (isWebInspectorEnabled()) {
        d->m_page->inspectorController()->disconnectFrontend();
        d->m_page->settings()->setDeveloperExtrasEnabled(false);
        d->setPreventsScreenDimming(false);
    }
}

bool WebPage::isWebInspectorEnabled()
{
    return d->m_page->settings()->developerExtrasEnabled();
}

void WebPage::enablePasswordEcho()
{
    d->m_page->settings()->setPasswordEchoEnabled(true);
}

void WebPage::disablePasswordEcho()
{
    d->m_page->settings()->setPasswordEchoEnabled(false);
}

void WebPage::dispatchInspectorMessage(const BlackBerry::Platform::String& message)
{
    d->m_page->inspectorController()->dispatchMessageFromFrontend(message);
}

void WebPage::inspectCurrentContextElement()
{
    if (isWebInspectorEnabled() && d->m_currentContextNode.get())
        d->m_page->inspectorController()->inspect(d->m_currentContextNode.get());
}

bool WebPagePrivate::compositorDrawsRootLayer() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return m_compositor && m_compositor->drawsRootLayer();

    // WebKit thread implementation:
    RenderView* renderView = m_mainFrame->contentRenderer();
    if (!renderView || !renderView->layer() || !renderView->layer()->backing())
        return false;

    return !renderView->layer()->backing()->paintingGoesToWindow();
#else
    return false;
#endif
}

void WebPagePrivate::setCompositorDrawsRootLayer(bool compositorDrawsRootLayer)
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_page->settings()->forceCompositingMode() == compositorDrawsRootLayer)
        return;

    // When the BlackBerry port forces compositing mode, the root layer stops
    // painting to window and starts painting to layer instead.
    m_page->settings()->setForceCompositingMode(compositorDrawsRootLayer);

    if (!m_mainFrame)
        return;

    if (FrameView* view = m_mainFrame->view())
        view->updateCompositingLayers();
#endif
}

#if USE(ACCELERATED_COMPOSITING)
void WebPagePrivate::scheduleRootLayerCommit()
{
    if (!(m_frameLayers && m_frameLayers->hasLayer()) && !m_overlayLayer)
        return;

    m_needsCommit = true;
    if (!m_rootLayerCommitTimer->isActive()) {
#if DEBUG_AC_COMMIT
        BBLOG(Platform::LogLevelCritical, "%s: m_rootLayerCommitTimer->isActive() = %d", WTF_PRETTY_FUNCTION, m_rootLayerCommitTimer->isActive());
#endif
        m_rootLayerCommitTimer->startOneShot(0);
    }
}

static bool needsLayoutRecursive(FrameView* view)
{
    if (view->needsLayout())
        return true;

    bool subframesNeedsLayout = false;
    const HashSet<RefPtr<Widget> >* viewChildren = view->children();
    HashSet<RefPtr<Widget> >::const_iterator end = viewChildren->end();
    for (HashSet<RefPtr<Widget> >::const_iterator current = viewChildren->begin(); current != end && !subframesNeedsLayout; ++current) {
        Widget* widget = (*current).get();
        if (widget->isFrameView())
            subframesNeedsLayout |= needsLayoutRecursive(static_cast<FrameView*>(widget));
    }

    return subframesNeedsLayout;
}

LayerRenderingResults WebPagePrivate::lastCompositingResults() const
{
    if (m_compositor)
        return m_compositor->lastCompositingResults();
    return LayerRenderingResults();
}

GraphicsLayer* WebPagePrivate::overlayLayer()
{
    if (!m_overlayLayer)
        m_overlayLayer = GraphicsLayer::create(0, this);

    return m_overlayLayer.get();
}

void WebPagePrivate::setCompositor(PassRefPtr<WebPageCompositorPrivate> compositor)
{
    using namespace BlackBerry::Platform;

    // We depend on the current thread being the WebKit thread when it's not the Compositing thread.
    // That seems extremely likely to be the case, but let's assert just to make sure.
    ASSERT(webKitThreadMessageClient()->isCurrentThread());

    if (m_compositor || m_client->window())
        m_backingStore->d->suspendScreenUpdates();

    // The m_compositor member has to be modified during a sync call for thread
    // safe access to m_compositor and its refcount.
    userInterfaceThreadMessageClient()->dispatchSyncMessage(createMethodCallMessage(&WebPagePrivate::setCompositorHelper, this, compositor));

    if (m_compositor || m_client->window()) // the new compositor, if one was set
        m_backingStore->d->resumeScreenUpdates(BackingStore::RenderAndBlit);
}

void WebPagePrivate::setCompositorHelper(PassRefPtr<WebPageCompositorPrivate> compositor)
{
    using namespace BlackBerry::Platform;

    // The m_compositor member has to be modified during a sync call for thread
    // safe access to m_compositor and its refcount.
    ASSERT(userInterfaceThreadMessageClient()->isCurrentThread());

    m_compositor = compositor;
    if (m_compositor) {
        m_compositor->setPage(this);

        m_compositor->setBackgroundColor(m_webSettings->backgroundColor());
    }

    // The previous compositor, if any, has now released it's OpenGL resources,
    // so we can safely free the owned context, if any.
    m_ownedContext.clear();
}

void WebPagePrivate::setCompositorBackgroundColor(const Color& backgroundColor)
{
    if (m_compositor)
        m_compositor->setBackgroundColor(backgroundColor);
}

void WebPagePrivate::commitRootLayer(const IntRect& layoutRectForCompositing,
                                     const IntSize& contentsSizeForCompositing,
                                     bool drawsRootLayer)
{
#if DEBUG_AC_COMMIT
    BBLOG(Platform::LogLevelCritical, "%s: m_compositor = 0x%x",
            WTF_PRETTY_FUNCTION, m_compositor.get());
#endif

    if (!m_compositor)
        return;

    // Frame layers
    LayerWebKitThread* rootLayer = 0;
    if (m_frameLayers)
        rootLayer = m_frameLayers->rootLayer();

    if (rootLayer && rootLayer->layerCompositingThread() != m_compositor->rootLayer())
        m_compositor->setRootLayer(rootLayer->layerCompositingThread());

    // Overlay layers
    LayerWebKitThread* overlayLayer = 0;
    if (m_overlayLayer)
        overlayLayer = m_overlayLayer->platformLayer();

    if (overlayLayer && overlayLayer->layerCompositingThread() != m_compositor->overlayLayer())
        m_compositor->setOverlayLayer(overlayLayer->layerCompositingThread());

    m_compositor->setLayoutRectForCompositing(layoutRectForCompositing);
    m_compositor->setContentsSizeForCompositing(contentsSizeForCompositing);
    m_compositor->setDrawsRootLayer(drawsRootLayer);

    if (rootLayer)
        rootLayer->commitOnCompositingThread();

    if (overlayLayer)
        overlayLayer->commitOnCompositingThread();

    scheduleCompositingRun();
}

bool WebPagePrivate::commitRootLayerIfNeeded()
{
#if DEBUG_AC_COMMIT
    BBLOG(Platform::LogLevelCritical, "%s: m_suspendRootLayerCommit = %d, m_needsCommit = %d, m_frameLayers = 0x%x, m_frameLayers->hasLayer() = %d, needsLayoutRecursive() = %d",
            WTF_PRETTY_FUNCTION,
            m_suspendRootLayerCommit,
            m_needsCommit,
            m_frameLayers.get(),
            m_frameLayers && m_frameLayers->hasLayer(),
            m_mainFrame && m_mainFrame->view() && needsLayoutRecursive(m_mainFrame->view()));
#endif

    if (m_suspendRootLayerCommit)
        return false;

    if (!m_needsCommit)
        return false;

    // Don't bail if the layers were removed and we now need a one shot drawing sync as a consequence.
    if (!(m_frameLayers && m_frameLayers->hasLayer()) && !m_overlayLayer
     && !m_needsOneShotDrawingSynchronization)
        return false;

    FrameView* view = m_mainFrame->view();
    if (!view)
        return false;

    // This can do pretty much anything depending on the overlay,
    // so in case it causes relayout or schedule a commit, call it early.
    updateDelegatedOverlays();

    // If we sync compositing layers when a layout is pending, we may cause painting of compositing
    // layer content to occur before layout has happened, which will cause paintContents() to bail.
    if (needsLayoutRecursive(view)) {
        // In case of one shot drawing synchronization, you
        // should first layoutIfNeeded, render, then commit and draw the layers.
        ASSERT(!needsOneShotDrawingSynchronization());
        return false;
    }

    willComposite();
    m_needsCommit = false;
    // We get here either due to the commit timer, which would have called
    // render if a one shot sync was needed. Or we get called from render
    // before the timer times out, which means we are doing a one shot anyway.
    m_needsOneShotDrawingSynchronization = false;

    if (m_rootLayerCommitTimer->isActive())
        m_rootLayerCommitTimer->stop();

    double scale = currentScale();
    if (m_frameLayers && m_frameLayers->hasLayer())
        m_frameLayers->commitOnWebKitThread(scale);

    if (m_overlayLayer)
        m_overlayLayer->platformLayer()->commitOnWebKitThread(scale);

    // Stash the visible content rect according to webkit thread
    // This is the rectangle used to layout fixed positioned elements,
    // and that's what the layer renderer wants.
    IntRect layoutRectForCompositing(scrollPosition(), actualVisibleSize());
    IntSize contentsSizeForCompositing = contentsSize();
    bool drawsRootLayer = compositorDrawsRootLayer();

    // Commit changes made to the layers synchronously with the compositing thread.
    Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        Platform::createMethodCallMessage(
            &WebPagePrivate::commitRootLayer,
            this,
            layoutRectForCompositing,
            contentsSizeForCompositing,
            drawsRootLayer));

    didComposite();
    return true;
}

void WebPagePrivate::rootLayerCommitTimerFired(Timer<WebPagePrivate>*)
{
    if (m_suspendRootLayerCommit)
        return;

#if DEBUG_AC_COMMIT
    BBLOG(Platform::LogLevelCritical, "%s", WTF_PRETTY_FUNCTION);
#endif

    m_backingStore->d->instrumentBeginFrame();

    // The commit timer may have fired just before the layout timer, or for some
    // other reason we need layout. It's not allowed to commit when a layout is
    // pending, becaues a commit can cause parts of the web page to be rendered
    // to texture.
    // The layout can also turn of compositing altogether, so we need to be prepared
    // to handle a one shot drawing synchronization after the layout.
    requestLayoutIfNeeded();

    // If we transitioned to drawing the root layer using compositor instead of
    // backing store, doing a one shot drawing synchronization with the
    // backing store is never necessary, because the backing store draws
    // nothing.
    if (!compositorDrawsRootLayer()) {
        // If we are doing direct rendering and have a single rendering target,
        // committing is equivalent to a one shot drawing synchronization.
        // We need to re-render the web page, re-render the layers, and
        // then blit them on top of the re-rendered web page.
        if (m_backingStore->d->isOpenGLCompositing() && m_backingStore->d->shouldDirectRenderingToWindow())
            setNeedsOneShotDrawingSynchronization();

        if (needsOneShotDrawingSynchronization()) {
#if DEBUG_AC_COMMIT
            BBLOG(Platform::LogLevelCritical, "%s: OneShotDrawingSynchronization code path!", WTF_PRETTY_FUNCTION);
#endif

            const IntRect windowRect = IntRect(IntPoint::zero(), viewportSize());
            m_backingStore->d->repaint(windowRect, true /*contentChanged*/, true /*immediate*/);
            return;
        }
    }

    commitRootLayerIfNeeded();
}

void WebPagePrivate::setRootLayerWebKitThread(Frame* frame, LayerWebKitThread* layer)
{
    // This method updates the FrameLayers based on input from WebCore.
    // FrameLayers keeps track of the layer proxies attached to frames.
    // We will have to compute a new root layer and update the compositor.
    if (!layer && !m_frameLayers)
        return;

    if (!layer) {
         ASSERT(m_frameLayers);
         m_frameLayers->removeLayerByFrame(frame);
         if (!m_frameLayers->hasLayer())
             m_frameLayers.clear();
    } else {
        if (!m_frameLayers)
            m_frameLayers = adoptPtr(new FrameLayers(this));

        if (!m_frameLayers->containsLayerForFrame(frame))
            m_frameLayers->addLayer(frame, layer);

        ASSERT(m_frameLayers);
    }

    LayerCompositingThread* rootLayerCompositingThread = 0;
    if (m_frameLayers && m_frameLayers->rootLayer())
        rootLayerCompositingThread = m_frameLayers->rootLayer()->layerCompositingThread();

    setRootLayerCompositingThread(rootLayerCompositingThread);
}

void WebPagePrivate::setRootLayerCompositingThread(LayerCompositingThread* layer)
{
    if (!Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
            Platform::createMethodCallMessage(&WebPagePrivate::setRootLayerCompositingThread, this, layer));
        return;
    }

    if (layer && !m_compositor)
        createCompositor();

    // Don't ASSERT(m_compositor) here because setIsAcceleratedCompositingActive(true)
    // may not turn accelerated compositing on since m_backingStore is 0.
    if (m_compositor)
        m_compositor->setRootLayer(layer);
}

bool WebPagePrivate::createCompositor()
{
    // If there's no window, the compositor will be supplied by the API client
    if (!m_client->window())
        return false;

    m_ownedContext = GLES2Context::create(this);
    m_compositor = WebPageCompositorPrivate::create(this, 0);
    m_compositor->setContext(m_ownedContext.get());

    // The compositor is created in a sync message, so there's no risk of race condition on the
    // WebSettings.
    m_compositor->setBackgroundColor(m_webSettings->backgroundColor());

    return true;
}

void WebPagePrivate::destroyCompositor()
{
    // m_compositor is a RefPtr, so it may live on beyond this point.
    // Disconnect the compositor from us
    m_compositor->detach();
    m_compositor.clear();
    m_ownedContext.clear();
}

void WebPagePrivate::syncDestroyCompositorOnCompositingThread()
{
    if (!m_compositor)
        return;

    Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        Platform::createMethodCallMessage(
            &WebPagePrivate::destroyCompositor, this));
}

void WebPagePrivate::releaseLayerResources()
{
    if (!isAcceleratedCompositingActive())
        return;

    if (m_frameLayers)
        m_frameLayers->releaseLayerResources();

    Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        Platform::createMethodCallMessage(&WebPagePrivate::releaseLayerResourcesCompositingThread, this));
}

void WebPagePrivate::releaseLayerResourcesCompositingThread()
{
     m_compositor->releaseLayerResources();
}

void WebPagePrivate::suspendRootLayerCommit()
{
    if (m_suspendRootLayerCommit)
        return;

    m_suspendRootLayerCommit = true;

    if (!m_compositor)
        return;

    releaseLayerResources();
}

void WebPagePrivate::resumeRootLayerCommit()
{
    if (!m_suspendRootLayerCommit)
        return;

    m_suspendRootLayerCommit = false;
    m_needsCommit = true;
}

bool WebPagePrivate::needsOneShotDrawingSynchronization()
{
    return m_needsOneShotDrawingSynchronization;
}

void WebPagePrivate::setNeedsOneShotDrawingSynchronization()
{
    if (compositorDrawsRootLayer()) {
        scheduleRootLayerCommit();
        return;
    }

    // This means we have to commit layers on next render, or render on the next commit,
    // whichever happens first.
    m_needsCommit = true;
    m_needsOneShotDrawingSynchronization = true;
}

void WebPagePrivate::notifyFlushRequired(const GraphicsLayer*)
{
    scheduleRootLayerCommit();
}
#endif // USE(ACCELERATED_COMPOSITING)

void WebPagePrivate::enterFullscreenForNode(Node* node)
{
#if ENABLE(VIDEO)
    if (!node || !node->hasTagName(HTMLNames::videoTag))
        return;

    MediaPlayer* player = static_cast<HTMLMediaElement*>(node)->player();
    if (!player)
        return;

    MediaPlayerPrivate* mmrPlayer = static_cast<MediaPlayerPrivate*>(player->implementation());
    if (!mmrPlayer)
        return;

    Platform::Graphics::Window* window = mmrPlayer->getWindow();
    if (!window)
        return;

    const char* contextName = mmrPlayer->mmrContextName();
    if (!contextName)
        return;

    mmrPlayer->setFullscreenWebPageClient(m_client);
    m_fullscreenVideoNode = node;
    m_client->fullscreenStart(contextName, window, mmrPlayer->getWindowScreenRect());
#endif
}

void WebPagePrivate::exitFullscreenForNode(Node* node)
{
#if ENABLE(VIDEO)
    if (m_fullscreenVideoNode.get()) {
        m_client->fullscreenStop();
        m_fullscreenVideoNode = 0;
    }

    if (!node || !node->hasTagName(HTMLNames::videoTag))
        return;

    MediaPlayer* player = static_cast<HTMLMediaElement*>(node)->player();
    if (!player)
        return;

    MediaPlayerPrivate* mmrPlayer = static_cast<MediaPlayerPrivate*>(player->implementation());
    if (!mmrPlayer)
        return;

    // Fullscreen mode is being turned off, so MediaPlayerPrivate no longer needs the pointer.
    mmrPlayer->setFullscreenWebPageClient(0);
#endif
}

#if ENABLE(FULLSCREEN_API)
// TODO: We should remove this helper class when we decide to support all elements.
static bool containsVideoTags(Element* element)
{
    for (Node* node = element->firstChild(); node; node = node->traverseNextNode(element)) {
        if (node->hasTagName(HTMLNames::videoTag))
            return true;
    }
    return false;
}

void WebPagePrivate::enterFullScreenForElement(Element* element)
{
#if ENABLE(VIDEO)
    // TODO: We should not check video tag when we decide to support all elements.
    if (!element || (!element->hasTagName(HTMLNames::videoTag) && !containsVideoTags(element)))
        return;
    if (m_webSettings->fullScreenVideoCapable()) {
        // The Browser chrome has its own fullscreen video widget it wants to
        // use, and this is a video element. The only reason that
        // webkitWillEnterFullScreenForElement() and
        // webkitDidEnterFullScreenForElement() are still called in this case
        // is so that exitFullScreenForElement() gets called later.
        enterFullscreenForNode(element);
    } else {
        // When an element goes fullscreen, the viewport size changes and the scroll
        // position might change. So we keep track of it here, in order to restore it
        // once element leaves fullscreen.
        WebCore::IntPoint scrollPosition = m_mainFrame->view()->scrollPosition();
        m_xScrollOffsetBeforeFullScreen = scrollPosition.x();

        // The current scale can be clamped to a greater minimum scale when we relayout contents during
        // the change of the viewport size. Cache the current scale so that we can restore it when
        // leaving fullscreen. Otherwise, it is possible that we will use the wrong scale.
        m_scaleBeforeFullScreen = currentScale();

        // No fullscreen video widget has been made available by the Browser
        // chrome, or this is not a video element. The webkitRequestFullScreen
        // Javascript call is often made on a div element.
        // This is where we would hide the browser's chrome if we wanted to.
        client()->fullscreenStart();
        m_fullscreenVideoNode = element;
    }
#endif
}

void WebPagePrivate::exitFullScreenForElement(Element* element)
{
#if ENABLE(VIDEO)
    // TODO: We should not check video tag when we decide to support all elements.
    if (!element || (!element->hasTagName(HTMLNames::videoTag) && !containsVideoTags(element)))
        return;
    if (m_webSettings->fullScreenVideoCapable()) {
        // The Browser chrome has its own fullscreen video widget.
        exitFullscreenForNode(element);
    } else {
        // FIXME: Do we really need to suspend/resume both backingstore and screen here?
        m_backingStore->d->suspendBackingStoreUpdates();
        m_backingStore->d->suspendScreenUpdates();

        // When leaving fullscreen mode, we need to restore the 'x' scroll position
        // before fullscreen.
        // FIXME: We may need to respect 'y' position as well, because the web page always scrolls to
        // the top when leaving fullscreen mode.
        WebCore::IntPoint scrollPosition = m_mainFrame->view()->scrollPosition();
        m_mainFrame->view()->setScrollPosition(
            WebCore::IntPoint(m_xScrollOffsetBeforeFullScreen, scrollPosition.y()));
        m_xScrollOffsetBeforeFullScreen = -1;

        if (m_scaleBeforeFullScreen > 0) {
            // Restore the scale when leaving fullscreen. We can't use TransformationMatrix::scale(double) here, as it
            // will multiply the scale rather than set the scale.
            // FIXME: We can refactor this into setCurrentScale(double) if it is useful in the future.
            m_transformationMatrix->setM11(m_scaleBeforeFullScreen);
            m_transformationMatrix->setM22(m_scaleBeforeFullScreen);
            m_scaleBeforeFullScreen = -1.0;
        }

        notifyTransformChanged();
        // FIXME: Do we really need to suspend/resume both backingstore and screen here?
        m_backingStore->d->resumeBackingStoreUpdates();
        m_backingStore->d->resumeScreenUpdates(BackingStore::RenderAndBlit);

        // This is where we would restore the browser's chrome
        // if hidden above.
        client()->fullscreenStop();
        m_fullscreenVideoNode = 0;
    }
#endif
}
#endif

void WebPagePrivate::didChangeSettings(WebSettings* webSettings)
{
    Settings* coreSettings = m_page->settings();
    m_page->setGroupName(webSettings->pageGroupName());
    coreSettings->setXSSAuditorEnabled(webSettings->xssAuditorEnabled());
    coreSettings->setLoadsImagesAutomatically(webSettings->loadsImagesAutomatically());
    coreSettings->setShouldDrawBorderWhileLoadingImages(webSettings->shouldDrawBorderWhileLoadingImages());
    coreSettings->setScriptEnabled(webSettings->isJavaScriptEnabled());
    coreSettings->setPrivateBrowsingEnabled(webSettings->isPrivateBrowsingEnabled());
    coreSettings->setDeviceSupportsMouse(webSettings->deviceSupportsMouse());
    coreSettings->setDefaultFixedFontSize(webSettings->defaultFixedFontSize());
    coreSettings->setDefaultFontSize(webSettings->defaultFontSize());
    coreSettings->setMinimumLogicalFontSize(webSettings->minimumFontSize());
    if (!webSettings->serifFontFamily().empty())
        coreSettings->setSerifFontFamily(String(webSettings->serifFontFamily()));
    if (!webSettings->fixedFontFamily().empty())
        coreSettings->setFixedFontFamily(String(webSettings->fixedFontFamily()));
    if (!webSettings->sansSerifFontFamily().empty())
        coreSettings->setSansSerifFontFamily(String(webSettings->sansSerifFontFamily()));
    if (!webSettings->standardFontFamily().empty())
        coreSettings->setStandardFontFamily(String(webSettings->standardFontFamily()));
    coreSettings->setJavaScriptCanOpenWindowsAutomatically(webSettings->canJavaScriptOpenWindowsAutomatically());
    coreSettings->setAllowScriptsToCloseWindows(webSettings->canJavaScriptOpenWindowsAutomatically()); // Why are we using the same value as setJavaScriptCanOpenWindowsAutomatically()?
    coreSettings->setPluginsEnabled(webSettings->arePluginsEnabled());
    coreSettings->setDefaultTextEncodingName(webSettings->defaultTextEncodingName());
    coreSettings->setDownloadableBinaryFontsEnabled(webSettings->downloadableBinaryFontsEnabled());
    coreSettings->setSpatialNavigationEnabled(m_webSettings->isSpatialNavigationEnabled());
    coreSettings->setAsynchronousSpellCheckingEnabled(m_webSettings->isAsynchronousSpellCheckingEnabled());

    BlackBerry::Platform::String stylesheetURL = webSettings->userStyleSheetLocation();
    if (!stylesheetURL.empty())
        coreSettings->setUserStyleSheetLocation(KURL(KURL(), stylesheetURL));

    coreSettings->setFirstScheduledLayoutDelay(webSettings->firstScheduledLayoutDelay());
    coreSettings->setUseCache(webSettings->useWebKitCache());
    coreSettings->setCookieEnabled(webSettings->areCookiesEnabled());

#if ENABLE(SQL_DATABASE)
    // DatabaseTracker can only be initialized for once, so it doesn't
    // make sense to change database path after DatabaseTracker has
    // already been initialized.
    static bool dbinit = false;
    if (!dbinit && !webSettings->databasePath().empty()) {
        dbinit = true;
        DatabaseTracker::initializeTracker(webSettings->databasePath());
    }

    // The directory of cacheStorage for one page group can only be initialized once.
    static bool acinit = false;
    if (!acinit && !webSettings->appCachePath().empty()) {
        acinit = true;
        cacheStorage().setCacheDirectory(webSettings->appCachePath());
    }

    coreSettings->setLocalStorageDatabasePath(webSettings->localStoragePath());
    Database::setIsAvailable(webSettings->isDatabasesEnabled());
    DatabaseSync::setIsAvailable(webSettings->isDatabasesEnabled());

    coreSettings->setLocalStorageEnabled(webSettings->isLocalStorageEnabled());
    coreSettings->setOfflineWebApplicationCacheEnabled(webSettings->isAppCacheEnabled());

    m_page->group().groupSettings()->setLocalStorageQuotaBytes(webSettings->localStorageQuota());
    coreSettings->setSessionStorageQuota(webSettings->sessionStorageQuota());
    coreSettings->setUsesPageCache(webSettings->maximumPagesInCache());
    coreSettings->setFrameFlatteningEnabled(webSettings->isFrameFlatteningEnabled());
#endif

#if ENABLE(INDEXED_DATABASE)
    m_page->group().groupSettings()->setIndexedDBDatabasePath(webSettings->indexedDataBasePath());
#endif


#if ENABLE(WEB_SOCKETS)
    WebSocket::setIsAvailable(webSettings->areWebSocketsEnabled());
#endif

#if ENABLE(FULLSCREEN_API)
    // This allows Javascript to call webkitRequestFullScreen() on an element.
    coreSettings->setFullScreenEnabled(true);
#endif

#if ENABLE(VIEWPORT_REFLOW)
    coreSettings->setTextReflowEnabled(webSettings->textReflowMode() == WebSettings::TextReflowEnabled);
#endif

    // FIXME: We don't want HTMLTokenizer to yield for anything other than email case because
    // call to currentTime() is too expensive on our platform. See RIM Bug #746.
    coreSettings->setShouldUseFirstScheduledLayoutDelay(webSettings->isEmailMode());
    coreSettings->setProcessHTTPEquiv(!webSettings->isEmailMode());

    coreSettings->setShouldUseCrossOriginProtocolCheck(!webSettings->allowCrossSiteRequests());
    coreSettings->setWebSecurityEnabled(!webSettings->allowCrossSiteRequests());

    cookieManager().setPrivateMode(webSettings->isPrivateBrowsingEnabled());

    CredentialStorage::setPrivateMode(webSettings->isPrivateBrowsingEnabled());

    if (m_mainFrame && m_mainFrame->view()) {
        Color backgroundColor(webSettings->backgroundColor());
        m_mainFrame->view()->updateBackgroundRecursively(backgroundColor, backgroundColor.hasAlpha());

        Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            createMethodCallMessage(&WebPagePrivate::setCompositorBackgroundColor, this, backgroundColor));
    }
    if (m_backingStore) {
        m_backingStore->d->setWebPageBackgroundColor(m_mainFrame && m_mainFrame->view()
            ? m_mainFrame->view()->documentBackgroundColor()
            : webSettings->backgroundColor());
    }

    m_page->setDeviceScaleFactor(webSettings->devicePixelRatio());
}

BlackBerry::Platform::String WebPage::textHasAttribute(const BlackBerry::Platform::String& query) const
{
    if (Document* doc = d->m_page->focusController()->focusedOrMainFrame()->document())
        return doc->queryCommandValue(query);

    return "";
}

void WebPage::setJavaScriptCanAccessClipboard(bool enabled)
{
    d->m_page->settings()->setJavaScriptCanAccessClipboard(enabled);
}

#if USE(ACCELERATED_COMPOSITING)
void WebPagePrivate::blitVisibleContents()
{
    if (m_backingStore->d->shouldDirectRenderingToWindow())
        return;

    m_backingStore->d->blitVisibleContents();
}

void WebPagePrivate::scheduleCompositingRun()
{
    if (WebPageCompositorClient* compositorClient = compositor()->client()) {
        double animationTime = compositorClient->requestAnimationFrame();
        compositorClient->invalidate(animationTime);
        return;
    }

    blitVisibleContents();
}

#endif

void WebPage::setWebGLEnabled(bool enabled)
{
    d->m_page->settings()->setWebGLEnabled(enabled);
}

bool WebPage::isWebGLEnabled() const
{
    return d->m_page->settings()->webGLEnabled();
}

void WebPagePrivate::frameUnloaded(const Frame* frame)
{
    m_inputHandler->frameUnloaded(frame);
    m_inPageSearchManager->frameUnloaded(frame);
}

const BlackBerry::Platform::String& WebPagePrivate::defaultUserAgent()
{
    static BlackBerry::Platform::String* defaultUserAgent = 0;
    if (!defaultUserAgent) {
        BlackBerry::Platform::DeviceInfo* info = BlackBerry::Platform::DeviceInfo::instance();
        char uaBuffer[256];
        int uaSize = snprintf(uaBuffer, 256, "Mozilla/5.0 (%s) AppleWebKit/%d.%d+ (KHTML, like Gecko) Version/%s %sSafari/%d.%d+",
            info->family(), WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, info->osVersion(),
            info->isMobile() ? "Mobile " : "", WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION);

        if (uaSize <= 0 || uaSize >= 256)
            BLACKBERRY_CRASH();

        defaultUserAgent = new BlackBerry::Platform::String(uaBuffer, uaSize);
    }

    return *defaultUserAgent;
}

WebTapHighlight* WebPage::tapHighlight() const
{
    return d->m_tapHighlight.get();
}

void WebPage::addOverlay(WebOverlay* overlay)
{
#if USE(ACCELERATED_COMPOSITING)
    if (overlay->d->graphicsLayer()) {
        overlay->d->setPage(d);
        d->overlayLayer()->addChild(overlay->d->graphicsLayer());
    }
#endif
}

void WebPage::removeOverlay(WebOverlay* overlay)
{
#if USE(ACCELERATED_COMPOSITING)
    if (overlay->d->graphicsLayer()->parent() != d->overlayLayer())
        return;

    overlay->removeFromParent();
    overlay->d->clear();
    overlay->d->setPage(0);
#endif
}

void WebPage::addCompositingThreadOverlay(WebOverlay* overlay)
{
#if USE(ACCELERATED_COMPOSITING)
    ASSERT(Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (!d->compositor())
        return;

    overlay->d->setPage(d);
    d->compositor()->addOverlay(overlay->d->layerCompositingThread());
#endif
}

void WebPage::removeCompositingThreadOverlay(WebOverlay* overlay)
{
#if USE(ACCELERATED_COMPOSITING)
    ASSERT(Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (d->compositor())
        d->compositor()->removeOverlay(overlay->d->layerCompositingThread());
    overlay->d->clear();
    overlay->d->setPage(0);
#endif
}

void WebPage::popupOpened(PagePopupBlackBerry* webPopup)
{
    ASSERT(!d->m_selectPopup);
    d->m_selectPopup = webPopup;
}

void WebPage::popupClosed()
{
    ASSERT(d->m_selectPopup);
    d->m_selectPopup = 0;
}

bool WebPage::hasOpenedPopup() const
{
    return d->m_selectPopup;
}

PagePopupBlackBerry* WebPage::popup()
{
    return d->m_selectPopup;
}

void WebPagePrivate::setInspectorOverlayClient(InspectorOverlay::InspectorOverlayClient* inspectorOverlayClient)
{
    if (inspectorOverlayClient) {
        if (!m_inspectorOverlay)
            m_inspectorOverlay = InspectorOverlay::create(this, inspectorOverlayClient);
        else
            m_inspectorOverlay->setClient(inspectorOverlayClient);
        m_inspectorOverlay->update();
        scheduleRootLayerCommit();
    } else {
        if (m_inspectorOverlay) {
            m_inspectorOverlay->clear();
            m_inspectorOverlay = nullptr;
            scheduleRootLayerCommit();
        }
    }
}

void WebPagePrivate::applySizeOverride(int overrideWidth, int overrideHeight)
{
    m_client->requestUpdateViewport(overrideWidth, overrideHeight);
}

void WebPagePrivate::setTextZoomFactor(float textZoomFactor)
{
    if (!m_mainFrame)
        return;

    m_mainFrame->setTextZoomFactor(textZoomFactor);
}

void WebPagePrivate::restoreHistoryViewState(Platform::IntSize contentsSize, Platform::IntPoint scrollPosition, double scale, bool shouldReflowBlock)
{
    if (!m_mainFrame) {
        // FIXME: Do we really need to suspend/resume both backingstore and screen here?
        m_backingStore->d->resumeBackingStoreUpdates();
        m_backingStore->d->resumeScreenUpdates(BackingStore::RenderAndBlit);
        return;
    }

    m_mainFrame->view()->setContentsSizeFromHistory(contentsSize);

    // Here we need to set scroll position what we asked for.
    // So we use ScrollView::constrainsScrollingToContentEdge(false).
    bool oldConstrainsScrollingToContentEdge = m_mainFrame->view()->constrainsScrollingToContentEdge();
    m_mainFrame->view()->setConstrainsScrollingToContentEdge(false);
    setScrollPosition(scrollPosition);
    m_mainFrame->view()->setConstrainsScrollingToContentEdge(oldConstrainsScrollingToContentEdge);

    m_shouldReflowBlock = shouldReflowBlock;

    bool didZoom = zoomAboutPoint(scale, m_mainFrame->view()->scrollPosition(), true /* enforceScaleClamping */, true /*forceRendering*/, true /*isRestoringZoomLevel*/);
    // If we're already at that scale, then we should still force rendering
    // since our scroll position changed.
    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    m_backingStore->d->resumeBackingStoreUpdates();
    m_backingStore->d->resumeScreenUpdates(BackingStore::RenderAndBlit);

    if (!didZoom) {
        // We need to notify the client of the scroll position and content size change(s) above even if we didn't scale.
        notifyTransformedContentsSizeChanged();
        notifyTransformedScrollChanged();
    }
}

IntSize WebPagePrivate::screenSize() const
{
    return Platform::Graphics::Screen::primaryScreen()->size();
}

void WebPagePrivate::postponeDocumentStyleRecalc()
{
    if (Document* document = m_mainFrame->document()) {
        m_documentChildNeedsStyleRecalc = document->childNeedsStyleRecalc();
        document->clearChildNeedsStyleRecalc();

        m_documentStyleRecalcPostponed = document->isPendingStyleRecalc();
        document->unscheduleStyleRecalc();
    }
}

void WebPagePrivate::resumeDocumentStyleRecalc()
{
    if (Document* document = m_mainFrame->document()) {
        if (m_documentChildNeedsStyleRecalc)
            document->setChildNeedsStyleRecalc();

        if (m_documentStyleRecalcPostponed)
            document->scheduleStyleRecalc();
    }

    m_documentChildNeedsStyleRecalc = false;
    m_documentStyleRecalcPostponed = false;
}

const HitTestResult& WebPagePrivate::hitTestResult(const IntPoint& contentPos)
{
    if (m_cachedHitTestContentPos != contentPos) {
        m_cachedHitTestContentPos = contentPos;
        m_cachedHitTestResult = m_mainFrame->eventHandler()->hitTestResultAtPoint(m_cachedHitTestContentPos, true /*allowShadowContent*/);
    }

    return m_cachedHitTestResult;
}

void WebPagePrivate::clearCachedHitTestResult()
{
    m_cachedHitTestContentPos = WebCore::IntPoint(-1, -1);
}

void WebPagePrivate::willComposite()
{
    if (!m_page->settings()->developerExtrasEnabled())
        return;
    InspectorInstrumentation::willComposite(m_page);
}

void WebPagePrivate::didComposite()
{
    if (!m_page->settings()->developerExtrasEnabled())
        return;
    InspectorInstrumentation::didComposite(m_page);
}

void WebPage::updateNotificationPermission(const BlackBerry::Platform::String& requestId, bool allowed)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    d->notificationManager().updatePermission(requestId, allowed);
#else
    UNUSED_PARAM(requestId);
    UNUSED_PARAM(allowed);
#endif
}

void WebPage::notificationClicked(const BlackBerry::Platform::String& notificationId)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    d->notificationManager().notificationClicked(notificationId);
#else
    UNUSED_PARAM(notificationId);
#endif
}

void WebPage::notificationClosed(const BlackBerry::Platform::String& notificationId)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    d->notificationManager().notificationClosed(notificationId);
#else
    UNUSED_PARAM(notificationId);
#endif
}

void WebPage::notificationError(const BlackBerry::Platform::String& notificationId)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    d->notificationManager().notificationError(notificationId);
#else
    UNUSED_PARAM(notificationId);
#endif
}

void WebPage::notificationShown(const BlackBerry::Platform::String& notificationId)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    d->notificationManager().notificationShown(notificationId);
#else
    UNUSED_PARAM(notificationId);
#endif
}

}
}
