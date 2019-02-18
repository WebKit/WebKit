/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(RESOURCE_USAGE)

#include <array>
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>

namespace WebCore {

// v(name, id, subcategory)
#define WEBCORE_EACH_MEMORY_CATEGORIES(v) \
    v(bmalloc, 0, false) \
    v(LibcMalloc, 1, false) \
    v(JSJIT, 2, false) \
    v(WebAssembly, 3, false) \
    v(Images, 4, false) \
    v(GCHeap, 5, true) \
    v(GCOwned, 6, true) \
    v(Other, 7, false) \
    v(Layers, 8, false) \

namespace MemoryCategory {
#define WEBCORE_DEFINE_MEMORY_CATEGORY(name, id, subcategory) static constexpr unsigned name = id;
WEBCORE_EACH_MEMORY_CATEGORIES(WEBCORE_DEFINE_MEMORY_CATEGORY)
#undef WEBCORE_DEFINE_MEMORY_CATEGORY

#define WEBCORE_DEFINE_MEMORY_CATEGORY(name, id, subcategory) + 1
static constexpr unsigned NumberOfCategories = 0 WEBCORE_EACH_MEMORY_CATEGORIES(WEBCORE_DEFINE_MEMORY_CATEGORY);
#undef WEBCORE_DEFINE_MEMORY_CATEGORY
}

struct MemoryCategoryInfo {
    constexpr MemoryCategoryInfo() = default; // Needed for std::array.
    constexpr MemoryCategoryInfo(unsigned category, bool subcategory = false)
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

struct ThreadCPUInfo {
    enum class Type : uint8_t {
        Unknown,
        Main,
        WebKit,
    };

    String name;
    String identifier;
    float cpu { 0 };
    Type type { ThreadCPUInfo::Type::Unknown };
};

struct ResourceUsageData {
    ResourceUsageData() = default;

    float cpu { 0 };
    float cpuExcludingDebuggerThreads { 0 };
    Vector<ThreadCPUInfo> cpuThreads;

    size_t totalDirtySize { 0 };
    size_t totalExternalSize { 0 };
    std::array<MemoryCategoryInfo, MemoryCategory::NumberOfCategories> categories { {
#define WEBCORE_DEFINE_MEMORY_CATEGORY(name, id, subcategory) MemoryCategoryInfo { MemoryCategory::name, subcategory },
WEBCORE_EACH_MEMORY_CATEGORIES(WEBCORE_DEFINE_MEMORY_CATEGORY)
#undef WEBCORE_DEFINE_MEMORY_CATEGORY
    } };
    MonotonicTime timestamp { MonotonicTime::now() };
    MonotonicTime timeOfNextEdenCollection { MonotonicTime::nan() };
    MonotonicTime timeOfNextFullCollection { MonotonicTime::nan() };
};

} // namespace WebCore

#endif // ResourceUsageData_h
