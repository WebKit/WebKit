/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace JSC { namespace Wasm {

class Callee;

class CalleeRegistry {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CalleeRegistry);
public:
    static void initialize();
    static CalleeRegistry& singleton();

    Lock& getLock() { return m_lock; }

    void registerCallee(Callee* callee)
    {
        auto locker = holdLock(m_lock);
        m_calleeSet.add(callee);
    }

    void unregisterCallee(Callee* callee)
    {
        auto locker = holdLock(m_lock);
        m_calleeSet.remove(callee);
    }

    const HashSet<Callee*>& allCallees(const AbstractLocker&)
    {
        return m_calleeSet;
    }

    bool isValidCallee(const AbstractLocker&, Callee* callee)
    {
        if (!HashSet<Callee*>::isValidValue(callee))
            return false;
        return m_calleeSet.contains(callee);
    }

    CalleeRegistry() = default;

private:
    Lock m_lock;
    HashSet<Callee*> m_calleeSet;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
