/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "kjs_events.h"
#include "kjs_events.lut.h"

#include "Frame.h"
#include "kjs_window.h"
#include "kjs_views.h"
#include "kjs_proxy.h"
#include "DocumentImpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "xml/EventNames.h"
#include "rendering/render_object.h"
#include "CachedImage.h"

#include <kdebug.h>

using namespace DOM;
using namespace EventNames;
using namespace khtml;

namespace KJS {

static JSValue* jsStringOrUndefined(const DOMString& str)
{
    return str.isNull() ? jsUndefined() : jsString(str);
}

// -------------------------------------------------------------------------

JSAbstractEventListener::JSAbstractEventListener(bool _html)
    : html(_html)
{
}

void JSAbstractEventListener::handleEvent(EventListenerEvent ele, bool isWindowEvent)
{
#ifdef KJS_DEBUGGER
    if (KJSDebugWin::instance() && KJSDebugWin::instance()->inSession())
        return;
#endif

    EventImpl *event = ele;

    JSObject* listener = listenerObj();
    if (!listener)
        return;

    Window* window = windowObj();
    Frame *frame = window->frame();
    if (!frame)
        return;
    KJSProxyImpl* proxy = frame->jScript();
    if (!proxy)
        return;

    JSLock lock;
  
    ScriptInterpreter* interpreter = proxy->interpreter();
    ExecState* exec = interpreter->globalExec();
  
    bool hasHandleEvent = false;
    JSValue* handleEventFuncValue = listener->get(exec, "handleEvent");
    JSObject* handleEventFunc = 0;
    if (handleEventFuncValue->isObject()) {      
        handleEventFunc = static_cast<JSObject*>(handleEventFuncValue);
        if (!handleEventFunc->implementsCall())
            handleEventFunc = 0;
    }
  
    if (handleEventFunc || listener->implementsCall()) {
        ref();
      
        List args;
        args.append(getDOMEvent(exec, event));
      
        // Set the event we're handling in the Window object
        window->setCurrentEvent(event);
        // ... and in the interpreter
        interpreter->setCurrentEvent(event);
      
        JSValue* retval;
        if (hasHandleEvent)
            retval = handleEventFunc->call(exec, listener, args);
        else {
            JSObject* thisObj;
            if (isWindowEvent)
                thisObj = window;
            else
                thisObj = static_cast<JSObject*>(getDOMNode(exec, event->currentTarget()));
            retval = listener->call(exec, thisObj, args);
        }

        window->setCurrentEvent(0);
        interpreter->setCurrentEvent(0);

        if (exec->hadException()) {
            JSObject* exception = exec->exception()->toObject(exec);
            DOMString message = exception->get(exec, messagePropertyName)->toString(exec).domString();
            int lineNumber = exception->get(exec, "line")->toInt32(exec);
            DOMString sourceURL = exception->get(exec, "sourceURL")->toString(exec).domString();
            if (Interpreter::shouldPrintExceptions())
                printf("(event handler):%s\n", message.qstring().utf8().data());
            frame->addMessageToConsole(message, lineNumber, sourceURL);
            exec->clearException();
        } else {
            if (!retval->isUndefinedOrNull() && event->storesResultAsString())
                event->storeResult(retval->toString(exec).domString());
            if (html) {
                QVariant ret = ValueToVariant(exec, retval);
                if (ret.type() == QVariant::Bool && !ret.toBool())
                    event->preventDefault();
            }
        }

        DocumentImpl::updateDocumentsRendering();
        deref();
    }
}

DOMString JSAbstractEventListener::eventListenerType()
{
    if (html)
        return "_khtml_HTMLEventListener";
    return "_khtml_JSEventListener";
}

// -------------------------------------------------------------------------

JSUnprotectedEventListener::JSUnprotectedEventListener(JSObject* _listener, Window* _win, bool _html)
  : JSAbstractEventListener(_html)
  , listener(_listener)
  , win(_win)
{
    if (_listener)
        _win->jsUnprotectedEventListeners.set(_listener, this);
}

JSUnprotectedEventListener::~JSUnprotectedEventListener()
{
    if (listener && win)
        win->jsUnprotectedEventListeners.remove(listener);
}

JSObject* JSUnprotectedEventListener::listenerObj() const
{ 
    return listener; 
}

Window* JSUnprotectedEventListener::windowObj() const
{
    return win;
}

void JSUnprotectedEventListener::clearWindowObj()
{
    win = 0;
}

void JSUnprotectedEventListener::mark()
{
    if (listener && !listener->marked())
        listener->mark();
}

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(JSObject* _listener, Window* _win, bool _html)
    : JSAbstractEventListener(_html)
    , listener(_listener)
    , win(_win)
{
    if (_listener)
        _win->jsEventListeners.set(_listener, this);
}

JSEventListener::~JSEventListener()
{
    if (listener && win)
        win->jsEventListeners.remove(listener);
}

JSObject* JSEventListener::listenerObj() const
{ 
    return listener; 
}

Window* JSEventListener::windowObj() const
{
    return win;
}

void JSEventListener::clearWindowObj()
{
    win = 0;
}

// -------------------------------------------------------------------------

JSLazyEventListener::JSLazyEventListener(const DOMString& _code, Window* _win, NodeImpl* _originalNode, int lineno)
  : JSEventListener(0, _win, true),
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

JSObject* JSLazyEventListener::listenerObj() const
{
    parseCode();
    return listener;
}

JSValue* JSLazyEventListener::eventParameterName() const
{
    static ProtectedPtr<JSValue> eventString = jsString("event");
    return eventString.get();
}

void JSLazyEventListener::parseCode() const
{
    if (parsed)
        return;
    parsed = true;

    Frame *frame = windowObj()->frame();
    KJSProxyImpl *proxy = 0;
    if (frame)
        proxy = frame->jScript();

    if (proxy) {
        ScriptInterpreter* interpreter = proxy->interpreter();
        ExecState* exec = interpreter->globalExec();

        JSLock lock;
        JSObject* constr = interpreter->builtinFunction();
        List args;

        UString sourceURL(frame->url().url());
        args.append(eventParameterName());
        args.append(jsString(code));
        listener = constr->construct(exec, args, sourceURL, lineNumber); // ### is globalExec ok ?

        if (exec->hadException()) {
            exec->clearException();

            // failed to parse, so let's just make this listener a no-op
            listener = 0;
        } else if (originalNode) {
            // Add the event's home element to the scope
            // (and the document, and the form - see HTMLElement::eventHandlerScope)
            ScopeChain scope = listener->scope();

            JSValue* thisObj = getDOMNode(exec, originalNode);
            if (thisObj->isObject()) {
                static_cast<DOMNode*>(thisObj)->pushEventHandlerScope(exec, scope);
                listener->setScope(scope);
            }
        }
    }

    // no more need to keep the unparsed code around
    code = DOMString();

    if (listener)
        windowObj()->jsEventListeners.set(listener, const_cast<JSLazyEventListener*>(this));
}

JSValue* getNodeEventListener(NodeImpl* n, const AtomicString& eventType)
{
    if (JSAbstractEventListener* listener = static_cast<JSAbstractEventListener*>(n->getHTMLEventListener(eventType)))
        if (JSValue* obj = listener->listenerObj())
            return obj;
    return jsNull();
}

// -------------------------------------------------------------------------

const ClassInfo EventConstructor::info = { "EventConstructor", 0, &EventConstructorTable, 0 };
/*
@begin EventConstructorTable 3
  CAPTURING_PHASE       DOM::Event::CAPTURING_PHASE     DontDelete|ReadOnly
  AT_TARGET             DOM::Event::AT_TARGET           DontDelete|ReadOnly
  BUBBLING_PHASE        DOM::Event::BUBBLING_PHASE      DontDelete|ReadOnly
# Reverse-engineered from Netscape
  MOUSEDOWN             1                               DontDelete|ReadOnly
  MOUSEUP               2                               DontDelete|ReadOnly
  MOUSEOVER             4                               DontDelete|ReadOnly
  MOUSEOUT              8                               DontDelete|ReadOnly
  MOUSEMOVE             16                              DontDelete|ReadOnly
  MOUSEDRAG             32                              DontDelete|ReadOnly
  CLICK                 64                              DontDelete|ReadOnly
  DBLCLICK              128                             DontDelete|ReadOnly
  KEYDOWN               256                             DontDelete|ReadOnly
  KEYUP                 512                             DontDelete|ReadOnly
  KEYPRESS              1024                            DontDelete|ReadOnly
  DRAGDROP              2048                            DontDelete|ReadOnly
  FOCUS                 4096                            DontDelete|ReadOnly
  BLUR                  8192                            DontDelete|ReadOnly
  SELECT                16384                           DontDelete|ReadOnly
  CHANGE                32768                           DontDelete|ReadOnly
@end
*/

bool EventConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<EventConstructor, DOMObject>(exec, &EventConstructorTable, this, propertyName, slot);
}

