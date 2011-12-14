/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebMediaCacheManagerProxy.h"

#include "WebContext.h"
#include "WebMediaCacheManagerMessages.h"
#include "WebSecurityOrigin.h"

namespace WebKit {

PassRefPtr<WebMediaCacheManagerProxy> WebMediaCacheManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebMediaCacheManagerProxy(context));
}

WebMediaCacheManagerProxy::WebMediaCacheManagerProxy(WebContext* context)
    : m_webContext(context)
{
}

WebMediaCacheManagerProxy::~WebMediaCacheManagerProxy()
{
}

void WebMediaCacheManagerProxy::invalidate()
{
    invalidateCallbackMap(m_arrayCallbacks);
}

bool WebMediaCacheManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebMediaCacheManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebMediaCacheManagerProxyMessage(connection, messageID, arguments);
}

void WebMediaCacheManagerProxy::getHostnamesWithMediaCache(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): When we're sending this to multiple processes, we need to aggregate the
    // callback data when it comes back.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebMediaCacheManager::GetHostnamesWithMediaCache(callbackID));
}
    
void WebMediaCacheManagerProxy::didGetHostnamesWithMediaCache(const Vector<String>& hostnameList, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    size_t hostnameCount = hostnameList.size();
    Vector<RefPtr<APIObject> > hostnames(hostnameCount);

    for (size_t i = 0; i < hostnameCount; ++i)
        hostnames[i] = WebString::create(hostnameList[i]);

    callback->performCallbackWithReturnValue(ImmutableArray::adopt(hostnames).get());
}

void WebMediaCacheManagerProxy::clearCacheForHostname(const String& hostname)
{
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebMediaCacheManager::ClearCacheForHostname(hostname));
}

void WebMediaCacheManagerProxy::clearCacheForAllHostnames()
{
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebMediaCacheManager::ClearCacheForAllHostnames());
}

} // namespace WebKit
