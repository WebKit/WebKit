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

#ifndef Platform_h
#define Platform_h

#include "WebCommon.h"

namespace WebKit {

class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebPeerConnection00Handler;
class WebPeerConnection00HandlerClient;
class WebPeerConnectionHandler;
class WebPeerConnectionHandlerClient;
class WebURLLoader;

class Platform {
public:
    WEBKIT_EXPORT static void initialize(Platform*);
    WEBKIT_EXPORT static void shutdown();
    WEBKIT_EXPORT static Platform* current();

    // Network -------------------------------------------------------------

    // Returns a new WebURLLoader instance.
    virtual WebURLLoader* createURLLoader() { return 0; }

    // WebRTC ----------------------------------------------------------

    // DEPRECATED
    // Creates an WebPeerConnectionHandler for DeprecatedPeerConnection.
    // May return null if WebRTC functionality is not avaliable or out of resources.
    virtual WebPeerConnectionHandler* createPeerConnectionHandler(WebPeerConnectionHandlerClient*) { return 0; }

    // Creates an WebPeerConnection00Handler for PeerConnection00.
    // This is an highly experimental feature not yet in the WebRTC standard.
    // May return null if WebRTC functionality is not avaliable or out of resources.
    virtual WebPeerConnection00Handler* createPeerConnection00Handler(WebPeerConnection00HandlerClient*) { return 0; }

    // May return null if WebRTC functionality is not avaliable or out of resources.
    virtual WebMediaStreamCenter* createMediaStreamCenter(WebMediaStreamCenterClient*) { return 0; }

protected:
    ~Platform() { }
};

} // namespace WebKit

#endif