JSValue *EventConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return jsNumber(token);
}

JSValue *getEventConstructor(ExecState *exec)
{
  return cacheGlobalObject<EventConstructor>(exec, "[[event.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMEvent::info = { "Event", 0, &DOMEventTable, 0 };
/*
@begin DOMEventTable 12
  type          DOMEvent::Type          DontDelete|ReadOnly
  target        DOMEvent::Target        DontDelete|ReadOnly
  currentTarget DOMEvent::CurrentTarget DontDelete|ReadOnly
  srcElement    DOMEvent::SrcElement    DontDelete|ReadOnly
  eventPhase    DOMEvent::EventPhase    DontDelete|ReadOnly
  bubbles       DOMEvent::Bubbles       DontDelete|ReadOnly
  cancelable    DOMEvent::Cancelable    DontDelete|ReadOnly
  timeStamp     DOMEvent::TimeStamp     DontDelete|ReadOnly
  returnValue   DOMEvent::ReturnValue   DontDelete
  cancelBubble  DOMEvent::CancelBubble  DontDelete
  dataTransfer  DOMEvent::DataTransfer  DontDelete|ReadOnly
  clipboardData  DOMEvent::ClipboardData  DontDelete|ReadOnly
@end
@begin DOMEventProtoTable 3
  stopPropagation       DOMEvent::StopPropagation       DontDelete|Function 0
  preventDefault        DOMEvent::PreventDefault        DontDelete|Function 0
  initEvent             DOMEvent::InitEvent             DontDelete|Function 3
@end
*/
KJS_DEFINE_PROTOTYPE(DOMEventProto)
KJS_IMPLEMENT_PROTOFUNC(DOMEventProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMEvent", DOMEventProto, DOMEventProtoFunc)

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
    JSObject::mark();
    if (clipboard && !clipboard->marked())
        clipboard->mark();
}

bool DOMEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMEvent, DOMObject>(exec, &DOMEventTable, this, propertyName, slot);
}

JSValue *DOMEvent::getValueProperty(ExecState *exec, int token) const
{
  EventImpl &event = *m_impl;
  switch (token) {
  case Type:
    return jsString(event.type().domString());
  case Target:
  case SrcElement: /*MSIE extension - "the object that fired the event"*/
    return getDOMNode(exec, event.target());
  case CurrentTarget:
    return getDOMNode(exec, event.currentTarget());
  case EventPhase:
    return jsNumber(event.eventPhase());
  case Bubbles:
    return jsBoolean(event.bubbles());
  case CancelBubble:
    return jsBoolean(event.getCancelBubble());
  case ReturnValue:
    return jsBoolean(!event.defaultPrevented());
  case Cancelable:
    return jsBoolean(event.cancelable());
  case TimeStamp:
    return jsNumber(event.timeStamp());
  case ClipboardData:
  {
    if (event.isClipboardEvent()) {
      ClipboardEventImpl *impl = static_cast<ClipboardEventImpl *>(&event);
      if (!clipboard) {
        clipboard = new Clipboard(exec, impl->clipboard());
      }
      return clipboard;
    } else {
      return jsUndefined();
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
      return jsUndefined();
    }
  }
  default:
    kdWarning() << "Unhandled token in DOMEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

void DOMEvent::put(ExecState *exec, const Identifier &propertyName,
                      JSValue *value, int attr)
{
  lookupPut<DOMEvent, DOMObject>(exec, propertyName, value, attr,
                                          &DOMEventTable, this);
}

void DOMEvent::putValueProperty(ExecState *exec, int token, JSValue *value, int)
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

JSValue *DOMEventProtoFunc::callAsFunction(ExecState *exec, JSObject * thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMEvent::info))
    return throwError(exec, TypeError);
  EventImpl &event = *static_cast<DOMEvent *>( thisObj )->impl();
  switch (id) {
    case DOMEvent::StopPropagation:
      event.stopPropagation();
      return jsUndefined();
    case DOMEvent::PreventDefault:
      event.preventDefault();
      return jsUndefined();
    case DOMEvent::InitEvent:
      event.initEvent(AtomicString(args[0]->toString(exec).domString()), args[1]->toBoolean(exec), args[2]->toBoolean(exec));
      return jsUndefined();
  };
  return jsUndefined();
}

JSValue *getDOMEvent(ExecState *exec, EventImpl *e)
{
  if (!e)
    return jsNull();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());

  JSLock lock;

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

  return ret;
}

