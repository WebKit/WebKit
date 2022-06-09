/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <WebCore/HTTPHeaderMap.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(HTTPHeaderMap, ComparisonOperator)
{
    HTTPHeaderMap map1;
    HTTPHeaderMap map2;

    map1.add("Cache-Control"_str, "max-age:0"_str);
    map1.add("ETag"_str, "foo"_str);
    map1.add("Not-Common1"_str, "bar"_str);
    map1.add("Not-Common2"_str, "bar"_str);

    map2.add("ETag"_str, "foo"_str);
    map2.add("Cache-Control"_str, "max-age:0"_str);
    map2.add("Not-Common2"_str, "bar"_str);
    map2.add("Not-Common1"_str, "bar"_str);

    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);

    map1.add("Last-Modified"_str, "123455"_str);
    map2.add("Last-Modified"_str, "123456"_str);
    EXPECT_FALSE(map1 == map2);
    EXPECT_TRUE(map1 != map2);

    map1.remove("Last-Modified"_str);
    map2.remove("Last-Modified"_str);
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);

    map1.add("Not-Common3"_str, "bar"_str);
    map1.add("Not-Common3"_str, "baz"_str);
    EXPECT_FALSE(map1 == map2);
    EXPECT_TRUE(map1 != map2);

    map1.remove("Not-Common3"_str);
    map2.remove("Not-Common3"_str);
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);

    map1.add("NOT-COMMON4"_str, "foo");
    map2.add("Not-Common4"_str, "foo");
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);
}

}
