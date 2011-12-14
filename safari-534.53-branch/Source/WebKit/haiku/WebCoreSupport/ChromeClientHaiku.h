/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
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

#ifndef ChromeClientHaiku_h
#define ChromeClientHaiku_h

#include "ChromeClient.h"
#include "FloatRect.h"
#include "RefCounted.h"

namespace WebCore {

    class FloatRect;
    class Page;
    struct FrameLoadRequest;

    class ChromeClientHaiku : public ChromeClient {
    public:
        ChromeClientHaiku();
        virtual ~ChromeClientHaiku();
        virtual void chromeDestroyed();

        virtual void* webView() const { return 0; }
        virtual void setWindowRect(const FloatRect&);
        virtual FloatRect windowRect();

        virtual FloatRect pageRect();

        virtual void focus();
        virtual void unfocus();

        virtual bool canTakeFocus(FocusDirection);
        virtual void takeFocus(FocusDirection);

        virtual void focusedNodeChanged(Node*);
        virtual void focusedFrameChanged(Frame*);

        virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&);
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

        virtual void addMessageToConsole(const String& message, unsigned int lineNumber,
                                 const String& sourceID);
        virtual void addMessageToConsole(MessageSource, MessageLevel, const String& message,
                                 unsigned int lineNumber, const String& sourceID);
        virtual void addMessageToConsole(MessageSource, MessageType, MessageLevel,
                                 const String&, unsigned int, const String&);

        virtual bool canRunBeforeUnloadConfirmPanel();

        virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame);

        virtual void closeWindowSoon();

        virtual void runJavaScriptAlert(Frame*, const String&);
        virtual bool runJavaScriptConfirm(Frame*, const String&);
        virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result);
        virtual bool shouldInterruptJavaScript();

        virtual void setStatusbarText(const WTF::String&);
        virtual WebCore::KeyboardUIMode keyboardUIMode();
        virtual IntRect windowResizerRect() const;

        virtual void invalidateWindow(const IntRect&, bool);
        virtual void invalidateContentsAndWindow(const IntRect&, bool);
        virtual void invalidateContentsForSlowScroll(const IntRect&, bool);
        virtual void scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect);

        virtual IntPoint screenToWindow(const IntPoint&) const;
        virtual IntRect windowToScreen(const IntRect&) const;
        virtual PlatformPageClient platformPageClient() const;
        virtual void contentsSizeChanged(Frame*, const IntSize&) const;
        virtual void scrollRectIntoView(const IntRect&, const ScrollView*) const;

        void addToDirtyRegion(const IntRect&);
        void scrollBackingStore(int, int, const IntRect&, const IntRect&);
        void updateBackingStore();

        virtual void scrollbarsModeDidChange() const { }
        virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

        void setToolTip(const String&);

        virtual void setToolTip(const String&, TextDirection);

        void print(Frame*);

#if ENABLE(DATABASE)
        virtual void exceededDatabaseQuota(Frame*, const String& databaseName);
#endif

        virtual bool selectItemWritingDirectionIsNatural();
        virtual bool selectItemAlignmentFollowsMenuWritingDirection();
        virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const;
        virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);
        virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*);
#endif
#if ENABLE(CONTEXT_MENUS)
        virtual void showContextMenu() { }
#endif

        // This is an asynchronous call. The ChromeClient can display UI asking the user for permission
        // to use Geolococation.
        virtual void requestGeolocationPermissionForFrame(Frame*, Geolocation*);
        virtual void cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*) { }

        virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>);
        virtual void chooseIconForFiles(const Vector<String>&, FileChooser*);

        virtual void setCursor(const Cursor&);
        virtual void setCursorHiddenUntilMouseMoves(bool);

        // Notification that the given form element has changed. This function
        // will be called frequently, so handling should be very fast.
        virtual void formStateDidChange(const Node*);

        virtual bool shouldRubberBandInDirection(WebCore::ScrollDirection) const { return true; }
        virtual void numWheelEventHandlersChanged(unsigned) { }
    };

} // namespace WebCore

#endif

