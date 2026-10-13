/*
 * WKCWebView.h
 * 
 * Copyright (c) 2010-2015 ACCESS CO., LTD. All rights reserved.
 * Modernized 2025 for webkit-wiiu.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WKCWebView_h
#define WKCWebView_h

// class definition

#include "WKCVersion.h"

#include "WKCEnums.h"
#include "WKCMemoryEventHandler.h"
#include "WKCTimerEventHandler.h"
#include "WKCRSSLinkInfo.h"
#include "WKCSkin.h"
#include "WKCMediaSkin.h"
// wkcmedia.h forward-declares the WKCMediaPlayerProcs / WKCMediaPlayerCallbacks
// peer structs used by the MediaPlayerProcs typedef and WKCWebKitSetMediaPlayerProcs
// below. Without it the typedef fails ('WKCMediaPlayerProcs does not name a type'),
// which cascades to every translation unit that includes WKCWebView.h.
#include <wkc/wkcmedia.h>
#include "helpers/WKCLinkHash.h"
#include "helpers/WKCHelpersEnums.h"
#include "helpers/WKCString.h"

#include <stddef.h>

// NOTE: WKC_USE_CUSTOM_BACKFORWARD_LIST was #defined here in the original.
// The modernized WKCWebView.cpp no longer uses a custom BackForwardClient —
// BackForwardListImpl was removed from WebKit, so the history methods are now
// thin wrappers over WebCore::BackForwardController (canGoBack/goBack/etc.)
// plus no-op stubs for the index-based ones. The #define is gone and the
// formerly-guarded history declarations below are now always visible so they
// match the stubs the .cpp provides.

// prototypes
namespace WKC {
class Node;
class HistoryItem;
class Page;
class ResourceHandle;
class String;
class HitTestResult;

class WKCWebView;
class WKCClientBuilders;
class WKCWebFrame;
class WKCWebViewPrivate;
class WKCWebInspector;
class WKCHitTestResult;
class WKCWebElementInfo;
class WKCSettings;
class WKCNetworkEventHandler;
class WKCWebViewPrefs;
class WKCSecurityOrigin;

typedef struct CompositionUnderline_ CompositionUnderline;
}

// callbacks
/*@{*/

/** @brief Namespace of API layer of NetFront Browser NX WKC */
namespace WKC {
    typedef WKCFileProcs FileSystemProcs;
    typedef WKCMediaPlayerProcs MediaPlayerProcs;
    typedef WKCPasteboardProcs PasteboardProcs;

    /** @brief Structure for storing client certificate informations */
    struct clientCertInfo_ {
        const char* issuer;
        const char* subject;
        const char* notbefore;
        const char* notafter;
        const char* serialNumber;
    };
    typedef struct clientCertInfo_ clientCertInfo;

    /** @brief Structure for storing hardware offscreen settings */
    struct HWOffscreenDeviceParams_ {
        void (*fLockProc)(void* in_opaque);
        void (*fUnlockProc)(void* in_opaque);
        bool fEnable;
        bool fEnableForImagebuffer;
        unsigned int fScreenWidth;
        unsigned int fScreenHeight;
    };
    typedef struct HWOffscreenDeviceParams_ HWOffscreenDeviceParams;

    typedef struct LayerCallbacks_ {
        bool (*fTextureMakeProc)(void* in_layer, int in_width, int in_height, int in_bpp, void** out_bitmap, int* out_rowbytes, int* out_width, int* out_height, void** out_opaque_texture);
        void (*fTextureDeleteProc)(void *in_layer, void* in_bitmap);
        void (*fTextureUpdateProc)(void *in_opaque, int in_width, int in_height, void *in_bitmap);
        bool (*fTextureChangeProc)(void* in_layer, int in_width, int in_height, int in_bpp, void** out_bitmap, int* out_rowbytes, int* out_width, int* out_height, void** out_opaque_texture);
        void (*fDidChangeParentProc)(void *in_layer);
        bool (*fCanAllocateProc)(int in_width, int in_height);
    } LayerCallbacks;

