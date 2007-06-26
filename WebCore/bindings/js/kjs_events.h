/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef kjs_events_h
#define kjs_events_h

#include "EventListener.h"
#include "PlatformString.h"
#include "kjs_dom.h"
#include "kjs_html.h"

namespace WebCore {
    class Clipboard;
    class Event;
    class KeyboardEvent;
    class MouseEvent;
    class MutationEvent;
    class UIEvent;
}

namespace KJS {

    class Window;
    class JSClipboard;
    
    class JSAbstractEventListener : public WebCore::EventListener {
    public:
        JSAbstractEventListener(bool HTML = false);
        virtual void handleEvent(WebCore::Event*, bool isWindowEvent);
        virtual bool isHTMLEventListener() const;
        virtual JSObject* listenerObj() const = 0;
        virtual Window* windowObj() const = 0;
    private:
        bool html;
    };

    class JSUnprotectedEventListener : public JSAbstractEventListener {
    public:
        JSUnprotectedEventListener(JSObject* listener, Window*, bool HTML = false);
        virtual ~JSUnprotectedEventListener();
        virtual JSObject* listenerObj() const;
        virtual Window* windowObj() const;
        void clearWindowObj();
        virtual void mark();
    private:
        JSObject* listener;
        Window* win;
    };

    class JSEventListener : public JSAbstractEventListener {
    public:
        JSEventListener(JSObject* listener, Window*, bool HTML = false);
        virtual ~JSEventListener();
        virtual JSObject* listenerObj() const;
        virtual Window* windowObj() const;
        void clearWindowObj();
    protected:
        mutable ProtectedPtr<JSObject> listener;
    private:
        ProtectedPtr<Window> win;
    };

    class JSLazyEventListener : public JSEventListener {
    public:
        JSLazyEventListener(const WebCore::String& functionName, const WebCore::String& code, Window*, WebCore::Node*, int lineno = 0);
        virtual JSObject* listenerObj() const;
    private:
        virtual JSValue* eventParameterName() const;
        void parseCode() const;

        mutable WebCore::String m_functionName;
        mutable WebCore::String code;
        mutable bool parsed;
        int lineNumber;
        WebCore::Node* originalNode;
    };

    JSValue* getNodeEventListener(WebCore::Node* n, const WebCore::AtomicString& eventType);

    class JSClipboard : public DOMObject {
        friend class JSClipboardPrototypeFunction;
    public:
        JSClipboard(ExecState*, WebCore::Clipboard* ds);
        virtual ~JSClipboard();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual void put(ExecState*, const Identifier&, JSValue*, int attr = None);
        void putValueProperty(ExecState*, int token, JSValue*, int attr);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        enum { ClearData, GetData, SetData, Types, SetDragImage, DropEffect, EffectAllowed };

        WebCore::Clipboard* impl() const { return m_impl.get(); }

    private:
        RefPtr<WebCore::Clipboard> m_impl;
    };

    JSValue* toJS(ExecState*, WebCore::Clipboard*);
    WebCore::Clipboard* toClipboard(JSValue*);

} // namespace

#endif
