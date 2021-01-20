/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContentSecurityPolicy.h"

#include "ContentSecurityPolicyClient.h"
#include "ContentSecurityPolicyDirective.h"
#include "ContentSecurityPolicyDirectiveList.h"
#include "ContentSecurityPolicyDirectiveNames.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicySource.h"
#include "ContentSecurityPolicySourceList.h"
#include "DOMStringList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "FormData.h"
#include "Frame.h"
#include "HTMLParserIdioms.h"
#include "InspectorInstrumentation.h"
#include "JSExecState.h"
#include "JSWindowProxy.h"
#include "LegacySchemeRegistry.h"
#include "ParsingUtilities.h"
#include "PingLoader.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityPolicyViolationEvent.h"
#include "Settings.h"
#include "TextEncoding.h"
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/JSONValues.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/TextPosition.h>


namespace WebCore {
using namespace Inspector;

static String consoleMessageForViolation(const char* effectiveViolatedDirective, const ContentSecurityPolicyDirective& violatedDirective, const URL& blockedURL, const char* prefix, const char* subject = "it")
{
    StringBuilder result;
    if (violatedDirective.directiveList().isReportOnly())
        result.appendLiteral("[Report Only] ");
    result.append(prefix);
    if (!blockedURL.isEmpty()) {
        result.append(' ');
        result.append(blockedURL.stringCenterEllipsizedToLength());
    }
    result.appendLiteral(" because ");
    result.append(subject);
    if (violatedDirective.isDefaultSrc()) {
        result.appendLiteral(" appears in neither the ");
        result.append(effectiveViolatedDirective);
        result.appendLiteral(" directive nor the default-src directive of the Content Security Policy.");
    } else {
        result.appendLiteral(" does not appear in the ");
        result.append(effectiveViolatedDirective);
        result.appendLiteral(" directive of the Content Security Policy.");
    }
    return result.toString();
}

ContentSecurityPolicy::ContentSecurityPolicy(URL&& protectedURL, ContentSecurityPolicyClient* client)
    : m_client { client }
    , m_protectedURL { WTFMove(protectedURL) }
{
    updateSourceSelf(SecurityOrigin::create(m_protectedURL).get());
}

ContentSecurityPolicy::ContentSecurityPolicy(URL&& protectedURL, ScriptExecutionContext& scriptExecutionContext)
    : m_scriptExecutionContext(&scriptExecutionContext)
    , m_protectedURL { WTFMove(protectedURL) }
{
    ASSERT(scriptExecutionContext.securityOrigin());
    updateSourceSelf(*scriptExecutionContext.securityOrigin());
}

ContentSecurityPolicy::~ContentSecurityPolicy() = default;

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other) 
{
    if (m_hasAPIPolicy)
        return;
    ASSERT(m_policies.isEmpty());
    for (auto& policy : other->m_policies)
        didReceiveHeader(policy->header(), policy->headerType(), ContentSecurityPolicy::PolicyFrom::Inherited, String { });
    m_referrer = other->m_referrer;
    m_httpStatusCode = other->m_httpStatusCode;
}

void ContentSecurityPolicy::createPolicyForPluginDocumentFrom(const ContentSecurityPolicy& other)
{
    if (m_hasAPIPolicy)
        return;
    ASSERT(m_policies.isEmpty());
    for (auto& policy : other.m_policies)
        didReceiveHeader(policy->header(), policy->headerType(), ContentSecurityPolicy::PolicyFrom::InheritedForPluginDocument, String { });
    m_referrer = other.m_referrer;
    m_httpStatusCode = other.m_httpStatusCode;
}

void ContentSecurityPolicy::copyUpgradeInsecureRequestStateFrom(const ContentSecurityPolicy& other)
{
    m_upgradeInsecureRequests = other.m_upgradeInsecureRequests;
    m_insecureNavigationRequestsToUpgrade.add(other.m_insecureNavigationRequestsToUpgrade.begin(), other.m_insecureNavigationRequestsToUpgrade.end());
}

bool ContentSecurityPolicy::allowRunningOrDisplayingInsecureContent(const URL& url)
{
    bool allow = true;
    bool isReportOnly = false;
    for (auto& policy : m_policies) {
        if (!policy->hasBlockAllMixedContentDirective())
            continue;

        isReportOnly = policy->isReportOnly();

        StringBuilder consoleMessage;
        if (isReportOnly)
            consoleMessage.appendLiteral("[Report Only] ");
        consoleMessage.append("Blocked mixed content ");
        consoleMessage.append(url.stringCenterEllipsizedToLength());
        consoleMessage.appendLiteral(" because ");
        consoleMessage.append("'block-all-mixed-content' appears in the Content Security Policy.");
        reportViolation(ContentSecurityPolicyDirectiveNames::blockAllMixedContent, ContentSecurityPolicyDirectiveNames::blockAllMixedContent, *policy, url, consoleMessage.toString());

        if (!isReportOnly)
            allow = false;
    }
    return allow;
}