    // NOTE: the WKCEPUBData* / WKCEPUBDataNav* / WKCEPUBDataSpine* structures
    // and the EPageProgressDirection* enum that lived here have been removed
    // along with the EPUB ebook-reader class (see further down). They were
    // ebook-format metadata types, unrelated to web browsing.
}

/*@}*/

// APIs
namespace WKC {

/*@{*/

WKC_API bool WKCWebKitInitialize(void* memory, unsigned int memory_size, void* font_memory, unsigned int font_memory_size, WKCMemoryEventHandler& memory_event_handler, WKCTimerEventHandler& timer_event_handler);
WKC_API void WKCWebKitFinalize();
WKC_API void WKCWebKitForceTerminate();
WKC_API void WKCWebKitForceFinalize();
WKC_API void WKCWebKitResetMaxHeapUsage();
WKC_API unsigned int WKCWebKitAvailableMemory();
WKC_API unsigned int WKCWebKitMaxAvailableBlock();

enum {
    EJSGCTypeDoNotSweep,
    EJSGCTypeDoSweep,
    EJSGCTypes
};

WKC_API void WKCWebKitRequestGarbageCollect(bool is_now = false, int gctype = EJSGCTypeDoSweep);
WKC_API unsigned int WKCWebKitFontHeapSize();
WKC_API bool WKCWebKitSuspendFont();
WKC_API void WKCWebKitResumeFont(void* font_memory, unsigned int font_memory_size);
WKC_API int WKCWebKitRegisterFontOnMemory(const unsigned char* memPtr, unsigned int len);
WKC_API int WKCWebKitRegisterFontInFile(const char* filePath);
WKC_API void WKCWebKitUnregisterFonts();
WKC_API bool WKCWebKitSetFontScale(int id, float scale);

WKC_API void WKCWebKitWakeUp(void* data);
WKC_API unsigned int WKCWebKitGetTickCount();

WKC_API void WKCWebKitSetHWOffscreenDeviceParams(const HWOffscreenDeviceParams* params, void* opaque);
WKC_API void WKCWebKitSetLayerCallbacks(const LayerCallbacks* callbacks);
WKC_API void WKCWebKitGetLayerProperties(void* layer, void** opaque_texture, int* width, int* height, bool* need_yflip, void** offscreen, int*offscreenwidth, int* offscreenheight);

WKC_API void* WKCWebKitOffscreenNew(OffscreenFormat format, void* bitmap, int rowbytes, const WKCSize* size);
WKC_API void WKCWebKitOffscreenDelete(void* offscreen);
WKC_API bool WKCWebKitOffscreenIsError(void* offscreen);
WKC_API void* WKCWebKitDrawContextNew(void* offscreen);
WKC_API void WKCWebKitDrawContextDelete(void* context);
WKC_API bool WKCWebKitDrawContextIsError(void* context);

WKC_API void WKCWebKitSetPluginInstances(void* instance1, void* instance2);

WKC_API void* WKCWebKitSSLRegisterRootCA(const char* cert, int cert_len);
WKC_API void* WKCWebKitSSLRegisterRootCAByDER(const char* cert, int cert_len);
WKC_API int   WKCWebKitSSLUnregisterRootCA(void* certid);
WKC_API void  WKCWebKitSSLRootCADeleteAll(void);
WKC_API void* WKCWebKitSSLRegisterCRL(const char* crl, int crl_len);
WKC_API int   WKCWebKitSSLUnregisterCRL(void* crlid);
WKC_API void  WKCWebKitSSLCRLDeleteAll(void);
WKC_API void* WKCWebKitSSLRegisterClientCert(const unsigned char* pkcs12, int pkcs12_len, const unsigned char* pass, int pass_len);
WKC_API void* WKCWebKitSSLRegisterClientCertByDER(const unsigned char* cert, int cert_len, const unsigned char* key, int key_len);
WKC_API int   WKCWebKitSSLUnregisterClientCert(void* certid);
WKC_API void  WKCWebKitSSLClientCertDeleteAll(void);
WKC_API bool  WKCWebKitSSLRegisterBlackCert(const char* issuerCommonName, const char* SerialNumber);
WKC_API void  WKCWebKitSSLBlackCertDeleteAll(void);
WKC_API bool  WKCWebKitSSLRegisterEVSSLOID(const char *issuerCommonName, const char *OID, const char *sha1FingerPrint, const char *SerialNumber);
WKC_API void  WKCWebKitSSLEVSSLOIDDeleteAll(void);
WKC_API void  WKCWebKitSSLSetAllowServerHost(const char *host_w_port);
WKC_API void WKCWebKitSetFileSystemProcs(const WKC::FileSystemProcs* procs);
WKC_API void WKCWebKitSetMediaPlayerProcs(const WKC::MediaPlayerProcs* procs);
WKC_API void WKCWebKitSetPasteboardProcs(const WKC::PasteboardProcs* procs);

WKC_API const char** WKCWebKitSSLGetServerCertChain(const char* in_url, int& out_num);
WKC_API void WKCWebKitSSLFreeServerCertChain(const char** chain, int num);

WKC_API bool WKCWebKitSetGlyphCache(int format, void* cache, const WKCSize* size);
WKC_API bool WKCWebKitSetImageCache(int format, void* cache, const WKCSize* size);
WKC_API bool WKCWebKitIsMemoryCrashing();
WKC_API void WKCWebKitSetWebAudioResourcePath(const char* path);
WKC_API void WKCWebKitSetWebInspectorResourcePath(const char* path);
WKC_API bool WKCWebKitStartWebInspector(const char* addr, int port, bool(*modalcycle)(void*), void* opaque);
WKC_API void WKCWebKitStopWebInspector();

WKC_API void WKCWebKitClearCookies(void);
WKC_API int  WKCWebKitCookieSerializeNum(void);
WKC_API int  WKCWebKitCookieSerialize(char* buff, int bufflen);
WKC_API void WKCWebKitCookieDeserialize(const char* buff, bool restart);
WKC_API int WKCWebKitCookieGet(const char* uri, char* buf, unsigned int len);
WKC_API void WKCWebKitCookieSet(const char* uri, const char* cookie);
WKC_API int WKCWebKitCurrentWebSocketConnectionsNum(void);

WKC_API void WKCWebKitSetDNSPrefetchProc(void(*requestprefetchproc)(const char*), void* resolverlocker);
WKC_API void WKCWebKitCachePrefetchedDNSEntry(const char* hostname, const unsigned char* ipaddr);

WKC_API int WKCWebKitGetNumberOfSockets(void);
struct SocketStatistics_ {
    int fFd;
    unsigned int fRecvBytes;
    unsigned int fSendBytes;
};
typedef struct SocketStatistics_ SocketStatistics;
WKC_API int WKCWebKitGetSocketStatistics(int in_numberOfArray, SocketStatistics* out_statistics);

/** @brief Class that corresponds to the content display screen of the browser. */
class WKC_API WKCWebView
{
    friend class WKCWebFrame;
    friend class WKCWebViewPrefs;

public:
    // life and death
    static WKCWebView* create(WKCClientBuilders& builders);
    static void deleteWKCWebView(WKCWebView *self);

