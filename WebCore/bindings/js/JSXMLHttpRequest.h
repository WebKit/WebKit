/*
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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

class JSXMLHttpRequestConstructorImp : public DOMObject {
public:
    JSXMLHttpRequestConstructorImp(KJS::ExecState*, Document*);

    virtual bool implementsConstruct() const;
    virtual KJS::JSObject* construct(KJS::ExecState*, const KJS::List&);

private:
    RefPtr<Document> doc;
};

class JSXMLHttpRequest : public DOMObject {
public:
    JSXMLHttpRequest(KJS::JSObject* prototype, Document*);
    ~JSXMLHttpRequest();

    virtual const KJS::ClassInfo* classInfo() const { return &info; }
    static const KJS::ClassInfo info;
    enum { Onload, Onreadystatechange, ReadyState, ResponseText, ResponseXML, Status,
        StatusText, Abort, GetAllResponseHeaders, GetResponseHeader, Open, Send, SetRequestHeader, OverrideMIMEType,
        AddEventListener, RemoveEventListener, DispatchEvent };

    virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
    KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
    virtual void put(KJS::ExecState*, const KJS::Identifier& propertyName, KJS::JSValue*);
    void putValueProperty(KJS::ExecState*, int token, KJS::JSValue*);
    virtual bool toBoolean(KJS::ExecState*) const { return true; }
    virtual void mark();

    XMLHttpRequest* impl() const { return m_impl.get(); }

private:
    RefPtr<XMLHttpRequest> m_impl;
};

KJS::JSValue* jsXMLHttpRequestPrototypeFunctionAbort(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionGetAllResponseHeaders(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionGetResponseHeader(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionOpen(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionSend(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionSetRequestHeader(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionOverrideMIMEType(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionAddEventListener(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionRemoveEventListener(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXMLHttpRequestPrototypeFunctionDispatchEvent(KJS::ExecState*, KJS::JSObject*, const KJS::List&);

} // namespace WebCore

#endif // JSXMLHttpRequest_h
