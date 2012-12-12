/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDataChannelHandlerChromium_h
#define RTCDataChannelHandlerChromium_h

#if ENABLE(MEDIA_STREAM)

#include "RTCDataChannelHandler.h"
#include "RTCDataChannelHandlerClient.h"
#include <public/WebRTCDataChannelHandler.h>
#include <public/WebRTCDataChannelHandlerClient.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class RTCDataChannelHandlerClient;

class RTCDataChannelHandlerChromium : public RTCDataChannelHandler, public WebKit::WebRTCDataChannelHandlerClient {
public:
    static PassOwnPtr<RTCDataChannelHandler> create(WebKit::WebRTCDataChannelHandler*);
    virtual ~RTCDataChannelHandlerChromium();

    virtual void setClient(RTCDataChannelHandlerClient*) OVERRIDE;

    virtual String label() OVERRIDE;
    virtual bool isReliable() OVERRIDE;
    virtual unsigned long bufferedAmount() OVERRIDE;
    virtual bool sendStringData(const String&) OVERRIDE;
    virtual bool sendRawData(const char*, size_t) OVERRIDE;
    virtual void close() OVERRIDE;

    // WebKit::WebRTCDataChannelHandlerClient implementation.
    virtual void didChangeReadyState(ReadyState) const OVERRIDE;
    virtual void didReceiveStringData(const WebKit::WebString&) const OVERRIDE;
    virtual void didReceiveRawData(const char*, size_t) const OVERRIDE;
    virtual void didDetectError() const OVERRIDE;

private:
    explicit RTCDataChannelHandlerChromium(WebKit::WebRTCDataChannelHandler*);

    OwnPtr<WebKit::WebRTCDataChannelHandler> m_webHandler;
    RTCDataChannelHandlerClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCDataChannelHandlerChromium_h
