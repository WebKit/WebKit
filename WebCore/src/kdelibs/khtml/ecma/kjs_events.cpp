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
 *
 *  $Id$
 */

#include "kjs_window.h"
#include "kjs_events.h"
#include "kjs_views.h"
#include "kjs_proxy.h"
#include <dom_string.h>
#include <qptrdict.h>
#include <qlist.h>
#include "khtml_part.h"
#include <xml/dom_nodeimpl.h>
#include <kjs/kjs.h>

using namespace KJS;

QPtrDict<DOMEvent> events;

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(KJSO _listener, const KJSO &_win, bool _html)
{
    listener = _listener;
    html = _html;
    win = _win;
    static_cast<Window*>(win.imp())->jsEventListeners.append(this);
}

JSEventListener::~JSEventListener()
{
    static_cast<Window*>(win.imp())->jsEventListeners.removeRef(this);
}

void JSEventListener::handleEvent(DOM::Event &evt)
{
  if (listener.implementsCall() && static_cast<Window*>(win.imp())->part() ) {
    KJScript *scr = static_cast<Window*>(win.imp())->part()->jScript()->jScript();
    List args;
    args.append(getDOMEvent(evt));

    scr->init(); // set a valid current interpreter
    KJSO thisVal = getDOMNode(evt.currentTarget());
    List *scope = 0;
    if (thisVal.type() != NullType)
      scope = static_cast<DOMNode*>(thisVal.imp())->eventHandlerScope();
    Global::current().setExtra(static_cast<Window*>(win.imp())->part());
    scr->call(listener, thisVal, args, *scope);
    QVariant ret = KJSOToVariant(scr->returnValue());
    if (scope)
      delete scope;
    if (ret.type() == QVariant::Bool && ret.toBool() == false)
        evt.preventDefault();
  }
}

DOM::DOMString JSEventListener::eventListenerType()
{
    if (html)
	return "_khtml_HTMLEventListener";
    else
	return "_khtml_JSEventListener";
}

KJSO KJS::getNodeEventListener(DOM::Node n, int eventId)
{
    DOM::EventListener *listener = n.handle()->getHTMLEventListener(eventId);
    if (listener)
	return static_cast<JSEventListener*>(listener)->listenerObj();
    else
	return Null();
}

// -------------------------------------------------------------------------

const TypeInfo EventPrototype::info = { "EventPrototype", HostType, 0, 0, 0 };
// ### make this protype of Event objects?

KJSO EventPrototype::tryGet(const UString &p) const
{
  if (p == "CAPTURING_PHASE")
    return Number((unsigned int)DOM::Event::CAPTURING_PHASE);
  else if (p == "AT_TARGET")
    return Number((unsigned int)DOM::Event::AT_TARGET);
  else if (p == "BUBBLING_PHASE")
    return Number((unsigned int)DOM::Event::BUBBLING_PHASE);

  return DOMObject::tryGet(p);
}

KJSO KJS::getEventPrototype()
{
    KJSO proto = Global::current().get("[[event.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object eventProto( new EventPrototype );
        Global::current().put("[[event.prototype]]", eventProto);
        return eventProto;
    }
}

// -------------------------------------------------------------------------

const TypeInfo DOMEvent::info = { "Event", HostType, 0, 0, 0 };


DOMEvent::~DOMEvent()
{
  events.remove(event.handle());
}


KJSO DOMEvent::tryGet(const UString &p) const
{
  if (p == "type")
    return String(event.type());
  else if (p == "target")
    return getDOMNode(event.target());
  else if (p == "currentTarget")
    return getDOMNode(event.currentTarget());
  else if (p == "eventPhase")
    return Number((unsigned int)event.eventPhase());
  else if (p == "bubbles")
    return Boolean(event.bubbles());
  else if (p == "cancelable")
    return Boolean(event.cancelable());
  else if (p == "timeStamp")
    return Number((long unsigned int)event.timeStamp()); // ### long long ?
  else if (p == "stopPropagation")
    return new DOMEventFunc(event,DOMEventFunc::StopPropagation);
  else if (p == "preventDefault")
    return new DOMEventFunc(event,DOMEventFunc::PreventDefault);
  else if (p == "initEvent")
    return new DOMEventFunc(event,DOMEventFunc::InitEvent);
  else
    return DOMObject::tryGet(p);
}

