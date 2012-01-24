/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#ifndef WebSocket_h
#define WebSocket_h

#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"

namespace WebCore { class WebSocketChannel; }

namespace WebKit {

class WebArrayBuffer;
class WebDocument;
class WebString;
class WebURL;
class WebSocketClient;

class WebSocket {
public:
    enum CloseEventCode {
        CloseEventCodeNotSpecified = -1,
        CloseEventCodeNormalClosure = 1000,
        CloseEventCodeGoingAway = 1001,
        CloseEventCodeProtocolError = 1002,
        CloseEventCodeUnsupportedData = 1003,
        CloseEventCodeFrameTooLarge = 1004,
        CloseEventCodeNoStatusRcvd = 1005,
        CloseEventCodeAbnormalClosure = 1006,
        CloseEventCodeInvalidUTF8 = 1007,
        CloseEventCodeMinimumUserDefined = 3000,
        CloseEventCodeMaximumUserDefined = 4999
    };

    enum BinaryType {
        BinaryTypeBlob = 0,
        BinaryTypeArrayBuffer = 1
    };

    WEBKIT_EXPORT static WebSocket* create(const WebDocument&, WebSocketClient*);
    virtual ~WebSocket() { }

    // These functions come from binaryType attribute of the WebSocket API
    // specification. It specifies binary object type for receiving binary
    // frames representation. Receiving text frames are always mapped to
    // WebString type regardless of this attribute.
    // Default type is BinaryTypeBlob. But currently it is not supported.
    // Set BinaryTypeArrayBuffer here ahead of using binary communication.
    // See also, The WebSocket API - http://www.w3.org/TR/websockets/ .
    virtual BinaryType binaryType() const = 0;
    virtual bool setBinaryType(BinaryType) = 0;

    virtual void connect(const WebURL&, const WebString& protocol) = 0;
    virtual WebString subprotocol() = 0;
    virtual bool sendText(const WebString&) = 0;
    virtual bool sendArrayBuffer(const WebArrayBuffer&) = 0;
    virtual unsigned long bufferedAmount() const = 0;
    virtual void close(int code, const WebString& reason) = 0;
    virtual void fail(const WebString& reason) = 0;
    virtual void disconnect() = 0;
};

} // namespace WebKit

#endif // WebSocket_h
