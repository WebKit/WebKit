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

private:
    struct CacheEntry {
        static PassOwnPtr<CacheEntry> create() { return adoptPtr(new CacheEntry()); }
        static PassOwnPtr<CacheEntry> createAndUse(PassOwnPtr<ScaledImageFragment> image) { return adoptPtr(new CacheEntry(image, 1)); }

        CacheEntry()
            : useCount(0)
        {
        }

        CacheEntry(PassOwnPtr<ScaledImageFragment> image, int count)
            : cachedImage(image)
            , useCount(count)
        {
        }

        ~CacheEntry()
        {
            ASSERT(!useCount);
        }

        OwnPtr<ScaledImageFragment> cachedImage;
        int useCount;
    };

    ImageDecodingStore();
    void prune();

    Vector<OwnPtr<CacheEntry> > m_cacheEntries;

    typedef std::pair<const ImageFrameGenerator*, SkISize> CacheIdentifier;
    typedef HashMap<CacheIdentifier, CacheEntry*> CacheMap;
    CacheMap m_cacheMap;

    // Protect concurrent access to all instances of CacheEntry stored in m_cacheEntries and
    // m_cacheMap as well as the containers.
    Mutex m_mutex;
};

} // namespace WebCore

#endif
