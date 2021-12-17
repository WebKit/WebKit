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

#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "SecurityContext.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include <functional>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
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
class JSWindowProxy;
class ResourceRequest;
class ScriptExecutionContext;
class SecurityOrigin;
class TextEncoding;
struct ContentSecurityPolicyClient;

typedef Vector<std::unique_ptr<ContentSecurityPolicyDirectiveList>> CSPDirectiveListVector;

class ContentSecurityPolicy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ContentSecurityPolicy(URL&&, ScriptExecutionContext&);
    WEBCORE_EXPORT explicit ContentSecurityPolicy(URL&&, ContentSecurityPolicyClient* = nullptr);
    WEBCORE_EXPORT ~ContentSecurityPolicy();

    void copyStateFrom(const ContentSecurityPolicy*);
    void copyUpgradeInsecureRequestStateFrom(const ContentSecurityPolicy&);
    void createPolicyForPluginDocumentFrom(const ContentSecurityPolicy&);

    void didCreateWindowProxy(JSWindowProxy&) const;

    enum class PolicyFrom {
        API,
        HTTPEquivMeta,
        HTTPHeader,
        Inherited,
        InheritedForPluginDocument,
    };
    WEBCORE_EXPORT ContentSecurityPolicyResponseHeaders responseHeaders() const;
    enum ReportParsingErrors { No, Yes };
    WEBCORE_EXPORT void didReceiveHeaders(const ContentSecurityPolicyResponseHeaders&, String&& referrer, ReportParsingErrors = ReportParsingErrors::Yes);
    void didReceiveHeaders(const ContentSecurityPolicy&, ReportParsingErrors = ReportParsingErrors::Yes);
    void didReceiveHeader(const String&, ContentSecurityPolicyHeaderType, ContentSecurityPolicy::PolicyFrom, String&& referrer, int httpStatusCode = 0);

    bool allowScriptWithNonce(const String& nonce, bool overrideContentSecurityPolicy = false) const;
    bool allowStyleWithNonce(const String& nonce, bool overrideContentSecurityPolicy = false) const;

    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy = false) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy = false) const;
    bool allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, StringView scriptContent, bool overrideContentSecurityPolicy = false) const;
    bool allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, StringView styleContent, bool overrideContentSecurityPolicy = false) const;

    bool allowEval(JSC::JSGlobalObject*, bool overrideContentSecurityPolicy = false) const;

    bool allowPluginType(const String& type, const String& typeAttribute, const URL&, bool overrideContentSecurityPolicy = false) const;

    bool allowFrameAncestors(const Frame&, const URL&, bool overrideContentSecurityPolicy = false) const;
    WEBCORE_EXPORT bool allowFrameAncestors(const Vector<RefPtr<SecurityOrigin>>& ancestorOrigins, const URL&, bool overrideContentSecurityPolicy = false) const;
    WEBCORE_EXPORT bool overridesXFrameOptions() const;

    enum class RedirectResponseReceived { No, Yes };
    WEBCORE_EXPORT bool allowScriptFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowImageFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowStyleFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowFontFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
#if ENABLE(APPLICATION_MANIFEST)
    bool allowManifestFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
#endif
    bool allowMediaFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;

    bool allowChildFrameFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
    WEBCORE_EXPORT bool allowChildContextFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
    WEBCORE_EXPORT bool allowConnectToSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No, const URL& requestedURL = URL()) const;
    bool allowFormAction(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;

    bool allowObjectFromSource(const URL&, RedirectResponseReceived = RedirectResponseReceived::No) const;
    bool allowBaseURI(const URL&, bool overrideContentSecurityPolicy = false) const;

    void setOverrideAllowInlineStyle(bool);

    void gatherReportURIs(DOMStringList&) const;

    bool allowRunningOrDisplayingInsecureContent(const URL&);

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
    bool urlMatchesSelf(const URL&, bool forFrameSrc) const;
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
        m_hashAlgorithmsForInlineScripts.add(hashAlgorithmsForInlineScripts);
    }
    void addHashAlgorithmsForInlineStylesheets(OptionSet<ContentSecurityPolicyHashAlgorithm> hashAlgorithmsForInlineStylesheets)
    {
        m_hashAlgorithmsForInlineStylesheets.add(hashAlgorithmsForInlineStylesheets);
    }

    // Used by ContentSecurityPolicySource
    bool protocolMatchesSelf(const URL&) const;

    void setUpgradeInsecureRequests(bool);
    bool upgradeInsecureRequests() const { return m_upgradeInsecureRequests; }
    enum class InsecureRequestType { Load, FormSubmission, Navigation };
    WEBCORE_EXPORT void upgradeInsecureRequestIfNeeded(ResourceRequest&, InsecureRequestType) const;
    WEBCORE_EXPORT void upgradeInsecureRequestIfNeeded(URL&, InsecureRequestType) const;

    HashSet<SecurityOriginData> takeNavigationRequestsToUpgrade();
    void inheritInsecureNavigationRequestsToUpgradeFromOpener(const ContentSecurityPolicy&);
    void setInsecureNavigationRequestsToUpgrade(HashSet<SecurityOriginData>&&);

    void setClient(ContentSecurityPolicyClient* client) { m_client = client; }
    void updateSourceSelf(const SecurityOrigin&);

    void setDocumentURL(URL& documentURL) { m_documentURL = documentURL; }

    SandboxFlags sandboxFlags() const { return m_sandboxFlags; }

