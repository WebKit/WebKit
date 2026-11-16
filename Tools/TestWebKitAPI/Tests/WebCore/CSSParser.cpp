/*
 * Copyright (C) 2014 Igalia, S.L. All rights reserved.
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#include <WebCore/CSSColorValue.h>
#include <WebCore/CSSCustomPropertyValue.h>
#include <WebCore/CSSGridTemplateListValue.h>
#include <WebCore/CSSKeywordValueInlines.h>
#include <WebCore/CSSParser.h>
#include <WebCore/CSSSerializationContext.h>
#include <WebCore/CSSValueList.h>
#include <WebCore/Color.h>
#include <WebCore/MutableStyleProperties.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/StyleRule.h>
#include <WebCore/StyleSheetContents.h>
#include <wtf/StackPointer.h>
#include <wtf/Threading.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CSSParser, ParseColorInput)
{
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "color: #ff0000;"_s, strictCSSParserContext()));
    auto value = properties->getPropertyCSSValue(CSSPropertyID::Color).unsafeGet();

    ASSERT_TRUE(is<CSSValue>(value));
    Color valueColor(Color::red);

    EXPECT_TRUE(value->isColor());
    EXPECT_EQ(valueColor, CSSColorValue::absoluteColor(*value));
}

TEST(CSSParser, ParseColorWithNewlineAndWhitespacesInput)
{
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "color:  \n    #ff0000;"_s, strictCSSParserContext()));
    auto value = properties->getPropertyCSSValue(CSSPropertyID::Color).unsafeGet();

    ASSERT_TRUE(is<CSSValue>(value));
    Color valueColor(Color::red);

    EXPECT_TRUE(value->isColor());
    EXPECT_EQ(valueColor, CSSColorValue::absoluteColor(*value));
}

TEST(CSSParser, ParseCustomPropertyWithNewlineInput)
{
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "--mycustomprop: ValueHere\nWithAnotherValue;"_s, strictCSSParserContext()));
    auto customPropValue = downcast<CSSCustomPropertyValue>(properties->propertyAt(0).value());

    ASSERT_TRUE(is<CSSCustomPropertyValue>(customPropValue));
    auto customText = customPropValue->cssText(CSS::defaultSerializationContext());
    customText.convertTo16Bit();

    EXPECT_EQ("ValueHere\nWithAnotherValue"_s, customText);
}

TEST(CSSParser, ParseCustomPropertyWithNewlineAndWhitespacesInput)
{
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "--mycustomprop: ValueHere\nWithAnotherValue         ShouldPreserveAllWhitespace;"_s, strictCSSParserContext()));
    auto customPropValue = downcast<CSSCustomPropertyValue>(properties->propertyAt(0).value());

    ASSERT_TRUE(is<CSSCustomPropertyValue>(customPropValue));
    auto customText = customPropValue->cssText(CSS::defaultSerializationContext());
    customText.convertTo16Bit();

    EXPECT_EQ("ValueHere\nWithAnotherValue         ShouldPreserveAllWhitespace"_s, customText);
}

TEST(CSSParser, ParseCustomPropertyWithNewlineBetweenIdentInput)
{
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "--mycustomprop: foo\nbar"_s, strictCSSParserContext()));
    auto customPropValue = downcast<CSSCustomPropertyValue>(properties->propertyAt(0).value());

    ASSERT_TRUE(is<CSSCustomPropertyValue>(customPropValue));
    auto customText = customPropValue->cssText(CSS::defaultSerializationContext());
    customText.convertTo16Bit();

    EXPECT_EQ("foo\nbar"_s, customText);
}

TEST(CSSParser, ParseColorPropertyWithNewlineBetweenIdentInput)
{
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "color: #ff0000;"_s, strictCSSParserContext()));
    auto value = properties->propertyAt(0).value();

    ASSERT_TRUE(is<CSSValue>(value));

    Color valueColor(Color::red);

    EXPECT_TRUE(value->isColor());
    EXPECT_EQ(valueColor, CSSColorValue::absoluteColor(*value));
}

TEST(CSSParser, ParseTextTransformPropertyWithNewlineBetweenTwoIdentInput)
{
    auto check = [&] (auto& properties) {
        ASSERT_EQ((size_t)1, properties->size());

        auto value = properties->propertyAt(0).value();
        ASSERT_TRUE(is<CSSValueList>(value));
        auto& valueList = *downcast<CSSValueList>(value);

        ASSERT_EQ((size_t)2, valueList.size());
        EXPECT_EQ(CSSValueID::Capitalize, valueID(valueList[0]));
        EXPECT_EQ(CSSValueID::FullWidth, valueID(valueList[1]));
    };

    auto properties = MutableStyleProperties::create();
    ASSERT_TRUE(CSSParser::parseDeclarationList(properties, "text-transform: capitalize\nfull-width;"_s, strictCSSParserContext()));
    check(properties);

    auto value = properties->propertyAt(0).value();
    auto serialized = value->cssText(CSS::defaultSerializationContext());
    EXPECT_EQ(serialized, "capitalize full-width"_s);

    auto properties2 = MutableStyleProperties::create();

    StringBuilder builder;
    builder.append("text-transform: "_s, serialized, ";"_s);
    auto decl = builder.toString();

    ASSERT_TRUE(CSSParser::parseDeclarationList(properties2, decl, strictCSSParserContext()));
    check(properties2);
}

#if !ASAN_ENABLED

static constexpr uint8_t stackPaintByte = 0xA5;

// Paints from 1 KB below this frame down to paintSize below stackTop, clamped inside the stack; returns the lowest painted address.
NEVER_INLINE static uintptr_t paintStackBelow(uintptr_t stackTop, size_t paintSize)
{
    auto paintTop = reinterpret_cast<uintptr_t>(currentStackPointer()) - 1 * KB;
    auto stackLimit = reinterpret_cast<uintptr_t>(Thread::currentSingleton().stack().end()) + 64 * KB;
    auto paintBottom = std::max(stackTop - paintSize, stackLimit);
    memsetSpan(unsafeMakeSpan(reinterpret_cast<uint8_t*>(paintBottom), paintTop - paintBottom), stackPaintByte);
    WTF::compilerFence();
    return paintBottom;
}

NEVER_INLINE static void parseStyleSheet(StyleSheetContents& styleSheet, const String& text)
{
    styleSheet.parseString(text);
}

// Returns the distance from stackTop down to the lowest byte above paintBottom that no longer holds stackPaintByte.
NEVER_INLINE static size_t deepestStackUseBelow(uintptr_t stackTop, uintptr_t paintBottom)
{
    auto stack = unsafeMakeSpan(reinterpret_cast<const volatile uint8_t*>(paintBottom), stackTop - paintBottom);
    for (size_t i = 0; i < stack.size(); ++i) {
        if (stack[i] != stackPaintByte)
            return stackTop - (paintBottom + i);
    }
    return 0;
}

TEST(CSSParser, ParseMediaAndStyleRulesNestedToRuleListLimitStackUse)
{
    // 64 pairs nest 128 rule lists, the parser's maximumRuleListNestingLevel.
    constexpr unsigned nestingPairs = 64;
    // The default size of a secondary thread's stack on macOS.
    constexpr size_t stackUseLimit = 512 * KB;

    ProcessWarming::initializeNames();

    StringBuilder builder;
    for (unsigned i = 0; i < nestingPairs; ++i)
        builder.append("@media all { a { "_s);
    builder.append("color: green; "_s);
    for (unsigned i = 0; i < nestingPairs; ++i)
        builder.append("} } "_s);
    builder.append("b { color: red; }"_s);
    auto styleSheetText = builder.toString();

    Ref styleSheet = StyleSheetContents::create(strictCSSParserContext());

    auto stackTop = reinterpret_cast<uintptr_t>(currentStackPointer());
    auto paintBottom = paintStackBelow(stackTop, 1 * MB);
    parseStyleSheet(styleSheet, styleSheetText);
    auto stackUse = deepestStackUseBelow(stackTop, paintBottom);

    ASSERT_GT(stackTop - paintBottom, stackUseLimit);
    EXPECT_LT(stackUse, stackUseLimit);

    auto& rules = styleSheet->childRules();
    ASSERT_EQ(2u, rules.size());
    EXPECT_TRUE(rules.first()->isMediaRule());
    EXPECT_TRUE(rules.last()->isStyleRule());
}

#endif // !ASAN_ENABLED

static unsigned computeNumberOfTracks(const SpaceSeparatedVector<Variant<CSS::GridLineNames, CSS::GridTrackSize>>& repeatedTracks)
{
    unsigned numberOfTracks = 0;
    for (auto& repeatedTrack : repeatedTracks) {
        if (WTF::holdsAlternative<CSS::GridTrackSize>(repeatedTrack))
            ++numberOfTracks;
    }
    return numberOfTracks;
}

static unsigned computeNumberOfTracks(const CSSGridTemplateListValue& templateListValue)
{
    return WTF::switchOn(templateListValue.list(),
        [](CSS::Keyword::None) -> unsigned {
            return 0;
        },
        [](const CSS::GridSubgrid&) -> unsigned {
            return 0;
        },
        [](const CSS::GridTrackList& trackList) -> unsigned {
            unsigned numberOfTracks = 0;
            for (auto& track : trackList.value) {
                WTF::switchOn(track,
                    [&](const CSS::GridLineNames&) {
                        // Ignored in count.
                    },
                    [&](const CSS::GridTrackSize&) {
                        ++numberOfTracks;
                    },
                    [&](const CSS::GridTrackRepeatFunction& repeatFunction) {
                        WTF::switchOn(repeatFunction->repetitions,
                            [&](const CSS::Integer<CSS::Positive, unsigned>& numberOfRepetitions) {
                                return WTF::switchOn(numberOfRepetitions,
                                    [&](const CSS::Integer<CSS::Positive, unsigned>::Raw& numberOfRepetitions) {
                                        numberOfTracks += numberOfRepetitions.value * computeNumberOfTracks(repeatFunction->repeated);
                                    },
                                    [&](const CSS::Integer<CSS::Positive, unsigned>::Calc&) {
                                        // Number of repetitions is not yet calculated.
                                    }
                                );
                            },
                            [&](CSS::SpecificKeyword auto const& /* auto-fit or auto-fill */) {
                                // Ignored in count.
                            }
                        );
                    }
                );
            }
            return numberOfTracks;
        }
    );
}

