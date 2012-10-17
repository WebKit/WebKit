
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

#include <public/WebMediaStreamComponent.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebRTCStatsRequest.h>
#include <public/WebRTCStatsResponse.h>

#include "RTCStatsRequest.h"
#include "RTCStatsResponse.h"
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

namespace WebKit {

WebRTCStatsRequest::WebRTCStatsRequest(const PassRefPtr<RTCStatsRequest>& request)
    : m_private(request)
{
}

void WebRTCStatsRequest::assign(const WebRTCStatsRequest& other)
{
    m_private = other.m_private;
}

void WebRTCStatsRequest::reset()
{
    m_private.reset();
}

WebRTCStatsResponse WebRTCStatsRequest::createResponse() const
{
    return WebRTCStatsResponse(m_private->createResponse());
}

bool WebRTCStatsRequest::hasSelector() const
{
    return m_private->hasSelector();
}

const WebMediaStreamDescriptor WebRTCStatsRequest::stream() const
{
    return WebMediaStreamDescriptor(m_private->stream());
}

const WebMediaStreamComponent WebRTCStatsRequest::component() const
{
    return WebMediaStreamComponent(m_private->component());
}

void WebRTCStatsRequest::requestSucceeded(const WebRTCStatsResponse& response) const
{
    m_private->requestSucceeded(response);
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