void ContentSecurityPolicy::didCreateWindowProxy(JSWindowProxy& windowProxy) const
{
    auto* window = windowProxy.window();
    ASSERT(window);
    ASSERT(window->scriptExecutionContext());
    ASSERT(window->scriptExecutionContext()->contentSecurityPolicy() == this);
    if (!windowProxy.world().isNormal()) {
        window->setEvalEnabled(true);
        return;
    }
    window->setEvalEnabled(m_lastPolicyEvalDisabledErrorMessage.isNull(), m_lastPolicyEvalDisabledErrorMessage);
    window->setWebAssemblyEnabled(m_lastPolicyWebAssemblyDisabledErrorMessage.isNull(), m_lastPolicyWebAssemblyDisabledErrorMessage);
}

ContentSecurityPolicyResponseHeaders ContentSecurityPolicy::responseHeaders() const
{
    if (!m_cachedResponseHeaders) {
        ContentSecurityPolicyResponseHeaders result;
        result.m_headers.reserveInitialCapacity(m_policies.size());
        for (auto& policy : m_policies)
            result.m_headers.uncheckedAppend({ policy->header(), policy->headerType() });
        result.m_httpStatusCode = m_httpStatusCode;
        m_cachedResponseHeaders = WTFMove(result);
    }
    return *m_cachedResponseHeaders;
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers, String&& referrer, ReportParsingErrors reportParsingErrors)
{
    SetForScope<bool> isReportingEnabled(m_isReportingEnabled, reportParsingErrors == ReportParsingErrors::Yes);
    for (auto& header : headers.m_headers)
        didReceiveHeader(header.first, header.second, ContentSecurityPolicy::PolicyFrom::HTTPHeader, String { });
    m_referrer = WTFMove(referrer);
    m_httpStatusCode = headers.m_httpStatusCode;
}

void ContentSecurityPolicy::didReceiveHeader(const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicy::PolicyFrom policyFrom, String&& referrer, int httpStatusCode)
{
    if (m_hasAPIPolicy)
        return;

    m_referrer = WTFMove(referrer);
    m_httpStatusCode = httpStatusCode;

    if (policyFrom == PolicyFrom::API) {
        ASSERT(m_policies.isEmpty());
        m_hasAPIPolicy = true;
    }

    m_cachedResponseHeaders = WTF::nullopt;

    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    readCharactersForParsing(header, [&](auto buffer) {
        auto begin = buffer.position();
    
        while (buffer.hasCharactersRemaining()) {
            skipUntil(buffer, ',');

            // header1,header2 OR header1
            //        ^                  ^
            m_policies.append(ContentSecurityPolicyDirectiveList::create(*this, String(begin, buffer.position() - begin), type, policyFrom));

            // Skip the comma, and begin the next header from the current position.
            ASSERT(buffer.atEnd() || *buffer == ',');
            skipExactly(buffer, ',');
            begin = buffer.position();
        }
    });
    

    if (m_scriptExecutionContext)
        applyPolicyToScriptExecutionContext();
}

void ContentSecurityPolicy::updateSourceSelf(const SecurityOrigin& securityOrigin)
{
    m_selfSourceProtocol = securityOrigin.protocol();
    m_selfSource = makeUnique<ContentSecurityPolicySource>(*this, m_selfSourceProtocol, securityOrigin.host(), securityOrigin.port(), emptyString(), false, false);
}

