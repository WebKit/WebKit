/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/text/StringSearch.h>

namespace TestWebKitAPI {

TEST(WTF_StringSearch, ConstexprBMHFind)
{
    static BoyerMooreHorspoolTable<uint8_t> table("Hello World"_s);
    size_t result = table.find("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZHello WorldabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"_s, "Hello World"_s);
    EXPECT_EQ(result, 104U);
}

TEST(WTF_StringSearch, Basic)
{
    std::tuple<const char*, const char*> tests[] = {
        { "Hello World", "World" },
        { "日本語", "World" },
        { "Hello日本語World", "日本語" },
        { "Hello", "Hello World" },
        { "Hey", "" },
        { "", "" },
        { "", "Hey" },
        { "     日本語", "   日本語" },
        { "     ", "日本語" },
    };

    for (auto [cstring, cpattern] : tests) {
        String string = String::fromUTF8(cstring);
        String pattern = String::fromUTF8(cpattern);
        BoyerMooreHorspoolTable<uint8_t> table(pattern);
        EXPECT_EQ(table.find(string, pattern), string.find(pattern)) << cstring << " " << cpattern;
    }
}

} // namespace TestWebKitAPI
