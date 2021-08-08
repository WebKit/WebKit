/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "CrossOriginOpenerPolicy.h"

#include "CrossOriginEmbedderPolicy.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

// https://html.spec.whatwg.org/multipage/origin.html#obtain-coop
CrossOriginOpenerPolicy obtainCrossOriginOpenerPolicy(const ResourceResponse& response, const ScriptExecutionContext& context)
{
    std::optional<CrossOriginEmbedderPolicy> coep;
    auto ensureCOEP = [&coep, &response, &context]() -> CrossOriginEmbedderPolicy& {
        if (!coep)
            coep = obtainCrossOriginEmbedderPolicy(response, context);
        return *coep;
    };
    auto parseCOOP = [&response, &ensureCOEP](HTTPHeaderName headerName, auto& value, auto& reportingEndpoint) {
        auto coopParsingResult = parseStructuredFieldValue(response.httpHeaderField(headerName));
        if (!coopParsingResult)
            return;

        if (coopParsingResult->first == "same-origin") {
            auto& coep = ensureCOEP();
            if (coep.value == CrossOriginEmbedderPolicyValue::RequireCORP || (headerName == HTTPHeaderName::CrossOriginOpenerPolicyReportOnly && coep.reportOnlyValue == CrossOriginEmbedderPolicyValue::RequireCORP))
                value = CrossOriginOpenerPolicyValue::SameOriginPlusCOEP;
            else
                value = CrossOriginOpenerPolicyValue::SameOrigin;
        } else if (coopParsingResult->first == "same-origin-allow-popups")
            value = CrossOriginOpenerPolicyValue::SameOriginAllowPopups;

        reportingEndpoint = coopParsingResult->second.get("report-to"_s);
    };

    CrossOriginOpenerPolicy policy;
    // FIXME: about:blank should be marked as secure as per https://w3c.github.io/webappsec-secure-contexts/#potentially-trustworthy-url.
    if (!context.isSecureContext() && context.url() != aboutBlankURL())
        return policy;

    parseCOOP(HTTPHeaderName::CrossOriginOpenerPolicy, policy.value, policy.reportingEndpoint);
    parseCOOP(HTTPHeaderName::CrossOriginOpenerPolicyReportOnly, policy.reportOnlyValue, policy.reportOnlyReportingEndpoint);
    return policy;
}

} // namespace WebCore
