// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef ChromeClient_h
#define ChromeClient_h

#include "FocusDirection.h"

namespace WebCore {

    class FloatRect;
    class Frame;
    class Page;
    class String;
    
    struct FrameLoadRequest;
    
    class ChromeClient {
    public:
        virtual ~ChromeClient() {  }
        virtual void chromeDestroyed() = 0;
        
        virtual void setWindowRect(const FloatRect&) = 0;
        virtual FloatRect windowRect() = 0;
        
        virtual FloatRect pageRect() = 0;
        
        virtual float scaleFactor() = 0;
    
        virtual void focus() = 0;
        virtual void unfocus() = 0;

        virtual bool canTakeFocus(FocusDirection) = 0;
        virtual void takeFocus(FocusDirection) = 0;

        virtual Page* createWindow(const FrameLoadRequest&) = 0;
        virtual Page* createModalDialog(const FrameLoadRequest&) = 0;
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
};

}

#endif // ChromeClient_h
