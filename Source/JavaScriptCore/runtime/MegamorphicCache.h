/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "Structure.h"
#include <wtf/TZoneMalloc.h>

namespace JSC {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MegamorphicCache);

class MegamorphicCache {
    WTF_MAKE_TZONE_ALLOCATED(MegamorphicCache);
    WTF_MAKE_NONCOPYABLE(MegamorphicCache);
public:
    static constexpr uint32_t loadCachePrimarySize = 2048;
    static constexpr uint32_t loadCacheSecondarySize = 512;
    static_assert(hasOneBitSet(loadCachePrimarySize), "size should be a power of two.");
    static_assert(hasOneBitSet(loadCacheSecondarySize), "size should be a power of two.");
    static constexpr uint32_t loadCachePrimaryMask = loadCachePrimarySize - 1;
    static constexpr uint32_t loadCacheSecondaryMask = loadCacheSecondarySize - 1;

    static constexpr uint32_t storeCachePrimarySize = 2048;
    static constexpr uint32_t storeCacheSecondarySize = 512;
    static_assert(hasOneBitSet(storeCachePrimarySize), "size should be a power of two.");
    static_assert(hasOneBitSet(storeCacheSecondarySize), "size should be a power of two.");
    static constexpr uint32_t storeCachePrimaryMask = storeCachePrimarySize - 1;
    static constexpr uint32_t storeCacheSecondaryMask = storeCacheSecondarySize - 1;

    static constexpr uint32_t hasCachePrimarySize = 512;
    static constexpr uint32_t hasCacheSecondarySize = 128;
    static_assert(hasOneBitSet(hasCachePrimarySize), "size should be a power of two.");
    static_assert(hasOneBitSet(hasCacheSecondarySize), "size should be a power of two.");
    static constexpr uint32_t hasCachePrimaryMask = hasCachePrimarySize - 1;
    static constexpr uint32_t hasCacheSecondaryMask = hasCacheSecondarySize - 1;

    static constexpr uint32_t hasOwnCachePrimarySize = 2048;
    static constexpr uint32_t hasOwnCacheSecondarySize = 512;
    static_assert(hasOneBitSet(hasOwnCachePrimarySize), "size should be a power of two.");
    static_assert(hasOneBitSet(hasOwnCacheSecondarySize), "size should be a power of two.");
    static constexpr uint32_t hasOwnCachePrimaryMask = hasOwnCachePrimarySize - 1;
    static constexpr uint32_t hasOwnCacheSecondaryMask = hasOwnCacheSecondarySize - 1;

    static constexpr uint16_t invalidEpoch = 0;
    static constexpr PropertyOffset maxOffset = UINT16_MAX;

    struct LoadEntry {
        static ptrdiff_t offsetOfUid() { return OBJECT_OFFSETOF(LoadEntry, m_uid); }
        static ptrdiff_t offsetOfStructureID() { return OBJECT_OFFSETOF(LoadEntry, m_structureID); }
        static ptrdiff_t offsetOfEpoch() { return OBJECT_OFFSETOF(LoadEntry, m_epoch); }
        static ptrdiff_t offsetOfOffset() { return OBJECT_OFFSETOF(LoadEntry, m_offset); }
        static ptrdiff_t offsetOfHolder() { return OBJECT_OFFSETOF(LoadEntry, m_holder); }

        void initAsMiss(StructureID structureID, UniquedStringImpl* uid, uint16_t epoch)
        {
            m_uid = uid;
            m_structureID = structureID;
            m_epoch = epoch;
            m_offset = 0;
            m_holder = nullptr;
        }

        void initAsHit(StructureID structureID, UniquedStringImpl* uid, uint16_t epoch, JSCell* holder, uint16_t offset, bool ownProperty)
        {
            m_uid = uid;
            m_structureID = structureID;
            m_epoch = epoch;
            m_offset = offset;
            m_holder = (ownProperty) ? JSCell::seenMultipleCalleeObjects() : holder;
        }

        RefPtr<UniquedStringImpl> m_uid;
        StructureID m_structureID { };
        uint16_t m_epoch { invalidEpoch };
        uint16_t m_offset { 0 };
        JSCell* m_holder { nullptr };
    };

    struct StoreEntry {
        static ptrdiff_t offsetOfUid() { return OBJECT_OFFSETOF(StoreEntry, m_uid); }
        static ptrdiff_t offsetOfOldStructureID() { return OBJECT_OFFSETOF(StoreEntry, m_oldStructureID); }
        static ptrdiff_t offsetOfNewStructureID() { return OBJECT_OFFSETOF(StoreEntry, m_newStructureID); }
        static ptrdiff_t offsetOfEpoch() { return OBJECT_OFFSETOF(StoreEntry, m_epoch); }
        static ptrdiff_t offsetOfOffset() { return OBJECT_OFFSETOF(StoreEntry, m_offset); }

