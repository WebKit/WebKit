/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContentSecurityPolicyResponseHeaders.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

namespace JSC {
class ExecState;
}

namespace WTF {
class OrdinalNumber;
}

namespace WebCore {

class ContentSecurityPolicyDirective;
class ContentSecurityPolicyDirectiveList;
class ContentSecurityPolicySource;
class DOMStringList;
class Frame;
class JSDOMWindowShell;
class ResourceRequest;
class ScriptExecutionContext;
class SecurityOrigin;
class TextEncoding;
class URL;

enum class ContentSecurityPolicyHashAlgorithm;

typedef Vector<std::unique_ptr<ContentSecurityPolicyDirectiveList>> CSPDirectiveListVector;
typedef int SandboxFlags;

class ContentSecurityPolicy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ContentSecurityPolicy(ScriptExecutionContext&);
    explicit ContentSecurityPolicy(const SecurityOrigin&, const Frame* = nullptr);
    ~ContentSecurityPolicy();

    void copyStateFrom(const ContentSecurityPolicy*);
    void copyUpgradeInsecureRequestStateFrom(const ContentSecurityPolicy&);

    void didCreateWindowShell(JSDOMWindowShell&) const;

    enum class PolicyFrom {
        HTTPEquivMeta,
        HTTPHeader,
        Inherited,
    };
    ContentSecurityPolicyResponseHeaders responseHeaders() const;
    enum ReportParsingErrors { No, Yes };
    void didReceiveHeaders(const ContentSecurityPolicyResponseHeaders&, ReportParsingErrors = ReportParsingErrors::Yes);
    void processHTTPEquiv(const String& content, ContentSecurityPolicyHeaderType type) { didReceiveHeader(content, type, ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta); }

