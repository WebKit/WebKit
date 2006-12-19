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

#ifndef Chrome_h
#define Chrome_h

#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class ChromeClient;
    class ContextMenu;
    class FloatRect;
    class Frame;
    class Page;
    class String;
    struct FrameLoadRequest;
    
    class Chrome {
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

        Page* createWindow(const FrameLoadRequest&) const;
        Page* createModalDialog(const FrameLoadRequest&) const;
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

        void addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID);

        bool canRunBeforeUnloadConfirmPanel();
        bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame);

        void closeWindowSoon();

    private:
        Page* m_page;
        ChromeClient* m_client;
    };
}

#endif // Chrome_h
