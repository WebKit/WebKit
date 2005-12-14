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
    class WheelEventImpl;
}

namespace KJS {

  class Window;
  class Clipboard;
    
  class JSAbstractEventListener : public DOM::EventListener {
  public:
    JSAbstractEventListener(bool _html = false);
    virtual ~JSAbstractEventListener();
    virtual void handleEvent(DOM::EventListenerEvent evt, bool isWindowEvent);
    virtual DOM::DOMString eventListenerType();
    virtual JSObject *listenerObj() const = 0;
    virtual JSObject *windowObj() const = 0;
    JSObject *listenerObjImp() const { return listenerObj(); }
  protected:
    bool html;
  };

  class JSUnprotectedEventListener : public JSAbstractEventListener {
  public:
    JSUnprotectedEventListener(JSObject *_listener, JSObject *_win, bool _html = false);
    virtual ~JSUnprotectedEventListener();
    virtual JSObject *listenerObj() const;
    virtual JSObject *windowObj() const;
    void clearWindowObj();
    void mark();
  protected:
    JSObject *listener;
    JSObject *win;
  };

  class JSEventListener : public JSAbstractEventListener {
  public:
    JSEventListener(JSObject *_listener, JSObject *_win, bool _html = false);
    virtual ~JSEventListener();
    virtual JSObject *listenerObj() const;
    virtual JSObject *windowObj() const;
    void clearWindowObj();
  protected:
    mutable ProtectedPtr<JSObject> listener;
    ProtectedPtr<JSObject> win;
  };

  class JSLazyEventListener : public JSEventListener {
  public:
    JSLazyEventListener(QString _code, JSObject *_win, DOM::NodeImpl *node, int lineno = 0);
    virtual void handleEvent(DOM::EventListenerEvent evt, bool isWindowEvent);
    JSObject *listenerObj() const;
    
  protected:
    virtual JSValue *eventParameterName() const;
    
  private:
    void parseCode() const;
    
    mutable QString code;
    mutable bool parsed;
    int lineNumber;
    DOM::NodeImpl *originalNode;
  };

  JSValue *getNodeEventListener(DOM::NodeImpl *n, const DOM::AtomicString &eventType);

  // Constructor for Event - currently only used for some global vars
  class EventConstructor : public DOMObject {
  public:
    EventConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  JSValue *getEventConstructor(ExecState *exec);

  class DOMEvent : public DOMObject {
  public:
    DOMEvent(ExecState *exec, DOM::EventImpl *e);
    ~DOMEvent();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName,
			JSValue *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, JSValue *value, int);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Type, Target, CurrentTarget, EventPhase, Bubbles,
           Cancelable, TimeStamp, StopPropagation, PreventDefault, InitEvent,
	   // MS IE equivalents
	   SrcElement, ReturnValue, CancelBubble, ClipboardData, DataTransfer };
    DOM::EventImpl *impl() const { return m_impl.get(); }
  protected:
    RefPtr<DOM::EventImpl> m_impl;
    mutable Clipboard *clipboard;
  };

  JSValue *getDOMEvent(ExecState *exec, DOM::EventImpl *e);

  DOM::EventImpl *toEvent(JSValue *); // returns 0 if value is not a DOMEvent object

  // Constructor object EventException
  class EventExceptionConstructor : public DOMObject {
  public:
    EventExceptionConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  JSValue *getEventExceptionConstructor(ExecState *exec);

  class DOMUIEvent : public DOMEvent {
  public:
    DOMUIEvent(ExecState *exec, DOM::UIEventImpl *ue);
    ~DOMUIEvent();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { View, Detail, KeyCode, CharCode, LayerX, LayerY, PageX, PageY, Which, InitUIEvent };
  };

  class DOMMouseEvent : public DOMUIEvent {
  public:
    DOMMouseEvent(ExecState *exec, DOM::MouseEventImpl *me);
    ~DOMMouseEvent();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
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
    DOMKeyboardEvent(ExecState *exec, DOM::KeyboardEventImpl *ke);
    ~DOMKeyboardEvent();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const;
    static const ClassInfo info;
    enum { KeyIdentifier, KeyLocation, CtrlKey, ShiftKey, AltKey, MetaKey, AltGraphKey, InitKeyboardEvent};
  };

  // Constructor object MutationEvent
  class MutationEventConstructor : public DOMObject {
  public:
    MutationEventConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  JSValue *getMutationEventConstructor(ExecState *exec);

  class DOMMutationEvent : public DOMEvent {
  public:
    DOMMutationEvent(ExecState *exec, DOM::MutationEventImpl *me);
    ~DOMMutationEvent();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { AttrChange, RelatedNode, AttrName, PrevValue, NewValue,
           InitMutationEvent };
  };
  
    class DOMWheelEvent : public DOMUIEvent {
    public:
        DOMWheelEvent(ExecState *, DOM::WheelEventImpl *);
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        JSValue *getValueProperty(ExecState *, int token) const;
        // no put - all read-only
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { ScreenX, ScreenY, ClientX, X, ClientY, Y, OffsetX, OffsetY,
           CtrlKey, ShiftKey, AltKey, MetaKey, WheelDelta };
    };

  class Clipboard : public DOMObject {
  friend class ClipboardProtoFunc;
  public:
    Clipboard(ExecState *exec, DOM::ClipboardImpl *ds);
    ~Clipboard();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, JSValue *value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { ClearData, GetData, SetData, Types, SetDragImage, DropEffect, EffectAllowed };
  private:
    DOM::ClipboardImpl *clipboard;
  };

} // namespace

#endif
