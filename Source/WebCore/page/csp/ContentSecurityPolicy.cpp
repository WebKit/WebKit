/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

#include "BlobURL.h"
#include "CSPViolationReportBody.h"
#include "ContentSecurityPolicyClient.h"
#include "ContentSecurityPolicyDirective.h"
#include "ContentSecurityPolicyDirectiveList.h"
#include "ContentSecurityPolicyDirectiveNames.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicySource.h"
#include "ContentSecurityPolicySourceList.h"
#include "DOMStringList.h"
#include "DocumentInlines.h"
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
#include "Report.h"
#include "ReportingClient.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityPolicyViolationEvent.h"
#include "Settings.h"
#include "SubresourceIntegrity.h"
#include "ViolationReportType.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <pal/crypto/CryptoDigest.h>
#include <pal/text/TextEncoding.h>
#include <wtf/JSONValues.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {
using namespace Inspector;

static String consoleMessageForViolation(const ContentSecurityPolicyDirective& violatedDirective, const URL& blockedURL, const char* prefix, const char* subject = "it")
{
    bool isDefaultSrc = violatedDirective.isDefaultSrc();
    String name = violatedDirective.nameForReporting();
    if (violatedDirective.nameForReporting().startsWith(StringView { ContentSecurityPolicyDirectiveNames::scriptSrc }))
        name = ContentSecurityPolicyDirectiveNames::scriptSrc;
    else if (violatedDirective.nameForReporting().startsWith(StringView { ContentSecurityPolicyDirectiveNames::styleSrc }))
        name = ContentSecurityPolicyDirectiveNames::styleSrc;

    return makeString(violatedDirective.directiveList().isReportOnly() ? "[Report Only] " : "",
        prefix, blockedURL.isEmpty() ? "" : " ", blockedURL.stringCenterEllipsizedToLength(), " because ", subject,
        isDefaultSrc ? " appears in neither the " : " does not appear in the ",
        name,
        isDefaultSrc ? " directive nor the default-src directive of the Content Security Policy." : " directive of the Content Security Policy.");
}

ContentSecurityPolicy::ContentSecurityPolicy(URL&& protectedURL, ContentSecurityPolicyClient* client, ReportingClient* reportingClient)
    : m_client { client }
    , m_reportingClient { reportingClient }
    , m_protectedURL { WTFMove(protectedURL) }
{
    updateSourceSelf(SecurityOrigin::create(m_protectedURL).get());
}

static ReportingClient* reportingClientForContext(ScriptExecutionContext& scriptExecutionContext)
{
    if (auto* document = dynamicDowncast<Document>(scriptExecutionContext))
        return document;

    if (auto* workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext))
        return workerGlobalScope;

    return nullptr;
}

ContentSecurityPolicy::ContentSecurityPolicy(URL&& protectedURL, ScriptExecutionContext& scriptExecutionContext)
    : m_scriptExecutionContext(&scriptExecutionContext)
    , m_reportingClient { reportingClientForContext(scriptExecutionContext) }
    , m_protectedURL { WTFMove(protectedURL) }
{
    ASSERT(scriptExecutionContext.securityOrigin());
    updateSourceSelf(*scriptExecutionContext.securityOrigin());
    // FIXME: handle the non-document case.
    if (is<Document>(m_scriptExecutionContext)) {
        if (auto* page = downcast<Document>(*m_scriptExecutionContext).page())
            m_contentSecurityPolicyModeForExtension = page->contentSecurityPolicyModeForExtension();
    }
}

ContentSecurityPolicy::~ContentSecurityPolicy() = default;

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other, ShouldMakeIsolatedCopy shouldMakeIsolatedCopy)
{
    if (m_hasAPIPolicy)
        return;
    ASSERT(m_policies.isEmpty());
    for (auto& policy : other->m_policies)
        didReceiveHeader(policy->header(), policy->headerType(), ContentSecurityPolicy::PolicyFrom::Inherited, String { });
    m_referrer = shouldMakeIsolatedCopy == ShouldMakeIsolatedCopy::Yes ? other->m_referrer.isolatedCopy() : other->m_referrer;
    m_httpStatusCode = other->m_httpStatusCode;
}

