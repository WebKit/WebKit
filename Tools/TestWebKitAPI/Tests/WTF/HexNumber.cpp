/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WTFStringUtilities.h"

#include <wtf/HexNumber.h>

namespace TestWebKitAPI {

// Not using builder.toString() or builder.toStringPreserveCapacity() because they all
// change internal state of builder.
#define expectBuilderContent(expected, builder) \
    { \
        if (builder.is8Bit()) \
            EXPECT_EQ(String(expected), String(builder.characters8(), builder.length())); \
        else \
            EXPECT_EQ(String(expected), String(builder.characters16(), builder.length())); \
    } \

TEST(WTF, HexNumber)
{
    uint32_t integer = 10;
    uint32_t largeInteger = 64206;
    unsigned char byte = 128;

    {
        StringBuilder builder;
        builder.append(hex(integer));
        expectBuilderContent("A", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(integer, Lowercase));
        expectBuilderContent("a", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(integer, 1, Lowercase));
        expectBuilderContent("a", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(integer, 2, Lowercase));
        expectBuilderContent("0a", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(integer, 3, Lowercase));
        expectBuilderContent("00a", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(integer, 4, Lowercase));
        expectBuilderContent("000a", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(largeInteger));
        expectBuilderContent("FACE", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(largeInteger, 2));
        expectBuilderContent("FACE", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(byte));
        expectBuilderContent("80", builder);
    }

    {
        StringBuilder builder;
        builder.append(hex(static_cast<unsigned char>(integer), 2));
        expectBuilderContent("0A", builder);
    }
}

} // namespace
