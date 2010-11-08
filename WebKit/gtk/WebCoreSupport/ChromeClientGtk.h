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
#include "KURL.h"
#include "PopupMenu.h"
#include "SearchPopupMenu.h"

typedef struct _WebKitWebView WebKitWebView;

namespace WebCore {
class PopupMenuClient;
}

namespace WebKit {

    class ChromeClient : public WebCore::ChromeClient {
    public:
        ChromeClient(WebKitWebView*);
        WebKitWebView* webView() const { return m_webView; }

        virtual void chromeDestroyed();

        virtual void setWindowRect(const WebCore::FloatRect&);
        virtual WebCore::FloatRect windowRect();

        virtual WebCore::FloatRect pageRect();

        virtual float scaleFactor();

        virtual void focus();
        virtual void unfocus();

        virtual bool canTakeFocus(WebCore::FocusDirection);
        virtual void takeFocus(WebCore::FocusDirection);

        virtual void focusedNodeChanged(WebCore::Node*);
        virtual void focusedFrameChanged(WebCore::Frame*);

        virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&);
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

        virtual void addMessageToConsole(WebCore::MessageSource source, WebCore::MessageType type,
                                         WebCore::MessageLevel level, const WTF::String& message,
                                         unsigned int lineNumber, const WTF::String& sourceID);

        virtual bool canRunBeforeUnloadConfirmPanel();
        virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame* frame);

        virtual void closeWindowSoon();

        virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&);
        virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&);
        virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result);
        virtual void setStatusbarText(const WTF::String&);
        virtual bool shouldInterruptJavaScript();
        virtual bool tabsToLinks() const;

        virtual WebCore::IntRect windowResizerRect() const;

        virtual void invalidateWindow(const WebCore::IntRect&, bool);
        virtual void invalidateContentsAndWindow(const WebCore::IntRect&, bool);
        virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool);
        virtual void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect);

        virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
        virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
        virtual PlatformPageClient platformPageClient() const;
        virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const;

        virtual void scrollbarsModeDidChange() const;
        virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags);

        virtual void setToolTip(const WTF::String&, WebCore::TextDirection);

        virtual void print(WebCore::Frame*);
#if ENABLE(DATABASE)
        virtual void exceededDatabaseQuota(WebCore::Frame*, const WTF::String&);
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);
        virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*);
#endif
#if ENABLE(CONTEXT_MENUS)
        virtual void showContextMenu() { }
#endif
        virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>);
        virtual void chooseIconForFiles(const Vector<WTF::String>&, WebCore::FileChooser*);

        virtual void formStateDidChange(const WebCore::Node*) { }

        virtual PassOwnPtr<WebCore::HTMLParserQuirks> createHTMLParserQuirks() { return 0; }

        virtual void setCursor(const WebCore::Cursor&);

        virtual void scrollRectIntoView(const WebCore::IntRect&, const WebCore::ScrollView*) const {}
        virtual void requestGeolocationPermissionForFrame(WebCore::Frame*, WebCore::Geolocation*);
        virtual void cancelGeolocationPermissionRequestForFrame(WebCore::Frame*, WebCore::Geolocation*);

        virtual bool selectItemWritingDirectionIsNatural();
        virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const;
        virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const;
#if ENABLE(VIDEO)
        virtual bool supportsFullscreenForNode(const WebCore::Node*);
        virtual void enterFullscreenForNode(WebCore::Node*);
        virtual void exitFullscreenForNode(WebCore::Node*);
#endif

    private:
        WebKitWebView* m_webView;
        WebCore::KURL m_hoveredLinkURL;
    };
}

#endif // ChromeClient_h
