/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DocumentIsolationPolicy.h"

#include "HTTPHeaderNames.h"
#include "RFC8941.h"
#include "ResourceResponse.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

DocumentIsolationPolicy obtainDocumentIsolationPolicy(const ResourceResponse& response, IsSecureContext isSecureContext)
{
    return obtainDocumentIsolationPolicy(response.url(), parseDocumentIsolationPolicy(response), isSecureContext);
}

DocumentIsolationPolicy obtainDocumentIsolationPolicy(const URL& url, DocumentIsolationPolicy headerPolicy, IsSecureContext isSecureContext)
{
    // FIXME: Get the blob URL entry's policy container directly instead of reading the headers it is reflected as.
    if (isSecureContext == IsSecureContext::No && !url.protocolIsBlob())
        return { };
    return headerPolicy;
}

// As for Cross-Origin-Embedder-Policy, report-to is only used with a policy value and has to be a string.
// https://github.com/WICG/document-isolation-policy/issues/10
static std::pair<DocumentIsolationPolicyValue, String> parseDocumentIsolationPolicyHeader(StringView headerValue)
{
    auto parsedItem = RFC8941::parseItemStructuredFieldValue(headerValue);
    if (!parsedItem)
        return { };

    // FIXME: Support isolate-and-credentialless.
    auto* token = std::get_if<RFC8941::Token>(&parsedItem->first);
    if (!token || token->string() != "isolate-and-require-corp"_s)
        return { };

    auto* reportTo = parsedItem->second.getIf<String>("report-to"_s);
    return { DocumentIsolationPolicyValue::IsolateAndRequireCORP, reportTo ? *reportTo : String { } };
}

DocumentIsolationPolicy parseDocumentIsolationPolicy(const ResourceResponse& response)
{
    auto [value, reportingEndpoint] = parseDocumentIsolationPolicyHeader(response.httpHeaderField(HTTPHeaderName::DocumentIsolationPolicy));
    auto [reportOnlyValue, reportOnlyReportingEndpoint] = parseDocumentIsolationPolicyHeader(response.httpHeaderField(HTTPHeaderName::DocumentIsolationPolicyReportOnly));
    return { value, reportOnlyValue, WTF::move(reportingEndpoint), WTF::move(reportOnlyReportingEndpoint) };
}

void DocumentIsolationPolicy::addPolicyHeadersTo(ResourceResponse& response) const
{
    auto addHeader = [&response](HTTPHeaderName headerName, DocumentIsolationPolicyValue value, const String& reportingEndpoint) {
        if (value == DocumentIsolationPolicyValue::None)
            return;
        ASSERT(value == DocumentIsolationPolicyValue::IsolateAndRequireCORP);
        auto escapedReportingEndpoint = reportingEndpoint.isEmpty() ? std::nullopt : RFC8941::escapeString(reportingEndpoint);
        if (!escapedReportingEndpoint)
            response.setHTTPHeaderField(headerName, "isolate-and-require-corp"_s);
        else
            response.setHTTPHeaderField(headerName, makeString("isolate-and-require-corp; report-to=\""_s, *escapedReportingEndpoint, '"'));
    };
    addHeader(HTTPHeaderName::DocumentIsolationPolicy, value, reportingEndpoint);
    addHeader(HTTPHeaderName::DocumentIsolationPolicyReportOnly, reportOnlyValue, reportOnlyReportingEndpoint);
}

} // namespace WebCore
