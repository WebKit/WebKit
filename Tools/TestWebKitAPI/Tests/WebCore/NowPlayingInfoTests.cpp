/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <WebCore/NowPlayingInfo.h>

namespace TestWebKitAPI {

static void testEmptyArtwork(const WebCore::NowPlayingInfoArtwork& artwork)
{
    EXPECT_TRUE(artwork.src.isEmpty());
    EXPECT_TRUE(artwork.mimeType.isEmpty());
    EXPECT_EQ(nullptr, artwork.image.get());
}

TEST(NowPlayingInfoArtwork, DefaultConstruction)
{
    WebCore::NowPlayingInfoArtwork test;

    testEmptyArtwork(test);
}

TEST(NowPlayingInfoArtwork, ValueConstruction)
{
    WebCore::NowPlayingInfoArtwork test { "http://artwork.com/how_so_pretty.jpeg"_s, "image/jpeg"_s, nullptr };

    EXPECT_EQ("http://artwork.com/how_so_pretty.jpeg"_s, test.src);
    EXPECT_EQ("image/jpeg"_s, test.mimeType);
}

TEST(NowPlayingInfoArtwork, OperatorEqual)
{
    WebCore::NowPlayingInfoArtwork test1 { "http://artwork.com/how_so_pretty.jpeg"_s, "image/jpeg"_s, nullptr };
    WebCore::NowPlayingInfoArtwork test2 { "http://artwork.com/how_so_pretty.jpeg"_s, "image/jpeg"_s, nullptr };

    EXPECT_TRUE(test1 == test2);
    EXPECT_FALSE(test1 != test2);
}

TEST(NowPlayingInfoArtwork, OperatorDifferent)
{
    WebCore::NowPlayingInfoArtwork test1 { "http://artwork.com/how_so_pretty.jpeg"_s, "image/jpeg"_s, nullptr };
    WebCore::NowPlayingInfoArtwork test2 { "http://artwork.com/how_so_visually_challenging.png"_s, "image/png"_s, nullptr };

    EXPECT_FALSE(test1 == test2);
    EXPECT_TRUE(test1 != test2);
}

}
