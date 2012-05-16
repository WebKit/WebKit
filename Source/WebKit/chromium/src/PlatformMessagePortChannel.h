/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef PlatformMessagePortChannel_h
#define PlatformMessagePortChannel_h


#include "MessagePortChannel.h"
#include <public/WebMessagePortChannelClient.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

namespace WebKit {
class WebMessagePortChannel;
}

namespace WebCore {

class MessagePort;

// PlatformMessagePortChannel is a platform-dependent interface to the remote side of a message channel.
class PlatformMessagePortChannel : public ThreadSafeRefCounted<PlatformMessagePortChannel>,
                                   public WebKit::WebMessagePortChannelClient {
public:
    static void createChannel(PassRefPtr<MessagePort>, PassRefPtr<MessagePort>);
    static PassRefPtr<PlatformMessagePortChannel> create();
    static PassRefPtr<PlatformMessagePortChannel> create(WebKit::WebMessagePortChannel*);

    // APIs delegated from MessagePortChannel.h
    bool entangleIfOpen(MessagePort*);
    void disentangle();
    void postMessageToRemote(PassOwnPtr<MessagePortChannel::EventData>);
    bool tryGetMessageFromRemote(OwnPtr<MessagePortChannel::EventData>&);
    void close();
    bool isConnectedTo(MessagePort* port);
    bool hasPendingActivity();

    // Releases ownership of the contained web channel.
    WebKit::WebMessagePortChannel* webChannelRelease();

    virtual ~PlatformMessagePortChannel();

private:
    PlatformMessagePortChannel();
    PlatformMessagePortChannel(WebKit::WebMessagePortChannel*);

    void setEntangledChannel(PassRefPtr<PlatformMessagePortChannel>);

    // WebKit::WebMessagePortChannelClient implementation
    virtual void messageAvailable();

    // Mutex used to ensure exclusive access to the object internals.
    Mutex m_mutex;

    // Pointer to our entangled pair - cleared when close() is called.
    RefPtr<PlatformMessagePortChannel> m_entangledChannel;

    // The port we are connected to - this is the port that is notified when new messages arrive.
    MessagePort* m_localPort;

    WebKit::WebMessagePortChannel* m_webChannel;
};

} // namespace WebCore

#endif // PlatformMessagePortChannel_h
