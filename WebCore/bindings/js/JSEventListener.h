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
#include <runtime/Protect.h>

namespace WebCore {

    class Event;
    class JSDOMGlobalObject;
    class Node;

    class JSAbstractEventListener : public EventListener {
    public:
        virtual void handleEvent(Event*, bool isWindowEvent);
        virtual bool isInline() const;
        virtual JSC::JSObject* listenerObj() const = 0;
        virtual JSDOMGlobalObject* globalObject() const = 0;

    protected:
        JSAbstractEventListener(bool isInline)
            : m_isInline(isInline)
        {
        }

    private:
        bool m_isInline;
    };

    class JSEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSEventListener> create(JSC::JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
        {
            return adoptRef(new JSEventListener(listener, globalObject, isInline));
        }
        virtual ~JSEventListener();

        virtual JSC::JSObject* listenerObj() const;
        virtual JSDOMGlobalObject* globalObject() const;
        void clearGlobalObject();
        void mark();

    private:
        JSEventListener(JSC::JSObject* listener, JSDOMGlobalObject*, bool isInline);

        JSC::JSObject* m_listener;
        JSDOMGlobalObject* m_globalObject;
    };

    class JSProtectedEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSProtectedEventListener> create(JSC::JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
        {
            return adoptRef(new JSProtectedEventListener(listener, globalObject, isInline));
        }
        virtual ~JSProtectedEventListener();

        virtual JSC::JSObject* listenerObj() const;
        virtual JSDOMGlobalObject* globalObject() const;
        void clearGlobalObject();

    protected:
        JSProtectedEventListener(JSC::JSObject* listener, JSDOMGlobalObject*, bool isInline);

        mutable JSC::ProtectedPtr<JSC::JSObject> m_listener;

    private:
        JSC::ProtectedPtr<JSDOMGlobalObject> m_globalObject;
    };

    class JSLazyEventListener : public JSProtectedEventListener {
    public:
        enum LazyEventListenerType {
            HTMLLazyEventListener
#if ENABLE(SVG)
            , SVGLazyEventListener
#endif
        };

        virtual bool wasCreatedFromMarkup() const { return true; }

        static PassRefPtr<JSLazyEventListener> create(LazyEventListenerType type, const String& functionName, const String& code, JSDOMGlobalObject* globalObject, Node* node, int lineNumber)
        {
            return adoptRef(new JSLazyEventListener(type, functionName, code, globalObject, node, lineNumber));
        }
        virtual JSC::JSObject* listenerObj() const;

    protected:
        JSLazyEventListener(LazyEventListenerType type, const String& functionName, const String& code, JSDOMGlobalObject*, Node*, int lineNumber);

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
