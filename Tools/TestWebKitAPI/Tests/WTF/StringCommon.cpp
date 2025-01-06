/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <wtf/Vector.h>
#include <wtf/text/StringCommon.h>

namespace TestWebKitAPI {

#if CPU(ARM64)
TEST(WTF_StringCommon, Find8NonASCII)
{
    Vector<LChar> vector(4096);
    vector.fill('a');

    EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 4096)));

    vector[4095] = 0x80;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.data(), 4095);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 4095 - i)));

    vector[1024] = 0x80;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0xff;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0x7f;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.data(), 4095);

    vector[0] = 0xff;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.data(), 0);
    for (int i = 0; i < 16; ++i) {
        vector[i] = 0xff;
        EXPECT_EQ(WTF::find8NonASCII(vector.subspan(i, 4096 - i)) - vector.data(), i);
    }
}

TEST(WTF_StringCommon, Find16NonASCII)
{
    Vector<UChar> vector(4096);
    vector.fill('a');

    EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 4096)));

    vector[4095] = 0x80;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.data(), 4095);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 4095 - i)));

    vector[1024] = 0x80;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0xff;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0x7f;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.data(), 4095);

    vector[0] = 0xff;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.data(), 0);
    for (int i = 0; i < 16; ++i) {
        vector[i] = 0xff;
        EXPECT_EQ(WTF::find16NonASCII(vector.subspan(i, 4096 - i)) - vector.data(), i);
    }
}
#endif

TEST(WTF_StringCommon, FindIgnoringASCIICaseWithoutLengthIdentical)
{
    EXPECT_EQ(WTF::findIgnoringASCIICaseWithoutLength("needle", "needle"), 0UL);
    EXPECT_EQ(WTF::findIgnoringASCIICaseWithoutLength("needle", "needley"), WTF::notFound);
    EXPECT_EQ(WTF::findIgnoringASCIICaseWithoutLength("needley", "needle"), 0UL);
}

TEST(WTF_StringCommon, CopyElements64To8)
{
    Vector<uint8_t> destination;
    destination.resize(4096);

    Vector<uint64_t> source;
    source.reserveInitialCapacity(4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.data(), source.data(), 4096);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint8_t>(i));
}

TEST(WTF_StringCommon, CopyElements64To16)
{
    Vector<uint16_t> destination;
    destination.resize(4096 + 4 + 4096);

    Vector<uint64_t> source;
    source.reserveInitialCapacity(4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);
    source.append(0xffff);
    source.append(0x10000);
    source.append(UINT64_MAX);
    source.append(0x7fff);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.data(), source.data(), 4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint16_t>(i));
    EXPECT_EQ(destination[4096 + 0], 0xffffU);
    EXPECT_EQ(destination[4096 + 1], 0x0000U);
    EXPECT_EQ(destination[4096 + 2], 0xffffU);
    EXPECT_EQ(destination[4096 + 3], 0x7fffU);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[4096 + 4 + i], static_cast<uint16_t>(i));
}

TEST(WTF_StringCommon, CopyElements64To32)
{
    Vector<uint32_t> destination;
    destination.resize(4096 + 4 + 4096);

    Vector<uint64_t> source;
    source.reserveInitialCapacity(4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);
    source.append(0xffffffffU);
    source.append(0x100000000ULL);
    source.append(UINT64_MAX);
    source.append(0x7fffffffU);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.data(), source.data(), 4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint32_t>(i));
    EXPECT_EQ(destination[4096 + 0], 0xffffffffU);
    EXPECT_EQ(destination[4096 + 1], 0x00000000U);
    EXPECT_EQ(destination[4096 + 2], 0xffffffffU);
    EXPECT_EQ(destination[4096 + 3], 0x7fffffffU);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[4096 + 4 + i], static_cast<uint32_t>(i));
}

TEST(WTF_StringCommon, CopyElements32To16)
{
    Vector<uint16_t> destination;
    destination.resize(4096 + 4 + 4096);

    Vector<uint32_t> source;
    source.reserveInitialCapacity(4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);
    source.append(0xffff);
    source.append(0x10000);
    source.append(UINT32_MAX);
    source.append(0x7fff);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.data(), source.data(), 4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint16_t>(i));
    EXPECT_EQ(destination[4096 + 0], 0xffffU);
    EXPECT_EQ(destination[4096 + 1], 0x0000U);
    EXPECT_EQ(destination[4096 + 2], 0xffffU);
    EXPECT_EQ(destination[4096 + 3], 0x7fffU);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[4096 + 4 + i], static_cast<uint16_t>(i));
}

