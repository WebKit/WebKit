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

#include "config.h"
#include "ContentSecurityPolicyDirectiveList.h"

#include "Document.h"
#include "Frame.h"
#include "ParsingUtilities.h"
#include "SecurityContext.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// CSP 1.0 Directives
static const char connectSrc[] = "connect-src";
static const char defaultSrc[] = "default-src";
static const char fontSrc[] = "font-src";
static const char frameSrc[] = "frame-src";
static const char imgSrc[] = "img-src";
static const char mediaSrc[] = "media-src";
static const char objectSrc[] = "object-src";
static const char reportURI[] = "report-uri";
static const char sandbox[] = "sandbox";
static const char scriptSrc[] = "script-src";
static const char styleSrc[] = "style-src";

// CSP 1.1 Directives
static const char baseURI[] = "base-uri";
static const char childSrc[] = "child-src";
static const char formAction[] = "form-action";
static const char pluginTypes[] = "plugin-types";
static const char frameAncestors[] = "frame-ancestors";
#if ENABLE(CSP_NEXT)
static const char reflectedXSS[] = "reflected-xss";
#endif

#if ENABLE(CSP_NEXT)

static inline bool isExperimentalDirectiveName(const String& name)
{
    return equalLettersIgnoringASCIICase(name, reflectedXSS);
}

#else

static inline bool isExperimentalDirectiveName(const String&)
{
    return false;
}

#endif

bool isCSPDirectiveName(const String& name)
{
    return equalLettersIgnoringASCIICase(name, baseURI)
        || equalLettersIgnoringASCIICase(name, connectSrc)
        || equalLettersIgnoringASCIICase(name, defaultSrc)
        || equalLettersIgnoringASCIICase(name, fontSrc)
        || equalLettersIgnoringASCIICase(name, formAction)
        || equalLettersIgnoringASCIICase(name, frameSrc)
        || equalLettersIgnoringASCIICase(name, imgSrc)
        || equalLettersIgnoringASCIICase(name, mediaSrc)
        || equalLettersIgnoringASCIICase(name, objectSrc)
        || equalLettersIgnoringASCIICase(name, pluginTypes)
        || equalLettersIgnoringASCIICase(name, reportURI)
        || equalLettersIgnoringASCIICase(name, sandbox)
        || equalLettersIgnoringASCIICase(name, scriptSrc)
        || equalLettersIgnoringASCIICase(name, styleSrc)
        || isExperimentalDirectiveName(name);
}

static bool isDirectiveNameCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

static bool isDirectiveValueCharacter(UChar c)
{
    return isASCIISpace(c) || (c >= 0x21 && c <= 0x7e); // Whitespace + VCHAR
}

static bool isNotASCIISpace(UChar c)
{
    return !isASCIISpace(c);
}

static inline bool checkEval(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowEval();
}

static inline bool checkInline(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowInline();
}

static inline bool checkSource(ContentSecurityPolicySourceListDirective* directive, const URL& url)
{
    return !directive || directive->allows(url);
}

static inline bool checkHash(ContentSecurityPolicySourceListDirective* directive, const ContentSecurityPolicyHash& hash)
{
    return !directive || directive->allows(hash);
}

static inline bool checkNonce(ContentSecurityPolicySourceListDirective* directive, const String& nonce)
{
    return !directive || directive->allows(nonce);
}

static inline bool checkFrameAncestors(ContentSecurityPolicySourceListDirective* directive, const Frame& frame)
{
    if (!directive)
        return true;
    for (Frame* current = frame.tree().parent(); current; current = current->tree().parent()) {
        if (!directive->allows(current->document()->url()))
            return false;
    }
    return true;
}

static inline bool checkMediaType(ContentSecurityPolicyMediaListDirective* directive, const String& type, const String& typeAttribute)
{
    if (!directive)
        return true;
    if (typeAttribute.isEmpty() || typeAttribute.stripWhiteSpace() != type)
        return false;
    return directive->allows(type);
}

