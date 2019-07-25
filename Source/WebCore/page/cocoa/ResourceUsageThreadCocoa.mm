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

#if ENABLE(RESOURCE_USAGE)

#include "WorkerThread.h"
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/SamplingProfiler.h>
#include <JavaScriptCore/VM.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <wtf/MachSendRight.h>
#include <wtf/ResourceUsage.h>
#include <wtf/spi/cocoa/MachVMSPI.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static unsigned categoryForVMTag(unsigned tag)
{
    switch (tag) {
    case VM_MEMORY_IOKIT:
    case VM_MEMORY_LAYERKIT:
        return MemoryCategory::Layers;
    case VM_MEMORY_IMAGEIO:
    case VM_MEMORY_CGIMAGE:
        return MemoryCategory::Images;
    case VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE:
        return MemoryCategory::IsoHeap;
    case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR:
        return MemoryCategory::JSJIT;
    case VM_MEMORY_JAVASCRIPT_CORE:
        return MemoryCategory::Gigacage;
    case VM_MEMORY_MALLOC:
    case VM_MEMORY_MALLOC_HUGE:
    case VM_MEMORY_MALLOC_LARGE:
    case VM_MEMORY_MALLOC_SMALL:
    case VM_MEMORY_MALLOC_TINY:
    case VM_MEMORY_MALLOC_NANO:
        return MemoryCategory::LibcMalloc;
    case VM_MEMORY_TCMALLOC:
        return MemoryCategory::bmalloc;
    default:
        return MemoryCategory::Other;
    }
}

struct ThreadInfo {
    MachSendRight sendRight;
    float usage { 0 };
    String threadName;
    String dispatchQueueName;
};

static Vector<ThreadInfo> threadInfos()
{
    thread_array_t threadList = nullptr;
    mach_msg_type_number_t threadCount = 0;
    kern_return_t kr = task_threads(mach_task_self(), &threadList, &threadCount);
    ASSERT(kr == KERN_SUCCESS);
    if (kr != KERN_SUCCESS)
        return { };

    Vector<ThreadInfo> infos;
    for (mach_msg_type_number_t i = 0; i < threadCount; ++i) {
        MachSendRight sendRight = MachSendRight::adopt(threadList[i]);

        thread_info_data_t threadInfo;
        mach_msg_type_number_t threadInfoCount = THREAD_INFO_MAX;
        kr = thread_info(sendRight.sendRight(), THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&threadInfo), &threadInfoCount);
        ASSERT(kr == KERN_SUCCESS);
        if (kr != KERN_SUCCESS)
            continue;

        thread_identifier_info_data_t threadIdentifierInfo;
        mach_msg_type_number_t threadIdentifierInfoCount = THREAD_IDENTIFIER_INFO_COUNT;
        kr = thread_info(sendRight.sendRight(), THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&threadIdentifierInfo), &threadIdentifierInfoCount);
        ASSERT(kr == KERN_SUCCESS);
        if (kr != KERN_SUCCESS)
            continue;

        thread_extended_info_data_t threadExtendedInfo;
        mach_msg_type_number_t threadExtendedInfoCount = THREAD_EXTENDED_INFO_COUNT;
        kr = thread_info(sendRight.sendRight(), THREAD_EXTENDED_INFO, reinterpret_cast<thread_info_t>(&threadExtendedInfo), &threadExtendedInfoCount);
        ASSERT(kr == KERN_SUCCESS);
        if (kr != KERN_SUCCESS)
            continue;

        float usage = 0;
        auto threadBasicInfo = reinterpret_cast<thread_basic_info_t>(threadInfo);
        if (!(threadBasicInfo->flags & TH_FLAGS_IDLE))
            usage = threadBasicInfo->cpu_usage / static_cast<float>(TH_USAGE_SCALE) * 100.0;

        String threadName = String(threadExtendedInfo.pth_name);
        String dispatchQueueName;
        if (threadIdentifierInfo.dispatch_qaddr) {
            dispatch_queue_t queue = *reinterpret_cast<dispatch_queue_t*>(threadIdentifierInfo.dispatch_qaddr);
            dispatchQueueName = String(dispatch_queue_get_label(queue));
        }

        infos.append(ThreadInfo { WTFMove(sendRight), usage, threadName, dispatchQueueName });
    }

    kr = vm_deallocate(mach_task_self(), (vm_offset_t)threadList, threadCount * sizeof(thread_t));
    ASSERT(kr == KERN_SUCCESS);

    return infos;
}

void ResourceUsageThread::platformSaveStateBeforeStarting()
{
#if ENABLE(SAMPLING_PROFILER)
    m_samplingProfilerMachThread = m_vm->samplingProfiler() ? m_vm->samplingProfiler()->machThread() : MACH_PORT_NULL;
#endif
}

