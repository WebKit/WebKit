/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * All rights reserved.
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

#ifndef ChromeClientWx_H
#define ChromeClientWx_H

#include "ChromeClient.h"
#include "FocusDirection.h"
#include "IntRect.h"
#include "WebView.h"

namespace WebCore {

class ChromeClientWx : public ChromeClient {
public:
    ChromeClientWx(wxWebView*);
    virtual ~ChromeClientWx();
    virtual void chromeDestroyed();

    virtual void setWindowRect(const FloatRect&);
    virtual FloatRect windowRect();

    virtual FloatRect pageRect();

    virtual float scaleFactor();

    virtual void focus();
    virtual void unfocus();

    virtual bool canTakeFocus(FocusDirection);
    virtual void takeFocus(FocusDirection);

    virtual void focusedNodeChanged(Node*);
    virtual void focusedFrameChanged(Frame*);

    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&);
    virtual Page* createModalDialog(Frame*, const FrameLoadRequest&);
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

    virtual void addMessageToConsole(MessageSource source,
                                     MessageType type,
                                     MessageLevel level,
                                     const String& message,
                                     unsigned int lineNumber,
                                     const String& sourceID);

    virtual bool canRunBeforeUnloadConfirmPanel();
    virtual bool runBeforeUnloadConfirmPanel(const String& message,
                                             Frame* frame);

    virtual void closeWindowSoon();
    
    virtual void runJavaScriptAlert(Frame*, const String&);
    virtual bool runJavaScriptConfirm(Frame*, const String&);
    virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result);
    virtual void setStatusbarText(const String&);
    virtual bool shouldInterruptJavaScript();
    
    virtual WebCore::KeyboardUIMode keyboardUIMode();

    virtual IntRect windowResizerRect() const;
    virtual void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
    virtual void updateBackingStore();
    
    virtual void invalidateWindow(const IntRect&, bool);
    virtual void invalidateContentsAndWindow(const IntRect&, bool);
    virtual void invalidateContentsForSlowScroll(const IntRect&, bool);
    virtual void scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect);

    virtual IntPoint screenToWindow(const IntPoint&) const;
    virtual IntRect windowToScreen(const IntRect&) const;
    virtual PlatformPageClient platformPageClient() const;
    virtual void contentsSizeChanged(Frame*, const IntSize&) const;

    virtual void scrollbarsModeDidChange() const { }
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

    virtual void setToolTip(const String&, TextDirection);

    virtual void print(Frame*);

#if ENABLE(DATABASE)
    virtual void exceededDatabaseQuota(Frame*, const String&);
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);
    virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*);
#endif

#if ENABLE(CONTEXT_MENUS)
    virtual void showContextMenu() { }
#endif

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>);
    virtual void chooseIconForFiles(const Vector<String>&, FileChooser*);

    virtual void formStateDidChange(const Node*) { }

    virtual void setCursor(const Cursor&);

    virtual void scrollRectIntoView(const IntRect&, const ScrollView*) const {}

    virtual void requestGeolocationPermissionForFrame(Frame*, Geolocation*);
    virtual void cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*) { }

    virtual bool selectItemWritingDirectionIsNatural();
    virtual bool selectItemAlignmentFollowsMenuWritingDirection();
    virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const;
    virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const;

private:
    wxWebView* m_webView;
};

}
#endif // ChromeClientWx_H
