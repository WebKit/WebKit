/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSDOMApplicationCache.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "AtomicString.h"
#include "DOMApplicationCache.h"
#include "DOMWindow.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "JSDOMWindowCustom.h"
#include "JSEvent.h"
#include "JSEventListener.h"

using namespace KJS;

namespace WebCore {
    
JSValue* JSDOMApplicationCache::add(ExecState* exec, const ArgList& args)
{
    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!frame)
        return jsUndefined();
    const KURL& url = frame->loader()->completeURL(args[0]->toString(exec));
    
    ExceptionCode ec = 0;
    impl()->add(url, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}
    
JSValue* JSDOMApplicationCache::remove(ExecState* exec, const ArgList& args)
{
    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!frame)
        return jsUndefined();
    const KURL& url = frame->loader()->completeURL(args[0]->toString(exec));
    
    ExceptionCode ec = 0;
    impl()->remove(url, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}
    
JSValue* JSDOMApplicationCache::addEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    RefPtr<JSUnprotectedEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, args[1], true);
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args[0]->toString(exec), listener.release(), args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSDOMApplicationCache::removeEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = toJSDOMWindow(frame)->findJSUnprotectedEventListener(exec, args[1], true);
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));
    return jsUndefined();
    
}
    
JSValue* JSDOMApplicationCache::dispatchEvent(KJS::ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    
    bool result = impl()->dispatchEvent(toEvent(args[0]), ec);
    setDOMException(exec, ec);
    return jsBoolean(result);    
}

void JSDOMApplicationCache::setOnchecking(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnCheckingListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::onchecking(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onCheckingListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::setOnerror(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnErrorListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::onerror(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onErrorListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::setOnnoupdate(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnNoUpdateListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::onnoupdate(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onNoUpdateListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::setOndownloading(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnDownloadingListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::ondownloading(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onDownloadingListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::setOnprogress(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnProgressListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::onprogress(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onProgressListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::setOnupdateready(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnUpdateReadyListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::onupdateready(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onUpdateReadyListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::setOncached(ExecState* exec, JSValue* value)
{
    if (Frame* frame = impl()->frame())
        impl()->setOnCachedListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSDOMApplicationCache::oncached(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onCachedListener()))
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    return jsNull();
}

void JSDOMApplicationCache::mark()
{
    DOMObject::mark();
 
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onCheckingListener()))
        listener->mark();
        
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onErrorListener()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onNoUpdateListener()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onDownloadingListener()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onProgressListener()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onUpdateReadyListener()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onCachedListener()))
        listener->mark();
    
    typedef DOMApplicationCache::EventListenersMap EventListenersMap;
    typedef DOMApplicationCache::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = m_impl->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter) {
            JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(vecIter->get());
            listener->mark();
        }
    }
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
