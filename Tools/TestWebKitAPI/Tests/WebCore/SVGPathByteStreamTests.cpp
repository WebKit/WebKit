/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/Path.h>
#include <WebCore/SVGPathByteStream.h>
#include <WebCore/SVGPathUtilities.h>
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

using namespace WebCore;

// Values of WebCore::SVGPathSegType, which is not exported.
static constexpr uint8_t moveToAbs = 2;
static constexpr uint8_t lineToAbs = 4;
static constexpr uint8_t arcAbs = 10;

static void appendFloat(Vector<uint8_t>& bytes, float value)
{
    bytes.append(asByteSpan(value));
}

static void appendPoint(Vector<uint8_t>& bytes, float x, float y)
{
    appendFloat(bytes, x);
    appendFloat(bytes, y);
}

static Vector<uint8_t> moveToBytes()
{
    Vector<uint8_t> bytes;
    bytes.append(moveToAbs);
    appendPoint(bytes, 10, 20);
    return bytes;
}

static Vector<uint8_t> arcBytes(uint8_t largeArc, uint8_t sweep)
{
    auto bytes = moveToBytes();
    bytes.append(arcAbs);
    appendFloat(bytes, 50);
    appendFloat(bytes, 50);
    appendFloat(bytes, 0);
    bytes.append(largeArc);
    bytes.append(sweep);
    appendPoint(bytes, 100, 100);
    return bytes;
}

static Path pathFromBytes(Vector<uint8_t>&& bytes)
{
    return buildPathFromByteStream(SVGPathByteStream { WTF::move(bytes) });
}

TEST(SVGPathByteStream, TruncatedCommandArguments)
{
    // A move-to with only 3 of its 8 argument bytes.
    Vector<uint8_t> truncatedMoveTo { moveToAbs, 0, 0, 0 };
    EXPECT_TRUE(pathFromBytes(WTF::move(truncatedMoveTo)).isEmpty());

    // A valid move-to followed by a line-to with only one of its two floats.
    auto truncatedLineTo = moveToBytes();
    truncatedLineTo.append(lineToAbs);
    appendFloat(truncatedLineTo, 30);
    EXPECT_TRUE(pathFromBytes(WTF::move(truncatedLineTo)).definitelyEqual(pathFromBytes(moveToBytes())));

    // A valid arc cut short in the middle of its target point.
    auto truncatedArc = arcBytes(1, 1);
    truncatedArc.shrink(truncatedArc.size() - 2);
    EXPECT_TRUE(pathFromBytes(WTF::move(truncatedArc)).definitelyEqual(pathFromBytes(moveToBytes())));
}

TEST(SVGPathByteStream, InvalidArcFlags)
{
    auto validArcPath = pathFromBytes(arcBytes(1, 0));
    EXPECT_FALSE(validArcPath.definitelyEqual(pathFromBytes(moveToBytes())));

    // Flags are stored as bools, so any byte other than 0 or 1 is invalid and ends parsing.
    EXPECT_TRUE(pathFromBytes(arcBytes(2, 0)).definitelyEqual(pathFromBytes(moveToBytes())));
    EXPECT_TRUE(pathFromBytes(arcBytes(0, 0xFF)).definitelyEqual(pathFromBytes(moveToBytes())));
}

TEST(SVGPathByteStream, CanBlendTruncatedStreams)
{
    SVGPathByteStream valid { arcBytes(1, 1) };
    EXPECT_TRUE(canBlendSVGPathByteStreams(valid, valid));

    auto truncatedBytes = arcBytes(1, 1);
    truncatedBytes.shrink(truncatedBytes.size() - 1);
    SVGPathByteStream truncated { WTF::move(truncatedBytes) };
    EXPECT_FALSE(canBlendSVGPathByteStreams(valid, truncated));
    EXPECT_FALSE(canBlendSVGPathByteStreams(truncated, valid));

    SVGPathByteStream invalidFlag { arcBytes(1, 2) };
    EXPECT_FALSE(canBlendSVGPathByteStreams(valid, invalidFlag));
}

} // namespace TestWebKitAPI
