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
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

// Handles appcache.addEventListner(name, func, capture) method calls
CALLBACK_FUNC_DECL(DOMApplicationCacheAddEventListener)
{
    INC_STATS("DOMApplicationCache.addEventListener()");
    DOMApplicationCache* appcache = V8DOMWrapper::convertToNativeObject<DOMApplicationCache>(V8ClassIndex::DOMAPPLICATIONCACHE, args.Holder());

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(appcache, args[1], false, ListenerFindOrCreate);
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

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(appcache, args[1], false, ListenerFindOnly);
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