EventImpl *toEvent(JSValue *val)
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

JSValue *EventExceptionConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return jsNumber(token);
}

JSValue *getEventExceptionConstructor(ExecState *exec)
{
  return cacheGlobalObject<EventExceptionConstructor>(exec, "[[eventException.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMUIEvent::info = { "UIEvent", &DOMEvent::info, &DOMUIEventTable, 0 };
/*
@begin DOMUIEventTable 8
  view          DOMUIEvent::View        DontDelete|ReadOnly
  detail        DOMUIEvent::Detail      DontDelete|ReadOnly
  keyCode       DOMUIEvent::KeyCode     DontDelete|ReadOnly
  charCode      DOMUIEvent::CharCode    DontDelete|ReadOnly
  layerX        DOMUIEvent::LayerX      DontDelete|ReadOnly
  layerY        DOMUIEvent::LayerY      DontDelete|ReadOnly
  pageX         DOMUIEvent::PageX       DontDelete|ReadOnly
  pageY         DOMUIEvent::PageY       DontDelete|ReadOnly
  which         DOMUIEvent::Which       DontDelete|ReadOnly
@end
@begin DOMUIEventProtoTable 1
  initUIEvent   DOMUIEvent::InitUIEvent DontDelete|Function 5
@end
*/
KJS_DEFINE_PROTOTYPE(DOMUIEventProto)
KJS_IMPLEMENT_PROTOFUNC(DOMUIEventProtoFunc)
KJS_IMPLEMENT_PROTOTYPE_WITH_PARENT("DOMUIEvent",DOMUIEventProto,DOMUIEventProtoFunc,DOMEventProto)

DOMUIEvent::DOMUIEvent(ExecState *exec, UIEventImpl *e)
  : DOMEvent(exec, e)
{
  setPrototype(DOMUIEventProto::self(exec));
}

bool DOMUIEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMUIEvent, DOMEvent>(exec, &DOMUIEventTable, this, propertyName, slot);
}

JSValue *DOMUIEvent::getValueProperty(ExecState *exec, int token) const
{
  UIEventImpl &event = *static_cast<UIEventImpl *>(impl());
  switch (token) {
  case View:
    return getDOMAbstractView(exec, event.view());
  case Detail:
    return jsNumber(event.detail());
  case KeyCode:
    return jsNumber(event.keyCode());
  case CharCode:
    return jsNumber(event.charCode());
  case LayerX:
    return jsNumber(event.layerX());
  case LayerY:
    return jsNumber(event.layerY());
  case PageX:
    return jsNumber(event.pageX());
  case PageY:
    return jsNumber(event.pageY());
  case Which:
    return jsNumber(event.which());
  default:
    kdWarning() << "Unhandled token in DOMUIEvent::getValueProperty : " << token << endl;
    return jsUndefined();
  }
}

JSValue *DOMUIEventProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMUIEvent::info))
    return throwError(exec, TypeError);
  UIEventImpl &uiEvent = *static_cast<UIEventImpl *>(static_cast<DOMUIEvent *>(thisObj)->impl());
  switch (id) {
    case DOMUIEvent::InitUIEvent:
      uiEvent.initUIEvent(AtomicString(args[0]->toString(exec).domString()),
                          args[1]->toBoolean(exec),
                          args[2]->toBoolean(exec),
                          toAbstractView(args[3]),
                          args[4]->toInt32(exec));
      return jsUndefined();
  }
  return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMMouseEvent::info = { "MouseEvent", &DOMUIEvent::info, &DOMMouseEventTable, 0 };

