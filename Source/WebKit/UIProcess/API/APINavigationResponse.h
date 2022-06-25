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

#pragma once

#include "APIFrameInfo.h"
#include "APIObject.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

namespace API {

class FrameInfo;

class NavigationResponse final : public ObjectImpl<Object::Type::NavigationResponse> {
public:
    template<typename... Args> static Ref<NavigationResponse> create(Args&&... args)
    {
        return adoptRef(*new NavigationResponse(std::forward<Args>(args)...));
    }

    FrameInfo& frame() { return m_frame.get(); }

    const WebCore::ResourceRequest& request() const { return m_request; }
    const WebCore::ResourceResponse& response() const { return m_response; }

    bool canShowMIMEType() const { return m_canShowMIMEType; }
    const WTF::String& downloadAttribute() const { return m_downloadAttribute; }

private:
    NavigationResponse(API::FrameInfo& frame, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, bool canShowMIMEType, const WTF::String& downloadAttribute)
        : m_frame(frame)
        , m_request(request)
        , m_response(response)
        , m_canShowMIMEType(canShowMIMEType)
        , m_downloadAttribute(downloadAttribute) { }

    Ref<FrameInfo> m_frame;

    WebCore::ResourceRequest m_request;
    WebCore::ResourceResponse m_response;
    bool m_canShowMIMEType;
    WTF::String m_downloadAttribute;
};

} // namespace API
