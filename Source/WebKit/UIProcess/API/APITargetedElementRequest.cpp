/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "APITargetedElementRequest.h"

#include "PageClient.h"
#include "WebPageProxy.h"

namespace API {

WebCore::FloatPoint TargetedElementRequest::point() const
{
    if (!std::holds_alternative<WebCore::FloatPoint>(m_request.data))
        return { };

    return std::get<WebCore::FloatPoint>(m_request.data);
}

void TargetedElementRequest::setPoint(WebCore::FloatPoint point)
{
    m_request.data = point;
}

WTF::String TargetedElementRequest::searchText() const
{
    if (!std::holds_alternative<WTF::String>(m_request.data))
        return { };

    return std::get<WTF::String>(m_request.data);
}

void TargetedElementRequest::setSearchText(WTF::String&& searchText)
{
    m_request.data = WTFMove(searchText);
}

WebCore::TargetedElementRequest TargetedElementRequest::makeRequest(const WebKit::WebPageProxy& page) const
{
    auto request = m_request;
    if (std::holds_alternative<WebCore::FloatPoint>(m_request.data))
        request.data = page.protectedPageClient()->webViewToRootView(point());
    return request;
}

} // namespace API
