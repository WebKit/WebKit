 /*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ParsedContentType.h"

#include <wtf/text/CString.h>

namespace WebCore {

class DummyParsedContentType {
public:
    void setContentType(const SubstringRange&, Mode) const { }
    void setContentTypeParameter(const String&, const String&, Mode) const { }
};

static void skipSpaces(const String& input, unsigned& startIndex)
{
    while (startIndex < input.length() && input[startIndex] == ' ')
        ++startIndex;
}

static bool isQuotedStringTokenCharacter(UChar c)
{
    return (c >= ' ' && c <= '~') || (c >= 0x80 && c <= 0xFF) || c == '\t';
}

static bool isTokenCharacter(UChar c)
{
    return isASCII(c) && c > ' ' && c != '"' && c != '(' && c != ')' && c != ',' && c != '/' && (c < ':' || c > '@') && (c < '[' || c > ']');
}

using CharacterMeetsCondition = bool (*)(UChar);

static Optional<SubstringRange> parseToken(const String& input, unsigned& startIndex, CharacterMeetsCondition characterMeetsCondition, Mode mode, bool skipTrailingWhitespace = false)
{
    unsigned inputLength = input.length();
    unsigned tokenStart = startIndex;
    unsigned& tokenEnd = startIndex;

    if (tokenEnd >= inputLength)
        return WTF::nullopt;

    while (tokenEnd < inputLength && characterMeetsCondition(input[tokenEnd])) {
        if (mode == Mode::Rfc2045 && !isTokenCharacter(input[tokenEnd]))
            break;
        ++tokenEnd;
    }

    if (tokenEnd == tokenStart)
        return WTF::nullopt;
    if (skipTrailingWhitespace) {
        while (input[tokenEnd - 1] == ' ')
            --tokenEnd;
    }
    return SubstringRange(tokenStart, tokenEnd - tokenStart);
}

static bool containsNonTokenCharacters(const String& input, SubstringRange range)
{
    for (unsigned index = 0; index < range.second; ++index) {
        if (!isTokenCharacter(input[range.first + index]))
            return true;
    }
    return false;
}

static Optional<SubstringRange> parseQuotedString(const String& input, unsigned& startIndex, Mode mode)
{
    unsigned inputLength = input.length();
    unsigned quotedStringStart = startIndex + 1;
    unsigned& quotedStringEnd = startIndex;

    if (quotedStringEnd >= inputLength)
        return WTF::nullopt;

    if (input[quotedStringEnd++] != '"' || quotedStringEnd >= inputLength)
        return WTF::nullopt;

    bool lastCharacterWasBackslash = false;
    char currentCharacter;
    while ((currentCharacter = input[quotedStringEnd++]) != '"' || lastCharacterWasBackslash) {
        if (quotedStringEnd >= inputLength) {
            if (mode == Mode::Rfc2045)
                return WTF::nullopt;
            break;
        }
        if (currentCharacter == '\\' && !lastCharacterWasBackslash) {
            lastCharacterWasBackslash = true;
            continue;
        }
        if (lastCharacterWasBackslash)
            lastCharacterWasBackslash = false;
    }
    return SubstringRange(quotedStringStart, quotedStringEnd - quotedStringStart - 1);
}

static String substringForRange(const String& string, const SubstringRange& range)
{
    return string.substring(range.first, range.second);
}

// From http://tools.ietf.org/html/rfc2045#section-5.1:
//
// content := "Content-Type" ":" type "/" subtype
//            *(";" parameter)
//            ; Matching of media type and subtype
//            ; is ALWAYS case-insensitive.
//
// type := discrete-type / composite-type
//
// discrete-type := "text" / "image" / "audio" / "video" /
//                  "application" / extension-token
//
// composite-type := "message" / "multipart" / extension-token
//
// extension-token := ietf-token / x-token
//
// ietf-token := <An extension token defined by a
//                standards-track RFC and registered
//                with IANA.>
//
// x-token := <The two characters "X-" or "x-" followed, with
//             no intervening white space, by any token>
//
// subtype := extension-token / iana-token
//
// iana-token := <A publicly-defined extension token. Tokens
//                of this form must be registered with IANA
//                as specified in RFC 2048.>
//
// parameter := attribute "=" value
//
// attribute := token
//              ; Matching of attributes
//              ; is ALWAYS case-insensitive.
//
// value := token / quoted-string
//
// token := 1*<any (US-ASCII) CHAR except SPACE, CTLs,
//             or tspecials>
//
// tspecials :=  "(" / ")" / "<" / ">" / "@" /
//               "," / ";" / ":" / "\" / <">
//               "/" / "[" / "]" / "?" / "="
//               ; Must be in quoted-string,
//               ; to use within parameter values

static bool isNotForwardSlash(UChar ch)
{
    return ch != '/';
}

static bool isNotSemicolon(UChar ch)
{
    return ch != ';';
}

static bool isNotSemicolonOrEqualSign(UChar ch)
{
    return ch != ';' && ch != '=';
}

template <class ReceiverType>
bool parseContentType(const String& contentType, ReceiverType& receiver, Mode mode)
{
    unsigned index = 0;
    unsigned contentTypeLength = contentType.length();
    skipSpaces(contentType, index);
    if (index >= contentTypeLength)  {
        LOG_ERROR("Invalid Content-Type string '%s'", contentType.ascii().data());
        return false;
    }

    unsigned contentTypeStart = index;
    auto typeRange = parseToken(contentType, index, isNotForwardSlash, mode);
    if (!typeRange || containsNonTokenCharacters(contentType, *typeRange)) {
        LOG_ERROR("Invalid Content-Type, invalid type value.");
        return false;
    }

    if (contentType[index++] != '/') {
        LOG_ERROR("Invalid Content-Type, missing '/'.");
        return false;
    }

    auto subTypeRange = parseToken(contentType, index, isNotSemicolon, mode, mode == Mode::MimeSniff);
    if (!subTypeRange || containsNonTokenCharacters(contentType, *subTypeRange)) {
        LOG_ERROR("Invalid Content-Type, invalid subtype value.");
        return false;
    }

    // There should not be any quoted strings until we reach the parameters.
    size_t semiColonIndex = contentType.find(';', contentTypeStart);
    if (semiColonIndex == notFound) {
        receiver.setContentType(SubstringRange(contentTypeStart, contentTypeLength - contentTypeStart), mode);
        return true;
    }

    receiver.setContentType(SubstringRange(contentTypeStart, semiColonIndex - contentTypeStart), mode);
    index = semiColonIndex + 1;
    while (true) {
        skipSpaces(contentType, index);
        auto keyRange = parseToken(contentType, index, isNotSemicolonOrEqualSign, mode);
        if (mode == Mode::Rfc2045 && (!keyRange || index >= contentTypeLength)) {
            LOG_ERROR("Invalid Content-Type parameter name.");
            return false;
        }

        // Should we tolerate spaces here?
        if (mode == Mode::Rfc2045) {
            if (contentType[index++] != '=' || index >= contentTypeLength) {
                LOG_ERROR("Invalid Content-Type malformed parameter.");
                return false;
            }
        } else {
            if (index >= contentTypeLength)
                break;
            if (contentType[index] != '=' && contentType[index] != ';') {
                LOG_ERROR("Invalid Content-Type malformed parameter.");
                return false;
            }
            if (contentType[index++] == ';')
                continue;
        }

        String parameterName = substringForRange(contentType, *keyRange);

        // Should we tolerate spaces here?
        Optional<SubstringRange> valueRange;
        if (contentType[index] == '"') {
            valueRange = parseQuotedString(contentType, index, mode);
            if (mode == Mode::MimeSniff)
                parseToken(contentType, index, isNotSemicolon, mode);
        } else
            valueRange = parseToken(contentType, index, isNotSemicolon, mode, mode == Mode::MimeSniff);

        if (!valueRange) {
            if (mode == Mode::MimeSniff)
                continue;
            LOG_ERROR("Invalid Content-Type, invalid parameter value.");
            return false;
        }

        String parameterValue = substringForRange(contentType, *valueRange);
        // Should we tolerate spaces here?
        if (mode == Mode::Rfc2045 && index < contentTypeLength && contentType[index++] != ';') {
            LOG_ERROR("Invalid Content-Type, invalid character at the end of key/value parameter.");
            return false;
        }

        receiver.setContentTypeParameter(parameterName, parameterValue, mode);

        if (index >= contentTypeLength)
            return true;
    }

    return true;
}

bool isValidContentType(const String& contentType, Mode mode)
{
    if (contentType.contains('\r') || contentType.contains('\n'))
        return false;

    DummyParsedContentType parsedContentType = DummyParsedContentType();
    return parseContentType<DummyParsedContentType>(contentType, parsedContentType, mode);
}

ParsedContentType::ParsedContentType(const String& contentType, Mode mode)
    : m_contentType(contentType.stripWhiteSpace())
{
    parseContentType<ParsedContentType>(m_contentType, *this, mode);
}

String ParsedContentType::charset() const
{
    return parameterValueForName("charset");
}

String ParsedContentType::parameterValueForName(const String& name) const
{
    return m_parameters.get(name);
}

size_t ParsedContentType::parameterCount() const
{
    return m_parameters.size();
}

void ParsedContentType::setContentType(const SubstringRange& contentRange, Mode mode)
{
    m_mimeType = substringForRange(m_contentType, contentRange).stripWhiteSpace();
    if (mode == Mode::MimeSniff)
        m_mimeType.convertToASCIILowercase();
}

static bool isNonTokenCharacter(UChar ch)
{
    return !isTokenCharacter(ch);
}

static bool isNonQuotedStringTokenCharacter(UChar ch)
{
    return !isQuotedStringTokenCharacter(ch);
}

void ParsedContentType::setContentTypeParameter(const String& keyName, const String& keyValue, Mode mode)
{
    if (mode == Mode::MimeSniff) {
        if (m_parameters.contains(keyName) || keyName.find(isNonTokenCharacter) != notFound || keyValue.find(isNonQuotedStringTokenCharacter) != notFound)
            return;
        keyName.convertToASCIILowercase();
    }
    m_parameters.set(keyName, keyValue);
}

}
