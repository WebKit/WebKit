/*
 *  Copyright (C) 2004, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
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

#include "config.h"
#include "JSXMLHttpRequest.h"

#include "DOMWindow.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "XMLHttpRequest.h"
#include "kjs_events.h"
#include "kjs_window.h"

#include "JSXMLHttpRequest.lut.h"

namespace KJS {

using namespace WebCore;

////////////////////// JSXMLHttpRequest Object ////////////////////////

/* Source for JSXMLHttpRequestPrototypeTable.
@begin JSXMLHttpRequestPrototypeTable 7
  abort                 &JSXMLHttpRequestPrototypeFunctionAbort::create                   DontDelete|Function 0
  getAllResponseHeaders &JSXMLHttpRequestPrototypeFunctionGetAllResponseHeaders::create   DontDelete|Function 0
  getResponseHeader     &JSXMLHttpRequestPrototypeFunctionGetResponseHeader::create       DontDelete|Function 1
  open                  &JSXMLHttpRequestPrototypeFunctionOpen::create                    DontDelete|Function 5
  overrideMimeType      &JSXMLHttpRequestPrototypeFunctionOverrideMIMEType::create        DontDelete|Function 1
  send                  &JSXMLHttpRequestPrototypeFunctionSend::create                    DontDelete|Function 1
  setRequestHeader      &JSXMLHttpRequestPrototypeFunctionSetRequestHeader::create        DontDelete|Function 2
# from the EventTarget interface
# FIXME: add DOM3 EventTarget methods (addEventListenerNS, removeEventListenerNS).
  addEventListener      &JSXMLHttpRequestPrototypeFunctionAddEventListener::create        DontDelete|Function 3
  removeEventListener   &JSXMLHttpRequestPrototypeFunctionRemoveEventListener::create     DontDelete|Function 3
  dispatchEvent         &JSXMLHttpRequestPrototypeFunctionDispatchEvent::create           DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(JSXMLHttpRequestPrototype)
KJS_IMPLEMENT_PROTOTYPE("JSXMLHttpRequest", JSXMLHttpRequestPrototype)

JSXMLHttpRequestConstructorImp::JSXMLHttpRequestConstructorImp(ExecState* exec, Document* d)
    : doc(d)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
    putDirect(exec->propertyNames().prototype, JSXMLHttpRequestPrototype::self(exec), None);
}

bool JSXMLHttpRequestConstructorImp::implementsConstruct() const
{
    return true;
}

JSObject* JSXMLHttpRequestConstructorImp::construct(ExecState* exec, const List&)
{
    return new JSXMLHttpRequest(exec, doc.get());
}

const ClassInfo JSXMLHttpRequest::info = { "JSXMLHttpRequest", 0, &JSXMLHttpRequestTable };

/* Source for JSXMLHttpRequestTable.
@begin JSXMLHttpRequestTable 7
  readyState            JSXMLHttpRequest::ReadyState              DontDelete|ReadOnly
  responseText          JSXMLHttpRequest::ResponseText            DontDelete|ReadOnly
  responseXML           JSXMLHttpRequest::ResponseXML             DontDelete|ReadOnly
  status                JSXMLHttpRequest::Status                  DontDelete|ReadOnly
  statusText            JSXMLHttpRequest::StatusText              DontDelete|ReadOnly
  onreadystatechange    JSXMLHttpRequest::Onreadystatechange      DontDelete
  onload                JSXMLHttpRequest::Onload                  DontDelete
@end
*/

bool JSXMLHttpRequest::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSXMLHttpRequest, DOMObject>(exec, &JSXMLHttpRequestTable, this, propertyName, slot);
}

JSValue* JSXMLHttpRequest::getValueProperty(ExecState* exec, int token) const
{
    ExceptionCode ec = 0;

    switch (token) {
        case ReadyState:
            return jsNumber(m_impl->getReadyState());
        case ResponseText:
            return jsOwnedStringOrNull(m_impl->getResponseText());
        case ResponseXML:
            if (Document* responseXML = m_impl->getResponseXML())
                return toJS(exec, responseXML);
            return jsNull();
        case Status: {
            JSValue* result = jsNumber(m_impl->getStatus(ec));
            setDOMException(exec, ec);
            return result;
        }
        case StatusText: {
            JSValue* result = jsString(m_impl->getStatusText(ec));
            setDOMException(exec, ec);
            return result;
        }
        case Onreadystatechange:
            if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onReadyStateChangeListener()))
                if (JSObject* listenerObj = listener->listenerObj())
                    return listenerObj;
            return jsNull();
        case Onload:
            if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadListener()))
                if (JSObject* listenerObj = listener->listenerObj())
                    return listenerObj;
            return jsNull();
        default:
            return 0;
    }
}

void JSXMLHttpRequest::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    lookupPut<JSXMLHttpRequest,DOMObject>(exec, propertyName, value, attr, &JSXMLHttpRequestTable, this );
}

void JSXMLHttpRequest::putValueProperty(ExecState* exec, int token, JSValue* value, int /*attr*/)
{
    switch (token) {
        case Onreadystatechange: {
            Document* doc = m_impl->document();
            if (!doc)
                return;
            Frame* frame = doc->frame();
            if (!frame)
                return;
            m_impl->setOnReadyStateChangeListener(KJS::Window::retrieveWindow(frame)->findOrCreateJSUnprotectedEventListener(value, true));
            break;
        }
        case Onload: {
            Document* doc = m_impl->document();
            if (!doc)
                return;
            Frame* frame = doc->frame();
            if (!frame)
                return;
            m_impl->setOnLoadListener(KJS::Window::retrieveWindow(frame)->findOrCreateJSUnprotectedEventListener(value, true));
            break;
        }
    }
}

