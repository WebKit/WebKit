/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "ContentSecurityPolicyDirective.h"
#include "ContentSecurityPolicyDirectiveList.h"
#include "ContentSecurityPolicyDirectiveNames.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicySource.h"
#include "ContentSecurityPolicySourceList.h"
#include "CryptoDigest.h"
#include "DOMStringList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FormData.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLParserIdioms.h"
#include "InspectorInstrumentation.h"
#include "JSDOMWindowShell.h"
#include "JSMainThreadExecState.h"
#include "ParsingUtilities.h"
#include "PingLoader.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "SecurityPolicyViolationEvent.h"
#include "TextEncoding.h"
#include <inspector/InspectorValues.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextPosition.h>

using namespace Inspector;

namespace WebCore {

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

ContentSecurityPolicy::ContentSecurityPolicy(ScriptExecutionContext& scriptExecutionContext)
    : m_scriptExecutionContext(&scriptExecutionContext)
    , m_sandboxFlags(SandboxNone)
{
    ASSERT(scriptExecutionContext.securityOrigin());
    auto& securityOrigin = *scriptExecutionContext.securityOrigin();
    m_selfSourceProtocol = securityOrigin.protocol();
    m_selfSource = std::make_unique<ContentSecurityPolicySource>(*this, m_selfSourceProtocol, securityOrigin.host(), securityOrigin.port(), emptyString(), false, false);
}

ContentSecurityPolicy::ContentSecurityPolicy(const SecurityOrigin& securityOrigin, const Frame* frame)
    : m_frame(frame)
    , m_sandboxFlags(SandboxNone)
{
    m_selfSourceProtocol = securityOrigin.protocol();
    m_selfSource = std::make_unique<ContentSecurityPolicySource>(*this, m_selfSourceProtocol, securityOrigin.host(), securityOrigin.port(), emptyString(), false, false);
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other) 
{
    ASSERT(m_policies.isEmpty());
    for (auto& policy : other->m_policies)
        didReceiveHeader(policy->header(), policy->headerType(), ContentSecurityPolicy::PolicyFrom::Inherited);
}

void ContentSecurityPolicy::didCreateWindowShell(JSDOMWindowShell& windowShell) const
{
    JSDOMWindow* window = windowShell.window();
    ASSERT(window);
    ASSERT(window->scriptExecutionContext());
    ASSERT(window->scriptExecutionContext()->contentSecurityPolicy() == this);
    if (!windowShell.world().isNormal()) {
        window->setEvalEnabled(true);
        return;
    }
    window->setEvalEnabled(m_lastPolicyEvalDisabledErrorMessage.isNull(), m_lastPolicyEvalDisabledErrorMessage);
}

ContentSecurityPolicyResponseHeaders ContentSecurityPolicy::responseHeaders() const
{
    ContentSecurityPolicyResponseHeaders result;
    result.m_headers.reserveInitialCapacity(m_policies.size());
    for (auto& policy : m_policies)
        result.m_headers.uncheckedAppend({ policy->header(), policy->headerType() });
    return result;
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers, ReportParsingErrors reportParsingErrors)
{
    TemporaryChange<bool> isReportingEnabled(m_isReportingEnabled, reportParsingErrors == ReportParsingErrors::Yes);
    for (auto& header : headers.m_headers)
        didReceiveHeader(header.first, header.second, ContentSecurityPolicy::PolicyFrom::HTTPHeader);
}

void ContentSecurityPolicy::didReceiveHeader(const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicy::PolicyFrom policyFrom)
{
    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    auto characters = StringView(header).upconvertedCharacters();
    const UChar* begin = characters;
    const UChar* position = begin;
    const UChar* end = begin + header.length();
    while (position < end) {
        skipUntil<UChar>(position, end, ',');

        // header1,header2 OR header1
        //        ^                  ^
        std::unique_ptr<ContentSecurityPolicyDirectiveList> policy = ContentSecurityPolicyDirectiveList::create(*this, String(begin, position - begin), type, policyFrom);
        const ContentSecurityPolicyDirective* violatedDirective = policy->violatedDirectiveForUnsafeEval();
        if (violatedDirective && !violatedDirective->directiveList().isReportOnly())
            m_lastPolicyEvalDisabledErrorMessage = policy->evalDisabledErrorMessage();

        m_policies.append(policy.release());

        // Skip the comma, and begin the next header from the current position.
        ASSERT(position == end || *position == ',');
        skipExactly<UChar>(position, end, ',');
        begin = position;
    }

    if (m_scriptExecutionContext)
        applyPolicyToScriptExecutionContext();
}

void ContentSecurityPolicy::applyPolicyToScriptExecutionContext()
{
    ASSERT(m_scriptExecutionContext);
    if (!m_lastPolicyEvalDisabledErrorMessage.isNull())
        m_scriptExecutionContext->disableEval(m_lastPolicyEvalDisabledErrorMessage);
    if (m_sandboxFlags != SandboxNone && is<Document>(m_scriptExecutionContext))
        m_scriptExecutionContext->enforceSandboxFlags(m_sandboxFlags);
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value)
{
    m_overrideInlineStyleAllowed = value;
}

bool ContentSecurityPolicy::urlMatchesSelf(const URL& url) const
{
    return m_selfSource->matches(url);
}

bool ContentSecurityPolicy::protocolMatchesSelf(const URL& url) const
{
    if (equalLettersIgnoringASCIICase(m_selfSourceProtocol, "http"))
        return url.protocolIsInHTTPFamily();
    return equalIgnoringASCIICase(url.protocol(), m_selfSourceProtocol);
}

static CryptoDigest::Algorithm toCryptoDigestAlgorithm(ContentSecurityPolicyHashAlgorithm algorithm)
{
    switch (algorithm) {
    case ContentSecurityPolicyHashAlgorithm::SHA_256:
        return CryptoDigest::Algorithm::SHA_256;
    case ContentSecurityPolicyHashAlgorithm::SHA_384:
        return CryptoDigest::Algorithm::SHA_384;
    case ContentSecurityPolicyHashAlgorithm::SHA_512:
        return CryptoDigest::Algorithm::SHA_512;
    }
    ASSERT_NOT_REACHED();
    return CryptoDigest::Algorithm::SHA_512;
}

template<typename Predicate>
bool ContentSecurityPolicy::foundHashOfContentInAllPolicies(Predicate&& predicate, const String& content, OptionSet<ContentSecurityPolicyHashAlgorithm> algorithms) const
{
    if (algorithms.isEmpty() || content.isEmpty())
        return false;

    // FIXME: We should compute the document encoding once and cache it instead of computing it on each invocation.
    TextEncoding documentEncoding;
    if (is<Document>(m_scriptExecutionContext))
        documentEncoding = downcast<Document>(*m_scriptExecutionContext).textEncoding();
    const TextEncoding& encodingToUse = documentEncoding.isValid() ? documentEncoding : UTF8Encoding();

    // FIXME: Compute the digest with respect to the raw bytes received from the page.
    // See <https://bugs.webkit.org/show_bug.cgi?id=155184>.
    CString contentCString = encodingToUse.encode(content, EntitiesForUnencodables);
    for (auto algorithm : algorithms) {
        auto cryptoDigest = CryptoDigest::create(toCryptoDigestAlgorithm(algorithm));
        cryptoDigest->addBytes(contentCString.data(), contentCString.length());
        Vector<uint8_t> digest = cryptoDigest->computeHash();
        if (!violatedDirectiveInAnyPolicy(std::forward<Predicate>(predicate), std::make_pair(algorithm, digest)))
            return true;
    }
    return false;
}

bool ContentSecurityPolicy::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
    reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
    bool isReportOnly = violatedDirective->directiveList().isReportOnly();
    if (!isReportOnly)
        reportBlockedScriptExecutionToInspector(violatedDirective->text());
    return isReportOnly;
}

