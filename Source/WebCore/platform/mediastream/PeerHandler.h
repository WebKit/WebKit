/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef PeerHandler_h
#define PeerHandler_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamDescriptor.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class PeerHandlerClient {
public:
    virtual ~PeerHandlerClient() { }

    virtual void iceProcessingCompleted() = 0;
    virtual void sdpGenerated(const String& sdp) = 0;
    virtual void dataStreamMessageReceived(const char* data, unsigned length) = 0;
    virtual void remoteStreamAdded(PassRefPtr<MediaStreamDescriptor>) = 0;
    virtual void remoteStreamRemoved(MediaStreamDescriptor*) = 0;
};

class PeerHandler {
    WTF_MAKE_NONCOPYABLE(PeerHandler);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<PeerHandler> create(PeerHandlerClient* client, const String& serverConfiguration, const String& username)
    {
        return adoptPtr(new PeerHandler(client, serverConfiguration, username));
    }
    virtual ~PeerHandler();

    void produceInitialOffer(const MediaStreamDescriptorVector& pendingAddStreams);
    void handleInitialOffer(const String& sdp);
    void processSDP(const String& sdp);
    void processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams);
    void sendDataStreamMessage(const char* data, unsigned length);

    void stop();

private:
    PeerHandler(PeerHandlerClient*, const String& serverConfiguration, const String& username);

    PeerHandlerClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // PeerHandler_h
