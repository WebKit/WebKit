/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
#include <wtf/TimingScope.h>

#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace WTF {

namespace {

class State {
    WTF_MAKE_NONCOPYABLE(State);
    WTF_MAKE_FAST_ALLOCATED;
public:

    struct CallData {
        Seconds totalDuration;
        unsigned callCount { 0 };
        
        Seconds meanDuration() const { return totalDuration / callCount; }
    };

    State() = default;
    
    const CallData& addToTotal(const char* name, Seconds duration)
    {
        auto locker = holdLock(lock);
        auto& result = totals.add(name, CallData()).iterator->value;
        ++result.callCount;
        result.totalDuration += duration;
        return result;
    }

private:
    HashMap<const char*, CallData> totals;
    Lock lock;
};

State& state()
{
    static Atomic<State*> s_state;
    return ensurePointer(s_state, [] { return new State(); });
}

} // anonymous namespace

void TimingScope::scopeDidEnd()
{
    const auto& data = state().addToTotal(m_name, MonotonicTime::now() - m_startTime);
    if (!(data.callCount % m_logIterationInterval))
        WTFLogAlways("%s: %u calls, mean duration: %.6fms, total duration: %.6fms", m_name, data.callCount, data.meanDuration().milliseconds(), data.totalDuration.milliseconds());
}

} // namespace WebCore
