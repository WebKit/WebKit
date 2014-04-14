/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include <wtf/text/StringView.h>

namespace TestWebKitAPI {

TEST(WTF, StringViewEmptyVsNull)
{
    StringView nullView;
    EXPECT_TRUE(nullView.isNull());
    EXPECT_TRUE(nullView.isEmpty());

    // Test in a boolean context to test operator bool().
    if (nullView)
        FAIL();
    else
        SUCCEED();

    if (!nullView)
        SUCCEED();
    else
        FAIL();

    StringView emptyView = StringView::empty();
    EXPECT_FALSE(emptyView.isNull());
    EXPECT_TRUE(emptyView.isEmpty());

    // Test in a boolean context to test operator bool().
    if (emptyView)
        SUCCEED();
    else
        FAIL();

    if (!emptyView)
        FAIL();
    else
        SUCCEED();

    StringView viewWithCharacters(String("hello"));
    EXPECT_FALSE(viewWithCharacters.isNull());
    EXPECT_FALSE(viewWithCharacters.isEmpty());

    // Test in a boolean context to test operator bool().
    if (viewWithCharacters)
        SUCCEED();
    else
        FAIL();

    if (!viewWithCharacters)
        FAIL();
    else
        SUCCEED();
}

} // namespace TestWebKitAPI