void ContentSecurityPolicy::inheritHeadersFrom(const ContentSecurityPolicyResponseHeaders& headers)
{
    if (m_hasAPIPolicy)
        return;
    ASSERT(m_policies.isEmpty());
    for (auto& [header, headerType] : headers.m_headers)
        didReceiveHeader(header, headerType, PolicyFrom::Inherited, String { });
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

void ContentSecurityPolicy::copyUpgradeInsecureRequestStateFrom(const ContentSecurityPolicy& other, ShouldMakeIsolatedCopy shouldMakeIsolatedCopy)
{
    m_upgradeInsecureRequests = other.m_upgradeInsecureRequests;
    m_insecureNavigationRequestsToUpgrade = shouldMakeIsolatedCopy == ShouldMakeIsolatedCopy::Yes ? crossThreadCopy(other.m_insecureNavigationRequestsToUpgrade) : other.m_insecureNavigationRequestsToUpgrade;
}

bool ContentSecurityPolicy::allowRunningOrDisplayingInsecureContent(const URL& url)
{
    bool allow = true;
    for (auto& policy : m_policies) {
        if (!policy->hasBlockAllMixedContentDirective())
            continue;
        bool isReportOnly = policy->isReportOnly();
        auto message = makeString(isReportOnly ? "[Report Only] " : "", "Blocked mixed content ",
            url.stringCenterEllipsizedToLength(), " because 'block-all-mixed-content' appears in the Content Security Policy.");
        reportViolation(ContentSecurityPolicyDirectiveNames::blockAllMixedContent, *policy, url.string(), message);
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
        auto makeValidHTTPHeaderIfNeeded = [this](auto& header) {
            // If coming from an existing HTTP header it should be valid already.
            if (m_isHeaderDelivered)
                return header;

            // Newlines are valid in http-equiv but not in HTTP header value.
            auto returnValue = makeStringByReplacingAll(header, '\n', ""_s);
            returnValue = makeStringByReplacingAll(returnValue, '\r', ""_s);
            return returnValue;
        };

        ContentSecurityPolicyResponseHeaders result;
        result.m_headers = m_policies.map([&makeValidHTTPHeaderIfNeeded](auto& policy) {
            return std::pair { makeValidHTTPHeaderIfNeeded(policy->header()), policy->headerType() };
        });
        result.m_httpStatusCode = m_httpStatusCode;
        m_cachedResponseHeaders = WTFMove(result);
    }
    return *m_cachedResponseHeaders;
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers, String&& referrer, ReportParsingErrors reportParsingErrors)
{
    SetForScope isReportingEnabled(m_isReportingEnabled, reportParsingErrors == ReportParsingErrors::Yes);
    for (auto& header : headers.m_headers)
        didReceiveHeader(header.first, header.second, ContentSecurityPolicy::PolicyFrom::HTTPHeader, String { });
    m_referrer = WTFMove(referrer);
    m_httpStatusCode = headers.m_httpStatusCode;
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicy& other, ReportParsingErrors reportParsingErrors)
{
    SetForScope isReportingEnabled(m_isReportingEnabled, reportParsingErrors == ReportParsingErrors::Yes);
    for (auto& policy : other.m_policies)
        didReceiveHeader(policy->header(), policy->headerType(), ContentSecurityPolicy::PolicyFrom::HTTPHeader, String { });
    m_referrer = other.m_referrer;
    m_httpStatusCode = other.m_httpStatusCode;
    m_upgradeInsecureRequests = other.m_upgradeInsecureRequests;
    m_insecureNavigationRequestsToUpgrade.add(other.m_insecureNavigationRequestsToUpgrade.begin(), other.m_insecureNavigationRequestsToUpgrade.end());
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
    } else if (policyFrom == PolicyFrom::HTTPHeader)
        m_isHeaderDelivered = true;

    m_cachedResponseHeaders = std::nullopt;

    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    readCharactersForParsing(header, [&](auto buffer) {
        skipWhile<isASCIISpace>(buffer);
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
    m_selfSourceProtocol = securityOrigin.protocol().convertToASCIILowercase();
    m_selfSource = makeUnique<ContentSecurityPolicySource>(*this, m_selfSourceProtocol, securityOrigin.host(), securityOrigin.port(), emptyString(), false, false, IsSelfSource::Yes);
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
        m_scriptExecutionContext->enforceSandboxFlags(m_sandboxFlags, SecurityContext::SandboxFlagsSource::CSP);
    if (enableStrictMixedContentMode)
        m_scriptExecutionContext->setStrictMixedContentMode(true);
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value)
{
    m_overrideInlineStyleAllowed = value;
}

bool ContentSecurityPolicy::urlMatchesSelf(const URL& url, bool forFrameSrc) const
{
    // As per https://w3c.github.io/webappsec-csp/#match-url-to-source-expression, we compare the URL origin with the policy origin.
    // We get origin using https://url.spec.whatwg.org/#concept-url-origin which has specific blob URLs treatment as follow.
    if (forFrameSrc && url.protocolIsBlob())
        return m_selfSource->matches(BlobURL::getOriginURL(url));
    return m_selfSource->matches(url);
}

bool ContentSecurityPolicy::allowContentSecurityPolicySourceStarToMatchAnyProtocol() const
{
    if (is<Document>(m_scriptExecutionContext))
        return downcast<Document>(*m_scriptExecutionContext).settings().allowContentSecurityPolicySourceStarToMatchAnyProtocol();
    return false;
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

static Vector<ResourceCryptographicDigest> parseSubResourceIntegrityIntoDigests(const String& subResourceIntegrity)
{
    auto encodedDigests = parseIntegrityMetadata(subResourceIntegrity);
    if (!encodedDigests)
        return { };

    return WTF::compactMap(*encodedDigests, [](auto& encodedDigest) {
        return decodeEncodedResourceCryptographicDigest(encodedDigest);
    });
}

static Vector<ContentSecurityPolicyHash> generateHashesForContent(const StringView content, OptionSet<ContentSecurityPolicyHashAlgorithm> algorithms)
{
    CString utf8Content = content.utf8(StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    Vector<ContentSecurityPolicyHash> hashes;
    for (auto algorithm : algorithms) {
        auto hash = cryptographicDigestForBytes(algorithm, utf8Content.data(), utf8Content.length());
        hashes.append(hash);
    }

    return hashes;
}

bool ContentSecurityPolicy::allowJavaScriptURLs(const String& contextURL, const OrdinalNumber& contextLine, const String& source, Element* element) const
{
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, URL(), "Refused to execute a script", "its hash or 'unsafe-inline'");
        reportViolation(violatedDirective, "inline"_s, consoleMessage, contextURL, source, TextPosition(contextLine, OrdinalNumber()), URL(), nullptr, element);
        if (!didNotifyInspector && violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };

    auto contentHashes = generateHashesForContent(source, m_hashAlgorithmsForInlineScripts);
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForInlineJavascriptURL, contentHashes);
}

bool ContentSecurityPolicy::allowInlineEventHandlers(const String& contextURL, const OrdinalNumber& contextLine, const String& source, Element* element, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, URL(), "Refused to execute a script for an inline event handler", "'unsafe-inline'");
        reportViolation(violatedDirective, "inline"_s, consoleMessage, contextURL, source, TextPosition(contextLine, OrdinalNumber()), URL(), nullptr, element);
        if (!didNotifyInspector && !violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };

    auto contentHashes = generateHashesForContent(source, m_hashAlgorithmsForInlineScripts);
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForInlineEventHandlers, contentHashes);
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

bool ContentSecurityPolicy::shouldPerformEarlyCSPCheck() const
{
    // We perform checks early if strict-dynamic is included in the CSP policy because
    // we have access to necessary information about the script that we do not have later on.
    for (auto& policy : m_policies) {
        if (policy.get()->strictDynamicIncluded())
            return true;
    }
    return false;
}

bool ContentSecurityPolicy::allowNonParserInsertedScripts(const URL& sourceURL, const URL& contextURL, const OrdinalNumber& contextLine, const String& nonce, const StringView& scriptContent, ParserInserted parserInserted) const
{
    if (!shouldPerformEarlyCSPCheck())
        return true;

    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        TextPosition sourcePosition(contextLine, OrdinalNumber());
        const char* message = sourceURL.isEmpty() ? "Refused to execute a script" : "Refused to load";
        String consoleMessage = consoleMessageForViolation(violatedDirective, sourceURL, message);
        reportViolation(violatedDirective, sourceURL.isEmpty() ? "inline"_s : sourceURL.string(), consoleMessage, contextURL.string(), scriptContent, sourcePosition);
    };

    auto contentHashes = generateHashesForContent(scriptContent, m_hashAlgorithmsForInlineScripts);
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForNonParserInsertedScripts, strippedNonce, contentHashes, sourceURL, parserInserted);
}

