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

#ifndef DeprecatedPeerConnectionHandlerInternal_h
#define DeprecatedPeerConnectionHandlerInternal_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamDescriptor.h"
#include <public/WebPeerConnectionHandlerClient.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebPeerConnectionHandler;
class WebString;
class WebMediaStreamDescriptor;
}

namespace WebCore {

class DeprecatedPeerConnectionHandlerClient;

class DeprecatedPeerConnectionHandlerInternal : public WebKit::WebPeerConnectionHandlerClient {
public:
    DeprecatedPeerConnectionHandlerInternal(DeprecatedPeerConnectionHandlerClient*, const String& serverConfiguration, const String& username);
    ~DeprecatedPeerConnectionHandlerInternal();

    virtual void produceInitialOffer(const MediaStreamDescriptorVector& pendingAddStreams);
    virtual void handleInitialOffer(const String& sdp);
    virtual void processSDP(const String& sdp);
    virtual void processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams);
    virtual void sendDataStreamMessage(const char* data, size_t length);
    virtual void stop();

    // WebKit::WebPeerConnectionHandlerClient implementation.
    virtual void didCompleteICEProcessing();
    virtual void didGenerateSDP(const WebKit::WebString& sdp);
    virtual void didReceiveDataStreamMessage(const char* data, size_t length);
    virtual void didAddRemoteStream(const WebKit::WebMediaStreamDescriptor&);
    virtual void didRemoveRemoteStream(const WebKit::WebMediaStreamDescriptor&);

private:
    OwnPtr<WebKit::WebPeerConnectionHandler> m_webHandler;
    DeprecatedPeerConnectionHandlerClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // DeprecatedPeerConnectionHandlerInternal_h
