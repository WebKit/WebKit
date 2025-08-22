/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "APINavigationResponse.h"

#include "APIFrameInfo.h"
#include "APINavigation.h"
#include "FrameInfoData.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"

namespace API {

NavigationResponse::NavigationResponse(API::FrameInfo& frame, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, bool canShowMIMEType, WTF::String&& downloadAttribute, Navigation* navigation)
    : m_frame(frame)
    , m_request(request)
    , m_response(response)
    , m_canShowMIMEType(canShowMIMEType)
    , m_downloadAttribute(WTFMove(downloadAttribute))
    , m_navigation(navigation) { }

NavigationResponse::~NavigationResponse() = default;

FrameInfo* NavigationResponse::navigationInitiatingFrame()
{
    if (!m_navigation)
        return nullptr;
    auto& frameInfo = m_navigation->originatingFrameInfo();
    if (!frameInfo)
        return nullptr;
    RefPtr frame = WebKit::WebFrameProxy::webFrame(frameInfo->frameID);
    m_sourceFrame = FrameInfo::create(WebKit::FrameInfoData { *frameInfo });
    return m_sourceFrame.get();
}

}
