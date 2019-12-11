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

namespace WebCore {

static bool isMediaTypeCharacter(UChar c)
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
    auto characters = StringView(value).upconvertedCharacters();
    const UChar* begin = characters;
    const UChar* position = begin;
    const UChar* end = begin + value.length();

    // 'plugin-types ____;' OR 'plugin-types;'
    if (value.isEmpty()) {
        directiveList().policy().reportInvalidPluginTypes(value);
        return;
    }

    while (position < end) {
        // _____ OR _____mime1/mime1
        // ^        ^
        skipWhile<UChar, isASCIISpace>(position, end);
        if (position == end)
            return;

        // mime1/mime1 mime2/mime2
        // ^
        begin = position;
        if (!skipExactly<UChar, isMediaTypeCharacter>(position, end)) {
            skipWhile<UChar, isNotASCIISpace>(position, end);
            directiveList().policy().reportInvalidPluginTypes(String(begin, position - begin));
            continue;
        }
        skipWhile<UChar, isMediaTypeCharacter>(position, end);

        // mime1/mime1 mime2/mime2
        //      ^
        if (!skipExactly<UChar>(position, end, '/')) {
            skipWhile<UChar, isNotASCIISpace>(position, end);
            directiveList().policy().reportInvalidPluginTypes(String(begin, position - begin));
            continue;
        }

        // mime1/mime1 mime2/mime2
        //       ^
        if (!skipExactly<UChar, isMediaTypeCharacter>(position, end)) {
            skipWhile<UChar, isNotASCIISpace>(position, end);
            directiveList().policy().reportInvalidPluginTypes(String(begin, position - begin));
            continue;
        }
        skipWhile<UChar, isMediaTypeCharacter>(position, end);

        // mime1/mime1 mime2/mime2 OR mime1/mime1  OR mime1/mime1/error
        //            ^                          ^               ^
        if (position < end && isNotASCIISpace(*position)) {
            skipWhile<UChar, isNotASCIISpace>(position, end);
            directiveList().policy().reportInvalidPluginTypes(String(begin, position - begin));
            continue;
        }
        m_pluginTypes.add(String(begin, position - begin));

        ASSERT(position == end || isASCIISpace(*position));
    }
}

} // namespace WebCore
