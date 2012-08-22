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

#include "config.h"
#include "MockWebKitPlatformSupport.h"

#include "MockWebMediaStreamCenter.h"
#include "MockWebRTCPeerConnectionHandler.h"
#include <wtf/Assertions.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

PassOwnPtr<MockWebKitPlatformSupport> MockWebKitPlatformSupport::create()
{
    return adoptPtr(new MockWebKitPlatformSupport());
}

MockWebKitPlatformSupport::MockWebKitPlatformSupport()
{
}

MockWebKitPlatformSupport::~MockWebKitPlatformSupport()
{
}

void MockWebKitPlatformSupport::cryptographicallyRandomValues(unsigned char*, size_t)
{
    CRASH();
}

#if ENABLE(MEDIA_STREAM)
WebMediaStreamCenter* MockWebKitPlatformSupport::createMediaStreamCenter(WebMediaStreamCenterClient* client)
{
    if (!m_mockMediaStreamCenter)
        m_mockMediaStreamCenter = adoptPtr(new MockWebMediaStreamCenter(client));

    return m_mockMediaStreamCenter.get();
}

WebRTCPeerConnectionHandler* MockWebKitPlatformSupport::createRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client)
{
    return new MockWebRTCPeerConnectionHandler(client);
}
#endif // ENABLE(MEDIA_STREAM)
