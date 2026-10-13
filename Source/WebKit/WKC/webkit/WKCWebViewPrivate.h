/*
 * WKCWebViewPrivate.h
 * 
 * Copyright (c) 2010-2014 ACCESS CO., LTD. All rights reserved.
 * Modernized 2025 for webkit-wiiu (devkitPPC / Aroma bare-metal).
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

#ifndef WKCWebViewPrivate_h
#define WKCWebViewPrivate_h

#include "WKCClientBuilders.h"
#include "WKCEnums.h"

#include "helpers/WKCHelpersEnums.h"

#include "IntRect.h"
#include "FloatPoint.h"

#include <memory>
#include <wtf/text/WTFString.h>

namespace WebCore {
    class Page;
    class GraphicsLayer;
    class GraphicsContext;
    class LocalFrame;   // modern WebKit: AbstractFrame splits into LocalFrame / RemoteFrame
    class Node;
    class NotificationClient;
}

namespace WKC {
class WKCWebView;
class WKCWebFrame;
class ChromeClientWKC;
class InspectorBackendClientWKC;
class DropDownListClientWKC;
class FrameLoaderClientWKC;
class WKCSettings;
class WKCWebViewPrefs;
class DeviceMotionClientWKC;
class DeviceOrientationClientWKC;
class WKCOverlayIf;
class WKCOverlayList;

class Node;
class Page;
class NodePrivate;
class PagePrivate;

// class definitions

class WKCWebViewPrivate
{
    friend class WKCWebView;
    friend class FrameLoaderClientWKC;

public:
    static WKCWebViewPrivate* create(WKCWebView* parent, WKCClientBuilders& builders);
    ~WKCWebViewPrivate();
    void notifyForceTerminate();

    inline WebCore::Page* core() const { return m_corePage; };
    WKC::Page* wkcCore() const;
    inline WKCWebView* parent() const { return m_parent; };
    inline WKCClientBuilders& clientBuilders() const { return m_clientBuilders; };

    // Modern WebKit: Page::mainFrame() returns an AbstractFrame& that may be
    // local or remote. Every call site in WKCWebView.cpp that used to call
    // core()->mainFrame() now goes through this helper, which performs the
    // local-frame downcast once and returns nullptr for the (never-expected
    // on this single-process bare-metal port) remote case.
    WebCore::LocalFrame* localMainFrame() const;

    bool connectInspectorFrontend();
    void disconnectInspectorFrontend();
    void dispatchInspectorMessageFromFrontend(const WTF::String&);

    inline DropDownListClientWKC* dropdownlistclient() { return m_dropdownlist; };
    // settings
    WKCSettings* settings();

    // drawings
    bool setOffscreen(OffscreenFormat format, void* bitmap, int rowbytes, const WebCore::IntSize& offscreensize, const WebCore::IntSize& viewsize, bool fixedlayout, const WebCore::IntSize* desktopsize, bool needsLayout);
#ifdef WKC_CUSTOMER_PATCH_0304674
    void setOffscreenPointer(void* bitmap);
#endif

    void notifyResizeDesktopSize(const WebCore::IntSize& size, bool in_resizeevent);
    void notifyResizeViewSize(const WebCore::IntSize& size);
    void notifyRelayout();
    void notifyPaintOffscreen(const WebCore::IntRect& rect);
#ifdef USE_WKC_CAIRO
    void notifyPaintToContext(const WebCore::IntRect& rect, void* context);
#endif
    void notifyPaintOffscreenFrom(const WebCore::IntRect& rect, const WKCPoint& p);
    void notifyScrollOffscreen(const WebCore::IntRect& rect, const WebCore::IntSize& diff);
    void notifyServiceScriptedAnimations();
    const WebCore::IntSize& desktopSize() const;
    const WebCore::IntSize& viewSize() const;
    const WebCore::IntSize& defaultDesktopSize() const;
    const WebCore::IntSize& defaultViewSize() const;
    void setUseAntiAliasForDrawings(bool flag);
    void setUseBilinearForScaledImages(bool flag);
    static void setUseBilinearForCanvasImages(bool flag);
    static void setUseAntiAliasForCanvas(bool flag);
    void setScrollPositionForOffscreen(const WebCore::IntPoint& scrollPosition);
    void notifyScrollPositionChanged();

    bool transparent() { return m_isTransparent; };
    void setTransparent(bool flag);

    inline float opticalZoomLevel() const { return m_opticalZoomLevel; };
    inline const WKCFloatPoint& opticalZoomOffset() const { return m_opticalZoomOffset; };
    void setOpticalZoom(float, const WKCFloatPoint&);

    void enterCompositingMode();

    WKC::Node* findNeighboringEditableNode(WKC::WKCFocusDirection direction);

    WKC::Node* getFocusedNode();
    WKC::Node* getNodeFromPoint(int x, int y);

    inline bool isZoomFullContent() const { return m_isZoomFullContent; };
    inline void setZoomFullContent(bool flag) { m_isZoomFullContent = flag; };

    void setEditable(bool enable) { m_editable = enable; }
    bool editable() const { return m_editable; }

    WebCore::GraphicsLayer* rootGraphicsLayer() { return m_rootGraphicsLayer; }
    void setRootGraphicsLayer(WebCore::GraphicsLayer* layer) { m_rootGraphicsLayer = layer; }

    void addOverlay(WKCOverlayIf* overlay, int zOrder, int fixedDirectionFlag);
    void removeOverlay(WKCOverlayIf* overlay);
    void updateOverlay(const WebCore::IntRect& rect, bool immediate);

private:
    WKCWebViewPrivate(WKCWebView* parent, WKCClientBuilders& builders);
    bool construct();

    bool prepareDrawings();
#ifdef USE_WKC_CAIRO
    bool recoverFromCairoError();
#endif

private:
    WKCWebView* m_parent;
    WKCClientBuilders& m_clientBuilders;
    ChromeClientWKC* m_chromeClient; // non-owning; owned by WebCore::Page
    std::unique_ptr<WebCore::NotificationClient> m_notificationClient;

    // core
    WebCore::Page* m_corePage;
    WKC::PagePrivate* m_wkcCorePage;

    // instances
    WKCWebFrame* m_mainFrame;
    InspectorBackendClientWKC* m_inspector; // owned by WebCore::Page
    DropDownListClientWKC* m_dropdownlist;
    WKCSettings* m_settings;

    // offscreen
    void* m_drawContext;
    void* m_offscreen;
#ifdef USE_WKC_CAIRO
    int m_offscreenFormat;
    void* m_offscreenBitmap;
    int m_offscreenRowBytes;
    WKCSize m_offscreenSize;
#endif

    WebCore::IntSize m_desktopSize;
    WebCore::IntSize m_viewSize;
    WebCore::IntSize m_defaultDesktopSize;
    WebCore::IntSize m_defaultViewSize;

    float m_opticalZoomLevel;
    WKCFloatPoint m_opticalZoomOffset;

    bool m_isZoomFullContent;
    bool m_isTransparent;
    WKC::LoadStatus m_loadStatus;

    // temporary string resources
    unsigned short* m_encoding;
    unsigned short* m_customEncoding;
    unsigned short* m_selectionText;

    bool m_forceTerminated;

    WKC::NodePrivate* m_focusedNode;
    WKC::NodePrivate* m_nodeFromPoint;
    WKC::NodePrivate* m_editableNode; // input or textarea or contenteditable

    WebCore::Node* m_lastNodeUnderMouse;

    bool m_editable;

    WebCore::GraphicsLayer* m_rootGraphicsLayer;

    // OwnPtr was removed from WTF — use std::unique_ptr
    std::unique_ptr<WKCOverlayList> m_overlayList;
};

} // namespace

#endif // WKCWebViewPrivate_h
