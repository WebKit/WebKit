// -*- c-basic-offset: 2 -*-
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

#ifndef _KJS_EVENTS_H_
#define _KJS_EVENTS_H_

#include "ecma/kjs_dom.h"
#include "ecma/kjs_html.h"
#include "dom/dom2_events.h"

namespace DOM {
    class ClipboardImpl;
    class WheelEventImpl;
}

namespace KJS {

  class Window;
  class Clipboard;
    
  class JSAbstractEventListener : public DOM::EventListener {
  public:
    JSAbstractEventListener(bool _html = false);
    virtual ~JSAbstractEventListener();
    virtual void handleEvent(DOM::Event &evt, bool isWindowEvent);
    virtual DOM::DOMString eventListenerType();
    virtual Object listenerObj() const = 0;
    virtual Object windowObj() const = 0;
    ObjectImp *listenerObjImp() const { return listenerObj().imp(); }
  protected:
    bool html;
  };

  class JSUnprotectedEventListener : public JSAbstractEventListener {
  public:
    JSUnprotectedEventListener(Object _listener, const Object &_win, bool _html = false);
    virtual ~JSUnprotectedEventListener();
    virtual Object listenerObj() const;
    virtual Object windowObj() const;
    void mark();
  protected:
    Object listener;
    Object win;
  };

  class JSEventListener : public JSAbstractEventListener {
  public:
    JSEventListener(Object _listener, const Object &_win, bool _html = false);
    virtual ~JSEventListener();
    virtual Object listenerObj() const;
    virtual Object windowObj() const;
  protected:
    mutable ProtectedObject listener;
    ProtectedObject win;
  };

  class JSLazyEventListener : public JSEventListener {
  public:
    JSLazyEventListener(QString _code, const Object &_win, DOM::NodeImpl *node, int lineno = 0);
    virtual void handleEvent(DOM::Event &evt, bool isWindowEvent);
    Object listenerObj() const;
    
  private:
    void parseCode() const;
    
    mutable QString code;
    mutable bool parsed;
    int lineNumber;
    DOM::NodeImpl *originalNode;
  };


  Value getNodeEventListener(DOM::Node n, int eventId);

  // Constructor for Event - currently only used for some global vars
  class EventConstructor : public DOMObject {
  public:
    EventConstructor(ExecState *) { }
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  Value getEventConstructor(ExecState *exec);

  class DOMEvent : public DOMObject {
  public:
    DOMEvent(ExecState *exec, DOM::Event e);
    ~DOMEvent();
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName,
			const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Type, Target, CurrentTarget, EventPhase, Bubbles,
           Cancelable, TimeStamp, StopPropagation, PreventDefault, InitEvent,
	   // MS IE equivalents
	   SrcElement, ReturnValue, CancelBubble, ClipboardData, DataTransfer };
    DOM::Event toEvent() const { return event; }
  protected:
    DOM::Event event;
    mutable Clipboard *clipboard;
  };

  Value getDOMEvent(ExecState *exec, DOM::Event e);

  /**
   * Convert an object to an Event. Returns a null Event if not possible.
   */
  DOM::Event toEvent(const Value&);

  // Constructor object EventException
  class EventExceptionConstructor : public DOMObject {
  public:
    EventExceptionConstructor(ExecState *) { }
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  Value getEventExceptionConstructor(ExecState *exec);

  class DOMUIEvent : public DOMEvent {
  public:
    DOMUIEvent(ExecState *exec, DOM::UIEvent ue) : DOMEvent(exec, ue) {}
    ~DOMUIEvent();
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { View, Detail, KeyCode, CharCode, LayerX, LayerY, PageX, PageY, Which, InitUIEvent };
    DOM::UIEvent toUIEvent() const { return static_cast<DOM::UIEvent>(event); }
  };

  class DOMMouseEvent : public DOMUIEvent {
  public:
    DOMMouseEvent(ExecState *exec, DOM::MouseEvent me) : DOMUIEvent(exec, me) {}
    ~DOMMouseEvent();
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    virtual void mark();
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { ScreenX, ScreenY, ClientX, X, ClientY, Y, OffsetX, OffsetY,
           CtrlKey, ShiftKey, AltKey,
           MetaKey, Button, RelatedTarget, FromElement, ToElement,
           InitMouseEvent };
    DOM::MouseEvent toMouseEvent() const { return static_cast<DOM::MouseEvent>(event); }
  };

  class DOMKeyboardEvent : public DOMUIEvent {
  public:
    DOMKeyboardEvent(ExecState *exec, DOM::KeyboardEvent ke) : DOMUIEvent(exec, ke) {}
    ~DOMKeyboardEvent();
    virtual Value tryGet(ExecState *exec, const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const;
    static const ClassInfo info;
    enum { KeyIdentifier, KeyLocation, CtrlKey, ShiftKey, AltKey, MetaKey, AltGraphKey, InitKeyboardEvent};
    DOM::KeyboardEvent toKeyboardEvent() const { return event; }
  };

  // Constructor object MutationEvent
  class MutationEventConstructor : public DOMObject {
  public:
    MutationEventConstructor(ExecState *) { }
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  Value getMutationEventConstructor(ExecState *exec);

  class DOMMutationEvent : public DOMEvent {
  public:
    DOMMutationEvent(ExecState *exec, DOM::MutationEvent me) : DOMEvent(exec, me) {}
    ~DOMMutationEvent();
    virtual Value tryGet(ExecState *exec,const Identifier &p) const;
    Value getValueProperty(ExecState *, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { AttrChange, RelatedNode, AttrName, PrevValue, NewValue,
           InitMutationEvent };
    DOM::MutationEvent toMutationEvent() const { return static_cast<DOM::MutationEvent>(event); }
  };
  
    class DOMWheelEvent : public DOMUIEvent {
    public:
        DOMWheelEvent(ExecState *, DOM::WheelEventImpl *);
        virtual Value tryGet(ExecState *, const Identifier &p) const;
        Value getValueProperty(ExecState *, int token) const;
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
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { ClearData, GetData, SetData, Types, SetDragImage, DropEffect, EffectAllowed };
  private:
    DOM::ClipboardImpl *clipboard;
  };
  
  
}; // namespace

#endif