void ContentSecurityPolicy::applyPolicyToScriptExecutionContext()
{
    ASSERT(m_scriptExecutionContext);

    // Update source self as the security origin may have changed between the time we were created and now.
    // For instance, we may have been initially created for an about:blank iframe that later inherited the
    // security origin of its owner document.
    ASSERT(m_scriptExecutionContext->securityOrigin());
    updateSourceSelf(*m_scriptExecutionContext->securityOrigin());

    bool enableStrictMixedContentMode = false;
    for (auto& policy : m_policies) {
        const ContentSecurityPolicyDirective* violatedDirective = policy->violatedDirectiveForUnsafeEval();
        if (violatedDirective && !violatedDirective->directiveList().isReportOnly()) {
            m_lastPolicyEvalDisabledErrorMessage = policy->evalDisabledErrorMessage();
            m_lastPolicyWebAssemblyDisabledErrorMessage = policy->webAssemblyDisabledErrorMessage();
        }
        if (policy->hasBlockAllMixedContentDirective() && !policy->isReportOnly())
            enableStrictMixedContentMode = true;
    }

    if (!m_lastPolicyEvalDisabledErrorMessage.isNull())
        m_scriptExecutionContext->disableEval(m_lastPolicyEvalDisabledErrorMessage);
    if (!m_lastPolicyWebAssemblyDisabledErrorMessage.isNull())
        m_scriptExecutionContext->disableWebAssembly(m_lastPolicyWebAssemblyDisabledErrorMessage);
    if (m_sandboxFlags != SandboxNone && is<Document>(m_scriptExecutionContext))
        m_scriptExecutionContext->enforceSandboxFlags(m_sandboxFlags);
    if (enableStrictMixedContentMode)
        m_scriptExecutionContext->setStrictMixedContentMode(true);
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value)
{
    m_overrideInlineStyleAllowed = value;
}

bool ContentSecurityPolicy::urlMatchesSelf(const URL& url) const
{
    return m_selfSource->matches(url);
}

bool ContentSecurityPolicy::allowContentSecurityPolicySourceStarToMatchAnyProtocol() const
{
    if (is<Document>(m_scriptExecutionContext))
        return downcast<Document>(*m_scriptExecutionContext).settings().allowContentSecurityPolicySourceStarToMatchAnyProtocol();
    return false;
}

bool ContentSecurityPolicy::protocolMatchesSelf(const URL& url) const
{
    if (equalLettersIgnoringASCIICase(m_selfSourceProtocol, "http"))
        return url.protocolIsInHTTPFamily();
    return equalIgnoringASCIICase(url.protocol(), m_selfSourceProtocol);
}

template<typename Predicate, typename... Args>
typename std::enable_if<!std::is_convertible<Predicate, ContentSecurityPolicy::ViolatedDirectiveCallback>::value, bool>::type ContentSecurityPolicy::allPoliciesWithDispositionAllow(Disposition disposition, Predicate&& predicate, Args&&... args) const
{
    bool isReportOnly = disposition == ContentSecurityPolicy::Disposition::ReportOnly;
    for (auto& policy : m_policies) {
        if (policy->isReportOnly() != isReportOnly)
            continue;
        if ((policy.get()->*predicate)(std::forward<Args>(args)...))
            return false;
    }
    return true;
}

template<typename Predicate, typename... Args>
bool ContentSecurityPolicy::allPoliciesWithDispositionAllow(Disposition disposition, ViolatedDirectiveCallback&& callback, Predicate&& predicate, Args&&... args) const
{
    bool isReportOnly = disposition == ContentSecurityPolicy::Disposition::ReportOnly;
    bool isAllowed = true;
    for (auto& policy : m_policies) {
        if (policy->isReportOnly() != isReportOnly)
            continue;
        if (const ContentSecurityPolicyDirective* violatedDirective = (policy.get()->*predicate)(std::forward<Args>(args)...)) {
            isAllowed = false;
            callback(*violatedDirective);
        }
    }
    return isAllowed;
}

template<typename Predicate, typename... Args>
bool ContentSecurityPolicy::allPoliciesAllow(ViolatedDirectiveCallback&& callback, Predicate&& predicate, Args&&... args) const
{
    bool isAllowed = true;
    for (auto& policy : m_policies) {
        if (const ContentSecurityPolicyDirective* violatedDirective = (policy.get()->*predicate)(std::forward<Args>(args)...)) {
            if (!violatedDirective->directiveList().isReportOnly())
                isAllowed = false;
            callback(*violatedDirective);
        }
    }
    return isAllowed;
}

template<typename Predicate>
ContentSecurityPolicy::HashInEnforcedAndReportOnlyPoliciesPair ContentSecurityPolicy::findHashOfContentInPolicies(Predicate&& predicate, const String& content, OptionSet<ContentSecurityPolicyHashAlgorithm> algorithms) const
{
    if (algorithms.isEmpty() || content.isEmpty())
        return { false, false };

    // FIXME: We should compute the document encoding once and cache it instead of computing it on each invocation.
    TextEncoding documentEncoding;
    if (is<Document>(m_scriptExecutionContext))
        documentEncoding = downcast<Document>(*m_scriptExecutionContext).textEncoding();
    const TextEncoding& encodingToUse = documentEncoding.isValid() ? documentEncoding : UTF8Encoding();

    // FIXME: Compute the digest with respect to the raw bytes received from the page.
    // See <https://bugs.webkit.org/show_bug.cgi?id=155184>.
    auto encodedContent = encodingToUse.encode(content, UnencodableHandling::Entities);
    bool foundHashInEnforcedPolicies = false;
    bool foundHashInReportOnlyPolicies = false;
    for (auto algorithm : algorithms) {
        ContentSecurityPolicyHash hash = cryptographicDigestForBytes(algorithm, encodedContent.data(), encodedContent.size());
        if (!foundHashInEnforcedPolicies && allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::Enforce, std::forward<Predicate>(predicate), hash))
            foundHashInEnforcedPolicies = true;
        if (!foundHashInReportOnlyPolicies && allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::ReportOnly, std::forward<Predicate>(predicate), hash))
            foundHashInReportOnlyPolicies = true;
        if (foundHashInEnforcedPolicies && foundHashInReportOnlyPolicies)
            break;
    }
    return { foundHashInEnforcedPolicies, foundHashInReportOnlyPolicies };
}

bool ContentSecurityPolicy::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
        if (!didNotifyInspector && violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
}

bool ContentSecurityPolicy::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), "Refused to execute a script for an inline event handler", "'unsafe-inline'");
        reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
        if (!didNotifyInspector && !violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
}

bool ContentSecurityPolicy::allowScriptWithNonce(const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    if (strippedNonce.isEmpty())
        return false;
    // FIXME: We need to report violations in report-only policies. See <https://bugs.webkit.org/show_bug.cgi?id=159830>.
    return allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::Enforce, &ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptNonce, strippedNonce);
}

bool ContentSecurityPolicy::allowStyleWithNonce(const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    if (strippedNonce.isEmpty())
        return false;
    // FIXME: We need to report violations in report-only policies. See <https://bugs.webkit.org/show_bug.cgi?id=159830>.
    return allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::Enforce, &ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleNonce, strippedNonce);
}

bool ContentSecurityPolicy::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& scriptContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool didNotifyInspector = false;
    auto [foundHashInEnforcedPolicies, foundHashInReportOnlyPolicies] = findHashOfContentInPolicies(&ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptHash, scriptContent, m_hashAlgorithmsForInlineScripts);
    if (foundHashInEnforcedPolicies && foundHashInReportOnlyPolicies)
        return true;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
        if (!didNotifyInspector && !violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };
    // FIXME: We should not report that the inline script violated a policy when its hash matched a source
    // expression in the policy and the page has more than one policy. See <https://bugs.webkit.org/show_bug.cgi?id=159832>.
    if (!foundHashInReportOnlyPolicies)
        allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::ReportOnly, handleViolatedDirective, &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
    return foundHashInEnforcedPolicies || allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::Enforce, handleViolatedDirective, &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
}

bool ContentSecurityPolicy::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& styleContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (m_overrideInlineStyleAllowed)
        return true;
    auto [foundHashInEnforcedPolicies, foundHashInReportOnlyPolicies] = findHashOfContentInPolicies(&ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleHash, styleContent, m_hashAlgorithmsForInlineStylesheets);
    if (foundHashInEnforcedPolicies && foundHashInReportOnlyPolicies)
        return true;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::styleSrc, violatedDirective, URL(), "Refused to apply a stylesheet", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(ContentSecurityPolicyDirectiveNames::styleSrc, violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
    };
    // FIXME: We should not report that the inline stylesheet violated a policy when its hash matched a source
    // expression in the policy and the page has more than one policy. See <https://bugs.webkit.org/show_bug.cgi?id=159832>.
    if (!foundHashInReportOnlyPolicies)
        allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::ReportOnly, handleViolatedDirective, &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyle);
    return foundHashInEnforcedPolicies || allPoliciesWithDispositionAllow(ContentSecurityPolicy::Disposition::Enforce, handleViolatedDirective, &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyle);
}

bool ContentSecurityPolicy::allowEval(JSC::JSGlobalObject* state, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), "Refused to execute a script", "'unsafe-eval'");
        reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, violatedDirective, URL(), consoleMessage, state);
        if (!didNotifyInspector && !violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeEval);
}