/*
@begin DOMMouseEventTable 16
  screenX       DOMMouseEvent::ScreenX  DontDelete|ReadOnly
  screenY       DOMMouseEvent::ScreenY  DontDelete|ReadOnly
  clientX       DOMMouseEvent::ClientX  DontDelete|ReadOnly
  x             DOMMouseEvent::X        DontDelete|ReadOnly
  clientY       DOMMouseEvent::ClientY  DontDelete|ReadOnly
  y             DOMMouseEvent::Y        DontDelete|ReadOnly
  offsetX       DOMMouseEvent::OffsetX  DontDelete|ReadOnly
  offsetY       DOMMouseEvent::OffsetY  DontDelete|ReadOnly
  ctrlKey       DOMMouseEvent::CtrlKey  DontDelete|ReadOnly
  shiftKey      DOMMouseEvent::ShiftKey DontDelete|ReadOnly
  altKey        DOMMouseEvent::AltKey   DontDelete|ReadOnly
  metaKey       DOMMouseEvent::MetaKey  DontDelete|ReadOnly
  button        DOMMouseEvent::Button   DontDelete|ReadOnly
  relatedTarget DOMMouseEvent::RelatedTarget DontDelete|ReadOnly
  fromElement   DOMMouseEvent::FromElement DontDelete|ReadOnly
  toElement     DOMMouseEvent::ToElement        DontDelete|ReadOnly
@end
@begin DOMMouseEventProtoTable 1
  initMouseEvent        DOMMouseEvent::InitMouseEvent   DontDelete|Function 15
@end
*/
KJS_DEFINE_PROTOTYPE(DOMMouseEventProto)
KJS_IMPLEMENT_PROTOFUNC(DOMMouseEventProtoFunc)
KJS_IMPLEMENT_PROTOTYPE_WITH_PARENT("DOMMouseEvent",DOMMouseEventProto,DOMMouseEventProtoFunc,DOMUIEventProto)

