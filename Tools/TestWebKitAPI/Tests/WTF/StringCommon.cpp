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

    EXPECT_FALSE(WTF::find8NonASCII(vector.data(), 4096));

    vector[4095] = 0x80;
    EXPECT_EQ(WTF::find8NonASCII(vector.data(), 4096) - vector.data(), 4095);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_FALSE(WTF::find8NonASCII(vector.data(), 4095 - i));

    vector[1024] = 0x80;
    EXPECT_EQ(WTF::find8NonASCII(vector.data(), 4096) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find8NonASCII(vector.data(), 1023));

    vector[1024] = 0xff;
    EXPECT_EQ(WTF::find8NonASCII(vector.data(), 4096) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find8NonASCII(vector.data(), 1023));

    vector[1024] = 0x7f;
    EXPECT_EQ(WTF::find8NonASCII(vector.data(), 4096) - vector.data(), 4095);

    vector[0] = 0xff;
    EXPECT_EQ(WTF::find8NonASCII(vector.data(), 4096) - vector.data(), 0);
    for (int i = 0; i < 16; ++i) {
        vector[i] = 0xff;
        EXPECT_EQ(WTF::find8NonASCII(vector.data() + i, 4096 - i) - vector.data(), i);
    }
}

TEST(WTF_StringCommon, Find16NonASCII)
{
    Vector<UChar> vector(4096);
    vector.fill('a');

    EXPECT_FALSE(WTF::find16NonASCII(vector.data(), 4096));

    vector[4095] = 0x80;
    EXPECT_EQ(WTF::find16NonASCII(vector.data(), 4096) - vector.data(), 4095);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_FALSE(WTF::find16NonASCII(vector.data(), 4095 - i));

    vector[1024] = 0x80;
    EXPECT_EQ(WTF::find16NonASCII(vector.data(), 4096) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find16NonASCII(vector.data(), 1023));

    vector[1024] = 0xff;
    EXPECT_EQ(WTF::find16NonASCII(vector.data(), 4096) - vector.data(), 1024);
    EXPECT_FALSE(WTF::find16NonASCII(vector.data(), 1023));

    vector[1024] = 0x7f;
    EXPECT_EQ(WTF::find16NonASCII(vector.data(), 4096) - vector.data(), 4095);

    vector[0] = 0xff;
    EXPECT_EQ(WTF::find16NonASCII(vector.data(), 4096) - vector.data(), 0);
    for (int i = 0; i < 16; ++i) {
        vector[i] = 0xff;
        EXPECT_EQ(WTF::find16NonASCII(vector.data() + i, 4096 - i) - vector.data(), i);
    }
}
#endif

} // namespace
