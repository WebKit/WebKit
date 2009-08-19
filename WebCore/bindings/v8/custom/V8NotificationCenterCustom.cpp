/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#if ENABLE(NOTIFICATIONS)

#include "NotImplemented.h"
#include "Notification.h"
#include "NotificationCenter.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8CustomVoidCallback.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(NotificationAddEventListener)
{
    INC_STATS("DOM.Notification.addEventListener()");
    Notification* notification = V8DOMWrapper::convertToNativeObject<Notification>(V8ClassIndex::NOTIFICATION, args.Holder());

    RefPtr<EventListener> listener;
    ScriptExecutionContext* context = notification->scriptExecutionContext();
    if (context->isWorkerContext())
        listener = static_cast<WorkerContext*>(context)->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(args[1]), false, false);
    else {
        V8Proxy* proxy = V8Proxy::retrieve(context);
        if (!proxy)
            return v8::Undefined();
        listener = proxy->eventListeners()->findOrCreateWrapper<V8EventListener>(proxy->frame(), args[1], true);
    }

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        notification->addEventListener(type, listener, useCapture);
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(NotificationRemoveEventListener)
{
    INC_STATS("DOM.Notification.removeEventListener()");
    Notification* notification = V8DOMWrapper::convertToNativeObject<Notification>(V8ClassIndex::NOTIFICATION, args.Holder());

    RefPtr<EventListener> listener;
    ScriptExecutionContext* context = notification->scriptExecutionContext();
    if (context->isWorkerContext())
        listener = static_cast<WorkerContext*>(context)->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(args[1]), false, true);
    else {
        V8Proxy* proxy = V8Proxy::retrieve(context);
        if (!proxy)
            return v8::Undefined();
        RefPtr<EventListener> listener = proxy->eventListeners()->findWrapper(args[1], false);
    }

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        notification->removeEventListener(type, listener.get(), useCapture);
    }

    return v8::Undefined();
}

ACCESSOR_SETTER(NotificationEventHandler)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::NOTIFICATION, info.This());
    if (holder.IsEmpty())
        return;

    Notification* notification = V8DOMWrapper::convertToNativeObject<Notification>(V8ClassIndex::NOTIFICATION, holder);
    ScriptExecutionContext* context = notification->scriptExecutionContext();

    if (!context)
        return;

    String key = toWebCoreString(name);
    ASSERT(key.startsWith("on"));
    String eventType = key.substring(2);

    if (value->IsNull()) {
        // Clear the event listener
        notification->clearAttributeEventListener(eventType);
    } else {
        RefPtr<EventListener> listener;
        if (context->isWorkerContext())
            listener = static_cast<WorkerContext*>(context)->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(value), false, false);
        else {
            V8Proxy* proxy = V8Proxy::retrieve(context);
            if (!proxy)
                return;
            listener = proxy->eventListeners()->findOrCreateWrapper<V8EventListener>(proxy->frame(), value, true);
        }

        if (listener)
            notification->setAttributeEventListener(eventType, listener);
    }
}

ACCESSOR_GETTER(NotificationEventHandler)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return v8::Undefined();

    Notification* notification = V8DOMWrapper::convertToNativeObject<Notification>(V8ClassIndex::NOTIFICATION, holder);

    String key = toWebCoreString(name);
    ASSERT(key.startsWith("on"));
    String eventType = key.substring(2);

    EventListener* listener = notification->getAttributeEventListener(eventType);
    return V8DOMWrapper::convertEventListenerToV8Object(listener);
}

CALLBACK_FUNC_DECL(NotificationCenterCreateHTMLNotification)
{
    INC_STATS(L"DOM.NotificationCenter.CreateHTMLNotification()");
    NotificationCenter* notificationCenter = V8DOMWrapper::convertToNativeObject<NotificationCenter>(V8ClassIndex::NOTIFICATIONCENTER, args.Holder());
    ScriptExecutionContext* context = notificationCenter->context();
    ExceptionCode ec = 0;
    String url = toWebCoreString(args[0]);
    RefPtr<Notification> notification = Notification::create(url, context, ec, notificationCenter->presenter());

    if (ec)
        return throwError(ec);

    if (context->isWorkerContext())
        return WorkerContextExecutionProxy::convertToV8Object(V8ClassIndex::NOTIFICATION, notification.get());

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::NOTIFICATION, notification.get());
}

CALLBACK_FUNC_DECL(NotificationCenterCreateNotification)
{
    INC_STATS(L"DOM.NotificationCenter.CreateNotification()");
    NotificationCenter* notificationCenter = V8DOMWrapper::convertToNativeObject<NotificationCenter>(V8ClassIndex::NOTIFICATIONCENTER, args.Holder());
    NotificationContents contents(toWebCoreString(args[0]), toWebCoreString(args[1]), toWebCoreString(args[2]));

    ScriptExecutionContext* context = notificationCenter->context();
    ExceptionCode ec = 0;
    RefPtr<Notification> notification = Notification::create(contents, context, ec, notificationCenter->presenter());

    if (ec)
        return throwError(ec);

    if (context->isWorkerContext())
        return WorkerContextExecutionProxy::convertToV8Object(V8ClassIndex::NOTIFICATION, notification.get());

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::NOTIFICATION, notification.get());
}

CALLBACK_FUNC_DECL(NotificationCenterRequestPermission)
{
    INC_STATS(L"DOM.NotificationCenter.RequestPermission()");
    NotificationCenter* notificationCenter = V8DOMWrapper::convertToNativeObject<NotificationCenter>(V8ClassIndex::NOTIFICATIONCENTER, args.Holder());
    ScriptExecutionContext* context = notificationCenter->context();

    // Requesting permission is only valid from a page context.
    if (context->isWorkerContext())
        return throwError(NOT_SUPPORTED_ERR);

    RefPtr<V8CustomVoidCallback> callback;
    if (args.Length() > 0) {
        if (!args[0]->IsObject())
            return throwError("Callback must be of valid type.", V8Proxy::TypeError);
 
        callback = V8CustomVoidCallback::create(args[0], V8Proxy::retrieveFrameForCurrentContext());
    }

    notificationCenter->requestPermission(callback.release());
    return v8::Undefined();
}


}  // namespace WebCore

#endif  // ENABLE(NOTIFICATIONS)
