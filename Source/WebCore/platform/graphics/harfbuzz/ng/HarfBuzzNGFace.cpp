/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HarfBuzzNGFace.h"

#include "FontPlatformData.h"
#include "hb.h"
#include <wtf/HashMap.h>

namespace WebCore {

// Though we have FontCache class, which provides the cache mechanism for
// WebKit's font objects, we also need additional caching layer for HarfBuzz
// to reduce the memory consumption because hb_face_t should be associated with
// underling font data (e.g. CTFontRef, FTFace).
typedef pair<hb_face_t*, unsigned> FaceCacheEntry;
typedef HashMap<uint64_t, FaceCacheEntry, WTF::IntHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t> > HarfBuzzNGFaceCache;

static HarfBuzzNGFaceCache* harfbuzzFaceCache()
{
    DEFINE_STATIC_LOCAL(HarfBuzzNGFaceCache, s_harfbuzzFaceCache, ());
    return &s_harfbuzzFaceCache;
}

HarfBuzzNGFace::HarfBuzzNGFace(FontPlatformData* platformData, uint64_t uniqueID)
    : m_platformData(platformData)
    , m_uniqueID(uniqueID)
{
    HarfBuzzNGFaceCache::iterator result = harfbuzzFaceCache()->find(m_uniqueID);
    if (result == harfbuzzFaceCache()->end()) {
        m_face = createFace();
        ASSERT(m_face);
        harfbuzzFaceCache()->set(m_uniqueID, FaceCacheEntry(m_face, 1));
    } else {
        ++(result.get()->value.second);
        m_face = result.get()->value.first;
    }
}

HarfBuzzNGFace::~HarfBuzzNGFace()
{
    HarfBuzzNGFaceCache::iterator result = harfbuzzFaceCache()->find(m_uniqueID);
    ASSERT(result != harfbuzzFaceCache()->end());
    ASSERT(result.get()->value.second > 0);
    --(result.get()->value.second);
    if (!(result.get()->value.second)) {
        hb_face_destroy(result.get()->value.first);
        harfbuzzFaceCache()->remove(m_uniqueID);
    }
}

} // namespace WebCore
