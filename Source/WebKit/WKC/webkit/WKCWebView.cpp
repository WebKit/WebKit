/*
 * WKCWebView.cpp 
 * Copyright (C) 2007-2009 various GTK/WebKit contributors
 * Copyright (c) 2010-2015 ACCESS CO., LTD. All rights reserved.
 * Modernized 2025 for webkit-wiiu (devkitPPC / Aroma bare-metal).
 *
 * Dead code removed:
 *   - BackForwardListImpl history block  (API removed from WebKit)
 *   - PageCache / BackForwardCache       (removed from WebKit)
 *   - WebCore::iconDatabase()            (removed from WebKit)
 *   - WebInspectorServer                 (removed from WebKit)
 *   - EPUB inner class                   (Nintendo-only, no upstream)
 *   - Most SSL functions                 (stubbed; fill in if libcurl handles SSL)
 *   - RuntimeEnabledFeatures old API     (restructured in modern WebKit)
 *
 * NOTE on PageConfiguration:
 *   The exact fields and whether clients are raw-pointer or UniqueRef depends
 *   on exactly which WebKit commit your fork tracks.  The construct() function
 *   below uses UniqueRef / WTFMove throughout.  If your fork still uses the
 *   old PageClients struct, swap PageConfiguration for PageClients and use
 *   raw pointers as the original code did.
 */

// wkc/wkcclib.h (pulled in transitively) both #includes <sys/time.h> and then
// defines its own `struct timezone`, which redefines the newlib one on this
// devkitPPC target. Opt out of that redefinition; the system struct is used.
#define __WKC_OMIT_DEFINE_TIMEZONE
#define __WKC_OMIT_DEFINE_TIMESPEC

#include "config.h"

#include "WKCWebView.h"
#include "WKCWebViewPrivate.h"
#include "WKCWebFrame.h"
#include "WKCWebFramePrivate.h"
#include "WKCPlatformEvents.h"
#include "WKCClientBuilders.h"
#include "WKCWebViewPrefs.h"
#include "WKCPrefs.h"
#include "WKCMemoryInfo.h"
#include "WKCOverlayPrivate.h"

#include "ChromeClientWKC.h"
#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "InspectorBackendClientWKC.h"
#endif
#if ENABLE(NOTIFICATIONS)
#include "NotificationClientWKC.h"
#endif
#if ENABLE(CONTEXT_MENUS)
#include "ContextMenuClientWKC.h"
#endif
#include "EditorClientWKC.h"
#include "DragClientWKC.h"
#include "FrameLoaderClientWKC.h"
// The WKC Inspector backend bridges DevTools Protocol JSON to the embedder.
// The embedder controls its frontend transport; WKC opens no network listener.
#include "DropDownListClientWKC.h"
#if ENABLE(GEOLOCATION)
#include "GeolocationClientWKC.h"
#include "GeolocationController.h"
#endif
#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionClientWKC.h"
#include "DeviceOrientationClientWKC.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#endif

// WKC peer headers
#include <wkc/wkcpeer.h>
#include <wkc/wkcgpeer.h>
#include <wkc/wkcmpeer.h>
#include <wkc/wkcmediapeer.h>
#include <wkc/wkcglpeer.h>
#include <wkc/wkcsocket.h>
#include <wkc/wkcheappeer.h>

// WKC helper wrappers
#include "helpers/WKCHistoryItem.h"
#include "helpers/WKCNode.h"
#include "helpers/WKCSettings.h"
#include "helpers/WKCEditor.h"
#include "helpers/privates/WKCHistoryItemPrivate.h"
#include "helpers/privates/WKCResourceHandlePrivate.h"
#include "helpers/privates/WKCNodePrivate.h"
#include "helpers/privates/WKCPagePrivate.h"
#include "helpers/privates/WKCEventHandlerPrivate.h"
#include "helpers/privates/WKCFocusControllerPrivate.h"
#include "helpers/privates/WKCHitTestResultPrivate.h"

// WebCore
#include "BackForwardController.h"
#include "Chrome.h"
#include "CookieJar.h"
#include "CrossOriginPreflightResultCache.h"
#include "DocumentLoader.h"
#include "EventHandler.h"
#include "HandleUserInputEventResult.h"
#include "ScrollingCoordinatorTypes.h"
#include "FocusController.h"
#include "FindOptions.h"
#include "EventNames.h"
#include "SharedStringHash.h"
#include "VisitedLinkStore.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "RenderLayerScrollableArea.h"
#include "ScriptController.h"
#include "FontCache.h"
#include "GarbageCollectionController.h"
#include "ContainerNode.h"
#include "RenderBox.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HistoryItem.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "ImageBufferDataWKC.h"
#include "ImageWKC.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "DocumentView.h"
#include "RenderObjectInlines.h"
#include "EventTargetInlines.h"
#include "NodeInlines.h"
#include "MemoryCache.h"
#include "NodeList.h"
#include "EmptyClients.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformTouchEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressTracker.h"
#include "RenderFrameSet.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
// Legacy WKC entry points are retained as an adapter over the modern CURL
// networking context. Do not report successful initialization while returning
// a null manager: WKC cookie/SSL callers rely on this singleton being usable.
#include "CurlContext.h"
#include <curl/curl.h>
#include <cstring>
#include <string>
namespace WebCore {
class ResourceHandleManager {
public:
    static ResourceHandleManager* sharedInstance() { return &singleton(); }
    static bool isExistSharedInstance() { return true; }
    static bool createSharedInstance() { (void)CurlContext::singleton(); return true; }
    static void deleteSharedInstance() { }
    static void forceTerminateInstance() { }

    void clearCookies()
    {
        withCookieHandle([](CURL* handle) { curl_easy_setopt(handle, CURLOPT_COOKIELIST, "ALL"); });
    }
    int CookieSerializeNum()
    {
        auto* list = cookieList();
        int count = 0;
        for (auto* item = list; item; item = item->next) ++count;
        curl_slist_free_all(list);
        return count;
    }
    int CookieSerialize(char* buffer, int capacity)
    {
        if (!buffer || capacity <= 0) return 0;
        buffer[0] = '\\0';
        auto* list = cookieList();
        int written = 0;
        for (auto* item = list; item; item = item->next) {
            int length = static_cast<int>(::strlen(item->data)) + 1;
            if (written + length > capacity) break;
            ::memcpy(buffer + written, item->data, length - 1);
            buffer[written + length - 1] = '\\n';
            written += length;
        }
        curl_slist_free_all(list);
        return written;
    }
    void CookieDeserialize(const char* data, bool)
    {
        if (!data) return;
        withCookieHandle([&](CURL* handle) {
            const char* line = data;
            while (*line) {
                const char* newline = ::strchr(line, '\\n');
                size_t length = newline ? static_cast<size_t>(newline - line) : ::strlen(line);
                if (length) {
                    std::string command(line, length);
                    curl_easy_setopt(handle, CURLOPT_COOKIELIST, command.c_str());
                }
                if (!newline) break;
                line = newline + 1;
            }
        });
    }
    void clearHTTPCache() { }
    void permitRequest(void*, bool) { }
    void setAllowServerHost(const char*) { }

    // Custom WKC certificate-management APIs have no equivalent public
    // registration API in CurlContext. Fail explicitly rather than pretending
    // that certificates were installed or accepted.
    void* SSLRegisterRootCA(const char*, int) { return nullptr; }
    void* SSLRegisterRootCAByDER(const char*, int) { return nullptr; }
    int SSLUnregisterRootCA(void*) { return -1; }
    void SSLRootCADeleteAll() { }
    void* SSLRegisterCRL(const char*, int) { return nullptr; }
    int SSLUnregisterCRL(void*) { return -1; }
    void SSLCRLDeleteAll() { }
    void* SSLRegisterClientCert(const unsigned char*, int, const unsigned char*, int) { return nullptr; }
    void* SSLRegisterClientCertByDER(const unsigned char*, int, const unsigned char*, int) { return nullptr; }
    int SSLUnregisterClientCert(void*) { return -1; }
    void SSLClientCertDeleteAll() { }
    bool SSLRegisterBlackCert(const char*, const char*) { return false; }
    void SSLBlackCertDeleteAll() { }
    bool SSLRegisterEVSSLOID(const char*, const char*, const char*, const char*) { return false; }
    void SSLEVSSLOIDDeleteAll() { }
    const char** getServerCertChain(const char*, int& count) { count = 0; return nullptr; }
    void freeServerCertChain(const char**, int) { }

private:
    static ResourceHandleManager& singleton()
    {
        static ResourceHandleManager manager;
        return manager;
    }
    template<typename Function> static void withCookieHandle(Function&& function)
    {
        CURL* handle = curl_easy_init();
        if (!handle) return;
        curl_easy_setopt(handle, CURLOPT_SHARE, CurlContext::singleton().shareHandle().handle());
        curl_easy_setopt(handle, CURLOPT_COOKIEFILE, "");
        function(handle);
        curl_easy_cleanup(handle);
    }
    static curl_slist* cookieList()
    {
        curl_slist* list = nullptr;
        withCookieHandle([&](CURL* handle) {
            curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &list);
        });
        return list;
    }
};
}
#include "Settings.h"
#include "SpatialNavigation.h"
#if ENABLE(GAMEPAD)
#include "GamepadProvider.h"
#include "GamepadProviderClient.h"
#include "PlatformGamepad.h"
#endif
#include "Text.h"
#include "TextEncodingRegistry.h"
#include "TextIterator.h"
#include "UserGestureIndicator.h"

#include <JavaScriptCore/InitializeThreading.h>
#include <pal/SessionID.h>
#include <wtf/UniqueRef.h>
#include <wtf/URL.h>
#include <wtf/text/Base64.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>

#include "FastMalloc.h"
#include "NotImplemented.h"

#include <cctype>
#include <string>

namespace WebCore {
// Concrete WKC GraphicsContext backend (GraphicsContext itself is abstract).
// Defined in Source/WebCore/platform/graphics/WKC/GraphicsContextWKC.cpp.
std::unique_ptr<GraphicsContext> createGraphicsContextWKC(void* drawContext);
}

// ─── libxml2 / libxslt reset (external C linkage) ────────────────────────────
extern "C" {
void wkc_libxml2_resetVariables(void);
#if ENABLE(XSLT)
void wkc_libxslt_resetVariables(void);
#endif
void wkc_sqlite3_force_terminate(void);
#ifdef WKC_USE_WKC_OWN_CAIRO
void wkc_pixman_resetVariables(void);
cairo_public void wkc_cairo_resetVariables(void);
#endif
}

// ─── Forward declarations ─────────────────────────────────────────────────────
namespace WebCore {
extern void InitializeLoggingChannelsIfNecessary();
extern void EventLoop_setCycleProc(bool(*)(void*), void*);
}

namespace WTF {
extern void finalizeMainThreadPlatform();
}

// =============================================================================
// Gamepad API support (W3C Gamepad API) — bridges main.cpp's per-frame
// VPAD/KPAD reads into WebCore's navigator.getGamepads().
//
// Lives here rather than in its own file because the only thing that calls
// into it is WKCWebView::initializeGamepads() / notifyGamepadEvent() below.
//
// NOTE ON VERSION DRIFT: WebCore::PlatformGamepad's and GamepadProvider's
// exact pure-virtual signatures have shifted a few times over the years
// (index moved into PlatformGamepad in 2014; EventMakesGamepadsVisible was
// added to the connect/activity callbacks later). The classes below target
// a reasonably current WebKit. If the build fails in this block, diff it
// against Source/WebCore/platform/gamepad/PlatformGamepad.h and
// GamepadProvider.h / GamepadProviderClient.h in your tree — it should be a
// couple of signature tweaks, not a redesign.
// =============================================================================
#if ENABLE(GAMEPAD)
namespace WebCore {

// ─── WKCGamepad — represents ONE controller ──────────────────────────────────
class WKCGamepad final : public PlatformGamepad {
public:
    explicit WKCGamepad(unsigned index)
        : PlatformGamepad(index)
    {
        m_connectTime = m_lastUpdateTime = MonotonicTime::now();
    }
    ~WKCGamepad() final = default;

    // Called every time main.cpp delivers a new frame of input for this pad.
    void updateValues(const String& id, long long /*timestampInSec*/,
        int naxes, const float* axes, int nbuttons, const float* buttons)
    {
        m_id = id;

        if ((int)m_axisValues.size() != naxes)
            m_axisValues.resize(naxes);
        for (int i = 0; i < naxes; i++)
            m_axisValues[i] = (double)axes[i];

        if ((int)m_buttonValues.size() != nbuttons)
            m_buttonValues.resize(nbuttons);
        for (int i = 0; i < nbuttons; i++)
            m_buttonValues[i] = (double)buttons[i];

        m_lastUpdateTime = MonotonicTime::now();
    }

    // ── PlatformGamepad overrides ───────────────────────────────────────────
    const String& id() const final { return m_id; }
    const String& mapping() const final { return m_mapping; }
    const Vector<double>& axisValues() const final { return m_axisValues; }
    const Vector<double>& buttonValues() const final { return m_buttonValues; }
    const char* source() const final { return "WKC"; }

    // No haptics peer hooked up on bare-metal WiiU — empty set tells
    // GamepadHapticActuator.playEffect() to resolve as unsupported.
    Vector<GamepadHapticEffectType> supportedEffectTypes() const { return { }; }

private:
    String         m_id;
    // Leave empty for "no standard mapping" per spec, or set to "standard"_s
    // if main.cpp fills axes[]/buttons[] in the canonical W3C order
    // (A/B/X/Y, two sticks, two triggers, d-pad) — see notifyGamepadEvent().
    String         m_mapping;
    Vector<double> m_axisValues;
    Vector<double> m_buttonValues;
};

// ─── WKCGamepadProvider — the registry all controllers live in ──────────────
class WKCGamepadProvider final : public GamepadProvider {
    friend class NeverDestroyed<WKCGamepadProvider>;
public:
    static WKCGamepadProvider& singleton()
    {
        static NeverDestroyed<WKCGamepadProvider> provider;
        // Register through WebCore's provider registry. Defining a second
        // GamepadProvider::singleton() here collides with upstream WebCore.
        GamepadProvider::setSharedProvider(provider.get());
        return provider;
    }

    // WKCWebView::initializeGamepads(int num) — pre-size the slot table.
    // Slots stay disconnected (nullptr) until updateGamepad() is first
    // called for that index, which is what fires "gamepadconnected".
    void setSlotCount(int num)
    {
        if (num < 0) num = 0;
        // Grow only — shrinking would invalidate indices main.cpp still uses.
        while ((int)m_gamepads.size() < num)
            m_gamepads.append(nullptr);
        rebuildGamepadRefs();
    }

    // WKCWebView::notifyGamepadEvent(...) — one frame of input for one pad.
    void updateGamepad(int index, const String& id, long long timestampInSec,
        int naxes, const float* axes, int nbuttons, const float* buttons)
    {
        if (index < 0) return;
        if ((int)m_gamepads.size() <= index)
            setSlotCount(index + 1);

        bool isNewConnection = !m_gamepads[index];
        if (isNewConnection)
            m_gamepads[index] = makeUnique<WKCGamepad>((unsigned)index);

        auto* pad = m_gamepads[index].get();
        pad->updateValues(id, timestampInSec, naxes, axes, nbuttons, buttons);

        if (isNewConnection) {
            rebuildGamepadRefs();
            for (auto* client : m_clients)
                client->platformGamepadConnected(*pad, EventMakesGamepadsVisible::Yes);
        } else {
            for (auto* client : m_clients)
                client->platformGamepadInputActivity(EventMakesGamepadsVisible::Yes);
        }
    }

