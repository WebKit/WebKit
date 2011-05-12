 /*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "ContentTypeParser.h"

#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static void skipSpaces(const String& input, size_t& startIndex)
{
    while (startIndex < input.length() && input[startIndex] == ' ')
        ++startIndex;
}

static bool isTokenCharacter(char c)
{
    return isASCII(c) && c > ' ' && c != '"' && c != '(' && c != ')' && c != ',' && c != '/' && (c < ':' || c > '@') && (c < '[' || c > ']');
}

static String parseToken(const String& input, size_t& startIndex)
{
    if (startIndex >= input.length())
        return String();

    StringBuilder stringBuilder;
    while (startIndex < input.length()) {
        char currentCharacter = input[startIndex];
        if (!isTokenCharacter(currentCharacter))
            return stringBuilder.toString();
        stringBuilder.append(currentCharacter);
        ++startIndex;
    }
    return stringBuilder.toString();
}

static String parseQuotedString(const String& input, size_t& startIndex)
{
    if (startIndex >= input.length())
        return String();

    if (input[startIndex++] != '"' || startIndex >= input.length())
        return String();

    StringBuilder stringBuilder;
    bool lastCharacterWasBackslash = false;
    char currentCharacter;
    while ((currentCharacter = input[startIndex++]) != '"' || lastCharacterWasBackslash) {
        if (startIndex >= input.length())
            return String();
        if (currentCharacter == '\\' && !lastCharacterWasBackslash) {
            lastCharacterWasBackslash = true;
            continue;
        }
        if (lastCharacterWasBackslash)
            lastCharacterWasBackslash = false;
        stringBuilder.append(currentCharacter);
    }
    return stringBuilder.toString();
}

ContentTypeParser::ContentTypeParser(const String& contentType)
    : m_contentType(contentType.stripWhiteSpace())
{
    parse();
}

String ContentTypeParser::charset() const
{
    return parameterValueForName("charset");
}

String ContentTypeParser::parameterValueForName(const String& name) const
{
    return m_parameters.get(name);
}

size_t ContentTypeParser::parameterCount() const
{
    return m_parameters.size();
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

void ContentTypeParser::parse()
{
    DEFINE_STATIC_LOCAL(const String, contentTypeParameterName, ("Content-Type"));

    if (!m_contentType.startsWith(contentTypeParameterName)) {
        LOG_ERROR("Invalid Content-Type string '%s'", m_contentType.ascii().data());
        return;
    }
    size_t contentTypeLength = m_contentType.length();
    size_t index = contentTypeParameterName.length();
    skipSpaces(m_contentType, index);
    if (index >= contentTypeLength || m_contentType[index] != ':' || ++index >= contentTypeLength)  {
        LOG_ERROR("Invalid Content-Type string '%s'", m_contentType.ascii().data());
        return;
    }

    // There should not be any quoted strings until we reach the parameters.
    size_t semiColonIndex = m_contentType.find(';', index);
    if (semiColonIndex == notFound) {
        m_mimeType = m_contentType.substring(index).stripWhiteSpace();
        return;
    }

    m_mimeType = m_contentType.substring(index, semiColonIndex - index).stripWhiteSpace();
    index = semiColonIndex + 1;
    while (true) {
        skipSpaces(m_contentType, index);
        String key = parseToken(m_contentType, index);
        if (key.isEmpty() || index >= contentTypeLength) {
            LOG_ERROR("Invalid Content-Type parameter name.");
            return;
        }
        // Should we tolerate spaces here?
        if (m_contentType[index++] != '=' || index >= contentTypeLength) {
            LOG_ERROR("Invalid Content-Type malformed parameter.");
            return;
        }

        // Should we tolerate spaces here?
        String value;
        if (m_contentType[index] == '"')
            value = parseQuotedString(m_contentType, index);
        else
            value = parseToken(m_contentType, index);

        if (value.isNull()) {
            LOG_ERROR("Invalid Content-Type, invalid parameter value.");
            return;
        }

        // Should we tolerate spaces here?
        if (index < contentTypeLength && m_contentType[index++] != ';') {
            LOG_ERROR("Invalid Content-Type, invalid character at the end of key/value parameter.");
            return;
        }

        m_parameters.set(key, value);

        if (index >= contentTypeLength)
            return;
    }
}

}
