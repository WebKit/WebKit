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
#include "khtml_part.h"
#include "kjs_window.h"
#include "kjs_events.h"
#include "kjs_events.lut.h"
#include "kjs_views.h"
#include "kjs_proxy.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "rendering/render_object.h"
#include "misc/loader.h"

#include <kdebug.h>

using DOM::ClipboardEventImpl;
using DOM::DocumentImpl;
using DOM::EventImpl;
using DOM::EventListenerEvent;
using DOM::KeyboardEventImpl;
using DOM::MouseEventImpl;
using DOM::UIEventImpl;
using DOM::MutationEventImpl;
using DOM::MouseRelatedEventImpl;
using DOM::NodeImpl;
using DOM::WheelEventImpl;

using khtml::RenderObject;

namespace KJS {

// -------------------------------------------------------------------------

JSAbstractEventListener::JSAbstractEventListener(bool _html)
  : html(_html)
{
}

JSAbstractEventListener::~JSAbstractEventListener()
{
}

void JSAbstractEventListener::handleEvent(EventListenerEvent ele, bool isWindowEvent)
{
#ifdef KJS_DEBUGGER
  if (KJSDebugWin::instance() && KJSDebugWin::instance()->inSession())
    return;
#endif

#if KHTML_NO_CPLUSPLUS_DOM
  EventImpl *evt = ele;
#else
  EventImpl *evt = ele.handle();
#endif

  ObjectImp *listener = listenerObj();
  ObjectImp *win = windowObj();

  KHTMLPart *part = static_cast<Window*>(win)->part();
  KJSProxy *proxy = 0;
  if (part)
      proxy = KJSProxy::proxy( part );

  ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(proxy->interpreter());
  ExecState *exec = interpreter->globalExec();
  
  bool hasHandleEvent = false;
  ValueImp *handleEventFuncValue = 0;
  ObjectImp *handleEventFunc = 0;
  
  handleEventFuncValue = listener->get(exec, "handleEvent");
  if (handleEventFuncValue->isObject()) {      
      handleEventFunc = static_cast<ObjectImp *>(handleEventFuncValue);
            
      if (handleEventFunc->implementsCall())
          hasHandleEvent = true;
  }
  
  if (proxy && (listener->implementsCall() || hasHandleEvent)) {
    ref();

    List args;
    args.append(getDOMEvent(exec,evt));

    Window *window = static_cast<Window*>(win);
    // Set the event we're handling in the Window object
    window->setCurrentEvent(evt);
    // ... and in the interpreter
    interpreter->setCurrentEvent(evt);

    ObjectImp *thisObj;
    if (isWindowEvent) {
        thisObj = win;
    } else {
        Interpreter::lock();
        thisObj = static_cast<ObjectImp *>(getDOMNode(exec, evt->currentTarget()));
        Interpreter::unlock();
    }

    Interpreter::lock();
    ValueImp *retval;
    if (hasHandleEvent)
        retval = handleEventFunc->call(exec, listener, args);
    else
        retval = listener->call(exec, thisObj, args);
    Interpreter::unlock();

    window->setCurrentEvent( 0 );
    interpreter->setCurrentEvent( 0 );
#if APPLE_CHANGES
    if ( exec->hadException() ) {
        Interpreter::lock();
        char *message = exec->exception()->toObject(exec)->get(exec, messagePropertyName)->toString(exec).ascii();
        int lineNumber =  exec->exception()->toObject(exec)->get(exec, "line")->toInt32(exec);
        QString sourceURL;
        {
          // put this in a block to make sure UString is deallocated inside the lock
          UString uSourceURL = exec->exception()->toObject(exec)->get(exec, "sourceURL")->toString(exec);
          sourceURL = uSourceURL.qstring();
        }
        Interpreter::unlock();
        if (Interpreter::shouldPrintExceptions()) {
	    printf("(event handler):%s\n", message);
	}
        KWQ(part)->addMessageToConsole(message, lineNumber, sourceURL);
        exec->clearException();
    }
#else
    if ( exec->hadException() )
        exec->clearException();
#endif

    else if (html)
    {
        QVariant ret = ValueToVariant(exec, retval);
        if (ret.type() == QVariant::Bool && ret.toBool() == false)
            evt->preventDefault();
    }
    DOM::DocumentImpl::updateDocumentsRendering();
    deref();
  }
}

DOM::DOMString JSAbstractEventListener::eventListenerType()
{
    if (html)
	return "_khtml_HTMLEventListener";
    else
	return "_khtml_JSEventListener";
}

// -------------------------------------------------------------------------

JSUnprotectedEventListener::JSUnprotectedEventListener(ObjectImp *_listener, ObjectImp *_win, bool _html)
  : JSAbstractEventListener(_html)
  , listener(_listener)
  , win(_win)
{
    if (_listener) {
      static_cast<Window*>(win)->jsUnprotectedEventListeners.insert(_listener, this);
    }
}

JSUnprotectedEventListener::~JSUnprotectedEventListener()
{
    if (listener) {
      static_cast<Window*>(win)->jsUnprotectedEventListeners.remove(listener);
    }
}

ObjectImp *JSUnprotectedEventListener::listenerObj() const
{ 
    return listener; 
}

ObjectImp *JSUnprotectedEventListener::windowObj() const
{
    return win;
}

void JSUnprotectedEventListener::mark()
{
  ObjectImp *listenerImp = listener;
  if (listenerImp && !listenerImp->marked())
    listenerImp->mark();
}

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(ObjectImp *_listener, ObjectImp *_win, bool _html)
  : JSAbstractEventListener(_html)
  , listener(_listener)
  , win(_win)
{
    if (_listener)
      static_cast<Window*>(_win)->jsEventListeners.insert(_listener, this);
}

JSEventListener::~JSEventListener()
{
    if (ObjectImp *l = listener) {
        ObjectImp *w = win;
        static_cast<Window *>(w)->jsEventListeners.remove(l);
    }
}

ObjectImp *JSEventListener::listenerObj() const
{ 
    return listener; 
}

ObjectImp *JSEventListener::windowObj() const
{
    return win;
}

// -------------------------------------------------------------------------

JSLazyEventListener::JSLazyEventListener(QString _code, ObjectImp *_win, NodeImpl *_originalNode, int lineno)
  : JSEventListener(NULL, _win, true),
    code(_code),
    parsed(false)
{
    lineNumber = lineno;

    // We don't retain the original node, because we assume it
    // will stay alive as long as this handler object is around
    // and we need to avoid a reference cycle. If JS transfers
    // this handler to another node, parseCode will be called and
    // then originalNode is no longer needed.
    
    originalNode = _originalNode;
}

void JSLazyEventListener::handleEvent(EventListenerEvent evt, bool isWindowEvent)
{
    parseCode();
    ObjectImp *listenerObj = listener;
    if (listenerObj)
        JSEventListener::handleEvent(evt, isWindowEvent);
}


ObjectImp *JSLazyEventListener::listenerObj() const
{
  parseCode();
  return listener;
}

void JSLazyEventListener::parseCode() const
{
  if (!parsed) {
    ObjectImp *w = win;
    KHTMLPart *part = static_cast<Window *>(w)->part();
    KJSProxy *proxy = 0L;
    if (part)
      proxy = KJSProxy::proxy( part );

    if (proxy) {
      ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(proxy->interpreter());
      ExecState *exec = interpreter->globalExec();

      Interpreter::lock();
      //Constructor constr(Global::current().get("Function"));
      ObjectImp *constr = interpreter->builtinFunction();
      List args;

      static ProtectedValue eventString = String("event");
      UString sourceURL(part->m_url.url());
      args.append(eventString);
      args.append(String(code));
      listener = constr->construct(exec, args, sourceURL, lineNumber); // ### is globalExec ok ?

      Interpreter::unlock();

      if (exec->hadException()) {
	exec->clearException();

	// failed to parse, so let's just make this listener a no-op
	listener = NULL;
      } else if (originalNode) {
        // Add the event's home element to the scope
        // (and the document, and the form - see HTMLElement::eventHandlerScope)
        ScopeChain scope = listener->scope();
        
        Interpreter::lock();
        ObjectImp *thisObj = static_cast<ObjectImp *>(getDOMNode(exec, originalNode));
        Interpreter::unlock();
        
        if (thisObj) {
          Interpreter::lock();
          static_cast<DOMNode*>(thisObj)->pushEventHandlerScope(exec, scope);
          Interpreter::unlock();
          
          listener->setScope(scope);
        }
      }
    }

    // no more need to keep the unparsed code around
    code = QString();
    
    if (ObjectImp *l = listener) {
        ObjectImp *w = win;
        static_cast<Window *>(w)->jsEventListeners.insert(l, const_cast<JSLazyEventListener *>(this));
    }
    
    parsed = true;
  }
}

ValueImp *getNodeEventListener(NodeImpl *n, int eventId)
{
  JSAbstractEventListener *listener = static_cast<JSAbstractEventListener *>(n->getHTMLEventListener(eventId));
  if (listener)
    if (ValueImp *obj = listener->listenerObjImp())
      return obj;
  return jsNull();
}

// -------------------------------------------------------------------------

const ClassInfo EventConstructor::info = { "EventConstructor", 0, &EventConstructorTable, 0 };
/*
@begin EventConstructorTable 3
  CAPTURING_PHASE	DOM::Event::CAPTURING_PHASE	DontDelete|ReadOnly
  AT_TARGET		DOM::Event::AT_TARGET		DontDelete|ReadOnly
  BUBBLING_PHASE	DOM::Event::BUBBLING_PHASE	DontDelete|ReadOnly
# Reverse-engineered from Netscape
  MOUSEDOWN		1				DontDelete|ReadOnly
  MOUSEUP		2				DontDelete|ReadOnly
  MOUSEOVER		4				DontDelete|ReadOnly
  MOUSEOUT		8				DontDelete|ReadOnly
  MOUSEMOVE		16				DontDelete|ReadOnly
  MOUSEDRAG		32				DontDelete|ReadOnly
  CLICK			64				DontDelete|ReadOnly
  DBLCLICK		128				DontDelete|ReadOnly
  KEYDOWN		256				DontDelete|ReadOnly
  KEYUP			512				DontDelete|ReadOnly
  KEYPRESS		1024				DontDelete|ReadOnly
  DRAGDROP		2048				DontDelete|ReadOnly
  FOCUS			4096				DontDelete|ReadOnly
  BLUR			8192				DontDelete|ReadOnly
  SELECT		16384				DontDelete|ReadOnly
  CHANGE		32768				DontDelete|ReadOnly
@end
*/

bool EventConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<EventConstructor, DOMObject>(exec, &EventConstructorTable, this, propertyName, slot);
}

