/*
 * Copyright (C) 2014 Igalia, S.L. All rights reserved.
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

#include <WebCore/CSSGridIntegerRepeatValue.h>
#include <WebCore/CSSParser.h>
#include <WebCore/CSSValueList.h>
#include <WebCore/StyleProperties.h>

namespace TestWebKitAPI {

using namespace WebCore;

static unsigned computeNumberOfTracks(CSSValueList& valueList)
{
    unsigned numberOfTracks = 0;
    for (const auto& value : valueList) {
        if (value->isGridLineNamesValue())
            continue;
        if (is<CSSGridIntegerRepeatValue>(value)) {
            auto& repeatValue = downcast<CSSGridIntegerRepeatValue>(value.get());
            numberOfTracks += repeatValue.repetitions() * computeNumberOfTracks(repeatValue);
            continue;
        }
        ++numberOfTracks;
    }
    return numberOfTracks;
}

TEST(CSSPropertyParserTest, GridTrackLimits)
{
    struct {
        const CSSPropertyID propertyID;
        const char* input;
        const size_t output;
    } testCases[] = {
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(999999, 20px);", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(999999, 20px);", 999999},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000000, 10%);", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000000, 10%);", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000000, [first] -webkit-min-content [last]);", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000000, [first] -webkit-min-content [last]);", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000001, auto);", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000001, auto);", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(400000, 2em minmax(10px, -webkit-max-content) 0.5fr);", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(400000, 2em minmax(10px, -webkit-max-content) 0.5fr);", 999999},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last]);", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last]);", 999999},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(100000000000000000000, 10% 1fr);", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(100000000000000000000, 10% 1fr);", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(100000000000000000000, 10% 5em 1fr auto auto 15px -webkit-min-content);", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(100000000000000000000, 10% 5em 1fr auto auto 15px -webkit-min-content);", 999999},
    };

    CSSParser parser(strictCSSParserContext());
    auto properties = MutableStyleProperties::create();

    for (auto& testCase : testCases) {
        ASSERT_TRUE(parser.parseDeclaration(properties, testCase.input));
        RefPtr<CSSValue> value = properties->getPropertyCSSValue(testCase.propertyID);

        ASSERT_TRUE(value->isValueList());
        EXPECT_EQ(computeNumberOfTracks(*downcast<CSSValueList>(value.get())), testCase.output);
    }
}

} // namespace TestWebKitAPI
