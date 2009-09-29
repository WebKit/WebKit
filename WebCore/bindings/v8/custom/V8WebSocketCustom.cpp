/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_SOCKETS)

#include "WebSocket.h"

#include "Frame.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

static PassRefPtr<EventListener> getEventListener(WebSocket* webSocket, v8::Local<v8::Value> value, bool findOnly)
{
#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateObjectEventListener(value, false, findOnly);
#endif

    V8Proxy* proxy = V8Proxy::retrieve(webSocket->scriptExecutionContext());
    if (proxy) {
        V8EventListenerList* list = proxy->objectListeners();
        return findOnly ? list->findWrapper(value, false) : list->findOrCreateWrapper<V8ObjectEventListener>(proxy->frame(), value, false);
    }

    return PassRefPtr<EventListener>();
}

ACCESSOR_GETTER(WebSocketOnopen)
{
    INC_STATS("DOM.WebSocket.onopen._get");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, info.Holder());
    if (webSocket->onopen()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(webSocket->onopen());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(WebSocketOnopen)
{
    INC_STATS("DOM.WebSocket.onopen._set");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, info.Holder());
    if (value->IsNull()) {
        if (webSocket->onopen()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(webSocket->onopen());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kWebSocketCacheIndex);
        }
        // Clear the listener.
        webSocket->setOnopen(0);
    } else {
        RefPtr<EventListener> listener = getEventListener(webSocket, value, false);
        if (listener) {
            webSocket->setOnopen(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWebSocketCacheIndex);
        }
    }
}

ACCESSOR_GETTER(WebSocketOnmessage)
{
    INC_STATS("DOM.WebSocket.onmessage._get");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, info.Holder());
    if (webSocket->onmessage()) {
        RefPtr<V8ObjectEventListener> listener = static_cast<V8ObjectEventListener*>(webSocket->onmessage());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(WebSocketOnmessage)
{
    INC_STATS("DOM.WebSocket.onmessage._set");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, info.Holder());
    if (value->IsNull()) {
        if (webSocket->onmessage()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(webSocket->onmessage());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kWebSocketCacheIndex);
        }
        // Clear the listener.
        webSocket->setOnmessage(0);
    } else {
        RefPtr<EventListener> listener = getEventListener(webSocket, value, false);
        if (listener) {
            webSocket->setOnmessage(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWebSocketCacheIndex);
        }
    }
}

ACCESSOR_GETTER(WebSocketOnclose)
{
    INC_STATS("DOM.WebSocket.onclose._get");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, info.Holder());
    if (webSocket->onclose()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(webSocket->onclose());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(WebSocketOnclose)
{
    INC_STATS("DOM.WebSocket.onclose._set");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, info.Holder());
    if (value->IsNull()) {
        if (webSocket->onclose()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(webSocket->onclose());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kWebSocketCacheIndex);
        }
        // Clear the listener.
        webSocket->setOnclose(0);
    } else {
        RefPtr<EventListener> listener = getEventListener(webSocket, value, false);
        if (listener) {
            webSocket->setOnclose(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWebSocketCacheIndex);
        }
    }
}

// ??? AddEventListener, RemoveEventListener

CALLBACK_FUNC_DECL(WebSocketConstructor)
{
    INC_STATS("DOM.WebSocket.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object custructor cannot be called as a function.");
    if (args.Length() == 0)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    v8::TryCatch tryCatch;
    v8::Handle<v8::String> urlstring = args[0]->ToString();
    if (tryCatch.HasCaught())
        return throwError(tryCatch.Exception());
    if (urlstring.IsEmpty())
        return throwError("Empty URL", V8Proxy::SyntaxError);

    // Get the script execution context.
    ScriptExecutionContext* context = 0;
    // TODO: Workers
    if (!context) {
        Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
        if (!frame)
            return throwError("WebSocket constructor's associated frame is not available", V8Proxy::ReferenceError);
        context = frame->document();
    }

    const KURL& url = context->completeURL(toWebCoreString(urlstring));

    RefPtr<WebSocket> webSocket = WebSocket::create(context);
    ExceptionCode ec = 0;

    if (args.Length() < 2)
        webSocket->connect(url, ec);
    else {
        v8::TryCatch tryCatchProtocol;
        v8::Handle<v8::String> protocol = args[1]->ToString();
        if (tryCatchProtocol.HasCaught())
            return throwError(tryCatchProtocol.Exception());
        webSocket->connect(url, toWebCoreString(protocol), ec);
    }
    if (ec)
        return throwError(ec);

    // Setup the standard wrapper object internal fields.
    V8DOMWrapper::setDOMWrapper(args.Holder(), V8ClassIndex::ToInt(V8ClassIndex::WEBSOCKET), webSocket.get());

    // Add object to the wrapper map.
    webSocket->ref();
    V8DOMWrapper::setJSWrapperForActiveDOMObject(webSocket.get(), v8::Persistent<v8::Object>::New(args.Holder()));

    return args.Holder();
}

CALLBACK_FUNC_DECL(WebSocketSend)
{
    INC_STATS("DOM.WebSocket.send()");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, args.Holder());

    ExceptionCode ec = 0;
    bool ret = false;
    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);
    else {
        String msg = toWebCoreString(args[0]);
        ret = webSocket->send(msg, ec);
    }
    if (ec)
        return throwError(ec);
    return v8Boolean(ret);
}

CALLBACK_FUNC_DECL(WebSocketClose)
{
    INC_STATS("DOM.WebSocket.close()");
    WebSocket* webSocket = V8DOMWrapper::convertToNativeObject<WebSocket>(V8ClassIndex::WEBSOCKET, args.Holder());

    webSocket->close();
    return v8::Undefined();
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