    void notifyForceTerminate();

    // off-screen draw
    bool setOffscreen(OffscreenFormat format, void* bitmap, int rowbytes, const WKCSize& offscreensize, const WKCSize& viewsize, bool fixedlayout, const WKCSize* const desktopsize = 0, bool needsLayout = true);

#ifdef WKC_CUSTOMER_PATCH_0304674
    void setOffscreenPointer( void* bitmap );
#endif

    void notifyResizeViewSize(const WKCSize& size);
    void notifyResizeDesktopSize(const WKCSize& size, bool in_resizeevent = true);
    void notifyRelayout(bool force = false);
    void notifyPaintOffscreenFrom(const WKCRect& rect, const WKCPoint& p);
    void notifyPaintOffscreen(const WKCRect& rect);
#ifdef USE_WKC_CAIRO
    void notifyPaintToContext(const WKCRect& rect, void* context);
#endif
    void notifyScrollOffscreen(const WKCRect& rect, const WKCSize& diff);
    void notifyServiceScriptedAnimations();

    // events
    bool notifyKeyPress(WKC::Key key, WKC::Modifier modifiers, bool in_autorepeat=false);
    bool notifyKeyRelease(WKC::Key key, WKC::Modifier modifiers);
    bool notifyKeyChar(unsigned int in_char);
    bool notifyIMEComposition(const unsigned short* in_string, WKC::CompositionUnderline* in_underlines, unsigned int in_underlineNum, unsigned int in_cursorPosition, unsigned int in_selectionEnd, bool in_confirm);
    bool notifyMouseDown(const WKCPoint& pos, WKC::MouseButton button, WKC::Modifier modifiers);
    bool notifyMouseUp(const WKCPoint& pos, WKC::MouseButton button, WKC::Modifier modifiers);
    bool notifyMouseMove(const WKCPoint& pos, WKC::MouseButton button, WKC::Modifier modifiers);
    bool notifyMouseDoubleClick(const WKCPoint& pos, WKC::MouseButton button, WKC::Modifier modifiers);
    bool notifyMouseWheel(const WKCPoint& pos, const WKCSize& diff, WKC::Modifier modifiers);
    void notifySetMousePressed(bool pressed);
    void notifyLostMouseCapture();

