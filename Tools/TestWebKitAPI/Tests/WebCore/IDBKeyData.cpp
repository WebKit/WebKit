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

#include "Helpers/Test.h"
#include <WebCore/IDBKeyData.h>
#include <wtf/StdSet.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(IDBKeyData, NullAndInvalidAreNotEquivalentInvalidKey)
{
    IDBKeyData null;
    IDBKeyData invalid(IDBKeyData::Invalid { });

    EXPECT_TRUE(null.isNull());
    EXPECT_FALSE(invalid.isNull());
    EXPECT_EQ(null.type(), IndexedDB::KeyType::Invalid);
    EXPECT_EQ(invalid.type(), IndexedDB::KeyType::Invalid);

    // operator== and operator<=> must agree: these are not equal.
    EXPECT_NE(null, invalid);
    EXPECT_TRUE(is_neq(null <=> invalid));

    // Reflexivity.
    EXPECT_EQ(null, null);
    EXPECT_EQ(invalid, invalid);
}

TEST(IDBKeyData, NullStringAndEmptyStringAreNotEquivalentStringKey)
{
    IDBKeyData nullStr(String { });
    IDBKeyData emptyStr(emptyString());

    EXPECT_TRUE(std::get<String>(nullStr.value()).isNull());
    EXPECT_FALSE(std::get<String>(emptyStr.value()).isNull());

    // operator== and operator<=> must agree: these are not equal.
    EXPECT_NE(nullStr, emptyStr);
    EXPECT_TRUE(is_neq(nullStr <=> emptyStr));

    // Two null-string keys are equal.
    IDBKeyData nullStr2(String { });
    EXPECT_EQ(nullStr, nullStr2);

    // Two empty-string keys are equal.
    IDBKeyData emptyStr2(emptyString());
    EXPECT_EQ(emptyStr, emptyStr2);
}

TEST(IDBKeyData, SetDistinguishesNullAndInvalid)
{
    IDBKeyData null;
    IDBKeyData invalid(IDBKeyData::Invalid { });

    IDBKeyDataSet set;
    set.insert(null);
    set.insert(invalid);
    EXPECT_EQ(set.size(), 2u);
}

TEST(IDBKeyData, SetDistinguishesNullStringAndEmptyString)
{
    IDBKeyData nullStr(String { });
    IDBKeyData emptyStr(emptyString());

    IDBKeyDataSet set;
    set.insert(nullStr);
    set.insert(emptyStr);
    EXPECT_EQ(set.size(), 2u);
}

} // namespace TestWebKitAPI
