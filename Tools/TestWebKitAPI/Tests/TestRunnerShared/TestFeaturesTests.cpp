/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "TestCommand.h"
#include "TestFeatures.h"
#include <span>
#include <string_view>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/StdFilesystem.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(ParseDisabledKeys, SplitsCommaSeparatedList)
{
    EXPECT_EQ(WTR::parseDisabledKeys("FirstKey,SecondKey,ThirdKey"), (std::set<std::string> { "FirstKey", "SecondKey", "ThirdKey" }));
}

TEST(ParseDisabledKeys, EmptyStringHasNoKeys)
{
    EXPECT_TRUE(WTR::parseDisabledKeys("").empty());
}

TEST(ParseDisabledKeys, TrimsWhitespaceAroundKeys)
{
    EXPECT_EQ(WTR::parseDisabledKeys(" FirstKey ,\tSecondKey\n,  ThirdKey"), (std::set<std::string> { "FirstKey", "SecondKey", "ThirdKey" }));
}

TEST(ParseDisabledKeys, DropsEmptyItems)
{
    EXPECT_EQ(WTR::parseDisabledKeys(",FirstKey,, ,SecondKey,"), (std::set<std::string> { "FirstKey", "SecondKey" }));
}

TEST(ParseDisabledKeys, AcceptsMacroLikeKeys)
{
    EXPECT_EQ(WTR::parseDisabledKeys("RC_REMOVE_FOO(3),3SAR,Key-5"), (std::set<std::string> { "RC_REMOVE_FOO(3)", "3SAR", "Key-5" }));
}

TEST(ParseDisabledKeys, DropsKeysContainingWhitespaceOrQuotes)
{
    EXPECT_EQ(WTR::parseDisabledKeys("ValidKey,Inner Space,Inner\tTab,\"DoubleQuoted\",Single'Quote,OtherValidKey"), (std::set<std::string> { "ValidKey", "OtherValidKey" }));
}

class TestHeaderForTest : public testing::Test {
protected:
    void TearDown() override
    {
        if (m_testFilePath)
            FileSystem::deleteFile(m_testFilePath);
    }

    WTR::TestHeader testHeaderForTestFileWithFirstLine(std::string_view firstLine)
    {
        auto [path, handle] = FileSystem::openTemporaryFile("TestHeaderForTest"_s, ".html"_s);
        m_testFilePath = path;
        EXPECT_TRUE(!!handle);
        EXPECT_TRUE(!!handle.write(byteCast<uint8_t>(std::span { firstLine })));
        handle = { };

        WTR::TestCommand command;
        command.absolutePath = std::filesystem::path { utf8(m_testFilePath).data() };
        std::unordered_map<std::string, WTR::TestHeaderKeyType> keyTypeMap { { "useFlexibleViewport", WTR::TestHeaderKeyType::BoolTestRunner } };
        return WTR::testHeaderForTest(command, keyTypeMap);
    }

private:
    RefPtr<StringImpl> m_testFilePath;
};

TEST_F(TestHeaderForTest, CommaSeparatedSkipIfDisabledKeys)
{
    auto header = testHeaderForTestFileWithFirstLine("<!DOCTYPE html><!-- webkit-test-runner [ skipIfDisabled=OtherKey,TestKey ] -->");
    EXPECT_EQ(header.skipIfDisabledKeys, (std::vector<std::string> { "OtherKey", "TestKey" }));
}

TEST_F(TestHeaderForTest, RepeatedSkipIfDisabledKeys)
{
    auto header = testHeaderForTestFileWithFirstLine("<!DOCTYPE html><!-- webkit-test-runner [ skipIfDisabled=OtherKey skipIfDisabled=TestKey ] -->");
    EXPECT_EQ(header.skipIfDisabledKeys, (std::vector<std::string> { "OtherKey", "TestKey" }));
}

TEST_F(TestHeaderForTest, HeaderWithoutSkipIfDisabledHasNoSkipIfDisabledKeys)
{
    auto header = testHeaderForTestFileWithFirstLine("<!DOCTYPE html><!-- webkit-test-runner [ useFlexibleViewport=true ] -->");
    EXPECT_TRUE(header.skipIfDisabledKeys.empty());
    EXPECT_EQ(header.features.boolTestRunnerFeatures, (std::unordered_map<std::string, bool> { { "useFlexibleViewport", true } }));
}

