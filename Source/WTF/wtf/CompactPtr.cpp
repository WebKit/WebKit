/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include <wtf/CompactPtr.h>

#include <wtf/AccessibleAddress.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SegmentedVector.h>
#include <wtf/Threading.h>

namespace WTF {

#if HAVE(36BIT_ADDRESS)

struct OutsizedCompactPtrManager {
    HashMap<void*, OutsizedCompactPtr::Encoded> addedPointers;
    SegmentedVector<void*> outsizedPointers;
};

static Lock outsizedCompactPtrLock;
static OutsizedCompactPtrManager* outsizedCompactPtrManager;

static void ensureOutsizedCompactPtrManager()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        // WTF::initialize() is only needed to ensure that lowestAccessibleAddress()
        // is ready for use.
        WTF::initialize();
        RELEASE_ASSERT(lowestAccessibleAddress() >= OutsizedCompactPtr::addressRangeForOutsizedPtrEncoding);

        static NeverDestroyed<OutsizedCompactPtrManager> manager;
        outsizedCompactPtrManager = &manager.get();
    });
}

OutsizedCompactPtr::Encoded OutsizedCompactPtr::encode(void* ptr)
{
    if (!outsizedCompactPtrManager)
        ensureOutsizedCompactPtrManager();

    Locker locker { outsizedCompactPtrLock };

    auto& addedPointers = outsizedCompactPtrManager->addedPointers;
    auto iter = addedPointers.find(ptr);
    if (iter != addedPointers.end())
        return iter->value;

    auto& outsizedPointers = outsizedCompactPtrManager->outsizedPointers;
    size_t entryIndex = outsizedPointers.size();
    Encoded encoded = entryIndex + OutsizedCompactPtr::minEncoding;
    RELEASE_ASSERT(encoded < OutsizedCompactPtr::maxEncoding, encoded, OutsizedCompactPtr::maxEncoding);
    addedPointers.add(ptr, encoded);
    outsizedPointers.append(ptr);

    return encoded;
}

void* OutsizedCompactPtr::decode(uint32_t encoded)
{
    Locker locker { outsizedCompactPtrLock };
    auto& outsizedPointers = outsizedCompactPtrManager->outsizedPointers;
    size_t entryIndex = encoded - OutsizedCompactPtr::minEncoding;
    RELEASE_ASSERT(entryIndex < outsizedPointers.size(), entryIndex, outsizedPointers.size());
    return outsizedPointers[entryIndex];
}

#endif // HAVE(36BIT_ADDRESS)

} // namespace WTF
