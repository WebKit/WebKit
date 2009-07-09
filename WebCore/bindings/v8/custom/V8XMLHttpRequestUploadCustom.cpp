/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "XMLHttpRequestUpload.h"

#include "ExceptionCode.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "XMLHttpRequest.h"

#include <wtf/Assertions.h>

namespace WebCore {

ACCESSOR_GETTER(XMLHttpRequestUploadOnabort)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onabort._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onabort()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onabort());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnabort)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onabort._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onabort()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onabort());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kXMLHttpRequestCacheIndex);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnabort(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->findOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnabort(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kXMLHttpRequestCacheIndex);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnerror)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onerror._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onerror()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onerror());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnerror)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onerror._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onerror()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onerror());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kXMLHttpRequestCacheIndex);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnerror(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->findOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnerror(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kXMLHttpRequestCacheIndex);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnload)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onload._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onload()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onload());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnload)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onload._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onload()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onload());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kXMLHttpRequestCacheIndex);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnload(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->findOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnload(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kXMLHttpRequestCacheIndex);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnloadstart)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onloadstart._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onloadstart()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onloadstart());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnloadstart)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onloadstart._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onloadstart()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onloadstart());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kXMLHttpRequestCacheIndex);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnloadstart(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->findOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnloadstart(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kXMLHttpRequestCacheIndex);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnprogress)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onprogress._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onprogress()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onprogress());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Null();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnprogress)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onprogress._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onprogress()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onprogress());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kXMLHttpRequestCacheIndex);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnprogress(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->findOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnprogress(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kXMLHttpRequestCacheIndex);
        }
    }
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadAddEventListener)
{
    INC_STATS("DOM.XMLHttpRequestUpload.addEventListener()");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, args.Holder());

    XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = proxy->findOrCreateObjectEventListener(args[1], false);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        xmlHttpRequestUpload->addEventListener(type, listener, useCapture);

        createHiddenDependency(args.Holder(), args[1], V8Custom::kXMLHttpRequestCacheIndex);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadRemoveEventListener)
{
    INC_STATS("DOM.XMLHttpRequestUpload.removeEventListener()");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8DOMWrapper::convertToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, args.Holder());

    XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined(); // Probably leaked.

    RefPtr<EventListener> listener = proxy->findObjectEventListener(args[1], false);

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        xmlHttpRequestUpload->removeEventListener(type, listener.get(), useCapture);

        removeHiddenDependency(args.Holder(), args[1], V8Custom::kXMLHttpRequestCacheIndex);
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadDispatchEvent)
{
    INC_STATS("DOM.XMLHttpRequestUpload.dispatchEvent()");
    return throwError(NOT_SUPPORTED_ERR);
}

} // namespace WebCore
