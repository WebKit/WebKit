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

#ifndef WebPeerConnectionHandler_h
#define WebPeerConnectionHandler_h

#include "WebString.h"
#include "WebVector.h"

namespace WebKit {

class WebMediaStreamDescriptor;
class WebPeerConnectionHandlerClient;

// Note:
// SDP stands for Session Description Protocol, which is intended for describing
// multimedia sessions for the purposes of session announcement, session
// invitation, and other forms of multimedia session initiation.
//
// More information can be found here:
// http://tools.ietf.org/html/rfc4566
// http://en.wikipedia.org/wiki/Session_Description_Protocol


class WebPeerConnectionHandler {
public:
    virtual ~WebPeerConnectionHandler() { }

    virtual void initialize(const WebString& serverConfiguration, const WebString& username) { };

    virtual void produceInitialOffer(const WebVector<WebMediaStreamDescriptor>& pendingAddStreams) = 0;
    virtual void handleInitialOffer(const WebString& sdp) = 0;
    virtual void processSDP(const WebString& sdp) = 0;
    virtual void processPendingStreams(const WebVector<WebMediaStreamDescriptor>& pendingAddStreams, const WebVector<WebMediaStreamDescriptor>& pendingRemoveStreams) = 0;
    virtual void sendDataStreamMessage(const char* data, size_t length) = 0;

    virtual void stop() = 0;
};

} // namespace WebKit

#endif // WebPeerConnectionHandler_h