ContentSecurityPolicyDirectiveList::ContentSecurityPolicyDirectiveList(ContentSecurityPolicy& policy, ContentSecurityPolicyHeaderType type)
    : m_policy(policy)
    , m_headerType(type)
    , m_reportOnly(false)
    , m_haveSandboxPolicy(false)
    , m_reflectedXSSDisposition(ContentSecurityPolicy::ReflectedXSSUnset)
{
    m_reportOnly = (type == ContentSecurityPolicyHeaderType::Report || type == ContentSecurityPolicyHeaderType::PrefixedReport);
}

std::unique_ptr<ContentSecurityPolicyDirectiveList> ContentSecurityPolicyDirectiveList::create(ContentSecurityPolicy& policy, const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicy::PolicyFrom from)
{
    auto directives = std::make_unique<ContentSecurityPolicyDirectiveList>(policy, type);
    directives->parse(header, from);

    if (!checkEval(directives->operativeDirective(directives->m_scriptSrc.get()))) {
        String message = makeString("Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"", directives->operativeDirective(directives->m_scriptSrc.get())->text(), "\".\n");
        directives->setEvalDisabledErrorMessage(message);
    }

    if (directives->isReportOnly() && directives->reportURIs().isEmpty())
        policy.reportMissingReportURI(header);

    return directives;
}

void ContentSecurityPolicyDirectiveList::reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const URL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine, JSC::ExecState* state) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy.reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header, contextURL, contextLine, state);
}

ContentSecurityPolicySourceListDirective* ContentSecurityPolicyDirectiveList::operativeDirective(ContentSecurityPolicySourceListDirective* directive) const
{
    return directive ? directive : m_defaultSrc.get();
}

static String consoleMessageForViolation(const String& effectiveDirective, const URL& blockedURL, bool violatesDefaultSrc, const char* prefix, const char* subject = "it")
{
    StringBuilder result;
    result.append(prefix);
    if (!blockedURL.isEmpty()) {
        result.append(' ');
        result.append(blockedURL.stringCenterEllipsizedToLength());
    }
    result.appendLiteral(" because ");
    result.append(subject);
    if (violatesDefaultSrc) {
        result.appendLiteral(" appears in neither the ");
        result.append(effectiveDirective);
        result.appendLiteral(" directive nor the default-src directive of the Content Security Policy.");
    } else {
        result.appendLiteral(" does not appear in the ");
        result.append(effectiveDirective);
        result.appendLiteral(" directive of the Content Security Policy.");
    }
    return result.toString();
}

bool ContentSecurityPolicyDirectiveList::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkInline(operativeDirective))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(scriptSrc, URL(), operativeDirective == m_defaultSrc.get(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(operativeDirective->text(), scriptSrc, consoleMessage, URL(), contextURL, contextLine);
        if (!m_reportOnly)
            m_policy.reportBlockedScriptExecutionToInspector(operativeDirective->text());
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkInline(operativeDirective))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(scriptSrc, URL(), operativeDirective == m_defaultSrc.get(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(operativeDirective->text(), scriptSrc, consoleMessage, URL(), contextURL, contextLine);
        if (!m_reportOnly)
            m_policy.reportBlockedScriptExecutionToInspector(operativeDirective->text());
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkInline(operativeDirective))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(scriptSrc, URL(), operativeDirective == m_defaultSrc.get(), "Refused to execute a script", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(operativeDirective->text(), scriptSrc, consoleMessage, URL(), contextURL, contextLine);
        if (!m_reportOnly)
            m_policy.reportBlockedScriptExecutionToInspector(operativeDirective->text());
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowInlineScriptWithHash(const ContentSecurityPolicyHash& hash) const
{
    return checkHash(operativeDirective(m_scriptSrc.get()), hash);
}

bool ContentSecurityPolicyDirectiveList::allowScriptWithNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_scriptSrc.get()), nonce);
}

bool ContentSecurityPolicyDirectiveList::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkInline(operativeDirective))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(styleSrc, URL(), operativeDirective == m_defaultSrc.get(), "Refused to apply a stylesheet", "its hash, its nonce, or 'unsafe-inline'");
        reportViolation(operativeDirective->text(), styleSrc, consoleMessage, URL(), contextURL, contextLine);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowInlineStyleWithHash(const ContentSecurityPolicyHash& hash) const
{
    return checkHash(operativeDirective(m_styleSrc.get()), hash);
}

