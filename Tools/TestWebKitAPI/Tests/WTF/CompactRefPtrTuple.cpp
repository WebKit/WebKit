/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "RefLogger.h"
#include "Utilities.h"
#include <wtf/CompactRefPtrTuple.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

TEST(WTF_CompactRefPtrTuple, Basic)
{
    DerivedRefLogger a("a");

    CompactRefPtrTuple<RefLogger, uint16_t> empty;
    EXPECT_EQ(nullptr, empty.pointer());
    EXPECT_EQ(0, empty.type());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setPointer(&a);
        EXPECT_EQ(&a, ptr.pointer());
        EXPECT_EQ(&a.name, &ptr.pointer()->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setPointer(&a);
        EXPECT_EQ(&a, ptr.pointer());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setPointer(&a);
        EXPECT_EQ(&a, ptr.pointer());
        ptr.setPointer(nullptr);
        EXPECT_EQ(nullptr, ptr.pointer());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setType(0xffff);
        ptr.setPointer(&a);
        EXPECT_EQ(&a, ptr.pointer());
        EXPECT_EQ(&a.name, &ptr.pointer()->name);
        EXPECT_EQ(ptr.type(), 0xFFFF);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setType(0xffff);
        ptr.setPointer(&a);
        EXPECT_EQ(&a, ptr.pointer());
        EXPECT_EQ(ptr.type(), 0xFFFF);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setType(0xffff);
        ptr.setPointer(&a);
        EXPECT_EQ(&a, ptr.pointer());
        ptr.setPointer(nullptr);
        EXPECT_EQ(nullptr, ptr.pointer());
        EXPECT_EQ(ptr.type(), 0xFFFF);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setPointer(&a);
        ptr.setType(0xffff);
        EXPECT_EQ(&a, ptr.pointer());
        EXPECT_EQ(&a.name, &ptr.pointer()->name);
        EXPECT_EQ(ptr.type(), 0xFFFF);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setPointer(&a);
        ptr.setType(0xffff);
        EXPECT_EQ(&a, ptr.pointer());
        EXPECT_EQ(ptr.type(), 0xFFFF);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtrTuple<RefLogger, uint16_t> ptr;
        ptr.setPointer(&a);
        ptr.setType(0xffff);
        EXPECT_EQ(&a, ptr.pointer());
        ptr.setPointer(nullptr);
        EXPECT_EQ(nullptr, ptr.pointer());
        EXPECT_EQ(ptr.type(), 0xFFFF);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI
