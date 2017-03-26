/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "B3TimingScope.h"

#if ENABLE(B3_JIT)

#include "B3Common.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace JSC { namespace B3 {

namespace {

class State {
    WTF_MAKE_NONCOPYABLE(State);
    WTF_MAKE_FAST_ALLOCATED;
public:
    State() { }
    
    Seconds addToTotal(const char* name, Seconds duration)
    {
        auto locker = holdLock(lock);
        return totals.add(name, Seconds(0)).iterator->value += duration;
    }
    
private:
    HashMap<const char*, Seconds> totals;
    Lock lock;
};

State& state()
{
    static Atomic<State*> s_state;
    return ensurePointer(s_state, [] { return new State(); });
}

} // anonymous namespace

TimingScope::TimingScope(const char* name)
    : m_name(name)
{
    if (shouldMeasurePhaseTiming())
        m_before = MonotonicTime::now();
}

TimingScope::~TimingScope()
{
    if (shouldMeasurePhaseTiming()) {
        Seconds duration = MonotonicTime::now() - m_before;
        dataLog(
            "[B3] ", m_name, " took: ", duration.milliseconds(), " ms ",
            "(total: ", state().addToTotal(m_name, duration).milliseconds(), " ms).\n");
    }
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

