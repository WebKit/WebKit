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

#pragma once

#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/ProcessID.h>
#include <wtf/Spectrum.h>
#include <wtf/StackTrace.h>
#include <wtf/StackShot.h>
#include <wtf/Threading.h>
#include <wtf/WordLock.h>

namespace WTF {

class StackShotProfiler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StackShotProfiler(unsigned numFrames, unsigned framesToSkip, unsigned stacksToReport)
        : m_numFrames(numFrames)
        , m_framesToSkip(framesToSkip)
        , m_stacksToReport(stacksToReport)
    {
        Thread::create("StackShotProfiler", [this] () { run(); });
    }

    // NEVER_INLINE so that framesToSkip is predictable.
    NEVER_INLINE void profile()
    {
        Locker locker { m_lock };
        m_profile.add(StackShot(m_numFrames + m_framesToSkip));
        m_totalCount++;
    }
    
private:
    NO_RETURN void run()
    {
        for (;;) {
            sleep(1_s);
            Locker locker { m_lock };
            auto list = m_profile.buildList();
            dataLog("\nHottest stacks in ", getCurrentProcessID(), ":\n");
            for (size_t i = list.size(), count = 0; i-- && count < m_stacksToReport; count++) {
                auto& entry = list[i];
                dataLog("\nTop #", count + 1, " stack: ", entry.count * 100 / m_totalCount, "%\n");
                StackTrace trace(entry.key.array() + m_framesToSkip, entry.key.size() - m_framesToSkip);
                dataLog(trace);
            }
            dataLog("\n");
        }
    }
    
    WordLock m_lock;
    Spectrum<StackShot, double> m_profile;
    double m_totalCount { 0 };
    unsigned m_numFrames;
    unsigned m_framesToSkip;
    unsigned m_stacksToReport;
};

#define STACK_SHOT_PROFILE(numFrames, framesToSkip, stacksToReport) do { \
    static StackShotProfiler* stackShotProfiler; \
    static std::once_flag stackShotProfilerOnceFlag; \
    std::call_once(stackShotProfilerOnceFlag, [] { stackShotProfiler = new StackShotProfiler(numFrames, framesToSkip, stacksToReport); }); \
    stackShotProfiler->profile(); \
} while (false)

} // namespace WTF