bool ContentSecurityPolicyDirectiveList::allowStyleWithNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_styleSrc.get()), nonce);
}

bool ContentSecurityPolicyDirectiveList::allowEval(JSC::ExecState* state, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkEval(operativeDirective))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(scriptSrc, URL(), operativeDirective == m_defaultSrc.get(), "Refused to execute a script", "'unsafe-eval'");
        // FIXME: Query JSC::ExecState for source file, line and column information.
        reportViolation(operativeDirective->text(), scriptSrc, consoleMessage, URL(), String(), WTF::OrdinalNumber::beforeFirst(), state);
        if (!m_reportOnly)
            m_policy.reportBlockedScriptExecutionToInspector(operativeDirective->text());
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowPluginType(const String& type, const String& typeAttribute, const URL& url, ReportingStatus reportingStatus) const
{
    if (checkMediaType(m_pluginTypes.get(), type, typeAttribute))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        bool violatesDefaultSrc = false;
        String consoleMessage = consoleMessageForViolation(pluginTypes, url, violatesDefaultSrc, "Refused to load", "its MIME type");
        reportViolation(m_pluginTypes->text(), pluginTypes, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowScriptFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(scriptSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), scriptSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowObjectFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    if (url.isBlankURL())
        return true;
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_objectSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(objectSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), objectSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowChildContextFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_childSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(childSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), childSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowChildFrameFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    if (url.isBlankURL())
        return true;

    // We must enforce the frame-src directive (if specified) before enforcing the child-src directive for a nested browsing
    // context by <https://w3c.github.io/webappsec-csp/2/#directive-child-src-nested> (29 August 2015).
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_frameSrc ? m_frameSrc.get() : m_childSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(frameSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), frameSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowImageFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_imgSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(imgSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), imgSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowStyleFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(styleSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), styleSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowFontFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_fontSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(fontSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), fontSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowMediaFromSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_mediaSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(mediaSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to load");
        reportViolation(operativeDirective->text(), mediaSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowConnectToSource(const URL& url, ReportingStatus reportingStatus) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_connectSrc.get());
    if (checkSource(operativeDirective, url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        String consoleMessage = consoleMessageForViolation(connectSrc, url, operativeDirective == m_defaultSrc.get(), "Refused to connect to");
        reportViolation(operativeDirective->text(), connectSrc, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowFormAction(const URL& url, ReportingStatus reportingStatus) const
{
    if (checkSource(m_formAction.get(), url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        bool violatesDefaultSrc = false;
        String consoleMessage = consoleMessageForViolation(formAction, url, violatesDefaultSrc, "Refused to load");
        reportViolation(m_formAction->text(), formAction, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowBaseURI(const URL& url, ReportingStatus reportingStatus) const
{
    if (checkSource(m_baseURI.get(), url))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        bool violatesDefaultSrc = false;
        String consoleMessage = consoleMessageForViolation(baseURI, url, violatesDefaultSrc, "Refused to change the document base URL to");
        reportViolation(m_baseURI->text(), baseURI, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

bool ContentSecurityPolicyDirectiveList::allowFrameAncestors(const Frame& frame, const URL& url, ReportingStatus reportingStatus) const
{
    if (checkFrameAncestors(m_frameAncestors.get(), frame))
        return true;
    if (reportingStatus == ReportingStatus::SendReport) {
        bool violatesDefaultSrc = false;
        String consoleMessage = consoleMessageForViolation(frameAncestors, url, violatesDefaultSrc, "Refused to load");
        reportViolation(m_frameAncestors->text(), frameAncestors, consoleMessage, url);
    }
    return denyIfEnforcingPolicy();
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void ContentSecurityPolicyDirectiveList::parse(const String& policy, ContentSecurityPolicy::PolicyFrom policyFrom)
{
    m_header = policy;
    if (policy.isEmpty())
        return;

    auto characters = StringView(policy).upconvertedCharacters();
    const UChar* position = characters;
    const UChar* end = position + policy.length();

    while (position < end) {
        const UChar* directiveBegin = position;
        skipUntil<UChar>(position, end, ';');

        String name, value;
        if (parseDirective(directiveBegin, position, name, value)) {
            ASSERT(!name.isEmpty());
            switch (policyFrom) {
            case ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta:
                if (equalLettersIgnoringASCIICase(name, sandbox) || equalLettersIgnoringASCIICase(name, reportURI)
                    || equalLettersIgnoringASCIICase(name, frameAncestors)) {
                    m_policy.reportInvalidDirectiveInHTTPEquivMeta(name);
                    break;
                }
                FALLTHROUGH;
            default:
                addDirective(name, value);
                break;
            }
        }

        ASSERT(position == end || *position == ';');
        skipExactly<UChar>(position, end, ';');
    }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool ContentSecurityPolicyDirectiveList::parseDirective(const UChar* begin, const UChar* end, String& name, String& value)
{
    ASSERT(name.isEmpty());
    ASSERT(value.isEmpty());

    const UChar* position = begin;
    skipWhile<UChar, isASCIISpace>(position, end);

    // Empty directive (e.g. ";;;"). Exit early.
    if (position == end)
        return false;

    const UChar* nameBegin = position;
    skipWhile<UChar, isDirectiveNameCharacter>(position, end);

    // The directive-name must be non-empty.
    if (nameBegin == position) {
        skipWhile<UChar, isNotASCIISpace>(position, end);
        m_policy.reportUnsupportedDirective(String(nameBegin, position - nameBegin));
        return false;
    }

    name = String(nameBegin, position - nameBegin);

    if (position == end)
        return true;

    if (!skipExactly<UChar, isASCIISpace>(position, end)) {
        skipWhile<UChar, isNotASCIISpace>(position, end);
        m_policy.reportUnsupportedDirective(String(nameBegin, position - nameBegin));
        return false;
    }

    skipWhile<UChar, isASCIISpace>(position, end);

    const UChar* valueBegin = position;
    skipWhile<UChar, isDirectiveValueCharacter>(position, end);

    if (position != end) {
        m_policy.reportInvalidDirectiveValueCharacter(name, String(valueBegin, end - valueBegin));
        return false;
    }

    // The directive-value may be empty.
    if (valueBegin == position)
        return true;

    value = String(valueBegin, position - valueBegin);
    return true;
}

void ContentSecurityPolicyDirectiveList::parseReportURI(const String& name, const String& value)
{
    if (!m_reportURIs.isEmpty()) {
        m_policy.reportDuplicateDirective(name);
        return;
    }

    auto characters = StringView(value).upconvertedCharacters();
    const UChar* position = characters;
    const UChar* end = position + value.length();

    while (position < end) {
        skipWhile<UChar, isASCIISpace>(position, end);

        const UChar* urlBegin = position;
        skipWhile<UChar, isNotASCIISpace>(position, end);

        if (urlBegin < position)
            m_reportURIs.append(value.substring(urlBegin - characters, position - urlBegin));
    }
}


template<class CSPDirectiveType>
void ContentSecurityPolicyDirectiveList::setCSPDirective(const String& name, const String& value, std::unique_ptr<CSPDirectiveType>& directive)
{
    if (directive) {
        m_policy.reportDuplicateDirective(name);
        return;
    }
    directive = std::make_unique<CSPDirectiveType>(name, value, m_policy);
}

void ContentSecurityPolicyDirectiveList::applySandboxPolicy(const String& name, const String& sandboxPolicy)
{
    if (m_reportOnly) {
        m_policy.reportInvalidDirectiveInReportOnlyMode(name);
        return;
    }
    if (m_haveSandboxPolicy) {
        m_policy.reportDuplicateDirective(name);
        return;
    }
    m_haveSandboxPolicy = true;
    String invalidTokens;
    m_policy.enforceSandboxFlags(SecurityContext::parseSandboxPolicy(sandboxPolicy, invalidTokens));
    if (!invalidTokens.isNull())
        m_policy.reportInvalidSandboxFlags(invalidTokens);
}

void ContentSecurityPolicyDirectiveList::parseReflectedXSS(const String& name, const String& value)
{
    if (m_reflectedXSSDisposition != ContentSecurityPolicy::ReflectedXSSUnset) {
        m_policy.reportDuplicateDirective(name);
        m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
        return;
    }

    if (value.isEmpty()) {
        m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
        m_policy.reportInvalidReflectedXSS(value);
        return;
    }

    auto characters = StringView(value).upconvertedCharacters();
    const UChar* position = characters;
    const UChar* end = position + value.length();

    skipWhile<UChar, isASCIISpace>(position, end);
    const UChar* begin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    // value1
    //       ^
    if (equalLettersIgnoringASCIICase(begin, position - begin, "allow"))
        m_reflectedXSSDisposition = ContentSecurityPolicy::AllowReflectedXSS;
    else if (equalLettersIgnoringASCIICase(begin, position - begin, "filter"))
        m_reflectedXSSDisposition = ContentSecurityPolicy::FilterReflectedXSS;
    else if (equalLettersIgnoringASCIICase(begin, position - begin, "block"))
        m_reflectedXSSDisposition = ContentSecurityPolicy::BlockReflectedXSS;
    else {
        m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
        m_policy.reportInvalidReflectedXSS(value);
        return;
    }

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end && m_reflectedXSSDisposition != ContentSecurityPolicy::ReflectedXSSUnset)
        return;

    // value1 value2
    //        ^
    m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
    m_policy.reportInvalidReflectedXSS(value);
}

void ContentSecurityPolicyDirectiveList::addDirective(const String& name, const String& value)
{
    ASSERT(!name.isEmpty());

    if (equalLettersIgnoringASCIICase(name, defaultSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_defaultSrc);
        m_policy.addHashAlgorithmsForInlineScripts(m_defaultSrc->hashAlgorithmsUsed());
        m_policy.addHashAlgorithmsForInlineStylesheets(m_defaultSrc->hashAlgorithmsUsed());
    } else if (equalLettersIgnoringASCIICase(name, scriptSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_scriptSrc);
        m_policy.addHashAlgorithmsForInlineScripts(m_scriptSrc->hashAlgorithmsUsed());
    } else if (equalLettersIgnoringASCIICase(name, styleSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_styleSrc);
        m_policy.addHashAlgorithmsForInlineStylesheets(m_styleSrc->hashAlgorithmsUsed());
    } else if (equalLettersIgnoringASCIICase(name, objectSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_objectSrc);
    else if (equalLettersIgnoringASCIICase(name, frameSrc)) {
        // FIXME: Log to console "The frame-src directive is deprecated. Use the child-src directive instead."
        // See <https://bugs.webkit.org/show_bug.cgi?id=155773>.
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_frameSrc);
    } else if (equalLettersIgnoringASCIICase(name, imgSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_imgSrc);
    else if (equalLettersIgnoringASCIICase(name, fontSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_fontSrc);
    else if (equalLettersIgnoringASCIICase(name, mediaSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_mediaSrc);
    else if (equalLettersIgnoringASCIICase(name, connectSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_connectSrc);
    else if (equalLettersIgnoringASCIICase(name, childSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_childSrc);
    else if (equalLettersIgnoringASCIICase(name, formAction))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_formAction);
    else if (equalLettersIgnoringASCIICase(name, baseURI))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_baseURI);
    else if (equalLettersIgnoringASCIICase(name, frameAncestors)) {
        if (m_reportOnly) {
            m_policy.reportInvalidDirectiveInReportOnlyMode(name);
            return;
        }
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_frameAncestors);
    } else if (equalLettersIgnoringASCIICase(name, pluginTypes))
        setCSPDirective<ContentSecurityPolicyMediaListDirective>(name, value, m_pluginTypes);
    else if (equalLettersIgnoringASCIICase(name, sandbox))
        applySandboxPolicy(name, value);
    else if (equalLettersIgnoringASCIICase(name, reportURI))
        parseReportURI(name, value);
#if ENABLE(CSP_NEXT)
    else if (m_policy.experimentalFeaturesEnabled()) {
        if (equalLettersIgnoringASCIICase(name, reflectedXSS))
            parseReflectedXSS(name, value);
        else
            m_policy.reportUnsupportedDirective(name);
    }
#endif
    else
        m_policy.reportUnsupportedDirective(name);
}

} // namespace WebCore
