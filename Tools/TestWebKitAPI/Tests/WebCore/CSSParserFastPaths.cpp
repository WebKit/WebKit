/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/CSSParserFastPaths.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CSSParserFastPaths, ParseRgbAndRgba)
{
    StringView expectedValidInputs[] = {
        "rgb(255,0,0)"_s,
        "rgba(255,0,0)"_s,
        "rgb(255,0,0,1.0)"_s,
        "rgba(255,0,0,1.0)"_s,
        "rgb(0,255,0)"_s,
        "rgba(0,255,0)"_s,
        "rgb(0,255,0,1.0)"_s,
        "rgba(0,255,0,1.0)"_s,
        "rgb(0,0,255)"_s,
        "rgba(0,0,255)"_s,
        "rgb(0,0,255,1.0)"_s,
        "rgba(0,0,255,1.0)"_s
    };

    for (auto input : expectedValidInputs)
        EXPECT_TRUE(CSSParserFastPaths::parseSimpleColor(input));

    StringView expectedInvalidInputs[] = {
        "rgb(255,0,0"_s,
        "rgba(255,0,0"_s,
        "rgb(255,0,0,"_s,
        "rgba(255,0,0,"_s,
        "rgb(255,0,0?"_s,
        "rgba(255,0,0?"_s,
        "rgb(255,0,0,)"_s,
        "rgba(255,0,0,)"_s,
        "rgb(255,0,0,1.0"_s,
        "rgba(255,0,0,1.0"_s,
        "rgb(255,0,0?1.0,"_s,
        "rgba(255,0,0?1.0,"_s,
        "rgb(255,0,0,1.0?"_s,
        "rgba(255,0,0,1.0?"_s,
        "rgb(255,0,0,1.0,)"_s,
        "rgba(255,0,0,1.0,)"_s,
    };

    for (auto input : expectedInvalidInputs)
        EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor(input));
}

} // namespace TestWebKitAPI
