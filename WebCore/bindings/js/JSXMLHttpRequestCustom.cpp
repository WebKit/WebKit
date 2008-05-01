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

#include "XMLHttpRequest.h"

#include "DOMWindow.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSDOMWindow.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "kjs_events.h"

using namespace KJS;

namespace WebCore {

void JSXMLHttpRequest::mark()
{
    Base::mark();

    if (JSUnprotectedEventListener* onReadyStateChangeListener = static_cast<JSUnprotectedEventListener*>(m_impl->onReadyStateChangeListener()))
        onReadyStateChangeListener->mark();

    if (JSUnprotectedEventListener* onLoadListener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadListener()))
        onLoadListener->mark();
    
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
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onReadyStateChangeListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSXMLHttpRequest::setOnreadystatechange(ExecState*, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnReadyStateChangeListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(value, true));
    }   
}

JSValue* JSXMLHttpRequest::onload(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onLoadListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSXMLHttpRequest::setOnload(ExecState*, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnLoadListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(value, true));
    }
}

JSValue* JSXMLHttpRequest::onprogress(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onProgressListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSXMLHttpRequest::setOnprogress(ExecState*, JSValue* value)
{
    if (Document* document = impl()->document()) {
        if (Frame* frame = document->frame())
            impl()->setOnProgressListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(value, true));
    }
}

// Custom functions
JSValue* JSXMLHttpRequest::open(ExecState* exec, const List& args)
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

JSValue* JSXMLHttpRequest::setRequestHeader(ExecState* exec, const List& args)
{
    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    ExceptionCode ec = 0;
    impl()->setRequestHeader(args[0]->toString(exec), args[1]->toString(exec), ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::send(ExecState* exec, const List& args)
{
    String body;
    if (args.size() >= 1) {
        if (args[0]->toObject(exec)->inherits(&JSDocument::s_info))
            body = static_cast<Document*>(static_cast<JSDocument*>(args[0]->toObject(exec))->impl())->toString();
        else {
            // converting certain values (like null) to object can set an exception
            if (exec->hadException())
                exec->clearException();
            else
                body = args[0]->toString(exec);
        }
    }

    ExceptionCode ec = 0;
    impl()->send(body, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::getResponseHeader(ExecState* exec, const List& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    ExceptionCode ec = 0;
    JSValue* header = jsStringOrNull(impl()->getResponseHeader(args[0]->toString(exec), ec));
    setDOMException(exec, ec);
    return header;
}

JSValue* JSXMLHttpRequest::overrideMimeType(ExecState* exec, const List& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    impl()->overrideMimeType(args[0]->toString(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::addEventListener(ExecState* exec, const List& args)
{
    Document* document = impl()->document();
    if (!document)
        return jsUndefined();
    Frame* frame = document->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(args[1], true);
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::removeEventListener(ExecState* exec, const List& args)
{
    Document* document = impl()->document();
    if (!document)
        return jsUndefined();
    Frame* frame = document->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(args[1], true);
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequest::dispatchEvent(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    bool result = impl()->dispatchEvent(toEvent(args[0]), ec);
    setDOMException(exec, ec);
    return jsBoolean(result);    
}

} // namespace WebCore