    // Call when a controller is physically unplugged / desynced, so the page
    // gets "gamepaddisconnected" instead of a stale frozen state.
    void disconnectGamepad(int index)
    {
        if (index < 0 || index >= (int)m_gamepads.size() || !m_gamepads[index])
            return;
        auto pad = WTFMove(m_gamepads[index]);
        rebuildGamepadRefs();
        for (auto* client : m_clients)
            client->platformGamepadDisconnected(*pad);
    }

    // ── GamepadProvider overrides ───────────────────────────────────────────
    void startMonitoringGamepads(GamepadProviderClient& client) final { m_clients.add(&client); }
    void stopMonitoringGamepads(GamepadProviderClient& client) final { m_clients.remove(&client); }
    const Vector<WeakPtr<PlatformGamepad>>& platformGamepads() final { return m_gamepadRefs; }

private:
    WKCGamepadProvider() = default;

    void rebuildGamepadRefs()
    {
        m_gamepadRefs.clear();
        m_gamepadRefs.reserveInitialCapacity(m_gamepads.size());
        for (auto& pad : m_gamepads)
            m_gamepadRefs.append(pad ? WeakPtr<PlatformGamepad> { *pad } : nullptr);
    }

    Vector<std::unique_ptr<WKCGamepad>>  m_gamepads;
    Vector<WeakPtr<PlatformGamepad>>     m_gamepadRefs;
    HashSet<GamepadProviderClient*>      m_clients;
};

// The provider is registered through GamepadProvider::setSharedProvider()
// when WKCGamepadProvider::singleton() is first requested. This leaves the
// generic singleton definition owned by upstream WebCore.

} // namespace WebCore
#endif // ENABLE(GAMEPAD)

// =============================================================================
namespace WKC {
// =============================================================================

// ─── WKCWebViewPrivate ────────────────────────────────────────────────────────

WKCWebViewPrivate::WKCWebViewPrivate(WKCWebView* parent, WKCClientBuilders& builders)
    : m_parent(parent)
    , m_clientBuilders(builders)
{
    m_corePage        = nullptr;
    m_wkcCorePage     = nullptr;
    m_mainFrame       = nullptr;
    m_inspector       = nullptr;
    m_dropdownlist    = nullptr;
    m_settings        = nullptr;
    m_chromeClient    = nullptr;

    m_offscreen       = nullptr;
    m_drawContext     = nullptr;

#ifdef USE_WKC_CAIRO
    m_offscreenFormat   = 0;
    m_offscreenBitmap   = nullptr;
    m_offscreenRowBytes = 0;
    m_offscreenSize.fWidth  = 0;
    m_offscreenSize.fHeight = 0;
#endif

    m_isZoomFullContent = true;
    m_isTransparent     = false;
    m_loadStatus        = ELoadStatusNone;
    m_opticalZoomLevel  = 1.f;
    WKCFloatPoint_Set(&m_opticalZoomOffset, 0, 0);

    m_encoding        = nullptr;
    m_customEncoding  = nullptr;
    m_selectionText   = nullptr;

    m_focusedNode         = nullptr;
    m_nodeFromPoint       = nullptr;
    m_editableNode        = nullptr;
    m_lastNodeUnderMouse  = nullptr;
    m_rootGraphicsLayer   = nullptr;

    m_editable          = true;
    m_forceTerminated   = false;
}

WKCWebViewPrivate::~WKCWebViewPrivate()
{
    if (m_forceTerminated)
        return;

    delete m_focusedNode;
    delete m_nodeFromPoint;
    delete m_editableNode;

    if (m_offscreen) {
        wkcOffscreenDeletePeer(m_offscreen);
        m_offscreen = nullptr;
    }
    if (m_drawContext) {
        wkcDrawContextDeletePeer(m_drawContext);
        m_drawContext = nullptr;
    }

    if (m_encoding)       { WTF::fastFree(m_encoding);       m_encoding      = nullptr; }
    if (m_customEncoding) { WTF::fastFree(m_customEncoding); m_customEncoding = nullptr; }
    if (m_selectionText)  { WTF::fastFree(m_selectionText);  m_selectionText  = nullptr; }

    if (m_wkcCorePage) {
        delete m_wkcCorePage;
        m_wkcCorePage = nullptr;
    }
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_inspector) {
        m_inspector->disconnectFrontend();
        m_inspector = nullptr;
    }
#endif
    if (m_corePage) {
        // Detach main frame before destroying the page
        if (auto* lf = localMainFrame())
            lf->loader().detachFromParent();
        // Release the ref leaked in construct(); drops refcount to 0.
        m_corePage->deref();
        m_corePage = nullptr;
        // m_mainFrame freed automatically by corePage teardown
    }
    if (m_dropdownlist) {
        delete m_dropdownlist;
        m_dropdownlist = nullptr;
    }
    // m_inspector owned by PageConfiguration / Page in modern WebKit
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
}

WKCWebViewPrivate*
WKCWebViewPrivate::create(WKCWebView* parent, WKCClientBuilders& builders)
{
    auto* self = new WKCWebViewPrivate(parent, builders);
    if (!self) return nullptr;
    if (!self->construct()) {
        delete self;
        return nullptr;
    }
    return self;
}

// -----------------------------------------------------------------------------
// construct() — creates WebCore::Page with all WKC clients
//
// NOTE: PageConfiguration field names and whether clients are raw-pointer or
// UniqueRef varies by WebKit version.  The pattern below targets a reasonably
// modern WebKit (post-2020) where clients are passed as UniqueRef.
// If your webkit-wiiu fork still uses Page::PageClients, see the comment block
// at the bottom of this function for the older pattern.
// -----------------------------------------------------------------------------
bool
WKCWebViewPrivate::construct()
{
#if ENABLE(GAMEPAD)
    // Register the Wii U peer-event implementation before the page can expose
    // navigator.getGamepads() to scripts.
    (void)WebCore::WKCGamepadProvider::singleton();
#endif
    // ── PageConfiguration ──────────────────────────────────────────────────
    // Modern WebCore::PageConfiguration has no default constructor and its
    // client fields are UniqueRef (or unique_ptr for dragClient). Start from a
    // fully-populated empty-client configuration, then override the clients WKC
    // supplies. The WKC clients expose create() factories (private ctors), so
    // wrap the returned raw pointers via makeUniqueRefFromNonNullUniquePtr.
    auto pageConfiguration = WebCore::pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());

    m_chromeClient = ChromeClientWKC::create(this);
    if (!m_chromeClient)
        return false;
    pageConfiguration.chromeClient =
        WTF::makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<WebCore::ChromeClient>(m_chromeClient));
#if ENABLE(JAVASCRIPT_DEBUGGER)
    auto inspectorBackendClient = std::make_unique<InspectorBackendClientWKC>(*m_chromeClient);
    m_inspector = inspectorBackendClient.get();
    pageConfiguration.inspectorBackendClient = WTFMove(inspectorBackendClient);
#endif
#if ENABLE(CONTEXT_MENUS)
    pageConfiguration.contextMenuClient =
        WTF::makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<WebCore::ContextMenuClient>(ContextMenuClientWKC::create(this)));
#endif
    pageConfiguration.editorClient =
        WTF::makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<WebCore::EditorClient>(EditorClientWKC::create(this)));
#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient =
        std::unique_ptr<WebCore::DragClient>(DragClientWKC::create(this));
#endif

    // Page is RefCounted with a private constructor; create() returns Ref<Page>.
    // WKC owns the page via a manual raw pointer, so leak one ref here and
    // release it with deref() in the destructor.
    m_corePage = &WebCore::Page::create(WTFMove(pageConfiguration)).leakRef();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_inspector)
        m_inspector->setPage(*m_corePage);
#endif

    m_wkcCorePage = new PagePrivate(m_corePage);

#if ENABLE(NOTIFICATIONS)
    // Page's controller keeps a non-owning NotificationClient pointer. The
    // private view retains the bridge until Page teardown has destroyed it.
    m_notificationClient = std::make_unique<NotificationClientWKC>(*m_chromeClient);
    WebCore::provideNotification(m_corePage, m_notificationClient.get());
#endif

    // ── Supplement clients (geolocation, device orientation) ───────────────
#if ENABLE(GEOLOCATION)
    {
        auto* geolocationClient = GeolocationClientWKC::create(this);
        if (geolocationClient)
            WebCore::provideGeolocationTo(*m_corePage, *geolocationClient);
    }
#endif
#if ENABLE(DEVICE_ORIENTATION)
    {
        auto* deviceMotionClient = DeviceMotionClientWKC::create(this);
        if (deviceMotionClient)
            WebCore::provideDeviceMotionTo(*m_corePage, *deviceMotionClient);

        auto* deviceOrientationClient = DeviceOrientationClientWKC::create(this);
        if (deviceOrientationClient)
            WebCore::provideDeviceOrientationTo(*m_corePage, *deviceOrientationClient);
    }
#endif

    // ── Main frame ─────────────────────────────────────────────────────────
    m_mainFrame = WKCWebFrame::create(this, m_clientBuilders);
    if (!m_mainFrame) return false;
    m_mainFrame->privateFrame()->core()->init();

    // ── Drop-down list client ──────────────────────────────────────────────
    m_dropdownlist = DropDownListClientWKC::create(this);
    if (!m_dropdownlist) return false;

    // ── WKC settings wrapper ───────────────────────────────────────────────
    m_settings = new WKCSettings(this);
    if (!m_settings) return false;

    // ── WebCore::Settings defaults ─────────────────────────────────────────
    // Use page.settings() in modern WebKit (returns Settings& not Settings*).
    auto& s = m_corePage->settings();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    // Inspector protocol access is still opt-in at the WKCWebView API level;
    // enabling developer extras makes WebCore's Inspector controller functional.
    s.setDeveloperExtrasEnabled(true);
#endif

    s.setDefaultTextEncodingName("UTF-8"_s);
    s.setSerifFontFamily("Times New Roman"_s);
    s.setFixedFontFamily("Courier New"_s);
    s.setSansSerifFontFamily("Arial"_s);
    s.setStandardFontFamily("Times New Roman"_s);
    s.setLoadsImagesAutomatically(true);
    s.setShrinksStandaloneImagesToFit(true);
    s.setShouldPrintBackgrounds(true);
    s.setScriptEnabled(true);
    s.setSpatialNavigationEnabled(true);
    s.setImagesEnabled(true);
    s.setMediaEnabled(true);
    // Web Storage is needed by modern sites for preferences and application state.
    s.setLocalStorageEnabled(true);
    s.setTextAreasAreResizable(true);
    s.setJavaScriptCanOpenWindowsAutomatically(false);
    s.setJavaScriptCanAccessClipboard(false);
    s.setAllowUniversalAccessFromFileURLs(false);
    s.setDOMPasteAllowed(false);
    s.setNeedsSiteSpecificQuirks(false);
    s.setDefaultFixedFontSize(14);
    s.setDefaultFontSize(14);
    s.setAuthorAndUserStylesEnabled(true);
    s.setAcceleratedCompositingEnabled(false);
    // WebGL is off for bare-metal WiiU — no GPU command queue hooked up
    s.setWebGLEnabled(false);
    // setDNSPrefetchingEnabled / setWebAudioEnabled / setHyperlinkAuditingEnabled
    // were removed from modern WebCore::Settings.
    s.setLayoutFallbackWidth(1024);
    s.setAllowsInlineMediaPlayback(true);

#if ENABLE(VIDEO_TRACK)
    s.setShouldDisplaySubtitles(true);
    s.setShouldDisplayCaptions(true);
    s.setShouldDisplayTextDescriptions(true);
#endif

    // IndexedDB — the old call here was
    // WebCore::RuntimeEnabledFeatures::setWebkitIndexedDBEnabled(true).
    // RuntimeEnabledFeatures is gone; IndexedDB is now a normal Settings.yaml
    // entry ("IndexedDBAPIEnabled") generated into Settings.h the same way
    // as everything else in this function.
    s.setIndexedDBAPIEnabled(true);

    // MediaStream — the old call here was
    // WebCore::RuntimeEnabledFeatures::setMediaStreamEnabled(true).
    // WebKit removed the MediaStreamEnabled preference entirely (the commit
    // message says "this flag is no longer in use, does not guard any
    // feature"). getUserMedia()/RTCPeerConnection are unconditionally
    // present in modern WebKit now — there is nothing to toggle, and adding
    // a setMediaStreamEnabled() call here will fail to compile. If you want
    // camera/mic actually working you'd implement a UserMediaClient/
    // MediaStreamCenter peer for libogc, which is a separate, much bigger
    // task than a Settings flag — not needed just to unblock page loading.

    // Clipping — set on the view once it exists
    // LocalFrameView::setClipsRepaints was removed from modern WebCore.

    return true;

    /*
     * ── OLDER PageClients FALLBACK ─────────────────────────────────────────
     * If your webkit-wiiu fork still has Page::PageClients, replace the block
     * above starting at "PageConfiguration pageConfig" with:
     *
     *   WebCore::Page::PageClients cli;
     *   cli.chromeClient    = chromeClient.release();
     *   cli.editorClient    = editorClient.release();
     *   cli.inspectorClient = inspectorClient.release();
     *   // ... etc
     *   m_corePage = new WebCore::Page(cli);
     * ──────────────────────────────────────────────────────────────────────
     */
}

// ─── Helper: safely get the main LocalFrame ──────────────────────────────────
WebCore::LocalFrame*
WKCWebViewPrivate::localMainFrame() const
{
    if (!m_corePage) return nullptr;
    auto& f = m_corePage->mainFrame();
    // Modern WebKit has Frame with LocalFrame / RemoteFrame subclasses.
    // For this bare-metal port every frame is local.
    return dynamicDowncast<WebCore::LocalFrame>(f);
}

bool
WKCWebViewPrivate::connectInspectorFrontend()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    return m_inspector && m_inspector->connectFrontend();
#else
    return false;
#endif
}

void
WKCWebViewPrivate::disconnectInspectorFrontend()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_inspector)
        m_inspector->disconnectFrontend();
#endif
}

void
WKCWebViewPrivate::dispatchInspectorMessageFromFrontend(const WTF::String& message)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_inspector)
        m_inspector->dispatchMessageFromFrontend(message);
#else
    (void)message;
#endif
}

// ─── Force-terminate notification ────────────────────────────────────────────
void
WKCWebViewPrivate::notifyForceTerminate()
{
    m_forceTerminated = true;
    if (m_mainFrame)
        m_mainFrame->notifyForceTerminate();
}

// ─── Accessors ────────────────────────────────────────────────────────────────
WKCSettings*
WKCWebViewPrivate::settings()
{
    return m_settings;
}

WKC::Page*
WKCWebViewPrivate::wkcCore() const
{
    return &m_wkcCorePage->wkc();
}

const WebCore::IntSize& WKCWebViewPrivate::desktopSize()        const { return m_desktopSize; }
const WebCore::IntSize& WKCWebViewPrivate::viewSize()           const { return m_viewSize; }
const WebCore::IntSize& WKCWebViewPrivate::defaultDesktopSize() const { return m_defaultDesktopSize; }
const WebCore::IntSize& WKCWebViewPrivate::defaultViewSize()    const { return m_defaultViewSize; }