Completion DOMEventFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
    case StopPropagation:
      event.stopPropagation();
      result = Undefined();
      break;
    case PreventDefault:
      event.preventDefault();
      result = Undefined();
      break;
    case InitEvent:
      event.initEvent(args[0].toString().value().string(),args[1].toBoolean().value(),args[2].toBoolean().value());
      result = Undefined();
      break;
  };

  return Completion(ReturnValue,result);
}

KJSO KJS::getDOMEvent(DOM::Event e)
{
  DOMEvent *ret;
  if (e.isNull())
    return Null();
  else if ((ret = events[e.handle()]))
    return ret;

  DOM::DOMString module = e.eventModuleName();
  if (module == "UIEvents")
    ret = new DOMUIEvent(static_cast<DOM::UIEvent>(e));
  else if (module == "MouseEvents")
    ret = new DOMMouseEvent(static_cast<DOM::MouseEvent>(e));
  else if (module == "MutationEvents")
    ret = new DOMMutationEvent(static_cast<DOM::MutationEvent>(e));
  else
    ret = new DOMEvent(e);

  events.insert(e.handle(),ret);
  return ret;
}

DOM::Event KJS::toEvent(const KJSO& obj)
{
  if (!obj.derivedFrom("Event"))
    return DOM::Event();

  const DOMEvent *dobj = static_cast<const DOMEvent*>(obj.imp());
  return dobj->toEvent();
}

// -------------------------------------------------------------------------


const TypeInfo EventExceptionPrototype::info = { "EventExceptionPrototype", HostType, 0, 0, 0 };
// ### make this protype of EventException objects?

KJSO EventExceptionPrototype::tryGet(const UString &p) const
{
  if (p == "UNSPECIFIED_EVENT_TYPE_ERR")
    return Number((unsigned int)DOM::EventException::UNSPECIFIED_EVENT_TYPE_ERR);

  return DOMObject::tryGet(p);
}

KJSO KJS::getEventExceptionPrototype()
{
    KJSO proto = Global::current().get("[[eventException.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object eventExceptionProto( new EventExceptionPrototype );
        Global::current().put("[[eventException.prototype]]", eventExceptionProto);
        return eventExceptionProto;
    }
}

// -------------------------------------------------------------------------

const TypeInfo DOMUIEvent::info = { "UIEvent", HostType, &DOMEvent::info, 0, 0 };


DOMUIEvent::~DOMUIEvent()
{
}


KJSO DOMUIEvent::tryGet(const UString &p) const
{
  if (p == "view")
    return getDOMAbstractView(static_cast<DOM::UIEvent>(event).view());
  else if (p == "detail")
    return Number(static_cast<DOM::UIEvent>(event).detail());
  else if (p == "initUIEvent")
    return new DOMUIEventFunc(static_cast<DOM::UIEvent>(event),DOMUIEventFunc::InitUIEvent);
  else
    return DOMEvent::tryGet(p);
}

Completion DOMUIEventFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
    case InitUIEvent: {
      DOM::AbstractView v = toAbstractView(args[3]);
      static_cast<DOM::UIEvent>(uiEvent).initUIEvent(args[0].toString().value().string(),
                                                     args[1].toBoolean().value(),
                                                     args[2].toBoolean().value(),
                                                     v,
                                                     args[4].toNumber().intValue());
      }
      result = Undefined();
      break;
  };

  return Completion(ReturnValue,result);
}

// -------------------------------------------------------------------------

const TypeInfo DOMMouseEvent::info = { "MouseEvent", HostType, &DOMUIEvent::info, 0, 0 };


DOMMouseEvent::~DOMMouseEvent()
{
}


KJSO DOMMouseEvent::tryGet(const UString &p) const
{

  if (p == "screenX")
    return Number(static_cast<DOM::MouseEvent>(event).screenX());
  else if (p == "screenY")
    return Number(static_cast<DOM::MouseEvent>(event).screenY());
  else if (p == "clientX")
    return Number(static_cast<DOM::MouseEvent>(event).clientX());
  else if (p == "clientY")
    return Number(static_cast<DOM::MouseEvent>(event).clientY());
  else if (p == "ctrlKey")
    return Boolean(static_cast<DOM::MouseEvent>(event).ctrlKey());
  else if (p == "shiftKey")
    return Boolean(static_cast<DOM::MouseEvent>(event).shiftKey());
  else if (p == "altKey")
    return Boolean(static_cast<DOM::MouseEvent>(event).altKey());
  else if (p == "metaKey")
    return Boolean(static_cast<DOM::MouseEvent>(event).metaKey());
  else if (p == "button")
    return Number((unsigned int)static_cast<DOM::MouseEvent>(event).button());
  else if (p == "relatedTarget")
    return getDOMNode(static_cast<DOM::MouseEvent>(event).relatedTarget());
  else if (p == "initMouseEvent")
    return new DOMMouseEventFunc(static_cast<DOM::MouseEvent>(event),DOMMouseEventFunc::InitMouseEvent);
  else
    return DOMUIEvent::tryGet(p);
}

