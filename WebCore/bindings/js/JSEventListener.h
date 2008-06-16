/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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

#ifndef JSEventListener_h
#define JSEventListener_h

#include "EventListener.h"
#include "PlatformString.h"
#include <kjs/protect.h>

namespace WebCore {

    class Event;
    class JSDOMWindow;
    class Node;

    class JSAbstractEventListener : public EventListener {
    public:
        virtual void handleEvent(Event*, bool isWindowEvent);
        virtual bool isHTMLEventListener() const;
        virtual KJS::JSObject* listenerObj() const = 0;
        virtual JSDOMWindow* window() const = 0;

    protected:
        JSAbstractEventListener(bool isHTML)
            : m_isHTML(isHTML)
        {
        }

    private:
        bool m_isHTML;
    };

    class JSUnprotectedEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSUnprotectedEventListener> create(KJS::JSObject* listener, JSDOMWindow* window, bool isHTML)
        {
            return adoptRef(new JSUnprotectedEventListener(listener, window, isHTML));
        }
        virtual ~JSUnprotectedEventListener();

        virtual KJS::JSObject* listenerObj() const;
        virtual JSDOMWindow* window() const;
        void clearWindow();
        void mark();

    private:
        JSUnprotectedEventListener(KJS::JSObject* listener, JSDOMWindow*, bool isHTML);

        KJS::JSObject* m_listener;
        JSDOMWindow* m_window;
    };

    class JSEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSEventListener> create(KJS::JSObject* listener, JSDOMWindow* window, bool isHTML)
        {
            return adoptRef(new JSEventListener(listener, window, isHTML));
        }
        virtual ~JSEventListener();

        virtual KJS::JSObject* listenerObj() const;
        virtual JSDOMWindow* window() const;
        void clearWindow();

    protected:
        JSEventListener(KJS::JSObject* listener, JSDOMWindow*, bool isHTML);

        mutable KJS::ProtectedPtr<KJS::JSObject> m_listener;

    private:
        KJS::ProtectedPtr<JSDOMWindow> m_window;
    };

    class JSLazyEventListener : public JSEventListener {
    public:
        static PassRefPtr<JSLazyEventListener> create(const String& functionName, const String& code, JSDOMWindow* window, Node* node, int lineNumber)
        {
            return adoptRef(new JSLazyEventListener(functionName, code, window, node, lineNumber));
        }
        virtual KJS::JSObject* listenerObj() const;

    protected:
        JSLazyEventListener(const String& functionName, const String& code, JSDOMWindow*, Node*, int lineNumber);

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

} // namespace WebCore

#endif // JSEventListener_h
