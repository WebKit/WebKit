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
#include "WebStorageEventDispatcherImpl.h"

#include "KURL.h"
#include "SecurityOrigin.h"
#include "StorageAreaProxy.h"

#include "platform/WebURL.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {

extern const char* pageGroupName;

void WebStorageEventDispatcher::dispatchLocalStorageEvent(
        const WebString& key, const WebString& oldValue,
        const WebString& newValue, const WebURL& origin,
        const WebURL& pageURL, WebStorageArea* sourceAreaInstance,
        bool originatedInProcess)
{
    RefPtr<WebCore::SecurityOrigin> securityOrigin = WebCore::SecurityOrigin::create(origin);
    WebCore::StorageAreaProxy::dispatchLocalStorageEvent(
            pageGroupName, key, oldValue, newValue, securityOrigin.get(), pageURL,
            sourceAreaInstance, originatedInProcess);
}

void WebStorageEventDispatcher::dispatchSessionStorageEvent(
        const WebString& key, const WebString& oldValue,
        const WebString& newValue, const WebURL& origin,
        const WebURL& pageURL, const WebStorageNamespace& sessionNamespace,
        WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    RefPtr<WebCore::SecurityOrigin> securityOrigin = WebCore::SecurityOrigin::create(origin);
    WebCore::StorageAreaProxy::dispatchSessionStorageEvent(
            pageGroupName, key, oldValue, newValue, securityOrigin.get(), pageURL,
            sessionNamespace, sourceAreaInstance, originatedInProcess);
}


// FIXME: remove the WebStorageEventDispatcherImpl class soon.

WebStorageEventDispatcher* WebStorageEventDispatcher::create()
{
    return new WebStorageEventDispatcherImpl();
}

WebStorageEventDispatcherImpl::WebStorageEventDispatcherImpl()
    : m_eventDispatcher(adoptPtr(new WebCore::StorageEventDispatcherImpl(pageGroupName)))
{
    ASSERT(m_eventDispatcher);
}

void WebStorageEventDispatcherImpl::dispatchStorageEvent(const WebString& key, const WebString& oldValue,
                                                         const WebString& newValue, const WebString& origin,
                                                         const WebURL& pageURL, bool isLocalStorage)
{
    WebCore::StorageType storageType = isLocalStorage ? WebCore::LocalStorage : WebCore::SessionStorage;
    RefPtr<WebCore::SecurityOrigin> securityOrigin = WebCore::SecurityOrigin::createFromString(origin);
    m_eventDispatcher->dispatchStorageEvent(key, oldValue, newValue, securityOrigin.get(), pageURL, storageType);
}

} // namespace WebKit