// opticalZoomLevel/opticalZoomOffset/editable/setEditable are defined inline
// in WKCWebViewPrivate.h.

// ─── Offscreen setup ─────────────────────────────────────────────────────────
bool
WKCWebViewPrivate::setOffscreen(OffscreenFormat format, void* bitmap, int rowbytes,
    const WebCore::IntSize& offscreensize, const WebCore::IntSize& viewsize,
    bool fixedlayout, const WebCore::IntSize* desktopsize, bool needsLayout)
{
    int pformat = 0;
    WKCSize size;

    if (!desktopsize)
        desktopsize = &offscreensize;

    if (m_drawContext) { wkcDrawContextDeletePeer(m_drawContext); m_drawContext = nullptr; }
    if (m_offscreen)   { wkcOffscreenDeletePeer(m_offscreen);   m_offscreen   = nullptr; }

    switch (format) {
    case EOffscreenFormatRGBA5650:     pformat = WKC_OFFSCREEN_TYPE_RGBA5650;     break;
    case EOffscreenFormatARGB8888:     pformat = WKC_OFFSCREEN_TYPE_ARGB8888;     break;
    case EOffscreenFormatPolygon:      pformat = WKC_OFFSCREEN_TYPE_POLYGON;      break;
    case EOffscreenFormatCairo16:      pformat = WKC_OFFSCREEN_TYPE_CAIRO16;      break;
    case EOffscreenFormatCairo32:      pformat = WKC_OFFSCREEN_TYPE_CAIRO32;      break;
    case EOffscreenFormatCairoSurface: pformat = WKC_OFFSCREEN_TYPE_CAIROSURFACE; break;
    default: return false;
    }

    size.fWidth  = offscreensize.width();
    size.fHeight = offscreensize.height();

#ifdef USE_WKC_CAIRO
    m_offscreenFormat   = pformat;
    m_offscreenBitmap   = bitmap;
    m_offscreenRowBytes = rowbytes;
    m_offscreenSize     = size;
#endif

    m_offscreen = wkcOffscreenNewPeer(pformat, bitmap, rowbytes, &size);
    if (!m_offscreen) return false;

    m_drawContext = wkcDrawContextNewPeer(m_offscreen);
    if (!m_drawContext) return false;

    if (needsLayout) {
        m_desktopSize        = *desktopsize;
        m_defaultDesktopSize = *desktopsize;
        m_defaultViewSize    = viewsize;
        m_viewSize           = viewsize;

        auto* frame = localMainFrame();
        if (!frame || !frame->view()) return false;

        if (fixedlayout) {
            frame->view()->setUseFixedLayout(true);
            frame->view()->setFixedLayoutSize(viewsize);
        } else {
            frame->view()->setUseFixedLayout(false);
        }
        frame->view()->resize(desktopsize->width(), desktopsize->height());
        frame->view()->forceLayout();
        frame->view()->adjustViewSize();
    }

    return true;
}

// ─── Resize ───────────────────────────────────────────────────────────────────
void
WKCWebViewPrivate::notifyResizeDesktopSize(const WebCore::IntSize& size, bool sendresizeevent)
{
    m_desktopSize = size;
    auto* frame = localMainFrame();
    if (!frame || !frame->view()) return;
    frame->view()->resize(size.width(), size.height());
    // EventHandler::sendResizeEvent was removed; resize is handled by the view.
    (void)sendresizeevent;
    frame->view()->forceLayout();
    frame->view()->adjustViewSize();
    updateOverlay(WebCore::IntRect(), true);
}

void
WKCWebViewPrivate::notifyResizeViewSize(const WebCore::IntSize& size)
{
    auto* frame = localMainFrame();
    if (!frame || !frame->view()) return;
    m_viewSize = size;
    frame->view()->setFixedLayoutSize(size);
}

// ─── Layout ───────────────────────────────────────────────────────────────────
void
WKCWebViewPrivate::notifyRelayout()
{
    auto* frame = localMainFrame();
    if (!frame || !frame->contentRenderer() || !frame->view()) return;
    frame->view()->updateLayoutAndStyleIfNeededRecursive();
}

// ─── Paint ────────────────────────────────────────────────────────────────────
void
WKCWebViewPrivate::notifyPaintOffscreen(const WebCore::IntRect& rect)
{
#ifdef USE_WKC_CAIRO
    if (!recoverFromCairoError()) return;
#else
    if (!m_offscreen) return;
#endif

    auto* frame = localMainFrame();
    if (!frame || !frame->contentRenderer() || !frame->view()) return;

    auto ctxPtr = WebCore::createGraphicsContextWKC(m_drawContext);
    if (!ctxPtr) return;
    auto& ctx = *ctxPtr;
    wkcOffscreenBeginPaintPeer(m_offscreen);
    WebCore::FloatRect cr(rect.x(), rect.y(), rect.width(), rect.height());
    ctx.save();
#if USE(WKC_CAIRO)
    wkcDrawContextSetOpticalZoomPeer(m_drawContext, m_opticalZoomLevel, &m_opticalZoomOffset);
#endif
    ctx.clip(cr);
    frame->view()->paint(ctx, rect);
    if (m_overlayList && !m_rootGraphicsLayer)
        m_overlayList->paintOffscreen(ctx);
    ctx.restore();
    wkcOffscreenEndPaintPeer(m_offscreen);
}

void
WKCWebViewPrivate::notifyPaintOffscreenFrom(const WebCore::IntRect& rect, const WKCPoint& p)
{
#ifdef USE_WKC_CAIRO
    if (!recoverFromCairoError()) return;
#else
    if (!m_offscreen) return;
#endif

    auto* frame = localMainFrame();
    if (!frame || !frame->contentRenderer() || !frame->view()) return;

    auto ctxPtr = WebCore::createGraphicsContextWKC(m_drawContext);
    if (!ctxPtr) return;
    auto& ctx = *ctxPtr;
    wkcOffscreenBeginPaintPeer(m_offscreen);

    WebCore::IntRect scrolledRect = rect;
    scrolledRect.move(p.fX, p.fY);
    float transX = (float)p.fX;
    float transY = (float)p.fY;
    WebCore::FloatRect cr(
        scrolledRect.x()      - transX,
        scrolledRect.y()      - transY,
        scrolledRect.width(),
        scrolledRect.height());

    ctx.save();
#ifdef USE_WKC_CAIRO
    wkcDrawContextSetOpticalZoomPeer(m_drawContext, m_opticalZoomLevel, &m_opticalZoomOffset);
#endif
    ctx.clip(cr);
    ctx.translate(-transX, -transY);
    frame->view()->paintContents(ctx, scrolledRect);
    if (m_overlayList && !m_rootGraphicsLayer) {
        WebCore::FloatRect visibleRect = frame->view()->visibleContentRect();
        ctx.translate(visibleRect.x(), visibleRect.y());
        m_overlayList->paintOffscreen(ctx);
    }
    ctx.restore();
    wkcOffscreenEndPaintPeer(m_offscreen);
}

#ifdef USE_WKC_CAIRO
#include <cairo.h>
void
WKCWebViewPrivate::notifyPaintToContext(const WebCore::IntRect& rect, void* context)
{
    auto* frame = localMainFrame();
    if (!frame || !frame->contentRenderer() || !frame->view()) return;
    WebCore::GraphicsContext ctx((cairo_t*)context);
    WebCore::FloatRect cr(rect.x(), rect.y(), rect.width(), rect.height());
    ctx.save();
    ctx.clip(cr);
    frame->view()->paint(ctx, rect);
    ctx.restore();
}
#endif

void
WKCWebViewPrivate::notifyScrollOffscreen(const WebCore::IntRect& rect, const WebCore::IntSize& diff)
{
    if (!m_offscreen) return;
    WKCRect r = { rect.x(), rect.y(), rect.width(), rect.height() };
    WKCSize d = { diff.width(), diff.height() };
    wkcOffscreenBeginPaintPeer(m_offscreen);
    wkcOffscreenScrollPeer(m_offscreen, &r, &d);
    wkcOffscreenEndPaintPeer(m_offscreen);
}

void
WKCWebViewPrivate::notifyServiceScriptedAnimations()
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    auto* frame = localMainFrame();
    if (!frame || !frame->view()) return;
    // MonotonicTime replaces the old DOMTimeStamp helper
    frame->view()->serviceScriptedAnimations(MonotonicTime::now());
#endif
}

// ─── Transparency / zoom ─────────────────────────────────────────────────────
void
WKCWebViewPrivate::setTransparent(bool flag)
{
    m_isTransparent = flag;
    auto* frame = localMainFrame();
    if (frame && frame->view())
        frame->view()->setTransparent(flag);
}

void
WKCWebViewPrivate::setOpticalZoom(float zoomLevel, const WKCFloatPoint& offset)
{
    if (!m_offscreen) return;
    m_opticalZoomLevel  = zoomLevel;
    m_opticalZoomOffset = offset;
    wkcOffscreenSetOpticalZoomPeer(m_offscreen, zoomLevel, &offset);
}

// ─── Rendering quality ───────────────────────────────────────────────────────
void
WKCWebViewPrivate::setUseAntiAliasForDrawings(bool flag)
{
    if (!m_offscreen) return;
    wkcOffscreenSetUseAntiAliasForPolygonPeer(m_offscreen, flag);
}

// static
void
WKCWebViewPrivate::setUseAntiAliasForCanvas(bool flag)
{
#ifndef USE_WKC_CAIRO
    WebCore::ImageBufferData::setUseAA(flag);
#endif
}

void
WKCWebViewPrivate::setUseBilinearForScaledImages(bool flag)
{
    if (!m_offscreen) return;
    wkcOffscreenSetUseInterpolationForImagePeer(m_offscreen, flag);
}

// static
void
WKCWebViewPrivate::setUseBilinearForCanvasImages(bool flag)
{
#ifndef USE_WKC_CAIRO
    WebCore::ImageBufferData::setUseBilinear(flag);
#endif
}

// ─── Scroll ───────────────────────────────────────────────────────────────────
void
WKCWebViewPrivate::setScrollPositionForOffscreen(const WebCore::IntPoint& pos)
{
    if (!m_offscreen) return;
    const WKCPoint wpos = { pos.x(), pos.y() };
    wkcOffscreenSetScrollPositionPeer(m_offscreen, &wpos);
}

void
WKCWebViewPrivate::notifyScrollPositionChanged()
{
    auto* frame = localMainFrame();
    // LocalFrameView::scrollPositionChanged is now private and the peer has
    // already applied the scroll; nothing to forward to WebCore here.
    (void)frame;
}

// ─── Cairo error recovery ─────────────────────────────────────────────────────
#ifdef USE_WKC_CAIRO
bool
WKCWebViewPrivate::recoverFromCairoError()
{
    if (!m_offscreenBitmap) return false;
    if (!m_offscreen || wkcOffscreenIsErrorPeer(m_offscreen) ||
        !m_drawContext || wkcDrawContextIsErrorPeer(m_drawContext)) {
        if (m_drawContext) { wkcDrawContextDeletePeer(m_drawContext); m_drawContext = nullptr; }
        if (m_offscreen)   { wkcOffscreenDeletePeer(m_offscreen);   m_offscreen   = nullptr; }
        m_offscreen = wkcOffscreenNewPeer(m_offscreenFormat, m_offscreenBitmap,
                                          m_offscreenRowBytes, &m_offscreenSize);
        if (!m_offscreen) return false;
        m_drawContext = wkcDrawContextNewPeer(m_offscreen);
        if (!m_drawContext) {
            wkcOffscreenDeletePeer(m_offscreen);
            m_offscreen = nullptr;
            return false;
        }
    }
    return true;
}
#endif

// ─── Node / hit-test helpers ─────────────────────────────────────────────────
WKC::Node*
WKCWebViewPrivate::getFocusedNode()
{
    auto* frame = localMainFrame();
    if (!frame || !frame->document()) return nullptr;
    auto* node = frame->document()->focusedElement();
    if (!node) return nullptr;
    if (!m_focusedNode || m_focusedNode->webcore() != node) {
        delete m_focusedNode;
        m_focusedNode = NodePrivate::create(node);
    }
    return &m_focusedNode->wkc();
}

WKC::Node*
WKCWebViewPrivate::getNodeFromPoint(int x, int y)
{
    auto* frame = localMainFrame();
    WebCore::Node* node = nullptr;

    while (frame) {
        auto* doc  = frame->document();
        auto* view = frame->view();
        if (!doc || !view) break;

        WebCore::IntPoint docPoint = view->windowToContents(WebCore::IntPoint(x, y));
        auto* rv = doc->renderView();
        if (!rv) break;

        WebCore::HitTestRequest request({ WebCore::HitTestRequest::Type::ReadOnly,
                                          WebCore::HitTestRequest::Type::Active });
        WebCore::HitTestResult result(docPoint);
        rv->layer()->hitTest(request, result);
        node = result.innerNode();
        while (node && !node->isElementNode())
            node = node->parentNode();
        if (node) {
            if (auto* host = node->shadowHost())
                node = host;
        }
        // Recurse into sub-frames
        auto subframe = WebCore::EventHandler::subframeForTargetNode(node);
        frame = subframe ? dynamicDowncast<WebCore::LocalFrame>(subframe.get()) : nullptr;
    }

    if (!node) return nullptr;
    if (!m_nodeFromPoint || m_nodeFromPoint->webcore() != node) {
        delete m_nodeFromPoint;
        m_nodeFromPoint = NodePrivate::create(node);
    }
    return &m_nodeFromPoint->wkc();
}

WKC::Node*
WKCWebViewPrivate::findNeighboringEditableNode(WKC::WKCFocusDirection direction)
{
    if (direction != FocusDirectionForward && direction != FocusDirectionBackward)
        return nullptr;

    auto& fc = m_corePage->focusController();

    // FocusController's non-mutating focusable-node finder (and FocusScope) were
    // removed from modern WebCore; the only public traversal entry point is
    // advanceFocus(), which moves focus. Advance (bounded, to avoid cycling)
    // until we land on an editable element, then return it.
    WebCore::Element* startElement = nullptr;
    if (auto* frame = fc.focusedOrMainFrame())
        startElement = frame->document() ? frame->document()->focusedElement() : nullptr;

    for (unsigned guard = 0; guard < 4096; ++guard) {
        if (!fc.advanceFocus(static_cast<WebCore::FocusDirection>(direction), nullptr))
            return nullptr;
        auto* frame = fc.focusedOrMainFrame();
        auto* document = frame ? frame->document() : nullptr;
        auto* element = document ? document->focusedElement() : nullptr;
        if (!element || element == startElement)
            return nullptr;
        if (element->isTextFormControlElement() || element->hasEditableStyle()) {
            auto* node = static_cast<WebCore::Node*>(element);
            if (!m_editableNode || m_editableNode->webcore() != node) {
                delete m_editableNode;
                m_editableNode = NodePrivate::create(node);
            }
            return &m_editableNode->wkc();
        }
    }
    return nullptr;
}

// ─── Overlays ─────────────────────────────────────────────────────────────────
void WKCWebViewPrivate::addOverlay(WKCOverlayIf* overlay, int zOrder, int fixedDirectionFlag)
{
    if (!m_overlayList)
        m_overlayList = WKCOverlayList::create(this);
    m_overlayList->add(overlay, zOrder, fixedDirectionFlag);
}

void WKCWebViewPrivate::removeOverlay(WKCOverlayIf* overlay)
{
    if (m_overlayList && m_overlayList->remove(overlay) && m_overlayList->empty())
        m_overlayList = nullptr;
}