void ResourceUsageThread::platformCollectCPUData(JSC::VM*, ResourceUsageData& data)
{
    Vector<ThreadInfo> threads = threadInfos();
    if (threads.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Main thread is always first.
    ASSERT(threads[0].dispatchQueueName == "com.apple.main-thread");

    mach_port_t resourceUsageMachThread = mach_thread_self();
    mach_port_t mainThreadMachThread = threads[0].sendRight.sendRight();

    HashSet<mach_port_t> knownWebKitThreads;
    {
        LockHolder lock(Thread::allThreadsMutex());
        for (auto* thread : Thread::allThreads(lock)) {
            mach_port_t machThread = thread->machThread();
            if (machThread != MACH_PORT_NULL)
                knownWebKitThreads.add(machThread);
        }
    }

    HashMap<mach_port_t, String> knownWorkerThreads;
    {
        LockHolder lock(WorkerThread::workerThreadsMutex());
        for (auto* thread : WorkerThread::workerThreads(lock)) {
            mach_port_t machThread = thread->thread()->machThread();
            if (machThread != MACH_PORT_NULL)
                knownWorkerThreads.set(machThread, thread->identifier().isolatedCopy());
        }
    }

    auto isDebuggerThread = [&](const ThreadInfo& thread) -> bool {
        mach_port_t machThread = thread.sendRight.sendRight();
        if (machThread == resourceUsageMachThread)
            return true;
#if ENABLE(SAMPLING_PROFILER)
        if (machThread == m_samplingProfilerMachThread)
            return true;
#endif
        return false;
    };

    auto isWebKitThread = [&](const ThreadInfo& thread) -> bool {
        mach_port_t machThread = thread.sendRight.sendRight();
        if (knownWebKitThreads.contains(machThread))
            return true;

        // The bmalloc scavenger thread is below WTF. Detect it by its name.
        if (thread.threadName == "JavaScriptCore bmalloc scavenger")
            return true;

        // WebKit uses many WorkQueues with common prefixes.
        if (thread.dispatchQueueName.startsWith("com.apple.IPC.")
            || thread.dispatchQueueName.startsWith("com.apple.WebKit.")
            || thread.dispatchQueueName.startsWith("org.webkit."))
            return true;

        return false;
    };

    for (auto& thread : threads) {
        data.cpu += thread.usage;
        if (isDebuggerThread(thread))
            continue;

        data.cpuExcludingDebuggerThreads += thread.usage;

        mach_port_t machThread = thread.sendRight.sendRight();
        if (machThread == mainThreadMachThread) {
            data.cpuThreads.append(ThreadCPUInfo { "Main Thread"_s, String(), thread.usage, ThreadCPUInfo::Type::Main});
            continue;
        }

        String threadIdentifier = knownWorkerThreads.get(machThread);
        bool isWorkerThread = !threadIdentifier.isEmpty();
        ThreadCPUInfo::Type type = (isWorkerThread || isWebKitThread(thread)) ? ThreadCPUInfo::Type::WebKit : ThreadCPUInfo::Type::Unknown;
        data.cpuThreads.append(ThreadCPUInfo { thread.threadName, threadIdentifier, thread.usage, type });
    }
}

void ResourceUsageThread::platformCollectMemoryData(JSC::VM* vm, ResourceUsageData& data)
{
    auto tags = pagesPerVMTag();
    std::array<TagInfo, MemoryCategory::NumberOfCategories> pagesPerCategory;
    size_t totalDirtyPages = 0;
    for (unsigned i = 0; i < 256; ++i) {
        pagesPerCategory[categoryForVMTag(i)].dirty += tags[i].dirty;
        pagesPerCategory[categoryForVMTag(i)].reclaimable += tags[i].reclaimable;
        totalDirtyPages += tags[i].dirty;
    }

    for (auto& category : data.categories) {
        if (category.isSubcategory) // Only do automatic tallying for top-level categories.
            continue;
        category.dirtySize = pagesPerCategory[category.type].dirty * vmPageSize();
        category.reclaimableSize = pagesPerCategory[category.type].reclaimable * vmPageSize();
    }
    data.totalDirtySize = totalDirtyPages * vmPageSize();

    size_t currentGCHeapCapacity = vm->heap.blockBytesAllocated();
    size_t currentGCOwnedExtra = vm->heap.extraMemorySize();
    size_t currentGCOwnedExternal = vm->heap.externalMemorySize();
    ASSERT(currentGCOwnedExternal <= currentGCOwnedExtra);

    data.categories[MemoryCategory::GCHeap].dirtySize = currentGCHeapCapacity;
    data.categories[MemoryCategory::GCOwned].dirtySize = currentGCOwnedExtra - currentGCOwnedExternal;
    data.categories[MemoryCategory::GCOwned].externalSize = currentGCOwnedExternal;

    auto& mallocBucket = isFastMallocEnabled() ? data.categories[MemoryCategory::bmalloc] : data.categories[MemoryCategory::LibcMalloc];

    // First subtract memory allocated by the GC heap, since we track that separately.
    mallocBucket.dirtySize -= currentGCHeapCapacity;

    // It would be nice to assert that the "GC owned" amount is smaller than the total dirty malloc size,
    // but since the "GC owned" accounting is inexact, it's not currently feasible.
    size_t currentGCOwnedGenerallyInMalloc = currentGCOwnedExtra - currentGCOwnedExternal;
    if (currentGCOwnedGenerallyInMalloc < mallocBucket.dirtySize)
        mallocBucket.dirtySize -= currentGCOwnedGenerallyInMalloc;

    data.totalExternalSize = currentGCOwnedExternal;

    data.timeOfNextEdenCollection = data.timestamp + vm->heap.edenActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
    data.timeOfNextFullCollection = data.timestamp + vm->heap.fullActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
}

}

#endif
