/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/SWRegistrationStore.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/Timer.h>
#include <wtf/CompletionHandler.h>

namespace WebCore {
class SWServer;
}

namespace WebKit {

class NetworkStorageManager;

class WebSWRegistrationStore final : public WebCore::SWRegistrationStore, public CanMakeWeakPtr<WebSWRegistrationStore> {
public:
    WebSWRegistrationStore(WebCore::SWServer&, NetworkStorageManager&);

private:
    // WebCore::SWRegistrationStore
    void clearAll(CompletionHandler<void()>&&);
    void flushChanges(CompletionHandler<void()>&&);
    void closeFiles(CompletionHandler<void()>&&);
    void importRegistrations(CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerContextData>>)>&&);
    void updateRegistration(const WebCore::ServiceWorkerContextData&);
    void removeRegistration(const WebCore::ServiceWorkerRegistrationKey&);

    void scheduleUpdateIfNecessary();
    void updateToStorage(CompletionHandler<void()>&&);
    void updateTimerFired() { updateToStorage([] { }); }

    WeakPtr<WebCore::SWServer> m_server;
    WeakPtr<NetworkStorageManager> m_manager;
    WebCore::Timer m_updateTimer;
    HashMap<WebCore::ServiceWorkerRegistrationKey, std::optional<WebCore::ServiceWorkerContextData>> m_updates;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
