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

using namespace JSC;

namespace WebCore {

void JSDOMApplicationCache::mark()
{
    DOMObject::mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onchecking()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onerror()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onnoupdate()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->ondownloading()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onprogress()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onupdateready()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->oncached()))
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

JSValue* JSDOMApplicationCache::add(ExecState* exec, const ArgList& args)
{
    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!frame)
        return jsUndefined();
    const KURL& url = frame->loader()->completeURL(args.at(exec, 0)->toString(exec));
    
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
    const KURL& url = frame->loader()->completeURL(args.at(exec, 0)->toString(exec));
    
    ExceptionCode ec = 0;
    impl()->remove(url, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSDOMApplicationCache::addEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->associatedFrame();
    if (!frame)
        return jsUndefined();
    RefPtr<JSUnprotectedEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, args.at(exec, 1), true);
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args.at(exec, 0)->toString(exec), listener.release(), args.at(exec, 2)->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSDOMApplicationCache::removeEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->associatedFrame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = toJSDOMWindow(frame)->findJSUnprotectedEventListener(exec, args.at(exec, 1), true);
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args.at(exec, 0)->toString(exec), listener, args.at(exec, 2)->toBoolean(exec));
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
