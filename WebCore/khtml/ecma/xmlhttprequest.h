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

#ifndef _XMLHTTPREQUEST_H_
#define _XMLHTTPREQUEST_H_

#include "ecma/kjs_binding.h"
#include "ecma/kjs_dom.h"
#include "misc/loader.h"

namespace KJS {

  class XMLHttpRequestConstructorImp : public ObjectImp {
  public:
    XMLHttpRequestConstructorImp(ExecState *exec, const DOM::Document &d);
    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);
  private:
    DOM::Document doc;
  };

  class XMLHttpRequest : public DOMObject, public khtml::CachedObjectClient {
  public:
    XMLHttpRequest(ExecState *, const DOM::Document &d);
    ~XMLHttpRequest();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int /*attr*/);
    void notifyFinished(khtml::CachedObject *);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Onreadystatechange, ReadyState, ResponseText, ResponseXML, Status, StatusText, Abort, GetAllResponseHeaders, GetResponseHeader, Open, Send, SetRequestHeader };
  private:
  };

}; // namespace

#endif