ValueImp *EventConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number(token);
}

ValueImp *getEventConstructor(ExecState *exec)
{
  return cacheGlobalObject<EventConstructor>(exec, "[[event.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMEvent::info = { "Event", 0, &DOMEventTable, 0 };
/*
@begin DOMEventTable 12
  type		DOMEvent::Type		DontDelete|ReadOnly
  target	DOMEvent::Target	DontDelete|ReadOnly
  currentTarget	DOMEvent::CurrentTarget	DontDelete|ReadOnly
  srcElement	DOMEvent::SrcElement	DontDelete|ReadOnly
  eventPhase	DOMEvent::EventPhase	DontDelete|ReadOnly
  bubbles	DOMEvent::Bubbles	DontDelete|ReadOnly
  cancelable	DOMEvent::Cancelable	DontDelete|ReadOnly
  timeStamp	DOMEvent::TimeStamp	DontDelete|ReadOnly
  returnValue   DOMEvent::ReturnValue   DontDelete
  cancelBubble  DOMEvent::CancelBubble  DontDelete
  dataTransfer	DOMEvent::DataTransfer  DontDelete|ReadOnly
  clipboardData  DOMEvent::ClipboardData  DontDelete|ReadOnly
@end
@begin DOMEventProtoTable 3
  stopPropagation 	DOMEvent::StopPropagation	DontDelete|Function 0
  preventDefault 	DOMEvent::PreventDefault	DontDelete|Function 0
  initEvent		DOMEvent::InitEvent		DontDelete|Function 3
@end
*/
DEFINE_PROTOTYPE("DOMEvent", DOMEventProto)
IMPLEMENT_PROTOFUNC(DOMEventProtoFunc)
IMPLEMENT_PROTOTYPE(DOMEventProto, DOMEventProtoFunc)

DOMEvent::DOMEvent(ExecState *exec, EventImpl *e)
  : m_impl(e), clipboard(0) 
{
  setPrototype(DOMEventProto::self(exec));
}

DOMEvent::~DOMEvent()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

// pass marks through to JS objects we hold during garbage collection
void DOMMouseEvent::mark()
{
    ObjectImp::mark();
    if (clipboard && !clipboard->marked())
        clipboard->mark();
}

bool DOMEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMEvent, DOMObject>(exec, &DOMEventTable, this, propertyName, slot);
}

ValueImp *DOMEvent::getValueProperty(ExecState *exec, int token) const
{
  EventImpl &event = *m_impl;
  switch (token) {
  case Type:
    return String(event.type());
  case Target:
  case SrcElement: /*MSIE extension - "the object that fired the event"*/
    return getDOMNode(exec, event.target());
  case CurrentTarget:
    return getDOMNode(exec, event.currentTarget());
  case EventPhase:
    return Number((unsigned int)event.eventPhase());
  case Bubbles:
    return Boolean(event.bubbles());
  case CancelBubble:
    return Boolean(event.getCancelBubble());
  case ReturnValue:
    return Boolean(!event.defaultPrevented());
  case Cancelable:
    return Boolean(event.cancelable());
  case TimeStamp:
    return Number((long unsigned int)event.timeStamp()); // ### long long ?
  case ClipboardData:
  {
    if (event.isClipboardEvent()) {
      ClipboardEventImpl *impl = static_cast<ClipboardEventImpl *>(&event);
      if (!clipboard) {
        clipboard = new Clipboard(exec, impl->clipboard());
      }
      return clipboard;
    } else {
      return Undefined();
    }
  }
  case DataTransfer:
  {
    if (event.isDragEvent()) {
      MouseEventImpl *impl = static_cast<MouseEventImpl *>(&event);
      if (!clipboard) {
        clipboard = new Clipboard(exec, impl->clipboard());
      }
      return clipboard;
    } else {
      return Undefined();
    }
  }
  default:
    kdWarning() << "Unhandled token in DOMEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

void DOMEvent::put(ExecState *exec, const Identifier &propertyName,
                      ValueImp *value, int attr)
{
  lookupPut<DOMEvent, DOMObject>(exec, propertyName, value, attr,
                                          &DOMEventTable, this);
}

void DOMEvent::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
  EventImpl &event = *m_impl;
  switch (token) {
  case ReturnValue:
    event.setDefaultPrevented(!value->toBoolean(exec));
    break;
  case CancelBubble:
    event.setCancelBubble(value->toBoolean(exec));
    break;
  default:
    break;
  }
}

ValueImp *DOMEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp * thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMEvent::info))
    return throwError(exec, TypeError);
  EventImpl &event = *static_cast<DOMEvent *>( thisObj )->impl();
  switch (id) {
    case DOMEvent::StopPropagation:
      event.stopPropagation();
    case DOMEvent::PreventDefault:
      event.preventDefault();
      return Undefined();
    case DOMEvent::InitEvent:
      event.initEvent(args[0]->toString(exec).domString(),args[1]->toBoolean(exec),args[2]->toBoolean(exec));
      return Undefined();
  };
  return Undefined();
}

