/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef NetworkRequest_h
#define NetworkRequest_h

#include "NetworkConnectionToWebProcess.h"
#include <WebCore/ResourceRequest.h>

namespace WebKit {

typedef uint64_t ResourceLoadIdentifier;

class NetworkRequest : public RefCounted<NetworkRequest>, public NetworkConnectionToWebProcessObserver {
public:
    static RefPtr<NetworkRequest> create(const WebCore::ResourceRequest& request, ResourceLoadIdentifier identifier, NetworkConnectionToWebProcess* connection)
    {
        return adoptRef(new NetworkRequest(request, identifier, connection));
    }
    
    ~NetworkRequest();

    void connectionToWebProcessDidClose(NetworkConnectionToWebProcess*);
    
    ResourceLoadIdentifier identifier() { return m_identifier; }
    
    NetworkConnectionToWebProcess* connectionToWebProcess() { return m_connection.get(); }

private:
    NetworkRequest(const WebCore::ResourceRequest&, ResourceLoadIdentifier, NetworkConnectionToWebProcess*);

    WebCore::ResourceRequest m_request;
    ResourceLoadIdentifier m_identifier;

    RefPtr<NetworkConnectionToWebProcess> m_connection;
};

} // namespace WebKit

#endif // NetworkRequest_h