bool ContentSecurityPolicy::allowFrameAncestors(const Frame& frame, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    Frame& topFrame = frame.tree().top();
    if (&frame == &topFrame)
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::frameAncestors, violatedDirective, url, "Refused to load");
        reportViolation(ContentSecurityPolicyDirectiveNames::frameAncestors, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestor, frame);
}

bool ContentSecurityPolicy::overridesXFrameOptions() const
{
    // If a resource is delivered with an policy that includes a directive named frame-ancestors and whose disposition
    // is "enforce", then the X-Frame-Options header MUST be ignored.
    // https://www.w3.org/TR/CSP3/#frame-ancestors-and-frame-options
    for (auto& policy : m_policies) {
        if (!policy->isReportOnly() && policy->hasFrameAncestorsDirective())
            return true;
    }
    return false;
}

bool ContentSecurityPolicy::allowFrameAncestors(const Vector<RefPtr<SecurityOrigin>>& ancestorOrigins, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool isTopLevelFrame = ancestorOrigins.isEmpty();
    if (isTopLevelFrame)
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::frameAncestors, violatedDirective, url, "Refused to load");
        reportViolation(ContentSecurityPolicyDirectiveNames::frameAncestors, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestorOrigins, ancestorOrigins);
}

bool ContentSecurityPolicy::allowPluginType(const String& type, const String& typeAttribute, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::pluginTypes, violatedDirective, url, "Refused to load", "its MIME type");
        reportViolation(ContentSecurityPolicyDirectiveNames::pluginTypes, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForPluginType, type, typeAttribute);
}

bool ContentSecurityPolicy::allowObjectFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol().toStringWithoutCopying()))
        return true;
    // As per section object-src of the Content Security Policy Level 3 spec., <http://w3c.github.io/webappsec-csp> (Editor's Draft, 29 February 2016),
    // "If plugin content is loaded without an associated URL (perhaps an object element lacks a data attribute, but loads some default plugin based
    // on the specified type), it MUST be blocked if object-src's value is 'none', but will otherwise be allowed".
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::objectSrc, violatedDirective, url, "Refused to load");
        reportViolation(ContentSecurityPolicyDirectiveNames::objectSrc, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForObjectSource, url, redirectResponseReceived == RedirectResponseReceived::Yes, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::Yes);
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol().toStringWithoutCopying()))
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        const char* effectiveViolatedDirective = violatedDirective.name() == ContentSecurityPolicyDirectiveNames::frameSrc ? ContentSecurityPolicyDirectiveNames::frameSrc : ContentSecurityPolicyDirectiveNames::childSrc;
        String consoleMessage = consoleMessageForViolation(effectiveViolatedDirective, violatedDirective, url, "Refused to load");
        reportViolation(effectiveViolatedDirective, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForFrame, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowResourceFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const char* name, ResourcePredicate resourcePredicate) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol().toStringWithoutCopying()))
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(name, violatedDirective, url, "Refused to load");
        reportViolation(name, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), resourcePredicate, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowChildContextFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::childSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForChildContext);
}

bool ContentSecurityPolicy::allowScriptFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::scriptSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForScript);
}

bool ContentSecurityPolicy::allowImageFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::imgSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForImage);
}

bool ContentSecurityPolicy::allowStyleFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::styleSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForStyle);
}

bool ContentSecurityPolicy::allowFontFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::fontSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForFont);
}

#if ENABLE(APPLICATION_MANIFEST)
bool ContentSecurityPolicy::allowManifestFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::manifestSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForManifest);
}
#endif // ENABLE(APPLICATION_MANIFEST)

bool ContentSecurityPolicy::allowMediaFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::mediaSrc, &ContentSecurityPolicyDirectiveList::violatedDirectiveForMedia);
}

bool ContentSecurityPolicy::allowConnectToSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol().toStringWithoutCopying()))
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::connectSrc, violatedDirective, url, "Refused to connect to");
        reportViolation(ContentSecurityPolicyDirectiveNames::connectSrc, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForConnectSource, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowFormAction(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    return allowResourceFromSource(url, redirectResponseReceived, ContentSecurityPolicyDirectiveNames::formAction, &ContentSecurityPolicyDirectiveList::violatedDirectiveForFormAction);
}

bool ContentSecurityPolicy::allowBaseURI(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol().toStringWithoutCopying()))
        return true;
    String sourceURL;
    TextPosition sourcePosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::baseURI, violatedDirective, url, "Refused to change the document base URL to");
        reportViolation(ContentSecurityPolicyDirectiveNames::baseURI, violatedDirective, url, consoleMessage, sourceURL, sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForBaseURI, url);
}

