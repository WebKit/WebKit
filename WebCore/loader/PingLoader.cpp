/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "PingLoader.h"

#include "Frame.h"
#include "ResourceHandle.h"
#include "SecurityOrigin.h"
#include <wtf/OwnPtr.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

void PingLoader::loadImage(Frame* frame, const KURL& url)
{
    if (!frame->document()->securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url);
        return;
    }

    ResourceRequest request(url);
    request.setTargetType(ResourceRequest::TargetIsImage);
    request.setHTTPHeaderField("Cache-Control", "max-age=0");
    if (!SecurityOrigin::shouldHideReferrer(request.url(), frame->loader()->outgoingReferrer()))
        request.setHTTPReferrer(frame->loader()->outgoingReferrer());
    frame->loader()->addExtraFieldsToSubresourceRequest(request);
    OwnPtr<PingLoader> pingLoader = adoptPtr(new PingLoader(frame, request));
    
    // Leak the ping loader, since it will kill itself as soon as it receives a response.
    PingLoader* leakedPingLoader = pingLoader.leakPtr();
    UNUSED_PARAM(leakedPingLoader);
}

PingLoader::PingLoader(Frame* frame, const ResourceRequest& request)
{
    m_handle = ResourceHandle::create(frame->loader()->networkingContext(), request, this, false, false);
}

PingLoader::~PingLoader()
{
    m_handle->cancel();
}

}
