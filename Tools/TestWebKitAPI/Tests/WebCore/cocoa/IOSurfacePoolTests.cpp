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

#if HAVE(IOSURFACE)

#include "Test.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/IOSurface.h>
#include <WebCore/IOSurfacePool.h>
#include <wtf/MachSendRight.h>

namespace TestWebKitAPI {
using namespace WebCore;

TEST(IOSurfacePoolTest, TakeSurfaceFindsSurfaceThatIsNoLongerInUse)
{
    auto pool = IOSurfacePool::create();
    IntSize size { 5, 5 };
    auto colorSpace = ColorSpace::SRGB();

    auto surface = IOSurface::create(nullptr, size, colorSpace);
    ASSERT_NE(surface, nullptr);
    IOSurfaceRef expected = surface->surface();

    auto sendRight = surface->createSendRight();
    EXPECT_TRUE(surface->isInUse());

    pool->addSurface(WTF::move(surface));
    EXPECT_EQ(pool->takeSurface(size, colorSpace, IOSurface::Format::BGRA, UseLosslessCompression::No), nullptr);
    sendRight = { };
    auto taken = pool->takeSurface(size, colorSpace, IOSurface::Format::BGRA, UseLosslessCompression::No);
    ASSERT_NE(taken, nullptr);
    EXPECT_EQ(taken->surface(), expected);
}

TEST(IOSurfacePoolTest, TileSizeHintSizesInUseLimit)
{
    auto pool = IOSurfacePool::create();
    pool->setPoolSize(1024 * MB);
    auto defaultLimit = pool->inUseBytesLimitForTesting();

    // Front and back buffers for 4 tiles of the hinted size.
    pool->setTileSizeHint(40 * MB);
    EXPECT_EQ(pool->inUseBytesLimitForTesting(), 320 * MB);

    // Never below the platform default.
    pool->setTileSizeHint(1 * MB);
    EXPECT_EQ(pool->inUseBytesLimitForTesting(), defaultLimit);

    // At most half the pool.
    pool->setTileSizeHint(200 * MB);
    EXPECT_EQ(pool->inUseBytesLimitForTesting(), 512 * MB);
    pool->setPoolSize(256 * MB);
    EXPECT_EQ(pool->inUseBytesLimitForTesting(), std::max<size_t>(defaultLimit, 128 * MB));

    // No hint: the platform default.
    pool->setTileSizeHint(0);
    EXPECT_EQ(pool->inUseBytesLimitForTesting(), defaultLimit);
}

} // namespace TestWebKitAPI

#endif // HAVE(IOSURFACE)
