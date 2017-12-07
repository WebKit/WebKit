/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>
#include <wtf/text/LineEnding.h>

namespace TestWebKitAPI {

const uint8_t null = 0;
const uint8_t CR = '\r';
const uint8_t LF = '\n';
const uint8_t letterA = 'a';
const uint8_t letterB = 'b';

static const char* stringify(const Vector<uint8_t>& vector)
{
    static char buffer[100];
    char* out = buffer;
    for (auto character : vector) {
        switch (character) {
        case '\0':
            *out++ = '<';
            *out++ = '0';
            *out++ = '>';
            break;
        case '\r':
            *out++ = '<';
            *out++ = 'C';
            *out++ = 'R';
            *out++ = '>';
            break;
        case '\n':
            *out++ = '<';
            *out++ = 'L';
            *out++ = 'F';
            *out++ = '>';
            break;
        default:
            *out++ = character;
        }
    }
    *out = '\0';
    return buffer;
}

TEST(WTF, LineEndingNormalizeToLF)
{
    EXPECT_STREQ("", stringify(normalizeLineEndingsToLF({ })));

    EXPECT_STREQ("<0>", stringify(normalizeLineEndingsToLF({ null })));
    EXPECT_STREQ("a", stringify(normalizeLineEndingsToLF({ letterA })));
    EXPECT_STREQ("<LF>", stringify(normalizeLineEndingsToLF({ CR })));
    EXPECT_STREQ("<LF>", stringify(normalizeLineEndingsToLF({ LF })));

    EXPECT_STREQ("<LF>", stringify(normalizeLineEndingsToLF({ CR, LF })));
    EXPECT_STREQ("<LF><LF>", stringify(normalizeLineEndingsToLF({ LF, LF })));
    EXPECT_STREQ("<LF><LF>", stringify(normalizeLineEndingsToLF({ CR, CR })));
    EXPECT_STREQ("<LF><LF>", stringify(normalizeLineEndingsToLF({ LF, CR })));

    EXPECT_STREQ("a<LF>", stringify(normalizeLineEndingsToLF({ letterA, CR })));
    EXPECT_STREQ("a<LF>", stringify(normalizeLineEndingsToLF({ letterA, LF })));
    EXPECT_STREQ("a<LF>", stringify(normalizeLineEndingsToLF({ letterA, CR, LF })));
    EXPECT_STREQ("a<LF><LF>", stringify(normalizeLineEndingsToLF({ letterA, LF, CR })));

    EXPECT_STREQ("a<LF>b", stringify(normalizeLineEndingsToLF({ letterA, CR, letterB })));
    EXPECT_STREQ("a<LF>b", stringify(normalizeLineEndingsToLF({ letterA, LF, letterB })));
    EXPECT_STREQ("a<LF>b", stringify(normalizeLineEndingsToLF({ letterA, CR, LF, letterB })));
    EXPECT_STREQ("a<LF><LF>b", stringify(normalizeLineEndingsToLF({ letterA, LF, CR, letterB })));
}

TEST(WTF, LineEndingNormalizeToCRLF)
{
    EXPECT_STREQ("", stringify(normalizeLineEndingsToCRLF({ })));

    EXPECT_STREQ("<0>", stringify(normalizeLineEndingsToCRLF({ null })));
    EXPECT_STREQ("a", stringify(normalizeLineEndingsToCRLF({ letterA })));
    EXPECT_STREQ("<CR><LF>", stringify(normalizeLineEndingsToCRLF({ CR })));
    EXPECT_STREQ("<CR><LF>", stringify(normalizeLineEndingsToCRLF({ LF })));

    EXPECT_STREQ("<CR><LF>", stringify(normalizeLineEndingsToCRLF({ CR, LF })));
    EXPECT_STREQ("<CR><LF><CR><LF>", stringify(normalizeLineEndingsToCRLF({ LF, LF })));
    EXPECT_STREQ("<CR><LF><CR><LF>", stringify(normalizeLineEndingsToCRLF({ CR, CR })));
    EXPECT_STREQ("<CR><LF><CR><LF>", stringify(normalizeLineEndingsToCRLF({ LF, CR })));

    EXPECT_STREQ("a<CR><LF>", stringify(normalizeLineEndingsToCRLF({ letterA, CR })));
    EXPECT_STREQ("a<CR><LF>", stringify(normalizeLineEndingsToCRLF({ letterA, LF })));
    EXPECT_STREQ("a<CR><LF>", stringify(normalizeLineEndingsToCRLF({ letterA, CR, LF })));
    EXPECT_STREQ("a<CR><LF><CR><LF>", stringify(normalizeLineEndingsToCRLF({ letterA, LF, CR })));

    EXPECT_STREQ("a<CR><LF>b", stringify(normalizeLineEndingsToCRLF({ letterA, CR, letterB })));
    EXPECT_STREQ("a<CR><LF>b", stringify(normalizeLineEndingsToCRLF({ letterA, LF, letterB })));
    EXPECT_STREQ("a<CR><LF>b", stringify(normalizeLineEndingsToCRLF({ letterA, CR, LF, letterB })));
    EXPECT_STREQ("a<CR><LF><CR><LF>b", stringify(normalizeLineEndingsToCRLF({ letterA, LF, CR, letterB })));
}

} // namespace TestWebKitAPI
