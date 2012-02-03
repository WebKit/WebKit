/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef ChromeClientGtk_h
#define ChromeClientGtk_h

#include "ChromeClient.h"
#include "GtkAdjustmentWatcher.h"
#include "IntRect.h"
#include "IntSize.h"
#include "KURL.h"
#include "PopupMenu.h"
#include "Region.h"
#include "SearchPopupMenu.h"
#include "Timer.h"

using namespace WebCore;
typedef struct _WebKitWebView WebKitWebView;

namespace WebCore {
class PopupMenuClient;
}

namespace WebKit {

    class ChromeClient : public WebCore::ChromeClient {
    public:
        ChromeClient(WebKitWebView*);
        virtual void* webView() const { return m_webView; }
        GtkAdjustmentWatcher* adjustmentWatcher() { return &m_adjustmentWatcher; }

        virtual void chromeDestroyed();

        virtual void setWindowRect(const FloatRect&);
        virtual FloatRect windowRect();

        virtual FloatRect pageRect();

        virtual void focus();
        virtual void unfocus();

        virtual bool canTakeFocus(FocusDirection);
        virtual void takeFocus(FocusDirection);

        virtual void focusedNodeChanged(Node*);
        virtual void focusedFrameChanged(Frame*);

        virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&);
        virtual void show();

        virtual bool canRunModal();
        virtual void runModal();

        virtual void setToolbarsVisible(bool);
        virtual bool toolbarsVisible();

        virtual void setStatusbarVisible(bool);
        virtual bool statusbarVisible();

        virtual void setScrollbarsVisible(bool);
        virtual bool scrollbarsVisible();

        virtual void setMenubarVisible(bool);
        virtual bool menubarVisible();

        virtual void setResizable(bool);

        virtual void addMessageToConsole(MessageSource source, MessageType type,
                                         MessageLevel level, const WTF::String& message,
                                         unsigned int lineNumber, const WTF::String& sourceID);

        virtual bool canRunBeforeUnloadConfirmPanel();
        virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, Frame* frame);

        virtual void closeWindowSoon();

        virtual void runJavaScriptAlert(Frame*, const WTF::String&);
        virtual bool runJavaScriptConfirm(Frame*, const WTF::String&);
        virtual bool runJavaScriptPrompt(Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result);
        virtual void setStatusbarText(const WTF::String&);
        virtual bool shouldInterruptJavaScript();
        virtual KeyboardUIMode keyboardUIMode();

        virtual IntRect windowResizerRect() const;
#if ENABLE(REGISTER_PROTOCOL_HANDLER) 
        virtual void registerProtocolHandler(const WTF::String&, const WTF::String&, const WTF::String&, const WTF::String&); 
#endif 
        virtual void invalidateRootView(const IntRect&, bool);
        virtual void invalidateContentsAndRootView(const IntRect&, bool);
        virtual void invalidateContentsForSlowScroll(const IntRect&, bool);
        virtual void scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect);

        virtual IntPoint screenToRootView(const IntPoint&) const;
        virtual IntRect rootViewToScreen(const IntRect&) const;
        virtual PlatformPageClient platformPageClient() const;
        virtual void contentsSizeChanged(Frame*, const IntSize&) const;

        virtual void scrollbarsModeDidChange() const;
        virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

        virtual void setToolTip(const WTF::String&, TextDirection);

        virtual void dispatchViewportPropertiesDidChange(const ViewportArguments&) const;

        virtual void print(Frame*);
#if ENABLE(SQL_DATABASE)
        virtual void exceededDatabaseQuota(Frame*, const WTF::String&);
#endif
        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);
        virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t totalSpaceNeeded);
#if ENABLE(CONTEXT_MENUS)
        virtual void showContextMenu() { }
#endif
        virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>);
        virtual void loadIconForFiles(const Vector<WTF::String>&, FileIconLoader*);

        virtual void formStateDidChange(const Node*) { }

        virtual void setCursor(const Cursor&);
        virtual void setCursorHiddenUntilMouseMoves(bool);

        virtual void scrollRectIntoView(const IntRect&) const { }
        virtual void requestGeolocationPermissionForFrame(Frame*, Geolocation*);
        virtual void cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*);

        virtual bool selectItemWritingDirectionIsNatural();
        virtual bool selectItemAlignmentFollowsMenuWritingDirection();
        virtual bool hasOpenedPopup() const;
        virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const;
        virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const;
#if ENABLE(VIDEO)
        virtual bool supportsFullscreenForNode(const Node*);
        virtual void enterFullscreenForNode(Node*);
        virtual void exitFullscreenForNode(Node*);
#endif

#if ENABLE(FULLSCREEN_API)
        virtual bool supportsFullScreenForElement(const Element*, bool withKeyboard);
        virtual void enterFullScreenForElement(Element*);
        virtual void exitFullScreenForElement(Element*);
#endif

        virtual bool shouldRubberBandInDirection(ScrollDirection) const { return true; }
        virtual void numWheelEventHandlersChanged(unsigned) { }

#if USE(ACCELERATED_COMPOSITING) 
        virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*);
        virtual void setNeedsOneShotDrawingSynchronization();
        virtual void scheduleCompositingLayerSync();
        virtual CompositingTriggerFlags allowedCompositingTriggers() const;
#endif 

        void performAllPendingScrolls();
        void paint(Timer<ChromeClient>*);
        void widgetSizeChanged(const IntSize& oldWidgetSize, IntSize newSize);

    private:
        WebKitWebView* m_webView;
        GtkAdjustmentWatcher m_adjustmentWatcher;
        KURL m_hoveredLinkURL;
        unsigned int m_closeSoonTimer;

        Timer <ChromeClient> m_displayTimer;
        Region m_dirtyRegion;
        Vector<IntRect> m_rectsToScroll;
        Vector<IntSize> m_scrollOffsets;
        double m_lastDisplayTime;
        unsigned int m_repaintSoonSourceId;
    };
}

#endif // ChromeClient_h
