/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDTMFSenderHandlerChromium_h
#define RTCDTMFSenderHandlerChromium_h

#if ENABLE(MEDIA_STREAM)

#include "RTCDTMFSenderHandler.h"
#include "RTCDTMFSenderHandlerClient.h"
#include <public/WebRTCDTMFSenderHandler.h>
#include <public/WebRTCDTMFSenderHandlerClient.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class RTCDTMFSenderHandlerClient;

class RTCDTMFSenderHandlerChromium : public RTCDTMFSenderHandler, public WebKit::WebRTCDTMFSenderHandlerClient {
public:
    static PassOwnPtr<RTCDTMFSenderHandler> create(WebKit::WebRTCDTMFSenderHandler*);
    virtual ~RTCDTMFSenderHandlerChromium();

    virtual void setClient(RTCDTMFSenderHandlerClient*) OVERRIDE;

    virtual String currentToneBuffer() OVERRIDE;

    virtual bool canInsertDTMF() OVERRIDE;
    virtual bool insertDTMF(const String& tones, long duration, long interToneGap) OVERRIDE;

    // WebKit::WebRTCDTMFSenderHandlerClient implementation.
    virtual void didPlayTone(const WebKit::WebString& tone) const OVERRIDE;

private:
    explicit RTCDTMFSenderHandlerChromium(WebKit::WebRTCDTMFSenderHandler*);

    OwnPtr<WebKit::WebRTCDTMFSenderHandler> m_webHandler;
    RTCDTMFSenderHandlerClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCDTMFSenderHandlerChromium_h
