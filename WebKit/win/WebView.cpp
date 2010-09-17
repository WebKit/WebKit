/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebView.h"

#include "CFDictionaryPropertyBag.h"
#include "DOMCoreClasses.h"
#include "FullscreenVideoController.h"
#include "MarshallingHelpers.h"
#include "SoftLinking.h"
#include "WebBackForwardList.h"
#include "WebChromeClient.h"
#include "WebContextMenuClient.h"
#include "WebCoreTextRenderer.h"
#include "WebDatabaseManager.h"
#include "WebDocumentLoader.h"
#include "WebDownload.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebElementPropertyBag.h"
#include "WebFrame.h"
#include "WebGeolocationControllerClient.h"
#include "WebGeolocationPosition.h"
#include "WebIconDatabase.h"
#include "WebInspector.h"
#include "WebInspectorClient.h"
#include "WebKit.h"
#include "WebKitDLL.h"
#include "WebKitLogging.h"
#include "WebKitStatisticsPrivate.h"
#include "WebKitSystemBits.h"
#include "WebMutableURLRequest.h"
#include "WebNotificationCenter.h"
#include "WebPlatformStrategies.h"
#include "WebPluginHalterClient.h"
#include "WebPreferences.h"
#include "WebScriptWorld.h"
#include "WindowsTouch.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSValue.h>
#include <WebCore/AbstractDatabase.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/BString.h>
#include <WebCore/BackForwardList.h>
#include <WebCore/BitmapInfo.h>
#include <WebCore/Cache.h>
#include <WebCore/Chrome.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/CookieStorageWin.h>
#include <WebCore/Cursor.h>
#include <WebCore/Document.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/Editor.h>
#include <WebCore/EventHandler.h>
#include <WebCore/EventNames.h>
#include <WebCore/FileSystem.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/FrameWin.h>
#include <WebCore/GDIObjectCounter.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/IntRect.h>
#include <WebCore/JSElement.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Language.h>
#include <WebCore/Logging.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/Page.h>
#include <WebCore/PageCache.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginDatabase.h>
#include <WebCore/PluginView.h>
#include <WebCore/PopupMenu.h>
#include <WebCore/PopupMenuWin.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/RenderWidget.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/ScriptValue.h>
#include <WebCore/Scrollbar.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SelectionController.h>
#include <WebCore/Settings.h>
#include <WebCore/SimpleFontData.h>
#include <WebCore/TypingCommand.h>
#include <WebCore/WindowMessageBroadcaster.h>
#include <wtf/Threading.h>

#if ENABLE(CLIENT_BASED_GEOLOCATION)
#include <WebCore/GeolocationController.h>
#include <WebCore/GeolocationError.h>
#endif

#if PLATFORM(CG)
#include <CoreGraphics/CGContext.h>
#endif

#if PLATFORM(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if USE(CFNETWORK)
#include <CFNetwork/CFURLCachePriv.h>
#include <CFNetwork/CFURLProtocolPriv.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h> 
#endif

#include <ShlObj.h>
#include <comutil.h>
#include <dimm.h>
#include <oleacc.h>
#include <tchar.h>
#include <windowsx.h>
#include <wtf/HashSet.h>
#include <wtf/text/CString.h>

// Soft link functions for gestures and panning feedback
SOFT_LINK_LIBRARY(USER32);
SOFT_LINK_OPTIONAL(USER32, GetGestureInfo, BOOL, WINAPI, (HGESTUREINFO, PGESTUREINFO));
SOFT_LINK_OPTIONAL(USER32, SetGestureConfig, BOOL, WINAPI, (HWND, DWORD, UINT, PGESTURECONFIG, UINT));
SOFT_LINK_OPTIONAL(USER32, CloseGestureInfoHandle, BOOL, WINAPI, (HGESTUREINFO));
SOFT_LINK_LIBRARY(Uxtheme);
SOFT_LINK_OPTIONAL(Uxtheme, BeginPanningFeedback, BOOL, WINAPI, (HWND));
SOFT_LINK_OPTIONAL(Uxtheme, EndPanningFeedback, BOOL, WINAPI, (HWND, BOOL));
SOFT_LINK_OPTIONAL(Uxtheme, UpdatePanningFeedback, BOOL, WINAPI, (HWND, LONG, LONG, BOOL));

using namespace WebCore;
using namespace std;
using JSC::JSLock;

static HMODULE accessibilityLib;
static HashSet<WebView*> pendingDeleteBackingStoreSet;

static String osVersion();
static String webKitVersion();

WebView* kit(Page* page)
{
    return page ? static_cast<WebChromeClient*>(page->chrome()->client())->webView() : 0;
}

class PreferencesChangedOrRemovedObserver : public IWebNotificationObserver {
public:
    static PreferencesChangedOrRemovedObserver* sharedInstance();

private:
    PreferencesChangedOrRemovedObserver() {}
    ~PreferencesChangedOrRemovedObserver() {}

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) { return E_FAIL; }
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return 0; }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return 0; }

public:
    // IWebNotificationObserver
    virtual HRESULT STDMETHODCALLTYPE onNotify( 
        /* [in] */ IWebNotification* notification);

private:
    HRESULT notifyPreferencesChanged(WebCacheModel);
    HRESULT notifyPreferencesRemoved(WebCacheModel);
};

PreferencesChangedOrRemovedObserver* PreferencesChangedOrRemovedObserver::sharedInstance()
{
    static PreferencesChangedOrRemovedObserver* shared = new PreferencesChangedOrRemovedObserver;
    return shared;
}

HRESULT PreferencesChangedOrRemovedObserver::onNotify(IWebNotification* notification)
{
    HRESULT hr = S_OK;

    COMPtr<IUnknown> unkPrefs;
    hr = notification->getObject(&unkPrefs);
    if (FAILED(hr))
        return hr;

    COMPtr<IWebPreferences> preferences(Query, unkPrefs);
    if (!preferences)
        return E_NOINTERFACE;

    WebCacheModel cacheModel;
    hr = preferences->cacheModel(&cacheModel);
    if (FAILED(hr))
        return hr;

    BSTR nameBSTR;
    hr = notification->name(&nameBSTR);
    if (FAILED(hr))
        return hr;
    BString name;
    name.adoptBSTR(nameBSTR);

    if (wcscmp(name, WebPreferences::webPreferencesChangedNotification()) == 0)
        return notifyPreferencesChanged(cacheModel);

    if (wcscmp(name, WebPreferences::webPreferencesRemovedNotification()) == 0)
        return notifyPreferencesRemoved(cacheModel);

    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT PreferencesChangedOrRemovedObserver::notifyPreferencesChanged(WebCacheModel cacheModel)
{
    HRESULT hr = S_OK;

    if (!WebView::didSetCacheModel() || cacheModel > WebView::cacheModel())
        WebView::setCacheModel(cacheModel);
    else if (cacheModel < WebView::cacheModel()) {
        WebCacheModel sharedPreferencesCacheModel;
        hr = WebPreferences::sharedStandardPreferences()->cacheModel(&sharedPreferencesCacheModel);
        if (FAILED(hr))
            return hr;
        WebView::setCacheModel(max(sharedPreferencesCacheModel, WebView::maxCacheModelInAnyInstance()));
    }

    return hr;
}

HRESULT PreferencesChangedOrRemovedObserver::notifyPreferencesRemoved(WebCacheModel cacheModel)
{
    HRESULT hr = S_OK;

    if (cacheModel == WebView::cacheModel()) {
        WebCacheModel sharedPreferencesCacheModel;
        hr = WebPreferences::sharedStandardPreferences()->cacheModel(&sharedPreferencesCacheModel);
        if (FAILED(hr))
            return hr;
        WebView::setCacheModel(max(sharedPreferencesCacheModel, WebView::maxCacheModelInAnyInstance()));
    }

    return hr;
}


const LPCWSTR kWebViewWindowClassName = L"WebViewWindowClass";

const int WM_XP_THEMECHANGED = 0x031A;
const int WM_VISTA_MOUSEHWHEEL = 0x020E;

static const int maxToolTipWidth = 250;

static const int delayBeforeDeletingBackingStoreMsec = 5000;

static ATOM registerWebView();

static void initializeStaticObservers();

static HRESULT updateSharedSettingsFromPreferencesIfNeeded(IWebPreferences*);

HRESULT createMatchEnumerator(Vector<IntRect>* rects, IEnumTextMatches** matches);

static bool continuousSpellCheckingEnabled;
static bool grammarCheckingEnabled;

static bool s_didSetCacheModel;
static WebCacheModel s_cacheModel = WebCacheModelDocumentViewer;

enum {
    UpdateActiveStateTimer = 1,
    DeleteBackingStoreTimer = 2,
};

// WebView ----------------------------------------------------------------

bool WebView::s_allowSiteSpecificHacks = false;

WebView::WebView()
    : m_refCount(0)
#if !ASSERT_DISABLED
    , m_deletionHasBegun(false)
#endif
    , m_hostWindow(0)
    , m_viewWindow(0)
    , m_mainFrame(0)
    , m_page(0)
    , m_hasCustomDropTarget(false)
    , m_useBackForwardList(true)
    , m_userAgentOverridden(false)
    , m_zoomMultiplier(1.0f)
    , m_zoomsTextOnly(false)
    , m_mouseActivated(false)
    , m_dragData(0)
    , m_currentCharacterCode(0)
    , m_isBeingDestroyed(false)
    , m_paintCount(0)
    , m_hasSpellCheckerDocumentTag(false)
    , m_smartInsertDeleteEnabled(false)
    , m_didClose(false)
    , m_inIMEComposition(0)
    , m_toolTipHwnd(0)
    , m_closeWindowTimer(0)
    , m_topLevelParent(0)
    , m_deleteBackingStoreTimerActive(false)
    , m_transparent(false)
    , m_selectTrailingWhitespaceEnabled(false)
    , m_lastPanX(0)
    , m_lastPanY(0)
    , m_xOverpan(0)
    , m_yOverpan(0)
#if USE(ACCELERATED_COMPOSITING)
    , m_isAcceleratedCompositing(false)
#endif
    , m_nextDisplayIsSynchronous(false)
    , m_lastSetCursor(0)
{
    JSC::initializeThreading();
    WTF::initializeMainThread();

    m_backingStoreSize.cx = m_backingStoreSize.cy = 0;

    CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper,(void**)&m_dropTargetHelper);

    initializeStaticObservers();

    WebPreferences* sharedPreferences = WebPreferences::sharedStandardPreferences();
    BOOL enabled;
    if (SUCCEEDED(sharedPreferences->continuousSpellCheckingEnabled(&enabled)))
        continuousSpellCheckingEnabled = !!enabled;
    if (SUCCEEDED(sharedPreferences->grammarCheckingEnabled(&enabled)))
        grammarCheckingEnabled = !!enabled;

    WebViewCount++;
    gClassCount++;
    gClassNameCount.add("WebView");
}

WebView::~WebView()
{
    deleteBackingStore();

    // the tooltip window needs to be explicitly destroyed since it isn't a WS_CHILD
    if (::IsWindow(m_toolTipHwnd))
        ::DestroyWindow(m_toolTipHwnd);

    ASSERT(!m_page);
    ASSERT(!m_preferences);
    ASSERT(!m_viewWindow);

    WebViewCount--;
    gClassCount--;
    gClassNameCount.remove("WebView");
}

WebView* WebView::createInstance()
{
    WebView* instance = new WebView();
    instance->AddRef();
    return instance;
}

void initializeStaticObservers()
{
    static bool initialized;
    if (initialized)
        return;
    initialized = true;

    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->addObserver(PreferencesChangedOrRemovedObserver::sharedInstance(), WebPreferences::webPreferencesChangedNotification(), 0);
    notifyCenter->addObserver(PreferencesChangedOrRemovedObserver::sharedInstance(), WebPreferences::webPreferencesRemovedNotification(), 0);
}

static HashSet<WebView*>& allWebViewsSet()
{
    static HashSet<WebView*> allWebViewsSet;
    return allWebViewsSet;
}

void WebView::addToAllWebViewsSet()
{
    allWebViewsSet().add(this);
}

void WebView::removeFromAllWebViewsSet()
{
    allWebViewsSet().remove(this);
}

void WebView::setCacheModel(WebCacheModel cacheModel)
{
#if USE(CFNETWORK)
    if (s_didSetCacheModel && cacheModel == s_cacheModel)
        return;

    RetainPtr<CFURLCacheRef> cfurlCache(AdoptCF, CFURLCacheCopySharedURLCache());
    RetainPtr<CFStringRef> cfurlCacheDirectory(AdoptCF, wkCopyFoundationCacheDirectory());
    if (!cfurlCacheDirectory)
        cfurlCacheDirectory.adoptCF(WebCore::localUserSpecificStorageDirectory().createCFString());

    // As a fudge factor, use 1000 instead of 1024, in case the reported byte 
    // count doesn't align exactly to a megabyte boundary.
    unsigned long long memSize = WebMemorySize() / 1024 / 1000;
    unsigned long long diskFreeSize = WebVolumeFreeSize(cfurlCacheDirectory.get()) / 1024 / 1000;

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    double deadDecodedDataDeletionInterval = 0;

    unsigned pageCacheCapacity = 0;

    CFIndex cfurlCacheMemoryCapacity = 0;
    CFIndex cfurlCacheDiskCapacity = 0;

    switch (cacheModel) {
    case WebCacheModelDocumentViewer: {
        // Page cache capacity (in pages)
        pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024; 

        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;

        // Foundation memory cache capacity (in bytes)
        cfurlCacheMemoryCapacity = 0;

        // Foundation disk cache capacity (in bytes)
        cfurlCacheDiskCapacity = CFURLCacheDiskCapacity(cfurlCache.get());

        break;
    }
    case WebCacheModelDocumentBrowser: {
        // Page cache capacity (in pages)
        if (memSize >= 1024)
            pageCacheCapacity = 3;
        else if (memSize >= 512)
            pageCacheCapacity = 2;
        else if (memSize >= 256)
            pageCacheCapacity = 1;
        else
            pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024; 

        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;

        // Foundation memory cache capacity (in bytes)
        if (memSize >= 2048)
            cfurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 1024)
            cfurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 512)
            cfurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            cfurlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            cfurlCacheDiskCapacity = 50 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            cfurlCacheDiskCapacity = 40 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            cfurlCacheDiskCapacity = 30 * 1024 * 1024;
        else
            cfurlCacheDiskCapacity = 20 * 1024 * 1024;

        break;
    }
    case WebCacheModelPrimaryWebBrowser: {
        // Page cache capacity (in pages)
        // (Research indicates that value / page drops substantially after 3 pages.)
        if (memSize >= 2048)
            pageCacheCapacity = 5;
        else if (memSize >= 1024)
            pageCacheCapacity = 4;
        else if (memSize >= 512)
            pageCacheCapacity = 3;
        else if (memSize >= 256)
            pageCacheCapacity = 2;
        else
            pageCacheCapacity = 1;

        // Object cache capacities (in bytes)
        // (Testing indicates that value / MB depends heavily on content and
        // browsing pattern. Even growth above 128MB can have substantial 
        // value / MB for some content / browsing patterns.)
        if (memSize >= 2048)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 32 * 1024 * 1024; 

        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;

        // This code is here to avoid a PLT regression. We can remove it if we
        // can prove that the overall system gain would justify the regression.
        cacheMaxDeadCapacity = max(24u, cacheMaxDeadCapacity);

        deadDecodedDataDeletionInterval = 60;

        // Foundation memory cache capacity (in bytes)
        // (These values are small because WebCore does most caching itself.)
        if (memSize >= 1024)
            cfurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 512)
            cfurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 256)
            cfurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            cfurlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            cfurlCacheDiskCapacity = 175 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            cfurlCacheDiskCapacity = 150 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            cfurlCacheDiskCapacity = 125 * 1024 * 1024;
        else if (diskFreeSize >= 2048)
            cfurlCacheDiskCapacity = 100 * 1024 * 1024;
        else if (diskFreeSize >= 1024)
            cfurlCacheDiskCapacity = 75 * 1024 * 1024;
        else
            cfurlCacheDiskCapacity = 50 * 1024 * 1024;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }

    // Don't shrink a big disk cache, since that would cause churn.
    cfurlCacheDiskCapacity = max(cfurlCacheDiskCapacity, CFURLCacheDiskCapacity(cfurlCache.get()));

    cache()->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    cache()->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    pageCache()->setCapacity(pageCacheCapacity);

    CFURLCacheSetMemoryCapacity(cfurlCache.get(), cfurlCacheMemoryCapacity);
    CFURLCacheSetDiskCapacity(cfurlCache.get(), cfurlCacheDiskCapacity);

    s_didSetCacheModel = true;
    s_cacheModel = cacheModel;
    return;
#endif
}

WebCacheModel WebView::cacheModel()
{
    return s_cacheModel;
}

bool WebView::didSetCacheModel()
{
    return s_didSetCacheModel;
}

WebCacheModel WebView::maxCacheModelInAnyInstance()
{
    WebCacheModel cacheModel = WebCacheModelDocumentViewer;

    HashSet<WebView*>::iterator end = allWebViewsSet().end();
    for (HashSet<WebView*>::iterator it = allWebViewsSet().begin(); it != end; ++it) {
        COMPtr<IWebPreferences> pref;
        if (FAILED((*it)->preferences(&pref)))
            continue;
        WebCacheModel prefCacheModel = WebCacheModelDocumentViewer;
        if (FAILED(pref->cacheModel(&prefCacheModel)))
            continue;

        cacheModel = max(cacheModel, prefCacheModel);
    }

    return cacheModel;
}

HRESULT STDMETHODCALLTYPE WebView::close()
{
    if (m_didClose)
        return S_OK;

    m_didClose = true;

#if USE(ACCELERATED_COMPOSITING)
    setAcceleratedCompositing(false);
#endif

    WebNotificationCenter::defaultCenterInternal()->postNotificationName(_bstr_t(WebViewWillCloseNotification).GetBSTR(), static_cast<IWebView*>(this), 0);

    if (m_uiDelegatePrivate)
        m_uiDelegatePrivate->webViewClosing(this);

    removeFromAllWebViewsSet();

    if (m_page) {
        if (Frame* frame = m_page->mainFrame())
            frame->loader()->detachFromParent();
    }

    if (m_mouseOutTracker) {
        m_mouseOutTracker->dwFlags = TME_CANCEL;
        ::TrackMouseEvent(m_mouseOutTracker.get());
        m_mouseOutTracker.set(0);
    }
    
    revokeDragDrop();

    if (m_viewWindow) {
        // We can't check IsWindow(m_viewWindow) here, because that will return true even while
        // we're already handling WM_DESTROY. So we check !isBeingDestroyed() instead.
        if (!isBeingDestroyed())
            DestroyWindow(m_viewWindow);
        // Either we just destroyed m_viewWindow, or it's in the process of being destroyed. Either
        // way, we clear it out to make sure we don't try to use it later.
        m_viewWindow = 0;
    }

    setHostWindow(0);

    setDownloadDelegate(0);
    setEditingDelegate(0);
    setFrameLoadDelegate(0);
    setFrameLoadDelegatePrivate(0);
    setHistoryDelegate(0);
    setPolicyDelegate(0);
    setResourceLoadDelegate(0);
    setUIDelegate(0);
    setFormDelegate(0);
    setPluginHalterDelegate(0);

    if (m_webInspector)
        m_webInspector->webViewClosed();

    delete m_page;
    m_page = 0;

    registerForIconNotification(false);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->removeObserver(this, WebPreferences::webPreferencesChangedNotification(), static_cast<IWebPreferences*>(m_preferences.get()));

    if (COMPtr<WebPreferences> preferences = m_preferences) {
        BSTR identifier = 0;
        preferences->identifier(&identifier);

        m_preferences = 0;
        preferences->didRemoveFromWebView();
        // Make sure we release the reference, since WebPreferences::removeReferenceForIdentifier will check for last reference to WebPreferences
        preferences = 0;
        if (identifier) {
            WebPreferences::removeReferenceForIdentifier(identifier);
            SysFreeString(identifier);
        }
    }

    deleteBackingStore();
    return S_OK;
}

void WebView::repaint(const WebCore::IntRect& windowRect, bool contentChanged, bool immediate, bool repaintContentOnly)
{
#if USE(ACCELERATED_COMPOSITING)
    if (isAcceleratedCompositing())
        setRootLayerNeedsDisplay();
#endif

    if (!repaintContentOnly) {
        RECT rect = windowRect;
        ::InvalidateRect(m_viewWindow, &rect, false);
    }
    if (contentChanged)
        addToDirtyRegion(windowRect);
    if (immediate) {
        if (repaintContentOnly)
            updateBackingStore(core(topLevelFrame())->view());
        else
            ::UpdateWindow(m_viewWindow);
    }
}

void WebView::deleteBackingStore()
{
    pendingDeleteBackingStoreSet.remove(this);

    if (m_deleteBackingStoreTimerActive) {
        KillTimer(m_viewWindow, DeleteBackingStoreTimer);
        m_deleteBackingStoreTimerActive = false;
    }
    m_backingStoreBitmap.clear();
    m_backingStoreDirtyRegion.clear();
#if USE(ACCELERATED_COMPOSITING)
    if (m_layerRenderer)
        m_layerRenderer->setBackingStoreDirty(false);
#endif

    m_backingStoreSize.cx = m_backingStoreSize.cy = 0;
}

bool WebView::ensureBackingStore()
{
    RECT windowRect;
    ::GetClientRect(m_viewWindow, &windowRect);
    LONG width = windowRect.right - windowRect.left;
    LONG height = windowRect.bottom - windowRect.top;
    if (width > 0 && height > 0 && (width != m_backingStoreSize.cx || height != m_backingStoreSize.cy)) {
        deleteBackingStore();

        m_backingStoreSize.cx = width;
        m_backingStoreSize.cy = height;
        BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(IntSize(m_backingStoreSize));

        void* pixels = NULL;
        m_backingStoreBitmap = RefCountedHBITMAP::create(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0));
        return true;
    }

    return false;
}

void WebView::addToDirtyRegion(const IntRect& dirtyRect)
{
    // FIXME: We want an assert here saying that the dirtyRect is inside the clienRect,
    // but it was being hit during our layout tests, and is being investigated in
    // http://webkit.org/b/29350.

    HRGN newRegion = ::CreateRectRgn(dirtyRect.x(), dirtyRect.y(),
                                     dirtyRect.right(), dirtyRect.bottom());
    addToDirtyRegion(newRegion);
}

void WebView::addToDirtyRegion(HRGN newRegion)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);

    if (m_backingStoreDirtyRegion) {
        HRGN combinedRegion = ::CreateRectRgn(0,0,0,0);
        ::CombineRgn(combinedRegion, m_backingStoreDirtyRegion->handle(), newRegion, RGN_OR);
        ::DeleteObject(newRegion);
        m_backingStoreDirtyRegion = RefCountedHRGN::create(combinedRegion);
    } else
        m_backingStoreDirtyRegion = RefCountedHRGN::create(newRegion);

#if USE(ACCELERATED_COMPOSITING)
    if (m_layerRenderer)
        m_layerRenderer->setBackingStoreDirty(true);
#endif

    if (m_uiDelegatePrivate)
        m_uiDelegatePrivate->webViewDidInvalidate(this);
}