DOMMouseEvent::DOMMouseEvent(ExecState *exec, MouseEventImpl *e)
  : DOMUIEvent(exec, e)
{
  setPrototype(DOMMouseEventProto::self(exec));
}

bool DOMMouseEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMMouseEvent, DOMUIEvent>(exec, &DOMMouseEventTable, this, propertyName, slot);
}

JSValue *DOMMouseEvent::getValueProperty(ExecState *exec, int token) const
{
  MouseEventImpl &event = *static_cast<MouseEventImpl *>(impl());
  switch (token) {
  case ScreenX:
    return jsNumber(event.screenX());
  case ScreenY:
    return jsNumber(event.screenY());
  case ClientX:
    return jsNumber(event.clientX());
  case ClientY:
    return jsNumber(event.clientY());
  case OffsetX:
    return jsNumber(event.offsetX());
  case OffsetY:
    return jsNumber(event.offsetY());
  case CtrlKey:
    return jsBoolean(event.ctrlKey());
  case ShiftKey:
    return jsBoolean(event.shiftKey());
  case AltKey:
    return jsBoolean(event.altKey());
  case MetaKey:
    return jsBoolean(event.metaKey());
  case Button:
    return jsNumber(event.button());
  case ToElement:
    return getDOMNode(exec, event.toElement());
  case FromElement:
    return getDOMNode(exec, event.fromElement());
  case RelatedTarget:
    return getDOMNode(exec, event.relatedTarget());
  case X:
    return jsNumber(event.x());
  case Y:
    return jsNumber(event.y());
  default:
    kdWarning() << "Unhandled token in DOMMouseEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

JSValue *DOMMouseEventProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMMouseEvent::info))
    return throwError(exec, TypeError);
  MouseEventImpl &mouseEvent = *static_cast<MouseEventImpl *>(static_cast<DOMMouseEvent *>(thisObj)->impl());
  switch (id) {
    case DOMMouseEvent::InitMouseEvent:
      mouseEvent.initMouseEvent(AtomicString(args[0]->toString(exec).domString()), // typeArg
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
      return jsUndefined();
  }
  return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMKeyboardEvent::info = { "KeyboardEvent", &DOMUIEvent::info, &DOMKeyboardEventTable, 0 };

/*
@begin DOMKeyboardEventTable 5
  keyIdentifier DOMKeyboardEvent::KeyIdentifier DontDelete|ReadOnly
  keyLocation   DOMKeyboardEvent::KeyLocation   DontDelete|ReadOnly
  ctrlKey       DOMKeyboardEvent::CtrlKey       DontDelete|ReadOnly
  shiftKey      DOMKeyboardEvent::ShiftKey      DontDelete|ReadOnly
  altKey        DOMKeyboardEvent::AltKey        DontDelete|ReadOnly
  metaKey       DOMKeyboardEvent::MetaKey       DontDelete|ReadOnly
  altGraphKey   DOMKeyboardEvent::AltGraphKey   DontDelete|ReadOnly
@end
@begin DOMKeyboardEventProtoTable 1
  initKeyboardEvent     DOMKeyboardEvent::InitKeyboardEvent     DontDelete|Function 11
@end
*/
KJS_DEFINE_PROTOTYPE(DOMKeyboardEventProto)
KJS_IMPLEMENT_PROTOFUNC(DOMKeyboardEventProtoFunc)
KJS_IMPLEMENT_PROTOTYPE_WITH_PARENT("DOMKeyboardEvent", DOMKeyboardEventProto, DOMKeyboardEventProtoFunc, DOMUIEventProto)

DOMKeyboardEvent::DOMKeyboardEvent(ExecState *exec, KeyboardEventImpl *e)
  : DOMUIEvent(exec, e)
{
  setPrototype(DOMKeyboardEventProto::self(exec));
}

bool DOMKeyboardEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMKeyboardEvent, DOMUIEvent>(exec, &DOMKeyboardEventTable, this, propertyName, slot);
}

