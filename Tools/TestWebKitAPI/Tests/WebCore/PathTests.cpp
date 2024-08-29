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

#include <WebCore/Path.h>
namespace TestWebKitAPI {

using namespace WebCore;

TEST(Path, DefinitelyEqual)
{
    Path path1;
    Path path2;
    const Path emptyPath;

    // Empty vs. empty.
    ASSERT_TRUE(path1.definitelyEqual(path2));

    constexpr auto testRect = FloatRect { 23, 12, 100, 200 };

    // Single segment vs. single segment.
    path1.addRect(testRect);
    path2.addRect(testRect);

    ASSERT_TRUE(path1.definitelyEqual(path1));
    ASSERT_TRUE(path1.definitelyEqual(path2));
    ASSERT_TRUE(path2.definitelyEqual(path1));

    // Single segment vs. impl.
    path1.ensureImplForTesting();
    ASSERT_TRUE(path1.definitelyEqual(path2));
    ASSERT_TRUE(path2.definitelyEqual(path1));

    // Impl vs. impl.
    path2.ensureImplForTesting();
    ASSERT_TRUE(path1.definitelyEqual(path2));
    ASSERT_TRUE(path2.definitelyEqual(path1));

    // Trigger impl. copying
    auto pathCopy = path1;
    ASSERT_TRUE(path1.definitelyEqual(pathCopy));
    ASSERT_TRUE(pathCopy.definitelyEqual(path1));

    // Empty vs. empty impl.
    Path emptyImplPath;
    emptyImplPath.ensureImplForTesting();
    ASSERT_TRUE(emptyPath.definitelyEqual(emptyImplPath));
    ASSERT_TRUE(emptyImplPath.definitelyEqual(emptyPath));
}

TEST(Path, DefinitelyNotEqual)
{
    Path path1;
    Path path2;
    const Path emptyPath;

    constexpr auto testRect1 = FloatRect { 23, 12, 100, 200 };
    constexpr auto testRect2 = FloatRect { 23, 13, 100, 200 };
    path1.addRect(testRect1);

    // Single segment vs. empty.
    ASSERT_FALSE(path1.definitelyEqual(emptyPath));
    ASSERT_FALSE(emptyPath.definitelyEqual(path1));

    // Single segment vs single segment.
    path2.addRect(testRect2);
    ASSERT_FALSE(path1.definitelyEqual(path2));
    ASSERT_FALSE(path2.definitelyEqual(path1));

    // Empty vs impl.
    path1.ensureImplForTesting();
    ASSERT_FALSE(emptyPath.definitelyEqual(path1));
    ASSERT_FALSE(path1.definitelyEqual(emptyPath));

    // Impl vs impl.
    path2.ensureImplForTesting();
    ASSERT_FALSE(path1.definitelyEqual(path2));
    ASSERT_FALSE(path2.definitelyEqual(path1));
}

}
