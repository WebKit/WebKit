/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
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

#include "WebSocketExtensionParser.h"

#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>

namespace WebCore {

bool WebSocketExtensionParser::finished()
{
    return m_data.empty();
}

bool WebSocketExtensionParser::parsedSuccessfully()
{
    return m_data.empty() && !m_didFailParsing;
}

static bool isSeparator(char character)
{
    static constexpr auto separatorCharacters = "()<>@,;:\\\"/[]?={} \t"_s;
    const char* p = strchr(separatorCharacters.characters(), character);
    return p && *p;
}

void WebSocketExtensionParser::skipSpaces()
{
    while (!m_data.empty() && (m_data[0] == ' ' || m_data[0] == '\t'))
        m_data = m_data.subspan(1);
}

bool WebSocketExtensionParser::consumeToken()
{
    skipSpaces();
    auto start = m_data;
    size_t tokenLength = 0;
    while (tokenLength < m_data.size() && isASCIIPrintable(m_data[tokenLength]) && !isSeparator(m_data[tokenLength]))
        ++tokenLength;
    if (tokenLength) {
        m_currentToken = String(start.first(tokenLength));
        m_data = m_data.subspan(tokenLength);
        return true;
    }
    return false;
}

bool WebSocketExtensionParser::consumeQuotedString()
{
    skipSpaces();
    if (m_data.empty() || m_data[0] != '"')
        return false;

    Vector<char> buffer;
    m_data = m_data.subspan(1);
    while (!m_data.empty() && m_data[0] != '"') {
        if (m_data[0] == '\\') {
            m_data = m_data.subspan(1);
            if (m_data.empty())
                return false;
        }
        buffer.append(m_data[0]);
        m_data = m_data.subspan(1);
    }
    if (m_data.empty() || m_data[0] != '"')
        return false;
    m_currentToken = String::fromUTF8(buffer.span());
    if (m_currentToken.isNull())
        return false;
    m_data = m_data.subspan(1);
    return true;
}

bool WebSocketExtensionParser::consumeQuotedStringOrToken()
{
    // This is ok because consumeQuotedString() doesn't update m_data or
    // set m_didFailParsing to true on failure.
    return consumeQuotedString() || consumeToken();
}

bool WebSocketExtensionParser::consumeCharacter(char character)
{
    skipSpaces();
    if (!m_data.empty() && m_data[0] == character) {
        m_data = m_data.subspan(1);
        return true;
    }
    return false;
}

bool WebSocketExtensionParser::parseExtension(String& extensionToken, HashMap<String, String>& extensionParameters)
{
    // Parse extension-token.
    if (!consumeToken()) {
        m_didFailParsing = true;
        return false;
    }

    extensionToken = currentToken();

    // Parse extension-parameters if exists.
    while (consumeCharacter(';')) {
        if (!consumeToken()) {
            m_didFailParsing = true;
            return false;
        }

        String parameterToken = currentToken();
        if (consumeCharacter('=')) {
            if (consumeQuotedStringOrToken())
                extensionParameters.add(parameterToken, currentToken());
            else {
                m_didFailParsing = true;
                return false;
            }
        } else
            extensionParameters.add(parameterToken, String());
    }
    if (!finished() && !consumeCharacter(',')) {
        m_didFailParsing = true;
        return false;
    }

    return true;
}

} // namespace WebCore