JSValue *DOMKeyboardEvent::getValueProperty(ExecState *exec, int token) const
{
  KeyboardEventImpl &event = *static_cast<KeyboardEventImpl *>(impl());
  switch (token) {
  case KeyIdentifier:
    return jsString(event.keyIdentifier());
  case KeyLocation:
    return jsNumber(event.keyLocation());
  case CtrlKey:
    return jsBoolean(event.ctrlKey());
  case ShiftKey:
    return jsBoolean(event.shiftKey());
  case AltKey:
    return jsBoolean(event.altKey());
  case MetaKey:
    return jsBoolean(event.metaKey());
  case AltGraphKey:
    return jsBoolean(event.altGraphKey());
  default:
    kdWarning() << "Unhandled token in DOMKeyboardEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

JSValue *DOMKeyboardEventProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMKeyboardEvent::info))
    return throwError(exec, TypeError);
  KeyboardEventImpl &event = *static_cast<KeyboardEventImpl *>(static_cast<DOMUIEvent *>(thisObj)->impl());
  switch (id) {
    case DOMKeyboardEvent::InitKeyboardEvent:
      event.initKeyboardEvent(AtomicString(args[0]->toString(exec).domString()), // typeArg
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
      return jsUndefined();
  }
  return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo MutationEventConstructor::info = { "MutationEventConstructor", 0, &MutationEventConstructorTable, 0 };
/*
@begin MutationEventConstructorTable 3
  MODIFICATION  DOM::MutationEvent::MODIFICATION        DontDelete|ReadOnly
  ADDITION      DOM::MutationEvent::ADDITION            DontDelete|ReadOnly
  REMOVAL       DOM::MutationEvent::REMOVAL             DontDelete|ReadOnly
@end
*/
bool MutationEventConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<MutationEventConstructor, DOMObject>(exec, &MutationEventConstructorTable, this, propertyName, slot);
}

JSValue *MutationEventConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return jsNumber(token);
}

JSValue *getMutationEventConstructor(ExecState *exec)
{
  return cacheGlobalObject<MutationEventConstructor>(exec, "[[mutationEvent.constructor]]");
}

// -------------------------------------------------------------------------

const ClassInfo DOMMutationEvent::info = { "MutationEvent", &DOMEvent::info, &DOMMutationEventTable, 0 };
/*
@begin DOMMutationEventTable 5
  relatedNode   DOMMutationEvent::RelatedNode   DontDelete|ReadOnly
  prevValue     DOMMutationEvent::PrevValue     DontDelete|ReadOnly
  newValue      DOMMutationEvent::NewValue      DontDelete|ReadOnly
  attrName      DOMMutationEvent::AttrName      DontDelete|ReadOnly
  attrChange    DOMMutationEvent::AttrChange    DontDelete|ReadOnly
@end
@begin DOMMutationEventProtoTable 1
  initMutationEvent     DOMMutationEvent::InitMutationEvent     DontDelete|Function 8
@end
*/
KJS_DEFINE_PROTOTYPE(DOMMutationEventProto)
KJS_IMPLEMENT_PROTOFUNC(DOMMutationEventProtoFunc)
KJS_IMPLEMENT_PROTOTYPE_WITH_PARENT("DOMMutationEvent",DOMMutationEventProto,DOMMutationEventProtoFunc,DOMEventProto)