    /** @brief Structure that stores touch coordinate information */
    struct TouchPoint_ {
        int fId;
        int fState;
        WKCPoint fPoint;
    };
    typedef struct TouchPoint_ TouchPoint;
    bool notifyTouchEvent(int type, const TouchPoint* points, int npoints, WKC::Modifier modifiers);

    bool notifyScroll(WKC::ScrollType scrolltype);
    void notifyFocusIn();
    void notifyFocusOut();
    WKC::Node* findFocusableNode(const WKC::FocusDirection direction, const WKCRect* specificRect = 0);
    WKC::Node* findFocusableNodeInRect(const WKC::FocusDirection direction, const WKCRect* rect, bool enableContainer = false );
    WKC::Node* findNearestFocusableNodeFromPoint(const WKCPoint point, const WKCRect* rect = 0);
    WKC::Node* findNeighboringEditableNode(WKC::WKCFocusDirection direction);
    bool setFocusedNode(WKC::Node* node);
    void notifySuspend();
    void notifyResume();
    void notifyScrollPositionChanged();

    bool notifyScroll(int dx, int dy);
    bool notifyScrollTo(int x, int y);
    void scrollPosition(WKCPoint& pos);
    void maximumScrollPosition(WKCPoint& pos) const;
    void minimumScrollPosition(WKCPoint& pos) const;
    void contentsSize(WKCSize& size);

    void notifyChromeVisible(bool in_visible);
    bool chromeVisible();

    // Gamepad (W3C Gamepad API). See WKCWebView.cpp for the main.cpp call flow.
    static void initializeGamepads(int num);
    static bool notifyGamepadEvent(int index, const WKC::String& id, long long timestamp, int naxes, const float* axes, int nbuttons, const float* buttons);

    // APIs
    const unsigned short* title();
    const char* uri();

    WKC::Page* core();
    WKC::WKCSettings* settings();

    bool canGoBack();
    bool canGoBackOrForward(int steps);
    bool canGoForward();
    bool goBack();
    void goBackOrForward(int steps);
    bool goForward();

