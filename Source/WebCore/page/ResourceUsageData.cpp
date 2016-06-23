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

#include "config.h"
#include "ResourceUsageData.h"

#if ENABLE(RESOURCE_USAGE)

namespace WebCore {

ResourceUsageData::ResourceUsageData()
{
    // VM tag categories.
    categories[MemoryCategory::JSJIT] = MemoryCategoryInfo(MemoryCategory::JSJIT);
    categories[MemoryCategory::Images] = MemoryCategoryInfo(MemoryCategory::Images);
    categories[MemoryCategory::Layers] = MemoryCategoryInfo(MemoryCategory::Layers);
    categories[MemoryCategory::LibcMalloc] = MemoryCategoryInfo(MemoryCategory::LibcMalloc);
    categories[MemoryCategory::bmalloc] = MemoryCategoryInfo(MemoryCategory::bmalloc);
    categories[MemoryCategory::Other] = MemoryCategoryInfo(MemoryCategory::Other);

    // Sub categories (e.g breakdown of bmalloc tag.)
    categories[MemoryCategory::GCHeap] = MemoryCategoryInfo(MemoryCategory::GCHeap, true);
    categories[MemoryCategory::GCOwned] = MemoryCategoryInfo(MemoryCategory::GCOwned, true);
}

ResourceUsageData::ResourceUsageData(const ResourceUsageData& other)
    : cpu(other.cpu)
    , totalDirtySize(other.totalDirtySize)
    , totalExternalSize(other.totalExternalSize)
    , timeOfNextEdenCollection(other.timeOfNextEdenCollection)
    , timeOfNextFullCollection(other.timeOfNextFullCollection)
{
    std::copy(other.categories.begin(), other.categories.end(), categories.begin());
}

}

#endif // ENABLE(RESOURCE_USAGE)
