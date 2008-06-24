/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSXMLHttpRequest.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSDOMWindowCustom.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "XMLHttpRequest.h"

using namespace KJS;

namespace WebCore {

void JSXMLHttpRequest::mark()
{
    Base::mark();

    if (JSUnprotectedEventListener* onReadyStateChangeListener = static_cast<JSUnprotectedEventListener*>(m_impl->onReadyStateChangeListener()))
        onReadyStateChangeListener->mark();

    if (JSUnprotectedEventListener* onAbortListener = static_cast<JSUnprotectedEventListener*>(m_impl->onAbortListener()))
        onAbortListener->mark();

    if (JSUnprotectedEventListener* onErrorListener = static_cast<JSUnprotectedEventListener*>(m_impl->onErrorListener()))
        onErrorListener->mark();

    if (JSUnprotectedEventListener* onLoadListener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadListener()))
        onLoadListener->mark();

    if (JSUnprotectedEventListener* onLoadStartListener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadStartListener()))
        onLoadStartListener->mark();
    
    if (JSUnprotectedEventListener* onProgressListener = static_cast<JSUnprotectedEventListener*>(m_impl->onProgressListener()))
        onProgressListener->mark();
    
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

JSValue* JSXMLHttpRequest::onreadystatechange(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onReadyStateChangeListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequest::setOnreadystatechange(ExecState* exec, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnReadyStateChangeListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
    }   
}


JSValue* JSXMLHttpRequest::onabort(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onAbortListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequest::setOnabort(ExecState* exec, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnAbortListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
    }
}

JSValue* JSXMLHttpRequest::onerror(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onErrorListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequest::setOnerror(ExecState* exec, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnErrorListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
    }
}

JSValue* JSXMLHttpRequest::onload(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onLoadListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequest::setOnload(ExecState* exec, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnLoadListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
    }
}

JSValue* JSXMLHttpRequest::onloadstart(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onLoadStartListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequest::setOnloadstart(ExecState* exec, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnLoadStartListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
    }
}

JSValue* JSXMLHttpRequest::onprogress(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onProgressListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequest::setOnprogress(ExecState* exec, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnProgressListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
    }
}

// Custom functions
JSValue* JSXMLHttpRequest::open(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!frame)
        return jsUndefined();
    const KURL& url = frame->loader()->completeURL(args[1]->toString(exec));

    ExceptionCode ec = 0;

    String method = args[0]->toString(exec);
    bool async = true;
    if (args.size() >= 3)
        async = args[2]->toBoolean(exec);

    if (args.size() >= 4 && !args[3]->isUndefined()) {
        String user = valueToStringWithNullCheck(exec, args[3]);

        if (args.size() >= 5 && !args[4]->isUndefined()) {
            String password = valueToStringWithNullCheck(exec, args[4]);
            impl()->open(method, url, async, user, password, ec);
        } else
            impl()->open(method, url, async, user, ec);
    } else
        impl()->open(method, url, async, ec);

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::setRequestHeader(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    ExceptionCode ec = 0;
    impl()->setRequestHeader(args[0]->toString(exec), args[1]->toString(exec), ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::send(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    if (args.isEmpty())
        impl()->send(ec);
    else {
        JSValue* val = args[0];
        if (val->isUndefinedOrNull())
            impl()->send(ec);
        else if (val->isObject(&JSDocument::s_info))
            impl()->send(toDocument(val), ec);
        else
            impl()->send(val->toString(exec), ec);
    }

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::getResponseHeader(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    ExceptionCode ec = 0;
    JSValue* header = jsStringOrNull(exec, impl()->getResponseHeader(args[0]->toString(exec), ec));
    setDOMException(exec, ec);
    return header;
}

JSValue* JSXMLHttpRequest::overrideMimeType(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    impl()->overrideMimeType(args[0]->toString(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::addEventListener(ExecState* exec, const ArgList& args)
{
    Document* document = impl()->document();
    if (!document)
        return jsUndefined();
    Frame* frame = document->frame();
    if (!frame)
        return jsUndefined();
    RefPtr<JSUnprotectedEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, args[1], true);
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args[0]->toString(exec), listener.release(), args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::removeEventListener(ExecState* exec, const ArgList& args)
{
    Document* document = impl()->document();
    if (!document)
        return jsUndefined();
    Frame* frame = document->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = toJSDOMWindow(frame)->findJSUnprotectedEventListener(exec, args[1], true);
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::dispatchEvent(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    bool result = impl()->dispatchEvent(toEvent(args[0]), ec);
    setDOMException(exec, ec);
    return jsBoolean(result);    
}

} // namespace WebCore