String ContentSecurityPolicy::deprecatedURLForReporting(const URL& url) const
{
    if (!url.isValid())
        return { };
    if (!url.isHierarchical() || url.protocolIs("file"))
        return url.protocol().toString();
    return static_cast<SecurityOriginData>(*m_selfSource).securityOrigin()->canRequest(url) ? url.strippedForUseAsReferrer() : SecurityOrigin::create(url)->toString();
}

void ContentSecurityPolicy::reportViolation(const String& violatedDirective, const ContentSecurityPolicyDirective& effectiveViolatedDirective, const URL& blockedURL, const String& consoleMessage, JSC::JSGlobalObject* state) const
{
    // FIXME: Extract source file and source position from JSC::ExecState.
    return reportViolation(violatedDirective, effectiveViolatedDirective.text(), effectiveViolatedDirective.directiveList(), blockedURL, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber::beforeFirst()), state);
}

void ContentSecurityPolicy::reportViolation(const String& effectiveViolatedDirective, const String& violatedDirective, const ContentSecurityPolicyDirectiveList& violatedDirectiveList, const URL& blockedURL, const String& consoleMessage, JSC::JSGlobalObject* state) const
{
    // FIXME: Extract source file and source position from JSC::ExecState.
    return reportViolation(effectiveViolatedDirective, violatedDirective, violatedDirectiveList, blockedURL, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber::beforeFirst()), state);
}

void ContentSecurityPolicy::reportViolation(const String& effectiveViolatedDirective, const ContentSecurityPolicyDirective& violatedDirective, const URL& blockedURL, const String& consoleMessage, const String& sourceURL, const TextPosition& sourcePosition, JSC::JSGlobalObject* state) const
{
    return reportViolation(effectiveViolatedDirective, violatedDirective.text(), violatedDirective.directiveList(), blockedURL, consoleMessage, sourceURL, sourcePosition, state);
}

