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

#ifndef WebRTCDataChannelHandlerClient_h
#define WebRTCDataChannelHandlerClient_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebKit {

class WebRTCDataChannelHandlerClient {
public:
    enum ReadyState {
        ReadyStateConnecting = 0,
        ReadyStateOpen = 1,
        ReadyStateClosing = 2,
        ReadyStateClosed = 3,
    };

    virtual ~WebRTCDataChannelHandlerClient() { }

    virtual void didChangeReadyState(ReadyState) const = 0;
    virtual void didReceiveStringData(const WebString&) const = 0;
    virtual void didReceiveRawData(const char*, size_t) const = 0;
    virtual void didDetectError() const = 0;
};

} // namespace WebKit

#endif // WebRTCDataChannelHandlerClient_h