ValueImp *getDOMEvent(ExecState *exec, EventImpl *e)
{
  if (!e)
    return Null();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());

  Interpreter::lock();

  DOMObject *ret = interp->getDOMObject(e);
  if (!ret) {
    if (e->isKeyboardEvent())
      ret = new DOMKeyboardEvent(exec, static_cast<KeyboardEventImpl *>(e));
    else if (e->isMouseEvent())
      ret = new DOMMouseEvent(exec, static_cast<MouseEventImpl *>(e));
    else if (e->isWheelEvent())
      ret = new DOMWheelEvent(exec, static_cast<WheelEventImpl *>(e));
    else if (e->isUIEvent())
      ret = new DOMUIEvent(exec, static_cast<UIEventImpl *>(e));
    else if (e->isMutationEvent())
      ret = new DOMMutationEvent(exec, static_cast<MutationEventImpl *>(e));
    else
      ret = new DOMEvent(exec, e);

    interp->putDOMObject(e, ret);
  }

  Interpreter::unlock();

  return ret;
}

EventImpl *toEvent(ValueImp *val)
{
    if (!val || !val->isObject(&DOMEvent::info))
        return 0;
    return static_cast<DOMEvent *>(val)->impl();
}

// -------------------------------------------------------------------------


const ClassInfo EventExceptionConstructor::info = { "EventExceptionConstructor", 0, &EventExceptionConstructorTable, 0 };
/*
@begin EventExceptionConstructorTable 1
  UNSPECIFIED_EVENT_TYPE_ERR    DOM::EventException::UNSPECIFIED_EVENT_TYPE_ERR DontDelete|ReadOnly
@end
*/
bool EventExceptionConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<EventExceptionConstructor, DOMObject>(exec, &EventExceptionConstructorTable, this, propertyName, slot);
}

ValueImp *EventExceptionConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number(token);
}

ValueImp *getEventExceptionConstructor(ExecState *exec)
{
  return cacheGlobalObject<EventExceptionConstructor>(exec, "[[eventException.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMUIEvent::info = { "UIEvent", &DOMEvent::info, &DOMUIEventTable, 0 };
/*
@begin DOMUIEventTable 8
  view		DOMUIEvent::View	DontDelete|ReadOnly
  detail	DOMUIEvent::Detail	DontDelete|ReadOnly
  keyCode	DOMUIEvent::KeyCode	DontDelete|ReadOnly
  charCode	DOMUIEvent::CharCode	DontDelete|ReadOnly
  layerX	DOMUIEvent::LayerX	DontDelete|ReadOnly
  layerY	DOMUIEvent::LayerY	DontDelete|ReadOnly
  pageX		DOMUIEvent::PageX	DontDelete|ReadOnly
  pageY		DOMUIEvent::PageY	DontDelete|ReadOnly
  which		DOMUIEvent::Which	DontDelete|ReadOnly
@end
@begin DOMUIEventProtoTable 1
  initUIEvent	DOMUIEvent::InitUIEvent	DontDelete|Function 5
@end
*/
DEFINE_PROTOTYPE("DOMUIEvent",DOMUIEventProto)
IMPLEMENT_PROTOFUNC(DOMUIEventProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMUIEventProto,DOMUIEventProtoFunc,DOMEventProto)

DOMUIEvent::DOMUIEvent(ExecState *exec, UIEventImpl *e)
  : DOMEvent(exec, e)
{
  setPrototype(DOMUIEventProto::self(exec));
}

DOMUIEvent::~DOMUIEvent()
{
}

bool DOMUIEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMUIEvent, DOMEvent>(exec, &DOMUIEventTable, this, propertyName, slot);
}

ValueImp *DOMUIEvent::getValueProperty(ExecState *exec, int token) const
{
  UIEventImpl &event = *static_cast<UIEventImpl *>(impl());
  switch (token) {
  case View:
    return getDOMAbstractView(exec, event.view());
  case Detail:
    return Number(event.detail());
  case KeyCode:
    return Number(event.keyCode());
  case CharCode:
    return Number(event.charCode());
  case LayerX:
    return Number(event.layerX());
  case LayerY:
    return Number(event.layerY());
  case PageX:
    return Number(event.pageX());
  case PageY:
    return Number(event.pageY());
  case Which:
    return Number(event.which());
  default:
    kdWarning() << "Unhandled token in DOMUIEvent::getValueProperty : " << token << endl;
    return Undefined();
  }
}

ValueImp *DOMUIEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMUIEvent::info))
    return throwError(exec, TypeError);
  UIEventImpl &uiEvent = *static_cast<UIEventImpl *>(static_cast<DOMUIEvent *>(thisObj)->impl());
  switch (id) {
    case DOMUIEvent::InitUIEvent:
      uiEvent.initUIEvent(args[0]->toString(exec).domString(),
                          args[1]->toBoolean(exec),
                          args[2]->toBoolean(exec),
                          toAbstractView(args[3]),
                          args[4]->toInt32(exec));
      return Undefined();
  }
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMMouseEvent::info = { "MouseEvent", &DOMUIEvent::info, &DOMMouseEventTable, 0 };

