/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

class NativeCallee;

class NativeCalleeRegistry {
    WTF_MAKE_TZONE_ALLOCATED(NativeCalleeRegistry);
    WTF_MAKE_NONCOPYABLE(NativeCalleeRegistry);
public:
    static void NODELETE initialize();
    static NativeCalleeRegistry& NODELETE singleton();

    Lock& getLock() LIFETIME_BOUND WTF_RETURNS_LOCK(m_lock) { return m_lock; }

    void registerCallee(NativeCallee* callee)
    {
        Locker locker { m_lock };
        auto addResult = m_calleeSet.add(callee);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    template<typename CalleeContainer>
    void registerCallees(const CalleeContainer& callees)
    {
        Locker locker { m_lock };
        for (auto& callee : callees) {
            auto addResult = m_calleeSet.add(callee.ptr());
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
    }

    void unregisterCallee(NativeCallee* callee)
    {
        Locker locker { m_lock };
        m_calleeSet.remove(callee);
    }

    const UncheckedKeyHashSet<NativeCallee*>& allCallees() WTF_REQUIRES_LOCK(m_lock)
    {
        return m_calleeSet;
    }

    bool isValidCallee(NativeCallee* callee)  WTF_REQUIRES_LOCK(m_lock)
    {
        if (!UncheckedKeyHashSet<NativeCallee*>::isValidValue(callee))
            return false;
        return m_calleeSet.contains(callee);
    }

    NativeCalleeRegistry() = default;

private:
    Lock m_lock;
    UncheckedKeyHashSet<NativeCallee*> m_calleeSet WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace JSC
