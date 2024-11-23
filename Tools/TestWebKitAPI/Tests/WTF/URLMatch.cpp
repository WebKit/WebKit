/*
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
#include <wtf/URLMatch.h>

#include "Test.h"
#include "WTFTestUtilities.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

using namespace URLMatch;

class WTF_URLMatch : public testing::Test {
protected:
    void CheckRule(const String& ruleString);

    std::optional<Rule> tryParseRule(const String& ruleString)
    {
        auto result = parser.parseLine(ruleString);
        if (!result)
            return std::nullopt;
        return result.value();
    }

    Rule parseRule(const String& ruleString)
    {
        auto result = parser.parseLine(ruleString);
        return result ? result.value() : Rule();
    }

    URLMatch::Parser parser;
};

TEST_F(WTF_URLMatch, ParseLineSimple)
{
    auto rule = parseRule("example.com"_s);

    EXPECT_EQ(rule.pattern, "example.com"_s);
    EXPECT_EQ(rule.elementTypes, Rule::defaultElementTypes());
    EXPECT_FALSE(rule.activationTypes);
    EXPECT_TRUE(rule.domains.isEmpty());
    EXPECT_FALSE(rule.isException);
    EXPECT_EQ(rule.anchorLeft, Anchor::None);
    EXPECT_EQ(rule.anchorRight, Anchor::None);
    EXPECT_EQ(rule.isThirdParty, TriState::DontCare);
    EXPECT_FALSE(rule.matchCase);
}

TEST_F(WTF_URLMatch, ParseMultipleLines)
{
    auto source = "||example.com\n@@||example.com/images/*$image\n\n!comment\n"_s;

    auto result = parser.parse(source);
    ASSERT_TRUE(result);

    auto rules = result.value();
    ASSERT_EQ(rules.size(), 2u);

    EXPECT_EQ(rules[0].pattern, "example.com"_s);
    EXPECT_FALSE(rules[0].isException);

    EXPECT_EQ(rules[1].pattern, "example.com/images/*"_s);
    EXPECT_TRUE(rules[1].isException);
    EXPECT_TRUE(rules[1].elementTypes.contains(ElementType::Image));
}

TEST_F(WTF_URLMatch, ToStringSimple)
{
    URLMatch::Rule rule {
        .pattern = "Movies"_s,
        .isException = true,
        .anchorLeft = Anchor::Boundary,
        .anchorRight = Anchor::Boundary,
        .matchCase = true,
    };

    rule.domains.append("example.com"_s);
    rule.domains.append("example.net"_s);

    EXPECT_EQ(rule.toString(), "@@|Movies|$match-case,domain=example.com|example.net"_s);
}

void WTF_URLMatch::CheckRule(const String& ruleString)
{
    auto rule = parseRule(ruleString);
    ASSERT_TRUE(rule.isValid());

    EXPECT_EQ(rule.toString(), ruleString);
}

TEST_F(WTF_URLMatch, ParseAndBackToString)
{
    CheckRule("||example.com^"_s);
    CheckRule("Movies|$~third-party,domain=example.com"_s);
    CheckRule("@@|Movies|$match-case,domain=example.com|example.net"_s);
    CheckRule("||example.com/project/*.jpg$image"_s);
    CheckRule("||example.com/project/*.jpg$~image"_s);
    CheckRule("@@||example.com/project/index.html$document"_s);
    CheckRule("@@||example.com/project/index.html$~script,document"_s);
}

}
