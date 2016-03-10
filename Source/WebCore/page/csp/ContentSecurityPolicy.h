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

#ifndef ContentSecurityPolicy_h
#define ContentSecurityPolicy_h

#include "ContentSecurityPolicyResponseHeaders.h"
#include "ScriptState.h"
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

namespace WTF {
class OrdinalNumber;
}

namespace WebCore {

class ContentSecurityPolicyDirectiveList;
class ContentSecurityPolicySource;
class DOMStringList;
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
    explicit ContentSecurityPolicy(const SecurityOrigin&);
    ~ContentSecurityPolicy();

    void copyStateFrom(const ContentSecurityPolicy*);

    // Be sure to update the behavior of XSSAuditor::combineXSSProtectionHeaderAndCSP whenever you change this enum's content or ordering.
    enum ReflectedXSSDisposition {
        ReflectedXSSUnset = 0,
        AllowReflectedXSS,
        ReflectedXSSInvalid,
        FilterReflectedXSS,
        BlockReflectedXSS
    };
    ReflectedXSSDisposition reflectedXSSDisposition() const;

    enum class PolicyFrom {
        HTTPEquivMeta,
        HTTPHeader,
        Inherited,
    };
    ContentSecurityPolicyResponseHeaders responseHeaders() const;
    void didReceiveHeaders(const ContentSecurityPolicyResponseHeaders&);
    void processHTTPEquiv(const String& content, ContentSecurityPolicyHeaderType type) { didReceiveHeader(content, type, ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta); }

    enum class ReportingStatus {
        SendReport,
        SuppressReport
    };
    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& scriptContent, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& styleContent, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowEval(JSC::ExecState* = nullptr, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowPluginType(const String& type, const String& typeAttribute, const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowScriptFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowObjectFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowChildFrameFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowChildContextFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowImageFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowStyleFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowFontFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowMediaFromSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowConnectToSource(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowFormAction(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;
    bool allowBaseURI(const URL&, bool overrideContentSecurityPolicy = false, ContentSecurityPolicy::ReportingStatus = ContentSecurityPolicy::ReportingStatus::SendReport) const;

    void setOverrideAllowInlineStyle(bool);

    bool isActive() const;

    void gatherReportURIs(DOMStringList&) const;

    String evalDisabledErrorMessage() const;

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

    // Used by ContentSecurityPolicyDirectiveList
    void reportDuplicateDirective(const String&) const;
    void reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value) const;
    void reportInvalidSandboxFlags(const String&) const;
    void reportInvalidReflectedXSS(const String&) const;
    void reportInvalidDirectiveInReportOnlyMode(const String&) const;
    void reportInvalidDirectiveInHTTPEquivMeta(const String&) const;
    void reportMissingReportURI(const String&) const;
    void reportUnsupportedDirective(const String&) const;
    void reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const URL& blockedURL, const Vector<String>& reportURIs, const String& header, const String& contextURL = String(), const WTF::OrdinalNumber& contextLine = WTF::OrdinalNumber::beforeFirst(), JSC::ExecState* = nullptr) const;
    void reportBlockedScriptExecutionToInspector(const String& directiveText) const;
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

private:
    void logToConsole(const String& message, const String& contextURL = String(), const WTF::OrdinalNumber& contextLine = WTF::OrdinalNumber::beforeFirst(), JSC::ExecState* = nullptr) const;
    void applyPolicyToScriptExecutionContext();

    void didReceiveHeader(const String&, ContentSecurityPolicyHeaderType, ContentSecurityPolicy::PolicyFrom);

    const TextEncoding& documentEncoding() const;

    ScriptExecutionContext* m_scriptExecutionContext { nullptr };
    std::unique_ptr<ContentSecurityPolicySource> m_selfSource;
    String m_selfSourceProtocol;
    CSPDirectiveListVector m_policies;
    String m_lastPolicyEvalDisabledErrorMessage;
    SandboxFlags m_sandboxFlags;
    bool m_overrideInlineStyleAllowed { false };
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsForInlineScripts;
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsForInlineStylesheets;
};

}

#endif