/*
@begin DOMMouseEventTable 16
  screenX	DOMMouseEvent::ScreenX	DontDelete|ReadOnly
  screenY	DOMMouseEvent::ScreenY	DontDelete|ReadOnly
  clientX	DOMMouseEvent::ClientX	DontDelete|ReadOnly
  x		DOMMouseEvent::X	DontDelete|ReadOnly
  clientY	DOMMouseEvent::ClientY	DontDelete|ReadOnly
  y		DOMMouseEvent::Y	DontDelete|ReadOnly
  offsetX	DOMMouseEvent::OffsetX	DontDelete|ReadOnly
  offsetY	DOMMouseEvent::OffsetY	DontDelete|ReadOnly
  ctrlKey	DOMMouseEvent::CtrlKey	DontDelete|ReadOnly
  shiftKey	DOMMouseEvent::ShiftKey	DontDelete|ReadOnly
  altKey	DOMMouseEvent::AltKey	DontDelete|ReadOnly
  metaKey	DOMMouseEvent::MetaKey	DontDelete|ReadOnly
  button	DOMMouseEvent::Button	DontDelete|ReadOnly
  relatedTarget	DOMMouseEvent::RelatedTarget DontDelete|ReadOnly
  fromElement	DOMMouseEvent::FromElement DontDelete|ReadOnly
  toElement	DOMMouseEvent::ToElement	DontDelete|ReadOnly
@end
@begin DOMMouseEventProtoTable 1
  initMouseEvent	DOMMouseEvent::InitMouseEvent	DontDelete|Function 15
@end
*/
DEFINE_PROTOTYPE("DOMMouseEvent",DOMMouseEventProto)
IMPLEMENT_PROTOFUNC(DOMMouseEventProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMMouseEventProto,DOMMouseEventProtoFunc,DOMUIEventProto)

DOMMouseEvent::DOMMouseEvent(ExecState *exec, MouseEventImpl *e)
  : DOMUIEvent(exec, e)
{
  setPrototype(DOMMouseEventProto::self(exec));
}

DOMMouseEvent::~DOMMouseEvent()
{
}

bool DOMMouseEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMMouseEvent, DOMUIEvent>(exec, &DOMMouseEventTable, this, propertyName, slot);
}

static QPoint offsetFromTarget(const MouseRelatedEventImpl *e)
{
    int x = e->clientX();
    int y = e->clientY();

    NodeImpl *n = e->target();
    if (n) {
        DocumentImpl *doc = n->getDocument();
        if (doc) {
            doc->updateRendering();
            RenderObject *r = n->renderer();
            if (r) {
                int rx, ry;
                if (r->absolutePosition(rx, ry)) {
                    x -= rx;
                    y -= ry;
                }
            }
        }
    }
    return QPoint(x, y);
}

ValueImp *DOMMouseEvent::getValueProperty(ExecState *exec, int token) const
{
  MouseEventImpl &event = *static_cast<MouseEventImpl *>(impl());
  switch (token) {
  case ScreenX:
    return Number(event.screenX());
  case ScreenY:
    return Number(event.screenY());
  case ClientX:
  case X:
    return Number(event.clientX());
  case ClientY:
  case Y:
    return Number(event.clientY());
  case OffsetX: // MSIE extension
    return Number(offsetFromTarget(&event).x());
  case OffsetY: // MSIE extension
    return Number(offsetFromTarget(&event).y());
  case CtrlKey:
    return Boolean(event.ctrlKey());
  case ShiftKey:
    return Boolean(event.shiftKey());
  case AltKey:
    return Boolean(event.altKey());
  case MetaKey:
    return Boolean(event.metaKey());
  case Button:
  {
    // Tricky. The DOM (and khtml) use 0 for LMB, 1 for MMB and 2 for RMB
    // but MSIE uses 1=LMB, 2=RMB, 4=MMB, as a bitfield
    int domButton = event.button();
    int button = domButton==0 ? 1 : domButton==1 ? 4 : domButton==2 ? 2 : 0;
    return Number( (unsigned int)button );
  }
  case ToElement:
    // MSIE extension - "the object toward which the user is moving the mouse pointer"
    if (event.id() == DOM::EventImpl::MOUSEOUT_EVENT)
      return getDOMNode(exec, event.relatedTarget());
    return getDOMNode(exec,event.target());
  case FromElement:
    // MSIE extension - "object from which activation
    // or the mouse pointer is exiting during the event" (huh?)
    if (event.id() == DOM::EventImpl::MOUSEOUT_EVENT)
      return getDOMNode(exec, event.target());
    /* fall through */
  case RelatedTarget:
    return getDOMNode(exec, event.relatedTarget());
  default:
    kdWarning() << "Unhandled token in DOMMouseEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

ValueImp *DOMMouseEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMMouseEvent::info))
    return throwError(exec, TypeError);
  MouseEventImpl &mouseEvent = *static_cast<MouseEventImpl *>(static_cast<DOMMouseEvent *>(thisObj)->impl());
  switch (id) {
    case DOMMouseEvent::InitMouseEvent:
      mouseEvent.initMouseEvent(args[0]->toString(exec).domString(), // typeArg
                                args[1]->toBoolean(exec), // canBubbleArg
                                args[2]->toBoolean(exec), // cancelableArg
                                toAbstractView(args[3]), // viewArg
                                args[4]->toInt32(exec), // detailArg
                                args[5]->toInt32(exec), // screenXArg
                                args[6]->toInt32(exec), // screenYArg
                                args[7]->toInt32(exec), // clientXArg
                                args[8]->toInt32(exec), // clientYArg
                                args[9]->toBoolean(exec), // ctrlKeyArg
                                args[10]->toBoolean(exec), // altKeyArg
                                args[11]->toBoolean(exec), // shiftKeyArg
                                args[12]->toBoolean(exec), // metaKeyArg
                                args[13]->toInt32(exec), // buttonArg
                                toNode(args[14])); // relatedTargetArg
      return Undefined();
  }
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMKeyboardEvent::info = { "KeyboardEvent", &DOMUIEvent::info, &DOMKeyboardEventTable, 0 };

