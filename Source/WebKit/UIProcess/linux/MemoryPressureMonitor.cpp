/*
 * Copyright (C) 2016, 2018 Igalia S.L.
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
#include "MemoryPressureMonitor.h"

#if OS(LINUX)

#include "WebProcessPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wtf/Threading.h>
#include <wtf/UniStdExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

static const size_t notSet = static_cast<size_t>(-1);

static const Seconds s_minPollingInterval { 1_s };
static const Seconds s_maxPollingInterval { 5_s };
static const double s_minUsedMemoryPercentageForPolling = 50;
static const double s_maxUsedMemoryPercentageForPolling = 85;
static const int s_memoryPresurePercentageThreshold = 90;
static const int s_memoryPresurePercentageThresholdCritical = 95;
// cgroups.7: The usual place for such mounts is under a tmpfs(5)
// filesystem mounted at /sys/fs/cgroup.
static const char* s_cgroupMemoryPath = "/sys/fs/cgroup/%s/%s/%s";
static const char* s_cgroupController = "/proc/self/cgroup";
static const unsigned maxCgroupPath = 4096; // PATH_MAX = 4096 from (Linux) include/uapi/linux/limits.h

static size_t lowWatermarkPages()
{
    FILE* file = fopen("/proc/zoneinfo", "r");
    if (!file)
        return notSet;

    size_t low = 0;
    bool inZone = false;
    bool foundLow = false;
    char buffer[128];
    while (char* line = fgets(buffer, 128, file)) {
        if (!strncmp(line, "Node", 4)) {
            inZone = true;
            foundLow = false;
            continue;
        }

        char* token = strtok(line, " ");
        if (!token)
            continue;

        if (!strcmp(token, "low")) {
            if (!inZone || foundLow) {
                low = notSet;
                break;
            }
            token = strtok(nullptr, " ");
            if (!token) {
                low = notSet;
                break;
            }
            bool ok = true;
            auto value = toIntegralType<size_t>(token, strlen(token), &ok);
            if (ok)
                low += value;
            foundLow = true;
        }
    }
    fclose(file);

    return low;
}

static inline size_t systemPageSize()
{
    static size_t pageSize = 0;
    if (!pageSize)
        pageSize = sysconf(_SC_PAGE_SIZE);
    return pageSize;
}

// If MemAvailable was not present in /proc/meminfo, because it's an old kernel version,
// we can do the same calculation with the information we have from meminfo and the low watermaks.
// See https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773
static size_t calculateMemoryAvailable(size_t memoryFree, size_t activeFile, size_t inactiveFile, size_t slabReclaimable)
{
    if (memoryFree == notSet || activeFile == notSet || inactiveFile == notSet || slabReclaimable == notSet)
        return notSet;

    size_t lowWatermark = lowWatermarkPages();
    if (lowWatermark == notSet)
        return notSet;

    lowWatermark *= systemPageSize() / KB;

    // Estimate the amount of memory available for userspace allocations, without causing swapping.
    // Free memory cannot be taken below the low watermark, before the system starts swapping.
    lowWatermark *= systemPageSize() / KB;
    size_t memoryAvailable = memoryFree - lowWatermark;

    // Not all the page cache can be freed, otherwise the system will start swapping. Assume at least
    // half of the page cache, or the low watermark worth of cache, needs to stay.
    size_t pageCache = activeFile + inactiveFile;
    pageCache -= std::min(pageCache / 2, lowWatermark);
    memoryAvailable += pageCache;

    // Part of the reclaimable slab consists of items that are in use, and cannot be freed.
    // Cap this estimate at the low watermark.
    memoryAvailable += slabReclaimable - std::min(slabReclaimable / 2, lowWatermark);
    return memoryAvailable;
}


size_t getCgroupFileValue(CString cgroupControllerName, CString cgroupControllerPath, CString cgroupFileName)
{
    char buffer[128];
    char cgroupPath[maxCgroupPath];
    char* token;
    FILE* file;
    bool ok = true;

    snprintf(cgroupPath, maxCgroupPath, s_cgroupMemoryPath, cgroupControllerName.data(), cgroupControllerPath.data(), cgroupFileName.data());
    file = fopen(cgroupPath, "r");
    if (file) {
        token = fgets(buffer, 128, file);
        fclose(file);
        size_t value = toIntegralType<size_t>(token, strlen(token), &ok);
        if (!ok)
            value = notSet;
        return value;
    }
    return notSet;
}

size_t getMemoryTotalWithCgroup(CString memoryControllerName)
{
    size_t value = notSet;

    // Check memory limits in cgroupV2
    value = getCgroupFileValue("/", memoryControllerName, CString("memory.memsw.max"));
    if (value != notSet)
        return value;

    value = getCgroupFileValue("/", memoryControllerName, CString("memory.max"));
    size_t valueHigh = getCgroupFileValue("/", memoryControllerName, CString("memory.high"));
    if (value != notSet && valueHigh != notSet) {
        value = std::min(value, valueHigh);
        return value;
    }
    if (valueHigh != notSet)
        return valueHigh;
    if (value != notSet)
        return value;

    // Check memory limits in cgroupV1
    value = getCgroupFileValue("memory", memoryControllerName, CString("memory.memsw.limit_in_bytes"));
    if (value != notSet)
        return value;

    value = getCgroupFileValue("memory", memoryControllerName, CString("memory.limit_in_bytes"));
    if (value != notSet)
        return value;

    return value;
}

static size_t getMemoryUsageWithCgroup(CString memoryControllerName)
{
    size_t value = notSet;

    // Check memory limits in cgroupV2
    value = getCgroupFileValue("/", memoryControllerName, CString("memory.current"));
    if (value != notSet)
        return value;

    // Check memory limits in cgroupV1
    value = getCgroupFileValue("memory", memoryControllerName, CString("memory.usage_in_bytes"));
    if (value != notSet)
        return value;

    return notSet;
}

// This file describes control groups to which the process with
// the corresponding PID belongs. The displayed information differs
// for cgroups version 1 and version 2 hierarchies.
//
// Example:
//
// $ cat /proc/self/cgroup
// 12:hugetlb:/
// 11:rdma:/
// 10:net_cls,net_prio:/
// 9:devices:/user.slice
// 8:memory:/user.slice
// 7:freezer:/user/psaavedra/0
// 6:pids:/user.slice/user-1000.slice/user@1000.service
// 5:blkio:/user.slice
// 4:perf_event:/
// 3:cpu,cpuacct:/user.slice
// 2:cpuset:/
// 1:name=systemd:/user.slice/user-1000.slice/user@1000.service/gnome-terminal-server.service
// 0::/user.slice/user-1000.slice/user@1000.service/gnome-terminal-server.service
static CString getCgroupControllerPath(const char* controllerName)
{
    CString memoryControllerPath;
    FILE* file = fopen(s_cgroupController, "r");
    if (!file)
        return CString();

    char buffer[maxCgroupPath];
    while (char* line = fgets(buffer, maxCgroupPath, file)) {
        char* token = strtok(line, "\n");
        if (!token)
            break;

        token = strtok(token, ":");
        token = strtok(nullptr, ":");
        if (!strcmp(token, controllerName)) {
            token = strtok(nullptr, ":");
            memoryControllerPath = CString(token);
            fclose(file);
            return memoryControllerPath;
        }
        if (!strncmp(token, "name=systemd", 12)) {
            token = strtok(nullptr, ":");
            memoryControllerPath = CString(token);
        }
    }
    fclose(file);
    return memoryControllerPath;
}


static int systemMemoryUsedAsPercentage()
{
    FILE* file = fopen("/proc/meminfo", "r");
    if (!file)
        return -1;

    size_t memoryAvailable, memoryTotal, memoryFree, activeFile, inactiveFile, slabReclaimable;
    memoryAvailable = memoryTotal = memoryFree = activeFile = inactiveFile = slabReclaimable = notSet;
    char buffer[128];
    while (char* line = fgets(buffer, 128, file)) {
        char* token = strtok(line, " ");
        bool ok = true;
        if (!token)
            break;

        if (!strcmp(token, "MemAvailable:")) {
            if ((token = strtok(nullptr, " "))) {
                memoryAvailable = toIntegralType<size_t>(token, strlen(token), &ok);
                if (!ok)
                    memoryAvailable = notSet;
                if (memoryTotal != notSet)
                    break;
            }
        } else if (!strcmp(token, "MemTotal:")) {
            if ((token = strtok(nullptr, " "))) {
                memoryTotal = toIntegralType<size_t>(token, strlen(token), &ok);
                if (!ok)
                    memoryTotal = notSet;
            } else
                break;
        } else if (!strcmp(token, "MemFree:")) {
            if ((token = strtok(nullptr, " "))) {
                memoryFree = toIntegralType<size_t>(token, strlen(token), &ok);
                if (!ok)
                    memoryFree = notSet;
            } else
                break;
        } else if (!strcmp(token, "Active(file):")) {
            if ((token = strtok(nullptr, " "))) {
                activeFile = toIntegralType<size_t>(token, strlen(token), &ok);
                if (!ok)
                    activeFile = notSet;
            } else
                break;
        } else if (!strcmp(token, "Inactive(file):")) {
            if ((token = strtok(nullptr, " "))) {
                inactiveFile = toIntegralType<size_t>(token, strlen(token), &ok);
                if (!ok)
                    inactiveFile = notSet;
            } else
                break;
        } else if (!strcmp(token, "SReclaimable:")) {
            if ((token = strtok(nullptr, " "))) {
                slabReclaimable = toIntegralType<size_t>(token, strlen(token), &ok);
                if (!ok)
                    slabReclaimable = notSet;
            } else
                break;
        }

        if (memoryTotal != notSet && memoryFree != notSet && activeFile != notSet && inactiveFile != notSet && slabReclaimable != notSet)
            break;
    }
    fclose(file);

    if (!memoryTotal || memoryTotal == notSet)
        return -1;

    if (memoryAvailable == notSet) {
        memoryAvailable = calculateMemoryAvailable(memoryFree, activeFile, inactiveFile, slabReclaimable);
        if (memoryAvailable == notSet)
            return -1;
    }

    if (memoryAvailable > memoryTotal)
        return -1;

    int memoryUsagePercentage = ((memoryTotal - memoryAvailable) * 100) / memoryTotal;
    CString memoryControllerPath = getCgroupControllerPath("memory");
    if (!memoryControllerPath.isNull()) {
        memoryTotal = getMemoryTotalWithCgroup(memoryControllerPath);
        size_t memoryUsage = getMemoryUsageWithCgroup(memoryControllerPath);
        if (memoryTotal != notSet && memoryUsage != notSet) {
            int memoryUsagePercentageWithCgroup = 100 * ((float) memoryUsage / (float) memoryTotal);
            if (memoryUsagePercentageWithCgroup > memoryUsagePercentage)
                memoryUsagePercentage = memoryUsagePercentageWithCgroup;
        }
    }
    return memoryUsagePercentage;
}

static inline Seconds pollIntervalForUsedMemoryPercentage(int usedPercentage)
{
    // Use a different poll interval depending on the currently memory used,
    // to avoid polling too often when the system is under low memory usage.
    if (usedPercentage < s_minUsedMemoryPercentageForPolling)
        return s_maxPollingInterval;

    if (usedPercentage >= s_maxUsedMemoryPercentageForPolling)
        return s_minPollingInterval;

    return s_minPollingInterval + (s_maxPollingInterval - s_minPollingInterval) *
        ((s_maxUsedMemoryPercentageForPolling - usedPercentage) / (s_maxUsedMemoryPercentageForPolling - s_minUsedMemoryPercentageForPolling));
}

MemoryPressureMonitor& MemoryPressureMonitor::singleton()
{
    static NeverDestroyed<MemoryPressureMonitor> memoryMonitor;
    return memoryMonitor;
}

void MemoryPressureMonitor::start()
{
    if (m_started)
        return;

    m_started = true;

    Thread::create("MemoryPressureMonitor", [] {
        Seconds pollInterval = s_maxPollingInterval;
        while (true) {
            sleep(pollInterval);

            int usedPercentage = systemMemoryUsedAsPercentage();
            if (usedPercentage == -1) {
                WTFLogAlways("Failed to get the memory usage");
                break;
            }

            if (usedPercentage >= s_memoryPresurePercentageThreshold) {
                bool isCritical = (usedPercentage >= s_memoryPresurePercentageThresholdCritical);
                for (auto* processPool : WebProcessPool::allProcessPools())
                    processPool->sendMemoryPressureEvent(isCritical);
            }
            pollInterval = pollIntervalForUsedMemoryPercentage(usedPercentage);
        }
    })->detach();
}


} // namespace WebKit

#endif // OS(LINUX)
