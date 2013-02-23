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

#ifndef MockPlatform_h
#define MockPlatform_h

#include <public/Platform.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebTestRunner {
class WebTestInterfaces;
}

class MockPlatform : public WebKit::Platform {
public:
    static PassOwnPtr<MockPlatform> create();
    ~MockPlatform();

    void setInterfaces(WebTestRunner::WebTestInterfaces*);
    virtual void cryptographicallyRandomValues(unsigned char* buffer, size_t length) OVERRIDE;

#if ENABLE(MEDIA_STREAM)
    virtual WebKit::WebMediaStreamCenter* createMediaStreamCenter(WebKit::WebMediaStreamCenterClient*) OVERRIDE;
    virtual WebKit::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(WebKit::WebRTCPeerConnectionHandlerClient*) OVERRIDE;
#endif // ENABLE(MEDIA_STREAM)

private:
    MockPlatform();

    WebTestRunner::WebTestInterfaces* m_interfaces;

#if ENABLE(MEDIA_STREAM)
    OwnPtr<WebKit::WebMediaStreamCenter> m_mockMediaStreamCenter;
#endif // ENABLE(MEDIA_STREAM)
};

#endif // MockPlatform_h
