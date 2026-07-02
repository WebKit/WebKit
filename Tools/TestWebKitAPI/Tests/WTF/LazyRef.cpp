/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "RefLogger.h"
#include <wtf/LazyRef.h>
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

class LazyRefHolder {
public:
    LazyRefHolder(const char* name)
        : m_log(name)
        , m_ref(
            [](LazyRefHolder& holder, auto& ref) {
                ref.set(Ref { holder.m_log });
            })
    {
    }

    DerivedRefLogger& log() { return m_log; }
    RefLogger& ref() { return m_ref.get(*this); }
    RefLogger* refIfExists() { return m_ref.getIfExists(); }

private:
    DerivedRefLogger m_log;
    LazyRef<LazyRefHolder, RefLogger> m_ref;
};

TEST(WTF_LazyRef, Basic)
{
    {
        LazyRefHolder holder("a");
        EXPECT_EQ(holder.refIfExists(), nullptr);
        EXPECT_EQ(&holder.ref(), &holder.log());
        EXPECT_EQ(&holder.ref().name, &holder.log().name);
        EXPECT_EQ(holder.refIfExists(), &holder.log());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        LazyRefHolder holder("a");
    }
    EXPECT_STREQ("" , takeLogStr().c_str());
}

} // namespace TestWebKitAPI
