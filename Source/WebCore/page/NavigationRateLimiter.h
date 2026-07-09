/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

// Rate limiter to prevent excessive navigation requests.
class NavigationRateLimiter {
    WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(NavigationRateLimiter);
    WTF_MAKE_NONCOPYABLE(NavigationRateLimiter);
    WTF_MAKE_NONMOVABLE(NavigationRateLimiter);
public:
    NavigationRateLimiter() = default;

    bool navigationAllowed()
    {
        auto currentTime = MonotonicTime::now();

        // Check if we've exceeded the time window and need to reset.
        if (currentTime - m_windowStartTime > m_windowDuration) {
            m_windowStartTime = currentTime;
            m_navigationCount = 0;
            m_limitMessageSent = false;
        }

        // Allow navigation if we're still under the limit.
        if (m_navigationCount < m_maxNavigationsPerWindow) {
            ++m_navigationCount;
            return true;
        }

        return false;
    }

    bool wasReported() const { return m_limitMessageSent; }
    void markReported() { m_limitMessageSent = true; }

    // Testing support
    void setParametersForTesting(unsigned maxNavigations, Seconds duration)
    {
        m_maxNavigationsPerWindow = maxNavigations;
        m_windowDuration = duration;
        resetForTesting();
    }

    void resetForTesting()
    {
        m_windowStartTime = MonotonicTime::now();
        m_navigationCount = 0;
        m_limitMessageSent = false;
    }

private:
    friend class Navigation;

    // Sliding window rate limiter: allows 200 navigations per 10 second window (~20/sec sustained).
    // Chromium uses 200 navigations per 10 seconds (same ~20/sec rate):
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/navigation_rate_limiter.cc
    // Both prevent IPC flooding and stack overflow from recursive navigation patterns.
    unsigned m_maxNavigationsPerWindow { 200 };
    Seconds m_windowDuration { 10_s };

    MonotonicTime m_windowStartTime { MonotonicTime::now() };
    unsigned m_navigationCount { 0 };
    bool m_limitMessageSent { false };
};

} // namespace WebCore