TEST(CSSPropertyParser, GridTrackLimits)
{
    struct {
        const CSSPropertyID propertyID;
        ASCIILiteral input;
        const size_t output;
    }

    testCases[] = {
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(999999, 20px);"_s, 999999 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(999999, 20px);"_s, 999999 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(1000000, 10%);"_s, 1000000 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(1000000, 10%);"_s, 1000000 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(1000000, [first] -webkit-min-content [last]);"_s, 1000000 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(1000000, [first] -webkit-min-content [last]);"_s, 1000000 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(1000001, auto);"_s, 1000000 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(1000001, auto);"_s, 1000000 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(400000, 2em minmax(10px, -webkit-max-content) 0.5fr);"_s, 999999 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(400000, 2em minmax(10px, -webkit-max-content) 0.5fr);"_s, 999999 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last]);"_s, 999999 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last]);"_s, 999999 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(100000000000000000000, 10% 1fr);"_s, 1000000 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(100000000000000000000, 10% 1fr);"_s, 1000000 },
        { CSSPropertyID::GridTemplateColumns, "grid-template-columns: repeat(100000000000000000000, 10% 5em 1fr auto auto 15px -webkit-min-content);"_s, 999999 },
        { CSSPropertyID::GridTemplateRows, "grid-template-rows: repeat(100000000000000000000, 10% 5em 1fr auto auto 15px -webkit-min-content);"_s, 999999 },
    };

    auto properties = MutableStyleProperties::create();

    for (auto& testCase : testCases) {
        ASSERT_TRUE(CSSParser::parseDeclarationList(properties, testCase.input, strictCSSParserContext()));
        RefPtr<CSSValue> value = properties->getPropertyCSSValue(testCase.propertyID);

        ASSERT_TRUE(is<CSSGridTemplateListValue>(value.get()));
        EXPECT_EQ(computeNumberOfTracks(downcast<CSSGridTemplateListValue>(*value)), testCase.output);
    }
}

} // namespace TestWebKitAPI