bool ContentSecurityPolicy::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), "Refused to execute a script for an inline event handler", "'unsafe-inline'");
    reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
    bool isReportOnly = violatedDirective->directiveList().isReportOnly();
    if (!isReportOnly)
        reportBlockedScriptExecutionToInspector(violatedDirective->text());
    return isReportOnly;
}

bool ContentSecurityPolicy::allowScriptWithNonce(const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    if (strippedNonce.isEmpty())
        return false;
    return !violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptNonce, strippedNonce);
}

bool ContentSecurityPolicy::allowStyleWithNonce(const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    if (strippedNonce.isEmpty())
        return false;
    return !violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleNonce, strippedNonce);
}

bool ContentSecurityPolicy::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& scriptContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (foundHashOfContentInAllPolicies(&ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptHash, scriptContent, m_hashAlgorithmsForInlineScripts))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
    reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
    bool isReportOnly = violatedDirective->directiveList().isReportOnly();
    if (!isReportOnly)
        reportBlockedScriptExecutionToInspector(violatedDirective->text());
    return isReportOnly;
}

bool ContentSecurityPolicy::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& styleContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (m_overrideInlineStyleAllowed)
        return true;
    if (foundHashOfContentInAllPolicies(&ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleHash, styleContent, m_hashAlgorithmsForInlineStylesheets))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyle);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::styleSrc, *violatedDirective, URL(), "Refused to apply a stylesheet", "its hash, its nonce, or 'unsafe-inline'");
    reportViolation(ContentSecurityPolicyDirectiveNames::styleSrc, *violatedDirective, URL(), consoleMessage, contextURL, TextPosition(contextLine, WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowEval(JSC::ExecState* state, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeEval);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), "Refused to execute a script", "'unsafe-eval'");
    reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, URL(), consoleMessage, state);
    bool isReportOnly = violatedDirective->directiveList().isReportOnly();
    if (!isReportOnly)
        reportBlockedScriptExecutionToInspector(violatedDirective->text());
    return isReportOnly;
}

