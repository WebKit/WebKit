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

#if ENABLE_PHYSICAL_PAGE_MAP 

#include "VMAllocate.h"
#include <unordered_set>

namespace bmalloc {

// This class is useful for debugging bmalloc's footprint.
class PhysicalPageMap {
public:

    void commit(void* ptr, size_t size)
    {
        forEachPhysicalPage(ptr, size, [&] (void* ptr) {
            m_physicalPages.insert(ptr);
        });
    }

    void decommit(void* ptr, size_t size)
    {
        forEachPhysicalPage(ptr, size, [&] (void* ptr) {
            m_physicalPages.erase(ptr);
        });
    }

    size_t footprint()
    {
        return static_cast<size_t>(m_physicalPages.size()) * vmPageSizePhysical();
    }

private:
    template <typename F>
    void forEachPhysicalPage(void* ptr, size_t size, F f)
    {
        char* begin = roundUpToMultipleOf(vmPageSizePhysical(), static_cast<char*>(ptr));
        char* end = roundDownToMultipleOf(vmPageSizePhysical(), static_cast<char*>(ptr) + size);
        while (begin < end) {
            f(begin);
            begin += vmPageSizePhysical();
        }
    }

    std::unordered_set<void*> m_physicalPages;
};

} // namespace bmalloc

#endif // ENABLE_PHYSICAL_PAGE_MAP 