DOMMutationEvent::DOMMutationEvent(ExecState *exec, MutationEventImpl *e)
  : DOMEvent(exec, e)
{
  setPrototype(DOMMutationEventProto::self(exec));
}

bool DOMMutationEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMMutationEvent, DOMEvent>(exec, &DOMMutationEventTable, this, propertyName, slot);
}

JSValue *DOMMutationEvent::getValueProperty(ExecState *exec, int token) const
{
  MutationEventImpl &event = *static_cast<MutationEventImpl *>(impl());
  switch (token) {
  case RelatedNode:
    return getDOMNode(exec, event.relatedNode());
  case PrevValue:
    return jsString(event.prevValue());
  case NewValue:
    return jsString(event.newValue());
  case AttrName:
    return jsString(event.attrName());
  case AttrChange:
    return jsNumber(event.attrChange());
  default:
    kdWarning() << "Unhandled token in DOMMutationEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

JSValue *DOMMutationEventProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMMutationEvent::info))
    return throwError(exec, TypeError);
  MutationEventImpl &mutationEvent = *static_cast<MutationEventImpl *>(static_cast<DOMEvent *>(thisObj)->impl());
  switch (id) {
    case DOMMutationEvent::InitMutationEvent:
      mutationEvent.initMutationEvent(AtomicString(args[0]->toString(exec).domString()), // typeArg,
                                      args[1]->toBoolean(exec), // canBubbleArg
                                      args[2]->toBoolean(exec), // cancelableArg
                                      toNode(args[3]), // relatedNodeArg
                                      args[4]->toString(exec).domString(), // prevValueArg
                                      args[5]->toString(exec).domString(), // newValueArg
                                      args[6]->toString(exec).domString(), // attrNameArg
                                      args[7]->toInt32(exec)); // attrChangeArg
      return jsUndefined();
  }
  return jsUndefined();
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
KJS_DEFINE_PROTOTYPE(DOMWheelEventProto)
KJS_IMPLEMENT_PROTOFUNC(DOMWheelEventProtoFunc)
KJS_IMPLEMENT_PROTOTYPE_WITH_PARENT("DOMWheelEvent",DOMWheelEventProto,DOMWheelEventProtoFunc,DOMEventProto)

DOMWheelEvent::DOMWheelEvent(ExecState *exec, DOM::WheelEventImpl *e)
    : DOMUIEvent(exec, e)
{
}

bool DOMWheelEvent::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<DOMWheelEvent, DOMEvent>(exec, &DOMWheelEventTable, this, propertyName, slot);
}

JSValue *DOMWheelEvent::getValueProperty(ExecState *exec, int token) const
{
    DOM::WheelEventImpl *e = static_cast<DOM::WheelEventImpl *>(impl());
    switch (token) {
        case AltKey:
            return jsBoolean(e->altKey());
        case ClientX:
            return jsNumber(e->clientX());
        case ClientY:
            return jsNumber(e->clientY());
        case CtrlKey:
            return jsNumber(e->ctrlKey());
        case MetaKey:
            return jsNumber(e->metaKey());
        case OffsetX:
            return jsNumber(e->offsetX());
        case OffsetY:
            return jsNumber(e->offsetY());
        case ScreenX:
            return jsNumber(e->screenX());
        case ScreenY:
            return jsNumber(e->screenY());
        case ShiftKey:
            return jsBoolean(e->shiftKey());
        case WheelDelta:
            return jsNumber(e->wheelDelta());
        case X:
            return jsNumber(e->x());
        case Y:
            return jsNumber(e->y());
    }
    return jsUndefined();
}

JSValue *DOMWheelEventProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    if (!thisObj->inherits(&DOMWheelEvent::info))
        return throwError(exec, TypeError);
    return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo Clipboard::info = { "Clipboard", 0, &ClipboardTable, 0 };

/* Source for ClipboardTable. Use "make hashtables" to regenerate.
@begin ClipboardTable 3
  dropEffect    Clipboard::DropEffect   DontDelete
  effectAllowed Clipboard::EffectAllowed        DontDelete
  types         Clipboard::Types        DontDelete|ReadOnly
@end
@begin ClipboardProtoTable 4
  clearData     Clipboard::ClearData    DontDelete|Function 0
  getData       Clipboard::GetData      DontDelete|Function 1
  setData       Clipboard::SetData      DontDelete|Function 2
  setDragImage  Clipboard::SetDragImage DontDelete|Function 3
@end
*/