bool ContentSecurityPolicy::allowInlineScript(const String& contextURL, const OrdinalNumber& contextLine, StringView scriptContent, Element& element, const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy || shouldPerformEarlyCSPCheck())
        return true;
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, URL(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(violatedDirective, "inline"_s, consoleMessage, contextURL, scriptContent, TextPosition(contextLine, OrdinalNumber()), URL(), nullptr, &element);
        if (!didNotifyInspector && !violatedDirective.directiveList().isReportOnly()) {
            reportBlockedScriptExecutionToInspector(violatedDirective.text());
            didNotifyInspector = true;
        }
    };

    auto contentHashes = generateHashesForContent(scriptContent, m_hashAlgorithmsForInlineScripts);
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScriptElement, strippedNonce, contentHashes);
}

bool ContentSecurityPolicy::allowInlineStyle(const String& contextURL, const OrdinalNumber& contextLine, StringView styleContent, CheckUnsafeHashes shouldCheckUnsafeHashes, Element& element, const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (m_overrideInlineStyleAllowed)
        return true;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, URL(), "Refused to apply a stylesheet", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(violatedDirective, "inline"_s, consoleMessage, contextURL, styleContent, TextPosition(contextLine, OrdinalNumber()), URL(), nullptr, &element);
    };

    auto contentHashes = generateHashesForContent(styleContent, m_hashAlgorithmsForInlineStylesheets);
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);

    if (shouldCheckUnsafeHashes == CheckUnsafeHashes::Yes)
        return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyleAttribute, strippedNonce, contentHashes);

    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyleElement, strippedNonce, contentHashes);
}

