/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_CAPTURE)

#include "NetworkCaptureResource.h"
#include "NetworkDataTask.h"
#include <WebCore/ResourceRequest.h>

namespace WebCore {
class ResourceError;
class ResourceResponse;
}

namespace WebKit {
class NetworkDataTaskClient;
class NetworkLoadParameters;
class NetworkSession;
}

namespace WebKit {
namespace NetworkCapture {

struct RequestSentEvent;
struct ResponseReceivedEvent;
struct RedirectReceivedEvent;
struct RedirectSentEvent;
struct DataReceivedEvent;
struct FinishedEvent;

class NetworkDataTaskReplay : public NetworkDataTask {
public:
    static Ref<NetworkDataTaskReplay> create(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters, Resource* resource)
    {
        return adoptRef(*new NetworkDataTaskReplay(session, client, parameters, resource));
    }

    void replayRequestSent(const RequestSentEvent&);
    void replayResponseReceived(const ResponseReceivedEvent&);
    void replayRedirectReceived(const RedirectReceivedEvent&);
    void replayRedirectSent(const RedirectSentEvent&);
    void replayDataReceived(const DataReceivedEvent&);
    void replayFinished(const FinishedEvent&);

private:
    NetworkDataTaskReplay(NetworkSession&, NetworkDataTaskClient&, const NetworkLoadParameters&, Resource*);
    ~NetworkDataTaskReplay() override;

    void resume() override;
    void suspend() override;
    void cancel() override;
    void complete();
    void invalidateAndCancel() override;

    State state() const override { return m_state; }

    void enqueueEventHandler();

    enum class Error {
        NoError = 0,
        NotFoundError = 1,
        MethodNotAllowed = 5
    };

    void didReceiveResponse(WebCore::ResourceResponse&&);
    void didFinish();
    void didFinish(Error);
    void didFinish(const WebCore::ResourceError&);

    State m_state { State::Suspended };
    WebCore::ResourceRequest m_currentRequest;
    Resource* m_resource;
    Resource::EventStream m_eventStream;
};

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
