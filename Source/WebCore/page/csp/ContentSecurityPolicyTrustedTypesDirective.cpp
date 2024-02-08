/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentSecurityPolicyTrustedTypesDirective.h"

#include "ContentSecurityPolicy.h"
#include "ContentSecurityPolicyDirectiveList.h"
#include "ParsingUtilities.h"
#include <wtf/text/StringCommon.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<typename CharacterType> static bool isTrustedTypesNone(StringParsingBuffer<CharacterType> buffer)
{
    skipWhile<isASCIIWhitespace>(buffer);

    if (!skipExactlyIgnoringASCIICase(buffer, "'none'"_s))
        return false;

    skipWhile<isASCIIWhitespace>(buffer);

    return buffer.atEnd();
}

template<typename CharacterType> static bool isTrustedTypeCharacter(CharacterType c)
{
    return !isASCIIWhitespace(c);
}

template<typename CharacterType> static bool isPolicyNameCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '-' || c == '#' || c == '=' || c == '_' || c == '/' || c == '@' || c == '.' || c == '%';
}

ContentSecurityPolicyTrustedTypesDirective::ContentSecurityPolicyTrustedTypesDirective(const ContentSecurityPolicyDirectiveList& directiveList, const String& name, const String& value)
    : ContentSecurityPolicyDirective(directiveList, name, value)
{
    parse(value);
}

bool ContentSecurityPolicyTrustedTypesDirective::allows(const String& value, bool isDuplicate, AllowTrustedTypePolicy& details) const
{
    auto invalidPolicy = value.find([](UChar ch) {
        return !isPolicyNameCharacter(ch);
    });

    if (isDuplicate && !m_allowDuplicates)
        details = AllowTrustedTypePolicy::DisallowedDuplicateName;
    else if (isDuplicate && value == "default"_s)
        details = AllowTrustedTypePolicy::DisallowedDuplicateName;
    else if (invalidPolicy != notFound)
        details = AllowTrustedTypePolicy::DisallowedName;
    else if (!(m_allowAny || m_list.contains(value)))
        details = AllowTrustedTypePolicy::DisallowedName;
    else
        details = AllowTrustedTypePolicy::Allowed;

    return details == AllowTrustedTypePolicy::Allowed;
}

void ContentSecurityPolicyTrustedTypesDirective::parse(const String& value)
{
    // 'trusted-types;'
    if (value.isEmpty())
        return;

    readCharactersForParsing(value, [&](auto buffer) {
        if (isTrustedTypesNone(buffer))
            return;

        while (buffer.hasCharactersRemaining()) {
            skipWhile<isASCIIWhitespace>(buffer);
            if (buffer.atEnd())
                return;

            auto beginPolicy = buffer.position();
            skipWhile<isTrustedTypeCharacter>(buffer);

            auto policyBuffer = StringParsingBuffer { beginPolicy, buffer.position() };

            if (skipExactlyIgnoringASCIICase(policyBuffer, "'allow-duplicates'"_s)) {
                m_allowDuplicates = true;
                continue;
            }

            if (skipExactlyIgnoringASCIICase(policyBuffer, "'none'"_s)) {
                directiveList().policy().reportInvalidTrustedTypesNoneKeyword();
                continue;
            }

            if (skipExactly(policyBuffer, '*')) {
                m_allowAny = true;
                continue;
            }

            if (skipExactly<isPolicyNameCharacter>(policyBuffer)) {
                auto policy = String(beginPolicy, buffer.position() - beginPolicy);
                m_list.add(policy);
            } else {
                auto policy = String(beginPolicy, buffer.position() - beginPolicy);
                directiveList().policy().reportInvalidTrustedTypesPolicy(policy);
            }

            ASSERT(buffer.atEnd() || isASCIIWhitespace(*buffer));
        }
    });
}

} // namespace WebCore
