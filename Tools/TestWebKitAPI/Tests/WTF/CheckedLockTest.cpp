/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <wtf/CheckedLock.h>

#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

namespace {
class MyValue {
public:
    void setValue(int value)
    {
        Locker holdLock { m_lock };
        m_value = value;
    }
    void maybeSetOtherValue(int value)
    {
        if (!m_otherLock.tryLock())
            return;
        Locker holdLock { AdoptLockTag { }, m_otherLock };
        m_otherValue = value;
    }
    // This function can be used to manually check that compile fails.
    template<typename T> void shouldFailCompile(T t)
    {
        m_value = t;
    }
    private:
    CheckedLock m_lock;
    int m_value WTF_GUARDED_BY_LOCK(m_lock) { 77 };
    CheckedLock m_otherLock;
    int m_otherValue WTF_GUARDED_BY_LOCK(m_otherLock) { 88 };
};

}

TEST(WTF_CheckedLock, CheckedLockCompiles)
{
    MyValue v;
    v.setValue(7);
    v.maybeSetOtherValue(34);
}

} // namespace TestWebKitAPI
