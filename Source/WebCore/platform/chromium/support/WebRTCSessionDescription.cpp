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

#if ENABLE(MEDIA_STREAM)

#include <public/WebRTCSessionDescription.h>

#include "RTCSessionDescriptionDescriptor.h"
#include <public/WebString.h>

using namespace WebCore;

namespace WebKit {

WebRTCSessionDescription::WebRTCSessionDescription(const PassRefPtr<RTCSessionDescriptionDescriptor>& sessionDescription)
    : m_private(sessionDescription)
{
}

void WebRTCSessionDescription::assign(const WebRTCSessionDescription& other)
{
    m_private = other.m_private;
}

void WebRTCSessionDescription::reset()
{
    m_private.reset();
}

WebRTCSessionDescription::operator WTF::PassRefPtr<WebCore::RTCSessionDescriptionDescriptor>() const
{
    return m_private.get();
}

void WebRTCSessionDescription::initialize(const WebString& type, const WebString& sdp)
{
    m_private = RTCSessionDescriptionDescriptor::create(type, sdp);
}

WebString WebRTCSessionDescription::type() const
{
    ASSERT(!m_private.isNull());
    return m_private.get()->type();
}

void WebRTCSessionDescription::setType(const WebString& type)
{
    ASSERT(!m_private.isNull());
    return m_private.get()->setType(type);
}

WebString WebRTCSessionDescription::sdp() const
{
    ASSERT(!m_private.isNull());
    return m_private.get()->sdp();
}

void WebRTCSessionDescription::setSDP(const WebString& sdp)
{
    ASSERT(!m_private.isNull());
    return m_private.get()->setSdp(sdp);
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

