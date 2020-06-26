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
#include "ContentSecurityPolicyMediaListDirective.h"

#include "ContentSecurityPolicy.h"
#include "ContentSecurityPolicyDirectiveList.h"
#include "ParsingUtilities.h"
#include <wtf/text/StringHash.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

template<typename CharacterType> static bool isMediaTypeCharacter(CharacterType c)
{
    return !isASCIISpace(c) && c != '/';
}

ContentSecurityPolicyMediaListDirective::ContentSecurityPolicyMediaListDirective(const ContentSecurityPolicyDirectiveList& directiveList, const String& name, const String& value)
    : ContentSecurityPolicyDirective(directiveList, name, value)
{
    parse(value);
}

bool ContentSecurityPolicyMediaListDirective::allows(const String& type) const
{
    return m_pluginTypes.contains(type);
}

void ContentSecurityPolicyMediaListDirective::parse(const String& value)
{
    // 'plugin-types ____;' OR 'plugin-types;'
    if (value.isEmpty()) {
        directiveList().policy().reportInvalidPluginTypes(value);
        return;
    }

    readCharactersForParsing(value, [&](auto buffer) {
        using CharacterType = typename decltype(buffer)::CharacterType;

        while (buffer.hasCharactersRemaining()) {
            // _____ OR _____mime1/mime1
            // ^        ^
            skipWhile<CharacterType, isASCIISpace>(buffer);
            if (buffer.atEnd())
                return;

            // mime1/mime1 mime2/mime2
            // ^
            auto begin = buffer.position();
            if (!skipExactly<CharacterType, isMediaTypeCharacter>(buffer)) {
                skipWhile<CharacterType, isNotASCIISpace>(buffer);
                directiveList().policy().reportInvalidPluginTypes(String(begin, buffer.position() - begin));
                continue;
            }
            skipWhile<CharacterType, isMediaTypeCharacter>(buffer);

            // mime1/mime1 mime2/mime2
            //      ^
            if (!skipExactly(buffer, '/')) {
                skipWhile<CharacterType, isNotASCIISpace>(buffer);
                directiveList().policy().reportInvalidPluginTypes(String(begin, buffer.position() - begin));
                continue;
            }

            // mime1/mime1 mime2/mime2
            //       ^
            if (!skipExactly<CharacterType, isMediaTypeCharacter>(buffer)) {
                skipWhile<CharacterType, isNotASCIISpace>(buffer);
                directiveList().policy().reportInvalidPluginTypes(String(begin, buffer.position() - begin));
                continue;
            }
            skipWhile<CharacterType, isMediaTypeCharacter>(buffer);

            // mime1/mime1 mime2/mime2 OR mime1/mime1  OR mime1/mime1/error
            //            ^                          ^               ^
            if (buffer.hasCharactersRemaining() && isNotASCIISpace(*buffer)) {
                skipWhile<CharacterType, isNotASCIISpace>(buffer);
                directiveList().policy().reportInvalidPluginTypes(String(begin, buffer.position() - begin));
                continue;
            }
            m_pluginTypes.add(String(begin, buffer.position() - begin));

            ASSERT(buffer.atEnd() || isASCIISpace(*buffer));
        }
    });
}

} // namespace WebCore