private:
    void logToConsole(const String& message, const String& contextURL = String(), const WTF::OrdinalNumber& contextLine = WTF::OrdinalNumber::beforeFirst(), const WTF::OrdinalNumber& contextColumn = WTF::OrdinalNumber::beforeFirst(), JSC::JSGlobalObject* = nullptr) const;
    void applyPolicyToScriptExecutionContext();

    // Implements the deprecated CSP2 "strip uri for reporting" algorithm from <https://www.w3.org/TR/CSP2/#violation-reports>.
    String deprecatedURLForReporting(const URL&) const;

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

    using ResourcePredicate = const ContentSecurityPolicyDirective *(ContentSecurityPolicyDirectiveList::*)(const URL &, bool) const;
    bool allowResourceFromSource(const URL&, RedirectResponseReceived, const char*, ResourcePredicate) const;

    using HashInEnforcedAndReportOnlyPoliciesPair = std::pair<bool, bool>;
    template<typename Predicate> HashInEnforcedAndReportOnlyPoliciesPair findHashOfContentInPolicies(Predicate&&, StringView content, OptionSet<ContentSecurityPolicyHashAlgorithm>) const WARN_UNUSED_RETURN;

    void reportViolation(const String& effectiveViolatedDirective, const ContentSecurityPolicyDirective& violatedDirective, const URL& blockedURL, const String& consoleMessage, JSC::JSGlobalObject*) const;
    void reportViolation(const String& effectiveViolatedDirective, const String& violatedDirective, const ContentSecurityPolicyDirectiveList&, const URL& blockedURL, const String& consoleMessage, JSC::JSGlobalObject* = nullptr) const;
    void reportViolation(const String& effectiveViolatedDirective, const ContentSecurityPolicyDirective& violatedDirective, const URL& blockedURL, const String& consoleMessage, const String& sourceURL, const TextPosition& sourcePosition, const URL& preRedirectURL = URL(), JSC::JSGlobalObject* = nullptr) const;
    void reportViolation(const String& effectiveViolatedDirective, const String& violatedDirective, const ContentSecurityPolicyDirectiveList& violatedDirectiveList, const URL& blockedURL, const String& consoleMessage, const String& sourceURL, const TextPosition& sourcePosition, JSC::JSGlobalObject*, const URL& preRedirectURL = URL()) const;
    void reportBlockedScriptExecutionToInspector(const String& directiveText) const;

    // We can never have both a script execution context and a ContentSecurityPolicyClient.
    ScriptExecutionContext* m_scriptExecutionContext { nullptr };
    ContentSecurityPolicyClient* m_client { nullptr };
    URL m_protectedURL;
    std::optional<URL> m_documentURL;
    std::unique_ptr<ContentSecurityPolicySource> m_selfSource;
    String m_selfSourceProtocol;
    CSPDirectiveListVector m_policies;
    String m_lastPolicyEvalDisabledErrorMessage;
    String m_lastPolicyWebAssemblyDisabledErrorMessage;
    String m_referrer;
    SandboxFlags m_sandboxFlags { SandboxNone };
    bool m_overrideInlineStyleAllowed { false };
    bool m_isReportingEnabled { true };
    bool m_upgradeInsecureRequests { false };
    bool m_hasAPIPolicy { false };
    int m_httpStatusCode { 0 };
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsForInlineScripts;
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsForInlineStylesheets;
    HashSet<SecurityOriginData> m_insecureNavigationRequestsToUpgrade;
    mutable std::optional<ContentSecurityPolicyResponseHeaders> m_cachedResponseHeaders;
};

}
