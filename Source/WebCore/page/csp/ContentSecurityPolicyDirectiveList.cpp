/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "HTTPParsers.h"
#include "ParsingUtilities.h"
#include "SecurityContext.h"
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

template<typename CharacterType> static bool isDirectiveNameCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

template<typename CharacterType> static bool isDirectiveValueCharacter(CharacterType c)
{
    return isASCIISpace(c) || (c >= 0x21 && c <= 0x7e); // Whitespace + VCHAR
}

static inline bool checkEval(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowEval();
}

static inline bool checkWasmEval(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowWasmEval();
}

static inline bool checkInline(ContentSecurityPolicySourceListDirective* directive)
{
    return !directive || directive->allowInline();
}

static inline bool checkUnsafeHashes(ContentSecurityPolicySourceListDirective* directive, const Vector<ContentSecurityPolicyHash>& hashes)
{
    return !directive || directive->allowUnsafeHashes(hashes);
}

static inline bool checkNonParserInsertedScripts(ContentSecurityPolicySourceListDirective* directive, ParserInserted parserInserted)
{
    if (!directive)
        return true;

    return directive->allowNonParserInsertedScripts() && parserInserted == ParserInserted::No;
}

static inline bool checkSource(ContentSecurityPolicySourceListDirective* directive, const URL& url, bool didReceiveRedirectResponse = false, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone shouldAllowEmptyURLIfSourceListEmpty = ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone::No)
{
    return !directive || directive->allows(url, didReceiveRedirectResponse, shouldAllowEmptyURLIfSourceListEmpty);
}

static inline bool checkHashes(ContentSecurityPolicySourceListDirective* directive, const Vector<ContentSecurityPolicyHash>& hashes)
{
    return !directive || directive->allows(hashes);
}

static inline bool checkNonce(ContentSecurityPolicySourceListDirective* directive, const String& nonce)
{
    return !directive || directive->allows(nonce);
}

