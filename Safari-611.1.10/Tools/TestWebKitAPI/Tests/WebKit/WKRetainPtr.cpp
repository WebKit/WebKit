/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI

#include "PlatformUtilities.h"
#include <wtf/HashMap.h>

namespace TestWebKitAPI {

TEST(WebKit, WKRetainPtr)
{
    WKRetainPtr<WKStringRef> string1 = adoptWK(WKStringCreateWithUTF8CString("a"));
    WKRetainPtr<WKStringRef> string2 = adoptWK(WKStringCreateWithUTF8CString("a"));
    WKRetainPtr<WKStringRef> string3 = adoptWK(WKStringCreateWithUTF8CString("a"));
    WKRetainPtr<WKStringRef> string4 = adoptWK(WKStringCreateWithUTF8CString("a"));

    HashMap<WKRetainPtr<WKStringRef>, int> map;

    map.set(string2, 2);
    map.set(string1, 1);

    EXPECT_TRUE(map.contains(string1));
    EXPECT_TRUE(map.contains(string2));
    EXPECT_FALSE(map.contains(string3));
    EXPECT_FALSE(map.contains(string4));

    EXPECT_EQ(1, map.get(string1));
    EXPECT_EQ(2, map.get(string2));
}

} // namespace TestWebKitAPI

#endif
