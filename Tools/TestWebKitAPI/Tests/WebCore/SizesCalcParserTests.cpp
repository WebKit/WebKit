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

#include <WebCore/CSSParser.h>
#include <WebCore/CSSParserToken.h>
#include <WebCore/CSSParserTokenRange.h>
#include <WebCore/CSSTokenizer.h>
#include <WebCore/SizesCalcParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

struct SizesCalcTestCase {
    const String input;
    const float output;
    const bool valid;
};

TEST(SizesCalcParserTest, TestSizesCalcs)
{
    SizesCalcTestCase test_cases[] = {
        { "calc((100vw - 2 * 40px - 2 * 30px) / 3)"_s, 120, true },
        { "calc((100vw - 40px - 60px - 40px) / 3)"_s, 120, true },
        { "calc((50vw + 40px + 30px + 40px) / 3)"_s, 120, true },
        { "calc((100vw - 2 / 2 * 40px - 2 * 30px) / 4)"_s, 100, true },
        { "calc((100vw - 2 * 2 / 2 * 40px - 2 * 30px) / 3)"_s, 120, true },
        { "calc((100vw - 2 * 2 / 2 * 40px - 2 * 30px) / 3)"_s, 120, true },
        { "calc((100vw - 2 * 2 * 20px - 2 * 30px) / 3)"_s, 120, true },
        { "calc((100vw - 320px / 2 / 2 - 2 * 30px) / 3)"_s, 120, true }
    };
    // for (auto& [input, output, valid] : test_cases) {
    // SizesCalcParser calcParser(
    //     CSSParserTokenRange(CSSTokenizer(input).tokenRange()),
    //     media_values);
    // ASSERT_EQ(valid, calcParser.isValid());
    // if (calcParser.isValid())
    //   ASSERT_EQ(output, calcParser.result());
    // }
}

} // namespace TestWebKitAPI
