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
        virtual bool isInline() const;
        virtual JSC::JSObject* listenerObj() const = 0;
        virtual JSDOMWindow* window() const = 0;

    protected:
        JSAbstractEventListener(bool isInline)
            : m_isInline(isInline)
        {
        }

    private:
        bool m_isInline;
    };

    class JSUnprotectedEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSUnprotectedEventListener> create(JSC::JSObject* listener, JSDOMWindow* window, bool isInline)
        {
            return adoptRef(new JSUnprotectedEventListener(listener, window, isInline));
        }
        virtual ~JSUnprotectedEventListener();

        virtual JSC::JSObject* listenerObj() const;
        virtual JSDOMWindow* window() const;
        void clearWindow();
        void mark();

    private:
        JSUnprotectedEventListener(JSC::JSObject* listener, JSDOMWindow*, bool isInline);

        JSC::JSObject* m_listener;
        JSDOMWindow* m_window;
    };

    class JSEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSEventListener> create(JSC::JSObject* listener, JSDOMWindow* window, bool isInline)
        {
            return adoptRef(new JSEventListener(listener, window, isInline));
        }
        virtual ~JSEventListener();

        virtual JSC::JSObject* listenerObj() const;
        virtual JSDOMWindow* window() const;
        void clearWindow();

    protected:
        JSEventListener(JSC::JSObject* listener, JSDOMWindow*, bool isInline);

        mutable JSC::ProtectedPtr<JSC::JSObject> m_listener;

    private:
        JSC::ProtectedPtr<JSDOMWindow> m_window;
    };

    class JSLazyEventListener : public JSEventListener {
    public:
        enum LazyEventListenerType {
            HTMLLazyEventListener
#if ENABLE(SVG)
            , SVGLazyEventListener
#endif
        };

        virtual bool wasCreatedFromMarkup() const { return true; }

        static PassRefPtr<JSLazyEventListener> create(LazyEventListenerType type, const String& functionName, const String& code, JSDOMWindow* window, Node* node, int lineNumber)
        {
            return adoptRef(new JSLazyEventListener(type, functionName, code, window, node, lineNumber));
        }
        virtual JSC::JSObject* listenerObj() const;

    protected:
        JSLazyEventListener(LazyEventListenerType type, const String& functionName, const String& code, JSDOMWindow*, Node*, int lineNumber);

    private:
        void parseCode() const;

        mutable String m_functionName;
        mutable String m_code;
        mutable bool m_parsed;
        int m_lineNumber;
        Node* m_originalNode;

        LazyEventListenerType m_type;
    };

} // namespace WebCore

#endif // JSEventListener_h