void JSXMLHttpRequest::mark()
{
    DOMObject::mark();

    JSUnprotectedEventListener* onReadyStateChangeListener = static_cast<JSUnprotectedEventListener*>(m_impl->onReadyStateChangeListener());
    JSUnprotectedEventListener* onLoadListener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadListener());

    if (onReadyStateChangeListener)
        onReadyStateChangeListener->mark();

    if (onLoadListener)
        onLoadListener->mark();
    
    typedef XMLHttpRequest::EventListenersMap EventListenersMap;
    typedef XMLHttpRequest::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = m_impl->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter) {
            JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(vecIter->get());
            listener->mark();
        }
    }
}


JSXMLHttpRequest::JSXMLHttpRequest(ExecState* exec, Document* d)
  : m_impl(new XMLHttpRequest(d))
{
    setPrototype(JSXMLHttpRequestPrototype::self(exec));
    ScriptInterpreter::putDOMObject(m_impl.get(), this);
}

JSXMLHttpRequest::~JSXMLHttpRequest()
{
    m_impl->setOnReadyStateChangeListener(0);
    m_impl->setOnLoadListener(0);
    m_impl->eventListeners().clear();
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* JSXMLHttpRequestPrototypeFunctionAbort::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);

    request->impl()->abort();
    return jsUndefined();
}

JSValue* JSXMLHttpRequestPrototypeFunctionGetAllResponseHeaders::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);
    ExceptionCode ec = 0;

    JSValue* headers = jsStringOrUndefined(request->impl()->getAllResponseHeaders(ec));
    setDOMException(exec, ec);
    return headers;
}

JSValue* JSXMLHttpRequestPrototypeFunctionGetResponseHeader::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);
    ExceptionCode ec = 0;

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* header = jsStringOrNull(request->impl()->getResponseHeader(args[0]->toString(exec), ec));
    setDOMException(exec, ec);
    return header;
}


JSValue* JSXMLHttpRequestPrototypeFunctionOpen::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);
    ExceptionCode ec = 0;

    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    String method = args[0]->toString(exec);
    Frame* frame = Window::retrieveActive(exec)->impl()->frame();
    if (!frame)
        return jsUndefined();
    KURL url = frame->loader()->completeURL(DeprecatedString(args[1]->toString(exec)));

    bool async = true;
    if (args.size() >= 3)
        async = args[2]->toBoolean(exec);

    if (args.size() >= 4 && !args[3]->isUndefined()) {
        String user = valueToStringWithNullCheck(exec, args[3]);

        if (args.size() >= 5 && !args[4]->isUndefined()) {
            String password = valueToStringWithNullCheck(exec, args[4]);
            request->impl()->open(method, url, async, user, password, ec);
        } else
            request->impl()->open(method, url, async, user, ec);
    } else
        request->impl()->open(method, url, async, ec);

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSXMLHttpRequestPrototypeFunctionSend::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);
    ExceptionCode ec = 0;

    String body;

    if (args.size() >= 1) {
        if (args[0]->toObject(exec)->inherits(&JSDocument::info)) {
            Document* doc = static_cast<Document*>(static_cast<JSDocument*>(args[0]->toObject(exec))->impl());
            body = doc->toString().deprecatedString();
        } else {
            // converting certain values (like null) to object can set an exception
            if (exec->hadException())
                exec->clearException();
            else
                body = args[0]->toString(exec);
        }
    }

    request->impl()->send(body, ec);
    setDOMException(exec, ec);

    return jsUndefined();
}

JSValue* JSXMLHttpRequestPrototypeFunctionSetRequestHeader::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);
    ExceptionCode ec = 0;

    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    request->impl()->setRequestHeader(args[0]->toString(exec), args[1]->toString(exec), ec);
    setDOMException(exec, ec);
    return jsUndefined();

}

JSValue* JSXMLHttpRequestPrototypeFunctionOverrideMIMEType::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    request->impl()->overrideMIMEType(args[0]->toString(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequestPrototypeFunctionAddEventListener::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);

    Document* doc = request->impl()->document();
    if (!doc)
        return jsUndefined();
    Frame* frame = doc->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = KJS::Window::retrieveWindow(frame)->findOrCreateJSUnprotectedEventListener(args[1], true);
    if (!listener)
        return jsUndefined();
    request->impl()->addEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequestPrototypeFunctionRemoveEventListener::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);

    Document* doc = request->impl()->document();
    if (!doc)
        return jsUndefined();
    Frame* frame = doc->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = KJS::Window::retrieveWindow(frame)->findOrCreateJSUnprotectedEventListener(args[1], true);
    if (!listener)
        return jsUndefined();
    request->impl()->removeEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequestPrototypeFunctionDispatchEvent::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLHttpRequest::info))
        return throwError(exec, TypeError);

    JSXMLHttpRequest* request = static_cast<JSXMLHttpRequest*>(thisObj);
    ExceptionCode ec = 0;

    bool result = request->impl()->dispatchEvent(toEvent(args[0]), ec);
    setDOMException(exec, ec);
    return jsBoolean(result);
}

} // end namespace
