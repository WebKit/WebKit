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
#include <type_traits>
#include <windows.h>
#include <psapi.h>
#include <wtf/MallocPtr.h>
#include <wtf/win/Win32Handle.h>

namespace WTF {

size_t memoryFootprint()
{
    // We would like to calculate size of private working set.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms684891(v=vs.85).aspx
    // > The working set of a program is a collection of those pages in its virtual address
    // > space that have been recently referenced. It includes both shared and private data.
    // > The shared data includes pages that contain all instructions your application executes,
    // > including those in your DLLs and the system DLLs. As the working set size increases,
    // > memory demand increases.
    Win32Handle process(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId()));
    if (!process.isValid())
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
    std::aligned_storage<sizeOfBufferOnStack, alignof(PSAPI_WORKING_SET_INFORMATION)>::type bufferOnStack;
    auto* workingSetsOnStack = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(&bufferOnStack);
    if (QueryWorkingSet(process.get(), workingSetsOnStack, sizeOfBufferOnStack))
        return countSizeOfPrivateWorkingSet(*workingSetsOnStack);

    auto updateNumberOfEntries = [&] (size_t numberOfEntries) {
        // If working set increases between first QueryWorkingSet and second QueryWorkingSet, the second one can fail.
        // At that time, we should increase numberOfEntries.
        return std::max(minNumberOfEntries, numberOfEntries + numberOfEntries / 4 + 1);
    };

    for (size_t numberOfEntries = updateNumberOfEntries(workingSetsOnStack->NumberOfEntries);;) {
        size_t workingSetSizeInBytes = sizeof(PSAPI_WORKING_SET_INFORMATION) + sizeof(PSAPI_WORKING_SET_BLOCK) * numberOfEntries;
        auto workingSets = MallocPtr<PSAPI_WORKING_SET_INFORMATION>::malloc(workingSetSizeInBytes);
        if (QueryWorkingSet(process.get(), workingSets.get(), workingSetSizeInBytes))
            return countSizeOfPrivateWorkingSet(*workingSets);

        if (GetLastError() != ERROR_BAD_LENGTH)
            return 0;
        numberOfEntries = updateNumberOfEntries(workingSets->NumberOfEntries);
    }
}

}