    void stopLoading();
    void reload();
    void reloadBypassCache();
    void loadURI(const char* uri, const char* referrer = 0);
    // Accept an address-bar value: navigate to a URL or run a web search.
    // Returns false only when the supplied input is empty after trimming.
    bool loadURLOrSearch(const char* input, const char* referrer = 0);
    void loadString(const char* content, const unsigned short* mimetype, const unsigned short* encoding, const char* base_uri);
    void loadHTMLString(const char* content, const char* base_uri);

    bool searchText(const unsigned short* text, bool case_sensitive, bool forward, bool wrap);
    unsigned int markTextMatches(const unsigned short* string, bool case_sensitive, unsigned int limit);
    void setHighlightTextMatches(bool highlight);
    void unmarkTextMatches();

    WKCWebFrame* mainFrame();
    WKCWebFrame* focusedFrame();

    void executeScript(const char* script);

    // Explicitly connect an embedder-provided JavaScript Inspector frontend.
    // Outgoing DevTools Protocol JSON is delivered through ChromeClientIf;
    // incoming protocol JSON is passed to dispatchInspectorMessageFromFrontend.
    // These methods do not open a network listener themselves.
    bool connectInspectorFrontend();
    void disconnectInspectorFrontend();
    void dispatchInspectorMessageFromFrontend(const char* message);

    bool hasSelection();
    void clearSelection();
    WKCRect selectionBoundingBox(bool textonly, bool useSelectionHeight);
    const unsigned short* selectionText();
    void selectAll();

    bool canShowMimeType(const unsigned short* mime_type);

    float zoomLevel();
    float setZoomLevel(float zoom_level);
    void zoomIn(float ratio);
    void zoomOut(float ratio);
    float textOnlyZoomLevel();
    float setTextOnlyZoomLevel(float zoom_level);
    void textOnlyZoomIn(float ratio);
    void textOnlyZoomOut(float ratio);
    bool fullContentZoom();
    void setFullContentZoom(bool full_content_zoom);

    float opticalZoomLevel() const;
    const WKCFloatPoint& opticalZoomOffset() const;
    float setOpticalZoom(float zoom_level, const WKCFloatPoint& offset);

    void viewSize(WKCSize& size) const;

    const unsigned short* encoding();
    void setCustomEncoding(const unsigned short* encoding);
    const unsigned short* customEncoding();

    WKC::LoadStatus loadStatus();
    double progress();

    bool hitTestResultForNode(const WKC::Node* node, WKC::HitTestResult& result);

    void enterCompositingMode();

    // caches
    static void cachedSize(unsigned int& dead_resource, unsigned int& live_resource);
    static void clearCaches(bool clearhttpcache = false);
    static size_t fontDataCount();
    static size_t inactiveFontDataCount();
    static void clearFontCache(bool in_clearsAll);
    static void clearCrossOriginPreflightResultCache();

    // plugins
    static void setPluginsFolder(const char* in_folder);

    unsigned int getRSSLinkNum();
    unsigned int getRSSLinkInfo(WKCRSSLinkInfo* info, unsigned int info_len);
    WKC::Node* getFocusedNode();
    WKC::Node* getNodeFromPoint(int x, int y);
    bool clickableFromPoint(int x, int y);
    bool draggableFromPoint(int x, int y);

    /** @brief Scroll bar part types */
    enum ScrollbarPart {
        NoPart,
        BackButtonPart,
        ForwardButtonPart,
        BackTrackPart,
        ThumbPart,
        ForwardTrackPart,
        ScrollbarBGPart,
        TrackBGPart,
        BackButtonStartPart,
        BackButtonEndPart,
        ForwardButtonStartPart,
        ForwardButtonEndPart,
    };

    // cookie setting to WebCore
    void setCookieEnabled(bool flag);
    bool cookieEnabled();

    // permit to send request
    static void permitSendRequest(void *handle, bool permit);

    // History (visited links)
    bool addVisitedLink(const char* uri, const unsigned short* title, const struct tm* date);
    bool addVisitedLinkHash(LinkHash hash);

