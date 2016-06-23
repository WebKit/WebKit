/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef ResourceUsageData_h
#define ResourceUsageData_h

#if ENABLE(RESOURCE_USAGE)

#include <array>

namespace WebCore {

namespace MemoryCategory {
static const unsigned bmalloc = 0;
static const unsigned LibcMalloc = 1;
static const unsigned JSJIT = 2;
static const unsigned Images = 3;
static const unsigned GCHeap = 4;
static const unsigned GCOwned = 5;
static const unsigned Other = 6;
static const unsigned Layers = 7;
static const unsigned NumberOfCategories = 8;
}

struct MemoryCategoryInfo {
    MemoryCategoryInfo() { } // Needed for std::array.
    MemoryCategoryInfo(unsigned category, bool subcategory = false)
        : isSubcategory(subcategory)
        , type(category)
    {
    }

    size_t totalSize() const { return dirtySize + externalSize; }

    size_t dirtySize { 0 };
    size_t reclaimableSize { 0 };
    size_t externalSize { 0 };
    bool isSubcategory { false };
    unsigned type { MemoryCategory::NumberOfCategories };
};

struct ResourceUsageData {
    ResourceUsageData();
    ResourceUsageData(const ResourceUsageData& data);

    float cpu { 0 };
    size_t totalDirtySize { 0 };
    size_t totalExternalSize { 0 };
    std::array<MemoryCategoryInfo, MemoryCategory::NumberOfCategories> categories;
    double timeOfNextEdenCollection { 0 };
    double timeOfNextFullCollection { 0 };
};

} // namespace WebCore

#endif // ResourceUsageData_h

#endif // ENABLE(RESOURCE_USAGE)
