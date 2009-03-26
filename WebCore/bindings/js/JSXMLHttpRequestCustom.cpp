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
#include "File.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSDOMWindowCustom.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSFile.h"
#include "XMLHttpRequest.h"
#include <runtime/Error.h>
#include <interpreter/Interpreter.h>

using namespace JSC;

namespace WebCore {

void JSXMLHttpRequest::mark()
{
    Base::mark();

    if (XMLHttpRequestUpload* upload = m_impl->optionalUpload()) {
        DOMObject* wrapper = getCachedDOMObjectWrapper(*Heap::heap(this)->globalData(), upload);
        if (wrapper && !wrapper->marked())
            wrapper->mark();
    }

    if (JSEventListener* onReadyStateChangeListener = static_cast<JSEventListener*>(m_impl->onreadystatechange()))
        onReadyStateChangeListener->mark();

    if (JSEventListener* onAbortListener = static_cast<JSEventListener*>(m_impl->onabort()))
        onAbortListener->mark();

    if (JSEventListener* onErrorListener = static_cast<JSEventListener*>(m_impl->onerror()))
        onErrorListener->mark();

    if (JSEventListener* onLoadListener = static_cast<JSEventListener*>(m_impl->onload()))
        onLoadListener->mark();

    if (JSEventListener* onLoadStartListener = static_cast<JSEventListener*>(m_impl->onloadstart()))
        onLoadStartListener->mark();
    
    if (JSEventListener* onProgressListener = static_cast<JSEventListener*>(m_impl->onprogress()))
        onProgressListener->mark();
    
    typedef XMLHttpRequest::EventListenersMap EventListenersMap;
    typedef XMLHttpRequest::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = m_impl->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter) {
            JSEventListener* listener = static_cast<JSEventListener*>(vecIter->get());
            listener->mark();
        }
    }
}

// Custom functions
JSValuePtr JSXMLHttpRequest::open(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    const KURL& url = impl()->scriptExecutionContext()->completeURL(args.at(exec, 1).toString(exec));
    String method = args.at(exec, 0).toString(exec);
    bool async = true;
    if (args.size() >= 3)
        async = args.at(exec, 2).toBoolean(exec);

    ExceptionCode ec = 0;
    if (args.size() >= 4 && !args.at(exec, 3).isUndefined()) {
        String user = valueToStringWithNullCheck(exec, args.at(exec, 3));

        if (args.size() >= 5 && !args.at(exec, 4).isUndefined()) {
            String password = valueToStringWithNullCheck(exec, args.at(exec, 4));
            impl()->open(method, url, async, user, password, ec);
        } else
            impl()->open(method, url, async, user, ec);
    } else
        impl()->open(method, url, async, ec);

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValuePtr JSXMLHttpRequest::setRequestHeader(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return throwError(exec, SyntaxError, "Not enough arguments");

    ExceptionCode ec = 0;
    impl()->setRequestHeader(args.at(exec, 0).toString(exec), args.at(exec, 1).toString(exec), ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValuePtr JSXMLHttpRequest::send(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    if (args.isEmpty())
        impl()->send(ec);
    else {
        JSValuePtr val = args.at(exec, 0);
        if (val.isUndefinedOrNull())
            impl()->send(ec);
        else if (val.isObject(&JSDocument::s_info))
            impl()->send(toDocument(val), ec);
        else if (val.isObject(&JSFile::s_info))
            impl()->send(toFile(val), ec);
        else
            impl()->send(val.toString(exec), ec);
    }

    int signedLineNumber;
    intptr_t sourceID;
    UString sourceURL;
    JSValuePtr function;
    exec->interpreter()->retrieveLastCaller(exec, signedLineNumber, sourceID, sourceURL, function);
    impl()->setLastSendLineNumber(signedLineNumber >= 0 ? signedLineNumber : 0);
    impl()->setLastSendURL(sourceURL);

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValuePtr JSXMLHttpRequest::getResponseHeader(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    ExceptionCode ec = 0;
    JSValuePtr header = jsStringOrNull(exec, impl()->getResponseHeader(args.at(exec, 0).toString(exec), ec));
    setDOMException(exec, ec);
    return header;
}

JSValuePtr JSXMLHttpRequest::overrideMimeType(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    impl()->overrideMimeType(args.at(exec, 0).toString(exec));
    return jsUndefined();
}

JSValuePtr JSXMLHttpRequest::addEventListener(ExecState* exec, const ArgList& args)
{
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(impl()->scriptExecutionContext());
    if (!globalObject)
        return jsUndefined();
    RefPtr<JSEventListener> listener = globalObject->findOrCreateJSEventListener(exec, args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args.at(exec, 0).toString(exec), listener.release(), args.at(exec, 2).toBoolean(exec));
    return jsUndefined();
}

JSValuePtr JSXMLHttpRequest::removeEventListener(ExecState* exec, const ArgList& args)
{
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(impl()->scriptExecutionContext());
    if (!globalObject)
        return jsUndefined();
    JSEventListener* listener = globalObject->findJSEventListener(exec, args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args.at(exec, 0).toString(exec), listener, args.at(exec, 2).toBoolean(exec));
    return jsUndefined();
}

JSValuePtr JSXMLHttpRequest::responseText(ExecState* exec) const
{
    return jsOwnedStringOrNull(exec, impl()->responseText());
}

} // namespace WebCore