KJS_DEFINE_PROTOTYPE(ClipboardProto)
KJS_IMPLEMENT_PROTOFUNC(ClipboardProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("Clipboard", ClipboardProto, ClipboardProtoFunc)

Clipboard::Clipboard(ExecState *exec, DOM::ClipboardImpl *cb)
  : clipboard(cb)
{
    setPrototype(ClipboardProto::self(exec));
}

bool Clipboard::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<Clipboard, DOMObject>(exec, &ClipboardTable, this, propertyName, slot);
}

JSValue *Clipboard::getValueProperty(ExecState *exec, int token) const
{
    switch (token) {
        case DropEffect:
            assert(clipboard->isForDragging() || clipboard->dropEffect().isNull());
            return jsStringOrUndefined(clipboard->dropEffect());
        case EffectAllowed:
            assert(clipboard->isForDragging() || clipboard->effectAllowed().isNull());
            return jsStringOrUndefined(clipboard->effectAllowed());
        case Types:
        {
            QStringList qTypes = clipboard->types();
            if (qTypes.isEmpty()) {
                return jsNull(); 
            } else {
                List list;
                for (QStringList::Iterator it = qTypes.begin(); it != qTypes.end(); ++it) {
                    list.append(jsString(UString(*it)));
                }
                return exec->lexicalInterpreter()->builtinArray()->construct(exec, list);
            }
        }
        default:
            kdWarning() << "Clipboard::getValueProperty unhandled token " << token << endl;
            return NULL;
    }
}

void Clipboard::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
    lookupPut<Clipboard,DOMObject>(exec, propertyName, value, attr, &ClipboardTable, this );
}

void Clipboard::putValueProperty(ExecState *exec, int token, JSValue *value, int /*attr*/)
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

JSValue *ClipboardProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    if (!thisObj->inherits(&Clipboard::info))
        return throwError(exec, TypeError);

    Clipboard *cb = static_cast<Clipboard *>(thisObj);
    switch (id) {
        case Clipboard::ClearData:
            if (args.size() == 0) {
                cb->clipboard->clearAllData();
                return jsUndefined();
            } else if (args.size() == 1) {
                cb->clipboard->clearData(args[0]->toString(exec).domString());
                return jsUndefined();
            } else {
                return throwError(exec, SyntaxError, "clearData: Invalid number of arguments");
            }
        case Clipboard::GetData:
        {
            if (args.size() == 1) {
                bool success;
                DOM::DOMString result = cb->clipboard->getData(args[0]->toString(exec).domString(), success);
                if (success) {
                    return jsString(result);
                } else {
                    return jsUndefined();
                }
            } else {
                return throwError(exec, SyntaxError, "getData: Invalid number of arguments");
            }
        }
        case Clipboard::SetData:
            if (args.size() == 2) {
                return jsBoolean(cb->clipboard->setData(args[0]->toString(exec).domString(), args[1]->toString(exec).domString()));
            } else {
                return throwError(exec, SyntaxError, "setData: Invalid number of arguments");
            }
        case Clipboard::SetDragImage:
        {
            if (!cb->clipboard->isForDragging()) {
                return jsUndefined();
            }

            if (args.size() != 3)
                return throwError(exec, SyntaxError, "setDragImage: Invalid number of arguments");

            int x = (int)args[1]->toNumber(exec);
            int y = (int)args[2]->toNumber(exec);

            // See if they passed us a node
            NodeImpl *node = toNode(args[0]);
            if (node) {
                if (node->isElementNode()) {
                    cb->clipboard->setDragImageElement(node, IntPoint(x,y));                    
                    return jsUndefined();
                } else {
                    return throwError(exec, SyntaxError, "setDragImageFromElement: Invalid first argument");
                }
            }

            // See if they passed us an Image object
            JSObject *o = static_cast<JSObject*>(args[0]);
            if (o->isObject() && o->inherits(&Image::info)) {
                Image *JSImage = static_cast<Image*>(o);
                cb->clipboard->setDragImage(JSImage->image()->pixmap(), IntPoint(x,y));                
                return jsUndefined();
            } else {
                return throwError(exec, TypeError);
            }
        }
    }
    return jsUndefined();
}

}