        void init(StructureID oldStructureID, StructureID newStructureID, UniquedStringImpl* uid, uint16_t epoch, uint16_t offset)
        {
            m_uid = uid;
            m_oldStructureID = oldStructureID;
            m_newStructureID = newStructureID;
            m_epoch = epoch;
            m_offset = offset;
        }

        RefPtr<UniquedStringImpl> m_uid;
        StructureID m_oldStructureID { };
        StructureID m_newStructureID { };
        uint16_t m_epoch { invalidEpoch };
        uint16_t m_offset { 0 };
    };

    struct HasEntry {
        static ptrdiff_t offsetOfUid() { return OBJECT_OFFSETOF(HasEntry, m_uid); }
        static ptrdiff_t offsetOfStructureID() { return OBJECT_OFFSETOF(HasEntry, m_structureID); }
        static ptrdiff_t offsetOfEpoch() { return OBJECT_OFFSETOF(HasEntry, m_epoch); }
        static ptrdiff_t offsetOfResult() { return OBJECT_OFFSETOF(HasEntry, m_result); }

        void init(StructureID structureID, UniquedStringImpl* uid, uint16_t epoch, bool result)
        {
            m_uid = uid;
            m_structureID = structureID;
            m_epoch = epoch;
            m_result = !!result;
        }

        RefPtr<UniquedStringImpl> m_uid;
        StructureID m_structureID { };
        uint16_t m_epoch { invalidEpoch };
        uint16_t m_result { false };
    };

    using HasOwnEntry = HasEntry;

    static ptrdiff_t offsetOfLoadCachePrimaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_loadCachePrimaryEntries); }
    static ptrdiff_t offsetOfLoadCacheSecondaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_loadCacheSecondaryEntries); }

    static ptrdiff_t offsetOfStoreCachePrimaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_storeCachePrimaryEntries); }
    static ptrdiff_t offsetOfStoreCacheSecondaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_storeCacheSecondaryEntries); }

    static ptrdiff_t offsetOfHasCachePrimaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_hasCachePrimaryEntries); }
    static ptrdiff_t offsetOfHasCacheSecondaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_hasCacheSecondaryEntries); }

    static ptrdiff_t offsetOfHasOwnCachePrimaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_hasOwnCachePrimaryEntries); }
    static ptrdiff_t offsetOfHasOwnCacheSecondaryEntries() { return OBJECT_OFFSETOF(MegamorphicCache, m_hasOwnCacheSecondaryEntries); }

    static ptrdiff_t offsetOfEpoch() { return OBJECT_OFFSETOF(MegamorphicCache, m_epoch); }

    MegamorphicCache() = default;

#if CPU(ADDRESS64) && !ENABLE(STRUCTURE_ID_WITH_SHIFT)
    // Because Structure is allocated with 16-byte alignment, we should assume that StructureID's lower 4 bits are zeros.
    static constexpr unsigned structureIDHashShift1 = 4;
#else
    // When using STRUCTURE_ID_WITH_SHIFT, all bits can be different. Thus we do not need to shift the first level.
    static constexpr unsigned structureIDHashShift1 = 0;
