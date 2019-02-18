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
#include <pal/spi/cocoa/MachVMSPI.h>
#include <wtf/MachSendRight.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

size_t vmPageSize()
{
#if PLATFORM(IOS_FAMILY)
    return vm_kernel_page_size;
#else
    static size_t cached = sysconf(_SC_PAGESIZE);
    return cached;
#endif
}

void logFootprintComparison(const std::array<TagInfo, 256>& before, const std::array<TagInfo, 256>& after)
{
    const size_t pageSize = vmPageSize();

    WTFLogAlways("Per-tag breakdown of memory reclaimed by pressure handler:");
    WTFLogAlways("  ## %16s %10s %10s %10s", "VM Tag", "Before", "After", "Diff");
    for (unsigned i = 0; i < 256; ++i) {
        ssize_t dirtyBefore = before[i].dirty * pageSize;
        ssize_t dirtyAfter = after[i].dirty * pageSize;
        ssize_t dirtyDiff = dirtyAfter - dirtyBefore;
        if (!dirtyBefore && !dirtyAfter)
            continue;
        String tagName = displayNameForVMTag(i);
        if (!tagName)
            tagName = makeString("Tag ", i);
        WTFLogAlways("  %02X %16s %10ld %10ld %10ld",
            i,
            tagName.ascii().data(),
            dirtyBefore,
            dirtyAfter,
            dirtyDiff
        );
    }
}

const char* displayNameForVMTag(unsigned tag)
{
    switch (tag) {
    case VM_MEMORY_IOKIT: return "IOKit";
    case VM_MEMORY_LAYERKIT: return "CoreAnimation";
    case VM_MEMORY_IMAGEIO: return "ImageIO";
    case VM_MEMORY_CGIMAGE: return "CG image";
    case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR: return "JSC JIT";
    case VM_MEMORY_JAVASCRIPT_CORE: return "WebAssembly";
    case VM_MEMORY_MALLOC: return "malloc";
    case VM_MEMORY_MALLOC_HUGE: return "malloc (huge)";
    case VM_MEMORY_MALLOC_LARGE: return "malloc (large)";
    case VM_MEMORY_MALLOC_SMALL: return "malloc (small)";
    case VM_MEMORY_MALLOC_TINY: return "malloc (tiny)";
    case VM_MEMORY_MALLOC_NANO: return "malloc (nano)";
    case VM_MEMORY_TCMALLOC: return "bmalloc";
    case VM_MEMORY_FOUNDATION: return "Foundation";
    case VM_MEMORY_STACK: return "Stack";
    case VM_MEMORY_SQLITE: return "SQLite";
    case VM_MEMORY_UNSHARED_PMAP: return "pmap (unshared)";
    case VM_MEMORY_DYLIB: return "dylib";
    case VM_MEMORY_CORESERVICES: return "CoreServices";
    case VM_MEMORY_OS_ALLOC_ONCE: return "os_alloc_once";
    case VM_MEMORY_LIBDISPATCH: return "libdispatch";
    default: return nullptr;
    }
}

std::array<TagInfo, 256> pagesPerVMTag()
{
    std::array<TagInfo, 256> tags;
    task_t task = mach_task_self();
    mach_vm_size_t size;
    uint32_t depth = 0;
    struct vm_region_submap_info_64 info = { };
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    for (mach_vm_address_t addr = 0; ; addr += size) {
        int purgeableState;
        if (mach_vm_purgable_control(task, addr, VM_PURGABLE_GET_STATE, &purgeableState) != KERN_SUCCESS)
            purgeableState = VM_PURGABLE_DENY;

        kern_return_t kr = mach_vm_region_recurse(task, &addr, &size, &depth, (vm_region_info_t)&info, &count);
        if (kr != KERN_SUCCESS)
            break;

        if (purgeableState == VM_PURGABLE_VOLATILE) {
            tags[info.user_tag].reclaimable += info.pages_resident;
            continue;
        }

        if (purgeableState == VM_PURGABLE_EMPTY) {
            tags[info.user_tag].reclaimable += size / vmPageSize();
            continue;
        }

        bool anonymous = !info.external_pager;
        if (anonymous) {
            tags[info.user_tag].dirty += info.pages_resident - info.pages_reusable;
            tags[info.user_tag].reclaimable += info.pages_reusable;
        } else
            tags[info.user_tag].dirty += info.pages_dirtied;
    }

    return tags;
}

static unsigned categoryForVMTag(unsigned tag)
{
    switch (tag) {
    case VM_MEMORY_IOKIT:
    case VM_MEMORY_LAYERKIT:
        return MemoryCategory::Layers;
    case VM_MEMORY_IMAGEIO:
    case VM_MEMORY_CGIMAGE:
        return MemoryCategory::Images;
    case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR:
        return MemoryCategory::JSJIT;
    case VM_MEMORY_JAVASCRIPT_CORE:
        return MemoryCategory::WebAssembly;
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
