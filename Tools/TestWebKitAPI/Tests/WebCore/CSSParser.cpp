/*
 * Copyright (C) 2014 Igalia, S.L. All rights reserved.
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

#include <WebCore/CSSColorValue.h>
#include <WebCore/CSSCustomPropertyValue.h>
#include <WebCore/CSSGridIntegerRepeatValue.h>
#include <WebCore/CSSParser.h>
#include <WebCore/CSSValueList.h>
#include <WebCore/Color.h>
#include <WebCore/MutableStyleProperties.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CSSParser, ParseColorInput)
{
    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(parser.parseDeclaration(properties, "color: #ff0000;"_s));
    auto value = properties->getPropertyCSSValue(CSSPropertyColor).get();

    ASSERT_TRUE(is<CSSValue>(value));
    Color valueColor(Color::red);

    EXPECT_TRUE(value->isColor());
    EXPECT_EQ(valueColor, CSSColorValue::absoluteColor(*value));
}

TEST(CSSParser, ParseColorWithNewlineAndWhitespacesInput)
{
    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(parser.parseDeclaration(properties, "color:  \n    #ff0000;"_s));
    auto value = properties->getPropertyCSSValue(CSSPropertyColor).get();

    ASSERT_TRUE(is<CSSValue>(value));
    Color valueColor(Color::red);

    EXPECT_TRUE(value->isColor());
    EXPECT_EQ(valueColor, CSSColorValue::absoluteColor(*value));
}

TEST(CSSParser, ParseCustomPropertyWithNewlineInput)
{
    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(parser.parseDeclaration(properties, "--mycustomprop: ValueHere\nWithAnotherValue;"_s));
    auto customPropValue = downcast<CSSCustomPropertyValue>(properties->propertyAt(0).value());

    ASSERT_TRUE(is<CSSCustomPropertyValue>(customPropValue));
    auto customText = customPropValue->cssText();
    customText.convertTo16Bit();

    EXPECT_EQ("ValueHere\nWithAnotherValue"_s, customText);
}

TEST(CSSParser, ParseCustomPropertyWithNewlineAndWhitespacesInput)
{
    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(parser.parseDeclaration(properties, "--mycustomprop: ValueHere\nWithAnotherValue         ShouldPreserveAllWhitespace;"_s));
    auto customPropValue = downcast<CSSCustomPropertyValue>(properties->propertyAt(0).value());

    ASSERT_TRUE(is<CSSCustomPropertyValue>(customPropValue));
    auto customText = customPropValue->cssText();
    customText.convertTo16Bit();

    EXPECT_EQ("ValueHere\nWithAnotherValue         ShouldPreserveAllWhitespace"_s, customText);
}

TEST(CSSParser, ParseCustomPropertyWithNewlineBetweenIdentInput)
{
    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(parser.parseDeclaration(properties, "--mycustomprop: foo\nbar"_s));
    auto customPropValue = downcast<CSSCustomPropertyValue>(properties->propertyAt(0).value());

    ASSERT_TRUE(is<CSSCustomPropertyValue>(customPropValue));
    auto customText = customPropValue->cssText();
    customText.convertTo16Bit();

    EXPECT_EQ("foo\nbar"_s, customText);
}

TEST(CSSParser, ParseColorPropertyWithNewlineBetweenIdentInput)
{
    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    ASSERT_TRUE(parser.parseDeclaration(properties, "color: #ff0000;"_s));
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
        EXPECT_EQ(CSSValueCapitalize, valueList[0].valueID());
        EXPECT_EQ(CSSValueFullWidth, valueList[1].valueID());
    };

    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();
    ASSERT_TRUE(parser.parseDeclaration(properties, "text-transform: capitalize\nfull-width;"_s));
    check(properties);

    auto value = properties->propertyAt(0).value();
    auto serialized = value->cssText();
    EXPECT_EQ(serialized, "capitalize full-width"_s);

    CSSParser parser2(strictCSSParserContext());
    auto properties2 = MutableStyleProperties::create();

    StringBuilder builder;
    builder.append("text-transform: "_s, serialized, ";"_s);
    auto decl = builder.toString();

    ASSERT_TRUE(parser.parseDeclaration(properties2, decl));
    check(properties2);
}

static unsigned computeNumberOfTracks(const CSSValueContainingVector& valueList)
{
    unsigned numberOfTracks = 0;
    for (auto& value : valueList) {
        if (value.isGridLineNamesValue())
            continue;
        if (is<CSSGridIntegerRepeatValue>(value)) {
            auto& repeatValue = downcast<CSSGridIntegerRepeatValue>(value);
            numberOfTracks += repeatValue.repetitions().resolveAsIntegerNoConversionDataRequired() * computeNumberOfTracks(repeatValue);
            continue;
        }
        ++numberOfTracks;
    }
    return numberOfTracks;
}

TEST(CSSPropertyParser, GridTrackLimits)
{
    struct {
        const CSSPropertyID propertyID;
        ASCIILiteral input;
        const size_t output;
    }

    testCases[] = {
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(999999, 20px);"_s, 999999 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(999999, 20px);"_s, 999999 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000000, 10%);"_s, 1000000 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000000, 10%);"_s, 1000000 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000000, [first] -webkit-min-content [last]);"_s, 1000000 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000000, [first] -webkit-min-content [last]);"_s, 1000000 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000001, auto);"_s, 1000000 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000001, auto);"_s, 1000000 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(400000, 2em minmax(10px, -webkit-max-content) 0.5fr);"_s, 999999 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(400000, 2em minmax(10px, -webkit-max-content) 0.5fr);"_s, 999999 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last]);"_s, 999999 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last]);"_s, 999999 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(100000000000000000000, 10% 1fr);"_s, 1000000 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(100000000000000000000, 10% 1fr);"_s, 1000000 },
        { CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(100000000000000000000, 10% 5em 1fr auto auto 15px -webkit-min-content);"_s, 999999 },
        { CSSPropertyGridTemplateRows, "grid-template-rows: repeat(100000000000000000000, 10% 5em 1fr auto auto 15px -webkit-min-content);"_s, 999999 },
    };

    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    for (auto& testCase : testCases) {
        ASSERT_TRUE(parser.parseDeclaration(properties, testCase.input));
        RefPtr<CSSValue> value = properties->getPropertyCSSValue(testCase.propertyID);

        ASSERT_TRUE(is<CSSValueContainingVector>(value.get()));
        EXPECT_EQ(computeNumberOfTracks(downcast<CSSValueContainingVector>(*value)), testCase.output);
    }
}

} // namespace TestWebKitAPI
