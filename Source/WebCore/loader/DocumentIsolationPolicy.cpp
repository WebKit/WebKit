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

#include "CORPViolationReportBody.h"
#include "CrossOriginEmbedderPolicy.h"
#include "FormData.h"
#include "HTTPHeaderNames.h"
#include "JSFetchRequestDestination.h"
#include "RFC8941.h"
#include "Report.h"
#include "ReportingClient.h"
#include "ResourceResponse.h"
#include "ViolationReportType.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

static constexpr auto isolateAndRequireCORPToken { "isolate-and-require-corp"_s };
static constexpr auto isolateAndCredentiallessToken { "isolate-and-credentialless"_s };

DocumentIsolationPolicy obtainDocumentIsolationPolicy(const ResourceResponse& response, IsSecureContext isSecureContext, CredentiallessSupport credentiallessSupport)
{
    return obtainDocumentIsolationPolicy(response.url(), parseDocumentIsolationPolicy(response), isSecureContext, credentiallessSupport);
}

DocumentIsolationPolicy obtainDocumentIsolationPolicy(const URL& url, DocumentIsolationPolicy headerPolicy, IsSecureContext isSecureContext, CredentiallessSupport credentiallessSupport)
{
    // FIXME: Get the blob URL entry's policy container directly instead of reading the headers it is reflected as.
    if (isSecureContext == IsSecureContext::No && !url.protocolIsBlob())
        return { };
    if (credentiallessSupport == CredentiallessSupport::No) {
        if (headerPolicy.value == DocumentIsolationPolicyValue::IsolateAndCredentialless) {
            headerPolicy.value = DocumentIsolationPolicyValue::None;
            headerPolicy.reportingEndpoint = nullptr;
        }
        if (headerPolicy.reportOnlyValue == DocumentIsolationPolicyValue::IsolateAndCredentialless) {
            headerPolicy.reportOnlyValue = DocumentIsolationPolicyValue::None;
            headerPolicy.reportOnlyReportingEndpoint = nullptr;
        }
    }
    return headerPolicy;
}

// As for Cross-Origin-Embedder-Policy, report-to is only used with a policy value and has to be a string.
// https://github.com/WICG/document-isolation-policy/issues/10
static std::pair<DocumentIsolationPolicyValue, RefPtr<StringImpl>> parseDocumentIsolationPolicyHeader(StringView headerValue)
{
    auto parsedItem = RFC8941::parseItemStructuredFieldValue(headerValue);
    if (!parsedItem)
        return { };

    auto* token = std::get_if<RFC8941::Token>(&parsedItem->first);
    if (!token)
        return { };

    auto value = [&] {
        if (equal(token->string(), isolateAndRequireCORPToken))
            return DocumentIsolationPolicyValue::IsolateAndRequireCORP;
        if (equal(token->string(), isolateAndCredentiallessToken))
            return DocumentIsolationPolicyValue::IsolateAndCredentialless;
        return DocumentIsolationPolicyValue::None;
    }();
    if (value == DocumentIsolationPolicyValue::None)
        return { };

    auto* reportTo = parsedItem->second.getIf<RefPtr<StringImpl>>("report-to"_s);
    return { value, reportTo ? *reportTo : nullptr };
}

DocumentIsolationPolicy parseDocumentIsolationPolicy(const ResourceResponse& response)
{
    auto [value, reportingEndpoint] = parseDocumentIsolationPolicyHeader(response.httpHeaderField(HTTPHeaderName::DocumentIsolationPolicy));
    auto [reportOnlyValue, reportOnlyReportingEndpoint] = parseDocumentIsolationPolicyHeader(response.httpHeaderField(HTTPHeaderName::DocumentIsolationPolicyReportOnly));
    return { value, reportOnlyValue, WTF::move(reportingEndpoint), WTF::move(reportOnlyReportingEndpoint) };
}

void DocumentIsolationPolicy::addPolicyHeadersTo(ResourceResponse& response) const
{
    auto addHeader = [&response](HTTPHeaderName headerName, DocumentIsolationPolicyValue value, StringImpl* reportingEndpoint) {
        if (value == DocumentIsolationPolicyValue::None)
            return;
        auto token = value == DocumentIsolationPolicyValue::IsolateAndCredentialless ? isolateAndCredentiallessToken : isolateAndRequireCORPToken;
        auto escapedReportingEndpoint = isEmpty(reportingEndpoint) ? std::nullopt : RFC8941::escapeString(reportingEndpoint);
        if (!escapedReportingEndpoint)
            response.setHTTPHeaderField(headerName, StringImpl::create(token).ptr());
        else
            response.setHTTPHeaderField(headerName, makeString(token, "; report-to=\""_s, *escapedReportingEndpoint, '"'));
    };
    addHeader(HTTPHeaderName::DocumentIsolationPolicy, value, reportingEndpoint);
    addHeader(HTTPHeaderName::DocumentIsolationPolicyReportOnly, reportOnlyValue, reportOnlyReportingEndpoint);
}

void sendDocumentIsolationPolicyCORPViolation(ReportingClient& reportingClient, const URL& contextURL, StringImpl* endpoint, COEPDisposition disposition, FetchOptions::Destination destination, const URL& blockedURL)
{
    // https://fetch.spec.whatwg.org/#serialize-a-response-url-for-reporting
    URL serializedBlockedURL = blockedURL;
    serializedBlockedURL.removeCredentials();
    serializedBlockedURL.removeFragmentIdentifier();

    Ref reportBody = CORPViolationReportBody::create(disposition, serializedBlockedURL, destination);
    Ref report = Report::create("dip"_si, contextURL.string(), WTF::move(reportBody));
    reportingClient.notifyReportObservers(WTF::move(report));

    if (isEmpty(endpoint))
        return;

    Ref reportFormData = Report::createReportFormDataForViolation("dip"_si, contextURL, reportingClient.httpUserAgent(), endpoint, [&](auto& body) {
        body.setString("type"_si, "corp"_si);
        body.setString("blockedURL"_si, serializedBlockedURL.string());
        body.setString("destination"_si, convertEnumerationToString(destination));
        body.setString("disposition"_si, disposition == COEPDisposition::Reporting ? "reporting"_si : "enforce"_si);
    });
    RefPtr<StringImpl> endpointToken { endpoint };
    reportingClient.sendReportToEndpoints(contextURL, { }, singleElementSpan(endpointToken), WTF::move(reportFormData), ViolationReportType::CORPViolation);
}

} // namespace WebCore
