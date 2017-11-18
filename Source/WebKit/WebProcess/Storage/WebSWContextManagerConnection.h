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
#include <WebCore/ServiceWorkerTypes.h>

namespace IPC {
class FormDataReference;
}

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
    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, Ref<WebCore::SerializedScriptValue>&& message, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin) final;
    void didFinishInstall(WebCore::ServiceWorkerIdentifier, bool wasSuccessful) final;
    void didFinishActivation(WebCore::ServiceWorkerIdentifier) final;
    void setServiceWorkerHasPendingEvents(WebCore::ServiceWorkerIdentifier, bool) final;
    void workerTerminated(WebCore::ServiceWorkerIdentifier) final;

    // IPC messages.
    void serviceWorkerStartedWithMessage(WebCore::ServiceWorkerIdentifier, const String& exceptionMessage) final;
    void installServiceWorker(const WebCore::ServiceWorkerContextData&);
    void startFetch(WebCore::SWServerConnectionIdentifier, uint64_t fetchIdentifier, std::optional<WebCore::ServiceWorkerIdentifier>, WebCore::ResourceRequest&&, WebCore::FetchOptions&&, IPC::FormDataReference&&);
    void postMessageToServiceWorkerGlobalScope(WebCore::ServiceWorkerIdentifier destinationIdentifier, const IPC::DataReference& message, WebCore::ServiceWorkerClientIdentifier sourceIdentifier, WebCore::ServiceWorkerClientData&& sourceData);
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier);
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier);
    void terminateWorker(WebCore::ServiceWorkerIdentifier);

    Ref<IPC::Connection> m_connectionToStorageProcess;
    uint64_t m_pageID { 0 };
    uint64_t m_previousServiceWorkerID { 0 };
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
