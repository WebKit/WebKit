/*
 * Copyright (C) 2017, 2020 Igalia S.L.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "ResourceUsageThread.h"

#if ENABLE(RESOURCE_USAGE) && OS(LINUX)

#include "WorkerThread.h"
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/SamplingProfiler.h>
#include <JavaScriptCore/VM.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/Threading.h>
#include <wtf/linux/CurrentProcessMemoryStatus.h>

namespace WebCore {

static float cpuPeriod()
{
    FILE* file = fopen("/proc/stat", "r");
    if (!file)
        return 0;

    static const unsigned statMaxLineLength = 512;
    char buffer[statMaxLineLength + 1];
    char* line = fgets(buffer, statMaxLineLength, file);
    if (!line) {
        fclose(file);
        return 0;
    }

    unsigned long long userTime, niceTime, systemTime, idleTime;
    unsigned long long ioWait, irq, softIrq, steal, guest, guestnice;
    ioWait = irq = softIrq = steal = guest = guestnice = 0;
    int retVal = sscanf(buffer, "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",
        &userTime, &niceTime, &systemTime, &idleTime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
    // We expect 10 values to be matched by sscanf
    if (retVal != 10) {
        fclose(file);
        return 0;
    }


    // Keep parsing if we still don't know cpuCount.
    static unsigned cpuCount = 0;
    if (!cpuCount) {
        while ((line = fgets(buffer, statMaxLineLength, file))) {
            if (strlen(line) > 4 && line[0] == 'c' && line[1] == 'p' && line[2] == 'u')
                cpuCount++;
            else
                break;
        }
    }
    fclose(file);

    if (!cpuCount)
        return 0;

    static unsigned long long previousTotalTime = 0;
    unsigned long long totalTime = userTime + niceTime + systemTime + irq + softIrq + idleTime + ioWait + steal;
    unsigned long long period = totalTime > previousTotalTime ? totalTime - previousTotalTime : 0;
    previousTotalTime = totalTime;
    return static_cast<float>(period) / cpuCount;
}

void ResourceUsageThread::platformSaveStateBeforeStarting()
{
#if ENABLE(SAMPLING_PROFILER)
    m_samplingProfilerThreadID = 0;

    if (auto* profiler = m_vm->samplingProfiler()) {
        if (auto* thread = profiler->thread())
            m_samplingProfilerThreadID = thread->id();
    }
#endif
}

struct ThreadInfo {
    Optional<String> name;
    Optional<float> cpuUsage;
    unsigned long long previousUtime { 0 };
    unsigned long long previousStime { 0 };
};

static HashMap<pid_t, ThreadInfo>& threadInfoMap()
{
    static LazyNeverDestroyed<HashMap<pid_t, ThreadInfo>> map;
    static std::once_flag flag;
    std::call_once(flag, [&] {
        map.construct();
    });
    return map;
}

static bool threadCPUUsage(pid_t id, float period, ThreadInfo& info)
{
    String path = makeString("/proc/self/task/", id, "/stat");
    int fd = open(path.utf8().data(), O_RDONLY);
    if (fd < 0)
        return false;

    static const ssize_t maxBufferLength = BUFSIZ - 1;
    char buffer[BUFSIZ];
    buffer[0] = '\0';

    ssize_t totalBytesRead = 0;
    while (totalBytesRead < maxBufferLength) {
        ssize_t bytesRead = read(fd, buffer + totalBytesRead, maxBufferLength - totalBytesRead);
        if (bytesRead < 0) {
            if (errno != EINTR) {
                close(fd);
                return false;
            }
            continue;
        }

        if (!bytesRead)
            break;

        totalBytesRead += bytesRead;
    }
    close(fd);
    buffer[totalBytesRead] = '\0';

    // Skip tid and name.
    char* position = strchr(buffer, ')');
    if (!position)
        return false;

    if (!info.name) {
        char* name = strchr(buffer, '(');
        if (!name)
            return false;
        name++;
        info.name = String::fromUTF8(name, position - name);
    }

    // Move after state.
    position += 4;

    // Skip ppid, pgrp, sid, tty_nr, tty_pgrp, flags, min_flt, cmin_flt, maj_flt, cmaj_flt.
    unsigned tokensToSkip = 10;
    while (tokensToSkip--) {
        while (!isASCIISpace(position[0]))
            position++;
        position++;
    }

    unsigned long long utime = strtoull(position, &position, 10);
    unsigned long long stime = strtoull(position, &position, 10);
    float usage = (utime + stime - (info.previousUtime + info.previousStime)) / period * 100.0;
    info.previousUtime = utime;
    info.previousStime = stime;

    info.cpuUsage = clampTo<float>(usage, 0, 100);
    return true;
}

static void collectCPUUsage(float period)
{
    DIR* dir = opendir("/proc/self/task");
    if (!dir) {
        threadInfoMap().clear();
        return;
    }

    HashSet<pid_t> previousTasks;
    for (const auto& key : threadInfoMap().keys())
        previousTasks.add(key);

    struct dirent* dp;
    while ((dp = readdir(dir))) {
        String name = String::fromUTF8(dp->d_name);
        if (name == "." || name == "..")
            continue;

        bool ok;
        pid_t id = name.toIntStrict(&ok);
        if (!ok)
            continue;

        auto& info = threadInfoMap().add(id, ThreadInfo()).iterator->value;
        if (!threadCPUUsage(id, period, info))
            threadInfoMap().remove(id);

        previousTasks.remove(id);
    }
    closedir(dir);

    threadInfoMap().removeIf([&](auto& entry)  {
        return previousTasks.contains(entry.key);
    });
}

void ResourceUsageThread::platformCollectCPUData(JSC::VM*, ResourceUsageData& data)
{
    float period = cpuPeriod();
    if (!period) {
        data.cpu = 0;
        data.cpuExcludingDebuggerThreads = 0;
        return;
    }

    collectCPUUsage(period);

    pid_t resourceUsageThreadID = Thread::currentID();

    HashSet<pid_t> knownWebKitThreads;
    {
        auto locker = holdLock(Thread::allThreadsMutex());
        for (auto* thread : Thread::allThreads(locker)) {
            if (auto id = thread->id())
                knownWebKitThreads.add(id);
        }
    }

    HashMap<pid_t, String> knownWorkerThreads;
    {
        LockHolder lock(WorkerThread::workerThreadsMutex());
        for (auto* thread : WorkerThread::workerThreads(lock)) {
            // Ignore worker threads that have not been fully started yet.
            if (!thread->thread())
                continue;

            if (auto id = thread->thread()->id())
                knownWorkerThreads.set(id, thread->identifier().isolatedCopy());
        }
    }

    auto isDebuggerThread = [&](pid_t id) -> bool {
        if (id == resourceUsageThreadID)
            return true;
#if ENABLE(SAMPLING_PROFILER)
        if (id == m_samplingProfilerThreadID)
            return true;
#endif
        return false;
    };

    auto isWebKitThread = [&](pid_t id, const String& name) -> bool {
        if (knownWebKitThreads.contains(id))
            return true;

        // The bmalloc scavenger thread is below WTF. Detect it by its name.
        if (name == "BMScavenger")
            return true;

        return false;
    };

    for (const auto& it : threadInfoMap()) {
        if (!it.value.cpuUsage)
            continue;

        float cpuUsage = it.value.cpuUsage.value();
        pid_t id = it.key;
        data.cpu += cpuUsage;
        if (isDebuggerThread(id))
            continue;

        data.cpuExcludingDebuggerThreads += cpuUsage;

        if (getpid() == id)
            data.cpuThreads.append(ThreadCPUInfo { "Main Thread"_s, { }, cpuUsage, ThreadCPUInfo::Type::Main});
        else {
            String threadIdentifier = knownWorkerThreads.get(id);
            bool isWorkerThread = !threadIdentifier.isEmpty();
            String name = it.value.name.valueOr(emptyString());
            ThreadCPUInfo::Type type = (isWorkerThread || isWebKitThread(id, name)) ? ThreadCPUInfo::Type::WebKit : ThreadCPUInfo::Type::Unknown;
            data.cpuThreads.append(ThreadCPUInfo { name, threadIdentifier, cpuUsage, type });
        }
    }
}

void ResourceUsageThread::platformCollectMemoryData(JSC::VM* vm, ResourceUsageData& data)
{
    ProcessMemoryStatus memoryStatus;
    currentProcessMemoryStatus(memoryStatus);
    data.totalDirtySize = memoryStatus.resident - memoryStatus.shared;

    size_t currentGCHeapCapacity = vm->heap.blockBytesAllocated();
    size_t currentGCOwnedExtra = vm->heap.extraMemorySize();
    size_t currentGCOwnedExternal = vm->heap.externalMemorySize();
    RELEASE_ASSERT(currentGCOwnedExternal <= currentGCOwnedExtra);

    data.categories[MemoryCategory::GCHeap].dirtySize = currentGCHeapCapacity;
    data.categories[MemoryCategory::GCOwned].dirtySize = currentGCOwnedExtra - currentGCOwnedExternal;
    data.categories[MemoryCategory::GCOwned].externalSize = currentGCOwnedExternal;

    data.totalExternalSize = currentGCOwnedExternal;

    data.timeOfNextEdenCollection = data.timestamp + vm->heap.edenActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
    data.timeOfNextFullCollection = data.timestamp + vm->heap.fullActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE) && OS(LINUX)