void WKCWebViewPrivate::updateOverlay(const WebCore::IntRect& rect, bool immediate)
{
    if (m_overlayList)
        m_overlayList->update(rect, immediate);
}

// =============================================================================
// WKCWebView — public implementation
// =============================================================================

bool WKCWebView::m_clipsRepaints = true;

WKCWebView::WKCWebView()
    : m_private(nullptr)
{
}

WKCWebView::~WKCWebView()
{
    if (m_private) {
        if (!m_private->m_forceTerminated)
            stopLoading();
        delete m_private;
        m_private = nullptr;
    }
}

WKCWebView*
WKCWebView::create(WKCClientBuilders& builders)
{
    auto* self = new WKCWebView();
    if (!self) return nullptr;
    if (!self->construct(builders)) {
        delete self;
        return nullptr;
    }
    return self;
}

bool
WKCWebView::construct(WKCClientBuilders& builders)
{
    m_private = WKCWebViewPrivate::create(this, builders);
    return m_private != nullptr;
}

void
WKCWebView::deleteWKCWebView(WKCWebView* self)
{
    delete self;
}

void
WKCWebView::notifyForceTerminate()
{
    if (m_private)
        m_private->notifyForceTerminate();
}

// ─── Offscreen drawing ───────────────────────────────────────────────────────

bool
WKCWebView::setOffscreen(OffscreenFormat format, void* bitmap, int rowbytes,
    const WKCSize& offscreensize, const WKCSize& viewsize,
    bool fixedlayout, const WKCSize* const desktopsize, bool needsLayout)
{
    WebCore::IntSize os(offscreensize.fWidth, offscreensize.fHeight);
    WebCore::IntSize vs(viewsize.fWidth, viewsize.fHeight);
    WebCore::IntSize ds;
    if (desktopsize) {
        ds.setWidth(desktopsize->fWidth);
        ds.setHeight(desktopsize->fHeight);
    }
    return m_private->setOffscreen(format, bitmap, rowbytes, os, vs,
                                   fixedlayout, desktopsize ? &ds : nullptr, needsLayout);
}

void WKCWebView::notifyResizeViewSize(const WKCSize& size)
{
    m_private->notifyResizeViewSize(WebCore::IntSize(size.fWidth, size.fHeight));
}

void WKCWebView::notifyResizeDesktopSize(const WKCSize& size, bool sendresizeevent)
{
    m_private->notifyResizeDesktopSize(WebCore::IntSize(size.fWidth, size.fHeight), sendresizeevent);
}

void WKCWebView::notifyRelayout(bool force)
{
    if (force) {
        auto* frame = m_private->localMainFrame();
        if (frame && frame->document() && frame->document()->renderView())
            frame->document()->renderView()->setNeedsLayout();
    }
    m_private->notifyRelayout();
}

void WKCWebView::notifyPaintOffscreenFrom(const WKCRect& rect, const WKCPoint& p)
{
    m_private->notifyPaintOffscreenFrom(
        WebCore::IntRect(rect.fPoint.fX, rect.fPoint.fY, rect.fSize.fWidth, rect.fSize.fHeight), p);
}

void WKCWebView::notifyPaintOffscreen(const WKCRect& rect)
{
    m_private->notifyPaintOffscreen(
        WebCore::IntRect(rect.fPoint.fX, rect.fPoint.fY, rect.fSize.fWidth, rect.fSize.fHeight));
}

#ifdef USE_WKC_CAIRO
void WKCWebView::notifyPaintToContext(const WKCRect& rect, void* context)
{
    m_private->notifyPaintToContext(
        WebCore::IntRect(rect.fPoint.fX, rect.fPoint.fY, rect.fSize.fWidth, rect.fSize.fHeight), context);
}
#endif

void WKCWebView::notifyScrollOffscreen(const WKCRect& rect, const WKCSize& diff)
{
    m_private->notifyScrollOffscreen(
        WebCore::IntRect(rect.fPoint.fX, rect.fPoint.fY, rect.fSize.fWidth, rect.fSize.fHeight),
        WebCore::IntSize(diff.fWidth, diff.fHeight));
}

void WKCWebView::notifyServiceScriptedAnimations()
{
    m_private->notifyServiceScriptedAnimations();
}

// ─── WKC → WebCore platform-event adapters ───────────────────────────────────
// The historical PlatformKeyboardEventWKC / PlatformMouseEventWKC /
// PlatformWheelEventWKC constructors (which took an opaque void*) were removed;
// build the modern platform events directly from the WKC peer event structs.
namespace {

static OptionSet<WebCore::PlatformEvent::Modifier> toPlatformModifiers(WKC::Modifier m)
{
    OptionSet<WebCore::PlatformEvent::Modifier> mods;
    if (m & WKC::EModifierCtrl)  mods.add(WebCore::PlatformEvent::Modifier::ControlKey);
    if (m & WKC::EModifierShift) mods.add(WebCore::PlatformEvent::Modifier::ShiftKey);
    if (m & WKC::EModifierAlt)   mods.add(WebCore::PlatformEvent::Modifier::AltKey);
    if (m & WKC::EModifierMod1)  mods.add(WebCore::PlatformEvent::Modifier::MetaKey);
    return mods;
}

static WebCore::PlatformKeyboardEvent toPlatformKeyboardEvent(const WKC::WKCKeyEvent& ev)
{
    using Type = WebCore::PlatformEvent::Type;
    Type type = Type::RawKeyDown;
    switch (ev.m_type) {
    case WKC::EKeyEventReleased: type = Type::KeyUp; break;
    case WKC::EKeyEventChar:     type = Type::Char; break;
    case WKC::EKeyEventPressed:
    default:                     type = Type::RawKeyDown; break;
    }
    WTF::String text;
    if (ev.m_char) {
        char16_t c = static_cast<char16_t>(ev.m_char);
        text = WTF::String(std::span<const char16_t>(&c, 1));
    }
    return WebCore::PlatformKeyboardEvent(type, text, text, WTF::String(), WTF::String(),
        WTF::String(), static_cast<int>(ev.m_key), ev.m_autoRepeat, /*isKeypad*/ false,
        /*isSystemKey*/ false, toPlatformModifiers(ev.m_modifiers), WTF::MonotonicTime::now());
}

static WebCore::MouseButton toPlatformMouseButton(WKC::MouseButton b)
{
    switch (b) {
    case WKC::EMouseButtonLeft:   return WebCore::MouseButton::Left;
    case WKC::EMouseButtonMiddle: return WebCore::MouseButton::Middle;
    case WKC::EMouseButtonRight:  return WebCore::MouseButton::Right;
    default:                      return WebCore::MouseButton::None;
    }
}

static WebCore::PlatformMouseEvent toPlatformMouseEvent(const WKC::WKCMouseEvent& ev)
{
    using Type = WebCore::PlatformEvent::Type;
    Type type = Type::MouseMoved;
    int clickCount = 1;
    switch (ev.m_type) {
    case WKC::EMouseEventDown:        type = Type::MousePressed; break;
    case WKC::EMouseEventUp:          type = Type::MouseReleased; break;
    case WKC::EMouseEventDoubleClick: type = Type::MousePressed; clickCount = 2; break;
    default:                          type = Type::MouseMoved; break;
    }
    WebCore::DoublePoint pos(ev.m_x, ev.m_y);
    return WebCore::PlatformMouseEvent(pos, pos, toPlatformMouseButton(ev.m_button), type,
        clickCount, toPlatformModifiers(ev.m_modifiers), WTF::MonotonicTime::now(), 0.0,
        WebCore::SyntheticClickType::NoTap, WebCore::MouseEventInputSource::UserDriven);
}

static WebCore::PlatformWheelEvent toPlatformWheelEvent(const WKC::WKCWheelEvent& ev)
{
    WebCore::IntPoint pos(ev.m_x, ev.m_y);
    return WebCore::PlatformWheelEvent(pos, pos,
        static_cast<float>(ev.m_dx), static_cast<float>(ev.m_dy),
        static_cast<float>(ev.m_dx) / 120.0f, static_cast<float>(ev.m_dy) / 120.0f,
        WebCore::PlatformWheelEventGranularity::ScrollByPixelWheelEvent,
        (ev.m_modifiers & WKC::EModifierShift) != 0, (ev.m_modifiers & WKC::EModifierCtrl) != 0,
        (ev.m_modifiers & WKC::EModifierAlt) != 0, (ev.m_modifiers & WKC::EModifierMod1) != 0);
}

} // anonymous namespace

// ─── Input events ────────────────────────────────────────────────────────────

bool WKCWebView::notifyKeyPress(WKC::Key key, WKC::Modifier modifiers, bool in_autorepeat)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) return false;
    WKC::WKCKeyEvent ev;
    ev.m_type = WKC::EKeyEventPressed;
    ev.m_key  = key;
    ev.m_modifiers = modifiers;
    ev.m_char = 0;
    ev.m_autoRepeat = in_autorepeat;
    WebCore::PlatformKeyboardEvent kev = toPlatformKeyboardEvent(ev);
    return frame->eventHandler().keyEvent(kev);
}

bool WKCWebView::notifyKeyRelease(WKC::Key key, WKC::Modifier modifiers)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) return false;
    WKC::WKCKeyEvent ev;
    ev.m_type = WKC::EKeyEventReleased;
    ev.m_key  = key;
    ev.m_modifiers = modifiers;
    ev.m_char = 0;
    ev.m_autoRepeat = false;
    WebCore::PlatformKeyboardEvent kev = toPlatformKeyboardEvent(ev);
    return frame->eventHandler().keyEvent(kev);
}

bool WKCWebView::notifyKeyChar(unsigned int in_char)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) return false;
    WKC::WKCKeyEvent ev;
    ev.m_type = WKC::EKeyEventChar;
    ev.m_char = in_char;
    ev.m_key  = (WKC::Key)0;
    ev.m_autoRepeat = false;
    WebCore::PlatformKeyboardEvent kev = toPlatformKeyboardEvent(ev);
    return frame->eventHandler().keyEvent(kev);
}

bool WKCWebView::notifyIMEComposition(const unsigned short* in_string,
    WKC::CompositionUnderline* in_underlines, unsigned int in_underlineNum,
    unsigned int in_cursorPosition, unsigned int in_selectionEnd, bool in_confirm)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;

    // Convert the null-terminated UTF-16 peer string to a WTF::String.
    unsigned strLen = 0;
    if (in_string)
        while (in_string[strLen]) ++strLen;
    WTF::String composition(std::span<const char16_t>(reinterpret_cast<const char16_t*>(in_string), strLen));

    if (in_confirm) {
        frame->editor().confirmComposition(composition);
    } else {
        WTF::Vector<WebCore::CompositionUnderline> underlines;
        if (in_underlineNum > 0) {
            underlines.resize(in_underlineNum);
            for (unsigned i = 0; i < in_underlineNum; i++) {
                underlines[i].startOffset = in_underlines[i].startOffset;
                underlines[i].endOffset   = in_underlines[i].endOffset;
                underlines[i].thick       = in_underlines[i].thick;
                unsigned c = in_underlines[i].color;
                underlines[i].color = WebCore::Color(WebCore::SRGBA<uint8_t> {
                    static_cast<uint8_t>((c >> 16) & 0xff),
                    static_cast<uint8_t>((c >> 8) & 0xff),
                    static_cast<uint8_t>(c & 0xff),
                    static_cast<uint8_t>((c >> 24) & 0xff) });
            }
        }
        // Modern setComposition also takes highlights and annotation maps.
        frame->editor().setComposition(composition, underlines, { }, { }, in_cursorPosition, in_selectionEnd);
    }
    return true;
}

bool WKCWebView::notifyMouseDown(const WKCPoint& pos, WKC::MouseButton button, WKC::Modifier modifiers)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;
    WKC::WKCMouseEvent ev;
    ev.m_type   = WKC::EMouseEventDown;
    ev.m_button = button;
    ev.m_x = pos.fX; ev.m_y = pos.fY;
    ev.m_modifiers = modifiers;
    ev.m_timestampinsec = wkcGetTickCountPeer() / 1000;
    WebCore::PlatformMouseEvent mev = toPlatformMouseEvent(ev);
    return frame->eventHandler().handleMousePressEvent(mev).wasHandled();
}

bool WKCWebView::notifyMouseUp(const WKCPoint& pos, WKC::MouseButton button, Modifier modifiers)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;
    WKC::WKCMouseEvent ev;
    ev.m_type   = WKC::EMouseEventUp;
    ev.m_button = button;
    ev.m_x = pos.fX; ev.m_y = pos.fY;
    ev.m_modifiers = modifiers;
    ev.m_timestampinsec = wkcGetTickCountPeer() / 1000;
    WebCore::PlatformMouseEvent mev = toPlatformMouseEvent(ev);
    return frame->eventHandler().handleMouseReleaseEvent(mev).wasHandled();
}

bool WKCWebView::notifyMouseMove(const WKCPoint& pos, WKC::MouseButton button, Modifier modifiers)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) return false;
    WKC::WKCMouseEvent ev;
    ev.m_type   = WKC::EMouseEventMove;
    ev.m_button = button;
    ev.m_x = pos.fX; ev.m_y = pos.fY;
    ev.m_modifiers = modifiers;
    ev.m_timestampinsec = wkcGetTickCountPeer() / 1000;
    WebCore::PlatformMouseEvent mev = toPlatformMouseEvent(ev);
    return frame->eventHandler().mouseMoved(mev).wasHandled();
}

bool WKCWebView::notifyMouseDoubleClick(const WKCPoint& pos, WKC::MouseButton button, WKC::Modifier modifiers)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;
    WKC::WKCMouseEvent ev;
    ev.m_type   = WKC::EMouseEventDoubleClick;
    ev.m_button = button;
    ev.m_x = pos.fX; ev.m_y = pos.fY;
    ev.m_modifiers = modifiers;
    ev.m_timestampinsec = wkcGetTickCountPeer() / 1000;
    WebCore::PlatformMouseEvent mev = toPlatformMouseEvent(ev);
    return frame->eventHandler().handleMousePressEvent(mev).wasHandled();
}

bool WKCWebView::notifyMouseWheel(const WKCPoint& pos, const WKCSize& diff, WKC::Modifier modifiers)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;
    WKC::WKCWheelEvent ev;
    ev.m_dx = diff.fWidth;
    ev.m_dy = diff.fHeight;
    ev.m_x  = pos.fX;
    ev.m_y  = pos.fY;
    ev.m_modifiers = modifiers;
    WebCore::PlatformWheelEvent wev = toPlatformWheelEvent(ev);
    constexpr OptionSet<WebCore::WheelEventProcessingSteps> wheelSteps {
        WebCore::WheelEventProcessingSteps::SynchronousScrolling,
        WebCore::WheelEventProcessingSteps::BlockingDOMEventDispatch };
    return frame->eventHandler().handleWheelEvent(wev, wheelSteps).first.wasHandled();
}

void WKCWebView::notifySetMousePressed(bool pressed)
{
    auto* frame = m_private->localMainFrame();
    // EventHandler::setMousePressed was removed; no public setter exists.
    (void)frame; (void)pressed;
}

void WKCWebView::notifyLostMouseCapture()
{
    auto* frame = m_private->localMainFrame();
    if (frame) frame->eventHandler().lostMouseCapture();
}

