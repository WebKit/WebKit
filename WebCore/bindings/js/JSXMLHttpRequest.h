// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef JSXMLHttpRequest_h
#define JSXMLHttpRequest_h

#include "kjs_binding.h"

namespace WebCore {

class XMLHttpRequest;
class Document;

}

namespace KJS {

class JSXMLHttpRequestConstructorImp : public DOMObject {
public:
    JSXMLHttpRequestConstructorImp(ExecState*, WebCore::Document*);

    virtual bool implementsConstruct() const;
    virtual JSObject* construct(ExecState*, const List&);

private:
    RefPtr<WebCore::Document> doc;
};

class JSXMLHttpRequest : public DOMObject {
public:
    JSXMLHttpRequest(JSObject* prototype, WebCore::Document*);
    ~JSXMLHttpRequest();

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Onload, Onreadystatechange, ReadyState, ResponseText, ResponseXML, Status,
        StatusText, Abort, GetAllResponseHeaders, GetResponseHeader, Open, Send, SetRequestHeader, OverrideMIMEType,
        AddEventListener, RemoveEventListener, DispatchEvent };

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, int attr = None);
    void putValueProperty(ExecState*, int token, JSValue* value, int /*attr*/);
    virtual bool toBoolean(ExecState*) const { return true; }
    virtual void mark();

    WebCore::XMLHttpRequest* impl() const { return m_impl.get(); }

private:
    RefPtr<WebCore::XMLHttpRequest> m_impl;
};

JSValue* jsXMLHttpRequestPrototypeFunctionAbort(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionGetAllResponseHeaders(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionGetResponseHeader(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionOpen(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionSend(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionSetRequestHeader(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionOverrideMIMEType(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionAddEventListener(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionRemoveEventListener(ExecState*, JSObject*, const List&);
JSValue* jsXMLHttpRequestPrototypeFunctionDispatchEvent(ExecState*, JSObject*, const List&);

} // namespace KJS

#endif // JSXMLHttpRequest_h
