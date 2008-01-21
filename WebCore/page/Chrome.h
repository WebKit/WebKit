// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Chrome_h
#define Chrome_h

#include "FocusDirection.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#ifndef __OBJC__
class NSView;
#endif
#endif

namespace WebCore {

    class ChromeClient;
    class ContextMenu;
    class FloatRect;
    class Frame;
    class HitTestResult;
    class IntRect;
    class Page;
    class String;
    
    struct FrameLoadRequest;
    struct WindowFeatures;
    
    enum MessageSource {
        HTMLMessageSource,
        XMLMessageSource,
        JSMessageSource,
        CSSMessageSource,
        OtherMessageSource
    };

    enum MessageLevel {
        TipMessageLevel,
        LogMessageLevel,
        WarningMessageLevel,
        ErrorMessageLevel
    };

    class Chrome : Noncopyable {
    public:
        Chrome(Page*, ChromeClient*);
        ~Chrome();

        ChromeClient* client() { return m_client; }

        void setWindowRect(const FloatRect&) const;
        FloatRect windowRect() const;

        FloatRect pageRect() const;
        
        float scaleFactor();

        void focus() const;
        void unfocus() const;

        bool canTakeFocus(FocusDirection) const;
        void takeFocus(FocusDirection) const;

        Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&) const;
        void show() const;

        bool canRunModal() const;
        bool canRunModalNow() const;
        void runModal() const;

        void setToolbarsVisible(bool) const;
        bool toolbarsVisible() const;
        
        void setStatusbarVisible(bool) const;
        bool statusbarVisible() const;
        
        void setScrollbarsVisible(bool) const;
        bool scrollbarsVisible() const;
        
        void setMenubarVisible(bool) const;
        bool menubarVisible() const;
        
        void setResizable(bool) const;

        void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);

        bool canRunBeforeUnloadConfirmPanel();
        bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame);

        void closeWindowSoon();

        void runJavaScriptAlert(Frame*, const String&);
        bool runJavaScriptConfirm(Frame*, const String&);
        bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result);                
        void setStatusbarText(Frame*, const String&);
        bool shouldInterruptJavaScript();

        IntRect windowResizerRect() const;
        void addToDirtyRegion(const IntRect&);
        void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
        void updateBackingStore();

        void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

        void setToolTip(const HitTestResult&);

        void print(Frame*);
        
#if PLATFORM(MAC)
        void focusNSView(NSView*);
#endif

    private:
        Page* m_page;
        ChromeClient* m_client;
    };
}

#endif // Chrome_h
