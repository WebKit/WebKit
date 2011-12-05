/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef ResourceHandleInternal_h
#define ResourceHandleInternal_h

#include "ResourceRequest.h"
#include "platform/WebCommon.h"
#include "platform/WebURLLoader.h"
#include "platform/WebURLLoaderClient.h"

namespace WebCore {

class ResourceHandle;
class ResourceHandleClient;

class ResourceHandleInternal : public WebKit::WebURLLoaderClient {
public:
    ResourceHandleInternal(const ResourceRequest&, ResourceHandleClient*);

    virtual ~ResourceHandleInternal() { }

    void start();
    void cancel();
    void setDefersLoading(bool);
    bool allowStoredCredentials() const;

    // WebURLLoaderClient methods:
    virtual void willSendRequest(WebKit::WebURLLoader*, WebKit::WebURLRequest&, const WebKit::WebURLResponse&);
    virtual void didSendData(WebKit::WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(WebKit::WebURLLoader*, const WebKit::WebURLResponse&);
    virtual void didDownloadData(WebKit::WebURLLoader*, int dataLength);
    virtual void didReceiveData(WebKit::WebURLLoader*, const char* data, int dataLength, int encodedDataLength);

    virtual void didReceiveCachedMetadata(WebKit::WebURLLoader*, const char* data, int dataLength);
    virtual void didFinishLoading(WebKit::WebURLLoader*, double finishTime);
    virtual void didFail(WebKit::WebURLLoader*, const WebKit::WebURLError&);

    enum ConnectionState {
        ConnectionStateNew,
        ConnectionStateStarted,
        ConnectionStateReceivedResponse,
        ConnectionStateReceivingData,
        ConnectionStateFinishedLoading,
        ConnectionStateCanceled,
        ConnectionStateFailed,
    };

    void setOwner(ResourceHandle* owner) { m_owner = owner; }
    ResourceRequest& request() { return m_request; }
    ResourceHandleClient* client() const { return m_client; }
    void setClient(ResourceHandleClient* client) { m_client = client; }
    WebKit::WebURLLoader* loader() const { return m_loader.get(); }

    static ResourceHandleInternal* FromResourceHandle(ResourceHandle*);

private:
    ResourceRequest m_request;
    ResourceHandle* m_owner;
    ResourceHandleClient* m_client;
    OwnPtr<WebKit::WebURLLoader> m_loader;

    // Used for sanity checking to make sure we don't experience illegal state
    // transitions.
    ConnectionState m_state;
};

} // namespace WebCore

#endif
