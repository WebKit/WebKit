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

#include "Test.h"
#include <variant>
#include <wtf/CrossThreadCopier.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_CrossThreadCopier, CopyLVString)
{
    String original { "1234"_s };
    auto copy = crossThreadCopy(original);
    EXPECT_TRUE(original.impl()->hasOneRef());
    EXPECT_TRUE(copy.impl()->hasOneRef());
    EXPECT_FALSE(original.impl() == copy.impl());
}

TEST(WTF_CrossThreadCopier, MoveRVString)
{
    String original { "1234"_s };
    auto copy = crossThreadCopy(WTFMove(original));
    EXPECT_TRUE(copy.impl()->hasOneRef());
    EXPECT_NULL(original.impl());
}

TEST(WTF_CrossThreadCopier, CopyRVStringHavingTwoRef)
{
    String original { "1234"_s };
    String original2 { original };
    auto copy = crossThreadCopy(WTFMove(original));
    EXPECT_EQ(original.impl()->refCount(), 2u);
    EXPECT_FALSE(original.impl() == copy.impl());
    EXPECT_TRUE(copy.impl()->hasOneRef());
}

TEST(WTF_CrossThreadCopier, CopyLVOptionalString)
{
    std::optional<String> original { "1234"_s };
    auto copy = crossThreadCopy(original);
    EXPECT_TRUE(original->impl()->hasOneRef());
    EXPECT_TRUE(copy->impl()->hasOneRef());
    EXPECT_FALSE(original->impl() == copy->impl());
}

TEST(WTF_CrossThreadCopier, MoveRVOptionalString)
{
    std::optional<String> original { "1234"_s };
    auto copy = crossThreadCopy(WTFMove(original));
    EXPECT_TRUE(copy->impl()->hasOneRef());
    EXPECT_NULL(original->impl());
}

TEST(WTF_CrossThreadCopier, CopyRVOptionalStringHavingTwoRef)
{
    String string { "1234"_s };
    std::optional<String> original { string };
    auto copy = crossThreadCopy(original);
    EXPECT_EQ(original->impl()->refCount(), 2u);
    EXPECT_FALSE(original->impl() == copy->impl());
    EXPECT_TRUE(copy->impl()->hasOneRef());
}

TEST(WTF_CrossThreadCopier, Pair)
{
    std::pair pair1 { "foo"_str, "bar"_str };
    auto* firstStringImpl = pair1.first.impl();
    auto* secondStringImpl = pair1.second.impl();
    auto copy = crossThreadCopy(pair1);
    EXPECT_EQ(copy, pair1);
    EXPECT_NE(copy.first.impl(), firstStringImpl);
    EXPECT_NE(copy.second.impl(), secondStringImpl);

    std::pair pair2 { "foo"_str, "bar"_str };
    firstStringImpl = pair2.first.impl();
    secondStringImpl = pair2.second.impl();
    copy = crossThreadCopy(WTFMove(pair2));
    EXPECT_EQ(copy, pair1);
    EXPECT_EQ(copy.first.impl(), firstStringImpl);
    EXPECT_EQ(copy.second.impl(), secondStringImpl);
    EXPECT_TRUE(pair2.first.isNull());
    EXPECT_TRUE(pair2.second.isNull());
}

TEST(WTF_CrossThreadCopier, Variant)
{
    std::variant<String, URL> variant;
    variant = "foo"_str;
    auto* impl = std::get<String>(variant).impl();
    auto copy = crossThreadCopy(variant);
    ASSERT_EQ(copy, variant);
    EXPECT_NE(std::get<String>(copy).impl(), impl);

    variant = URL { "bar"_str };
    impl = std::get<URL>(variant).string().impl();
    copy = crossThreadCopy(variant);
    ASSERT_EQ(copy, variant);
    EXPECT_NE(std::get<URL>(copy).string().impl(), impl);

    variant = "foo"_str;
    impl = std::get<String>(variant).impl();
    copy = crossThreadCopy(WTFMove(variant));
    ASSERT_EQ(std::get<String>(copy), "foo"_str);
    EXPECT_EQ(std::get<String>(copy).impl(), impl);

    variant = URL { "bar"_str };
    impl = std::get<URL>(variant).string().impl();
    copy = crossThreadCopy(WTFMove(variant));
    ASSERT_EQ(std::get<URL>(copy), URL { "bar"_str });
    EXPECT_EQ(std::get<URL>(copy).string().impl(), impl);
}

TEST(WTF_CrossThreadCopier, HashMap)
{
    HashMap<CString, StringImpl*> impls;

    HashMap<String, String> map;
    map.add("foo"_str, "fooValue"_str);
    map.add("bar"_str, "barValue"_str);
    for (auto& [key, value] : map) {
        impls.add(key.utf8(), key.impl());
        impls.add(value.utf8(), value.impl());
    }

    auto copy = crossThreadCopy(map);
    EXPECT_EQ(copy, map);
    for (auto& [key, value] : copy) {
        EXPECT_NE(key.impl(), impls.get(key.utf8()));
        EXPECT_NE(value.impl(), impls.get(value.utf8()));
    }

    auto copy2 = crossThreadCopy(WTFMove(map));
    EXPECT_EQ(copy2, copy);
    EXPECT_TRUE(map.isEmpty());
    for (auto& [key, value] : copy2) {
        EXPECT_EQ(key.impl(), impls.get(key.utf8()));
        EXPECT_EQ(value.impl(), impls.get(value.utf8()));
    }
}

TEST(WTF_CrossThreadCopier, HashSet)
{
    HashMap<CString, StringImpl*> impls;

    HashSet<String> set;
    set.add("foo"_str);
    set.add("bar"_str);
    for (auto& item : set)
        impls.add(item.utf8(), item.impl());

    auto copy = crossThreadCopy(set);
    EXPECT_EQ(copy, set);
    for (auto& item : copy)
        EXPECT_NE(item.impl(), impls.get(item.utf8()));

    auto copy2 = crossThreadCopy(WTFMove(set));
    EXPECT_EQ(copy2, copy);
    EXPECT_TRUE(set.isEmpty());
    for (auto& item : copy2)
        EXPECT_EQ(item.impl(), impls.get(item.utf8()));
}

TEST(WTF_CrossThreadCopier, Optional)
{
    std::optional<String> optional;
    optional = "foo"_str;
    auto* impl = optional->impl();

    auto copy = crossThreadCopy(optional);
    EXPECT_EQ(copy, optional);
    EXPECT_NE(copy->impl(), impl);

    auto copy2 = crossThreadCopy(WTFMove(optional));
    EXPECT_EQ(copy2, copy);
    EXPECT_EQ(copy2->impl(), impl);
}

} // namespace TestWebKitAPI
