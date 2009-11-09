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

#ifndef WebSocketStreamHandleClient_h
#define WebSocketStreamHandleClient_h

#include "WebCommon.h"

namespace WebKit {

class WebData;
class WebSocketStreamError;
class WebSocketStreamHandle;
class WebURL;

class WebSocketStreamHandleClient {
public:

    // Called when WebSocketStreamHandle is going to open the URL.
    virtual void willOpenStream(WebSocketStreamHandle*, const WebURL&) = 0;

    // Called when Socket Stream is opened.
    virtual void didOpenStream(WebSocketStreamHandle*, int /* maxPendingSendAllowed */) = 0;

    // Called when |amountSent| bytes are sent.
    virtual void didSendData(WebSocketStreamHandle*, int /* amountSent */) = 0;

    // Called when data are received.
    virtual void didReceiveData(WebSocketStreamHandle*, const WebData&) = 0;

    // Called when Socket Stream is closed.
    virtual void didClose(WebSocketStreamHandle*) = 0;

    // Called when Socket Stream has an error.
    virtual void didFail(WebSocketStreamHandle*, const WebSocketStreamError&) = 0;

    // FIXME: auth challenge for proxy
};

} // namespace WebKit

#endif