bool ContentSecurityPolicy::allowEval(JSC::JSGlobalObject* state, LogToConsole shouldLogToConsole, StringView codeContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    bool didNotifyInspector = false;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = shouldLogToConsole == LogToConsole::Yes ? consoleMessageForViolation(violatedDirective, URL(), "Refused to execute a script", "'unsafe-eval'") : String();
        reportViolation(violatedDirective, "eval"_s, consoleMessage, state, codeContent);
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
    auto& topFrame = frame.tree().top();
    if (&frame == &topFrame)
        return true;
    String sourceURL;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, url.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
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
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, url.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestorOrigins, ancestorOrigins);
}

bool ContentSecurityPolicy::allowPluginType(const String& type, const String& typeAttribute, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String sourceURL;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load", "its MIME type");
        reportViolation(violatedDirective, url.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForPluginType, type, typeAttribute);
}

bool ContentSecurityPolicy::allowObjectFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    // As per section object-src of the Content Security Policy Level 3 spec., <http://w3c.github.io/webappsec-csp> (Editor's Draft, 29 February 2016),
    // "If plugin content is loaded without an associated URL (perhaps an object element lacks a data attribute, but loads some default plugin based
    // on the specified type), it MUST be blocked if object-src's value is 'none', but will otherwise be allowed".
    String sourceURL;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    const auto& blockedURL = !preRedirectURL.isNull() ? preRedirectURL : url;
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, blockedURL.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForObjectSource, url, redirectResponseReceived == RedirectResponseReceived::Yes, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::Yes);
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()) || url.protocolIsJavaScript())
        return true;
    String sourceURL;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, url.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForFrame, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowResourceFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, ResourcePredicate resourcePredicate, const URL& preRedirectURL) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    String sourceURL;
    const auto& blockedURL = !preRedirectURL.isNull() ? preRedirectURL : url;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, blockedURL.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), resourcePredicate, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowWorkerFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;

    String sourceURL;
    const auto& blockedURL = !preRedirectURL.isNull() ? preRedirectURL : url;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        auto consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, blockedURL.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };

    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForWorker, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowScriptFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL, const String& subResourceIntegrity, const String& nonce) const
{
    if (shouldPerformEarlyCSPCheck())
        return true;
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;

    String sourceURL;
    const auto& blockedURL = !preRedirectURL.isNull() ? preRedirectURL : url;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, blockedURL.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };

    auto subResourceIntegrityDigests = parseSubResourceIntegrityIntoDigests(subResourceIntegrity);
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForScript, url, redirectResponseReceived == RedirectResponseReceived::Yes, subResourceIntegrityDigests, strippedNonce);
}

