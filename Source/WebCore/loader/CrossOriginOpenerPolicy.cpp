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

#include "ContentSecurityPolicy.h"
#include "CrossOriginEmbedderPolicy.h"
#include "FormData.h"
#include "Frame.h"
#include "HTTPHeaderNames.h"
#include "NavigationRequester.h"
#include "Page.h"
#include "PingLoader.h"
#include "RFC8941.h"
#include "Report.h"
#include "ReportingClient.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "SecurityPolicy.h"
#include "ViolationReportType.h"

namespace WebCore {

static ASCIILiteral crossOriginOpenerPolicyToString(const CrossOriginOpenerPolicyValue& coop)
{
    switch (coop) {
    case CrossOriginOpenerPolicyValue::SameOrigin:
    case CrossOriginOpenerPolicyValue::SameOriginPlusCOEP:
        return "same-origin"_s;
    case CrossOriginOpenerPolicyValue::SameOriginAllowPopups:
        return "same-origin-allow-popups"_s;
    case CrossOriginOpenerPolicyValue::UnsafeNone:
        break;
    }
    return "unsafe-none"_s;
}

static ASCIILiteral crossOriginOpenerPolicyValueToEffectivePolicyString(CrossOriginOpenerPolicyValue coop)
{
    switch (coop) {
    case CrossOriginOpenerPolicyValue::SameOriginAllowPopups:
        return "same-origin-allow-popups"_s;
    case CrossOriginOpenerPolicyValue::SameOrigin:
        return "same-origin"_s;
    case CrossOriginOpenerPolicyValue::SameOriginPlusCOEP:
        return "same-origin-plus-coep"_s;
    case CrossOriginOpenerPolicyValue::UnsafeNone:
        break;
    }
    return "unsafe-none"_s;
}

// https://html.spec.whatwg.org/multipage/origin.html#coop-violation-navigation-to
static void sendViolationReportWhenNavigatingToCOOPResponse(ReportingClient& reportingClient, CrossOriginOpenerPolicy coop, COOPDisposition disposition, const URL& coopURL, const URL& previousResponseURL, const SecurityOrigin& coopOrigin, const SecurityOrigin& previousResponseOrigin, const String& referrer)
{
    auto& endpoint = coop.reportingEndpointForDisposition(disposition);
    if (endpoint.isEmpty())
        return;

    auto report = Report::createReportFormDataForViolation("coop"_s, coopURL, reportingClient.httpUserAgent(), [&](auto& body) {
        body.setString("disposition"_s, disposition == COOPDisposition::Reporting ? "reporting"_s : "enforce"_s);
        body.setString("effectivePolicy"_s, crossOriginOpenerPolicyValueToEffectivePolicyString(disposition == COOPDisposition::Reporting ? coop.reportOnlyValue : coop.value));
        body.setString("previousResponseURL"_s, coopOrigin.isSameOriginAs(previousResponseOrigin) ? PingLoader::sanitizeURLForReport(previousResponseURL) : String());
        body.setString("type"_s, "navigation-to-response"_s);
        body.setString("referrer"_s, referrer);
    });
    reportingClient.sendReportToEndpoints(coopURL, { }, { endpoint }, WTFMove(report), ViolationReportType::CrossOriginOpenerPolicy);
}

// https://html.spec.whatwg.org/multipage/origin.html#coop-violation-navigation-from
static void sendViolationReportWhenNavigatingAwayFromCOOPResponse(ReportingClient& reportingClient, CrossOriginOpenerPolicy coop, COOPDisposition disposition, const URL& coopURL, const URL& nextResponseURL, const SecurityOrigin& coopOrigin, const SecurityOrigin& nextResponseOrigin, bool isCOOPResponseNavigationSource)
{
    auto& endpoint = coop.reportingEndpointForDisposition(disposition);
    if (endpoint.isEmpty())
        return;

    auto report = Report::createReportFormDataForViolation("coop"_s, coopURL, reportingClient.httpUserAgent(), [&](auto& body) {
        body.setString("disposition"_s, disposition == COOPDisposition::Reporting ? "reporting"_s : "enforce"_s);
        body.setString("effectivePolicy"_s, crossOriginOpenerPolicyValueToEffectivePolicyString(disposition == COOPDisposition::Reporting ? coop.reportOnlyValue : coop.value));
        body.setString("nextResponseURL"_s, coopOrigin.isSameOriginAs(nextResponseOrigin) || isCOOPResponseNavigationSource ? PingLoader::sanitizeURLForReport(nextResponseURL) : String());
        body.setString("type"_s, "navigation-from-response"_s);
    });

    reportingClient.sendReportToEndpoints(coopURL, { }, { endpoint }, WTFMove(report), ViolationReportType::CrossOriginOpenerPolicy);
}

// https://html.spec.whatwg.org/multipage/origin.html#check-browsing-context-group-switch-coop-value
static bool checkIfCOOPValuesRequireBrowsingContextGroupSwitch(bool isInitialAboutBlank, CrossOriginOpenerPolicyValue activeDocumentCOOPValue, const SecurityOrigin& activeDocumentNavigationOrigin, CrossOriginOpenerPolicyValue responseCOOPValue, const SecurityOrigin& responseOrigin)
{
    // If the result of matching activeDocumentCOOPValue, activeDocumentNavigationOrigin, responseCOOPValue, and responseOrigin is true, return false.
    // https://html.spec.whatwg.org/multipage/origin.html#matching-coop
    if (activeDocumentCOOPValue == CrossOriginOpenerPolicyValue::UnsafeNone && responseCOOPValue == CrossOriginOpenerPolicyValue::UnsafeNone)
        return false;
    if (activeDocumentCOOPValue == responseCOOPValue && activeDocumentNavigationOrigin.isSameOriginAs(responseOrigin))
        return false;

    // If all of the following are true:
    // - isInitialAboutBlank,
    // - activeDocumentCOOPValue's value is "same-origin-allow-popups".
    // - responseCOOPValue is "unsafe-none",
    // then return false.
    if (isInitialAboutBlank && activeDocumentCOOPValue == CrossOriginOpenerPolicyValue::SameOriginAllowPopups && responseCOOPValue == CrossOriginOpenerPolicyValue::UnsafeNone)
        return false;

    return true;
}

// https://html.spec.whatwg.org/multipage/origin.html#check-bcg-switch-navigation-report-only
static bool checkIfEnforcingReportOnlyCOOPWouldRequireBrowsingContextGroupSwitch(bool isInitialAboutBlank, const CrossOriginOpenerPolicy& activeDocumentCOOP, const SecurityOrigin& activeDocumentNavigationOrigin, const CrossOriginOpenerPolicy& responseCOOP, const SecurityOrigin& responseOrigin)
{
    if (!checkIfCOOPValuesRequireBrowsingContextGroupSwitch(isInitialAboutBlank, activeDocumentCOOP.reportOnlyValue, activeDocumentNavigationOrigin, responseCOOP.reportOnlyValue, responseOrigin))
        return false;

    if (checkIfCOOPValuesRequireBrowsingContextGroupSwitch(isInitialAboutBlank, activeDocumentCOOP.reportOnlyValue, activeDocumentNavigationOrigin, responseCOOP.value, responseOrigin))
        return true;

    if (checkIfCOOPValuesRequireBrowsingContextGroupSwitch(isInitialAboutBlank, activeDocumentCOOP.value, activeDocumentNavigationOrigin, responseCOOP.reportOnlyValue, responseOrigin))
        return true;

    return false;
}

static std::tuple<Ref<SecurityOrigin>, CrossOriginOpenerPolicy> computeResponseOriginAndCOOP(const ResourceResponse& response, const std::optional<NavigationRequester>& requester, ContentSecurityPolicy* responseCSP)
{
    // Non-initial empty documents (about:blank) should inherit their cross-origin-opener-policy from the navigation's initiator top level document,
    // if the initiator and its top level document are same-origin, or default (unsafe-none) otherwise.
    // https://github.com/whatwg/html/issues/6913
    if (SecurityPolicy::shouldInheritSecurityOriginFromOwner(response.url()) && requester)
        return std::make_tuple(requester->securityOrigin, requester->securityOrigin->isSameOriginAs(requester->topOrigin) ? requester->policyContainer.crossOriginOpenerPolicy : CrossOriginOpenerPolicy { });

    // If the HTTP response contains a CSP header, it may set sandbox flags, which would cause the origin to become opaque.
    auto responseOrigin = responseCSP && responseCSP->sandboxFlags() != SandboxNone ? SecurityOrigin::createOpaque() : SecurityOrigin::create(response.url());
    return std::make_tuple(WTFMove(responseOrigin), obtainCrossOriginOpenerPolicy(response));
}

// https://html.spec.whatwg.org/multipage/origin.html#coop-enforce
static CrossOriginOpenerPolicyEnforcementResult enforceResponseCrossOriginOpenerPolicy(ReportingClient& reportingClient, const CrossOriginOpenerPolicyEnforcementResult& currentCoopEnforcementResult, const URL& responseURL, SecurityOrigin& responseOrigin, const CrossOriginOpenerPolicy& responseCOOP, const String& referrer, bool isDisplayingInitialEmptyDocument)
{
    CrossOriginOpenerPolicyEnforcementResult newCOOPEnforcementResult = {
        responseURL,
        responseOrigin,
        responseCOOP,
        true /* isCurrentContextNavigationSource */,
        currentCoopEnforcementResult.needsBrowsingContextGroupSwitch,
        currentCoopEnforcementResult.needsBrowsingContextGroupSwitchDueToReportOnly
    };

    if (checkIfCOOPValuesRequireBrowsingContextGroupSwitch(isDisplayingInitialEmptyDocument, currentCoopEnforcementResult.crossOriginOpenerPolicy.value, currentCoopEnforcementResult.currentOrigin, responseCOOP.value, responseOrigin)) {
        newCOOPEnforcementResult.needsBrowsingContextGroupSwitch = true;
        sendViolationReportWhenNavigatingToCOOPResponse(reportingClient, responseCOOP, COOPDisposition::Enforce, responseURL, currentCoopEnforcementResult.url, responseOrigin, currentCoopEnforcementResult.currentOrigin, referrer);
        sendViolationReportWhenNavigatingAwayFromCOOPResponse(reportingClient, currentCoopEnforcementResult.crossOriginOpenerPolicy, COOPDisposition::Enforce, currentCoopEnforcementResult.url, responseURL, currentCoopEnforcementResult.currentOrigin, responseOrigin, currentCoopEnforcementResult.isCurrentContextNavigationSource);
    }

    if (checkIfEnforcingReportOnlyCOOPWouldRequireBrowsingContextGroupSwitch(isDisplayingInitialEmptyDocument, currentCoopEnforcementResult.crossOriginOpenerPolicy, currentCoopEnforcementResult.currentOrigin, responseCOOP, responseOrigin)) {
        newCOOPEnforcementResult.needsBrowsingContextGroupSwitchDueToReportOnly = true;
        sendViolationReportWhenNavigatingToCOOPResponse(reportingClient, responseCOOP, COOPDisposition::Reporting, responseURL, currentCoopEnforcementResult.url, responseOrigin, currentCoopEnforcementResult.currentOrigin, referrer);
        sendViolationReportWhenNavigatingAwayFromCOOPResponse(reportingClient, currentCoopEnforcementResult.crossOriginOpenerPolicy, COOPDisposition::Reporting, currentCoopEnforcementResult.url, responseURL, currentCoopEnforcementResult.currentOrigin, responseOrigin, currentCoopEnforcementResult.isCurrentContextNavigationSource);
    }

    return newCOOPEnforcementResult;
}

// https://html.spec.whatwg.org/multipage/origin.html#obtain-coop
CrossOriginOpenerPolicy obtainCrossOriginOpenerPolicy(const ResourceResponse& response)
{
    std::optional<CrossOriginEmbedderPolicy> coep;
    auto ensureCOEP = [&coep, &response]() -> CrossOriginEmbedderPolicy& {
        if (!coep)
            coep = obtainCrossOriginEmbedderPolicy(response, nullptr);
        return *coep;
    };
    auto parseCOOP = [&response, &ensureCOEP](HTTPHeaderName headerName, auto& value, auto& reportingEndpoint) {
        auto coopParsingResult = RFC8941::parseItemStructuredFieldValue(response.httpHeaderField(headerName));
        if (!coopParsingResult)
            return;

        auto* policyString = std::get_if<RFC8941::Token>(&coopParsingResult->first);
        if (!policyString)
            return;

        if (policyString->string() == "same-origin"_s) {
            auto& coep = ensureCOEP();
            if (coep.value == CrossOriginEmbedderPolicyValue::RequireCORP || (headerName == HTTPHeaderName::CrossOriginOpenerPolicyReportOnly && coep.reportOnlyValue == CrossOriginEmbedderPolicyValue::RequireCORP))
                value = CrossOriginOpenerPolicyValue::SameOriginPlusCOEP;
            else
                value = CrossOriginOpenerPolicyValue::SameOrigin;
        } else if (policyString->string() == "same-origin-allow-popups"_s)
            value = CrossOriginOpenerPolicyValue::SameOriginAllowPopups;

        if (auto* reportToString = coopParsingResult->second.getIf<String>("report-to"_s))
            reportingEndpoint = *reportToString;
    };

    CrossOriginOpenerPolicy policy;
    if (!SecurityOrigin::create(response.url())->isPotentiallyTrustworthy())
        return policy;

    parseCOOP(HTTPHeaderName::CrossOriginOpenerPolicy, policy.value, policy.reportingEndpoint);
    parseCOOP(HTTPHeaderName::CrossOriginOpenerPolicyReportOnly, policy.reportOnlyValue, policy.reportOnlyReportingEndpoint);
    return policy;
}

CrossOriginOpenerPolicy CrossOriginOpenerPolicy::isolatedCopy() const &
{
    return { value, reportingEndpoint.isolatedCopy(), reportOnlyValue, reportOnlyReportingEndpoint.isolatedCopy() };
}

CrossOriginOpenerPolicy CrossOriginOpenerPolicy::isolatedCopy() &&
{
    return { value, WTFMove(reportingEndpoint).isolatedCopy(), reportOnlyValue, WTFMove(reportOnlyReportingEndpoint).isolatedCopy() };
}

void addCrossOriginOpenerPolicyHeaders(ResourceResponse& response, const CrossOriginOpenerPolicy& coop)
{
    if (coop.value != CrossOriginOpenerPolicyValue::UnsafeNone) {
        if (coop.reportingEndpoint.isEmpty())
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginOpenerPolicy, crossOriginOpenerPolicyToString(coop.value));
        else
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginOpenerPolicy, makeString(crossOriginOpenerPolicyToString(coop.value), "; report-to=\"", coop.reportingEndpoint, '\"'));
    }
    if (coop.reportOnlyValue != CrossOriginOpenerPolicyValue::UnsafeNone) {
        if (coop.reportOnlyReportingEndpoint.isEmpty())
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginOpenerPolicyReportOnly, crossOriginOpenerPolicyToString(coop.reportOnlyValue));
        else
            response.setHTTPHeaderField(HTTPHeaderName::CrossOriginOpenerPolicyReportOnly, makeString(crossOriginOpenerPolicyToString(coop.reportOnlyValue), "; report-to=\"", coop.reportOnlyReportingEndpoint, '\"'));
    }
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch (Step 13.5.6)
std::optional<CrossOriginOpenerPolicyEnforcementResult> doCrossOriginOpenerHandlingOfResponse(ReportingClient& reportingClient, const ResourceResponse& response, const std::optional<NavigationRequester>& requester, ContentSecurityPolicy* responseCSP, SandboxFlags effectiveSandboxFlags, const String& referrer, bool isDisplayingInitialEmptyDocument, const CrossOriginOpenerPolicyEnforcementResult& currentCoopEnforcementResult)
{
    auto [responseOrigin, responseCOOP] = computeResponseOriginAndCOOP(response, requester, responseCSP);

    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch (Step 13.5.6.2)
    // If sandboxFlags is not empty and responseCOOP's value is not "unsafe-none", then set response to an appropriate network error and break.
    if (responseCOOP.value != CrossOriginOpenerPolicyValue::UnsafeNone && effectiveSandboxFlags != SandboxNone)
        return std::nullopt;

    return enforceResponseCrossOriginOpenerPolicy(reportingClient, currentCoopEnforcementResult, response.url(), responseOrigin, responseCOOP, referrer, isDisplayingInitialEmptyDocument);
}

CrossOriginOpenerPolicyEnforcementResult CrossOriginOpenerPolicyEnforcementResult::from(const URL& currentURL, Ref<SecurityOrigin>&& currentOrigin, const CrossOriginOpenerPolicy& crossOriginOpenerPolicy, std::optional<NavigationRequester> requester, const URL& openerURL)
{
    CrossOriginOpenerPolicyEnforcementResult result { currentURL, WTFMove(currentOrigin), crossOriginOpenerPolicy };
    result.isCurrentContextNavigationSource = requester && result.currentOrigin->isSameOriginAs(requester->securityOrigin);
    if (SecurityPolicy::shouldInheritSecurityOriginFromOwner(currentURL) && openerURL.isValid())
        result.url = openerURL;
    return result;
}

} // namespace WebCore
