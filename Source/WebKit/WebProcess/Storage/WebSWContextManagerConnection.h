/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "Connection.h"
#include "MessageReceiver.h"
#include <WebCore/SWContextManager.h>

namespace WebCore {
struct FetchOptions;
class ResourceRequest;
struct ServiceWorkerContextData;
}

namespace WebKit {

struct WebPreferencesStore;

class WebSWContextManagerConnection final : public WebCore::SWContextManager::Connection, public IPC::MessageReceiver {
public:
    WebSWContextManagerConnection(Ref<IPC::Connection>&&, uint64_t pageID, const WebPreferencesStore&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    void updatePreferences(const WebPreferencesStore&);

    // WebCore::SWContextManager::Connection.
    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, Ref<WebCore::SerializedScriptValue>&& message, uint64_t sourceServiceWorkerIdentifier, const String& sourceOrigin) final;

    // IPC messages.
    void startServiceWorker(uint64_t serverConnectionIdentifier, const WebCore::ServiceWorkerContextData&);
    void startFetch(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier, uint64_t serviceWorkerIdentifier, WebCore::ResourceRequest&&, WebCore::FetchOptions&&);
    void postMessageToServiceWorkerGlobalScope(uint64_t destinationServiceWorkerIdentifier, const IPC::DataReference& message, const WebCore::ServiceWorkerClientIdentifier& sourceIdentifier, const String& sourceOrigin);

    Ref<IPC::Connection> m_connectionToStorageProcess;
    uint64_t m_pageID { 0 };
    uint64_t m_previousServiceWorkerID { 0 };
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