void WebView::scrollBackingStore(FrameView* frameView, int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);

    // If there's no backing store we don't need to update it
    if (!m_backingStoreBitmap) {
        if (m_uiDelegatePrivate)
            m_uiDelegatePrivate->webViewScrolled(this);

        return;
    }

    // Make a region to hold the invalidated scroll area.
    HRGN updateRegion = ::CreateRectRgn(0, 0, 0, 0);

    // Collect our device context info and select the bitmap to scroll.
    HDC windowDC = ::GetDC(m_viewWindow);
    HDC bitmapDC = ::CreateCompatibleDC(windowDC);
    HGDIOBJ oldBitmap = ::SelectObject(bitmapDC, m_backingStoreBitmap->handle());
    
    // Scroll the bitmap.
    RECT scrollRectWin(scrollViewRect);
    RECT clipRectWin(clipRect);
    ::ScrollDC(bitmapDC, dx, dy, &scrollRectWin, &clipRectWin, updateRegion, 0);
    RECT regionBox;
    ::GetRgnBox(updateRegion, &regionBox);

    // Flush.
    GdiFlush();

    // Add the dirty region to the backing store's dirty region.
    addToDirtyRegion(updateRegion);

    if (m_uiDelegatePrivate)
        m_uiDelegatePrivate->webViewScrolled(this);

    // Update the backing store.
    updateBackingStore(frameView, bitmapDC, false);

    // Clean up.
    ::SelectObject(bitmapDC, oldBitmap);
    ::DeleteDC(bitmapDC);
    ::ReleaseDC(m_viewWindow, windowDC);
}

void WebView::sizeChanged(const IntSize& newSize)
{
    deleteBackingStore();

    if (Frame* coreFrame = core(topLevelFrame()))
        coreFrame->view()->resize(newSize);

#if USE(ACCELERATED_COMPOSITING)
    if (m_layerRenderer)
        m_layerRenderer->resize();
#endif
}

// This emulates the Mac smarts for painting rects intelligently.  This is very
// important for us, since we double buffer based off dirty rects.
static void getUpdateRects(HRGN region, const IntRect& dirtyRect, Vector<IntRect>& rects)
{
    ASSERT_ARG(region, region);

    const int cRectThreshold = 10;
    const float cWastedSpaceThreshold = 0.75f;

    rects.clear();

    DWORD regionDataSize = GetRegionData(region, sizeof(RGNDATA), NULL);
    if (!regionDataSize) {
        rects.append(dirtyRect);
        return;
    }

    Vector<unsigned char> buffer(regionDataSize);
    RGNDATA* regionData = reinterpret_cast<RGNDATA*>(buffer.data());
    GetRegionData(region, regionDataSize, regionData);
    if (regionData->rdh.nCount > cRectThreshold) {
        rects.append(dirtyRect);
        return;
    }

    double singlePixels = 0.0;
    unsigned i;
    RECT* rect;
    for (i = 0, rect = reinterpret_cast<RECT*>(regionData->Buffer); i < regionData->rdh.nCount; i++, rect++)
        singlePixels += (rect->right - rect->left) * (rect->bottom - rect->top);

    double unionPixels = dirtyRect.width() * dirtyRect.height();
    double wastedSpace = 1.0 - (singlePixels / unionPixels);
    if (wastedSpace <= cWastedSpaceThreshold) {
        rects.append(dirtyRect);
        return;
    }

    for (i = 0, rect = reinterpret_cast<RECT*>(regionData->Buffer); i < regionData->rdh.nCount; i++, rect++)
        rects.append(*rect);
}

void WebView::updateBackingStore(FrameView* frameView, HDC dc, bool backingStoreCompletelyDirty, WindowsToPaint windowsToPaint)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);

    HDC windowDC = 0;
    HDC bitmapDC = dc;
    if (!dc) {
        windowDC = ::GetDC(m_viewWindow);
        bitmapDC = ::CreateCompatibleDC(windowDC);
        ::SelectObject(bitmapDC, m_backingStoreBitmap->handle());
    }

    if (m_backingStoreBitmap && (m_backingStoreDirtyRegion || backingStoreCompletelyDirty)) {
        // Do a layout first so that everything we render to the backing store is always current.
        if (Frame* coreFrame = core(m_mainFrame))
            if (FrameView* view = coreFrame->view())
                view->updateLayoutAndStyleIfNeededRecursive();

        Vector<IntRect> paintRects;
        if (!backingStoreCompletelyDirty && m_backingStoreDirtyRegion) {
            RECT regionBox;
            ::GetRgnBox(m_backingStoreDirtyRegion->handle(), &regionBox);
            getUpdateRects(m_backingStoreDirtyRegion->handle(), regionBox, paintRects);
        } else {
            RECT clientRect;
            ::GetClientRect(m_viewWindow, &clientRect);
            paintRects.append(clientRect);
        }

        for (unsigned i = 0; i < paintRects.size(); ++i)
            paintIntoBackingStore(frameView, bitmapDC, paintRects[i], windowsToPaint);

        if (m_uiDelegatePrivate)
            m_uiDelegatePrivate->webViewPainted(this);

        m_backingStoreDirtyRegion.clear();
#if USE(ACCELERATED_COMPOSITING)
        if (m_layerRenderer)
            m_layerRenderer->setBackingStoreDirty(false);
#endif
    }

    if (!dc) {
        ::DeleteDC(bitmapDC);
        ::ReleaseDC(m_viewWindow, windowDC);
    }

    GdiFlush();
}

void WebView::paint(HDC dc, LPARAM options)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);

    Frame* coreFrame = core(m_mainFrame);
    if (!coreFrame)
        return;
    FrameView* frameView = coreFrame->view();

    RECT rcPaint;
    HDC hdc;
    OwnPtr<HRGN> region;
    int regionType = NULLREGION;
    PAINTSTRUCT ps;
    WindowsToPaint windowsToPaint;
    if (!dc) {
        region.set(CreateRectRgn(0,0,0,0));
        regionType = GetUpdateRgn(m_viewWindow, region.get(), false);
        hdc = BeginPaint(m_viewWindow, &ps);
        rcPaint = ps.rcPaint;
        // We're painting to the screen, and our child windows can handle
        // painting themselves to the screen.
        windowsToPaint = PaintWebViewOnly;
    } else {
        hdc = dc;
        ::GetClientRect(m_viewWindow, &rcPaint);
        if (options & PRF_ERASEBKGND)
            ::FillRect(hdc, &rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));
        // Since we aren't painting to the screen, we want to paint all our
        // children into the HDC.
        windowsToPaint = PaintWebViewAndChildren;
    }

    bool backingStoreCompletelyDirty = ensureBackingStore();
    if (!m_backingStoreBitmap) {
        if (!dc)
            EndPaint(m_viewWindow, &ps);
        return;
    }

    m_paintCount++;

    HDC bitmapDC = ::CreateCompatibleDC(hdc);
    HGDIOBJ oldBitmap = ::SelectObject(bitmapDC, m_backingStoreBitmap->handle());

    // Update our backing store if needed.
    updateBackingStore(frameView, bitmapDC, backingStoreCompletelyDirty, windowsToPaint);

#if USE(ACCELERATED_COMPOSITING)
    if (!isAcceleratedCompositing()) {
#endif
        // Now we blit the updated backing store
        IntRect windowDirtyRect = rcPaint;
        
        // Apply the same heuristic for this update region too.
        Vector<IntRect> blitRects;
        if (region && regionType == COMPLEXREGION)
            getUpdateRects(region.get(), windowDirtyRect, blitRects);
        else
            blitRects.append(windowDirtyRect);

        for (unsigned i = 0; i < blitRects.size(); ++i)
            paintIntoWindow(bitmapDC, hdc, blitRects[i]);
#if USE(ACCELERATED_COMPOSITING)
    } else
        updateRootLayerContents();
#endif

    ::SelectObject(bitmapDC, oldBitmap);
    ::DeleteDC(bitmapDC);

    if (!dc)
        EndPaint(m_viewWindow, &ps);

    m_paintCount--;

    if (active())
        cancelDeleteBackingStoreSoon();
    else
        deleteBackingStoreSoon();
}

void WebView::paintIntoBackingStore(FrameView* frameView, HDC bitmapDC, const IntRect& dirtyRect, WindowsToPaint windowsToPaint)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);

    // FIXME: We want an assert here saying that the dirtyRect is inside the clienRect,
    // but it was being hit during our layout tests, and is being investigated in
    // http://webkit.org/b/29350.

    RECT rect = dirtyRect;

#if FLASH_BACKING_STORE_REDRAW
    HDC dc = ::GetDC(m_viewWindow);
    OwnPtr<HBRUSH> yellowBrush(CreateSolidBrush(RGB(255, 255, 0)));
    FillRect(dc, &rect, yellowBrush.get());
    GdiFlush();
    Sleep(50);
    paintIntoWindow(bitmapDC, dc, dirtyRect);
    ::ReleaseDC(m_viewWindow, dc);
#endif

    GraphicsContext gc(bitmapDC, m_transparent);
    gc.setShouldIncludeChildWindows(windowsToPaint == PaintWebViewAndChildren);
    gc.save();
    if (m_transparent)
        gc.clearRect(dirtyRect);
    else
        FillRect(bitmapDC, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

    COMPtr<IWebUIDelegatePrivate2> uiPrivate(Query, m_uiDelegate);
    if (uiPrivate)
        uiPrivate->drawBackground(this, reinterpret_cast<OLE_HANDLE>(bitmapDC), &rect);

    if (frameView && frameView->frame() && frameView->frame()->contentRenderer()) {
        gc.clip(dirtyRect);
        frameView->paint(&gc, dirtyRect);
    }
    gc.restore();
}

void WebView::paintIntoWindow(HDC bitmapDC, HDC windowDC, const IntRect& dirtyRect)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);
#if FLASH_WINDOW_REDRAW
    OwnPtr<HBRUSH> greenBrush = CreateSolidBrush(RGB(0, 255, 0));
    RECT rect = dirtyRect;
    FillRect(windowDC, &rect, greenBrush.get());
    GdiFlush();
    Sleep(50);
#endif

    // Blit the dirty rect from the backing store into the same position
    // in the destination DC.
    BitBlt(windowDC, dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height(), bitmapDC,
           dirtyRect.x(), dirtyRect.y(), SRCCOPY);
}

void WebView::frameRect(RECT* rect)
{
    ::GetWindowRect(m_viewWindow, rect);
}

class WindowCloseTimer : public WebCore::SuspendableTimer {
public:
    static WindowCloseTimer* create(WebView*);

private:
    WindowCloseTimer(ScriptExecutionContext*, WebView*);
    virtual void contextDestroyed();
    virtual void fired();

    WebView* m_webView;
};

WindowCloseTimer* WindowCloseTimer::create(WebView* webView)
{
    ASSERT_ARG(webView, webView);
    Frame* frame = core(webView->topLevelFrame());
    ASSERT(frame);
    if (!frame)
        return 0;

    Document* document = frame->document();
    ASSERT(document);
    if (!document)
        return 0;

    return new WindowCloseTimer(document, webView);
}

WindowCloseTimer::WindowCloseTimer(ScriptExecutionContext* context, WebView* webView)
    : SuspendableTimer(context)
    , m_webView(webView)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(webView, webView);
}

void WindowCloseTimer::contextDestroyed()
{
    SuspendableTimer::contextDestroyed();
    delete this;
}

void WindowCloseTimer::fired()
{
    m_webView->closeWindowTimerFired();
}

void WebView::closeWindowSoon()
{
    if (m_closeWindowTimer)
        return;

    m_closeWindowTimer = WindowCloseTimer::create(this);
    if (!m_closeWindowTimer)
        return;
    m_closeWindowTimer->startOneShot(0);

    AddRef();
}

void WebView::closeWindowTimerFired()
{
    closeWindow();
    Release();
}

void WebView::closeWindow()
{
    if (m_hasSpellCheckerDocumentTag) {
        if (m_editingDelegate)
            m_editingDelegate->closeSpellDocument(this);
        m_hasSpellCheckerDocumentTag = false;
    }

    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(uiDelegate(&ui)))
        ui->webViewClose(this);
}

bool WebView::canHandleRequest(const WebCore::ResourceRequest& request)
{
    // On the mac there's an about url protocol implementation but CFNetwork doesn't have that.
    if (equalIgnoringCase(String(request.url().protocol()), "about"))
        return true;

#if USE(CFNETWORK)
    if (CFURLProtocolCanHandleRequest(request.cfURLRequest()))
        return true;

    // FIXME: Mac WebKit calls _representationExistsForURLScheme here
    return false;
#else
    return true;
#endif
}

String WebView::standardUserAgentWithApplicationName(const String& applicationName)
{
    return String::format("Mozilla/5.0 (Windows; U; %s; %s) AppleWebKit/%s (KHTML, like Gecko)%s%s", osVersion().latin1().data(), defaultLanguage().latin1().data(), webKitVersion().latin1().data(), (applicationName.length() ? " " : ""), applicationName.latin1().data());
}

Page* WebView::page()
{
    return m_page;
}

bool WebView::handleContextMenuEvent(WPARAM wParam, LPARAM lParam)
{
    // Translate the screen coordinates into window coordinates
    POINT coords = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (coords.x == -1 || coords.y == -1) {
        // The contextMenuController() holds onto the last context menu that was popped up on the
        // page until a new one is created. We need to clear this menu before propagating the event
        // through the DOM so that we can detect if we create a new menu for this event, since we
        // won't create a new menu if the DOM swallows the event and the defaultEventHandler does
        // not run.
        m_page->contextMenuController()->clearContextMenu();

        Frame* focusedFrame = m_page->focusController()->focusedOrMainFrame();
        return focusedFrame->eventHandler()->sendContextMenuEventForKey();

    } else {
        if (!::ScreenToClient(m_viewWindow, &coords))
            return false;
    }

    lParam = MAKELPARAM(coords.x, coords.y);

    m_page->contextMenuController()->clearContextMenu();

    IntPoint documentPoint(m_page->mainFrame()->view()->windowToContents(coords));
    HitTestResult result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(documentPoint, false);
    Frame* targetFrame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document()->frame() : m_page->focusController()->focusedOrMainFrame();

    targetFrame->view()->setCursor(pointerCursor());
    PlatformMouseEvent mouseEvent(m_viewWindow, WM_RBUTTONUP, wParam, lParam);
    bool handledEvent = targetFrame->eventHandler()->sendContextMenuEvent(mouseEvent);
    if (!handledEvent)
        return false;

    // Show the menu
    ContextMenu* coreMenu = m_page->contextMenuController()->contextMenu();
    if (!coreMenu)
        return false;

    Node* node = coreMenu->hitTestResult().innerNonSharedNode();
    if (!node)
        return false;

    Frame* frame = node->document()->frame();
    if (!frame)
        return false;

    FrameView* view = frame->view();
    if (!view)
        return false;

    POINT point(view->contentsToWindow(coreMenu->hitTestResult().point()));

    // Translate the point to screen coordinates
    if (!::ClientToScreen(m_viewWindow, &point))
        return false;

    BOOL hasCustomMenus = false;
    if (m_uiDelegate)
        m_uiDelegate->hasCustomMenuImplementation(&hasCustomMenus);

    if (hasCustomMenus)
        m_uiDelegate->trackCustomPopupMenu((IWebView*)this, (OLE_HANDLE)(ULONG64)coreMenu->platformDescription(), &point);
    else {
        // Surprisingly, TPM_RIGHTBUTTON means that items are selectable with either the right OR left mouse button
        UINT flags = TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_VERPOSANIMATION | TPM_HORIZONTAL
            | TPM_LEFTALIGN | TPM_HORPOSANIMATION;
        ::TrackPopupMenuEx(coreMenu->platformDescription(), flags, point.x, point.y, m_viewWindow, 0);
    }

    return true;
}

bool WebView::onMeasureItem(WPARAM /*wParam*/, LPARAM lParam)
{
    if (!m_uiDelegate)
        return false;

    BOOL hasCustomMenus = false;
    m_uiDelegate->hasCustomMenuImplementation(&hasCustomMenus);
    if (!hasCustomMenus)
        return false;

    m_uiDelegate->measureCustomMenuItem((IWebView*)this, (void*)lParam);
    return true;
}

bool WebView::onDrawItem(WPARAM /*wParam*/, LPARAM lParam)
{
    if (!m_uiDelegate)
        return false;

    BOOL hasCustomMenus = false;
    m_uiDelegate->hasCustomMenuImplementation(&hasCustomMenus);
    if (!hasCustomMenus)
        return false;

    m_uiDelegate->drawCustomMenuItem((IWebView*)this, (void*)lParam);
    return true;
}

bool WebView::onInitMenuPopup(WPARAM wParam, LPARAM /*lParam*/)
{
    if (!m_uiDelegate)
        return false;

    HMENU menu = (HMENU)wParam;
    if (!menu)
        return false;

    BOOL hasCustomMenus = false;
    m_uiDelegate->hasCustomMenuImplementation(&hasCustomMenus);
    if (!hasCustomMenus)
        return false;

    m_uiDelegate->addCustomMenuDrawingData((IWebView*)this, (OLE_HANDLE)(ULONG64)menu);
    return true;
}

bool WebView::onUninitMenuPopup(WPARAM wParam, LPARAM /*lParam*/)
{
    if (!m_uiDelegate)
        return false;

    HMENU menu = (HMENU)wParam;
    if (!menu)
        return false;

    BOOL hasCustomMenus = false;
    m_uiDelegate->hasCustomMenuImplementation(&hasCustomMenus);
    if (!hasCustomMenus)
        return false;

    m_uiDelegate->cleanUpCustomMenuDrawingData((IWebView*)this, (OLE_HANDLE)(ULONG64)menu);
    return true;
}

void WebView::performContextMenuAction(WPARAM wParam, LPARAM lParam, bool byPosition)
{
    ContextMenu* menu = m_page->contextMenuController()->contextMenu();
    ASSERT(menu);

    ContextMenuItem* item = byPosition ? menu->itemAtIndex((unsigned)wParam, (HMENU)lParam) : menu->itemWithAction((ContextMenuAction)wParam);
    if (!item)
        return;
    m_page->contextMenuController()->contextMenuItemSelected(item);
    delete item;
}

bool WebView::handleMouseEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    static LONG globalClickCount;
    static IntPoint globalPrevPoint;
    static MouseButton globalPrevButton;
    static LONG globalPrevMouseDownTime;

    if (message == WM_CANCELMODE) {
        m_page->mainFrame()->eventHandler()->lostMouseCapture();
        return true;
    }

    // Create our event.
    // On WM_MOUSELEAVE we need to create a mouseout event, so we force the position
    // of the event to be at (MINSHORT, MINSHORT).
    LPARAM position = (message == WM_MOUSELEAVE) ? ((MINSHORT << 16) | MINSHORT) : lParam;
    PlatformMouseEvent mouseEvent(m_viewWindow, message, wParam, position, m_mouseActivated);

    setMouseActivated(false);

    bool insideThreshold = abs(globalPrevPoint.x() - mouseEvent.pos().x()) < ::GetSystemMetrics(SM_CXDOUBLECLK) &&
                           abs(globalPrevPoint.y() - mouseEvent.pos().y()) < ::GetSystemMetrics(SM_CYDOUBLECLK);
    LONG messageTime = ::GetMessageTime();

    bool handled = false;

    if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN) {
        // FIXME: I'm not sure if this is the "right" way to do this
        // but without this call, we never become focused since we don't allow
        // the default handling of mouse events.
        SetFocus(m_viewWindow);

        // Always start capturing events when the mouse goes down in our HWND.
        ::SetCapture(m_viewWindow);

        if (((messageTime - globalPrevMouseDownTime) < (LONG)::GetDoubleClickTime()) && 
            insideThreshold &&
            mouseEvent.button() == globalPrevButton)
            globalClickCount++;
        else
            // Reset the click count.
            globalClickCount = 1;
        globalPrevMouseDownTime = messageTime;
        globalPrevButton = mouseEvent.button();
        globalPrevPoint = mouseEvent.pos();
        
        mouseEvent.setClickCount(globalClickCount);
        handled = m_page->mainFrame()->eventHandler()->handleMousePressEvent(mouseEvent);
    } else if (message == WM_LBUTTONDBLCLK || message == WM_MBUTTONDBLCLK || message == WM_RBUTTONDBLCLK) {
        globalClickCount++;
        mouseEvent.setClickCount(globalClickCount);
        handled = m_page->mainFrame()->eventHandler()->handleMousePressEvent(mouseEvent);
    } else if (message == WM_LBUTTONUP || message == WM_MBUTTONUP || message == WM_RBUTTONUP) {
        // Record the global position and the button of the up.
        globalPrevButton = mouseEvent.button();
        globalPrevPoint = mouseEvent.pos();
        mouseEvent.setClickCount(globalClickCount);
        m_page->mainFrame()->eventHandler()->handleMouseReleaseEvent(mouseEvent);
        ::ReleaseCapture();
    } else if (message == WM_MOUSELEAVE && m_mouseOutTracker) {
        // Once WM_MOUSELEAVE is fired windows clears this tracker
        // so there is no need to disable it ourselves.
        m_mouseOutTracker.set(0);
        m_page->mainFrame()->eventHandler()->mouseMoved(mouseEvent);
        handled = true;
    } else if (message == WM_MOUSEMOVE) {
        if (!insideThreshold)
            globalClickCount = 0;
        mouseEvent.setClickCount(globalClickCount);
        handled = m_page->mainFrame()->eventHandler()->mouseMoved(mouseEvent);
        if (!m_mouseOutTracker) {
            m_mouseOutTracker.set(new TRACKMOUSEEVENT);
            m_mouseOutTracker->cbSize = sizeof(TRACKMOUSEEVENT);
            m_mouseOutTracker->dwFlags = TME_LEAVE;
            m_mouseOutTracker->hwndTrack = m_viewWindow;
            ::TrackMouseEvent(m_mouseOutTracker.get());
        }
    }
    return handled;
}

bool WebView::gestureNotify(WPARAM wParam, LPARAM lParam)
{
    GESTURENOTIFYSTRUCT* gn = reinterpret_cast<GESTURENOTIFYSTRUCT*>(lParam);

    Frame* coreFrame = core(m_mainFrame);
    if (!coreFrame)
        return false;

    ScrollView* view = coreFrame->view();
    if (!view)
        return false;

    // If we don't have this function, we shouldn't be receiving this message
    ASSERT(SetGestureConfigPtr());

    bool hitScrollbar = false;
    POINT gestureBeginPoint = {gn->ptsLocation.x, gn->ptsLocation.y};
    HitTestRequest request(HitTestRequest::ReadOnly);
    for (Frame* childFrame = m_page->mainFrame(); childFrame; childFrame = EventHandler::subframeForTargetNode(m_gestureTargetNode.get())) {
        FrameView* frameView = childFrame->view();
        if (!frameView)
            break;
        RenderView* renderView = childFrame->document()->renderView();
        if (!renderView)
            break;
        RenderLayer* layer = renderView->layer();
        if (!layer)
            break;

        HitTestResult result(frameView->screenToContents(gestureBeginPoint));
        layer->hitTest(request, result);
        m_gestureTargetNode = result.innerNode();

        if (!hitScrollbar)
            hitScrollbar = result.scrollbar();
    }

    if (!hitScrollbar) {
        // The hit testing above won't detect if we've hit the main frame's vertical scrollbar. Check that manually now.
        RECT webViewRect;
        GetWindowRect(m_viewWindow, &webViewRect);
        hitScrollbar = view->verticalScrollbar() && (gestureBeginPoint.x > (webViewRect.right - view->verticalScrollbar()->theme()->scrollbarThickness()));  
    }

    bool canBeScrolled = false;
    if (m_gestureTargetNode) {
        for (RenderObject* renderer = m_gestureTargetNode->renderer(); renderer; renderer = renderer->parent()) {
            if (renderer->isBox() && toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()) {
                canBeScrolled = true;
                break;
            }
        }
    }

    // We always allow two-fingered panning with inertia and a gutter (which limits movement to one
    // direction in most cases).
    DWORD dwPanWant = GC_PAN | GC_PAN_WITH_INERTIA | GC_PAN_WITH_GUTTER;
    // We never allow single-fingered horizontal panning. That gesture is reserved for creating text
    // selections. This matches IE.
    DWORD dwPanBlock = GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;

    if (hitScrollbar || !canBeScrolled) {
        // The part of the page under the gesture can't be scrolled, or the gesture is on a scrollbar.
        // Disallow single-fingered vertical panning in this case, too, so we'll fall back to the default
        // behavior (which allows the scrollbar thumb to be dragged, text selections to be made, etc.).
        dwPanBlock |= GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
    } else {
        // The part of the page the gesture is under can be scrolled, and we're not under a scrollbar.
        // Allow single-fingered vertical panning in this case, so the user will be able to pan the page
        // with one or two fingers.
        dwPanWant |= GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
    }

    GESTURECONFIG gc = { GID_PAN, dwPanWant, dwPanBlock };
    return SetGestureConfigPtr()(m_viewWindow, 0, 1, &gc, sizeof(GESTURECONFIG));
}

