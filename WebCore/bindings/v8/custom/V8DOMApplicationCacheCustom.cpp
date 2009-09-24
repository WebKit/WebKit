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
#include "DOMApplicationCache.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCacheHost.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Document.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

static const bool kFindOnly = true;
static const bool kFindOrCreate = false;

static PassRefPtr<EventListener> argumentToEventListener(DOMApplicationCache* appcache, v8::Local<v8::Value> value, bool findOnly)
{
    V8Proxy* proxy = V8Proxy::retrieve(appcache->scriptExecutionContext());
    if (proxy)
        return findOnly ? proxy->objectListeners()->findWrapper(value, false)
                        : proxy->objectListeners()->findOrCreateWrapper<V8ObjectEventListener>(proxy->frame(), value, false);
    return 0;
}

static v8::Local<v8::Object> eventListenerToV8Object(EventListener* listener)
{
    return (static_cast<V8ObjectEventListener*>(listener))->getListenerObject();
}

static inline String toEventID(v8::Local<v8::String> value)
{
    String key = toWebCoreString(value);
    ASSERT(key.startsWith("on"));
    return key.substring(2);
}

// Handles appcache.onfooevent attribute getting
ACCESSOR_GETTER(DOMApplicationCacheEventHandler)
{
    INC_STATS("DOMApplicationCache.onevent_getter");
    DOMApplicationCache* appcache = V8DOMWrapper::convertToNativeObject<DOMApplicationCache>(V8ClassIndex::DOMAPPLICATIONCACHE, info.Holder());
    if (EventListener* listener = appcache->getAttributeEventListener(toEventID(name)))
        return eventListenerToV8Object(listener);
    return v8::Null();
}

// Handles appcache.onfooevent attribute setting
ACCESSOR_SETTER(DOMApplicationCacheEventHandler)
{
    INC_STATS("DOMApplicationCache.onevent_setter");
    DOMApplicationCache* appcache = V8DOMWrapper::convertToNativeObject<DOMApplicationCache>(V8ClassIndex::DOMAPPLICATIONCACHE, info.Holder());
    String eventType = toEventID(name);

    if (EventListener* oldListener = appcache->getAttributeEventListener(eventType)) {
        v8::Local<v8::Object> object = eventListenerToV8Object(oldListener);
        removeHiddenDependency(info.Holder(), object, V8Custom::kDOMApplicationCacheCacheIndex);
        appcache->clearAttributeEventListener(eventType);
    }

    if (value->IsFunction()) {
        RefPtr<EventListener> newListener = argumentToEventListener(appcache, value, kFindOrCreate);
        if (newListener) {
            createHiddenDependency(info.Holder(), value, V8Custom::kDOMApplicationCacheCacheIndex);
            appcache->setAttributeEventListener(eventType, newListener);
        }
    }
}

// Handles appcache.addEventListner(name, func, capture) method calls
CALLBACK_FUNC_DECL(DOMApplicationCacheAddEventListener)
{
    INC_STATS("DOMApplicationCache.addEventListener()");
    DOMApplicationCache* appcache = V8DOMWrapper::convertToNativeObject<DOMApplicationCache>(V8ClassIndex::DOMAPPLICATIONCACHE, args.Holder());

    RefPtr<EventListener> listener = argumentToEventListener(appcache, args[1], kFindOrCreate);
    if (listener) {
        createHiddenDependency(args.Holder(), args[1], V8Custom::kDOMApplicationCacheCacheIndex);
        String eventType = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        appcache->addEventListener(eventType, listener, useCapture);
    }
    return v8::Undefined();
}

// Handles appcache.removeEventListner(name, func, capture) method calls
CALLBACK_FUNC_DECL(DOMApplicationCacheRemoveEventListener)
{
    INC_STATS("DOMApplicationCache.removeEventListener()");
    DOMApplicationCache* appcache = V8DOMWrapper::convertToNativeObject<DOMApplicationCache>(V8ClassIndex::DOMAPPLICATIONCACHE, args.Holder());

    RefPtr<EventListener> listener = argumentToEventListener(appcache, args[1], kFindOnly);
    if (listener) {
        removeHiddenDependency(args.Holder(), args[1], V8Custom::kDOMApplicationCacheCacheIndex);
        String eventType = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        appcache->removeEventListener(eventType, listener.get(), useCapture);
    }
    return v8::Undefined();
}

} // namespace WebCore

#endif  // ENABLE(OFFLINE_WEB_APPLICATIONS)
