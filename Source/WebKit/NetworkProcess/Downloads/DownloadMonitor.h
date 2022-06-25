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

#include <WebCore/Timer.h>
#include <wtf/Deque.h>

namespace WebKit {

class Download;

class DownloadMonitor {
    WTF_MAKE_NONCOPYABLE(DownloadMonitor); WTF_MAKE_FAST_ALLOCATED;
public:
    DownloadMonitor(Download&);
    
    void applicationDidEnterBackground();
    void applicationWillEnterForeground();
    void downloadReceivedBytes(uint64_t);
    void timerFired();

private:
    Download& m_download;

    double measuredThroughputRate() const;
    uint32_t testSpeedMultiplier() const;
    
    struct Timestamp {
        MonotonicTime time;
        uint64_t bytesReceived;
    };
    static constexpr size_t timestampCapacity = 10;
    Deque<Timestamp, timestampCapacity> m_timestamps;
    WebCore::Timer m_timer { *this, &DownloadMonitor::timerFired };
    size_t m_interval { 0 };
};

} // namespace WebKit