/*
@begin DOMKeyboardEventTable 5
  keyIdentifier	DOMKeyboardEvent::KeyIdentifier	DontDelete|ReadOnly
  keyLocation	DOMKeyboardEvent::KeyLocation	DontDelete|ReadOnly
  ctrlKey	DOMKeyboardEvent::CtrlKey	DontDelete|ReadOnly
  shiftKey	DOMKeyboardEvent::ShiftKey	DontDelete|ReadOnly
  altKey	DOMKeyboardEvent::AltKey	DontDelete|ReadOnly
  metaKey	DOMKeyboardEvent::MetaKey	DontDelete|ReadOnly
  altGraphKey	DOMKeyboardEvent::AltGraphKey	DontDelete|ReadOnly
@end
@begin DOMKeyboardEventProtoTable 1
  initKeyboardEvent	DOMKeyboardEvent::InitKeyboardEvent	DontDelete|Function 11
@end
*/
DEFINE_PROTOTYPE("DOMKeyboardEvent", DOMKeyboardEventProto)
IMPLEMENT_PROTOFUNC(DOMKeyboardEventProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMKeyboardEventProto, DOMKeyboardEventProtoFunc, DOMUIEventProto)

DOMKeyboardEvent::DOMKeyboardEvent(ExecState *exec, KeyboardEventImpl *e)
  : DOMUIEvent(exec, e)
{
  setPrototype(DOMKeyboardEventProto::self(exec));
}

DOMKeyboardEvent::~DOMKeyboardEvent()
{
}

const ClassInfo* DOMKeyboardEvent::classInfo() const
{
    return &info;
}

bool DOMKeyboardEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMKeyboardEvent, DOMUIEvent>(exec, &DOMKeyboardEventTable, this, propertyName, slot);
}

ValueImp *DOMKeyboardEvent::getValueProperty(ExecState *exec, int token) const
{
  KeyboardEventImpl &event = *static_cast<KeyboardEventImpl *>(impl());
  switch (token) {
  case KeyIdentifier:
    return String(event.keyIdentifier());
  case KeyLocation:
    return Number(event.keyLocation());
  case CtrlKey:
    return Boolean(event.ctrlKey());
  case ShiftKey:
    return Boolean(event.shiftKey());
  case AltKey:
    return Boolean(event.altKey());
  case MetaKey:
    return Boolean(event.metaKey());
  case AltGraphKey:
    return Boolean(event.altGraphKey());
  default:
    kdWarning() << "Unhandled token in DOMKeyboardEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

ValueImp *DOMKeyboardEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMKeyboardEvent::info))
    return throwError(exec, TypeError);
  KeyboardEventImpl &event = *static_cast<KeyboardEventImpl *>(static_cast<DOMUIEvent *>(thisObj)->impl());
  switch (id) {
    case DOMKeyboardEvent::InitKeyboardEvent:
      event.initKeyboardEvent(args[0]->toString(exec).domString(), // typeArg
                              args[1]->toBoolean(exec), // canBubbleArg
                              args[2]->toBoolean(exec), // cancelableArg
                              toAbstractView(args[3]), // viewArg
                              args[4]->toString(exec).domString(), // keyIdentifier
                              args[5]->toInt32(exec), // keyLocationArg
                              args[6]->toBoolean(exec), // ctrlKeyArg
                              args[7]->toBoolean(exec), // altKeyArg
                              args[8]->toBoolean(exec), // shiftKeyArg
                              args[9]->toBoolean(exec), // metaKeyArg
                              args[10]->toBoolean(exec)); // altGraphKeyArg
      return Undefined();
  }
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo MutationEventConstructor::info = { "MutationEventConstructor", 0, &MutationEventConstructorTable, 0 };
/*
@begin MutationEventConstructorTable 3
  MODIFICATION	DOM::MutationEvent::MODIFICATION	DontDelete|ReadOnly
  ADDITION	DOM::MutationEvent::ADDITION		DontDelete|ReadOnly
  REMOVAL	DOM::MutationEvent::REMOVAL		DontDelete|ReadOnly
@end
*/
bool MutationEventConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<MutationEventConstructor, DOMObject>(exec, &MutationEventConstructorTable, this, propertyName, slot);
}

ValueImp *MutationEventConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number(token);
}