void ContentSecurityPolicy::reportViolation(const String& effectiveViolatedDirective, const String& violatedDirective, const ContentSecurityPolicyDirectiveList& violatedDirectiveList, const URL& blockedURL, const String& consoleMessage, const String& sourceURL, const TextPosition& sourcePosition, JSC::JSGlobalObject* state) const
{
    logToConsole(consoleMessage, sourceURL, sourcePosition.m_line, sourcePosition.m_column, state);

    if (!m_isReportingEnabled)
        return;

    // FIXME: Support sending reports from worker.
    CSPInfo info;
    info.documentURI = blockedURL.string();
    if (m_client)
        m_client->willSendCSPViolationReport(info);
    else {
        if (!is<Document>(m_scriptExecutionContext))
            return;

        auto& document = downcast<Document>(*m_scriptExecutionContext);
        auto* frame = document.frame();
        if (!frame)
            return;

        info.documentURI = document.url().strippedForUseAsReferrer();

        auto stack = createScriptCallStack(JSExecState::currentState(), 2);
        auto* callFrame = stack->firstNonNativeCallFrame();
        if (callFrame && callFrame->lineNumber()) {
            info.sourceFile = deprecatedURLForReporting(URL { URL { }, callFrame->sourceURL() });
            info.lineNumber = callFrame->lineNumber();
            info.columnNumber = callFrame->columnNumber();
        }
    }
    ASSERT(m_client || is<Document>(m_scriptExecutionContext));

    String blockedURI = deprecatedURLForReporting(blockedURL);
    // FIXME: Is it policy to not use the status code for HTTPS, or is that a bug?
    unsigned short httpStatusCode = m_selfSourceProtocol == "http" ? m_httpStatusCode : 0;

    // 1. Dispatch violation event.
    SecurityPolicyViolationEvent::Init violationEventInit;
    violationEventInit.documentURI = info.documentURI;
    violationEventInit.referrer = m_referrer;
    violationEventInit.blockedURI = blockedURI;
    violationEventInit.violatedDirective = violatedDirective;
    violationEventInit.effectiveDirective = effectiveViolatedDirective;
    violationEventInit.originalPolicy = violatedDirectiveList.header();
    violationEventInit.sourceFile = info.sourceFile;
    violationEventInit.statusCode = httpStatusCode;
    violationEventInit.lineNumber =  info.lineNumber;
    violationEventInit.columnNumber = info.columnNumber;
    if (m_client)
        m_client->enqueueSecurityPolicyViolationEvent(WTFMove(violationEventInit));
    else
        downcast<Document>(*m_scriptExecutionContext).enqueueSecurityPolicyViolationEvent(WTFMove(violationEventInit));

    // 2. Send violation report (if applicable).
    auto& reportURIs = violatedDirectiveList.reportURIs();
    if (reportURIs.isEmpty())
        return;

    // We need to be careful here when deciding what information to send to the
    // report-uri. Currently, we send only the current document's URL and the
    // directive that was violated. The document's URL is safe to send because
    // it's the document itself that's requesting that it be sent. You could
    // make an argument that we shouldn't send HTTPS document URLs to HTTP
    // report-uris (for the same reasons that we suppress the Referer in that
    // case), but the Referer is sent implicitly whereas this request is only
    // sent explicitly. As for which directive was violated, that's pretty
    // harmless information.

    auto cspReport = JSON::Object::create();
    cspReport->setString("document-uri"_s, info.documentURI);
    cspReport->setString("referrer"_s, m_referrer);
    cspReport->setString("violated-directive"_s, violatedDirective);
    cspReport->setString("effective-directive"_s, effectiveViolatedDirective);
    cspReport->setString("original-policy"_s, violatedDirectiveList.header());
    cspReport->setString("blocked-uri"_s, blockedURI);
    cspReport->setInteger("status-code"_s, httpStatusCode);
    if (!info.sourceFile.isNull()) {
        cspReport->setString("source-file"_s, info.sourceFile);
        cspReport->setInteger("line-number"_s, info.lineNumber);
        cspReport->setInteger("column-number"_s, info.columnNumber);
    }

    auto reportObject = JSON::Object::create();
    reportObject->setObject("csp-report"_s, WTFMove(cspReport));

    auto report = FormData::create(reportObject->toJSONString().utf8());

    if (m_client) {
        for (const auto& url : reportURIs)
            m_client->sendCSPViolationReport(URL { m_protectedURL, url }, report.copyRef());
    } else {
        auto& document = downcast<Document>(*m_scriptExecutionContext);
        for (const auto& url : reportURIs)
            PingLoader::sendViolationReport(*document.frame(), URL { m_protectedURL, url }, report.copyRef(), ViolationReportType::ContentSecurityPolicy);
    }
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name) const
{
    String message;
    if (equalLettersIgnoringASCIICase(name, "allow"))
        message = "The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect."_s;
    else if (equalLettersIgnoringASCIICase(name, "options"))
        message = "The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect."_s;
    else if (equalLettersIgnoringASCIICase(name, "policy-uri"))
        message = "The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header."_s;
    else
        message = makeString("Unrecognized Content-Security-Policy directive '", name, "'.\n"); // FIXME: Why does this include a newline?

    logToConsole(message);
}

void ContentSecurityPolicy::reportDirectiveAsSourceExpression(const String& directiveName, const String& sourceExpression) const
{
    logToConsole("The Content Security Policy directive '" + directiveName + "' contains '" + sourceExpression + "' as a source expression. Did you mean '" + directiveName + " ...; " + sourceExpression + "...' (note the semicolon)?");
}

void ContentSecurityPolicy::reportDuplicateDirective(const String& name) const
{
    logToConsole(makeString("Ignoring duplicate Content-Security-Policy directive '", name, "'.\n"));
}