bool ContentSecurityPolicy::allowImageFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    return allowResourceFromSource(url, redirectResponseReceived, &ContentSecurityPolicyDirectiveList::violatedDirectiveForImage, preRedirectURL);
}

bool ContentSecurityPolicy::allowPrefetchFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    return allowResourceFromSource(url, redirectResponseReceived, &ContentSecurityPolicyDirectiveList::violatedDirectiveForPrefetch, preRedirectURL);
}

bool ContentSecurityPolicy::allowStyleFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL, const String& nonce) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    String sourceURL;
    const auto& blockedURL = !preRedirectURL.isNull() ? preRedirectURL : url;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to load");
        reportViolation(violatedDirective, blockedURL.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };

    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForStyle, url, redirectResponseReceived == RedirectResponseReceived::Yes, strippedNonce);
}

bool ContentSecurityPolicy::allowFontFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    return allowResourceFromSource(url, redirectResponseReceived, &ContentSecurityPolicyDirectiveList::violatedDirectiveForFont, preRedirectURL);
}

#if ENABLE(APPLICATION_MANIFEST)
bool ContentSecurityPolicy::allowManifestFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    return allowResourceFromSource(url, redirectResponseReceived, &ContentSecurityPolicyDirectiveList::violatedDirectiveForManifest, preRedirectURL);
}
#endif // ENABLE(APPLICATION_MANIFEST)

bool ContentSecurityPolicy::allowMediaFromSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    return allowResourceFromSource(url, redirectResponseReceived, &ContentSecurityPolicyDirectiveList::violatedDirectiveForMedia, preRedirectURL);
}

bool ContentSecurityPolicy::allowConnectToSource(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    String sourceURL;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to connect to");
        reportViolation(violatedDirective, url.string(), consoleMessage, sourceURL, StringView(), sourcePosition, preRedirectURL);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForConnectSource, url, redirectResponseReceived == RedirectResponseReceived::Yes);
}

bool ContentSecurityPolicy::allowFormAction(const URL& url, RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    return allowResourceFromSource(url, redirectResponseReceived, &ContentSecurityPolicyDirectiveList::violatedDirectiveForFormAction, preRedirectURL);
}

bool ContentSecurityPolicy::allowBaseURI(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    String sourceURL;
    TextPosition sourcePosition(OrdinalNumber::beforeFirst(), OrdinalNumber());
    auto handleViolatedDirective = [&] (const ContentSecurityPolicyDirective& violatedDirective) {
        String consoleMessage = consoleMessageForViolation(violatedDirective, url, "Refused to change the document base URL to");
        reportViolation(violatedDirective, url.string(), consoleMessage, sourceURL, StringView(), sourcePosition);
    };
    return allPoliciesAllow(WTFMove(handleViolatedDirective), &ContentSecurityPolicyDirectiveList::violatedDirectiveForBaseURI, url);
}

static bool shouldReportProtocolOnly(const URL& url)
{
    return !url.isHierarchical() || url.protocolIs("file"_s);
}

