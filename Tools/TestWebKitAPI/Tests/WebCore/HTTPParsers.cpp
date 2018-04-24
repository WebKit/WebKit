/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <WebCore/HTTPParsers.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(HTTPParsers, ParseFromOriginHeader)
{
    EXPECT_TRUE(parseFromOriginHeader("") == FromOriginDisposition::None);
    EXPECT_TRUE(parseFromOriginHeader(" ") == FromOriginDisposition::None);

    EXPECT_TRUE(parseFromOriginHeader("same") == FromOriginDisposition::Same);
    EXPECT_TRUE(parseFromOriginHeader("Same") == FromOriginDisposition::Same);
    EXPECT_TRUE(parseFromOriginHeader("SAME") == FromOriginDisposition::Same);
    EXPECT_TRUE(parseFromOriginHeader(" same ") == FromOriginDisposition::Same);

    EXPECT_TRUE(parseFromOriginHeader("same-site") == FromOriginDisposition::SameSite);
    EXPECT_TRUE(parseFromOriginHeader("Same-Site") == FromOriginDisposition::SameSite);
    EXPECT_TRUE(parseFromOriginHeader("SAME-SITE") == FromOriginDisposition::SameSite);
    EXPECT_TRUE(parseFromOriginHeader(" same-site ") == FromOriginDisposition::SameSite);

    EXPECT_TRUE(parseFromOriginHeader("zame") == FromOriginDisposition::Invalid);
    EXPECT_TRUE(parseFromOriginHeader("samesite") == FromOriginDisposition::Invalid);
    EXPECT_TRUE(parseFromOriginHeader("same site") == FromOriginDisposition::Invalid);
    EXPECT_TRUE(parseFromOriginHeader("same–site") == FromOriginDisposition::Invalid);
    EXPECT_TRUE(parseFromOriginHeader("SAMESITE") == FromOriginDisposition::Invalid);
    EXPECT_TRUE(parseFromOriginHeader("") == FromOriginDisposition::Invalid);
}

} // namespace TestWebKitAPI
