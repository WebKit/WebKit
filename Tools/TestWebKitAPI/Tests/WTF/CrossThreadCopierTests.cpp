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

#include "Helpers/Test.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/URL.h>
#include <wtf/Variant.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_CrossThreadCopier, CopyLVString)
{
    RefPtr<StringImpl> original = StringImpl::create("1234"_s);
    auto copy = crossThreadCopy(original);
    EXPECT_TRUE(original.get()->hasOneRef());
    EXPECT_TRUE(copy.get()->hasOneRef());
    EXPECT_FALSE(original.get() == copy.get());
}

TEST(WTF_CrossThreadCopier, MoveRVString)
{
    RefPtr<StringImpl> original = StringImpl::create("1234"_s);
    auto copy = crossThreadCopy(WTF::move(original));
    EXPECT_TRUE(copy.get()->hasOneRef());
    EXPECT_NULL(original.get());
}

TEST(WTF_CrossThreadCopier, CopyRVStringHavingTwoRef)
{
    RefPtr<StringImpl> original = StringImpl::create("1234"_s);
    RefPtr<StringImpl> original2 { original };
    auto copy = crossThreadCopy(WTF::move(original));
    EXPECT_EQ(original.get()->refCount(), 2u);
    EXPECT_FALSE(original.get() == copy.get());
    EXPECT_TRUE(copy.get()->hasOneRef());
}

TEST(WTF_CrossThreadCopier, CopyLVOptionalString)
{
    std::optional<RefPtr<StringImpl>> original { "1234"_s };
    auto copy = crossThreadCopy(original);
    EXPECT_TRUE(original->get()->hasOneRef());
    EXPECT_TRUE(copy->get()->hasOneRef());
    EXPECT_FALSE(original->get() == copy->get());
}

TEST(WTF_CrossThreadCopier, MoveRVOptionalString)
{
    std::optional<RefPtr<StringImpl>> original { "1234"_s };
    auto copy = crossThreadCopy(WTF::move(original));
    EXPECT_TRUE(copy->get()->hasOneRef());
    EXPECT_NULL(original->get());
}

TEST(WTF_CrossThreadCopier, CopyRVOptionalStringHavingTwoRef)
{
    RefPtr<StringImpl> string = StringImpl::create("1234"_s);
    std::optional<RefPtr<StringImpl>> original { string };
    auto copy = crossThreadCopy(original);
    EXPECT_EQ(original->get()->refCount(), 2u);
    EXPECT_FALSE(original->get() == copy->get());
    EXPECT_TRUE(copy->get()->hasOneRef());
}

TEST(WTF_CrossThreadCopier, Pair)
{
    std::pair pair1 { "foo"_str, "bar"_str };
    auto* firstStringImpl = pair1.first.get();
    auto* secondStringImpl = pair1.second.get();
    auto copy = crossThreadCopy(pair1);
    EXPECT_EQ(copy, pair1);
    EXPECT_NE(copy.first.get(), firstStringImpl);
    EXPECT_NE(copy.second.get(), secondStringImpl);

    std::pair pair2 { "foo"_str, "bar"_str };
    firstStringImpl = pair2.first.get();
    secondStringImpl = pair2.second.get();
    copy = crossThreadCopy(WTF::move(pair2));
    EXPECT_EQ(copy, pair1);
    EXPECT_EQ(copy.first.get(), firstStringImpl);
    EXPECT_EQ(copy.second.get(), secondStringImpl);
    EXPECT_TRUE(!pair2.first);
    EXPECT_TRUE(!pair2.second);
}

