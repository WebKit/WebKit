/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "config.h"
#include <wtf/MemoryFootprint.h>

#include <algorithm>
#include <optional>
#include <type_traits>
#include <windows.h>
#include <psapi.h>
#include <wtf/MallocSpan.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>
#include <wtf/win/Win32Handle.h>

namespace WTF {

// Compute the size of the private working set with QueryWorkingSetEx over the committed
// regions of the address space, in bounded batches with no retry loop.
static std::optional<size_t> memoryFootprintFromWorkingSetEx()
{
    constexpr size_t batchSize = 16 * 1024;
    Vector<PSAPI_WORKING_SET_EX_INFORMATION> batch;
    batch.reserveInitialCapacity(batchSize);

    size_t pageSize = 0;
    size_t numberOfPrivateResidentPages = 0;
    bool sawSuccessfulQuery = false;

    auto flushBatch = [&] () -> bool {
        if (batch.isEmpty())
            return true;
        // Wine only implements this query for the GetCurrentProcess() pseudo handle.
        if (!QueryWorkingSetEx(GetCurrentProcess(), batch.mutableSpan().data(), batch.size() * sizeof(PSAPI_WORKING_SET_EX_INFORMATION)))
            return false;
        sawSuccessfulQuery = true;
        for (auto& entry : batch) {
            if (entry.VirtualAttributes.Valid && !entry.VirtualAttributes.Shared)
                numberOfPrivateResidentPages++;
        }
        batch.shrink(0);
        return true;
    };

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    pageSize = systemInfo.dwPageSize;
    if (!pageSize)
        return std::nullopt;

    MEMORY_BASIC_INFORMATION memoryInfo;
    uintptr_t address = reinterpret_cast<uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    uintptr_t maximumAddress = reinterpret_cast<uintptr_t>(systemInfo.lpMaximumApplicationAddress);
    while (address < maximumAddress && VirtualQuery(reinterpret_cast<LPCVOID>(address), &memoryInfo, sizeof(memoryInfo)) == sizeof(memoryInfo)) {
        if (memoryInfo.State == MEM_COMMIT) {
            uintptr_t regionEnd = reinterpret_cast<uintptr_t>(memoryInfo.BaseAddress) + memoryInfo.RegionSize;
            for (uintptr_t page = reinterpret_cast<uintptr_t>(memoryInfo.BaseAddress); page < regionEnd; page += pageSize) {
                batch.append({ reinterpret_cast<PVOID>(page), { } });
                if (batch.size() == batchSize && !flushBatch())
                    return std::nullopt;
            }
        }
        uintptr_t next = reinterpret_cast<uintptr_t>(memoryInfo.BaseAddress) + memoryInfo.RegionSize;
        if (next <= address)
            break;
        address = next;
    }
    if (!flushBatch() || !sawSuccessfulQuery)
        return std::nullopt;

    return numberOfPrivateResidentPages * pageSize;
}

static size_t memoryFootprintFromWorkingSetList()
{
    // We would like to calculate size of private working set.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms684891(v=vs.85).aspx
    // > The working set of a program is a collection of those pages in its virtual address
    // > space that have been recently referenced. It includes both shared and private data.
    // > The shared data includes pages that contain all instructions your application executes,
    // > including those in your DLLs and the system DLLs. As the working set size increases,
    // > memory demand increases.
    auto process = Win32Handle::adopt(::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId()));
    if (!process)
        return 0;

    auto countSizeOfPrivateWorkingSet = [] (const PSAPI_WORKING_SET_INFORMATION& workingSets) {
        constexpr const size_t pageSize = 4 * KB;
        size_t numberOfPrivateWorkingSetPages = 0;
        for (size_t i = 0; i < workingSets.NumberOfEntries; ++i) {
            // https://msdn.microsoft.com/en-us/library/windows/desktop/ms684902(v=vs.85).aspx
            PSAPI_WORKING_SET_BLOCK workingSetBlock = workingSets.WorkingSetInfo[i];
            if (!workingSetBlock.Shared)
                numberOfPrivateWorkingSetPages++;
        }
        return numberOfPrivateWorkingSetPages * pageSize;
    };

    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms684946(v=vs.85).aspx
    constexpr const size_t minNumberOfEntries = 16;
    constexpr const size_t sizeOfBufferOnStack = sizeof(PSAPI_WORKING_SET_INFORMATION) + minNumberOfEntries * sizeof(PSAPI_WORKING_SET_BLOCK);
    alignas(PSAPI_WORKING_SET_INFORMATION) std::byte bufferOnStack[sizeOfBufferOnStack];
    auto* workingSetsOnStack = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(&bufferOnStack);
    if (QueryWorkingSet(process.get(), workingSetsOnStack, sizeOfBufferOnStack))
        return countSizeOfPrivateWorkingSet(*workingSetsOnStack);

    // NumberOfEntries is only written on an ERROR_BAD_LENGTH failure.
    if (GetLastError() != ERROR_BAD_LENGTH)
        return 0;

    auto updateNumberOfEntries = [&] (size_t numberOfEntries) {
        // If working set increases between first QueryWorkingSet and second QueryWorkingSet, the second one can fail.
        // At that time, we should increase numberOfEntries.
        return std::max(minNumberOfEntries, numberOfEntries + numberOfEntries / 4 + 1);
    };

    constexpr const size_t maxNumberOfEntries = 16 * 1024 * 1024;
    for (size_t numberOfEntries = updateNumberOfEntries(workingSetsOnStack->NumberOfEntries); numberOfEntries <= maxNumberOfEntries;) {
        size_t workingSetSizeInBytes = roundUpToMultipleOf(sizeof(PSAPI_WORKING_SET_INFORMATION), sizeof(PSAPI_WORKING_SET_INFORMATION) + sizeof(PSAPI_WORKING_SET_BLOCK) * numberOfEntries);

        auto workingSets = MallocSpan<PSAPI_WORKING_SET_INFORMATION>::tryMalloc(workingSetSizeInBytes);
        auto workingSetsSpan = workingSets.mutableSpan();
        if (workingSetsSpan.empty())
            return 0;
        if (QueryWorkingSet(process.get(), workingSetsSpan.data(), workingSetsSpan.size_bytes()))
            return countSizeOfPrivateWorkingSet(workingSetsSpan[0]);

        if (GetLastError() != ERROR_BAD_LENGTH)
            return 0;
        numberOfEntries = updateNumberOfEntries(workingSetsSpan[0].NumberOfEntries);
    }
    return 0;
}

size_t memoryFootprint()
{
    if (auto footprint = memoryFootprintFromWorkingSetEx())
        return *footprint;
    return memoryFootprintFromWorkingSetList();
}

}
