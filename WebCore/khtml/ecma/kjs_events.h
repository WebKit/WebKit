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

#ifndef KJS_EVENTS_H
#define KJS_EVENTS_H

#include "kjs_dom.h"
#include "kjs_html.h"

#include "dom/dom2_events.h"
#include <kjs/protect.h>

namespace DOM {
    class ClipboardImpl;
    class EventImpl;
    class KeyboardEventImpl;
    class MouseEventImpl;
    class MutationEventImpl;
    class UIEventImpl;
}

namespace KJS {

    class Window;
    class Clipboard;
    
    class JSAbstractEventListener : public DOM::EventListener {
    public:
        JSAbstractEventListener(bool HTML = false);
        virtual void handleEvent(DOM::EventListenerEvent, bool isWindowEvent);
        virtual DOM::DOMString eventListenerType();
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
        JSLazyEventListener(const DOM::DOMString& code, Window*, DOM::NodeImpl*, int lineno = 0);
        virtual JSObject *listenerObj() const;
    private:
        virtual JSValue *eventParameterName() const;
        void parseCode() const;

        mutable DOM::DOMString code;
        mutable bool parsed;
        int lineNumber;
        DOM::NodeImpl* originalNode;
    };

    JSValue* getNodeEventListener(DOM::NodeImpl* n, const DOM::AtomicString& eventType);

    // Constructor for Event - currently only used for some global vars
    class EventConstructor : public DOMObject {
    public:
        EventConstructor(ExecState*) { }
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        // no put - all read-only
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    };

    JSValue* getEventConstructor(ExecState*);

    class DOMEvent : public DOMObject {
    public:
        DOMEvent(ExecState*, DOM::EventImpl*);
        virtual ~DOMEvent();
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual void put(ExecState*, const Identifier&, JSValue*, int attr = None);
        void putValueProperty(ExecState*, int token, JSValue*, int);
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Type, Target, CurrentTarget, EventPhase, Bubbles,
               Cancelable, TimeStamp, StopPropagation, PreventDefault, InitEvent,
               // MS IE equivalents
               SrcElement, ReturnValue, CancelBubble, ClipboardData, DataTransfer };
        DOM::EventImpl *impl() const { return m_impl.get(); }
    protected:
        RefPtr<DOM::EventImpl> m_impl;
        mutable Clipboard* clipboard;
    };

    JSValue* getDOMEvent(ExecState*, DOM::EventImpl*);

    DOM::EventImpl* toEvent(JSValue*); // returns 0 if value is not a DOMEvent object

    KJS_DEFINE_PROTOTYPE(DOMEventProto)

    // Constructor object EventException
    class EventExceptionConstructor : public DOMObject {
    public:
        EventExceptionConstructor(ExecState*) { }
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        // no put - all read-only
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    };

    JSValue* getEventExceptionConstructor(ExecState*);

    class DOMUIEvent : public DOMEvent {
    public:
        DOMUIEvent(ExecState*, DOM::UIEventImpl*);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        // no put - all read-only
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { View, Detail, KeyCode, CharCode, LayerX, LayerY, PageX, PageY, Which, InitUIEvent };
    };

    class DOMMouseEvent : public DOMUIEvent {
    public:
        DOMMouseEvent(ExecState*, DOM::MouseEventImpl *me);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual void mark();
        // no put - all read-only
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { ScreenX, ScreenY, ClientX, X, ClientY, Y, OffsetX, OffsetY,
               CtrlKey, ShiftKey, AltKey,
               MetaKey, Button, RelatedTarget, FromElement, ToElement,
               InitMouseEvent };
    };

    class DOMKeyboardEvent : public DOMUIEvent {
    public:
        DOMKeyboardEvent(ExecState*, DOM::KeyboardEventImpl *ke);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        // no put - all read-only
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { KeyIdentifier, KeyLocation, CtrlKey, ShiftKey, AltKey, MetaKey, AltGraphKey, InitKeyboardEvent};
    };

    class Clipboard : public DOMObject {
    friend class ClipboardProtoFunc;
    public:
        Clipboard(ExecState*, DOM::ClipboardImpl *ds);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual void put(ExecState*, const Identifier&, JSValue*, int attr = None);
        void putValueProperty(ExecState*, int token, JSValue*, int attr);
        virtual bool toBoolean(ExecState*) const { return true; }
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { ClearData, GetData, SetData, Types, SetDragImage, DropEffect, EffectAllowed };
    private:
        RefPtr<DOM::ClipboardImpl> clipboard;
    };

} // namespace

#endif
