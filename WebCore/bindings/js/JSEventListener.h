/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSDOMWindow.h"
#include <runtime/Protect.h>

namespace WebCore {

    class JSDOMGlobalObject;

    class JSAbstractEventListener : public EventListener {
    public:
        bool isInline() const { return m_isInline; }

    protected:
        JSAbstractEventListener(bool isInline)
            : m_isInline(isInline)
        {
        }

    private:
        virtual void handleEvent(Event*, bool isWindowEvent);
        virtual JSDOMGlobalObject* globalObject() const = 0;
        virtual bool virtualIsInline() const;

        bool m_isInline;
    };

    class JSEventListener : public JSAbstractEventListener {
    public:
        static PassRefPtr<JSEventListener> create(JSC::JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
        {
            return adoptRef(new JSEventListener(listener, globalObject, isInline));
        }
        virtual ~JSEventListener();

        void clearGlobalObject() { m_globalObject = 0; }

        virtual JSC::JSObject* jsFunction() const;
        virtual void clearJSFunction();
        virtual void markJSFunction();

    private:
        JSEventListener(JSC::JSObject* function, JSDOMGlobalObject*, bool isInline);

        virtual JSDOMGlobalObject* globalObject() const;

        void clearJSFunctionInline();

        JSC::JSObject* m_jsFunction;
        JSDOMGlobalObject* m_globalObject;
    };

} // namespace WebCore

#endif // JSEventListener_h