String ContentSecurityPolicy::createURLForReporting(const URL& url, const String& violatedDirective, bool usesReportingAPI) const
{
    // This implements the deprecated CSP2 "strip uri for reporting" algorithm from https://www.w3.org/TR/CSP2/#violation-reports
    // with the change that cross-origin is considered safe except on some directives.
    // The frame-src, object-src, and block-all-mixed-content directives would allow a website to put another in a frame
    // and listen to emitted securitypolicyviolations.
    bool directiveIsSafe = violatedDirective != ContentSecurityPolicyDirectiveNames::frameSrc
        && violatedDirective != ContentSecurityPolicyDirectiveNames::objectSrc
        && violatedDirective != ContentSecurityPolicyDirectiveNames::blockAllMixedContent;
    auto securityOrigin = static_cast<SecurityOriginData>(*m_selfSource).securityOrigin();

    if (!url.isValid())
        return { };
    if (shouldReportProtocolOnly(url))
        return url.protocol().toString();

    // WPT indicates that modern Reporting API expects explicit port in reported URLs
    //     content-security-policy/reporting-api/report-to-directive-allowed-in-meta.https.sub.html
    //     content-security-policy/reporting-api/reporting-api-sends-reports-on-violation.https.sub.html
    if (securityOrigin->canRequest(url) || directiveIsSafe)
        return usesReportingAPI ? url.strippedForUseAsReferrerWithExplicitPort() : url.strippedForUseAsReferrer();

    return SecurityOrigin::create(url)->toString();
}

void ContentSecurityPolicy::reportViolation(const ContentSecurityPolicyDirective& violatedDirective, const String& blockedURL, const String& consoleMessage, JSC::JSGlobalObject* state, StringView sourceContent) const
{
    // FIXME: Extract source file, and position from JSC::ExecState.
    return reportViolation(violatedDirective.nameForReporting().convertToASCIILowercase(), violatedDirective.directiveList(), blockedURL, consoleMessage, String(), sourceContent, TextPosition(OrdinalNumber::beforeFirst(), OrdinalNumber::beforeFirst()), state);
}

void ContentSecurityPolicy::reportViolation(const String& violatedDirective, const ContentSecurityPolicyDirectiveList& violatedDirectiveList, const String& blockedURL, const String& consoleMessage, JSC::JSGlobalObject* state) const
{
    // FIXME: Extract source file, content, and position from JSC::ExecState.
    return reportViolation(violatedDirective, violatedDirectiveList, blockedURL, consoleMessage, String(), StringView(), TextPosition(OrdinalNumber::beforeFirst(), OrdinalNumber::beforeFirst()), state);
}

void ContentSecurityPolicy::reportViolation(const ContentSecurityPolicyDirective& violatedDirective, const String& blockedURL, const String& consoleMessage, const String& sourceURL, const StringView& sourceContent, const TextPosition& sourcePosition, const URL& preRedirectURL, JSC::JSGlobalObject* state, Element* element) const
{
    return reportViolation(violatedDirective.nameForReporting().convertToASCIILowercase(), violatedDirective.directiveList(), blockedURL, consoleMessage, sourceURL, sourceContent, sourcePosition, state, preRedirectURL, element);
}