TEST(WTF_StringCommon, CharactersContain8)
{
    {
        Vector<LChar> source;
        EXPECT_FALSE((charactersContain<LChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 0, 1>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 0, 1, 2>(source.span())));
    }

    {
        Vector<LChar> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_TRUE((charactersContain<LChar, 0>(source.span())));
        EXPECT_TRUE((charactersContain<LChar, 1>(source.span())));
        EXPECT_TRUE((charactersContain<LChar, 2>(source.span())));
        EXPECT_TRUE((charactersContain<LChar, 2, 3>(source.span())));
        EXPECT_TRUE((charactersContain<LChar, 16, 14>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 16>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 16, 15>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 16, 15, 17>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 16, 15, 17, 18>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 0x81, 0x82>(source.span())));
    }

    {
        Vector<LChar> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_FALSE((charactersContain<LChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 0xff>(source.span())));
        EXPECT_TRUE((charactersContain<LChar, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<LChar, 250>(source.span())));
        EXPECT_TRUE((charactersContain<LChar, 249>(source.span())));
    }
}

TEST(WTF_StringCommon, CharactersContain16)
{
    {
        Vector<UChar> source;
        EXPECT_FALSE((charactersContain<UChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0, 1>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0, 1, 2>(source.span())));
    }

    {
        Vector<UChar> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_TRUE((charactersContain<UChar, 0>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 1>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 2>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 2, 3>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 16, 14>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 16>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 16, 15>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 16, 15, 17>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 16, 15, 17, 18>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x81, 0x82>(source.span())));
    }

    {
        Vector<UChar> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_FALSE((charactersContain<UChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0xff>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 250>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 249>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 0, 249>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x101>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1001>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1001, 0x1001>(source.span())));
    }

    {
        Vector<UChar> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i + 0x1000);
        }
        EXPECT_FALSE((charactersContain<UChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0xff>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 250>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 249>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x101>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 0x1001>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1000>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1100>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1000 + 256>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1000 + 250>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 0x1000 + 249>(source.span())));
        EXPECT_TRUE((charactersContain<UChar, 0x1000 + 249, 0>(source.span())));
        EXPECT_FALSE((charactersContain<UChar, 0x1000 + 250, 0>(source.span())));
    }
}

TEST(WTF_StringCommon, CountMatchedCharacters8)
{
    {
        Vector<LChar> source;
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 1)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 2)), 0U);
    }

    {
        Vector<LChar> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 2)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 3)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 14)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 15)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 16)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 17)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 18)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0x81)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0x82)), 0U);
    }

    {
        Vector<LChar> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0x81)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 249)), 1U);
    }

    {
        Vector<LChar> source;
        for (unsigned c = 0; c < 1024; ++c) {
            for (unsigned i = 0; i < 250; ++i) {
                if (i & 0x1)
                    source.append(i);
            }
        }
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 1)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0x81)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 249)), 1024U);
    }

    {
        Vector<LChar> source;
        for (unsigned c = 0; c < 1024; ++c) {
            for (unsigned i = 0; i < 250; ++i)
                source.append(1);
        }
        source.append(1);
        source.append(1);
        source.append(1);

        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 1)), source.size());
        EXPECT_EQ((WTF::countMatchedCharacters<LChar>(source.span(), 0x81)), 0U);
    }
}

TEST(WTF_StringCommon, CountMatchedCharacters16)
{
    {
        Vector<UChar> source;
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 1)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 2)), 0U);
    }

    {
        Vector<UChar> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 2)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 3)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 14)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 15)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 16)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 17)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 18)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0x81)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0x82)), 0U);
    }

    {
        Vector<UChar> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0x81)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 249)), 1U);
    }

    {
        Vector<UChar> source;
        for (unsigned c = 0; c < 1024; ++c) {
            for (unsigned i = 0; i < 250; ++i) {
                if (i & 0x1)
                    source.append(i);
            }
        }
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 1)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0x81)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 249)), 1024U);
    }

    {
        Vector<UChar> source;
        for (unsigned c = 0; c < 0xffff; ++c) {
            for (unsigned i = 0; i < 250; ++i)
                source.append(1);
        }
        source.append(1);
        source.append(1);
        source.append(1);

        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 1)), source.size());
        EXPECT_EQ((WTF::countMatchedCharacters<UChar>(source.span(), 0x81)), 0U);
    }
}

} // namespace