    bool allowScriptWithNonce(const String& nonce, bool overrideContentSecurityPolicy = false) const;
    bool allowStyleWithNonce(const String& nonce, bool overrideContentSecurityPolicy = false) const;

    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy = false) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy = false) const;
    bool allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& scriptContent, bool overrideContentSecurityPolicy = false) const;
    bool allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& styleContent, bool overrideContentSecurityPolicy = false) const;

    bool allowEval(JSC::ExecState*, bool overrideContentSecurityPolicy = false) const;

    bool allowPluginType(const String& type, const String& typeAttribute, const URL&, bool overrideContentSecurityPolicy = false) const;

    bool allowFrameAncestors(const Frame&, const URL&, bool overrideContentSecurityPolicy = false) const;

    enum class RedirectResponseReceived { No, Yes };
    bool allowScriptFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowChildFrameFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowChildContextFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowImageFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowStyleFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowFontFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowMediaFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowConnectToSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowFormAction(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;

    bool allowObjectFromSource(const URL&, bool overrideContentSecurityPolicy = false, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowBaseURI(const URL&, bool overrideContentSecurityPolicy = false) const;

    void setOverrideAllowInlineStyle(bool);

    void gatherReportURIs(DOMStringList&) const;

    bool experimentalFeaturesEnabled() const;

    // The following functions are used by internal data structures to call back into this object when parsing, validating,
    // and applying a Content Security Policy.
    // FIXME: We should make the various directives serve only as state stores for the parsed policy and remove these functions.
    // This class should traverse the directives, validating the policy, and applying it to the script execution context.

    // Used by ContentSecurityPolicyMediaListDirective
    void reportInvalidPluginTypes(const String&) const;

    // Used by ContentSecurityPolicySourceList
    void reportDirectiveAsSourceExpression(const String& directiveName, const String& sourceExpression) const;
    void reportInvalidPathCharacter(const String& directiveName, const String& value, const char) const;
    void reportInvalidSourceExpression(const String& directiveName, const String& source) const;
    bool urlMatchesSelf(const URL&) const;
    bool allowContentSecurityPolicySourceStarToMatchAnyProtocol() const;

    // Used by ContentSecurityPolicyDirectiveList
    void reportDuplicateDirective(const String&) const;
    void reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value) const;
    void reportInvalidSandboxFlags(const String&) const;
    void reportInvalidDirectiveInReportOnlyMode(const String&) const;
    void reportInvalidDirectiveInHTTPEquivMeta(const String&) const;
    void reportMissingReportURI(const String&) const;
    void reportUnsupportedDirective(const String&) const;
    void enforceSandboxFlags(SandboxFlags sandboxFlags) { m_sandboxFlags |= sandboxFlags; }
    void addHashAlgorithmsForInlineScripts(OptionSet<ContentSecurityPolicyHashAlgorithm> hashAlgorithmsForInlineScripts)
    {
        m_hashAlgorithmsForInlineScripts |= hashAlgorithmsForInlineScripts;
    }
    void addHashAlgorithmsForInlineStylesheets(OptionSet<ContentSecurityPolicyHashAlgorithm> hashAlgorithmsForInlineStylesheets)
    {
        m_hashAlgorithmsForInlineStylesheets |= hashAlgorithmsForInlineStylesheets;
    }

    // Used by ContentSecurityPolicySource
    bool protocolMatchesSelf(const URL&) const;

    void setUpgradeInsecureRequests(bool);
    bool upgradeInsecureRequests() const { return m_upgradeInsecureRequests; }
    enum class InsecureRequestType { Load, FormSubmission, Navigation };
    void upgradeInsecureRequestIfNeeded(ResourceRequest&, InsecureRequestType) const;
    void upgradeInsecureRequestIfNeeded(URL&, InsecureRequestType) const;

    HashSet<RefPtr<SecurityOrigin>>&& takeNavigationRequestsToUpgrade();
    void inheritInsecureNavigationRequestsToUpgradeFromOpener(const ContentSecurityPolicy&);
    void setInsecureNavigationRequestsToUpgrade(HashSet<RefPtr<SecurityOrigin>>&&);

private:
    void logToConsole(const String& message, const String& contextURL = String(), const WTF::OrdinalNumber& contextLine = WTF::OrdinalNumber::beforeFirst(), JSC::ExecState* = nullptr) const;
    void updateSourceSelf(const SecurityOrigin&);
    void applyPolicyToScriptExecutionContext();

    void didReceiveHeader(const String&, ContentSecurityPolicyHeaderType, ContentSecurityPolicy::PolicyFrom);

    const TextEncoding& documentEncoding() const;

    enum class Disposition {
        Enforce,
        ReportOnly,
    };

    using ViolatedDirectiveCallback = std::function<void (const ContentSecurityPolicyDirective&)>;

    template<typename Predicate, typename... Args>
    typename std::enable_if<!std::is_convertible<Predicate, ViolatedDirectiveCallback>::value, bool>::type allPoliciesWithDispositionAllow(Disposition, Predicate&&, Args&&...) const;

    template<typename Predicate, typename... Args>
    bool allPoliciesWithDispositionAllow(Disposition, ViolatedDirectiveCallback&&, Predicate&&, Args&&...) const;

    template<typename Predicate, typename... Args>
    bool allPoliciesAllow(ViolatedDirectiveCallback&&, Predicate&&, Args&&...) const WARN_UNUSED_RETURN;

    using HashInEnforcedAndReportOnlyPoliciesPair = std::pair<bool, bool>;
    template<typename Predicate> HashInEnforcedAndReportOnlyPoliciesPair findHashOfContentInPolicies(Predicate&&, const String& content, OptionSet<ContentSecurityPolicyHashAlgorithm>) const WARN_UNUSED_RETURN;

    void reportViolation(const String& violatedDirective, const ContentSecurityPolicyDirective& effectiveViolatedDirective, const URL& blockedURL, const String& consoleMessage, JSC::ExecState*) const;
    void reportViolation(const String& violatedDirective, const ContentSecurityPolicyDirective& effectiveViolatedDirective, const URL& blockedURL, const String& consoleMessage, const String& sourceURL, const TextPosition& sourcePosition, JSC::ExecState* = nullptr) const;
    void reportBlockedScriptExecutionToInspector(const String& directiveText) const;

    // We can never have both a script execution context and a frame.
    ScriptExecutionContext* m_scriptExecutionContext { nullptr };
    const Frame* m_frame { nullptr };
    std::unique_ptr<ContentSecurityPolicySource> m_selfSource;
    String m_selfSourceProtocol;
    CSPDirectiveListVector m_policies;
    String m_lastPolicyEvalDisabledErrorMessage;
    SandboxFlags m_sandboxFlags;
    bool m_overrideInlineStyleAllowed { false };
    bool m_isReportingEnabled { true };
    bool m_upgradeInsecureRequests { false };
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsForInlineScripts;
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsForInlineStylesheets;
    HashSet<RefPtr<SecurityOrigin>> m_insecureNavigationRequestsToUpgrade;
};

}
