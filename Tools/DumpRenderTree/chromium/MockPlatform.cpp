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
#include "MockPlatform.h"

#include "WebTestInterfaces.h"
#include <public/WebMediaStreamCenter.h>
#include <wtf/Assertions.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;
using namespace WebTestRunner;

PassOwnPtr<MockPlatform> MockPlatform::create()
{
    return adoptPtr(new MockPlatform());
}

MockPlatform::MockPlatform()
{
}

MockPlatform::~MockPlatform()
{
}

void MockPlatform::setInterfaces(WebTestInterfaces* interfaces)
{
    m_interfaces = interfaces;
}

void MockPlatform::cryptographicallyRandomValues(unsigned char*, size_t)
{
    CRASH();
}

#if ENABLE(MEDIA_STREAM)
WebMediaStreamCenter* MockPlatform::createMediaStreamCenter(WebMediaStreamCenterClient* client)
{
    ASSERT(m_interfaces);

    if (!m_mockMediaStreamCenter)
        m_mockMediaStreamCenter = adoptPtr(m_interfaces->createMediaStreamCenter(client));

    return m_mockMediaStreamCenter.get();
}

WebRTCPeerConnectionHandler* MockPlatform::createRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client)
{
    ASSERT(m_interfaces);

    return m_interfaces->createWebRTCPeerConnectionHandler(client);
}
#endif // ENABLE(MEDIA_STREAM)
