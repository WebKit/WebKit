// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
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

#import "xmlhttprequest.h"
#import "xmlhttprequest.lut.h"

#include <kdebug.h>

using namespace KJS;

////////////////////// XMLHttpRequest Object ////////////////////////

/* Source for XMLHttpRequestProtoTable. Use "make hashtables" to regenerate.
@begin XMLHttpRequestProtoTable 7
  abort			XMLHttpRequest::Abort			DontDelete|Function 0
  getAllResponseHeaders	XMLHttpRequest::GetAllResponseHeaders	DontDelete|Function 0
  getResponseHeader	XMLHttpRequest::GetResponseHeader	DontDelete|Function 1
  open			XMLHttpRequest::Open			DontDelete|Function 5
  send			XMLHttpRequest::Send			DontDelete|Function 1
  setRequestHeader	XMLHttpRequest::SetRequestHeader	DontDelete|Function 2
@end
*/
DEFINE_PROTOTYPE("XMLHttpRequest",XMLHttpRequestProto)
IMPLEMENT_PROTOFUNC(XMLHttpRequestProtoFunc)
IMPLEMENT_PROTOTYPE(XMLHttpRequestProto,XMLHttpRequestProtoFunc)

namespace KJS {

XMLHttpRequestConstructorImp::XMLHttpRequestConstructorImp(ExecState *, const DOM::Document &d)
    : ObjectImp(), doc(d)
{
}

bool XMLHttpRequestConstructorImp::implementsConstruct() const
{
  return true;
}

Object XMLHttpRequestConstructorImp::construct(ExecState *exec, const List &)
{
  return Object(new XMLHttpRequest(exec, doc));
}

const ClassInfo XMLHttpRequest::info = { "XMLHttpRequest", 0, &XMLHttpRequestTable, 0 };

/* Source for XMLHttpRequestTable. Use "make hashtables" to regenerate.
@begin XMLHttpRequestTable 6
  readyState		XMLHttpRequest::ReadyState		DontDelete|ReadOnly
  responseText		XMLHttpRequest::ResponseText		DontDelete|ReadOnly
  responseXML		XMLHttpRequest::ResponseXML		DontDelete|ReadOnly
  status		XMLHttpRequest::Status			DontDelete|ReadOnly
  statusText		XMLHttpRequest::StatusText		DontDelete|ReadOnly
  onreadystatechange	XMLHttpRequest::Onreadystatechange	DontDelete
@end
*/

Value XMLHttpRequest::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<XMLHttpRequest,DOMObject>(exec, propertyName, &XMLHttpRequestTable, this);
}

Value XMLHttpRequest::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case ReadyState:
    return Undefined();
  case ResponseText:
    return Undefined();
  case ResponseXML:
    return Undefined();
  case Status:
    return Undefined();
  case StatusText:
    return Undefined();
  case Onreadystatechange:
    return Null();
  default:
    kdWarning() << "XMLHttpRequest::getValueProperty unhandled token " << token << endl;
    return Value();
  }
}

void XMLHttpRequest::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  DOMObjectLookupPut<XMLHttpRequest,DOMObject>(exec, propertyName, value, attr, &XMLHttpRequestTable, this );
}

void XMLHttpRequest::putValue(ExecState *exec, int token, const Value& value, int /*attr*/)
{
  switch(token) {
  case Onreadystatechange:
    break;
  default:
    kdWarning() << "HTMLDocument::putValue unhandled token " << token << endl;
  }
}

void XMLHttpRequest::notifyFinished(khtml::CachedObject *)
{
}

XMLHttpRequest::XMLHttpRequest(ExecState *exec, const DOM::Document &d)
    : DOMObject(XMLHttpRequestProto::self(exec))
{
}

XMLHttpRequest::~XMLHttpRequest()
{
}

Value XMLHttpRequestProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&XMLHttpRequest::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  switch (id) {
  case XMLHttpRequest::Abort:
    return Undefined();
  case XMLHttpRequest::GetAllResponseHeaders:
    return Undefined();
  case XMLHttpRequest::GetResponseHeader:
    return Undefined();
  case XMLHttpRequest::Open:
    return Undefined();
  case XMLHttpRequest::Send:
    return Undefined();
  case XMLHttpRequest::SetRequestHeader:
    return Undefined();
  }

  return Undefined();
}

}; // end namespace
