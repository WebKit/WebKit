/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "APIFrameInfo.h"

#include "APIFrameHandle.h"
#include "FrameInfoData.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"

namespace API {

Ref<FrameInfo> FrameInfo::create(WebKit::FrameInfoData&& frameInfoData, WebKit::WebPageProxy* page)
{
    return adoptRef(*new FrameInfo(WTFMove(frameInfoData), page));
}

Ref<FrameInfo> FrameInfo::create(const WebKit::WebFrameProxy& frame, WebCore::SecurityOriginData&& securityOrigin)
{
    WebKit::FrameInfoData frameInfoData {
        frame.isMainFrame(),
        WebCore::ResourceRequest(frame.url()), // FIXME: This should use the full request of the frame, not just the URL.
        WTFMove(securityOrigin),
        frame.frameID()
    };
    return create(WTFMove(frameInfoData), frame.page());
}

FrameInfo::FrameInfo(const WebKit::FrameInfoData& frameInfoData, WebKit::WebPageProxy* page)
    : m_isMainFrame { frameInfoData.isMainFrame }
    , m_request { frameInfoData.request }
    , m_securityOrigin { SecurityOrigin::create(frameInfoData.securityOrigin.securityOrigin()) }
    , m_handle { API::FrameHandle::create(frameInfoData.frameID ? *frameInfoData.frameID : WebCore::FrameIdentifier{ }) }
    , m_page { page }
{
}

FrameInfo::~FrameInfo() = default;

} // namespace API
