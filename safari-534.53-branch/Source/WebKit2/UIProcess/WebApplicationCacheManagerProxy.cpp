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
#include "WebApplicationCacheManagerProxy.h"

#include "SecurityOriginData.h"
#include "WebApplicationCacheManagerMessages.h"
#include "WebContext.h"
#include "WebSecurityOrigin.h"

namespace WebKit {

PassRefPtr<WebApplicationCacheManagerProxy> WebApplicationCacheManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebApplicationCacheManagerProxy(context));
}

WebApplicationCacheManagerProxy::WebApplicationCacheManagerProxy(WebContext* context)
    : m_webContext(context)
{
}

WebApplicationCacheManagerProxy::~WebApplicationCacheManagerProxy()
{
}

void WebApplicationCacheManagerProxy::invalidate()
{
    invalidateCallbackMap(m_arrayCallbacks);
}

bool WebApplicationCacheManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebApplicationCacheManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebApplicationCacheManagerProxyMessage(connection, messageID, arguments);
}

void WebApplicationCacheManagerProxy::getApplicationCacheOrigins(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): The application cache shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebApplicationCacheManager::GetApplicationCacheOrigins(callbackID));
}
    
void WebApplicationCacheManagerProxy::didGetApplicationCacheOrigins(const Vector<SecurityOriginData>& originDatas, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    performAPICallbackWithSecurityOriginDataVector(originDatas, callback.get());
}

void WebApplicationCacheManagerProxy::deleteEntriesForOrigin(WebSecurityOrigin* origin)
{
    SecurityOriginData securityOriginData;
    securityOriginData.protocol = origin->protocol();
    securityOriginData.host = origin->host();
    securityOriginData.port = origin->port();

    // FIXME (Multi-WebProcess): The application cache shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebApplicationCacheManager::DeleteEntriesForOrigin(securityOriginData));
}

void WebApplicationCacheManagerProxy::deleteAllEntries()
{
    // FIXME (Multi-WebProcess): The application cache shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebApplicationCacheManager::DeleteAllEntries());
}

} // namespace WebKit