    // images
    enum {
        EInternalColorFormat8888,
        EInternalColorFormat5515withMask,
        EInternalColorFormat8888or565,
        EInternalColorFormats
    };
    static void setInternalColorFormat(int fmt);
    void setUseBilinearForScaledImages(bool flag);
    void setUseAntiAliasForDrawings(bool flag);
    static void setUseBilinearForCanvasImages(bool flag);
    static void setUseAntiAliasForCanvas(bool flag);

    void setScrollPositionForOffscreen(const WKCPoint& scrollPosition);

    // JS Heap
    static void jsJITCodePageAllocatedBytes(size_t& allocated_bytes, size_t& total_bytes, size_t& max_allocatable_bytes);

    // extra draw
    static void setClipsRepaints(bool enable) { m_clipsRepaints = enable; }
    static bool clipsRepaints() { return m_clipsRepaints; }

    // scroll node
    static void scrollNodeByRecursively(WKC::Node* node, int dx, int dy);
    static void scrollNodeBy(WKC::Node* node, int dx, int dy);
    static bool isScrollableNode(const WKC::Node* node);
    static bool canScrollNodeInDirection(const WKC::Node* node, WKC::WKCFocusDirection direction);

    // JS Extension
    void notifyJSExtensionEvent(JSExtensionEvent eventId) const;

    // session storage and local storage
    void clearSessionStorage();
    static void clearLocalStorage();

    // page visibility.
    // NOTE: Prerender / Preview states were removed from modern WebKit's
    // PageVisibilityState. setVisibilityState() maps everything to
    // Visible/Hidden; the two extra enumerators are kept only so any caller
    // passing them still compiles.
    enum {
        PageVisibilityStateVisible,
        PageVisibilityStateHidden,
        PageVisibilityStatePrerender,
        PageVisibilityStatePreview,
        PageVisibilityStates
    };
    void setVisibilityState(int state, bool isInitialState);


    bool editable();
    void setEditable(bool enable);

    // repaint throttling (no-op stub)
    static void setRepaintThrottling(double deferredRepaintDelay, double initialDeferredRepaintDelayDuringLoading, double maxDeferredRepaintDelayDuringLoading, double deferredRepaintDelayIncrementDuringLoading);

    // fullscreen (no-op stubs)
    void cancelFullScreen();
    bool isFullScreen() const;

    // NOTE: the EPUB inner class (ebook-file reader) has been removed entirely.
    // It was custom Nintendo code with no upstream WebKit equivalent, unrelated
    // to web browsing. The m_EPUB member and epub() accessor are gone too.

private:
    WKCWebView();
    ~WKCWebView();
    bool construct(WKCClientBuilders& builders);

private:
    WKCWebViewPrivate* m_private;
    static bool m_clipsRepaints;
};

/*@{*/
/** @brief Namespace of API layer of NetFront Browser NX WKC IDN @n */
namespace IDN {
WKC_API int fromUnicode(const unsigned short* idn, char* host, int maxhost);
WKC_API int toUnicode(const char* host, unsigned short* idn, int maxidn);
} // namespace IDN
/*@}*/

/*@{*/
/** @brief Namespace of API layer of NetFront Browser NX WKC NetUtil @n */
namespace NetUtil {
WKC_API int correctIPAddress(const char *in_ipaddress);
/*@}*/
} // namespace NetUtil

/*@{*/
/** @brief Namespace of API layer of NetFront Browser NX WKC Base64 @n */
namespace Base64 {
WKC_API int base64Encode(const char* in, char* buf, int buflen);
/*@}*/
} // namespace Base64

/*@}*/

#ifndef WKC_WEB_VIEW_COMPILE_ASSERT
#define WKC_WEB_VIEW_COMPILE_ASSERT(exp, name) typedef int dummy##name [(exp) ? 1 : -1]
#endif

} // namespace

#endif  // WKCWebView_h
