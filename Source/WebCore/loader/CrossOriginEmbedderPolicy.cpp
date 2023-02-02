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
#include "CrossOriginEmbedderPolicy.h"

#include "COEPInheritenceViolationReportBody.h"
#include "CORPViolationReportBody.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPHeaderNames.h"
#include "JSFetchRequestDestination.h"
#include "PingLoader.h"
#include "RFC8941.h"
#include "Report.h"
#include "ReportingClient.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ViolationReportType.h"
#include <wtf/persistence/PersistentCoders.h>

namespace WebCore {

// https://html.spec.whatwg.org/multipage/origin.html#obtain-an-embedder-policy
CrossOriginEmbedderPolicy obtainCrossOriginEmbedderPolicy(const ResourceResponse& response, const ScriptExecutionContext* context)
{
    auto parseCOEPHeader = [&response](HTTPHeaderName headerName, auto& value, auto& reportingEndpoint) {
        auto coepParsingResult = RFC8941::parseItemStructuredFieldValue(response.httpHeaderField(headerName));
        if (!coepParsingResult)
            return;
        auto* policyString = std::get_if<RFC8941::Token>(&coepParsingResult->first);
        if (!policyString || policyString->string() != "require-corp"_s)
            return;

        value = CrossOriginEmbedderPolicyValue::RequireCORP;
        if (auto* reportToString = coepParsingResult->second.getIf<String>("report-to"_s))
            reportingEndpoint = *reportToString;
    };

    CrossOriginEmbedderPolicy policy;
    if (context && !context->settingsValues().crossOriginEmbedderPolicyEnabled)
        return policy;
    if (!SecurityOrigin::create(response.url())->isPotentiallyTrustworthy())
        return policy;

    parseCOEPHeader(HTTPHeaderName::CrossOriginEmbedderPolicy, policy.value, policy.reportingEndpoint);
    parseCOEPHeader(HTTPHeaderName::CrossOriginEmbedderPolicyReportOnly, policy.reportOnlyValue, policy.reportOnlyReportingEndpoint);
    return policy;
}

CrossOriginEmbedderPolicy CrossOriginEmbedderPolicy::isolatedCopy() const &
{
    return {
        value,
        reportOnlyValue,
        reportingEndpoint.isolatedCopy(),
        reportOnlyReportingEndpoint.isolatedCopy()
    };
}

CrossOriginEmbedderPolicy CrossOriginEmbedderPolicy::isolatedCopy() &&
{
    return {
        value,
        reportOnlyValue,
        WTFMove(reportingEndpoint).isolatedCopy(),
        WTFMove(reportOnlyReportingEndpoint).isolatedCopy()
    };
}

void CrossOriginEmbedderPolicy::addPolicyHeadersTo(ResourceResponse& response) const
{
    if (value != CrossOriginEmbedderPolicyValue::UnsafeNone) {
        ASSERT(value == CrossOriginEmbedderPolicyValue::RequireCORP);
        if (reportingEndpoint.isEmpty())
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginEmbedderPolicy, "require-corp"_s);
        else
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginEmbedderPolicy, makeString("require-corp; report-to=\"", reportingEndpoint, '\"'));
    }
    if (reportOnlyValue != CrossOriginEmbedderPolicyValue::UnsafeNone) {
        ASSERT(reportOnlyValue == CrossOriginEmbedderPolicyValue::RequireCORP);
        if (reportOnlyReportingEndpoint.isEmpty())
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginEmbedderPolicyReportOnly, "require-corp"_s);
        else
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginEmbedderPolicyReportOnly, makeString("require-corp; report-to=\"", reportOnlyReportingEndpoint, '\"'));
    }
}

// https://html.spec.whatwg.org/multipage/origin.html#queue-a-cross-origin-embedder-policy-inheritance-violation
void sendCOEPInheritenceViolation(ReportingClient& reportingClient, const URL& embedderURL, const String& endpoint, COEPDisposition disposition, const String& type, const URL& blockedURL)
{
    auto reportBody = COEPInheritenceViolationReportBody::create(disposition, blockedURL, AtomString { type });
    auto report = Report::create("coep"_s, embedderURL.string(), WTFMove(reportBody));
    reportingClient.notifyReportObservers(WTFMove(report));

    if (endpoint.isEmpty())
        return;

    auto reportFormData = Report::createReportFormDataForViolation("coep"_s, embedderURL, reportingClient.httpUserAgent(), endpoint, [&](auto& body) {
        body.setString("disposition"_s, disposition == COEPDisposition::Reporting ? "reporting"_s : "enforce"_s);
        body.setString("type"_s, type);
        body.setString("blockedURL"_s, PingLoader::sanitizeURLForReport(blockedURL));
    });
    reportingClient.sendReportToEndpoints(embedderURL, { }, { endpoint }, WTFMove(reportFormData), ViolationReportType::COEPInheritenceViolation);
}

// https://fetch.spec.whatwg.org/#queue-a-cross-origin-embedder-policy-corp-violation-report
void sendCOEPCORPViolation(ReportingClient& reportingClient, const URL& embedderURL, const String& endpoint, COEPDisposition disposition, FetchOptions::Destination destination, const URL& blockedURL)
{
    auto reportBody = CORPViolationReportBody::create(disposition, blockedURL, destination);
    auto report = Report::create("coep"_s, embedderURL.string(), WTFMove(reportBody));
    reportingClient.notifyReportObservers(WTFMove(report));

    if (endpoint.isEmpty())
        return;

    auto reportFormData = Report::createReportFormDataForViolation("coep"_s, embedderURL, reportingClient.httpUserAgent(), endpoint, [&](auto& body) {
        body.setString("disposition"_s, disposition == COEPDisposition::Reporting ? "reporting"_s : "enforce"_s);
        body.setString("type"_s, "corp"_s);
        body.setString("blockedURL"_s, PingLoader::sanitizeURLForReport(blockedURL));
        body.setString("destination"_s, convertEnumerationToString(destination));
    });
    reportingClient.sendReportToEndpoints(embedderURL, { }, { endpoint }, WTFMove(reportFormData), ViolationReportType::CORPViolation);
}

void CrossOriginEmbedderPolicy::encode(WTF::Persistence::Encoder& encoder) const
{
    encoder << value << reportingEndpoint << reportOnlyValue << reportOnlyReportingEndpoint;
}

std::optional<CrossOriginEmbedderPolicy> CrossOriginEmbedderPolicy::decode(WTF::Persistence::Decoder& decoder)
{
    std::optional<CrossOriginEmbedderPolicyValue> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    std::optional<String> reportingEndpoint;
    decoder >> reportingEndpoint;
    if (!reportingEndpoint)
        return std::nullopt;

    std::optional<CrossOriginEmbedderPolicyValue> reportOnlyValue;
    decoder >> reportOnlyValue;
    if (!reportOnlyValue)
        return std::nullopt;

    std::optional<String> reportOnlyReportingEndpoint;
    decoder >> reportOnlyReportingEndpoint;
    if (!reportOnlyReportingEndpoint)
        return std::nullopt;

    return { {
        *value,
        *reportOnlyValue,
        WTFMove(*reportingEndpoint),
        WTFMove(*reportOnlyReportingEndpoint)
    } };
}

} // namespace WebCore
