/*
 * Copyright (C) 2011 Adam Barth. All Rights Reserved.
 * Copyright (C) 2011 Daniel Bates (dbates@intudata.com).
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
#include "XSSAuditor.h"

#include "DecodeEscapeSequences.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocumentParser.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "HTMLParserIdioms.h"
#include "SVGNames.h"
#include "Settings.h"
#include "TextResourceDecoder.h"
#include "XLinkNames.h"
#include <wtf/ASCIICType.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

using namespace HTMLNames;

static bool isNonCanonicalCharacter(UChar c)
{
    // We remove all non-ASCII characters, including non-printable ASCII characters.
    //
    // Note, we don't remove backslashes like PHP stripslashes(), which among other things converts "\\0" to the \0 character.
    // Instead, we remove backslashes and zeros (since the string "\\0" =(remove backslashes)=> "0"). However, this has the
    // adverse effect that we remove any legitimate zeros from a string.
    // We also remove forward-slash, because it is common for some servers to collapse successive path components, eg,
    // a//b becomes a/b.
    //
    // For instance: new String("http://localhost:8000") => new String("http:localhost:8").
    return (c == '\\' || c == '0' || c == '\0' || c == '/' || c >= 127);
}

static bool isRequiredForInjection(UChar c)
{
    return (c == '\'' || c == '"' || c == '<' || c == '>');
}

static bool isTerminatingCharacter(UChar c)
{
    return (c == '&' || c == '/' || c == '"' || c == '\'' || c == '<' || c == '>' || c == ',');
}

static bool isHTMLQuote(UChar c)
{
    return (c == '"' || c == '\'');
}

static bool isJSNewline(UChar c)
{
    // Per ecma-262 section 7.3 Line Terminators.
    return (c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029);
}

static bool startsHTMLCommentAt(const String& string, size_t start)
{
    return (start + 3 < string.length() && string[start] == '<' && string[start + 1] == '!' && string[start + 2] == '-' && string[start + 3] == '-');
}

static bool startsSingleLineCommentAt(const String& string, size_t start)
{
    return (start + 1 < string.length() && string[start] == '/' && string[start + 1] == '/');
}

static bool startsMultiLineCommentAt(const String& string, size_t start)
{
    return (start + 1 < string.length() && string[start] == '/' && string[start + 1] == '*');
}

static bool startsOpeningScriptTagAt(const String& string, size_t start)
{
    return start + 6 < string.length() && string[start] == '<'
        && WTF::toASCIILowerUnchecked(string[start + 1]) == 's'
        && WTF::toASCIILowerUnchecked(string[start + 2]) == 'c'
        && WTF::toASCIILowerUnchecked(string[start + 3]) == 'r'
        && WTF::toASCIILowerUnchecked(string[start + 4]) == 'i'
        && WTF::toASCIILowerUnchecked(string[start + 5]) == 'p'
        && WTF::toASCIILowerUnchecked(string[start + 6]) == 't';
}

// If other files need this, we should move this to HTMLParserIdioms.h
template<size_t inlineCapacity>
bool threadSafeMatch(const Vector<UChar, inlineCapacity>& vector, const QualifiedName& qname)
{
    return equalIgnoringNullity(vector, qname.localName().impl());
}

static bool hasName(const HTMLToken& token, const QualifiedName& name)
{
    return threadSafeMatch(token.name(), name);
}

static bool findAttributeWithName(const HTMLToken& token, const QualifiedName& name, size_t& indexOfMatchingAttribute)
{
    // Notice that we're careful not to ref the StringImpl here because we might be on a background thread.
    const String& attrName = name.namespaceURI() == XLinkNames::xlinkNamespaceURI ? "xlink:" + name.localName().string() : name.localName().string();

    for (size_t i = 0; i < token.attributes().size(); ++i) {
        if (equalIgnoringNullity(token.attributes().at(i).name, attrName)) {
            indexOfMatchingAttribute = i;
            return true;
        }
    }
    return false;
}

static bool isNameOfInlineEventHandler(const Vector<UChar, 32>& name)
{
    const size_t lengthOfShortestInlineEventHandlerName = 5; // To wit: oncut.
    if (name.size() < lengthOfShortestInlineEventHandlerName)
        return false;
    return name[0] == 'o' && name[1] == 'n';
}

static bool isDangerousHTTPEquiv(const String& value)
{
    String equiv = value.stripWhiteSpace();
    return equalLettersIgnoringASCIICase(equiv, "refresh");
}

static inline String decode16BitUnicodeEscapeSequences(const String& string)
{
    // Note, the encoding is ignored since each %u-escape sequence represents a UTF-16 code unit.
    return decodeEscapeSequences<Unicode16BitEscapeSequence>(string, UTF8Encoding());
}

static inline String decodeStandardURLEscapeSequences(const String& string, const TextEncoding& encoding)
{
    // We use decodeEscapeSequences() instead of decodeURLEscapeSequences() (declared in URL.h) to
    // avoid platform-specific URL decoding differences (e.g. URLGoogle).
    return decodeEscapeSequences<URLEscapeSequence>(string, encoding);
}

static String fullyDecodeString(const String& string, const TextEncoding& encoding)
{
    size_t oldWorkingStringLength;
    String workingString = string;
    do {
        oldWorkingStringLength = workingString.length();
        workingString = decode16BitUnicodeEscapeSequences(decodeStandardURLEscapeSequences(workingString, encoding));
    } while (workingString.length() < oldWorkingStringLength);
    workingString.replace('+', ' ');
    return workingString;
}

static void truncateForSrcLikeAttribute(String& decodedSnippet)
{
    // In HTTP URLs, characters following the first ?, #, or third slash may come from
    // the page itself and can be merely ignored by an attacker's server when a remote
    // script or script-like resource is requested. In data URLs, the payload starts at
    // the first comma, and the first /*, //, or <!-- may introduce a comment. Also
    // data URLs may use the same string literal tricks as with script content itself.
    // In either case, content following this may come from the page and may be ignored
    // when the script is executed. Also, any of these characters may now be represented
    // by the (enlarged) set of HTML5 entities.
    // For simplicity, we don't differentiate based on URL scheme, and stop at the first
    // & (since it might be part of an entity for any of the subsequent punctuation)
    // the first # or ?, the third slash, or the first slash, <, ', or " once a comma
    // is seen.
    int slashCount = 0;
    bool commaSeen = false;
    for (size_t currentLength = 0; currentLength < decodedSnippet.length(); ++currentLength) {
        UChar currentChar = decodedSnippet[currentLength];
        if (currentChar == '&'
            || currentChar == '?'
            || currentChar == '#'
            || ((currentChar == '/' || currentChar == '\\') && (commaSeen || ++slashCount > 2))
            || (currentChar == '<' && commaSeen)
            || (currentChar == '\'' && commaSeen)
            || (currentChar == '"' && commaSeen)) {
            decodedSnippet.truncate(currentLength);
            return;
        }
        if (currentChar == ',')
            commaSeen = true;
    }
}

static void truncateForScriptLikeAttribute(String& decodedSnippet)
{
    // Beware of trailing characters which came from the page itself, not the
    // injected vector. Excluding the terminating character covers common cases
    // where the page immediately ends the attribute, but doesn't cover more
    // complex cases where there is other page data following the injection.
    // Generally, these won't parse as JavaScript, so the injected vector
    // typically excludes them from consideration via a single-line comment or
    // by enclosing them in a string literal terminated later by the page's own
    // closing punctuation. Since the snippet has not been parsed, the vector
    // may also try to introduce these via entities. As a result, we'd like to
    // stop before the first "//", the first <!--, the first entity, or the first
    // quote not immediately following the first equals sign (taking whitespace
    // into consideration). To keep things simpler, we don't try to distinguish
    // between entity-introducing ampersands vs. other uses, nor do we bother to
    // check for a second slash for a comment, nor do we bother to check for
    // !-- following a less-than sign. We stop instead on any ampersand
    // slash, or less-than sign.
    size_t position = 0;
    if ((position = decodedSnippet.find('=')) != notFound
        && (position = decodedSnippet.find(isNotHTMLSpace, position + 1)) != notFound
        && (position = decodedSnippet.find(isTerminatingCharacter, isHTMLQuote(decodedSnippet[position]) ? position + 1 : position)) != notFound) {
        decodedSnippet.truncate(position);
    }
}

static bool isSemicolonSeparatedAttribute(const HTMLToken::Attribute& attribute)
{
    return threadSafeMatch(attribute.name, SVGNames::valuesAttr);
}

static bool semicolonSeparatedValueContainsJavaScriptURL(StringView semicolonSeparatedValue)
{
    for (auto value : semicolonSeparatedValue.split(';')) {
        if (WTF::protocolIsJavaScript(value))
            return true;
    }
    return false;
}

XSSAuditor::XSSAuditor()
    : m_isEnabled(false)
    , m_xssProtection(XSSProtectionDisposition::Enabled)
    , m_didSendValidXSSProtectionHeader(false)
    , m_state(Uninitialized)
    , m_scriptTagNestingLevel(0)
    , m_encoding(UTF8Encoding())
{
    // Although tempting to call init() at this point, the various objects
    // we want to reference might not all have been constructed yet.
}

void XSSAuditor::initForFragment()
{
    ASSERT(isMainThread());
    ASSERT(m_state == Uninitialized);
    m_state = Initialized;
    // When parsing a fragment, we don't enable the XSS auditor because it's
    // too much overhead.
    ASSERT(!m_isEnabled);
}

void XSSAuditor::init(Document* document, XSSAuditorDelegate* auditorDelegate)
{
    ASSERT(isMainThread());
    if (m_state == Initialized)
        return;
    ASSERT(m_state == Uninitialized);
    m_state = Initialized;

    if (RefPtr<Frame> frame = document->frame())
        m_isEnabled = frame->settings().xssAuditorEnabled();

    if (!m_isEnabled)
        return;

    m_documentURL = document->url().isolatedCopy();

    // In theory, the Document could have detached from the Frame after the
    // XSSAuditor was constructed.
    if (!document->frame()) {
        m_isEnabled = false;
        return;
    }

    if (m_documentURL.isEmpty()) {
        // The URL can be empty when opening a new browser window or calling window.open("").
        m_isEnabled = false;
        return;
    }

    if (m_documentURL.protocolIsData()) {
        m_isEnabled = false;
        return;
    }

    if (document->decoder())
        m_encoding = document->decoder()->encoding();

    m_decodedURL = canonicalize(m_documentURL.string(), TruncationStyle::None);
    if (m_decodedURL.find(isRequiredForInjection) == notFound)
        m_decodedURL = String();

    if (RefPtr<DocumentLoader> documentLoader = document->frame()->loader().documentLoader()) {
        String headerValue = documentLoader->response().httpHeaderField(HTTPHeaderName::XXSSProtection);
        String errorDetails;
        unsigned errorPosition = 0;
        String parsedReportURL;
        URL reportURL;
        m_xssProtection = parseXSSProtectionHeader(headerValue, errorDetails, errorPosition, parsedReportURL);
        m_didSendValidXSSProtectionHeader = !headerValue.isNull() && m_xssProtection != XSSProtectionDisposition::Invalid;

        if ((m_xssProtection == XSSProtectionDisposition::Enabled || m_xssProtection == XSSProtectionDisposition::BlockEnabled) && !parsedReportURL.isEmpty()) {
            reportURL = document->completeURL(parsedReportURL);
            if (MixedContentChecker::isMixedContent(document->securityOrigin(), reportURL)) {
                errorDetails = "insecure reporting URL for secure page";
                m_xssProtection = XSSProtectionDisposition::Invalid;
                reportURL = URL();
                m_didSendValidXSSProtectionHeader = false;
            }
        }
        if (m_xssProtection == XSSProtectionDisposition::Invalid) {
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Error parsing header X-XSS-Protection: ", headerValue, ": ", errorDetails, " at character position ", errorPosition, ". The default protections will be applied."));
            m_xssProtection = XSSProtectionDisposition::Enabled;
        }

        if (auditorDelegate)
            auditorDelegate->setReportURL(reportURL.isolatedCopy());
        RefPtr<FormData> httpBody = documentLoader->originalRequest().httpBody();
        if (httpBody && !httpBody->isEmpty()) {
            String httpBodyAsString = httpBody->flattenToString();
            if (!httpBodyAsString.isEmpty()) {
                m_decodedHTTPBody = canonicalize(httpBodyAsString, TruncationStyle::None);
                if (m_decodedHTTPBody.find(isRequiredForInjection) == notFound)
                    m_decodedHTTPBody = String();
            }
        }
    }

    if (m_decodedURL.isEmpty() && m_decodedHTTPBody.isEmpty()) {
        m_isEnabled = false;
        return;
    }
}

std::unique_ptr<XSSInfo> XSSAuditor::filterToken(const FilterTokenRequest& request)
{
    ASSERT(m_state == Initialized);
    if (!m_isEnabled || m_xssProtection == XSSProtectionDisposition::Disabled)
        return nullptr;

    bool didBlockScript = false;
    if (request.token.type() == HTMLToken::StartTag)
        didBlockScript = filterStartToken(request);
    else if (m_scriptTagNestingLevel) {
        if (request.token.type() == HTMLToken::Character)
            didBlockScript = filterCharacterToken(request);
        else if (request.token.type() == HTMLToken::EndTag)
            filterEndToken(request);
    }

    if (!didBlockScript)
        return nullptr;

    bool didBlockEntirePage = m_xssProtection == XSSProtectionDisposition::BlockEnabled;
    return std::make_unique<XSSInfo>(m_documentURL, didBlockEntirePage, m_didSendValidXSSProtectionHeader);
}

bool XSSAuditor::filterStartToken(const FilterTokenRequest& request)
{
    bool didBlockScript = eraseDangerousAttributesIfInjected(request);

    if (hasName(request.token, scriptTag)) {
        didBlockScript |= filterScriptToken(request);
        ASSERT(request.shouldAllowCDATA || !m_scriptTagNestingLevel);
        m_scriptTagNestingLevel++;
    } else if (hasName(request.token, objectTag))
        didBlockScript |= filterObjectToken(request);
    else if (hasName(request.token, paramTag))
        didBlockScript |= filterParamToken(request);
    else if (hasName(request.token, embedTag))
        didBlockScript |= filterEmbedToken(request);
    else if (hasName(request.token, appletTag))
        didBlockScript |= filterAppletToken(request);
    else if (hasName(request.token, iframeTag) || hasName(request.token, frameTag))
        didBlockScript |= filterFrameToken(request);
    else if (hasName(request.token, metaTag))
        didBlockScript |= filterMetaToken(request);
    else if (hasName(request.token, baseTag))
        didBlockScript |= filterBaseToken(request);
    else if (hasName(request.token, formTag))
        didBlockScript |= filterFormToken(request);
    else if (hasName(request.token, inputTag))
        didBlockScript |= filterInputToken(request);
    else if (hasName(request.token, buttonTag))
        didBlockScript |= filterButtonToken(request);

    return didBlockScript;
}

void XSSAuditor::filterEndToken(const FilterTokenRequest& request)
{
    ASSERT(m_scriptTagNestingLevel);
    if (hasName(request.token, scriptTag)) {
        m_scriptTagNestingLevel--;
        ASSERT(request.shouldAllowCDATA || !m_scriptTagNestingLevel);
    }
}

bool XSSAuditor::filterCharacterToken(const FilterTokenRequest& request)
{
    ASSERT(m_scriptTagNestingLevel);
    if (m_wasScriptTagFoundInRequest && isContainedInRequest(canonicalizedSnippetForJavaScript(request))) {
        request.token.clear();
        LChar space = ' ';
        request.token.appendToCharacter(space); // Technically, character tokens can't be empty.
        return true;
    }
    return false;
}

bool XSSAuditor::filterScriptToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, scriptTag));

    m_wasScriptTagFoundInRequest = isContainedInRequest(canonicalizedSnippetForTagName(request));

    bool didBlockScript = false;
    if (m_wasScriptTagFoundInRequest) {
        didBlockScript |= eraseAttributeIfInjected(request, srcAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(request, SVGNames::hrefAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(request, XLinkNames::hrefAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
    }

    return didBlockScript;
}

bool XSSAuditor::filterObjectToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, objectTag));

    bool didBlockScript = false;
    if (isContainedInRequest(canonicalizedSnippetForTagName(request))) {
        didBlockScript |= eraseAttributeIfInjected(request, dataAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(request, typeAttr);
        didBlockScript |= eraseAttributeIfInjected(request, classidAttr);
    }
    return didBlockScript;
}

bool XSSAuditor::filterParamToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, paramTag));

    size_t indexOfNameAttribute;
    if (!findAttributeWithName(request.token, nameAttr, indexOfNameAttribute))
        return false;

    const HTMLToken::Attribute& nameAttribute = request.token.attributes().at(indexOfNameAttribute);
    if (!HTMLParamElement::isURLParameter(String(nameAttribute.value)))
        return false;

    return eraseAttributeIfInjected(request, valueAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
}

bool XSSAuditor::filterEmbedToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, embedTag));

    bool didBlockScript = false;
    if (isContainedInRequest(canonicalizedSnippetForTagName(request))) {
        didBlockScript |= eraseAttributeIfInjected(request, codeAttr, String(), TruncationStyle::SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(request, srcAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(request, typeAttr);
    }
    return didBlockScript;
}

bool XSSAuditor::filterAppletToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, appletTag));

    bool didBlockScript = false;
    if (isContainedInRequest(canonicalizedSnippetForTagName(request))) {
        didBlockScript |= eraseAttributeIfInjected(request, codeAttr, String(), TruncationStyle::SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(request, objectAttr);
    }
    return didBlockScript;
}

bool XSSAuditor::filterFrameToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, iframeTag) || hasName(request.token, frameTag));

    bool didBlockScript = eraseAttributeIfInjected(request, srcdocAttr, String(), TruncationStyle::ScriptLikeAttribute);
    if (isContainedInRequest(canonicalizedSnippetForTagName(request)))
        didBlockScript |= eraseAttributeIfInjected(request, srcAttr, String(), TruncationStyle::SrcLikeAttribute);

    return didBlockScript;
}

bool XSSAuditor::filterMetaToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, metaTag));

    return eraseAttributeIfInjected(request, http_equivAttr);
}

bool XSSAuditor::filterBaseToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, baseTag));

    return eraseAttributeIfInjected(request, hrefAttr);
}

bool XSSAuditor::filterFormToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, formTag));

    return eraseAttributeIfInjected(request, actionAttr, WTF::blankURL().string());
}

bool XSSAuditor::filterInputToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, inputTag));

    return eraseAttributeIfInjected(request, formactionAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
}

bool XSSAuditor::filterButtonToken(const FilterTokenRequest& request)
{
    ASSERT(request.token.type() == HTMLToken::StartTag);
    ASSERT(hasName(request.token, buttonTag));

    return eraseAttributeIfInjected(request, formactionAttr, WTF::blankURL().string(), TruncationStyle::SrcLikeAttribute);
}

bool XSSAuditor::eraseDangerousAttributesIfInjected(const FilterTokenRequest& request)
{
    static NeverDestroyed<String> safeJavaScriptURL(MAKE_STATIC_STRING_IMPL("javascript:void(0)"));

    bool didBlockScript = false;
    for (size_t i = 0; i < request.token.attributes().size(); ++i) {
        const HTMLToken::Attribute& attribute = request.token.attributes().at(i);
        bool isInlineEventHandler = isNameOfInlineEventHandler(attribute.name);
        // FIXME: It would be better if we didn't create a new String for every attribute in the document.
        String strippedValue = stripLeadingAndTrailingHTMLSpaces(String(attribute.value));
        bool valueContainsJavaScriptURL = (!isInlineEventHandler && WTF::protocolIsJavaScript(strippedValue)) || (isSemicolonSeparatedAttribute(attribute) && semicolonSeparatedValueContainsJavaScriptURL(strippedValue));
        if (!isInlineEventHandler && !valueContainsJavaScriptURL)
            continue;
        if (!isContainedInRequest(canonicalize(snippetFromAttribute(request, attribute), TruncationStyle::ScriptLikeAttribute)))
            continue;
        request.token.eraseValueOfAttribute(i);
        if (valueContainsJavaScriptURL)
            request.token.appendToAttributeValue(i, safeJavaScriptURL.get());
        didBlockScript = true;
    }
    return didBlockScript;
}

bool XSSAuditor::eraseAttributeIfInjected(const FilterTokenRequest& request, const QualifiedName& attributeName, const String& replacementValue, TruncationStyle truncationStyle)
{
    size_t indexOfAttribute = 0;
    if (!findAttributeWithName(request.token, attributeName, indexOfAttribute))
        return false;

    const HTMLToken::Attribute& attribute = request.token.attributes().at(indexOfAttribute);
    if (!isContainedInRequest(canonicalize(snippetFromAttribute(request, attribute), truncationStyle)))
        return false;

    if (threadSafeMatch(attributeName, srcAttr)) {
        if (isLikelySafeResource(String(attribute.value)))
            return false;
    } else if (threadSafeMatch(attributeName, http_equivAttr)) {
        if (!isDangerousHTTPEquiv(String(attribute.value)))
            return false;
    }

    request.token.eraseValueOfAttribute(indexOfAttribute);
    if (!replacementValue.isEmpty())
        request.token.appendToAttributeValue(indexOfAttribute, replacementValue);
    return true;
}

String XSSAuditor::canonicalizedSnippetForTagName(const FilterTokenRequest& request)
{
    // Grab a fixed number of characters equal to the length of the token's name plus one (to account for the "<").
    return canonicalize(request.sourceTracker.source(request.token).substring(0, request.token.name().size() + 1), TruncationStyle::None);
}

String XSSAuditor::snippetFromAttribute(const FilterTokenRequest& request, const HTMLToken::Attribute& attribute)
{
    // The range doesn't include the character which terminates the value. So,
    // for an input of |name="value"|, the snippet is |name="value|. For an
    // unquoted input of |name=value |, the snippet is |name=value|.
    // FIXME: We should grab one character before the name also.
    return request.sourceTracker.source(request.token, attribute.startOffset, attribute.endOffset);
}

String XSSAuditor::canonicalize(const String& snippet, TruncationStyle truncationStyle)
{
    String decodedSnippet = fullyDecodeString(snippet, m_encoding);
    if (truncationStyle != TruncationStyle::None) {
        decodedSnippet.truncate(kMaximumFragmentLengthTarget);
        if (truncationStyle == TruncationStyle::SrcLikeAttribute)
            truncateForSrcLikeAttribute(decodedSnippet);
        else if (truncationStyle == TruncationStyle::ScriptLikeAttribute)
            truncateForScriptLikeAttribute(decodedSnippet);
    }
    return decodedSnippet.removeCharacters(&isNonCanonicalCharacter);
}

String XSSAuditor::canonicalizedSnippetForJavaScript(const FilterTokenRequest& request)
{
    String string = request.sourceTracker.source(request.token);
    size_t startPosition = 0;
    size_t endPosition = string.length();
    size_t foundPosition = notFound;
    size_t lastNonSpacePosition = notFound;

    // Skip over initial comments to find start of code.
    while (startPosition < endPosition) {
        while (startPosition < endPosition && isHTMLSpace(string[startPosition]))
            startPosition++;

        // Under SVG/XML rules, only HTML comment syntax matters and the parser returns
        // these as a separate comment tokens. Having consumed whitespace, we need not look
        // further for these.
        if (request.shouldAllowCDATA)
            break;

        // Under HTML rules, both the HTML and JS comment synatx matters, and the HTML
        // comment ends at the end of the line, not with -->.
        if (startsHTMLCommentAt(string, startPosition) || startsSingleLineCommentAt(string, startPosition)) {
            while (startPosition < endPosition && !isJSNewline(string[startPosition]))
                startPosition++;
        } else if (startsMultiLineCommentAt(string, startPosition)) {
            if (startPosition + 2 < endPosition && (foundPosition = string.find("*/", startPosition + 2)) != notFound)
                startPosition = foundPosition + 2;
            else
                startPosition = endPosition;
        } else
            break;
    }

    String result;
    while (startPosition < endPosition && !result.length()) {
        // Stop at next comment (using the same rules as above for SVG/XML vs HTML), when we encounter a comma,
        // when we hit an opening <script> tag, or when we exceed the maximum length target. The comma rule
        // covers a common parameter concatenation case performed by some web servers.
        lastNonSpacePosition = notFound;
        for (foundPosition = startPosition; foundPosition < endPosition; foundPosition++) {
            if (!request.shouldAllowCDATA) {
                if (startsSingleLineCommentAt(string, foundPosition)
                    || startsMultiLineCommentAt(string, foundPosition)
                    || startsHTMLCommentAt(string, foundPosition)) {
                    break;
                }
            }
            if (string[foundPosition] == ',')
                break;

            if (lastNonSpacePosition != notFound && startsOpeningScriptTagAt(string, foundPosition)) {
                foundPosition = lastNonSpacePosition + 1;
                break;
            }
            if (foundPosition > startPosition + kMaximumFragmentLengthTarget) {
                // After hitting the length target, we can only stop at a point where we know we are
                // not in the middle of a %-escape sequence. For the sake of simplicity, approximate
                // not stopping inside a (possibly multiply encoded) %-escape sequence by breaking on
                // whitespace only. We should have enough text in these cases to avoid false positives.
                if (isHTMLSpace(string[foundPosition]))
                    break;
            }

            if (!isHTMLSpace(string[foundPosition]))
                lastNonSpacePosition = foundPosition;
        }

        result = canonicalize(string.substring(startPosition, foundPosition - startPosition), TruncationStyle::None);
        startPosition = foundPosition + 1;
    }
    return result;
}