bool WKCWebView::notifyTouchEvent(int type, const TouchPoint* points, int npoints, WKC::Modifier in_modifiers)
{
#if ENABLE(TOUCH_EVENTS)
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;
    WKC::WKCTouchEvent ev = {};
    WTF::Vector<WKCTouchPoint> tp(npoints);
    for (int i = 0; i < npoints; i++) {
        tp[i].m_id    = points[i].fId;
        tp[i].m_state = points[i].fState;
        WKCPoint_SetPoint(&tp[i].m_pos, &points[i].fPoint);
    }
    ev.m_type    = type;
    ev.m_points  = tp.data();
    ev.m_npoints = npoints;
    ev.m_modifiers = in_modifiers;
    ev.m_timestampinsec = wkcGetTickCountPeer() / 1000;
    WebCore::PlatformTouchEvent tev((void*)&ev);
    return frame->eventHandler().handleTouchEvent(tev);
#else
    return false;
#endif
}

bool WKCWebView::notifyScroll(WKC::ScrollType type)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;

    // ScrollDirection / ScrollGranularity enums are scoped in modern WebKit
    using SD = WebCore::ScrollDirection;
    using SG = WebCore::ScrollGranularity;

    SD dir = SD::ScrollUp;
    SG gra = SG::Line;

    switch (type) {
    case EScrollUp:        dir = SD::ScrollUp;    gra = SG::Line;     break;
    case EScrollDown:      dir = SD::ScrollDown;  gra = SG::Line;     break;
    case EScrollLeft:      dir = SD::ScrollLeft;  gra = SG::Line;     break;
    case EScrollRight:     dir = SD::ScrollRight; gra = SG::Line;     break;
    case EScrollPageUp:    dir = SD::ScrollUp;    gra = SG::Page;     break;
    case EScrollPageDown:  dir = SD::ScrollDown;  gra = SG::Page;     break;
    case EScrollPageLeft:  dir = SD::ScrollLeft;  gra = SG::Page;     break;
    case EScrollPageRight: dir = SD::ScrollRight; gra = SG::Page;     break;
    case EScrollTop:       dir = SD::ScrollUp;    gra = SG::Document; break;
    case EScrollBottom:    dir = SD::ScrollDown;  gra = SG::Document; break;
    default: return false;
    }

    if (!frame->eventHandler().scrollOverflow(dir, gra))
        frame->view()->scroll(dir, gra);
    return true;
}

bool WKCWebView::notifyScroll(int dx, int dy)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) return false;
    frame->view()->scrollBy(WebCore::IntSize(dx, dy));
    m_private->updateOverlay(WebCore::IntRect(), true);
    return true;
}

bool WKCWebView::notifyScrollTo(int x, int y)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) return false;
    frame->view()->setScrollPosition(WebCore::IntPoint(x, y));
    m_private->updateOverlay(WebCore::IntRect(), true);
    return true;
}

void WKCWebView::scrollPosition(WKCPoint& pos)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) { pos.fX = pos.fY = 0; return; }
    auto p = frame->view()->scrollPosition();
    pos.fX = p.x(); pos.fY = p.y();
}

void WKCWebView::minimumScrollPosition(WKCPoint& pos) const
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) { pos.fX = pos.fY = 0; return; }
    auto p = frame->view()->minimumScrollPosition();
    pos.fX = p.x(); pos.fY = p.y();
}

void WKCWebView::maximumScrollPosition(WKCPoint& pos) const
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) { pos.fX = pos.fY = 0; return; }
    auto p = frame->view()->maximumScrollPosition();
    pos.fX = p.x(); pos.fY = p.y();
}

void WKCWebView::contentsSize(WKCSize& size)
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->view()) { size.fWidth = size.fHeight = 0; return; }
    auto s = frame->view()->contentsSize();
    size.fWidth = s.width(); size.fHeight = s.height();
}

// ─── Focus ────────────────────────────────────────────────────────────────────

void WKCWebView::notifyFocusIn()
{
    auto& fc = m_private->m_corePage->focusController();
    fc.setActive(true);
    if (fc.focusedFrame())
        fc.setFocused(true);
    else {
        fc.setFocused(true);
        fc.setFocusedFrame(m_private->localMainFrame());
    }
}

void WKCWebView::notifyFocusOut()
{
    auto& fc = m_private->m_corePage->focusController();
    fc.setActive(false);
    fc.setFocused(false);
}

void WKCWebView::notifyScrollPositionChanged()
{
    m_private->notifyScrollPositionChanged();
}

WKC::Node* WKCWebView::findFocusableNode(const WKC::FocusDirection direction, const WKCRect* specificRect)
{
    if (!m_private->wkcCore()->focusController()) return nullptr;
    return m_private->wkcCore()->focusController()->findNextFocusableNode(direction, specificRect);
}

WKC::Node* WKCWebView::findFocusableNodeInRect(const WKC::FocusDirection direction, const WKCRect* rect, bool enableContainer)
{
    if (!m_private->wkcCore()->focusController()) return nullptr;
    return m_private->wkcCore()->focusController()->findNextFocusableNodeInRect(
        direction, m_private->m_mainFrame->core(), rect, enableContainer);
}

WKC::Node* WKCWebView::findNearestFocusableNodeFromPoint(const WKCPoint point, const WKCRect* rect)
{
    if (!m_private->wkcCore()->focusController()) return nullptr;
    return m_private->wkcCore()->focusController()->findNearestFocusableNodeFromPoint(point, rect);
}

WKC::Node* WKCWebView::findNeighboringEditableNode(WKC::WKCFocusDirection direction)
{
    return m_private->findNeighboringEditableNode(direction);
}

bool WKCWebView::setFocusedNode(WKC::Node* inode)
{
    auto& fc = m_private->m_corePage->focusController();
    auto* coreNode = inode ? inode->priv().webcore() : nullptr;

    if (coreNode) {
        auto* focusedFrame = fc.focusedOrMainFrame();
        auto* focusedDoc   = focusedFrame ? focusedFrame->document() : nullptr;
        auto* newDoc       = &coreNode->document();
        if (focusedDoc && newDoc != focusedDoc)
            focusedDoc->setFocusedElement(nullptr);
        if (newDoc && newDoc->frame())
            fc.setFocusedFrame(newDoc->frame());
    }

    auto* targetFrame = fc.focusedOrMainFrame();
    auto* targetDoc   = targetFrame ? targetFrame->document() : nullptr;
    if (!targetDoc) return false;
    return targetDoc->setFocusedElement(
        coreNode && coreNode->isElementNode() ? downcast<WebCore::Element>(coreNode) : nullptr);
}

void WKCWebView::notifySuspend()  { /* not implemented */ }
void WKCWebView::notifyResume()   { /* not implemented */ }

void WKCWebView::notifyChromeVisible(bool)
{
    // Chrome::setChromeVisible / chromeVisible were removed from modern WebCore.
}

bool WKCWebView::chromeVisible()
{
    // Chrome::chromeVisible was removed; the WKC embedder owns chrome visibility.
    return true;
}

// ─── Navigation ───────────────────────────────────────────────────────────────

const unsigned short* WKCWebView::title() { return m_private->m_mainFrame->title(); }
const char*           WKCWebView::uri()   { return m_private->m_mainFrame->uri(); }

bool WKCWebView::canGoBack()
{
    return m_private->m_corePage->backForward().canGoBackOrForward(-1);
}
bool WKCWebView::canGoBackOrForward(int steps)
{
    return m_private->m_corePage->backForward().canGoBackOrForward(steps);
}
bool WKCWebView::canGoForward()
{
    return m_private->m_corePage->backForward().canGoBackOrForward(1);
}
bool WKCWebView::goBack()
{
    return m_private->m_corePage->backForward().goBack();
}
void WKCWebView::goBackOrForward(int steps)
{
    m_private->m_corePage->backForward().goBackOrForward(steps);
}
bool WKCWebView::goForward()
{
    return m_private->m_corePage->backForward().goForward();
}

void WKCWebView::stopLoading()
{
    auto* frame = m_private->localMainFrame();
    if (frame) frame->loader().stopForUserCancel();
}

void WKCWebView::reload()
{
    auto* frame = m_private->localMainFrame();
    if (frame) frame->loader().reload();
}

void WKCWebView::reloadBypassCache()
{
    auto* frame = m_private->localMainFrame();
    // ReloadOption::FromOrigin is the modern equivalent of reloadBypassingCache
    if (frame) frame->loader().reload(WebCore::ReloadOption::FromOrigin);
}

void WKCWebView::loadURI(const char* uri, const char* referer)
{
    // A view can exist briefly before its main frame is attached, and teardown
    // may call into the public API after that frame has been released. Treat
    // both states as a rejected navigation instead of dereferencing a null frame.
    if (!uri || !uri[0] || !m_private || !m_private->m_mainFrame)
        return;

    m_private->m_mainFrame->loadURI(uri, referer);
}

/**
 * Normalize address-bar input before handing it to WebCore.
 *
 * Bare host names are treated as HTTPS URLs; ordinary text is sent to Google
 * Search. URL encoding is done on UTF-8 bytes so non-ASCII search terms remain
 * valid and no browser-side JavaScript is needed.
 */
bool WKCWebView::loadURLOrSearch(const char* input, const char* referer)
{
    if (!input || !m_private || !m_private->m_mainFrame)
        return false;

    std::string value(input);
    auto isSpace = [](unsigned char c) { return !!std::isspace(c); };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    if (value.empty())
        return false;

    std::string target;
    if (value.compare(0, 8, "https://") == 0
        || value.compare(0, 7, "http://") == 0
        || value.compare(0, 7, "about:") == 0
        || value.compare(0, 5, "file:") == 0
        || value.find("://") != std::string::npos) {
        target = value;
    } else if (value.compare(0, 2, "//") == 0) {
        target = "https:" + value;
    } else {
        // A dotted hostname, localhost, or bracketed IP is an address rather
        // than a search phrase. HTTPS is the safe default for bare hosts.
        bool looksLikeHost = value.find('.') != std::string::npos
            || value.compare(0, 9, "localhost") == 0
            || value[0] == '[';
        if (looksLikeHost) {
            target = "https://" + value;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            target = "https://www.google.com/search?q=";
            for (unsigned char c : value) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                    || (c >= '0' && c <= '9') || c == '-' || c == '_'
                    || c == '.' || c == '~') {
                    target += static_cast<char>(c);
                } else {
                    target += '%';
                    target += hex[c >> 4];
                    target += hex[c & 0x0F];
                }
            }
        }
    }

    // Validate the final target with WebKit's own URL parser before
    // reporting success to the browser UI. ResourceHandle only supports web
    // protocols here; rejecting malformed/unsupported targets prevents a
    // broken address-bar value from looking like a successful navigation.
    WTF::URL parsedTarget { WTF::URL(), WTF::String::fromUTF8(target) };
    if (!parsedTarget.isValid())
        return false;

    if (!parsedTarget.protocolIsInHTTPFamily()
        && !parsedTarget.protocolIs("about")
        && !parsedTarget.protocolIs("file"))
        return false;

    auto normalizedTarget = parsedTarget.string().utf8();
    if (normalizedTarget.isEmpty())
        return false;

    loadURI(normalizedTarget.data(), referer);
    return true;
}

void WKCWebView::loadString(const char* content, const unsigned short* mimetype,
                             const unsigned short* encoding, const char* base_uri)
{
    if (!content || !content[0] || !m_private) return;
    m_private->m_mainFrame->loadString(content, mimetype, encoding, base_uri);
}

void WKCWebView::loadHTMLString(const char* content, const char* base_uri)
{
    static const unsigned short cTextHtml[] = { 't','e','x','t','/','h','t','m','l',0 };
    loadString(content, cTextHtml, nullptr, base_uri);
}

// ─── Search ───────────────────────────────────────────────────────────────────

bool WKCWebView::searchText(const unsigned short* text, bool case_sensitive, bool forward, bool wrap)
{
    // Modern find API uses OptionSet<FindOption> instead of the old
    // TextCaseSensitivity / FindDirection / ShouldWrap enums.
    WebCore::FindOptions opts;
    if (!case_sensitive) opts.add(WebCore::FindOption::CaseInsensitive);
    if (!forward)        opts.add(WebCore::FindOption::Backwards);
    if (wrap)            opts.add(WebCore::FindOption::WrapAround);
    return m_private->m_corePage->findString(WTF::String(text), opts).range.has_value();
}

unsigned int WKCWebView::markTextMatches(const unsigned short* string, bool case_sensitive, unsigned int limit)
{
    WebCore::FindOptions opts;
    if (!case_sensitive) opts.add(WebCore::FindOption::CaseInsensitive);
    return m_private->m_corePage->markAllMatchesForText(WTF::String(string), opts, false, limit);
}

void WKCWebView::setHighlightTextMatches(bool highlight)
{
    for (auto* frame = m_private->localMainFrame(); frame; ) {
        frame->editor().setMarkedTextMatchesAreHighlighted(highlight);
        auto* next = frame->tree().traverseNext(WebCore::CanWrap::Yes);
        frame = next ? downcast<WebCore::LocalFrame>(next) : nullptr;
    }
}

void WKCWebView::unmarkTextMatches()
{
    m_private->m_corePage->unmarkAllTextMatches();
}

// ─── Frame accessors ─────────────────────────────────────────────────────────

static WKCWebFrame*
kit(WebCore::LocalFrame* frame)
{
    if (!frame) return nullptr;
    auto* client = static_cast<FrameLoaderClientWKC*>(&frame->loader().client());
    return client ? client->webFrame() : nullptr;
}

WKCWebFrame* WKCWebView::mainFrame()
{
    return m_private->m_mainFrame;
}

WKCWebFrame* WKCWebView::focusedFrame()
{
    auto* f = m_private->m_corePage->focusController().focusedFrame();
    auto* lf = dynamicDowncast<WebCore::LocalFrame>(f);
    return lf ? kit(lf) : nullptr;
}

// ─── Script ───────────────────────────────────────────────────────────────────

void WKCWebView::executeScript(const char* script)
{
    auto* frame = m_private->localMainFrame();
    if (frame)
        frame->script().executeScriptIgnoringException(WTF::String::fromUTF8(script), JSC::SourceTaintedOrigin::Untainted, true);
}

bool WKCWebView::connectInspectorFrontend()
{
    return m_private && m_private->connectInspectorFrontend();
}

void WKCWebView::disconnectInspectorFrontend()
{
    if (m_private)
        m_private->disconnectInspectorFrontend();
}

void WKCWebView::dispatchInspectorMessageFromFrontend(const char* message)
{
    if (!m_private || !message)
        return;

    m_private->dispatchInspectorMessageFromFrontend(WTF::String::fromUTF8(message));
}

// ─── Selection ────────────────────────────────────────────────────────────────

bool WKCWebView::hasSelection()
{
    auto* frame = m_private->m_corePage->focusController().focusedOrMainFrame();
    if (!frame) return false;
    return frame->selection().selection().isRange();
}

void WKCWebView::clearSelection()
{
    for (auto* frame = m_private->localMainFrame(); frame; ) {
        frame->selection().clear();
        auto* next = frame->tree().traverseNext();
        frame = next ? downcast<WebCore::LocalFrame>(next) : nullptr;
    }
}

void WKCWebView::selectAll()
{
    auto* frame = m_private->m_corePage->focusController().focusedOrMainFrame();
    if (frame) frame->selection().selectAll();
}

