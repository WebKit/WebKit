/*
 * Copyright (C) 2016, 2018, 2020 Igalia S.L.
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

// /proc filesystems are directly maintained by the kernel.
// On open the kernel will provide the process a static copy of the data if the
// data in question is dynamically changing.
static const char* s_procMeminfo = "/proc/meminfo";
static const char* s_procZoneinfo = "/proc/zoneinfo";
static const char* s_procSelfCgroup = "/proc/self/cgroup";
static const unsigned maxCgroupPath = 4096; // PATH_MAX = 4096 from (Linux) include/uapi/linux/limits.h

#define CGROUP_V2_HIERARCHY 0
#define CGROUP_NAME_BUFFER_SIZE 40
#define MEMINFO_TOKEN_BUFFER_SIZE 50
#define STRINGIFY_EXPANDED(val) #val
#define STRINGIFY(val) STRINGIFY_EXPANDED(val)
#define ZONEINFO_TOKEN_BUFFER_SIZE 128

// The lowWatermark is the sum of the low watermarks across all zones as the
// MemAvailable info was implemented in /proc/meminfo since version 3.14 of the
// kernel (added by commit 34e431b0a, source git.kernel.org):
//
// MemAvailable: An estimate of how much memory is available for starting new
//               applications, without swapping. Calculated from MemFree,
//               SReclaimable, the size of the file LRU lists, and the low
//               watermarks in each zone.
//               The estimate takes into account that the system needs some
//               page cache to function well, and that not all reclaimable
//               slab will be reclaimable, due to items being in use. The
//               impact of those factors will vary from system to system.
//
// The fscanf() reads the input stream file until the argument list passed as
// parameters is successfully filled.
//
// In our immplemetation the `while (!feof(zoneInfoFile))` loop follows the next
// logic:
//
// - the first `fscanf(zoneInfoFile, " Node %*u, zone %...[^\n]\n", buffer);`
//   iterates the `Node` sections.
// - Then, when we found a Normal node, we start to read each single
//   `fscanf(zoneInfoFile, "%...s", buffer);` until find a `low` token.
// - We read the next token which is the actual `low` value and we add it to the
//   `sumLow` summation.
//
// The second fscanf() reads tokens one by one because the format of each row is
// not homogeneous (2, 3 or 6 values):
//
//   Node 0, zone   Normal
//     pages free     27303
//           min      20500
//           low      24089
//           high     27678
//           spanned  3414016
//           present  3414016
//           managed  3337293
//           protection: (0, 0, 0, 0, 0)
static size_t lowWatermarkPages(FILE* zoneInfoFile)
{
    size_t low = 0;
    size_t sumLow = 0;
    char buffer[ZONEINFO_TOKEN_BUFFER_SIZE + 1];
    bool inNormalZone = false;

    if (!zoneInfoFile || fseek(zoneInfoFile, 0, SEEK_SET))
        return notSet;

    while (!feof(zoneInfoFile)) {
        int r;
        r = fscanf(zoneInfoFile, " Node %*u, zone %" STRINGIFY(ZONEINFO_TOKEN_BUFFER_SIZE) "[^\n]\n", buffer);
        if (r == 2 && !strcmp(buffer, "Normal"))
            inNormalZone = true;
        r = fscanf(zoneInfoFile, "%" STRINGIFY(ZONEINFO_TOKEN_BUFFER_SIZE) "s", buffer);
        if (r == 1 && inNormalZone && !strcmp(buffer, "low")) {
            r = fscanf(zoneInfoFile, "%zu", &low);
            if (r == 1) {
                sumLow += low;
                continue;
            }
        }
    }
    return sumLow;
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
static size_t calculateMemoryAvailable(size_t memoryFree, size_t activeFile, size_t inactiveFile, size_t slabReclaimable, FILE* zoneInfoFile)
{
    if (memoryFree == notSet || activeFile == notSet || inactiveFile == notSet || slabReclaimable == notSet)
        return notSet;

    size_t lowWatermark = lowWatermarkPages(zoneInfoFile);
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

FILE* getCgroupFile(CString cgroupControllerName, CString cgroupControllerPath, CString cgroupFileName)
{
    char cgroupPath[maxCgroupPath];
    snprintf(cgroupPath, maxCgroupPath, s_cgroupMemoryPath, cgroupControllerName.data(), cgroupControllerPath.data(), cgroupFileName.data());
    LOG_VERBOSE(MemoryPressure, "Open: %s", cgroupPath);
    FILE* file = fopen(cgroupPath, "r");
    if (file)
        setbuf(file, nullptr);
    return file;
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
static CString getCgroupControllerPath(FILE* cgroupControllerFile, const char* controllerName)
{
    if (!cgroupControllerFile || fseek(cgroupControllerFile, 0, SEEK_SET))
        return CString();

    CString cgroupMemoryControllerPath;
    while (!feof(cgroupControllerFile)) {
        unsigned hierarchyId;
        char name[CGROUP_NAME_BUFFER_SIZE + 1];
        char path[maxCgroupPath + 1];
        name[0] = path[0] = '\0';
        int scanResult = fscanf(cgroupControllerFile, "%u:", &hierarchyId);
        if (scanResult != 1)
            return CString();
        if (hierarchyId == CGROUP_V2_HIERARCHY) {
            scanResult = fscanf(cgroupControllerFile, ":%" STRINGIFY(PATH_MAX) "[^\n]", path);
            if (scanResult != 1)
                return CString();
        } else {
            scanResult = fscanf(cgroupControllerFile, "%" STRINGIFY(CGROUP_NAME_BUFFER_SIZE) "[^:]:%" STRINGIFY(PATH_MAX) "[^\n]", name, path);
            if (scanResult != 2)
                return CString();
        }
        if (!strcmp(name, controllerName)) {
            cgroupMemoryControllerPath = CString(path);
            LOG_VERBOSE(MemoryPressure, "memoryControllerName - %s namespace (hierarchy: %d): %s", controllerName, hierarchyId, cgroupMemoryControllerPath.data());
            return cgroupMemoryControllerPath;
        }
        if (!strcmp(name, "name=systemd")) {
            cgroupMemoryControllerPath = CString(path);
            LOG_VERBOSE(MemoryPressure, "memoryControllerName - systemd namespace (hierarchy: %d): %s", hierarchyId, cgroupMemoryControllerPath.data());
            return cgroupMemoryControllerPath;
        }
        if (!strcmp(name, "")) {
            cgroupMemoryControllerPath = CString(path);
            LOG_VERBOSE(MemoryPressure, "memoryControllerName - empty namespace (hierarchy: %d): %s", hierarchyId, cgroupMemoryControllerPath.data());
            return cgroupMemoryControllerPath;
        }
    }
    return CString();
}


static int systemMemoryUsedAsPercentage(FILE* memInfoFile, FILE* zoneInfoFile, CGroupMemoryController* memoryController)
{
    if (!memInfoFile || fseek(memInfoFile, 0, SEEK_SET))
        return -1;

    size_t memoryAvailable, memoryTotal, memoryFree, activeFile, inactiveFile, slabReclaimable;
    memoryAvailable = memoryTotal = memoryFree = activeFile = inactiveFile = slabReclaimable = notSet;

    while (!feof(memInfoFile)) {
        char token[MEMINFO_TOKEN_BUFFER_SIZE + 1] = { 0 };
        size_t amount;
        if (fscanf(memInfoFile, "%" STRINGIFY(MEMINFO_TOKEN_BUFFER_SIZE) "s%zukB", token, &amount) != 2)
            continue;
        if (!strcmp(token, "MemTotal:"))
            memoryTotal = amount;
        else if (!strcmp(token, "MemFree:"))
            memoryFree = amount;
        else if (!strcmp(token, "MemAvailable:"))
            memoryAvailable = amount;
        else if (!strcmp(token, "Active(file):"))
            activeFile = amount;
        else if (!strcmp(token, "Inactive(file):"))
            inactiveFile = amount;
        else if (!strcmp(token, "SReclaimable:"))
            slabReclaimable = amount;

        if (memoryTotal != notSet && memoryFree != notSet && activeFile != notSet && inactiveFile != notSet && slabReclaimable != notSet)
            break;
    }

    if (!memoryTotal || memoryTotal == notSet)
        return -1;

    if (memoryAvailable == notSet) {
        memoryAvailable = calculateMemoryAvailable(memoryFree, activeFile, inactiveFile, slabReclaimable, zoneInfoFile);
        if (memoryAvailable == notSet)
            return -1;
    }

    if (memoryAvailable > memoryTotal)
        return -1;

    int memoryUsagePercentage = ((memoryTotal - memoryAvailable) * 100) / memoryTotal;
    LOG_VERBOSE(MemoryPressure, "MemoryPressureMonitor::memory: real (memory total=%zu MB) (memory available=%zu MB) (memory usage percentage=%d MB)", memoryTotal, memoryAvailable, memoryUsagePercentage);
    if (memoryController->isActive()) {
        memoryTotal = memoryController->getMemoryTotalWithCgroup();
        size_t memoryUsage = memoryController->getMemoryUsageWithCgroup();
        if (memoryTotal != notSet && memoryUsage != notSet) {
            int memoryUsagePercentageWithCgroup = 100 * ((float) memoryUsage / (float) memoryTotal);
            LOG_VERBOSE(MemoryPressure, "MemoryPressureMonitor::memory: cgroup (memory total=%zu bytes) (memory usage=%zu bytes) (memory usage percentage=%d bytes)", memoryTotal, memoryUsage, memoryUsagePercentageWithCgroup);
            if (memoryUsagePercentageWithCgroup > memoryUsagePercentage)
                memoryUsagePercentage = memoryUsagePercentageWithCgroup;
        }
    }
    LOG_VERBOSE(MemoryPressure, "MemoryPressureMonitor::memory: memoryUsagePercentage (%d)", memoryUsagePercentage);
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

struct FileHandleDeleter {
    void operator()(FILE* f) { fclose(f); }
};

using FileHandle = std::unique_ptr<FILE, FileHandleDeleter>;

static bool tryOpeningForUnbufferedReading(FileHandle& handle, const char* filePath)
{
    // Check whether the file handle is already valid.
    if (handle)
        return true;

    // Else, try opening it and disable buffering after opening.
    if (auto* f = fopen(filePath, "r")) {
        setbuf(f, nullptr);
        handle.reset(f);
        return true;
    }

    // Could not produce a valid handle.
    return false;
}

void MemoryPressureMonitor::start()
{
    if (m_started)
        return;

    m_started = true;

    Thread::create("MemoryPressureMonitor", [] {
        FileHandle memInfoFile, zoneInfoFile, cgroupControllerFile;
        CGroupMemoryController memoryController = CGroupMemoryController();
        Seconds pollInterval = s_maxPollingInterval;
        while (true) {
            sleep(pollInterval);

            // Cannot operate without this one, retry opening on the next iteration after sleeping.
            if (!tryOpeningForUnbufferedReading(memInfoFile, s_procMeminfo))
                continue;

            // The monitor can work without these two, but it will be more precise if thy are eventually opened: keep trying.
            tryOpeningForUnbufferedReading(zoneInfoFile, s_procZoneinfo);
            tryOpeningForUnbufferedReading(cgroupControllerFile, s_procSelfCgroup);

            CString cgroupMemoryControllerPath = getCgroupControllerPath(cgroupControllerFile.get(), "memory");
            memoryController.setMemoryControllerPath(cgroupMemoryControllerPath);
            int usedPercentage = systemMemoryUsedAsPercentage(memInfoFile.get(), zoneInfoFile.get(), &memoryController);
            if (usedPercentage == -1) {
                WTFLogAlways("Failed to get the memory usage");
                pollInterval = s_maxPollingInterval;
                continue;
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

void CGroupMemoryController::setMemoryControllerPath(CString memoryControllerPath)
{
    if (memoryControllerPath == m_cgroupMemoryControllerPath)
        return;

    m_cgroupMemoryControllerPath = memoryControllerPath;
    disposeMemoryController();

    m_cgroupV2MemoryCurrentFile = getCgroupFile("/", memoryControllerPath, CString("memory.current"));
    m_cgroupV2MemoryMemswMaxFile = getCgroupFile("/", memoryControllerPath, CString("memory.memsw.max"));
    m_cgroupV2MemoryMaxFile = getCgroupFile("/", memoryControllerPath, CString("memory.max"));
    m_cgroupV2MemoryHighFile = getCgroupFile("/", memoryControllerPath, CString("memory.high"));

    m_cgroupMemoryMemswLimitInBytesFile = getCgroupFile("memory", memoryControllerPath, CString("memory.memsw.limit_in_bytes"));
    m_cgroupMemoryLimitInBytesFile = getCgroupFile("memory", memoryControllerPath, CString("memory.limit_in_bytes"));
    m_cgroupMemoryUsageInBytesFile = getCgroupFile("memory", memoryControllerPath, CString("memory.usage_in_bytes"));
}

void CGroupMemoryController::disposeMemoryController()
{
    if (m_cgroupMemoryMemswLimitInBytesFile)
        fclose(m_cgroupMemoryMemswLimitInBytesFile);
    if (m_cgroupMemoryLimitInBytesFile)
        fclose(m_cgroupMemoryLimitInBytesFile);
    if (m_cgroupMemoryUsageInBytesFile)
        fclose(m_cgroupMemoryUsageInBytesFile);

    if (m_cgroupV2MemoryMemswMaxFile)
        fclose(m_cgroupV2MemoryMemswMaxFile);
    if (m_cgroupV2MemoryMaxFile)
        fclose(m_cgroupV2MemoryMaxFile);
    if (m_cgroupV2MemoryHighFile)
        fclose(m_cgroupV2MemoryHighFile);
}

size_t CGroupMemoryController::getCgroupFileValue(FILE *file)
{
    if (!file || fseek(file, 0, SEEK_SET))
        return notSet;

    size_t value;
    return (fscanf(file, "%zu", &value) == 1) ? value : notSet;
}

size_t CGroupMemoryController::getMemoryTotalWithCgroup()
{
    size_t value = notSet;

    // Check memory limits in cgroupV2
    value = getCgroupFileValue(m_cgroupV2MemoryMemswMaxFile);
    if (value != notSet)
        return value;

    value = getCgroupFileValue(m_cgroupV2MemoryMaxFile);
    size_t valueHigh = getCgroupFileValue(m_cgroupV2MemoryHighFile);
    if (value != notSet && valueHigh != notSet) {
        value = std::min(value, valueHigh);
        return value;
    }
    if (valueHigh != notSet)
        return valueHigh;
    if (value != notSet)
        return value;

    // Check memory limits in cgroupV1
    value = getCgroupFileValue(m_cgroupMemoryMemswLimitInBytesFile);
    if (value != notSet)
        return value;

    value = getCgroupFileValue(m_cgroupMemoryLimitInBytesFile);
    if (value != notSet)
        return value;

    return value;
}

size_t CGroupMemoryController::getMemoryUsageWithCgroup()
{
    size_t value = notSet;

    // Check memory limits in cgroupV2
    value = getCgroupFileValue(m_cgroupV2MemoryCurrentFile);
    if (value != notSet)
        return value;

    // Check memory limits in cgroupV1
    value = getCgroupFileValue(m_cgroupMemoryUsageInBytesFile);
    if (value != notSet)
        return value;

    return notSet;
}

} // namespace WebKit

#endif // OS(LINUX)