void ContentSecurityPolicy::reportInvalidPluginTypes(const String& pluginType) const
{
    String message;
    if (pluginType.isNull())
        message = "'plugin-types' Content Security Policy directive is empty; all plugins will be blocked.\n";
    else
        message = makeString("Invalid plugin type in 'plugin-types' Content Security Policy directive: '", pluginType, "'.\n");
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSandboxFlags(const String& invalidFlags) const
{
    logToConsole("Error while parsing the 'sandbox' Content Security Policy directive: " + invalidFlags);
}

void ContentSecurityPolicy::reportInvalidDirectiveInReportOnlyMode(const String& directiveName) const
{
    logToConsole("The Content Security Policy directive '" + directiveName + "' is ignored when delivered in a report-only policy.");
}

void ContentSecurityPolicy::reportInvalidDirectiveInHTTPEquivMeta(const String& directiveName) const
{
    logToConsole("The Content Security Policy directive '" + directiveName + "' is ignored when delivered via an HTML meta element.");
}

void ContentSecurityPolicy::reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value) const
{
    String message = makeString("The value for Content Security Policy directive '", directiveName, "' contains an invalid character: '", value, "'. Non-whitespace characters outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC 3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1.");
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPathCharacter(const String& directiveName, const String& value, const char invalidChar) const
{
    ASSERT(invalidChar == '#' || invalidChar == '?');

    String ignoring;
    if (invalidChar == '?')
        ignoring = "The query component, including the '?', will be ignored.";
    else
        ignoring = "The fragment identifier, including the '#', will be ignored.";

    String message = makeString("The source list for Content Security Policy directive '", directiveName, "' contains a source with an invalid path: '", value, "'. ", ignoring);
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSourceExpression(const String& directiveName, const String& source) const
{
    String message = makeString("The source list for Content Security Policy directive '", directiveName, "' contains an invalid source: '", source, "'. It will be ignored.");
    if (equalLettersIgnoringASCIICase(source, "'none'"))
        message = makeString(message, " Note that 'none' has no effect unless it is the only expression in the source list.");
    logToConsole(message);
}

void ContentSecurityPolicy::reportMissingReportURI(const String& policy) const
{
    logToConsole("The Content Security Policy '" + policy + "' was delivered in report-only mode, but does not specify a 'report-uri'; the policy will have no effect. Please either add a 'report-uri' directive, or deliver the policy via the 'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::logToConsole(const String& message, const String& contextURL, const WTF::OrdinalNumber& contextLine, const WTF::OrdinalNumber& contextColumn, JSC::JSGlobalObject* state) const
{
    if (!m_isReportingEnabled)
        return;

    if (m_client)
        m_client->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, 0);
    else if (m_scriptExecutionContext)
        m_scriptExecutionContext->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, contextURL, contextLine.oneBasedInt(), contextColumn.oneBasedInt(), state);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(const String& directiveText) const
{
    if (m_scriptExecutionContext)
        InspectorInstrumentation::scriptExecutionBlockedByCSP(m_scriptExecutionContext, directiveText);
}

void ContentSecurityPolicy::upgradeInsecureRequestIfNeeded(ResourceRequest& request, InsecureRequestType requestType) const
{
    URL url = request.url();
    upgradeInsecureRequestIfNeeded(url, requestType);
    request.setURL(url);
}

void ContentSecurityPolicy::upgradeInsecureRequestIfNeeded(URL& url, InsecureRequestType requestType) const
{
    if (!url.protocolIs("http") && !url.protocolIs("ws"))
        return;

    bool upgradeRequest = m_insecureNavigationRequestsToUpgrade.contains(SecurityOriginData::fromURL(url));
    if (requestType == InsecureRequestType::Load || requestType == InsecureRequestType::FormSubmission)
        upgradeRequest |= m_upgradeInsecureRequests;

    if (!upgradeRequest)
        return;

    if (url.protocolIs("http"))
        url.setProtocol("https");
    else {
        ASSERT(url.protocolIs("ws"));
        url.setProtocol("wss");
    }

    if (url.port() && url.port().value() == 80)
        url.setPort(443);
}

void ContentSecurityPolicy::setUpgradeInsecureRequests(bool upgradeInsecureRequests)
{
    m_upgradeInsecureRequests = upgradeInsecureRequests;
    if (!m_upgradeInsecureRequests)
        return;

    if (!m_scriptExecutionContext)
        return;

    // Store the upgrade domain as an 'insecure' protocol so we can quickly identify
    // origins we should upgrade.
    URL upgradeURL = m_scriptExecutionContext->url();
    if (upgradeURL.protocolIs("https"))
        upgradeURL.setProtocol("http");
    else if (upgradeURL.protocolIs("wss"))
        upgradeURL.setProtocol("ws");
    
    m_insecureNavigationRequestsToUpgrade.add(SecurityOriginData::fromURL(upgradeURL));
}

void ContentSecurityPolicy::inheritInsecureNavigationRequestsToUpgradeFromOpener(const ContentSecurityPolicy& other)
{
    m_insecureNavigationRequestsToUpgrade.add(other.m_insecureNavigationRequestsToUpgrade.begin(), other.m_insecureNavigationRequestsToUpgrade.end());
}

HashSet<SecurityOriginData> ContentSecurityPolicy::takeNavigationRequestsToUpgrade()
{
    return WTFMove(m_insecureNavigationRequestsToUpgrade);
}

void ContentSecurityPolicy::setInsecureNavigationRequestsToUpgrade(HashSet<SecurityOriginData>&& insecureNavigationRequests)
{
    m_insecureNavigationRequestsToUpgrade = WTFMove(insecureNavigationRequests);
}

}