void ContentSecurityPolicy::reportViolation(const String& effectiveViolatedDirective, const ContentSecurityPolicyDirectiveList& violatedDirectiveList, const String& blockedURLString, const String& consoleMessage, const String& sourceURL, const StringView& sourceContent, const TextPosition& sourcePosition, JSC::JSGlobalObject* state, const URL& preRedirectURL, Element* element) const
{
    logToConsole(consoleMessage, sourceURL, sourcePosition.m_line, sourcePosition.m_column, state);

    if (!m_isReportingEnabled)
        return;

    // FIXME: Support sending reports from worker.
    CSPInfo info;

    bool usesReportTo = !violatedDirectiveList.reportToTokens().isEmpty();

    String blockedURI;
    if (blockedURLString == "eval"_s || blockedURLString == "inline"_s)
        blockedURI = blockedURLString;
    else {
        // If there is a redirect then we use the pre-redirect URL: https://www.w3.org/TR/CSP3/#security-violation-reports.
        blockedURI = createURLForReporting(preRedirectURL.isNull() ? URL { blockedURLString } : preRedirectURL, effectiveViolatedDirective, usesReportTo);
    }

    info.documentURI = m_documentURL ? m_documentURL.value().strippedForUseAsReferrer() : blockedURI;
    info.lineNumber = sourcePosition.m_line.oneBasedInt();
    info.columnNumber = sourcePosition.m_column.oneBasedInt();
    info.sample = violatedDirectiveList.shouldReportSample(effectiveViolatedDirective) ? sourceContent.left(40).toString() : emptyString();

    if (!m_client) {
        if (!usesReportTo && !is<Document>(m_scriptExecutionContext))
            return;

        auto& document = downcast<Document>(*m_scriptExecutionContext);
        auto* frame = document.frame();
        if (!frame)
            return;

        info.documentURI = shouldReportProtocolOnly(document.url()) ? document.url().protocol().toString() : document.url().strippedForUseAsReferrer();

        auto stack = createScriptCallStack(JSExecState::currentState(), 2);
        auto* callFrame = stack->firstNonNativeCallFrame();
        if (callFrame && callFrame->lineNumber()) {
            info.sourceFile = createURLForReporting(URL { callFrame->sourceURL() }, effectiveViolatedDirective, usesReportTo);
            info.lineNumber = callFrame->lineNumber();
            info.columnNumber = callFrame->columnNumber();
        }
    }
    ASSERT(m_client || is<Document>(m_scriptExecutionContext));

    // FIXME: Is it policy to not use the status code for HTTPS, or is that a bug?
    unsigned short httpStatusCode = m_selfSourceProtocol == "http"_s ? m_httpStatusCode : 0;
    
    // WPT indicate modern Reporting API expect valid status code, regardless of protocol:
    //     content-security-policy/reporting-api/report-to-directive-allowed-in-meta.https.sub.html
    //     content-security-policy/reporting-api/reporting-api-sends-reports-on-violation.https.sub.html
    if (usesReportTo)
        httpStatusCode = m_httpStatusCode;

    // 1. Dispatch violation event.
    SecurityPolicyViolationEventInit violationEventInit;
    violationEventInit.documentURI = info.documentURI;
    violationEventInit.referrer = m_referrer;
    violationEventInit.blockedURI = blockedURI;
    violationEventInit.violatedDirective = effectiveViolatedDirective; // Historical alias to effectiveDirective: https://www.w3.org/TR/CSP3/#violation-events.
    violationEventInit.effectiveDirective = effectiveViolatedDirective;
    violationEventInit.originalPolicy = violatedDirectiveList.header();
    violationEventInit.sourceFile = info.sourceFile;

    // WPT expects modern Reporting API expects source file to be populated with document URI instead of blank:
    //     content-security-policy/reporting-api/report-to-directive-allowed-in-meta.https.sub.html
    //     content-security-policy/reporting-api/reporting-api-sends-reports-on-violation.https.sub.html
    if (usesReportTo && violationEventInit.sourceFile.isNull())
        violationEventInit.sourceFile = info.documentURI;

    violationEventInit.disposition = violatedDirectiveList.isReportOnly() ? SecurityPolicyViolationEventDisposition::Report : SecurityPolicyViolationEventDisposition::Enforce;
    violationEventInit.statusCode = httpStatusCode;
    violationEventInit.lineNumber =  info.lineNumber;
    violationEventInit.columnNumber = info.columnNumber;
    violationEventInit.sample = info.sample;
    violationEventInit.bubbles = true;
    violationEventInit.composed = true;

    Vector<String> endpointURIs;
    Vector<String> endpointTokens;

    auto reportBody = CSPViolationReportBody::create(SecurityPolicyViolationEventInit { violationEventInit });

    if (usesReportTo && m_reportingClient) {
        m_reportingClient->notifyReportObservers(Report::create(reportBody->type(), info.documentURI, reportBody.copyRef()));
        endpointTokens = violatedDirectiveList.reportToTokens();
    } else
        endpointURIs = violatedDirectiveList.reportURIs();

    if (m_client)
        m_client->enqueueSecurityPolicyViolationEvent(WTFMove(violationEventInit));
    else {
        auto& document = downcast<Document>(*m_scriptExecutionContext);
        if (element && element->document() == document)
            element->enqueueSecurityPolicyViolationEvent(WTFMove(violationEventInit));
        else
            document.enqueueSecurityPolicyViolationEvent(WTFMove(violationEventInit));
    }

    // 2. Send violation report (if applicable).
    if (endpointURIs.isEmpty() && endpointTokens.isEmpty())
        return;

    RELEASE_ASSERT(m_reportingClient || (!m_client && !m_scriptExecutionContext));
    if (!m_reportingClient)
        return;

    auto reportURL = m_documentURL ? m_documentURL.value().strippedForUseAsReferrer() : blockedURI;

    auto reportFormData = reportBody->createReportFormDataForViolation(usesReportTo, violatedDirectiveList.isReportOnly());
    m_reportingClient->sendReportToEndpoints(m_protectedURL, endpointURIs, endpointTokens, WTFMove(reportFormData), ViolationReportType::ContentSecurityPolicy);
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name) const
{
    String message;
    if (equalLettersIgnoringASCIICase(name, "allow"_s))
        message = "The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect."_s;
    else if (equalLettersIgnoringASCIICase(name, "options"_s))
        message = "The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect."_s;
    else if (equalLettersIgnoringASCIICase(name, "policy-uri"_s))
        message = "The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header."_s;
    else
        message = makeString("Unrecognized Content-Security-Policy directive '", name, "'.\n"); // FIXME: Why does this include a newline?

    logToConsole(message);
}

