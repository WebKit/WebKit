/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include <wtf/CrossThreadCopier.h>

#include "Test.h"
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_CrossThreadCopier, CopyLVString)
{
    String original { "1234" };
    auto copy = crossThreadCopy(original);
    EXPECT_TRUE(original.impl()->hasOneRef());
    EXPECT_TRUE(copy.impl()->hasOneRef());
    EXPECT_FALSE(original.impl() == copy.impl());
}

TEST(WTF_CrossThreadCopier, MoveRVString)
{
    String original { "1234" };
    auto copy = crossThreadCopy(WTFMove(original));
    EXPECT_TRUE(copy.impl()->hasOneRef());
    EXPECT_NULL(original.impl());
}

TEST(WTF_CrossThreadCopier, CopyRVStringHavingTwoRef)
{
    String original { "1234" };
    String original2 { original };
    auto copy = crossThreadCopy(WTFMove(original));
    EXPECT_EQ(original.impl()->refCount(), 2);
    EXPECT_FALSE(original.impl() == copy.impl());
    EXPECT_TRUE(copy.impl()->hasOneRef());
}

TEST(WTF_CrossThreadCopier, CopyLVOptionalString)
{
    Optional<String> original { "1234" };
    auto copy = crossThreadCopy(original);
    EXPECT_TRUE(original->impl()->hasOneRef());
    EXPECT_TRUE(copy->impl()->hasOneRef());
    EXPECT_FALSE(original->impl() == copy->impl());
}

TEST(WTF_CrossThreadCopier, MoveRVOptionalString)
{
    Optional<String> original { "1234" };
    auto copy = crossThreadCopy(WTFMove(original));
    EXPECT_TRUE(copy->impl()->hasOneRef());
    EXPECT_NULL(original->impl());
}

TEST(WTF_CrossThreadCopier, CopyRVOptionalStringHavingTwoRef)
{
    String string { "1234" };
    Optional<String> original { string };
    auto copy = crossThreadCopy(original);
    EXPECT_EQ(original->impl()->refCount(), 2);
    EXPECT_FALSE(original->impl() == copy->impl());
    EXPECT_TRUE(copy->impl()->hasOneRef());
}

} // namespace TestWebKitAPI
