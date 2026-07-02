/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include <wtf/UniqueArray.h>

namespace TestWebKitAPI {

static unsigned numberOfConstructions { 0 };
class NonTrivialDestructor {
public:
    NonTrivialDestructor()
    {
        numberOfConstructions++;
    }

    ~NonTrivialDestructor()
    {
        (*m_log)++;
    }

    void setLog(unsigned* log)
    {
        m_log = log;
    }

private:
    unsigned* m_log { nullptr };
};

TEST(WTF_UniqueArray, NonTrivialDestructor)
{
    unsigned size = 42;
    unsigned log = 0;
    {
        EXPECT_EQ(0U, log);
        auto array = makeUniqueArray<NonTrivialDestructor>(size);
        EXPECT_EQ(0U, log);
        EXPECT_EQ(size, numberOfConstructions);
        for (unsigned i = 0; i < size; ++i)
            array[i].setLog(&log);
    }
    EXPECT_EQ(log, size);
}

TEST(WTF_UniqueArray, TrivialDestructor)
{
    unsigned size = 42;
    {
        auto array = makeUniqueArray<unsigned>(size);
        for (unsigned i = 0; i < size; ++i) {
            EXPECT_EQ(0U, array[i]) << i;
            array[i] = 42;
        }
        for (unsigned i = 0; i < size; ++i)
            EXPECT_EQ(42U, array[i]);
    }
}

} // namespace TestWebKitAPI
