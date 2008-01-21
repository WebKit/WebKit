// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007, 2008 Apple, Inc. All rights reserved.
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

#ifndef ChromeClient_h
#define ChromeClient_h

#include "FocusDirection.h"

namespace WebCore {

    class FloatRect;
    class Frame;
    class HitTestResult;
    class IntRect;
    class Page;
    class String;
    
    struct FrameLoadRequest;
    struct WindowFeatures;
    
    class ChromeClient {
    public:
        virtual void chromeDestroyed() = 0;
        
        virtual void setWindowRect(const FloatRect&) = 0;
        virtual FloatRect windowRect() = 0;
        
        virtual FloatRect pageRect() = 0;
        
        virtual float scaleFactor() = 0;
    
        virtual void focus() = 0;
        virtual void unfocus() = 0;

        virtual bool canTakeFocus(FocusDirection) = 0;
        virtual void takeFocus(FocusDirection) = 0;

        // The Frame pointer provides the ChromeClient with context about which
        // Frame wants to create the new Page.  Also, the newly created window
        // should not be shown to the user until the ChromeClient of the newly
        // created Page has its show method called.
        virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&) = 0;
        virtual void show() = 0;

        virtual bool canRunModal() = 0;
        virtual void runModal() = 0;

        virtual void setToolbarsVisible(bool) = 0;
        virtual bool toolbarsVisible() = 0;
        
        virtual void setStatusbarVisible(bool) = 0;
        virtual bool statusbarVisible() = 0;
        
        virtual void setScrollbarsVisible(bool) = 0;
        virtual bool scrollbarsVisible() = 0;
        
        virtual void setMenubarVisible(bool) = 0;
        virtual bool menubarVisible() = 0;

        virtual void setResizable(bool) = 0;
        
        virtual void addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID) = 0;

        virtual bool canRunBeforeUnloadConfirmPanel() = 0;
        virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame) = 0;

        virtual void closeWindowSoon() = 0;
        
        virtual void runJavaScriptAlert(Frame*, const String&) = 0;
        virtual bool runJavaScriptConfirm(Frame*, const String&) = 0;
        virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) = 0;
        
        virtual void setStatusbarText(const String&) = 0;
        virtual bool shouldInterruptJavaScript() = 0;
        virtual bool tabsToLinks() const = 0;

        virtual IntRect windowResizerRect() const = 0;
        virtual void addToDirtyRegion(const IntRect&) = 0;
        virtual void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect) = 0;
        virtual void updateBackingStore() = 0;

        virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) = 0;

        virtual void setToolTip(const String&) = 0;

        virtual void print(Frame*) = 0;

        virtual void exceededDatabaseQuota(Frame*, const String& databaseName) = 0;

    protected:
        virtual ~ChromeClient() { }
    };

}

#endif // ChromeClient_h