const unsigned short* WKCWebView::selectionText()
{
    auto* frame = m_private->m_corePage->focusController().focusedOrMainFrame();
    if (!frame) return nullptr;
    auto range = frame->selection().selection().toNormalizedRange();
    if (!range) return nullptr;
    if (m_private->m_selectionText) {
        WTF::fastFree(m_private->m_selectionText);
        m_private->m_selectionText = nullptr;
    }
    WTF::String text = WebCore::plainText(*range);
    auto chars = text.charactersWithNullTermination();
    if (!chars)
        return nullptr;
    m_private->m_selectionText = wkc_wstrdup(reinterpret_cast<const unsigned short*>(chars.value().span().data()));
    return m_private->m_selectionText;
}

WKCRect WKCWebView::selectionBoundingBox(bool textonly, bool useSelectionHeight)
{
    // Simplified: return the union of selection rects
    auto* frame = m_private->m_corePage->focusController().focusedOrMainFrame();
    if (!frame) { WKCRect r = {0,0,0,0}; return r; }
    auto range = frame->selection().selection().toNormalizedRange();
    if (!range) { WKCRect r = {0,0,0,0}; return r; }
    WebCore::IntRect result;
    for (auto& rect : WebCore::RenderObject::absoluteTextRects(*range, useSelectionHeight ? OptionSet<WebCore::RenderObject::BoundingRectBehavior> { WebCore::RenderObject::BoundingRectBehavior::UseSelectionHeight } : OptionSet<WebCore::RenderObject::BoundingRectBehavior> { }))
        result.unite(rect);
    WKCRect r = { result.x(), result.y(), result.width(), result.height() };
    return r;
}

// ─── Core / settings accessors ───────────────────────────────────────────────

WKC::Page*       WKCWebView::core()     { return m_private->wkcCore(); }
WKCSettings*     WKCWebView::settings() { return m_private->settings(); }

bool WKCWebView::canShowMimeType(const unsigned short* mime_type)
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return false;
    return frame->loader().client().canShowMIMEType(WTF::String(mime_type));
}

float WKCWebView::zoomLevel()
{
    auto* frame = m_private->localMainFrame();
    return frame ? frame->pageZoomFactor() : 1.0f;
}

float WKCWebView::setZoomLevel(float zoom_level)
{
    auto* frame = m_private->localMainFrame();
    if (frame) {
        wkcOffscreenClearGlyphCachePeer();
        frame->setPageZoomFactor(zoom_level);
    }
    return zoom_level;
}

void WKCWebView::zoomIn(float ratio)  { setZoomLevel(zoomLevel() + ratio); }
void WKCWebView::zoomOut(float ratio) { setZoomLevel(zoomLevel() - ratio); }

float WKCWebView::textOnlyZoomLevel()
{
    auto* frame = m_private->localMainFrame();
    return frame ? frame->textZoomFactor() : 1.0f;
}

float WKCWebView::setTextOnlyZoomLevel(float zoom_level)
{
    auto* frame = m_private->localMainFrame();
    if (frame) { wkcOffscreenClearGlyphCachePeer(); frame->setTextZoomFactor(zoom_level); }
    return zoom_level;
}

void WKCWebView::textOnlyZoomIn(float ratio)  { setTextOnlyZoomLevel(textOnlyZoomLevel() + ratio); }
void WKCWebView::textOnlyZoomOut(float ratio) { setTextOnlyZoomLevel(textOnlyZoomLevel() - ratio); }

bool WKCWebView::fullContentZoom() { return m_private->m_isZoomFullContent; }
void WKCWebView::setFullContentZoom(bool full_content_zoom)
{
    if (m_private->m_isZoomFullContent == full_content_zoom) return;
    float cur = zoomLevel();
    m_private->m_isZoomFullContent = full_content_zoom;
    setZoomLevel(cur);
}

float WKCWebView::opticalZoomLevel() const               { return m_private->opticalZoomLevel(); }
const WKCFloatPoint& WKCWebView::opticalZoomOffset() const { return m_private->opticalZoomOffset(); }

float WKCWebView::setOpticalZoom(float zoom_level, const WKCFloatPoint& offset)
{
    m_private->setOpticalZoom(zoom_level, offset);
    return zoom_level;
}

void WKCWebView::viewSize(WKCSize& size) const
{
    const auto& s = m_private->viewSize();
    size.fWidth = s.width(); size.fHeight = s.height();
}

const unsigned short* WKCWebView::encoding()
{
    auto* frame = m_private->localMainFrame();
    if (!frame || !frame->document()) return nullptr;
    WTF::String enc = frame->document()->characterSetWithUTF8Fallback();
    if (enc.isEmpty()) return nullptr;
    if (m_private->m_encoding) { WTF::fastFree(m_private->m_encoding); m_private->m_encoding = nullptr; }
    auto encChars = enc.charactersWithNullTermination();
    if (!encChars) return nullptr;
    m_private->m_encoding = wkc_wstrdup(reinterpret_cast<const unsigned short*>(encChars.value().span().data()));
    return m_private->m_encoding;
}

void WKCWebView::setCustomEncoding(const unsigned short* encoding)
{
    auto* frame = m_private->localMainFrame();
    if (frame) frame->loader().reloadWithOverrideEncoding(WTF::String(encoding));
}

const unsigned short* WKCWebView::customEncoding()
{
    auto* frame = m_private->localMainFrame();
    if (!frame) return nullptr;
    WTF::String override = frame->loader().documentLoader()
        ? frame->loader().documentLoader()->overrideEncoding() : WTF::String();
    if (override.isEmpty()) return nullptr;
    if (m_private->m_customEncoding) { WTF::fastFree(m_private->m_customEncoding); m_private->m_customEncoding = nullptr; }
    auto ovChars = override.charactersWithNullTermination();
    if (!ovChars) return nullptr;
    m_private->m_customEncoding = wkc_wstrdup(reinterpret_cast<const unsigned short*>(ovChars.value().span().data()));
    return m_private->m_customEncoding;
}

WKC::LoadStatus WKCWebView::loadStatus() { return m_private->m_loadStatus; }

double WKCWebView::progress()
{
    return m_private->m_corePage->progress().estimatedProgress();
}

// ─── Hit testing / node queries ──────────────────────────────────────────────

bool WKCWebView::hitTestResultForNode(const WKC::Node* node, WKC::HitTestResult& result)
{
    if (!node) return false;
    auto* n = node->priv().webcore();
    auto& ht = const_cast<WebCore::HitTestResult&>(result.priv()->webcore());
    // HitTestResult::setPoint was removed; the stored result already carries the point.
    ht.setInnerNode(n);
    ht.setInnerNonSharedNode(n);
    if (n->hasTagName(WebCore::HTMLNames::areaTag)) {
        auto img = downcast<WebCore::HTMLAreaElement>(*n).imageElement();
        if (img) ht.setInnerNonSharedNode(img.get());
    }
    auto* link = n->enclosingLinkEventParentOrSelf();
    if (link && link->isElementNode())
        ht.setURLElement(static_cast<WebCore::Element*>(link));
    return true;
}

WKC::Node* WKCWebView::getFocusedNode()         { return m_private->getFocusedNode(); }
WKC::Node* WKCWebView::getNodeFromPoint(int x, int y) { return m_private->getNodeFromPoint(x, y); }

bool WKCWebView::clickableFromPoint(int x, int y)
{
    auto* pnode = getNodeFromPoint(x, y);
    if (!pnode) return false;
    return pnode->priv().webcore()->hasEventListeners(WebCore::eventNames().clickEvent);
}

bool WKCWebView::draggableFromPoint(int x, int y)
{
    auto* pnode = getNodeFromPoint(x, y);
    if (!pnode) return false;
    auto* node = pnode->priv().webcore();
    bool hasmousedown = false;
    while (node) {
        if (!hasmousedown)
            hasmousedown = node->hasEventListeners(WebCore::eventNames().mousedownEvent);
        if (node->hasEventListeners(WebCore::eventNames().dragEvent)
         || node->hasEventListeners(WebCore::eventNames().dragstartEvent)
         || node->hasEventListeners(WebCore::eventNames().dragendEvent)
         || (hasmousedown && node->hasEventListeners(WebCore::eventNames().mousemoveEvent)))
            return true;
        if (node->isHTMLElement() && node->hasTagName(WebCore::HTMLNames::inputTag)) {
            if (auto* ie = dynamicDowncast<WebCore::HTMLInputElement>(*node); ie && ie->isRangeControl())
                return true;
        }
        node = node->parentNode();
    }
    return false;
}

// ─── Cache / memory ───────────────────────────────────────────────────────────

void WKCWebView::cachedSize(unsigned int& dead_resource, unsigned int& live_resource)
{
    // MemoryCache::liveSize/deadSize are no longer public; report the total
    // as dead (the WKC caller only uses these for reporting).
    auto& cache = WebCore::MemoryCache::singleton();
    live_resource = 0;
    dead_resource = cache.size();
}

void WKCWebView::clearCaches(bool clearhttpcache)
{
    auto& cache = WebCore::MemoryCache::singleton();

    // Do not set capacities to zero here: that permanently disables future
    // resource caching until some unrelated caller resets them. Evict unused
    // resources and drop decoded backing data from live resources instead,
    // preserving the memory budget initialized by WKCWebKitInitialize().
    cache.pruneDeadResourcesToSize(0);
    cache.pruneLiveResourcesToSize(0, true);

#if ENABLE(WKC_HTTPCACHE)
    if (WebCore::ResourceHandleManager::sharedInstance() && clearhttpcache)
        WebCore::ResourceHandleManager::sharedInstance()->clearHTTPCache();
#else
    // Modern WKC networking uses CurlContext rather than ResourceHandleManager.
    // There is no separate HTTP response disk-cache implementation to flush here;
    // a future persistent HTTP cache should expose its own real clearing API.
    UNUSED_PARAM(clearhttpcache);
#endif
    clearFontCache(true);
    clearCrossOriginPreflightResultCache();
}

size_t WKCWebView::fontDataCount()
    { return WebCore::FontCache::forCurrentThread().fontCount(); }
size_t WKCWebView::inactiveFontDataCount()
    { return WebCore::FontCache::forCurrentThread().inactiveFontCount(); }

void WKCWebView::clearFontCache(bool in_clearsAll)
{
    if (in_clearsAll)
        WebCore::FontCache::forCurrentThread().invalidate();
    else
        WebCore::FontCache::forCurrentThread().purgeInactiveFontData();
}

void WKCWebView::clearCrossOriginPreflightResultCache()
{
    // singleton() replaces the old shared() pattern
    WebCore::CrossOriginPreflightResultCache::singleton().clear();
}

// ─── Plugins folder ──────────────────────────────────────────────────────────
void WKCWebView::setPluginsFolder(const char*) { /* NPAPI plugins removed from modern WebKit */ }

// ─── Visibility ───────────────────────────────────────────────────────────────

void WKCWebView::setVisibilityState(int state, bool /*isInitialState*/)
{
    // Modern WebKit uses Page::setIsVisible(bool) / Page::setIsPrerender() etc.
    // Prerender / Preview states were removed; map everything to Visible/Hidden.
    bool visible = (state == PageVisibilityStateVisible);
    m_private->m_corePage->setIsVisible(visible);
}

// ─── Compositing ─────────────────────────────────────────────────────────────
void WKCWebView::enterCompositingMode()
{
    // No-op on bare-metal WiiU; compositing is disabled in Settings
}

// ─── Color format / rendering quality ────────────────────────────────────────

void WKCWebView::setInternalColorFormat(int fmt)
{
    switch (fmt) {
    case EInternalColorFormat8888:
        WebCore::ImageWKC::setInternalColorFormatARGB8888(false); break;
    case EInternalColorFormat8888or565:
    case EInternalColorFormat5515withMask:
        WebCore::ImageWKC::setInternalColorFormatARGB8888(true);  break;
    }
}

void WKCWebView::setUseAntiAliasForDrawings(bool f)    { m_private->setUseAntiAliasForDrawings(f); }
void WKCWebView::setUseAntiAliasForCanvas(bool f)      { WKCWebViewPrivate::setUseAntiAliasForCanvas(f); }
void WKCWebView::setUseBilinearForScaledImages(bool f) { m_private->setUseBilinearForScaledImages(f); }
void WKCWebView::setUseBilinearForCanvasImages(bool f) { WKCWebViewPrivate::setUseBilinearForCanvasImages(f); }

// ─── Cookies ─────────────────────────────────────────────────────────────────

void WKCWebView::setCookieEnabled(bool) { /* Page::setCookieEnabled removed; cookies controlled by NetworkStorageSession */ }
bool WKCWebView::cookieEnabled()             { return true; }

// ─── Visited links ────────────────────────────────────────────────────────────

bool WKCWebView::addVisitedLink(const char* uri, const unsigned short*, const struct tm*)
{
    if (!uri || !m_private->m_corePage) return false;
    WTF::URL url(WTF::URL(), WTF::String::fromUTF8(uri));
    m_private->m_corePage->visitedLinkStore().addVisitedLink(
        *m_private->m_corePage,
        WebCore::computeSharedStringHash(url.string()));
    return true;
}

bool WKCWebView::addVisitedLinkHash(LinkHash hash)
{
    if (!m_private->m_corePage) return false;
    m_private->m_corePage->visitedLinkStore().addVisitedLink(*m_private->m_corePage, hash);
    return true;
}

// ─── Permit / block network requests ─────────────────────────────────────────
void WKCWebView::permitSendRequest(void* handle, bool permit)
{
    auto* mgr = WebCore::ResourceHandleManager::sharedInstance();
    if (mgr) mgr->permitRequest(handle, permit);
}

// ─── Scroll position (offscreen) ─────────────────────────────────────────────
void WKCWebView::setScrollPositionForOffscreen(const WKCPoint& sp)
{
    m_private->setScrollPositionForOffscreen(WebCore::IntPoint(sp.fX, sp.fY));
}

// ─── Scroll helpers ───────────────────────────────────────────────────────────

void WKCWebView::scrollNodeByRecursively(WKC::Node* node, int dx, int dy)
{
    if (!node) return;
    auto* n = node->priv().webcore();
    auto* renderer = n->renderer();
    auto* layer = renderer ? renderer->enclosingLayer() : nullptr;
    auto* sa = layer ? layer->scrollableArea() : nullptr;
    if (sa) sa->scrollByRecursively(WebCore::IntSize(dx, dy));
}

void WKCWebView::scrollNodeBy(WKC::Node* node, int dx, int dy)
{
    if (!node) return;
    auto* n = node->priv().webcore();
    auto* renderer = n->renderer();
    auto* layer = renderer ? renderer->enclosingLayer() : nullptr;
    if (!layer) return;
    if (renderer->hasNonVisibleOverflow()) {
        if (auto* sa = layer->scrollableArea())
            sa->scrollToOffset(sa->scrollOffset() + WebCore::IntSize(dx, dy));
    } else {
        renderer->view().frameView().scrollBy(WebCore::IntSize(dx, dy));
    }
}

bool WKCWebView::isScrollableNode(const WKC::Node* node)
{
    if (!node) return false;
    auto* n = node->priv().webcore();
    auto* box = dynamicDowncast<WebCore::RenderBox>(n->renderer());
    if (!box) return false;
    return box->canBeScrolledAndHasScrollableArea() && n->hasChildNodes();
}

bool WKCWebView::canScrollNodeInDirection(const WKC::Node* node, WKC::WKCFocusDirection direction)
{
    if (!node) return false;
    auto* container = dynamicDowncast<WebCore::ContainerNode>(node->priv().webcore());
    if (!container) return false;
    return WebCore::canScrollInDirection(*container,
                                         static_cast<WebCore::FocusDirection>(direction));
}