Completion DOMMouseEventFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
    case InitMouseEvent:
      mouseEvent.initMouseEvent(args[0].toString().value().string(), // typeArg
                                args[1].toBoolean().value(), // canBubbleArg
                                args[2].toBoolean().value(), // cancelableArg
                                toAbstractView(args[3]), // viewArg
                                args[4].toNumber().intValue(), // detailArg
                                args[5].toNumber().intValue(), // screenXArg
                                args[6].toNumber().intValue(), // screenYArg
                                args[7].toNumber().intValue(), // clientXArg
                                args[8].toNumber().intValue(), // clientYArg
                                args[9].toBoolean().value(), // ctrlKeyArg
                                args[10].toBoolean().value(), // altKeyArg
                                args[11].toBoolean().value(), // shiftKeyArg
                                args[12].toBoolean().value(), // metaKeyArg
                                args[13].toNumber().intValue(), // buttonArg
                                toNode(args[14])); // relatedTargetArg
      result = Undefined();
      break;
  };

  return Completion(ReturnValue,result);
}

// -------------------------------------------------------------------------

const TypeInfo MutationEventPrototype::info = { "MutationEventPrototype", HostType, 0, 0, 0 };
// ### make this protype of MutationEvent objects?
// ### should the prototype of this be EventPrototype?

KJSO MutationEventPrototype::tryGet(const UString &p) const
{
  if (p == "MODIFICATION")
    return Number((unsigned int)DOM::MutationEvent::MODIFICATION);
  else if (p == "ADDITION")
    return Number((unsigned int)DOM::MutationEvent::ADDITION);
  else if (p == "REMOVAL")
    return Number((unsigned int)DOM::MutationEvent::REMOVAL);

  return DOMObject::tryGet(p);
}

KJSO KJS::getMutationEventPrototype()
{
    KJSO proto = Global::current().get("[[mutationEvent.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object mutationEventProto( new MutationEventPrototype );
        Global::current().put("[[mutationEvent.prototype]]", mutationEventProto);
        return mutationEventProto;
    }
}

// -------------------------------------------------------------------------

const TypeInfo DOMMutationEvent::info = { "MutationEvent", HostType, &DOMEvent::info, 0, 0 };


DOMMutationEvent::~DOMMutationEvent()
{
}


KJSO DOMMutationEvent::tryGet(const UString &p) const
{
  if (p == "relatedNode")
    return getDOMNode(static_cast<DOM::MutationEvent>(event).relatedNode());
  else if (p == "prevValue")
    return String(static_cast<DOM::MutationEvent>(event).prevValue());
  else if (p == "newValue")
    return String(static_cast<DOM::MutationEvent>(event).newValue());
  else if (p == "attrName")
    return String(static_cast<DOM::MutationEvent>(event).attrName());
  else if (p == "attrChange")
    return Number((unsigned int)static_cast<DOM::MutationEvent>(event).attrChange());
  else if (p == "initMutationEvent")
    return new DOMMutationEventFunc(static_cast<DOM::MutationEvent>(event),DOMMutationEventFunc::InitMutationEvent);
  else
    return DOMEvent::tryGet(p);
}

Completion DOMMutationEventFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
    case InitMutationEvent:
      mutationEvent.initMutationEvent(args[0].toString().value().string(), // typeArg,
                                      args[1].toBoolean().value(), // canBubbleArg
                                      args[2].toBoolean().value(), // cancelableArg
                                      toNode(args[3]), // relatedNodeArg
                                      args[4].toString().value().string(), // prevValueArg
                                      args[5].toString().value().string(), // newValueArg
                                      args[6].toString().value().string(), // attrNameArg
                                      args[7].toNumber().intValue()); // attrChangeArg
      result = Undefined();
      break;
  };

  return Completion(ReturnValue,result);
}