bool WebView::gesture(WPARAM wParam, LPARAM lParam) 
{
    // We want to bail out if we don't have either of these functions
    if (!GetGestureInfoPtr() || !CloseGestureInfoHandlePtr())
        return false;

    HGESTUREINFO gestureHandle = reinterpret_cast<HGESTUREINFO>(lParam);
    
    GESTUREINFO gi = {0};
    gi.cbSize = sizeof(GESTUREINFO);

    if (!GetGestureInfoPtr()(gestureHandle, reinterpret_cast<PGESTUREINFO>(&gi)))
        return false;

    switch (gi.dwID) {
    case GID_BEGIN:
        m_lastPanX = gi.ptsLocation.x;
        m_lastPanY = gi.ptsLocation.y;

        break;
    case GID_END:
        m_gestureTargetNode = 0;
        break;
    case GID_PAN: {
        // Where are the fingers currently?
        long currentX = gi.ptsLocation.x;
        long currentY = gi.ptsLocation.y;
        // How far did we pan in each direction?
        long deltaX = currentX - m_lastPanX;
        long deltaY = currentY - m_lastPanY;
        // Calculate the overpan for window bounce
        m_yOverpan -= m_lastPanY - currentY;
        m_xOverpan -= m_lastPanX - currentX;
        // Update our class variables with updated values
        m_lastPanX = currentX;
        m_lastPanY = currentY;

        Frame* coreFrame = core(m_mainFrame);
        if (!coreFrame) {
            CloseGestureInfoHandlePtr()(gestureHandle);
            return false;
        }

        if (!m_gestureTargetNode || !m_gestureTargetNode->renderer())
            return false;

        // We negate here since panning up moves the content up, but moves the scrollbar down.
        m_gestureTargetNode->renderer()->enclosingLayer()->scrollByRecursively(-deltaX, -deltaY);
           
        if (!(UpdatePanningFeedbackPtr() && BeginPanningFeedbackPtr() && EndPanningFeedbackPtr())) {
            CloseGestureInfoHandlePtr()(gestureHandle);
            return true;
        }

        if (gi.dwFlags & GF_BEGIN) {
            BeginPanningFeedbackPtr()(m_viewWindow);
            m_yOverpan = 0;
        } else if (gi.dwFlags & GF_END) {
            EndPanningFeedbackPtr()(m_viewWindow, true);
            m_yOverpan = 0;
        }

        ScrollView* view = coreFrame->view();
        if (!view) {
            CloseGestureInfoHandlePtr()(gestureHandle);
            return true;
        }
        Scrollbar* vertScrollbar = view->verticalScrollbar();
        if (!vertScrollbar) {
            CloseGestureInfoHandlePtr()(gestureHandle);
            return true;
        }

        // FIXME: Support Horizontal Window Bounce. <https://webkit.org/b/28500>.
        // FIXME: If the user starts panning down after a window bounce has started, the window doesn't bounce back 
        // until they release their finger. <https://webkit.org/b/28501>.
        if (vertScrollbar->currentPos() == 0)
            UpdatePanningFeedbackPtr()(m_viewWindow, 0, m_yOverpan, gi.dwFlags & GF_INERTIA);
        else if (vertScrollbar->currentPos() >= vertScrollbar->maximum())
            UpdatePanningFeedbackPtr()(m_viewWindow, 0, m_yOverpan, gi.dwFlags & GF_INERTIA);

        CloseGestureInfoHandlePtr()(gestureHandle);
        return true;
    }
    default:
        break;
    }

    // If we get to this point, the gesture has not been handled. We forward
    // the call to DefWindowProc by returning false, and we don't need to 
    // to call CloseGestureInfoHandle. 
    // http://msdn.microsoft.com/en-us/library/dd353228(VS.85).aspx
    return false;
}

bool WebView::mouseWheel(WPARAM wParam, LPARAM lParam, bool isMouseHWheel)
{
    // Ctrl+Mouse wheel doesn't ever go into WebCore.  It is used to
    // zoom instead (Mac zooms the whole Desktop, but Windows browsers trigger their
    // own local zoom modes for Ctrl+wheel).
    if (wParam & MK_CONTROL) {
        short delta = short(HIWORD(wParam));
        if (delta < 0)
            makeTextSmaller(0);
        else
            makeTextLarger(0);
        return true;
    }
    
    // FIXME: This doesn't fix https://bugs.webkit.org/show_bug.cgi?id=28217. This only fixes https://bugs.webkit.org/show_bug.cgi?id=28203.
    HWND focusedWindow = GetFocus();
    if (focusedWindow && focusedWindow != m_viewWindow) {
        // Our focus is on a different hwnd, see if it's a PopupMenu and if so, set the focus back on us (which will hide the popup).
        TCHAR className[256];

        // Make sure truncation won't affect the comparison.
        ASSERT(ARRAYSIZE(className) > _tcslen(PopupMenuWin::popupClassName()));

        if (GetClassName(focusedWindow, className, ARRAYSIZE(className)) && !_tcscmp(className, PopupMenuWin::popupClassName())) {
            // We don't let the WebView scroll here for two reasons - 1) To match Firefox behavior, 2) If we do scroll, we lose the
            // focus ring around the select menu.
            SetFocus(m_viewWindow);
            return true;
        }
    }

    PlatformWheelEvent wheelEvent(m_viewWindow, wParam, lParam, isMouseHWheel);
    Frame* coreFrame = core(m_mainFrame);
    if (!coreFrame)
        return false;

    return coreFrame->eventHandler()->handleWheelEvent(wheelEvent);
}

bool WebView::verticalScroll(WPARAM wParam, LPARAM /*lParam*/)
{
    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (LOWORD(wParam)) {
    case SB_LINEDOWN:
        granularity = ScrollByLine;
        direction = ScrollDown;
        break;
    case SB_LINEUP:
        granularity = ScrollByLine;
        direction = ScrollUp;
        break;
    case SB_PAGEDOWN:
        granularity = ScrollByDocument;
        direction = ScrollDown;
        break;
    case SB_PAGEUP:
        granularity = ScrollByDocument;
        direction = ScrollUp;
        break;
    default:
        return false;
        break;
    }
    
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    return frame->eventHandler()->scrollRecursively(direction, granularity);
}

bool WebView::horizontalScroll(WPARAM wParam, LPARAM /*lParam*/)
{
    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (LOWORD(wParam)) {
    case SB_LINELEFT:
        granularity = ScrollByLine;
        direction = ScrollLeft;
        break;
    case SB_LINERIGHT:
        granularity = ScrollByLine;
        direction = ScrollRight;
        break;
    case SB_PAGELEFT:
        granularity = ScrollByDocument;
        direction = ScrollLeft;
        break;
    case SB_PAGERIGHT:
        granularity = ScrollByDocument;
        direction = ScrollRight;
        break;
    default:
        return false;
    }

    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    return frame->eventHandler()->scrollRecursively(direction, granularity);
}


bool WebView::execCommand(WPARAM wParam, LPARAM /*lParam*/)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    switch (LOWORD(wParam)) {
        case SelectAll:
            return frame->editor()->command("SelectAll").execute();
        case Undo:
            return frame->editor()->command("Undo").execute();
        case Redo:
            return frame->editor()->command("Redo").execute();
    }
    return false;
}

bool WebView::keyUp(WPARAM virtualKeyCode, LPARAM keyData, bool systemKeyDown)
{
    PlatformKeyboardEvent keyEvent(m_viewWindow, virtualKeyCode, keyData, PlatformKeyboardEvent::KeyUp, systemKeyDown);

    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    m_currentCharacterCode = 0;

    return frame->eventHandler()->keyEvent(keyEvent);
}

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;


struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

struct KeyPressEntry {
    unsigned charCode;
    unsigned modifiers;
    const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
    { VK_LEFT,   0,                  "MoveLeft"                                    },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"                  },
    { VK_LEFT,   CtrlKey,            "MoveWordLeft"                                },
    { VK_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"              },
    { VK_RIGHT,  0,                  "MoveRight"                                   },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"                 },
    { VK_RIGHT,  CtrlKey,            "MoveWordRight"                               },
    { VK_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"             },
    { VK_UP,     0,                  "MoveUp"                                      },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection"                    },
    { VK_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"                },
    { VK_DOWN,   0,                  "MoveDown"                                    },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection"                  },
    { VK_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"              },
    { VK_PRIOR,  0,                  "MovePageUp"                                  },
    { VK_NEXT,   0,                  "MovePageDown"                                },
    { VK_HOME,   0,                  "MoveToBeginningOfLine"                       },
    { VK_HOME,   ShiftKey,           "MoveToBeginningOfLineAndModifySelection"     },
    { VK_HOME,   CtrlKey,            "MoveToBeginningOfDocument"                   },
    { VK_HOME,   CtrlKey | ShiftKey, "MoveToBeginningOfDocumentAndModifySelection" },

    { VK_END,    0,                  "MoveToEndOfLine"                             },
    { VK_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"           },
    { VK_END,    CtrlKey,            "MoveToEndOfDocument"                         },
    { VK_END,    CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection"       },

    { VK_BACK,   0,                  "DeleteBackward"                              },
    { VK_BACK,   ShiftKey,           "DeleteBackward"                              },
    { VK_DELETE, 0,                  "DeleteForward"                               },
    { VK_BACK,   CtrlKey,            "DeleteWordBackward"                          },
    { VK_DELETE, CtrlKey,            "DeleteWordForward"                           },
    
    { 'B',       CtrlKey,            "ToggleBold"                                  },
    { 'I',       CtrlKey,            "ToggleItalic"                                },

    { VK_ESCAPE, 0,                  "Cancel"                                      },
    { VK_OEM_PERIOD, CtrlKey,        "Cancel"                                      },
    { VK_TAB,    0,                  "InsertTab"                                   },
    { VK_TAB,    ShiftKey,           "InsertBacktab"                               },
    { VK_RETURN, 0,                  "InsertNewline"                               },
    { VK_RETURN, CtrlKey,            "InsertNewline"                               },
    { VK_RETURN, AltKey,             "InsertNewline"                               },
    { VK_RETURN, ShiftKey,           "InsertNewline"                               },
    { VK_RETURN, AltKey | ShiftKey,  "InsertNewline"                               },

    // It's not quite clear whether clipboard shortcuts and Undo/Redo should be handled
    // in the application or in WebKit. We chose WebKit.
    { 'C',       CtrlKey,            "Copy"                                        },
    { 'V',       CtrlKey,            "Paste"                                       },
    { 'X',       CtrlKey,            "Cut"                                         },
    { 'A',       CtrlKey,            "SelectAll"                                   },
    { VK_INSERT, CtrlKey,            "Copy"                                        },
    { VK_DELETE, ShiftKey,           "Cut"                                         },
    { VK_INSERT, ShiftKey,           "Paste"                                       },
    { 'Z',       CtrlKey,            "Undo"                                        },
    { 'Z',       CtrlKey | ShiftKey, "Redo"                                        },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                   },
    { '\t',   ShiftKey,           "InsertBacktab"                               },
    { '\r',   0,                  "InsertNewline"                               },
    { '\r',   CtrlKey,            "InsertNewline"                               },
    { '\r',   AltKey,             "InsertNewline"                               },
    { '\r',   ShiftKey,           "InsertNewline"                               },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

const char* WebView::interpretKeyEvent(const KeyboardEvent* evt)
{
    ASSERT(evt->type() == eventNames().keydownEvent || evt->type() == eventNames().keypressEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < _countof(keyDownEntries); i++)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (unsigned i = 0; i < _countof(keyPressEntries); i++)
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (evt->shiftKey())
        modifiers |= ShiftKey;
    if (evt->altKey())
        modifiers |= AltKey;
    if (evt->ctrlKey())
        modifiers |= CtrlKey;

    if (evt->type() == eventNames().keydownEvent) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool WebView::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent || keyEvent->isSystemKey())  // do not treat this as text input if it's a system key event
        return false;

    Editor::Command command = frame->editor()->command(interpretKeyEvent(evt));

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(evt);
    }

     if (command.execute(evt))
        return true;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (evt->charCode() < ' ')
        return false;

    return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}

bool WebView::keyDown(WPARAM virtualKeyCode, LPARAM keyData, bool systemKeyDown)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (virtualKeyCode == VK_CAPITAL)
        frame->eventHandler()->capsLockStateMayHaveChanged();

    PlatformKeyboardEvent keyEvent(m_viewWindow, virtualKeyCode, keyData, PlatformKeyboardEvent::RawKeyDown, systemKeyDown);
    bool handled = frame->eventHandler()->keyEvent(keyEvent);

    // These events cannot be canceled, and we have no default handling for them.
    // FIXME: match IE list more closely, see <http://msdn2.microsoft.com/en-us/library/ms536938.aspx>.
    if (systemKeyDown && virtualKeyCode != VK_RETURN)
        return false;

    if (handled) {
        // FIXME: remove WM_UNICHAR, too
        MSG msg;
        // WM_SYSCHAR events should not be removed, because access keys are implemented in WebCore in WM_SYSCHAR handler.
        if (!systemKeyDown)
            ::PeekMessage(&msg, m_viewWindow, WM_CHAR, WM_CHAR, PM_REMOVE);
        return true;
    }

    // We need to handle back/forward using either Backspace(+Shift) or Ctrl+Left/Right Arrow keys.
    if ((virtualKeyCode == VK_BACK && keyEvent.shiftKey()) || (virtualKeyCode == VK_RIGHT && keyEvent.ctrlKey()))
        return m_page->goForward();
    if (virtualKeyCode == VK_BACK || (virtualKeyCode == VK_LEFT && keyEvent.ctrlKey()))
        return m_page->goBack();

    // Need to scroll the page if the arrow keys, pgup/dn, or home/end are hit.
    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (virtualKeyCode) {
        case VK_LEFT:
            granularity = ScrollByLine;
            direction = ScrollLeft;
            break;
        case VK_RIGHT:
            granularity = ScrollByLine;
            direction = ScrollRight;
            break;
        case VK_UP:
            granularity = ScrollByLine;
            direction = ScrollUp;
            break;
        case VK_DOWN:
            granularity = ScrollByLine;
            direction = ScrollDown;
            break;
        case VK_HOME:
            granularity = ScrollByDocument;
            direction = ScrollUp;
            break;
        case VK_END:
            granularity = ScrollByDocument;
            direction = ScrollDown;
            break;
        case VK_PRIOR:
            granularity = ScrollByPage;
            direction = ScrollUp;
            break;
        case VK_NEXT:
            granularity = ScrollByPage;
            direction = ScrollDown;
            break;
        default:
            return false;
    }

    return frame->eventHandler()->scrollRecursively(direction, granularity);
}

bool WebView::keyPress(WPARAM charCode, LPARAM keyData, bool systemKeyDown)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    PlatformKeyboardEvent keyEvent(m_viewWindow, charCode, keyData, PlatformKeyboardEvent::Char, systemKeyDown);
    // IE does not dispatch keypress event for WM_SYSCHAR.
    if (systemKeyDown)
        return frame->eventHandler()->handleAccessKey(keyEvent);
    return frame->eventHandler()->keyEvent(keyEvent);
}

bool WebView::registerWebViewWindowClass()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    haveRegisteredWindowClass = true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = WebViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 4; // 4 bytes for the IWebView pointer
    wcex.hInstance      = gInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = ::LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebViewWindowClassName;
    wcex.hIconSm        = 0;

    return !!RegisterClassEx(&wcex);
}

static HWND findTopLevelParent(HWND window)
{
    if (!window)
        return 0;

    HWND current = window;
    for (HWND parent = GetParent(current); current; current = parent, parent = GetParent(parent))
        if (!parent || !(GetWindowLongPtr(current, GWL_STYLE) & (WS_POPUP | WS_CHILD)))
            return current;
    ASSERT_NOT_REACHED();
    return 0;
}

LRESULT CALLBACK WebView::WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    WebView* webView = reinterpret_cast<WebView*>(longPtr);
    WebFrame* mainFrameImpl = webView ? webView->topLevelFrame() : 0;
    if (!mainFrameImpl || webView->isBeingDestroyed())
        return DefWindowProc(hWnd, message, wParam, lParam);

    // hold a ref, since the WebView could go away in an event handler.
    COMPtr<WebView> protector(webView);
    ASSERT(webView);

    // Windows Media Player has a modal message loop that will deliver messages
    // to us at inappropriate times and we will crash if we handle them when
    // they are delivered. We repost paint messages so that we eventually get
    // a chance to paint once the modal loop has exited, but other messages
    // aren't safe to repost, so we just drop them.
    if (PluginView::isCallingPlugin()) {
        if (message == WM_PAINT)
            PostMessage(hWnd, message, wParam, lParam);
        return 0;
    }

    bool handled = true;

    switch (message) {
        case WM_PAINT: {
            webView->paint(0, 0);
            break;
        }
        case WM_PRINTCLIENT:
            webView->paint((HDC)wParam, lParam);
            break;
        case WM_DESTROY:
            webView->setIsBeingDestroyed();
            webView->close();
            break;
        case WM_GESTURENOTIFY:
            handled = webView->gestureNotify(wParam, lParam);
            break;
        case WM_GESTURE:
            handled = webView->gesture(wParam, lParam);
            break;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MOUSELEAVE:
        case WM_CANCELMODE:
            if (Frame* coreFrame = core(mainFrameImpl))
                if (coreFrame->view()->didFirstLayout())
                    handled = webView->handleMouseEvent(message, wParam, lParam);
            break;
        case WM_MOUSEWHEEL:
        case WM_VISTA_MOUSEHWHEEL:
            if (Frame* coreFrame = core(mainFrameImpl))
                if (coreFrame->view()->didFirstLayout())
                    handled = webView->mouseWheel(wParam, lParam, message == WM_VISTA_MOUSEHWHEEL);
            break;
        case WM_SYSKEYDOWN:
            handled = webView->keyDown(wParam, lParam, true);
            break;
        case WM_KEYDOWN:
            handled = webView->keyDown(wParam, lParam);
            break;
        case WM_SYSKEYUP:
            handled = webView->keyUp(wParam, lParam, true);
            break;
        case WM_KEYUP:
            handled = webView->keyUp(wParam, lParam);
            break;
        case WM_SYSCHAR:
            handled = webView->keyPress(wParam, lParam, true);
            break;
        case WM_CHAR:
            handled = webView->keyPress(wParam, lParam);
            break;
        // FIXME: We need to check WM_UNICHAR to support supplementary characters (that don't fit in 16 bits).
        case WM_SIZE:
            if (lParam != 0)
                webView->sizeChanged(IntSize(LOWORD(lParam), HIWORD(lParam)));
            break;
        case WM_SHOWWINDOW:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
            if (wParam == 0) {
                // The window is being hidden (e.g., because we switched tabs).
                // Null out our backing store.
                webView->deleteBackingStore();
            }
#if USE(ACCELERATED_COMPOSITING)
            else if (webView->isAcceleratedCompositing())
                webView->layerRendererBecameVisible();
#endif
            break;
        case WM_SETFOCUS: {
            COMPtr<IWebUIDelegate> uiDelegate;
            COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate;
            if (SUCCEEDED(webView->uiDelegate(&uiDelegate)) && uiDelegate
                && SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**) &uiDelegatePrivate)) && uiDelegatePrivate)
                uiDelegatePrivate->webViewReceivedFocus(webView);

            FocusController* focusController = webView->page()->focusController();
            if (Frame* frame = focusController->focusedFrame()) {
                // Send focus events unless the previously focused window is a
                // child of ours (for example a plugin).
                if (!IsChild(hWnd, reinterpret_cast<HWND>(wParam)))
                    focusController->setFocused(true);
            } else
                focusController->setFocused(true);
            break;
        }
        case WM_KILLFOCUS: {
            COMPtr<IWebUIDelegate> uiDelegate;
            COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate;
            HWND newFocusWnd = reinterpret_cast<HWND>(wParam);
            if (SUCCEEDED(webView->uiDelegate(&uiDelegate)) && uiDelegate
                && SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**) &uiDelegatePrivate)) && uiDelegatePrivate)
                uiDelegatePrivate->webViewLostFocus(webView, (OLE_HANDLE)(ULONG64)newFocusWnd);

            FocusController* focusController = webView->page()->focusController();
            Frame* frame = focusController->focusedOrMainFrame();
            webView->resetIME(frame);
            // Send blur events unless we're losing focus to a child of ours.
            if (!IsChild(hWnd, newFocusWnd))
                focusController->setFocused(false);

            // If we are pan-scrolling when we lose focus, stop the pan scrolling.
            frame->eventHandler()->stopAutoscrollTimer();

            break;
        }
        case WM_WINDOWPOSCHANGED:
            if (reinterpret_cast<WINDOWPOS*>(lParam)->flags & SWP_SHOWWINDOW)
                webView->updateActiveStateSoon();
            handled = false;
            break;
        case WM_CUT:
            webView->cut(0);
            break;
        case WM_COPY:
            webView->copy(0);
            break;
        case WM_PASTE:
            webView->paste(0);
            break;
        case WM_CLEAR:
            webView->delete_(0);
            break;
        case WM_COMMAND:
            if (HIWORD(wParam))
                handled = webView->execCommand(wParam, lParam);
            else // If the high word of wParam is 0, the message is from a menu
                webView->performContextMenuAction(wParam, lParam, false);
            break;
        case WM_MENUCOMMAND:
            webView->performContextMenuAction(wParam, lParam, true);
            break;
        case WM_CONTEXTMENU:
            handled = webView->handleContextMenuEvent(wParam, lParam);
            break;
        case WM_INITMENUPOPUP:
            handled = webView->onInitMenuPopup(wParam, lParam);
            break;
        case WM_MEASUREITEM:
            handled = webView->onMeasureItem(wParam, lParam);
            break;
        case WM_DRAWITEM:
            handled = webView->onDrawItem(wParam, lParam);
            break;
        case WM_UNINITMENUPOPUP:
            handled = webView->onUninitMenuPopup(wParam, lParam);
            break;
        case WM_XP_THEMECHANGED:
            if (Frame* coreFrame = core(mainFrameImpl)) {
                webView->deleteBackingStore();
                coreFrame->page()->theme()->themeChanged();
                ScrollbarTheme::nativeTheme()->themeChanged();
                RECT windowRect;
                ::GetClientRect(hWnd, &windowRect);
                ::InvalidateRect(hWnd, &windowRect, false);
#if USE(ACCELERATED_COMPOSITING)
                if (webView->isAcceleratedCompositing())
                    webView->setRootLayerNeedsDisplay();
#endif
           }
            break;
        case WM_MOUSEACTIVATE:
            webView->setMouseActivated(true);
            handled = false;
            break;
        case WM_GETDLGCODE: {
            COMPtr<IWebUIDelegate> uiDelegate;
            COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate;
            LONG_PTR dlgCode = 0;
            UINT keyCode = 0;
            if (lParam) {
                LPMSG lpMsg = (LPMSG)lParam;
                if (lpMsg->message == WM_KEYDOWN)
                    keyCode = (UINT) lpMsg->wParam;
            }
            if (SUCCEEDED(webView->uiDelegate(&uiDelegate)) && uiDelegate
                && SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**) &uiDelegatePrivate)) && uiDelegatePrivate
                && SUCCEEDED(uiDelegatePrivate->webViewGetDlgCode(webView, keyCode, &dlgCode)))
                return dlgCode;
            handled = false;
            break;
        }
        case WM_GETOBJECT:
            handled = webView->onGetObject(wParam, lParam, lResult);
            break;
        case WM_IME_STARTCOMPOSITION:
            handled = webView->onIMEStartComposition();
            break;
        case WM_IME_REQUEST:
            lResult = webView->onIMERequest(wParam, lParam);
            break;
        case WM_IME_COMPOSITION:
            handled = webView->onIMEComposition(lParam);
            break;
        case WM_IME_ENDCOMPOSITION:
            handled = webView->onIMEEndComposition();
            break;
        case WM_IME_CHAR:
            handled = webView->onIMEChar(wParam, lParam);
            break;
        case WM_IME_NOTIFY:
            handled = webView->onIMENotify(wParam, lParam, &lResult);
            break;
        case WM_IME_SELECT:
            handled = webView->onIMESelect(wParam, lParam);
            break;
        case WM_IME_SETCONTEXT:
            handled = webView->onIMESetContext(wParam, lParam);
            break;
        case WM_TIMER:
            switch (wParam) {
                case UpdateActiveStateTimer:
                    KillTimer(hWnd, UpdateActiveStateTimer);
                    webView->updateActiveState();
                    break;
                case DeleteBackingStoreTimer:
                    webView->deleteBackingStore();
                    break;
            }
            break;
        case WM_SETCURSOR:
            handled = ::SetCursor(webView->m_lastSetCursor);
            break;
        case WM_VSCROLL:
            handled = webView->verticalScroll(wParam, lParam);
            break;
        case WM_HSCROLL:
            handled = webView->horizontalScroll(wParam, lParam);
            break;
        default:
            handled = false;
            break;
    }

    if (!handled)
        lResult = DefWindowProc(hWnd, message, wParam, lParam);
    
    // Let the client know whether we consider this message handled.
    return (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP) ? !handled : lResult;
}

