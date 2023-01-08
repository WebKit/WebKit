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

#include "config.h"
#include "DownloadMonitor.h"

#include "Download.h"
#include "Logging.h"

#define DOWNLOAD_MONITOR_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - DownloadMonitor::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

constexpr uint64_t operator"" _kbps(unsigned long long kilobytesPerSecond)
{
    return kilobytesPerSecond * 1024;
}

struct ThroughputInterval {
    Seconds time;
    uint64_t bytesPerSecond;
};

static const ThroughputInterval throughputIntervals[] = {
    { 1_min, 1_kbps },
    { 5_min, 2_kbps },
    { 10_min, 4_kbps },
    { 15_min, 8_kbps },
    { 20_min, 16_kbps },
    { 25_min, 32_kbps },
    { 30_min, 64_kbps },
    { 45_min, 96_kbps },
    { 60_min, 128_kbps }
};

static Seconds timeUntilNextInterval(size_t currentInterval)
{
    RELEASE_ASSERT(currentInterval + 1 < std::size(throughputIntervals));
    return throughputIntervals[currentInterval + 1].time - throughputIntervals[currentInterval].time;
}

DownloadMonitor::DownloadMonitor(Download& download)
    : m_download(download)
{
}

double DownloadMonitor::measuredThroughputRate() const
{
    uint64_t bytes { 0 };
    for (const auto& timestamp : m_timestamps)
        bytes += timestamp.bytesReceived;
    if (!bytes)
        return 0;
    ASSERT(!m_timestamps.isEmpty());
    Seconds timeDifference = m_timestamps.last().time.secondsSinceEpoch() - m_timestamps.first().time.secondsSinceEpoch();
    double seconds = timeDifference.seconds();
    if (!seconds)
        return 0;
    return bytes / seconds;
}

void DownloadMonitor::downloadReceivedBytes(uint64_t bytesReceived)
{
    if (m_timestamps.size() > timestampCapacity - 1) {
        ASSERT(m_timestamps.size() == timestampCapacity);
        m_timestamps.removeFirst();
    }
    m_timestamps.append({ MonotonicTime::now(), bytesReceived });
}

void DownloadMonitor::applicationWillEnterForeground()
{
    DOWNLOAD_MONITOR_RELEASE_LOG("applicationWillEnterForeground (id = %" PRIu64 ")", m_download.downloadID().toUInt64());
    m_timer.stop();
    m_interval = 0;
}

void DownloadMonitor::applicationDidEnterBackground()
{
    DOWNLOAD_MONITOR_RELEASE_LOG("applicationDidEnterBackground (id = %" PRIu64 ")", m_download.downloadID().toUInt64());
    ASSERT(!m_timer.isActive());
    ASSERT(!m_interval);
    m_timer.startOneShot(throughputIntervals[0].time / testSpeedMultiplier());
}

uint32_t DownloadMonitor::testSpeedMultiplier() const
{
    return m_download.testSpeedMultiplier();
}

void DownloadMonitor::timerFired()
{
    downloadReceivedBytes(0);

    RELEASE_ASSERT(m_interval < std::size(throughputIntervals));
    if (measuredThroughputRate() < throughputIntervals[m_interval].bytesPerSecond) {
        DOWNLOAD_MONITOR_RELEASE_LOG("timerFired: cancelling download (id = %" PRIu64 ")", m_download.downloadID().toUInt64());
        m_download.cancel([](auto&) { }, Download::IgnoreDidFailCallback::No);
    } else if (m_interval + 1 < std::size(throughputIntervals)) {
        DOWNLOAD_MONITOR_RELEASE_LOG("timerFired: sufficient throughput rate (id = %" PRIu64 ")", m_download.downloadID().toUInt64());
        m_timer.startOneShot(timeUntilNextInterval(m_interval++) / testSpeedMultiplier());
    } else
        DOWNLOAD_MONITOR_RELEASE_LOG("timerFired: Download reached threshold to not be terminated (id = %" PRIu64 ")", m_download.downloadID().toUInt64());
}

} // namespace WebKit