TEST_F(TestHeaderForTest, SkipIfDisabledIsNotATestFeature)
{
    auto header = testHeaderForTestFileWithFirstLine("<!DOCTYPE html><!-- webkit-test-runner [ useFlexibleViewport=true skipIfDisabled=TestKey ] -->");
    WTR::TestFeatures expectedFeatures;
    expectedFeatures.boolTestRunnerFeatures = { { "useFlexibleViewport", true } };
    EXPECT_TRUE(header.features == expectedFeatures);
    EXPECT_EQ(header.skipIfDisabledKeys, (std::vector<std::string> { "TestKey" }));
}

TEST(FirstDisabledKeyForTest, ReturnsFirstHeaderKeyInDisabledSet)
{
    WTR::TestHeader header;
    header.skipIfDisabledKeys = { "EnabledKey", "ZuluKey", "AlphaKey" };
    EXPECT_EQ(WTR::firstDisabledKeyForTest(WTR::TestCommand { }, header, { "AlphaKey", "ZuluKey" }), std::optional<std::string> { "ZuluKey" });
}

TEST(FirstDisabledKeyForTest, ReturnsNulloptWhenNoHeaderKeyIsDisabled)
{
    WTR::TestHeader header;
    header.skipIfDisabledKeys = { "EnabledKey", "OtherEnabledKey" };
    EXPECT_EQ(WTR::firstDisabledKeyForTest(WTR::TestCommand { }, header, { "DisabledKey" }), std::nullopt);
    EXPECT_EQ(WTR::firstDisabledKeyForTest(WTR::TestCommand { }, header, { }), std::nullopt);
}

TEST(FirstDisabledKeyForTest, ReturnsNulloptWithoutSkipIfDisabledKeys)
{
    EXPECT_EQ(WTR::firstDisabledKeyForTest(WTR::TestCommand { }, WTR::TestHeader { }, { "DisabledKey" }), std::nullopt);
}

TEST(FirstDisabledKeyForTest, IgnoresSkipIfDisabledKeysInAReference)
{
    WTR::TestCommand command;
    command.isReference = true;
    WTR::TestHeader header;
    header.skipIfDisabledKeys = { "DisabledKey" };
    EXPECT_EQ(WTR::firstDisabledKeyForTest(command, header, { "DisabledKey" }), std::nullopt);
}

TEST(SimilarDisabledKey, FlagsTransposedLetters)
{
    EXPECT_EQ(WTR::similarDisabledKey("TsetKey", { "TestKey" }), std::optional<std::string> { "TestKey" });
}

TEST(SimilarDisabledKey, FlagsOneSubstitutedLetter)
{
    EXPECT_EQ(WTR::similarDisabledKey("SOME_MACRA_4", { "SOME_MACRO_4" }), std::optional<std::string> { "SOME_MACRO_4" });
}

TEST(SimilarDisabledKey, FlagsTwoEditsInALongKey)
{
    EXPECT_EQ(WTR::similarDisabledKey("SOME_MAC_4", { "SOME_MACRO_4" }), std::optional<std::string> { "SOME_MACRO_4" });
}

TEST(SimilarDisabledKey, IgnoresTwoEditsInAShortKey)
{
    EXPECT_EQ(WTR::similarDisabledKey("Kez", { "KeyA" }), std::nullopt);
}

TEST(SimilarDisabledKey, FlagsCaseOnlyDifference)
{
    EXPECT_EQ(WTR::similarDisabledKey("testkey", { "TestKey" }), std::optional<std::string> { "TestKey" });
}

TEST(SimilarDisabledKey, IgnoresDigitOnlyDifferences)
{
    EXPECT_EQ(WTR::similarDisabledKey("SOME_MACRO_4", { "SOME_MACRO_5" }), std::nullopt);
    EXPECT_EQ(WTR::similarDisabledKey("SOME_MACRO(4)", { "SOME_MACRO(5)" }), std::nullopt);
    EXPECT_EQ(WTR::similarDisabledKey("SAR3", { "SAR12" }), std::nullopt);
}

TEST(SimilarDisabledKey, IgnoresUnrelatedKeys)
{
    EXPECT_EQ(WTR::similarDisabledKey("TestKey", { "OtherKey", "RC_REMOVE_FOO(3)" }), std::nullopt);
}

TEST(SimilarDisabledKey, IgnoresAnExactMatch)
{
    EXPECT_EQ(WTR::similarDisabledKey("TestKey", { "TestKey" }), std::nullopt);
}

TEST(ParseInputLine, ReferenceTokenMarksAReferenceLoad)
{
    EXPECT_TRUE(WTR::parseInputLine("/test-expected.html'--reference").isReference);
}

TEST(ParseInputLine, LoadWithoutReferenceTokenIsNotAReference)
{
    EXPECT_FALSE(WTR::parseInputLine("/test.html'--timeout'1000").isReference);
}

} // namespace TestWebKitAPI