bool WebView::developerExtrasEnabled() const
{
    if (m_preferences->developerExtrasDisabledByOverride())
        return false;

#ifdef NDEBUG
    BOOL enabled;
    return SUCCEEDED(m_preferences->developerExtrasEnabled(&enabled)) && enabled;
#else
    return true;
#endif
}

static String osVersion()
{
    String osVersion;
    OSVERSIONINFO versionInfo = {0};
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    GetVersionEx(&versionInfo);

    if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        if (versionInfo.dwMajorVersion == 4) {
            if (versionInfo.dwMinorVersion == 0)
                osVersion = "Windows 95";
            else if (versionInfo.dwMinorVersion == 10)
                osVersion = "Windows 98";
            else if (versionInfo.dwMinorVersion == 90)
                osVersion = "Windows 98; Win 9x 4.90";
        }
    } else if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
        osVersion = String::format("Windows NT %d.%d", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion);

    if (!osVersion.length())
        osVersion = String::format("Windows %d.%d", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion);

    return osVersion;
}

static String webKitVersion()
{
    String versionStr = "420+";
    void* data = 0;

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    TCHAR path[MAX_PATH];
    GetModuleFileName(gInstance, path, ARRAYSIZE(path));
    DWORD handle;
    DWORD versionSize = GetFileVersionInfoSize(path, &handle);
    if (!versionSize)
        goto exit;
    data = malloc(versionSize);
    if (!data)
        goto exit;
    if (!GetFileVersionInfo(path, 0, versionSize, data))
        goto exit;
    UINT cbTranslate;
    if (!VerQueryValue(data, TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &cbTranslate))
        goto exit;
    TCHAR key[256];
    _stprintf_s(key, ARRAYSIZE(key), TEXT("\\StringFileInfo\\%04x%04x\\ProductVersion"), lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
    LPCTSTR productVersion;
    UINT productVersionLength;
    if (!VerQueryValue(data, (LPTSTR)(LPCTSTR)key, (void**)&productVersion, &productVersionLength))
        goto exit;
    versionStr = String(productVersion, productVersionLength);

exit:
    if (data)
        free(data);
    return versionStr;
}

const String& WebView::userAgentForKURL(const KURL&)
{
    if (m_userAgentOverridden)
        return m_userAgentCustom;

    if (!m_userAgentStandard.length())
        m_userAgentStandard = WebView::standardUserAgentWithApplicationName(m_applicationName);
    return m_userAgentStandard;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, CLSID_WebView))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebView*>(this);
    else if (IsEqualGUID(riid, IID_IWebView))
        *ppvObject = static_cast<IWebView*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewPrivate))
        *ppvObject = static_cast<IWebViewPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebIBActions))
        *ppvObject = static_cast<IWebIBActions*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewCSS))
        *ppvObject = static_cast<IWebViewCSS*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewEditing))
        *ppvObject = static_cast<IWebViewEditing*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewUndoableEditing))
        *ppvObject = static_cast<IWebViewUndoableEditing*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewEditingActions))
        *ppvObject = static_cast<IWebViewEditingActions*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationObserver))
        *ppvObject = static_cast<IWebNotificationObserver*>(this);
    else if (IsEqualGUID(riid, IID_IDropTarget))
        *ppvObject = static_cast<IDropTarget*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebView::AddRef(void)
{
    ASSERT(!m_deletionHasBegun);
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebView::Release(void)
{
    ASSERT(!m_deletionHasBegun);

    if (m_refCount == 1) {
        // Call close() now so that clients don't have to. (It's harmless to call close() multiple
        // times.) We do this here instead of in our destructor because close() can cause AddRef()
        // and Release() to be called, and if that happened in our destructor we would be destroyed
        // more than once.
        close();
    }

    ULONG newRef = --m_refCount;
    if (!newRef) {
#if !ASSERT_DISABLED
        m_deletionHasBegun = true;
#endif
        delete(this);
    }

    return newRef;
}

// IWebView --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::canShowMIMEType( 
    /* [in] */ BSTR mimeType,
    /* [retval][out] */ BOOL* canShow)
{
    String mimeTypeStr(mimeType, SysStringLen(mimeType));

    if (!canShow)
        return E_POINTER;

    *canShow = MIMETypeRegistry::isSupportedImageMIMEType(mimeTypeStr) ||
        MIMETypeRegistry::isSupportedNonImageMIMEType(mimeTypeStr) ||
        (m_page && m_page->pluginData() && m_page->pluginData()->supportsMimeType(mimeTypeStr)) ||
        shouldUseEmbeddedView(mimeTypeStr);
    
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canShowMIMETypeAsHTML( 
    /* [in] */ BSTR /*mimeType*/,
    /* [retval][out] */ BOOL* canShow)
{
    // FIXME
    *canShow = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::MIMETypesShownAsHTML( 
    /* [retval][out] */ IEnumVARIANT** /*enumVariant*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setMIMETypesShownAsHTML( 
        /* [size_is][in] */ BSTR* /*mimeTypes*/,
        /* [in] */ int /*cMimeTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::URLFromPasteboard( 
    /* [in] */ IDataObject* /*pasteboard*/,
    /* [retval][out] */ BSTR* /*url*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::URLTitleFromPasteboard( 
    /* [in] */ IDataObject* /*pasteboard*/,
    /* [retval][out] */ BSTR* /*urlTitle*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

static void WebKitSetApplicationCachePathIfNecessary()
{
    static bool initialized = false;
    if (initialized)
        return;

    String path = localUserSpecificStorageDirectory();
    if (!path.isNull())
        cacheStorage().setCacheDirectory(path);

    initialized = true;
}

bool WebView::shouldInitializeTrackPointHack()
{
    static bool shouldCreateScrollbars;
    static bool hasRunTrackPointCheck;

    if (hasRunTrackPointCheck)
        return shouldCreateScrollbars;

    hasRunTrackPointCheck = true;
    const TCHAR trackPointKeys[][50] = { TEXT("Software\\Lenovo\\TrackPoint"),
        TEXT("Software\\Lenovo\\UltraNav"),
        TEXT("Software\\Alps\\Apoint\\TrackPoint"),
        TEXT("Software\\Synaptics\\SynTPEnh\\UltraNavUSB"),
        TEXT("Software\\Synaptics\\SynTPEnh\\UltraNavPS2") };

    for (int i = 0; i < 5; ++i) {
        HKEY trackPointKey;
        int readKeyResult = ::RegOpenKeyEx(HKEY_CURRENT_USER, trackPointKeys[i], 0, KEY_READ, &trackPointKey);
        ::RegCloseKey(trackPointKey);
        if (readKeyResult == ERROR_SUCCESS) {
            shouldCreateScrollbars = true;
            return shouldCreateScrollbars;
        }
    }

    return shouldCreateScrollbars;
}
    
HRESULT STDMETHODCALLTYPE WebView::initWithFrame( 
    /* [in] */ RECT frame,
    /* [in] */ BSTR frameName,
    /* [in] */ BSTR groupName)
{
    HRESULT hr = S_OK;

    if (m_viewWindow)
        return E_FAIL;

    registerWebViewWindowClass();

    m_viewWindow = CreateWindowEx(0, kWebViewWindowClassName, 0, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top, m_hostWindow ? m_hostWindow : HWND_MESSAGE, 0, gInstance, 0);
    ASSERT(::IsWindow(m_viewWindow));

    if (shouldInitializeTrackPointHack()) {
        // If we detected a registry key belonging to a TrackPoint driver, then create fake trackpoint
        // scrollbars, so the WebView will receive WM_VSCROLL and WM_HSCROLL messages. We create one
        // vertical scrollbar and one horizontal to allow for receiving both types of messages.
        ::CreateWindow(TEXT("SCROLLBAR"), TEXT("FAKETRACKPOINTHSCROLLBAR"), WS_CHILD | WS_VISIBLE | SBS_HORZ, 0, 0, 0, 0, m_viewWindow, 0, gInstance, 0);
        ::CreateWindow(TEXT("SCROLLBAR"), TEXT("FAKETRACKPOINTVSCROLLBAR"), WS_CHILD | WS_VISIBLE | SBS_VERT, 0, 0, 0, 0, m_viewWindow, 0, gInstance, 0);
    }

    hr = registerDragDrop();
    if (FAILED(hr))
        return hr;

    WebPreferences* sharedPreferences = WebPreferences::sharedStandardPreferences();
    sharedPreferences->willAddToWebView();
    m_preferences = sharedPreferences;

    InitializeLoggingChannelsIfNecessary();
#if ENABLE(DATABASE)
    WebKitInitializeWebDatabasesIfNecessary();
#endif
    WebKitSetApplicationCachePathIfNecessary();
    WebPlatformStrategies::initialize();

#if USE(SAFARI_THEME)
    BOOL shouldPaintNativeControls;
    if (SUCCEEDED(m_preferences->shouldPaintNativeControls(&shouldPaintNativeControls)))
        Settings::setShouldPaintNativeControls(shouldPaintNativeControls);
#endif

    BOOL useHighResolutionTimer;
    if (SUCCEEDED(m_preferences->shouldUseHighResolutionTimers(&useHighResolutionTimer)))
        Settings::setShouldUseHighResolutionTimers(useHighResolutionTimer);

    Page::PageClients pageClients;
    pageClients.chromeClient = new WebChromeClient(this);
    pageClients.contextMenuClient = new WebContextMenuClient(this);
    pageClients.editorClient = new WebEditorClient(this);
    pageClients.dragClient = new WebDragClient(this);
    pageClients.inspectorClient = new WebInspectorClient(this);
    pageClients.pluginHalterClient = new WebPluginHalterClient(this);
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    pageClients.geolocationControllerClient = new WebGeolocationControllerClient(this);
#endif
    m_page = new Page(pageClients);
    m_page->settings()->setMinDOMTimerInterval(0.004);

    BSTR localStoragePath;
    if (SUCCEEDED(m_preferences->localStorageDatabasePath(&localStoragePath))) {
        m_page->settings()->setLocalStorageDatabasePath(String(localStoragePath, SysStringLen(localStoragePath)));
        SysFreeString(localStoragePath);
    }

    if (m_uiDelegate) {
        BSTR path;
        if (SUCCEEDED(m_uiDelegate->ftpDirectoryTemplatePath(this, &path))) {
            m_page->settings()->setFTPDirectoryTemplatePath(String(path, SysStringLen(path)));
            SysFreeString(path);
        }
    }

    WebFrame* webFrame = WebFrame::createInstance();
    RefPtr<Frame> coreFrame = webFrame->init(this, m_page, 0);
    m_mainFrame = webFrame;
    webFrame->Release(); // The WebFrame is owned by the Frame, so release our reference to it.

    coreFrame->tree()->setName(String(frameName, SysStringLen(frameName)));
    coreFrame->init();
    setGroupName(groupName);

    addToAllWebViewsSet();

    #pragma warning(suppress: 4244)
    SetWindowLongPtr(m_viewWindow, 0, (LONG_PTR)this);
    ShowWindow(m_viewWindow, SW_SHOW);

    initializeToolTipWindow();
    windowAncestryDidChange();

    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->addObserver(this, WebPreferences::webPreferencesChangedNotification(), static_cast<IWebPreferences*>(m_preferences.get()));
    m_preferences->postPreferencesChangesNotification();

    setSmartInsertDeleteEnabled(TRUE);
    return hr;
}

static bool initCommonControls()
{
    static bool haveInitialized = false;
    if (haveInitialized)
        return true;

    INITCOMMONCONTROLSEX init;
    init.dwSize = sizeof(init);
    init.dwICC = ICC_TREEVIEW_CLASSES;
    haveInitialized = !!::InitCommonControlsEx(&init);
    return haveInitialized;
}

void WebView::initializeToolTipWindow()
{
    if (!initCommonControls())
        return;

    m_toolTipHwnd = CreateWindowEx(WS_EX_TRANSPARENT, TOOLTIPS_CLASS, 0, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                   m_viewWindow, 0, 0, 0);
    if (!m_toolTipHwnd)
        return;

    TOOLINFO info = {0};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS ;
    info.uId = reinterpret_cast<UINT_PTR>(m_viewWindow);

    ::SendMessage(m_toolTipHwnd, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info));
    ::SendMessage(m_toolTipHwnd, TTM_SETMAXTIPWIDTH, 0, maxToolTipWidth);

    ::SetWindowPos(m_toolTipHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WebView::setToolTip(const String& toolTip)
{
    if (!m_toolTipHwnd)
        return;

    if (toolTip == m_toolTip)
        return;

    m_toolTip = toolTip;

    if (!m_toolTip.isEmpty()) {
        TOOLINFO info = {0};
        info.cbSize = sizeof(info);
        info.uFlags = TTF_IDISHWND;
        info.uId = reinterpret_cast<UINT_PTR>(m_viewWindow);
        info.lpszText = const_cast<UChar*>(m_toolTip.charactersWithNullTermination());
        ::SendMessage(m_toolTipHwnd, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(&info));
    }

    ::SendMessage(m_toolTipHwnd, TTM_ACTIVATE, !m_toolTip.isEmpty(), 0);
}

HRESULT WebView::notifyDidAddIcon(IWebNotification* notification)
{
    COMPtr<IPropertyBag> propertyBag;
    HRESULT hr = notification->userInfo(&propertyBag);
    if (FAILED(hr))
        return hr;
    if (!propertyBag)
        return E_FAIL;

    COMPtr<CFDictionaryPropertyBag> dictionaryPropertyBag;
    hr = propertyBag->QueryInterface(&dictionaryPropertyBag);
    if (FAILED(hr))
        return hr;

    CFDictionaryRef dictionary = dictionaryPropertyBag->dictionary();
    if (!dictionary)
        return E_FAIL;

    CFTypeRef value = CFDictionaryGetValue(dictionary, WebIconDatabase::iconDatabaseNotificationUserInfoURLKey());
    if (!value)
        return E_FAIL;
    if (CFGetTypeID(value) != CFStringGetTypeID())
        return E_FAIL;

    String mainFrameURL;
    if (m_mainFrame)
        mainFrameURL = m_mainFrame->url().string();

    if (!mainFrameURL.isEmpty() && mainFrameURL == String((CFStringRef)value))
        dispatchDidReceiveIconFromWebFrame(m_mainFrame);

    return hr;
}

void WebView::registerForIconNotification(bool listen)
{
    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    if (listen)
        nc->addObserver(this, WebIconDatabase::iconDatabaseDidAddIconNotification(), 0);
    else
        nc->removeObserver(this, WebIconDatabase::iconDatabaseDidAddIconNotification(), 0);
}

void WebView::dispatchDidReceiveIconFromWebFrame(WebFrame* frame)
{
    registerForIconNotification(false);

    if (m_frameLoadDelegate)
        // FIXME: <rdar://problem/5491010> - Pass in the right HBITMAP. 
        m_frameLoadDelegate->didReceiveIcon(this, 0, frame);
}

HRESULT STDMETHODCALLTYPE WebView::setUIDelegate( 
    /* [in] */ IWebUIDelegate* d)
{
    m_uiDelegate = d;

    if (m_uiDelegatePrivate)
        m_uiDelegatePrivate = 0;

    if (d) {
        if (FAILED(d->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&m_uiDelegatePrivate)))
            m_uiDelegatePrivate = 0;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::uiDelegate( 
    /* [out][retval] */ IWebUIDelegate** d)
{
    if (!m_uiDelegate)
        return E_FAIL;

    return m_uiDelegate.copyRefTo(d);
}

HRESULT STDMETHODCALLTYPE WebView::setResourceLoadDelegate( 
    /* [in] */ IWebResourceLoadDelegate* d)
{
    m_resourceLoadDelegate = d;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::resourceLoadDelegate( 
    /* [out][retval] */ IWebResourceLoadDelegate** d)
{
    if (!m_resourceLoadDelegate)
        return E_FAIL;

    return m_resourceLoadDelegate.copyRefTo(d);
}

HRESULT STDMETHODCALLTYPE WebView::setDownloadDelegate( 
    /* [in] */ IWebDownloadDelegate* d)
{
    m_downloadDelegate = d;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::downloadDelegate( 
    /* [out][retval] */ IWebDownloadDelegate** d)
{
    if (!m_downloadDelegate)
        return E_FAIL;

    return m_downloadDelegate.copyRefTo(d);
}

HRESULT STDMETHODCALLTYPE WebView::setFrameLoadDelegate( 
    /* [in] */ IWebFrameLoadDelegate* d)
{
    m_frameLoadDelegate = d;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::frameLoadDelegate( 
    /* [out][retval] */ IWebFrameLoadDelegate** d)
{
    if (!m_frameLoadDelegate)
        return E_FAIL;

    return m_frameLoadDelegate.copyRefTo(d);
}

HRESULT STDMETHODCALLTYPE WebView::setPolicyDelegate( 
    /* [in] */ IWebPolicyDelegate* d)
{
    m_policyDelegate = d;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::policyDelegate( 
    /* [out][retval] */ IWebPolicyDelegate** d)
{
    if (!m_policyDelegate)
        return E_FAIL;
    return m_policyDelegate.copyRefTo(d);
}

HRESULT STDMETHODCALLTYPE WebView::mainFrame( 
    /* [out][retval] */ IWebFrame** frame)
{
    if (!frame) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *frame = m_mainFrame;
    if (!m_mainFrame)
        return E_FAIL;

    m_mainFrame->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::focusedFrame( 
    /* [out][retval] */ IWebFrame** frame)
{
    if (!frame) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *frame = 0;
    Frame* f = m_page->focusController()->focusedFrame();
    if (!f)
        return E_FAIL;

    WebFrame* webFrame = kit(f);
    if (!webFrame)
        return E_FAIL;

    return webFrame->QueryInterface(IID_IWebFrame, (void**) frame);
}
HRESULT STDMETHODCALLTYPE WebView::backForwardList( 
    /* [out][retval] */ IWebBackForwardList** list)
{
    if (!m_useBackForwardList)
        return E_FAIL;
 
    *list = WebBackForwardList::createInstance(m_page->backForwardList());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setMaintainsBackForwardList( 
    /* [in] */ BOOL flag)
{
    m_useBackForwardList = !!flag;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::goBack( 
    /* [retval][out] */ BOOL* succeeded)
{
    *succeeded = m_page->goBack();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::goForward( 
    /* [retval][out] */ BOOL* succeeded)
{
    *succeeded = m_page->goForward();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::goToBackForwardItem( 
    /* [in] */ IWebHistoryItem* item,
    /* [retval][out] */ BOOL* succeeded)
{
    *succeeded = FALSE;

    COMPtr<WebHistoryItem> webHistoryItem;
    HRESULT hr = item->QueryInterface(&webHistoryItem);
    if (FAILED(hr))
        return hr;

    m_page->goToItem(webHistoryItem->historyItem(), FrameLoadTypeIndexedBackForward);
    *succeeded = TRUE;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setTextSizeMultiplier( 
    /* [in] */ float multiplier)
{
    if (!m_mainFrame)
        return E_FAIL;
    setZoomMultiplier(multiplier, true);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setPageSizeMultiplier( 
    /* [in] */ float multiplier)
{
    if (!m_mainFrame)
        return E_FAIL;
    setZoomMultiplier(multiplier, false);
    return S_OK;
}

void WebView::setZoomMultiplier(float multiplier, bool isTextOnly)
{
    m_zoomMultiplier = multiplier;
    m_zoomsTextOnly = isTextOnly;

    if (Frame* coreFrame = core(m_mainFrame)) {
        if (FrameView* view = coreFrame->view()) {
            if (m_zoomsTextOnly)
                view->setPageAndTextZoomFactors(1, multiplier);
            else
                view->setPageAndTextZoomFactors(multiplier, 1);
        }
    }
}

HRESULT STDMETHODCALLTYPE WebView::textSizeMultiplier( 
    /* [retval][out] */ float* multiplier)
{
    *multiplier = zoomMultiplier(true);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::pageSizeMultiplier( 
    /* [retval][out] */ float* multiplier)
{
    *multiplier = zoomMultiplier(false);
    return S_OK;
}

float WebView::zoomMultiplier(bool isTextOnly)
{
    if (isTextOnly != m_zoomsTextOnly)
        return 1.0f;
    return m_zoomMultiplier;
}

HRESULT STDMETHODCALLTYPE WebView::setApplicationNameForUserAgent( 
    /* [in] */ BSTR applicationName)
{
    m_applicationName = String(applicationName, SysStringLen(applicationName));
    m_userAgentStandard = String();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::applicationNameForUserAgent( 
    /* [retval][out] */ BSTR* applicationName)
{
    *applicationName = SysAllocStringLen(m_applicationName.characters(), m_applicationName.length());
    if (!*applicationName && m_applicationName.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomUserAgent( 
    /* [in] */ BSTR userAgentString)
{
    m_userAgentOverridden = userAgentString;
    m_userAgentCustom = String(userAgentString, SysStringLen(userAgentString));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::customUserAgent( 
    /* [retval][out] */ BSTR* userAgentString)
{
    *userAgentString = 0;
    if (!m_userAgentOverridden)
        return S_OK;
    *userAgentString = SysAllocStringLen(m_userAgentCustom.characters(), m_userAgentCustom.length());
    if (!*userAgentString && m_userAgentCustom.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::userAgentForURL( 
    /* [in] */ BSTR url,
    /* [retval][out] */ BSTR* userAgent)
{
    String userAgentString = userAgentForKURL(MarshallingHelpers::BSTRToKURL(url));
    *userAgent = SysAllocStringLen(userAgentString.characters(), userAgentString.length());
    if (!*userAgent && userAgentString.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::supportsTextEncoding( 
    /* [retval][out] */ BOOL* supports)
{
    *supports = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomTextEncodingName( 
    /* [in] */ BSTR encodingName)
{
    if (!m_mainFrame)
        return E_FAIL;

    HRESULT hr;
    BSTR oldEncoding;
    hr = customTextEncodingName(&oldEncoding);
    if (FAILED(hr))
        return hr;

    if (oldEncoding != encodingName && (!oldEncoding || !encodingName || _tcscmp(oldEncoding, encodingName))) {
        if (Frame* coreFrame = core(m_mainFrame))
            coreFrame->loader()->reloadWithOverrideEncoding(String(encodingName, SysStringLen(encodingName)));
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::customTextEncodingName( 
    /* [retval][out] */ BSTR* encodingName)
{
    HRESULT hr = S_OK;
    COMPtr<IWebDataSource> dataSource;
    COMPtr<WebDataSource> dataSourceImpl;
    *encodingName = 0;

    if (!m_mainFrame)
        return E_FAIL;

    if (FAILED(m_mainFrame->provisionalDataSource(&dataSource)) || !dataSource) {
        hr = m_mainFrame->dataSource(&dataSource);
        if (FAILED(hr) || !dataSource)
            return hr;
    }

    hr = dataSource->QueryInterface(IID_WebDataSource, (void**)&dataSourceImpl);
    if (FAILED(hr))
        return hr;

    BString str = dataSourceImpl->documentLoader()->overrideEncoding();
    if (FAILED(hr))
        return hr;

    if (!*encodingName)
        *encodingName = SysAllocStringLen(m_overrideEncoding.characters(), m_overrideEncoding.length());

    if (!*encodingName && m_overrideEncoding.length())
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setMediaStyle( 
    /* [in] */ BSTR /*media*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::mediaStyle( 
    /* [retval][out] */ BSTR* /*media*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::stringByEvaluatingJavaScriptFromString( 
    /* [in] */ BSTR script, // assumes input does not have "JavaScript" at the begining.
    /* [retval][out] */ BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    Frame* coreFrame = core(m_mainFrame);
    if (!coreFrame)
        return E_FAIL;

    JSC::JSValue scriptExecutionResult = coreFrame->script()->executeScript(WTF::String(script), true).jsValue();
    if (!scriptExecutionResult)
        return E_FAIL;
    else if (scriptExecutionResult.isString()) {
        JSLock lock(JSC::SilenceAssertionsOnly);
        JSC::ExecState* exec = coreFrame->script()->globalObject(mainThreadNormalWorld())->globalExec();
        *result = BString(ustringToString(scriptExecutionResult.getString(exec)));
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::windowScriptObject( 
    /* [retval][out] */ IWebScriptObject** /*webScriptObject*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setPreferences( 
    /* [in] */ IWebPreferences* prefs)
{
    if (!prefs)
        prefs = WebPreferences::sharedStandardPreferences();

    if (m_preferences == prefs)
        return S_OK;

    COMPtr<WebPreferences> webPrefs(Query, prefs);
    if (!webPrefs)
        return E_NOINTERFACE;
    webPrefs->willAddToWebView();

    COMPtr<WebPreferences> oldPrefs = m_preferences;

    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    nc->removeObserver(this, WebPreferences::webPreferencesChangedNotification(), static_cast<IWebPreferences*>(m_preferences.get()));

    BSTR identifier = 0;
    oldPrefs->identifier(&identifier);
    oldPrefs->didRemoveFromWebView();
    oldPrefs = 0; // Make sure we release the reference, since WebPreferences::removeReferenceForIdentifier will check for last reference to WebPreferences

    m_preferences = webPrefs;

    if (identifier) {
        WebPreferences::removeReferenceForIdentifier(identifier);
        SysFreeString(identifier);
    }

    nc->addObserver(this, WebPreferences::webPreferencesChangedNotification(), static_cast<IWebPreferences*>(m_preferences.get()));

    m_preferences->postPreferencesChangesNotification();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::preferences( 
    /* [retval][out] */ IWebPreferences** prefs)
{
    if (!prefs)
        return E_POINTER;
    *prefs = m_preferences.get();
    if (m_preferences)
        m_preferences->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setPreferencesIdentifier( 
    /* [in] */ BSTR /*anIdentifier*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::preferencesIdentifier( 
    /* [retval][out] */ BSTR* /*anIdentifier*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

static void systemParameterChanged(WPARAM parameter)
{
#if PLATFORM(CG)
    if (parameter == SPI_SETFONTSMOOTHING || parameter == SPI_SETFONTSMOOTHINGTYPE || parameter == SPI_SETFONTSMOOTHINGCONTRAST || parameter == SPI_SETFONTSMOOTHINGORIENTATION)
        wkSystemFontSmoothingChanged();
#endif
}

void WebView::windowReceivedMessage(HWND, UINT message, WPARAM wParam, LPARAM)
{
    switch (message) {
    case WM_NCACTIVATE:
        updateActiveStateSoon();
        if (!wParam)
            deleteBackingStoreSoon();
        break;
    case WM_SETTINGCHANGE:
        systemParameterChanged(wParam);
        break;
    }
}

void WebView::updateActiveStateSoon() const
{
    // This function is called while processing the WM_NCACTIVATE message.
    // While processing WM_NCACTIVATE when we are being deactivated, GetActiveWindow() will
    // still return our window. If we were to call updateActiveState() in that case, we would
    // wrongly think that we are still the active window. To work around this, we update our
    // active state after a 0-delay timer fires, at which point GetActiveWindow() will return
    // the newly-activated window.

    SetTimer(m_viewWindow, UpdateActiveStateTimer, 0, 0);
}

void WebView::deleteBackingStoreSoon()
{
    if (pendingDeleteBackingStoreSet.size() > 2) {
        Vector<WebView*> views;
        HashSet<WebView*>::iterator end = pendingDeleteBackingStoreSet.end();
        for (HashSet<WebView*>::iterator it = pendingDeleteBackingStoreSet.begin(); it != end; ++it)
            views.append(*it);
        for (int i = 0; i < views.size(); ++i)
            views[i]->deleteBackingStore();
        ASSERT(pendingDeleteBackingStoreSet.isEmpty());
    }

    pendingDeleteBackingStoreSet.add(this);
    m_deleteBackingStoreTimerActive = true;
    SetTimer(m_viewWindow, DeleteBackingStoreTimer, delayBeforeDeletingBackingStoreMsec, 0);
}

void WebView::cancelDeleteBackingStoreSoon()
{
    if (!m_deleteBackingStoreTimerActive)
        return;
    pendingDeleteBackingStoreSet.remove(this);
    m_deleteBackingStoreTimerActive = false;
    KillTimer(m_viewWindow, DeleteBackingStoreTimer);
}

HRESULT STDMETHODCALLTYPE WebView::setHostWindow( 
    /* [in] */ OLE_HANDLE oleWindow)
{
    HWND window = (HWND)(ULONG64)oleWindow;
    if (m_viewWindow) {
        if (window)
            SetParent(m_viewWindow, window);
        else if (!isBeingDestroyed()) {
            // Turn the WebView into a message-only window so it will no longer be a child of the
            // old host window and will be hidden from screen. We only do this when
            // isBeingDestroyed() is false because doing this while handling WM_DESTROY can leave
            // m_viewWindow in a weird state (see <http://webkit.org/b/29337>).
            SetParent(m_viewWindow, HWND_MESSAGE);
        }
    }

    m_hostWindow = window;

    windowAncestryDidChange();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::hostWindow( 
    /* [retval][out] */ OLE_HANDLE* window)
{
    *window = (OLE_HANDLE)(ULONG64)m_hostWindow;
    return S_OK;
}


static Frame *incrementFrame(Frame *curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

HRESULT STDMETHODCALLTYPE WebView::searchFor( 
    /* [in] */ BSTR str,
    /* [in] */ BOOL forward,
    /* [in] */ BOOL caseFlag,
    /* [in] */ BOOL wrapFlag,
    /* [retval][out] */ BOOL* found)
{
    if (!found)
        return E_INVALIDARG;
    
    if (!m_page || !m_page->mainFrame())
        return E_UNEXPECTED;

    if (!str || !SysStringLen(str))
        return E_INVALIDARG;

    *found = m_page->findString(String(str, SysStringLen(str)), caseFlag ? TextCaseSensitive : TextCaseInsensitive, forward ? FindDirectionForward : FindDirectionBackward, wrapFlag);
    return S_OK;
}

bool WebView::active()
{
    HWND activeWindow = GetActiveWindow();
    return (activeWindow && m_topLevelParent == findTopLevelParent(activeWindow));
}

void WebView::updateActiveState()
{
    m_page->focusController()->setActive(active());
}

HRESULT STDMETHODCALLTYPE WebView::updateFocusedAndActiveState()
{
    updateActiveState();

    bool active = m_page->focusController()->isActive();
    Frame* mainFrame = m_page->mainFrame();
    Frame* focusedFrame = m_page->focusController()->focusedOrMainFrame();
    mainFrame->selection()->setFocused(active && mainFrame == focusedFrame);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::executeCoreCommandByName(BSTR bName, BSTR bValue)
{
    String name(bName, SysStringLen(bName));
    String value(bValue, SysStringLen(bValue));

    m_page->focusController()->focusedOrMainFrame()->editor()->command(name).execute(value);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::clearMainFrameName()
{
    m_page->mainFrame()->tree()->clearName();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::markAllMatchesForText(
    BSTR str, BOOL caseSensitive, BOOL highlight, UINT limit, UINT* matches)
{
    if (!matches)
        return E_INVALIDARG;

    if (!m_page || !m_page->mainFrame())
        return E_UNEXPECTED;

    if (!str || !SysStringLen(str))
        return E_INVALIDARG;

    *matches = m_page->markAllMatchesForText(String(str, SysStringLen(str)), caseSensitive ? TextCaseSensitive : TextCaseInsensitive, highlight, limit);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::unmarkAllTextMatches()
{
    if (!m_page || !m_page->mainFrame())
        return E_UNEXPECTED;

    m_page->unmarkAllTextMatches();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::rectsForTextMatches(
    IEnumTextMatches** pmatches)
{
    Vector<IntRect> allRects;
    WebCore::Frame* frame = m_page->mainFrame();
    do {
        if (Document* document = frame->document()) {
            IntRect visibleRect = frame->view()->visibleContentRect();
            Vector<IntRect> frameRects = document->markers()->renderedRectsForMarkers(DocumentMarker::TextMatch);
            IntPoint frameOffset(-frame->view()->scrollOffset().width(), -frame->view()->scrollOffset().height());
            frameOffset = frame->view()->convertToContainingWindow(frameOffset);

            Vector<IntRect>::iterator end = frameRects.end();
            for (Vector<IntRect>::iterator it = frameRects.begin(); it < end; it++) {
                it->intersect(visibleRect);
                it->move(frameOffset.x(), frameOffset.y());
                allRects.append(*it);
            }
        }
        frame = incrementFrame(frame, true, false);
    } while (frame);

    return createMatchEnumerator(&allRects, pmatches);
}

HRESULT STDMETHODCALLTYPE WebView::generateSelectionImage(BOOL forceWhiteText, OLE_HANDLE* hBitmap)
{
    *hBitmap = 0;

    WebCore::Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        HBITMAP bitmap = imageFromSelection(frame, forceWhiteText ? TRUE : FALSE);
        *hBitmap = (OLE_HANDLE)(ULONG64)bitmap;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::selectionRect(RECT* rc)
{
    WebCore::Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        IntRect ir = enclosingIntRect(frame->selection()->bounds());
        ir = frame->view()->convertToContainingWindow(ir);
        ir.move(-frame->view()->scrollOffset().width(), -frame->view()->scrollOffset().height());
        rc->left = ir.x();
        rc->top = ir.y();
        rc->bottom = rc->top + ir.height();
        rc->right = rc->left + ir.width();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::registerViewClass( 
    /* [in] */ IWebDocumentView* /*view*/,
    /* [in] */ IWebDocumentRepresentation* /*representation*/,
    /* [in] */ BSTR /*forMIMEType*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setGroupName( 
        /* [in] */ BSTR groupName)
{
    if (!m_page)
        return S_OK;
    m_page->setGroupName(String(groupName, SysStringLen(groupName)));
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::groupName( 
        /* [retval][out] */ BSTR* groupName)
{
    *groupName = 0;
    if (!m_page)
        return S_OK;
    String groupNameString = m_page->groupName();
    *groupName = SysAllocStringLen(groupNameString.characters(), groupNameString.length());
    if (!*groupName && groupNameString.length())
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::estimatedProgress( 
        /* [retval][out] */ double* estimatedProgress)
{
    *estimatedProgress = m_page->progress()->estimatedProgress();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::isLoading( 
        /* [retval][out] */ BOOL* isLoading)
{
    COMPtr<IWebDataSource> dataSource;
    COMPtr<IWebDataSource> provisionalDataSource;

    if (!isLoading)
        return E_POINTER;

    *isLoading = FALSE;

    if (!m_mainFrame)
        return E_FAIL;

    if (SUCCEEDED(m_mainFrame->dataSource(&dataSource)))
        dataSource->isLoading(isLoading);

    if (*isLoading)
        return S_OK;

    if (SUCCEEDED(m_mainFrame->provisionalDataSource(&provisionalDataSource)))
        provisionalDataSource->isLoading(isLoading);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::elementAtPoint( 
        /* [in] */ LPPOINT point,
        /* [retval][out] */ IPropertyBag** elementDictionary)
{
    if (!elementDictionary) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *elementDictionary = 0;

    Frame* frame = core(m_mainFrame);
    if (!frame)
        return E_FAIL;

    IntPoint webCorePoint = IntPoint(point->x, point->y);
    HitTestResult result = HitTestResult(webCorePoint);
    if (frame->contentRenderer())
        result = frame->eventHandler()->hitTestResultAtPoint(webCorePoint, false);
    *elementDictionary = WebElementPropertyBag::createInstance(result);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteboardTypesForSelection( 
    /* [retval][out] */ IEnumVARIANT** /*enumVariant*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::writeSelectionWithPasteboardTypes( 
        /* [size_is][in] */ BSTR* /*types*/,
        /* [in] */ int /*cTypes*/,
        /* [in] */ IDataObject* /*pasteboard*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteboardTypesForElement( 
    /* [in] */ IPropertyBag* /*elementDictionary*/,
    /* [retval][out] */ IEnumVARIANT** /*enumVariant*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::writeElement( 
        /* [in] */ IPropertyBag* /*elementDictionary*/,
        /* [size_is][in] */ BSTR* /*withPasteboardTypes*/,
        /* [in] */ int /*cWithPasteboardTypes*/,
        /* [in] */ IDataObject* /*pasteboard*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectedText(
        /* [out, retval] */ BSTR* text)
{
    if (!text) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *text = 0;

    Frame* focusedFrame = (m_page && m_page->focusController()) ? m_page->focusController()->focusedOrMainFrame() : 0;
    if (!focusedFrame)
        return E_FAIL;

    String frameSelectedText = focusedFrame->editor()->selectedText();
    *text = SysAllocStringLen(frameSelectedText.characters(), frameSelectedText.length());
    if (!*text && frameSelectedText.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::centerSelectionInVisibleArea(
        /* [in] */ IUnknown* /* sender */)
{
    Frame* coreFrame = core(m_mainFrame);
    if (!coreFrame)
        return E_FAIL;

    coreFrame->selection()->revealSelection(ScrollAlignment::alignCenterAlways);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE WebView::moveDragCaretToPoint( 
        /* [in] */ LPPOINT /*point*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::removeDragCaret( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setDrawsBackground( 
        /* [in] */ BOOL /*drawsBackground*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::drawsBackground( 
        /* [retval][out] */ BOOL* /*drawsBackground*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setMainFrameURL( 
        /* [in] */ BSTR /*urlString*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameURL( 
        /* [retval][out] */ BSTR* urlString)
{
    if (!urlString)
        return E_POINTER;

    if (!m_mainFrame)
        return E_FAIL;

    COMPtr<IWebDataSource> dataSource;

    if (FAILED(m_mainFrame->provisionalDataSource(&dataSource))) {
        if (FAILED(m_mainFrame->dataSource(&dataSource)))
            return E_FAIL;
    }

    if (!dataSource) {
        *urlString = 0;
        return S_OK;
    }
    
    COMPtr<IWebMutableURLRequest> request;
    if (FAILED(dataSource->request(&request)) || !request)
        return E_FAIL;

    if (FAILED(request->URL(urlString)))
        return E_FAIL;

    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameDocument( 
        /* [retval][out] */ IDOMDocument** document)
{
    if (document)
        *document = 0;
    if (!m_mainFrame)
        return E_FAIL;
    return m_mainFrame->DOMDocument(document);
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameTitle( 
        /* [retval][out] */ BSTR* /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameIcon( 
        /* [retval][out] */ OLE_HANDLE* /*hBitmap*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::registerURLSchemeAsLocal( 
        /* [in] */ BSTR scheme)
{
    if (!scheme)
        return E_POINTER;

    SchemeRegistry::registerURLSchemeAsLocal(String(scheme, ::SysStringLen(scheme)));

    return S_OK;
}

// IWebIBActions ---------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::takeStringURLFrom( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::stopLoading( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->stopLoading();
}
    
HRESULT STDMETHODCALLTYPE WebView::reload( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->reload();
}
    
HRESULT STDMETHODCALLTYPE WebView::canGoBack( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    *result = !!(m_page->backForwardList()->backItem() && !m_page->defersLoading());
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::goBack( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::canGoForward( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    *result = !!(m_page->backForwardList()->forwardItem() && !m_page->defersLoading());
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::goForward( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// FIXME: This code should move into WebCore so it can be shared by all the WebKits.
#define MinimumZoomMultiplier   0.5f
#define MaximumZoomMultiplier   3.0f
#define ZoomMultiplierRatio     1.2f

HRESULT STDMETHODCALLTYPE WebView::canMakeTextLarger( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    bool canGrowMore = canZoomIn(m_zoomsTextOnly);
    *result = canGrowMore ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canZoomPageIn( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    bool canGrowMore = canZoomIn(false);
    *result = canGrowMore ? TRUE : FALSE;
    return S_OK;
}

bool WebView::canZoomIn(bool isTextOnly)
{
    return zoomMultiplier(isTextOnly) * ZoomMultiplierRatio < MaximumZoomMultiplier;
}
    
HRESULT STDMETHODCALLTYPE WebView::makeTextLarger( 
        /* [in] */ IUnknown* /*sender*/)
{
    return zoomIn(m_zoomsTextOnly);
}

HRESULT STDMETHODCALLTYPE WebView::zoomPageIn( 
        /* [in] */ IUnknown* /*sender*/)
{
    return zoomIn(false);
}

HRESULT WebView::zoomIn(bool isTextOnly)
{
    if (!canZoomIn(isTextOnly))
        return E_FAIL;
    setZoomMultiplier(zoomMultiplier(isTextOnly) * ZoomMultiplierRatio, isTextOnly);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canMakeTextSmaller( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    bool canShrinkMore = canZoomOut(m_zoomsTextOnly);
    *result = canShrinkMore ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canZoomPageOut( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    bool canShrinkMore = canZoomOut(false);
    *result = canShrinkMore ? TRUE : FALSE;
    return S_OK;
}

bool WebView::canZoomOut(bool isTextOnly)
{
    return zoomMultiplier(isTextOnly) / ZoomMultiplierRatio > MinimumZoomMultiplier;
}

HRESULT STDMETHODCALLTYPE WebView::makeTextSmaller( 
        /* [in] */ IUnknown* /*sender*/)
{
    return zoomOut(m_zoomsTextOnly);
}

HRESULT STDMETHODCALLTYPE WebView::zoomPageOut( 
        /* [in] */ IUnknown* /*sender*/)
{
    return zoomOut(false);
}

HRESULT WebView::zoomOut(bool isTextOnly)
{
    if (!canZoomOut(isTextOnly))
        return E_FAIL;
    setZoomMultiplier(zoomMultiplier(isTextOnly) / ZoomMultiplierRatio, isTextOnly);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canMakeTextStandardSize( 
    /* [in] */ IUnknown* /*sender*/,
    /* [retval][out] */ BOOL* result)
{
    // Since we always reset text zoom and page zoom together, this should continue to return an answer about text zoom even if its not enabled.
    bool notAlreadyStandard = canResetZoom(true);
    *result = notAlreadyStandard ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canResetPageZoom( 
    /* [in] */ IUnknown* /*sender*/,
    /* [retval][out] */ BOOL* result)
{
    bool notAlreadyStandard = canResetZoom(false);
    *result = notAlreadyStandard ? TRUE : FALSE;
    return S_OK;
}

bool WebView::canResetZoom(bool isTextOnly)
{
    return zoomMultiplier(isTextOnly) != 1.0f;
}

HRESULT STDMETHODCALLTYPE WebView::makeTextStandardSize( 
    /* [in] */ IUnknown* /*sender*/)
{
    return resetZoom(true);
}

HRESULT STDMETHODCALLTYPE WebView::resetPageZoom( 
    /* [in] */ IUnknown* /*sender*/)
{
    return resetZoom(false);
}

HRESULT WebView::resetZoom(bool isTextOnly)
{
    if (!canResetZoom(isTextOnly))
        return E_FAIL;
    setZoomMultiplier(1.0f, isTextOnly);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::toggleContinuousSpellChecking( 
    /* [in] */ IUnknown* /*sender*/)
{
    HRESULT hr;
    BOOL enabled;
    if (FAILED(hr = isContinuousSpellCheckingEnabled(&enabled)))
        return hr;
    return setContinuousSpellCheckingEnabled(enabled ? FALSE : TRUE);
}

HRESULT STDMETHODCALLTYPE WebView::toggleSmartInsertDelete( 
    /* [in] */ IUnknown* /*sender*/)
{
    BOOL enabled = FALSE;
    HRESULT hr = smartInsertDeleteEnabled(&enabled);
    if (FAILED(hr))
        return hr;

    return setSmartInsertDeleteEnabled(enabled ? FALSE : TRUE);
}

HRESULT STDMETHODCALLTYPE WebView::toggleGrammarChecking( 
    /* [in] */ IUnknown* /*sender*/)
{
    BOOL enabled;
    HRESULT hr = isGrammarCheckingEnabled(&enabled);
    if (FAILED(hr))
        return hr;

    return setGrammarCheckingEnabled(enabled ? FALSE : TRUE);
}

HRESULT STDMETHODCALLTYPE WebView::reloadFromOrigin( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->reloadFromOrigin();
}

// IWebViewCSS -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::computedStyleForElement( 
        /* [in] */ IDOMElement* /*element*/,
        /* [in] */ BSTR /*pseudoElement*/,
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebViewEditing -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::editableDOMRangeForPoint( 
        /* [in] */ LPPOINT /*point*/,
        /* [retval][out] */ IDOMRange** /*range*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setSelectedDOMRange( 
        /* [in] */ IDOMRange* /*range*/,
        /* [in] */ WebSelectionAffinity /*affinity*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectedDOMRange( 
        /* [retval][out] */ IDOMRange** /*range*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectionAffinity( 
        /* [retval][out][retval][out] */ WebSelectionAffinity* /*affinity*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setEditable( 
        /* [in] */ BOOL /*flag*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::isEditable( 
        /* [retval][out] */ BOOL* /*isEditable*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setTypingStyle( 
        /* [in] */ IDOMCSSStyleDeclaration* /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::typingStyle( 
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setSmartInsertDeleteEnabled( 
        /* [in] */ BOOL flag)
{
    m_smartInsertDeleteEnabled = !!flag;
    if (m_smartInsertDeleteEnabled)
        setSelectTrailingWhitespaceEnabled(false);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::smartInsertDeleteEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    *enabled = m_smartInsertDeleteEnabled ? TRUE : FALSE;
    return S_OK;
}
 
HRESULT STDMETHODCALLTYPE WebView::setSelectTrailingWhitespaceEnabled( 
        /* [in] */ BOOL flag)
{
    m_selectTrailingWhitespaceEnabled = !!flag;
    if (m_selectTrailingWhitespaceEnabled)
        setSmartInsertDeleteEnabled(false);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::isSelectTrailingWhitespaceEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    *enabled = m_selectTrailingWhitespaceEnabled ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setContinuousSpellCheckingEnabled( 
        /* [in] */ BOOL flag)
{
    if (continuousSpellCheckingEnabled != !!flag) {
        continuousSpellCheckingEnabled = !!flag;
        COMPtr<IWebPreferences> prefs;
        if (SUCCEEDED(preferences(&prefs)))
            prefs->setContinuousSpellCheckingEnabled(flag);
    }
    
    BOOL spellCheckingEnabled;
    if (SUCCEEDED(isContinuousSpellCheckingEnabled(&spellCheckingEnabled)) && spellCheckingEnabled)
        preflightSpellChecker();
    else
        m_mainFrame->unmarkAllMisspellings();

    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::isContinuousSpellCheckingEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    *enabled = (continuousSpellCheckingEnabled && continuousCheckingAllowed()) ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::spellCheckerDocumentTag( 
        /* [retval][out] */ int* tag)
{
    // we just use this as a flag to indicate that we've spell checked the document
    // and need to close the spell checker out when the view closes.
    *tag = 0;
    m_hasSpellCheckerDocumentTag = true;
    return S_OK;
}

static COMPtr<IWebEditingDelegate> spellingDelegateForTimer;

static void preflightSpellCheckerNow()
{
    spellingDelegateForTimer->preflightChosenSpellServer();
    spellingDelegateForTimer = 0;
}

static void CALLBACK preflightSpellCheckerTimerCallback(HWND, UINT, UINT_PTR id, DWORD)
{
    ::KillTimer(0, id);
    preflightSpellCheckerNow();
}

void WebView::preflightSpellChecker()
{
    // As AppKit does, we wish to delay tickling the shared spellchecker into existence on application launch.
    if (!m_editingDelegate)
        return;

    BOOL exists;
    spellingDelegateForTimer = m_editingDelegate;
    if (SUCCEEDED(m_editingDelegate->sharedSpellCheckerExists(&exists)) && exists)
        preflightSpellCheckerNow();
    else
        ::SetTimer(0, 0, 2000, preflightSpellCheckerTimerCallback);
}

bool WebView::continuousCheckingAllowed()
{
    static bool allowContinuousSpellChecking = true;
    static bool readAllowContinuousSpellCheckingDefault = false;
    if (!readAllowContinuousSpellCheckingDefault) {
        COMPtr<IWebPreferences> prefs;
        if (SUCCEEDED(preferences(&prefs))) {
            BOOL allowed;
            prefs->allowContinuousSpellChecking(&allowed);
            allowContinuousSpellChecking = !!allowed;
        }
        readAllowContinuousSpellCheckingDefault = true;
    }
    return allowContinuousSpellChecking;
}

HRESULT STDMETHODCALLTYPE WebView::undoManager( 
        /* [retval][out] */ IWebUndoManager** /*manager*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setEditingDelegate( 
        /* [in] */ IWebEditingDelegate* d)
{
    m_editingDelegate = d;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::editingDelegate( 
        /* [retval][out] */ IWebEditingDelegate** d)
{
    if (!d) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *d = m_editingDelegate.get();
    if (!*d)
        return E_FAIL;

    (*d)->AddRef();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::styleDeclarationWithText( 
        /* [in] */ BSTR /*text*/,
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::hasSelectedRange( 
        /* [retval][out] */ BOOL* hasSelectedRange)
{
    *hasSelectedRange = m_page->mainFrame()->selection()->isRange();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::cutEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    Editor* editor = m_page->focusController()->focusedOrMainFrame()->editor();
    *enabled = editor->canCut() || editor->canDHTMLCut();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::copyEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    Editor* editor = m_page->focusController()->focusedOrMainFrame()->editor();
    *enabled = editor->canCopy() || editor->canDHTMLCopy();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    Editor* editor = m_page->focusController()->focusedOrMainFrame()->editor();
    *enabled = editor->canPaste() || editor->canDHTMLPaste();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::deleteEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    *enabled = m_page->focusController()->focusedOrMainFrame()->editor()->canDelete();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::editingEnabled( 
        /* [retval][out] */ BOOL* enabled)
{
    *enabled = m_page->focusController()->focusedOrMainFrame()->editor()->canEdit();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::isGrammarCheckingEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = grammarCheckingEnabled ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setGrammarCheckingEnabled( 
    BOOL enabled)
{
    if (!m_editingDelegate) {
        LOG_ERROR("No NSSpellChecker");
        return E_FAIL;
    }

    if (grammarCheckingEnabled == !!enabled)
        return S_OK;
    
    grammarCheckingEnabled = !!enabled;
    COMPtr<IWebPreferences> prefs;
    if (SUCCEEDED(preferences(&prefs)))
        prefs->setGrammarCheckingEnabled(enabled);
    
    m_editingDelegate->updateGrammar();

    // We call _preflightSpellChecker when turning continuous spell checking on, but we don't need to do that here
    // because grammar checking only occurs on code paths that already preflight spell checking appropriately.
    
    BOOL grammarEnabled;
    if (SUCCEEDED(isGrammarCheckingEnabled(&grammarEnabled)) && !grammarEnabled)
        m_mainFrame->unmarkAllBadGrammar();

    return S_OK;
}

// IWebViewUndoableEditing -----------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithNode( 
        /* [in] */ IDOMNode* /*node*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithText( 
        /* [in] */ BSTR text)
{
    String textString(text, ::SysStringLen(text));
    Position start = m_page->mainFrame()->selection()->selection().start();
    m_page->focusController()->focusedOrMainFrame()->editor()->insertText(textString, 0);
    m_page->mainFrame()->selection()->setBase(start);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithMarkupString( 
        /* [in] */ BSTR /*markupString*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithArchive( 
        /* [in] */ IWebArchive* /*archive*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::deleteSelection( void)
{
    Editor* editor = m_page->focusController()->focusedOrMainFrame()->editor();
    editor->deleteSelectionWithSmartDelete(editor->canSmartCopyOrDelete());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::clearSelection( void)
{
    m_page->focusController()->focusedOrMainFrame()->selection()->clear();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::applyStyle( 
        /* [in] */ IDOMCSSStyleDeclaration* /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebViewEditingActions ------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::copy( 
        /* [in] */ IUnknown* /*sender*/)
{
    m_page->focusController()->focusedOrMainFrame()->editor()->command("Copy").execute();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::cut( 
        /* [in] */ IUnknown* /*sender*/)
{
    m_page->focusController()->focusedOrMainFrame()->editor()->command("Cut").execute();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::paste( 
        /* [in] */ IUnknown* /*sender*/)
{
    m_page->focusController()->focusedOrMainFrame()->editor()->command("Paste").execute();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::copyURL( 
        /* [in] */ BSTR url)
{
    m_page->focusController()->focusedOrMainFrame()->editor()->copyURL(MarshallingHelpers::BSTRToKURL(url), "");
    return S_OK;
}


HRESULT STDMETHODCALLTYPE WebView::copyFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::delete_( 
        /* [in] */ IUnknown* /*sender*/)
{
    m_page->focusController()->focusedOrMainFrame()->editor()->command("Delete").execute();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteAsPlainText( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteAsRichText( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeAttributes( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeDocumentBackgroundColor( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeColor( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignCenter( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignJustified( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignLeft( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignRight( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::checkSpelling( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_editingDelegate) {
        LOG_ERROR("No NSSpellChecker");
        return E_FAIL;
    }
    
    core(m_mainFrame)->editor()->advanceToNextMisspelling();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::showGuessPanel( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_editingDelegate) {
        LOG_ERROR("No NSSpellChecker");
        return E_FAIL;
    }
    
    // Post-Tiger, this menu item is a show/hide toggle, to match AppKit. Leave Tiger behavior alone
    // to match rest of OS X.
    BOOL showing;
    if (SUCCEEDED(m_editingDelegate->spellingUIIsShowing(&showing)) && showing) {
        m_editingDelegate->showSpellingUI(FALSE);
    }
    
    core(m_mainFrame)->editor()->advanceToNextMisspelling(true);
    m_editingDelegate->showSpellingUI(TRUE);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::performFindPanelAction( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::startSpeaking( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::stopSpeaking( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebNotificationObserver -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::onNotify( 
    /* [in] */ IWebNotification* notification)
{
    BSTR nameBSTR;
    HRESULT hr = notification->name(&nameBSTR);
    if (FAILED(hr))
        return hr;

    BString name;
    name.adoptBSTR(nameBSTR);

    if (!wcscmp(name, WebIconDatabase::iconDatabaseDidAddIconNotification()))
        return notifyDidAddIcon(notification);

    if (!wcscmp(name, WebPreferences::webPreferencesChangedNotification()))
        return notifyPreferencesChanged(notification);

    return hr;
}

HRESULT WebView::notifyPreferencesChanged(IWebNotification* notification)
{
    HRESULT hr;

    COMPtr<IUnknown> unkPrefs;
    hr = notification->getObject(&unkPrefs);
    if (FAILED(hr))
        return hr;

    COMPtr<IWebPreferences> preferences(Query, unkPrefs);
    if (!preferences)
        return E_NOINTERFACE;

    ASSERT(preferences == m_preferences);

    BSTR str;
    int size;
    BOOL enabled;

    Settings* settings = m_page->settings();

    hr = preferences->cursiveFontFamily(&str);
    if (FAILED(hr))
        return hr;
    settings->setCursiveFontFamily(AtomicString(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->defaultFixedFontSize(&size);
    if (FAILED(hr))
        return hr;
    settings->setDefaultFixedFontSize(size);

    hr = preferences->defaultFontSize(&size);
    if (FAILED(hr))
        return hr;
    settings->setDefaultFontSize(size);
    
    hr = preferences->defaultTextEncodingName(&str);
    if (FAILED(hr))
        return hr;
    settings->setDefaultTextEncodingName(String(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->fantasyFontFamily(&str);
    if (FAILED(hr))
        return hr;
    settings->setFantasyFontFamily(AtomicString(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->fixedFontFamily(&str);
    if (FAILED(hr))
        return hr;
    settings->setFixedFontFamily(AtomicString(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->isJavaEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setJavaEnabled(!!enabled);

    hr = preferences->isJavaScriptEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setJavaScriptEnabled(!!enabled);

    hr = preferences->javaScriptCanOpenWindowsAutomatically(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setJavaScriptCanOpenWindowsAutomatically(!!enabled);

    hr = preferences->minimumFontSize(&size);
    if (FAILED(hr))
        return hr;
    settings->setMinimumFontSize(size);

    hr = preferences->minimumLogicalFontSize(&size);
    if (FAILED(hr))
        return hr;
    settings->setMinimumLogicalFontSize(size);

    hr = preferences->arePlugInsEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setPluginsEnabled(!!enabled);

    hr = preferences->privateBrowsingEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setPrivateBrowsingEnabled(!!enabled);

    hr = preferences->sansSerifFontFamily(&str);
    if (FAILED(hr))
        return hr;
    settings->setSansSerifFontFamily(AtomicString(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->serifFontFamily(&str);
    if (FAILED(hr))
        return hr;
    settings->setSerifFontFamily(AtomicString(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->standardFontFamily(&str);
    if (FAILED(hr))
        return hr;
    settings->setStandardFontFamily(AtomicString(str, SysStringLen(str)));
    SysFreeString(str);

    hr = preferences->loadsImagesAutomatically(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setLoadsImagesAutomatically(!!enabled);

    hr = preferences->userStyleSheetEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    if (enabled) {
        hr = preferences->userStyleSheetLocation(&str);
        if (FAILED(hr))
            return hr;

        RetainPtr<CFStringRef> urlString(AdoptCF, String(str, SysStringLen(str)).createCFString());
        RetainPtr<CFURLRef> url(AdoptCF, CFURLCreateWithString(kCFAllocatorDefault, urlString.get(), 0));

        // Check if the passed in string is a path and convert it to a URL.
        // FIXME: This is a workaround for nightly builds until we can get Safari to pass 
        // in an URL here. See <rdar://problem/5478378>
        if (!url) {
            DWORD len = SysStringLen(str) + 1;

            int result = WideCharToMultiByte(CP_UTF8, 0, str, len, 0, 0, 0, 0);
            Vector<UInt8> utf8Path(result);
            if (!WideCharToMultiByte(CP_UTF8, 0, str, len, (LPSTR)utf8Path.data(), result, 0, 0))
                return E_FAIL;

            url.adoptCF(CFURLCreateFromFileSystemRepresentation(0, utf8Path.data(), result - 1, false));
        }

        settings->setUserStyleSheetLocation(url.get());
        SysFreeString(str);
    } else {
        settings->setUserStyleSheetLocation(KURL());
    }

    hr = preferences->shouldPrintBackgrounds(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setShouldPrintBackgrounds(!!enabled);

    hr = preferences->textAreasAreResizable(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setTextAreasAreResizable(!!enabled);

    WebKitEditableLinkBehavior behavior;
    hr = preferences->editableLinkBehavior(&behavior);
    if (FAILED(hr))
        return hr;
    settings->setEditableLinkBehavior((EditableLinkBehavior)behavior);

    WebKitEditingBehavior editingBehavior;
    hr = preferences->editingBehavior(&editingBehavior);
    if (FAILED(hr))
        return hr;
    settings->setEditingBehaviorType((EditingBehaviorType)editingBehavior);

    hr = preferences->usesPageCache(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setUsesPageCache(!!enabled);

    hr = preferences->isDOMPasteAllowed(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setDOMPasteAllowed(!!enabled);

    hr = preferences->shouldPaintCustomScrollbars(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setShouldPaintCustomScrollbars(!!enabled);

    hr = preferences->zoomsTextOnly(&enabled);
    if (FAILED(hr))
        return hr;

    if (m_zoomsTextOnly != !!enabled)
        setZoomMultiplier(m_zoomMultiplier, enabled);

    settings->setShowsURLsInToolTips(false);
    settings->setForceFTPDirectoryListings(true);
    settings->setDeveloperExtrasEnabled(developerExtrasEnabled());
    settings->setNeedsSiteSpecificQuirks(s_allowSiteSpecificHacks);

    FontSmoothingType smoothingType;
    hr = preferences->fontSmoothing(&smoothingType);
    if (FAILED(hr))
        return hr;
    settings->setFontRenderingMode(smoothingType != FontSmoothingTypeWindows ? NormalRenderingMode : AlternateRenderingMode);

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (prefsPrivate) {
        hr = prefsPrivate->authorAndUserStylesEnabled(&enabled);
        if (FAILED(hr))
            return hr;
        settings->setAuthorAndUserStylesEnabled(enabled);
    }

    hr = prefsPrivate->inApplicationChromeMode(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setApplicationChromeMode(enabled);

    hr = prefsPrivate->offlineWebApplicationCacheEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setOfflineWebApplicationCacheEnabled(enabled);

#if ENABLE(DATABASE)
    hr = prefsPrivate->databasesEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    AbstractDatabase::setIsAvailable(enabled);
#endif

    hr = prefsPrivate->localStorageEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setLocalStorageEnabled(enabled);

    hr = prefsPrivate->experimentalNotificationsEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setExperimentalNotificationsEnabled(enabled);

    hr = prefsPrivate->isWebSecurityEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setWebSecurityEnabled(!!enabled);

    hr = prefsPrivate->allowUniversalAccessFromFileURLs(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setAllowUniversalAccessFromFileURLs(!!enabled);

    hr = prefsPrivate->allowFileAccessFromFileURLs(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setAllowFileAccessFromFileURLs(!!enabled);

    hr = prefsPrivate->javaScriptCanAccessClipboard(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setJavaScriptCanAccessClipboard(!!enabled);

    hr = prefsPrivate->isXSSAuditorEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setXSSAuditorEnabled(!!enabled);

#if USE(SAFARI_THEME)
    hr = prefsPrivate->shouldPaintNativeControls(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setShouldPaintNativeControls(!!enabled);
#endif

    hr = prefsPrivate->shouldUseHighResolutionTimers(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setShouldUseHighResolutionTimers(enabled);

    UINT runTime;
    hr = prefsPrivate->pluginAllowedRunTime(&runTime);
    if (FAILED(hr))
        return hr;
    settings->setPluginAllowedRunTime(runTime);

    hr = prefsPrivate->isFrameFlatteningEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setFrameFlatteningEnabled(enabled);

#if USE(ACCELERATED_COMPOSITING)
    hr = prefsPrivate->acceleratedCompositingEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setAcceleratedCompositingEnabled(enabled);
#endif

    hr = prefsPrivate->showDebugBorders(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setShowDebugBorders(enabled);

    hr = prefsPrivate->showRepaintCounter(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setShowRepaintCounter(enabled);

#if ENABLE(3D_CANVAS)
    settings->setWebGLEnabled(true);
#endif  // ENABLE(3D_CANVAS)

    hr = prefsPrivate->isDNSPrefetchingEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setDNSPrefetchingEnabled(enabled);

    hr = prefsPrivate->memoryInfoEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    settings->setMemoryInfoEnabled(enabled);

    if (!m_closeWindowTimer)
        m_mainFrame->invalidate(); // FIXME

    hr = updateSharedSettingsFromPreferencesIfNeeded(preferences.get());
    if (FAILED(hr))
        return hr;

    return S_OK;
}

HRESULT updateSharedSettingsFromPreferencesIfNeeded(IWebPreferences* preferences)
{
    if (preferences != WebPreferences::sharedStandardPreferences())
        return S_OK;

    WebKitCookieStorageAcceptPolicy acceptPolicy;
    HRESULT hr = preferences->cookieStorageAcceptPolicy(&acceptPolicy);
    if (FAILED(hr))
        return hr;

#if USE(CFNETWORK)
    // Set cookie storage accept policy
    if (CFHTTPCookieStorageRef cookieStorage = currentCookieStorage())
        CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage, acceptPolicy);
#endif

    return S_OK;
}

// IWebViewPrivate ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::MIMETypeForExtension(
    /* [in] */ BSTR extension,
    /* [retval][out] */ BSTR* mimeType)
{
    if (!mimeType)
        return E_POINTER;

    String extensionStr(extension, SysStringLen(extension));

    *mimeType = BString(MIMETypeRegistry::getMIMETypeForExtension(extensionStr)).release();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomDropTarget(
    /* [in] */ IDropTarget* dt)
{
    ASSERT(::IsWindow(m_viewWindow));
    if (!dt)
        return E_POINTER;
    m_hasCustomDropTarget = true;
    revokeDragDrop();
    return ::RegisterDragDrop(m_viewWindow,dt);
}

HRESULT STDMETHODCALLTYPE WebView::removeCustomDropTarget()
{
    if (!m_hasCustomDropTarget)
        return S_OK;
    m_hasCustomDropTarget = false;
    revokeDragDrop();
    return registerDragDrop();
}

HRESULT STDMETHODCALLTYPE WebView::setInViewSourceMode( 
        /* [in] */ BOOL flag)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->setInViewSourceMode(flag);
}
    
HRESULT STDMETHODCALLTYPE WebView::inViewSourceMode( 
        /* [retval][out] */ BOOL* flag)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->inViewSourceMode(flag);
}

HRESULT STDMETHODCALLTYPE WebView::viewWindow( 
        /* [retval][out] */ OLE_HANDLE *window)
{
    *window = (OLE_HANDLE)(ULONG64)m_viewWindow;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setFormDelegate( 
    /* [in] */ IWebFormDelegate *formDelegate)
{
    m_formDelegate = formDelegate;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::formDelegate( 
    /* [retval][out] */ IWebFormDelegate **formDelegate)
{
    if (!m_formDelegate)
        return E_FAIL;

    return m_formDelegate.copyRefTo(formDelegate);
}

HRESULT STDMETHODCALLTYPE WebView::setFrameLoadDelegatePrivate( 
    /* [in] */ IWebFrameLoadDelegatePrivate* d)
{
    m_frameLoadDelegatePrivate = d;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::frameLoadDelegatePrivate( 
    /* [out][retval] */ IWebFrameLoadDelegatePrivate** d)
{
    if (!m_frameLoadDelegatePrivate)
        return E_FAIL;
        
    return m_frameLoadDelegatePrivate.copyRefTo(d);
}

HRESULT STDMETHODCALLTYPE WebView::scrollOffset( 
    /* [retval][out] */ LPPOINT offset)
{
    if (!offset)
        return E_POINTER;
    IntSize offsetIntSize = m_page->mainFrame()->view()->scrollOffset();
    offset->x = offsetIntSize.width();
    offset->y = offsetIntSize.height();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::scrollBy( 
    /* [in] */ LPPOINT offset)
{
    if (!offset)
        return E_POINTER;
    m_page->mainFrame()->view()->scrollBy(IntSize(offset->x, offset->y));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::visibleContentRect( 
    /* [retval][out] */ LPRECT rect)
{
    if (!rect)
        return E_POINTER;
    FloatRect visibleContent = m_page->mainFrame()->view()->visibleContentRect();
    rect->left = (LONG) visibleContent.x();
    rect->top = (LONG) visibleContent.y();
    rect->right = (LONG) visibleContent.right();
    rect->bottom = (LONG) visibleContent.bottom();
    return S_OK;
}

static DWORD dragOperationToDragCursor(DragOperation op) {
    DWORD res = DROPEFFECT_NONE;
    if (op & DragOperationCopy) 
        res = DROPEFFECT_COPY;
    else if (op & DragOperationLink) 
        res = DROPEFFECT_LINK;
    else if (op & DragOperationMove) 
        res = DROPEFFECT_MOVE;
    else if (op & DragOperationGeneric) 
        res = DROPEFFECT_MOVE; //This appears to be the Firefox behaviour
    return res;
}

DragOperation WebView::keyStateToDragOperation(DWORD grfKeyState) const
{
    if (!m_page)
        return DragOperationNone;

    // Conforms to Microsoft's key combinations as documented for 
    // IDropTarget::DragOver. Note, grfKeyState is the current 
    // state of the keyboard modifier keys on the keyboard. See:
    // <http://msdn.microsoft.com/en-us/library/ms680129(VS.85).aspx>.
    DragOperation operation = m_page->dragController()->sourceDragOperation();

    if ((grfKeyState & (MK_CONTROL | MK_SHIFT)) == (MK_CONTROL | MK_SHIFT))
        operation = DragOperationLink;
    else if ((grfKeyState & MK_CONTROL) == MK_CONTROL)
        operation = DragOperationCopy;
    else if ((grfKeyState & MK_SHIFT) == MK_SHIFT)
        operation = DragOperationGeneric;

    return operation;
}

HRESULT STDMETHODCALLTYPE WebView::DragEnter(
        IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    m_dragData = 0;

    if (m_dropTargetHelper)
        m_dropTargetHelper->DragEnter(m_viewWindow, pDataObject, (POINT*)&pt, *pdwEffect);

    POINTL localpt = pt;
    ::ScreenToClient(m_viewWindow, (LPPOINT)&localpt);
    DragData data(pDataObject, IntPoint(localpt.x, localpt.y), 
        IntPoint(pt.x, pt.y), keyStateToDragOperation(grfKeyState));
    *pdwEffect = dragOperationToDragCursor(m_page->dragController()->dragEntered(&data));

    m_lastDropEffect = *pdwEffect;
    m_dragData = pDataObject;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::DragOver(
        DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    if (m_dropTargetHelper)
        m_dropTargetHelper->DragOver((POINT*)&pt, *pdwEffect);

    if (m_dragData) {
        POINTL localpt = pt;
        ::ScreenToClient(m_viewWindow, (LPPOINT)&localpt);
        DragData data(m_dragData.get(), IntPoint(localpt.x, localpt.y), 
            IntPoint(pt.x, pt.y), keyStateToDragOperation(grfKeyState));
        *pdwEffect = dragOperationToDragCursor(m_page->dragController()->dragUpdated(&data));
    } else
        *pdwEffect = DROPEFFECT_NONE;

    m_lastDropEffect = *pdwEffect;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::DragLeave()
{
    if (m_dropTargetHelper)
        m_dropTargetHelper->DragLeave();

    if (m_dragData) {
        DragData data(m_dragData.get(), IntPoint(), IntPoint(), 
            DragOperationNone);
        m_page->dragController()->dragExited(&data);
        m_dragData = 0;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::Drop(
        IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    if (m_dropTargetHelper)
        m_dropTargetHelper->Drop(pDataObject, (POINT*)&pt, *pdwEffect);

    m_dragData = 0;
    *pdwEffect = m_lastDropEffect;
    POINTL localpt = pt;
    ::ScreenToClient(m_viewWindow, (LPPOINT)&localpt);
    DragData data(pDataObject, IntPoint(localpt.x, localpt.y), 
        IntPoint(pt.x, pt.y), keyStateToDragOperation(grfKeyState));
    m_page->dragController()->performDrag(&data);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::canHandleRequest( 
    IWebURLRequest *request,
    BOOL *result)
{
    COMPtr<WebMutableURLRequest> requestImpl;

    HRESULT hr = request->QueryInterface(&requestImpl);
    if (FAILED(hr))
        return hr;

    *result = !!canHandleRequest(requestImpl->resourceRequest());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::standardUserAgentWithApplicationName( 
    BSTR applicationName,
    BSTR* groupName)
{
    if (!groupName) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *groupName;

    if (!applicationName) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    BString applicationNameBString(applicationName);
    *groupName = BString(standardUserAgentWithApplicationName(String(applicationNameBString, SysStringLen(applicationNameBString)))).release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::clearFocusNode()
{
    if (m_page && m_page->focusController())
        m_page->focusController()->setFocusedNode(0, 0);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setInitialFocus(
    /* [in] */ BOOL forward)
{
    if (m_page && m_page->focusController()) {
        Frame* frame = m_page->focusController()->focusedOrMainFrame();
        frame->document()->setFocusedNode(0);
        m_page->focusController()->setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, 0);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setTabKeyCyclesThroughElements( 
    /* [in] */ BOOL cycles)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(!!cycles);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::tabKeyCyclesThroughElements( 
    /* [retval][out] */ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = m_page && m_page->tabKeyCyclesThroughElements() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setAllowSiteSpecificHacks(
    /* [in] */ BOOL allow)
{
    s_allowSiteSpecificHacks = !!allow;
    // FIXME: This sets a global so it needs to call notifyPreferencesChanged
    // on all WebView objects (not just itself).
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::addAdditionalPluginDirectory( 
        /* [in] */ BSTR directory)
{
    PluginDatabase::installedPlugins()->addExtraPluginDirectory(String(directory, SysStringLen(directory)));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::loadBackForwardListFromOtherView( 
    /* [in] */ IWebView* otherView)
{
    if (!m_page)
        return E_FAIL;
    
    // It turns out the right combination of behavior is done with the back/forward load
    // type.  (See behavior matrix at the top of WebFramePrivate.)  So we copy all the items
    // in the back forward list, and go to the current one.
    BackForwardList* backForwardList = m_page->backForwardList();
    ASSERT(!backForwardList->currentItem()); // destination list should be empty

    COMPtr<WebView> otherWebView;
    if (FAILED(otherView->QueryInterface(&otherWebView)))
        return E_FAIL;
    BackForwardList* otherBackForwardList = otherWebView->m_page->backForwardList();
    if (!otherBackForwardList->currentItem())
        return S_OK; // empty back forward list, bail
    
    HistoryItem* newItemToGoTo = 0;

    int lastItemIndex = otherBackForwardList->forwardListCount();
    for (int i = -otherBackForwardList->backListCount(); i <= lastItemIndex; ++i) {
        if (!i) {
            // If this item is showing , save away its current scroll and form state,
            // since that might have changed since loading and it is normally not saved
            // until we leave that page.
            otherWebView->m_page->mainFrame()->loader()->history()->saveDocumentAndScrollState();
        }
        RefPtr<HistoryItem> newItem = otherBackForwardList->itemAtIndex(i)->copy();
        if (!i) 
            newItemToGoTo = newItem.get();
        backForwardList->addItem(newItem.release());
    }
    
    ASSERT(newItemToGoTo);
    m_page->goToItem(newItemToGoTo, FrameLoadTypeIndexedBackForward);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::clearUndoRedoOperations()
{
    if (Frame* frame = m_page->focusController()->focusedOrMainFrame())
        frame->editor()->clearUndoRedoOperations();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::shouldClose( 
    /* [retval][out] */ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = TRUE;
    if (Frame* frame = m_page->mainFrame())
        *result = frame->loader()->shouldClose();
    return S_OK;
}

HRESULT WebView::registerDragDrop()
{
    ASSERT(::IsWindow(m_viewWindow));
    return ::RegisterDragDrop(m_viewWindow, this);
}

HRESULT WebView::revokeDragDrop()
{
    if (!m_viewWindow)
        return S_OK;

    return ::RevokeDragDrop(m_viewWindow);
}

HRESULT WebView::setProhibitsMainFrameScrolling(BOOL b)
{
    if (!m_page)
        return E_FAIL;

    m_page->mainFrame()->view()->setProhibitsScrolling(b);
    return S_OK;
}

HRESULT WebView::setShouldApplyMacFontAscentHack(BOOL b)
{
    SimpleFontData::setShouldApplyMacAscentHack(b);
    return S_OK;
}

class IMMDict {
    typedef HIMC (CALLBACK *getContextPtr)(HWND);
    typedef BOOL (CALLBACK *releaseContextPtr)(HWND, HIMC);
    typedef LONG (CALLBACK *getCompositionStringPtr)(HIMC, DWORD, LPVOID, DWORD);
    typedef BOOL (CALLBACK *setCandidateWindowPtr)(HIMC, LPCANDIDATEFORM);
    typedef BOOL (CALLBACK *setOpenStatusPtr)(HIMC, BOOL);
    typedef BOOL (CALLBACK *notifyIMEPtr)(HIMC, DWORD, DWORD, DWORD);
    typedef BOOL (CALLBACK *associateContextExPtr)(HWND, HIMC, DWORD);

public:
    getContextPtr getContext;
    releaseContextPtr releaseContext;
    getCompositionStringPtr getCompositionString;
    setCandidateWindowPtr setCandidateWindow;
    setOpenStatusPtr setOpenStatus;
    notifyIMEPtr notifyIME;
    associateContextExPtr associateContextEx;

    static const IMMDict& dict();
private:
    IMMDict();
    HMODULE m_instance;
};

const IMMDict& IMMDict::dict()
{
    static IMMDict instance;
    return instance;
}

IMMDict::IMMDict()
{
    m_instance = ::LoadLibrary(TEXT("IMM32.DLL"));
    getContext = reinterpret_cast<getContextPtr>(::GetProcAddress(m_instance, "ImmGetContext"));
    ASSERT(getContext);
    releaseContext = reinterpret_cast<releaseContextPtr>(::GetProcAddress(m_instance, "ImmReleaseContext"));
    ASSERT(releaseContext);
    getCompositionString = reinterpret_cast<getCompositionStringPtr>(::GetProcAddress(m_instance, "ImmGetCompositionStringW"));
    ASSERT(getCompositionString);
    setCandidateWindow = reinterpret_cast<setCandidateWindowPtr>(::GetProcAddress(m_instance, "ImmSetCandidateWindow"));
    ASSERT(setCandidateWindow);
    setOpenStatus = reinterpret_cast<setOpenStatusPtr>(::GetProcAddress(m_instance, "ImmSetOpenStatus"));
    ASSERT(setOpenStatus);
    notifyIME = reinterpret_cast<notifyIMEPtr>(::GetProcAddress(m_instance, "ImmNotifyIME"));
    ASSERT(notifyIME);
    associateContextEx = reinterpret_cast<associateContextExPtr>(::GetProcAddress(m_instance, "ImmAssociateContextEx"));
    ASSERT(associateContextEx);
}

HIMC WebView::getIMMContext() 
{
    HIMC context = IMMDict::dict().getContext(m_viewWindow);
    return context;
}

void WebView::releaseIMMContext(HIMC hIMC)
{
    if (!hIMC)
        return;
    IMMDict::dict().releaseContext(m_viewWindow, hIMC);
}

void WebView::prepareCandidateWindow(Frame* targetFrame, HIMC hInputContext) 
{
    IntRect caret;
    if (RefPtr<Range> range = targetFrame->selection()->selection().toNormalizedRange()) {
        ExceptionCode ec = 0;
        RefPtr<Range> tempRange = range->cloneRange(ec);
        caret = targetFrame->editor()->firstRectForRange(tempRange.get());
    }
    caret = targetFrame->view()->contentsToWindow(caret);
    CANDIDATEFORM form;
    form.dwIndex = 0;
    form.dwStyle = CFS_EXCLUDE;
    form.ptCurrentPos.x = caret.x();
    form.ptCurrentPos.y = caret.y() + caret.height();
    form.rcArea.top = caret.y();
    form.rcArea.bottom = caret.bottom();
    form.rcArea.left = caret.x();
    form.rcArea.right = caret.right();
    IMMDict::dict().setCandidateWindow(hInputContext, &form);
}

void WebView::resetIME(Frame* targetFrame)
{
    if (targetFrame)
        targetFrame->editor()->confirmCompositionWithoutDisturbingSelection();

    if (HIMC hInputContext = getIMMContext()) {
        IMMDict::dict().notifyIME(hInputContext, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        releaseIMMContext(hInputContext);
    }
}

void WebView::updateSelectionForIME()
{
    Frame* targetFrame = m_page->focusController()->focusedOrMainFrame();
    if (!targetFrame || !targetFrame->editor()->hasComposition())
        return;
    
    if (targetFrame->editor()->ignoreCompositionSelectionChange())
        return;

    unsigned start;
    unsigned end;
    if (!targetFrame->editor()->getCompositionSelection(start, end))
        resetIME(targetFrame);
}

void WebView::setInputMethodState(bool enabled)
{
    IMMDict::dict().associateContextEx(m_viewWindow, 0, enabled ? IACE_DEFAULT : 0);
}

void WebView::selectionChanged()
{
    updateSelectionForIME();
}

bool WebView::onIMEStartComposition()
{
    LOG(TextInput, "onIMEStartComposition");
    m_inIMEComposition++;
    Frame* targetFrame = m_page->focusController()->focusedOrMainFrame();
    if (!targetFrame)
        return true;

    HIMC hInputContext = getIMMContext();
    prepareCandidateWindow(targetFrame, hInputContext);
    releaseIMMContext(hInputContext);
    return true;
}

static bool getCompositionString(HIMC hInputContext, DWORD type, String& result)
{
    int compositionLength = IMMDict::dict().getCompositionString(hInputContext, type, 0, 0);
    if (compositionLength <= 0)
        return false;
    Vector<UChar> compositionBuffer(compositionLength / 2);
    compositionLength = IMMDict::dict().getCompositionString(hInputContext, type, (LPVOID)compositionBuffer.data(), compositionLength);
    result = String(compositionBuffer.data(), compositionLength / 2);
    ASSERT(!compositionLength || compositionBuffer[0]);
    ASSERT(!compositionLength || compositionBuffer[compositionLength / 2 - 1]);
    return true;
}

static void compositionToUnderlines(const Vector<DWORD>& clauses, const Vector<BYTE>& attributes, Vector<CompositionUnderline>& underlines)
{
    if (clauses.isEmpty()) {
        underlines.clear();
        return;
    }
  
    const size_t numBoundaries = clauses.size() - 1;
    underlines.resize(numBoundaries);
    for (unsigned i = 0; i < numBoundaries; i++) {
        underlines[i].startOffset = clauses[i];
        underlines[i].endOffset = clauses[i + 1];
        BYTE attribute = attributes[clauses[i]];
        underlines[i].thick = attribute == ATTR_TARGET_CONVERTED || attribute == ATTR_TARGET_NOTCONVERTED;
        underlines[i].color = Color(0,0,0);
    }
}

#if !LOG_DISABLED
#define APPEND_ARGUMENT_NAME(name) \
    if (lparam & name) { \
        if (needsComma) \
            result += ", "; \
            result += #name; \
        needsComma = true; \
    }

static String imeCompositionArgumentNames(LPARAM lparam)
{
    String result;
    bool needsComma = false;
    if (lparam & GCS_COMPATTR) {
        result = "GCS_COMPATTR";
        needsComma = true;
    }
    APPEND_ARGUMENT_NAME(GCS_COMPCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_COMPREADSTR);
    APPEND_ARGUMENT_NAME(GCS_COMPREADATTR);
    APPEND_ARGUMENT_NAME(GCS_COMPREADCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_COMPSTR);
    APPEND_ARGUMENT_NAME(GCS_CURSORPOS);
    APPEND_ARGUMENT_NAME(GCS_DELTASTART);
    APPEND_ARGUMENT_NAME(GCS_RESULTCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_RESULTREADCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_RESULTREADSTR);
    APPEND_ARGUMENT_NAME(GCS_RESULTSTR);
    APPEND_ARGUMENT_NAME(CS_INSERTCHAR);
    APPEND_ARGUMENT_NAME(CS_NOMOVECARET);

    return result;
}

static String imeNotificationName(WPARAM wparam)
{
    switch (wparam) {
    case IMN_CHANGECANDIDATE:
        return "IMN_CHANGECANDIDATE";
    case IMN_CLOSECANDIDATE:
        return "IMN_CLOSECANDIDATE";
    case IMN_CLOSESTATUSWINDOW:
        return "IMN_CLOSESTATUSWINDOW";
    case IMN_GUIDELINE:
        return "IMN_GUIDELINE";
    case IMN_OPENCANDIDATE:
        return "IMN_OPENCANDIDATE";
    case IMN_OPENSTATUSWINDOW:
        return "IMN_OPENSTATUSWINDOW";
    case IMN_SETCANDIDATEPOS:
        return "IMN_SETCANDIDATEPOS";
    case IMN_SETCOMPOSITIONFONT:
        return "IMN_SETCOMPOSITIONFONT";
    case IMN_SETCOMPOSITIONWINDOW:
        return "IMN_SETCOMPOSITIONWINDOW";
    case IMN_SETCONVERSIONMODE:
        return "IMN_SETCONVERSIONMODE";
    case IMN_SETOPENSTATUS:
        return "IMN_SETOPENSTATUS";
    case IMN_SETSENTENCEMODE:
        return "IMN_SETSENTENCEMODE";
    case IMN_SETSTATUSWINDOWPOS:
        return "IMN_SETSTATUSWINDOWPOS";
    default:
        return "Unknown (" + String::number(wparam) + ")";
    }
}

static String imeRequestName(WPARAM wparam)
{
    switch (wparam) {
    case IMR_CANDIDATEWINDOW:
        return "IMR_CANDIDATEWINDOW";
    case IMR_COMPOSITIONFONT:
        return "IMR_COMPOSITIONFONT";
    case IMR_COMPOSITIONWINDOW:
        return "IMR_COMPOSITIONWINDOW";
    case IMR_CONFIRMRECONVERTSTRING:
        return "IMR_CONFIRMRECONVERTSTRING";
    case IMR_DOCUMENTFEED:
        return "IMR_DOCUMENTFEED";
    case IMR_QUERYCHARPOSITION:
        return "IMR_QUERYCHARPOSITION";
    case IMR_RECONVERTSTRING:
        return "IMR_RECONVERTSTRING";
    default:
        return "Unknown (" + String::number(wparam) + ")";
    }
}
#endif

bool WebView::onIMEComposition(LPARAM lparam)
{
    LOG(TextInput, "onIMEComposition %s", imeCompositionArgumentNames(lparam).latin1().data());
    HIMC hInputContext = getIMMContext();
    if (!hInputContext)
        return true;

    Frame* targetFrame = m_page->focusController()->focusedOrMainFrame();
    if (!targetFrame || !targetFrame->editor()->canEdit())
        return true;

    prepareCandidateWindow(targetFrame, hInputContext);

    if (lparam & GCS_RESULTSTR || !lparam) {
        String compositionString;
        if (!getCompositionString(hInputContext, GCS_RESULTSTR, compositionString) && lparam)
            return true;
        
        targetFrame->editor()->confirmComposition(compositionString);
    } else {
        String compositionString;
        if (!getCompositionString(hInputContext, GCS_COMPSTR, compositionString))
            return true;
        
        // Composition string attributes
        int numAttributes = IMMDict::dict().getCompositionString(hInputContext, GCS_COMPATTR, 0, 0);
        Vector<BYTE> attributes(numAttributes);
        IMMDict::dict().getCompositionString(hInputContext, GCS_COMPATTR, attributes.data(), numAttributes);

        // Get clauses
        int numClauses = IMMDict::dict().getCompositionString(hInputContext, GCS_COMPCLAUSE, 0, 0);
        Vector<DWORD> clauses(numClauses / sizeof(DWORD));
        IMMDict::dict().getCompositionString(hInputContext, GCS_COMPCLAUSE, clauses.data(), numClauses);

        Vector<CompositionUnderline> underlines;
        compositionToUnderlines(clauses, attributes, underlines);

        int cursorPosition = LOWORD(IMMDict::dict().getCompositionString(hInputContext, GCS_CURSORPOS, 0, 0));

        targetFrame->editor()->setComposition(compositionString, underlines, cursorPosition, 0);
    }

    return true;
}

bool WebView::onIMEEndComposition()
{
    LOG(TextInput, "onIMEEndComposition");
    // If the composition hasn't been confirmed yet, it needs to be cancelled.
    // This happens after deleting the last character from inline input hole.
    Frame* targetFrame = m_page->focusController()->focusedOrMainFrame();
    if (targetFrame && targetFrame->editor()->hasComposition())
        targetFrame->editor()->confirmComposition(String());

    if (m_inIMEComposition)
        m_inIMEComposition--;

    return true;
}

bool WebView::onIMEChar(WPARAM wparam, LPARAM lparam)
{
    UNUSED_PARAM(wparam);
    UNUSED_PARAM(lparam);
    LOG(TextInput, "onIMEChar U+%04X %08X", wparam, lparam);
    return true;
}

bool WebView::onIMENotify(WPARAM wparam, LPARAM, LRESULT*)
{
    UNUSED_PARAM(wparam);
    LOG(TextInput, "onIMENotify %s", imeNotificationName(wparam).latin1().data());
    return false;
}

LRESULT WebView::onIMERequestCharPosition(Frame* targetFrame, IMECHARPOSITION* charPos)
{
    if (charPos->dwCharPos && !targetFrame->editor()->hasComposition())
        return 0;
    IntRect caret;
    if (RefPtr<Range> range = targetFrame->editor()->hasComposition() ? targetFrame->editor()->compositionRange() : targetFrame->selection()->selection().toNormalizedRange()) {
        ExceptionCode ec = 0;
        RefPtr<Range> tempRange = range->cloneRange(ec);
        tempRange->setStart(tempRange->startContainer(ec), tempRange->startOffset(ec) + charPos->dwCharPos, ec);
        caret = targetFrame->editor()->firstRectForRange(tempRange.get());
    }
    caret = targetFrame->view()->contentsToWindow(caret);
    charPos->pt.x = caret.x();
    charPos->pt.y = caret.y();
    ::ClientToScreen(m_viewWindow, &charPos->pt);
    charPos->cLineHeight = caret.height();
    ::GetWindowRect(m_viewWindow, &charPos->rcDocument);
    return true;
}

LRESULT WebView::onIMERequestReconvertString(Frame* targetFrame, RECONVERTSTRING* reconvertString)
{
    RefPtr<Range> selectedRange = targetFrame->selection()->toNormalizedRange();
    String text = selectedRange->text();
    if (!reconvertString)
        return sizeof(RECONVERTSTRING) + text.length() * sizeof(UChar);

    unsigned totalSize = sizeof(RECONVERTSTRING) + text.length() * sizeof(UChar);
    if (totalSize > reconvertString->dwSize)
        return 0;
    reconvertString->dwCompStrLen = text.length();
    reconvertString->dwStrLen = text.length();
    reconvertString->dwTargetStrLen = text.length();
    reconvertString->dwStrOffset = sizeof(RECONVERTSTRING);
    memcpy(reconvertString + 1, text.characters(), text.length() * sizeof(UChar));
    return totalSize;
}

LRESULT WebView::onIMERequest(WPARAM request, LPARAM data)
{
    LOG(TextInput, "onIMERequest %s", imeRequestName(request).latin1().data());
    Frame* targetFrame = m_page->focusController()->focusedOrMainFrame();
    if (!targetFrame || !targetFrame->editor()->canEdit())
        return 0;

    switch (request) {
        case IMR_RECONVERTSTRING:
            return onIMERequestReconvertString(targetFrame, (RECONVERTSTRING*)data);

        case IMR_QUERYCHARPOSITION:
            return onIMERequestCharPosition(targetFrame, (IMECHARPOSITION*)data);
    }
    return 0;
}

bool WebView::onIMESelect(WPARAM wparam, LPARAM lparam)
{
    UNUSED_PARAM(wparam);
    UNUSED_PARAM(lparam);
    LOG(TextInput, "onIMESelect locale %ld %s", lparam, wparam ? "select" : "deselect");
    return false;
}

bool WebView::onIMESetContext(WPARAM wparam, LPARAM)
{
    LOG(TextInput, "onIMESetContext %s", wparam ? "active" : "inactive");
    return false;
}

HRESULT STDMETHODCALLTYPE WebView::inspector(IWebInspector** inspector)
{
    if (!m_webInspector)
        m_webInspector.adoptRef(WebInspector::createInstance(this));

    return m_webInspector.copyRefTo(inspector);
}

HRESULT STDMETHODCALLTYPE WebView::windowAncestryDidChange()
{
    HWND newParent;
    if (m_viewWindow)
        newParent = findTopLevelParent(m_hostWindow);
    else {
        // There's no point in tracking active state changes of our parent window if we don't have
        // a window ourselves.
        newParent = 0;
    }

    if (newParent == m_topLevelParent)
        return S_OK;

    if (m_topLevelParent)
        WindowMessageBroadcaster::removeListener(m_topLevelParent, this);

    m_topLevelParent = newParent;

    if (m_topLevelParent)
        WindowMessageBroadcaster::addListener(m_topLevelParent, this);

    updateActiveState();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::paintDocumentRectToContext(
    /* [in] */ RECT rect,
    /* [in] */ OLE_HANDLE deviceContext)
{
    if (!deviceContext)
        return E_POINTER;

    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->paintDocumentRectToContext(rect, deviceContext);
}

HRESULT STDMETHODCALLTYPE WebView::paintScrollViewRectToContextAtPoint(
    /* [in] */ RECT rect,
    /* [in] */ POINT pt,
    /* [in] */ OLE_HANDLE deviceContext)
{
    if (!deviceContext)
        return E_POINTER;

    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->paintScrollViewRectToContextAtPoint(rect, pt, deviceContext);
}

HRESULT STDMETHODCALLTYPE WebView::reportException(
    /* [in] */ JSContextRef context,
    /* [in] */ JSValueRef exception)
{
    if (!context || !exception)
        return E_FAIL;

    JSLock lock(JSC::SilenceAssertionsOnly);
    JSC::ExecState* execState = toJS(context);

    // Make sure the context has a DOMWindow global object, otherwise this context didn't originate from a WebView.
    if (!toJSDOMWindow(execState->lexicalGlobalObject()))
        return E_FAIL;

    WebCore::reportException(execState, toJS(execState, exception));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::elementFromJS(
    /* [in] */ JSContextRef context,
    /* [in] */ JSValueRef nodeObject,
    /* [retval][out] */ IDOMElement **element)
{
    if (!element)
        return E_POINTER;

    *element = 0;

    if (!context)
        return E_FAIL;

    if (!nodeObject)
        return E_FAIL;

    JSLock lock(JSC::SilenceAssertionsOnly);
    Element* elt = toElement(toJS(toJS(context), nodeObject));
    if (!elt)
        return E_FAIL;

    *element = DOMElement::createInstance(elt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomHTMLTokenizerTimeDelay(
    /* [in] */ double timeDelay)
{
    if (!m_page)
        return E_FAIL;

    m_page->setCustomHTMLTokenizerTimeDelay(timeDelay);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomHTMLTokenizerChunkSize(
    /* [in] */ int chunkSize)
{
    if (!m_page)
        return E_FAIL;

    m_page->setCustomHTMLTokenizerChunkSize(chunkSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::backingStore(
    /* [out, retval] */ OLE_HANDLE* hBitmap)
{
    if (!hBitmap)
        return E_POINTER;
    if (!m_backingStoreBitmap)
        return E_FAIL;
    *hBitmap = reinterpret_cast<OLE_HANDLE>(m_backingStoreBitmap->handle());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setTransparent(BOOL transparent)
{
    if (m_transparent == !!transparent)
        return S_OK;

    m_transparent = transparent;
    m_mainFrame->updateBackground();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::transparent(BOOL* transparent)
{
    if (!transparent)
        return E_POINTER;

    *transparent = this->transparent() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCookieEnabled(BOOL enable)
{
    if (!m_page)
        return E_FAIL;

    m_page->setCookieEnabled(enable);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::cookieEnabled(BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;

    if (!m_page)
        return E_FAIL;

    *enabled = m_page->cookieEnabled();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setMediaVolume(float volume)
{
    if (!m_page)
        return E_FAIL;

    m_page->setMediaVolume(volume);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::mediaVolume(float* volume)
{
    if (!volume)
        return E_POINTER;

    if (!m_page)
        return E_FAIL;

    *volume = m_page->mediaVolume();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setDefersCallbacks(BOOL defersCallbacks)
{
    if (!m_page)
        return E_FAIL;

    m_page->setDefersLoading(defersCallbacks);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::defersCallbacks(BOOL* defersCallbacks)
{
    if (!defersCallbacks)
        return E_POINTER;

    if (!m_page)
        return E_FAIL;

    *defersCallbacks = m_page->defersLoading();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::globalHistoryItem(IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;

    if (!m_page)
        return E_FAIL;

    if (!m_page->globalHistoryItem()) {
        *item = 0;
        return S_OK;
    }

    *item = WebHistoryItem::createInstance(m_page->globalHistoryItem());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setAlwaysUsesComplexTextCodePath(BOOL complex)
{
    WebCoreSetAlwaysUsesComplexTextCodePath(complex);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::alwaysUsesComplexTextCodePath(BOOL* complex)
{
    if (!complex)
        return E_POINTER;

    *complex = WebCoreAlwaysUsesComplexTextCodePath();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::registerEmbeddedViewMIMEType(BSTR mimeType)
{
    if (!mimeType)
        return E_POINTER;

    if (!m_embeddedViewMIMETypes)
        m_embeddedViewMIMETypes.set(new HashSet<String>);

    m_embeddedViewMIMETypes->add(String(mimeType, ::SysStringLen(mimeType)));
    return S_OK;
}

bool WebView::shouldUseEmbeddedView(const WTF::String& mimeType) const
{
    if (!m_embeddedViewMIMETypes)
        return false;

    return m_embeddedViewMIMETypes->contains(mimeType);
}

bool WebView::onGetObject(WPARAM wParam, LPARAM lParam, LRESULT& lResult) const
{
    lResult = 0;

    if (lParam != OBJID_CLIENT)
        return false;

    AXObjectCache::enableAccessibility();

    // Get the accessible object for the top-level frame.
    WebFrame* mainFrameImpl = topLevelFrame();
    if (!mainFrameImpl)
        return false;

    COMPtr<IAccessible> accessible = mainFrameImpl->accessible();
    if (!accessible)
        return false;

    if (!accessibilityLib) {
        if (!(accessibilityLib = ::LoadLibrary(TEXT("oleacc.dll"))))
            return false;
    }

    static LPFNLRESULTFROMOBJECT procPtr = reinterpret_cast<LPFNLRESULTFROMOBJECT>(::GetProcAddress(accessibilityLib, "LresultFromObject"));
    if (!procPtr)
        return false;

    // LresultFromObject returns a reference to the accessible object, stored
    // in an LRESULT. If this call is not successful, Windows will handle the
    // request through DefWindowProc.
    return SUCCEEDED(lResult = procPtr(__uuidof(IAccessible), wParam, accessible.get()));
}

STDMETHODIMP WebView::AccessibleObjectFromWindow(HWND hwnd, DWORD objectID, REFIID riid, void** ppObject)
{
    ASSERT(accessibilityLib);
    static LPFNACCESSIBLEOBJECTFROMWINDOW procPtr = reinterpret_cast<LPFNACCESSIBLEOBJECTFROMWINDOW>(::GetProcAddress(accessibilityLib, "AccessibleObjectFromWindow"));
    if (!procPtr)
        return E_FAIL;
    return procPtr(hwnd, objectID, riid, ppObject);
}

HRESULT WebView::setMemoryCacheDelegateCallsEnabled(BOOL enabled)
{
    m_page->setMemoryCacheClientCallsEnabled(enabled);
    return S_OK;
}

HRESULT WebView::setJavaScriptURLsAreAllowed(BOOL areAllowed)
{
    m_page->setJavaScriptURLsAreAllowed(areAllowed);
    return S_OK;
}

HRESULT WebView::setCanStartPlugins(BOOL canStartPlugins)
{
    m_page->setCanStartMedia(canStartPlugins);
    return S_OK;
}

static String toString(BSTR bstr)
{
    return String(bstr, SysStringLen(bstr));
}

static KURL toKURL(BSTR bstr)
{
    return KURL(KURL(), toString(bstr));
}

void WebView::enterFullscreenForNode(Node* node)
{
    if (!node->hasTagName(HTMLNames::videoTag))
        return;

#if ENABLE(VIDEO)
    HTMLMediaElement* videoElement = static_cast<HTMLMediaElement*>(node);

    if (m_fullscreenController) {
        if (m_fullscreenController->mediaElement() == videoElement) {
            // The backend may just warn us that the underlaying plaftormMovie()
            // has changed. Just force an update.
            m_fullscreenController->setMediaElement(videoElement);
            return; // No more to do.
        }

        // First exit Fullscreen for the old mediaElement.
        m_fullscreenController->mediaElement()->exitFullscreen();
        // This previous call has to trigger exitFullscreen,
        // which has to clear m_fullscreenController.
        ASSERT(!m_fullscreenController);
    }

    m_fullscreenController = new FullscreenVideoController;
    m_fullscreenController->setMediaElement(videoElement);
    m_fullscreenController->enterFullscreen();
#endif
}

void WebView::exitFullscreen()
{
#if ENABLE(VIDEO)
    if (m_fullscreenController)
        m_fullscreenController->exitFullscreen();
    m_fullscreenController = 0;
#endif
}

static PassOwnPtr<Vector<String> > toStringVector(unsigned patternsCount, BSTR* patterns)
{
    // Convert the patterns into a Vector.
    if (patternsCount == 0)
        return 0;
    Vector<String>* patternsVector = new Vector<String>;
    for (unsigned i = 0; i < patternsCount; ++i)
        patternsVector->append(toString(patterns[i]));
    return patternsVector;
}

HRESULT WebView::addUserScriptToGroup(BSTR groupName, IWebScriptWorld* iWorld, BSTR source, BSTR url, 
                                      unsigned whitelistCount, BSTR* whitelist,
                                      unsigned blacklistCount, BSTR* blacklist,
                                      WebUserScriptInjectionTime injectionTime)
{
    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_POINTER;

    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->addUserScriptToWorld(world->world(), toString(source), toKURL(url),
                                    toStringVector(whitelistCount, whitelist), toStringVector(blacklistCount, blacklist),
                                    injectionTime == WebInjectAtDocumentStart ? InjectAtDocumentStart : InjectAtDocumentEnd,
                                    InjectInAllFrames);

    return S_OK;
}

HRESULT WebView::addUserStyleSheetToGroup(BSTR groupName, IWebScriptWorld* iWorld, BSTR source, BSTR url,
                                          unsigned whitelistCount, BSTR* whitelist,
                                          unsigned blacklistCount, BSTR* blacklist)
{
    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_POINTER;

    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->addUserStyleSheetToWorld(world->world(), toString(source), toKURL(url),
                                        toStringVector(whitelistCount, whitelist), toStringVector(blacklistCount, blacklist),
                                        InjectInAllFrames);

    return S_OK;
}

HRESULT WebView::removeUserScriptFromGroup(BSTR groupName, IWebScriptWorld* iWorld, BSTR url)
{
    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_POINTER;

    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->removeUserScriptFromWorld(world->world(), toKURL(url));

    return S_OK;
}

HRESULT WebView::removeUserStyleSheetFromGroup(BSTR groupName, IWebScriptWorld* iWorld, BSTR url)
{
    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_POINTER;

    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->removeUserStyleSheetFromWorld(world->world(), toKURL(url));

    return S_OK;
}

HRESULT WebView::removeUserScriptsFromGroup(BSTR groupName, IWebScriptWorld* iWorld)
{
    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_POINTER;

    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->removeUserScriptsFromWorld(world->world());
    return S_OK;
}

HRESULT WebView::removeUserStyleSheetsFromGroup(BSTR groupName, IWebScriptWorld* iWorld)
{
    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_POINTER;

    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->removeUserStyleSheetsFromWorld(world->world());
    return S_OK;
}

HRESULT WebView::removeAllUserContentFromGroup(BSTR groupName)
{
    String group = toString(groupName);
    if (group.isEmpty())
        return E_INVALIDARG;

    PageGroup* pageGroup = PageGroup::pageGroup(group);
    ASSERT(pageGroup);
    if (!pageGroup)
        return E_FAIL;

    pageGroup->removeAllUserContent();
    return S_OK;
}

HRESULT WebView::invalidateBackingStore(const RECT* rect)
{
    if (!IsWindow(m_viewWindow))
        return S_OK;

    RECT clientRect;
    if (!GetClientRect(m_viewWindow, &clientRect))
        return E_FAIL;

    RECT rectToInvalidate;
    if (!rect)
        rectToInvalidate = clientRect;
    else if (!IntersectRect(&rectToInvalidate, &clientRect, rect))
        return S_OK;

    repaint(rectToInvalidate, true);
    return S_OK;
}

HRESULT WebView::addOriginAccessWhitelistEntry(BSTR sourceOrigin, BSTR destinationProtocol, BSTR destinationHost, BOOL allowDestinationSubdomains)
{
    SecurityOrigin::addOriginAccessWhitelistEntry(*SecurityOrigin::createFromString(String(sourceOrigin, SysStringLen(sourceOrigin))), String(destinationProtocol, SysStringLen(destinationProtocol)), String(destinationHost, SysStringLen(destinationHost)), allowDestinationSubdomains);
    return S_OK;
}

HRESULT WebView::removeOriginAccessWhitelistEntry(BSTR sourceOrigin, BSTR destinationProtocol, BSTR destinationHost, BOOL allowDestinationSubdomains)
{
    SecurityOrigin::removeOriginAccessWhitelistEntry(*SecurityOrigin::createFromString(String(sourceOrigin, SysStringLen(sourceOrigin))), String(destinationProtocol, SysStringLen(destinationProtocol)), String(destinationHost, SysStringLen(destinationHost)), allowDestinationSubdomains);
    return S_OK;
}

HRESULT WebView::resetOriginAccessWhitelists()
{
    SecurityOrigin::resetOriginAccessWhitelists();
    return S_OK;
}
 
HRESULT WebView::setHistoryDelegate(IWebHistoryDelegate* historyDelegate)
{
    m_historyDelegate = historyDelegate;
    return S_OK;
}

HRESULT WebView::historyDelegate(IWebHistoryDelegate** historyDelegate)
{
    if (!historyDelegate)
        return E_POINTER;

    return m_historyDelegate.copyRefTo(historyDelegate);
}

HRESULT WebView::addVisitedLinks(BSTR* visitedURLs, unsigned visitedURLCount)
{
    PageGroup& group = core(this)->group();
    
    for (unsigned i = 0; i < visitedURLCount; ++i) {
        BSTR url = visitedURLs[i];
        unsigned length = SysStringLen(url);
        group.addVisitedLink(url, length);
    }

    return S_OK;
}

void WebView::downloadURL(const KURL& url)
{
    // It's the delegate's job to ref the WebDownload to keep it alive - otherwise it will be
    // destroyed when this function returns.
    COMPtr<WebDownload> download(AdoptCOM, WebDownload::createInstance(url, m_downloadDelegate.get()));
    download->start();
}

#if USE(ACCELERATED_COMPOSITING)
void WebView::setRootChildLayer(WebCore::WKCACFLayer* layer)
{
    setAcceleratedCompositing(layer ? true : false);
    if (m_layerRenderer)
        m_layerRenderer->setRootChildLayer(layer);
}

void WebView::setAcceleratedCompositing(bool accelerated)
{
    if (m_isAcceleratedCompositing == accelerated || !WKCACFLayerRenderer::acceleratedCompositingAvailable())
        return;

    if (accelerated) {
        m_layerRenderer = WKCACFLayerRenderer::create(this);
        if (m_layerRenderer) {
            m_isAcceleratedCompositing = true;

            // Create the root layer
            ASSERT(m_viewWindow);
            m_layerRenderer->setHostWindow(m_viewWindow);
            m_layerRenderer->createRenderer();
        }
    } else {
        m_layerRenderer = 0;
        m_isAcceleratedCompositing = false;
    }
}

void releaseBackingStoreCallback(void* info, const void* data, size_t size)
{
    // Release the backing store bitmap previously retained by updateRootLayerContents().
    ASSERT(info);
    if (info)
        static_cast<RefCountedHBITMAP*>(info)->deref();
}

void WebView::updateRootLayerContents()
{
    if (!m_backingStoreBitmap || !m_layerRenderer)
        return;

    // Get the backing store into a CGImage
    BITMAP bitmap;
    GetObject(m_backingStoreBitmap->handle(), sizeof(bitmap), &bitmap);
    size_t bmSize = bitmap.bmWidthBytes * bitmap.bmHeight;
    RetainPtr<CGDataProviderRef> cgData(AdoptCF,
        CGDataProviderCreateWithData(static_cast<void*>(m_backingStoreBitmap.get()),
                                                        bitmap.bmBits, bmSize,
                                                        releaseBackingStoreCallback));
    RetainPtr<CGColorSpaceRef> space(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> backingStoreImage(AdoptCF, CGImageCreate(bitmap.bmWidth, bitmap.bmHeight,
                                     8, bitmap.bmBitsPixel, 
                                     bitmap.bmWidthBytes, space.get(), 
                                     kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                     cgData.get(), 0, false, 
                                     kCGRenderingIntentDefault));

    // Retain the backing store bitmap so that it is not deleted by deleteBackingStore()
    // while still in use within CA. When CA is done with the bitmap, it will
    // call releaseBackingStoreCallback(), which will release the backing store bitmap.
    m_backingStoreBitmap->ref();

    // Hand the CGImage to CACF for compositing
    if (m_nextDisplayIsSynchronous) {
        m_layerRenderer->setRootContentsAndDisplay(backingStoreImage.get());
        m_nextDisplayIsSynchronous = false;
    } else
        m_layerRenderer->setRootContents(backingStoreImage.get());
}

void WebView::layerRendererBecameVisible()
{
    m_layerRenderer->createRenderer();
}
#endif

HRESULT STDMETHODCALLTYPE WebView::setPluginHalterDelegate(IWebPluginHalterDelegate* d)
{
    m_pluginHalterDelegate = d;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::pluginHalterDelegate(IWebPluginHalterDelegate** d)
{
    if (!d)
        return E_POINTER;

    if (!m_pluginHalterDelegate)
        return E_FAIL;

    return m_pluginHalterDelegate.copyRefTo(d);
}

static PluginView* pluginViewForNode(IDOMNode* domNode)
{
    COMPtr<DOMNode> webKitDOMNode(Query, domNode);
    if (!webKitDOMNode)
        return 0;

    Node* node = webKitDOMNode->node();
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;

    Widget* widget = toRenderWidget(renderer)->widget();
    if (!widget || !widget->isPluginView())
        return 0;

    return static_cast<PluginView*>(widget);
}

HRESULT WebView::isNodeHaltedPlugin(IDOMNode* domNode, BOOL* result)
{
    if (!domNode || !result)
        return E_POINTER;

    *result = FALSE;

    PluginView* view = pluginViewForNode(domNode);
    if (!view)
        return E_FAIL;

    *result = view->isHalted();
    return S_OK;
}

HRESULT WebView::restartHaltedPluginForNode(IDOMNode* domNode)
{
    if (!domNode)
        return E_POINTER;

    PluginView* view = pluginViewForNode(domNode);
    if (!view)
        return E_FAIL;

    view->restart();
    return S_OK;
}

HRESULT WebView::hasPluginForNodeBeenHalted(IDOMNode* domNode, BOOL* result)
{
    if (!domNode || !result)
        return E_POINTER;

    *result = FALSE;

    PluginView* view = pluginViewForNode(domNode);
    if (!view)
        return E_FAIL;

    *result = view->hasBeenHalted();
    return S_OK;
}

HRESULT WebView::setGeolocationProvider(IWebGeolocationProvider* locationProvider)
{
    m_geolocationProvider = locationProvider;
    return S_OK;
}

HRESULT WebView::geolocationProvider(IWebGeolocationProvider** locationProvider)
{
    if (!locationProvider)
        return E_POINTER;

    if (!m_geolocationProvider)
        return E_FAIL;

    return m_geolocationProvider.copyRefTo(locationProvider);
}

HRESULT WebView::geolocationDidChangePosition(IWebGeolocationPosition* position)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    if (!m_page)
        return E_FAIL;
    m_page->geolocationController()->positionChanged(core(position));
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebView::geolocationDidFailWithError(IWebError* error)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    if (!m_page)
        return E_FAIL;
    if (!error)
        return E_POINTER;

    BSTR descriptionBSTR;
    if (FAILED(error->localizedDescription(&descriptionBSTR)))
        return E_FAIL;
    String descriptionString(descriptionBSTR, SysStringLen(descriptionBSTR));
    SysFreeString(descriptionBSTR);

    RefPtr<GeolocationError> geolocationError = GeolocationError::create(GeolocationError::PositionUnavailable, descriptionString);
    m_page->geolocationController()->errorOccurred(geolocationError.get());
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebView::setDomainRelaxationForbiddenForURLScheme(BOOL forbidden, BSTR scheme)
{
    SecurityOrigin::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme, SysStringLen(scheme)));
    return S_OK;
}

HRESULT WebView::registerURLSchemeAsSecure(BSTR scheme)
{
    SchemeRegistry::registerURLSchemeAsSecure(toString(scheme));
    return S_OK;
}

HRESULT WebView::nextDisplayIsSynchronous()
{
    m_nextDisplayIsSynchronous = true;
    return S_OK;
}

#if USE(ACCELERATED_COMPOSITING)
bool WebView::shouldRender() const
{
    Frame* coreFrame = core(m_mainFrame);
    if (!coreFrame)
        return true;
    FrameView* frameView = coreFrame->view();
    if (!frameView)
        return true;

    return !frameView->layoutPending();
}
#endif

class EnumTextMatches : public IEnumTextMatches
{
    long m_ref;
    UINT m_index;
    Vector<IntRect> m_rects;
public:
    EnumTextMatches(Vector<IntRect>* rects) : m_index(0), m_ref(1)
    {
        m_rects = *rects;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv)
    {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IEnumTextMatches)) {
            *ppv = this;
            AddRef();
        }

        return *ppv?S_OK:E_NOINTERFACE;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        return m_ref++;
    }
    
    virtual ULONG STDMETHODCALLTYPE Release()
    {
        if (m_ref == 1) {
            delete this;
            return 0;
        }
        else
            return m_ref--;
    }

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG, RECT* rect, ULONG* pceltFetched)
    {
        if (m_index < m_rects.size()) {
            if (pceltFetched)
                *pceltFetched = 1;
            *rect = m_rects[m_index];
            m_index++;
            return S_OK;
        }

        if (pceltFetched)
            *pceltFetched = 0;

        return S_FALSE;
    }
    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        m_index += celt;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Reset(void)
    {
        m_index = 0;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumTextMatches**)
    {
        return E_NOTIMPL;
    }
};

HRESULT createMatchEnumerator(Vector<IntRect>* rects, IEnumTextMatches** matches)
{
    *matches = new EnumTextMatches(rects);
    return (*matches)?S_OK:E_OUTOFMEMORY;
}

Page* core(IWebView* iWebView)
{
    Page* page = 0;

    COMPtr<WebView> webView;
    if (SUCCEEDED(iWebView->QueryInterface(&webView)) && webView)
        page = webView->page();

    return page;
}
