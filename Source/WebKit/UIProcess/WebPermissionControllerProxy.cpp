/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebPermissionControllerProxy.h"

#include "WebPageProxy.h"
#include "WebPermissionControllerProxyMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/PermissionDescriptor.h>
#include <WebCore/PermissionQuerySource.h>
#include <WebCore/PermissionState.h>
#include <WebCore/SecurityOriginData.h>
#include <optional>
#include <wtf/GetPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace WebKit {

WebPermissionControllerProxy::WebPermissionControllerProxy(WebProcessProxy& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::WebPermissionControllerProxy::messageReceiverName(), *this);
}

WebPermissionControllerProxy::~WebPermissionControllerProxy()
{
    m_process.removeMessageReceiver(Messages::WebPermissionControllerProxy::messageReceiverName());
}

void WebPermissionControllerProxy::query(const WebCore::ClientOrigin& clientOrigin, const WebCore::PermissionDescriptor& descriptor, std::optional<WebPageProxyIdentifier> identifier, WebCore::PermissionQuerySource source, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler)
{
    auto webPageProxy = identifier ? m_process.webPage(identifier.value()) : mostReasonableWebPageProxy(clientOrigin.topOrigin, source);

    if (!webPageProxy) {
        completionHandler(WebCore::PermissionState::Prompt);
        return;
    }

    webPageProxy->queryPermission(clientOrigin, descriptor, WTFMove(completionHandler));
}

RefPtr<WebPageProxy> WebPermissionControllerProxy::mostReasonableWebPageProxy(const WebCore::SecurityOriginData& topOrigin, WebCore::PermissionQuerySource source) const
{
    ASSERT(source == WebCore::PermissionQuerySource::SharedWorker || source == WebCore::PermissionQuerySource::ServiceWorker);
    
    RefPtr<WebPageProxy> webPageProxy;
    auto findWebPageProxy = [&topOrigin, &webPageProxy] (auto* processes) {
        if (!processes)
            return; 

        for (auto& process : *processes) {
            for (auto& potentialWebPageProxy : getPtr(process)->pages()) {
                if (!potentialWebPageProxy)
                    continue;
                if (WebCore::SecurityOriginData::fromURL(URL { potentialWebPageProxy->currentURL() }) != topOrigin)
                    continue;
                // The most reasonable webPageProxy is the newest one (the one with the greatest identifier).
                if (webPageProxy && webPageProxy->identifier() > potentialWebPageProxy->identifier())
                    continue;
                webPageProxy = WTFMove(potentialWebPageProxy);
            }
        }
    };

    Vector<Ref<WebProcessProxy>> currentProcess { Ref { m_process } };
    findWebPageProxy(&currentProcess);

    switch (source) {
    case WebCore::PermissionQuerySource::ServiceWorker:
        findWebPageProxy(m_process.serviceWorkerClientProcesses());
        break;
    case WebCore::PermissionQuerySource::SharedWorker:
        findWebPageProxy(m_process.sharedWorkerClientProcesses());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return webPageProxy;
}

} // namespace WebKit