void ContentSecurityPolicy::reportDirectiveAsSourceExpression(const String& directiveName, StringView sourceExpression) const
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
        message = "'plugin-types' Content Security Policy directive is empty; all plugins will be blocked.\n"_s;
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
    logToConsole(makeString("The value for Content Security Policy directive '", directiveName, "' contains an invalid character: '", value, "'. Non-whitespace characters outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC 3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1."));
}

void ContentSecurityPolicy::reportInvalidPathCharacter(const String& directiveName, const String& value, const char invalidChar) const
{
    ASSERT(invalidChar == '#' || invalidChar == '?');

    const char* ignoring;
    if (invalidChar == '?')
        ignoring = "The query component, including the '?', will be ignored.";
    else
        ignoring = "The fragment identifier, including the '#', will be ignored.";
    logToConsole(makeString("The source list for Content Security Policy directive '", directiveName, "' contains a source with an invalid path: '", value, "'. ", ignoring));
}

void ContentSecurityPolicy::reportInvalidSourceExpression(const String& directiveName, const String& source) const
{
    logToConsole(makeString("The source list for Content Security Policy directive '", directiveName, "' contains an invalid source: '", source, "'. It will be ignored.",
        equalLettersIgnoringASCIICase(source, "'none'"_s) ? " Note that 'none' has no effect unless it is the only expression in the source list." : ""));
}

void ContentSecurityPolicy::reportMissingReportURI(const String& policy) const
{
    logToConsole("The Content Security Policy '" + policy + "' was delivered in report-only mode, but does not specify a 'report-uri'; the policy will have no effect. Please either add a 'report-uri' directive, or deliver the policy via the 'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::reportMissingReportToTokens(const String& policy) const
{
    logToConsole("The Content Security Policy '" + policy + "' was delivered in report-only mode, but does not specify a 'report-to'; the policy will have no effect. Please either add a 'report-to' directive, or deliver the policy via the 'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::logToConsole(const String& message, const String& contextURL, const OrdinalNumber& contextLine, const OrdinalNumber& contextColumn, JSC::JSGlobalObject* state) const
{
    if (message.isEmpty())
        return;

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
    if (!url.protocolIs("http"_s) && !url.protocolIs("ws"_s))
        return;

    bool upgradeRequest = m_insecureNavigationRequestsToUpgrade.contains(SecurityOriginData::fromURL(url));
    if (requestType == InsecureRequestType::Load || requestType == InsecureRequestType::FormSubmission)
        upgradeRequest |= m_upgradeInsecureRequests;

    if (!upgradeRequest)
        return;

    if (url.protocolIs("http"_s))
        url.setProtocol("https"_s);
    else {
        ASSERT(url.protocolIs("ws"_s));
        url.setProtocol("wss"_s);
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
    if (upgradeURL.protocolIs("https"_s))
        upgradeURL.setProtocol("http"_s);
    else if (upgradeURL.protocolIs("wss"_s))
        upgradeURL.setProtocol("ws"_s);
    
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