// ─── Editable ─────────────────────────────────────────────────────────────────
bool WKCWebView::editable()             { return m_private->editable(); }
void WKCWebView::setEditable(bool flag) { m_private->setEditable(flag); }

// ─── Repaint throttling (stub) ────────────────────────────────────────────────
void WKCWebView::setRepaintThrottling(double, double, double, double) { /* stub */ }

// ─── FullScreen (stub) ────────────────────────────────────────────────────────
void WKCWebView::cancelFullScreen()    { /* stub */ }
bool WKCWebView::isFullScreen() const  { return false; }

// ─── Storage (stub) ───────────────────────────────────────────────────────────
void     WKCWebView::clearSessionStorage()                  { /* stub */ }
void     WKCWebView::clearLocalStorage()                    { /* stub */ }


// ─── Gamepad (W3C Gamepad API) ────────────────────────────────────────────────
//
// Called from main.cpp. Typical flow:
//   - once at startup: s_wkc_view->initializeGamepads(4)  // however many you
//     want to support — main.cpp can map GamePad + up to 4 Wii Remotes
//   - every frame: s_wkc_view->notifyGamepadEvent(index, "Wii U GamePad",
//       timestamp, naxes, axesArray, nbuttons, buttonsArray)
//     for whichever controller indices currently have a device attached.
//     A page only sees a controller in navigator.getGamepads() once you've
//     called notifyGamepadEvent() for that index at least once — sending no
//     events for an index just means "not connected," matching the spec.
//
// Button/axis ORDER matters if you want games to use the "standard" gamepad
// mapping (the layout sites assume by default — A/B/X/Y, two sticks, two
// triggers, d-pad). If main.cpp fills the buttons[]/axes[] arrays in that
// exact order, set m_mapping = "standard"_s in WKCGamepad's constructor;
// otherwise leave it empty and games will read raw indices without knowing
// what's what (works, just less plug-and-play for typical web games).

#if ENABLE(GAMEPAD)
void WKCWebView::initializeGamepads(int num)
{
    WebCore::WKCGamepadProvider::singleton().setSlotCount(num);
}

bool WKCWebView::notifyGamepadEvent(int index, const WKC::String& id, long long timestamp, int naxes,
                                     const float* axes, int nbuttons, const float* buttons)
{
    if (index < 0 || !axes || !buttons) return false;
    WebCore::WKCGamepadProvider::singleton().updateGamepad(
        index, WTF::String(id), timestamp, naxes, axes, nbuttons, buttons);
    return true;
}
#else
void WKCWebView::initializeGamepads(int)
{
}

bool WKCWebView::notifyGamepadEvent(int, const WKC::String&, long long, int,
                                     const float*, int, const float*)
{
    return false;
}
#endif // ENABLE(GAMEPAD)

// ─── JIT heap info ────────────────────────────────────────────────────────────
void WKCWebView::jsJITCodePageAllocatedBytes(size_t& a, size_t& t, size_t& m)
{
    a = t = m = 0;
}

// ─── RSS (stub) ───────────────────────────────────────────────────────────────
unsigned int WKCWebView::getRSSLinkNum()                             { return 0; }
unsigned int WKCWebView::getRSSLinkInfo(WKCRSSLinkInfo*, unsigned int) { return 0; }

// ─── JS extension events (stub — WiiU-specific) ──────────────────────────────
void WKCWebView::notifyJSExtensionEvent(JSExtensionEvent) const { /* stub */ }

// =============================================================================
// Global WKCWebKit functions
// =============================================================================

// ─── Memory / timer event handler wrappers ───────────────────────────────────

class WKCWebKitMemoryEventHandler {
public:
    explicit WKCWebKitMemoryEventHandler(WKCMemoryEventHandler& h) : m_h(h) {}
    ~WKCWebKitMemoryEventHandler() {}

    bool   checkMemoryAvailability(unsigned int size, bool forimage)
        { return m_h.checkMemoryAvailability(size, forimage); }
    bool   checkMemoryAllocatable(unsigned int size, WKCMemoryEventHandler::AllocationReason r)
        { return m_h.checkMemoryAllocatable(size, r); }
    void*  notifyMemoryExhaust(unsigned int size, unsigned int& allocated)
        { return m_h.notifyMemoryExhaust(size, allocated); }
    void   notifyMemoryAllocationError(unsigned int size, WKCMemoryEventHandler::AllocationReason r)
        { m_h.notifyMemoryAllocationError(size, r); }
    void   notifyCrash(const char* f, int l, const char* fn, const char* a)
        { m_h.notifyCrash(f, l, fn, a); }
    void   notifyStackOverflow(bool nr, unsigned ss, unsigned c, unsigned m,
               void* st, void* sb, void* cs, const char* f, int l, const char* fn)
        { m_h.notifyStackOverflow(nr, ss, c, m, st, sb, cs, f, l, fn); }
private:
    WKCMemoryEventHandler& m_h;
};

class WKCWebKitTimerEventHandler {
public:
    explicit WKCWebKitTimerEventHandler(WKCTimerEventHandler& h) : m_h(h) {}
    ~WKCWebKitTimerEventHandler() {}
    bool requestWakeUp(unsigned int ms, void* data) { return m_h.requestWakeUp(ms, data); }
private:
    WKCTimerEventHandler& m_h;
};

// Static storage (avoids heap allocation before the heap is initialized)
alignas(WKCWebKitMemoryEventHandler) static unsigned char
    gMemBuf[sizeof(WKCWebKitMemoryEventHandler)];
alignas(WKCWebKitTimerEventHandler)  static unsigned char
    gTimBuf[sizeof(WKCWebKitTimerEventHandler)];

static WKCWebKitMemoryEventHandler* gMemoryEventHandler =
    reinterpret_cast<WKCWebKitMemoryEventHandler*>(gMemBuf);
static WKCWebKitTimerEventHandler*  gTimerEventHandler  =
    reinterpret_cast<WKCWebKitTimerEventHandler*>(gTimBuf);

// ─── Peer callbacks ───────────────────────────────────────────────────────────

static void* WKCWebKitNotifyNoMemory(unsigned int request_size)
{
    unsigned int dummy = 0;
    WebCore::ResourceHandleManager::forceTerminateInstance();
    return gMemoryEventHandler->notifyMemoryExhaust(request_size, dummy);
}

static WKCMemoryEventHandler::AllocationReason
WKCWebKitConvertAllocationReason(int in_reason)
{
    switch (in_reason) {
    case WKC_MEMORYALLOC_TYPE_IMAGE_UNSPECIFIED:   return WKCMemoryEventHandler::ImageUnspecified;
    case WKC_MEMORYALLOC_TYPE_IMAGE_ANIMATED:      return WKCMemoryEventHandler::ImageAnimatedImage;
    case WKC_MEMORYALLOC_TYPE_IMAGE_JPEG:          return WKCMemoryEventHandler::ImageJpeg;
    case WKC_MEMORYALLOC_TYPE_IMAGE_PNG:           return WKCMemoryEventHandler::ImagePng;
    case WKC_MEMORYALLOC_TYPE_LAYER:               return WKCMemoryEventHandler::Layer;
    case WKC_MEMORYALLOC_TYPE_SHAREDBUFFER_GIF:    return WKCMemoryEventHandler::SharedBufferGif;
    case WKC_MEMORYALLOC_TYPE_SHAREDBUFFER_JPEG:   return WKCMemoryEventHandler::SharedBufferJpeg;
    case WKC_MEMORYALLOC_TYPE_SHAREDBUFFER_PNG:    return WKCMemoryEventHandler::SharedBufferPng;
    case WKC_MEMORYALLOC_TYPE_JAVASCRIPT_HEAP:     return WKCMemoryEventHandler::JavaScriptHeap;
    case WKC_MEMORYALLOC_TYPE_ASSEMBLERBUFFER:     return WKCMemoryEventHandler::AssemblerBuffer;
    default:                                       return WKCMemoryEventHandler::Normal;
    }
}

static void WKCWebKitNotifyMemoryAllocationError(unsigned int size, int reason)
{ gMemoryEventHandler->notifyMemoryAllocationError(size, WKCWebKitConvertAllocationReason(reason)); }

static void WKCWebKitNotifyCrash(const char* f, int l, const char* fn, const char* a)
{ gMemoryEventHandler->notifyCrash(f, l, fn, a); }

static void WKCWebKitNotifyStackOverflow(bool nr, unsigned ss, unsigned c, unsigned m,
    void* st, void* sb, void* cs, const char* f, int l, const char* fn)
{ gMemoryEventHandler->notifyStackOverflow(nr, ss, c, m, st, sb, cs, f, l, fn); }

static bool WKCWebKitCheckMemoryAvailability(unsigned int size, bool forimage)
{ return gMemoryEventHandler ? gMemoryEventHandler->checkMemoryAvailability(size, forimage) : false; }

static bool WKCWebKitCheckMemoryAllocatable(unsigned int size, int reason)
{ return gMemoryEventHandler->checkMemoryAllocatable(size, WKCWebKitConvertAllocationReason(reason)); }

// ─── Malloc / free / realloc forwarded to WTF ────────────────────────────────

static void* peer_malloc_proc(unsigned int size, int crashonfailure)
{
    if (crashonfailure) return WTF::fastMalloc(size);
    WTF::TryMallocReturnValue rv = WTF::tryFastMalloc(size);
    void* ptr = nullptr;
    if (!rv.getValue(ptr))
        return nullptr;
    return ptr;
}
static void peer_free_proc(void* ptr) { WTF::fastFree(ptr); }
static void* peer_realloc_proc(void* ptr, unsigned int size, int crashonfailure)
{
    if (crashonfailure) return WTF::fastRealloc(ptr, size);
    WTF::TryMallocReturnValue rv = WTF::tryFastRealloc(ptr, size);
    void* newptr = nullptr;
    if (!rv.getValue(newptr))
        return nullptr;
    return newptr;
}

static bool WKCWebKitRequestWakeUp(unsigned int ms, void* data)
{ return gTimerEventHandler->requestWakeUp(ms, data); }

// ─── Public global API ────────────────────────────────────────────────────────

WKC_API void WKCWebKitWakeUp(void* opaque) { wkcTimerWakeUpPeer(opaque); }
unsigned int WKCWebKitGetTickCount()       { return wkcGetTickCountPeer(); }
bool         WKCWebKitIsMemoryCrashing()  { return wkcMemoryIsCrashingPeer(); }

// ─── Initialize ───────────────────────────────────────────────────────────────

bool
WKCWebKitInitialize(void* memory, unsigned int memory_size,
    void* font_memory, unsigned int font_memory_size,
    WKCMemoryEventHandler& memory_event_handler,
    WKCTimerEventHandler&  timer_event_handler)
{
    // Reset global variables in bundled third-party libs
    wkc_libxml2_resetVariables();
#if ENABLE(XSLT)
    wkc_libxslt_resetVariables();
#endif
#ifdef WKC_USE_WKC_OWN_CAIRO
    wkc_pixman_resetVariables();
    wkc_cairo_resetVariables();
#endif

    if (!wkcSystemInitializePeer())    return false;
    if (!wkcDebugPrintInitializePeer()) return false;

    // Placement-new the wrapper objects into static storage
    new (gMemBuf) WKCWebKitMemoryEventHandler(memory_event_handler);
    new (gTimBuf) WKCWebKitTimerEventHandler(timer_event_handler);

    if (!wkcMemoryInitializePeer(peer_malloc_proc, peer_free_proc, peer_realloc_proc))
        return false;

    wkcMemorySetNotifyNoMemoryProcPeer(WKCWebKitNotifyNoMemory);
    wkcMemorySetNotifyMemoryAllocationErrorProcPeer(WKCWebKitNotifyMemoryAllocationError);
    wkcMemorySetNotifyCrashProcPeer(WKCWebKitNotifyCrash);
    wkcMemorySetNotifyStackOverflowProcPeer(WKCWebKitNotifyStackOverflow);
    wkcMemorySetCheckAvailabilityProcPeer(WKCWebKitCheckMemoryAvailability);
    wkcMemorySetCheckMemoryAllocatableProcPeer(WKCWebKitCheckMemoryAllocatable);

    if (!wkcThreadInitializePeer())   return false;

#if ENABLE(WEBGL)
    if (!wkcGLInitializePeer())       return false;
#endif
#if USE(ACCELERATED_COMPOSITING)
    wkcLayerInitializePeer();
#endif

    if (!wkcHWOffscreenInitializePeer()) return false;
    if (!wkcAudioInitializePeer())       return false;
    if (!wkcMediaPlayerInitializePeer()) return false;

    if (!wkcTimerInitializePeer(WKCWebKitRequestWakeUp)) return false;

    if (!wkcFontEngineInitializePeer(font_memory, font_memory_size,
            (fontPeerMalloc)peer_malloc_proc, (fontPeerFree)peer_free_proc, true))
        return false;

    wkcHeapInitializePeer(memory, memory_size);

    // Threading must be initialized before any JSC / WebCore use
    JSC::initialize();
    WebCore::InitializeLoggingChannelsIfNecessary();

    if (!wkcFileInitializePeer()) return false;
    if (!wkcNetInitializePeer())  return false;
    if (!wkcSSLInitializePeer())  return false;

    PAL::atomCanonicalTextEncodingName("UTF-8"_s);

#if ENABLE(SQL_DATABASE)
    WebCore::SQLiteFileSystem::registerSQLiteVFS();
#endif

    WKC::WKCPrefs::initialize();

    if (!WKCGlobalSettings::isExistSharedInstance())
        if (!WKCGlobalSettings::createSharedInstance(true))
            return false;

    if (!WebCore::ResourceHandleManager::isExistSharedInstance())
        if (!WebCore::ResourceHandleManager::createSharedInstance())
            return false;

    // Memory cache initial budget: 1 MB dead + unlimited live
    WebCore::MemoryCache::singleton().setCapacities(0, 0, 1 * 1024 * 1024);
    WebCore::MemoryCache::singleton().setDeadDecodedDataDeletionInterval(Seconds { 0 });

    return true;
}

// ─── Finalize ─────────────────────────────────────────────────────────────────

void WKCWebKitFinalize()
{
    if (WebCore::ResourceHandleManager::isExistSharedInstance())
        WebCore::ResourceHandleManager::deleteSharedInstance();

    WKC::WKCPrefs::finalize();

    wkcHWOffscreenFinalizePeer();
    wkcMediaPlayerFinalizePeer();
    wkcAudioFinalizePeer();
#if USE(ACCELERATED_COMPOSITING)
    wkcLayerFinalizePeer();
#endif
#if ENABLE(WEBGL)
    wkcGLFinalizePeer();
#endif
    wkcSSLFinalizePeer();
    wkcFontEngineFinalizePeer();
    wkcSystemFinalizePeer();
    wkcNetFinalizePeer();
    wkcFileFinalizePeer();
    wkcTimerFinalizePeer();
    wkcHeapFinalizePeer();

    WTF::finalizeMainThreadPlatform();

    gTimerEventHandler->~WKCWebKitTimerEventHandler();
    memset(gTimBuf, 0, sizeof(gTimBuf));
    gMemoryEventHandler->~WKCWebKitMemoryEventHandler();
    memset(gMemBuf, 0, sizeof(gMemBuf));

    wkcThreadFinalizePeer();
    wkcMemoryFinalizePeer();
    wkcDebugPrintFinalizePeer();
}

// ─── Force terminate ─────────────────────────────────────────────────────────

