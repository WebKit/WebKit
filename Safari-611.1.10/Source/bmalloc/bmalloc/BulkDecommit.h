/*
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

#pragma once

#include "VMAllocate.h"
#include <vector>

namespace bmalloc {

class BulkDecommit {
    using Data = std::vector<std::pair<char*, size_t>>;

public:
    void addEager(void* ptr, size_t size)
    {
        add(m_eager, ptr, size);
    }
    void addLazy(void* ptr, size_t size)
    {
        add(m_lazy, ptr, size);
    }
    void processEager()
    {
        process(m_eager);
    }
    void processLazy()
    {
        process(m_lazy);
    }

private:
    void add(Data& data, void* ptr, size_t size)
    {
        char* begin = roundUpToMultipleOf(vmPageSizePhysical(), static_cast<char*>(ptr));
        char* end = roundDownToMultipleOf(vmPageSizePhysical(), static_cast<char*>(ptr) + size);
        if (begin >= end)
            return;
        data.push_back({begin, end - begin});
    }

    void process(BulkDecommit::Data& decommits)
    {
        std::sort(
            decommits.begin(), decommits.end(),
            [&] (const auto& a, const auto& b) -> bool {
                return a.first < b.first;
            });

        char* run = nullptr;
        size_t runSize = 0;
        for (unsigned i = 0; i < decommits.size(); ++i) {
            auto& pair = decommits[i];
            if (run + runSize != pair.first) {
                if (run)
                    vmDeallocatePhysicalPages(run, runSize);
                run = pair.first;
                runSize = pair.second;
            } else {
                BASSERT(run);
                runSize += pair.second;
            }
        }

        if (run)
            vmDeallocatePhysicalPages(run, runSize);
    }

    Data m_eager;
    Data m_lazy;
};

} // namespace bmalloc
