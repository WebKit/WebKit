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
#include <WebCore/ServiceWorkerFetch.h>

namespace WebKit {

class WebServiceWorkerFetchTaskClient final : public WebCore::ServiceWorkerFetch::Client {
public:
    static Ref<WebServiceWorkerFetchTaskClient> create(Ref<IPC::Connection>&& connection, uint64_t serverConnectionIdentifier, uint64_t fetchTaskIdentifier)
    {
        return adoptRef(*new WebServiceWorkerFetchTaskClient(WTFMove(connection), serverConnectionIdentifier, fetchTaskIdentifier));
    }

    ~WebServiceWorkerFetchTaskClient();

private:
    WebServiceWorkerFetchTaskClient(Ref<IPC::Connection>&&, uint64_t serverConnectionIdentifier, uint64_t fetchTaskIdentifier);

    void didReceiveResponse(const WebCore::ResourceResponse&) final;
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final;
    void didFail() final;
    void didFinish() final;
    void didNotHandle() final;

    RefPtr<IPC::Connection> m_connection;
    uint64_t m_serverConnectionIdentifier { 0 };
    uint64_t m_fetchTaskIdentifier { 0 };
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