SuffixTree<ASCIICodebook>* XSSAuditor::decodedHTTPBodySuffixTree()
{
    const unsigned minimumLengthForSuffixTree = 512; // FIXME: Tune this parameter.
    const unsigned suffixTreeDepth = 5;

    if (!m_decodedHTTPBodySuffixTree && m_decodedHTTPBody.length() >= minimumLengthForSuffixTree)
        m_decodedHTTPBodySuffixTree = std::make_unique<SuffixTree<ASCIICodebook>>(m_decodedHTTPBody, suffixTreeDepth);
    return m_decodedHTTPBodySuffixTree.get();
}

bool XSSAuditor::isContainedInRequest(const String& decodedSnippet)
{
    if (decodedSnippet.isEmpty())
        return false;
    if (m_decodedURL.containsIgnoringASCIICase(decodedSnippet))
        return true;
    auto* decodedHTTPBodySuffixTree = this->decodedHTTPBodySuffixTree();
    if (decodedHTTPBodySuffixTree && !decodedHTTPBodySuffixTree->mightContain(decodedSnippet))
        return false;
    return m_decodedHTTPBody.containsIgnoringASCIICase(decodedSnippet);
}

bool XSSAuditor::isLikelySafeResource(const String& url)
{
    // Give empty URLs and about:blank a pass. Making a resourceURL from an
    // empty string below will likely later fail the "no query args test" as
    // it inherits the document's query args.
    if (url.isEmpty() || url == WTF::blankURL().string())
        return true;

    // If the resource is loaded from the same host as the enclosing page, it's
    // probably not an XSS attack, so we reduce false positives by allowing the
    // request, ignoring scheme and port considerations. If the resource has a
    // query string, we're more suspicious, however, because that's pretty rare
    // and the attacker might be able to trick a server-side script into doing
    // something dangerous with the query string.  
    if (m_documentURL.host().isEmpty())
        return false;

    URL resourceURL(m_documentURL, url);
    return (m_documentURL.host() == resourceURL.host() && resourceURL.query().isEmpty());
}

} // namespace WebCore
