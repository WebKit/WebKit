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

#include "ContentSecurityPolicyDirectiveNames.h"
#include "Document.h"
#include "Frame.h"
#include "ParsingUtilities.h"
#include "SecurityContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

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

static inline bool checkSource(ContentSecurityPolicySourceListDirective* directive, const URL& url, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone shouldAllowEmptyURLIfSourceListEmpty = ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::No)
{
    return !directive || directive->allows(url, shouldAllowEmptyURLIfSourceListEmpty);
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
        if (!directive->allows(current->document()->url(), ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::No))
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

ContentSecurityPolicySourceListDirective* ContentSecurityPolicyDirectiveList::operativeDirective(ContentSecurityPolicySourceListDirective* directive) const
{
    return directive ? directive : m_defaultSrc.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeEval() const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkEval(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScript() const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyle() const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptHash(const ContentSecurityPolicyHash& hash) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkHash(operativeDirective, hash))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleHash(const ContentSecurityPolicyHash& hash) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkHash(operativeDirective, hash))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptNonce(const String& nonce) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkNonce(operativeDirective, nonce))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleNonce(const String& nonce) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkNonce(operativeDirective, nonce))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForBaseURI(const URL& url) const
{
    if (checkSource(m_baseURI.get(), url))
        return nullptr;
    return m_baseURI.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForChildContext(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_childSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForConnectSource(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_connectSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFont(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_fontSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFormAction(const URL& url) const
{
    if (checkSource(m_formAction.get(), url))
        return nullptr;
    return m_formAction.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFrame(const URL& url) const
{
    if (url.isBlankURL())
        return nullptr;

    // We must enforce the frame-src directive (if specified) before enforcing the child-src directive for a nested browsing
    // context by <https://w3c.github.io/webappsec-csp/2/#directive-child-src-nested> (29 August 2015).
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_frameSrc ? m_frameSrc.get() : m_childSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestor(const Frame& frame) const
{
    if (checkFrameAncestors(m_frameAncestors.get(), frame))
        return nullptr;
    return m_frameAncestors.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForImage(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_imgSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForMedia(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_mediaSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForObjectSource(const URL& url, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone shouldAllowEmptyURLIfSourceListEmpty) const
{
    if (url.isBlankURL())
        return nullptr;
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_objectSrc.get());
    if (checkSource(operativeDirective, url, shouldAllowEmptyURLIfSourceListEmpty))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForPluginType(const String& type, const String& typeAttribute) const
{
    if (checkMediaType(m_pluginTypes.get(), type, typeAttribute))
        return nullptr;
    return m_pluginTypes.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForScript(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForStyle(const URL& url) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkSource(operativeDirective, url))
        return nullptr;
    return operativeDirective;
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
                if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::sandbox)
                    || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportURI)
                    || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::frameAncestors)) {
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
    directive = std::make_unique<CSPDirectiveType>(*this, name, value);
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

    if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::defaultSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_defaultSrc);
        m_policy.addHashAlgorithmsForInlineScripts(m_defaultSrc->hashAlgorithmsUsed());
        m_policy.addHashAlgorithmsForInlineStylesheets(m_defaultSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::scriptSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_scriptSrc);
        m_policy.addHashAlgorithmsForInlineScripts(m_scriptSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::styleSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_styleSrc);
        m_policy.addHashAlgorithmsForInlineStylesheets(m_styleSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::objectSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_objectSrc);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::frameSrc)) {
        // FIXME: Log to console "The frame-src directive is deprecated. Use the child-src directive instead."
        // See <https://bugs.webkit.org/show_bug.cgi?id=155773>.
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_frameSrc);
    } else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::imgSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_imgSrc);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::fontSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_fontSrc);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::mediaSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_mediaSrc);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::connectSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_connectSrc);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::childSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_childSrc);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::formAction))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_formAction);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::baseURI))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_baseURI);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::frameAncestors)) {
        if (m_reportOnly) {
            m_policy.reportInvalidDirectiveInReportOnlyMode(name);
            return;
        }
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_frameAncestors);
    } else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::pluginTypes))
        setCSPDirective<ContentSecurityPolicyMediaListDirective>(name, value, m_pluginTypes);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::sandbox))
        applySandboxPolicy(name, value);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportURI))
        parseReportURI(name, value);
#if ENABLE(CSP_NEXT)
    else if (m_policy.experimentalFeaturesEnabled()) {
        if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reflectedXSS))
            parseReflectedXSS(name, value);
        else
            m_policy.reportUnsupportedDirective(name);
    }
#endif
    else
        m_policy.reportUnsupportedDirective(name);
}

} // namespace WebCore
