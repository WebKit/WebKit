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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef kjs_events_h
#define kjs_events_h

#include "EventListener.h"
#include "PlatformString.h"
#include "kjs_dom.h"
#include "kjs_html.h"
#include <kjs/protect.h>

namespace KJS {
    class Window;
}

namespace WebCore {

    class Clipboard;
    class Event;

    class JSAbstractEventListener : public EventListener {
    public:
        JSAbstractEventListener(bool html = false);

        virtual void handleEvent(Event*, bool isWindowEvent);
        virtual bool isHTMLEventListener() const;
        virtual KJS::JSObject* listenerObj() const = 0;
        virtual KJS::Window* windowObj() const = 0;

    private:
        bool m_html;
    };

    class JSUnprotectedEventListener : public JSAbstractEventListener {
    public:
        JSUnprotectedEventListener(KJS::JSObject* listener, KJS::Window*, bool html = false);
        virtual ~JSUnprotectedEventListener();

        virtual KJS::JSObject* listenerObj() const;
        virtual KJS::Window* windowObj() const;
        void clearWindowObj();
        void mark();
    private:
        KJS::JSObject* m_listener;
        KJS::Window* m_win;
    };

    class JSEventListener : public JSAbstractEventListener {
    public:
        JSEventListener(KJS::JSObject* listener, KJS::Window*, bool html = false);
        virtual ~JSEventListener();

        virtual KJS::JSObject* listenerObj() const;
        virtual KJS::Window* windowObj() const;
        void clearWindowObj();

    protected:
        mutable KJS::ProtectedPtr<KJS::JSObject> m_listener;

    private:
        KJS::ProtectedPtr<KJS::Window> m_win;
    };

    class JSLazyEventListener : public JSEventListener {
    public:
        JSLazyEventListener(const String& functionName, const String& code, KJS::Window*, Node*, int lineNumber = 0);
        virtual KJS::JSObject* listenerObj() const;

    private:
        virtual KJS::JSValue* eventParameterName() const;
        void parseCode() const;

        mutable String m_functionName;
        mutable String m_code;
        mutable bool m_parsed;
        int m_lineNumber;
        Node* m_originalNode;
    };

    KJS::JSValue* getNodeEventListener(Node*, const AtomicString& eventType);

    class JSClipboard : public KJS::DOMObject {
    public:
        JSClipboard(KJS::JSObject* prototype, Clipboard*);
        virtual ~JSClipboard();

        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        virtual void put(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue*, int attr = KJS::None);
        void putValueProperty(KJS::ExecState*, int token, KJS::JSValue*, int attr);

        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;

        enum { ClearData, GetData, SetData, Types, SetDragImage, DropEffect, EffectAllowed };

        Clipboard* impl() const { return m_impl.get(); }

    private:
        RefPtr<Clipboard> m_impl;
    };

    KJS::JSValue* toJS(KJS::ExecState*, Clipboard*);
    Clipboard* toClipboard(KJS::JSValue*);

    // Functions
    KJS::JSValue* jsClipboardPrototypeFunctionClearData(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
    KJS::JSValue* jsClipboardPrototypeFunctionGetData(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
    KJS::JSValue* jsClipboardPrototypeFunctionSetData(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
    KJS::JSValue* jsClipboardPrototypeFunctionSetDragImage(KJS::ExecState*, KJS::JSObject*, const KJS::List&);

} // namespace WebCore

#endif // kjs_events_h
