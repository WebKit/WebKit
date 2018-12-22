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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/SWServerToContextConnection.h>

namespace WebKit {

class WebSWServerToContextConnection : public WebCore::SWServerToContextConnection, public IPC::MessageSender, public IPC::MessageReceiver {
public:
    template <typename... Args> static Ref<WebSWServerToContextConnection> create(Args&&... args)
    {
        return adoptRef(*new WebSWServerToContextConnection(std::forward<Args>(args)...));
    }

    void connectionClosed();

    IPC::Connection* ipcConnection() const { return m_ipcConnection.ptr(); }

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void terminate();

private:
    WebSWServerToContextConnection(const WebCore::SecurityOriginData&, Ref<IPC::Connection>&&);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() final;
    uint64_t messageSenderDestinationID() final;

    // Messages to the SW host WebProcess
    void installServiceWorkerContext(const WebCore::ServiceWorkerContextData&, PAL::SessionID, const String& userAgent) final;
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier) final;
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier) final;
    void terminateWorker(WebCore::ServiceWorkerIdentifier) final;
    void syncTerminateWorker(WebCore::ServiceWorkerIdentifier) final;
    void findClientByIdentifierCompleted(uint64_t requestIdentifier, const Optional<WebCore::ServiceWorkerClientData>&, bool hasSecurityError) final;
    void matchAllCompleted(uint64_t requestIdentifier, const Vector<WebCore::ServiceWorkerClientData>&) final;
    void claimCompleted(uint64_t requestIdentifier) final;
    void didFinishSkipWaiting(uint64_t callbackID) final;

    void connectionMayNoLongerBeNeeded() final;

    Ref<IPC::Connection> m_ipcConnection;
    
}; // class WebSWServerToContextConnection

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)

