/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "APIContentRuleList.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "WebCompiledContentRuleList.h"
#include <WebCore/CombinedURLFilters.h>
#include <WebCore/ContentExtensionParser.h>
#include <WebCore/URLFilterParser.h>

namespace API {

ContentRuleList::ContentRuleList(Ref<WebKit::WebCompiledContentRuleList>&& contentRuleList, WebKit::NetworkCache::Data&& mappedFile)
    : m_compiledRuleList(WTFMove(contentRuleList))
    , m_mappedFile(WTFMove(mappedFile))
{
}

ContentRuleList::~ContentRuleList()
{
}

const WTF::String& ContentRuleList::name() const
{
    return m_compiledRuleList->data().identifier;
}

bool ContentRuleList::supportsRegularExpression(const WTF::String& regex)
{
    using namespace WebCore::ContentExtensions;
    CombinedURLFilters combinedURLFilters;
    URLFilterParser urlFilterParser(combinedURLFilters);

    switch (urlFilterParser.addPattern(regex, false, 0)) {
    case URLFilterParser::Ok:
    case URLFilterParser::MatchesEverything:
        return true;
    case URLFilterParser::NonASCII:
    case URLFilterParser::UnsupportedCharacterClass:
    case URLFilterParser::BackReference:
    case URLFilterParser::ForwardReference:
    case URLFilterParser::MisplacedStartOfLine:
    case URLFilterParser::WordBoundary:
    case URLFilterParser::AtomCharacter:
    case URLFilterParser::Group:
    case URLFilterParser::Disjunction:
    case URLFilterParser::MisplacedEndOfLine:
    case URLFilterParser::EmptyPattern:
    case URLFilterParser::YarrError:
    case URLFilterParser::InvalidQuantifier:
        break;
    }
    return false;
}

std::error_code ContentRuleList::parseRuleList(const WTF::String& ruleList)
{
    auto result = WebCore::ContentExtensions::parseRuleList(ruleList);
    if (result.has_value())
        return { };

    return result.error();
}

} // namespace API

#endif // ENABLE(CONTENT_EXTENSIONS)
