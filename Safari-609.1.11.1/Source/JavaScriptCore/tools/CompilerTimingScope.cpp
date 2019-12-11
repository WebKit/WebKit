/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "CompilerTimingScope.h"

#include "Options.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace JSC {

namespace {

class CompilerTimingScopeState {
    WTF_MAKE_NONCOPYABLE(CompilerTimingScopeState);
    WTF_MAKE_FAST_ALLOCATED;
public:
    CompilerTimingScopeState() { }
    
    Seconds addToTotal(const char* compilerName, const char* name, Seconds duration)
    {
        auto locker = holdLock(lock);
        return totals.add(std::make_pair(compilerName, name), Seconds(0)).iterator->value += duration;
    }
    
private:
    HashMap<std::pair<const char*, const char*>, Seconds> totals;
    Lock lock;
};

CompilerTimingScopeState& compilerTimingScopeState()
{
    static Atomic<CompilerTimingScopeState*> s_state;
    return ensurePointer(s_state, [] { return new CompilerTimingScopeState(); });
}

} // anonymous namespace

CompilerTimingScope::CompilerTimingScope(const char* compilerName, const char* name)
    : m_compilerName(compilerName)
    , m_name(name)
{
    if (Options::logPhaseTimes())
        m_before = MonotonicTime::now();
}

CompilerTimingScope::~CompilerTimingScope()
{
    if (Options::logPhaseTimes()) {
        Seconds duration = MonotonicTime::now() - m_before;
        dataLog(
            "[", m_compilerName, "] ", m_name, " took: ", duration.milliseconds(), " ms ",
            "(total: ", compilerTimingScopeState().addToTotal(m_compilerName, m_name, duration).milliseconds(),
            " ms).\n");
    }
}

} // namespace JSC