void WKCWebKitForceTerminate()
{
    if (WebCore::ResourceHandleManager::isExistSharedInstance())
        WebCore::ResourceHandleManager::forceTerminateInstance();

    if (WKCGlobalSettings::isAutomatic())
        WKCGlobalSettings::deleteSharedInstance();

    WKC::WKCPrefs::forceTerminate();
    WKC::WKCWebViewPrefs::forceTerminate();

#ifdef USE_WKC_CAIRO
    wkcDrawContextForceTerminateAllocatedObjectsPeer();
#endif

    wkcHeapForceTerminatePeer();
    wkcFontEngineForceTerminatePeer();
    wkcMediaPlayerForceTerminatePeer();
    wkcAudioForceTerminatePeer();
    wkcHWOffscreenForceTerminatePeer();
#if USE(ACCELERATED_COMPOSITING)
    wkcLayerForceTerminatePeer();
#endif
#if ENABLE(WEBGL)
    wkcGLForceTerminatePeer();
#endif

    wkcTextBreakIteratorForceTerminatePeer();
    wkcOffscreenForceTerminatePeer();
    wkcDrawContextForceTerminatePeer();
    wkcSSLForceTerminatePeer();
    wkcNetForceTerminatePeer();
    wkcThreadForceTerminateThreadsPeer();
    wkcFileForceTerminatePeer();
    wkcTimerForceTerminatePeer();
    wkcThreadForceTerminateExceptThreadsPeer();
    wkcMemoryForceTerminatePeer();
    wkcDebugPrintForceTerminatePeer();

#if ENABLE(SQL_DATABASE)
    wkc_sqlite3_force_terminate();
#endif

    memset(gTimBuf, 0, sizeof(gTimBuf));
    memset(gMemBuf, 0, sizeof(gMemBuf));
}

void WKCWebKitForceFinalize() { WKCWebKitForceTerminate(); }

// ─── Font management ─────────────────────────────────────────────────────────

bool WKCWebKitSuspendFont()
{
    if (wkcFontEngineCanSuspendPeer()) { wkcFontEngineFinalizePeer(); return true; }
    return false;
}

void WKCWebKitResumeFont(void* font_memory, unsigned int font_memory_size)
{
    if (wkcFontEngineCanSuspendPeer())
        wkcFontEngineInitializePeer(font_memory, font_memory_size,
            (fontPeerMalloc)peer_malloc_proc, (fontPeerFree)peer_free_proc, true);
}

unsigned int WKCWebKitFontHeapSize() { return wkcFontEngineHeapSizePeer(); }

int WKCWebKitRegisterFontOnMemory(const unsigned char* memPtr, unsigned int len)
{
    return wkcFontEngineRegisterSystemFontPeer(WKC_FONT_ENGINE_REGISTER_TYPE_MEMORY, memPtr, len);
}

int WKCWebKitRegisterFontInFile(const char* filePath)
{
    if (!filePath) return -1;
    return wkcFontEngineRegisterSystemFontPeer(
        WKC_FONT_ENGINE_REGISTER_TYPE_FILE,
        reinterpret_cast<const unsigned char*>(filePath), ::strlen(filePath));
}

void WKCWebKitUnregisterFonts() { wkcFontEngineUnregisterFontsPeer(); }
bool WKCWebKitSetFontScale(int id, float scale) { return wkcFontEngineSetFontScalePeer(id, scale); }

// ─── HW offscreen / layer callbacks ─────────────────────────────────────────

void WKCWebKitSetHWOffscreenDeviceParams(const HWOffscreenDeviceParams* params, void* opaque)
{
    WKCHWOffscreenParams procs = {
        params->fLockProc,
        params->fUnlockProc,
        params->fEnable,
        params->fEnableForImagebuffer,
        params->fScreenWidth,
        params->fScreenHeight
    };
    wkcHWOffscreenSetParamsPeer(&procs, opaque);
#if ENABLE(WEBGL)
    wkcGLRegisterDeviceLockProcsPeer(params->fLockProc, params->fUnlockProc, opaque);
#endif
}

void WKCWebKitSetLayerCallbacks(const LayerCallbacks* callbacks)
{
#if USE(ACCELERATED_COMPOSITING)
    wkcLayerInitializeCallbacksPeer(
        callbacks->fTextureMakeProc,
        callbacks->fTextureDeleteProc,
        callbacks->fTextureUpdateProc,
        callbacks->fTextureChangeProc,
        callbacks->fDidChangeParentProc,
        callbacks->fCanAllocateProc);
#endif
}

// ─── Offscreen / DrawContext factory ─────────────────────────────────────────

void* WKCWebKitOffscreenNew(OffscreenFormat format, void* bitmap, int rowbytes, const WKCSize* size)
{
    int pformat = 0;
    switch (format) {
    case EOffscreenFormatRGBA5650:     pformat = WKC_OFFSCREEN_TYPE_RGBA5650;     break;
    case EOffscreenFormatARGB8888:     pformat = WKC_OFFSCREEN_TYPE_ARGB8888;     break;
    case EOffscreenFormatPolygon:      pformat = WKC_OFFSCREEN_TYPE_POLYGON;      break;
    case EOffscreenFormatCairo16:      pformat = WKC_OFFSCREEN_TYPE_CAIRO16;      break;
    case EOffscreenFormatCairo32:      pformat = WKC_OFFSCREEN_TYPE_CAIRO32;      break;
    case EOffscreenFormatCairoSurface: pformat = WKC_OFFSCREEN_TYPE_CAIROSURFACE; break;
    default: return nullptr;
    }
    return wkcOffscreenNewPeer(pformat, bitmap, rowbytes, size);
}

void WKCWebKitOffscreenDelete(void* offscreen)   { wkcOffscreenDeletePeer(offscreen); }
void* WKCWebKitDrawContextNew(void* offscreen)   { return wkcDrawContextNewPeer(offscreen); }
void  WKCWebKitDrawContextDelete(void* context)  { wkcDrawContextDeletePeer(context); }

bool WKCWebKitOffscreenIsError(void* offscreen)
{
#ifdef USE_WKC_CAIRO
    return wkcOffscreenIsErrorPeer(offscreen);
#else
    return false;
#endif
}

bool WKCWebKitDrawContextIsError(void* context)
{
#ifdef USE_WKC_CAIRO
    return wkcDrawContextIsErrorPeer(context);
#else
    return false;
#endif
}

// ─── Glyph / image caches ────────────────────────────────────────────────────

bool WKCWebKitSetGlyphCache(int format, void* cache, const WKCSize* size)
{
    if (cache) {
        if (!wkcOffscreenCreateGlyphCachePeer(format, cache, size))  return false;
        if (!wkcHWOffscreenCreateGlyphCachePeer(format, cache, size)) {
            wkcOffscreenDeleteGlyphCachePeer(); return false;
        }
        return true;
    }
    wkcHWOffscreenDeleteGlyphCachePeer();
    wkcOffscreenDeleteGlyphCachePeer();
    return true;
}

bool WKCWebKitSetImageCache(int format, void* cache, const WKCSize* size)
{
    if (cache) {
        if (!wkcOffscreenCreateImageCachePeer(format, cache, size))  return false;
        if (!wkcHWOffscreenCreateImageCachePeer(format, cache, size)) {
            wkcOffscreenDeleteImageCachePeer(); return false;
        }
        return true;
    }
    wkcHWOffscreenDeleteImageCachePeer();
    wkcOffscreenDeleteImageCachePeer();
    return true;
}

// ─── GC ───────────────────────────────────────────────────────────────────────

void WKCWebKitRequestGarbageCollect(bool is_now, int /*gctype*/)
{
    // Modern GarbageCollectionController always sweeps as needed; the legacy
    // "do not sweep" variant and explicit free-block release were removed.
    auto& gc = WebCore::GarbageCollectionController::singleton();
    if (is_now)
        gc.garbageCollectNow();
    else
        gc.garbageCollectSoon();
}

void WKCWebKitResetMaxHeapUsage() { wkcHeapResetMaxHeapUsagePeer(); }

// ─── Cookies (global) ────────────────────────────────────────────────────────

void WKCWebKitClearCookies()
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        m->clearCookies();
}

int WKCWebKitCookieSerializeNum()
{
    auto* m = WebCore::ResourceHandleManager::sharedInstance();
    return m ? m->CookieSerializeNum() : 0;
}

int WKCWebKitCookieSerialize(char* buff, int bufflen)
{
    auto* m = WebCore::ResourceHandleManager::sharedInstance();
    return m ? m->CookieSerialize(buff, bufflen) : 0;
}

void WKCWebKitCookieDeserialize(const char* buff, bool restart)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        m->CookieDeserialize(buff, restart);
}

// ─── DNS prefetch ────────────────────────────────────────────────────────────

void WKCWebKitSetDNSPrefetchProc(void(*proc)(const char*), void* locker)
    { wkcNetSetPrefetchDNSCallbackPeer(proc, locker); }

void WKCWebKitCachePrefetchedDNSEntry(const char* name, const unsigned char* ipaddr)
    { wkcNetCachePrefetchedDNSEntryPeer(name, ipaddr); }

int  WKCWebKitGetNumberOfSockets()
    { return wkcNetGetNumberOfSocketsPeer(); }

int  WKCWebKitGetSocketStatistics(int n, SocketStatistics* stats)
    { return wkcNetGetSocketStatisticsPeer(n, (WKCSocketStatistics*)stats); }

// ─── SSL (stubbed; libcurl handles the actual TLS) ────────────────────────────
// Implement these by forwarding into ResourceHandleManager SSL methods if needed.

void* WKCWebKitSSLRegisterRootCA(const char* cert, int cert_len)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterRootCA(cert, cert_len);
    return nullptr;
}
void* WKCWebKitSSLRegisterRootCAByDER(const char* cert, int cert_len)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterRootCAByDER(cert, cert_len);
    return nullptr;
}
int  WKCWebKitSSLUnregisterRootCA(void* id)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLUnregisterRootCA(id);
    return -1;
}
void WKCWebKitSSLRootCADeleteAll()
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance()) m->SSLRootCADeleteAll();
}
void* WKCWebKitSSLRegisterCRL(const char* crl, int crl_len)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterCRL(crl, crl_len);
    return nullptr;
}
int  WKCWebKitSSLUnregisterCRL(void* id)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLUnregisterCRL(id);
    return -1;
}
void WKCWebKitSSLCRLDeleteAll()
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance()) m->SSLCRLDeleteAll();
}
void* WKCWebKitSSLRegisterClientCert(const unsigned char* pkcs12, int pkcs12_len,
                                       const unsigned char* pass, int pass_len)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterClientCert(pkcs12, pkcs12_len, pass, pass_len);
    return nullptr;
}
void* WKCWebKitSSLRegisterClientCertByDER(const unsigned char* cert, int cert_len,
                                            const unsigned char* key, int key_len)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterClientCertByDER(cert, cert_len, key, key_len);
    return nullptr;
}
int  WKCWebKitSSLUnregisterClientCert(void* id)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLUnregisterClientCert(id);
    return -1;
}
void WKCWebKitSSLClientCertDeleteAll()
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance()) m->SSLClientCertDeleteAll();
}
bool WKCWebKitSSLRegisterBlackCert(const char* issuer, const char* serial)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterBlackCert(issuer, serial);
    return false;
}
void WKCWebKitSSLBlackCertDeleteAll()
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance()) m->SSLBlackCertDeleteAll();
}
bool WKCWebKitSSLRegisterEVSSLOID(const char* issuer, const char* oid,
                                    const char* sha1fp, const char* serial)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->SSLRegisterEVSSLOID(issuer, oid, sha1fp, serial);
    return false;
}
void WKCWebKitSSLEVSSLOIDDeleteAll()
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance()) m->SSLEVSSLOIDDeleteAll();
}
void WKCWebKitSSLSetAllowServerHost(const char* host)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance()) m->setAllowServerHost(host);
}
const char** WKCWebKitSSLGetServerCertChain(const char* url, int& out_num)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        return m->getServerCertChain(url, out_num);
    out_num = 0; return nullptr;
}
void WKCWebKitSSLFreeServerCertChain(const char** chain, int num)
{
    if (auto* m = WebCore::ResourceHandleManager::sharedInstance())
        m->freeServerCertChain(chain, num);
}

// ─── File system / media / pasteboard callbacks ───────────────────────────────

void WKCWebKitSetFileSystemProcs(const WKC::FileSystemProcs* procs)
    { wkcFileCallbackSetPeer(static_cast<const WKCFileProcs*>(procs)); }

void WKCWebKitSetMediaPlayerProcs(const WKC::MediaPlayerProcs* procs)
    { wkcMediaPlayerCallbackSetPeer(static_cast<const WKCMediaPlayerProcs*>(procs)); }

void WKCWebKitSetPasteboardProcs(const WKC::PasteboardProcs* procs)
    { wkcPasteboardCallbackSetPeer(static_cast<const WKCPasteboardProcs*>(procs)); }

// ─── Web Audio resource path ──────────────────────────────────────────────────
extern "C" void wkcMediaPlayerSetAudioResourcesPathPeer(const char*);
void WKCWebKitSetWebAudioResourcePath(const char* path)
    { wkcMediaPlayerSetAudioResourcesPathPeer(path); }

// ─── Inspector stubs (WebInspectorServer removed) ────────────────────────────
void WKCWebKitSetWebInspectorResourcePath(const char*) {}
bool WKCWebKitStartWebInspector(const char*, int, bool(*)(void*), void*) { return false; }
void WKCWebKitStopWebInspector() {}

// ─── Memory stats (deprecated, kept for ABI compat) ─────────────────────────
unsigned int WKCWebKitAvailableMemory()    { return Heap::GetAvailableSize(); }
unsigned int WKCWebKitMaxAvailableBlock()  { return Heap::GetMaxAvailableBlockSize(); }

// ─── IDN ─────────────────────────────────────────────────────────────────────
namespace IDN {
int toUnicode(const char* host, unsigned short* idn, int maxidn)
    { return wkcI18NIDNtoUnicodePeer((const unsigned char*)host, -1, idn, maxidn); }
int fromUnicode(const unsigned short* idn, char* host, int maxhost)
    { return wkcI18NIDNfromUnicodePeer(idn, -1, (unsigned char*)host, maxhost); }
} // namespace IDN

// ─── NetUtil ─────────────────────────────────────────────────────────────────
namespace NetUtil {
int correctIPAddress(const char* ipaddress)
    { return wkcNetCheckCorrectIPAddressPeer(ipaddress); }
} // namespace NetUtil

// ─── Base64 ──────────────────────────────────────────────────────────────────
namespace Base64 {
int base64Encode(const char* in, char* buf, int buflen)
{
    // WTF::base64Encode API changed in modern WTF — use StringView span form
    auto span = std::span<const uint8_t> {
        reinterpret_cast<const uint8_t*>(in), ::strlen(in) };
    WTF::String encoded = WTF::base64EncodeToString(span);
    auto utf8 = encoded.utf8();
    if (buf && buflen > (int)utf8.length())
        ::memcpy(buf, utf8.data(), utf8.length() + 1);
    return (int)utf8.length();
}
} // namespace Base64

// ─── WKCWebView::permitSendRequest (already defined above) ───────────────────
WKC_API void
WKCWebKitSetPluginInstances(void* /*instance1*/, void* /*instance2*/)
{
    // NPAPI plugin support was removed from modern WebKit; no-op.
}

} // namespace WKC
