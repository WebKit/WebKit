/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#include "kjs_dom.h"
#include <dom2_events.h>
#include <dom_misc.h>

namespace KJS {

  class Window;

  class JSEventListener : public DOM::EventListener {
  public:
    JSEventListener(KJSO _listener, const KJSO &_win, bool _html = false);
    virtual ~JSEventListener();
    virtual void handleEvent(DOM::Event &evt);
    virtual DOM::DOMString eventListenerType();
    KJSO listenerObj() { return listener; }
  protected:
    KJSO listener;
    bool html;
    KJSO win;
  };

  KJSO getNodeEventListener(DOM::Node n, int eventId);

  // Prototype object Event
  class EventPrototype : public DOMObject {
  public:
    EventPrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getEventPrototype();

  class DOMEvent : public DOMObject {
  public:
    DOMEvent(DOM::Event e) : event(e) {}
    ~DOMEvent();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    virtual DOM::Event toEvent() const { return event; }
  protected:
    DOM::Event event;
  };

  class DOMEventFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMEventFunc(DOM::Event e, int i) : event(e), id(i) { }
    Completion tryExecute(const List &);
    enum { StopPropagation, PreventDefault, InitEvent };
  private:
    DOM::Event event;
    int id;
  };

  KJSO getDOMEvent(DOM::Event e);

  /**
   * Convert an object to an Event. Returns a null Node if not possible.
   */
  DOM::Event toEvent(const KJSO&);

  // Prototype object EventException
  class EventExceptionPrototype : public DOMObject {
  public:
    EventExceptionPrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getEventExceptionPrototype();

  class DOMUIEvent : public DOMEvent {
  public:
    DOMUIEvent(DOM::UIEvent ue) : DOMEvent(ue) {}
    ~DOMUIEvent();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMUIEventFunc : public DOMFunction {
  public:
    DOMUIEventFunc(DOM::UIEvent ue, int i) : uiEvent(ue), id(i) { }
    Completion tryExecute(const List &);
    enum { InitUIEvent };
  private:
    DOM::UIEvent uiEvent;
    int id;
  };

  class DOMMouseEvent : public DOMUIEvent {
  public:
    DOMMouseEvent(DOM::MouseEvent me) : DOMUIEvent(me) {}
    ~DOMMouseEvent();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMMouseEventFunc : public DOMFunction {
  public:
    DOMMouseEventFunc(DOM::MouseEvent me, int i) : mouseEvent(me), id(i) { }
    Completion tryExecute(const List &);
    enum { InitMouseEvent };
  private:
    DOM::MouseEvent mouseEvent;
    int id;
  };

  // Prototype object MutationEvent
  class MutationEventPrototype : public DOMObject {
  public:
    MutationEventPrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getMutationEventPrototype();

  class DOMMutationEvent : public DOMEvent {
  public:
    DOMMutationEvent(DOM::MutationEvent me) : DOMEvent(me) {}
    ~DOMMutationEvent();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMMutationEventFunc : public DOMFunction {
  public:
    DOMMutationEventFunc(DOM::MutationEvent me, int i) : mutationEvent(me), id(i) { }
    Completion tryExecute(const List &);
    enum { InitMutationEvent };
  private:
    DOM::MutationEvent mutationEvent;
    int id;
  };

}; // namespace

#endif
