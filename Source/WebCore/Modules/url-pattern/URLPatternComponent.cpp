/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "URLPatternComponent.h"

#include "URLPatternCanonical.h"
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/RegExpObject.h>

namespace WebCore {
namespace URLPatternUtilities {

URLPatternComponent::URLPatternComponent(String&& patternString, JSC::Strong<JSC::RegExp>&& regex, Vector<String>&& groupNameList, bool hasRegexpGroups)
    : m_patternString(WTFMove(patternString))
    , m_regularExpression(WTFMove(regex))
    , m_groupNameList(WTFMove(groupNameList))
    , m_hasRegexpGroups(hasRegexpGroups)
{
}

ExceptionOr<URLPatternComponent> URLPatternComponent::compile(Ref<VM> vm, StringView input, EncodingCallbackType type, const URLPatternUtilities::URLPatternStringOptions& options)
{
    auto maybePartList = URLPatternUtilities::URLPatternParser::parse(input, options, type);
    if (maybePartList.hasException())
        return maybePartList.releaseException();
    Vector<Part> partList = maybePartList.releaseReturnValue();

    auto [regularExpressionString, nameList] = generateRegexAndNameList(partList, options);

    OptionSet<Yarr::Flags> flags = { Yarr::Flags::UnicodeSets };
    if (options.ignoreCase)
        flags.add(Yarr::Flags::IgnoreCase);

    RegExp* regularExpression = RegExp::create(vm, regularExpressionString, flags);
    if (!regularExpression->isValid())
        return Exception { ExceptionCode::TypeError, "Unable to create RegExp object regular expression from provided URLPattern string."_s };

    String patternString = generatePatternString(partList, options);

    bool hasRegexGroups = partList.containsIf([](auto& part) {
        return part.type == PartType::Regexp;
    });

    return URLPatternComponent { WTFMove(patternString), JSC::Strong<JSC::RegExp> { vm, regularExpression }, WTFMove(nameList), hasRegexGroups };
}

bool URLPatternComponent::matchSpecialSchemeProtocol(ScriptExecutionContext& context) const
{
    static constexpr std::array specialSchemeList { "ftp"_s, "file"_s, "http"_s, "https"_s, "ws"_s, "wss"_s };
    auto protocolRegex = RegExpObject::create(context.vm(), context.globalObject()->regExpStructure(), m_regularExpression.get(), true);

    bool isSchemeMatch = std::find_if(specialSchemeList.begin(), specialSchemeList.end(), [&](const String& scheme) {
        auto maybeMatch = protocolRegex->exec(context.globalObject(), JSC::jsString(context.vm(), scheme));
        return !maybeMatch.isNull();
    });

    return isSchemeMatch;
}

}
}
