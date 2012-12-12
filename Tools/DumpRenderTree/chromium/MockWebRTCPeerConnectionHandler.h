/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MockWebRTCPeerConnectionHandler_h
#define MockWebRTCPeerConnectionHandler_h

#if ENABLE(MEDIA_STREAM)

#include "WebTask.h"
#include <public/WebRTCPeerConnectionHandler.h>
#include <public/WebRTCSessionDescription.h>
#include <public/WebRTCSessionDescriptionRequest.h>
#include <public/WebRTCStatsRequest.h>

namespace WebKit {
class WebRTCPeerConnectionHandlerClient;
};

class MockWebRTCPeerConnectionHandler : public WebKit::WebRTCPeerConnectionHandler {
public:
    explicit MockWebRTCPeerConnectionHandler(WebKit::WebRTCPeerConnectionHandlerClient*);

    virtual bool initialize(const WebKit::WebRTCConfiguration&, const WebKit::WebMediaConstraints&) OVERRIDE;

    virtual void createOffer(const WebKit::WebRTCSessionDescriptionRequest&, const WebKit::WebMediaConstraints&) OVERRIDE;
    virtual void createAnswer(const WebKit::WebRTCSessionDescriptionRequest&, const WebKit::WebMediaConstraints&) OVERRIDE;
    virtual void setLocalDescription(const WebKit::WebRTCVoidRequest&, const WebKit::WebRTCSessionDescription&) OVERRIDE;
    virtual void setRemoteDescription(const WebKit::WebRTCVoidRequest&, const WebKit::WebRTCSessionDescription&) OVERRIDE;
    virtual WebKit::WebRTCSessionDescription localDescription() OVERRIDE;
    virtual WebKit::WebRTCSessionDescription remoteDescription() OVERRIDE;
    virtual bool updateICE(const WebKit::WebRTCConfiguration&, const WebKit::WebMediaConstraints&) OVERRIDE;
    virtual bool addICECandidate(const WebKit::WebRTCICECandidate&) OVERRIDE;
    virtual bool addStream(const WebKit::WebMediaStreamDescriptor&, const WebKit::WebMediaConstraints&) OVERRIDE;
    virtual void removeStream(const WebKit::WebMediaStreamDescriptor&) OVERRIDE;
    virtual void getStats(const WebKit::WebRTCStatsRequest&) OVERRIDE;
    virtual WebKit::WebRTCDataChannelHandler* createDataChannel(const WebKit::WebString& label, bool reliable) OVERRIDE;
    virtual void stop() OVERRIDE;

    // WebTask related methods
    WebTestRunner::WebTaskList* taskList() { return &m_taskList; }

private:
    MockWebRTCPeerConnectionHandler() { }

    WebKit::WebRTCPeerConnectionHandlerClient* m_client;
    bool m_stopped;
    WebTestRunner::WebTaskList m_taskList;
    WebKit::WebRTCSessionDescription m_localDescription;
    WebKit::WebRTCSessionDescription m_remoteDescription;
    int m_streamCount;
};

#endif // ENABLE(MEDIA_STREAM)

#endif // MockWebRTCPeerConnectionHandler_h