bool ContentSecurityPolicy::allowFrameAncestors(const Frame& frame, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    Frame& topFrame = frame.tree().top();
    if (&frame == &topFrame)
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestor, frame);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::frameAncestors, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::frameAncestors, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowPluginType(const String& type, const String& typeAttribute, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForPluginType, type, typeAttribute);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::pluginTypes, *violatedDirective, url, "Refused to load", "its MIME type");
    reportViolation(ContentSecurityPolicyDirectiveNames::pluginTypes, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowScriptFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForScript, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::scriptSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowObjectFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    // As per section object-src of the Content Security Policy Level 3 spec., <http://w3c.github.io/webappsec-csp> (Editor's Draft, 29 February 2016),
    // "If plugin content is loaded without an associated URL (perhaps an object element lacks a data attribute, but loads some default plugin based
    // on the specified type), it MUST be blocked if object-src's value is 'none', but will otherwise be allowed".
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForObjectSource, url, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::Yes);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::objectSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::objectSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForFrame, url);
    if (!violatedDirective)
        return true;
    const char* effectiveViolatedDirective = violatedDirective->name() == ContentSecurityPolicyDirectiveNames::frameSrc ? ContentSecurityPolicyDirectiveNames::frameSrc : ContentSecurityPolicyDirectiveNames::childSrc;
    String consoleMessage = consoleMessageForViolation(effectiveViolatedDirective, *violatedDirective, url, "Refused to load");
    reportViolation(effectiveViolatedDirective, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowChildContextFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForChildContext, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::childSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::childSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowImageFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForImage, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::imgSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::imgSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowStyleFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForStyle, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::styleSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::styleSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowFontFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForFont, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::fontSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::fontSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowMediaFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForMedia, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::mediaSrc, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::mediaSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowConnectToSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForConnectSource, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::connectSrc, *violatedDirective, url, "Refused to connect to");
    reportViolation(ContentSecurityPolicyDirectiveNames::connectSrc, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowFormAction(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForFormAction, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::formAction, *violatedDirective, url, "Refused to load");
    reportViolation(ContentSecurityPolicyDirectiveNames::formAction, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

bool ContentSecurityPolicy::allowBaseURI(const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;
    const ContentSecurityPolicyDirective* violatedDirective = violatedDirectiveInAnyPolicy(&ContentSecurityPolicyDirectiveList::violatedDirectiveForBaseURI, url);
    if (!violatedDirective)
        return true;
    String consoleMessage = consoleMessageForViolation(ContentSecurityPolicyDirectiveNames::baseURI, *violatedDirective, url, "Refused to change the document base URL to");
    reportViolation(ContentSecurityPolicyDirectiveNames::baseURI, *violatedDirective, url, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber()));
    return violatedDirective->directiveList().isReportOnly();
}

ContentSecurityPolicy::ReflectedXSSDisposition ContentSecurityPolicy::reflectedXSSDisposition() const
{
    ReflectedXSSDisposition disposition = ReflectedXSSUnset;
    for (auto& policy : m_policies) {
        if (policy->reflectedXSSDisposition() > disposition)
            disposition = std::max(disposition, policy->reflectedXSSDisposition());
    }
    return disposition;
}

static String stripURLForUseInReport(Document& document, const URL& url)
{
    if (!url.isValid())
        return String();
    if (!url.isHierarchical() || url.protocolIs("file"))
        return url.protocol();
    return document.securityOrigin()->canRequest(url) ? url.strippedForUseAsReferrer() : SecurityOrigin::create(url).get().toString();
}

void ContentSecurityPolicy::reportViolation(const String& violatedDirective, const ContentSecurityPolicyDirective& effectiveViolatedDirective, const URL& blockedURL, const String& consoleMessage, JSC::ExecState* state) const
{
    // FIXME: Extract source file and source position from JSC::ExecState.
    return reportViolation(violatedDirective, effectiveViolatedDirective, blockedURL, consoleMessage, String(), TextPosition(WTF::OrdinalNumber::beforeFirst(), WTF::OrdinalNumber::beforeFirst()), state);
}

void ContentSecurityPolicy::reportViolation(const String& effectiveViolatedDirective, const ContentSecurityPolicyDirective& violatedDirective, const URL& blockedURL, const String& consoleMessage, const String& sourceURL, const TextPosition& sourcePosition, JSC::ExecState* state) const
{
    logToConsole(consoleMessage, sourceURL, sourcePosition.m_line, state);

    if (!m_isReportingEnabled)
        return;

    // FIXME: Support sending reports from worker.
    if (!is<Document>(m_scriptExecutionContext) && !m_frame)
        return;

    ASSERT(!m_frame || effectiveViolatedDirective == ContentSecurityPolicyDirectiveNames::frameAncestors);

    Document& document = is<Document>(m_scriptExecutionContext) ? downcast<Document>(*m_scriptExecutionContext) : *m_frame->document();
    Frame* frame = document.frame();
    ASSERT(!m_frame || m_frame == frame);
    if (!frame)
        return;

    String documentURI;
    String blockedURI;
    if (is<Document>(m_scriptExecutionContext)) {
        documentURI = document.url().strippedForUseAsReferrer();
        blockedURI = stripURLForUseInReport(document, blockedURL);
    } else {
        // The URL of |document| may not have been initialized (say, when reporting a frame-ancestors violation).
        // So, we use the URL of the blocked document for the protected document URL.
        documentURI = blockedURL;
        blockedURI = blockedURL;
    }
    String violatedDirectiveText = violatedDirective.text();
    String originalPolicy = violatedDirective.directiveList().header();
    String referrer = document.referrer();
    ASSERT(document.loader());
    unsigned short statusCode = document.url().protocolIs("http") && document.loader() ? document.loader()->response().httpStatusCode() : 0;

    String sourceFile;
    int lineNumber = 0;
    int columnNumber = 0;
    RefPtr<ScriptCallStack> stack = createScriptCallStack(JSMainThreadExecState::currentState(), 2);
    const ScriptCallFrame* callFrame = stack->firstNonNativeCallFrame();
    if (callFrame && callFrame->lineNumber()) {
        sourceFile = stripURLForUseInReport(document, URL(URL(), callFrame->sourceURL()));
        lineNumber = callFrame->lineNumber();
        columnNumber = callFrame->columnNumber();
    }

    // 1. Dispatch violation event.
    bool canBubble = false;
    bool cancelable = false;
    document.enqueueDocumentEvent(SecurityPolicyViolationEvent::create(eventNames().securitypolicyviolationEvent, canBubble, cancelable, documentURI, referrer, blockedURI, violatedDirectiveText, effectiveViolatedDirective, originalPolicy, sourceFile, statusCode, lineNumber, columnNumber));

    // 2. Send violation report (if applicable).
    const Vector<String>& reportURIs = violatedDirective.directiveList().reportURIs();
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

    RefPtr<InspectorObject> cspReport = InspectorObject::create();
    cspReport->setString(ASCIILiteral("document-uri"), documentURI);
    cspReport->setString(ASCIILiteral("referrer"), referrer);
    cspReport->setString(ASCIILiteral("violated-directive"), violatedDirectiveText);
    cspReport->setString(ASCIILiteral("effective-directive"), effectiveViolatedDirective);
    cspReport->setString(ASCIILiteral("original-policy"), originalPolicy);
    cspReport->setString(ASCIILiteral("blocked-uri"), blockedURI);
    cspReport->setInteger(ASCIILiteral("status-code"), statusCode);
    if (!sourceFile.isNull()) {
        cspReport->setString(ASCIILiteral("source-file"), sourceFile);
        cspReport->setInteger(ASCIILiteral("line-number"), lineNumber);
        cspReport->setInteger(ASCIILiteral("column-number"), columnNumber);
    }

    RefPtr<InspectorObject> reportObject = InspectorObject::create();
    reportObject->setObject(ASCIILiteral("csp-report"), cspReport.release());

    RefPtr<FormData> report = FormData::create(reportObject->toJSONString().utf8());
    for (const auto& url : reportURIs)
        PingLoader::sendViolationReport(*frame, document.completeURL(url), report.copyRef(), ViolationReportType::ContentSecurityPolicy);
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name) const
{
    String message;
    if (equalLettersIgnoringASCIICase(name, "allow"))
        message = ASCIILiteral("The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect.");
    else if (equalLettersIgnoringASCIICase(name, "options"))
        message = ASCIILiteral("The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect.");
    else if (equalLettersIgnoringASCIICase(name, "policy-uri"))
        message = ASCIILiteral("The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header.");
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

void ContentSecurityPolicy::reportInvalidReflectedXSS(const String& invalidValue) const
{
    logToConsole("The 'reflected-xss' Content Security Policy directive has the invalid value \"" + invalidValue + "\". Value values are \"allow\", \"filter\", and \"block\".");
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

void ContentSecurityPolicy::logToConsole(const String& message, const String& contextURL, const WTF::OrdinalNumber& contextLine, JSC::ExecState* state) const
{
    if (!m_isReportingEnabled)
        return;

    // FIXME: <http://webkit.org/b/114317> ContentSecurityPolicy::logToConsole should include a column number
    if (m_scriptExecutionContext)
        m_scriptExecutionContext->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, contextURL, contextLine.oneBasedInt(), 0, state);
    else if (m_frame && m_frame->document())
        static_cast<ScriptExecutionContext*>(m_frame->document())->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, contextURL, contextLine.oneBasedInt(), 0, state);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(const String& directiveText) const
{
    if (m_scriptExecutionContext)
        InspectorInstrumentation::scriptExecutionBlockedByCSP(m_scriptExecutionContext, directiveText);
}

bool ContentSecurityPolicy::experimentalFeaturesEnabled() const
{
#if ENABLE(CSP_NEXT)
    return RuntimeEnabledFeatures::sharedFeatures().experimentalContentSecurityPolicyFeaturesEnabled();
#else
    return false;
#endif
}
    
}
