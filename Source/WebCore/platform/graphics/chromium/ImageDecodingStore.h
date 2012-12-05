/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageDecodingStore_h
#define ImageDecodingStore_h

#include "ScaledImageFragment.h"
#include "SkTypes.h"
#include "SkSize.h"
#include "SkSizeHash.h"

#include <wtf/DoublyLinkedList.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

namespace WebCore {

class ImageDecoder;
class ImageFrameGenerator;
class SharedBuffer;

class ImageDecodingStore {
public:
    static PassOwnPtr<ImageDecodingStore> create() { return adoptPtr(new ImageDecodingStore); }
    ~ImageDecodingStore();

    static ImageDecodingStore* instance();
    static void initializeOnce();
    static void shutdown();

    const ScaledImageFragment* lockCompleteCache(const ImageFrameGenerator*, const SkISize& scaledSize);
    const ScaledImageFragment* lockIncompleteCache(const ImageFrameGenerator*, const SkISize& scaledSize);
    void unlockCache(const ImageFrameGenerator*, const ScaledImageFragment*);
    const ScaledImageFragment* insertAndLockCache(const ImageFrameGenerator*, PassOwnPtr<ScaledImageFragment>);

    // Remove all cache entries indexed by ImageFrameGenerator.
    void removeCacheIndexedByGenerator(const ImageFrameGenerator*);

    void setCacheLimitInBytes(size_t);
    size_t memoryUsageInBytes();
    unsigned cacheEntries();

private:
    typedef std::pair<const ImageFrameGenerator*, SkISize> CacheIdentifier;

    class CacheEntry : public DoublyLinkedListNode<CacheEntry> {
        friend class DoublyLinkedListNode<CacheEntry>;
    public:
        static PassOwnPtr<CacheEntry> createAndUse(const ImageFrameGenerator* generator, PassOwnPtr<ScaledImageFragment> image)
        {
            return adoptPtr(new CacheEntry(generator, image, 1));
        }

        CacheEntry(const ImageFrameGenerator* generator, PassOwnPtr<ScaledImageFragment> image, int count)
            : m_prev(0)
            , m_next(0)
            , m_generator(generator)
            , m_cachedImage(image)
            , m_useCount(count)
        {
        }

        ~CacheEntry()
        {
            ASSERT(!m_useCount);
        }

        CacheIdentifier cacheKey() const { return std::make_pair(m_generator, m_cachedImage->scaledSize()); }
        const ScaledImageFragment* cachedImage() const { return m_cachedImage.get(); }
        int useCount() const { return m_useCount; }
        void incrementUseCount() { ++m_useCount; }
        void decrementUseCount() { --m_useCount; ASSERT(m_useCount >= 0); }

        // FIXME: getSafeSize() returns size in bytes truncated to a 32-bits integer.
        //        Find a way to get the size in 64-bits.
        size_t memoryUsageInBytes() const { return cachedImage()->bitmap().getSafeSize(); }

    private:
        CacheEntry* m_prev;
        CacheEntry* m_next;
        const ImageFrameGenerator* m_generator;
        OwnPtr<ScaledImageFragment> m_cachedImage;
        int m_useCount;
    };

    ImageDecodingStore();

    void prune();

    // These methods are called while m_mutex is locked.
    void insertCacheInternal(PassOwnPtr<CacheEntry>);
    void removeFromCacheInternal(const CacheEntry*, Vector<OwnPtr<CacheEntry> >* deletionList);
    void removeFromCacheListInternal(const Vector<OwnPtr<CacheEntry> >& deletionList);
    void incrementMemoryUsage(size_t size) { m_memoryUsageInBytes += size; }
    void decrementMemoryUsage(size_t size)
    {
        ASSERT(m_memoryUsageInBytes >= size);
        m_memoryUsageInBytes -= size;
    }

    // Head of this list is the least recently used cache entry.
    // Tail of this list is the most recently used cache entry.
    DoublyLinkedList<CacheEntry> m_orderedCacheList;

    typedef HashMap<CacheIdentifier, OwnPtr<CacheEntry> > CacheMap;
    CacheMap m_cacheMap;

    typedef HashSet<SkISize> SizeSet;
    typedef HashMap<const ImageFrameGenerator*, SizeSet> CachedSizeMap;
    CachedSizeMap m_cachedSizeMap;

    size_t m_cacheLimitInBytes;
    size_t m_memoryUsageInBytes;

    // Protect concurrent access to these members:
    //   m_orderedCacheList
    //   m_cacheMap and all CacheEntrys stored in it
    //   m_cachedSizeMap
    //   m_cacheLimitInBytes
    //   m_memoryUsageInBytes
    Mutex m_mutex;
};

} // namespace WebCore

#endif
