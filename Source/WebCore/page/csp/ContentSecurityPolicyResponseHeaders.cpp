/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ContentSecurityPolicyResponseHeaders.h"

#include "HTTPHeaderNames.h"
#include "ResourceResponse.h"

namespace WebCore {

ContentSecurityPolicyResponseHeaders::ContentSecurityPolicyResponseHeaders(const ResourceResponse& response)
{
    String policyValue = response.httpHeaderField(HTTPHeaderName::ContentSecurityPolicy);
    if (!policyValue.isEmpty())
        m_headers.append({ policyValue, ContentSecurityPolicyHeaderType::Enforce });

    policyValue = response.httpHeaderField(HTTPHeaderName::ContentSecurityPolicyReportOnly);
    if (!policyValue.isEmpty())
        m_headers.append({ policyValue, ContentSecurityPolicyHeaderType::Report });

    m_httpStatusCode = response.httpStatusCode();
}

ContentSecurityPolicyResponseHeaders ContentSecurityPolicyResponseHeaders::isolatedCopy() const
{
    ContentSecurityPolicyResponseHeaders isolatedCopy;
    isolatedCopy.m_headers.reserveInitialCapacity(m_headers.size());
    for (auto& header : m_headers)
        isolatedCopy.m_headers.uncheckedAppend({ header.first.isolatedCopy(), header.second });
    isolatedCopy.m_httpStatusCode = m_httpStatusCode;
    return isolatedCopy;
}

} // namespace WebCore
