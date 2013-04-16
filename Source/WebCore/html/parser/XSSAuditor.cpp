/*
 * Copyright (C) 2011 Adam Barth. All Rights Reserved.
 * Copyright (C) 2011 Daniel Bates (dbates@intudata.com).
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

#include "Console.h"
#include "DOMWindow.h"
#include "DecodeEscapeSequences.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLDocumentParser.h"
#include "HTMLNames.h"
#include "HTMLTokenizer.h"
#include "HTMLParamElement.h"
#include "HTMLParserIdioms.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"

#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;

static bool isNonCanonicalCharacter(UChar c)
{
    // We remove all non-ASCII characters, including non-printable ASCII characters.
    //
    // Note, we don't remove backslashes like PHP stripslashes(), which among other things converts "\\0" to the \0 character.
    // Instead, we remove backslashes and zeros (since the string "\\0" =(remove backslashes)=> "0"). However, this has the
    // adverse effect that we remove any legitimate zeros from a string.
    //
    // For instance: new String("http://localhost:8000") => new String("http://localhost:8").
    return (c == '\\' || c == '0' || c == '\0' || c >= 127);
}

static String canonicalize(const String& string)
{
    return string.removeCharacters(&isNonCanonicalCharacter);
}

static bool isRequiredForInjection(UChar c)
{
    return (c == '\'' || c == '"' || c == '<' || c == '>');
}

static bool isTerminatingCharacter(UChar c)
{
    return (c == '&' || c == '/' || c == '"' || c == '\'' || c == '<');
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
    return (start + 3 < string.length() && string[start] == '<' && string[start+1] == '!' && string[start+2] == '-' && string[start+3] == '-');
}

static bool startsSingleLineCommentAt(const String& string, size_t start)
{
    return (start + 1 < string.length() && string[start] == '/' && string[start+1] == '/');
}

static bool startsMultiLineCommentAt(const String& string, size_t start)
{
    return (start + 1 < string.length() && string[start] == '/' && string[start+1] == '*');
}

static bool hasName(const HTMLToken& token, const QualifiedName& name)
{
    return equalIgnoringNullity(token.name(), static_cast<const String&>(name.localName()));
}

static bool findAttributeWithName(const HTMLToken& token, const QualifiedName& name, size_t& indexOfMatchingAttribute)
{
    for (size_t i = 0; i < token.attributes().size(); ++i) {
        if (equalIgnoringNullity(token.attributes().at(i).m_name, name.localName())) {
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
    return equalIgnoringCase(equiv, "refresh") || equalIgnoringCase(equiv, "set-cookie");
}

static inline String decode16BitUnicodeEscapeSequences(const String& string)
{
    // Note, the encoding is ignored since each %u-escape sequence represents a UTF-16 code unit.
    return decodeEscapeSequences<Unicode16BitEscapeSequence>(string, UTF8Encoding());
}

static inline String decodeStandardURLEscapeSequences(const String& string, const TextEncoding& encoding)
{
    // We use decodeEscapeSequences() instead of decodeURLEscapeSequences() (declared in KURL.h) to
    // avoid platform-specific URL decoding differences (e.g. KURLGoogle).
    return decodeEscapeSequences<URLEscapeSequence>(string, encoding);
}

static String fullyDecodeString(const String& string, const TextResourceDecoder* decoder)
{
    const TextEncoding& encoding = decoder ? decoder->encoding() : UTF8Encoding();
    size_t oldWorkingStringLength;
    String workingString = string;
    do {
        oldWorkingStringLength = workingString.length();
        workingString = decode16BitUnicodeEscapeSequences(decodeStandardURLEscapeSequences(workingString, encoding));
    } while (workingString.length() < oldWorkingStringLength);
    workingString.replace('+', ' ');
    workingString = canonicalize(workingString);
    return workingString;
}

XSSAuditor::XSSAuditor(HTMLDocumentParser* parser)
    : m_parser(parser)
    , m_isEnabled(false)
    , m_xssProtection(XSSProtectionEnabled)
    , m_state(Uninitialized)
    , m_shouldAllowCDATA(false)
    , m_scriptTagNestingLevel(0)
    , m_notifiedClient(false)
{
    ASSERT(m_parser);
    if (Frame* frame = parser->document()->frame()) {
        if (Settings* settings = frame->settings())
            m_isEnabled = settings->xssAuditorEnabled();
    }
    // Although tempting to call init() at this point, the various objects
    // we want to reference might not all have been constructed yet.
}

void XSSAuditor::init()
{
    const size_t miniumLengthForSuffixTree = 512; // FIXME: Tune this parameter.
    const int suffixTreeDepth = 5;

    ASSERT(m_state == Uninitialized);
    m_state = Initialized;

    if (!m_isEnabled)
        return;

    // In theory, the Document could have detached from the Frame after the
    // XSSAuditor was constructed.
    if (!m_parser->document()->frame()) {
        m_isEnabled = false;
        return;
    }

    const KURL& url = m_parser->document()->url();

    if (url.isEmpty()) {
        // The URL can be empty when opening a new browser window or calling window.open("").
        m_isEnabled = false;
        return;
    }

    if (url.protocolIsData()) {
        m_isEnabled = false;
        return;
    }

    TextResourceDecoder* decoder = m_parser->document()->decoder();
    m_decodedURL = fullyDecodeString(url.string(), decoder);
    if (m_decodedURL.find(isRequiredForInjection) == notFound)
        m_decodedURL = String();

    if (DocumentLoader* documentLoader = m_parser->document()->frame()->loader()->documentLoader()) {
        DEFINE_STATIC_LOCAL(String, XSSProtectionHeader, ("X-XSS-Protection"));
        m_xssProtection = parseXSSProtectionHeader(documentLoader->response().httpHeaderField(XSSProtectionHeader));

        FormData* httpBody = documentLoader->originalRequest().httpBody();
        if (httpBody && !httpBody->isEmpty()) {
            String httpBodyAsString = httpBody->flattenToString();
            if (!httpBodyAsString.isEmpty()) {
                m_decodedHTTPBody = fullyDecodeString(httpBodyAsString, decoder);
                if (m_decodedHTTPBody.find(isRequiredForInjection) == notFound)
                    m_decodedHTTPBody = String();
                if (m_decodedHTTPBody.length() >= miniumLengthForSuffixTree)
                    m_decodedHTTPBodySuffixTree = adoptPtr(new SuffixTree<ASCIICodebook>(m_decodedHTTPBody, suffixTreeDepth));
            }
        }
    }

    if (m_decodedURL.isEmpty() && m_decodedHTTPBody.isEmpty())
        m_isEnabled = false;
}

void XSSAuditor::filterToken(HTMLToken& token)
{
    if (m_state == Uninitialized)
        init();
   
    ASSERT(m_state == Initialized);
    if (!m_isEnabled || m_xssProtection == XSSProtectionDisabled)
        return;

    bool didBlockScript = false;
    if (token.type() == HTMLTokenTypes::StartTag)
        didBlockScript = filterStartToken(token);
    else if (m_scriptTagNestingLevel) {
        if (token.type() == HTMLTokenTypes::Character)
            didBlockScript = filterCharacterToken(token);
        else if (token.type() == HTMLTokenTypes::EndTag)
            filterEndToken(token);
    }

    if (didBlockScript) {
        // FIXME: Consider using a more helpful console message.
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_parser->document()->addConsoleMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage);

        bool didBlockEntirePage = (m_xssProtection == XSSProtectionBlockEnabled);
        if (didBlockEntirePage)
             m_parser->document()->frame()->loader()->stopAllLoaders();

        if (!m_notifiedClient) {
            m_parser->document()->frame()->loader()->client()->didDetectXSS(m_parser->document()->url(), didBlockEntirePage);
            m_notifiedClient = true;
        }

        if (didBlockEntirePage)
            m_parser->document()->frame()->navigationScheduler()->scheduleLocationChange(m_parser->document()->securityOrigin(), blankURL(), blankURL());
    }
}

bool XSSAuditor::filterStartToken(HTMLToken& token)
{
    bool didBlockScript = eraseDangerousAttributesIfInjected(token);

    if (hasName(token, scriptTag)) {
        didBlockScript |= filterScriptToken(token);
        ASSERT(m_shouldAllowCDATA || !m_scriptTagNestingLevel);
        m_scriptTagNestingLevel++;
    } else if (hasName(token, objectTag))
        didBlockScript |= filterObjectToken(token);
    else if (hasName(token, paramTag))
        didBlockScript |= filterParamToken(token);
    else if (hasName(token, embedTag))
        didBlockScript |= filterEmbedToken(token);
    else if (hasName(token, appletTag))
        didBlockScript |= filterAppletToken(token);
    else if (hasName(token, iframeTag))
        didBlockScript |= filterIframeToken(token);
    else if (hasName(token, metaTag))
        didBlockScript |= filterMetaToken(token);
    else if (hasName(token, baseTag))
        didBlockScript |= filterBaseToken(token);
    else if (hasName(token, formTag))
        didBlockScript |= filterFormToken(token);

    return didBlockScript;
}

void XSSAuditor::filterEndToken(HTMLToken& token)
{
    ASSERT(m_scriptTagNestingLevel);
    if (hasName(token, scriptTag)) {
        m_scriptTagNestingLevel--;
        ASSERT(m_shouldAllowCDATA || !m_scriptTagNestingLevel);
    }
}

bool XSSAuditor::filterCharacterToken(HTMLToken& token)
{
    ASSERT(m_scriptTagNestingLevel);
    if (isContainedInRequest(m_cachedDecodedSnippet) && isContainedInRequest(decodedSnippetForJavaScript(token))) {
        token.eraseCharacters();
        token.appendToCharacter(' '); // Technically, character tokens can't be empty.
        return true;
    }
    return false;
}

bool XSSAuditor::filterScriptToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, scriptTag));

    m_cachedDecodedSnippet = decodedSnippetForName(token);
    m_shouldAllowCDATA = m_parser->tokenizer()->shouldAllowCDATA();

    if (isContainedInRequest(decodedSnippetForName(token)))
        return eraseAttributeIfInjected(token, srcAttr, blankURL().string(), SrcLikeAttribute);

    return false;
}

bool XSSAuditor::filterObjectToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, objectTag));

    bool didBlockScript = false;
    if (isContainedInRequest(decodedSnippetForName(token))) {
        didBlockScript |= eraseAttributeIfInjected(token, dataAttr, blankURL().string(), SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(token, typeAttr);
        didBlockScript |= eraseAttributeIfInjected(token, classidAttr);
    }
    return didBlockScript;
}

bool XSSAuditor::filterParamToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, paramTag));

    size_t indexOfNameAttribute;
    if (!findAttributeWithName(token, nameAttr, indexOfNameAttribute))
        return false;

    const HTMLToken::Attribute& nameAttribute = token.attributes().at(indexOfNameAttribute);
    String name = String(nameAttribute.m_value.data(), nameAttribute.m_value.size());

    if (!HTMLParamElement::isURLParameter(name))
        return false;

    return eraseAttributeIfInjected(token, valueAttr, blankURL().string(), SrcLikeAttribute);
}

bool XSSAuditor::filterEmbedToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, embedTag));

    bool didBlockScript = false;
    if (isContainedInRequest(decodedSnippetForName(token))) {
        didBlockScript |= eraseAttributeIfInjected(token, codeAttr, String(), SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(token, srcAttr, blankURL().string(), SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(token, typeAttr);
    }
    return didBlockScript;
}

bool XSSAuditor::filterAppletToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, appletTag));

    bool didBlockScript = false;
    if (isContainedInRequest(decodedSnippetForName(token))) {
        didBlockScript |= eraseAttributeIfInjected(token, codeAttr, String(), SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(token, objectAttr);
    }
    return didBlockScript;
}

bool XSSAuditor::filterIframeToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, iframeTag));

    bool didBlockScript = false;
    if (isContainedInRequest(decodedSnippetForName(token))) {
        didBlockScript |= eraseAttributeIfInjected(token, srcAttr, String(), SrcLikeAttribute);
        didBlockScript |= eraseAttributeIfInjected(token, srcdocAttr, String(), ScriptLikeAttribute);
    }
    return didBlockScript;
}

bool XSSAuditor::filterMetaToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, metaTag));

    return eraseAttributeIfInjected(token, http_equivAttr);
}

bool XSSAuditor::filterBaseToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, baseTag));

    return eraseAttributeIfInjected(token, hrefAttr);
}

bool XSSAuditor::filterFormToken(HTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    ASSERT(hasName(token, formTag));

    return eraseAttributeIfInjected(token, actionAttr, blankURL().string());
}

bool XSSAuditor::eraseDangerousAttributesIfInjected(HTMLToken& token)
{
    DEFINE_STATIC_LOCAL(String, safeJavaScriptURL, ("javascript:void(0)"));

    bool didBlockScript = false;
    for (size_t i = 0; i < token.attributes().size(); ++i) {
        const HTMLToken::Attribute& attribute = token.attributes().at(i);
        bool isInlineEventHandler = isNameOfInlineEventHandler(attribute.m_name);
        bool valueContainsJavaScriptURL = !isInlineEventHandler && protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(String(attribute.m_value.data(), attribute.m_value.size())));
        if (!isInlineEventHandler && !valueContainsJavaScriptURL)
            continue;
        if (!isContainedInRequest(decodedSnippetForAttribute(token, attribute, ScriptLikeAttribute)))
            continue;
        token.eraseValueOfAttribute(i);
        if (valueContainsJavaScriptURL)
            token.appendToAttributeValue(i, safeJavaScriptURL);
        didBlockScript = true;
    }
    return didBlockScript;
}

bool XSSAuditor::eraseAttributeIfInjected(HTMLToken& token, const QualifiedName& attributeName, const String& replacementValue, AttributeKind treatment)
{
    size_t indexOfAttribute = 0;
    if (findAttributeWithName(token, attributeName, indexOfAttribute)) {
        const HTMLToken::Attribute& attribute = token.attributes().at(indexOfAttribute);
        if (isContainedInRequest(decodedSnippetForAttribute(token, attribute, treatment))) {
            if (attributeName == srcAttr && isSameOriginResource(String(attribute.m_value.data(), attribute.m_value.size())))
                return false;
            if (attributeName == http_equivAttr && !isDangerousHTTPEquiv(String(attribute.m_value.data(), attribute.m_value.size())))
                return false;
            token.eraseValueOfAttribute(indexOfAttribute);
            if (!replacementValue.isEmpty())
                token.appendToAttributeValue(indexOfAttribute, replacementValue);
            return true;
        }
    }
    return false;
}

String XSSAuditor::decodedSnippetForName(const HTMLToken& token)
{
    // Grab a fixed number of characters equal to the length of the token's name plus one (to account for the "<").
    return fullyDecodeString(m_parser->sourceForToken(token), m_parser->document()->decoder()).substring(0, token.name().size() + 1);
}

String XSSAuditor::decodedSnippetForAttribute(const HTMLToken& token, const HTMLToken::Attribute& attribute, AttributeKind treatment)
{
    // The range doesn't inlcude the character which terminates the value. So,
    // for an input of |name="value"|, the snippet is |name="value|. For an
    // unquoted input of |name=value |, the snippet is |name=value|.
    // FIXME: We should grab one character before the name also.
    int start = attribute.m_nameRange.m_start - token.startIndex();
    int end = attribute.m_valueRange.m_end - token.startIndex();
    String decodedSnippet = fullyDecodeString(m_parser->sourceForToken(token).substring(start, end - start), m_parser->document()->decoder());
    decodedSnippet.truncate(kMaximumFragmentLengthTarget);
    if (treatment == SrcLikeAttribute) {
        int slashCount = 0;
        bool commaSeen = false;
        // In HTTP URLs, characters following the first ?, #, or third slash may come from 
        // the page itself and can be merely ignored by an attacker's server when a remote
        // script or script-like resource is requested. In DATA URLS, the payload starts at
        // the first comma, and the the first /*, //, or <!-- may introduce a comment. Characters
        // following this may come from the page itself and may be ignored when the script is
        // executed. For simplicity, we don't differentiate based on URL scheme, and stop at
        // the first # or ?, the third slash, or the first slash or < once a comma is seen.
        for (size_t currentLength = 0; currentLength < decodedSnippet.length(); ++currentLength) {
            UChar currentChar = decodedSnippet[currentLength];
            if (currentChar == '?'
                || currentChar == '#'
                || ((currentChar == '/' || currentChar == '\\') && (commaSeen || ++slashCount > 2))
                || (currentChar == '<' && commaSeen)) {
                decodedSnippet.truncate(currentLength);
                break;
            }
            if (currentChar == ',')
                commaSeen = true;
        }
    } else if (treatment == ScriptLikeAttribute) {
        // Beware of trailing characters which came from the page itself, not the 
        // injected vector. Excluding the terminating character covers common cases
        // where the page immediately ends the attribute, but doesn't cover more
        // complex cases where there is other page data following the injection. 
        // Generally, these won't parse as javascript, so the injected vector
        // typically excludes them from consideration via a single-line comment or
        // by enclosing them in a string literal terminated later by the page's own
        // closing punctuation. Since the snippet has not been parsed, the vector
        // may also try to introduce these via entities. As a result, we'd like to
        // stop before the first "//", the first <!--, the first entity, or the first
        // quote not immediately following the first equals sign (taking whitespace
        // into consideration). To keep things simpler, we don't try to distinguish
        // between entity-introducing amperands vs. other uses, nor do we bother to
        // check for a second slash for a comment, nor do we bother to check for
        // !-- following a less-than sign. We stop instead on any ampersand
        // slash, or less-than sign.
        size_t position = 0;
        if ((position = decodedSnippet.find("=")) != notFound
            && (position = decodedSnippet.find(isNotHTMLSpace, position + 1)) != notFound
            && (position = decodedSnippet.find(isTerminatingCharacter, isHTMLQuote(decodedSnippet[position]) ? position + 1 : position)) != notFound) {
            decodedSnippet.truncate(position);
        }
    }
    return decodedSnippet;
}

String XSSAuditor::decodedSnippetForJavaScript(const HTMLToken& token)
{
    String string = m_parser->sourceForToken(token);
    size_t startPosition = 0;
    size_t endPosition = string.length();
    size_t foundPosition = notFound;

    // Skip over initial comments to find start of code.
    while (startPosition < endPosition) {
        while (startPosition < endPosition && isHTMLSpace(string[startPosition]))
            startPosition++;

        // Under SVG/XML rules, only HTML comment syntax matters and the parser returns
        // these as a separate comment tokens. Having consumed whitespace, we need not look
        // further for these.
        if (m_shouldAllowCDATA)
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
        // Stop at next comment (using the same rules as above for SVG/XML vs HTML), when we 
        // encounter a comma, or when we  exceed the maximum length target. The comma rule
        // covers a common parameter concatenation case performed by some webservers.
        // After hitting the length target, we can only stop at a point where we know we are
        // not in the middle of a %-escape sequence. For the sake of simplicity, approximate
        // not stopping inside a (possibly multiply encoded) %-esacpe sequence by breaking on
        // whitespace only. We should have enough text in these cases to avoid false positives.
        for (foundPosition = startPosition; foundPosition < endPosition; foundPosition++) {
            if (!m_shouldAllowCDATA) {
                if (startsSingleLineCommentAt(string, foundPosition) || startsMultiLineCommentAt(string, foundPosition)) {
                    foundPosition += 2;
                    break;
                }
                if (startsHTMLCommentAt(string, foundPosition)) {
                    foundPosition += 4;
                    break;
                }
            }
            if (string[foundPosition] == ',' || (foundPosition > startPosition + kMaximumFragmentLengthTarget && isHTMLSpace(string[foundPosition]))) {
                break;
            }
        }

        result = fullyDecodeString(string.substring(startPosition, foundPosition - startPosition), m_parser->document()->decoder());
        startPosition = foundPosition + 1;
    }
    return result;
}

bool XSSAuditor::isContainedInRequest(const String& decodedSnippet)
{
    if (decodedSnippet.isEmpty())
        return false;
    if (m_decodedURL.find(decodedSnippet, 0, false) != notFound)
        return true;
    if (m_decodedHTTPBodySuffixTree && !m_decodedHTTPBodySuffixTree->mightContain(decodedSnippet))
        return false;
    return m_decodedHTTPBody.find(decodedSnippet, 0, false) != notFound;
}

bool XSSAuditor::isSameOriginResource(const String& url)
{
    // If the resource is loaded from the same URL as the enclosing page, it's
    // probably not an XSS attack, so we reduce false positives by allowing the
    // request. If the resource has a query string, we're more suspicious,
    // however, because that's pretty rare and the attacker might be able to
    // trick a server-side script into doing something dangerous with the query
    // string.
    KURL resourceURL(m_parser->document()->url(), url);
    return (m_parser->document()->url().host() == resourceURL.host() && resourceURL.query().isEmpty());
}

} // namespace WebCore