ValueImp *getMutationEventConstructor(ExecState *exec)
{
  return cacheGlobalObject<MutationEventConstructor>(exec, "[[mutationEvent.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMMutationEvent::info = { "MutationEvent", &DOMEvent::info, &DOMMutationEventTable, 0 };
/*
@begin DOMMutationEventTable 5
  relatedNode	DOMMutationEvent::RelatedNode	DontDelete|ReadOnly
  prevValue	DOMMutationEvent::PrevValue	DontDelete|ReadOnly
  newValue	DOMMutationEvent::NewValue	DontDelete|ReadOnly
  attrName	DOMMutationEvent::AttrName	DontDelete|ReadOnly
  attrChange	DOMMutationEvent::AttrChange	DontDelete|ReadOnly
@end
@begin DOMMutationEventProtoTable 1
  initMutationEvent	DOMMutationEvent::InitMutationEvent	DontDelete|Function 8
@end
*/
DEFINE_PROTOTYPE("DOMMutationEvent",DOMMutationEventProto)
IMPLEMENT_PROTOFUNC(DOMMutationEventProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMMutationEventProto,DOMMutationEventProtoFunc,DOMEventProto)

DOMMutationEvent::DOMMutationEvent(ExecState *exec, MutationEventImpl *e)
  : DOMEvent(exec, e)
{
  setPrototype(DOMMutationEventProto::self(exec));
}

DOMMutationEvent::~DOMMutationEvent()
{
}

bool DOMMutationEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMMutationEvent, DOMEvent>(exec, &DOMMutationEventTable, this, propertyName, slot);
}

ValueImp *DOMMutationEvent::getValueProperty(ExecState *exec, int token) const
{
  MutationEventImpl &event = *static_cast<MutationEventImpl *>(impl());
  switch (token) {
  case RelatedNode:
    return getDOMNode(exec, event.relatedNode());
  case PrevValue:
    return String(event.prevValue());
  case NewValue:
    return String(event.newValue());
  case AttrName:
    return String(event.attrName());
  case AttrChange:
    return Number(event.attrChange());
  default:
    kdWarning() << "Unhandled token in DOMMutationEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

ValueImp *DOMMutationEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMMutationEvent::info))
    return throwError(exec, TypeError);
  MutationEventImpl &mutationEvent = *static_cast<MutationEventImpl *>(static_cast<DOMEvent *>(thisObj)->impl());
  switch (id) {
    case DOMMutationEvent::InitMutationEvent:
      mutationEvent.initMutationEvent(args[0]->toString(exec).domString(), // typeArg,
                                      args[1]->toBoolean(exec), // canBubbleArg
                                      args[2]->toBoolean(exec), // cancelableArg
                                      toNode(args[3]), // relatedNodeArg
                                      args[4]->toString(exec).domString(), // prevValueArg
                                      args[5]->toString(exec).domString(), // newValueArg
                                      args[6]->toString(exec).domString(), // attrNameArg
                                      args[7]->toInt32(exec)); // attrChangeArg
      return Undefined();
  }
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMWheelEvent::info = { "WheelEvent", &DOMEvent::info, &DOMWheelEventTable, 0 };
/*
@begin DOMWheelEventTable 10
    altKey      DOMWheelEvent::AltKey       DontDelete|ReadOnly
    clientX     DOMWheelEvent::ClientX      DontDelete|ReadOnly
    clientY     DOMWheelEvent::ClientY      DontDelete|ReadOnly
    ctrlKey     DOMWheelEvent::CtrlKey      DontDelete|ReadOnly
    metaKey     DOMWheelEvent::MetaKey      DontDelete|ReadOnly
    offsetX     DOMWheelEvent::OffsetX      DontDelete|ReadOnly
    offsetY     DOMWheelEvent::OffsetY      DontDelete|ReadOnly
    screenX     DOMWheelEvent::ScreenX      DontDelete|ReadOnly
    screenY     DOMWheelEvent::ScreenY      DontDelete|ReadOnly
    shiftKey    DOMWheelEvent::ShiftKey     DontDelete|ReadOnly
    wheelDelta  DOMWheelEvent::WheelDelta   DontDelete|ReadOnly
    x           DOMWheelEvent::X            DontDelete|ReadOnly
    y           DOMWheelEvent::Y            DontDelete|ReadOnly
@end
@begin DOMWheelEventProtoTable 1
@end
*/
DEFINE_PROTOTYPE("DOMWheelEvent",DOMWheelEventProto)
IMPLEMENT_PROTOFUNC(DOMWheelEventProtoFunc)
IMPLEMENT_PROTOTYPE_WITH_PARENT(DOMWheelEventProto,DOMWheelEventProtoFunc,DOMEventProto)

DOMWheelEvent::DOMWheelEvent(ExecState *exec, DOM::WheelEventImpl *e)
    : DOMUIEvent(exec, e)
{
}

bool DOMWheelEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<DOMWheelEvent, DOMEvent>(exec, &DOMWheelEventTable, this, propertyName, slot);
}

ValueImp *DOMWheelEvent::getValueProperty(ExecState *exec, int token) const
{
    DOM::WheelEventImpl *e = static_cast<DOM::WheelEventImpl *>(impl());
    switch (token) {
        case AltKey:
            return Boolean(e->altKey());
        case ClientX:
        case X:
            return Number(e->clientX());
        case ClientY:
        case Y:
            return Number(e->clientY());
        case CtrlKey:
            return Number(e->ctrlKey());
        case MetaKey:
            return Number(e->metaKey());
        case OffsetX:
            return Number(offsetFromTarget(e).x());
        case OffsetY:
            return Number(offsetFromTarget(e).y());
        case ScreenX:
            return Number(e->screenX());
        case ScreenY:
            return Number(e->screenY());
        case ShiftKey:
            return Boolean(e->shiftKey());
        case WheelDelta:
            return Number(e->wheelDelta());
    }
    return Undefined();
}

ValueImp *DOMWheelEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    if (!thisObj->inherits(&DOMWheelEvent::info))
        return throwError(exec, TypeError);
    return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo Clipboard::info = { "Clipboard", 0, &ClipboardTable, 0 };

