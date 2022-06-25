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

#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>

namespace WebCore {

class FrameRateMonitor {
public:
    struct LateFrameInfo {
        MonotonicTime frameTime;
        MonotonicTime lastFrameTime;
    };
    using LateFrameCallback = Function<void(LateFrameInfo)>;
    explicit FrameRateMonitor(LateFrameCallback&&);

    void update();

    double observedFrameRate() const { return m_observedFrameRate; }
    size_t frameCount() const { return m_frameCount; }

private:
    LateFrameCallback m_lateFrameCallback;
    Deque<double, 120> m_observedFrameTimeStamps;
    double m_observedFrameRate { 0 };
    uint64_t m_frameCount { 0 };
};

inline FrameRateMonitor::FrameRateMonitor(LateFrameCallback&& callback)
    : m_lateFrameCallback(WTFMove(callback))
{
}

}