#endif
    static constexpr unsigned structureIDHashShift2 = structureIDHashShift1 + 11;
    static constexpr unsigned structureIDHashShift3 = structureIDHashShift1 + 9;

    static constexpr unsigned structureIDHashShift4 = structureIDHashShift1 + 11;
    static constexpr unsigned structureIDHashShift5 = structureIDHashShift1 + 9;

    static constexpr unsigned structureIDHashShift6 = structureIDHashShift1 + 9;
    static constexpr unsigned structureIDHashShift7 = structureIDHashShift1 + 7;

    static constexpr unsigned structureIDHashShift8 = structureIDHashShift1 + 10;
    static constexpr unsigned structureIDHashShift9 = structureIDHashShift1 + 8;

    ALWAYS_INLINE static uint32_t loadPrimaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t sid = bitwise_cast<uint32_t>(structureID);
        return ((sid >> structureIDHashShift1) ^ (sid >> structureIDHashShift2)) + uid->hash();
    }

    ALWAYS_INLINE static uint32_t loadSecondaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t key = bitwise_cast<uint32_t>(structureID) + static_cast<uint32_t>(bitwise_cast<uintptr_t>(uid));
        return key + (key >> structureIDHashShift3);
    }

    ALWAYS_INLINE static uint32_t storeCachePrimaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t sid = bitwise_cast<uint32_t>(structureID);
        return ((sid >> structureIDHashShift1) ^ (sid >> structureIDHashShift4)) + uid->hash();
    }

    ALWAYS_INLINE static uint32_t storeCacheSecondaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t key = bitwise_cast<uint32_t>(structureID) + static_cast<uint32_t>(bitwise_cast<uintptr_t>(uid));
        return key + (key >> structureIDHashShift5);
    }

    ALWAYS_INLINE static uint32_t hasCachePrimaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t sid = bitwise_cast<uint32_t>(structureID);
        return ((sid >> structureIDHashShift1) ^ (sid >> structureIDHashShift6)) + uid->hash();
    }

    ALWAYS_INLINE static uint32_t hasCacheSecondaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t key = bitwise_cast<uint32_t>(structureID) + static_cast<uint32_t>(bitwise_cast<uintptr_t>(uid));
        return key + (key >> structureIDHashShift7);
    }

    ALWAYS_INLINE static uint32_t hasOwnCachePrimaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t sid = bitwise_cast<uint32_t>(structureID);
        return ((sid >> structureIDHashShift1) ^ (sid >> structureIDHashShift8)) + uid->hash();
    }

    ALWAYS_INLINE static uint32_t hasOwnCacheSecondaryHash(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t key = bitwise_cast<uint32_t>(structureID) + static_cast<uint32_t>(bitwise_cast<uintptr_t>(uid));
        return key + (key >> structureIDHashShift9);
    }

    JS_EXPORT_PRIVATE void age(CollectionScope);

    void initAsMiss(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t primaryIndex = MegamorphicCache::loadPrimaryHash(structureID, uid) & loadCachePrimaryMask;
        auto& entry = m_loadCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::loadSecondaryHash(entry.m_structureID, entry.m_uid.get()) & loadCacheSecondaryMask;
            m_loadCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_loadCachePrimaryEntries[primaryIndex].initAsMiss(structureID, uid, m_epoch);
    }

    void initAsHit(StructureID structureID, UniquedStringImpl* uid, JSCell* holder, uint16_t offset, bool ownProperty)
    {
        uint32_t primaryIndex = MegamorphicCache::loadPrimaryHash(structureID, uid) & loadCachePrimaryMask;
        auto& entry = m_loadCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::loadSecondaryHash(entry.m_structureID, entry.m_uid.get()) & loadCacheSecondaryMask;
            m_loadCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_loadCachePrimaryEntries[primaryIndex].initAsHit(structureID, uid, m_epoch, holder, offset, ownProperty);
    }

    void initAsTransition(StructureID oldStructureID, StructureID newStructureID, UniquedStringImpl* uid, uint16_t offset)
    {
        uint32_t primaryIndex = MegamorphicCache::storeCachePrimaryHash(oldStructureID, uid) & storeCachePrimaryMask;
        auto& entry = m_storeCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::storeCacheSecondaryHash(entry.m_oldStructureID, entry.m_uid.get()) & storeCacheSecondaryMask;
            m_storeCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_storeCachePrimaryEntries[primaryIndex].init(oldStructureID, newStructureID, uid, m_epoch, offset);
    }

    void initAsReplace(StructureID structureID, UniquedStringImpl* uid, uint16_t offset)
    {
        uint32_t primaryIndex = MegamorphicCache::storeCachePrimaryHash(structureID, uid) & storeCachePrimaryMask;
        auto& entry = m_storeCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::storeCacheSecondaryHash(entry.m_oldStructureID, entry.m_uid.get()) & storeCacheSecondaryMask;
            m_storeCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_storeCachePrimaryEntries[primaryIndex].init(structureID, structureID, uid, m_epoch, offset);
    }

    void initAsHasHit(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t primaryIndex = MegamorphicCache::hasCachePrimaryHash(structureID, uid) & hasCachePrimaryMask;
        auto& entry = m_hasCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::hasCacheSecondaryHash(entry.m_structureID, entry.m_uid.get()) & hasCacheSecondaryMask;
            m_hasCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_hasCachePrimaryEntries[primaryIndex].init(structureID, uid, m_epoch, true);
    }

    void initAsHasMiss(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t primaryIndex = MegamorphicCache::hasCachePrimaryHash(structureID, uid) & hasCachePrimaryMask;
        auto& entry = m_hasCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::hasCacheSecondaryHash(entry.m_structureID, entry.m_uid.get()) & hasCacheSecondaryMask;
            m_hasCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_hasCachePrimaryEntries[primaryIndex].init(structureID, uid, m_epoch, false);
    }

    void initAsHasOwnHit(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t primaryIndex = MegamorphicCache::hasOwnCachePrimaryHash(structureID, uid) & hasOwnCachePrimaryMask;
        auto& entry = m_hasOwnCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::hasOwnCacheSecondaryHash(entry.m_structureID, entry.m_uid.get()) & hasOwnCacheSecondaryMask;
            m_hasOwnCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_hasOwnCachePrimaryEntries[primaryIndex].init(structureID, uid, m_epoch, true);
    }

    void initAsHasOwnMiss(StructureID structureID, UniquedStringImpl* uid)
    {
        uint32_t primaryIndex = MegamorphicCache::hasOwnCachePrimaryHash(structureID, uid) & hasOwnCachePrimaryMask;
        auto& entry = m_hasOwnCachePrimaryEntries[primaryIndex];
        if (entry.m_epoch == m_epoch) {
            uint32_t secondaryIndex = MegamorphicCache::hasOwnCacheSecondaryHash(entry.m_structureID, entry.m_uid.get()) & hasOwnCacheSecondaryMask;
            m_hasOwnCacheSecondaryEntries[secondaryIndex] = WTFMove(entry);
        }
        m_hasOwnCachePrimaryEntries[primaryIndex].init(structureID, uid, m_epoch, false);
    }

    ALWAYS_INLINE std::optional<bool> tryGetHasOwn(Structure* structure, UniquedStringImpl* uid)
    {
        StructureID structureID = structure->id();
        uint32_t primaryIndex = MegamorphicCache::hasOwnCachePrimaryHash(structureID, uid) & hasOwnCachePrimaryMask;
        auto& primaryEntry = m_hasOwnCachePrimaryEntries[primaryIndex];
        if (primaryEntry.m_uid == uid && primaryEntry.m_structureID == structureID && primaryEntry.m_epoch == m_epoch)
            return primaryEntry.m_result;

        uint32_t secondaryIndex = MegamorphicCache::hasOwnCacheSecondaryHash(structureID, uid) & hasOwnCacheSecondaryMask;
        auto& secondaryEntry = m_hasOwnCacheSecondaryEntries[secondaryIndex];
        if (secondaryEntry.m_uid == uid && secondaryEntry.m_structureID == structureID && secondaryEntry.m_epoch == m_epoch)
            return secondaryEntry.m_result;

        return std::nullopt;
    }

    ALWAYS_INLINE bool tryAddHasOwn(PropertySlot& slot, JSObject* object, UniquedStringImpl* uid, bool result)
    {
        if (!slot.isCacheable() && !slot.isUnset())
            return false;

        if (object->type() == GlobalProxyType)
            return false;

        Structure* structure = object->structure();

        if (!structure->propertyAccessesAreCacheable())
            return false;

        if (result) {
            ASSERT(!slot.isUnset());
            // If this is hit and slot is cacheable, we do not care about Dictionary.
            initAsHasOwnHit(structure->id(), uid);
            return true;
        }

        ASSERT(slot.isUnset());
        if (!structure->propertyAccessesAreCacheableForAbsence())
            return false;

        // FIXME: We should be able to flatten a dictionary object again.
        // https://bugs.webkit.org/show_bug.cgi?id=163092
        if (structure->isDictionary())
            return false;

        initAsHasOwnMiss(structure->id(), uid);
        return true;
    }

    uint16_t epoch() const { return m_epoch; }

    void bumpEpoch()
    {
        ++m_epoch;
        if (UNLIKELY(m_epoch == invalidEpoch))
            clearEntries();
    }

private:
    JS_EXPORT_PRIVATE void clearEntries();

    std::array<LoadEntry, loadCachePrimarySize> m_loadCachePrimaryEntries { };
    std::array<LoadEntry, loadCacheSecondarySize> m_loadCacheSecondaryEntries { };
    std::array<StoreEntry, storeCachePrimarySize> m_storeCachePrimaryEntries { };
    std::array<StoreEntry, storeCacheSecondarySize> m_storeCacheSecondaryEntries { };
    std::array<HasEntry, hasCachePrimarySize> m_hasCachePrimaryEntries { };
    std::array<HasEntry, hasCacheSecondarySize> m_hasCacheSecondaryEntries { };
    std::array<HasOwnEntry, hasOwnCachePrimarySize> m_hasOwnCachePrimaryEntries { };
    std::array<HasOwnEntry, hasOwnCacheSecondarySize> m_hasOwnCacheSecondaryEntries { };
    uint16_t m_epoch { 1 };
};

} // namespace JSC