TEST(WTF_CrossThreadCopier, Variant)
{
    Variant<RefPtr<StringImpl>, URL> variant;
    variant = "foo"_str;
    auto* impl = std::get<RefPtr<StringImpl>>(variant).get();
    auto copy = crossThreadCopy(variant);
    ASSERT_EQ(copy, variant);
    EXPECT_NE(std::get<RefPtr<StringImpl>>(copy).get(), impl);

    variant = URL { "bar"_str };
    impl = std::get<URL>(variant).string().get();
    copy = crossThreadCopy(variant);
    ASSERT_EQ(copy, variant);
    EXPECT_NE(std::get<URL>(copy).string().get(), impl);

    variant = "foo"_str;
    impl = std::get<RefPtr<StringImpl>>(variant).get();
    copy = crossThreadCopy(WTF::move(variant));
    ASSERT_EQ(std::get<RefPtr<StringImpl>>(copy), "foo"_str);
    EXPECT_EQ(std::get<RefPtr<StringImpl>>(copy).get(), impl);

    variant = URL { "bar"_str };
    impl = std::get<URL>(variant).string().get();
    copy = crossThreadCopy(WTF::move(variant));
    ASSERT_EQ(std::get<URL>(copy), URL { "bar"_str });
    EXPECT_EQ(std::get<URL>(copy).string().get(), impl);
}

TEST(WTF_CrossThreadCopier, UncheckedKeyHashMap)
{
    UncheckedKeyHashMap<UTF8CString, StringImpl*> impls;

    UncheckedKeyHashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> map;
    map.add("foo"_str, "fooValue"_str);
    map.add("bar"_str, "barValue"_str);
    for (auto& [key, value] : map) {
        impls.add(utf8(key), key.get());
        impls.add(utf8(value), value.get());
    }

    auto copy = crossThreadCopy(map);
    EXPECT_EQ(copy, map);
    for (auto& [key, value] : copy) {
        EXPECT_NE(key.get(), impls.get(utf8(key)));
        EXPECT_NE(value.get(), impls.get(utf8(value)));
    }

    auto copy2 = crossThreadCopy(WTF::move(map));
    EXPECT_EQ(copy2, copy);
    EXPECT_TRUE(map.isEmpty());
    for (auto& [key, value] : copy2) {
        EXPECT_EQ(key.get(), impls.get(utf8(key)));
        EXPECT_EQ(value.get(), impls.get(utf8(value)));
    }
}

TEST(WTF_CrossThreadCopier, HashMap)
{
    HashMap<UTF8CString, StringImpl*> impls;

    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> map;
    map.add("foo"_str, "fooValue"_str);
    map.add("bar"_str, "barValue"_str);
    for (auto& [key, value] : map) {
        impls.add(utf8(key), key.get());
        impls.add(utf8(value), value.get());
    }

    auto copy = crossThreadCopy(map);
    EXPECT_EQ(copy, map);
    for (auto& [key, value] : copy) {
        EXPECT_NE(key.get(), impls.get(utf8(key)));
        EXPECT_NE(value.get(), impls.get(utf8(value)));
    }

    auto copy2 = crossThreadCopy(WTF::move(map));
    EXPECT_EQ(copy2, copy);
    EXPECT_TRUE(map.isEmpty());
    for (auto& [key, value] : copy2) {
        EXPECT_EQ(key.get(), impls.get(utf8(key)));
        EXPECT_EQ(value.get(), impls.get(utf8(value)));
    }
}

TEST(WTF_CrossThreadCopier, HashSet)
{
    HashMap<UTF8CString, StringImpl*> impls;

    HashSet<RefPtr<StringImpl>> set;
    set.add("foo"_str);
    set.add("bar"_str);
    for (auto& item : set)
        impls.add(utf8(item), item.get());

    auto copy = crossThreadCopy(set);
    EXPECT_EQ(copy, set);
    for (auto& item : copy)
        EXPECT_NE(item.get(), impls.get(utf8(item)));

    auto copy2 = crossThreadCopy(WTF::move(set));
    EXPECT_EQ(copy2, copy);
    EXPECT_TRUE(set.isEmpty());
    for (auto& item : copy2)
        EXPECT_EQ(item.get(), impls.get(utf8(item)));
}

TEST(WTF_CrossThreadCopier, Optional)
{
    std::optional<RefPtr<StringImpl>> optional;
    optional = "foo"_str;
    auto* impl = optional->get();

    auto copy = crossThreadCopy(optional);
    EXPECT_EQ(copy, optional);
    EXPECT_NE(copy->get(), impl);

    auto copy2 = crossThreadCopy(WTF::move(optional));
    EXPECT_EQ(copy2, copy);
    EXPECT_EQ(copy2->get(), impl);
}

} // namespace TestWebKitAPI
