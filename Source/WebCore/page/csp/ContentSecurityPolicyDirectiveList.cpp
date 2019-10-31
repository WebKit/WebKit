/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

namespace WebCore {

static bool isDirectiveNameCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

static bool isDirectiveValueCharacter(UChar c)
{
    return isASCIISpace(c) || (c >= 0x21 && c <= 0x7e); // Whitespace + VCHAR
}

static inline bool checkEval(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowEval();
}

static inline bool checkInline(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowInline();
}

static inline bool checkSource(ContentSecurityPolicySourceListDirective* directive, const URL& url, bool didReceiveRedirectResponse = false, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone shouldAllowEmptyURLIfSourceListEmpty = ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::No)
{
    return !directive || directive->allows(url, didReceiveRedirectResponse, shouldAllowEmptyURLIfSourceListEmpty);
}

static inline bool checkHash(ContentSecurityPolicySourceListDirective* directive, const ContentSecurityPolicyHash& hash)
{
    return !directive || directive->allows(hash);
}

static inline bool checkNonce(ContentSecurityPolicySourceListDirective* directive, const String& nonce)
{
    return !directive || directive->allows(nonce);
}

// Used to compute the comparison URL when checking frame-ancestors. We do this weird conversion so that child
// frames of a page with a unique origin (e.g. about:blank) are not blocked due to their frame-ancestors policy
// and do not need to add the parent's URL to their policy. The latter could allow the child page to be framed
// by anyone. See <https://github.com/w3c/webappsec/issues/311> for more details.
static inline URL urlFromOrigin(const SecurityOrigin& origin)
{
    return { URL { }, origin.toString() };
}

static inline bool checkFrameAncestors(ContentSecurityPolicySourceListDirective* directive, const Frame& frame)
{
    if (!directive)
        return true;
    bool didReceiveRedirectResponse = false;
    for (Frame* current = frame.tree().parent(); current; current = current->tree().parent()) {
        URL origin = urlFromOrigin(current->document()->securityOrigin());
        if (!origin.isValid() || !directive->allows(origin, didReceiveRedirectResponse, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::No))
            return false;
    }
    return true;
}

static inline bool checkFrameAncestors(ContentSecurityPolicySourceListDirective* directive, const Vector<RefPtr<SecurityOrigin>>& ancestorOrigins)
{
    if (!directive)
        return true;
    bool didReceiveRedirectResponse = false;
    for (auto& origin : ancestorOrigins) {
        URL originURL = urlFromOrigin(*origin);
        if (!originURL.isValid() || !directive->allows(originURL, didReceiveRedirectResponse, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::No))
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
{
    m_reportOnly = (type == ContentSecurityPolicyHeaderType::Report || type == ContentSecurityPolicyHeaderType::PrefixedReport);
}

std::unique_ptr<ContentSecurityPolicyDirectiveList> ContentSecurityPolicyDirectiveList::create(ContentSecurityPolicy& policy, const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicy::PolicyFrom from)
{
    auto directives = makeUnique<ContentSecurityPolicyDirectiveList>(policy, type);
    directives->parse(header, from);

    if (!checkEval(directives->operativeDirective(directives->m_scriptSrc.get()))) {
        String evalDisabledMessage = makeString("Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"", directives->operativeDirective(directives->m_scriptSrc.get())->text(), "\".\n");
        directives->setEvalDisabledErrorMessage(evalDisabledMessage);
        String webAssemblyDisabledMessage = makeString("Refused to create a WebAssembly object because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"", directives->operativeDirective(directives->m_scriptSrc.get())->text(), "\".\n");
        directives->setWebAssemblyDisabledErrorMessage(webAssemblyDisabledMessage);
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

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForChildContext(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_childSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForConnectSource(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_connectSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFont(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_fontSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFormAction(const URL& url, bool didReceiveRedirectResponse) const
{
    if (checkSource(m_formAction.get(), url, didReceiveRedirectResponse))
        return nullptr;
    return m_formAction.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFrame(const URL& url, bool didReceiveRedirectResponse) const
{
    if (url.protocolIsAbout())
        return nullptr;

    // We must enforce the frame-src directive (if specified) before enforcing the child-src directive for a nested browsing
    // context by <https://w3c.github.io/webappsec-csp/2/#directive-child-src-nested> (29 August 2015).
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_frameSrc ? m_frameSrc.get() : m_childSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestor(const Frame& frame) const
{
    if (checkFrameAncestors(m_frameAncestors.get(), frame))
        return nullptr;
    return m_frameAncestors.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFrameAncestorOrigins(const Vector<RefPtr<SecurityOrigin>>& ancestorOrigins) const
{
    if (checkFrameAncestors(m_frameAncestors.get(), ancestorOrigins))
        return nullptr;
    return m_frameAncestors.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForImage(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_imgSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

#if ENABLE(APPLICATION_MANIFEST)
const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForManifest(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_manifestSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}
#endif // ENABLE(APPLICATION_MANIFEST)

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForMedia(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_mediaSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForObjectSource(const URL& url, bool didReceiveRedirectResponse, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone shouldAllowEmptyURLIfSourceListEmpty) const
{
    if (url.protocolIsAbout())
        return nullptr;
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_objectSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse, shouldAllowEmptyURLIfSourceListEmpty))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForPluginType(const String& type, const String& typeAttribute) const
{
    if (checkMediaType(m_pluginTypes.get(), type, typeAttribute))
        return nullptr;
    return m_pluginTypes.get();
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForScript(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_scriptSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForStyle(const URL& url, bool didReceiveRedirectResponse) const
{
    ContentSecurityPolicySourceListDirective* operativeDirective = this->operativeDirective(m_styleSrc.get());
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
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
            if (policyFrom == ContentSecurityPolicy::PolicyFrom::Inherited) {
                if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::upgradeInsecureRequests))
                    continue;
            } else if (policyFrom == ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta) {
                if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::sandbox)
                    || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportURI)
                    || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::frameAncestors)) {
                    m_policy.reportInvalidDirectiveInHTTPEquivMeta(name);
                    continue;
                }
            } else if (policyFrom == ContentSecurityPolicy::PolicyFrom::InheritedForPluginDocument) {
                if (!equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::pluginTypes)
                    && !equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportURI))
                    continue;
            }
            addDirective(name, value);
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
    directive = makeUnique<CSPDirectiveType>(*this, name, value);
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

void ContentSecurityPolicyDirectiveList::setUpgradeInsecureRequests(const String& name)
{
    if (m_reportOnly) {
        m_policy.reportInvalidDirectiveInReportOnlyMode(name);
        return;
    }
    if (m_upgradeInsecureRequests) {
        m_policy.reportDuplicateDirective(name);
        return;
    }
    m_upgradeInsecureRequests = true;
    m_policy.setUpgradeInsecureRequests(true);
}

void ContentSecurityPolicyDirectiveList::setBlockAllMixedContentEnabled(const String& name)
{
    if (m_hasBlockAllMixedContentDirective) {
        m_policy.reportDuplicateDirective(name);
        return;
    }
    m_hasBlockAllMixedContentDirective = true;
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
#if ENABLE(APPLICATION_MANIFEST)
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::manifestSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(name, value, m_manifestSrc);
#endif
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
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::upgradeInsecureRequests))
        setUpgradeInsecureRequests(name);
    else if (equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::blockAllMixedContent))
        setBlockAllMixedContentEnabled(name);
    else
        m_policy.reportUnsupportedDirective(name);
}

} // namespace WebCore
