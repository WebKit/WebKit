/*
 * Copyright (C) 2012 Koji Ishii <kojiishi@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(OPENTYPE_VERTICAL)

#include "OpenTypeTypes.h"
#include "SharedBuffer.h"
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace WebCore;

namespace {

struct TestTable : OpenType::TableBase {
    OpenType::Fixed version;
    OpenType::Int16 ascender;

    template <typename T> const T* validateOffset(const SharedBuffer& buffer, uint16_t offset) const
    {
        return TableBase::validateOffset<T>(buffer, offset);
    }
};

TEST(OpenTypeVerticalDataTest, ValidateTableTest)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(sizeof(TestTable));
    const TestTable* table = OpenType::validateTable<TestTable>(buffer);
    EXPECT_TRUE(table);

    buffer = SharedBuffer::create(sizeof(TestTable) - 1);
    table = OpenType::validateTable<TestTable>(buffer);
    EXPECT_FALSE(table);

    buffer = SharedBuffer::create(sizeof(TestTable) + 1);
    table = OpenType::validateTable<TestTable>(buffer);
    EXPECT_TRUE(table);
}

TEST(OpenTypeVerticalDataTest, ValidateOffsetTest)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(sizeof(TestTable));
    const TestTable* table = OpenType::validateTable<TestTable>(buffer);
    ASSERT_TRUE(table);

    // Test overflow
    EXPECT_FALSE(table->validateOffset<uint8_t>(*buffer, -1));

    // uint8_t is valid for all offsets
    for (uint16_t offset = 0; offset < sizeof(TestTable); offset++)
        EXPECT_TRUE(table->validateOffset<uint8_t>(*buffer, offset));
    EXPECT_FALSE(table->validateOffset<uint8_t>(*buffer, sizeof(TestTable)));
    EXPECT_FALSE(table->validateOffset<uint8_t>(*buffer, sizeof(TestTable) + 1));

    // For uint16_t, the last byte is invalid
    for (uint16_t offset = 0; offset < sizeof(TestTable) - 1; offset++)
        EXPECT_TRUE(table->validateOffset<uint16_t>(*buffer, offset));
    EXPECT_FALSE(table->validateOffset<uint16_t>(*buffer, sizeof(TestTable) - 1));
}

} // namespace

#endif // ENABLE(OPENTYPE_VERTICAL)
