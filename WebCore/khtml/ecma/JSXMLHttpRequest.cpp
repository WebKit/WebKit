/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "JSXMLHttpRequest.h"

#include "Frame.h"
#include "PlatformString.h"
#include "html_documentimpl.h"
#include "kjs_events.h"
#include "kjs_window.h"
#include "xmlhttprequest.h"

#include "JSXMLHttpRequest.lut.h"

using namespace WebCore;

namespace KJS {

////////////////////// JSXMLHttpRequest Object ////////////////////////

/* Source for JSXMLHttpRequestProtoTable.
@begin JSXMLHttpRequestProtoTable 7
  abort                 JSXMLHttpRequest::Abort                   DontDelete|Function 0
  getAllResponseHeaders JSXMLHttpRequest::GetAllResponseHeaders   DontDelete|Function 0
  getResponseHeader     JSXMLHttpRequest::GetResponseHeader       DontDelete|Function 1
  open                  JSXMLHttpRequest::Open                    DontDelete|Function 5
  overrideMimeType      JSXMLHttpRequest::OverrideMIMEType        DontDelete|Function 1
  send                  JSXMLHttpRequest::Send                    DontDelete|Function 1
  setRequestHeader      JSXMLHttpRequest::SetRequestHeader        DontDelete|Function 2
@end
*/
KJS_DEFINE_PROTOTYPE(JSXMLHttpRequestProto)
KJS_IMPLEMENT_PROTOFUNC(JSXMLHttpRequestProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("JSXMLHttpRequest", JSXMLHttpRequestProto, JSXMLHttpRequestProtoFunc)

JSXMLHttpRequestConstructorImp::JSXMLHttpRequestConstructorImp(ExecState *exec, DocumentImpl *d)
    : doc(d)
{
    putDirect(prototypePropertyName, JSXMLHttpRequestProto::self(exec), DontEnum|DontDelete|ReadOnly);
}

JSXMLHttpRequestConstructorImp::~JSXMLHttpRequestConstructorImp()
{
}

bool JSXMLHttpRequestConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject *JSXMLHttpRequestConstructorImp::construct(ExecState *exec, const List &)
{
  return new JSXMLHttpRequest(exec, doc.get());
}

const ClassInfo JSXMLHttpRequest::info = { "JSXMLHttpRequest", 0, &JSXMLHttpRequestTable, 0 };

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

bool JSXMLHttpRequest::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<JSXMLHttpRequest, DOMObject>(exec, &JSXMLHttpRequestTable, this, propertyName, slot);
}

JSValue* JSXMLHttpRequest::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case ReadyState:
    return jsNumber(m_impl->getReadyState());
  case ResponseText:
    return jsStringOrNull(m_impl->getResponseText());
  case ResponseXML:
    if (DocumentImpl* responseXML = m_impl->getResponseXML())
      return getDOMNode(exec, responseXML);
    return jsUndefined();
  case Status:
    int status = m_impl->getStatus();
    return status > 0 ? jsNumber(status) : jsUndefined();
  case StatusText:
    return jsStringOrUndefined(m_impl->getStatusText());
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

void JSXMLHttpRequest::put(ExecState *exec, const Identifier &propertyName, JSValue* value, int attr)
{
  lookupPut<JSXMLHttpRequest,DOMObject>(exec, propertyName, value, attr, &JSXMLHttpRequestTable, this );
}

void JSXMLHttpRequest::putValueProperty(ExecState *exec, int token, JSValue* value, int /*attr*/)
{
  switch(token) {
  case Onreadystatechange:
    m_impl->setOnReadyStateChangeListener(Window::retrieveActive(exec)->getJSUnprotectedEventListener(value, true));
    break;
  case Onload:
    m_impl->setOnLoadListener(Window::retrieveActive(exec)->getJSUnprotectedEventListener(value, true));
    break;
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
}


JSXMLHttpRequest::JSXMLHttpRequest(ExecState *exec, DocumentImpl *d)
  : m_impl(new XMLHttpRequest(d))
{
  setPrototype(JSXMLHttpRequestProto::self(exec));
  ScriptInterpreter::putDOMObject(m_impl.get(), this);
}

JSXMLHttpRequest::~JSXMLHttpRequest()
{
  m_impl->setOnReadyStateChangeListener(0);
  m_impl->setOnLoadListener(0);
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* JSXMLHttpRequestProtoFunc::callAsFunction(ExecState *exec, JSObject* thisObj, const List& args)
{
  if (!thisObj->inherits(&JSXMLHttpRequest::info))
    return throwError(exec, TypeError);

  JSXMLHttpRequest *request = static_cast<JSXMLHttpRequest *>(thisObj);

  switch (id) {
  case JSXMLHttpRequest::Abort:
    request->m_impl->abort();
    return jsUndefined();
  case JSXMLHttpRequest::GetAllResponseHeaders:
    return jsStringOrUndefined(request->m_impl->getAllResponseHeaders());
  case JSXMLHttpRequest::GetResponseHeader:
    if (args.size() != 1)
      return jsUndefined();
    return jsStringOrUndefined(request->m_impl->getResponseHeader(args[0]->toString(exec).domString()));
  case JSXMLHttpRequest::Open:
    {
      if (args.size() < 2 || args.size() > 5)
        return jsUndefined();
    
      DOMString method = args[0]->toString(exec).domString();
      KURL url = KURL(Window::retrieveActive(exec)->frame()->document()->completeURL(args[1]->toString(exec).qstring()));

      bool async = true;
      if (args.size() >= 3)
        async = args[2]->toBoolean(exec);
    
      DOMString user;
      if (args.size() >= 4)
        user = args[3]->toString(exec).domString();
      
      DOMString password;
      if (args.size() >= 5)
        password = args[4]->toString(exec).domString();

      request->m_impl->open(method, url, async, user, password);

      return jsUndefined();
    }
  case JSXMLHttpRequest::Send:
    {
      if (args.size() > 1)
        return jsUndefined();

      DOMString body;

      if (args.size() >= 1) {
        if (args[0]->toObject(exec)->inherits(&DOMDocument::info)) {
          DocumentImpl *doc = static_cast<DocumentImpl *>(static_cast<DOMDocument *>(args[0]->toObject(exec))->impl());
          body = doc->toString().qstring();
        } else {
          // converting certain values (like null) to object can set an exception
          exec->clearException();
          body = args[0]->toString(exec).domString();
        }
      }

      request->m_impl->send(body);

      return jsUndefined();
    }
  case JSXMLHttpRequest::SetRequestHeader:
    if (args.size() != 2)
      return jsUndefined();
    request->m_impl->setRequestHeader(args[0]->toString(exec).domString(), args[1]->toString(exec).domString());
    return jsUndefined();
  case JSXMLHttpRequest::OverrideMIMEType:
    if (args.size() != 1)
      return jsUndefined();
    request->m_impl->overrideMIMEType(args[0]->toString(exec).domString());
    return jsUndefined();
  }

  return jsUndefined();
}

} // end namespace