// Used to compute the comparison URL when checking frame-ancestors. We do this weird conversion so that child
// frames of a page with an opaque origin (e.g. about:blank) are not blocked due to their frame-ancestors policy
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
    for (auto* current = frame.tree().parent(); current; current = current->tree().parent()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(current);
        if (!localFrame)
            continue;
        URL origin = urlFromOrigin(localFrame->document()->securityOrigin());
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
    , m_reportOnly(type == ContentSecurityPolicyHeaderType::Report)
{
}

std::unique_ptr<ContentSecurityPolicyDirectiveList> ContentSecurityPolicyDirectiveList::create(ContentSecurityPolicy& policy, const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicy::PolicyFrom from)
{
    auto directives = makeUnique<ContentSecurityPolicyDirectiveList>(policy, type);
    directives->parse(header, from);

    if (!checkEval(directives->operativeDirective(directives->m_scriptSrc.get(), ContentSecurityPolicyDirectiveNames::scriptSrc)))
        directives->setEvalDisabledErrorMessage(makeString("Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"", directives->operativeDirective(directives->m_scriptSrc.get(), ContentSecurityPolicyDirectiveNames::scriptSrc)->text(), "\".\n"));

    if (!checkWasmEval(directives->operativeDirective(directives->m_scriptSrc.get(), ContentSecurityPolicyDirectiveNames::scriptSrc)))
        directives->setWebAssemblyDisabledErrorMessage(makeString("Refused to create a WebAssembly object because 'unsafe-eval' or 'wasm-unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"", directives->operativeDirective(directives->m_scriptSrc.get(), ContentSecurityPolicyDirectiveNames::scriptSrc)->text(), "\".\n"));

    if (directives->isReportOnly() && (directives->reportToTokens().isEmpty() && directives->reportURIs().isEmpty()))
        policy.reportMissingReportToTokens(header);

    return directives;
}

ContentSecurityPolicySourceListDirective* ContentSecurityPolicyDirectiveList::operativeDirectiveForWorkerSrc(ContentSecurityPolicySourceListDirective* directive, const String& nameForReporting) const
{
    // worker-src defers to child-src, then script-src, then default-src (https://www.w3.org/TR/CSP3/#changes-from-level-2).
    if (directive) {
        directive->setNameForReporting(nameForReporting);
        return directive;
    }

    if (m_childSrc.get()) {
        m_childSrc.get()->setNameForReporting(nameForReporting);
        return m_childSrc.get();
    }

    return operativeDirective(m_scriptSrc.get(), nameForReporting);
}

ContentSecurityPolicySourceListDirective* ContentSecurityPolicyDirectiveList::operativeDirective(ContentSecurityPolicySourceListDirective* directive, const String& nameForReporting) const
{
    if (directive) {
        directive->setNameForReporting(nameForReporting);
        return directive;
    }

    if (m_defaultSrc.get())
        m_defaultSrc.get()->setNameForReporting(nameForReporting);

    return m_defaultSrc.get();
}

ContentSecurityPolicySourceListDirective* ContentSecurityPolicyDirectiveList::operativeDirectiveScript(ContentSecurityPolicySourceListDirective* directive, const String& nameForReporting) const
{
    if (directive) {
        directive->setNameForReporting(nameForReporting);
        return directive;
    }
    return operativeDirective(m_scriptSrc.get(), nameForReporting);
}

ContentSecurityPolicySourceListDirective* ContentSecurityPolicyDirectiveList::operativeDirectiveStyle(ContentSecurityPolicySourceListDirective* directive, const String& nameForReporting) const
{
    if (directive) {
        directive->setNameForReporting(nameForReporting);
        return directive;
    }
    return operativeDirective(m_styleSrc.get(), nameForReporting);
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeEval() const
{
    auto* operativeDirective = this->operativeDirective(m_scriptSrc.get(), ContentSecurityPolicyDirectiveNames::scriptSrc);
    if (checkEval(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineScriptElement(const String& nonce, const Vector<ContentSecurityPolicyHash>& hashes) const
{
    auto* operativeDirective = this->operativeDirectiveScript(m_scriptSrcElem.get(), ContentSecurityPolicyDirectiveNames::scriptSrcElem);
    if (checkHashes(operativeDirective, hashes)
        || checkNonce(operativeDirective, nonce)
        || checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForInlineJavascriptURL(const Vector<ContentSecurityPolicyHash>& hashes) const
{
    auto* operativeDirective = this->operativeDirectiveScript(m_scriptSrcElem.get(), ContentSecurityPolicyDirectiveNames::scriptSrcElem);
    if (checkUnsafeHashes(operativeDirective, hashes)
        || checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForInlineEventHandlers(const Vector<ContentSecurityPolicyHash>& hashes) const
{
    auto* operativeDirective = this->operativeDirectiveScript(m_scriptSrcAttr.get(), ContentSecurityPolicyDirectiveNames::scriptSrcAttr);
    if (checkUnsafeHashes(operativeDirective, hashes)
        || checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForNonParserInsertedScripts(const String& nonce, const Vector<ContentSecurityPolicyHash>& hashes, const URL& url, ParserInserted parserInserted) const
{
    auto* operativeDirective = this->operativeDirectiveScript(m_scriptSrcElem.get(), ContentSecurityPolicyDirectiveNames::scriptSrcElem);
    if (checkHashes(operativeDirective, hashes)
        || checkNonParserInsertedScripts(operativeDirective, parserInserted)
        || checkNonce(operativeDirective, nonce)
        || (checkSource(operativeDirective, url) && !strictDynamicIncluded())
        || (url.isEmpty() && checkInline(operativeDirective)))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyleElement(const String& nonce, const Vector<ContentSecurityPolicyHash>& hashes) const
{
    auto* operativeDirective = this->operativeDirectiveStyle(m_styleSrcElem.get(), ContentSecurityPolicyDirectiveNames::styleSrcElem);
    if (checkHashes(operativeDirective, hashes)
        || checkNonce(operativeDirective, nonce)
        || checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForUnsafeInlineStyleAttribute(const String& nonce, const Vector<ContentSecurityPolicyHash>& hashes) const
{
    auto* operativeDirective = this->operativeDirectiveStyle(m_styleSrcAttr.get(), ContentSecurityPolicyDirectiveNames::styleSrcAttr);
    if (checkUnsafeHashes(operativeDirective, hashes)
        || checkNonce(operativeDirective, nonce)
        || checkInline(operativeDirective))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForScriptNonce(const String& nonce) const
{
    auto* operativeDirective = this->operativeDirectiveScript(m_scriptSrcElem.get(), ContentSecurityPolicyDirectiveNames::scriptSrc);
    if (checkNonce(operativeDirective, nonce))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForStyleNonce(const String& nonce) const
{
    auto* operativeDirective = this->operativeDirectiveStyle(m_styleSrcElem.get(), ContentSecurityPolicyDirectiveNames::styleSrc);
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
    auto* operativeDirective = this->operativeDirective(m_childSrc.get(), ContentSecurityPolicyDirectiveNames::childSrc);
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForConnectSource(const URL& url, bool didReceiveRedirectResponse) const
{
    auto* operativeDirective = this->operativeDirective(m_connectSrc.get(), ContentSecurityPolicyDirectiveNames::connectSrc);
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForFont(const URL& url, bool didReceiveRedirectResponse) const
{
    auto* operativeDirective = this->operativeDirective(m_fontSrc.get(), ContentSecurityPolicyDirectiveNames::fontSrc);
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
    auto* operativeDirective = this->operativeDirective(m_frameSrc ? m_frameSrc.get() : m_childSrc.get(), ContentSecurityPolicyDirectiveNames::frameSrc);
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
    auto* operativeDirective = this->operativeDirective(m_imgSrc.get(), ContentSecurityPolicyDirectiveNames::imgSrc);
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForPrefetch(const URL& url, bool didReceiveRedirectResponse) const
{
    auto* operativeDirective = this->operativeDirective(m_prefetchSrc.get(), ContentSecurityPolicyDirectiveNames::prefetchSrc);
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

#if ENABLE(APPLICATION_MANIFEST)
const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForManifest(const URL& url, bool didReceiveRedirectResponse) const
{
    auto* operativeDirective = this->operativeDirective(m_manifestSrc.get(), ContentSecurityPolicyDirectiveNames::manifestSrc);
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}
#endif // ENABLE(APPLICATION_MANIFEST)

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForMedia(const URL& url, bool didReceiveRedirectResponse) const
{
    auto* operativeDirective = this->operativeDirective(m_mediaSrc.get(), ContentSecurityPolicyDirectiveNames::mediaSrc);
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForObjectSource(const URL& url, bool didReceiveRedirectResponse, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone shouldAllowEmptyURLIfSourceListEmpty) const
{
    if (url.protocolIsAbout())
        return nullptr;
    auto* operativeDirective = this->operativeDirective(m_objectSrc.get(), ContentSecurityPolicyDirectiveNames::objectSrc);
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

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForWorker(const URL& url, bool didReceiveRedirectResponse)
{
    auto* operativeDirective = this->operativeDirectiveForWorkerSrc(m_workerSrc.get(), ContentSecurityPolicyDirectiveNames::workerSrc);
    // Per https://github.com/w3c/webappsec-csp/issues/200 we should allow workers when the directive contains 'strict-dynamic'
    if (checkSource(operativeDirective, url, didReceiveRedirectResponse)
        || checkNonParserInsertedScripts(operativeDirective, ParserInserted::No))
        return nullptr;
    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForScript(const URL& url, bool didReceiveRedirectResponse, const Vector<ResourceCryptographicDigest>& subResourceIntegrityDigests, const String& nonce) const
{
    auto* operativeDirective = this->operativeDirectiveScript(m_scriptSrcElem.get(), ContentSecurityPolicyDirectiveNames::scriptSrcElem);

    if (!operativeDirective
        || operativeDirective->containsAllHashes(subResourceIntegrityDigests)
        || checkNonce(operativeDirective, nonce)
        || (checkSource(operativeDirective, url, didReceiveRedirectResponse) && !strictDynamicIncluded()))
        return nullptr;

    return operativeDirective;
}

const ContentSecurityPolicyDirective* ContentSecurityPolicyDirectiveList::violatedDirectiveForStyle(const URL& url, bool didReceiveRedirectResponse, const String& nonce) const
{
    auto* operativeDirective = this->operativeDirective(m_styleSrc.get(), ContentSecurityPolicyDirectiveNames::styleSrcElem);
    if (checkNonce(operativeDirective, nonce)
        || checkSource(operativeDirective, url, didReceiveRedirectResponse))
        return nullptr;
    return operativeDirective;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void ContentSecurityPolicyDirectiveList::parse(const String& policy, ContentSecurityPolicy::PolicyFrom policyFrom)
{
    // A meta tag delievered CSP could contain invalid HTTP header values depending on how it was formatted in the document.
    // We want to store the CSP as a valid HTTP header for e.g. blob URL inheritance.
    if (policyFrom == ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta) {
        m_header = stripLeadingAndTrailingHTTPSpaces(policy).removeCharacters([](auto c) {
            return c == 0x00 || c == '\r' || c == '\n';
        });
    } else
        m_header = policy;

    if (policy.isEmpty())
        return;

    readCharactersForParsing(policy, [&](auto buffer) {
        while (buffer.hasCharactersRemaining()) {
            auto directiveBegin = buffer.position();
            skipUntil(buffer, ';');

            if (auto directive = parseDirective(StringParsingBuffer { directiveBegin, buffer.position() })) {
                ASSERT(!directive->name.isEmpty());
                if (policyFrom == ContentSecurityPolicy::PolicyFrom::Inherited) {
                    if (equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::upgradeInsecureRequests))
                        continue;
                } else if (policyFrom == ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta) {
                    if (equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::sandbox)
                        || equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::reportURI)
                        || equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::frameAncestors)) {
                        m_policy.reportInvalidDirectiveInHTTPEquivMeta(directive->name);
                        continue;
                    }
                } else if (policyFrom == ContentSecurityPolicy::PolicyFrom::InheritedForPluginDocument) {
                    if (!equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::pluginTypes)
                        && !equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::reportURI)
                        && !equalIgnoringASCIICase(directive->name, ContentSecurityPolicyDirectiveNames::reportTo))
                        continue;
                }
                addDirective(WTFMove(*directive));
            }

            ASSERT(buffer.atEnd() || *buffer == ';');
            skipExactly(buffer, ';');
        }
    });
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
template<typename CharacterType> auto ContentSecurityPolicyDirectiveList::parseDirective(StringParsingBuffer<CharacterType> buffer) -> std::optional<ParsedDirective>
{
    skipWhile<isASCIISpace>(buffer);

    // Empty directive (e.g. ";;;"). Exit early.
    if (buffer.atEnd())
        return std::nullopt;

    auto nameBegin = buffer.position();
    skipWhile<isDirectiveNameCharacter>(buffer);

    // The directive-name must be non-empty.
    if (nameBegin == buffer.position()) {
        skipWhile<isNotASCIISpace>(buffer);
        m_policy.reportUnsupportedDirective(String(nameBegin, buffer.position() - nameBegin));
        return std::nullopt;
    }

    auto name = String(nameBegin, buffer.position() - nameBegin);

    if (buffer.atEnd())
        return ParsedDirective { WTFMove(name), { } };

    if (!skipExactly<isASCIISpace>(buffer)) {
        skipWhile<isNotASCIISpace>(buffer);
        m_policy.reportUnsupportedDirective(String(nameBegin, buffer.position() - nameBegin));
        return std::nullopt;
    }

    skipWhile<isASCIISpace>(buffer);

    auto valueBegin = buffer.position();
    skipWhile<isDirectiveValueCharacter>(buffer);

    if (!buffer.atEnd()) {
        m_policy.reportInvalidDirectiveValueCharacter(name, String(valueBegin, buffer.end() - valueBegin));
        return std::nullopt;
    }

    // The directive-value may be empty.
    if (valueBegin == buffer.position())
        return ParsedDirective { WTFMove(name), { } };

    auto value = String(valueBegin, buffer.position() - valueBegin);
    return ParsedDirective { WTFMove(name), WTFMove(value) };
}

void ContentSecurityPolicyDirectiveList::parseReportURI(ParsedDirective&& directive)
{
    if (!m_reportURIs.isEmpty()) {
        m_policy.reportDuplicateDirective(directive.name);
        return;
    }

    readCharactersForParsing(directive.value, [&](auto buffer) {
        auto begin = buffer.position();
        while (buffer.hasCharactersRemaining()) {
            skipWhile<isASCIISpace>(buffer);

            auto urlBegin = buffer.position();
            skipWhile<isNotASCIISpace>(buffer);

            if (urlBegin < buffer.position())
                m_reportURIs.append(directive.value.substring(urlBegin - begin, buffer.position() - urlBegin));
        }
    });
}

void ContentSecurityPolicyDirectiveList::parseReportTo(ParsedDirective&& directive)
{
    if (!m_reportToTokens.isEmpty()) {
        m_policy.reportDuplicateDirective(directive.name);
        return;
    }

    readCharactersForParsing(directive.value, [&](auto buffer) {
        auto begin = buffer.position();
        while (buffer.hasCharactersRemaining()) {
            skipWhile<isASCIISpace>(buffer);

            auto urlBegin = buffer.position();
            skipWhile<isNotASCIISpace>(buffer);

            if (urlBegin < buffer.position())
                m_reportToTokens.append(directive.value.substring(urlBegin - begin, buffer.position() - urlBegin));
        }
    });
}

template<class CSPDirectiveType>
void ContentSecurityPolicyDirectiveList::setCSPDirective(ParsedDirective&& directive, std::unique_ptr<CSPDirectiveType>& existingDirective)
{
    if (existingDirective) {
        m_policy.reportDuplicateDirective(directive.name);
        return;
    }
    existingDirective = makeUnique<CSPDirectiveType>(*this, WTFMove(directive.name), WTFMove(directive.value));
}

void ContentSecurityPolicyDirectiveList::applySandboxPolicy(ParsedDirective&& directive)
{
    if (m_reportOnly) {
        m_policy.reportInvalidDirectiveInReportOnlyMode(directive.name);
        return;
    }
    if (m_haveSandboxPolicy) {
        m_policy.reportDuplicateDirective(directive.name);
        return;
    }
    m_haveSandboxPolicy = true;
    String invalidTokens;
    m_policy.enforceSandboxFlags(SecurityContext::parseSandboxPolicy(WTFMove(directive.value), invalidTokens));
    if (!invalidTokens.isNull())
        m_policy.reportInvalidSandboxFlags(invalidTokens);
}

void ContentSecurityPolicyDirectiveList::setUpgradeInsecureRequests(ParsedDirective&& directive)
{
    if (m_reportOnly) {
        m_policy.reportInvalidDirectiveInReportOnlyMode(WTFMove(directive.name));
        return;
    }
    if (m_upgradeInsecureRequests) {
        m_policy.reportDuplicateDirective(WTFMove(directive.name));
        return;
    }
    m_upgradeInsecureRequests = true;
    m_policy.setUpgradeInsecureRequests(true);
}

void ContentSecurityPolicyDirectiveList::setBlockAllMixedContentEnabled(ParsedDirective&& directive)
{
    if (m_hasBlockAllMixedContentDirective) {
        m_policy.reportDuplicateDirective(WTFMove(directive.name));
        return;
    }
    m_hasBlockAllMixedContentDirective = true;
}

void ContentSecurityPolicyDirectiveList::addDirective(ParsedDirective&& directive)
{
    ASSERT(!directive.name.isEmpty());

    if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::defaultSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_defaultSrc);
        m_policy.addHashAlgorithmsForInlineScripts(m_defaultSrc->hashAlgorithmsUsed());
        m_policy.addHashAlgorithmsForInlineStylesheets(m_defaultSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::scriptSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_scriptSrc);
        m_policy.addHashAlgorithmsForInlineScripts(m_scriptSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::scriptSrcElem)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_scriptSrcElem);
        m_policy.addHashAlgorithmsForInlineScripts(m_scriptSrcElem->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::scriptSrcAttr)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_scriptSrcAttr);
        m_policy.addHashAlgorithmsForInlineScripts(m_scriptSrcAttr->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::styleSrc)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_styleSrc);
        m_policy.addHashAlgorithmsForInlineStylesheets(m_styleSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::styleSrcElem)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_styleSrcElem);
        m_policy.addHashAlgorithmsForInlineStylesheets(m_styleSrcElem->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::styleSrcAttr)) {
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_styleSrcAttr);
        m_policy.addHashAlgorithmsForInlineStylesheets(m_styleSrcAttr->hashAlgorithmsUsed());
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::objectSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_objectSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::workerSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_workerSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::frameSrc)) {
        // FIXME: Log to console "The frame-src directive is deprecated. Use the child-src directive instead."
        // See <https://bugs.webkit.org/show_bug.cgi?id=155773>.
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_frameSrc);
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::imgSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_imgSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::fontSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_fontSrc);
#if ENABLE(APPLICATION_MANIFEST)
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::manifestSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_manifestSrc);
#endif
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::mediaSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_mediaSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::connectSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_connectSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::childSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_childSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::formAction))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_formAction);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::baseURI))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_baseURI);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::frameAncestors)) {
        if (m_reportOnly) {
            m_policy.reportInvalidDirectiveInReportOnlyMode(directive.name);
            return;
        }
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_frameAncestors);
    } else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::pluginTypes))
        setCSPDirective<ContentSecurityPolicyMediaListDirective>(WTFMove(directive), m_pluginTypes);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::prefetchSrc))
        setCSPDirective<ContentSecurityPolicySourceListDirective>(WTFMove(directive), m_prefetchSrc);
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::sandbox))
        applySandboxPolicy(WTFMove(directive));
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::reportTo))
        parseReportTo(WTFMove(directive));
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::reportURI))
        parseReportURI(WTFMove(directive));
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::upgradeInsecureRequests))
        setUpgradeInsecureRequests(WTFMove(directive));
    else if (equalIgnoringASCIICase(directive.name, ContentSecurityPolicyDirectiveNames::blockAllMixedContent))
        setBlockAllMixedContentEnabled(WTFMove(directive));
    else
        m_policy.reportUnsupportedDirective(WTFMove(directive.name));
}

bool ContentSecurityPolicyDirectiveList::strictDynamicIncluded() const
{
    ContentSecurityPolicySourceListDirective* directive = this->operativeDirectiveScript(m_scriptSrcElem.get(), ContentSecurityPolicyDirectiveNames::scriptSrc);
    return directive && directive->allowNonParserInsertedScripts();
}

bool ContentSecurityPolicyDirectiveList::shouldReportSample(const String& violatedDirective) const
{
    ContentSecurityPolicySourceListDirective* directive = nullptr;
    if (violatedDirective.startsWith(StringView { ContentSecurityPolicyDirectiveNames::styleSrc }))
        directive = m_styleSrc.get();
    else if (violatedDirective.startsWith(StringView { ContentSecurityPolicyDirectiveNames::scriptSrc }))
        directive = m_scriptSrc.get();

    return directive && directive->shouldReportSample();
}

} // namespace WebCore