/* Source for ClipboardTable. Use "make hashtables" to regenerate.
@begin ClipboardTable 3
  dropEffect	Clipboard::DropEffect	DontDelete
  effectAllowed	Clipboard::EffectAllowed	DontDelete
  types         Clipboard::Types	DontDelete|ReadOnly
@end
@begin ClipboardProtoTable 4
  clearData	Clipboard::ClearData	DontDelete|Function 0
  getData	Clipboard::GetData	DontDelete|Function 1
  setData	Clipboard::SetData	DontDelete|Function 2
  setDragImage	Clipboard::SetDragImage	DontDelete|Function 3
@end
*/

DEFINE_PROTOTYPE("Clipboard", ClipboardProto)
IMPLEMENT_PROTOFUNC(ClipboardProtoFunc)
IMPLEMENT_PROTOTYPE(ClipboardProto, ClipboardProtoFunc)

Clipboard::Clipboard(ExecState *exec, DOM::ClipboardImpl *cb)
  : clipboard(cb)
{
    setPrototype(ClipboardProto::self(exec));
  
    if (clipboard)
        clipboard->ref();
}

Clipboard::~Clipboard()
{
    if (clipboard)
        clipboard->deref();
}

static ValueImp *stringOrUndefined(const DOM::DOMString &str)
{
    if (str.isNull()) {
        return Undefined();
    } else {
        return String(str);
    }
}

bool Clipboard::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<Clipboard, DOMObject>(exec, &ClipboardTable, this, propertyName, slot);
}

ValueImp *Clipboard::getValueProperty(ExecState *exec, int token) const
{
    switch (token) {
        case DropEffect:
            assert(clipboard->isForDragging() || clipboard->dropEffect().isNull());
            return stringOrUndefined(clipboard->dropEffect());
        case EffectAllowed:
            assert(clipboard->isForDragging() || clipboard->effectAllowed().isNull());
            return stringOrUndefined(clipboard->effectAllowed());
        case Types:
        {
            QStringList qTypes = clipboard->types();
            if (qTypes.isEmpty()) {
                return Null(); 
            } else {
                List list;
                for (QStringList::Iterator it = qTypes.begin(); it != qTypes.end(); ++it) {
                    list.append(String(UString(*it)));
                }
                return exec->lexicalInterpreter()->builtinArray()->construct(exec, list);
            }
        }
        default:
            kdWarning() << "Clipboard::getValueProperty unhandled token " << token << endl;
            return NULL;
    }
}

void Clipboard::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
    lookupPut<Clipboard,DOMObject>(exec, propertyName, value, attr, &ClipboardTable, this );
}

void Clipboard::putValueProperty(ExecState *exec, int token, ValueImp *value, int /*attr*/)
{
    switch (token) {
        case DropEffect:
            // can never set this when not for dragging, thus getting always returns NULL string
            if (clipboard->isForDragging())
                clipboard->setDropEffect(value->toString(exec).domString());
            break;
        case EffectAllowed:
            // can never set this when not for dragging, thus getting always returns NULL string
            if (clipboard->isForDragging())
                clipboard->setEffectAllowed(value->toString(exec).domString());
            break;
        default:
            kdWarning() << "Clipboard::putValueProperty unhandled token " << token << endl;
    }
}

ValueImp *ClipboardProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    if (!thisObj->inherits(&Clipboard::info))
        return throwError(exec, TypeError);

    Clipboard *cb = static_cast<Clipboard *>(thisObj);
    switch (id) {
        case Clipboard::ClearData:
            if (args.size() == 0) {
                cb->clipboard->clearAllData();
                return Undefined();
            } else if (args.size() == 1) {
                cb->clipboard->clearData(args[0]->toString(exec).domString());
                return Undefined();
            } else {
                return throwError(exec, SyntaxError, "clearData: Invalid number of arguments");
            }
        case Clipboard::GetData:
        {
            if (args.size() == 1) {
                bool success;
                DOM::DOMString result = cb->clipboard->getData(args[0]->toString(exec).domString(), success);
                if (success) {
                    return String(result);
                } else {
                    return Undefined();
                }
            } else {
                return throwError(exec, SyntaxError, "getData: Invalid number of arguments");
            }
        }
        case Clipboard::SetData:
            if (args.size() == 2) {
                return Boolean(cb->clipboard->setData(args[0]->toString(exec).domString(), args[1]->toString(exec).domString()));
            } else {
                return throwError(exec, SyntaxError, "setData: Invalid number of arguments");
            }
        case Clipboard::SetDragImage:
        {
            if (!cb->clipboard->isForDragging()) {
                return Undefined();
            }

            if (args.size() != 3)
                return throwError(exec, SyntaxError, "setDragImage: Invalid number of arguments");

            int x = (int)args[1]->toNumber(exec);
            int y = (int)args[2]->toNumber(exec);

            // See if they passed us a node
            NodeImpl *node = toNode(args[0]);
            if (node) {
                if (node->isElementNode()) {
                    cb->clipboard->setDragImageElement(node, QPoint(x,y));                    
                    return Undefined();
                } else {
                    return throwError(exec, SyntaxError, "setDragImageFromElement: Invalid first argument");
                }
            }

            // See if they passed us an Image object
            ObjectImp *o = static_cast<ObjectImp*>(args[0]);
            if (o->inherits(&Image::info)) {
                Image *JSImage = static_cast<Image*>(o);
                cb->clipboard->setDragImage(JSImage->image()->pixmap(), QPoint(x,y));                
                return Undefined();
            } else {
                return throwError(exec, TypeError);
            }
        }
    }
    return Undefined();
}

}
