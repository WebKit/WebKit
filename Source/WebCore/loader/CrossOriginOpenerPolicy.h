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

#pragma once

#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ContentSecurityPolicy;
class LocalFrame;
class ResourceResponse;
class ScriptExecutionContext;
class SecurityOrigin;

struct NavigationRequester;
struct ReportingClient;

typedef int SandboxFlags;

// https://html.spec.whatwg.org/multipage/origin.html#cross-origin-opener-policy-value
enum class CrossOriginOpenerPolicyValue : uint8_t {
    UnsafeNone,
    SameOrigin,
    SameOriginPlusCOEP,
    SameOriginAllowPopups
};

enum class COOPDisposition : bool { Reporting , Enforce };

// https://html.spec.whatwg.org/multipage/origin.html#cross-origin-opener-policy
struct CrossOriginOpenerPolicy {
    CrossOriginOpenerPolicyValue value { CrossOriginOpenerPolicyValue::UnsafeNone };
    CrossOriginOpenerPolicyValue reportOnlyValue { CrossOriginOpenerPolicyValue::UnsafeNone };

    String reportingEndpoint;
    String reportOnlyReportingEndpoint;

    const String& reportingEndpointForDisposition(COOPDisposition) const;
    bool hasReportingEndpoint(COOPDisposition) const;

    CrossOriginOpenerPolicy isolatedCopy() const &;
    CrossOriginOpenerPolicy isolatedCopy() &&;

    void addPolicyHeadersTo(ResourceResponse&) const;
};

inline const String& CrossOriginOpenerPolicy::reportingEndpointForDisposition(COOPDisposition disposition) const
{
    return disposition == COOPDisposition::Enforce ? reportingEndpoint : reportOnlyReportingEndpoint;
}

inline bool CrossOriginOpenerPolicy::hasReportingEndpoint(COOPDisposition disposition) const
{
    return !reportingEndpointForDisposition(disposition).isEmpty();
}

inline bool operator==(const CrossOriginOpenerPolicy& a, const CrossOriginOpenerPolicy& b)
{
    return a.value == b.value && a.reportOnlyValue == b.reportOnlyValue && a.reportingEndpoint == b.reportingEndpoint && a.reportOnlyReportingEndpoint == b.reportOnlyReportingEndpoint;
}

// https://html.spec.whatwg.org/multipage/origin.html#coop-enforcement-result
struct CrossOriginOpenerPolicyEnforcementResult {
    WEBCORE_EXPORT static CrossOriginOpenerPolicyEnforcementResult from(const URL& currentURL, Ref<SecurityOrigin>&& currentOrigin, const CrossOriginOpenerPolicy&, std::optional<NavigationRequester>, const URL& openerURL);

    URL url;
    Ref<SecurityOrigin> currentOrigin;
    CrossOriginOpenerPolicy crossOriginOpenerPolicy;
    bool isCurrentContextNavigationSource { true };
    bool needsBrowsingContextGroupSwitch { false };
    bool needsBrowsingContextGroupSwitchDueToReportOnly { false };
};

CrossOriginOpenerPolicy obtainCrossOriginOpenerPolicy(const ResourceResponse&);
WEBCORE_EXPORT std::optional<CrossOriginOpenerPolicyEnforcementResult> doCrossOriginOpenerHandlingOfResponse(ReportingClient&, const ResourceResponse&, const std::optional<NavigationRequester>&, ContentSecurityPolicy* responseCSP, SandboxFlags effectiveSandboxFlags, const String& referrer, bool isDisplayingInitialEmptyDocument, const CrossOriginOpenerPolicyEnforcementResult& currentCoopEnforcementResult);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::CrossOriginOpenerPolicyValue> {
    using values = EnumValues<
    WebCore::CrossOriginOpenerPolicyValue,
    WebCore::CrossOriginOpenerPolicyValue::UnsafeNone,
    WebCore::CrossOriginOpenerPolicyValue::SameOrigin,
    WebCore::CrossOriginOpenerPolicyValue::SameOriginPlusCOEP,
    WebCore::CrossOriginOpenerPolicyValue::SameOriginAllowPopups
    >;
};

} // namespace WTF
