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

#include <wtf/InlineWeakPtr.h>
#include <wtf/RefCountedWithInlineWeakPtr.h>
#include <wtf/RefPtr.h>

namespace {

class ObjectThatRefsItselfDuringDestruction final : public RefCountedWithInlineWeakPtr<ObjectThatRefsItselfDuringDestruction> {
public:
    static Ref<ObjectThatRefsItselfDuringDestruction> create(RefPtr<ObjectThatRefsItselfDuringDestruction>* escapeTo)
    {
        return createRefCountedWithInlineWeakPtr<ObjectThatRefsItselfDuringDestruction>(escapeTo);
    }

    explicit ObjectThatRefsItselfDuringDestruction(RefPtr<ObjectThatRefsItselfDuringDestruction>* escapeTo)
        : m_escapeTo(escapeTo)
    {
    }

    ~ObjectThatRefsItselfDuringDestruction()
    {
        // Take a strong reference during destruction. If m_escapeTo is null,
        // it's transient (dropped before this destructor returns); otherwise
        // it escapes and should trip RELEASE_ASSERT(m_strongCount == 1).
        RefPtr<ObjectThatRefsItselfDuringDestruction> strong(this);
        if (m_escapeTo)
            *m_escapeTo = WTF::move(strong);
    }

private:
    RefPtr<ObjectThatRefsItselfDuringDestruction>* m_escapeTo { nullptr };
};

} // namespace

namespace TestWebKitAPI {

TEST(WTF_RefCountedWithInlineWeakPtr, TransientRefDuringDestruction)
{
    unsigned weakCountAfterDeref = 0;
    {
        InlineWeakPtr<ObjectThatRefsItselfDuringDestruction> weak;
        {
            auto object = ObjectThatRefsItselfDuringDestruction::create(nullptr);
            weak = object.ptr();
            EXPECT_EQ(weak.get(), object.ptr());
        }
        // The transient ref()/deref() inside ~T() must not re-enter destruction,
        // and the strong count must reach zero afterward so the weak pointer clears.
        EXPECT_EQ(weak.get(), nullptr);
        weakCountAfterDeref = 1;
    }
    EXPECT_EQ(weakCountAfterDeref, 1u); // reached without crashing
}

#if !defined(NDEBUG) || defined(GTEST_HAS_DEATH_TEST)
TEST(WTF_RefCountedWithInlineWeakPtrDeathTest, RefEscapesDuringDestruction)
{
    auto shouldCrash = [] {
        RefPtr<ObjectThatRefsItselfDuringDestruction> escaped;
        {
            auto object = ObjectThatRefsItselfDuringDestruction::create(&escaped);
        }
        // Not reached: derefSlowCase() runs ~T(), then RELEASE_ASSERTs that no
        // strong reference escaped.
    };

    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}
#endif

} // namespace TestWebKitAPI
